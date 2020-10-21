#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <direct.h>

#include "animatron.h"
#include "entities.h"
#include "cas.h"

#include "utils.h"
#include "animatron.h"

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

// TODO: fixup the headers above. don't need them all anymore

void CopyFile(const char* src, const char* dest)
{
    std::vector<unsigned char> data;

    // read the data into memory
    {
        FILE* file = nullptr;
        fopen_s(&file, src, "rb");
        if (!file)
        {
            printf("Failed to copy file!! Could not open %s for read.\n", src);
            return;
        }

        fseek(file, 0, SEEK_END);
        data.resize(ftell(file));
        fseek(file, 0, SEEK_SET);

        fread(data.data(), data.size(), 1, file);
        fclose(file);
    }

    // write the new file
    {
        FILE* file = nullptr;
        fopen_s(&file, dest, "wb");
        if (!file)
        {
            printf("Failed to copy file!! Could not open %s for write.\n", dest);
            return;
        }

        fwrite(data.data(), data.size(), 1, file);
        fclose(file);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("Usage:\n    animatron <sourcefile> <destfile>\n\n    <sourcefile> is a json file describing the animation.\n    <destfile> is the name and location of the mp4 output file.\n\n");
        return 1;
    }

    // handle command line arguments
    const char* srcFile = argv[1];
    const char* destFile = nullptr;
    char destFileBuffer[1024];
    if (argc < 3)
    {
        strcpy(destFileBuffer, srcFile);
        int len = (int)strlen(destFileBuffer);
        while (len > 0 && destFileBuffer[len] != '.')
            len--;
        if (len == 0)
        {
            destFile = "out.mp4";
        }
        else
        {
            destFileBuffer[len] = 0;
            strcat(destFileBuffer, ".mp4");
            destFile = destFileBuffer;
        }
    }
    else
    {
        destFile = argv[2];
    }

    // start the timer
    std::chrono::high_resolution_clock::time_point timeStart = std::chrono::high_resolution_clock::now();

    // load the document
    Data::Document document;
    if (!ReadFromJSONFile(document, srcFile))
    {
        return 1;
    }

    // load the config
    if (!ReadFromJSONFile(document.config, "internal/config.json"))
    {
        printf("Could not load internal/config.json!");
        return 1;
    }

    // document validation and fixup
    if (!ValidateAndFixupDocument(document))
    {
        printf("Document validation failed\n");
        return 1;
    }

    // report what we are doing
    int framesTotal = TotalFrameCount(document);
    printf("Animatron v%i.%i\n", c_programVersionMajor, c_programVersionMinor);
    printf("Rendering with %i threads...\n", omp_get_max_threads());
    printf("  srcFile: %s\n", srcFile);
    printf("  destFile: %s\n", destFile);
    printf("  %i frames rendered at %i x %i with %i samples per pixel, output to %i x %i\n",
        framesTotal, document.renderSizeX, document.renderSizeY, document.samplesPerPixel,
        document.outputSizeX, document.outputSizeY);

    // Render and write out each frame multithreadedly
    std::vector<ThreadContext> threadContexts(omp_get_max_threads());
    Context context;

    bool wasError = false;
    std::atomic<int> framesDone(0);
    std::atomic<int> framesRendered(0);
    std::atomic<int> framesRecycled(0);

    // debug builds are single threaded
    #if _DEBUG
        omp_set_num_threads(1);
    #endif

    std::atomic<int> nextFrameIndex(0);
    #pragma omp parallel
    while(1)
    {
        int frameIndex = nextFrameIndex++;
        if (frameIndex >= framesTotal)
            break;

        ThreadContext& threadContext = threadContexts[omp_get_thread_num()];
        threadContext.threadId = omp_get_thread_num();

        // report progress
        //if (omp_get_thread_num() == 0)
        {
            static int lastPercent = -1;
            int percent = int(100.0f * float(framesDone) / float(framesTotal - 1));
            if (lastPercent != percent)
            {
                lastPercent = percent;
                printf("\r%i%%", lastPercent);
            }
        }

        // render a frame
        int recycledFrameIndex = -1;
        size_t frameHash = 0;
        if (!RenderFrame(document, frameIndex, threadContext, context, recycledFrameIndex, frameHash))
        {
            wasError = true;
            break;
        }

        // write it out
        if (recycledFrameIndex == -1)
        {
            framesRendered++;

            sprintf_s(threadContext.outFileName, "build/%i.%s", frameIndex, (document.config.writeFrames == Data::ImageFileType::PNG) ? "png" : "bmp");
            if (document.config.writeFrames == Data::ImageFileType::PNG)
                stbi_write_png(threadContext.outFileName, document.outputSizeX, document.outputSizeY, 4, threadContext.pixelsU8.data(), document.outputSizeX * 4);
            else
                stbi_write_bmp(threadContext.outFileName, document.outputSizeX, document.outputSizeY, 4, threadContext.pixelsU8.data());

            // Set this frame in the frame cache for recycling
            context.frameCache.SetFrame(frameHash, frameIndex, threadContext.pixelsU8);
        }
        else
        {
            framesRecycled++;

            const char* extension = (document.config.writeFrames == Data::ImageFileType::PNG) ? "png" : "bmp";
            char fileName1[256];
            sprintf_s(fileName1, "build/%i.%s", recycledFrameIndex, extension);
            char fileName2[256];
            sprintf_s(fileName2, "build/%i.%s", frameIndex, extension);
            CopyFile(fileName1, fileName2);
        }

        framesDone++;
    }
    printf("\r100%%\n");

    // assemble the frames into a movie
    if (!wasError)
    {
        // Youtube recomended settings (https://gist.github.com/mikoim/27e4e0dc64e384adbcb91ff10a2d3678)
        printf("Assembling frames...\n");

        bool hasAudio = !document.audioFile.empty();

        char inputs[1024];
        if (!hasAudio)
            sprintf_s(inputs, "-i build/%%d.%s", (document.config.writeFrames == Data::ImageFileType::PNG) ? "png" : "bmp");
        else
            sprintf_s(inputs, "-i build/%%d.%s -i %s", (document.config.writeFrames == Data::ImageFileType::PNG) ? "png" : "bmp", document.audioFile.c_str());

        char audioOptions[1024];
        if (hasAudio)
            sprintf(audioOptions, " -c:a aac -b:a 384k ");
        else
            sprintf(audioOptions, " ");

        char containerOptions[1024];
        sprintf(containerOptions, "-frames:v %i -movflags faststart -c:v libx264 -profile:v high -bf 2 -g 30 -crf 18 -pix_fmt yuv420p %s", framesTotal, hasAudio ? "-filter_complex \"[1:0] apad \" -shortest" : "");

        char buffer[1024];
        sprintf_s(buffer, "%s -y -framerate %i %s%s%s %s", document.config.ffmpeg.c_str(), document.FPS, inputs, audioOptions, containerOptions, destFile);

        system(buffer);
    }

    // report how long it took and other stats
    {
        std::chrono::duration<float> seconds = (std::chrono::high_resolution_clock::now() - timeStart);
        float secondsPerFrame = seconds.count() / float(framesTotal);
        printf("Render Time: %0.3f seconds.\n  %0.3f seconds per frame average wall time (more threads make this lower)\n  %0.3f seconds per frame average actual computation time\n", seconds.count(), secondsPerFrame, secondsPerFrame * float(threadContexts.size()));
        printf("frames rendered: %i\nframes recycled: %i\n", framesRendered.load(), framesRecycled.load());
    }

    if (wasError)
    {
        return 1;
    }

    return 0;
}

/*
Animatron Editor
* put entities in a tree view and show their properties in a property panel
* scrub bar and preview window.
* unsure how to show keyframes. could be tree view for now probably.

TODO:
* should animatron lib files go into the animatronlib folder? i think so!
* maybe we need a folder for files for animatron the executable as well.

*/

// TODO: put resized image into CAS

// TODO: may be able to use ffmpeg with pattern type "none" and give the file name for each frmae, which means you don't need to make it sparse? https://www.ffmpeg.org/ffmpeg-formats.html#image2-1
// would be 1400+ files in the list though!!

// TODO: after video is out, write (or generate!) some documentation and a short tutorial on how to use it. also write up the blog post about how it works
// TODO: after this video is out, maybe make a df_serialize editor in C#? then make a video editor, where it uses this (as a DLL?) to render the frame the scrubber wants to see.

// TODO: maybe try a windows API to copy the file instead of what you are doing? CopyFileExA or CopyFileExW

// TODO: maybe gaussian blur the intro screen away. if so, do separated blur. maybe entities (or entity types?) should be able to have per thread storage, so that it could keep a temporary pixel buffer there for the separated blur?

// TODO: i think the camera look at is wrong. test it and see. clip.json is not doing right things

// TODO: need to clip lines against the z plane! can literally just do that, shouldn't be hard, but need z projections in matrices


/*
TODO:

* be able to halign and valign latex. maybe images too?

* maybe instead of having parents like we do, each position should have an optional parent

! in the future, maybe need to have parenting take a whole transform, instead of just position. unsure what it means for some things to be parented. work it out?

! check for and error on duplicate object ids

! don't forget that PMA can do ADDITIVE BLENDING!

// TODO: i think things need to parent off of scenes (to get camera) and transforms, instead of getting them by name (maybe?)

! flatten checkins for v1

* NEXT: the goal is to make the intro screen for simplexplanations2 which is about Kahns algorithm.
 * definitely want to be able to have a slowly rotating 3d tetrahedron. probably want to rotate a 2d triangle and line too. and have a point as well

* should probably figure out how to make variants be unions instead of having all fields soon. there are quite a few entity types!

* profile?

* add object parenting and getting transforms from parents.
 * a transform is probably a parent all it's own. local / global rotation pivot?

 * Parent off of a layer to render into a sublayer which is then merged back into the main image with alpha blending.

* Gaussian blur action for fun.
* Bloom?  Ruby has awesome bloom, ask!
* Tone map.

? if a node is un-named in the json file, maybe auto give it a unique name based on it's node index? you don't always need a name... only when referencing a node!

* be able to have different animation tracks for an object. have a keyframe specify the track number (sorts for applying them, so probably a float)
 * should probably have a bitset of what fields are present in json data.
 * this could be a feature of df_serialize to reflect out this parallel object of bools and also fill it in.
 * Right now if you have a keyframe spanning 0 to 10 for rotation and put a scale at 5, it makes the rotation start at 5 and go to 10. Multiple tracks would help this.

* prefabs for library of reusable objects
 * Prefabs are another list of things.
 * They are a list of entities.
 * Allow file includes in general where an included json could have any of the data a regular file could. Prefabs included but not limited to.
 * When you make an entity of type prefab, it makes objects with a name prefix (name. Or name::) before the name.
 * This lets you make dice etc that are reusable.

? are frame init and do action both really needed? right now no, but maybe can make it be later.

----- Low Priority Features -----



----- Future features (when needed) -----
* convert a canvas to greyscale
* multiple a canvas by a color
* lerp between canvases
* copy a canvas? (z order could make sure it's rendered before copy)
* resize a convas, with options for up / down filters. just have box and bicubic right now for main canvas
* audio synth.. wave forms, envelopes, per channel operations, FIR, IIR. karplus strong, etc.
* poisson blending could be cool to paste objects into scenes they arent from.
* Could do effects between frames but it would serialize it. lik ebeing able to blend with the previous frame.
* generate html documentation of file format
* whitted and path traced raytracing.  have lights / emissive i guess. unlit if no lights in whitted?
* sample cube maps from raytracing i guess
* other 2d sample sequences: R2, white noise, regular, jittered grid, bayer
! other subpixel jitter types to implement: white noise, projective blue noise, R2, sobol.
 * may also have some that animate over time to make the noise good over time
* if init times become a problem, could do content addressable storage and cache things like latex images.
* is there something we can use besides system() which can hide the output of the latex commands?
! could show an estimated time remaining along with the percent!
* allow animated gifs as output? i think ffmpeg can do that
* quantization options to decrease unique colors for stylistic effect
 * combine with point sampled canvas resizing!
* support recursive matrix parenting.
! can macros reflect to C#? if so, could make a C# editor for df_serialize and let it load/save binary/json
 * could also make this thing able to render a single frame of a clip at a specific time, and use it to make a scrubber bar for C#?

----- Notes on the architecture, for writeup -----
* having a sparse json reader means that keyframe data can be described as sparse json that is read. things are automatically key framable.
* having df_serialize means i add a field to the schema then... i can add it to the json and it shows up at runtime.  I can also already use it as keyframe data!
* using macros to define schema means that i can auto expand macros for other uses and turn runtime checks into compile errors. Like wanting a function per entity.
* cubic bezier for interpolating values seems good for linear and non linear blends. can even NOT do blending by making CPs all 0s or all 1s.
* omp made it super easy to multitread
* stb_image super easy to load images
* latex.exe and dvipng for latex support: text, diagrams, formulas
* multi sampling! having each thing (lines, fill, etc) know about multisampling is nice cause fills don't need multi sampling but lines do. don't need to make N buffers.
* zordering of objects
* 3d objects composed inline.
* premultiplied alpha, sRGB correctness, blue noise dithering before quantization from float to U8.
* not worrying a whole lot about perf. I probably should, it would be nice if it rendered faster. lots of time spent in linear to sRGB though actually.
 * probably would be lots faster on gpu. this was quick and cross platform, i'm happy.
 * i think caching more things could be helpful maybe. like don't need to calculate the same gradient every frame if it's expensive to do so. don't need to calculate a transform matrix every frame if it hasn't changed
* trying to keep focused on only implementing features as i need them mostly. mixed results
* a simple editor seems like it'd be real helpful and real easy to make... a list of entities and key frames in the file. a property sheet for each and ability to add / delete. then a scrub bar with a preview window. maybe make this program able to generate a specific frame instead of a full movie and use it for the preview? dunno if responsive enough
 * a simple editor for df_serialize would be nice, and this could be an extension of that. Basically just adding a scrub bar and preview window
* CAS cache. transient and not. how many frames are actually rendered?
*/