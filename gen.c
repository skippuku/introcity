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

char *
generate_c_header(ParseInfo * info, const char * output_filename) {
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
            char * saved_name = malloc(8);
            stbsp_snprintf(saved_name, 7, "%04x", type_index);

            hmput(complex_type_map, t->i_struct, saved_name);

            strputf(&s, "%s __intro_%s = {", (t->category == INTRO_ENUM)? "IntroEnum" : "IntroStruct", saved_name);
            if (t->category == INTRO_STRUCT || t->category == INTRO_UNION) {
                strputf(&s, "%u, %u, {\n", t->i_struct->count_members, t->i_struct->is_union);
                for (int m_index = 0; m_index < t->i_struct->count_members; m_index++) {
                    const IntroMember * m = &t->i_struct->members[m_index];
                    int32_t member_type_index = hmget(info->index_by_ptr_map, m->type);
                    strputf(&s, "%s{\"%s\", &__intro_types[%i], ", tab, m->name, member_type_index);
                    strputf(&s, "%u, %u},\n", m->offset, m->attr);
                }
            } else if (t->category == INTRO_ENUM) {
                strputf(&s, "%u, %u, %u, {\n", t->i_enum->count_members, t->i_enum->is_flags, t->i_enum->is_sequential);
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
        strputf(&s, "%s{\"%s\", 0x%x, %u},\n", tab, attr.name, attr.attr_type, attr.type_id);
    }
    strputf(&s, "};\n\n");

    strputf(&s, "const char * __intro_strings [%i] = {\n", (int)arrlen(info->string_set));
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
    strputf(&s, "%s__intro_strings,\n", tab);
    strputf(&s, "%s__intro_values,\n", tab);
    strputf(&s, "%s__intro_fns,\n", tab);
    strputf(&s, "%s{(IntroAttribute *)__intro_attr_t, ", tab);
    strputf(&s, "%s(IntroAttributeSpec *)__intro_attr_data, ", tab);
    strputf(&s, "%s%u, ", tab, info->attr.count_available);
    strputf(&s, "%s%u,{", tab, info->attr.first_flag);
    for (int i=0; i < LENGTH(g_builtin_attributes); i++) {
        strputf(&s, "IATTR_%s,", g_builtin_attributes[i].key);
    }
    strputf(&s, "}},\n");
    strputf(&s, "%s%u,", tab, info->count_types);
    strputf(&s, "%s%i,", tab, (int)arrlenu(info->string_set));
    strputf(&s, "%s%i,", tab, (int)arrlenu(info->value_buffer));
    strputf(&s, "%s%u,\n", tab, info->count_functions);
    strputf(&s, "};\n");

    strputf(&s, "#endif\n");

    hmfree(complex_type_map);

    return s;
}

int
generate_context_city(char * filename, ParseInfo * info) {
    IntroContext * saved = calloc(1, sizeof(IntroContext));

    saved->types = malloc(info->count_types * sizeof(IntroType));
    saved->count_types = info->count_types;
    for (int type_i=0; type_i < info->count_types; type_i++) {
        IntroType * type = info->types[type_i];
        saved->types[type_i] = *type;
    }

    saved->strings = (const char **)info->string_set;
    saved->count_strings = arrlen(info->string_set);

    saved->values = info->value_buffer;
    saved->size_values = arrlen(info->value_buffer);

    saved->functions = info->functions;
    saved->count_functions = info->count_functions;

    saved->attr = info->attr;

    int ret = intro_create_city_file(filename, saved, ITYPE(IntroContext));

    return ret;
}
