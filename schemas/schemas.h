#include <string>
#include <vector>

// ----------------------------- Utility Types -----------------------------

SCHEMA_BEGIN(Data, Color)
    SCHEMA_FIELD(float, R, 0.0f, "Red")
    SCHEMA_FIELD(float, G, 0.0f, "Green")
    SCHEMA_FIELD(float, B, 0.0f, "Blue")
    SCHEMA_FIELD(float, A, 0.0f, "Alpha")
SCHEMA_END()

SCHEMA_BEGIN(Data, ColorU8)
    SCHEMA_FIELD(uint8_t, R, 0, "Red")
    SCHEMA_FIELD(uint8_t, G, 0, "Green")
    SCHEMA_FIELD(uint8_t, B, 0, "Blue")
    SCHEMA_FIELD(uint8_t, A, 0, "Alpha")
SCHEMA_END()

// ----------------------------- Entity Types -----------------------------

SCHEMA_BEGIN(Data, Entity)
    SCHEMA_FIELD(std::string, id, "", "The unique ID of the entity")
    SCHEMA_FIELD(float, createTime, 0.0f, "The time in seconds that the object is created.")
    SCHEMA_FIELD(float, destroyTime, -1.0f, "The time in seconds that the object is destroyed. -1 means it is never destroyed")
SCHEMA_END()

SCHEMA_INHERIT_BEGIN(Data, Clear, Data::Entity)
    SCHEMA_FIELD(Color, color, Color{}, "The color of the clear")
SCHEMA_END()

// ----------------------------- Events -----------------------------

SCHEMA_BEGIN(Data, Event)
    SCHEMA_FIELD(std::string, entityId, "", "The ID of the entity affected by this event")
    SCHEMA_FIELD(float, time, 0.0f, "The time in seconds the event occurs")
    SCHEMA_FIELD(std::string, field, "", "The name of the field to change")
    SCHEMA_FIELD(std::string, newValue, "", "The new value of the field, as a JSON string")
SCHEMA_END()

// ----------------------------- The Document -----------------------------

SCHEMA_BEGIN(Data, Document)
    SCHEMA_FIELD(int, sizeX, 800, "The size of the render on the X axis")
    SCHEMA_FIELD(int, sizeY, 600, "The size of the render on the Y axis")
    SCHEMA_FIELD(float, duration, 4.0f, "The duration of the rendering")
    SCHEMA_FIELD(int, FPS, 30, "The frame rate of the render")
    SCHEMA_ARRAY(Clear, clears, "")
    SCHEMA_ARRAY(Event, events, "")
SCHEMA_END()
