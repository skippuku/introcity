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

    while (s && s < buffer_end) {
        if (*s == '\n') {
            line_is_directive = true;
            last_line = s + 1;
            line_num++;
            s++;
        } else if (isspace(*s)) {
            s++;
        } else if (*s == '#' && line_is_directive) {
            s++;
            Token tk = next_token(&s);
            bool paste_last_chunk = arrlast(if_depth);
            if (tk_equal(&tk, "include")) {
                tk = next_token(&s);
                if (arrlast(if_depth) && tk.type == TK_STRING) {
                    char * inc_filename = copy_and_terminate(tk.start, tk.length); // leak
                    preprocess_filename(inc_filename);
                } else { // TODO: implement <> includes
                }
            } else if (tk_equal(&tk, "if")) {
                arrput(if_depth_prlens, arrlen(if_depth));
                if (arrlast(if_depth)) {
                    tk = next_token(&s);
                    if (!(tk.length == 1 && *tk.start == '0')) {
                        arrput(if_depth, true);
                    } else {
                        arrput(if_depth, false);
                    }
                } else {
                    arrput(if_depth, false);
                }
            } else if (tk_equal(&tk, "ifdef")) {
                arrput(if_depth_prlens, arrlen(if_depth));
                if (arrlast(if_depth)) {
                    tk = next_token(&s);
                    char * name = copy_and_terminate(tk.start, tk.length);
                    arrput(if_depth, hmgeti(defines, name) >= 0 ? true : false);
                    free(name);
                } else {
                    arrput(if_depth, false);
                }
            } else if (tk_equal(&tk, "ifndef")) {
                arrput(if_depth_prlens, arrlen(if_depth));
                if (arrlast(if_depth)) {
                    tk = next_token(&s);
                    char * name = copy_and_terminate(tk.start, tk.length);
                    arrput(if_depth, hmgeti(defines, name) >= 0 ? false : true);
                    free(name);
                } else {
                    arrput(if_depth, false);
                }
            } else if (tk_equal(&tk, "endif")) {
                if (arrlen(if_depth) > 1) {
                    int prlen = arrpop(if_depth_prlens);
                    arrsetlen(if_depth, prlen);
                } else {
                    printf("Error.\n");
                    exit(1);
                }
            } else if (tk_equal(&tk, "else")) {
                bool else_state = !arrpop(if_depth);
                if (arrlast(if_depth)) {
                    arrput(if_depth, else_state);
                }
            } else if (tk_equal(&tk, "elif")) {
                bool else_state = !arrpop(if_depth);
                if (arrlast(if_depth)) {
                    arrput(if_depth, else_state);
                    tk = next_token(&s);
                    if (else_state && !(tk.length == 1 && *tk.start == '0')) {
                        arrput(if_depth, true);
                    } else {
                        arrput(if_depth, false);
                    }
                } else {
                    arrput(if_depth, false);
                }
            }

            if (last_line - last_paste > 0) {
                FileLoc loc;
                loc.offset = arrlen(result_buffer);
                loc.filename = filename;
                loc.line = last_paste_line_num;

                if (paste_last_chunk) {
                    strputn(result_buffer, last_paste, last_line - last_paste);
                }

                arrput(file_location_lookup, loc);
            }
            s = memchr(s, '\n', buffer_end - s);
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
run_preprocessor(char * filename) {
    sh_new_arena(defines);
    struct defines_s intro_define;
    intro_define.key = "__INTROCITY__";
    hmputs(defines, intro_define);

    arrput(if_depth, true);

    preprocess_filename(filename);
    strputnull(result_buffer);

    arrfree(if_depth);
    arrfree(if_depth_prlens);

    return result_buffer;
}
