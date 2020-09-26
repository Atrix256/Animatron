#include <array>
#include "schemas/types.h"
#include "utils.h"
#include <unordered_map>

// TODO: need to look at samplesPerPixel and jitterSequence.points, for multi sampling
// TODO: will have to turn PMA back into non PMA before writing it out (and before making it opaque!)

void EntityFill_Initialize(const Data::Document& document, Data::EntityFill& fill)
{
}

void EntityFill_DoAction(
    const Data::Document& document, 
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityFill& fill)
{
    Data::ColorPMA colorPMA = ToPremultipliedAlpha(fill.color);

    // if the color is opaque just do a fill
    if (fill.color.A >= 1.0f)
    {
        std::fill(pixels.begin(), pixels.end(), colorPMA);
        return;
    }

    // otherwise, do a blend operation
    for (Data::ColorPMA& pixel : pixels)
        pixel = Blend(pixel, colorPMA);
}

void EntityCircle_Initialize(const Data::Document& document, Data::EntityCircle& circle)
{
}

void EntityCircle_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string,
    Data::EntityVariant>& entityMap, std::vector<Data::ColorPMA>& pixels,
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

    // TODO: handle alpha blending. maybe some template parameter function for speed? or always pay worst case
    Data::ColorPMA colorPMA = ToPremultipliedAlpha(circle.color);

    // Draw the circle
    float canvasMinX, canvasMinY, canvasMaxX, canvasMaxY;
    PixelToCanvas(document, 0, 0, canvasMinX, canvasMinY);
    PixelToCanvas(document, document.renderSizeX - 1, document.renderSizeY - 1, canvasMaxX, canvasMaxY);
    for (int iy = minPixelY; iy <= maxPixelY; ++iy)
    {
        float percentY = float(iy) / float(document.renderSizeY - 1);
        float canvasY = Lerp(canvasMinY, canvasMaxY, percentY);
        float distY = abs(canvasY - circle.center.Y);
        Data::ColorPMA* pixel = &pixels[iy * document.renderSizeX + minPixelX];
        for (int ix = minPixelX; ix <= maxPixelX; ++ix)
        {
            float percentX = float(ix) / float(document.renderSizeX - 1);
            float canvasX = Lerp(canvasMinX, canvasMaxX, percentX);
            float distX = abs(canvasX - circle.center.X);

            float dist = (float)sqrt(distX*distX + distY * distY);
            dist -= circle.innerRadius;
            if (dist > 0.0f && dist <= circle.outerRadius)
                *pixel = colorPMA;
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
    std::vector<Data::ColorPMA>& pixels,
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
    Data::ColorPMA colorPMA = ToPremultipliedAlpha(rectangle.color);

    // Draw the rectangle
    for (int iy = minPixelY; iy <= maxPixelY; ++iy)
    {
        Data::ColorPMA* pixel = &pixels[iy * document.renderSizeX + minPixelX];
        for (int ix = minPixelX; ix <= maxPixelX; ++ix)
        {
            *pixel = colorPMA;
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
    std::vector<Data::ColorPMA>& pixels,
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
    Data::ColorPMA colorPMA = ToPremultipliedAlpha(line.color);

    // Draw the line
    for (int iy = minPixelY; iy <= maxPixelY; ++iy)
    {
        Data::ColorPMA* pixel = &pixels[iy * document.renderSizeX + minPixelX];
        for (int ix = minPixelX; ix <= maxPixelX; ++ix)
        {
            float canvasX, canvasY;
            PixelToCanvas(document, ix, iy, canvasX, canvasY);

            float distance = sdLine({ line.A.X, line.A.Y }, { line.B.X, line.B.Y }, { canvasX, canvasY });

            if (distance < line.width)
                *pixel = colorPMA;
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
    std::vector<Data::ColorPMA>& pixels,
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
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityLines3D& lines3d)
{
    // TODO: The lines want a camera, to be able to turn 3d lines into 2d. We probably need to make a map of all the entities and their correct state for this frame and pass it to each of these functions.
    // TODO: get the viewProj matrix from the camera
}
