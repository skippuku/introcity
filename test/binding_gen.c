#ifndef __INTRO__
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
#endif

#include "../lib/intro.h"
#include "../lib/intro.h.intro"

void
fprint_odin_type(FILE * out, const IntroType * type) {
    while (1) {
        if (type->category == INTRO_POINTER) {
            fprintf(out, "^");
        } else if (type->category == INTRO_ARRAY) {
            fprintf(out, "[%u]", type->array_size);
        } else {
            const char * name = NULL;
            switch(type->category) {
            case INTRO_U8:  name = "u8";  break;
            case INTRO_U16: name = "u16"; break;
            case INTRO_U32: name = "u32"; break;
            case INTRO_U64: name = "u64"; break;
            case INTRO_S8:  name = "i8";  break;
            case INTRO_S16: name = "i16"; break;
            case INTRO_S32: name = "i32"; break;
            case INTRO_S64: name = "i64"; break;
            case INTRO_F32: name = "f32"; break;
            case INTRO_F64: name = "f64"; break;
            default: break;
            }
            fprintf(out, "%s", (name)? name : type->name);
            break;
        }
        type = type->parent;
    }
}

int
main() {
    int count_functions = __intro_ctx.count_functions;
    IntroFunction ** functions = __intro_ctx.functions;

    FILE * out = fopen("intro.odin", "wb");

    const char * tab = "  ";

    for (int type_i=0; type_i < __intro_ctx.count_types; type_i++) {
        const IntroType * type = &__intro_ctx.types[type_i];
        if (type->category == INTRO_STRUCT && type->name && 0==memcmp(type->name, "Intro", 5)) {
            fprintf(out, "%s :: struct {\n", type->name);
            for (int mi=0; mi < type->i_struct->count_members; mi++) {
                IntroMember * m = &type->i_struct->members[mi];
                fprintf(out, "%s%s: ", tab, m->name);
                fprint_odin_type(out, m->type);
                fprintf(out, ",\n");
            }
            fprintf(out, "}\n\n");
        }
    }

    fprintf(out, "@(link_prefix=\"intro_\")");
    fprintf(out, "foreign intro {\n");
    for (int func_i=0; func_i < count_functions; func_i++) {
        const IntroFunction * func = functions[func_i];

        if (0 != memcmp(func->name, "intro_", 6)) continue;

        fprintf(out, "%s%s :: proc(", tab, &func->name[5]);
        for (int arg_i=0; arg_i < func->type->args->count; arg_i++) {
            if (arg_i > 0) fprintf(out, ", ");
            const char * name = func->arg_names[arg_i];
            const IntroType * type = func->type->args->types[arg_i];
            fprintf(out, "%s: ", name);
            fprint_odin_type(out, type);
        }
        if (func->type->parent && func->type->parent->category != INTRO_UNKNOWN) {
            fprintf(out, ") -> ");
            fprint_odin_type(out, func->type->parent);
            fprintf(out, " ---\n");
        } else {
            fprintf(out, ") ---\n");
        }
    }
    fprintf(out, "}\n");

    fclose(out);

    return 0;
}
