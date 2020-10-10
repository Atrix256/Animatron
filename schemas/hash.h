// This makes functions to hash struct defined types.
// It does this by recursing down to base types and hashing those.
// This is used for the CAS cache
#pragma once
#include "../df_serialize/df_serialize/_common.h"
#include "fnv1a.h"

#include <stdint.h>

template <class T>
inline void Hash(size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

inline void Hash(size_t& seed, const char* v)
{
    if (!v)
        return;

    while (*v)
    {
        Hash(seed, *v);
        v++;
    }
}

// Enums

#define ENUM_BEGIN(_NAMESPACE, _NAME, _DESCRIPTION)

#define ENUM_ITEM(_NAME, _DESCRIPTION)

#define ENUM_END()

// Structs

#define STRUCT_BEGIN(_NAMESPACE, _NAME, _DESCRIPTION) \
    inline void Hash(size_t& seed, const _NAMESPACE::_NAME& A) \
    { \

#define STRUCT_INHERIT_BEGIN(_NAMESPACE, _NAME, _BASE, _DESCRIPTION) \
    inline void Hash(size_t& seed, const _NAMESPACE::_NAME& A) \
    { \
        Hash(seed, (*(_BASE*)&A));

#define STRUCT_FIELD(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
        Hash(seed, A._NAME);

// Note: no serialize also means no hash
#define STRUCT_FIELD_NO_SERIALIZE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION)

#define STRUCT_DYNAMIC_ARRAY(_TYPE, _NAME, _DESCRIPTION) \
        for (size_t index = 0; index < A._NAME.size(); ++index) \
            Hash(seed, A._NAME[index]);

#define STRUCT_STATIC_ARRAY(_TYPE, _NAME, _SIZE, _DEFAULT, _DESCRIPTION) \
        for (size_t index = 0; index < _SIZE; ++index) \
            Hash(seed, A._NAME[index]);

#define STRUCT_END() \
    }

// Variants

#define VARIANT_BEGIN(_NAMESPACE, _NAME, _DESCRIPTION) \
    inline void Hash(size_t& seed, const _NAMESPACE::_NAME& A) \
    { \
        typedef _NAMESPACE::_NAME ThisType; \
        Hash(seed, A._index);

#define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
        if (A._index == ThisType::c_index_##_NAME) \
            Hash(seed, A._NAME);

#define VARIANT_END() \
    }

// expand the macros
#include "schemas.h"
