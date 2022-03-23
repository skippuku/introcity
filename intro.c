#include "intro.h"
#include "util.c"
#include "pre.c"
#include "parse.c"
#include "gen.c"

int
main(int argc, char * argv []) {
    char * output_filename = NULL;
    char * preprocessed_buffer = run_preprocessor(argc, argv, &output_filename);
    if (!preprocessed_buffer) {
        fprintf(stderr, "preprocessor failed.\n");
        return -1;
    }

    IntroInfo info = {0};
    int error = parse_preprocessed_text(preprocessed_buffer, &info);
    if (error) {
        fprintf(stderr, "parse failed. error code: %i\n", error);
        return error;
    }

    char * header = generate_c_header(&info);
    if (!header) {
        fprintf(stderr, "generator failed.\n");
        return -2;
    }

    error = dump_to_file(output_filename, header, strlen(header));
    if (error) {
        fprintf(stderr, "failed to write to file '%s'.\n", output_filename);
        return error;
    }

    return 0;
}
