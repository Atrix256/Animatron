#include <array>
#include "schemas/types.h"
#include "math.h"

void HandleEntity_EntityFill(const Data::Document& document, std::vector<Data::Color>& pixels, const Data::EntityFill& fill)
{
    // TODO: probably should switch to alpha blended color in the pixels?
    // TODO: if alpha isn't 1.0, should do an alpha blend instead of a straight fill.
    std::fill(pixels.begin(), pixels.end(), fill.color);
}

// TODO: could put this into a header. math.h and color.h don't quite fit. maybe combine those into math.h and put this in? dunno.
void PixelToCanvas(const Data::Document& document, int pixelX, int pixelY, float& canvasX, float& canvasY)
{
    // +/- 50 in canvas units is the largest square that can fit in the render, centered in the middle of the render.

    int canvasSizeInPixels = (document.sizeX >= document.sizeY) ? document.sizeY : document.sizeX;
    int centerPx = document.sizeX / 2;
    int centerPy = document.sizeY / 2;

    canvasX = 100.0f * float(pixelX - centerPx) / float(canvasSizeInPixels);
    canvasY = 100.0f * float(pixelY - centerPy) / float(canvasSizeInPixels);
}

void HandleEntity_EntityCircle(const Data::Document& document, std::vector<Data::Color>& pixels, const Data::EntityCircle& circle)
{
    // TODO: maybe could have an aspect ratio on the circle to squish it and stretch it? Nah... parent it off a transform!

    // TODO: this could be done better - like by finding the bounding box
    // TODO: handle alpha blending
    float canvasMinX, canvasMinY, canvasMaxX, canvasMaxY;
    PixelToCanvas(document, 0, 0, canvasMinX, canvasMinY);
    PixelToCanvas(document, document.sizeX - 1, document.sizeY - 1, canvasMaxX, canvasMaxY);
    for (int iy = 0; iy < document.sizeY; ++iy)
    {
        float percentY = float(iy) / float(document.sizeY - 1);
        float canvasY = Lerp(canvasMinY, canvasMaxY, percentY);
        float distY = abs(canvasY - circle.center.Y);
        Data::Color* pixel = &pixels[iy * document.sizeX];
        for (int ix = 0; ix < document.sizeX; ++ix)
        {
            float percentX = float(ix) / float(document.sizeX - 1);
            float canvasX = Lerp(canvasMinX, canvasMaxX, percentX);
            float distX = abs(canvasX - circle.center.X);

            float dist = sqrt(distX*distX + distY * distY);
            dist -= circle.innerRadius;
            if (dist > 0.0f && dist <= circle.outerRadius)
                *pixel = circle.color;
            pixel++;
        }
    }
    // TODO: how to do anti aliasing? maybe render too large and shrink?
}

