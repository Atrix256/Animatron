#pragma once

// TODO: do these headers all need to be here, or can they be internal? or elsewhere? maybe some belong in main (like json?)

#include "schemas/types.h"
#include "schemas/json.h"
#include "schemas/lerp.h"
#include "schemas/hash.h"

#include "config.h"

#include <omp.h>
#include <unordered_map>
#include <vector>

// Many frames are pixel identical. This allows that to be detected and a frame on disk be copied instead of re-rendered, or the pixels to be re-used
class FrameCache
{
public:
    struct FrameData
    {
        int frameIndex;
        std::vector<Data::ColorU8> pixels;
    };

    FrameCache()
    {
        omp_init_lock(&m_lock);
    }

    ~FrameCache()
    {
        omp_destroy_lock(&m_lock);
        m_lock = nullptr;
    }

    void Reset()
    {
        omp_set_lock(&m_lock);

        m_frameData.clear();
        m_frames.clear();

        omp_unset_lock(&m_lock);
    }

    const FrameData& GetFrame(size_t hash)
    {
        omp_set_lock(&m_lock);

        auto it = m_frames.find(hash);
        if (it != m_frames.end())
        {
            omp_unset_lock(&m_lock);
            return *it->second;
        }

        omp_unset_lock(&m_lock);

        static FrameData none{ -1 };
        return none;
    }

    void SetFrame(size_t hash, int frameNumber, const std::vector<Data::ColorU8>& pixels)
    {
        omp_set_lock(&m_lock);

        m_frameData.push_back(FrameData{ frameNumber, pixels });
        m_frames[hash] = &(*m_frameData.rbegin());

        omp_unset_lock(&m_lock);
    }

private:
    omp_lock_t m_lock;
    std::unordered_map<size_t, FrameData*> m_frames;
    std::vector<FrameData> m_frameData;
};

struct ThreadContext
{
    int threadId = -1;
    char outFileName[1024];
    std::vector<Data::ColorPMA> pixelsPMA;
    std::vector<Data::Color> pixels;
    std::vector<Data::ColorU8> pixelsU8;
};

struct Context
{
    FrameCache frameCache;
};

bool ValidateAndFixupDocument(Data::Document& document);

bool RenderFrame(const Data::Document& document, int frameIndex, ThreadContext& threadContext, Context& context, int& recycledFrameIndex, size_t& frameHash);
