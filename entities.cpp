#include <array>
#include "schemas/types.h"
#include "utils.h"
#include <unordered_map>

// TODO: need to look at samplesPerPixel and jitterSequenceType

void EntityFill_Initialize(const Data::Document& document, Data::EntityFill& fill)
{
}

void EntityFill_DoAction(
    const Data::Document& document, 
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::Color>& pixels,
    const Data::EntityFill& fill)
{
    // NOTE: no need to do anything for multi sampling.

    // if the color is opaque just do a fill
    if (fill.color.A >= 1.0f)
    {
        std::fill(pixels.begin(), pixels.end(), fill.color);
        return;
    }

    // TODO: do alpha blending. pre-multiplied alpha
    std::fill(pixels.begin(), pixels.end(), fill.color);
}

void EntityCircle_Initialize(const Data::Document& document, Data::EntityCircle& circle)
{
}

void EntityCircle_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string,
    Data::EntityVariant>& entityMap, std::vector<Data::Color>& pixels,
    const Data::EntityCircle& circle)
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

            float dist = (float)sqrt(distX*distX + distY * distY);
            dist -= circle.innerRadius;
            if (dist > 0.0f && dist <= circle.outerRadius)
                *pixel = circle.color;
            pixel++;
        }
    }
}

void EntityRectangle_Initialize(const Data::Document& document, Data::EntityRectangle& rectangle)
{
}

void EntityRectangle_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::Color>& pixels,
    const Data::EntityRectangle& rectangle)
{
    // Get the box of the rectangle
    int minPixelX, minPixelY, maxPixelX, maxPixelY;
    CanvasToPixel(document, rectangle.center.X - rectangle.width / 2, rectangle.center.Y - rectangle.height / 2, minPixelX, minPixelY);
    CanvasToPixel(document, rectangle.center.X + rectangle.width / 2, rectangle.center.Y + rectangle.height / 2, maxPixelX, maxPixelY);
    minPixelX--;
    minPixelY--;
    maxPixelX++;
    maxPixelY++;

    // clip the bounding box to the screen
    minPixelX = Clamp(minPixelX, 0, document.renderSizeX - 1);
    maxPixelX = Clamp(maxPixelX, 0, document.renderSizeX - 1);
    minPixelY = Clamp(minPixelY, 0, document.renderSizeY - 1);
    maxPixelY = Clamp(maxPixelY, 0, document.renderSizeY - 1);

    // TODO: handle alpha blending.

    // Draw the rectangle
    for (int iy = minPixelY; iy <= maxPixelY; ++iy)
    {
        Data::Color* pixel = &pixels[iy * document.renderSizeX + minPixelX];
        for (int ix = minPixelX; ix <= maxPixelX; ++ix)
        {
            *pixel = rectangle.color;
            pixel++;
        }
    }
}

void EntityLine_Initialize(const Data::Document& document, Data::EntityLine& line)
{
}

void EntityLine_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::Color>& pixels,
    const Data::EntityLine& line)
{
    // Get a bounding box of the line
    float minX = Min(line.A.X, line.B.X) - line.width;
    float minY = Min(line.A.Y, line.B.Y) - line.width;
    float maxX = Max(line.A.X, line.B.X) + line.width;
    float maxY = Max(line.A.Y, line.B.Y) + line.width;

    int minPixelX, minPixelY, maxPixelX, maxPixelY;
    CanvasToPixel(document, minX, minY, minPixelX, minPixelY);
    CanvasToPixel(document, maxX, maxY, maxPixelX, maxPixelY);

    // clip the bounding box to the screen
    minPixelX = Clamp(minPixelX, 0, document.renderSizeX - 1);
    maxPixelX = Clamp(maxPixelX, 0, document.renderSizeX - 1);
    minPixelY = Clamp(minPixelY, 0, document.renderSizeY - 1);
    maxPixelY = Clamp(maxPixelY, 0, document.renderSizeY - 1);

    // TODO: handle alpha blending.

    // Draw the line
    for (int iy = minPixelY; iy <= maxPixelY; ++iy)
    {
        Data::Color* pixel = &pixels[iy * document.renderSizeX + minPixelX];
        for (int ix = minPixelX; ix <= maxPixelX; ++ix)
        {
            float canvasX, canvasY;
            PixelToCanvas(document, ix, iy, canvasX, canvasY);

            float distance = sdLine({ line.A.X, line.A.Y }, { line.B.X, line.B.Y }, { canvasX, canvasY });

            if (distance < line.width)
                *pixel = line.color;
            pixel++;
        }
    }
}

void EntityCamera_Initialize(const Data::Document& document, Data::EntityCamera& camera)
{
    // TODO: calculate view/proj matrix!
    int ijkl = 0;
}

void EntityCamera_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::Color>& pixels,
    const Data::EntityCamera& camera)
{
    // nothing to do for a camera
}

void EntityLines3D_Initialize(const Data::Document& document, Data::EntityLines3D& lines3d)
{
}

void EntityLines3D_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap, 
    std::vector<Data::Color>& pixels,
    const Data::EntityLines3D& lines3d)
{
    // TODO: The lines want a camera, to be able to turn 3d lines into 2d. We probably need to make a map of all the entities and their correct state for this frame and pass it to each of these functions.
    // TODO: get the viewProj matrix from the camera
}
