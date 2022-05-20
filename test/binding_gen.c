#ifndef __INTRO__
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
#endif

#include "../lib/intro.h"
#include "../lib/intro.h.intro"

static const char * tab = "  ";

void
do_indent(FILE * out, int count) {
    for (int i=0; i < count; i++) fprintf(out, tab);
}

void
fprint_odin_type(FILE * out, const IntroType * type, int depth) {
    while (1) {
        if (type->category == INTRO_POINTER) {
            if (type->parent == ITYPE(void)) {
                fprintf(out, "rawptr");
                return;
            } else if (type->parent == ITYPE(char)) {
                fprintf(out, "cstring");
                return;
            } else {
                fprintf(out, "^");
            }
        } else if (type->category == INTRO_ARRAY) {
            // I don't know how you are supposed to deal with zero-length arrays in Odin...
            fprintf(out, "[%u]", type->array_size);
        } else {
            const char * name = NULL;
            bool raw_union = false;
            switch(type->category) {
            case INTRO_U8: {
                if (type == ITYPE(_Bool)) {
                    name = "bool";
                } else {
                    name = "u8";
                }
            }break;
            case INTRO_U16: name = "u16"; break;
            case INTRO_U32: name = "u32"; break;
            case INTRO_U64: name = "u64"; break;
            case INTRO_S8:  name = "i8";  break;
            case INTRO_S16: name = "i16"; break;
            case INTRO_S32: name = "i32"; break;
            case INTRO_S64: name = "i64"; break;
            case INTRO_F32: name = "f32"; break;
            case INTRO_F64: name = "f64"; break;
            case INTRO_UNION:
                raw_union = true;
                // fallthrough
            case INTRO_STRUCT: {
                if (depth == 0 || !type->name) {
                    fprintf(out, "struct %s {\n", (raw_union)? "#raw_union" : "");
                    for (int mi=0; mi < type->i_struct->count_members; mi++) {
                        IntroMember * m = &type->i_struct->members[mi];
                        do_indent(out, depth + 1);
                        fprintf(out, "%s: ", m->name);
                        fprint_odin_type(out, m->type, depth + 1);
                        fprintf(out, ",\n");
                    }
                    do_indent(out, depth);
                    fprintf(out, "}");
                    return;
                } else {
                    if (0==memcmp(type->name, "Intro", 5)) {
                        name = &type->name[5];
                    } else {
                        name = type->name;
                    }
                }
            }break;
            case INTRO_ENUM: {
                if (depth == 0) {
                    // try to detect namespace
                    int len_prefix = 1;
                    if (type->i_enum->count_members > 1) {
                        while (1) {
                            bool break_outer = false;
                            const char * first = type->i_enum->members[0].name;
                            for (int i=1; i < type->i_enum->count_members; i++) {
                                IntroEnumValue v = type->i_enum->members[i];
                                if (0 != memcmp(first, v.name, len_prefix)) {
                                    len_prefix -= 1;
                                    break_outer = true;
                                    break;
                                }
                            }
                            if (break_outer) break;
                            len_prefix += 1;
                        }
                    }
                    fprintf(out, "enum {\n", type->name);
                    for (int i=0; i < type->i_enum->count_members; i++) {
                        IntroEnumValue v = type->i_enum->members[i];
                        do_indent(out, depth + 1);
                        const char * ename;
                        fprintf(out, "%s = %i,\n", &v.name[len_prefix], (int)v.value);
                    }
                    do_indent(out, depth);
                    fprintf(out, "}");
                    return;
                }
            }break;
            default: break;
            }
            if (!name) name = type->name;
            fprintf(out, "%s", name);
            return;
        }
        type = type->parent;
        depth = 1;
    }
}

int
main() {
    int count_functions = __intro_ctx.count_functions;
    IntroFunction ** functions = __intro_ctx.functions;

    FILE * out = fopen("intro.odin", "wb");

    for (int type_i=0; type_i < __intro_ctx.count_types; type_i++) {
        const IntroType * type = &__intro_types[type_i];
        if (type->name && 0==memcmp(type->name, "Intro", 5)) {
            fprintf(out, "%s :: ", &type->name[5]);
            fprint_odin_type(out, type, 0);
            fprintf(out, "\n\n");
        }
    }

    fprintf(out, "@(link_prefix=\"intro_\")\n");
    fprintf(out, "foreign intro {\n");
    for (int func_i=0; func_i < count_functions; func_i++) {
        const IntroFunction * func = functions[func_i];

        if (0 != memcmp(func->name, "intro_", 6)) continue;

        fprintf(out, "%s%s :: proc(", tab, &func->name[6]);
        for (int arg_i=0; arg_i < func->type->args->count; arg_i++) {
            if (arg_i > 0) fprintf(out, ", ");
            const char * name = func->arg_names[arg_i];
            const IntroType * type = func->type->args->types[arg_i];
            fprintf(out, "%s: ", name);
            fprint_odin_type(out, type, 1);
        }
        if (func->type->parent && func->type->parent->category != INTRO_UNKNOWN) {
            fprintf(out, ") -> ");
            fprint_odin_type(out, func->type->parent, 1);
            fprintf(out, " ---\n");
        } else {
            fprintf(out, ") ---\n");
        }
    }
    fprintf(out, "}\n");

    fclose(out);

    return 0;
}
