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

// ----------------------------- Entity Types -----------------------------

STRUCT_BEGIN(Data, EntityClear, "Clear the screen")
    STRUCT_FIELD(Color, color, Color{}, "The color of the clear")
STRUCT_END()

VARIANT_BEGIN(Data, EntityVariant, "Storage for entity type specific information")
    VARIANT_TYPE(EntityClear, clear, EntityClear(), "")
VARIANT_END()

STRUCT_BEGIN(Data, Entity, "All information about an entity")
    STRUCT_FIELD(std::string, id, "", "The unique ID of the entity")
    STRUCT_FIELD(float, createTime, 0.0f, "The time in seconds that the object is created.")
    STRUCT_FIELD(float, destroyTime, -1.0f, "The time in seconds that the object is destroyed. -1 means it is never destroyed")
    STRUCT_FIELD(EntityVariant, data, EntityVariant(), "Entity type specific information")
STRUCT_END()

// ----------------------------- Events -----------------------------

STRUCT_BEGIN(Data, Event, "An Event")
    STRUCT_FIELD(std::string, entityId, "", "The ID of the entity affected by this event")
    STRUCT_FIELD(float, time, 0.0f, "The time in seconds the event occurs")
    STRUCT_FIELD(std::string, field, "", "The name of the field to change")
    STRUCT_FIELD(std::string, newValue, "", "The new value of the field, as a JSON string")
STRUCT_END()

// ----------------------------- The Document -----------------------------

STRUCT_BEGIN(Data, Document, "A document")
    STRUCT_FIELD(int, sizeX, 800, "The size of the render on the X axis")
    STRUCT_FIELD(int, sizeY, 600, "The size of the render on the Y axis")
    STRUCT_FIELD(float, duration, 4.0f, "The duration of the rendering")
    STRUCT_FIELD(int, FPS, 30, "The frame rate of the render")
    STRUCT_DYNAMIC_ARRAY(Entity, entities, "")
    STRUCT_DYNAMIC_ARRAY(Event, events, "")
STRUCT_END()
