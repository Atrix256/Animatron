#pragma once

#include "schemas/types.h"
#include "math.h"

// TODO: put this vector math in it's own file?

typedef std::array<float, 2> vec2;

template <size_t N>
std::array<float, N> operator * (const std::array<float, N>& A,float B)
{
    std::array<float, N> ret;
    for (size_t i = 0; i < N; ++i)
        ret[i] = A[i] * B;
    return ret;
}

template <size_t N>
std::array<float, N> operator - (const std::array<float, N>& A, const std::array<float, N>& B)
{
    std::array<float, N> ret;
    for (size_t i = 0; i < N; ++i)
        ret[i] = A[i] - B[i];
    return ret;
}

template <size_t N>
float Dot(const std::array<float, N>& A, const std::array<float, N>& B)
{
    float ret = 0.0f;
    for (size_t i = 0; i < N; ++i)
        ret += A[i] * B[i];
    return ret;
}

template <size_t N>
float Length(const std::array<float, N>& A)
{
    return (float)sqrt(Dot(A, A));
}



inline float sdLine(vec2 a, vec2 b, vec2 p)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = Clamp(Dot(pa, ba) / Dot(ba, ba), 0.0f, 1.0f);
    return Length(pa - ba * h);
}

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

    int canvasSizeInPixels = (document.renderSizeX >= document.renderSizeY) ? document.renderSizeY : document.renderSizeX;
    int centerPx = document.renderSizeX / 2;
    int centerPy = document.renderSizeY / 2;

    canvasX = 100.0f * float(pixelX - centerPx) / float(canvasSizeInPixels);
    canvasY = 100.0f * float(pixelY - centerPy) / float(canvasSizeInPixels);
}

inline void CanvasToPixel(const Data::Document& document, float canvasX, float canvasY, int& pixelX, int& pixelY)
{
    int canvasSizeInPixels = (document.renderSizeX >= document.renderSizeY) ? document.renderSizeY : document.renderSizeX;

    canvasX = canvasX * float(canvasSizeInPixels) / 100.0f;
    canvasY = canvasY * float(canvasSizeInPixels) / 100.0f;

    int centerPx = document.renderSizeX / 2;
    int centerPy = document.renderSizeY / 2;

    pixelX = int(canvasX + float(centerPx));
    pixelY = int(canvasY + float(centerPy));
}

inline Data::Color CubicHermite(const Data::Color& A, const Data::Color& B, const Data::Color& C, const Data::Color& D, float t)
{
    Data::Color ret;
    ret.R = CubicHermite(A.R, B.R, C.R, D.R, t);
    ret.G = CubicHermite(A.G, B.G, C.G, D.G, t);
    ret.B = CubicHermite(A.B, B.B, C.B, D.B, t);
    ret.A = CubicHermite(A.A, B.A, C.A, D.A, t);
    return ret;
}

inline Data::Color operator + (const Data::Color& A, const Data::Color& B)
{
    Data::Color ret;
    ret.R = A.R + B.R;
    ret.G = A.G + B.G;
    ret.B = A.B + B.B;
    ret.A = A.A + B.A;
    return ret;
}

inline Data::Color operator * (const Data::Color& A, float B)
{
    Data::Color ret;
    ret.R = A.R * B;
    ret.G = A.G * B;
    ret.B = A.B * B;
    ret.A = A.A * B;
    return ret;
}

inline Data::Color operator / (const Data::Color& A, float B)
{
    Data::Color ret;
    ret.R = A.R / B;
    ret.G = A.G / B;
    ret.B = A.B / B;
    ret.A = A.A / B;
    return ret;
}

void Resize(std::vector<Data::Color>& pixels, int sizeX, int sizeY, int desiredSizeX, int desiredSizeY);
