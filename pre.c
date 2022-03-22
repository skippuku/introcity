#include "util.c"
#include "lexer.c"

typedef struct {
    char * filename;
    char * buffer;
    size_t buffer_size;
    bool once;
} FileBuffer;
static FileBuffer * file_buffers = NULL;

typedef struct {
    char * key;
    char * str;
} Define;
static Define * defines = NULL;

typedef struct {
    size_t offset;
    char * filename;
    int file_offset;
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
    for (int i=0; i < highlight_start - segment_start; i++) arrput(*p_s, ' ');
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

// TODO: open original file instead of buffer for parse_error
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
    arrfree(s);
}
//#define parse_error(tk,message) parse_error_internal(buffer, tk, message)

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
    arrfree(s);
}
#define preprocess_error(tk, message)   preprocess_message_internal(last_to_be, filename, line_num, tk, message, 0)
#define preprocess_warning(tk, message) preprocess_message_internal(last_to_be, filename, line_num, tk, message, 1)

static bool * if_depth = NULL;
static int * if_depth_prlens = NULL;
static char * result_buffer = NULL;

static int
count_newlines_in_range(char * s, char * end, char ** o_last_line) {
    int result = 0;
    while (s < end) {
        if (*s++ == '\n') {
            *o_last_line = s;
            result += 1;
        }
    }
    return result;
}

static void
ignore_section(char ** buffer, char * filename, char * file_buffer, char ** o_paste_begin, char * ignore_begin, char * end) {
    // save last chunk location
    FileLoc loc;
    loc.offset = arrlen(*buffer);
    loc.filename = filename;
    loc.file_offset = *o_paste_begin - file_buffer;
    arrput(file_location_lookup, loc);

    // insert last chunk into result buffer
    strputf(buffer, "%.*s", (int)(ignore_begin - *o_paste_begin), *o_paste_begin);
    *o_paste_begin = end;
}

char *
strip_comments(char ** o_s) {
    char * content = NULL;
    bool last_was_identifier = false;
    while (1) {
        Token tk = pre_next_token(o_s);
        if (tk.type == TK_NEWLINE) {
            *o_s = tk.start;
            break;
        } else if (tk.type == TK_COMMENT) {
            continue;
        } else {
            if (last_was_identifier && tk.type == TK_IDENTIFIER) {
                arrput(content, ' ');
            }
            last_was_identifier = tk.type == TK_IDENTIFIER;
            strputf(&content, "%.*s", (int)(tk.length), tk.start);
        }
    }
    return content;
}

bool
parse_expression(char * s) {
    Token tk = next_token(&s);
    if (tk.type == TK_IDENTIFIER && tk_equal(&tk, "1")) {
        return true;
    } else {
        return false;
    }
}

void
pre_skip(char ** o_s, bool elif_ok) {
    int depth = 1;
    while (1) {
        Token tk = pre_next_token(o_s);
        if (tk.length == 1 && tk.start[0] == '#') {
            tk = pre_next_token(o_s);
            if (tk.type == TK_IDENTIFIER) {
                if (tk_equal(&tk, "if")
                 || tk_equal(&tk, "ifdef")
                 || tk_equal(&tk, "ifndef"))
                {
                    depth++;
                } else if (tk_equal(&tk, "endif")) {
                    depth--;
                } else if (tk_equal(&tk, "else")) {
                    if (depth == 1) {
                        while (tk.type != TK_NEWLINE && tk.type != TK_END) {
                            tk = pre_next_token(o_s);
                        }
                        *o_s = tk.start;
                        return;
                    }
                } else if (tk_equal(&tk, "elif")) {
                    if (elif_ok && depth == 1) {
                        char * expr = strip_comments(o_s);
                        bool expr_result = parse_expression(expr);
                        arrfree(expr);
                        if (expr_result) {
                            return;
                        }
                    }
                } else {
                    goto endif_nextline;
                }
            }
        } else {
        endif_nextline:
            while (tk.type != TK_NEWLINE && tk.type != TK_END) {
                tk = pre_next_token(o_s);
            }
            if (depth == 0 || tk.type == TK_END) {
                *o_s = tk.start;
                return;
            }
        }
    }
}

int
preprocess_filename(char ** result_buffer, char * filename) {
    char * file_buffer = NULL;
    size_t file_size;

    FileBuffer * buf_for_this_file = NULL;
    // search for buffer or create it if it doesn't exist
    for (int i=0; i < arrlen(file_buffers); i++) {
        FileBuffer * fb = &file_buffers[i];
        if (strcmp(filename, fb->filename) == 0) {
            if (fb->once) {
                return 0;
            }
            file_buffer = fb->buffer;
            file_size = fb->buffer_size;
            buf_for_this_file = fb;
            break;
        }
    }
    if (!file_buffer) {
        if ((file_buffer = read_entire_file(filename, &file_size))) {
            FileBuffer new_buf;
            new_buf.filename = filename;
            new_buf.buffer = file_buffer;
            new_buf.buffer_size = file_size;
            arrput(file_buffers, new_buf);
            buf_for_this_file = &arrlast(file_buffers);
        } else {
            return -1; // TODO(print_error)
        }
    }

    char * s = file_buffer;
    char * chunk_begin = s;

    while (1) {
        char * start_of_line = s;
        char * inc_filename = NULL;
        Token tk = pre_next_token(&s);

        if (tk.type == TK_END) {
            ignore_section(result_buffer, filename, file_buffer, &chunk_begin, s, NULL);
            break;
        } else if (tk.type == TK_COMMENT) {
            ignore_section(result_buffer, filename, file_buffer, &chunk_begin, tk.start, s);
        } else if (tk.type == TK_IDENTIFIER) {
            char terminated [tk.length + 1];
            memcpy(terminated, tk.start, tk.length);
            terminated[tk.length] = '\0';
            int def_index = shgeti(defines, terminated);
            if (def_index >= 0) {
                ignore_section(result_buffer, filename, file_buffer, &chunk_begin, tk.start, s);
                strputf(result_buffer, "%s", defines[def_index].str);
            }
        } else if (*tk.start == '#') {
            Token directive = pre_next_token(&s); // TODO: handle comments here
            if (directive.type != TK_IDENTIFIER) {
                goto unknown_directive;
            }

            if (tk_equal(&directive, "include")) {
                Token next = pre_next_token(&s);
                if (next.type == TK_STRING) {
                    // defer this till after the last section gets pasted
                    inc_filename = copy_and_terminate(next.start + 1, next.length - 2);
                } else if (*next.start == '<') { // TODO
                    // ignored for now
                } else {
                    // TODO(print_error)
                    puts("Error: unexpected token in include directive.");
                    return -1;
                }
            } else if (tk_equal(&directive, "define")) {
                // TODO: function-like macros
                Token macro_name = pre_next_token(&s);
                if (macro_name.type != TK_IDENTIFIER) {
                    // TODO(print_error) Expected identifier
                    return -1;
                }

                if (tk_equal(&macro_name, "I")) {
                    goto pre_nextline;
                }

                bool is_func_like = *s == '(';
                if (is_func_like) {
                    s = strchr(s, ')');
                    if (s == NULL) {
                        // TODO(print_error)
                        return -1;
                    }
                    s++;
                }

                char * macro_content = strip_comments(&s);

                // create macro
                Define def;
                def.key = copy_and_terminate(macro_name.start, macro_name.length);
                def.str = macro_content;
                shputs(defines, def);
            } else if (tk_equal(&directive, "undef")) {
                Token def = pre_next_token(&s);
                char * iden = copy_and_terminate(def.start, def.length);
                (void) shdel(defines, iden);
                free(iden);
            } else if (tk_equal(&directive, "if")) {
                char * expr = strip_comments(&s);
                bool expr_result = parse_expression(expr);
                arrfree(expr);
                if (!expr_result) {
                    pre_skip(&s, true);
                }
            } else if (tk_equal(&directive, "ifdef")) {
                tk = pre_next_token(&s);
                char * name = copy_and_terminate(tk.start, tk.length);
                int def_index = shgeti(defines, name);
                free(name);
                if (def_index < 0) {
                    pre_skip(&s, true);
                }
            } else if (tk_equal(&directive, "ifndef")) {
                tk = pre_next_token(&s);
                char * name = copy_and_terminate(tk.start, tk.length);
                int def_index = shgeti(defines, name);
                free(name);
                if (def_index >= 0) {
                    pre_skip(&s, true);
                }
            } else if (tk_equal(&directive, "elif")) {
                pre_skip(&s, false);
            } else if (tk_equal(&directive, "else")) {
            } else if (tk_equal(&directive, "endif")) {
            } else if (tk_equal(&directive, "error")) {
                preprocess_message_internal(start_of_line, filename, 0, &directive, "User error", 0); // TODO(line)
                return -1;
            } else if (tk_equal(&directive, "pragma")) {
                tk = pre_next_token(&s);
                if (tk_equal(&tk, "once")) {
                    buf_for_this_file->once = true;
                }
            } else {
            unknown_directive:
                // TODO: line number
                preprocess_message_internal(start_of_line, filename, 0, &directive, "Unknown directive", 0); // TODO(line)
                return -1;
            }

        pre_nextline:
            // find next line
            while (tk.type != TK_NEWLINE && tk.type != TK_END) tk = pre_next_token(&s);

            ignore_section(result_buffer, filename, file_buffer, &chunk_begin, start_of_line, s);

            if (inc_filename) {
                int error = preprocess_filename(result_buffer, inc_filename);
                if (error) return error;
            }
        }
    }

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

    int error = preprocess_filename(&result_buffer, filename);
    if (error) return NULL;

    strputnull(result_buffer);

    arrfree(if_depth);
    arrfree(if_depth_prlens);

    if (preprocess_only) {
        fputs(result_buffer, stdout);
        exit(0);
    }

    return result_buffer;
}
