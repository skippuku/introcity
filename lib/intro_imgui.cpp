extern "C" {
#include "intro.h"
}

#include "ext/stb_sprintf.h"

#include <imgui.h>

static const ImVec4 ptr_color    = ImVec4(0.9, 0.9, 0.2, 1.0);
static const ImVec4 struct_color = ImVec4(0.2, 0.9, 0.2, 1.0);
static const ImVec4 array_color  = ImVec4(0.8, 0.1, 0.3, 1.0);
static const ImVec4 default_color = ImVec4(1.0, 1.0, 1.0, 1.0);
static const ImVec4 enum_color   = ImVec4(0.9, 0.7, 0.1, 1.0);

int
intro_imgui_scalar_type(const IntroType * type) {
    switch(type->category) {
    case INTRO_U8:  return ImGuiDataType_U8;
    case INTRO_S8:  return ImGuiDataType_S8;
    case INTRO_U16: return ImGuiDataType_U16;
    case INTRO_S16: return ImGuiDataType_S16;
    case INTRO_U32: return ImGuiDataType_U32;
    case INTRO_S32: return ImGuiDataType_S32;
    case INTRO_U64: return ImGuiDataType_U64;
    case INTRO_S64: return ImGuiDataType_S64;
    case INTRO_F32: return ImGuiDataType_Float;
    case INTRO_F64: return ImGuiDataType_Double;
    default: return 0;
    }
}

static void edit_member(
    IntroContext * ctx,
    const char * name,
    void * member_data,
    const IntroType * type,
    int id,
    const void * container = NULL,
    const IntroType * container_type = NULL,
    const IntroMember * m = NULL
);

static void
edit_struct_children(IntroContext * ctx, void * src, const IntroType * s_type) {
    for (uint32_t m_index=0; m_index < s_type->i_struct->count_members; m_index++) {
        const IntroMember * m = &s_type->i_struct->members[m_index];
        edit_member(ctx, m->name, (char *)src + m->offset, m->type, m_index, src, s_type, m);
    }
}

static void
edit_array(IntroContext * ctx, void * src, const IntroType * type, int count) {
    size_t element_size = intro_size(type);
    for (int i=0; i < count; i++) {
        char name [64];
        stbsp_snprintf(name, 63, "[%i]", i);
        void * element_data = (char *)src + i * element_size;
        edit_member(ctx, name, element_data, type, i);
    }
}

static void
edit_member(IntroContext * ctx, const char * name, void * member_data, const IntroType * type, int id, const void * container, const IntroType * container_type, const IntroMember * m) {
    ImGui::PushID(id);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    int tree_flags = ImGuiTreeNodeFlags_SpanFullWidth;
    bool has_children = type->category == INTRO_STRUCT
                     || type->category == INTRO_UNION
                     || type->category == INTRO_ARRAY
                     || type->category == INTRO_POINTER;
    if (!has_children) {
        tree_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    bool is_open = ImGui::TreeNodeEx(name, tree_flags);

    int32_t note_index;
    if (m && intro_attribute_int(m, INTRO_ATTR_NOTE, &note_index)) {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            const char * note = ctx->notes[note_index];
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(note);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    int64_t length = -1;
    bool has_length = false;
    if (m && intro_attribute_length(container, container_type, m, &length)) {
        has_length = true;
    }

    ImGui::TableNextColumn();
    char type_buf [1024];
    intro_sprint_type_name(type_buf, type);
    ImVec4 type_color;
    switch(type->category) {
    case INTRO_STRUCT:
    case INTRO_UNION: type_color = struct_color; break;
    case INTRO_ENUM: type_color = enum_color; break;
    case INTRO_POINTER: type_color = ptr_color; break;
    case INTRO_ARRAY:   type_color = array_color; break;
    default: type_color = default_color; break;
    }
    ImGui::TextColored(type_color, "%s", type_buf);

    ImGui::TableNextColumn();
    ImGui::PushItemWidth(-1);
    if (type->category == INTRO_STRUCT || type->category == INTRO_UNION) {
        ImGui::TextDisabled("---");
        if (is_open) {
            edit_struct_children(ctx, member_data, type);
            ImGui::TreePop();
        }
    } else if (intro_is_scalar(type)) {
        if (type->name && strcmp(type->name, "bool") == 0) {
            ImGui::Checkbox("##", (bool *)member_data);
        } else {
            ImGui::DragScalar("##", intro_imgui_scalar_type(type), member_data);
        }
    } else if (type->category == INTRO_ENUM) {
        if (type->i_enum->is_flags) {
            int * flags_ptr = (int *)member_data;
            for (uint32_t e=0; e < type->i_enum->count_members; e++) {
                IntroEnumValue v = type->i_enum->members[e];
                ImGui::CheckboxFlags(v.name, flags_ptr, v.value);
            }
        } else {
            int current_value = *(int *)member_data;
            bool found_match = false;
            uint32_t current_index;
            for (uint32_t e=0; e < type->i_enum->count_members; e++) {
                IntroEnumValue v = type->i_enum->members[e];
                if (v.value == current_value) {
                    current_index = e;
                    found_match = true;
                }
            }
            if (!found_match) {
                ImGui::InputInt(NULL, (int *)member_data);
            } else {
                const char * preview = type->i_enum->members[current_index].name;
                if (ImGui::BeginCombo("##", preview)) {
                    for (uint32_t e=0; e < type->i_enum->count_members; e++) {
                        IntroEnumValue v = type->i_enum->members[e];
                        bool is_selected = (e == current_index);
                        if (ImGui::Selectable(v.name, is_selected)) {
                            current_index = e;
                        }
                        if (is_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                    *(int *)member_data = type->i_enum->members[current_index].value;
                }
            }
        }
    } else if (type->category == INTRO_ARRAY) {
        ImGui::TextDisabled("---");
        if (is_open) {
            edit_array(ctx, member_data, type->parent, (length > 0)? length : type->array_size);
            ImGui::TreePop();
        }
    } else if (type->category == INTRO_POINTER) {
        void * ptr_data = *(void **)member_data;
        if (ptr_data) {
            ImGui::TextColored(ptr_color, "0x%llx", (uintptr_t)ptr_data);
            if (!has_length) length = 1;
            if (length > 0) {
                if (is_open) {
                    edit_array(ctx, ptr_data, type->parent, length);
                }
            }
        } else {
            ImGui::TextDisabled("NULL");
        }
        if (is_open) {
            ImGui::TreePop();
        }
    } else {
        ImGui::TextDisabled("<unimplemented>");
    }
    ImGui::PopItemWidth();
    ImGui::PopID();
}

void
intro_imgui_edit_ctx(IntroContext * ctx, void * src, const IntroType * s_type, const char * name) {
    static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
    if (ImGui::BeginTable(name, 3, flags)) {
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("type");
        ImGui::TableSetupColumn("value");
        ImGui::TableHeadersRow();

        edit_member(ctx, name, src, s_type, (int)(uintptr_t)name);
        ImGui::EndTable();
    }
}
