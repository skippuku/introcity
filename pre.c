static size_t
fsize(FILE * file) {
    long location = ftell(file);
    fseek(file, 0, SEEK_END);
    size_t result = ftell(file);
    fseek(file, location, SEEK_SET);
    return result;
}

static char *
read_and_allocate_file(const char * filename, size_t * o_size) {
    FILE * file = fopen(filename, "rb");
    assert(file != NULL);
    size_t file_size = fsize(file);
    char * buffer = malloc(file_size + 1);
    if (fread(buffer, file_size, 1, file) != 1) {
        fclose(file);
        free(buffer);
        return NULL;
    }
    fclose(file);
    buffer[file_size] = '\0';
    if (o_size) *o_size = file_size;
    return buffer;
}

static bool
tk_equal(Token * tk, const char * str) {
    size_t len = strlen(str);
    return (tk->length == len && memcmp(tk->start, str, len) == 0);
}

static char *
copy_and_terminate(char * str, int length) {
    char * result = malloc(length + 1);
    memcpy(result, str, length);
    result[length] = '\0';
    return result;
}

struct defines_s {
    char * key;
} * defines = NULL;

typedef struct {
    size_t offset;
    char * filename;
    int line;
} FileLoc;
FileLoc * file_location_lookup = NULL;

bool * if_depth = NULL;
int * if_depth_prlens = NULL;
static char * result_buffer = NULL;

int
parse_expression(char ** o_s) {
    Token tk = next_token(o_s);
    if (tk.length == 1 && *tk.start == '0') {
        return 0;
    } else {
        return 1;
    }
}

void
preprocess_filename(char * filename) {
    size_t file_size;
    char * buffer = read_and_allocate_file(filename, &file_size);
    if (!buffer) {
        printf("Error: failed to read file \"%s\".\n", filename);
        exit(1);
    }

    char * buffer_end = buffer + file_size;
    char * last_line = buffer;
    char * last_paste = buffer;
    char * s = buffer;

    int line_num = 1;
    int last_paste_line_num = 1;
    bool line_is_directive = true;
    bool in_comment = false;

    // TODO: handle c-style comments
    while (s && s < buffer_end) {
        if (*s == '\n') {
            if (!in_comment) line_is_directive = true;
            last_line = s + 1;
            line_num++;
            s++;
        } else if (isspace(*s)) {
            s++;
        } else if (*s == '#' && line_is_directive) {
            s++;
            Token tk = next_token(&s);
            char * inc_filename = NULL;
            bool paste_last_chunk = arrlast(if_depth);
            if (tk_equal(&tk, "include")) {
                if (arrlast(if_depth)) {
                    tk = next_token(&s);
                    if (tk.type == TK_STRING) {
                        inc_filename = copy_and_terminate(tk.start, tk.length); // leak
                    } else { // TODO: implement <> includes
                    }
                }
            } else if (tk_equal(&tk, "if")) {
                arrput(if_depth_prlens, arrlen(if_depth));
                if (arrlast(if_depth)) {
                    arrput(if_depth, parse_expression(&s));
                } else {
                    arrput(if_depth, false);
                }
            } else if (tk_equal(&tk, "define")) {
                if (arrlast(if_depth)) {
                    tk = next_token(&s);
                    if (tk.type != TK_IDENTIFIER) {
                        fprintf(stderr, "Preprocessor error: Unknown symbol after define.\n");
                        exit(1);
                    }
                    struct defines_s new_def;
                    char * name = copy_and_terminate(tk.start, tk.length);
                    // NOTE: compilers warn on redefinitions, but we don't care
                    new_def.key = name;
                    shputs(defines, new_def);
                    free(name);
                }
            } else if (tk_equal(&tk, "undef")) {
                if (arrlast(if_depth)) {
                    tk = next_token(&s);
                    char * name = copy_and_terminate(tk.start, tk.length);
                    (void)shdel(defines, name);
                    free(name);
                }
            } else if (tk_equal(&tk, "ifdef")) {
                arrput(if_depth_prlens, arrlen(if_depth));
                if (arrlast(if_depth)) {
                    tk = next_token(&s);
                    char * name = copy_and_terminate(tk.start, tk.length);
                    arrput(if_depth, shgeti(defines, name) >= 0 ? true : false);
                    free(name);
                } else {
                    arrput(if_depth, false);
                }
            } else if (tk_equal(&tk, "ifndef")) {
                arrput(if_depth_prlens, arrlen(if_depth));
                if (arrlast(if_depth)) {
                    tk = next_token(&s);
                    char * name = copy_and_terminate(tk.start, tk.length);
                    arrput(if_depth, shgeti(defines, name) >= 0 ? false : true);
                    free(name);
                } else {
                    arrput(if_depth, false);
                }
            } else if (tk_equal(&tk, "endif")) {
                if (arrlen(if_depth) > 1) {
                    int prlen = arrpop(if_depth_prlens);
                    arrsetlen(if_depth, prlen);
                } else {
                    fprintf(stderr, "Preprocessor error: stray #endif\n");
                    exit(1);
                }
            } else if (tk_equal(&tk, "else")) {
                bool take_else = !arrpop(if_depth);
                if (arrlast(if_depth)) {
                    arrput(if_depth, take_else);
                }
            } else if (tk_equal(&tk, "elif")) {
                bool take_else = !arrpop(if_depth);
                if (arrlast(if_depth)) {
                    arrput(if_depth, take_else);
                    arrput(if_depth, (take_else && parse_expression(&s)));
                } else {
                    arrput(if_depth, false);
                }
            } else if (tk_equal(&tk, "error")) {
                if (arrlast(if_depth)) {
                    tk = next_token(&s);
                    fprintf(stderr, "Preprocessor error: \"%.*s\"\n", tk.length, tk.start);
                    exit(1);
                }
            }

            if (paste_last_chunk && last_line - last_paste > 0) {
                FileLoc loc;
                loc.offset = arrlen(result_buffer);
                loc.filename = filename;
                loc.line = last_paste_line_num;

                strputn(result_buffer, last_paste, last_line - last_paste);

                arrput(file_location_lookup, loc);
            }

            if (inc_filename) preprocess_filename(inc_filename);

            while (1) {
                while (*s != '\n' && *s != '\0') s++;
                if (*s == '\0') break;
                char * q = s;
                while (isspace(*--q));
                if (*q != '\\') break;
            }
            last_paste = s + 1;
            last_line = s + 1;
            last_paste_line_num = line_num + 1;
        } else {
            line_is_directive = false;
            s = memchr(s, '\n', buffer_end - s);
        }
    }
    FileLoc loc;
    loc.offset = arrlen(result_buffer);
    loc.filename = filename;
    loc.line = last_paste_line_num;

    strputn(result_buffer, last_paste, buffer_end - last_paste);

    arrput(file_location_lookup, loc);

    free(buffer);
}

char *
run_preprocessor(int argc, char ** argv, char ** o_output_filename) {
    sh_new_arena(defines);
    struct defines_s intro_define;
    intro_define.key = "__INTROCITY__";
    shputs(defines, intro_define);

    *o_output_filename = NULL;

    bool preprocess_only = false;
    char * filename = NULL;
    for (int i=1; i < argc; i++) {
        char * arg = argv[i];
        if (arg[0] == '-') {
            switch(arg[1]) {
            case 'D': {
                struct defines_s new_def;
                new_def.key = strlen(arg) == 2 ? argv[++i] : arg+2;
                shputs(defines, new_def);
            } break;

            case 'E': {
                preprocess_only = true;
            } break;

            case 'o': {
                *o_output_filename = argv[++i];
            } break;

            default: {
                fprintf(stderr, "Error: Unknown argument: %s\n", arg);
                exit(1);
            } break;
            }
        } else {
            if (filename) {
                fprintf(stderr, "Error: This program cannot currently parse more than 1 file.\n");
                exit(1);
            } else {
                filename = arg;
            }
        }
    }

    if (!filename) {
        fprintf(stderr, "No filename given.\n");
        exit(1);
    }
    if (*o_output_filename == NULL) {
        strput(*o_output_filename, filename);
        strput(*o_output_filename, ".intro");
        strputnull(*o_output_filename);
    }

    arrput(if_depth, true);

    preprocess_filename(filename);
    strputnull(result_buffer);

    arrfree(if_depth);
    arrfree(if_depth_prlens);

    if (preprocess_only) {
        fputs(result_buffer, stdout);
        exit(0);
    }

    return result_buffer;
}
