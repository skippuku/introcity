#include "lib/intro.h"
#include "global.c"
#include "config.c"
#include "expr.c"
#include "pre.c"
#include "parse.c"
#include "gen.c"

static void
enable_windows_console_color() {
#ifdef _WIN32
    HANDLE con = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(con, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(con, mode);
#endif
}

int
main(int argc, char * argv []) {
    enable_windows_console_color();

    if (argc > 1 && 0==strcmp(argv[1], "--gen-config")) {
        generate_config(argc - 2, &argv[2]);
        return 0;
    }

    metrics.start = nanotime();
    metrics.last = metrics.start;

    PreInfo pre_info = run_preprocessor(argc, argv);
    if (!pre_info.result_list || pre_info.ret != 0) {
        fprintf(stderr, "preprocessor failed.\n");
        return 1;
    }

    ParseInfo parse_info = {0};
    int error = parse_preprocessed_tokens(&pre_info, &parse_info);
    if (error) {
        fprintf(stderr, "parse failed.\n");
        return 2;
    }

    int return_code;
    switch(pre_info.gen_mode) {
    default:
    case GEN_HEADER:     return_code = generate_c_header(&pre_info, &parse_info); break;
    case GEN_CITY:       return_code = generate_context_city(&pre_info, &parse_info); break;
    case GEN_VIM_SYNTAX: return_code = generate_vim_syntax(&pre_info, &parse_info); break;
    }
    if (return_code < 0) return return_code;

    metrics.gen_time += nanointerval();

    if (pre_info.show_metrics) {
        char * buf = NULL;
        strputf(&buf, "intro version %s", VERSION);
#ifdef DEBUG
        strputf(&buf, " (debug)");
#endif
        strputf(&buf, "\n");

#define AS_SECONDS(t) ((t) / 1000000000.0)
        uint64_t now = nanotime();
        strputf(&buf, "Total time: %.6f s\n", AS_SECONDS(now - metrics.start));
        strputf(&buf, "|-Pre: %.6f s\n", AS_SECONDS(metrics.pre_time + metrics.lex_time + metrics.macro_time));
        strputf(&buf, "| |-Tokenization:    %.6f s\n", AS_SECONDS(metrics.lex_time));
        strputf(&buf, "| |-Macro expansion: %.6f s\n", AS_SECONDS(metrics.macro_time));
        strputf(&buf, "| |-Other:           %.6f s\n", AS_SECONDS(metrics.pre_time));
        strputf(&buf, "|   %'16lu files\n", (unsigned long)metrics.count_pre_files);
        strputf(&buf, "|   %'16lu lines\n", (unsigned long)metrics.count_pre_lines);
        strputf(&buf, "|   %'16lu tokens\n", (unsigned long)metrics.count_pre_tokens);
        strputf(&buf, "|-Parse: %.6f s\n", AS_SECONDS(metrics.parse_time + metrics.attribute_time));
        strputf(&buf, "| |-Types:      %.6f s\n", AS_SECONDS(metrics.parse_time));
        strputf(&buf, "| |-Attributes: %.6f s\n", AS_SECONDS(metrics.attribute_time));
        strputf(&buf, "|   %'11lu tokens\n", (unsigned long)metrics.count_parse_tokens);
        strputf(&buf, "|   %'11lu types\n", (unsigned long)metrics.count_parse_types);
        strputf(&buf, "|-Gen: %.6f s\n", AS_SECONDS(metrics.gen_time));
        strputf(&buf, "|   %'11lu types\n", (unsigned long)metrics.count_gen_types);
        fputs(buf, stdout);
        arrfree(buf);
    }

    return return_code;
}
