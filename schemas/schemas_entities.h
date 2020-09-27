// All entity types. In their own file so we can expand entity types specifically for generating code relating to entities.

VARIANT_BEGIN(Data, EntityVariant, "Storage for entity type specific information")
    VARIANT_TYPE(EntityFill, fill, EntityFill(), "")
    VARIANT_TYPE(EntityCircle, circle, EntityCircle(), "")
    VARIANT_TYPE(EntityRectangle, rectangle, EntityRectangle(), "")
    VARIANT_TYPE(EntityLine, line, EntityLine(), "")
    VARIANT_TYPE(EntityLine3D, line3d, EntityLine3D(), "")
    VARIANT_TYPE(EntityLines3D, lines3d, EntityLines3D(), "")
    VARIANT_TYPE(EntityCamera, camera, EntityCamera(), "")
VARIANT_END()
