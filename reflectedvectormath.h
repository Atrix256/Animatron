#pragma once

#include "schemas/types.h"

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

inline Data::Point3D GetMatrixColumn(const Data::Matrix3x3& mtx, int index)
{
    switch (index)
    {
    case 0: return Data::Point3D{ mtx.X.X, mtx.Y.X, mtx.Z.X };
    case 1: return Data::Point3D{ mtx.X.Y, mtx.Y.Y, mtx.Z.Y };
    case 2: 
    default:
        return Data::Point3D{ mtx.X.Z, mtx.Y.Z, mtx.Z.Z };
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

inline const Data::Point3D& GetMatrixRow(const Data::Matrix3x3& mtx, int index)
{
    switch (index)
    {
    case 0: return mtx.X;
    case 1: return mtx.Y;
    case 2:
    default:
        return mtx.Z;
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
    Data::Point3D result;
    result.X = Dot(point, GetMatrixColumn(mtx, 0));
    result.Y = Dot(point, GetMatrixColumn(mtx, 1));
    result.Z = Dot(point, GetMatrixColumn(mtx, 2));
    return result;
}

inline Data::Point4D Multiply(const Data::Point4D& point, const Data::Matrix4x4& mtx)
{
    Data::Point4D result;
    result.X = Dot(point, GetMatrixColumn(mtx, 0));
    result.Y = Dot(point, GetMatrixColumn(mtx, 1));
    result.Z = Dot(point, GetMatrixColumn(mtx, 2));
    result.W = Dot(point, GetMatrixColumn(mtx, 3));
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

    return Data::Point2D{ result.X, result.Y };
}

inline Data::Point3D operator - (const Data::Point3D& A, const Data::Point3D& B)
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
