#include <array>
#include "schemas/types.h"
#include "utils.h"
#include <unordered_map>
#include "stb/stb_image.h"
#include "schemas/lerp.h"
#include <algorithm>


bool EntityFill_Initialize(const Data::Document& document, Data::EntityFill& fill, int entityIndex)
{
    return true;
}

bool EntityFill_FrameInitialize(const Data::Document& document, Data::EntityFill& fill)
{
    return true;
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

bool EntityCircle_Initialize(const Data::Document& document, Data::EntityCircle& circle, int entityIndex)
{
    return true;
}

bool EntityCircle_FrameInitialize(const Data::Document& document, Data::EntityCircle& circle)
{
    return true;
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

bool EntityRectangle_Initialize(const Data::Document& document, Data::EntityRectangle& rectangle, int entityIndex)
{
    return true;
}

bool EntityRectangle_FrameInitialize(const Data::Document& document, Data::EntityRectangle& rectangle)
{
    return true;
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

bool EntityLine_Initialize(const Data::Document& document, Data::EntityLine& line, int entityIndex)
{
    return true;
}

bool EntityLine_FrameInitialize(const Data::Document& document, Data::EntityLine& line)
{
    return true;
}

void EntityLine_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityLine& line)
{
    DrawLine(document, pixels, line.A, line.B, line.width, ToPremultipliedAlpha(line.color));
}

bool EntityCamera_Initialize(const Data::Document& document, Data::EntityCamera& camera, int entityIndex)
{
    return true;
}

bool EntityCamera_FrameInitialize(const Data::Document& document, Data::EntityCamera& camera)
{
    Data::Matrix4x4 projMtx;

    if (camera.perspective)
    {
        // right handed perspective projection matrix
        // https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovrh
        // Adjusting Z.W to be -1/100 instead of -1 though to make the x/y and z axes on equal footing.

        float aspectRatio = float(document.renderSizeX) / float(document.renderSizeY);
        float FOVRadians = DegreesToRadians(camera.FOV);
        float yscale = 1.0f / tanf(FOVRadians / 2.0f);
        float xscale = yscale / (aspectRatio);

        float zf = camera.far;
        float zn = camera.near;

        projMtx.X = Data::Point4D{ xscale,   0.0f,             0.0f,  0.0f };
        projMtx.Y = Data::Point4D{   0.0f, yscale,             0.0f,  0.0f };
        projMtx.Z = Data::Point4D{   0.0f,   0.0f,   zf / (zn - zf), -0.01f };
        projMtx.W = Data::Point4D{   0.0f,   0.0f, zn*zf/ (zn - zf),  0.0f };
    }
    else
    {
        // ortho matrix leaving x and y alone so it's the same results as if using canvas space coordinates
        float zf = camera.far;
        float zn = camera.near;

        projMtx.X = Data::Point4D{ -1.0f,  0.0f,         0.0f, 0.0f };
        projMtx.Y = Data::Point4D{  0.0f, -1.0f,         0.0f, 0.0f };
        projMtx.Z = Data::Point4D{  0.0f,  0.0f, 1.0f/(zn-zf), 0.0f };
        projMtx.W = Data::Point4D{  0.0f,  0.0f,   zn/(zn-zf), 1.0f };
    }

    Data::Point3D forward = Normalize(camera.at - camera.position);
    Data::Point3D right = Normalize(Cross(forward, camera.up));
    Data::Point3D up = Normalize(Cross(right, forward));

    Data::Matrix4x4 viewMtx;
    viewMtx.X = Data::Point4D{ right.X, right.Y, right.Z, 0.0f };
    viewMtx.Y = Data::Point4D{ up.X, up.Y, up.Z, 0.0f };
    viewMtx.Z = Data::Point4D{ forward.X, forward.Y, forward.Z, 0.0 };
    viewMtx.W = Data::Point4D{-camera.position.X, -camera.position.Y, -camera.position.Z, 1.0f};
    camera.viewProj = Multiply(viewMtx, projMtx);

    return true;
}

void EntityCamera_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityCamera& camera)
{
    // nothing to do for a camera
}

bool EntityLine3D_Initialize(const Data::Document& document, Data::EntityLine3D& line3d, int entityIndex)
{
    return true;
}

bool EntityLine3D_FrameInitialize(const Data::Document& document, Data::EntityLine3D& line3d)
{
    return true;
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
        printf("Error: could not find line3d camera %s\n", line3d.camera.c_str());
        return;
    }
    if (it->second._index != Data::EntityVariant::c_index_camera)
    {
        printf("Error line3d camera was not a camera %s\n", line3d.camera.c_str());
        return;
    }
    const Data::EntityCamera& cameraEntity = it->second.camera;

    // Get the world matrix
    Data::Matrix4x4 transform;
    if (!line3d.transform.empty())
    {
        auto it = entityMap.find(line3d.transform);
        if (it == entityMap.end())
        {
            // TODO: this will be moot when we parent instead of look up by name, so maybe hold off on this
            // TODO: how to deal with errors better? should probably stop rendering
            // TOOD: should probably say what frame, and what entity this was, and write to a log string that is per thread, and write that out after exiting the thread loop
            printf("Error: could not find line3d transform %s\n", line3d.transform.c_str());
            return;
        }
        if (it->second._index != Data::EntityVariant::c_index_transform)
        {
            printf("Error line3d camera was not a camera %s\n", line3d.transform.c_str());
            return;
        }
        transform = Multiply(it->second.transform.mtx, cameraEntity.viewProj);
    }
    else
    {
        transform = cameraEntity.viewProj;
    }

    // draw the line
    Data::Point2D A = ProjectPoint3DToPoint2D(line3d.A, transform);
    Data::Point2D B = ProjectPoint3DToPoint2D(line3d.B, transform);
    DrawLine(document, pixels, A, B, line3d.width, ToPremultipliedAlpha(line3d.color));
}

bool EntityLines3D_Initialize(const Data::Document& document, Data::EntityLines3D& lines3d, int entityIndex)
{
    return true;
}

bool EntityLines3D_FrameInitialize(const Data::Document& document, Data::EntityLines3D& lines3d)
{
    return true;
}

void EntityLines3D_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap, 
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityLines3D& lines3d)
{
    // TODO: make helper for getting an entity of a specific type? could return pointer for failure as null?
    // get the camera
    auto it = entityMap.find(lines3d.camera);
    if (it == entityMap.end())
    {
        // TODO: this will be moot when we parent instead of look up by name, so maybe hold off on this
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

    // Get the world matrix
    Data::Matrix4x4 transform;
    if (!lines3d.transform.empty())
    {
        auto it = entityMap.find(lines3d.transform);
        if (it == entityMap.end())
        {
            // TODO: how to deal with errors better? should probably stop rendering
            // TOOD: should probably say what frame, and what entity this was, and write to a log string that is per thread, and write that out after exiting the thread loop
            printf("Error: could not find lines3d transform %s\n", lines3d.transform.c_str());
            return;
        }
        if (it->second._index != Data::EntityVariant::c_index_transform)
        {
            printf("Error lines3d camera was not a camera %s\n", lines3d.transform.c_str());
            return;
        }
        transform = Multiply(it->second.transform.mtx, cameraEntity.viewProj);
    }
    else
    {
        transform = cameraEntity.viewProj;
    }

    // draw the lines
    Data::Point2D lastPoint = ProjectPoint3DToPoint2D(lines3d.points[0], transform);
    for (int pointIndex = 1; pointIndex < lines3d.points.size(); ++pointIndex)
    {
        Data::Point2D nextPoint = ProjectPoint3DToPoint2D(lines3d.points[pointIndex], transform);
        DrawLine(document, pixels, lastPoint, nextPoint, lines3d.width, ToPremultipliedAlpha(lines3d.color));
        lastPoint = nextPoint;
    }
}

bool EntityTransform_Initialize(const Data::Document& document, Data::EntityTransform& transform, int entityIndex)
{
    return true;
}

bool EntityTransform_FrameInitialize(const Data::Document& document, Data::EntityTransform& transform)
{
    Data::Matrix4x4 translation;
    translation.W = Data::Point4D{transform.translation.X, transform.translation.Y, transform.translation.Z, 1.0f};

    Data::Matrix4x4 scale;
    scale.X.X = transform.scale.X;
    scale.Y.Y = transform.scale.Y;
    scale.Z.Z = transform.scale.Z;

    Data::Matrix4x4 rotation = Rotation(DegreesToRadians(transform.rotation.X), DegreesToRadians(transform.rotation.Y), DegreesToRadians(transform.rotation.Z));

    transform.mtx = Multiply(Multiply(scale, rotation), translation);

    return true;
}

void EntityTransform_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityTransform& transform)
{
}

bool EntityLatex_Initialize(const Data::Document& document, Data::EntityLatex& latex, int entityIndex)
{
    char buffer[4096];

    // make the latex file
    {
        sprintf_s(buffer, "build/latex%i.tex", entityIndex);
        FILE* file = nullptr;
        fopen_s(&file, buffer, "wb");
        if (!file)
        {
            printf("Could not open file for write: %s\n", buffer);
            return false;
        }

        fprintf(file,
            "\\documentclass[preview]{standalone}\n"
            "\\begin{document}\n"
            "%s\n"
            "\\end{document}\n",
            latex.latex.c_str()
        );

        fclose(file);
    }

    // make a dvi and then convert it to a png
    {
        // dvipng -T tight -D 300 test.dvi
        sprintf_s(buffer, "%slatex.exe -output-directory=build/ build/latex%i.tex", document.config.latexbinaries.c_str(), entityIndex);
        system(buffer);

        sprintf_s(buffer, "%sdvipng.exe -T tight -D %i -o build/latex%i.png build/latex%i.dvi", document.config.latexbinaries.c_str(), latex.DPI, entityIndex, entityIndex);
        system(buffer);
    }

    // load the image
    {
        sprintf_s(buffer, "build/latex%i.png", entityIndex);

        int w, h, channels;
        stbi_uc* pixels = stbi_load(buffer, &w, &h, &channels, 1);

        if (pixels == nullptr)
        {
            printf("could not load file %s\n", buffer);
            return false;
        }

        // set the width and height and copy the pixels
        latex._width = w;
        latex._height = h;
        latex._pixels.resize(w * h);
        memcpy(latex._pixels.data(), pixels, w * h);

        stbi_image_free(pixels);
    }
    return true;
}

bool EntityLatex_FrameInitialize(const Data::Document& document, Data::EntityLatex& latex)
{
    return true;
}

void EntityLatex_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityLatex& latex)
{
    // Get the box of the latex image
    int positionX, positionY;
    CanvasToPixel(document, latex.position.X, latex.position.Y, positionX, positionY);
    int minPixelX = positionX - latex._width / 2;
    int minPixelY = positionY - latex._height / 2;
    int maxPixelX = minPixelX + latex._width;
    int maxPixelY = minPixelY + latex._height;

    // clip the bounding box to the screen
    int startPixelX = Clamp(minPixelX, 0, document.renderSizeX - 1);
    int endPixelX = Clamp(maxPixelX, 0, document.renderSizeX);
    int startPixelY = Clamp(minPixelY, 0, document.renderSizeY - 1);
    int endPixelY = Clamp(maxPixelY, 0, document.renderSizeY);

    // calculate the offset of the image (like if the first N pixels got clipped, we start N pixels in when copying)
    int offsetX = startPixelX - minPixelX;
    int offsetY = startPixelY - minPixelY;

    Data::ColorPMA bg = ToPremultipliedAlpha(latex.background);
    Data::ColorPMA fg = ToPremultipliedAlpha(latex.foreground);

    // Draw the image
    for (int iy = startPixelY; iy < endPixelY; ++iy)
    {
        const uint8_t* srcPixel = &latex._pixels[(iy - startPixelY + offsetY) * latex._width + offsetX];
        Data::ColorPMA* destPixel = &pixels[iy * document.renderSizeX + startPixelX];

        for (int ix = startPixelX; ix < endPixelX; ++ix)
        {
            Data::ColorPMA srcColor;
            Lerp(fg, bg, srcColor, float(*srcPixel) / 255.0f);

            *destPixel = Blend(*destPixel, srcColor);
            srcPixel++;
            destPixel++;
        }
    }
}

bool EntityLinearGradient_Initialize(const Data::Document& document, Data::EntityLinearGradient& linearGradient, int entityIndex)
{
    // sort the gradients by key value to make logic easier
    std::sort(
        linearGradient.points.begin(),
        linearGradient.points.end(),
        [](const Data::GradientPoint& a, const Data::GradientPoint& b)
        {
            return a.value < b.value;
        }
    );
    return true;
}

bool EntityLinearGradient_FrameInitialize(const Data::Document& document, Data::EntityLinearGradient& linearGradient)
{
    return true;
}

void EntityLinearGradient_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityLinearGradient& linearGradient)
{
    // no colors, no gradient
    if (linearGradient.points.size() == 0)
        return;

    // get the canvas extents. used for gradient projection
    float canvasMinX, canvasMinY, canvasMaxX, canvasMaxY;
    PixelToCanvas(document, 0, 0, canvasMinX, canvasMinY);
    PixelToCanvas(document, document.renderSizeX - 1, document.renderSizeY - 1, canvasMaxX, canvasMaxY);

    Data::ColorPMA* pixel = pixels.data();
    for (int iy = 0; iy < document.renderSizeY; ++iy)
    {
        float percentY = float(iy) / float(document.renderSizeY - 1);
        float canvasY = Lerp(canvasMinY, canvasMaxY, percentY);

        for (int ix = 0; ix < document.renderSizeX; ++ix)
        {
            float percentX = float(ix) / float(document.renderSizeX - 1);
            float canvasX = Lerp(canvasMinX, canvasMaxX, percentX);

            float value = Dot(linearGradient.halfSpace, Data::Point3D{ canvasX, canvasY, 1.0f });

            auto it = std::lower_bound(
                linearGradient.points.begin(),
                linearGradient.points.end(),
                value,
                [] (const Data::GradientPoint& p, float v)
                {
                    return p.value < v;
                }
            );

            // if the value is beyond the control points, use the last color
            Data::Color color;
            if (it == linearGradient.points.end())
            {
                color = linearGradient.points.rbegin()->color;
            }
            // else if the value is lower than the first control point, use the first color
            else if (it == linearGradient.points.begin())
            {
                color = linearGradient.points.begin()->color;
            }
            // else we are between two control points, do a cubic bezier interpolation
            else
            {
                int index = int(it - linearGradient.points.begin());

                float percent = (value - linearGradient.points[index - 1].value) / (linearGradient.points[index].value - linearGradient.points[index - 1].value);

                float CP0 = linearGradient.points[index].blendControlPoints[0];
                float CP1 = linearGradient.points[index].blendControlPoints[1];
                float CP2 = linearGradient.points[index].blendControlPoints[2];
                float CP3 = linearGradient.points[index].blendControlPoints[3];

                float t = CubicBezierInterpolation(CP0, CP1, CP2, CP3, percent);

                Lerp(linearGradient.points[index - 1].color, linearGradient.points[index].color, color, t);
            }

            *pixel = Blend(*pixel, ToPremultipliedAlpha(color));

            pixel++;
        }
    }
}

// TODO: i think canvas to pixel and pixel to canvase need to flip the y axis over. the gradient suggests that.  investigate to be sure.




// TODO: latex DPI is not resolution independent. smaller movie = bigger latex. should fix!

// TODO: i think the camera look at is wrong. test it and see. clip.json is not doing right things

// TODO: have a cache for latex since it's static. that means it won't be created every run. it will also stop copying those pixels around.

// TODO: could put image support in since you basically already are for latex. don't need it yet though, so...

// TODO: i think things need to parent off of scenes (to get camera) and transforms, instead of getting them by name
// TODO: re-profile & see where the time is going
// TODO: need to clip lines against the z plane! can literally just do that, shouldn't be hard, but need z projections in matrices

// TODO: how can the camera get roll etc? is parenting it to a matrix good enough?
// TOOD: support recursive matrix parenting.
// TODO: have a to grey scale operation, a multiply by color operation (and other target?) and an add color operation (and other target?), and a copy operation