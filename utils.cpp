#include "utils.h"

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