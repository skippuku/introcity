#include "lib/intro.h"
#include "lexer.c"
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

    PreInfo pre_info = run_preprocessor(argc, argv);
    if (!pre_info.result_buffer || pre_info.ret != 0) {
        fprintf(stderr, "preprocessor failed.\n");
        return 1;
    }

    ParseInfo parse_info = {0};
    int error = parse_preprocessed_text(&pre_info, &parse_info);
    if (error) {
        fprintf(stderr, "parse failed.\n");
        return 2;
    }

    if (pre_info.gen_city) {
        return generate_context_city(pre_info.output_filename, &parse_info);
    } else {
        char * header = generate_c_header(&parse_info, pre_info.output_filename);
        if (!header) {
            fprintf(stderr, "generator failed.\n");
            return 3;
        }

        error = intro_dump_file(pre_info.output_filename, header, strlen(header));
        arrfree(header);
        if (error) {
            fprintf(stderr, "failed to write to file '%s'.\n", pre_info.output_filename);
            return 4;
        }
    }

    return 0;
}
