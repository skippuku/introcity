extern "C" {
#include "intro.h"
}

#include <imgui.h>

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

static void
intro_imgui__edit_struct_children(IntroContext * ctx, void * src, const IntroType * s_type) {
    for (int m_index=0; m_index < s_type->i_struct->count_members; m_index++) {
        const IntroMember * m = &s_type->i_struct->members[m_index];
        void * member_data = (uint8_t *)src + m->offset;
        int tree_flags = ImGuiTreeNodeFlags_SpanFullWidth;
        if (!(m->type->category == INTRO_STRUCT || m->type->category == INTRO_UNION)) {
            tree_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        bool is_open = ImGui::TreeNodeEx(m->name, ImGuiTreeNodeFlags_SpanFullWidth);

        int32_t note_index;
        if (intro_attribute_int(m, INTRO_ATTR_NOTE, &note_index)) {
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

        ImGui::TableNextColumn();
        char type_buf [1024];
        intro_sprint_type_name(type_buf, m->type);
        ImGui::Text("%s", type_buf);

        ImGui::TableNextColumn();
        if (m->type->category == INTRO_STRUCT || m->type->category == INTRO_UNION) {
            ImGui::TextDisabled("vvv");
            if (is_open) {
                intro_imgui__edit_struct_children(ctx, member_data, m->type);
                ImGui::TreePop();
            }
        } else if (strcmp(m->type->name, "bool") == 0) {
            ImGui::Checkbox(NULL, (bool *)member_data);
        } else if (intro_is_scalar(m->type)) {
            ImGui::DragScalar(NULL, intro_imgui_scalar_type(m->type), member_data);
        } else if (m->type->category == INTRO_ENUM) {
            if (m->type->i_enum->is_flags) {
                int * flags_ptr = (int *)member_data;
                for (int e=0; e < m->type->i_enum->count_members; e++) {
                    IntroEnumValue v = m->type->i_enum->members[e];
                    ImGui::CheckboxFlags(v.name, flags_ptr, v.value);
                }
            } else {
                int current_value = *(int *)member_data;
                bool found_match = false;
                int current_index;
                for (int e=0; e < m->type->i_enum->count_members; e++) {
                    IntroEnumValue v = m->type->i_enum->members[e];
                    if (v.value == current_value) {
                        current_index = e;
                        found_match = true;
                    }
                }
                if (!found_match) {
                    ImGui::InputInt(NULL, (int *)member_data);
                } else {
                    const char * preview = m->type->i_enum->members[current_index].name;
                    if (ImGui::BeginCombo(NULL, preview)) {
                        for (int e=0; e < m->type->i_enum->count_members; e++) {
                            IntroEnumValue v = m->type->i_enum->members[e];
                            bool is_selected = (e == current_index);
                            if (ImGui::Selectable(v.name, is_selected)) {
                                current_index = e;
                            }
                            if (is_selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    *(int *)member_data = m->type->i_enum->members[current_index].value;
                }
            }
        }
    }
}

void
intro_imgui_edit_ctx(IntroContext * ctx, void * src, const IntroType * s_type, const char * name) {
    if (s_type->category == INTRO_STRUCT) {
        static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
        if (ImGui::BeginTable(name, 3, flags)) {
            ImGui::TableSetupColumn("name");
            ImGui::TableSetupColumn("type");
            ImGui::TableSetupColumn("value");
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool is_open = ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_SpanFullWidth);

            ImGui::TableNextColumn();
            char type_buf [1024];
            intro_sprint_type_name(type_buf, s_type);
            ImGui::Text("%s", type_buf);

            ImGui::TableNextColumn();
            ImGui::TextDisabled("vvv");

            if (is_open) {
                intro_imgui__edit_struct_children(ctx, src, s_type);
                ImGui::TreePop();
            }

            ImGui::EndTable();
        }
    }
}
