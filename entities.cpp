#include <array>
#include "schemas/types.h"
#include "utils.h"

void HandleEntity_EntityFill(const Data::Document& document, std::vector<Data::Color>& pixels, const Data::EntityFill& fill)
{
    // if the color is opaque just do a fill
    if (fill.color.A >= 1.0f)
    {
        std::fill(pixels.begin(), pixels.end(), fill.color);
        return;
    }

    // TODO: do alpha blending. pre-multiplied alpha
    std::fill(pixels.begin(), pixels.end(), fill.color);
}

void HandleEntity_EntityCircle(const Data::Document& document, std::vector<Data::Color>& pixels, const Data::EntityCircle& circle)
{
    // TODO: maybe could have an aspect ratio on the circle to squish it and stretch it? Nah... parent it off a transform!

    // TODO: this could be done better - like by finding the bounding box. Use CanvasToPixel.
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

