#include "lib/intro.h"
#include "global.h"

static const char *
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

// TODO: rename parent to container (grand_parent -> meta_container) since parent already means something to types
// TODO: just pass the nest
static char *
get_parent_member_name(IntroInfo * info, IntroType * parent, int parent_index, int indirection_level, const char ** o_top_level_name) {
    NestInfo * nest = hmgetp_null(info->nest_map, parent);
    if (nest) {
        IntroType * grand_parent = nest->parent;
        int grand_parent_index = nest->member_index;

        char * result = get_parent_member_name(info, grand_parent, grand_parent_index, nest->indirection_level, o_top_level_name);
        strputf(&result, ".%s", parent->i_struct->members[parent_index].name);

        return result;
    } else {
        char * result = NULL;
        strputf(&result, "%s", parent->i_struct->members[parent_index].name);

        for (int i=0; i < indirection_level; i++) {
            strputf(&result, "[0]");
        }

        const char * top_level_name = get_ref_name(info, parent);
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

static const char *
category_str(int category) {
    switch(category) {
    default:
    case INTRO_UNKNOWN: return "INTRO_UNKNOWN";

    case INTRO_U8: return "INTRO_U8";
    case INTRO_U16: return "INTRO_U16";
    case INTRO_U32: return "INTRO_U32";
    case INTRO_U64: return "INTRO_U64";

    case INTRO_S8: return "INTRO_S8";
    case INTRO_S16: return "INTRO_S16";
    case INTRO_S32: return "INTRO_S32";
    case INTRO_S64: return "INTRO_S64";

    case INTRO_F32: return "INTRO_F32";
    case INTRO_F64: return "INTRO_F64";
    case INTRO_F128: return "INTRO_F128";

    case INTRO_ARRAY: return "INTRO_ARRAY";
    case INTRO_POINTER: return "INTRO_POINTER";

    case INTRO_ENUM: return "INTRO_ENUM";
    case INTRO_STRUCT: return "INTRO_STRUCT";
    case INTRO_UNION: return "INTRO_UNION";
    case INTRO_FUNCTION: return "INTRO_FUNCTION";
    }
}

static const char *
attr_value_str(int attr_value) {
    switch(attr_value) {
    default:
    case INTRO_V_FLAG: return "INTRO_V_FLAG";
    case INTRO_V_INT: return "INTRO_V_INT";
    case INTRO_V_FLOAT: return "INTRO_V_FLOAT";
    case INTRO_V_VALUE: return "INTRO_V_VALUE";
    case INTRO_V_MEMBER: return "INTRO_V_MEMBER";
    case INTRO_V_STRING: return "INTRO_V_STRING";
    }
}

char *
generate_c_header(IntroInfo * info, const char * output_filename) {
    // generate info needed to get offset and sizeof for anonymous types
    for (int i=0; i < hmlen(info->nest_map); i++) {
        NestInfo * nest = &info->nest_map[i];
        nest->parent_member_name = get_parent_member_name(info, nest->parent, nest->member_index, nest->indirection_level, &nest->top_level_name);
    }

    char * s = NULL;

    const char * tab = "";

    char * header_def = NULL;
    arrsetcap(header_def, strlen(output_filename) + 2);
    arrsetlen(header_def, 1);
    header_def[0] = '_';
    for (int i=0; i < strlen(output_filename); i++) {
        char c = output_filename[i];
        if (is_iden(c)) {
            arrput(header_def, c);
        } else {
            arrput(header_def, '_');
        }
    }
    strputnull(header_def);

    strputf(&s, "/* Generated with intro %s */\n\n", VERSION);
    strputf(&s, "#ifndef %s\n"
                "#define %s\n",
                header_def, header_def);
    strputf(&s, "#include <stddef.h>\n\n");

    struct {
        void * key;
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
                        strputf(&s, "%s{%i, %s, %i},\n", tab, attr->type, attr_value_str(attr->value_type), attr->v.i);
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
            const char * ref_name = NULL;
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
                    strputf(&s, "%s{\"%s\", &__intro_types[%i], %u, ", tab, m->name, member_type_index, (unsigned int)m->bitfield);
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

    // function argument types
    for (int arg_list_i=0; arg_list_i < info->count_arg_lists; arg_list_i++) {
        IntroTypePtrList * args = info->arg_lists[arg_list_i];

        char * saved_name = malloc(16);
        stbsp_snprintf(saved_name, 15, "arg_%04x", arg_list_i);

        hmput(complex_type_map, args, saved_name);

        strputf(&s, "IntroTypePtrList __intro_%s = {%u, {\n", saved_name, args->count);
        for (int arg_i=0; arg_i < args->count; arg_i++) {
            IntroType * arg_type = args->types[arg_i];
            int arg_type_index = hmget(info->index_by_ptr_map, arg_type);
            strputf(&s, "%s&__intro_types[%i],\n", tab, arg_type_index);
        }
        strputf(&s, "}};\n\n");
    }

    // function info
    for (int func_i=0; func_i < info->count_functions; func_i++) {
        IntroFunction * func = info->functions[func_i];
        int return_type_index = hmget(info->index_by_ptr_map, func->type);
        strputf(&s, "IntroFunction __intro_fn_%04x = {\"%s\", &__intro_types[%i], {\"%s\", %u, %u}, %i, %u, {\n",
                func_i, func->name, return_type_index,
                func->location.path, func->location.line, func->location.column,
                func->flags, func->has_body);
        for (int name_i=0; name_i < func->type->args->count; name_i++) {
            const char * name = func->arg_names[name_i];
            if (name) {
                strputf(&s, "%s\"%s\",\n", tab, name);
            } else {
                strputf(&s, "%s0,\n", tab);
            }
        }
        strputf(&s, "}};\n\n");
    }

    strputf(&s, "IntroFunction * __intro_fns [%u] = {\n", info->count_functions);
    for (int func_i=0; func_i < info->count_functions; func_i++) {
        strputf(&s, "&__intro_fn_%04x,\n", func_i);
    }
    strputf(&s, "};\n\n");

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
        strputf(&s, "%s, %u, ", category_str(t->category), t->flags);
        if (intro_is_complex(t) || t->category == INTRO_FUNCTION) {
            char * saved_name = hmget(complex_type_map, t->i_struct);
            if (saved_name) {
                const char * union_member = "ERROR";
                switch(t->category) {
                case INTRO_STRUCT:
                case INTRO_UNION:    union_member = "i_struct"; break;
                case INTRO_ENUM:     union_member = "i_enum"; break;
                case INTRO_FUNCTION: union_member = "args"; break;
                default: break; // never reached
                }
                strputf(&s, ".%s=&__intro_%s", union_member, saved_name);
            } else {
                strputf(&s, "0");
            }
        } else {
            if (t->category == INTRO_ARRAY) {
                strputf(&s, "%u", t->array_size);
            } else {
                strputf(&s, "0");
            }
        }
        if (t->location.path) {
            strputf(&s, ", {\"%s\", %u, %u}},\n", t->location.path, t->location.line, t->location.column);
        } else {
            strputf(&s, "},\n");
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
        const char * name;
        if (t->name) {
            if (strchr(t->name, ' ')) {
                name = make_identifier_safe_name(t->name);
            } else {
                name = t->name;
            }
            strputf(&s, "%sITYPE_%s = %i,\n", tab, name, type_index);
            if (name != t->name) free((void *)name);
        }
    }
    strputf(&s, "};\n\n");

    // context
    strputf(&s, "IntroContext __intro_ctx = {\n");
    strputf(&s, "%s__intro_types,\n", tab);
    strputf(&s, "%s__intro_notes,\n", tab);
    strputf(&s, "%s__intro_values,\n", tab);
    strputf(&s, "%s__intro_fns,\n", tab);
    strputf(&s, "%s%u,\n", tab, info->count_types);
    strputf(&s, "%s%i,\n", tab, (int)arrlenu(note_set));
    strputf(&s, "%s%i,\n", tab, (int)arrlenu(info->value_buffer));
    strputf(&s, "%s%u,\n", tab, info->count_functions);
    strputf(&s, "};\n");

    strputf(&s, "#endif\n");
    strputnull(s);

    hmfree(complex_type_map);

    return s;
}
