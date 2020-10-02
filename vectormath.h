#pragma once

#include <array>

typedef std::array<float, 2> vec2;
typedef std::array<float, 3> vec3;
typedef std::array<float, 4> vec4;

template <size_t N>
std::array<float, N> operator * (const std::array<float, N>& A, float B)
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
