#pragma once

#include "schemas/types.h"
#include "math.h"
#include "vectormath.h"
#include "reflectedvectormath.h"

// more sdf's here: https://www.iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm
inline float sdLine(vec2 a, vec2 b, vec2 pixel)
{
    vec2 pa = pixel - a;
    vec2 ba = b - a;
    float h = Clamp(Dot(pa, ba) / Dot(ba, ba), 0.0f, 1.0f);
    return Length(pa - ba * h);
}

inline float sdBox(vec2 pixel, vec2 boxPos, vec2 boxRadius)
{
    vec2 d = Abs(pixel - boxPos) - boxRadius;
    return Length(Max(d, 0.0f)) + std::min(std::max(d[0], d[1]), 0.0f);
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

inline void PixelToCanvas(const Data::Document& document, float pixelX, float pixelY, float& canvasX, float& canvasY)
{
    // +/- 50 in canvas units is the largest square that can fit in the render, centered in the middle of the render.

    int canvasSizeInPixels = (document.renderSizeX >= document.renderSizeY) ? document.renderSizeY : document.renderSizeX;
    int centerPx = document.renderSizeX / 2;
    int centerPy = document.renderSizeY / 2;

    canvasX = 100.0f * float(pixelX - centerPx) / float(canvasSizeInPixels);
    canvasY = -100.0f * float(pixelY - centerPy) / float(canvasSizeInPixels);
}

inline void PixelToCanvas(const Data::Document& document, int pixelX, int pixelY, float& canvasX, float& canvasY)
{
    PixelToCanvas(document, (float)pixelX, (float)pixelY, canvasX, canvasY);
}

inline int CanvasSizeInPixels(const Data::Document& document)
{
    return (document.renderSizeX >= document.renderSizeY) ? document.renderSizeY : document.renderSizeX;
}

inline void CanvasToPixelFloat(const Data::Document& document, float canvasX, float canvasY, float& pixelX, float& pixelY)
{
    int canvasSizeInPixels = CanvasSizeInPixels(document);

    canvasX = canvasX * float(canvasSizeInPixels) / 100.0f;
    canvasY = canvasY * float(canvasSizeInPixels) / 100.0f;

    int centerPx = document.renderSizeX / 2;
    int centerPy = document.renderSizeY / 2;

    pixelX = (float(centerPx) + canvasX);
    pixelY = (float(centerPy) - canvasY);
}

inline float CanvasLengthToPixelLength(const Data::Document& document, float canvasLength)
{
    float px1, px2, py;
    CanvasToPixelFloat(document, 0.0f, 0.0f, px1, py);
    CanvasToPixelFloat(document, canvasLength, 0.0f, px2, py);
    return px2 - px1;
}

inline void CanvasToPixel(const Data::Document& document, float canvasX, float canvasY, int& pixelX, int& pixelY)
{
    float pixelFloatX, pixelFloatY;
    CanvasToPixelFloat(document, canvasX, canvasY, pixelFloatX, pixelFloatY);
    pixelX = int(pixelFloatX);
    pixelY = int(pixelFloatY);
}

inline void GetPixelBoundingBox_TwoPointsRadius(const Data::Document& document, float canvasX1, float canvasY1, float canvasX2, float canvasY2, float canvasRadiusX, float canvasRadiusY, int& pixelMinX, int& pixelMinY, int& pixelMaxX, int& pixelMaxY)
{
    float minX = Min(canvasX1, canvasX2) - canvasRadiusX;
    float minY = Min(canvasY1, canvasY2) - canvasRadiusY;

    float maxX = Max(canvasX1, canvasX2) + canvasRadiusX;
    float maxY = Max(canvasY1, canvasY2) + canvasRadiusY;

    float px1, py1, px2, py2;
    CanvasToPixelFloat(document, minX, minY, px1, py1);
    CanvasToPixelFloat(document, maxX, maxY, px2, py2);

    pixelMinX = (int)floor(Min(px1, px2));
    pixelMinY = (int)floor(Min(py1, py2));
    pixelMaxX = (int)ceil(Max(px1, px2));
    pixelMaxY = (int)ceil(Max(py1, py2));
}

inline void GetPixelBoundingBox_TwoPoints(const Data::Document& document, float canvasX1, float canvasY1, float canvasX2, float canvasY2, int& pixelMinX, int& pixelMinY, int& pixelMaxX, int& pixelMaxY)
{
    float px1, py1, px2, py2;
    CanvasToPixelFloat(document, canvasX1, canvasY1, px1, py1);
    CanvasToPixelFloat(document, canvasX2, canvasY2, px2, py2);

    pixelMinX = (int)floor(Min(px1, px2));
    pixelMinY = (int)floor(Min(py1, py2));
    pixelMaxX = (int)ceil(Max(px1, px2));
    pixelMaxY = (int)ceil(Max(py1, py2));
}

inline void GetPixelBoundingBox_PointRadius(const Data::Document& document, float canvasX, float canvasY, float canvasRadiusX, float canvasRadiusY, int& pixelMinX, int& pixelMinY, int& pixelMaxX, int& pixelMaxY)
{
    GetPixelBoundingBox_TwoPoints(document, canvasX - canvasRadiusX, canvasY - canvasRadiusY, canvasX + canvasRadiusX, canvasY + canvasRadiusY, pixelMinX, pixelMinY, pixelMaxX, pixelMaxY);
}

inline void GetCanvasExtents(const Data::Document& document, float& canvasMinX, float& canvasMinY, float& canvasMaxX, float& canvasMaxY)
{
    float cx1, cy1, cx2, cy2;
    PixelToCanvas(document, 0, 0, cx1, cy1);
    PixelToCanvas(document, document.renderSizeX - 1, document.renderSizeY - 1, cx2, cy2);
    canvasMinX = Min(cx1, cx2);
    canvasMinY = Min(cy1, cy2);
    canvasMaxX = Max(cx1, cx2);
    canvasMaxY = Max(cy1, cy2);
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

inline Data::ColorPMA CubicHermite(const Data::ColorPMA& A, const Data::ColorPMA& B, const Data::ColorPMA& C, const Data::ColorPMA& D, float t)
{
    Data::ColorPMA ret;
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

inline Data::Color operator * (const Data::Color& A, const Data::Color& B)
{
    Data::Color ret;
    ret.R = A.R * B.R;
    ret.G = A.G * B.G;
    ret.B = A.B * B.B;
    ret.A = A.A * B.A;
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

inline Data::ColorPMA operator + (const Data::ColorPMA& A, const Data::ColorPMA& B)
{
    Data::ColorPMA ret;
    ret.R = A.R + B.R;
    ret.G = A.G + B.G;
    ret.B = A.B + B.B;
    ret.A = A.A + B.A;
    return ret;
}

inline Data::ColorPMA operator * (const Data::ColorPMA& A, const Data::ColorPMA& B)
{
    Data::ColorPMA ret;
    ret.R = A.R * B.R;
    ret.G = A.G * B.G;
    ret.B = A.B * B.B;
    ret.A = A.A * B.A;
    return ret;
}

inline Data::ColorPMA operator * (const Data::ColorPMA& A, float B)
{
    Data::ColorPMA ret;
    ret.R = A.R * B;
    ret.G = A.G * B;
    ret.B = A.B * B;
    ret.A = A.A * B;
    return ret;
}

inline Data::ColorPMA operator / (const Data::ColorPMA& A, float B)
{
    Data::ColorPMA ret;
    ret.R = A.R / B;
    ret.G = A.G / B;
    ret.B = A.B / B;
    ret.A = A.A / B;
    return ret;
}

inline Data::ColorPMA ToPremultipliedAlpha(const Data::Color& color)
{
    Data::ColorPMA ret;
    ret.R = color.R * color.A;
    ret.G = color.G * color.A;
    ret.B = color.B * color.A;
    ret.A = color.A;
    return ret;
}

inline Data::Color FromPremultipliedAlpha(const Data::ColorPMA& color)
{
    if (color.A == 0.0f)
        return Data::Color{ 0.0f, 0.0f, 0.0f, 0.0f };

    Data::Color ret;
    ret.R = color.R / color.A;
    ret.G = color.G / color.A;
    ret.B = color.B / color.A;
    ret.A = color.A;
    return ret;
}

inline Data::ColorPMA Blend(const Data::ColorPMA& out, const Data::ColorPMA& in)
{
    Data::ColorPMA ret;
    ret.R = in.R + out.R * (1.0f - in.A);
    ret.G = in.G + out.G * (1.0f - in.A);
    ret.B = in.B + out.B * (1.0f - in.A);
    ret.A = in.A + out.A * (1.0f - in.A);
    return ret;
}

void Resize(std::vector<Data::Color>& pixels, int sizeX, int sizeY, int desiredSizeX, int desiredSizeY);
void Resize(std::vector<Data::ColorPMA>& pixels, int sizeX, int sizeY, int desiredSizeX, int desiredSizeY);

bool MakeJitterSequence(Data::Document& document);

void DrawLine(const Data::Document& document, std::vector<Data::ColorPMA>& pixels, const Data::Point2D& A, const Data::Point2D& B, float width, const Data::ColorPMA& color);

inline void Fill(std::vector<Data::ColorPMA>& pixels, const Data::Color& color)
{
    Data::ColorPMA colorPMA = ToPremultipliedAlpha(color);

    // if the color is fully transparent, it's a no-op
    if (color.A == 0.0f)
        return;

    // if the color is opaque just do a fill
    if (color.A >= 1.0f)
    {
        std::fill(pixels.begin(), pixels.end(), colorPMA);
        return;
    }

    // otherwise, do a blend operation
    for (Data::ColorPMA& pixel : pixels)
        pixel = Blend(pixel, colorPMA);
}
