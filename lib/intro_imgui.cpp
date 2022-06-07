extern "C" {
#include "intro.h"
}

#include "ext/stb_sprintf.h"

#include <imgui.h>

#define GUIATTR(x) (ctx->attr.builtin.gui_##x)

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

    // TODO: i hate this
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

struct IntroImGuiScalarParams {
    float scale;
    void * min, * max;
    const char * format;
};

static IntroImGuiScalarParams
get_scalar_params(IntroContext * ctx, const IntroType * type, const IntroMember * m) {
    uint32_t attr = (m)? m->attr : type->attr;
    IntroImGuiScalarParams result = {};
    result.scale = 1.0f;
    intro_attribute_float_x(ctx, attr, GUIATTR(scale), &result.scale);
            
    IntroVariant max_var = {0}, min_var = {0};
    if (m) {
        intro_attribute_value_x(ctx, m, GUIATTR(min), &min_var);
        intro_attribute_value_x(ctx, m, GUIATTR(max), &max_var);
        result.min = min_var.data;
        result.max = max_var.data;
    }
    result.format = intro_attribute_string_x(ctx, attr, GUIATTR(format));

    return result;
}

static void
edit_member(IntroContext * ctx, const char * name, void * member_data, const IntroType * type, int id, const void * container, const IntroType * container_type, const IntroMember * m) {
    uint32_t attr = (m)? m->attr : type->attr;

    if (!intro_has_attribute_x(ctx, attr, GUIATTR(show))) {
        return;
    }

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

    const char * note = NULL;
    if (m && (note = intro_attribute_string_x(ctx, m->attr, (uint32_t)GUIATTR(note))) != NULL) {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(note);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    int64_t length = -1;
    bool has_length = false;
    if (m && intro_attribute_length_x(ctx, container, container_type, m, &length)) {
        has_length = true;
    }

    ImGui::TableNextColumn();
    char type_buf [1024];
    intro_sprint_type_name(type_buf, type);
    ImVec4 type_color;
    IntroVariant colorv;
    if (m && intro_attribute_value_x(ctx, m, GUIATTR(color), &colorv)) {
        uint8_t ch [4];
        memcpy(&ch, colorv.data, 4);
        type_color = ImVec4(ch[0] / 256.0f, ch[1] / 256.0f, ch[2] / 256.0f, ch[3] / 256.0f);
    } else {
        switch(type->category) {
        case INTRO_STRUCT:
        case INTRO_UNION: type_color = struct_color; break;
        case INTRO_ENUM: type_color = enum_color; break;
        case INTRO_POINTER: type_color = ptr_color; break;
        case INTRO_ARRAY:   type_color = array_color; break;
        default: type_color = default_color; break;
        }
    }
    if (has_length) {
        ImGui::TextColored(type_color, "%s (%li)", type_buf, length);
    } else {
        ImGui::TextColored(type_color, "%s", type_buf);
    }

    bool do_disable_pop = false;
    if (!intro_has_attribute_x(ctx, attr, GUIATTR(edit))) {
        ImGui::BeginDisabled();
        do_disable_pop = true;
    }
    ImGui::TableNextColumn();
    ImGui::PushItemWidth(-1);

    bool do_tree_place_holder = true;
    if (intro_has_attribute_x(ctx, attr, GUIATTR(edit_color))) {
        size_t size = intro_size(type);
        switch(size) {
        case 12:
            ImGui::ColorEdit3("##", (float *)member_data);
            break;
        case 16:
            ImGui::ColorEdit4("##", (float *)member_data);
            break;
        case 4:
            float im_color [4];
            uint8_t * buf = (uint8_t *)member_data;
            im_color[0] = buf[0] / 256.0f;
            im_color[1] = buf[1] / 256.0f;
            im_color[2] = buf[2] / 256.0f;
            im_color[3] = buf[3] / 256.0f;

            ImGui::ColorEdit4("##", im_color);

            buf[0] = im_color[0] * 256;
            buf[1] = im_color[1] * 256;
            buf[2] = im_color[2] * 256;
            buf[3] = im_color[3] * 256;
            break;
        defualt:
            ImGui::Text("bad color size");
            break;
        }
        do_tree_place_holder = false;
    }

    if (intro_has_attribute_x(ctx, attr, GUIATTR(vector))) {
        int count_components;
        const IntroType * scalar_type;
        if (type->category == INTRO_ARRAY) {
            count_components = type->array_size;
            scalar_type = type->of;
        } else if (type->category == INTRO_STRUCT || type->category == INTRO_UNION) {
            const IntroType * m_type = type->i_struct->members[0].type;
            scalar_type = m_type;
            count_components = intro_size(type) / intro_size(m_type);
        }
        auto param = get_scalar_params(ctx, type, m);
        ImGui::DragScalarN("##", intro_imgui_scalar_type(scalar_type), member_data, count_components, param.scale, param.min, param.max, param.format);
        do_tree_place_holder = false;
    }

    if (type->category == INTRO_STRUCT || type->category == INTRO_UNION) {
        if (do_tree_place_holder) ImGui::TextDisabled("---");
        if (is_open) {
            edit_struct_children(ctx, member_data, type);
            ImGui::TreePop();
        }
    } else if (intro_is_scalar(type)) {
        if (type->name && strcmp(type->name, "bool") == 0) {
            ImGui::Checkbox("##", (bool *)member_data);
        } else {
            auto param = get_scalar_params(ctx, type, m);

            ImGui::DragScalar("##", intro_imgui_scalar_type(type), member_data, param.scale, param.min, param.max, param.format);
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
        if (do_tree_place_holder) ImGui::TextDisabled("---");
        if (is_open) {
            edit_array(ctx, member_data, type->of, (length > 0)? length : type->array_size);
            ImGui::TreePop();
        }
    } else if (type->category == INTRO_POINTER) {
        void * ptr_data = *(void **)member_data;
        if (ptr_data) {
            ImGui::TextColored(ptr_color, "0x%llx", (uintptr_t)ptr_data);
            if (!has_length) length = 1;
            if (length > 0) {
                if (is_open) {
                    edit_array(ctx, ptr_data, type->of, length);
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
    if (do_disable_pop) ImGui::EndDisabled();
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
