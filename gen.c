#include "lib/intro.h"
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

static char *
make_identifier_safe_name(const char * name) {
    size_t name_len = strlen(name);
    char * result = malloc(name_len + 1);
    for (int i=0; i < name_len; i++) {
        result[i] = (name[i] == ' ')? '_' : name[i];
    }
    result[name_len] = '\0';
    return result;
}

char *
generate_c_header(IntroInfo * info) {
    // generate info needed to get offset and sizeof for anonymous types
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
    strputf(&s, "const IntroAttributeData __intro_attr [] = {\n");
    for (int type_index = 0; type_index < info->count_types; type_index++) {
        const IntroType * type = info->types[type_index];
        if ((type->category == INTRO_STRUCT || type->category == INTRO_UNION) && type->parent == NULL) {
            for (int member_index = 0; member_index < type->i_struct->count_members; member_index++) {
                const IntroMember * m = &type->i_struct->members[member_index];
                if (m->count_attributes > 0) {
                    for (int attr_index = 0; attr_index < m->count_attributes; attr_index++) {
                        const IntroAttributeData * attr = &m->attributes[attr_index];
                        strputf(&s, "%s{%i, %i, %i},\n", tab, attr->type, attr->value_type, attr->v.i);
                    }
                }
            }
        }
    }
    strputf(&s, "};\n\n");

    strputf(&s, "const char * __intro_notes [%i] = {\n", (int)arrlen(note_set));
    for (int i=0; i < arrlen(note_set); i++) {
        strputf(&s, "%s\"%s\",\n", tab, note_set[i]);
    }
    strputf(&s, "};\n\n");

    // complex type information (enums, structs, unions)
    int attr_list_index = 0;
    for (int type_index = 0; type_index < info->count_types; type_index++) {
        IntroType * t = info->types[type_index];
        if (intro_is_complex(t) && hmgeti(complex_type_map, t->i_struct) < 0) {
            const NestInfo * nest = hmgetp_null(info->nest_map, t);
            char * ref_name = NULL;
            if (!nest) {
                ref_name = get_ref_name(info, t);
                if (!ref_name) continue; // TODO: maybe we should warn here (this would require location information for types)
            } else {
                if (nest->top_level_name == NULL) continue;
            }

            char * saved_name = malloc(8);
            stbsp_snprintf(saved_name, 7, "%04x", type_index);

            hmput(complex_type_map, t->i_struct, saved_name);

            strputf(&s, "%s __intro_%s = {", (t->category == INTRO_ENUM)? "IntroEnum" : "IntroStruct", saved_name);
            if (!nest) {
                strputf(&s, "sizeof(%s)", ref_name);
            } else {
                strputf(&s, "sizeof(((%s*)0)->%s)", nest->top_level_name, nest->parent_member_name);
            }
            if (t->category == INTRO_STRUCT || t->category == INTRO_UNION) {
                strputf(&s, ", %u, %u, {\n", t->i_struct->count_members, t->i_struct->is_union);
                for (int m_index = 0; m_index < t->i_struct->count_members; m_index++) {
                    const IntroMember * m = &t->i_struct->members[m_index];
                    int32_t member_type_index = hmget(info->index_by_ptr_map, m->type);
                    strputf(&s, "%s{\"%s\", &__intro_types[%i], ", tab, m->name, member_type_index);
                    if (!nest) {
                        strputf(&s, "offsetof(%s, %s)", ref_name, m->name);
                    } else {
                        strputf(&s, "offsetof(%s, %s.%s) - offsetof(%s, %s)",
                                nest->top_level_name, nest->parent_member_name, m->name,
                                nest->top_level_name, nest->parent_member_name);
                    }
                    if (m->count_attributes > 0) {
                        strputf(&s, ", %u, &__intro_attr[%i]},\n", m->count_attributes, attr_list_index);
                        attr_list_index += m->count_attributes;
                    } else {
                        strputf(&s, ", 0},\n");
                    }
                }
            } else if (t->category == INTRO_ENUM) {
                strputf(&s, ", %u, %u, %u, {\n", t->i_enum->count_members, t->i_enum->is_flags, t->i_enum->is_sequential);
                for (int m_index = 0; m_index < t->i_enum->count_members; m_index++) {
                    const IntroEnumValue * v = &t->i_enum->members[m_index];
                    strputf(&s, "%s{\"%s\", %i},\n", tab, v->name, v->value);
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
        strputf(&s, "0x%02x, ", t->category);
        if (intro_is_complex(t)) {
            char * saved_name = hmget(complex_type_map, t->i_struct);
            if (saved_name) {
                strputf(&s, ".%s=&__intro_%s},\n", (t->category == INTRO_ENUM)? "i_enum" : "i_struct", saved_name);
            } else {
                strputf(&s, "0},\n");
            }
        } else {
            strputf(&s, "%u},\n", (t->category == INTRO_ARRAY)? t->array_size : 0);
        }
    }
    strputf(&s, "};\n\n");

    // values
    strputf(&s, "unsigned char __intro_values [%i] = {", (int)arrlen(info->value_buffer));
    for (int i=0; i < arrlen(info->value_buffer); i++) {
        if (i % 16 == 0) {
            strputf(&s, "\n%s", tab);
        }
        uint8_t byte = info->value_buffer[i];
        strputf(&s, "0x%02x, ", byte);
    }
    strputf(&s, "\n};\n\n");

    // type enum
    strputf(&s, "enum {\n");
    for (int type_index = 0; type_index < info->count_types; type_index++) {
        const IntroType * t = info->types[type_index];
        char * name;
        if (t->name) {
            if (strchr(t->name, ' ')) {
                name = make_identifier_safe_name(t->name);
            } else {
                name = t->name;
            }
            strputf(&s, "%sITYPE_%s = %i,\n", tab, name, type_index);
            if (name != t->name) free(name);
        }
    }
    strputf(&s, "};\n\n");

    // context
    strputf(&s, "IntroContext __intro_ctx = {\n");
    strputf(&s, "%s.types = __intro_types,\n", tab);
    strputf(&s, "%s.values = __intro_values,\n", tab);
    strputf(&s, "%s.notes = __intro_notes,\n", tab);
    strputf(&s, "%s.count_types = %u,\n", tab, info->count_types);
    strputf(&s, "%s.size_values = %i,\n", tab, (int)arrlenu(info->value_buffer));
    strputf(&s, "%s.count_notes = %i,\n", tab, (int)arrlenu(note_set));
    strputf(&s, "};\n");

    hmfree(complex_type_map);

    strputnull(s);
    return s;
}
