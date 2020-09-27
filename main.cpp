#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>
#include <omp.h>
#include <atomic>
#include <chrono>

#include "schemas/types.h"
#include "schemas/json.h"
#include "schemas/lerp.h"

#include "utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

// prototypes for entity handler functions. These are implemented in entities.cpp
#include "df_serialize/df_serialize/_common.h"
#define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
    void _TYPE##_DoAction( \
        const Data::Document& document, \
        const std::unordered_map<std::string, Data::EntityVariant>& entityMap, \
        std::vector<Data::ColorPMA>& pixels, \
        const Data::##_TYPE& _NAME); \
   void _TYPE##_Initialize(const Data::Document& document, Data::##_TYPE& _NAME);
#include "df_serialize/df_serialize/_fillunsetdefines.h"
#include "schemas/schemas_entities.h"

struct EntityTimelineKeyframe
{
    float time = 0.0f;
    std::array<float,4> blendControlPoints = { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f };
    Data::EntityVariant entity;
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
    float frameTime = float(frameIndex) / float(document.FPS);
    pixels.resize(document.renderSizeX*document.renderSizeY);
    std::fill(pixels.begin(), pixels.end(), Data::ColorPMA{ 0.0f, 0.0f, 0.0f, 0.0f });

    // Get the key frame interpolated state of each entity first, so that they can look at eachother (like 3d objects looking at their camera)
    std::unordered_map<std::string, Data::EntityVariant> entityMap;
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
            Data::EntityVariant entity;
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
                const Data::EntityVariant& entity1 = timeline.keyFrames[cursorIndex].entity;
                const Data::EntityVariant& entity2 = timeline.keyFrames[cursorIndex + 1].entity;

                // Do the lerp between keyframe entities
                Lerp(entity1, entity2, entity, blendPercent);
            }
            // otherwise we are beyond the last key frame, so just set the value
            else
            {
                entity = timeline.keyFrames[cursorIndex].entity;
            }

            // do per frame entity initialization
            switch (entity._index)
            {
                #include "df_serialize/df_serialize/_common.h"
                #define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
                    case Data::EntityVariant::c_index_##_NAME: ##_TYPE##_Initialize(document, entity.##_NAME); break;
                #include "df_serialize/df_serialize/_fillunsetdefines.h"
                #include "schemas/schemas_entities.h"
                default:
                {
                    printf("unhandled entity type in variant\n");
                    return false;
                }
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
        const Data::EntityVariant& entity = it->second;
        switch (entity._index)
        {
            #include "df_serialize/df_serialize/_common.h"
            #define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
                case Data::EntityVariant::c_index_##_NAME: _TYPE##_DoAction(document, entityMap, pixels, entity.##_NAME); break;
            #include "df_serialize/df_serialize/_fillunsetdefines.h"
            #include "schemas/schemas_entities.h"
            default:
            {
                printf("unhandled entity type in variant\n");
                return false;
            }
        }
    }

    return true;
}

int main(int argc, char** argv)
{
    // Read the data in
    const char* fileName = "./assets/clip.json";
    const char* outFilePath = "./out/";
    Data::Document document;
    const uint32_t versionMajor = document.versionMajor;
    const uint32_t versionMinor = document.versionMinor;
    if (!ReadFromJSONFile(document, fileName))
    {
        system("pause");
        return 1;
    }

    // verify
    if (document.program != "animatron")
    {
        printf("Not an animatron file!\n");
        system("pause");
        return 5;
    }

    // version fixup
    if (document.versionMajor != versionMajor || document.versionMinor != versionMinor)
    {
        // TODO: version fixup
        printf("Wrong version number: %i.%i, not %i.%i. Version upgrades are TODO\n", document.versionMajor, document.versionMinor, versionMajor, versionMinor);
        system("pause");
        return 6;
    }
    
    // data interpretation
    {
        if (document.renderSizeX == 0)
            document.renderSizeX = document.outputSizeX;
        if (document.renderSizeY == 0)
            document.renderSizeY = document.outputSizeY;
        if (document.samplesPerPixel < 1)
            document.samplesPerPixel = 1;

        MakeJitterSequence(document);
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
        newKeyFrame.entity = entity.data;
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
        if (keyFrame.newValue.empty())
            continue;

        auto it = entityTimelinesMap.find(keyFrame.entityId);
        if (it == entityTimelinesMap.end())
        {
            printf("Could not find entity %s for keyframe!\n", keyFrame.entityId.c_str());
            system("pause");
            return 2;
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
        if (!ReadFromJSONBuffer(newKeyFrame.entity, keyFrame.newValue))
        {
            printf("Could not read json data for keyframe! entity %s, time %f.\n", keyFrame.entityId.c_str(), keyFrame.time);
            system("pause");
            return 3;
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

    // report what we are doing
    int framesTotal = int(document.duration * float(document.FPS));
    printf("Animatron v%i.%i\n", versionMajor, versionMinor);
    printf("Rendering with %i threads...\n", omp_get_max_threads());
    printf("  input: %s\n", fileName);
    printf("  output: %s\n", outFilePath);
    printf("  %i frames rendered at %i x %i with %i samples per pixel, output to %i x %i\n",
        framesTotal, document.renderSizeX, document.renderSizeY, document.samplesPerPixel,
        document.outputSizeX, document.outputSizeY);

    // start the timer
    std::chrono::high_resolution_clock::time_point timeStart = std::chrono::high_resolution_clock::now();

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

        // Convert it to RGBAU8
        ColorToColorU8(threadData.pixels, threadData.pixelsU8);

        // force to opaque if we should
        if (document.forceOpaqueOutput)
        {
            for (Data::ColorU8& pixel : threadData.pixelsU8)
                pixel.A = 255;
        }

        // write it out
        sprintf_s(threadData.outFileName, "%s%i.png", outFilePath, frameIndex);
        stbi_write_png(threadData.outFileName, document.outputSizeX, document.outputSizeY, 4, threadData.pixelsU8.data(), document.outputSizeX * 4);

        framesDone++;
    }
    printf("\r100%%\n");

    // report how long it took
    {
        std::chrono::duration<float> seconds = (std::chrono::high_resolution_clock::now() - timeStart);
        float secondsPerFrame = seconds.count() / float(framesTotal);
        printf("Render Time: %0.3f seconds.\n  %0.3f seconds per frame average wall time (more threads make this lower)\n  %0.3f seconds per frame average actual computation time\n", seconds.count(), secondsPerFrame, secondsPerFrame * float(threadsData.size()));
    }

    if (wasError)
    {
        system("pause");
        return 4;
    }

    return 0;
}

/*
TODO:


TITLE SCREEN:
 * get a point, line, triangle, tetrahedron rotating on the screen.
 * TODO: have fill support gradients or make a vertical gradient fill node? using cubic hermite interpolation with y values and x values being colors.
 * make a more interesting than blank white background
 * make this be a specifically named json clip file. we can keep / reuse / improve this as time goes on



? latex support -> give it path to latex. have it make a file, run commands, generate images, load and use images. That would help with equations, but also text.

! flatten checkins for v1

* retest lines3d after you get tetrahedron working

* make some test scene that uses all the features? or meh, use em when you have use of em, just like you'll extend as needed?


* NEXT: the goal is to make the intro screen for simplexplanations2 which is about Kahns algorithm.
 * definitely want to be able to have a slowly rotating 3d tetrahedron. probably want to rotate a 2d triangle and line too. and have a point as well


* be able to change size of rendering at runtime and get rid of the render vs output size too
 * I think this could work by just having a canvas be an entity that can be parented off of, and it's always resized to the target when mering.
 * In this way, I think you can get rid of render/output size and just make that be supported through this canvas mechanism.
\
* add option for dithering before quantizing
 * Blue noise by default but IGN, Bayer and some other options available.
 * Probably could have a setting to over-quantize.
 * Probably also option for resize up using "nearest".
 * Could over quantize with Bayer and up sample with nearest to make retro looking things.

! other subpixel jitter types to implement: white noise, projective blue noise, R2, sobol.
 * may also have some that animate over time to make the noise good over time

! could show an estimated time remaining along with the percent!


* camera needs ortho vs perspective ability and parameters
 * test both!

* parenting and transforms next? after intro screen, should start making the video itself


* make 3d use a reversed z, infinite z projection matrix. document if it's left or right handed



* add a program and version number to the document, and verify it on load

* make it spit out a text file that says the ffmpeg command to combine it into a movie
* make this operate via command line

* profile?

* add object parenting and getting transforms from parents.
 * a transform is probably a parent all it's own. local / global rotation pivot?

 * textures
 * text

 * Parent off of a layer to render into a sublayer which is then merged back into the main image with alpha blending.

 * 3d lines to do fireframes, with 3d transforms. Projection is a parented transform too.
  * do the same with triangles. convert to 2d and render (with zbuffer)
  * no lights in scene = unlit. else, do diffuse only lighting.
  * whitted raytracing
  * path tracing
* Gaussian blur.
* Bloom?
* Tone map.
* cube maps

* there are more 2d sdf's here: https://www.iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm

! generate documentation from schemas?
* should document that +/-50 canvas units is the largest square that fits in the center.

* For aa, could also just render N frames at different (random? Lds?) Subpixel offsets and average them together. simpler interface than resizing.

* be able to have different animation tracks for an object. have a keyframe specify the track number (sorts for applying them, so probably a float)
 * should probably have a bitset of what fields are present in json data.
 * this could be a feature of df_serialize to reflect out this parallel object of bools and also fill it in.

TODO:
* pre multiplied alpha
* anti aliased (SDF? super sampling?). for super sampling, could have a render size and an output size.
? should we multithread this? i think we could... file writes may get heinous, but rendering should be fine with it.

Low priority:
* maybe generate html documentaion?
* could do 3d rendering later (path tracing) also whitted raytracing. can have 3d scenes and have defined lights. unlit if no lights defined.
* other 2d sample sequences: R2, white noise, regular, jittered grid.

TODO: 's for later
* option for different image shrink / grow operations. right now it box filters down and bicubics up.
* audio synth.. wave forms, envelopes, per channel operations, FIR, IIR. karplus strong, etc.
* poisson blending could be cool to paste objects into scenes they arent from.
* Could do effects between frames but it would serialize it. lik ebeing able to blend with the previous frame.
* prefabs for library of reusable objects
 * Prefabs are another list of things.
 * They are a list of entities.
 * Allow file includes in general where an included json could have any of the data a regular file could. Prefabs included but not limited to.
 * When you make an entity of type prefab, it makes objects with a name prefix (name. Or name::) before the name.
 * This lets you make dice etc that are reusable.
* convert to greyscale.
* multiply by color
* lerp between things
* copy?

*/

/*

Notes on the architecture:
* having a sparse json reader means that keyframe data can be described as sparse json that is read. things are automatically key framable.
* having df_serialize means i add a field to the schema then... i can add it to the json and it shows up at runtime.  I can also already use it as keyframe data!
* using macros to define schema means that i can auto expand macros for other uses and turn runtime checks into compile errors. Like wanting a function per entity.
* cubic bezier for interpolating values seems good for linear and non linear blends. can even NOT do blending by making CPs all 0s or all 1s.

*/