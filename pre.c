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
    if (!file) return NULL;
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

typedef struct {
    char * filename;
    char * buffer;
    size_t buffer_size;
} FileBuffer;
static FileBuffer * file_buffers = NULL;

typedef struct {
    char * key;
    char * start;
    int length;
} Define;
static Define * defines = NULL;

typedef struct {
    size_t offset;
    char * filename;
    int line;
} FileLoc;
FileLoc * file_location_lookup = NULL;

#define BOLD_RED "\e[1;31m"
#define BOLD_YELLOW "\e[1;33m"
#define CYAN "\e[0;36m"
#define WHITE "\e[0;37m"
#define BOLD_WHITE "\e[1;37m"
static void
strput_code_segment(char ** p_s, char * segment_start, char * segment_end, char * highlight_start, char * highlight_end, const char * highlight_color) {
    strputf(p_s, "%.*s", (int)(highlight_start - segment_start), segment_start);
    strputf(p_s, "%s%.*s" WHITE, highlight_color, (int)(highlight_end - highlight_start), highlight_start);
    strputf(p_s, "%.*s", (int)(segment_end - highlight_end), highlight_end);
    for (int i=0; i < (highlight_start - segment_start); i++) arrput(*p_s, ' ');
    for (int i=0; i < highlight_end - highlight_start; i++) arrput(*p_s, '~');
    arrput(*p_s, '\n');
    strputnull(*p_s);
}

static int
get_line(char * begin, char * pos, char ** o_start_of_line, char ** o_filename) {
    FileLoc * loc = NULL;
    for (int i = arrlen(file_location_lookup) - 1; i >= 0; i--) {
        if (pos - begin >= file_location_lookup[i].offset) {
            loc = &file_location_lookup[i];
            break;
        }
    }
    if (loc == NULL) return -1;
    char * s = begin + loc->offset;
    char * last_line = s;
    int line_num = loc->line;
    while (s < pos) {
        if (*s++ == '\n') {
            last_line = s;
            line_num++;
        }
    }
    if (o_start_of_line) *o_start_of_line = last_line;
    if (o_filename) *o_filename = loc->filename;
    return line_num;
}

static void
parse_error_internal(char * buffer, Token * tk, char * message) {
    char * start_of_line;
    char * filename;
    int line_num = get_line(buffer, tk->start, &start_of_line, &filename);
    char * s = NULL;
    if (line_num < 0) {
        strputf(&s, BOLD_RED "Error" WHITE " (?:?): %s\n\n", message);
        return;
    }
    strputf(&s, BOLD_RED "Error" WHITE " (%s:%i): %s\n\n", filename, line_num, message);
    char * end_of_line = strchr(tk->start + tk->length, '\n') + 1;
    strput_code_segment(&s, start_of_line, end_of_line, tk->start, tk->start + tk->length, BOLD_RED);
    fputs(s, stderr);
}
#define parse_error(tk,message) parse_error_internal(buffer, tk, message)

static void
preprocess_message_internal(char * start_of_line, char * filename, int line, Token * p_tk, char * message, int message_type) {
    char * end_of_line = strchr(p_tk->start + p_tk->length, '\n') + 1;
    char * s = NULL;
    const char * message_type_string = message_type == 1 ? "warning" : "error";
    const char * color = message_type == 1 ? BOLD_YELLOW : BOLD_RED;
    strputf(&s, "%s" "Preprocessor %s" WHITE " (" CYAN "%s:" BOLD_WHITE "%i" WHITE "): %s\n\n", color, message_type_string, filename, line, message);
    strput_code_segment(&s, start_of_line, end_of_line, p_tk->start, p_tk->start + p_tk->length, color);
    strputnull(s);
    fputs(s, stderr);
}
#define preprocess_error(tk, message)   preprocess_message_internal(last_line, filename, line_num, tk, message, 0)
#define preprocess_warning(tk, message) preprocess_message_internal(last_line, filename, line_num, tk, message, 1)

static bool * if_depth = NULL;
static int * if_depth_prlens = NULL;
static char * result_buffer = NULL;

static int
count_newlines_in_range(const char * s, const char * end) {
    int result = 0;
    while (s < end) {
        if (*s++ == '\n') result += 1;
    }
    return result;
}

int
parse_expression(char ** o_s) { // TODO
    Token tk = next_token(o_s);
    if (tk.length == 1 && *tk.start == '1') {
        return 1;
    } else {
        return 0;
    }
}

int
preprocess_filename(char * filename) {
    size_t file_size;

    char * buffer = NULL;
    for (int i=0; i < arrlen(file_buffers); i++) {
        FileBuffer * fb = &file_buffers[i];
        if (strcmp(filename, fb->filename) == 0) {
            buffer = fb->buffer;
            file_size = fb->buffer_size;
            break;
        }
    }
    if (!buffer) {
        if ((buffer = read_and_allocate_file(filename, &file_size))) {
            FileBuffer new_buf;
            new_buf.filename = filename;
            new_buf.buffer = buffer;
            new_buf.buffer_size = file_size;
            arrput(file_buffers, new_buf);
        } else {
            return -1;
        }
    }

    char * buffer_end = buffer + file_size;
    char * last_line = buffer;
    char * last_paste = buffer;
    char * s = buffer;

    int line_num = 1;
    int last_paste_line_num = 1;
    bool line_is_directive = true;

    // TODO: rewrite if/elif/else system
    // TODO: handle c-style comments
    // TODO: expand macros, open original file instead of buffer for parse_error
    while (s && s < buffer_end) {
        if (*s == '\n') {
            line_is_directive = true;
            last_line = s + 1;
            line_num++;
            s++;
        } else if (*s == '/' && *(s+1) == '*') {
            s += 1;
            while (*++s != '\0') {
                if (*s == '/' && *(s-1) == '*') {
                    s = memchr(s, '\n', buffer_end - s);
                    break;
                } else if (*s == '\n') {
                    // from above
                    last_line = s + 1;
                    line_num++;
                }
            }
        } else if (is_space(*s)) {
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
                        inc_filename = copy_and_terminate(tk.start, tk.length);
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
                        preprocess_error(&tk, "Expected identifier.");
                        exit(1);
                    }
                    Define new_def;
                    // NOTE: compilers warn on redefinitions, but we don't care
                    char * name = copy_and_terminate(tk.start, tk.length);
                    new_def.key = name;

                    bool valid = true;
                    char * after_name = tk.start + tk.length;
                    tk = next_token(&s);
                    if (memchr(after_name, '\n', tk.start - after_name) == NULL) {
                        if (tk.type == TK_L_PARENTHESIS) {
                            preprocess_warning(&tk, "Function-like macros are currently ignored.");
                            valid = false;
                        }
                        // @copy from below
                        while (1) {
                            while (*s != '\n' && *s != '\0') s++;
                            if (*s == '\0') break;
                            char * q = s;
                            while (is_space(*--q));
                            if (*q != '\\') break;
                            line_num++;
                            s++;
                        }
                        new_def.start = tk.start;
                        new_def.length = s - tk.start;
                    } else {
                        s = after_name;
                        new_def.start = "";
                        new_def.length = 0;
                    }
                    if (valid) {
                        //printf("new definition: %s = \"%.*s\"\n\n", new_def.key, new_def.length, new_def.start);
                        shputs(defines, new_def);
                    }
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
                    preprocess_error(&tk, "stray #endif.");
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
                    char * message = NULL;
                    Token error_string = next_token(&s);
                    if (error_string.type == TK_STRING) {
                        strputf(&message, "\"%.*s\"", error_string.length, error_string.start);
                        strputnull(message);
                    } else {
                        message = "Unspecified error.";
                    }
                    preprocess_error(&tk, message);
                    exit(1);
                }
            }

            if (paste_last_chunk && last_line - last_paste > 0) {
                FileLoc loc;
                loc.offset = arrlen(result_buffer);
                loc.filename = filename;
                loc.line = last_paste_line_num;

                strputf(&result_buffer, "%.*s", (int)(last_line - last_paste), last_paste);

                arrput(file_location_lookup, loc);
            }

            if (inc_filename) {
                int result = preprocess_filename(inc_filename);
                if (result < 0) {
                    preprocess_error(&tk, "Cannot read file.");
                    exit(1);
                }
            }

            while (1) {
                while (*s != '\n' && *s != '\0') s++;
                if (*s == '\0') break;
                char * q = s;
                while (is_space(*--q));
                if (*q != '\\') break;
                line_num++;
                s++;
            }
            last_paste = s + 1;
            last_line = s + 1;
            last_paste_line_num = line_num + 1;
        } else {
            line_is_directive = false;
            s = memchr(s, '\n', buffer_end - s);
        }
    }
    // @copy from above
    FileLoc loc;
    loc.offset = arrlen(result_buffer);
    loc.filename = filename;
    loc.line = last_paste_line_num;

    strputf(&result_buffer, "%.*s", (int)(buffer_end - last_paste), last_paste);

    arrput(file_location_lookup, loc);

    return 0;
}

char *
run_preprocessor(int argc, char ** argv, char ** o_output_filename) {
    sh_new_arena(defines);
    Define intro_define;
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
                Define new_def;
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
                fputs("Error: Unknown argument: ", stderr);
                fputs(arg, stderr);
                fputs("\n", stderr);
                exit(1);
            } break;
            }
        } else {
            if (filename) {
                fputs("Error: This program cannot currently parse more than 1 file.\n", stderr);
                exit(1);
            } else {
                filename = arg;
            }
        }
    }

    if (!filename) {
        fputs("No filename given.\n", stderr);
        exit(1);
    }
    if (*o_output_filename == NULL) {
        strputf(o_output_filename, "%s.intro", filename);
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
