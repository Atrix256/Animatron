#include "df_serialize/test/rapidjson/document.h"
#include "df_serialize/test/rapidjson/error/en.h"

#include "schemas/types.h"
#include "color.h"

#define MAKE_JSON_LOG(...) printf(__VA_ARGS__);
#include "schemas/json.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

bool GenerateFrame(const Data::Document& document, std::vector<Data::Color>& pixels, int frameIndex)
{
    float frameTime = float(frameIndex) / float(document.FPS);
    pixels.resize(document.sizeX*document.sizeY);
    std::fill(pixels.begin(), pixels.end(), Data::Color{ 0.0f, 0.0f, 0.0f, 0.0f });

    // copy the entities so we can apply events to them
    std::vector<Data::Entity> entities;
    for (const Data::Entity& entity : document.entities)
    {
        if (frameTime >= entity.createTime && (entity.destroyTime < 0.0f || frameTime < entity.destroyTime))
            entities.push_back(entity);
    }

    // Process events
    for (const Data::Event& event : document.events)
    {
        if (event.time > frameTime)
            continue;

        // TODO: apply events. use reflection to get offsets to field types or similar. Maybe the event value is JSON that is serialized in? do we need to escape it somehow?
        int ijkl = 0;
    }

    // process the entities
    for (const Data::Entity& entity : entities)
    {
        switch (entity.data._index)
        {
            case Data::EntityVariant::c_index_clear:
            {
                std::fill(pixels.begin(), pixels.end(), entity.data.clear.color);
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
        if (!GenerateFrame(document, pixels, frameIndex))
            break;

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

* rename event to keyframe.
 * it gives an object name and a serialization string for that keyframe
 * maybe have a way of knowing which fields were present when serializing? so we know those are the ones to lerp
  * actually probably not... just start from the first event and go to the last, and partially seralize over the top to get the next key frame. maybe?
 * for interpolation, give coefficients to a polynomial. probably a cubic bezier, in bernstein basis form, would be best (easiest to understand)

* rename to animatron

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