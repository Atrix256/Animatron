// This makes imgui UI for the types

#pragma once

#include "../df_serialize/_common.h"

// Enums 

#define ENUM_BEGIN(_NAMESPACE, _NAME, _DESCRIPTION) \
	bool ShowUI(_NAMESPACE::_NAME& value, const char* label) \
	{ \
        typedef _NAMESPACE::_NAME ThisType; \
        int v = (int)value; \
        bool ret = ImGui::Combo("", &v,

#define ENUM_ITEM(_NAME, _DESCRIPTION) \
        #_NAME "\0"

#define ENUM_END() \
        ); \
        if (ret) \
            value = (ThisType)v; \
        return ret; \
    }

// Structs

#define STRUCT_BEGIN(_NAMESPACE, _NAME, _DESCRIPTION) \
	bool ShowUI(_NAMESPACE::_NAME& value, const char* label) \
	{ \
        using namespace _NAMESPACE; \
        bool ret = false; \
        if (label[0] == 0 || ImGui::TreeNode(label)) \
        { \
            ImGui::PushID(#_NAME);

#define STRUCT_INHERIT_BEGIN(_NAMESPACE, _NAME, _BASE, _DESCRIPTION) \
	bool ShowUI(_NAMESPACE::_NAME& value, const char* label) \
	{ \
        using namespace _NAMESPACE; \
        bool ret = false; \
        if (label[0] == 0 || ImGui::TreeNode(label)) \
        { \
            ImGui::PushID(#_NAME); \
            ret = ShowUI(*(_BASE*)&value, "");

#define STRUCT_FIELD(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
            ImGui::PushID(#_NAME); \
		    ret |= ShowUI(value._NAME, #_NAME); \
            ImGui::PopID();

// No serialize means no editor reflection
#define STRUCT_FIELD_NO_SERIALIZE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION)

#define STRUCT_DYNAMIC_ARRAY(_TYPE, _NAME, _DESCRIPTION) \
            if (ImGui::TreeNode(#_NAME)) \
            { \
                int deleteIndex = -1; \
                for (size_t index = 0; index < TDYNAMICARRAY_SIZE(value._NAME); ++index) \
                { \
                    ImGui::Separator(); \
                    ImGui::PushID((int)index); \
                    ret |= ShowUI(value._NAME[index], ""); \
                    if (ImGui::Button("Delete")) \
                        deleteIndex = (int)index; \
                    ImGui::PopID(); \
                } \
                ImGui::Separator(); \
                if (ImGui::Button("Add")) \
                { \
                    ret = true; \
                    value._NAME.push_back(_TYPE{}); \
                } \
                ImGui::Separator(); \
                ImGui::TreePop(); \
                if (deleteIndex != -1) \
                { \
                    ret = true; \
                    value._NAME.erase(value._NAME.begin() + deleteIndex); \
                } \
            } 

#define STRUCT_STATIC_ARRAY(_TYPE, _NAME, _SIZE, _DEFAULT, _DESCRIPTION) \
            if (ImGui::TreeNode(#_NAME "[" #_SIZE "]")) \
            { \
                for (size_t index = 0; index < _SIZE; ++index) \
                { \
                    ImGui::PushID((int)index); \
                    ret |= ShowUI(value._NAME[index], ""); \
                    ImGui::PopID(); \
                } \
                if (label[0] != 0) \
                    ImGui::TreePop(); \
            }

#define STRUCT_END() \
            ImGui::PopID(); \
            ImGui::TreePop(); \
        } \
        return ret; \
	}

// Variants

#define VARIANT_BEGIN(_NAMESPACE, _NAME, _DESCRIPTION) \
	bool ShowUI(_NAMESPACE::_NAME& value, const char* label) \
	{ \
        bool ret = false; \
        if (ImGui::TreeNode(label)) \
        { \
		    typedef _NAMESPACE::_NAME ThisType; \
            ImGui::PushID(#_NAME); \
            int selectedIndex = -1; \
            const auto& typeInfo = g_variantTypeInfo_##_NAMESPACE##_##_NAME; \
            for (int i = 0; i < sizeof(typeInfo) / sizeof(typeInfo[0]); ++i) \
            { \
                if(value._index == typeInfo[i]._index) \
                    selectedIndex = i; \
            } \
            if (ImGui::BeginCombo("Object Type", selectedIndex < sizeof(typeInfo) / sizeof(typeInfo[0]) ? typeInfo[selectedIndex].name.c_str() : "", 0)) \
            { \
                for (int n = 0; n < sizeof(typeInfo) / sizeof(typeInfo[0]); ++n) \
                { \
                    const bool selected = (selectedIndex == n); \
                    if (ImGui::Selectable(typeInfo[n].name.c_str(), selected)) \
                    { \
                        value._index = typeInfo[n]._index; \
                        ret = true; \
                    } \
                    /* Set the initial focus when opening the combo (scrolling + keyboard navigation focus) */ \
                    if (selected) \
                        ImGui::SetItemDefaultFocus(); \
                } \
                ImGui::EndCombo(); \
            }

#define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) \
            if (value._index == ThisType::c_index_##_NAME) \
                ret |= ShowUI(value._NAME, "");

#define VARIANT_END() \
            ImGui::PopID(); \
            ImGui::TreePop(); \
        } \
        return ret; \
	}

// Built in types

bool ShowUI(uint8_t& value, const char* label)
{
    bool ret = false;
    int v = (int)value;
    if (ImGui::InputInt(label, &v))
    {
        value = (uint8_t)v;
        ret = true;
    }
    return ret;
}

bool ShowUI(uint16_t& value, const char* label)
{
    bool ret = false;
    int v = (int)value;
    if (ImGui::InputInt(label, &v))
    {
        value = (uint16_t)v;
        ret = true;
    }
    return ret;
}

bool ShowUI(uint32_t& value, const char* label)
{
    bool ret = false;
    int v = (int)value;
    if (ImGui::InputInt(label, &v))
    {
        value = (uint32_t)v;
        ret = true;
    }
    return ret;
}

bool ShowUI(uint64_t& value, const char* label)
{
    bool ret = false;
    int v = (int)value;
    if (ImGui::InputInt(label, &v))
    {
        value = (uint64_t)v;
        ret = true;
    }
    return ret;
}

bool ShowUI(int8_t& value, const char* label)
{
    bool ret = false;
    int v = (int)value;
    if (ImGui::InputInt(label, &v))
    {
        value = (int8_t)v;
        ret = true;
    }
    return ret;
}

bool ShowUI(int16_t& value, const char* label)
{
    bool ret = false;
    int v = (int)value;
    if (ImGui::InputInt(label, &v))
    {
        value = (int16_t)v;
        ret = true;
    }
    return ret;
}

bool ShowUI(int32_t& value, const char* label)
{
    bool ret = false;
    int v = (int)value;
    if (ImGui::InputInt(label, &v))
    {
        value = (int32_t)v;
        ret = true;
    }
    return ret;
}

bool ShowUI(int64_t& value, const char* label)
{
    bool ret = false;
    int v = (int)value;
    if (ImGui::InputInt(label, &v))
    {
        value = (int64_t)v;
        ret = true;
    }
    return ret;
}

bool ShowUI(float& value, const char* label)
{
    bool ret = ImGui::InputFloat(label, &value);
    return ret;
}

bool ShowUI(bool& value, const char* label)
{
    bool ret = ImGui::Checkbox(label , &value);
    return ret;
}

bool ShowUI(std::string& value, const char* label)
{
    bool ret = false;

    char buffer[1024];
    strcpy_s(buffer, value.c_str());
    if (ImGui::InputText(label, buffer, 1024))
    {
        value = buffer;
        ret = true;
    }
    return ret;
}
