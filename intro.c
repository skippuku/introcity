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
        printf("intro version %s\n\n", VERSION);
#define AS_SECONDS(t) ((t) / 1000000000.0)
        uint64_t now = nanotime();
        printf("Total: %.6f s\n", AS_SECONDS(now - metrics.start));
        printf("  Lex:   %.6f s\n", AS_SECONDS(metrics.pre_lex_time));
        printf("  Pre:   %.6f s\n", AS_SECONDS(metrics.pre_time));
        printf("  Parse: %.6f s\n", AS_SECONDS(metrics.parse_time));
        printf("  Gen:   %.6f s\n\n", AS_SECONDS(metrics.gen_time));

        printf("Lines parsed:  %lu\n", (unsigned long)metrics.count_lines);
        printf("Tokens parsed: %lu\n", (unsigned long)metrics.count_tokens);
    }

    return return_code;
}
