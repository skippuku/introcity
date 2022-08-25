#include "lib/intro.h"
#include "global.c"

#define INCLUDE_INTRO_DATA 0 // TODO: reenable after union selection is implemented

#if INCLUDE_INTRO_DATA
#include "lib/intro.h.intro"
#endif

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
generate_c_header(PreInfo * pre_info, ParseInfo * info) {
    char * s = NULL;

    char * header_def = NULL;
    arrsetcap(header_def, strlen(pre_info->output_filename) + 2);
    arrsetlen(header_def, 1);
    header_def[0] = '_';
    for (int i=0; i < strlen(pre_info->output_filename); i++) {
        char c = pre_info->output_filename[i];
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
        int value;
    } * complex_type_map = NULL;
    hmdefault(complex_type_map, -1);

    // forward declare type list
    strputf(&s, "extern IntroType __intro_t [%u];\n\n", info->count_types);

    char * mbr = NULL;
    char * ev = NULL;
    char * ct = NULL;

    // enums, structs, unions
    int struct_member_index = 0;
    int enum_value_index = 0;
    for (int type_index = 0; type_index < info->count_types; type_index++) {
        IntroType * t = info->types[type_index];
        if ((intro_is_complex(t) || t->category == INTRO_FUNCTION) && hmgeti(complex_type_map, t->__data) < 0) {

            switch(t->category) {
            case INTRO_STRUCT:
            case INTRO_UNION: {
                hmput(complex_type_map, t->__data, struct_member_index);
                for (int m_i=0; m_i < t->count; m_i++) {
                    IntroMember m = t->members[m_i];
                    int32_t member_type_index = hmget(info->index_by_ptr_map, m.type);
                    if (m.name) {
                        strputf(&mbr, "{\"%s\", ", m.name);
                    } else {
                        strputf(&mbr, "{0, ");
                    }
                    strputf(&mbr, "&__intro_t[%i], %u, %u},\n",
                                  member_type_index, m.offset, m.attr);
                }
                struct_member_index += t->count;
            }break;

            case INTRO_ENUM: {
                hmput(complex_type_map, t->__data, enum_value_index);
                for (int v_i=0; v_i < t->count; v_i++) {
                    IntroEnumValue v = t->values[v_i];
                    strputf(&ev, "{\"%s\", %i, %u},\n", v.name, v.value, v.attr);
                }
                enum_value_index += t->count;
            }break;

            case INTRO_FUNCTION: {
                hmput(complex_type_map, t->__data, type_index);
                strputf(&ct, "IntroType * __intro_%x [%u] = {", type_index, t->count);
                for (int arg_i=0; arg_i < arrlen(t->arg_types); arg_i++) {
                    IntroType * arg_type = t->arg_types[arg_i];
                    int arg_type_index = hmget(info->index_by_ptr_map, arg_type);
                    strputf(&ct, "&__intro_t[%i],", arg_type_index);
                }
                strputf(&ct, "};\n");
            }break;
            }
        }
    }

    arrput(mbr, 0);
    arrput(ev, 0);
    arrput(ct, 0);

    strputf(&s, "IntroMember __intro_mbr [] = {\n%s};\n\n", mbr);
    strputf(&s, "IntroEnumValue __intro_ev [] = {\n%s};\n\n", ev);
    strputf(&s, "%s", ct);

    arrfree(mbr);
    arrfree(ev);
    arrfree(ct);

    // function & macro arg/param names
    int name_index = 0;
    strputf(&s, "const char * __intro_argnm [] = {\n");
    for (int func_i=0; func_i < info->count_functions; func_i++) {
        IntroFunction * func = info->functions[func_i];
        for (int name_i=0; name_i < func->count_args; name_i++) {
            const char * name = func->arg_names[name_i];
            if (name) {
                strputf(&s, "\"%s\",", name);
            } else {
                strputf(&s, "0,");
            }
            name_index++;
            if ((name_index & 7) == 0) {
                strputf(&s, "\n");
            }
        }
    }
    for (int macro_i=0; macro_i < arrlen(pre_info->macros); macro_i++) {
        IntroMacro macro = pre_info->macros[macro_i];
        for (int name_i=0; name_i < macro.count_parameters; name_i++) {
            const char * name = macro.parameters[name_i];
            strputf(&s, "\"%s\",", name);

            name_index++;
            if ((name_index & 7) == 0) {
                strputf(&s, "\n");
            }
        }
    }
    strputf(&s, "};\n\n");

    // type list
    strputf(&s, "IntroType __intro_t [%u] = {\n", info->count_types);
    for (int type_index = 0; type_index < info->count_types; type_index++) {
        const IntroType * t = info->types[type_index];

        strputf(&s, "{{");
        int saved_index = hmget(complex_type_map, t->__data);
        if (saved_index >= 0) {
            switch(t->category) {
            case INTRO_STRUCT:
            case INTRO_UNION: {
                strputf(&s, "&__intro_mbr[%i]", saved_index);
            }break;

            case INTRO_ENUM: {
                strputf(&s, "&__intro_ev[%i]", saved_index);
            }break;

            case INTRO_FUNCTION: {
                strputf(&s, "&__intro_%x", saved_index);
            }break;

            default: {
                assert(0);
            }break;
            }
        } else if (t->of) {
            int type_index = hmget(info->index_by_ptr_map, t->of);
            strputf(&s, "&__intro_t[%i]", type_index);
        } else {
            strputf(&s, "0");
        }
        strputf(&s, "}, ");

        if (t->parent) {
            int32_t parent_index = hmget(info->index_by_ptr_map, t->parent);
            strputf(&s, "&__intro_t[%i], ", parent_index);
        } else {
            strputf(&s, "0, ");
        }

        if (t->name) {
            strputf(&s, "\"%s\", ", t->name);
        } else {
            strputf(&s, "0, ");
        }

        strputf(&s, "%u, %u, %u, 0x%02x, %u, 0x%02x},\n", t->count, t->attr, t->size, t->flags, t->align, t->category);
    }
    strputf(&s, "};\n\n");

    // function info
    name_index = 0;
    strputf(&s, "IntroFunction __intro_fn [%u] = {\n", info->count_functions);
    for (int func_i=0; func_i < info->count_functions; func_i++) {
        IntroFunction * func = info->functions[func_i];
        int type_index = hmget(info->index_by_ptr_map, func->type);
        int return_type_index = hmget(info->index_by_ptr_map, func->return_type);
        int saved_index = hmget(complex_type_map, func->type->__data);
        strputf(&s, "{\"%s\", &__intro_t[%i], &__intro_t[%i], &__intro_argnm[%i], &__intro_%x[1], {\"%s\", %u}, %u, %u},\n",
                    func->name, type_index, return_type_index, name_index, saved_index,
                    func->location.path, func->location.offset,
                    func->count_args, func->flags);
        name_index += func->count_args;
    }
    strputf(&s, "};\n\n");

    // attributes
    strputf(&s, "const IntroAttribute __intro_attr_t [] = {\n");
    for (int attr_index = 0; attr_index < info->attr.count_available; attr_index++) {
        IntroAttribute attr = info->attr.available[attr_index];
        strputf(&s, "{\"%s\", %u, %u, %u},\n", attr.name, attr.attr_type, attr.type_id, attr.propagated);
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

    // Macros
    strputf(&s, "IntroMacro __intro_macros [] = {\n");
    for (int macro_i=0; macro_i < arrlen(pre_info->macros); macro_i++) {
        IntroMacro macro = pre_info->macros[macro_i];
        strputf(&s, "{\"%s\", ", macro.name);
        if (macro.parameters) {
            strputf(&s, "&__intro_argnm[%i], ", name_index);
            name_index += macro.count_parameters;
        } else {
            strputf(&s, "0, ");
        }

        char * replace_string;
        bool do_free;
        if (macro.replace) {
            replace_string = create_escaped_string(macro.replace);
            do_free = true;
        } else {
            replace_string = "\"\"";
            do_free = false;
        }
        strputf(&s, "%s, {\"%s\", %u}, %u},\n", replace_string, macro.location.path, macro.location.offset, macro.count_parameters);
        if (do_free) arrfree(replace_string);
    }
    strputf(&s, "};\n\n");

    // context
    strputf(&s, "IntroContext __intro_ctx = {\n");
    strputf(&s, "__intro_t,\n");
    strputf(&s, "__intro_values,\n");
    strputf(&s, "__intro_fn,\n");
    strputf(&s, "__intro_macros,\n");

    strputf(&s, "%u,", info->count_types);
    strputf(&s, "%i,", (int)arrlenu(info->value_buffer));
    strputf(&s, "%u,", info->count_functions);
    strputf(&s, "%u,", (uint32_t)arrlen(pre_info->macros));
    strputf(&s, "\n");

    strputf(&s, "{(IntroAttribute *)__intro_attr_t, ");
    strputf(&s, "(IntroAttributeSpec *)__intro_attr_data, ");
    strputf(&s, "%u, ", info->attr.count_available);
    strputf(&s, "%u,{", info->attr.first_flag);
    for (int i=0; i < LENGTH(g_builtin_attributes); i++) {
        strputf(&s, "IATTR_%s,", g_builtin_attributes[i].key);
    }
    strputf(&s, "}},\n");

    strputf(&s, "};\n");
    strputf(&s, "#endif\n");

    hmfree(complex_type_map);

    int error = intro_dump_file(pre_info->output_filename, s, strlen(s));
    arrfree(s);

    if (error) return RET_FAILED_FILE_WRITE;
    else return 0;
}

int
generate_context_city(PreInfo * pre_info, ParseInfo * info) {
#if INCLUDE_INTRO_DATA
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

    ser.macros = pre_info->macros;
    ser.count_macros = arrlen(pre_info->macros);

    int ret = intro_create_city_file(pre_info->output_filename, &ser, ITYPE(IntroContext));

    free(ser.types);

    return ret;
#else
    return -1;
#endif
}

int
generate_vim_syntax(PreInfo * pre_info, ParseInfo * info) {
    char * buf = NULL;
    char ** s = &buf;

    strputf(s, "\" generated with intro %s\n\n", VERSION);

    for (int type_i=LENGTH(known_types)-1; type_i < info->count_types; type_i++) {
        const IntroType * type = info->types[type_i];
        if (type->name && type->parent && !strchr(type->name, ' ')) {
            strputf(s, "syn keyword Type %s\n", type->name);
        }
        if (!type->parent && type->category == INTRO_ENUM && type->count > 0) {
            strputf(s, "syn keyword Constant");
            for (int ei=0; ei < type->count; ei++) {
                IntroEnumValue value = type->values[ei];
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

    strputf(s, "\n\n");
    for (int macro_i=0; macro_i < arrlen(pre_info->macros); macro_i++) {
        IntroMacro macro = pre_info->macros[macro_i];
        strputf(s, "syn keyword Macro %s\n", macro.name);
    }

    intro_dump_file(pre_info->output_filename, buf, arrlen(buf));

    return 0;
}
