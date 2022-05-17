#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>

#include "global.h"
#include "lexer.c"

static char * filename_stdin = "__stdin__";

typedef struct {
    char * filename;
    char * buffer;
    size_t buffer_size;
    time_t mtime;
    bool once;
} FileBuffer;
static FileBuffer ** file_buffers = NULL;

typedef enum {
    MACRO_NOT_SPECIAL = 0,
    MACRO_defined,
    MACRO_FILE,
    MACRO_LINE,
    MACRO_DATE,
    MACRO_TIME,
    MACRO_COUNTER,
    MACRO_INCLUDE_LEVEL,
    MACRO_BASE_FILE,
    MACRO_FILE_NAME,
    MACRO_TIMESTAMP,
} SpecialMacro;

typedef struct {
    char * key;
    Token * replace_list;
    char ** arg_list;
    int32_t arg_count;
    SpecialMacro special;
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
    int * macro_index_stack;
    Token * list;
    char ** o_s;
} ExpandContext;

typedef struct {
    FileBuffer * current_file_buffer;
    struct {
        bool enabled;
        bool D;
        bool G;
        bool no_sys;
        char * custom_target;
        char * filename;
    } m_options;
    NameSet * dependency_set;
    ExprContext * expr_ctx;
    ExpandContext expand_ctx;

    const char * base_file;
    int counter;
    int include_level;

    int sys_header_first;
    int sys_header_last;
    bool is_sys_header;
    bool minimal_parse;
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

static const FileBuffer *
pre_find_file(const FileBuffer * current_file, const char * s) {
    const FileBuffer * file_info = current_file;
    if (s < file_info->buffer || s >= file_info->buffer + file_info->buffer_size) {
        for (int i=0; i < arrlen(file_buffers); i++) {
            char * fb_start = file_buffers[i]->buffer;
            char * fb_end = fb_start + file_buffers[i]->buffer_size;
            if (s >= fb_start && s < fb_end) {
                file_info = file_buffers[i];
                break;
            }
        }
    }
    return file_info;
}

static void
preprocess_message_internal(const FileBuffer * file_buffer, const Token * tk, char * message, int msg_type) {
    file_buffer = pre_find_file(file_buffer, tk->start);
    char * start_of_line;
    int line_num = count_newlines_in_range(file_buffer->buffer, tk->start, &start_of_line);
    message_internal(start_of_line, file_buffer->filename, line_num, tk->start, tk->start + tk->length, message, msg_type);
}

#define preprocess_error(tk, message)   preprocess_message_internal(ctx->current_file_buffer, tk, message, 0)
#define preprocess_warning(tk, message) preprocess_message_internal(ctx->current_file_buffer, tk, message, 1)

static Token
create_stringized(Token * list) {
    char * buf = NULL;
    arrput(buf, '"');
    for (int i=0; i < arrlen(list); i++) {
        Token tk = list[i];
        if (tk.preceding_space && i > 0) {
            arrput(buf, ' ');
        }
        if (tk.type == TK_STRING) {
            for (char * s = tk.start; s < tk.start + tk.length; s++) {
                if (*s == '"' || *s == '\\') {
                    arrput(buf, '\\');
                }
                arrput(buf, *s);
            }
            continue;
        }
        strputf(&buf, "%.*s", tk.length, tk.start);
    }
    arrput(buf, '"');
    strputnull(buf);

    Token result = {
        .start = buf, // TODO: leak
        .length = arrlen(buf) - 1,
        .type = TK_STRING,
    };
    return result;
}

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

static char *
path_extension(char * dest, const char * path) {
    char * forslash = strrchr(path, '/');
    char * period = strrchr(path, '.');
    if (!period || forslash > period) return NULL;

    return strcpy(dest, period);
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

static void
strput_tokens(char ** p_str, const Token * list, size_t count) {
    if (!list) return;
    for (int i=0; i < count; i++) {
        Token tk = list[i];
        if (tk.preceding_space) {
            arrput(*p_str, ' ');
        }
        strputf(p_str, "%.*s", tk.length, tk.start);
    }
}

void
tk_cat(Token ** p_list, const Token * ext, bool space) {
    Token * dest = arraddnptr(*p_list, arrlen(ext));
    for (int i=0; i < arrlen(ext); i++) {
        dest[i] = ext[i];
    }
    dest[0].preceding_space = space;
}

static bool macro_scan(PreContext * ctx, int macro_tk_index);

static Token
internal_macro_next_token(PreContext * ctx, int * o_index) {
    ++(*o_index);
    if (*o_index < arrlen(ctx->expand_ctx.list)) {
        return ctx->expand_ctx.list[*o_index];
    } else {
        Token tk;
        if (!ctx->expand_ctx.o_s) {
            tk = (Token){.type = TK_END};
            arrput(ctx->expand_ctx.list, tk);
            return tk;
        }
        do {
            tk = pre_next_token(ctx->expand_ctx.o_s);
        } while (tk.type == TK_COMMENT || tk.type == TK_NEWLINE);
        arrput(ctx->expand_ctx.list, tk);
        return tk;
    }
}

typedef struct {
    Token ** unexpanded_list;
    Token ** expanded_list;
    int count_tokens;
    bool complete;
} MacroArgs;

MacroArgs
get_macro_arguments(PreContext * ctx, int macro_tk_index) {
    MacroArgs result = {0};
    int i = macro_tk_index;
    char * prev_s = NULL;
    if (ctx->expand_ctx.o_s) prev_s = *ctx->expand_ctx.o_s;
    Token l_paren = internal_macro_next_token(ctx, &i);
    if (l_paren.type == TK_END || *l_paren.start != '(') {
        arrsetlen(ctx->expand_ctx.list, arrlen(ctx->expand_ctx.list) - 1);
        if (ctx->expand_ctx.o_s) *ctx->expand_ctx.o_s = prev_s;
        return result;
    }
    int paren_level = 1;
    int count_tokens = 1;
    Token ** arg_list = NULL;
    Token * arg_tks = NULL;
    while (1) {
        Token tk = internal_macro_next_token(ctx, &i);
        if (tk.type == TK_END) {
            preprocess_error(&l_paren, "No closing ')'.");
            exit(1);
        }
        count_tokens += 1;
        if (paren_level == 1 && *tk.start == ',') {
            arrput(arg_list, arg_tks);
            arg_tks = NULL;
            continue;
        }
        if (*tk.start == '(') {
            paren_level += 1;
        } else if (*tk.start == ')') {
            paren_level -= 1;
            if (paren_level == 0) {
                arrput(arg_list, arg_tks);
                break;
            }
        }
        arrput(arg_tks, tk);
    }
    arg_tks = NULL;

    Token ** unexpanded_list = NULL;
    arrsetcap(unexpanded_list, arrlen(arg_list));
    for (int arg_i=0; arg_i < arrlen(arg_list); arg_i++) {
        Token * tks = NULL;
        arraddnptr(tks, arrlen(arg_list[arg_i]));
        memcpy(tks, arg_list[arg_i], arrlen(arg_list[arg_i]) * sizeof(*arg_list[arg_i]));
        arrput(unexpanded_list, tks);
    }

    const ExpandContext prev_ctx = ctx->expand_ctx;
    for (int arg_i=0; arg_i < arrlen(arg_list); arg_i++) {
        for (int tk_i=0; tk_i < arrlen(arg_list[arg_i]); tk_i++) {
            if (arg_list[arg_i][tk_i].type == TK_IDENTIFIER) {
                ctx->expand_ctx = (ExpandContext){
                    .macro_index_stack = ctx->expand_ctx.macro_index_stack,
                    .list = arg_list[arg_i],
                    .o_s = NULL,
                };
                macro_scan(ctx, tk_i);
                arg_list[arg_i] = ctx->expand_ctx.list;
            }
        }
    }
    ctx->expand_ctx = prev_ctx;

    result.unexpanded_list = unexpanded_list;
    result.expanded_list = arg_list;
    result.count_tokens = count_tokens;
    result.complete = true;
    return result;
}

static void
free_macro_arguments(MacroArgs margs) {
    for (int i=0; i < arrlen(margs.unexpanded_list); i++) {
        arrfree(margs.unexpanded_list[i]);
    }
    arrfree(margs.unexpanded_list);
    for (int i=0; i < arrlen(margs.expanded_list); i++) {
        arrfree(margs.expanded_list[i]);
    }
    arrfree(margs.expanded_list);
}

static bool // true if token was expanded
macro_scan(PreContext * ctx, int macro_tk_index) {
    Token * macro_tk = &ctx->expand_ctx.list[macro_tk_index];
    STACK_TERMINATE(terminated_tk, macro_tk->start, macro_tk->length);

    ptrdiff_t macro_index = shgeti(defines, terminated_tk);
    if (macro_index < 0) {
        return false;
    }
    Define * macro = &defines[macro_index];
    if (macro->special != MACRO_NOT_SPECIAL) {
        char * buf = NULL; // TODO: leak
        int token_type = TK_STRING;
        switch(macro->special) {
        case MACRO_NOT_SPECIAL: break; // never reached

        case MACRO_defined: {
            int index = macro_tk_index;
            Token tk = internal_macro_next_token(ctx, &index);
            bool is_paren = false;
            if (tk.start && *tk.start == '(') {
                is_paren = true;
                tk = internal_macro_next_token(ctx, &index);
            }
            if (tk.type != TK_IDENTIFIER) {
                preprocess_error(&tk, "Expected identifier.");
                exit(1);
            }
            STACK_TERMINATE(defined_iden, tk.start, tk.length);
            char * replace = (shgeti(defines, defined_iden) >= 0)? "1" : "0";
            if (is_paren) {
                tk = internal_macro_next_token(ctx, &index);
                if (*tk.start != ')') {
                    preprocess_error(&tk, "Expected ')'.");
                    exit(1);
                }
            }
            Token replace_tk = (Token){
                .start = replace,
                .length = 1,
                .type = TK_IDENTIFIER,
                .preceding_space = macro_tk->preceding_space,
            };
            arrdeln(ctx->expand_ctx.list, macro_tk_index, 1 + (is_paren * 2));
            ctx->expand_ctx.list[macro_tk_index] = replace_tk;
            return true;
        }break;

        case MACRO_FILE: {
            strputf(&buf, "\"%s\"", ctx->current_file_buffer->filename);
        }break;

        case MACRO_LINE: {
            const FileBuffer * current_file = pre_find_file(ctx->current_file_buffer, macro_tk->start);
            char * start_of_line_;
            int line_num = count_newlines_in_range(current_file->buffer, macro_tk->start, &start_of_line_);
            strputf(&buf, "%i", line_num);
            token_type = TK_IDENTIFIER;
        }break;

        case MACRO_DATE: {
            char static_buf [12];
            time_t time_value;
            struct tm * date;

            time(&time_value);
            date = localtime(&time_value);
            strftime(static_buf, sizeof(static_buf), "%b %d %Y", date);

            strputf(&buf, "\"%s\"", static_buf);
        }break;

        case MACRO_TIME: {
            char static_buf [9];
            time_t time_value;
            struct tm * date;

            time(&time_value);
            date = localtime(&time_value);
            strftime(static_buf, sizeof(static_buf), "%H:%M:%S", date);

            strputf(&buf, "\"%s\"", static_buf);
        }break;

        case MACRO_COUNTER: {
            strputf(&buf, "%i", ctx->counter++);
            token_type = TK_IDENTIFIER;
        }break;

        case MACRO_INCLUDE_LEVEL: {
            strputf(&buf, "%i", ctx->include_level);
            token_type = TK_IDENTIFIER;
        }break;

        case MACRO_BASE_FILE: {
            strputf(&buf, "\"%s\"", ctx->base_file);
        }break;

        case MACRO_FILE_NAME: {
            char static_buf_ [1024];
            char * filename = NULL;
            path_dir(static_buf_, ctx->current_file_buffer->filename, &filename);
            strputf(&buf, "\"%s\"", filename);
        }break;

        case MACRO_TIMESTAMP: {
            char static_buf [256];
            struct tm * date;

            date = localtime(&ctx->current_file_buffer->mtime);
            strftime(static_buf, sizeof(static_buf), "%a %b %d %H:%M:%S %Y", date);

            strputf(&buf, "\"%s\"", static_buf);
        }break;
        }

        strputnull(buf);
        Token replace_tk = {
            .start = buf,
            .length = arrlen(buf) - 1,
            .type = token_type,
            .preceding_space = macro_tk->preceding_space,
        };
        ctx->expand_ctx.list[macro_tk_index] = replace_tk;
        return true;
    }
    for (int i=0; i < arrlen(ctx->expand_ctx.macro_index_stack); i++) {
        if (macro_index == ctx->expand_ctx.macro_index_stack[i]) {
            macro_tk->type = TK_DISABLED;
            return false;
        }
    }
    Token * replace_list = NULL;
    bool free_replace_list = false;
    int count_arg_tokens = 0;
    bool preceding_space = (macro_tk_index == 0)? false : macro_tk->preceding_space;

    macro_tk = NULL; // following code will make this invalid

    if (macro->func_like) {
        Token * list = NULL;
        MacroArgs margs = get_macro_arguments(ctx, macro_tk_index);
        if (!margs.complete) {
            return false;
        }
        count_arg_tokens = margs.count_tokens;
        
        Token ltk = {.start = ""};
        for (int tk_i=0; tk_i < arrlen(macro->replace_list); tk_i++) {
            Token tk = macro->replace_list[tk_i];
            bool replaced = false;
            if (tk.type == TK_IDENTIFIER) {
                if (macro->variadic && tk_equal(&tk, "__VA_ARGS__")) {
                    for (int arg_i=macro->arg_count; arg_i < arrlen(margs.expanded_list); arg_i++) {
                        tk_cat(&list, margs.expanded_list[arg_i], tk.preceding_space);
                        static const Token comma = {.start = ",", .length = 1};
                        if (arg_i != arrlen(margs.expanded_list) - 1) arrput(list, comma);
                    }
                    replaced = true;
                }
                for (int param_i=0; param_i < macro->arg_count; param_i++) {
                    if (tk_equal(&tk, macro->arg_list[param_i])) {
                        if (ltk.type == TK_HASH) {
                            Token stringized = create_stringized(margs.unexpanded_list[param_i]);
                            stringized.preceding_space = ltk.preceding_space;
                            arrput(list, stringized);
                        } else {
                            bool part_of_concat = ltk.type == TK_D_HASH || (tk_i+1 < arrlen(macro->replace_list) && macro->replace_list[tk_i+1].type == TK_D_HASH);
                            if (margs.unexpanded_list[param_i]) {
                                if (part_of_concat) {
                                    tk_cat(&list, margs.unexpanded_list[param_i], tk.preceding_space);
                                } else {
                                    tk_cat(&list, margs.expanded_list[param_i], tk.preceding_space);
                                }
                            } else {
                                if (part_of_concat) {
                                    arrput(list, (Token){.type = TK_PLACEHOLDER});
                                }
                            }
                        }
                        replaced = true;
                        break;
                    }
                }
            }
            if (!replaced && !(tk.type == TK_HASH)) {
                arrput(list, tk);
            }
            ltk = tk;
        }
        replace_list = list;
        free_replace_list = true;

        free_macro_arguments(margs);
    } else {
        replace_list = macro->replace_list;
    }

    // ## concat
    for (int i=0; i < arrlen(replace_list); i++) {
        Token tk = replace_list[i];
        if (tk.type == TK_D_HASH) {
            assert(i > 0 && i+1 < arrlen(replace_list));
            Token result = {0};
            Token last_tk = replace_list[i-1];
            Token next_tk = replace_list[i+1];
            if (last_tk.type == TK_PLACEHOLDER && next_tk.type == TK_PLACEHOLDER) {
                result.type = TK_PLACEHOLDER;
            } else {
                char * buf = NULL; // TODO: leak
                if (last_tk.type != TK_PLACEHOLDER) {
                    strputf(&buf, "%.*s", last_tk.length, last_tk.start);
                }
                if (next_tk.type != TK_PLACEHOLDER) {
                    strputf(&buf, "%.*s", next_tk.length, next_tk.start);
                }
                strputnull(buf);
                result.start = buf;
                result.length = arrlen(buf) - 1;
                result.preceding_space = last_tk.preceding_space;
                if (is_iden(result.start[0])) { // TODO: handle this more gracefully
                    result.type = TK_IDENTIFIER;
                }
            }
            i--;
            arrdeln(replace_list, i, MIN(2, arrlen(replace_list) - i));
            replace_list[i] = result;
        }
    }

    // insert list
    int diff = arrlen(replace_list) - count_arg_tokens - 1;
    if (diff > 0) {
        arrinsn(ctx->expand_ctx.list, macro_tk_index, diff);
    } else {
        arrdeln(ctx->expand_ctx.list, macro_tk_index, -diff);
    }
    for (int i=0; i < arrlen(replace_list); i++) {
        ctx->expand_ctx.list[macro_tk_index + i] = replace_list[i];
    }

    // rescan
    int back_offset = macro_tk_index + arrlen(replace_list) - arrlen(ctx->expand_ctx.list);
    arrpush(ctx->expand_ctx.macro_index_stack, macro_index);
    for (int i=macro_tk_index; i < arrlen(ctx->expand_ctx.list) + back_offset; i++) {
        if (ctx->expand_ctx.list[i].type == TK_IDENTIFIER) {
            macro_scan(ctx, i);
        }
    }
    (void)arrpop(ctx->expand_ctx.macro_index_stack);

    ctx->expand_ctx.list[macro_tk_index].preceding_space = preceding_space;

    if (free_replace_list) arrfree(replace_list);
    return true;
}

char *
expand_line(PreContext * ctx, char ** o_s, bool is_include) {
    Token * ptks = NULL;
    arrsetcap(ptks, 64);
    ExpandContext prev_ctx = ctx->expand_ctx;
    char * processed = NULL;
    while (1) {
        Token ptk = pre_next_token(o_s);
        if (ptk.type == TK_NEWLINE) {
            *o_s = ptk.start;
            break;
        } else if (ptk.type == TK_COMMENT) {
            continue;
        } else if (is_include && *ptk.start == '<') {
            char * closing = find_closing(ptk.start);
            if (!closing) {
                preprocess_error(&ptk, "No closing '>'.");
                exit(1);
            }
            strputf(&processed, " %.*s", (int)(closing - ptk.start + 1), ptk.start);
            *o_s = closing + 1;
        } else {
            arrsetlen(ptks, 1);
            ptks[0] = ptk;
            if (ptk.type == TK_IDENTIFIER) {
                ctx->expand_ctx = (ExpandContext){
                    .macro_index_stack = NULL,
                    .list = ptks,
                    .o_s = o_s,
                };
                ptks = ctx->expand_ctx.list;
                macro_scan(ctx, 0);
            }
            strput_tokens(&processed, ptks, arrlen(ptks));
        }
    }
    ctx->expand_ctx = prev_ctx;
    strputnull(processed);
    arrfree(ptks);

    return processed;
}

bool
parse_expression(PreContext * ctx, char ** o_s) {
    char * processed = expand_line(ctx, o_s, false);
    char * s = processed;
    Token * tks = NULL;
    while (1) {
        Token tk = next_token(&s);
        if (tk.type == TK_END) break;
        arrput(tks, tk);
    }

    Token err_tk = {0};
    ExprNode * tree = build_expression_tree(ctx->expr_ctx, tks, arrlen(tks), &err_tk);
    if (!tree && err_tk.start) {
        preprocess_error(&err_tk, "Invalid symbol in expression.");
        exit(1);
    }
    ExprProcedure * expr = build_expression_procedure(tree);
    intmax_t result = run_expression(expr);

    free(expr);
    reset_arena(ctx->expr_ctx->arena);
    arrfree(processed);

    return !!result;
}

static void
pre_skip(PreContext * ctx, char ** o_s, bool elif_ok) {
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
                        bool expr_result = parse_expression(ctx, o_s);
                        if (expr_result) {
                            return;
                        }
                    }
                }
                goto nextline;
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

    arrsetcap(ctx->expand_ctx.list, 64);
    arrsetlen(ctx->expand_ctx.list, 1);

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
                char * expanded_line = expand_line(ctx, &s, true); // TODO: leak
                char * is = expanded_line;
                Token next = pre_next_token(&is);
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
                        while (tk.type == TK_COMMENT) tk = pre_next_token(&ms);
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
                        while (tk.type == TK_COMMENT) tk = pre_next_token(&ms);
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
                        if (replace_list == NULL) tk.preceding_space = false;
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
                STACK_TERMINATE(iden, def.start, def.length);
                (void) shdel(defines, iden);
            } else if (tk_equal(&directive, "if")) {
                bool expr_result = parse_expression(ctx, &s);
                if (!expr_result) {
                    pre_skip(ctx, &s, true);
                }
            } else if (tk_equal(&directive, "ifdef")) {
                tk = pre_next_token(&s);
                STACK_TERMINATE(name, tk.start, tk.length);
                int def_index = shgeti(defines, name);
                if (def_index < 0) {
                    pre_skip(ctx, &s, true);
                }
            } else if (tk_equal(&directive, "ifndef")) {
                tk = pre_next_token(&s);
                STACK_TERMINATE(name, tk.start, tk.length);
                int def_index = shgeti(defines, name);
                if (def_index >= 0) {
                    pre_skip(ctx, &s, true);
                }
            } else if (tk_equal(&directive, "elif")) {
                pre_skip(ctx, &s, false);
            } else if (tk_equal(&directive, "else")) {
                pre_skip(ctx, &s, false);
            } else if (tk_equal(&directive, "endif")) {
            } else if (tk_equal(&directive, "error")) {
                preprocess_error(&directive, "User error.");
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
                STACK_TERMINATE(inc_filename, inc_file.tk.start + 1, inc_file.tk.length - 2);
                char ext_buf [128];
                if (0==strcmp(".intro", path_extension(ext_buf, inc_filename))) {
                    goto skip_and_add_include_dep;
                }
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
                if (ctx->m_options.G) {
                skip_and_add_include_dep: ;
                    char * inc_filepath_stored = copy_and_terminate(inc_filename, strlen(inc_filename));
                    shputs(ctx->dependency_set, (NameSet){inc_filepath_stored});
                    continue;
                } else {
                    preprocess_error(&inc_file.tk, "File not found.");
                    return -1;
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
                ctx->include_level++;

                int error = preprocess_filename(ctx, result_buffer, inc_filepath_stored);

                ctx->is_sys_header = prev;
                ctx->include_level--;

                if (error) return -1;
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
                    ctx->expand_ctx.o_s = &s;
                    ctx->expand_ctx.list[0] = tk;
                    if (macro_scan(ctx, 0)) {
                        const Token * list = ctx->expand_ctx.list;
                        ignore_section(result_buffer, filename, file_buffer, &chunk_begin, tk.start, s);
                        if (arrlen(list) > 0) {
                            strput_tokens(result_buffer, list, arrlen(list));
                        }
                        arrsetlen(ctx->expand_ctx.list, 1);
                        arrsetlen(ctx->expand_ctx.macro_index_stack, 0);
                    }
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
        time_t mtime;
        if (filename == filename_stdin) {
            file_buffer = read_stream(stdin);
            file_size = arrlen(file_buffer);
            time(&mtime);
        } else {
            file_buffer = intro_read_file(filename, &file_size);
            struct stat file_stat;
            stat(filename, &file_stat);
            mtime = file_stat.st_mtime;
        }
        if (!file_buffer) {
            return RET_FILE_NOT_FOUND;
        }
        FileBuffer * new_buf = calloc(1, sizeof(*new_buf));
        new_buf->filename = filename;
        new_buf->buffer = file_buffer;
        new_buf->buffer_size = file_size;
        new_buf->mtime = mtime;
        arrput(file_buffers, new_buf);
        ctx->current_file_buffer = arrlast(file_buffers);
    }

    if (ctx->include_level == 0) {
        ctx->base_file = filename;
    }

    return preprocess_buffer(ctx, result_buffer, file_buffer, filename);
}

static char intro_defs [] =
"#define __INTRO__ 1\n"
"#undef __GNUC__\n"
;

char *
run_preprocessor(int argc, char ** argv, char ** o_output_filepath) {
    sh_new_arena(defines);

    // init pre context
    PreContext ctx_ = {0}, *ctx = &ctx_;
    ctx->expr_ctx = calloc(1, sizeof(*ctx->expr_ctx));
    ctx->expr_ctx->mode = MODE_PRE;
    ctx->expr_ctx->arena = new_arena();

    static const struct{char * name; SpecialMacro value;} special_macros [] = {
        {"defined", MACRO_defined},
        {"__FILE__", MACRO_FILE},
        {"__LINE__", MACRO_LINE},
        {"__DATE__", MACRO_DATE},
        {"__TIME__", MACRO_TIME},
        {"__COUNTER__", MACRO_COUNTER},
        {"__INCLUDE_LEVEL__", MACRO_INCLUDE_LEVEL},
        {"__BASE_FILE__", MACRO_BASE_FILE},
        {"__FILE_NAME__", MACRO_FILE_NAME},
        {"__TIMESTAMP__", MACRO_TIMESTAMP},
    };
    for (int i=0; i < LENGTH(special_macros); i++) {
        Define special = {0};
        special.key = special_macros[i].name;
        special.special = special_macros[i].value;
        shputs(defines, special);
    }

    *o_output_filepath = NULL;

    bool preprocess_only = false;
    bool no_sys = false;
    char * filepath = NULL;

    for (int i=1; i < argc; i++) {
        #define ADJACENT() ((strlen(arg) == 2)? argv[++i] : arg+2)
        char * arg = argv[i];
        if (arg[0] == '-') {
            switch(arg[1]) {
            case '-': {
                arg = argv[i] + 2;
                if (0==strcmp(arg, "no-sys")) {
                    no_sys = true;
                } else if (0==strcmp(arg, "expr-test")) {
                    expr_test();
                    exit(0);
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

                case 'F' :{
                    ctx->m_options.filename = argv[++i];
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
                if (*(end - 1) == '\r') *(end - 1) = '\0';
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
        if (error == RET_FILE_NOT_FOUND) {
            fputs("File not found.\n", stderr);
        }
        return NULL;
    }

    strputnull(result_buffer);

    if (ctx->m_options.enabled) {
        char * ext = strrchr(filepath, '.');
        int len_basename = (ext)? ext - filepath : strlen(filepath);
        if (ctx->m_options.D && !ctx->m_options.filename) {
            char * dep_file = NULL;
            strputf(&dep_file, "%.*s.d", len_basename, filepath);
            ctx->m_options.filename = dep_file;
        }

        char * rule = NULL;
        if (ctx->m_options.custom_target) {
            strputf(&rule, "%s:", ctx->m_options.custom_target);
        } else {
            strputf(&rule, "%.*s.o:", len_basename, filepath);
        }
        strputf(&rule, " %s", filepath);
        for (int i=0; i < shlen(ctx->dependency_set); i++) {
            strputf(&rule, " %s", ctx->dependency_set[i].key);
        }
        arrput(rule, '\n');
        strputnull(rule);

        if (preprocess_only && !ctx->m_options.filename) {
            fputs(rule, stdout);
            exit(0);
        }

        assert(ctx->m_options.filename != NULL
               /* I don't know how you did this, but the program can't figure out where you want the dependencies. */);
        int error = intro_dump_file(ctx->m_options.filename, rule, arrlen(rule) - 1);
        if (error < 0) {
            fprintf(stderr, "Failed to write dependencies to '%s'\n", ctx->m_options.filename);
            exit(1);
        }
    }
    if (preprocess_only) {
        fputs(result_buffer, stdout);
        exit(0);
    }

    return result_buffer;
}
