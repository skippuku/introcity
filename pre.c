#include <sys/unistd.h>

#include "util.h"
#include "lexer.c"

static char * filename_stdin = "__stdin__";

typedef struct {
    char * filename;
    char * buffer;
    size_t buffer_size;
    bool once;
} FileBuffer;
static FileBuffer ** file_buffers = NULL;

typedef struct {
    char * key;
    Token * replace_list;
    char ** arg_list;
    int32_t arg_count;
    bool func_like;
    bool variadic;
} Define; // TODO: rename to Macro
static Define * defines = NULL;

typedef struct {
    size_t offset;
    char * filename;
    size_t file_offset;
    int line;
} FileLoc;
FileLoc * file_location_lookup = NULL;

typedef struct {
    ptrdiff_t * no_expand;
    FileBuffer * current_file_buffer;
    struct {
        bool enabled;
        bool D;
        bool G;
        bool no_sys;
        char * custom_target;
    } m_options;
    bool minimal_parse;
    NameSet * dependency_set;
    int sys_header_first;
    int sys_header_last;
    bool is_sys_header;
} PreContext;

const char ** include_paths = NULL;

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
count_newlines_in_range(char * s, char * end, char ** o_last_line) {
    int result = 1;
    *o_last_line = s;
    while (s < end) {
        if (*s++ == '\n') {
            *o_last_line = s;
            result += 1;
        }
    }
    return result;
}

static int
get_line(char * buffer_begin, char ** o_pos, char ** o_start_of_line, char ** o_filename) {
    FileLoc * loc = NULL;
    for (int i = arrlen(file_location_lookup)-1; i >= 0; i--) {
        ptrdiff_t offset = *o_pos - buffer_begin;
        if (offset >= file_location_lookup[i].offset) {
            loc = &file_location_lookup[i];
            break;
        }
    }
    if (loc == NULL) return -1;
    char * file_buffer = NULL;
    for (int i=0; i < arrlen(file_buffers); i++) {
        if (strcmp(loc->filename, file_buffers[i]->filename) == 0) {
            file_buffer = file_buffers[i]->buffer;
        }
    }
    assert(file_buffer);
    *o_pos = file_buffer + (loc->file_offset + ((*o_pos - buffer_begin) - loc->offset));
    char * last_line;
    int line_num = count_newlines_in_range(file_buffer, *o_pos, &last_line);
    *o_start_of_line = last_line;
    *o_filename = loc->filename;
    return line_num;
}

static void
message_internal(char * start_of_line, char * filename, int line, char * hl_start, char * hl_end, char * message, int message_type) {
    char * end_of_line = strchr(hl_end, '\n') + 1;
    char * s = NULL;
    const char * message_type_string = (message_type == 1)? "Warning" : "Error";
    const char * color = (message_type == 1)? BOLD_YELLOW : BOLD_RED;
    strputf(&s, "%s%s" WHITE " (" CYAN "%s:" BOLD_WHITE "%i" WHITE "): %s\n\n", color, message_type_string, filename, line, message);
    strput_code_segment(&s, start_of_line, end_of_line, hl_start, hl_end, color);
    strputnull(s);
    fputs(s, stderr);
    arrfree(s);
}

void
parse_msg_internal(char * buffer, const Token * tk, char * message, int message_type) {
    char * start_of_line = NULL;
    char * filename = "?";
    char * hl_start = tk->start;
    int line_num = get_line(buffer, &hl_start, &start_of_line, &filename);
    char * hl_end = hl_start + tk->length;
    message_internal(start_of_line, filename, line_num, hl_start, hl_end, message, message_type);
}

static void
preprocess_message_internal(const FileBuffer * file_buffer, const Token * tk, char * message, int msg_type) {
    char * start_of_line;
    if (tk->start < file_buffer->buffer || tk->start >= file_buffer->buffer + file_buffer->buffer_size) {
        for (int i=0; i < arrlen(file_buffers); i++) {
            char * fb_start = file_buffers[i]->buffer;
            char * fb_end = fb_start + file_buffers[i]->buffer_size;
            if (tk->start >= fb_start && tk->start < fb_end) {
                file_buffer = file_buffers[i];
                break;
            }
        }
    }
    int line_num = count_newlines_in_range(file_buffer->buffer, tk->start, &start_of_line);
    message_internal(start_of_line, file_buffer->filename, line_num, tk->start, tk->start + tk->length, message, msg_type);
}

#define preprocess_error(tk, message)   preprocess_message_internal(ctx->current_file_buffer, tk, message, 0)
#define preprocess_warning(tk, message) preprocess_message_internal(ctx->current_file_buffer, tk, message, 1)

static char * result_buffer = NULL;

static void
path_normalize(char * dest) {
    char * dest_start = dest;
    char * src = dest;
    while (*src) {
        if (*src == '\\') *src = '/';
        src++;
    }
    int depth = 0;
    src = dest;
    bool check_next = true;
    if (*src == '/') src++, dest++;
    char * last_dir = dest;
    while (*src) {
        if (check_next) {
            check_next = false;
            while (1) {
                if (memcmp(src, "/", 1)==0) {
                    src += 1;
                } else if (memcmp(src, "./", 2)==0) {
                    src += 2;
                } else if (memcmp(src, "../", 3)==0) {
                    if (depth > 0) {
                        dest = last_dir;
                        last_dir--;
                        while (--last_dir > dest_start && *last_dir != '/');
                        last_dir++;
                        depth -= 1;
                    } else {
                        depth = 0;
                        memcpy(dest, src, 3);
                        dest += 3;
                    }
                    src += 3;
                } else {
                    last_dir = dest;
                    depth += 1;
                    break;
                }
            }
        }
        if (*src == '/') {
            check_next = true;
        }
        *dest++ = *src++;
    }
    *dest = '\0';
}

static void
path_join(char * dest, const char * base, const char * ext) {
    strcpy(dest, base);
    strcat(dest, "/");
    strcat(dest, ext);
    path_normalize(dest);
}

static void
path_dir(char * dest, char * filepath, char ** o_filename) {
    char * end = strrchr(filepath, '/');
    if (end == NULL) {
        strcpy(dest, ".");
        if (o_filename) *o_filename = filepath;
    } else {
        size_t dir_length = end - filepath;
        memcpy(dest, filepath, dir_length);
        dest[dir_length] = '\0';
        if (o_filename) *o_filename = end + 1;
    }
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

static char *
strip_comments(char ** o_s) {
    char * content = NULL;
    Token ltk = {0};
    while (1) {
        Token tk = pre_next_token(o_s);
        if (tk.type == TK_NEWLINE) {
            *o_s = tk.start;
            break;
        } else if (tk.type == TK_COMMENT) {
            continue;
        } else {
            if (ltk.start && ltk.start + ltk.length < tk.start) {
                arrput(content, ' ');
            }
            strputf(&content, "%.*s", (int)(tk.length), tk.start);
            ltk = tk;
        }
    }
    return content;
}

static void
strput_tokens(char ** p_str, Token * list, size_t count) {
    if (!list) return;
    bool last_was_iden = false;
    for (int i=0; i < count; i++) {
        Token tk = list[i];
        if (last_was_iden && tk.type == TK_IDENTIFIER) {
            arrput(*p_str, ' ');
        }
        strputf(p_str, "%.*s", tk.length, tk.start);
        last_was_iden = (tk.type == TK_IDENTIFIER);
    }
}

void
tk_cat(Token ** p_list, Token * ext) {
    for (int i=0; i < arrlen(ext); i++) {
        arrput(*p_list, ext[i]);
    }
}

static Token * internal_expand(PreContext * ctx, Token * replace_list, char ** o_s);

// TODO: macros should probably be stored as text instead of tokens
//       this is messy
static Token *
try_expand_macro(PreContext * ctx, Token * macro_tk, bool pop_no_expand, size_t * o_count, char ** o_s) {
    *o_count = SIZE_MAX;
    STACK_TERMINATE(terminated_tk, macro_tk->start, macro_tk->length);

    ptrdiff_t map_index = shgeti(defines, terminated_tk);
    if (map_index < 0) {
        return NULL;
    }
    for (int i=0; i < arrlen(ctx->no_expand); i++) {
        if (map_index == ctx->no_expand[i]) {
            return NULL;
        }
    }
    int previous_no_expand_length = arrlen(ctx->no_expand);
    Define * macro = &defines[map_index];
    Token * replace_list = NULL;
    bool free_replace_list = false;

    if (macro->func_like && o_s) {
        // get arguments
        Token * list = NULL;
        Token ** args = NULL;
        char * s = *o_s;
        Token open_paren;
        do {
            open_paren = pre_next_token(&s);
        } while (open_paren.type == TK_COMMENT || open_paren.type == TK_NEWLINE);
        if (!(*open_paren.start == '(')) {
            return NULL;
        }
        Token * arg_tks = NULL;
        int paren_depth = 1;
        while (1) {
            Token tk = pre_next_token(&s);
            if (tk.type == TK_COMMENT || tk.type == TK_NEWLINE) {
            } else if (*tk.start == ',' && paren_depth == 1) {
                arrput(args, arg_tks);
                arg_tks = NULL;
            } else if (tk.type == TK_END) {
                goto free_args;
            } else {
                if (*tk.start == '(') {
                    paren_depth++;
                } else if (*tk.start == ')') {
                    paren_depth--;
                    if (paren_depth == 0) {
                        arrput(args, arg_tks);
                        *o_s = s;
                        break;
                    }
                }
                arrput(arg_tks, tk);
            }
        }
        
        for (int i=0; i < arrlen(args); i++) {
            Token * arg_expanded = internal_expand(ctx, args[i], NULL);
            arrfree(args[i]);
            args[i] = arg_expanded;
        }

        for (int tk_i=0; tk_i < arrlen(macro->replace_list); tk_i++) {
            Token tk = macro->replace_list[tk_i];
            bool replaced = false;
            if (tk.type == TK_IDENTIFIER) {
                if (macro->variadic && tk_equal(&tk, "__VA_ARGS__")) {
                    for (int arg_i=macro->arg_count; arg_i < arrlen(args); arg_i++) {
                        tk_cat(&list, args[arg_i]);
                        static const Token comma = {.start = ",", .length = 1};
                        if (arg_i != arrlen(args) - 1) arrput(list, comma);
                    }
                    replaced = true;
                }
                for (int param_i=0; param_i < macro->arg_count; param_i++) {
                    if (tk_equal(&tk, macro->arg_list[param_i])) {
                        tk_cat(&list, args[param_i]);
                        replaced = true;
                        break;
                    }
                }
            }
            if (!replaced) {
                arrput(list, tk);
            }
        }
        replace_list = list;
        list = NULL;
        free_replace_list = true;

    free_args:
        for (int i=0; i < arrlen(args); i++) {
            arrfree(args[i]);
        }
        arrfree(args);
    } else {
        replace_list = macro->replace_list;
    }

    arrpush(ctx->no_expand, map_index);

    // rescan
    Token * final_list = internal_expand(ctx, replace_list, o_s);

    if (pop_no_expand) arrsetlen(ctx->no_expand, previous_no_expand_length);

    if (free_replace_list) arrfree(replace_list);
    *o_count = arrlen(final_list);
    return final_list;
}

static Token *
internal_expand(PreContext * ctx, Token * replace_list, char ** o_s) {
    Token * final_list = NULL;
    if (arrlen(replace_list) > 0) {
        char * temp_buf = NULL; // TODO IMPORTANT: leak
        strput_tokens(&temp_buf, replace_list, arrlen(replace_list));
        char * s = temp_buf;
        Token tk, next = next_token(&s);
        while (1) {
            tk = next;
            if (tk.type == TK_END) {
                break;
            }
            next = next_token(&s);
            if (tk.type == TK_IDENTIFIER) {
                char * temp_s = next.start;
                char ** pass_s = (next.type == TK_END)? o_s : &temp_s;
                size_t count_inner;
                Token * inner_list = try_expand_macro(ctx, &tk, false, &count_inner, pass_s);
                if (count_inner != SIZE_MAX) {
                    if (inner_list && count_inner > 0) {
                        tk_cat(&final_list, inner_list);
                        arrfree(inner_list);
                    }
                    if (temp_s > s) {
                        s = temp_s;
                        next = next_token(&s);
                    }
                    continue;
                }
            }
            arrput(final_list, tk);
        }
    }
    return final_list;
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

static void
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
                    goto nextline;
                }
            }
        } else {
        nextline:
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

static char *
read_stream(FILE * file) {
    char * result = NULL;
    fseek(file, 0, SEEK_SET);
    const int read_size = 1024;
    char buf [read_size];
    while (1) {
        int read_res = fread(&buf, 1, read_size, file);
        memcpy(arraddnptr(result, read_res), buf, read_res);
        if (read_res < read_size) {
            arrput(result, '\0');
            break;
        }
    }
    return result;
}

int preprocess_filename(PreContext * ctx, char ** result_buffer, char * filename);

int
preprocess_buffer(PreContext * ctx, char ** result_buffer, char * file_buffer, char * filename) {
    char file_dir [1024];
    char * filename_nodir;
    (void) filename_nodir;
    path_dir(file_dir, filename, &filename_nodir);

    char * s = file_buffer;
    char * chunk_begin = s;

    while (1) {
        char * start_of_line = s;
        struct {
            bool exists;
            bool is_quote;
            bool is_next;
            Token tk;
        } inc_file = {0};
        Token tk = pre_next_token(&s);

        if (*tk.start == '#') {
            Token directive = pre_next_token(&s);
            while (directive.type == TK_COMMENT) directive = pre_next_token(&s);
            if (directive.type != TK_IDENTIFIER) {
                goto unknown_directive;
            }

            if (is_digit(*directive.start) || tk_equal(&directive, "line")) {
                // TODO
            } else if (tk_equal(&directive, "include") || (tk_equal(&directive, "include_next") && (inc_file.is_next = true))) {
                Token next = pre_next_token(&s);
                while (next.type == TK_COMMENT) next = pre_next_token(&s);
                // expansion is deferred until after the last section gets pasted
                inc_file.exists = true;
                if (next.type == TK_STRING) {
                    inc_file.is_quote = true;
                    inc_file.tk = next;
                } else if (*next.start == '<') {
                    char * tk_end = find_closing(next.start);
                    if (!tk_end) {
                        preprocess_error(&next, "No closing '>'.");
                        return -1;
                    }
                    inc_file.tk.start = next.start;
                    inc_file.tk.length = tk_end - next.start + 1;
                } else {
                    preprocess_error(&next, "Unexpected token in include directive.");
                    return -1;
                }
            } else if (tk_equal(&directive, "define")) {
                char * ms = s;
                Token macro_name = pre_next_token(&ms);
                if (macro_name.type != TK_IDENTIFIER) {
                    preprocess_error(&macro_name, "Expected identifier.");
                    return -1;
                }

                if (tk_equal(&macro_name, "I")) {
                    goto nextline;
                }

                char ** arg_list = NULL;
                bool is_func_like = (*ms == '(');
                bool variadic = false;
                // TODO: errors here do not put the line correctly
                if (is_func_like) {
                    ms++;
                    while (1) {
                        Token tk = pre_next_token(&ms);
                        if (tk.type == TK_IDENTIFIER) {
                            char * arg = copy_and_terminate(tk.start, tk.length);
                            arrput(arg_list, arg);
                        } else if (*tk.start == ')') {
                            break;
                        } else if (memcmp(tk.start, "...", 3) == 0) {
                            variadic = true;
                            ms += 2;
                        } else {
                            preprocess_error(&tk, "Invalid symbol.");
                            return -1;
                        }

                        tk = pre_next_token(&ms);
                        if (*tk.start == ')') {
                            break;
                        } else if (*tk.start == ',') {
                            if (variadic) {
                                preprocess_error(&tk, "You can't do that, man.");
                                return -1;
                            }
                        } else {
                            preprocess_error(&tk, "Invalid symbol.");
                            return -1;
                        }
                    }
                }

                Token * replace_list = NULL;
                s = ms;
                while (1) {
                    Token tk = pre_next_token(&s);
                    if (tk.type == TK_NEWLINE || tk.type == TK_END) {
                        s = tk.start;
                        break;
                    }
                    if (tk.type != TK_COMMENT) {
                        arrput(replace_list, tk);
                    }
                }

                // create macro
                Define def = {0};
                def.key = copy_and_terminate(macro_name.start, macro_name.length);
                def.replace_list = replace_list;
                if (is_func_like) {
                    def.arg_list = arg_list;
                    def.arg_count = arrlen(arg_list);
                    def.func_like = true;
                    def.variadic = variadic;
                }
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
                pre_skip(&s, false);
            } else if (tk_equal(&directive, "endif")) {
            } else if (tk_equal(&directive, "error")) {
                preprocess_error(&directive, "User error");
                return -1;
            } else if (tk_equal(&directive, "pragma")) {
                tk = pre_next_token(&s);
                if (tk_equal(&tk, "once")) {
                    ctx->current_file_buffer->once = true;
                }
            } else {
            unknown_directive:
                preprocess_error(&directive, "Unknown directive.");
                return -1;
            }

        nextline:
            // find next line
            while (tk.type != TK_NEWLINE && tk.type != TK_END) tk = pre_next_token(&s);

            if (!ctx->minimal_parse) ignore_section(result_buffer, filename, file_buffer, &chunk_begin, start_of_line, s);

            if (inc_file.exists) {
                char * inc_filename = copy_and_terminate(inc_file.tk.start + 1, inc_file.tk.length - 2);
                char inc_filepath [1024];
                bool is_from_sys = ctx->is_sys_header;
                if (inc_file.is_quote) {
                    path_join(inc_filepath, file_dir, inc_filename);
                    if (access(inc_filepath, F_OK) == 0) {
                        goto include_matched_file;
                    }
                }
                for (int i=0; i < arrlen(include_paths); i++) {
                    const char * include_path = include_paths[i];
                    if (inc_file.is_next) {
                        if (0==strcmp(file_dir, include_path)) {
                            inc_file.is_next = false;
                        }
                        continue;
                    }
                    path_join(inc_filepath, include_path, inc_filename);
                    if (access(inc_filepath, F_OK) == 0) {
                        if (i >= ctx->sys_header_first || i <= ctx->sys_header_last) {
                            is_from_sys = true;
                        }
                        goto include_matched_file;
                    }
                }
                if (!ctx->m_options.G) {
                    preprocess_error(&inc_file.tk, "File not found.");
                    return -1;
                } else {
                    char * inc_filepath_stored = copy_and_terminate(inc_filename, strlen(inc_filename));
                    shputs(ctx->dependency_set, (NameSet){inc_filepath_stored});
                    continue;
                }

            include_matched_file: ;
                char * inc_filepath_stored = copy_and_terminate(inc_filepath, strlen(inc_filepath));
                if (!(ctx->m_options.no_sys && is_from_sys)) {
                    shputs(ctx->dependency_set, (NameSet){inc_filepath_stored});
                } else {
                    if (ctx->minimal_parse) {
                        continue;
                    }
                }
                bool prev = ctx->is_sys_header;
                ctx->is_sys_header = is_from_sys;
                int error = preprocess_filename(ctx, result_buffer, inc_filepath_stored);
                ctx->is_sys_header = prev;
                if (error) return -1;
                free(inc_filename);
            }
        } else if (tk.type == TK_END) {
            if (!ctx->minimal_parse) ignore_section(result_buffer, filename, file_buffer, &chunk_begin, s, NULL);
            break;
        } else {
            if (ctx->minimal_parse) {
                // find next line
                while (tk.type != TK_NEWLINE && tk.type != TK_END) tk = pre_next_token(&s);
            } else {
                if (tk.type == TK_COMMENT) {
                    ignore_section(result_buffer, filename, file_buffer, &chunk_begin, tk.start, s);
                } else if (tk.type == TK_IDENTIFIER) {
                    ctx->no_expand = NULL;
                    size_t count = SIZE_MAX;
                    Token * list = try_expand_macro(ctx, &tk, true, &count, &s);
                    if (count != SIZE_MAX) {
                        ignore_section(result_buffer, filename, file_buffer, &chunk_begin, tk.start, s);
                        if (list && count > 0) {
                            strput_tokens(result_buffer, list, count);
                            arrfree(list);
                        }
                    }
                    arrfree(ctx->no_expand);
                }
            }
        }
    }

    return 0;
}

int
preprocess_filename(PreContext * ctx, char ** result_buffer, char * filename) {
    char * file_buffer = NULL;
    size_t file_size;

    // search for buffer or create it if it doesn't exist
    for (int i=0; i < arrlen(file_buffers); i++) {
        FileBuffer * fb = file_buffers[i];
        if (strcmp(filename, fb->filename) == 0) {
            if (fb->once) {
                return 0;
            }
            file_buffer = fb->buffer;
            file_size = fb->buffer_size;
            ctx->current_file_buffer = fb;
            break;
        }
    }
    if (!file_buffer) {
        if (filename == filename_stdin) {
            file_buffer = read_stream(stdin);
            file_size = arrlen(file_buffer);
        } else {
            file_buffer = intro_read_file(filename, &file_size);
        }
        if (!file_buffer) {
            return ERR_FILE_NOT_FOUND;
        }
        FileBuffer * new_buf = malloc(sizeof(*new_buf));
        new_buf->filename = filename;
        new_buf->buffer = file_buffer;
        new_buf->buffer_size = file_size;
        arrput(file_buffers, new_buf);
        ctx->current_file_buffer = arrlast(file_buffers);
    }

    return preprocess_buffer(ctx, result_buffer, file_buffer, filename);
}

static char intro_defs [] =
"#define __INTRO__ 1\n"
"#define __attribute__(x) \n"
"#define __extension__ \n"
;

char *
run_preprocessor(int argc, char ** argv, char ** o_output_filepath) {
    sh_new_arena(defines);

    PreContext ctx_ = {0}, *ctx = &ctx_;

    *o_output_filepath = NULL;

    bool preprocess_only = false;
    bool no_sys = false;
    char * filepath = NULL;

    for (int i=1; i < argc; i++) {
        #define ADJACENT() ((strlen(arg) == 2)? argv[++i] : arg+2);
        char * arg = argv[i];
        if (arg[0] == '-') {
            switch(arg[1]) {
            case '-': {
                arg = argv[i] + 2;
                if (0==strcmp(arg, "no-sys")) {
                    no_sys = true;
                } else {
                    fprintf(stderr, "Unknown option: '%s'\n", arg);
                    exit(1);
                }
            }break;

            case 'D': {
                Define new_def;
                new_def.key = ADJACENT();
                shputs(defines, new_def);
            }break;

            case 'U': {
                const char * iden = ADJACENT();
                (void)shdel(defines, iden);
            }break;

            case 'I': {
                const char * new_path = ADJACENT();
                arrput(include_paths, new_path);
            }break;

            case 'E': {
                preprocess_only = true;
            }break;

            case 'o': {
                *o_output_filepath = argv[++i];
            }break;

            case 0: {
                if (isatty(fileno(stdin))) {
                    fprintf(stderr, "Error: Cannot use terminal as file input.\n");
                    exit(1);
                }
                filepath = filename_stdin;
            }break;

            case 'M': {
                switch(arg[2]) {
                case 0: {
                }break;

                case 'M': {
                    ctx->m_options.no_sys = true;
                    if (arg[3] == 'D') ctx->m_options.D = true;
                }break;

                case 'D': {
                    ctx->m_options.D = true;
                }break;

                case 'G': {
                    ctx->m_options.G = true;
                }break;

                case 'T': {
                    ctx->m_options.custom_target = argv[++i];
                }break;

                default: goto unknown_option;
                }

                ctx->m_options.enabled = true;
            }break;

            unknown_option: {
                fprintf(stderr, "Error: Unknown argumen '%s'\n", arg);
                exit(1);
            }break;
            }
        } else {
            if (filepath) {
                fprintf(stderr, "Error: More than 1 file passed.\n");
                exit(1);
            } else {
                filepath = arg;
            }
        }
        #undef ADJACENT
    }

    if (!no_sys) {
        ctx->minimal_parse = true;

        char program_dir [1024];
        char path [1024];
        strcpy(program_dir, argv[0]);
        path_normalize(program_dir);
        path_dir(program_dir, program_dir, NULL);

        // sys paths
        const char * sys_inc_path = ".intro_inc";
        path_join(path, program_dir, sys_inc_path);
        char * paths_buffer = intro_read_file(path, NULL);
        char * s = paths_buffer;
        if (paths_buffer) {
            ctx->sys_header_first = arrlen(include_paths);
            while (1) {
                char buf [1024];
                char * end = strchr(s, '\n');
                if (!end) break;
                *end = '\0';
                strcpy(buf, s);
                path_normalize(buf);
                char * stored_path = copy_and_terminate(buf, strlen(buf));
                arrput(include_paths, stored_path);
                s = end + 1;
                if (!*s) break;
            }
            ctx->sys_header_last = arrlen(include_paths) - 1;
            free(paths_buffer);
        } else {
            fprintf(stderr, "No file at %s\n", path);
        }

        // sys defines
        const char * sys_def_path = ".intro_def";
        path_join(path, program_dir, sys_def_path);
        char * def_buffer = intro_read_file(path, NULL);
        if (def_buffer) {
            preprocess_buffer(ctx, NULL, def_buffer, path);
        } else {
            fprintf(stderr, "No file at %s\n", path);
        }

        if (ctx->m_options.enabled && !ctx->m_options.D) {
            preprocess_only = true;
            ctx->minimal_parse = true;
        } else {
            preprocess_buffer(ctx, NULL, intro_defs, "__intro_defs__");
        }
        ctx->minimal_parse = false;
    }

    if (!filepath) {
        fputs("No filename given.\n", stderr);
        exit(1);
    }
    if (*o_output_filepath == NULL) {
        strputf(o_output_filepath, "%s.intro", filepath);
        strputnull(*o_output_filepath);
    }

    int error = preprocess_filename(ctx, &result_buffer, filepath);
    if (error) {
        if (error == ERR_FILE_NOT_FOUND) {
            fputs("File not found.\n", stderr);
        }
        return NULL;
    }

    strputnull(result_buffer);

    if (preprocess_only) {
        if (ctx->m_options.enabled) {
            char * rule = NULL;
            if (ctx->m_options.custom_target) {
                strputf(&rule, "%s:", ctx->m_options.custom_target);
            } else {
                char * ext = strrchr(filepath, '.');
                int len_basename = (ext)? ext - filepath : strlen(filepath);
                strputf(&rule, "%.*s.o:", len_basename, filepath);
            }
            strputf(&rule, " %s", filepath);
            for (int i=0; i < shlen(ctx->dependency_set); i++) {
                strputf(&rule, " %s", ctx->dependency_set[i].key);
            }
            arrput(rule, '\n');
            strputnull(rule);
            fputs(rule, stdout);
        } else {
            fputs(result_buffer, stdout);
        }
        exit(0);
    }

    return result_buffer;
}
