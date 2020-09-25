#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>

#include "schemas/types.h"
#include "schemas/json.h"

#include "color.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

void ToPixelCoordinates(const Data::Document& document, float x, float y, int& px, int& py)
{
    float aspectRatio = float(document.sizeX) / float(document.sizeY);
    // TODO: handle aspect ratio

    // convert to uv (0 to 1)
    x /= 100.0f;
    y /= 100.0f;

    y *= aspectRatio;

    // put (0,0) in the middle of the image
    x += 0.5f;
    y += 0.5f;

    // convert to pixels
    px = int(float(document.sizeX) * x);
    py = int(float(document.sizeY) * y);
}

struct EntityTimelineKeyframe
{
    float time = 0.0f;
    std::array<float,2> blendControlPoints = { 1.0f / 3.0f, 2.0f / 3.0f };
    Data::EntityVariant entity;
};

struct EntityTimeline
{
    float createTime = 0.0f;
    float destroyTime = -1.0f;
    std::vector<EntityTimelineKeyframe> keyFrames;
};

void HandleEntity_Clear(
    const Data::Document& document,
    std::vector<Data::Color>& pixels,
    float blendPercent,
    const Data::EntityClear& A,
    const Data::EntityClear* B
)
{
    // TODO: should rename to fill. if alpha isn't 1.0, should do an alpha blend instead of a straight fill.

    Data::EntityClear fill = A;
    if (B != nullptr)
    {
        fill.color.R = Lerp(A.color.R, B->color.R, blendPercent);
        fill.color.G = Lerp(A.color.G, B->color.G, blendPercent);
        fill.color.B = Lerp(A.color.B, B->color.B, blendPercent);
        fill.color.A = Lerp(A.color.A, B->color.A, blendPercent);
    }

    std::fill(pixels.begin(), pixels.end(), fill.color);
}

void HandleEntity_Circle(
    const Data::Document& document,
    std::vector<Data::Color>& pixels,
    float blendPercent,
    const Data::EntityCircle& A,
    const Data::EntityCircle* B
)
{
    Data::EntityCircle circle = A;
    if (B != nullptr)
    {
        circle.center.X = Lerp(A.center.X, B->center.X, blendPercent);
        circle.center.Y = Lerp(A.center.Y, B->center.Y, blendPercent);
        circle.color.R = Lerp(A.color.R, B->color.R, blendPercent);
        circle.color.G = Lerp(A.color.G, B->color.G, blendPercent);
        circle.color.B = Lerp(A.color.B, B->color.B, blendPercent);
        circle.color.A = Lerp(A.color.A, B->color.A, blendPercent);
        circle.innerRadius = Lerp(A.innerRadius, B->innerRadius, blendPercent);
        circle.outerRadius = Lerp(A.outerRadius, B->outerRadius, blendPercent);
    }

    // TODO: maybe could have an aspect ratio on the circle to squish it and stretch it?

    // TODO: you are calculating distance in pixel space, not the coordinates, that's why the circle is staying a circle.

    // TODO: make a helper function!
    float innerRadiusPx = circle.innerRadius * float(document.sizeX) / 100.0f;
    float outerRadiusPx = circle.outerRadius * float(document.sizeY) / 100.0f;

    int centerPx = 0;
    int centerPy = 0;
    ToPixelCoordinates(document, circle.center.X, circle.center.Y, centerPx, centerPy);

    // TODO: this could be done better - like by finding the bounding box
    // TODO: handle alpha blending
    for (int iy = 0; iy < document.sizeY; ++iy)
    {
        float distY = abs(float(iy - centerPy));
        Data::Color* pixel = &pixels[iy * document.sizeX];
        for (int ix = 0; ix < document.sizeX; ++ix)
        {
            float distX = abs(float(ix - centerPx));
            float dist = sqrt(distX*distX + distY * distY);
            dist -= innerRadiusPx;
            if (dist > 0.0f && dist <= outerRadiusPx)
                *pixel = circle.color;
            pixel++;
        }
    }

    /*
    std::fill(pixels.begin(), pixels.end(), fill.color);
    */

    // TODO: draw a circle! need to convert coordinates and such

    // TODO: how to do anti aliasing? maybe render too large and shrink?
}

bool GenerateFrame(const Data::Document& document, const std::unordered_map<std::string, EntityTimeline>& entityTimelines, std::vector<Data::Color>& pixels, int frameIndex)
{
    // setup for the frame
    float frameTime = float(frameIndex) / float(document.FPS);
    pixels.resize(document.sizeX*document.sizeY);
    std::fill(pixels.begin(), pixels.end(), Data::Color{ 0.0f, 0.0f, 0.0f, 0.0f });

    // TODO: need some kind of ordering to the processing entities. the map is unordered. Z order?

    // process the entities
    for (auto& pair : entityTimelines)
    {
        // skip any entity that doesn't currently exist
        const EntityTimeline& timeline = pair.second;
        if (frameTime < timeline.createTime || (timeline.destroyTime >= 0.0f && frameTime > timeline.destroyTime))
            continue;

        int cursorIndex = 0;
        while (cursorIndex + 1 < timeline.keyFrames.size() && timeline.keyFrames[cursorIndex+1].time < frameTime)
            cursorIndex++;

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

        // do the entity action
        switch (timeline.keyFrames[0].entity._index)
        {
            case Data::EntityVariant::c_index_clear:
            {
                HandleEntity_Clear(
                    document,
                    pixels,
                    blendPercent,
                    entity1.clear,
                    entity2 ? &entity2->clear : nullptr
                );
                break;
            }
            case Data::EntityVariant::c_index_circle:
            {
                HandleEntity_Circle(
                    document,
                    pixels,
                    blendPercent,
                    entity1.circle,
                    entity2 ? &entity2->circle : nullptr
                );
                break;
            }
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
    std::unordered_map<std::string, EntityTimeline> entityTimelines;
    for (const Data::Entity& entity : document.entities)
    {
        EntityTimeline newTimeline;
        newTimeline.createTime = entity.createTime;
        newTimeline.destroyTime = entity.destroyTime;

        EntityTimelineKeyframe newKeyFrame;
        newKeyFrame.time = entity.createTime;
        newKeyFrame.entity = entity.data;
        newTimeline.keyFrames.push_back(newKeyFrame);

        entityTimelines[entity.id] = newTimeline;
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

        auto it = entityTimelines.find(keyFrame.entityId);
        if (it == entityTimelines.end())
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

* what units of measurement for drawing things?
 * I like "aspect ratio corrected UV" but also that is too small, so maybe like... *100? so percent instead of uv?

* rename project / solution to animatron. typod

* df_serialize desires
 * polymorphic types. already thinking about having an array of whatever types of things
 * fixed sized arrays. For colors, for instance, i want a size of 4. maybe have -1 be dynamic sized?
 * i want uint8_t. probably more basic types supported out of the box... maybe have a default templated thing, and specifics added as needed?
 * Need reflection of some kind for setting field name values. like "set field to json", where you give a field name and a json string.
 * could be better about errors, like a static assert on a templated class to say that a type is not supported?

TODO:
* pre multiplied alpha
* anti aliased (SDF? super sampling?). for super sampling, could have a render size and an output size.
? do we need a special time type? unsure if floats in seconds will cut it?
? should we multithread this? i think we could... file writes may get heinous, but rendering should be fine with it.
* probably should have non instant events -> blending non linearly etc
* should we put an ordering on things? like if you have 2 clears, how's it decide which to do?

Low priority:
* maybe generate html documentaion?
* could do 3d rendering later (path tracing)

*/