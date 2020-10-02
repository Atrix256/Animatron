#include "utils.h"
#include <random>

void Resize(std::vector<Data::Color> &pixels, int sizeX, int sizeY, int desiredSizeX, int desiredSizeY)
{
    // resize on x axis first
    if (sizeX != desiredSizeX)
    {
        // allocate the new image
        std::vector<Data::Color> newImage;
        newImage.resize(desiredSizeX*sizeY);

        // if we are growing the image, do cubic interpolation
        if (sizeX < desiredSizeX)
        {
            for (size_t iy = 0; iy < sizeY; ++iy)
            {
                Data::Color* destPixel = &newImage[iy*desiredSizeX];
                for (size_t ix = 0; ix < desiredSizeX; ++ix)
                {
                    float percent = float(ix) / float(desiredSizeX - 1);
                    float srcPixel = percent * float(sizeX);

                    int pixel_neg1 = Clamp(int(srcPixel) - 1, 0, sizeX - 1);
                    int pixel_0    = Clamp(int(srcPixel) + 0, 0, sizeX - 1);
                    int pixel_1    = Clamp(int(srcPixel) + 1, 0, sizeX - 1);
                    int pixel_2    = Clamp(int(srcPixel) + 2, 0, sizeX - 1);

                    float time = Fract(srcPixel);

                    const Data::Color& pn1 = pixels[iy * sizeX + pixel_neg1];
                    const Data::Color& pp0 = pixels[iy * sizeX + pixel_0];
                    const Data::Color& pp1 = pixels[iy * sizeX + pixel_1];
                    const Data::Color& pp2 = pixels[iy * sizeX + pixel_2];

                    *destPixel = CubicHermite(pn1, pp0, pp1, pp2, time);

                    destPixel++;
                }
            }
        }
        // if we are shrinking the image, do box filter / integration
        else
        {
            for (size_t iy = 0; iy < sizeY; ++iy)
            {
                Data::Color* destPixel = &newImage[iy*desiredSizeX];
                for (size_t ix = 0; ix < desiredSizeX; ++ix)
                {
                    float percentStart = float(ix) / float(desiredSizeX);
                    float percentEnd = float(ix + 1) / float(desiredSizeX);

                    float srcPixelStart = Clamp(percentStart * float(sizeX), 0.0f, float(sizeX - 1));
                    float srcPixelEnd = Clamp(percentEnd * float(sizeX), 0.0f, float(sizeX - 1));

                    Data::Color pixelSum;
                    float weightSum = 0.0f;

                    // integrate (average) over the footprint
                    while (srcPixelStart < srcPixelEnd)
                    {
                        // find how much of this pixel the foot print covers
                        float pixelEnd = Min((float)floor(srcPixelStart) + 1.0f, srcPixelEnd);
                        float pixelWeight = pixelEnd - srcPixelStart;

                        // read pixel, keep a sum of weight and color*weight 
                        pixelSum = pixelSum + pixels[iy * sizeX + int(srcPixelStart)] * pixelWeight;
                        weightSum += pixelWeight;

                        // move to next pixel
                        srcPixelStart = pixelEnd;
                    }

                    *destPixel = pixelSum / weightSum;
                    destPixel++;
                }
            }
        }

        // update the image
        pixels = newImage;
        sizeX = desiredSizeX;
    }

    // resize on y axis second
    if (sizeY != desiredSizeY)
    {
        // allocate the new image
        std::vector<Data::Color> newImage;
        newImage.resize(desiredSizeX*desiredSizeY);

        // if we are growing the image, do cubic interpolation
        if (sizeY < desiredSizeY)
        {
            for (size_t ix = 0; ix < sizeX; ++ix)
            {
                for (size_t iy = 0; iy < desiredSizeY; ++iy)
                {
                    float percent = float(iy) / float(desiredSizeY - 1);
                    float srcPixel = percent * float(sizeY);

                    int pixel_neg1 = Clamp(int(srcPixel) - 1, 0, sizeY - 1);
                    int pixel_0    = Clamp(int(srcPixel) + 0, 0, sizeY - 1);
                    int pixel_1    = Clamp(int(srcPixel) + 1, 0, sizeY - 1);
                    int pixel_2    = Clamp(int(srcPixel) + 2, 0, sizeY - 1);

                    float time = Fract(srcPixel);

                    const Data::Color& pn1 = pixels[pixel_neg1 * sizeX + ix];
                    const Data::Color& pp0 = pixels[pixel_0    * sizeX + ix];
                    const Data::Color& pp1 = pixels[pixel_1    * sizeX + ix];
                    const Data::Color& pp2 = pixels[pixel_2    * sizeX + ix];

                    Data::Color* destPixel = &newImage[iy*sizeX + ix];
                    *destPixel = CubicHermite(pn1, pp0, pp1, pp2, time);
                }
            }
        }
        // if we are shrinking the image, do box filter / integration
        else
        {
            for (size_t ix = 0; ix < sizeX; ++ix)
            {
                for (size_t iy = 0; iy < desiredSizeY; ++iy)
                {
                    float percentStart = float(iy) / float(desiredSizeY);
                    float percentEnd = float(iy + 1) / float(desiredSizeY);

                    float srcPixelStart = Clamp(percentStart * float(sizeY), 0.0f, float(sizeY - 1));
                    float srcPixelEnd = Clamp(percentEnd * float(sizeY), 0.0f, float(sizeY - 1));

                    Data::Color pixelSum;
                    float weightSum = 0.0f;

                    // integrate (average) over the footprint
                    while (srcPixelStart < srcPixelEnd)
                    {
                        // find how much of this pixel the foot print covers
                        float pixelEnd = Min((float)floor(srcPixelStart) + 1.0f, srcPixelEnd);
                        float pixelWeight = pixelEnd - srcPixelStart;

                        // read pixel, keep a sum of weight and color*weight 
                        pixelSum = pixelSum + pixels[int(srcPixelStart) * sizeX + ix] * pixelWeight;
                        weightSum += pixelWeight;

                        // move to next pixel
                        srcPixelStart = pixelEnd;
                    }

                    Data::Color* destPixel = &newImage[iy*sizeX + ix];
                    *destPixel = pixelSum / weightSum;
                    destPixel++;
                }
            }
        }

        // update the image
        pixels = newImage;
        sizeY = desiredSizeY;
    }
}

void MakeJitterSequence_MitchellsBlueNoise(Data::Document& document)
{
    std::mt19937 rng;
    static std::uniform_real_distribution<float> dist(0, 1);

    for (uint32_t i = 1; i <= document.samplesPerPixel; ++i)
    {
        // keep the candidate that is farthest from it's closest point
        size_t numCandidates = i;
        float bestDistance = 0.0f;
        Data::Point2D bestCandidatePos = Data::Point2D{ 0.0f, 0.0f };
        for (size_t candidate = 0; candidate < numCandidates; ++candidate)
        {
            Data::Point2D candidatePos = Data::Point2D{ dist(rng), dist(rng) };

            // calculate the closest distance from this point to an existing sample
            float minDist = FLT_MAX;
            for (const Data::Point2D& samplePos : document.jitterSequence.points)
            {
                float dist = DistanceUnitTorroidal(vec2{ samplePos.X, samplePos.Y }, vec2{ candidatePos.X, candidatePos.Y });
                if (dist < minDist)
                    minDist = dist;
            }

            if (minDist > bestDistance)
            {
                bestDistance = minDist;
                bestCandidatePos = candidatePos;
            }
        }
        document.jitterSequence.points.push_back(bestCandidatePos);
    }
}

bool MakeJitterSequence(Data::Document& document)
{
    if (document.samplesPerPixel == 1)
    {
        document.jitterSequence.points.push_back(Data::Point2D{0.5f, 0.5f});
        return true;
    }

    switch (document.jitterSequenceType)
    {
        case Data::SamplesType2D::MitchellsBlueNoise:
        {
            MakeJitterSequence_MitchellsBlueNoise(document);
            break;
        }
        default:
        {
            printf("Error: Unhandled jitter sequence type encountered!");
            return false;
        }
    }
    return true;
}

void DrawLine(const Data::Document& document, std::vector<Data::ColorPMA>& pixels, const Data::Point2D& A, const Data::Point2D& B, float width, const Data::ColorPMA& color)
{
    // Get a bounding box of the line
    float minX = Min(A.X, B.X) - width;
    float minY = Min(A.Y, B.Y) - width;
    float maxX = Max(A.X, B.X) + width;
    float maxY = Max(A.Y, B.Y) + width;

    int minPixelX, minPixelY, maxPixelX, maxPixelY;
    CanvasToPixel(document, minX, minY, minPixelX, minPixelY);
    CanvasToPixel(document, maxX, maxY, maxPixelX, maxPixelY);

    // clip the bounding box to the screen
    minPixelX = Clamp(minPixelX, 0, document.renderSizeX - 1);
    maxPixelX = Clamp(maxPixelX, 0, document.renderSizeX - 1);
    minPixelY = Clamp(minPixelY, 0, document.renderSizeY - 1);
    maxPixelY = Clamp(maxPixelY, 0, document.renderSizeY - 1);

    // Draw the line
    for (int iy = minPixelY; iy <= maxPixelY; ++iy)
    {
        Data::ColorPMA* pixel = &pixels[iy * document.renderSizeX + minPixelX];
        for (int ix = minPixelX; ix <= maxPixelX; ++ix)
        {
            // do multiple jittered samples per pixel and integrate (average) the result
            Data::ColorPMA samplesColor;
            for (uint32_t sampleIndex = 0; sampleIndex < document.samplesPerPixel; ++sampleIndex)
            {
                Data::Point2D offset = document.jitterSequence.points[sampleIndex];

                float canvasX, canvasY;
                PixelToCanvas(document, (float)ix + offset.X, (float)iy + offset.Y, canvasX, canvasY);

                float distance = sdLine({ A.X, A.Y }, { B.X, B.Y }, { canvasX, canvasY });

                if (distance < width)
                {
                    samplesColor.R += color.R / float(document.samplesPerPixel);
                    samplesColor.G += color.G / float(document.samplesPerPixel);
                    samplesColor.B += color.B / float(document.samplesPerPixel);
                    samplesColor.A += color.A / float(document.samplesPerPixel);
                }
            }

            // alpha blend the result in
            *pixel = Blend(*pixel, samplesColor);
            pixel++;
        }
    }
}