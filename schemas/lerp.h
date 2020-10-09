// This makes functions to lerp (linear interpolate) struct defined types.
// It does this by recursing down to base types and lerping those.
// This is used by key frame interpolation.
#pragma once
#include "../df_serialize/df_serialize/_common.h"
#include "../math.h"

#include <stdint.h>

// Enums

#define ENUM_BEGIN(_NAMESPACE, _NAME, _DESCRIPTION) \
    inline void Lerp(const _NAMESPACE::_NAME& A, const _NAMESPACE::_NAME& B, _NAMESPACE::_NAME& Result, float t) \
    { \
        Result = (t < 0.5f) ? A : B; \
    }

#define ENUM_ITEM(_NAME, _DESCRIPTION)

#define ENUM_END()

// Structs

#define STRUCT_BEGIN(_NAMESPACE, _NAME, _DESCRIPTION) \
    inline void Lerp(const _NAMESPACE::_NAME& A, const _NAMESPACE::_NAME& B, _NAMESPACE::_NAME& Result, float t) \
    { \

#define STRUCT_INHERIT_BEGIN(_NAMESPACE, _NAME, _BASE, _DESCRIPTION) \
    inline void Lerp(const _NAMESPACE::_NAME& A, const _NAMESPACE::_NAME& B, _NAMESPACE::_NAME& Result, float t) \
    { \
        Lerp((*(_BASE*)&A), (*(_BASE*)&B), (*(_BASE*)&Result), t);

#define STRUCT_FIELD(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
        Lerp(A._NAME, B._NAME, Result._NAME, t);

// Note: no serialize also means no lerp
#define STRUCT_FIELD_NO_SERIALIZE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION)

#define STRUCT_DYNAMIC_ARRAY(_TYPE, _NAME, _DESCRIPTION) \
        { \
            size_t size = A._NAME.size(); \
            if (B._NAME.size() < size) \
                size = B._NAME.size(); \
            for (size_t index = 0; index < size; ++index) \
                Lerp(A._NAME[index], B._NAME[index], Result._NAME[index], t); \
        }

#define STRUCT_STATIC_ARRAY(_TYPE, _NAME, _SIZE, _DEFAULT, _DESCRIPTION) \
        for (size_t index = 0; index < _SIZE; ++index) \
            Lerp(A._NAME[index], B._NAME[index], Result._NAME[index], t);

#define STRUCT_END() \
    }

// Variants

#define VARIANT_BEGIN(_NAMESPACE, _NAME, _DESCRIPTION) \
    inline void Lerp(const _NAMESPACE::_NAME& A, const _NAMESPACE::_NAME& B, _NAMESPACE::_NAME& Result, float t) \
    { \
        typedef _NAMESPACE::_NAME ThisType; \
        if (A._index != B._index) \
        { \
            Result = (t < 0.5f) ? A : B; \
            return; \
        } \
        Result._index = A._index;

#define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
        if (A._index == ThisType::c_index_##_NAME) \
            Lerp(A._NAME, B._NAME, Result._NAME, t);

#define VARIANT_END() \
    }

// Catch all for making easier to understand errors when an unhandled type is encountered

template <typename T>
inline void Lerp(const T& A, const T& B, T& Result, float t)
{
    static_assert(false, __FUNCSIG__ " : Unhandled type encountered in Lerp()");
}

// Basic types

// The test for equality is due to numerical problems where if A==B, the result of the lerp math is not always A!
#define STANDARD_LERP(_TYPE) \
    template<> \
    inline void Lerp<_TYPE>(const _TYPE& A, const _TYPE& B, _TYPE& Result, float t) \
    { \
        if (A == B) \
            Result = A; \
        else \
            Result = (_TYPE)Lerp((float)A, (float)B, t); \
    }

#define BINARY_LERP(_TYPE) \
    template<> \
    inline void Lerp<_TYPE>(const _TYPE& A, const _TYPE& B, _TYPE& Result, float t) \
    { \
        Result = (t < 0.5f) ? A : B; \
    }

STANDARD_LERP(float)
STANDARD_LERP(int8_t)
STANDARD_LERP(int16_t)
STANDARD_LERP(int32_t)
STANDARD_LERP(int64_t)
STANDARD_LERP(uint8_t)
STANDARD_LERP(uint16_t)
STANDARD_LERP(uint32_t)
STANDARD_LERP(uint64_t)

BINARY_LERP(std::string)
BINARY_LERP(bool)

#undef STANDARD_LERP
#undef BINARY_LERP

// expand the macros
#include "schemas.h"
