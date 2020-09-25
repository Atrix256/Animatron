#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>

// TODO: probably should put these into a different folder than just where the schemas live
#include "schemas/types.h"
#include "schemas/json.h"
#include "schemas/lerp.h"

#include "color.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

struct EntityTimelineKeyframe
{
    float time = 0.0f;
    std::array<float,2> blendControlPoints = { 1.0f / 3.0f, 2.0f / 3.0f };
    Data::EntityVariant entity;
};

struct EntityTimeline
{
    float zorder = 0.0f;
    float createTime = 0.0f;
    float destroyTime = -1.0f;
    std::vector<EntityTimelineKeyframe> keyFrames;
};

void HandleEntity_EntityFill(const Data::Document& document, std::vector<Data::Color>& pixels, const Data::EntityFill& fill)
{
    // TODO: probably should switch to alpha blended color in the pixels?
    // TODO: if alpha isn't 1.0, should do an alpha blend instead of a straight fill.
    std::fill(pixels.begin(), pixels.end(), fill.color);
}

void PixelToCanvas(const Data::Document& document, int pixelX, int pixelY, float& canvasX, float& canvasY)
{
    // +/- 50 in canvas units is the largest square that can fit in the render, centered in the middle of the render.

    int canvasSizeInPixels = (document.sizeX >= document.sizeY) ? document.sizeY : document.sizeX;
    int centerPx = document.sizeX / 2;
    int centerPy = document.sizeY / 2;

    canvasX = 100.0f * float(pixelX - centerPx) / float(canvasSizeInPixels);
    canvasY = 100.0f * float(pixelY - centerPy) / float(canvasSizeInPixels);
}

void HandleEntity_EntityCircle(const Data::Document& document, std::vector<Data::Color>& pixels, const Data::EntityCircle& circle)
{
    // TODO: maybe could have an aspect ratio on the circle to squish it and stretch it? Nah... parent it off a transform!

    // TODO: this could be done better - like by finding the bounding box
    // TODO: handle alpha blending
    float canvasMinX, canvasMinY, canvasMaxX, canvasMaxY;
    PixelToCanvas(document, 0, 0, canvasMinX, canvasMinY);
    PixelToCanvas(document, document.sizeX - 1, document.sizeY - 1, canvasMaxX, canvasMaxY);
    for (int iy = 0; iy < document.sizeY; ++iy)
    {
        float percentY = float(iy) / float(document.sizeY - 1);
        float canvasY = Lerp(canvasMinY, canvasMaxY, percentY);
        float distY = abs(canvasY - circle.center.Y);
        Data::Color* pixel = &pixels[iy * document.sizeX];
        for (int ix = 0; ix < document.sizeX; ++ix)
        {
            float percentX = float(ix) / float(document.sizeX - 1);
            float canvasX = Lerp(canvasMinX, canvasMaxX, percentX);
            float distX = abs(canvasX - circle.center.X);

            float dist = sqrt(distX*distX + distY * distY);
            dist -= circle.innerRadius;
            if (dist > 0.0f && dist <= circle.outerRadius)
                *pixel = circle.color;
            pixel++;
        }
    }
    // TODO: how to do anti aliasing? maybe render too large and shrink?
}

bool GenerateFrame(const Data::Document& document, const std::vector<const EntityTimeline*>& entityTimelines, std::vector<Data::Color>& pixels, int frameIndex)
{
    // setup for the frame
    float frameTime = float(frameIndex) / float(document.FPS);
    pixels.resize(document.sizeX*document.sizeY);
    std::fill(pixels.begin(), pixels.end(), Data::Color{ 0.0f, 0.0f, 0.0f, 0.0f });

    // process the entities
    for (const EntityTimeline* timeline_ : entityTimelines)
    {
        // skip any entity that doesn't currently exist
        const EntityTimeline& timeline = *timeline_;
        if (frameTime < timeline.createTime || (timeline.destroyTime >= 0.0f && frameTime > timeline.destroyTime))
            continue;

        // find where we are in the time line
        int cursorIndex = 0;
        while (cursorIndex + 1 < timeline.keyFrames.size() && timeline.keyFrames[cursorIndex+1].time < frameTime)
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

                float CPA = 0.0f;
                float CPB = timeline.keyFrames[cursorIndex + 1].blendControlPoints[0];
                float CPC = timeline.keyFrames[cursorIndex + 1].blendControlPoints[1];
                float CPD = 1.0f;

                blendPercent = CubicBezierInterpolation(CPA, CPB, CPC, CPD, t);
            }

            // Get the entity(ies) involved
            const Data::EntityVariant& entity1 = timeline.keyFrames[cursorIndex].entity;
            const Data::EntityVariant* entity2 = (cursorIndex + 1 < timeline.keyFrames.size())
                ? &timeline.keyFrames[cursorIndex + 1].entity
                : nullptr;

            // Do the lerp between keyframe entities
            Data::EntityVariant entity;
            Lerp(entity, *entity2, entity, blendPercent);
        }
        // otherwise we are beyond the last key frame, so just set the value
        else
        {
            entity = timeline.keyFrames[cursorIndex].entity;
        }

        // do the entity action
        switch (timeline.keyFrames[0].entity._index)
        {
            #include "df_serialize/df_serialize/_common.h"
            #define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
                case Data::EntityVariant::c_index_##_NAME: HandleEntity_##_TYPE(document, pixels, entity.##_NAME); break;
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
    if (!ReadFromJSONFile(document, fileName))
        return 1;

    // make a timeline for each entity by just starting with the entity definition
    std::unordered_map<std::string, EntityTimeline> entityTimelinesMap;
    for (const Data::Entity& entity : document.entities)
    {
        EntityTimeline newTimeline;
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

        // TODO: shouldn't have to do this! maybe make ReadFromJSONBuffer (also?) read from a string or char*. probably have both call into the constt char* version
        std::vector<char> temp;
        temp.resize(keyFrame.newValue.size() + 1);
        strcpy(temp.data(), keyFrame.newValue.c_str());
        if (!ReadFromJSONBuffer(newKeyFrame.entity, temp))
        {
            printf("Could not read json data for keyframe! entity %s, time %f.\n", keyFrame.entityId.c_str(), keyFrame.time);
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

    // TODO: use OMP to go multithreaded. probably need a per thread structure to hold pixels (to avoid alloc churn!) etc.

    // Render and write out each frame
    printf("Rendering clip...\n");
    char outFileName[1024];
    std::vector<Data::Color> pixels;
    std::vector<Data::ColorU8> pixelsU8;
    int framesTotal = int(document.duration * float(document.FPS));
    int lastPercent = -1;
    for (int frameIndex = 0; frameIndex < framesTotal; ++frameIndex)
    {
        // report progress
        int percent = int(100.0f * float(frameIndex) / float(framesTotal-1));
        if (lastPercent != percent)
        {
            lastPercent = percent;
            printf("\r%i%%", lastPercent);
        }

        // render a frame
        if (!GenerateFrame(document, entityTimelines, pixels, frameIndex))
            return 4;

        // Convert it to RGBAU8
        ColorToColorU8(pixels, pixelsU8);

        // write it out
        sprintf_s(outFileName, "%s%i.png", outFilePath, frameIndex);
        stbi_write_png(outFileName, document.sizeX, document.sizeY, 4, pixelsU8.data(), document.sizeX * 4);
    }
    printf("\r100%%\n");

    return 0;
}

/*
TODO:

* probably should put entities in a different file, leave this simple? could auto generate the functions expected, and the calls here. put entities into it's own schema header or something.

! generate documentation from schemas?
* should document that +/-50 canvas units is the largest square that fits in the center.

* rename project / solution to animatron. typod


TODO:
* pre multiplied alpha
* anti aliased (SDF? super sampling?). for super sampling, could have a render size and an output size.
? should we multithread this? i think we could... file writes may get heinous, but rendering should be fine with it.

Low priority:
* maybe generate html documentaion?
* could do 3d rendering later (path tracing) also whitted raytracing. can have 3d scenes and have defined lights. unlit if no lights defined.

*/