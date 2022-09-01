#define INTRO_IMPL
#include "lib/intro.h"
#include "dynalloc.c"
#include "global.c"
#include "config.c"
#include "expr.c"
#include "pre.c"
#include "parse.c"
#include "gen.c"

static void
init_platform() {
#ifdef _WIN32
    HANDLE con = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(con, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(con, mode);

    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    g_timer_freq = li.QuadPart;
#else
    g_timer_freq = 1000000000;
#endif
}

int
main(int argc, char * argv []) {
    init_platform();

    g_dynalloc = new_dyn_allocator();

    Config cfg = get_config(argc, argv);

    g_metrics.start = nanotime();
    g_metrics.last = g_metrics.start;

    PreInfo pre_info = run_preprocessor(&cfg);
    if (!pre_info.result_list || pre_info.ret != 0) {
        fprintf(stderr, "preprocessor failed.\n");
        return 1;
    }

    ParseInfo parse_info = {0};
    int error = parse_preprocessed_tokens(&cfg, &pre_info, &parse_info);
    if (error) {
        fprintf(stderr, "parse failed.\n");
        return 2;
    }

    if (generate_files(&cfg, &pre_info, &parse_info) != 0) {
        fprintf(stderr, "generation failed.\n");
        return 3;
    }

    if (cfg.show_metrics) {
        show_metrics();
    }

#ifdef DEBUG
    free_dyn_allocator(g_dynalloc);
#endif

    return 0;
}
