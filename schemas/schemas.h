#include <string>
#include <vector>

// ----------------------------- Utility Types -----------------------------

STRUCT_BEGIN(Data, Color, "A linear floating point color")
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
    STRUCT_STATIC_ARRAY(float, blendControlPoints, 2, {1.0f / 3.0f COMMA 2.0f / 3.0f}, "Middle two Cubic Bezier control points for blending from the previous value. The endpoints not given are 0.0 and 1.0")
STRUCT_END()

// ----------------------------- The Document -----------------------------

STRUCT_BEGIN(Data, Document, "A document")
    STRUCT_FIELD(int, outputSizeX, 320, "The size of the output render on the X axis")
    STRUCT_FIELD(int, outputSizeY, 200, "The size of the output render on the Y axis")
    STRUCT_FIELD(int, renderSizeX, 0, "The size of the render on the X axis. 0 means use output size. The rendered image will be sized down (for AA) or up to match the output size.")
    STRUCT_FIELD(int, renderSizeY, 0, "The size of the render on the Y axis. 0 means use output size. The rendered image will be sized down (for AA) or up to match the output size.")
    STRUCT_FIELD(float, duration, 4.0f, "The duration of the rendering")
    STRUCT_FIELD(int, FPS, 30, "The frame rate of the render")
    STRUCT_DYNAMIC_ARRAY(Entity, entities, "")
    STRUCT_DYNAMIC_ARRAY(KeyFrame, keyFrames, "")
STRUCT_END()
