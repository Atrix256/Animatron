#pragma once

#include "schemas/types.h"
#include "math.h"

inline Data::ColorU8 ColorToColorU8(const Data::Color& color)
{
    Data::ColorU8 ret;
    ret.R = (uint8_t)Clamp(LinearToSRGB(color.R) * 256.0f, 0.0f, 255.0f);
    ret.G = (uint8_t)Clamp(LinearToSRGB(color.G) * 256.0f, 0.0f, 255.0f);
    ret.B = (uint8_t)Clamp(LinearToSRGB(color.B) * 256.0f, 0.0f, 255.0f);
    ret.A = (uint8_t)Clamp(LinearToSRGB(color.A) * 256.0f, 0.0f, 255.0f);
    return ret;
}

inline void ColorToColorU8(const std::vector<Data::Color>& pixels, std::vector<Data::ColorU8>& pixelsU8)
{
    // convert the floating point rendered image to sRGB U8
    pixelsU8.resize(pixels.size());
    for (size_t pixelIndex = 0; pixelIndex < pixels.size(); ++pixelIndex)
        pixelsU8[pixelIndex] = ColorToColorU8(pixels[pixelIndex]);
}

inline void PixelToCanvas(const Data::Document& document, int pixelX, int pixelY, float& canvasX, float& canvasY)
{
    // +/- 50 in canvas units is the largest square that can fit in the render, centered in the middle of the render.

    int canvasSizeInPixels = (document.sizeX >= document.sizeY) ? document.sizeY : document.sizeX;
    int centerPx = document.sizeX / 2;
    int centerPy = document.sizeY / 2;

    canvasX = 100.0f * float(pixelX - centerPx) / float(canvasSizeInPixels);
    canvasY = 100.0f * float(pixelY - centerPy) / float(canvasSizeInPixels);
}

inline void CanvasToPixel(const Data::Document& document, float canvasX, float canvasY, int& pixelX, int& pixelY)
{
    // TODO: test to make sure this works ok
    int canvasSizeInPixels = (document.sizeX >= document.sizeY) ? document.sizeY : document.sizeX;

    canvasX = canvasX * float(canvasSizeInPixels) / 100.0f;
    canvasY = canvasY * float(canvasSizeInPixels) / 100.0f;

    int centerPx = document.sizeX / 2;
    int centerPy = document.sizeY / 2;

    pixelX = int(canvasX + float(centerPx));
    pixelY = int(canvasY + float(centerPy));
}
