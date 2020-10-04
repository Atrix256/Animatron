#include <array>
#include "schemas/types.h"
#include "utils.h"
#include <unordered_map>
#include "stb/stb_image.h"
#include "schemas/lerp.h"
#include <algorithm>

// TODO: move this to utils?

void Fill(std::vector<Data::ColorPMA>& pixels, const Data::Color& color)
{
    Data::ColorPMA colorPMA = ToPremultipliedAlpha(color);

    // if the color is fully transparent, it's a no-op
    if (color.A == 0.0f)
        return;

    // if the color is opaque just do a fill
    if (color.A >= 1.0f)
    {
        std::fill(pixels.begin(), pixels.end(), colorPMA);
        return;
    }

    // otherwise, do a blend operation
    for (Data::ColorPMA& pixel : pixels)
        pixel = Blend(pixel, colorPMA);
}



bool EntityFill_Initialize(const Data::Document& document, Data::EntityFill& fill, int entityIndex)
{
    return true;
}

bool EntityFill_FrameInitialize(const Data::Document& document, Data::EntityFill& fill)
{
    return true;
}

bool EntityFill_DoAction(
    const Data::Document& document, 
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityFill& fill)
{
    Fill(pixels, fill.color);
    return true;
}

bool EntityCircle_Initialize(const Data::Document& document, Data::EntityCircle& circle, int entityIndex)
{
    return true;
}

bool EntityCircle_FrameInitialize(const Data::Document& document, Data::EntityCircle& circle)
{
    return true;
}

bool EntityCircle_DoAction(
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

    return true;
}

bool EntityRectangle_Initialize(const Data::Document& document, Data::EntityRectangle& rectangle, int entityIndex)
{
    return true;
}

bool EntityRectangle_FrameInitialize(const Data::Document& document, Data::EntityRectangle& rectangle)
{
    return true;
}

bool EntityRectangle_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityRectangle& rectangle)
{
    Data::ColorPMA colorPMA = ToPremultipliedAlpha(rectangle.color);

    if (rectangle.expansion == 0.0f)
    {
        // Get the box of the rectangle
        int minPixelX, minPixelY, maxPixelX, maxPixelY;
        CanvasToPixel(document, rectangle.center.X - rectangle.radius.X, rectangle.center.Y - rectangle.radius.Y, minPixelX, minPixelY);
        CanvasToPixel(document, rectangle.center.X + rectangle.radius.X, rectangle.center.Y + rectangle.radius.Y, maxPixelX, maxPixelY);
        minPixelX--;
        minPixelY--;
        maxPixelX++;
        maxPixelY++;

        // clip the bounding box to the screen
        minPixelX = Clamp(minPixelX, 0, document.renderSizeX - 1);
        maxPixelX = Clamp(maxPixelX, 0, document.renderSizeX - 1);
        minPixelY = Clamp(minPixelY, 0, document.renderSizeY - 1);
        maxPixelY = Clamp(maxPixelY, 0, document.renderSizeY - 1);

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
    else
    {
        // get the canvas space box of the rectangle
        float minCanvasX, minCanvasY, maxCanvasX, maxCanvasY;
        minCanvasX = rectangle.center.X - rectangle.radius.X - rectangle.expansion;
        minCanvasY = rectangle.center.Y - rectangle.radius.Y - rectangle.expansion;
        maxCanvasX = rectangle.center.X + rectangle.radius.X + rectangle.expansion;
        maxCanvasY = rectangle.center.Y + rectangle.radius.Y + rectangle.expansion;

        // Get the pixel space box of the rectangle
        int minPixelX, minPixelY, maxPixelX, maxPixelY;
        CanvasToPixel(document, minCanvasX, minCanvasY, minPixelX, minPixelY);
        CanvasToPixel(document, maxCanvasX, maxCanvasY, maxPixelX, maxPixelY);
        minPixelX--;
        minPixelY--;
        maxPixelX++;
        maxPixelY++;

        // clip the bounding box to the screen
        minPixelX = Clamp(minPixelX, 0, document.renderSizeX - 1);
        maxPixelX = Clamp(maxPixelX, 0, document.renderSizeX - 1);
        minPixelY = Clamp(minPixelY, 0, document.renderSizeY - 1);
        maxPixelY = Clamp(maxPixelY, 0, document.renderSizeY - 1);

        // Draw the rectangle
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

                    float percentX = float(ix + offset.X - minPixelX) / float(maxPixelX - minPixelX);
                    float canvasX = Lerp(minCanvasX, maxCanvasX, percentX);

                    float percentY = float(iy + offset.Y - minPixelY) / float(maxPixelY - minPixelY);
                    float canvasY = Lerp(minCanvasY, maxCanvasY, percentY);

                    float dist = sdBox(vec2{ canvasX, canvasY }, vec2{ rectangle.center.X, rectangle.center.Y }, vec2{ rectangle.radius.X, rectangle.radius.Y });

                    if (dist <= rectangle.expansion)
                    {
                        samplesColor.R += colorPMA.R / float(document.samplesPerPixel);
                        samplesColor.G += colorPMA.G / float(document.samplesPerPixel);
                        samplesColor.B += colorPMA.B / float(document.samplesPerPixel);
                        samplesColor.A += colorPMA.A / float(document.samplesPerPixel);
                    }
                }

                *pixel = Blend(*pixel, samplesColor);

                pixel++;
            }
        }
    }

    return true;
}

bool EntityLine_Initialize(const Data::Document& document, Data::EntityLine& line, int entityIndex)
{
    return true;
}

bool EntityLine_FrameInitialize(const Data::Document& document, Data::EntityLine& line)
{
    return true;
}

bool EntityLine_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityLine& line)
{
    DrawLine(document, pixels, line.A, line.B, line.width, ToPremultipliedAlpha(line.color));
    return true;
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

bool EntityCamera_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityCamera& camera)
{
    // nothing to do for a camera
    return true;
}

bool EntityLine3D_Initialize(const Data::Document& document, Data::EntityLine3D& line3d, int entityIndex)
{
    return true;
}

bool EntityLine3D_FrameInitialize(const Data::Document& document, Data::EntityLine3D& line3d)
{
    return true;
}

bool EntityLine3D_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityLine3D& line3d)
{
    // get the camera
    auto it = entityMap.find(line3d.camera);
    if (it == entityMap.end())
    {
        printf("Error: could not find line3d camera %s\n", line3d.camera.c_str());
        return false;
    }
    if (it->second._index != Data::EntityVariant::c_index_camera)
    {
        printf("Error line3d camera was not a camera %s\n", line3d.camera.c_str());
        return false;
    }
    const Data::EntityCamera& cameraEntity = it->second.camera;

    // Get the world matrix
    Data::Matrix4x4 transform;
    if (!line3d.transform.empty())
    {
        auto it = entityMap.find(line3d.transform);
        if (it == entityMap.end())
        {
            printf("Error: could not find line3d transform %s\n", line3d.transform.c_str());
            return false;
        }
        if (it->second._index != Data::EntityVariant::c_index_transform)
        {
            printf("Error line3d camera was not a camera %s\n", line3d.transform.c_str());
            return false;
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

    return true;
}

bool EntityLines3D_Initialize(const Data::Document& document, Data::EntityLines3D& lines3d, int entityIndex)
{
    return true;
}

bool EntityLines3D_FrameInitialize(const Data::Document& document, Data::EntityLines3D& lines3d)
{
    return true;
}

bool EntityLines3D_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap, 
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityLines3D& lines3d)
{
    // get the camera
    auto it = entityMap.find(lines3d.camera);
    if (it == entityMap.end())
    {
        printf("Error: could not find lines3d camera %s\n", lines3d.camera.c_str());
        return false;
    }
    if (it->second._index != Data::EntityVariant::c_index_camera)
    {
        printf("Error lines3d camera was not a camera %s\n", lines3d.camera.c_str());
        return false;
    }
    const Data::EntityCamera& cameraEntity = it->second.camera;

    // need at least 2 points to make a line
    if (lines3d.points.size() < 2)
        return true;

    // Get the world matrix
    Data::Matrix4x4 transform;
    if (!lines3d.transform.empty())
    {
        auto it = entityMap.find(lines3d.transform);
        if (it == entityMap.end())
        {
            printf("Error: could not find lines3d transform %s\n", lines3d.transform.c_str());
            return false;
        }
        if (it->second._index != Data::EntityVariant::c_index_transform)
        {
            printf("Error lines3d camera was not a camera %s\n", lines3d.transform.c_str());
            return false;
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

    return true;
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

bool EntityTransform_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityTransform& transform)
{
    return true;
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

bool EntityLatex_DoAction(
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

    return true;
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

bool EntityLinearGradient_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityLinearGradient& linearGradient)
{
    // no colors, no gradient
    if (linearGradient.points.size() == 0)
        return true;

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

    return true;
}

bool EntityDigitalDissolve_Initialize(const Data::Document& document, Data::EntityDigitalDissolve& digitalDissolve, int entityIndex)
{
    return true;
}

bool EntityDigitalDissolve_FrameInitialize(const Data::Document& document, Data::EntityDigitalDissolve& digitalDissolve)
{
    return true;
}

bool EntityDigitalDissolve_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityDigitalDissolve& digitalDissolve)
{
    if (digitalDissolve.type != Data::DigitalDissolveType::BlueNoise)
    {
        printf("unknown digital dissolve type encountered!\n");
        return false;
    }

    // full background
    if (digitalDissolve.alpha <= 0.0f)
    {
        Fill(pixels, digitalDissolve.background);
        return true;
    }

    // full foreground
    if (digitalDissolve.alpha >= 1.0f)
    {
        Fill(pixels, digitalDissolve.foreground);
        return true;
    }

    // convert colors to PMA
    Data::ColorPMA bg = ToPremultipliedAlpha(digitalDissolve.background);
    Data::ColorPMA fg = ToPremultipliedAlpha(digitalDissolve.foreground);

    // do blending
    for (size_t iy = 0; iy < document.renderSizeY; ++iy)
    {
        size_t bny = size_t(float(iy) / digitalDissolve.scale.Y);

        Data::ColorPMA* pixel = &pixels[iy * document.renderSizeX];
        const Data::ColorU8* blueNoiseRow = &document.blueNoisePixels[(bny % document.blueNoiseHeight) * document.blueNoiseWidth];
        for (size_t ix = 0; ix < document.renderSizeX; ++ix)
        {
            size_t bnx = size_t(float(ix) / digitalDissolve.scale.X);

            float ditherValue = (blueNoiseRow[bnx % document.blueNoiseWidth].R) / 255.0f;
            
            if (ditherValue <= digitalDissolve.alpha)
                *pixel = Blend(*pixel, fg);
            else
                *pixel = Blend(*pixel, bg);

            pixel++;
        }
    }
    
    return true;
}

bool EntityImage_Initialize(const Data::Document& document, Data::EntityImage& image, int entityIndex)
{
    int channels;
    stbi_uc* pixels = stbi_load(image.fileName.c_str(), &image._rawwidth, &image._rawheight, &channels, 4);
    if (pixels == nullptr)
    {
        printf("Could not load image %s.\n", image.fileName.c_str());
        return false;
    }

    image._rawpixels.resize(image._rawwidth* image._rawheight);
    const Data::ColorU8* pixel = (Data::ColorU8*)pixels;

    for (size_t index = 0; index < image._rawpixels.size(); ++index)
    {
        Data::Color color{ float(pixel->R) / 255.0f, float(pixel->G) / 255.0f, float(pixel->B) / 255.0f, float(pixel->A) / 255.0f };
        color.R = SRGBToLinear(color.R);
        color.G = SRGBToLinear(color.G);
        color.B = SRGBToLinear(color.B);
        color.A = SRGBToLinear(color.A);
        image._rawpixels[index] = ToPremultipliedAlpha(color);
        pixel++;
    }

    stbi_image_free(pixels);
    return true;
}

bool EntityImage_FrameInitialize(const Data::Document& document, Data::EntityImage& image)
{
    // calculate the desired width of the image, in pixels
    Data::Point2D canvasMin, canvasMax;
    canvasMin = image.position - image.radius;
    canvasMax = image.position + image.radius;
    int pixelMinX, pixelMinY, pixelMaxX, pixelMaxY;
    CanvasToPixel(document, canvasMin.X, canvasMin.Y, pixelMinX, pixelMinY);
    CanvasToPixel(document, canvasMax.X, canvasMax.Y, pixelMaxX, pixelMaxY);

    // if it's already the right size, nothing to do
    int desiredWidth = pixelMaxX - pixelMinX;
    int desiredHeight = pixelMaxY - pixelMinY;
    if (image._width == desiredWidth && image._height == desiredHeight)
        return true;

    // resize
    image._pixels = image._rawpixels;
    image._width = desiredWidth;
    image._height = desiredHeight;
    Resize(image._pixels, image._rawwidth, image._rawheight, desiredWidth, desiredHeight);

    return true;
}

bool EntityImage_DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::EntityVariant>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::EntityImage& image)
{
    // calculate the begin and end of the image
    Data::Point2D canvasMin, canvasMax;
    canvasMin = image.position - image.radius;
    canvasMax = image.position + image.radius;
    int pixelMinX, pixelMinY, pixelMaxX, pixelMaxY;
    CanvasToPixel(document, canvasMin.X, canvasMin.Y, pixelMinX, pixelMinY);
    CanvasToPixel(document, canvasMax.X, canvasMax.Y, pixelMaxX, pixelMaxY);

    // clip the image to the screen
    int srcOffsetX = 0;
    int srcOffsetY = 0;
    if (pixelMinX < 0)
        srcOffsetX = pixelMinX * -1;
    if (pixelMinY < 0)
        srcOffsetY = pixelMinY * -1;
    pixelMinX = Clamp(pixelMinX, 0, document.renderSizeX);
    pixelMaxX = Clamp(pixelMaxX, 0, document.renderSizeX);
    pixelMinY = Clamp(pixelMinY, 0, document.renderSizeY);
    pixelMaxY = Clamp(pixelMaxY, 0, document.renderSizeY);

    // paste the image
    Data::ColorPMA tint = ToPremultipliedAlpha(image.tint);
    for (size_t iy = pixelMinY; iy < pixelMaxY; ++iy)
    {
        Data::ColorPMA* destPixel = &pixels[iy * document.renderSizeX + pixelMinX];
        const Data::ColorPMA* srcPixel = &image._pixels[(iy - pixelMinY + srcOffsetY) * image._width + srcOffsetX];

        for (size_t ix = pixelMinX; ix < pixelMaxX; ++ix)
        {
            *destPixel = Blend(*destPixel, *srcPixel * tint);
            srcPixel++;
            destPixel++;
        }
    }

    return true;
}