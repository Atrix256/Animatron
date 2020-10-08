#include <string>
#include <vector>

// ----------------------------- Utility Types -----------------------------

STRUCT_BEGIN(Data, Color, "A linear floating point color")
    STRUCT_FIELD(float, R, 0.0f, "Red")
    STRUCT_FIELD(float, G, 0.0f, "Green")
    STRUCT_FIELD(float, B, 0.0f, "Blue")
    STRUCT_FIELD(float, A, 0.0f, "Alpha")
STRUCT_END()

STRUCT_BEGIN(Data, ColorPMA, "A pre multiplied alpha version of Color")
    STRUCT_FIELD(float, R, 0.0f, "Red")
    STRUCT_FIELD(float, G, 0.0f, "Green")
    STRUCT_FIELD(float, B, 0.0f, "Blue")
    STRUCT_FIELD(float, A, 0.0f, "Alpha")
STRUCT_END()

STRUCT_BEGIN(Data, ColorU8, "An sRGB U8 color")
    STRUCT_FIELD(uint8_t, R, 0, "Red")
    STRUCT_FIELD(uint8_t, G, 0, "Green")
    STRUCT_FIELD(uint8_t, B, 0, "Blue")
    STRUCT_FIELD(uint8_t, A, 0, "Alpha")
STRUCT_END()

STRUCT_BEGIN(Data, Point2D, "A 2d Point")
    STRUCT_FIELD(float, X, 0.0f, "X")
    STRUCT_FIELD(float, Y, 0.0f, "Y")
STRUCT_END()

STRUCT_BEGIN(Data, Point3D, "A 3d Point")
    STRUCT_FIELD(float, X, 0.0f, "X")
    STRUCT_FIELD(float, Y, 0.0f, "Y")
    STRUCT_FIELD(float, Z, 0.0f, "Z")
STRUCT_END()

STRUCT_BEGIN(Data, Point4D, "A 4d Point")
    STRUCT_FIELD(float, X, 0.0f, "X")
    STRUCT_FIELD(float, Y, 0.0f, "Y")
    STRUCT_FIELD(float, Z, 0.0f, "Z")
    STRUCT_FIELD(float, W, 0.0f, "W")
STRUCT_END()

STRUCT_BEGIN(Data, GradientPoint, "A color gradient point")
    STRUCT_FIELD(float, value, 0.0f, "The gradient scalar value")
    STRUCT_FIELD(Color, color, Color(), "The color at this value")
    STRUCT_STATIC_ARRAY(float, blendControlPoints, 4, { 0.0f COMMA 1.0f / 3.0f COMMA 2.0f / 3.0f COMMA 1.0f }, "Cubic Bezier control points for blending from the previous value")
STRUCT_END()

STRUCT_BEGIN(Data, Matrix2x2, "A 2x2 matrix")
    STRUCT_FIELD(Point2D, X, Point2D{1.0f COMMA 0.0f}, "X axis")
    STRUCT_FIELD(Point2D, Y, Point2D{0.0f COMMA 1.0f}, "Y axis")
STRUCT_END()

STRUCT_BEGIN(Data, Matrix3x3, "A 3x3 matrix")
    STRUCT_FIELD(Point3D, X, Point3D{1.0f COMMA 0.0f COMMA  0.0f}, "X axis")
    STRUCT_FIELD(Point3D, Y, Point3D{0.0f COMMA 1.0f COMMA  0.0f}, "Y axis")
    STRUCT_FIELD(Point3D, Z, Point3D{0.0f COMMA 0.0f COMMA  1.0f}, "Z axis")
STRUCT_END()

STRUCT_BEGIN(Data, Matrix4x4, "A 4x4 matrix")
    STRUCT_FIELD(Point4D, X, Point4D{1.0f COMMA 0.0f COMMA  0.0f COMMA 0.0f}, "X axis")
    STRUCT_FIELD(Point4D, Y, Point4D{0.0f COMMA 1.0f COMMA  0.0f COMMA 0.0f}, "Y axis")
    STRUCT_FIELD(Point4D, Z, Point4D{0.0f COMMA 0.0f COMMA  1.0f COMMA 0.0f}, "Z axis")
    STRUCT_FIELD(Point4D, W, Point4D{0.0f COMMA 0.0f COMMA  0.0f COMMA 1.0f}, "W axis")
STRUCT_END()

STRUCT_BEGIN(Data, Point2DArray, "A dynamic array of Point2Ds")
    STRUCT_DYNAMIC_ARRAY(Point2D, points, "")
STRUCT_END()

// ----------------------------- Enums -----------------------------

ENUM_BEGIN(Data, SamplesType2D, "Type of 2d samples generated")
    ENUM_ITEM(MitchellsBlueNoise, "2d blue noise, made with Mitchell's Best Candidate algorithm. Good at hiding the error in low sample counts.")
ENUM_END()

ENUM_BEGIN(Data, DigitalDissolveType, "Types of digital dissolve")
    ENUM_ITEM(BlueNoise, "2d blue noise texture, made with void and cluster.")
ENUM_END()

// ----------------------------- Specific Entity Types -----------------------------

STRUCT_BEGIN(Data, EntityFill, "Fills the screen")
    STRUCT_FIELD(Color, color, Color{ 0.0f COMMA 0.0f COMMA 0.0f COMMA 1.0f }, "The color of the fill")
STRUCT_END()

STRUCT_BEGIN(Data, EntityCircle, "Draw a circle")
    STRUCT_FIELD(Point2D, center, Point2D(), "The location of the circle")
    STRUCT_FIELD(float, innerRadius, 9.0f, "The distance from the center that is not filled in.")
    STRUCT_FIELD(float, outerRadius, 10.0f, "The distance beyond that distance that is filled in.")
    STRUCT_FIELD(Color, color, Color{0.0f COMMA 0.0f COMMA 0.0f COMMA 1.0f}, "The color of the circle")
STRUCT_END()

STRUCT_BEGIN(Data, EntityRectangle, "Draw a rectangle")
    STRUCT_FIELD(Point2D, center, Point2D(), "The location of the rectangle")
    STRUCT_FIELD(Point2D, radius, Point2D{ 50 COMMA 50 }, "half width and height, in canvas coordinates")
    STRUCT_FIELD(float, expansion, 0.0f, "rounds the rectangle - minkowski sum of the rectangle and a circle of this radius")
    STRUCT_FIELD(Color, color, Color{ 0.0f COMMA 0.0f COMMA 0.0f COMMA 1.0f }, "The color of the rectangle")
STRUCT_END()

STRUCT_BEGIN(Data, EntityLine, "Draw a line")
    STRUCT_FIELD(Point2D, A, Point2D(), "One point of the line")
    STRUCT_FIELD(Point2D, B, Point2D(), "The other point of the line")
    STRUCT_FIELD(float, width, 1.0f, "Line width")
    STRUCT_FIELD(Color, color, Color{ 0.0f COMMA 0.0f COMMA 0.0f COMMA 1.0f }, "The color of the line")
STRUCT_END()

STRUCT_BEGIN(Data, EntityCubicBezier, "A cubic bezier curve, with weights per control point (rational curve).")
    STRUCT_FIELD(Point3D, A, Point3D{ 0.0f COMMA 0.0f COMMA 1.0f }, "XY are control point, Z is weight")
    STRUCT_FIELD(Point3D, B, Point3D{ 0.0f COMMA 0.0f COMMA 1.0f }, "XY are control point, Z is weight")
    STRUCT_FIELD(Point3D, C, Point3D{ 0.0f COMMA 0.0f COMMA 1.0f }, "XY are control point, Z is weight")
    STRUCT_FIELD(Point3D, D, Point3D{ 0.0f COMMA 0.0f COMMA 1.0f }, "XY are control point, Z is weight")
    STRUCT_FIELD(float, width, 1.0f, "Line width")
    STRUCT_FIELD(Color, color, Color{ 0.0f COMMA 0.0f COMMA 0.0f COMMA 1.0f }, "The color of the curve")
STRUCT_END()

STRUCT_BEGIN(Data, EntityLine3D, "Draw a 3d line")
    STRUCT_FIELD(Point3D, A, Point3D(), "One point of the line")
    STRUCT_FIELD(Point3D, B, Point3D(), "The other point of the line")
    STRUCT_FIELD(float, width, 1.0f, "Line width")
    STRUCT_FIELD(Color, color, Color{ 0.0f COMMA 0.0f COMMA 0.0f COMMA 1.0f }, "The color of the line")
    STRUCT_FIELD(std::string, camera, "", "The name of the camera used by the line")
    STRUCT_FIELD(std::string, transform, "", "The name of the transform used by the line")
STRUCT_END()

STRUCT_BEGIN(Data, EntityLines3D, "Draw 3d lines")
    STRUCT_DYNAMIC_ARRAY(Point3D, points, "The list of points in the lines")
    STRUCT_FIELD(float, width, 1.0f, "Width of lines")
    STRUCT_FIELD(Color, color, Color{ 0.0f COMMA 0.0f COMMA 0.0f COMMA 1.0f }, "The color of the lines")
    STRUCT_FIELD(std::string, camera, "", "The name of the camera used by the lines")
    STRUCT_FIELD(std::string, transform, "", "The name of the transform used by the lines")
STRUCT_END()

STRUCT_BEGIN(Data, EntityCamera, "A camera, used to turn 3d objects into 2d")
    STRUCT_FIELD(Point3D, position, Point3D{ 0.0f COMMA 0.0f COMMA -100.0f }, "The position of the camera")
    STRUCT_FIELD(Point3D, at, Point3D{ 0.0f COMMA 0.0f COMMA 0.0f }, "The point the camera is aimed at")
    STRUCT_FIELD(Point3D, up, Point3D{ 0.0f COMMA 1.0f COMMA 0.0f }, "The up direction for the camera's orientation")
    STRUCT_FIELD(bool, perspective, true, "If true, uses a perspective projection, else an orthographic projection. The perspective projection uses reversed z, infinite far plane projection.")
    STRUCT_FIELD(float, near, 0.1f, "The near plane distance. Used by perspective projection.")
    STRUCT_FIELD(float, far, 1000.0f, "The far plane distance. Used by perspective projection.")
    STRUCT_FIELD(float, FOV, 45.0f, "The vertical field of view in degrees. Used by perspective projection.")
    STRUCT_FIELD_NO_SERIALIZE(Matrix4x4, viewProj, Matrix4x4(), "The view projection matrix of the camera. Calculated each frame in the initialization function.")
STRUCT_END()

STRUCT_BEGIN(Data, EntityTransform, "")
    STRUCT_FIELD(Point3D, translation, Point3D(), "Translation")
    STRUCT_FIELD(Point3D, scale, Point3D{ 1.0f COMMA 1.0f COMMA 1.0f }, "Scale")
    STRUCT_FIELD(Point3D, rotation, Point3D(), "Rotation on each axis, in degrees")
    STRUCT_FIELD_NO_SERIALIZE(Matrix4x4, mtx, Matrix4x4(),"The transform matrix. Calculated each frame in the initialization function.")
STRUCT_END()

STRUCT_BEGIN(Data, EntityLatex, "")
    STRUCT_FIELD(Point2D, position, Point2D(), "The center of the generated image")
    STRUCT_FIELD(float, Scale, 1.0f, "A size scalar for the text")
    STRUCT_FIELD(Data::Color, foreground, Data::Color{ 0.0f COMMA 0.0f COMMA 0.0f COMMA 1.0f }, "The foreground color of the latex (where it's black in a rendered latex image)")
    STRUCT_FIELD(Data::Color, background, Data::Color{ 0.0f COMMA 0.0f COMMA 0.0f COMMA 0.0f }, "The foreground color of the latex (where it's white in a rendered latex image)")
    STRUCT_FIELD(std::string, latex, "", "The actual latex to render.")
    STRUCT_FIELD_NO_SERIALIZE(uint32_t, _width, 0, "the width of the generated image")
    STRUCT_FIELD_NO_SERIALIZE(uint32_t, _height, 0, "the height of the generated image")
    STRUCT_FIELD_NO_SERIALIZE(std::vector<uint8_t>, _pixels, std::vector<uint8_t>(), "The U8 pixels of the generated image")
STRUCT_END()

STRUCT_BEGIN(Data, EntityLinearGradient, "A linear gradient: define a 2d half space and a mapping from the scalar values to colors")
    STRUCT_FIELD(Point3D, halfSpace, Point3D{}, "A 2d version of a plane: x and y are a vector to project a point on, z is a value to add to that projection.")
    STRUCT_DYNAMIC_ARRAY(GradientPoint, points, "The gradient colors")
STRUCT_END()

STRUCT_BEGIN(Data, EntityDigitalDissolve, "A digital dissolve. An alpha value of 1 will show all foreground, a value of 0 will show all background, and inbetween values will show some of each.")
    STRUCT_FIELD(DigitalDissolveType, type, DigitalDissolveType::BlueNoise, "The type of pattern to use for digital dissolve")
    STRUCT_FIELD(Point2D, scale, Point2D{ 1.0f COMMA 1.0f }, "Pixel scale. Values larger than 1 make them chunkier")
    STRUCT_FIELD(Data::Color, foreground, Data::Color{ 0.0f COMMA 0.0f COMMA 0.0f COMMA 1.0f }, "The foreground color")
    STRUCT_FIELD(Data::Color, background, Data::Color{ 0.0f COMMA 0.0f COMMA 0.0f COMMA 0.0f }, "The background color")
    STRUCT_FIELD(float, alpha, 0.0f, "How much foreground to show in percentage")
STRUCT_END()

STRUCT_BEGIN(Data, EntityImage, "An image file")
    STRUCT_FIELD(std::string, fileName, "", "The image file")
    STRUCT_FIELD(Point2D, position, Point2D(), "The center of the image")
    STRUCT_FIELD(Point2D, radius, Point2D{ 50 COMMA 50 }, "half width and height, in canvas coordinates")
    STRUCT_FIELD(Color, tint, Color{ 1.0f COMMA 1.0f COMMA 1.0f COMMA 1.0f }, "A color to multiply the image by")
    STRUCT_FIELD_NO_SERIALIZE(int, _rawwidth, 0, "the width of the loaded image")
    STRUCT_FIELD_NO_SERIALIZE(int, _rawheight, 0, "the height of the loaded image")
    STRUCT_FIELD_NO_SERIALIZE(std::vector<ColorPMA>, _rawpixels, std::vector<ColorPMA>(), "The PMA pixels of the loaded image")
    STRUCT_FIELD_NO_SERIALIZE(int, _width, 0, "the width of the resized image")
    STRUCT_FIELD_NO_SERIALIZE(int, _height, 0, "the height of the resized image")
    STRUCT_FIELD_NO_SERIALIZE(std::vector<ColorPMA>, _pixels, std::vector<ColorPMA>(), "The PMA pixels of the resized image")
STRUCT_END()

// ----------------------------- Entity -----------------------------

#include "schemas_entities.h"

STRUCT_BEGIN(Data, Entity, "All information about an entity")
    STRUCT_FIELD(std::string, id, "", "The unique ID of the entity")
    STRUCT_FIELD(std::string, parent, "", "The parent of an entity. Positions are relative to the parent positions. If no parent given, the canvas is used.")
    STRUCT_FIELD(float, zorder, 0.0f, "Determines the order of rendering. higher numbers are on top.")
    STRUCT_FIELD(float, createTime, 0.0f, "The time in seconds that the object is created.")
    STRUCT_FIELD(float, destroyTime, -1.0f, "The time in seconds that the object is destroyed. -1 means it is never destroyed")
    STRUCT_FIELD(EntityVariant, data, EntityVariant(), "Entity type specific information")
STRUCT_END()

// ----------------------------- Key Frames -----------------------------

STRUCT_BEGIN(Data, KeyFrame, "A KeyFrame")
    STRUCT_FIELD(std::string, entityId, "", "The ID of the entity affected")
    STRUCT_FIELD(float, time, 0.0f, "The time in seconds the event occurs")
    STRUCT_FIELD(std::string, newValue, "", "JSON describing the value of the entity at this point in time")
    STRUCT_STATIC_ARRAY(float, blendControlPoints, 4, {0.0f COMMA 1.0f / 3.0f COMMA 2.0f / 3.0f COMMA 1.0f}, "Cubic Bezier control points for blending from the previous value")
STRUCT_END()

// ----------------------------- Application Settings File -----------------------------

STRUCT_BEGIN(Data, Configuration, "Application configuration, read from config.json")
    STRUCT_FIELD(std::string, program, "animatronconfig", "Identifier to make sure this is a file for Animatron to use")
    STRUCT_FIELD(uint32_t, versionMajor, -1, "Major version number")
    STRUCT_FIELD(uint32_t, versionMinor, -1, "Minor version number")

    STRUCT_FIELD(std::string, latexbinaries, "", "The path to where pdflatex.exe and dvipng.exe are. Used to render text and formulas. MikTex suggested!")
    STRUCT_FIELD(std::string, ffmpeg, "", "The path to where ffmpeg.exe is, including the exe name. Used to assemble frames into the final video. ")
STRUCT_END()

// ----------------------------- The Document -----------------------------

STRUCT_BEGIN(Data, Document, "A document")
    STRUCT_FIELD(std::string, program, "animatron", "Identifier to make sure this is a file for Animatron to use")
    STRUCT_FIELD(uint32_t, versionMajor, -1, "Major version number")
    STRUCT_FIELD(uint32_t, versionMinor, -1, "Minor version number")

    STRUCT_FIELD_NO_SERIALIZE(Configuration, config, Configuration(), "Application configuration, read from config.json")

    STRUCT_FIELD(int, outputSizeX, 320, "The size of the output render on the X axis")
    STRUCT_FIELD(int, outputSizeY, 200, "The size of the output render on the Y axis")
    STRUCT_FIELD(int, renderSizeX, 0, "The size of the render on the X axis. 0 means use output size. The rendered image will be sized down (for AA) or up to match the output size.")
    STRUCT_FIELD(int, renderSizeY, 0, "The size of the render on the Y axis. 0 means use output size. The rendered image will be sized down (for AA) or up to match the output size.")
    STRUCT_FIELD(float, duration, 4.0f, "The duration of the rendering")
    STRUCT_FIELD(int, FPS, 30, "The frame rate of the render")

    STRUCT_FIELD(std::string, audioFile, "", "The name of the audio file to attach to the video. Later on there will be better audio support.")

    STRUCT_FIELD(bool, blueNoiseDither, true, "If true, will use blue noise to dither the floating point color before quantizing to 8 bit color.")
    STRUCT_FIELD(bool, forceOpaqueOutput, true, "If true, it will force all output pixels to be opaque. False to let transparency be output.")

    STRUCT_FIELD(uint32_t, samplesPerPixel, 16, "The number of samples taken per pixel, increase for better anti aliasing but increased rendering cost.")
    STRUCT_FIELD(SamplesType2D, jitterSequenceType, SamplesType2D::MitchellsBlueNoise, "The jitter sequence to use for the samples in samplesPerPixel.")
    STRUCT_FIELD_NO_SERIALIZE(Point2DArray, jitterSequence, Point2DArray(), "The actual jitter sequence used per pixel")
     
    STRUCT_FIELD_NO_SERIALIZE(int, blueNoiseWidth, 0, "Width of loaded blue noise tetxure")
    STRUCT_FIELD_NO_SERIALIZE(int, blueNoiseHeight, 0, "Height of loaded blue noise tetxure")
    STRUCT_FIELD_NO_SERIALIZE(std::vector<Data::ColorU8>, blueNoisePixels, std::vector<Data::ColorU8>(), "pixels of loaded blue noise tetxure")

    STRUCT_DYNAMIC_ARRAY(Entity, entities, "")
    STRUCT_DYNAMIC_ARRAY(KeyFrame, keyFrames, "")
STRUCT_END()

// TODO: Bezier curve should be able to parent each point off of a different parent! Do this in next checkin, you are using it for clip 3!
