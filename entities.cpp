#include <array>
#include "schemas/types.h"
#include "utils.h"
#include <unordered_map>

// TODO: find a home for this?

static void DrawLine(const Data::Document& document, std::vector<Data::ColorPMA>& pixels, const Data::Point2D& A, const Data::Point2D& B, float width, const Data::ColorPMA& color)
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

// ==============================================

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

    Data::ColorPMA colorPMA = ToPremultipliedAlpha(circle.color);

    // Draw the circle
    float canvasMinX, canvasMinY, canvasMaxX, canvasMaxY;
    PixelToCanvas(document, 0, 0, canvasMinX, canvasMinY);
    PixelToCanvas(document, document.renderSizeX - 1, document.renderSizeY - 1, canvasMaxX, canvasMaxY);
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

                float percentX = (float(ix) + offset.X) / float(document.renderSizeX - 1);
                float canvasX = Lerp(canvasMinX, canvasMaxX, percentX);
                float distX = abs(canvasX - circle.center.X);

                float percentY = (float(iy) + offset.Y) / float(document.renderSizeY - 1);
                float canvasY = Lerp(canvasMinY, canvasMaxY, percentY);
                float distY = abs(canvasY - circle.center.Y);

                float dist = (float)sqrt(distX * distX + distY * distY);
                dist -= circle.innerRadius;

                if (dist > 0.0f && dist <= circle.outerRadius)
                {
                    samplesColor.R += colorPMA.R / float(document.samplesPerPixel);
                    samplesColor.G += colorPMA.G / float(document.samplesPerPixel);
                    samplesColor.B += colorPMA.B / float(document.samplesPerPixel);
                    samplesColor.A += colorPMA.A / float(document.samplesPerPixel);
                }
            }

            // alpha blend the result in
            *pixel = Blend(*pixel, samplesColor);
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

    Data::ColorPMA colorPMA = ToPremultipliedAlpha(rectangle.color);

    // Draw the rectangle
    for (int iy = minPixelY; iy <= maxPixelY; ++iy)
    {
        Data::ColorPMA* pixel = &pixels[iy * document.renderSizeX + minPixelX];
        for (int ix = minPixelX; ix <= maxPixelX; ++ix)
        {
            *pixel = Blend(*pixel, colorPMA);
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
    DrawLine(document, pixels, line.A, line.B, line.width, ToPremultipliedAlpha(line.color));
}

void EntityCamera_Initialize(const Data::Document& document, Data::EntityCamera& camera)
{
    // Deal with output z if we ever have a z buffer or similar. It's set to zero in these projection matrices.
    Data::Matrix4x4 projMtx;

    // TODO: make sure handedness matches.

    float canvasMinX, canvasMinY, canvasMaxX, canvasMaxY;
    PixelToCanvas(document, 0, 0, canvasMinX, canvasMinY);
    PixelToCanvas(document, document.renderSizeX - 1, document.renderSizeY - 1, canvasMaxX, canvasMaxY);

    // TODO: i don't think this is correct. i think the object is too small and too close.
    // TODO: the z axis near plane value is probably too small. z values should be more like xy values.
    float width = 1.0f / 100.0f;
    float height = 1.0f / 100.0f;

    if (camera.perspective)
    {
        projMtx.X = Data::Point4D{ 2.0f * camera.near / width, 0.0f, 0.0f, 0.0f };
        projMtx.Y = Data::Point4D{ 0.0f, 2.0f * camera.near / height, 0.0f, 0.0f };
        projMtx.Z = Data::Point4D{ 0.0f, 0.0f, 0.0f, -1.0f };
        projMtx.W = Data::Point4D{ 0.0f, 0.0f, 0.0f, 0.0f };
    }
    else
    {
        projMtx.X = Data::Point4D{ 2.0f / width, 0.0f, 0.0f, 0.0f };
        projMtx.Y = Data::Point4D{ 0.0f, 2.0f / height, 0.0f, 0.0f };
        projMtx.Z = Data::Point4D{ 0.0f, 0.0f, 1.0f, 0.0f };
        projMtx.W = Data::Point4D{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    // TODO: can change the camera settings with keyframes

    // TODO: make view matrix too and multiply it in

    Data::Point3D forward = Normalize(camera.at - camera.position);

    Data::Matrix4x4 viewMtx;
    viewMtx.X = Data::Point4D{ 1.0f, 0.0f, 0.0f, 0.0f }; // TODO: get this with cross product
    viewMtx.Y = Data::Point4D{ camera.up.X, camera.up.Y, camera.up.Z, 0.0f };
    viewMtx.Z = Data::Point4D{ forward.X, forward.Y, forward.Z, 0.0 };
    viewMtx.W = Data::Point4D{camera.position.X, camera.position.Y, camera.position.Z, 1.0f};


    // TODO: use at / up to make x, y , z axis

    camera.viewProj = Multiply(viewMtx, projMtx);

    int ijkl = 0;
    //camera.viewProj = projMtx;
}

void EntityCamera_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityCamera& camera)
{
    // nothing to do for a camera
}

void EntityLine3D_Initialize(const Data::Document& document, Data::EntityLine3D& line3d)
{
}

void EntityLine3D_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityLine3D& line3d)
{
    // get the camera
    auto it = entityMap.find(line3d.camera);
    if (it == entityMap.end())
    {
        // TODO: how to deal with errors better? should probably stop rendering
        // TOOD: should probably say what frame, and what entity this was, and write to a log string that is per thread, and write that out after exiting the thread loop
        printf("Error: could not find lines3d camera %s\n", line3d.camera.c_str());
        return;
    }
    if (it->second._index != Data::EntityVariant::c_index_camera)
    {
        printf("Error lines3d camera was not a camera %s\n", line3d.camera.c_str());
        return;
    }
    const Data::EntityCamera& cameraEntity = it->second.camera;

    // TODO: need a world matrix!
    auto Transform = [](const Data::Point3D& point) -> Data::Point3D
    {
        return Data::Point3D{ point.X, point.Y, point.Z + 2.0f };
    };

    Data::Point2D A = ProjectPoint3DToPoint2D(Transform(line3d.A), cameraEntity.viewProj);
    Data::Point2D B = ProjectPoint3DToPoint2D(Transform(line3d.B), cameraEntity.viewProj);
    DrawLine(document, pixels, A, B, line3d.width, ToPremultipliedAlpha(line3d.color));
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
    // get the camera
    auto it = entityMap.find(lines3d.camera);
    if (it == entityMap.end())
    {
        // TODO: how to deal with errors better? should probably stop rendering
        // TOOD: should probably say what frame, and what entity this was, and write to a log string that is per thread, and write that out after exiting the thread loop
        printf("Error: could not find lines3d camera %s\n", lines3d.camera.c_str());
        return;
    }
    if (it->second._index != Data::EntityVariant::c_index_camera)
    {
        printf("Error lines3d camera was not a camera %s\n", lines3d.camera.c_str());
        return;
    }
    const Data::EntityCamera& cameraEntity = it->second.camera;

    // need at least 2 points to make a line
    if (lines3d.points.size() < 2)
        return;

    // TODO: need a world matrix!
    auto Transform = [] (const Data::Point3D& point) -> Data::Point3D
    {
        return Data::Point3D{ point.X * 1.0f, point.Y * 1.0f, point.Z + 1.2f};
    };

    Data::Point2D lastPoint = ProjectPoint3DToPoint2D(Transform(lines3d.points[0]), cameraEntity.viewProj);
    for (int pointIndex = 1; pointIndex < lines3d.points.size(); ++pointIndex)
    {
        Data::Point2D nextPoint = ProjectPoint3DToPoint2D(Transform(lines3d.points[pointIndex]), cameraEntity.viewProj);

        // TODO: need to make some helper function to draw the line, instead of calling into another do action. we don't have a line entity!
        DrawLine(document, pixels, lastPoint, nextPoint, lines3d.width, ToPremultipliedAlpha(lines3d.color));
        lastPoint = nextPoint;
    }

    //ProjectPoint3DToPoint2D

    int ijkl = 0;
    // TODO: The lines want a camera, to be able to turn 3d lines into 2d. We probably need to make a map of all the entities and their correct state for this frame and pass it to each of these functions.
    // TODO: get the viewProj matrix from the camera
    // TODO: probably need an oriented bounded box? maybe everything needs a transform? i dunno.
}
