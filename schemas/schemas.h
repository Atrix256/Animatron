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

// ----------------------------- Specific Entity Types -----------------------------

STRUCT_BEGIN(Data, EntityFill, "Fills the screen")
    STRUCT_FIELD(Color, color, Color{}, "The color of the fill")
STRUCT_END()

STRUCT_BEGIN(Data, EntityCircle, "Draw a circle")
    STRUCT_FIELD(Point2D, center, Point2D(), "The location of the circle")
    STRUCT_FIELD(float, innerRadius, 0.9f, "The distance from the center that is not filled in.")
    STRUCT_FIELD(float, outerRadius, 1.0f, "The distance beyond that distance that is filled in.")
    STRUCT_FIELD(Color, color, Color{}, "The color of the circle")
STRUCT_END()

STRUCT_BEGIN(Data, EntityRectangle, "Draw a rectangle")
    STRUCT_FIELD(Point2D, center, Point2D(), "The location of the rectangle")
    STRUCT_FIELD(float, width, 10.0f, "Rectangle width")
    STRUCT_FIELD(float, height, 10.0f, "Rectangle height")
    STRUCT_FIELD(Color, color, Color{}, "The color of the rectangle")
STRUCT_END()

STRUCT_BEGIN(Data, EntityLine, "Draw a line")
    STRUCT_FIELD(Point2D, A, Point2D(), "One point of the line")
    STRUCT_FIELD(Point2D, B, Point2D(), "The other point of the line")
    STRUCT_FIELD(float, width, 1.0f, "Line width")
    STRUCT_FIELD(Color, color, Color{}, "The color of the line")
STRUCT_END()

STRUCT_BEGIN(Data, EntityLine3D, "Draw a 3d line")
    STRUCT_FIELD(Point3D, A, Point3D(), "One point of the line")
    STRUCT_FIELD(Point3D, B, Point3D(), "The other point of the line")
    STRUCT_FIELD(float, width, 1.0f, "Line width")
    STRUCT_FIELD(Color, color, Color{}, "The color of the line")
    STRUCT_FIELD(std::string, camera, "", "The name of the camera used by the line")
    STRUCT_FIELD(std::string, transform, "", "The name of the transform used by the line")
STRUCT_END()

STRUCT_BEGIN(Data, EntityLines3D, "Draw 3d lines")
    STRUCT_DYNAMIC_ARRAY(Point3D, points, "The list of points in the lines")
    STRUCT_FIELD(float, width, 1.0f, "Width of lines")
    STRUCT_FIELD(Color, color, Color{}, "The color of the lines")
    STRUCT_FIELD(std::string, camera, "", "The name of the camera used by the lines")
    STRUCT_FIELD(std::string, transform, "", "The name of the transform used by the lines")
STRUCT_END()

STRUCT_BEGIN(Data, EntityCamera, "A camera, used to turn 3d objects into 2d")
    STRUCT_FIELD(Point3D, position, Point3D{ 0.0f COMMA 0.0f COMMA -1.0f }, "The position of the camera")
    STRUCT_FIELD(Point3D, at, Point3D{ 0.0f COMMA 0.0f COMMA 0.0f }, "The point the camera is aimed at")
    STRUCT_FIELD(Point3D, up, Point3D{ 0.0f COMMA 1.0f COMMA 0.0f }, "The up direction for the camera's orientation")
    STRUCT_FIELD(bool, perspective, true, "If true, uses a perspective projection, else an orthographic projection. The perspective projection uses reversed z, infinite far plane projection.")
    STRUCT_FIELD(float, near, 0.1f, "The near plane distance. Used by perspective projection.")
    STRUCT_FIELD_NO_SERIALIZE(Matrix4x4, viewProj, Matrix4x4(), "The view projection matrix of the camera. Calculated each frame in the initialization function.")
STRUCT_END()

STRUCT_BEGIN(Data, EntityTransform, "")
    STRUCT_FIELD(Point3D, translation, Point3D(), "Translation")
    STRUCT_FIELD(Point3D, scale, Point3D{ 1.0f COMMA 1.0f COMMA 1.0f }, "Scale")
    STRUCT_FIELD(Point3D, rotation, Point3D(), "Rotation on each axis, in degrees")
    STRUCT_FIELD_NO_SERIALIZE(Matrix4x4, mtx, Matrix4x4(),"The transform matrix. Calculated each frame in the initialization function.")
STRUCT_END()

// ----------------------------- Entity Types -----------------------------

#include "schemas_entities.h"

STRUCT_BEGIN(Data, Entity, "All information about an entity")
    STRUCT_FIELD(std::string, id, "", "The unique ID of the entity")
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
    STRUCT_STATIC_ARRAY(float, blendControlPoints, 4, {0.0f COMMA 1.0f / 3.0f COMMA 2.0f / 3.0f COMMA 1.0f}, "Middle two Cubic Bezier control points for blending from the previous value. The endpoints not given are 0.0 and 1.0")
STRUCT_END()

// ----------------------------- Application Settings File -----------------------------

// TODO: put major/minor version for config and app in a shared header somewhere.
// TODO: make default version major/minor not be the current version, so it errors if they are omited
// TODO: make the program read and verify config.  Probably put a no serialize configuration on the document and then manually serialize it in from the other place. yeah!

STRUCT_BEGIN(Data, Configuration, "Application configuration, read from config.json")
    STRUCT_FIELD(std::string, program, "animatronconfig", "Identifier to make sure this is a file for Animatron to use")
    STRUCT_FIELD(uint32_t, versionMajor, 0, "Major version number")
    STRUCT_FIELD(uint32_t, versionMinor, 1, "Minor version number")

    STRUCT_FIELD(std::string, pdflatexexe, "", "The absolute path to where pdflatex.exe is (from latex), used to render text and formulas")
    STRUCT_FIELD(std::string, magickexe, "", "The absolute path to where magickexe.exe is (from image magick), used to convert pdf to png")
STRUCT_END()

// ----------------------------- The Document -----------------------------

STRUCT_BEGIN(Data, Document, "A document")
    STRUCT_FIELD(std::string, program, "animatron", "Identifier to make sure this is a file for Animatron to use")
    STRUCT_FIELD(uint32_t, versionMajor, 0, "Major version number")
    STRUCT_FIELD(uint32_t, versionMinor, 1, "Minor version number")

    STRUCT_FIELD_NO_SERIALIZE(Configuration, config, Configuration(), "Application configuration, read from config.json")

    STRUCT_FIELD(int, outputSizeX, 320, "The size of the output render on the X axis")
    STRUCT_FIELD(int, outputSizeY, 200, "The size of the output render on the Y axis")
    STRUCT_FIELD(int, renderSizeX, 0, "The size of the render on the X axis. 0 means use output size. The rendered image will be sized down (for AA) or up to match the output size.")
    STRUCT_FIELD(int, renderSizeY, 0, "The size of the render on the Y axis. 0 means use output size. The rendered image will be sized down (for AA) or up to match the output size.")
    STRUCT_FIELD(float, duration, 4.0f, "The duration of the rendering")
    STRUCT_FIELD(int, FPS, 30, "The frame rate of the render")
    STRUCT_FIELD(bool, forceOpaqueOutput, true, "If true, it will force all output pixels to be opaque. False to let transparency be output.")

    STRUCT_FIELD(uint32_t, samplesPerPixel, 16, "The number of samples taken per pixel, increase for better anti aliasing but increased rendering cost.")
    STRUCT_FIELD(SamplesType2D, jitterSequenceType, SamplesType2D::MitchellsBlueNoise, "The jitter sequence to use for the samples in samplesPerPixel.")
    STRUCT_FIELD_NO_SERIALIZE(Point2DArray, jitterSequence, Point2DArray(), "The actual jitter sequence used per pixel")

    STRUCT_DYNAMIC_ARRAY(Entity, entities, "")
    STRUCT_DYNAMIC_ARRAY(KeyFrame, keyFrames, "")
STRUCT_END()
