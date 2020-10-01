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

template <size_t N>
float DistanceUnitTorroidal(const std::array<float, N>& A, const std::array<float, N>& B)
{
    float ret = 0.0f;
    for (size_t i = 0; i < N; ++i)
    {
        float dist = abs(A[i] - B[i]);
        if (dist > 0.5f)
            dist = 1.0f - dist;
        ret += dist * dist;
    }
    return (float)sqrt(ret);
}





// ------- Reflected types math

inline Data::Point4D GetMatrixColumn(const Data::Matrix4x4& mtx, int index)
{
    switch (index)
    {
        case 0: return Data::Point4D{ mtx.X.X, mtx.Y.X, mtx.Z.X, mtx.W.X };
        case 1: return Data::Point4D{ mtx.X.Y, mtx.Y.Y, mtx.Z.Y, mtx.W.Y };
        case 2: return Data::Point4D{ mtx.X.Z, mtx.Y.Z, mtx.Z.Z, mtx.W.Z };
        case 3:
        default:
            return Data::Point4D{ mtx.X.W, mtx.Y.W, mtx.Z.W, mtx.W.W };
    }
}

inline Data::Point4D& GetMatrixRow(Data::Matrix4x4& mtx, int index)
{
    switch (index)
    {
        case 0: return mtx.X;
        case 1: return mtx.Y;
        case 2: return mtx.Z;
        case 3: 
        default:
            return mtx.W;
    }
}

inline const Data::Point4D& GetMatrixRow(const Data::Matrix4x4& mtx, int index)
{
    switch (index)
    {
        case 0: return mtx.X;
        case 1: return mtx.Y;
        case 2: return mtx.Z;
        case 3: 
        default:
            return mtx.W;
    }
}

inline float& GetPointElement(Data::Point4D& point, int index)
{
    switch (index)
    {
        case 0: return point.X;
        case 1: return point.Y;
        case 2: return point.Z;
        case 3:
        default:
            return point.W;
    }
}

inline float GetPointElement(const Data::Point4D& point, int index)
{
    switch (index)
    {
        case 0: return point.X;
        case 1: return point.Y;
        case 2: return point.Z;
        case 3: 
        default:
            return point.W;
    }
}

inline float Dot(const Data::Point4D& A, const Data::Point4D& B)
{
    return A.X * B.X + A.Y * B.Y + A.Z * B.Z + A.W * B.W;
}

inline float Dot(const Data::Point3D& A, const Data::Point3D& B)
{
    return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
}

inline Data::Matrix4x4 Multiply(const Data::Matrix4x4& A, const Data::Matrix4x4& B)
{
    Data::Matrix4x4 ret;
    for (int i = 0; i < 4; ++i)
    {
        const Data::Point4D rowA = GetMatrixRow(A, i);
        Data::Point4D& rowDest = GetMatrixRow(ret, i);
        for (int j = 0; j < 4; ++j)
        {
            const Data::Point4D& colB = GetMatrixColumn(B, j);
            GetPointElement(rowDest, j) = Dot(rowA, colB);
        }
    }
    return ret;
}

inline const Data::Matrix4x4 RotationX(float x)
{
    float cosTheta = (float)cos(x);
    float sinTheta = (float)sin(x);

    Data::Matrix4x4 ret;
    ret.X = Data::Point4D{ 1.0f, 0.0f, 0.0f, 0.0f };
    ret.Y = Data::Point4D{ 0.0f, cosTheta, -sinTheta, 0.0f };
    ret.Z = Data::Point4D{ 0.0f, sinTheta, cosTheta, 0.0f };
    ret.W = Data::Point4D{ 0.0f, 0.0f, 0.0f, 1.0f };
    return ret;
}

inline const Data::Matrix4x4 RotationY(float x)
{
    float cosTheta = (float)cos(x);
    float sinTheta = (float)sin(x);

    Data::Matrix4x4 ret;
    ret.X = Data::Point4D{ cosTheta, 0.0f, sinTheta, 0.0f };
    ret.Y = Data::Point4D{ 0.0f, 1.0f, 0.0f, 0.0f };
    ret.Z = Data::Point4D{ -sinTheta, 0.0f, cosTheta, 0.0f };
    ret.W = Data::Point4D{ 0.0f, 0.0f, 0.0f, 1.0f };
    return ret;
}

inline const Data::Matrix4x4 RotationZ(float x)
{
    float cosTheta = (float)cos(x);
    float sinTheta = (float)sin(x);

    Data::Matrix4x4 ret;
    ret.X = Data::Point4D{ cosTheta, -sinTheta, 0.0f, 0.0f };
    ret.Y = Data::Point4D{ sinTheta, cosTheta, 0.0f, 0.0f };
    ret.Z = Data::Point4D{ 0.0f, 0.0f, 1.0f, 0.0f };
    ret.W = Data::Point4D{ 0.0f, 0.0f, 0.0f, 1.0f };
    return ret;
}

inline const Data::Matrix4x4 Rotation(float x, float y, float z)
{
    return Multiply(Multiply(RotationX(x), RotationY(y)), RotationZ(z));
}

inline Data::Point3D Multiply(const Data::Point3D& point, const Data::Matrix3x3& mtx)
{
    // TODO: use Dot and matrix col functions
    Data::Point3D result;
    result.X = point.X * mtx.X.X + point.Y * mtx.Y.X + point.Z * mtx.Z.X;
    result.Y = point.X * mtx.X.Y + point.Y * mtx.Y.Y + point.Z * mtx.Z.Y;
    result.Z = point.X * mtx.X.Z + point.Y * mtx.Y.Z + point.Z * mtx.Z.Z;
    return result;
}

inline Data::Point4D Multiply(const Data::Point4D& point, const Data::Matrix4x4& mtx)
{
    // TODO: use Dot and matrix col functions
    Data::Point4D result;
    result.X = point.X * mtx.X.X + point.Y * mtx.Y.X + point.Z * mtx.Z.X + point.W * mtx.W.X;
    result.Y = point.X * mtx.X.Y + point.Y * mtx.Y.Y + point.Z * mtx.Z.Y + point.W * mtx.W.Y;
    result.Z = point.X * mtx.X.Z + point.Y * mtx.Y.Z + point.Z * mtx.Z.Z + point.W * mtx.W.Z;
    result.W = point.X * mtx.X.W + point.Y * mtx.Y.W + point.Z * mtx.Z.W + point.W * mtx.W.W;
    return result;
}

inline Data::Point2D ProjectPoint3DToPoint2D(const Data::Point3D& point, const Data::Matrix4x4& mtx)
{
    Data::Point4D result = Multiply(Data::Point4D{ point.X, point.Y, point.Z, 1.0f }, mtx);

    // homogeneous divide
    result.X /= result.W;
    result.Y /= result.W;
    result.Z /= result.W;
    result.W = 1.0f;

    return Data::Point2D{result.X, result.Y};
}

inline Data::Point3D operator - (const Data::Point3D & A, const Data::Point3D & B)
{
    Data::Point3D ret;
    ret.X = A.X - B.X;
    ret.Y = A.Y - B.Y;
    ret.Z = A.Z - B.Z;
    return ret;
}

inline float Length(const Data::Point3D& V)
{
    return (float)sqrtf(Dot(V, V));
}

inline Data::Point3D Normalize(const Data::Point3D& V)
{
    float len = Length(V);
    Data::Point3D p;
    p.X = V.X / len;
    p.Y = V.Y / len;
    p.Z = V.Z / len;
    return p;
}

inline Data::Point3D Cross(const Data::Point3D& a, const Data::Point3D& b)
{
    return
    {
        a.Y * b.Z - a.Z * b.Y,
        a.Z * b.X - a.X * b.Z,
        a.X * b.Y - a.Y * b.X
    };
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

inline void PixelToCanvas(const Data::Document& document, float pixelX, float pixelY, float& canvasX, float& canvasY)
{
    // +/- 50 in canvas units is the largest square that can fit in the render, centered in the middle of the render.

    int canvasSizeInPixels = (document.renderSizeX >= document.renderSizeY) ? document.renderSizeY : document.renderSizeX;
    int centerPx = document.renderSizeX / 2;
    int centerPy = document.renderSizeY / 2;

    canvasX = 100.0f * float(pixelX - centerPx) / float(canvasSizeInPixels);
    canvasY = 100.0f * float(pixelY - centerPy) / float(canvasSizeInPixels);
}

inline void PixelToCanvas(const Data::Document& document, int pixelX, int pixelY, float& canvasX, float& canvasY)
{
    PixelToCanvas(document, (float)pixelX, (float)pixelY, canvasX, canvasY);
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

bool MakeJitterSequence(Data::Document& document);