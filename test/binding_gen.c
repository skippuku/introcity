#ifndef __INTRO__
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
#endif

#include "../lib/intro.h"
#include "intro.h.intro"

typedef struct {
    const char * tab;
    const char * lib_name;
    const char * func_prefix;
    const char * type_prefix;
} OdinGenOptions;

void
do_indent(FILE * out, const char * tab, int count) {
    for (int i=0; i < count; i++) fputs(tab, out);
}

enum {
    HAS_C_PREFIX = 0x01,
    HAS_LIB_PREFIX = 0x02,
};

const char *
get_odin_name(const char * type_name, const char * prefix, int * o_flags) {
    int flags = 0;

    if (0==strncmp(type_name, "struct ", strlen("struct "))) {
        type_name = type_name + strlen("struct ");
        flags |= HAS_C_PREFIX;
    } else if (0==strncmp(type_name, "union ", strlen("union "))) {
        type_name = type_name + strlen("union ");
        flags |= HAS_C_PREFIX;
    } else if (0==strncmp(type_name, "enum ", strlen("enum "))) {
        type_name = type_name + strlen("enum ");
        flags |= HAS_C_PREFIX;
    }

    if (0==strncmp(type_name, prefix, strlen(prefix))) {
        type_name += strlen(prefix);
        flags |= HAS_LIB_PREFIX;
    }

    if (o_flags) *o_flags = flags;

    return type_name;
}

void
fprint_odin_type(FILE * out, const OdinGenOptions * opt, IntroContainer cntr, int depth) {
    const IntroType * type = cntr.type;
    uint32_t attr = intro_get_attr(cntr);

    if (type->category == INTRO_POINTER) {
        if (type->of == ITYPE(void)) {
            fprintf(out, "rawptr");
            return;
        } else if (type->of == ITYPE(char) && intro_has_attribute_x(INTRO_CTX, attr, IATTR_cstring)) {
            fprintf(out, "cstring");
            return;
        } else {
            if (intro_has_attribute_x(INTRO_CTX, attr, IATTR_length)) {
                fprintf(out, "[^]");
            } else {
                fprintf(out, "^");
            }
        }
        fprint_odin_type(out, opt, intro_push(&cntr, 0), depth);
    } else if (type->category == INTRO_ARRAY) {
        fprintf(out, "[%u]", type->count);
        fprint_odin_type(out, opt, intro_push(&cntr, 0), depth);
    } else {
        const char * name = NULL;
        bool raw_union = false;
        switch(type->category) {
        case INTRO_U8: {
            if (type == ITYPE(bool)) {
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
                fprintf(out, "struct %s{\n", (raw_union)? "#raw_union " : "");
                for (int mi=0; mi < type->count; mi++) {
                    IntroMember * m = &type->members[mi];
                    do_indent(out, opt->tab, depth + 1);
                    if (m->name) {
                        fprintf(out, "%s: ", m->name);
                    } else {
                        fprintf(out, "using _u%i: ", mi);
                    }
                    fprint_odin_type(out, opt, intro_push(&cntr, mi), depth + 1);
                    fprintf(out, ",\n");
                }
                do_indent(out, opt->tab, depth);
                fprintf(out, "}");
                return;
            } else {
                int flags;
                name = get_odin_name(type->name, opt->type_prefix, &flags);
            }
        }break;

        case INTRO_ENUM: {
            if (depth == 0) {
                // try to detect namespace
                int len_prefix = 1;
                if (type->count > 1) {
                    while (1) {
                        bool break_outer = false;
                        const char * first = type->values[0].name;
                        for (int i=1; i < type->count; i++) {
                            IntroEnumValue v = type->values[i];
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
                fprintf(out, "enum i32 {\n");
                for (int i=0; i < type->count; i++) {
                    IntroEnumValue v = type->values[i];
                    do_indent(out, opt->tab, depth + 1);
                    const char * ename;
                    fprintf(out, "%s = %i,\n", &v.name[len_prefix], (int)v.value);
                }
                do_indent(out, opt->tab, depth);
                fprintf(out, "}");
                return;
            }
        }break;

        default: break;
        }

        if (!name) name = type->name;
        fprintf(out, "%s", name);
    }
}

int
main() {
    OdinGenOptions _opt = {
        .type_prefix = "Intro",
        .func_prefix = "intro_",
        .tab         = "\t",
        .lib_name    = "intro",
    }, * opt = &_opt;

    assert(opt->lib_name);
    if (!opt->type_prefix) opt->type_prefix = "";
    if (!opt->func_prefix) opt->func_prefix = "";
    if (!opt->tab)         opt->tab         = "\t";

    FILE * out = fopen("intro.odin", "wb");

    fprintf(out, "package %s\n\n", opt->lib_name);

    int count_functions = INTRO_CTX->count_functions;
    IntroFunction * functions = INTRO_CTX->functions;

    for (int type_i=0; type_i < __intro_ctx.count_types; type_i++) {
        const IntroType * type = &INTRO_CTX->types[type_i];
        if (!type->name) continue;

        const int mask = HAS_LIB_PREFIX;

        int flags;
        const char * type_name = get_odin_name(type->name, opt->type_prefix, &flags);

        if ((flags & HAS_LIB_PREFIX) && !(flags & HAS_C_PREFIX)) {
            fprintf(out, "%s :: ", type_name);
            fprint_odin_type(out, opt, intro_cntr(NULL, type), 0);
            fprintf(out, "\n\n");
        }
    }

    fprintf(out, "@(link_prefix=\"%s\")\n", opt->func_prefix);
    fprintf(out, "foreign intro {\n");
    for (int func_i=0; func_i < count_functions; func_i++) {
        IntroFunction func = functions[func_i];

        int flags;
        const char * func_name = get_odin_name(func.name, opt->func_prefix, &flags);
        if (!(flags & HAS_LIB_PREFIX)) continue;

        fprintf(out, "%s%s :: proc(", opt->tab, func_name);
        for (int arg_i=0; arg_i < func.count_args; arg_i++) {
            if (arg_i > 0) fprintf(out, ", ");
            const char * name = func.arg_names[arg_i];
            const IntroType * type = func.arg_types[arg_i];
            fprintf(out, "%s: ", name);
            fprint_odin_type(out, opt, intro_cntr(NULL, type), 1);
        }
        if (func.return_type && func.return_type->category != INTRO_UNKNOWN) {
            fprintf(out, ") -> ");
            fprint_odin_type(out, opt, intro_cntr(NULL, func.return_type), 1);
            fprintf(out, " ---\n");
        } else {
            fprintf(out, ") ---\n");
        }
    }
    fprintf(out, "}\n");

    fclose(out);

    return 0;
}
