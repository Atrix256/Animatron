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
    // Get a bounding box of the circle
    int minPixelX, minPixelY, maxPixelX, maxPixelY;
    float totalRadius = circle.innerRadius + circle.outerRadius;
    CanvasToPixel(document, circle.center.X - totalRadius, circle.center.Y - totalRadius, minPixelX, minPixelY);
    CanvasToPixel(document, circle.center.X + totalRadius, circle.center.Y + totalRadius, maxPixelX, maxPixelY);
    minPixelX--;
    minPixelY--;
    maxPixelX++;
    maxPixelY++;

    // clip the bounding box to the screen
    minPixelX = Clamp(minPixelX, 0, document.renderSizeX - 1);
    maxPixelX = Clamp(maxPixelX, 0, document.renderSizeX - 1);
    minPixelY = Clamp(minPixelY, 0, document.renderSizeY - 1);
    maxPixelY = Clamp(maxPixelY, 0, document.renderSizeY - 1);

    // TODO: handle alpha blending. maybe some template parameter function for speed?

    // Draw the circle
    float canvasMinX, canvasMinY, canvasMaxX, canvasMaxY;
    PixelToCanvas(document, 0, 0, canvasMinX, canvasMinY);
    PixelToCanvas(document, document.renderSizeX - 1, document.renderSizeY - 1, canvasMaxX, canvasMaxY);
    for (int iy = minPixelY; iy <= maxPixelY; ++iy)
    {
        float percentY = float(iy) / float(document.renderSizeY - 1);
        float canvasY = Lerp(canvasMinY, canvasMaxY, percentY);
        float distY = abs(canvasY - circle.center.Y);
        Data::Color* pixel = &pixels[iy * document.renderSizeX + minPixelX];
        for (int ix = minPixelX; ix <= maxPixelX; ++ix)
        {
            float percentX = float(ix) / float(document.renderSizeX - 1);
            float canvasX = Lerp(canvasMinX, canvasMaxX, percentX);
            float distX = abs(canvasX - circle.center.X);

            float dist = sqrt(distX*distX + distY * distY);
            dist -= circle.innerRadius;
            if (dist > 0.0f && dist <= circle.outerRadius)
                *pixel = circle.color;
            pixel++;
        }
    }
}

