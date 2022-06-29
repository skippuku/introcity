#include "lib/intro.h"
#include "global.c"

#include "lib/intro.h.intro"

#ifndef VERSION
#define VERSION "unknown-version"
#endif

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

int
generate_c_header(const char * output_filename, ParseInfo * info) {
    char * s = NULL;

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
            char * saved_name = malloc(8);
            stbsp_snprintf(saved_name, 7, "%04x", type_index);

            hmput(complex_type_map, t->i_struct, saved_name);

            strputf(&s, "%s __intro_%s = {", (t->category == INTRO_ENUM)? "IntroEnum" : "IntroStruct", saved_name);
            if (t->category == INTRO_STRUCT || t->category == INTRO_UNION) {
                strputf(&s, "%u, %u, {\n", t->i_struct->count_members, t->i_struct->is_union);
                for (int m_index = 0; m_index < t->i_struct->count_members; m_index++) {
                    const IntroMember * m = &t->i_struct->members[m_index];
                    int32_t member_type_index = hmget(info->index_by_ptr_map, m->type);
                    strputf(&s, "{\"%s\", &__intro_types[%i], ", m->name, member_type_index);
                    strputf(&s, "%u, %u},\n", m->offset, m->attr);
                }
            } else if (t->category == INTRO_ENUM) {
                strputf(&s, "%u, %u, %u, {\n", t->i_enum->count_members, t->i_enum->is_flags, t->i_enum->is_sequential);
                for (int m_index = 0; m_index < t->i_enum->count_members; m_index++) {
                    const IntroEnumValue * v = &t->i_enum->members[m_index];
                    strputf(&s, "{\"%s\", %i},\n", v->name, v->value);
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
            strputf(&s, "&__intro_types[%i],\n", arg_type_index);
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
                strputf(&s, "\"%s\",\n", name);
            } else {
                strputf(&s, "0,\n");
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
        strputf(&s, "{");

        strputf(&s, "0x%02x, %u, ", t->category, t->flags);

        strputf(&s, "{");
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
        strputf(&s, "}, ");

        if (t->of) {
            int32_t of_index = hmget(info->index_by_ptr_map, t->of);
            strputf(&s, "&__intro_types[%i], ", of_index);
        } else {
            strputf(&s, "0, ");
        }

        if (t->parent) {
            int32_t parent_index = hmget(info->index_by_ptr_map, t->parent);
            strputf(&s, "&__intro_types[%i], ", parent_index);
        } else {
            strputf(&s, "0, ");
        }

        if (t->name) {
            strputf(&s, "\"%s\", ", t->name);
        } else {
            strputf(&s, "0, ");
        }

        strputf(&s, "%u, %u, %u", t->attr, t->size, t->align);

        if (t->location.path) {
            strputf(&s, ", {\"%s\", %u, %u}", t->location.path, t->location.line, t->location.column);
        } else {
            strputf(&s, ", {}");
        }

        strputf(&s, "},\n");
    }
    strputf(&s, "};\n\n");

    // attributes
    strputf(&s, "const IntroAttribute __intro_attr_t [] = {\n");
    for (int attr_index = 0; attr_index < info->attr.count_available; attr_index++) {
        IntroAttribute attr = info->attr.available[attr_index];
        strputf(&s, "{\"%s\", 0x%x, %u},\n", attr.name, attr.attr_type, attr.type_id);
    }
    strputf(&s, "};\n\n");

    strputf(&s, "const char * __intro_strings [%i] = {\n", (int)arrlen(info->string_set));
    for (int i=0; i < arrlen(info->string_set); i++) {
        strputf(&s, "\"%s\",\n", info->string_set[i]);
    }
    strputf(&s, "};\n\n");

    strputf(&s, "enum {\n");
    for (int i=0; i < info->attr.count_available; i++) {
        strputf(&s, "IATTR_%s = %i,\n", info->attr.available[i].name, i);
    }
    strputf(&s, "};\n\n");

    strputf(&s, "const unsigned char __intro_attr_data [] = {");
    for (int i=0; i < arrlen(info->attr.spec_buffer) * sizeof(info->attr.spec_buffer[0]); i++) {
        if (i % 16 == 0) {
            strputf(&s, "\n");
        }
        uint8_t byte = ((uint8_t *)info->attr.spec_buffer)[i];
        strputf(&s, "0x%02x,", byte);
    }
    strputf(&s, "\n};\n\n");

    // values
    strputf(&s, "unsigned char __intro_values [%i] = {", (int)arrlen(info->value_buffer));
    for (int i=0; i < arrlen(info->value_buffer); i++) {
        if (i % 16 == 0) {
            strputf(&s, "\n");
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
            strputf(&s, "ITYPE_%s = %i,\n", name, type_index);
            if (name != t->name) free((void *)name);
        }
    }
    strputf(&s, "};\n\n");

    // context
    
    strputf(&s, "IntroContext __intro_ctx = {\n");
    strputf(&s, "__intro_types,\n");
    strputf(&s, "__intro_strings,\n");
    strputf(&s, "__intro_values,\n");
    strputf(&s, "__intro_fns,\n");
    strputf(&s, "{(IntroAttribute *)__intro_attr_t, ");
    strputf(&s, "(IntroAttributeSpec *)__intro_attr_data, ");
    strputf(&s, "%u, ", info->attr.count_available);
    strputf(&s, "%u,{", info->attr.first_flag);
    for (int i=0; i < LENGTH(g_builtin_attributes); i++) {
        strputf(&s, "IATTR_%s,", g_builtin_attributes[i].key);
    }
    strputf(&s, "}},\n");
    strputf(&s, "%u,", info->count_types);
    strputf(&s, "%i,", (int)arrlenu(info->string_set));
    strputf(&s, "%i,", (int)arrlenu(info->value_buffer));
    strputf(&s, "%u,\n", info->count_functions);
    strputf(&s, "};\n");

    strputf(&s, "#endif\n");

    hmfree(complex_type_map);

    int error = intro_dump_file(output_filename, s, strlen(s));
    arrfree(s);

    if (error) return RET_FAILED_FILE_WRITE;
    else return 0;
}

int
generate_context_city(char * filename, ParseInfo * info) {
    IntroContext ser = {0};

    ser.types = malloc(info->count_types * sizeof(IntroType));
    ser.count_types = info->count_types;
    for (int type_i=0; type_i < info->count_types; type_i++) {
        IntroType * type = info->types[type_i];
        ser.types[type_i] = *type;
    }

    ser.strings = (const char **)info->string_set;
    ser.count_strings = arrlen(info->string_set);

    ser.values = info->value_buffer;
    ser.size_values = arrlen(info->value_buffer);

    ser.functions = info->functions;
    ser.count_functions = info->count_functions;

    ser.attr = info->attr;

    int ret = intro_create_city_file(filename, &ser, ITYPE(IntroContext));

    free(ser.types);

    return ret;
}

int
generate_vim_syntax(char * filename, ParseInfo * info) {
    char * buf = NULL;
    char ** s = &buf;

    strputf(s, "\" generated with intro version %s\n\n", VERSION);

    for (int type_i=LENGTH(known_types)-1; type_i < info->count_types; type_i++) {
        const IntroType * type = info->types[type_i];
        if (type->name && type->parent && !strchr(type->name, ' ')) {
            strputf(s, "syn keyword Type %s\n", type->name);
        }
        if (!type->parent && type->category == INTRO_ENUM && type->i_enum->count_members > 0) {
            strputf(s, "  syn keyword Constant");
            for (int ei=0; ei < type->i_enum->count_members; ei++) {
                IntroEnumValue value = type->i_enum->members[ei];
                strputf(s, " %s", value.name);
            }
            strputf(s, "\n");
        }
    }

    strputf(s, "\n");
    for (int func_i=0; func_i < info->count_functions; func_i++) {
        const IntroFunction * func = info->functions[func_i];
        strputf(s, "syn keyword Function %s\n", func->name);
    }

    strputf(s, "\nsyn keyword IntroAttribute");
    for (int attr_i=0; attr_i < info->attr.count_available; attr_i++) {
        IntroAttribute attr = info->attr.available[attr_i];
        strputf(s, " %s", attr.name);
    }
    strputf(s, "\n");

    intro_dump_file(filename, buf, arrlen(buf));

    return 0;
}
