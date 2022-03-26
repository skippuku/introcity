#include <stdlib.h>
#include <string.h>
#include "intro.h"
#include "util.c"

static char *
get_ref_name(IntroInfo * info, const IntroType * t) {
    if (!t->name) {
        for (int t2_index=15; t2_index < info->count_types; t2_index++) {
            const IntroType * t2 = info->types[t2_index];
            if (t2 == t) continue;
            if (t2->category == t->category && t2->i_struct == t->i_struct) {
                return t2->name;
            }
        }
        fprintf(stderr, "no way to reference anonymous struct.\n");
        return NULL;
    } else {
        return t->name;
    }
}

static char *
get_parent_member_name(IntroInfo * info, IntroType * parent, int parent_index, char ** o_top_level_name) {
    NestInfo * nest = hmgetp_null(info->nest_map, parent);
    if (nest) {
        IntroType * grand_parent = nest->parent;
        int grand_parent_index = nest->member_index;

        char * result = get_parent_member_name(info, grand_parent, grand_parent_index, o_top_level_name);
        strputf(&result, ".%s", parent->i_struct->members[parent_index].name);
        return result;
    } else {
        char * result = NULL;
        strputf(&result, "%s", parent->i_struct->members[parent_index].name);
        char * top_level_name = get_ref_name(info, parent);
        *o_top_level_name = top_level_name;
        return result;
    }
}

char *
generate_c_header(IntroInfo * info) {
    for (int i=0; i < hmlen(info->nest_map); i++) {
        NestInfo * nest = &info->nest_map[i];
        nest->parent_member_name = get_parent_member_name(info, nest->parent, nest->member_index, &nest->top_level_name);
    }

    char * s = NULL;

    const char * tab = "    ";

    const char * version = "v0.0";
    strputf(&s, "/* Generated with intro %s */\n\n", version);
    strputf(&s, "#include <stddef.h>\n\n");

    struct {
        IntroStruct * key;
        char * value;
    } * complex_type_map = NULL;

    // forward declare type list
    strputf(&s, "extern IntroType __intro_types [%u];\n\n", info->count_types);

    // attributes
    // NOTE: maybe this should be one long list and members point into an offset into the list
    for (int type_index = 0; type_index < info->count_types; type_index++) {
        const IntroType * type = info->types[type_index];
        if ((type->category == INTRO_STRUCT || type->category == INTRO_UNION) && type->parent == NULL) {
            for (int member_index = 0; member_index < type->i_struct->count_members; member_index++) {
                const IntroMember * m = &type->i_struct->members[member_index];
                if (m->count_attributes > 0) {
                    strputf(&s, "const IntroAttributeData __intro__attr_%i_%i [] = {\n", type_index, member_index);
                    for (int attr_index = 0; attr_index < m->count_attributes; attr_index++) {
                        const IntroAttributeData * attr = &m->attributes[attr_index];
                        strputf(&s, "%s{%i, %i, %i},\n", tab, attr->type, attr->value_type, attr->v.i);
                    }
                    strputf(&s, "};\n\n");
                }
            }
        }
    }

    if (note_set != NULL) {
        strputf(&s, "const char * __intro__notes [] = {\n");
        for (int i=0; i < arrlen(note_set); i++) {
            strputf(&s, "%s\"%s\",\n", tab, note_set[i]);
        }
        strputf(&s, "};\n\n");
    }

    // complex type information (enums, structs, unions)
    for (int type_index = 0; type_index < info->count_types; type_index++) {
        IntroType * t = info->types[type_index];
        if (t->category == INTRO_STRUCT && hmgeti(complex_type_map, t->i_struct) < 0) {
            const NestInfo * nest = hmgetp_null(info->nest_map, t);
            char * ref_name;
            if (!nest) {
                ref_name = get_ref_name(info, t);
            } else {
                ref_name = NULL;
                strputf(&ref_name, "%s_%s", nest->top_level_name, nest->parent_member_name);
                for (int i=0; i < arrlen(ref_name); i++) {
                    if (ref_name[i] == '.' || ref_name[i] == ' ') {
                        ref_name[i] = '_';
                    }
                }
                strputnull(ref_name);
            }
            if (!ref_name) return NULL;

            char * saved_name;
            if (strchr(ref_name, ' ')) {
                size_t name_len = strlen(ref_name);
                saved_name = malloc(name_len + 1);
                for (int i=0; i < name_len; i++) {
                    saved_name[i] = (ref_name[i] == ' ')? '_' : ref_name[i];
                }
                saved_name[name_len] = '\0';
            } else {
                saved_name = ref_name;
            }

            hmput(complex_type_map, t->i_struct, saved_name);

            strputf(&s, "IntroStruct __intro_%s = {%u, %u, {\n", saved_name, t->i_struct->count_members, t->i_struct->is_union);
            for (int m_index = 0; m_index < t->i_struct->count_members; m_index++) {
                const IntroMember * m = &t->i_struct->members[m_index];
                int32_t member_type_index = hmget(info->index_by_ptr_map, m->type);
                // TODO: attributes
                strputf(&s, "%s{\"%s\", &__intro_types[%i], ", tab, m->name, member_type_index);
                if (!nest) {
                    strputf(&s, "offsetof(%s, %s)", ref_name, m->name);
                } else {
                    strputf(&s, "offsetof(%s, %s.%s) - offsetof(%s, %s)",
                            nest->top_level_name, nest->parent_member_name, m->name,
                            nest->top_level_name, nest->parent_member_name);
                }
                if (m->count_attributes > 0) {
                    strputf(&s, ", %u, __intro__attr_%i_%i},\n", m->count_attributes, type_index, m_index);
                } else {
                    strputf(&s, ", 0},\n");
                }
            }
            strputf(&s, "}};\n\n");
        }
    }

    // type list
    strputf(&s, "IntroType __intro_types [%u] = {\n", info->count_types);
    for (int type_index = 0; type_index < info->count_types; type_index++) {
        const IntroType * t = info->types[type_index];
        strputf(&s, "%s{", tab);
        if (t->name) {
            strputf(&s, "\"%s\", ", t->name);
        } else {
            strputf(&s, "0, ");
        }
        if (t->parent) {
            int32_t parent_index = hmget(info->index_by_ptr_map, t->parent);
            strputf(&s, "&__intro_types[%i], ", parent_index);
        } else {
            strputf(&s, "0, ");
        }
        strputf(&s, "0x%03x, ", t->category);
        if (is_complex(t->category)) {
            char * saved_name = hmget(complex_type_map, t->i_struct);
            strputf(&s, ".%s=&__intro_%s},\n", (t->category == INTRO_ENUM)? "i_enum" : "i_struct", saved_name);
        } else {
            strputf(&s, "%u},\n", (t->category == INTRO_ARRAY)? t->array_size : 0);
        }
    }
    strputf(&s, "};\n\n");

    strputnull(s);
    return s;
}
