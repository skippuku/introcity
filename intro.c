#include "lib/intro.h"
#include "lexer.c"
#include "util.h"
#include "expr.c"
#include "pre.c"
#include "parse.c"
#include "gen.c"

#ifdef _WIN32
  #include <windef.h>
  #include <wingdi.h>
  #include <winbase.h>
  #include <wincon.h>
  #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
  #endif
#endif

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

    char * output_filename = NULL;
    char * preprocessed_buffer = run_preprocessor(argc, argv, &output_filename);
    if (!preprocessed_buffer) {
        fprintf(stderr, "preprocessor failed.\n");
        return -1;
    }

    IntroInfo info = {0};
    int error = parse_preprocessed_text(preprocessed_buffer, &info);
    if (error) {
        fprintf(stderr, "parse failed.\n");
        return error;
    }

    char * header = generate_c_header(&info);
    if (!header) {
        fprintf(stderr, "generator failed.\n");
        return -2;
    }

    error = intro_dump_file(output_filename, header, strlen(header));
    if (error) {
        fprintf(stderr, "failed to write to file '%s'.\n", output_filename);
        return error;
    }

    return 0;
}
