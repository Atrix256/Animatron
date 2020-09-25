#pragma once

template <typename T>
T Clamp(T value, T min, T max)
{
    if (value <= min)
        return min;
    else if (value >= max)
        return max;
    else
        return value;
}

inline float LinearToSRGB(float x)
{
    x = Clamp(x, 0.0f, 1.0f);
    if (x < 0.0031308f)
        return x * 12.92f;
    else
        return pow(x * 1.055f, 1.0f / 2.4f) - 0.055f;
}

inline float SRGBToLinear(float x)
{
    x = Clamp(x, 0.0f, 1.0f);
    if (x < 0.04045f)
        return x / 12.92f;
    else
        return pow((x + 0.055f) / 1.055f, 2.4f);
}

inline float Lerp(float A, float B, float t)
{
    return A * (1.0f - t) + B * t;
}

inline float CubicBezierInterpolation(float A, float B, float C, float D, float t)
{
    // Cubic bezier interpolation.
    // The four control points are at:
    // (0,A) (0.333, B) (0.666, C) (1, D)
    // at time 0, this returns A
    // at time 1, this returns D
    // in the middle, it does cubic interpolation with those control points.
    //
    // In Bernstein Basis form, the formula is...
    // y = As^3 + 3*Bs^2t + 3*Cst^2 + Dt^3
    // where s = 1-t

    float s = 1.0f - t;

    return
        A * s*s*s +
        3.0f * B * s*s*t +
        3.0f * C * s*t*t +
        D * t*t*t
        ;
}
