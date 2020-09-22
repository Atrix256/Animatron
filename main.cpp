#include "df_serialize/test/rapidjson/document.h"
#include "df_serialize/test/rapidjson//error/en.h"

#include "schemas/types.h"

#include "color.h"

#define MAKE_JSON_LOG(...) printf(__VA_ARGS__);
#include "schemas/json.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

void DoClear(const Data::Clear& clear, std::vector<Data::Color>& pixels)
{
    std::fill(pixels.begin(), pixels.end(), clear.color);
}

void GenerateFrame(const Data::Document& document, std::vector<Data::Color>& pixels, int frameIndex)
{
    float frameTime = float(frameIndex) / float(document.FPS);
    pixels.resize(document.sizeX*document.sizeY);

    // copy the entities so we can apply events to them
    std::vector<Data::Clear> clears = document.clears;

    // Process events
    for (const Data::Event& event : document.events)
    {
        if (event.time > frameTime)
            continue;

        // TODO: apply events. use reflection to get offsets to field types or similar. Maybe the event value is JSON that is serialized in? do we need to escape it somehow?
        int ijkl = 0;
    }

    // process the clears
    for (const Data::Clear& clear : clears)
        DoClear(clear, pixels);
}

int main(int argc, char** argv)
{
    // Read the data in
    const char* fileName = "./assets/clip.json";
    const char* outFilePath = "./out/";
    Data::Document document;
    if (!ReadFromJSON(document, fileName))
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
        GenerateFrame(document, pixels, frameIndex);

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

* df_serialize desires
 * polymorphic types. already thinking about having an array of whatever types of things
 * fixed sized arrays. For colors, for instance, i want a size of 4. maybe have -1 be dynamic sized?
 * i want uint8_t. probably more basic types supported out of the box... maybe have a default templated thing, and specifics added as needed?
 * Need reflection of some kind for setting field name values

TODO:
* pre multiplied alpha
* anti aliased
? do we need a special time type? unsure if floats in seconds will cut it?
? should we multithread this? i think we could... file writes may get heinous, but rendering should be fine with it.
* probably should have non instant events -> blending non linearly etc
* should we put an ordering on things? like if you have 2 clears, how's it decide which to do?

Low priority:
* maybe generate html documentaion?
* could do 3d rendering later (path tracing)

*/