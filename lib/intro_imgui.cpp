extern "C" {
#include "intro.h"
}

#include "ext/stb_sprintf.h"

#include <imgui.h>

#define GUIATTR(x) (ctx->attr.builtin.gui_##x)

static const ImVec4 ptr_color     = ImVec4(0.9, 0.9, 0.2, 1.0);
static const ImVec4 struct_color  = ImVec4(0.2, 0.9, 0.2, 1.0);
static const ImVec4 array_color   = ImVec4(0.8, 0.1, 0.3, 1.0);
static const ImVec4 default_color = ImVec4(1.0, 1.0, 1.0, 1.0);
static const ImVec4 enum_color    = ImVec4(0.9, 0.7, 0.1, 1.0);

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

static void edit_member(IntroContext *, const char *, IntroContainer, int);

static void
edit_array(IntroContext * ctx, const IntroContainer * p_cont, int count) {
    size_t element_size = p_cont->type->size;
    for (int i=0; i < count; i++) {
        char name [64];
        stbsp_snprintf(name, 63, "[%i]", i);
        void * element_data = (uint8_t *)p_cont->data + i * element_size;
        edit_member(ctx, name, intro_push(p_cont, i), i);
    }
}

struct IntroImGuiScalarParams {
    float scale;
    void * min, * max;
    const char * format;
};

static IntroImGuiScalarParams
get_scalar_params(IntroContext * ctx, const IntroType * type, uint32_t attr) {
    IntroImGuiScalarParams result = {};
    result.scale = 1.0f;
    intro_attribute_float_x(ctx, attr, GUIATTR(scale), &result.scale);
            
    IntroVariant max_var = {0}, min_var = {0}, var;
    intro_attribute_value_x(ctx, type, attr, GUIATTR(min), &min_var);
    intro_attribute_value_x(ctx, type, attr, GUIATTR(max), &max_var);
    result.min = min_var.data;
    result.max = max_var.data;
    result.format = (intro_attribute_value_x(ctx, NULL, attr, GUIATTR(format), &var))? (const char *)var.data : NULL;

    return result;
}

static void
do_note(const char * note) {
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

ImU32
color_from_var(const void * in) {
    auto buf = reinterpret_cast<const uint8_t *>(in);
    return IM_COL32(buf[0], buf[1], buf[2], buf[3]);
}

static void
edit_member(IntroContext * ctx, const char * name, IntroContainer cont, int id) {
    const IntroType * type = cont.type;
    const IntroMember * m = NULL;
    if (cont.parent && intro_has_members(cont.parent->type)) {
        m = &cont.parent->type->members[cont.index];
    }
    uint32_t attr = (m)? m->attr : type->attr;

    int64_t expr_result;
    if (
        !intro_has_attribute_x(ctx, attr, GUIATTR(show))
      ||(intro_attribute_expr_x(ctx, cont, ctx->attr.builtin.i_when, &expr_result) && !expr_result)
       )
    {
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

    IntroVariant colorv;
    ImU32 member_color = 0xffffffff;
    if (m && intro_attribute_value_x(ctx, type, m->attr, GUIATTR(color), &colorv)) {
        member_color = color_from_var(colorv.data);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, member_color);
    bool is_open = ImGui::TreeNodeEx((name)? name : "<anon>", tree_flags);
    ImGui::PopStyleColor();

    char type_buf [1024];
    intro_sprint_type_name(type_buf, type);

    ImVec4 type_color;
    if (intro_attribute_value_x(ctx, type, type->attr, GUIATTR(color), &colorv)) {
        type_color = ImColor(color_from_var(colorv.data)).Value;
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

    // drag drop

    if (ImGui::BeginDragDropSource()) {
        IntroVariant var;
        var.data = cont.data;
        var.type = type;
        ImGui::SetDragDropPayload("IntroVariant", &var, sizeof(IntroVariant));

        ImGui::TextColored(type_color, "%s", type_buf);

        ImGui::EndDragDropSource();
    }

    const ImGuiPayload * payload = ImGui::GetDragDropPayload();
    if (payload && payload->IsDataType("IntroVariant")) {
        auto var = (const IntroVariant *)payload->Data;
        if (var->type == type) {
            ImGui::SameLine(); ImGui::TextColored(ptr_color, "[ ]");
            if (ImGui::BeginDragDropTarget()) {
                if (ImGui::AcceptDragDropPayload("IntroVariant")) {
                    memcpy(cont.data, var->data, type->size);
                }
                
                ImGui::EndDragDropTarget();
            }
        }
    }

    const char * note = NULL;
    IntroVariant var;
    if (m && intro_attribute_value_x(ctx, NULL, m->attr, GUIATTR(note), &var)) {
        note = (char *)var.data;
        do_note(note);
    }

    int64_t length = -1;
    bool has_length = false;
    if (m && intro_attribute_length_x(ctx, cont, &length)) {
        has_length = true;
    }

    ImGui::TableNextColumn();
    if (has_length) {
        ImGui::TextColored(type_color, "%s (%li)", type_buf, (long int)length);
    } else {
        ImGui::TextColored(type_color, "%s", type_buf);
    }
    if (intro_attribute_value_x(ctx, NULL, type->attr, GUIATTR(note), &var)) {
        note = (char *)var.data;
        do_note(note);
    }

    ImGui::TableNextColumn();
    ImGui::PushItemWidth(-1);

    bool do_tree_place_holder = true;
    if (intro_has_attribute_x(ctx, attr, GUIATTR(edit_color))) {
        size_t size = type->size;
        switch(size) {
        case 12:
            ImGui::ColorEdit3("##", (float *)cont.data);
            break;
        case 16:
            ImGui::ColorEdit4("##", (float *)cont.data);
            break;
        case 4:
            float im_color [4];
            uint8_t * buf = (uint8_t *)cont.data;
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
            count_components = type->count;
            scalar_type = type->of;
        } else if (type->category == INTRO_STRUCT || type->category == INTRO_UNION) {
            const IntroType * m_type = type->members[0].type;
            scalar_type = m_type;
            count_components = type->size / m_type->size;
        }
        auto param = get_scalar_params(ctx, type, attr);
        ImGui::DragScalarN("##", intro_imgui_scalar_type(scalar_type), cont.data, count_components, param.scale, param.min, param.max, param.format);
        do_tree_place_holder = false;
    }

    if (type->category == INTRO_STRUCT || type->category == INTRO_UNION) {
        if (do_tree_place_holder) ImGui::TextDisabled("---");
        if (is_open) {
            for (uint32_t m_index=0; m_index < cont.type->count; m_index++) {
                const IntroMember * m = &cont.type->members[m_index];
                edit_member(ctx, m->name, intro_push(&cont, m_index), m_index);
            }
            ImGui::TreePop();
        }
    } else if (intro_is_scalar(type)) {
        if (type->name && strcmp(type->name, "bool") == 0) {
            ImGui::Checkbox("##", (bool *)cont.data);
        } else {
            auto param = get_scalar_params(ctx, type, attr);

            ImGui::DragScalar("##", intro_imgui_scalar_type(type), cont.data, param.scale, param.min, param.max, param.format);
        }
    } else if (type->category == INTRO_ENUM) {
        if ((type->flags & INTRO_IS_FLAGS)) {
            int * flags_ptr = (int *)cont.data;
            for (uint32_t e=0; e < type->count; e++) {
                IntroEnumValue v = type->values[e];
                ImGui::CheckboxFlags(v.name, flags_ptr, v.value);
            }
        } else {
            int current_value = *(int *)cont.data;
            bool found_match = false;
            uint32_t current_index;
            for (uint32_t e=0; e < type->count; e++) {
                IntroEnumValue v = type->values[e];
                if (v.value == current_value) {
                    current_index = e;
                    found_match = true;
                }
            }
            if (!found_match) {
                ImGui::InputInt(NULL, (int *)cont.data);
            } else {
                const char * preview = type->values[current_index].name;
                if (ImGui::BeginCombo("##", preview)) {
                    for (uint32_t e=0; e < type->count; e++) {
                        IntroEnumValue v = type->values[e];
                        bool is_selected = (e == current_index);
                        if (ImGui::Selectable(v.name, is_selected)) {
                            current_index = e;
                        }
                        if (is_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                    *(int *)cont.data = type->values[current_index].value;
                }
            }
        }
    } else if (type->category == INTRO_ARRAY) {
        if (do_tree_place_holder) {
            if (intro_has_attribute_x(ctx, attr, GUIATTR(edit_text))) {
                ImGui::InputText("##", (char *)cont.data, type->count);
            } else {
                ImGui::TextDisabled("---");
            }
        }
        if (is_open) {
            edit_array(ctx, &cont, (length > 0)? length : type->count);
            ImGui::TreePop();
        }
    } else if (type->category == INTRO_POINTER) {
        void * ptr_data = *(void **)cont.data;
        if (intro_has_attribute_x(ctx, attr, ctx->attr.builtin.i_cstring)) {
            ImGui::Text("\"%s\"", (const char *)ptr_data);
        } else if (ptr_data) {
            ImGui::TextColored(ptr_color, "0x%llx", (uintptr_t)ptr_data);
            if (!has_length) length = 1;
            if (length > 0) {
                if (is_open) {
                    edit_array(ctx, &cont, length);
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
intro_imgui_edit_x(IntroContext * ctx, IntroContainer cont, const char * name) {
    static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
    if (ImGui::BeginTable(name, 3, flags)) {
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("type");
        ImGui::TableSetupColumn("value");
        ImGui::TableHeadersRow();

        edit_member(ctx, name, cont, (int)(uintptr_t)name);
        ImGui::EndTable();
    }
}
