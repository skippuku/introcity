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

static void
show_metrics() {
    char * buf = NULL;
    strputf(&buf, "intro version %s", VERSION);
#ifdef DEBUG
    strputf(&buf, " (debug)");
#endif
    strputf(&buf, "\n");

#define AS_SECONDS(t) ((t) / (double)g_timer_freq)
    uint64_t now = nanotime();
    strputf(&buf, "Total time: %.6f s\n", AS_SECONDS(now - g_metrics.start));
    strputf(&buf, "|-Pre: %.6f s\n", AS_SECONDS(g_metrics.pre_time + g_metrics.lex_time));
    strputf(&buf, "| |-Tokenization: %.6f s\n", AS_SECONDS(g_metrics.lex_time));
    strputf(&buf, "| |-Other:        %.6f s\n", AS_SECONDS(g_metrics.pre_time));
    strputf(&buf, "|   %'11lu files\n", (unsigned long)g_metrics.count_pre_files);
    strputf(&buf, "|   %'11lu lines\n", (unsigned long)g_metrics.count_pre_lines);
    strputf(&buf, "|   %'11lu tokens\n", (unsigned long)g_metrics.count_pre_tokens);
    strputf(&buf, "|-Parse: %.6f s\n", AS_SECONDS(g_metrics.parse_time + g_metrics.attribute_time));
    strputf(&buf, "| |-Types:      %.6f s\n", AS_SECONDS(g_metrics.parse_time));
    strputf(&buf, "| |-Attributes: %.6f s\n", AS_SECONDS(g_metrics.attribute_time));
    strputf(&buf, "|   %'11lu tokens\n", (unsigned long)g_metrics.count_parse_tokens);
    strputf(&buf, "|   %'11lu types\n", (unsigned long)g_metrics.count_parse_types);
    strputf(&buf, "|-Gen: %.6f s\n", AS_SECONDS(g_metrics.gen_time));
    strputf(&buf, "|   %'11lu types\n", (unsigned long)g_metrics.count_gen_types);
    fputs(buf, stdout);
    arrfree(buf);
#undef AS_SECONDS
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

    free_dyn_allocator(g_dynalloc);

    return 0;
}
