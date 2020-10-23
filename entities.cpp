#include <array>
#include "schemas/types.h"
#include "utils.h"
#include <unordered_map>
#include "stb/stb_image.h"
#include "schemas/lerp.h"
#include <algorithm>
#include "entities.h"
#include "schemas/hash.h"
#include "cas.h"
#include "animatron.h"

#include <Windows.h>

void Run(const char* program, char* commandLine, const char* currentDirectory, bool waitForFinish = true)
{
    // additional information
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    // set the size of the structures
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // start the program up
    bool ret = CreateProcessA(program,   // the path
        commandLine,        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        CREATE_NO_WINDOW,
        NULL,           // Use parent's environment block
        currentDirectory,
        &si,            // Pointer to STARTUPINFO structure
        &pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
    );

    // wait for it if we should
    if (ret && waitForFinish)
        WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

bool EntityCircle_Action::DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::Entity>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::Entity& entity,
    int threadId,
    const EntityActionFrameContext& context)
{
    const Data::EntityCircle& circle = entity.data.circle;
    Data::Point2D center = circle.center + Point3D_XY(GetParentPosition(document, entityMap, entity));

    // Get a pixel space bounding box of the circle
    int minPixelX, minPixelY, maxPixelX, maxPixelY;
    GetPixelBoundingBox_PointRadius(document, center.X, center.Y, circle.innerRadius + circle.outerRadius, circle.innerRadius + circle.outerRadius, minPixelX, minPixelY, maxPixelX, maxPixelY);

    // clip the bounding box to the screen
    minPixelX = Clamp(minPixelX, 0, document.renderSizeX - 1);
    maxPixelX = Clamp(maxPixelX, 0, document.renderSizeX - 1);
    minPixelY = Clamp(minPixelY, 0, document.renderSizeY - 1);
    maxPixelY = Clamp(maxPixelY, 0, document.renderSizeY - 1);

    Data::ColorPMA colorPMA = ToPremultipliedAlpha(circle.color);

    // Draw the circle
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
                PixelToCanvas(document, float(ix) + offset.X, float(iy) + offset.Y, canvasX, canvasY);
                
                float distX = abs(canvasX - center.X);
                float distY = abs(canvasY - center.Y);
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

bool EntityRectangle_Action::DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::Entity>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::Entity& entity,
    int threadId,
    const EntityActionFrameContext& context)
{
    const Data::EntityRectangle& rectangle = entity.data.rectangle;
    Data::ColorPMA colorPMA = ToPremultipliedAlpha(rectangle.color);
    Data::Point2D center = rectangle.center + Point3D_XY(GetParentPosition(document, entityMap, entity));

    // Get the box of the rectangle
    int minPixelX, minPixelY, maxPixelX, maxPixelY;
    GetPixelBoundingBox_PointRadius(document, center.X, center.Y, rectangle.radius.X + rectangle.expansion, rectangle.radius.Y + rectangle.expansion, minPixelX, minPixelY, maxPixelX, maxPixelY);

    // clip the bounding box to the screen
    minPixelX = Clamp(minPixelX, 0, document.renderSizeX - 1);
    maxPixelX = Clamp(maxPixelX, 0, document.renderSizeX - 1);
    minPixelY = Clamp(minPixelY, 0, document.renderSizeY - 1);
    maxPixelY = Clamp(maxPixelY, 0, document.renderSizeY - 1);

    if (rectangle.expansion == 0.0f)
    {
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

                    float canvasX, canvasY;
                    PixelToCanvas(document, float(ix) + offset.X, float(iy) + offset.Y, canvasX, canvasY);

                    float dist = sdBox(vec2{ canvasX, canvasY }, vec2{ center.X, center.Y }, vec2{ rectangle.radius.X, rectangle.radius.Y });

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

bool EntityCamera_Action::FrameInitialize(const Data::Document& document, Data::Entity& entity, const EntityActionFrameContext& context)
{
    Data::EntityCamera& camera = entity.data.camera;
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

        float zf = camera.farZ;
        float zn = camera.nearZ;

        projMtx.X = Data::Point4D{ xscale,   0.0f,             0.0f,  0.0f };
        projMtx.Y = Data::Point4D{   0.0f, yscale,             0.0f,  0.0f };
        projMtx.Z = Data::Point4D{   0.0f,   0.0f,   zf / (zn - zf), -0.01f };
        projMtx.W = Data::Point4D{   0.0f,   0.0f, zn*zf/ (zn - zf),  0.0f };
    }
    else
    {
        // ortho matrix leaving x and y alone so it's the same results as if using canvas space coordinates
        float zf = camera.farZ;
        float zn = camera.nearZ;

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

bool EntityLine3D_Action::DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::Entity>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::Entity& entity,
    int threadId,
    const EntityActionFrameContext& context)
{
    const Data::EntityLine3D& line3d = entity.data.line3d;
    Data::Point3D offset = GetParentPosition(document, entityMap, entity);

    // get the camera
    auto it = entityMap.find(line3d.camera);
    if (it == entityMap.end())
    {
        printf("Error: could not find line3d camera %s\n", line3d.camera.c_str());
        return false;
    }
    if (it->second.data._index != Data::EntityVariant::c_index_camera)
    {
        printf("Error line3d camera was not a camera %s\n", line3d.camera.c_str());
        return false;
    }
    const Data::EntityCamera& cameraEntity = it->second.data.camera;

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
        if (it->second.data._index != Data::EntityVariant::c_index_transform)
        {
            printf("Error line3d camera was not a camera %s\n", line3d.transform.c_str());
            return false;
        }
        transform = Multiply(it->second.data.transform.mtx, cameraEntity.viewProj);
    }
    else
    {
        transform = cameraEntity.viewProj;
    }

    // draw the line
    Data::Point2D A = ProjectPoint3DToPoint2D(line3d.A + offset, transform);
    Data::Point2D B = ProjectPoint3DToPoint2D(line3d.B + offset, transform);
    DrawLine(document, pixels, A, B, line3d.width, ToPremultipliedAlpha(line3d.color));

    return true;
}

bool EntityLines3D_Action::DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::Entity>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::Entity& entity,
    int threadId,
    const EntityActionFrameContext& context)
{
    const Data::EntityLines3D& lines3d = entity.data.lines3d;
    Data::Point3D offset = GetParentPosition(document, entityMap, entity);

    // get the camera
    auto it = entityMap.find(lines3d.camera);
    if (it == entityMap.end())
    {
        printf("Error: could not find lines3d camera %s\n", lines3d.camera.c_str());
        return false;
    }
    if (it->second.data._index != Data::EntityVariant::c_index_camera)
    {
        printf("Error lines3d camera was not a camera %s\n", lines3d.camera.c_str());
        return false;
    }
    const Data::EntityCamera& cameraEntity = it->second.data.camera;

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
        if (it->second.data._index != Data::EntityVariant::c_index_transform)
        {
            printf("Error lines3d camera was not a camera %s\n", lines3d.transform.c_str());
            return false;
        }
        transform = Multiply(it->second.data.transform.mtx, cameraEntity.viewProj);
    }
    else
    {
        transform = cameraEntity.viewProj;
    }

    // draw the lines
    Data::Point2D lastPoint = ProjectPoint3DToPoint2D(lines3d.points[0] + offset, transform);
    for (int pointIndex = 1; pointIndex < lines3d.points.size(); ++pointIndex)
    {
        Data::Point2D nextPoint = ProjectPoint3DToPoint2D(lines3d.points[pointIndex] + offset, transform);
        DrawLine(document, pixels, lastPoint, nextPoint, lines3d.width, ToPremultipliedAlpha(lines3d.color));
        lastPoint = nextPoint;
    }

    return true;
}

bool EntityTransform_Action::FrameInitialize(const Data::Document& document, Data::Entity& entity, const EntityActionFrameContext& context)
{
    Data::EntityTransform& transform = entity.data.transform;

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

static bool GetOrMakeLatexImage(const char* latexBinaries, const char* latex, int DPI, uint32_t& width, uint32_t& height, unsigned char*& pixels, int threadId)
{
    // try and get the data from the CAS
    size_t hash = 0;
    Hash(hash, latex);
    Hash(hash, DPI);
    unsigned char* data = (unsigned char*)CAS::Get().Get(hash);

    // if it doesn't exist, create it
    if (!data)
    {
        char buffer[4096];
        char buffer2[4096];

        // make the latex file
        {
            sprintf_s(buffer, "build/latex%i.tex", threadId);
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
                latex
            );

            fclose(file);
        }

        // make a dvi and then convert it to a png
        {
            char currentDirectory[1024];
            GetCurrentDirectoryA(1024, currentDirectory);
            strcat_s(currentDirectory, "\\build\\");

            sprintf_s(buffer, "%slatex.exe", latexBinaries);
            sprintf_s(buffer2, "-output-directory=./ -interaction=nonstopmode latex%i.tex", threadId);
            Run(buffer, buffer2, currentDirectory);

            sprintf_s(buffer, "%sdvipng.exe", latexBinaries);
            sprintf_s(buffer2, "-T tight -D %i -o latex%i.png latex%i.dvi", DPI, threadId, threadId);
            Run(buffer, buffer2, currentDirectory);
        }

        // load the image, store it in the cache and then get it again
        {
            sprintf_s(buffer, "build/latex%i.png", threadId);

            int w, h, channels;
            stbi_uc* filePixels = stbi_load(buffer, &w, &h, &channels, 1);

            if (filePixels == nullptr)
            {
                printf("could not load file %s\n", buffer);
                return false;
            }

            // store this data in the CAS
            width = w;
            height = h;
            std::vector<unsigned char> newData;
            newData.resize(sizeof(width) + sizeof(height) + width * height);
            *((uint32_t*)&newData[sizeof(uint32_t) * 0]) = width;
            *((uint32_t*)&newData[sizeof(uint32_t) * 1]) = height;
            memcpy(&newData[sizeof(uint32_t) * 2], filePixels, width * height);
            CAS::Set(hash, newData, false);

            // free the memory
            stbi_image_free(filePixels);
        }

        // Get the data from the CAS now that we have set it
        data = (unsigned char*)CAS::Get().Get(hash);
    }

    // set the data we got from the CAS
    width = ((uint32_t*)data)[0];
    height = ((uint32_t*)data)[1];
    pixels = &data[sizeof(uint32_t) * 2];

    return true;
}

bool EntityLatex_Action::DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::Entity>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::Entity& entity,
    int threadId,
    const EntityActionFrameContext& context)
{
    const Data::EntityLatex& latex = entity.data.latex;

    uint32_t imageWidth, imageHeight;
    unsigned char* imagePixels;
    {
        // At 1920x1080, a scale of 1.0 gives you 300 DPI rendering from latex.
        // Not the most elegant thing, but it makes it resolution independent.
        int DPI = int((float(CanvasSizeInPixels(document)) / 1080.0f) * latex.scale * 300.0f);

        if (!GetOrMakeLatexImage(document.config.latexbinaries.c_str(), latex.latex.c_str(), DPI, imageWidth, imageHeight, imagePixels, threadId))
            return false;
    }

    Data::Point2D offset = Point3D_XY(GetParentPosition(document, entityMap, entity));

    // Get the box of the latex image
    int positionX, positionY;
    CanvasToPixel(document, latex.position.X + offset.X, latex.position.Y + offset.Y, positionX, positionY);
    int minPixelX = positionX - imageWidth / 2;
    int minPixelY = positionY - imageHeight / 2;
    int maxPixelX = minPixelX + imageWidth;
    int maxPixelY = minPixelY + imageHeight;

    // if it's completely off the screen, nothing to do
    if (minPixelX > document.renderSizeX - 1 || maxPixelX < 0 ||
        maxPixelY > document.renderSizeY - 1 || maxPixelY < 0)
    {
        return true;
    }

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
        const uint8_t* srcPixel = &imagePixels[(iy - startPixelY + offsetY) * imageWidth + offsetX];
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

bool EntityLinearGradient_Action::Initialize(const Data::Document& document, Data::Entity& entity, int entityIndex)
{
    Data::EntityLinearGradient& linearGradient = entity.data.linearGradient;

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

bool EntityLinearGradient_Action::DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::Entity>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::Entity& entity,
    int threadId,
    const EntityActionFrameContext& context)
{
    const Data::EntityLinearGradient& linearGradient = entity.data.linearGradient;

    // no colors, no gradient
    if (linearGradient.points.size() == 0)
        return true;

    // draw the gradient
    Data::ColorPMA* pixel = pixels.data();
    for (int iy = 0; iy < document.renderSizeY; ++iy)
    {
        for (int ix = 0; ix < document.renderSizeX; ++ix)
        {
            float canvasX, canvasY;
            PixelToCanvas(document, float(ix), float(iy), canvasX, canvasY);

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

                float CP0 = linearGradient.points[index].blendControlPoints.A;
                float CP1 = linearGradient.points[index].blendControlPoints.B;
                float CP2 = linearGradient.points[index].blendControlPoints.C;
                float CP3 = linearGradient.points[index].blendControlPoints.D;

                float t = CubicBezierInterpolation(CP0, CP1, CP2, CP3, percent);

                Lerp(linearGradient.points[index - 1].color, linearGradient.points[index].color, color, t);
            }

            *pixel = Blend(*pixel, ToPremultipliedAlpha(color));

            pixel++;
        }
    }

    return true;
}

bool EntityDigitalDissolve_Action::DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::Entity>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::Entity& entity,
    int threadId,
    const EntityActionFrameContext& context)
{
    const Data::EntityDigitalDissolve& digitalDissolve = entity.data.digitalDissolve;

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

    // resolution independent scaling
    float resolutionScale = float(CanvasSizeInPixels(document)) / 1080.0f;

    // do blending
    for (size_t iy = 0; iy < document.renderSizeY; ++iy)
    {
        size_t bny = size_t(float(iy) / (digitalDissolve.scale.Y * resolutionScale));

        Data::ColorPMA* pixel = &pixels[iy * document.renderSizeX];
        const Data::ColorU8* blueNoiseRow = &document.blueNoisePixels[(bny % document.blueNoiseHeight) * document.blueNoiseWidth];
        for (size_t ix = 0; ix < document.renderSizeX; ++ix)
        {
            size_t bnx = size_t(float(ix) / (digitalDissolve.scale.X * resolutionScale));

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

bool EntityImage_Action::Initialize(const Data::Document& document, Data::Entity& entity, int entityIndex)
{
    Data::EntityImage& image = entity.data.image;

    int channels;
    stbi_uc* pixels = stbi_load(image.fileName.c_str(), &image._image.rawwidth, &image._image.rawheight, &channels, 4);
    if (pixels == nullptr)
    {
        printf("Could not load image %s.\n", image.fileName.c_str());
        return false;
    }

    image._image.rawpixels.resize(image._image.rawwidth* image._image.rawheight);
    const Data::ColorU8* pixel = (Data::ColorU8*)pixels;

    for (size_t index = 0; index < image._image.rawpixels.size(); ++index)
    {
        Data::Color color{ float(pixel->R) / 255.0f, float(pixel->G) / 255.0f, float(pixel->B) / 255.0f, float(pixel->A) / 255.0f };
        color.R = SRGBToLinear(color.R);
        color.G = SRGBToLinear(color.G);
        color.B = SRGBToLinear(color.B);
        color.A = SRGBToLinear(color.A);
        image._image.rawpixels[index] = ToPremultipliedAlpha(color);
        pixel++;
    }

    stbi_image_free(pixels);
    return true;
}

bool EntityImage_Action::FrameInitialize(const Data::Document& document, Data::Entity& entity, const EntityActionFrameContext& context)
{
    Data::EntityImage& image = entity.data.image;

    // calculate the desired width of the image, in pixels
    int pixelMinX, pixelMinY, pixelMaxX, pixelMaxY;
    GetPixelBoundingBox_PointRadius(document, image.position.X, image.position.Y, image.radius.X, image.radius.Y, pixelMinX, pixelMinY, pixelMaxX, pixelMaxY);

    // if it's already the right size, nothing to do
    int desiredWidth = pixelMaxX - pixelMinX;
    int desiredHeight = pixelMaxY - pixelMinY;
    if (image._image.width == desiredWidth && image._image.height == desiredHeight)
        return true;

    // resize
    image._image.pixels = image._image.rawpixels;
    image._image.width = desiredWidth;
    image._image.height = desiredHeight;
    Resize(image._image.pixels, image._image.rawwidth, image._image.rawheight, desiredWidth, desiredHeight);

    return true;
}

bool EntityImage_Action::DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::Entity>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::Entity& entity,
    int threadId,
    const EntityActionFrameContext& context)
{
    const Data::EntityImage& image = entity.data.image;

    // calculate the begin and end of the image
    int pixelMinX, pixelMinY, pixelMaxX, pixelMaxY;
    GetPixelBoundingBox_PointRadius(document, image.position.X, image.position.Y, image.radius.X, image.radius.Y, pixelMinX, pixelMinY, pixelMaxX, pixelMaxY);

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
        const Data::ColorPMA* srcPixel = &image._image.pixels[(iy - pixelMinY + srcOffsetY) * image._image.width + srcOffsetX];

        for (size_t ix = pixelMinX; ix < pixelMaxX; ++ix)
        {
            *destPixel = Blend(*destPixel, *srcPixel * tint);
            srcPixel++;
            destPixel++;
        }
    }

    return true;
}

bool EntityFlipbook_Action::Initialize(const Data::Document& document, Data::Entity& entity, int entityIndex)
{
    Data::EntityFlipbook& flipbook = entity.data.flipbook;
    if (flipbook.fileNames.empty())
    {
        printf("No images specified for flipbook.\n");
        return false;
    }

    int channels = 0;

    flipbook._images.reserve(flipbook.fileNames.size());
    for (std::string& fileName : flipbook.fileNames)
    {
        flipbook._images.resize(flipbook._images.size() + 1);
        Data::LoadedImage& image = flipbook._images.back();

        stbi_uc* pixels = stbi_load(fileName.c_str(), &image.rawwidth, &image.rawheight, &channels, 4);

        if (pixels == nullptr)
        {
            printf("Could not load image %s.\n", fileName.c_str());
            return false;
        }

        image.rawpixels.resize(image.rawwidth * image.rawheight);
        const Data::ColorU8* pixel = (Data::ColorU8*)pixels;

        for (size_t index = 0; index < image.rawpixels.size(); ++index)
        {
            Data::Color color{ float(pixel->R) / 255.0f, float(pixel->G) / 255.0f, float(pixel->B) / 255.0f, float(pixel->A) / 255.0f };
            color.R = SRGBToLinear(color.R);
            color.G = SRGBToLinear(color.G);
            color.B = SRGBToLinear(color.B);
            color.A = SRGBToLinear(color.A);
            image.rawpixels[index] = ToPremultipliedAlpha(color);
            pixel++;
        }

        stbi_image_free(pixels);
    }
    return true;
}

bool EntityFlipbook_Action::FrameInitialize(const Data::Document& document, Data::Entity& entity, const EntityActionFrameContext& context)
{
    Data::EntityFlipbook& flipbook = entity.data.flipbook;

    // get the image we are using this frame
    int imageIndex = GetImageIndex(document, entity, context);
    Data::LoadedImage& image = flipbook._images[imageIndex];

    // calculate the desired width of the flipbook, in pixels
    int pixelMinX, pixelMinY, pixelMaxX, pixelMaxY;
    GetPixelBoundingBox_PointRadius(document, flipbook.position.X, flipbook.position.Y, flipbook.radius.X, flipbook.radius.Y, pixelMinX, pixelMinY, pixelMaxX, pixelMaxY);

    // if it's already the right size, nothing to do
    int desiredWidth = pixelMaxX - pixelMinX;
    int desiredHeight = pixelMaxY - pixelMinY;
    if (image.width == desiredWidth && image.height == desiredHeight)
        return true;

    // resize
    image.pixels = image.rawpixels;
    image.width = desiredWidth;
    image.height = desiredHeight;
    Resize(image.pixels, image.rawwidth, image.rawheight, desiredWidth, desiredHeight);

    return true;
}

bool EntityFlipbook_Action::DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::Entity>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::Entity& entity,
    int threadId,
    const EntityActionFrameContext& context)
{
    const Data::EntityFlipbook& flipbook = entity.data.flipbook;

    // get the image we are using this frame
    int imageIndex = GetImageIndex(document, entity, context);
    const Data::LoadedImage& image = flipbook._images[imageIndex];

    // calculate the begin and end of the image
    int pixelMinX, pixelMinY, pixelMaxX, pixelMaxY;
    GetPixelBoundingBox_PointRadius(document, flipbook.position.X, flipbook.position.Y, flipbook.radius.X, flipbook.radius.Y, pixelMinX, pixelMinY, pixelMaxX, pixelMaxY);

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
    Data::ColorPMA tint = ToPremultipliedAlpha(flipbook.tint);
    for (size_t iy = pixelMinY; iy < pixelMaxY; ++iy)
    {
        Data::ColorPMA* destPixel = &pixels[iy * document.renderSizeX + pixelMinX];
        const Data::ColorPMA* srcPixel = &image.pixels[(iy - pixelMinY + srcOffsetY) * image.width + srcOffsetX];

        for (size_t ix = pixelMinX; ix < pixelMaxX; ++ix)
        {
            *destPixel = Blend(*destPixel, *srcPixel * tint);
            srcPixel++;
            destPixel++;
        }
    }

    return true;
}

struct CubicBezierData
{
    struct CurvePoint
    {
        float t;
        float x, y;
    };

    uint32_t pointCount = 0;
    CurvePoint* points = nullptr;
};

static void GetOrMakeCubicBezierData(const Data::Document& document, const Data::EntityCubicBezier& cubicBezier, CubicBezierData& cubicBezierData)
{
    // hash the input
    size_t hash = 0;
    Hash(hash, document.renderSizeX);
    Hash(hash, document.renderSizeY);
    Hash(hash, cubicBezier.A);
    Hash(hash, cubicBezier.B);
    Hash(hash, cubicBezier.C);
    Hash(hash, cubicBezier.D);

    const Data::Point3D& A = cubicBezier.A;
    const Data::Point3D& B = cubicBezier.B;
    const Data::Point3D& C = cubicBezier.C;
    const Data::Point3D& D = cubicBezier.D;

    // if the data isn't already in the CAS make it and then put it in
    void* data = CAS::Get().Get(hash);
    if (!data)
    {
        // first we want to turn the curve into a bunch of line segments which are no more than 1 pixel long
        std::vector<CubicBezierData::CurvePoint> points;
        auto InsertCurvePoint = [&](float t)
        {
            float cz = CubicBezierInterpolation(A.Z, B.Z, C.Z, D.Z, t);

            float cx = CubicBezierInterpolation(A.X * A.Z, B.X * B.Z, C.X * C.Z, D.X * D.Z, t);
            cx /= cz;

            float cy = CubicBezierInterpolation(A.Y * A.Z, B.Y * B.Z, C.Y * C.Z, D.Y * D.Z, t);
            cy /= cz;

            float px, py;
            CanvasToPixelFloat(document, cx, cy, px, py);

            points.push_back({ t, px, py });
            std::sort(points.begin(), points.end(), [](const CubicBezierData::CurvePoint& A, const CubicBezierData::CurvePoint& B) { return A.t < B.t; });
        };
        InsertCurvePoint(0.0f);
        InsertCurvePoint(1.0f);
        int index = 0;
        while (index + 1 < points.size())
        {
            vec2 a = vec2{ points[index].x, points[index].y };
            vec2 b = vec2{ points[index + 1].x, points[index + 1].y };

            float length = Length(b - a);
            if (length > 1.0f)
            {
                InsertCurvePoint((points[index].t + points[index + 1].t) / 2.0f);
            }
            else
            {
                index++;
            }
        }

        // Now sort the points by x coordinate, so that we can do dimensional reduction and only test the points near our point on one axis
        std::sort(points.begin(), points.end(), [](const CubicBezierData::CurvePoint& A, const CubicBezierData::CurvePoint& B) { return A.x < B.x; });

        // put the data into contiguous memory and put it in the CAS
        std::vector<unsigned char> newData;
        newData.resize(sizeof(uint32_t) + points.size() * sizeof(points[0]));
        *((uint32_t*)&newData[0]) = (uint32_t)points.size();
        memcpy(&newData[sizeof(uint32_t)], points.data(), points.size() * sizeof(points[0]));

        // set the data
        CAS::Set(hash, newData, false);

        // get the data
        data = CAS::Get().Get(hash);
    }

    // Fill out the data from the CAS
    cubicBezierData.pointCount = ((uint32_t*)data)[0];
    cubicBezierData.points = (CubicBezierData::CurvePoint*) &((unsigned char*)data)[sizeof(uint32_t)];
}

bool EntityCubicBezier_Action::DoAction(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::Entity>& entityMap,
    std::vector<Data::ColorPMA>& pixels,
    const Data::Entity& entity,
    int threadId,
    const EntityActionFrameContext& context)
{
    const Data::EntityCubicBezier& cubicBezier = entity.data.cubicBezier;
    Data::ColorPMA colorPMA = ToPremultipliedAlpha(cubicBezier.color);

    Data::Point2D offsetCanvas = Point3D_XY(GetParentPosition(document, entityMap, entity));
    Data::Point2D offsetPx;
    CanvasOffsetToPixelOffset(document, offsetCanvas.X, offsetCanvas.Y, offsetPx.X, offsetPx.Y);

    // get or make the cached bezier data (expensive to calculate each frame)
    CubicBezierData cubicBezierData;
    GetOrMakeCubicBezierData(document, cubicBezier, cubicBezierData);

    Data::Point3D A = ToPoint3D(offsetCanvas) + cubicBezier.A;
    Data::Point3D B = ToPoint3D(offsetCanvas) + cubicBezier.B;
    Data::Point3D C = ToPoint3D(offsetCanvas) + cubicBezier.C;
    Data::Point3D D = ToPoint3D(offsetCanvas) + cubicBezier.D;

    // get the bounding box of the curve, from the bounding box of its control points
    float minCanvasX, minCanvasY, maxCanvasX, maxCanvasY;
    minCanvasX = Min(A.X, B.X, C.X, D.X);
    maxCanvasX = Max(A.X, B.X, C.X, D.X);
    minCanvasY = Min(A.Y, B.Y, C.Y, D.Y);
    maxCanvasY = Max(A.Y, B.Y, C.Y, D.Y);

    // Get the pixel space bounding box
    int minPixelX, minPixelY, maxPixelX, maxPixelY;
    GetPixelBoundingBox_TwoPoints(document, minCanvasX, minCanvasY, maxCanvasX, maxCanvasY, minPixelX, minPixelY, maxPixelX, maxPixelY);

    // clip the bounding box to the screen
    minPixelX = Clamp(minPixelX, 0, document.renderSizeX - 1);
    maxPixelX = Clamp(maxPixelX, 0, document.renderSizeX - 1);
    minPixelY = Clamp(minPixelY, 0, document.renderSizeY - 1);
    maxPixelY = Clamp(maxPixelY, 0, document.renderSizeY - 1);

    // Convert cubicBezier.width to pixels width
    float curveWidth = CanvasLengthToPixelLength(document, cubicBezier.width);

    // Draw it
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

                float pixelX = ix + offset.X - offsetPx.X;
                float pixelY = iy + offset.Y - offsetPx.Y;

                // binary search to find where to start the scan from
                auto it = std::upper_bound(cubicBezierData.points, &cubicBezierData.points[cubicBezierData.pointCount], pixelX,
                    [] (const float pixelX, const CubicBezierData::CurvePoint& curvePoint)
                    {
                        return pixelX < curvePoint.x;
                    }
                );
                uint32_t startIndex = (uint32_t)(it - cubicBezierData.points);

                // since the points of the curve are dense, we can find the distance to the closest point instead of line segments
                float closestDistanceSquared = FLT_MAX;
                for (uint32_t pointIndex = startIndex; pointIndex < cubicBezierData.pointCount; ++pointIndex)
                {
                   const CubicBezierData::CurvePoint& p = cubicBezierData.points[pointIndex];

                    if (p.x < floor(pixelX - curveWidth))
                        continue;

                    if (p.x > ceil(pixelX + curveWidth))
                        break;

                    float distanceSquared = LengthSquared(vec2{ p.x, p.y } - vec2{ pixelX, pixelY });
                    closestDistanceSquared = Min(closestDistanceSquared, distanceSquared);
                }

                if ((float)sqrt(closestDistanceSquared) < curveWidth)
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

    return true;
}
