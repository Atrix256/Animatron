#pragma once

#include "schemas/types.h"
#include "math.h"

Data::ColorU8 ColorToColorU8(const Data::Color& color)
{
    Data::ColorU8 ret;
    ret.R = (uint8_t)Clamp(LinearToSRGB(color.R) * 256.0f, 0.0f, 255.0f);
    ret.G = (uint8_t)Clamp(LinearToSRGB(color.G) * 256.0f, 0.0f, 255.0f);
    ret.B = (uint8_t)Clamp(LinearToSRGB(color.B) * 256.0f, 0.0f, 255.0f);
    ret.A = (uint8_t)Clamp(LinearToSRGB(color.A) * 256.0f, 0.0f, 255.0f);
    return ret;
}

void ColorToColorU8(const std::vector<Data::Color>& pixels, std::vector<Data::ColorU8>& pixelsU8)
{
    // convert the floating point rendered image to sRGB U8
    pixelsU8.resize(pixels.size());
    for (size_t pixelIndex = 0; pixelIndex < pixels.size(); ++pixelIndex)
        pixelsU8[pixelIndex] = ColorToColorU8(pixels[pixelIndex]);
}

