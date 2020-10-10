#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>
#include <omp.h>
#include <atomic>
#include <chrono>
#include <direct.h>

#include "schemas/types.h"
#include "schemas/json.h"
#include "schemas/lerp.h"
#include "config.h"
#include "entities.h"
#include "cas.h"

#include "utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

struct EntityTimelineKeyframe
{
    float time = 0.0f;
    std::array<float,4> blendControlPoints = { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f };
    Data::Entity entity;
};

struct EntityTimeline
{
    std::string id;
    float zorder = 0.0f;
    float createTime = 0.0f;
    float destroyTime = -1.0f;
    std::vector<EntityTimelineKeyframe> keyFrames;
};

bool GenerateFrame(const Data::Document& document, const std::vector<const EntityTimeline*>& entityTimelines, std::vector<Data::ColorPMA>& pixels, int frameIndex)
{
    // setup for the frame
    float frameTime = (float(frameIndex) / float(document.FPS)) + document.startTime;
    pixels.resize(document.renderSizeX*document.renderSizeY);
    std::fill(pixels.begin(), pixels.end(), Data::ColorPMA{ 0.0f, 0.0f, 0.0f, 0.0f });

    // Get the key frame interpolated state of each entity first, so that they can look at eachother (like 3d objects looking at their camera)
    std::unordered_map<std::string, Data::Entity> entityMap;
    {
        for (const EntityTimeline* timeline_ : entityTimelines)
        {
            // skip any entity that doesn't currently exist
            const EntityTimeline& timeline = *timeline_;
            if (frameTime < timeline.createTime || (timeline.destroyTime >= 0.0f && frameTime > timeline.destroyTime))
                continue;

            // find where we are in the time line
            int cursorIndex = 0;
            while (cursorIndex + 1 < timeline.keyFrames.size() && timeline.keyFrames[cursorIndex + 1].time < frameTime)
                cursorIndex++;

            // interpolate keyframes if we are between two key frames
            Data::Entity entity;
            if (cursorIndex + 1 < timeline.keyFrames.size())
            {
                // calculate the blend percentage from the key frame percentage and the control points
                float blendPercent = 0.0f;
                if (cursorIndex + 1 < timeline.keyFrames.size())
                {
                    float t = frameTime - timeline.keyFrames[cursorIndex].time;
                    t /= (timeline.keyFrames[cursorIndex + 1].time - timeline.keyFrames[cursorIndex].time);

                    float CPA = timeline.keyFrames[cursorIndex + 1].blendControlPoints[0];
                    float CPB = timeline.keyFrames[cursorIndex + 1].blendControlPoints[1];
                    float CPC = timeline.keyFrames[cursorIndex + 1].blendControlPoints[2];
                    float CPD = timeline.keyFrames[cursorIndex + 1].blendControlPoints[3];

                    blendPercent = Clamp(CubicBezierInterpolation(CPA, CPB, CPC, CPD, t), 0.0f, 1.0f);
                }

                // Get the entity(ies) involved
                const Data::Entity& entity1 = timeline.keyFrames[cursorIndex].entity;
                const Data::Entity& entity2 = timeline.keyFrames[cursorIndex + 1].entity;

                // Do the lerp between keyframe entities
                // Set entity to entity1 first though to catch anything that isn't serialized (and not lerped)
                entity = entity1;
                Lerp(entity1, entity2, entity, blendPercent);
            }
            // otherwise we are beyond the last key frame, so just set the value
            else
            {
                entity = timeline.keyFrames[cursorIndex].entity;
            }

            // do per frame entity initialization
            bool error = false;
            switch (entity.data._index)
            {
                #include "df_serialize/df_serialize/_common.h"
                #define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
                    case Data::EntityVariant::c_index_##_NAME: error = ! _TYPE##_Action::FrameInitialize(document, entity); break;
                #include "df_serialize/df_serialize/_fillunsetdefines.h"
                #include "schemas/schemas_entities.h"
                default:
                {
                    printf("unhandled entity type in variant\n");
                    return false;
                }
            }
            if (error)
            {
                printf("entity %s failed to FrameInitialize\n", timeline.id.c_str());
                return false;
            }

            entityMap[timeline.id] = entity;
        }
    }

    // process the entities in zorder
    for (const EntityTimeline* timeline_ : entityTimelines)
    {
        // skip any entity that doesn't currently exist
        const EntityTimeline& timeline = *timeline_;
        auto it = entityMap.find(timeline.id);
        if (it == entityMap.end())
            continue;

        // do the entity action
        bool error = false;
        const Data::Entity& entity = it->second;
        switch (entity.data._index)
        {
            #include "df_serialize/df_serialize/_common.h"
            #define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
                case Data::EntityVariant::c_index_##_NAME: error = ! _TYPE##_Action::DoAction(document, entityMap, pixels, entity); break;
            #include "df_serialize/df_serialize/_fillunsetdefines.h"
            #include "schemas/schemas_entities.h"
            default:
            {
                printf("unhandled entity type in variant\n");
                return false;
            }
        }
        if (error)
            return false;
    }

    return true;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("Usage:\n    animatron <sourcefile> <destfile>\n\n    <sourcefile> is a json file describing the animation.\n    <destfile> is the name and location of the mp4 output file.\n\n");
        return 1;
    }
    // Read the data in
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
    Data::Document document;
    if (!ReadFromJSONFile(document, srcFile))
    {
        system("pause");
        return 1;
    }

    // verify
    if (document.program != "animatron")
    {
        printf("Not an animatron file!\n");
        system("pause");
        return 1;
    }

    // version fixup
    if (document.versionMajor != c_documentVersionMajor || document.versionMinor != c_documentVersionMinor)
    {
        printf("Wrong version number: %i.%i, not %i.%i\n", document.versionMajor, document.versionMinor, c_documentVersionMajor, c_documentVersionMinor);
        system("pause");
        return 1;
    }

    // initialize CAS
    if (!CAS::Get().Init())
    {
        printf("Could not init CAS\n");
        return 1;
    }

    // read the config if possible
    if (ReadFromJSONFile(document.config, "internal/config.json"))
    {
        // verify
        if (document.config.program != "animatronconfig")
        {
            printf("Not an animatron config file!\n");
            system("pause");
            return 1;
        }

        // version fixup
        if (document.config.versionMajor != c_configVersionMajor || document.config.versionMinor != c_configVersionMinor)
        {
            printf("Wrong config version number: %i.%i, not %i.%i\n", document.config.versionMajor, document.config.versionMinor, c_configVersionMajor, c_configVersionMinor);
            system("pause");
            return 1;
        }
    }
    else
    {
        printf("Could not load internal/config.json!");
        return 1;
    }
    
    // data interpretation and fixup
    {
        if (document.renderSizeX == 0)
            document.renderSizeX = document.outputSizeX;
        if (document.renderSizeY == 0)
            document.renderSizeY = document.outputSizeY;
        if (document.samplesPerPixel < 1)
            document.samplesPerPixel = 1;

        MakeJitterSequence(document);
    }

    // report what we are doing
    int framesTotal = int(document.duration * float(document.FPS));
    printf("Animatron v%i.%i\n", c_programVersionMajor, c_programVersionMinor);
    printf("Rendering with %i threads...\n", omp_get_max_threads());
    printf("  srcFile: %s\n", srcFile);
    printf("  destFile: %s\n", destFile);
    printf("  %i frames rendered at %i x %i with %i samples per pixel, output to %i x %i\n",
        framesTotal, document.renderSizeX, document.renderSizeY, document.samplesPerPixel,
        document.outputSizeX, document.outputSizeY);

    // start the timer
    std::chrono::high_resolution_clock::time_point timeStart = std::chrono::high_resolution_clock::now();

    // Do one time initialization of entities
    {
        _mkdir("build");
        int entityIndex = -1;
        for (Data::Entity& entity : document.entities)
        {
            bool error = false;
            entityIndex++;
            // do per frame entity initialization
            switch (entity.data._index)
            {
                #include "df_serialize/df_serialize/_common.h"
                #define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
                    case Data::EntityVariant::c_index_##_NAME: error = ! _TYPE##_Action::Initialize(document, entity, entityIndex); break;
                #include "df_serialize/df_serialize/_fillunsetdefines.h"
                #include "schemas/schemas_entities.h"
                default:
                {
                    printf("unhandled entity type in variant\n");
                    return 1;
                }
            }
            if (error)
            {
                printf("entity %s failed to initialize.\n", entity.id.c_str());
                return 1;
            }
        }
    }

    // Load blue noise texture for dithering
    if (document.blueNoiseDither)
    {
        int blueNoiseComponents = 0;
        stbi_uc* pixels = stbi_load("internal/BlueNoiseRGBA.png", &document.blueNoiseWidth, &document.blueNoiseHeight, &blueNoiseComponents, 4);
        if (pixels == nullptr || document.blueNoiseWidth == 0 || document.blueNoiseHeight == 0)
        {
            printf("Could not load internal/BlueNoiseRGBA.png");
            return 1;
        }

        document.blueNoisePixels.resize(document.blueNoiseWidth * document.blueNoiseHeight);
        memcpy((unsigned char*)&document.blueNoisePixels[0], pixels, document.blueNoiseWidth * document.blueNoiseHeight * 4);

        stbi_image_free(pixels);
    }

    // make a timeline for each entity by just starting with the entity definition
    std::unordered_map<std::string, EntityTimeline> entityTimelinesMap;
    for (const Data::Entity& entity : document.entities)
    {
        EntityTimeline newTimeline;
        newTimeline.id = entity.id;
        newTimeline.zorder = entity.zorder;
        newTimeline.createTime = entity.createTime;
        newTimeline.destroyTime = entity.destroyTime;

        EntityTimelineKeyframe newKeyFrame;
        newKeyFrame.time = entity.createTime;
        newKeyFrame.entity = entity;
        newTimeline.keyFrames.push_back(newKeyFrame);

        entityTimelinesMap[entity.id] = newTimeline;
    }

    // sort the keyframes by time ascending to put them in order
    std::sort(
        document.keyFrames.begin(),
        document.keyFrames.end(),
        [](const Data::KeyFrame& a, const Data::KeyFrame& b)
        {
            return a.time < b.time;
        }
    );

    // process each key frame
    for (const Data::KeyFrame& keyFrame : document.keyFrames)
    {
        auto it = entityTimelinesMap.find(keyFrame.entityId);
        if (it == entityTimelinesMap.end())
        {
            printf("Could not find entity %s for keyframe!\n", keyFrame.entityId.c_str());
            system("pause");
            return 1;
        }

        // ignore events outside the lifetime of the entity
        if ((keyFrame.time < it->second.createTime) || (it->second.destroyTime >= 0.0f && keyFrame.time > it->second.destroyTime))
            continue;

        // make a new key frame entry
        EntityTimelineKeyframe newKeyFrame;
        newKeyFrame.time = keyFrame.time;
        newKeyFrame.blendControlPoints = keyFrame.blendControlPoints;

        // start the keyframe entity values at the last keyframe value, so people only make keyframes for the things they want to change
        newKeyFrame.entity = it->second.keyFrames.rbegin()->entity;

        // load the sparse json data over the keyframe data
        if (!keyFrame.newValue.empty())
        {
            bool error = false;
            switch (newKeyFrame.entity.data._index)
            {
                #include "df_serialize/df_serialize/_common.h"
                #define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
                            case Data::EntityVariant::c_index_##_NAME: error = !ReadFromJSONBuffer(newKeyFrame.entity.data.##_NAME, keyFrame.newValue); break;
                #include "df_serialize/df_serialize/_fillunsetdefines.h"
                #include "schemas/schemas_entities.h"
                default:
                {
                    printf("unhandled entity type in variant\n");
                    return 1;
                }
            }
            if (error)
            {
                printf("Could not read json data for keyframe! entity %s, time %f.\n", keyFrame.entityId.c_str(), keyFrame.time);
                system("pause");
                return 1;
            }
        }
        it->second.keyFrames.push_back(newKeyFrame);
    }

    // put the entities into a list sorted by z order ascending
    // stable sort to keep a deterministic ordering of elements for ties
    std::vector<const EntityTimeline*> entityTimelines;
    {
        for (auto& pair : entityTimelinesMap)
            entityTimelines.push_back(&pair.second);

        std::stable_sort(
            entityTimelines.begin(),
            entityTimelines.end(),
            [](const EntityTimeline* a, const EntityTimeline* b)
            {
                return a->zorder < b->zorder;
            }
        );
    }

    // Render and write out each frame multithreadedly
    struct ThreadData
    {
        char outFileName[1024];
        std::vector<Data::ColorPMA> pixelsPMA;
        std::vector<Data::Color> pixels;
        std::vector<Data::ColorU8> pixelsU8;
    };
    std::vector<ThreadData> threadsData(omp_get_max_threads());

    bool wasError = false;
    std::atomic<int> framesDone(0);

    // debug builds are single threaded
    #if _DEBUG
        omp_set_num_threads(1);
    #endif

    #pragma omp parallel for
    for (int frameIndex = 0; frameIndex < framesTotal; ++frameIndex)
    {
        ThreadData& threadData = threadsData[omp_get_thread_num()];

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
        if (!GenerateFrame(document, entityTimelines, threadData.pixelsPMA, frameIndex))
        {
            wasError = true;
            break;
        }
        
        // convert from PMA to non PMA
        threadData.pixels.resize(threadData.pixelsPMA.size());
        for (size_t index = 0; index < threadData.pixelsPMA.size(); ++index)
            threadData.pixels[index] = FromPremultipliedAlpha(threadData.pixelsPMA[index]);

        // resize from the rendered size to the output size
        Resize(threadData.pixels, document.renderSizeX, document.renderSizeY, document.outputSizeX, document.outputSizeY);

        // Do blue noise dithering if we should
        if (document.blueNoiseDither)
        {
            for (size_t iy = 0; iy < document.outputSizeY; ++iy)
            {
                const Data::ColorU8* blueNoiseRow = &document.blueNoisePixels[(iy % document.blueNoiseHeight) * document.blueNoiseWidth];
                Data::Color* destPixel = &threadData.pixels[iy * document.outputSizeX];

                for (size_t ix = 0; ix < document.outputSizeX; ++ix)
                {
                    destPixel->R += (float(blueNoiseRow[ix % document.blueNoiseWidth].R) / 255.0f) / 255.0f;
                    destPixel->G += (float(blueNoiseRow[ix % document.blueNoiseWidth].G) / 255.0f) / 255.0f;
                    destPixel->B += (float(blueNoiseRow[ix % document.blueNoiseWidth].B) / 255.0f) / 255.0f;
                    destPixel->A += (float(blueNoiseRow[ix % document.blueNoiseWidth].A) / 255.0f) / 255.0f;
                    destPixel++;
                }
            }
        }

        // Convert it to RGBAU8
        ColorToColorU8(threadData.pixels, threadData.pixelsU8);

        // force to opaque if we should
        if (document.forceOpaqueOutput)
        {
            for (Data::ColorU8& pixel : threadData.pixelsU8)
                pixel.A = 255;
        }

        // write it out
        sprintf_s(threadData.outFileName, "build/%i.png", frameIndex);
        stbi_write_png(threadData.outFileName, document.outputSizeX, document.outputSizeY, 4, threadData.pixelsU8.data(), document.outputSizeX * 4);

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
            sprintf_s(inputs, "-i build/%%d.png");
        else
            sprintf_s(inputs, "-i build/%%d.png -i %s", document.audioFile.c_str());

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

    // report how long it took
    {
        std::chrono::duration<float> seconds = (std::chrono::high_resolution_clock::now() - timeStart);
        float secondsPerFrame = seconds.count() / float(framesTotal);
        printf("Render Time: %0.3f seconds.\n  %0.3f seconds per frame average wall time (more threads make this lower)\n  %0.3f seconds per frame average actual computation time\n", seconds.count(), secondsPerFrame, secondsPerFrame * float(threadsData.size()));
    }

    if (wasError)
    {
        system("pause");
        return 1;
    }

    return 0;
}

// TODO: why does the first run of clip 4 make 7 latex CAS entries, and a second run makes 1 more?
// TODO: also, many threads make the CAS item in parallel. that is not great ):   could maybe make an entry in the CAS that it's pending and have threads spinlock til it's ready?

// TODO: after video is out, write (or generate!) some documentation and a short tutorial on how to use it.

// TODO: after this video is out, maybe make a df_serialize editor in C#? then make a video editor, where it uses this (as a DLL?) to render the frame the scrubber wants to see.

// TODO: i think canvas to pixel and pixel to canvas need to flip the y axis over. the gradient suggests that.  investigate to be sure.


// TODO: maybe gaussian blur the intro screen away. if so, do separated blur. maybe entities (or entity types?) should be able to have per thread storage, so that it could keep a temporary pixel buffer there for the separated blur?

// TODO: latex DPI is not resolution independent. smaller movie = bigger latex. should fix!
// TODO: the digital disolve isn't either. should fix that too. give size in canvas units perhaps

// TODO: i think the camera look at is wrong. test it and see. clip.json is not doing right things

// TODO: have a cache for latex since it's static. that means it won't be created every run. it will also stop copying those pixels around. could just have a CAS cache in build folder.
// TODO: also have a cache for resized images!

// TODO: need to clip lines against the z plane! can literally just do that, shouldn't be hard, but need z projections in matrices


/*
TODO:

* maybe instead of having parents like we do, each position should have an optional parent

! in the future, maybe need to have parenting take a whole transform, instead of just position. unsure what it means for some things to be parented. work it out?

! check for and error on duplicate object ids

! don't forget that PMA can do ADDITIVE BLENDING!

// TODO: i think things need to parent off of scenes (to get camera) and transforms, instead of getting them by name (maybe?)

* for non 

! flatten checkins for v1

* NEXT: the goal is to make the intro screen for simplexplanations2 which is about Kahns algorithm.
 * definitely want to be able to have a slowly rotating 3d tetrahedron. probably want to rotate a 2d triangle and line too. and have a point as well

* should probably figure out how to make variants be unions instead of having all fields soon. there are quite a few entity types!

* profile?

* add object parenting and getting transforms from parents.
 * a transform is probably a parent all it's own. local / global rotation pivot?

 * textures


 * Parent off of a layer to render into a sublayer which is then merged back into the main image with alpha blending.

* Gaussian blur action for fun.
* Bloom?
* Tone map.

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

* certain things are calculated per frame that don't change from frame to frame, but frames aren't processed in order so can't really cache them in a straightforward way.
 * maybe if there's a long stretch where something doesn't change (no active keyframe? or constant section) could calculate once and re-use.
 * this would be good for things like bezier curve points, and image resizing.
 * could probably store this in the CAS?

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

*/