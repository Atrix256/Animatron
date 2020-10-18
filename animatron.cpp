#include "animatron.h"
#include "utils.h"
#include "cas.h"
#include "entities.h"

#include <algorithm>
#include <direct.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

bool ValidateAndFixupDocument(Data::Document& document)
{
    // make sure the build folder exists
    _mkdir("build");

    // verify
    if (document.program != "animatron")
    {
        printf("Invalid animatron file!\n");
        return false;
    }

    if (document.config.program != "animatronconfig")
    {
        printf("Invalid animatron config file!\n");
        return false;
    }

    // version fixup
    if (document.versionMajor != c_documentVersionMajor || document.versionMinor != c_documentVersionMinor)
    {
        printf("Wrong version number: %i.%i, not %i.%i\n", document.versionMajor, document.versionMinor, c_documentVersionMajor, c_documentVersionMinor);
        return false;
    }

    if (document.config.versionMajor != c_configVersionMajor || document.config.versionMinor != c_configVersionMinor)
    {
        printf("Wrong config version number: %i.%i, not %i.%i\n", document.config.versionMajor, document.config.versionMinor, c_configVersionMajor, c_configVersionMinor);
        return false;
    }

    // give names to any entities which don't have names
    {
        for (size_t index = 0; index < document.entities.size(); ++index)
        {
            if (document.entities[index].id.empty())
            {
                char buffer[256];
                sprintf_s(buffer, "__ entity %zu", index);
                document.entities[index].id = buffer;
            }
        }
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

    // data interpretation and fixup
    {
        if (document.renderSizeX == 0)
            document.renderSizeX = document.outputSizeX;
        if (document.renderSizeY == 0)
            document.renderSizeY = document.outputSizeY;
        if (document.samplesPerPixel < 1)
            document.samplesPerPixel = 1;
    }

    // Make the sampling jitter sequence for the document
    MakeJitterSequence(document);

    // Load blue noise texture for dithering
    if (document.blueNoiseDither)
    {
        int blueNoiseComponents = 0;
        stbi_uc* pixels = stbi_load("internal/BlueNoiseRGBA.png", &document.blueNoiseWidth, &document.blueNoiseHeight, &blueNoiseComponents, 4);
        if (pixels == nullptr || document.blueNoiseWidth == 0 || document.blueNoiseHeight == 0)
        {
            printf("Could not load internal/BlueNoiseRGBA.png");
            return false;
        }

        document.blueNoisePixels.resize(document.blueNoiseWidth * document.blueNoiseHeight);
        memcpy((unsigned char*)&document.blueNoisePixels[0], pixels, document.blueNoiseWidth * document.blueNoiseHeight * 4);

        stbi_image_free(pixels);
    }

    // Do one time initialization of entities
    {
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
                    return false;
                }
            }
            if (error)
            {
                printf("entity %s failed to initialize.\n", entity.id.c_str());
                return false;
            }
        }
    }

    // initialize CAS. it's ok if this is called multiple times.
    if (!CAS::Get().Init())
    {
        printf("Could not init CAS\n");
        return 1;
    }

    // make a timeline for each entity by just starting with the entity definition
    
    for (const Data::Entity& entity : document.entities)
    {
        Data::RuntimeEntityTimeline newTimeline;
        newTimeline.id = entity.id;
        newTimeline.zorder = entity.zorder;
        newTimeline.createTime = entity.createTime;
        newTimeline.destroyTime = entity.destroyTime;

        Data::RuntimeEntityTimelineKeyframe newKeyFrame;
        newKeyFrame.time = entity.createTime;
        newKeyFrame.entity = entity;
        newTimeline.keyFrames.push_back(newKeyFrame);

        document.runtimeEntityTimelinesMap[entity.id] = newTimeline;
    }

    // process each key frame to fill out the timelines further
    for (const Data::KeyFrame& keyFrame : document.keyFrames)
    {
        auto it = document.runtimeEntityTimelinesMap.find(keyFrame.entityId);
        if (it == document.runtimeEntityTimelinesMap.end())
        {
            printf("Could not find entity %s for keyframe!\n", keyFrame.entityId.c_str());
            system("pause");
            return 1;
        }

        // ignore events outside the lifetime of the entity
        if ((keyFrame.time < it->second.createTime) || (it->second.destroyTime >= 0.0f && keyFrame.time > it->second.destroyTime))
            continue;

        // make a new key frame entry
        Data::RuntimeEntityTimelineKeyframe newKeyFrame;
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
    {
        for (auto& pair : document.runtimeEntityTimelinesMap)
            document.runtimeEntityTimelines.push_back(&pair.second);

        std::stable_sort(
            document.runtimeEntityTimelines.begin(),
            document.runtimeEntityTimelines.end(),
            [](const Data::RuntimeEntityTimeline* a, const Data::RuntimeEntityTimeline* b)
            {
                return a->zorder < b->zorder;
            }
        );
    }

    return true;
}

bool RenderFrame(const Data::Document& document, int frameIndex, ThreadContext& threadContext, Context& context, int& recycledFrameIndex, size_t& frameHash)
{
    std::vector<Data::ColorPMA>& pixels = threadContext.pixelsPMA;

    // setup for the frame
    float frameTime = (float(frameIndex) / float(document.FPS)) + document.startTime;
    pixels.resize(document.renderSizeX * document.renderSizeY);
    std::fill(pixels.begin(), pixels.end(), Data::ColorPMA{ 0.0f, 0.0f, 0.0f, 0.0f });

    // Get the key frame interpolated state of each entity first, so that they can look at eachother (like 3d objects looking at their camera)
    frameHash = 0;
    Hash(frameHash, document.renderSizeX);
    Hash(frameHash, document.renderSizeY);
    std::unordered_map<std::string, Data::Entity> entityMap;
    {
        for (const Data::RuntimeEntityTimeline* timeline_ : document.runtimeEntityTimelines)
        {
            // skip any entity that doesn't currently exist
            const Data::RuntimeEntityTimeline& timeline = *timeline_;
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

            Hash(frameHash, entity);

            entityMap[timeline.id] = entity;
        }
    }

    // if we have already rendered a frame with this hash, just copy that file
    {
        const FrameCache::FrameData& recycleFrame = context.frameCache.GetFrame(frameHash);
        recycledFrameIndex = recycleFrame.frameIndex;
        if (recycleFrame.frameIndex >= 0)
        {
            threadContext.pixelsU8 = recycleFrame.pixels;
            return true;
        }
    }

    // otherwise, render it again

    // process the entities in zorder
    for (const Data::RuntimeEntityTimeline* timeline_ : document.runtimeEntityTimelines)
    {
        // skip any entity that doesn't currently exist
        const Data::RuntimeEntityTimeline& timeline = *timeline_;
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
                case Data::EntityVariant::c_index_##_NAME: error = ! _TYPE##_Action::DoAction(document, entityMap, pixels, entity, threadContext.threadId); break;
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

    // convert from PMA to non PMA
    threadContext.pixels.resize(threadContext.pixelsPMA.size());
    for (size_t index = 0; index < threadContext.pixelsPMA.size(); ++index)
        threadContext.pixels[index] = FromPremultipliedAlpha(threadContext.pixelsPMA[index]);

    // resize from the rendered size to the output size
    Resize(threadContext.pixels, document.renderSizeX, document.renderSizeY, document.outputSizeX, document.outputSizeY);

    // Do blue noise dithering if we should
    if (document.blueNoiseDither)
    {
        for (size_t iy = 0; iy < document.outputSizeY; ++iy)
        {
            const Data::ColorU8* blueNoiseRow = &document.blueNoisePixels[(iy % document.blueNoiseHeight) * document.blueNoiseWidth];
            Data::Color* destPixel = &threadContext.pixels[iy * document.outputSizeX];

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
    ColorToColorU8(threadContext.pixels, threadContext.pixelsU8);

    // force to opaque if we should
    if (document.forceOpaqueOutput)
    {
        for (Data::ColorU8& pixel : threadContext.pixelsU8)
            pixel.A = 255;
    }

    return true;
}