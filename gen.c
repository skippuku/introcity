#include "lib/intro.h"
#include "global.c"

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

static char *
get_container_member_name(IntroInfo * info, NestInfo * nest, const char ** o_top_level_name) {
    NestInfo * container = hmgetp_null(info->nest_map, nest->container_type);
    if (container) {
        char * result = get_container_member_name(info, container, o_top_level_name);
        strputf(&result, ".%s", nest->container_type->i_struct->members[nest->member_index_in_container].name);
        return result;
    } else {
        char * result = NULL;
        strputf(&result, "%s", nest->container_type->i_struct->members[nest->member_index_in_container].name);

        for (int i=0; i < nest->indirection_level; i++) {
            strputf(&result, "[0]");
        }

        *o_top_level_name = get_ref_name(info, nest->container_type);
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

char *
generate_c_header(IntroInfo * info, const char * output_filename) {
    // generate info needed to get offset and sizeof for anonymous types
    for (int i=0; i < hmlen(info->nest_map); i++) {
        NestInfo * nest = &info->nest_map[i];
        nest->member_name_in_container = get_container_member_name(info, nest, &nest->top_level_name);
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
    arrput(header_def, 0);

    strputf(&s, "/* Generated with intro %s */\n\n", VERSION);
    strputf(&s, "#ifndef %s\n"
                "#define %s\n\n",
                header_def, header_def);

    struct {
        void * key;
        char * value;
    } * complex_type_map = NULL;

    // forward declare type list
    strputf(&s, "extern IntroType __intro_types [%u];\n\n", info->count_types);

    // complex type information (enums, structs, unions)
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
                strputf(&s, "sizeof(((%s*)0)->%s)", nest->top_level_name, nest->member_name_in_container);
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
                                nest->top_level_name, nest->member_name_in_container, m->name,
                                nest->top_level_name, nest->member_name_in_container);
                    }
                    strputf(&s, ", %u},\n", m->attr);
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
                strputf(&s, "&__intro_%s", saved_name);
            } else {
                strputf(&s, "0");
            }
        } else {
            if (t->category == INTRO_ARRAY) {
                strputf(&s, "(void *)0x%x", t->array_size);
            } else {
                strputf(&s, "0");
            }
        }
        if (t->location.path) {
            strputf(&s, ", {\"%s\", %u, %u}, ", t->location.path, t->location.line, t->location.column);
        } else {
            strputf(&s, ", {}, ");
        }

        strputf(&s, "%u", t->attr);
        strputf(&s, "},\n");
    }
    strputf(&s, "};\n\n");

    // attributes
    strputf(&s, "const IntroAttribute __intro_attr_t [] = {\n");
    for (int attr_index = 0; attr_index < info->attr.count_available; attr_index++) {
        IntroAttribute attr = info->attr.available[attr_index];
        strputf(&s, "%s{\"%s\", 0x%x, %u},\n", tab, attr.name, attr.attr_type, attr.type_id);
    }
    strputf(&s, "};\n\n");

    strputf(&s, "const char * __intro_notes [%i] = {\n", (int)arrlen(info->string_set));
    for (int i=0; i < arrlen(info->string_set); i++) {
        strputf(&s, "%s\"%s\",\n", tab, info->string_set[i]);
    }
    strputf(&s, "};\n\n");

    strputf(&s, "enum {\n");
    for (int i=0; i < info->attr.count_available; i++) {
        strputf(&s, "%sIATTR_%s = %i,\n", tab, info->attr.available[i].name, i);
    }
    strputf(&s, "};\n\n");

    strputf(&s, "const unsigned char __intro_attr_data [] = {");
    for (int i=0; i < arrlen(info->attr.spec_buffer) * sizeof(info->attr.spec_buffer[0]); i++) {
        if (i % 16 == 0) {
            strputf(&s, "\n%s", tab);
        }
        uint8_t byte = ((uint8_t *)info->attr.spec_buffer)[i];
        strputf(&s, "0x%02x,", byte);
    }
    strputf(&s, "\n};\n\n");

    // values
    strputf(&s, "unsigned char __intro_values [%i] = {", (int)arrlen(info->value_buffer));
    for (int i=0; i < arrlen(info->value_buffer); i++) {
        if (i % 16 == 0) {
            strputf(&s, "\n%s", tab);
        }
        uint8_t byte = info->value_buffer[i];
        strputf(&s, "0x%02x,", byte);
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
    strputf(&s, "%s{(IntroAttribute *)__intro_attr_t, ", tab);
    strputf(&s, "%s(IntroAttributeSpec *)__intro_attr_data, ", tab);
    strputf(&s, "%s%u, ", tab, info->attr.count_available);
    strputf(&s, "%s%u,", tab, info->attr.first_flag);
    for (int i=0; i < LENGTH(g_builtin_attributes); i++) {
        strputf(&s, "IATTR_%s,", g_builtin_attributes[i].key);
    }
    strputf(&s, "},\n");
    strputf(&s, "%s%u,", tab, info->count_types);
    strputf(&s, "%s%i,", tab, (int)arrlenu(info->string_set));
    strputf(&s, "%s%i,", tab, (int)arrlenu(info->value_buffer));
    strputf(&s, "%s%u,\n", tab, info->count_functions);
    strputf(&s, "};\n");

    strputf(&s, "#endif\n");

    hmfree(complex_type_map);

    return s;
}
