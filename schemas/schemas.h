#include <string>
#include <vector>

SCHEMA_BEGIN(Data, Entity)
    SCHEMA_FIELD(std::string, id, "", "The unique ID of the entity")
SCHEMA_END()


// A schema to unify the list of plants and animals into a single thing
SCHEMA_BEGIN(Data, Root)
    SCHEMA_ARRAY(Entity, entities, "")
SCHEMA_END()
