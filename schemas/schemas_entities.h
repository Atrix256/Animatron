// All entity types. In their own file so we can expand entity types specifically for generating code relating to entities.

VARIANT_BEGIN(Data, EntityVariant, "Storage for entity type specific information")
    VARIANT_TYPE(EntityFill, fill, EntityFill(), "")
    VARIANT_TYPE(EntityCircle, circle, EntityCircle(), "")
VARIANT_END()
