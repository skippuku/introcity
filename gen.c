#include <stdlib.h>
#include <string.h>
#include "intro.h"
#include "util.c"

char *
generate_c_header(IntroInfo * info) {
    char * s = NULL;

    const char * tab = "    ";

    const char * version = "v0.0";
    strputf(&s, "/* Generated with intro %s */\n\n", version);

    struct {
        IntroStruct * key;
        char * value;
    } * complex_type_map = NULL;

    // forward declare type list
    strputf(&s, "extern IntroType __intro_types [%u];\n\n", info->count_types);

    // complex type information (enums, structs, unions)
    for (int type_index = 0; type_index < info->count_types; type_index++) {
        const IntroType * t = &info->types[type_index];
        if (t->category == INTRO_STRUCT && hmgeti(complex_type_map, t->i_struct) < 0) {
            char * saved_name;
            char * ref_name;
            if (!t->name) {
                for (int t2_index=15; t2_index < info->count_types; t2_index++) {
                    if (t2_index == type_index) continue;
                    const IntroType * t2 = &info->types[t2_index];
                    if (t2->category == t->category && t2->i_struct == t->i_struct) {
                        saved_name = t2->name;
                        ref_name = saved_name;
                        break;
                    }
                }
                if (!saved_name) {
                    fprintf(stderr, "no way to reference anonymous struct.\n");
                    return NULL;
                }
            } else if (strchr(t->name, ' ')) {
                size_t name_len = strlen(t->name);
                saved_name = malloc(strlen(t->name) + 1);
                for (int i=0; i < name_len; i++) {
                    saved_name[i] = (t->name[i] == ' ')? '_' : t->name[i];
                }
                saved_name[name_len] = '\0';
                ref_name = t->name;
            } else {
                saved_name = t->name;
                ref_name = t->name;
            }

            hmput(complex_type_map, t->i_struct, saved_name);

            strputf(&s, "IntroStruct __intro_%s = {%u, %u, {\n", saved_name, t->i_struct->count_members, t->i_struct->is_union);
            for (int m_index = 0; m_index < t->i_struct->count_members; m_index++) {
                const IntroMember * m = &t->i_struct->members[m_index];
                int32_t member_type_index = hmget(info->index_by_ptr_map, m->type);
                strputf(&s, "%s{\"%s\", &__intro_types[%i], offsetof(%s, %s), 0},\n", // TODO: attributes
                             tab, m->name, member_type_index, ref_name, m->name);
            }
            strputf(&s, "}};\n\n");
        }
    }

    // type list
    strputf(&s, "IntroType __intro_types [%u] = {\n", info->count_types);
    for (int type_index = 0; type_index < info->count_types; type_index++) {
        const IntroType * t = &info->types[type_index];
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
        if (t->category == INTRO_STRUCT
         || t->category == INTRO_UNION
         || t->category == INTRO_ENUM)
        {
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
