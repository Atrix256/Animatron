#pragma once

#include "utils.h"
#include "animatron.h"

inline Data::Point3D GetParentPosition(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::Entity>& entityMap,
    const Data::Entity& entity);

struct EntityActionFrameContext
{
    int frameIndex = 0;
    float frameTime = 0.0f;
};

// Default base class functionality
// Functions which are fully implemented here are optional to be implemented.
// Functions which are only delcared here are required to be implemented.
struct EntityActionBase
{
    // Optional implmentation

    static bool Initialize(const Data::Document& document, Data::Entity& entity, int entityIndex) { return true; }

    static bool FrameInitialize(
        const Data::Document& document,
        Data::Entity& entity,
        const EntityActionFrameContext& context)
    {
        return true;
    }

    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context)
    {
        return true;
    }

    // If the entity has extra internal state, like how a flipbook uses the time to
    // know what image to show, you can implement this function to include that info.
    // Without this, the flipbook would return the same hash for different images shown
    // and it would only ever show one image.
    static void ExtraFrameHash(const Data::Document& document, Data::Entity& entity, const EntityActionFrameContext& context, size_t& hash)
    {
    }


    // Required implementation

    static Data::Point3D GetPosition(const Data::Entity& entity);


    // Internal functionality
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct EntityFill_Action : EntityActionBase
{
    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context)
    {
        Fill(pixels, entity.data.fill.color);
        return true;
    }

    static Data::Point3D GetPosition(const Data::Entity& entity) { return Data::Point3D{ 0.0f, 0.0f, 0.0f }; }
};

struct EntityCircle_Action : EntityActionBase
{
    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context);

    static Data::Point3D GetPosition(const Data::Entity& entity) { return ToPoint3D(entity.data.circle.center); }
};

struct EntityRectangle_Action : EntityActionBase
{
    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context);

    static Data::Point3D GetPosition(const Data::Entity& entity) { return ToPoint3D(entity.data.rectangle.center); }
};

struct EntityLine_Action : EntityActionBase
{
    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context)
    {
        Data::Point2D offset = Point3D_XY(GetParentPosition(document, entityMap, entity));

        DrawLine(document, pixels, entity.data.line.A + offset, entity.data.line.B + offset, entity.data.line.width, ToPremultipliedAlpha(entity.data.line.color));
        return true;
    }

    static Data::Point3D GetPosition(const Data::Entity& entity) { return ToPoint3D(entity.data.line.A + entity.data.line.B) * 0.5f; }
};

struct EntityLine3D_Action : EntityActionBase
{
    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context);

    static Data::Point3D GetPosition(const Data::Entity& entity) { return (entity.data.line3d.A + entity.data.line3d.B) * 0.5f; }
};

struct EntityLines3D_Action : EntityActionBase
{
    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context);

    static Data::Point3D GetPosition(const Data::Entity& entity)
    {
        Data::Point3D ret;
        if (entity.data.lines3d.points.size() > 0)
        {
            for (auto p : entity.data.lines3d.points)
                ret = ret + p;
            return ret / float(entity.data.lines3d.points.size());
        }
        else
        {
            return ret;
        }
    }
};

struct EntityCamera_Action : EntityActionBase
{
    static bool FrameInitialize(const Data::Document& document, Data::Entity& entity, const EntityActionFrameContext& context);

    static Data::Point3D GetPosition(const Data::Entity& entity)
    {
        return entity.data.camera.position;
    }
};

struct EntityTransform_Action : EntityActionBase
{
    static bool FrameInitialize(const Data::Document& document, Data::Entity& entity, const EntityActionFrameContext& context);

    static Data::Point3D GetPosition(const Data::Entity& entity)
    {
        return entity.data.transform.translation;
    }
};

struct EntityLatex_Action : EntityActionBase
{
    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context);

    static Data::Point3D GetPosition(const Data::Entity& entity) { return ToPoint3D(entity.data.latex.position); }
};

struct EntityLinearGradient_Action : EntityActionBase
{
    static bool Initialize(const Data::Document& document, Data::Entity& entity, int entityIndex);

    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context);

    static Data::Point3D GetPosition(const Data::Entity& entity) { return Data::Point3D{ 0.0f, 0.0f, 0.0f }; }
};

struct EntityDigitalDissolve_Action : EntityActionBase
{
    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context);

    static Data::Point3D GetPosition(const Data::Entity& entity) { return Data::Point3D{ 0.0f, 0.0f, 0.0f }; }
};

struct EntityImage_Action : EntityActionBase
{
    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context);

    static Data::Point3D GetPosition(const Data::Entity& entity) { return ToPoint3D(entity.data.image.position); }
};

struct EntityFlipbook_Action : EntityActionBase
{
    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context);

    static Data::Point3D GetPosition(const Data::Entity& entity) { return ToPoint3D(entity.data.image.position); }

    static int GetImageIndex(const Data::Document& document, const Data::Entity& entity, const EntityActionFrameContext& context)
    {
        const Data::EntityFlipbook& flipbook = entity.data.flipbook;

        int imageIndex = 0;
        if (flipbook.timePerFrame > 0.0f)
            imageIndex = int((context.frameTime - entity.createTime) / flipbook.timePerFrame);
        else
            imageIndex = Max(0, context.frameIndex - SecondsToFrameIndex(document, entity.createTime));

        if (flipbook.loop)
            imageIndex = imageIndex % (int)flipbook.fileNames.size();
        else
            imageIndex = Min(imageIndex, (int)flipbook.fileNames.size() - 1);

        return imageIndex;
    }

    static void ExtraFrameHash(const Data::Document& document, Data::Entity& entity, const EntityActionFrameContext& context, size_t& hash)
    {
        int imageIndex = GetImageIndex(document, entity, context);
        Hash(hash, imageIndex);
    }
};

struct EntityCubicBezier_Action : EntityActionBase
{
    static bool DoAction(
        const Data::Document& document,
        const std::unordered_map<std::string, Data::Entity>& entityMap,
        std::vector<Data::ColorPMA>& pixels,
        const Data::Entity& entity,
        int threadId,
        const EntityActionFrameContext& context);

    static Data::Point3D GetPosition(const Data::Entity& entity)
    {
        return (
            entity.data.cubicBezier.A +
            entity.data.cubicBezier.B +
            entity.data.cubicBezier.C +
            entity.data.cubicBezier.D) / 4.0f;
    }
};

inline Data::Point3D GetParentPosition(
    const Data::Document& document,
    const std::unordered_map<std::string, Data::Entity>& entityMap,
    const Data::Entity& entity)
{
    if (entity.parent != "")
    {
        auto it = entityMap.find(entity.parent);
        if (it == entityMap.end())
        {
            printf("could not find entity parent %s\n", entity.parent.c_str());
            return Data::Point3D{ 0, 0, 0 };
        }

        const Data::Entity& parentEntity = it->second;
        Data::Point3D grandParentPosition = GetParentPosition(document, entityMap, parentEntity);

        switch (parentEntity.data._index)
        {
            #include "df_serialize/df_serialize/_common.h"
            #define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
                case Data::EntityVariant::c_index_##_NAME: return grandParentPosition + _TYPE##_Action::GetPosition(parentEntity);
            #include "df_serialize/df_serialize/_fillunsetdefines.h"
            #include "schemas/schemas_entities.h"
            default: printf("unhandled entity type in variant\n");
        }
    }

    return Data::Point3D{ 0, 0, 0 };
}
