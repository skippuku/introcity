#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>

#include "global.h"
#include "lexer.c"

static const char * filename_stdin = "__stdin__";

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
    bool forced;
} Define;

typedef struct {
    int * macro_index_stack;
    Token * list;
    char ** o_s;
    char * last_macro_name;
} ExpandContext;

enum TargetMode {
    MT_NORMAL = 0,
    MT_SPACE,
    MT_NEWLINE,
};

typedef struct {
    char * result_buffer;
    FileInfo * current_file;
    Define * defines;
    MemArena * arena;
    struct {
        char * custom_target;
        char * filename;
        bool enabled;
        bool D;
        bool G;
        bool P;
        bool no_sys;
        bool use_msys_path;
        int8_t target_mode;
    } m_options;
    NameSet * dependency_set;
    ExprContext * expr_ctx;
    ExpandContext expand_ctx;
    LocationContext loc;

    const char * base_file;
    int counter;
    int include_level;

    int sys_header_first;
    int sys_header_last;
    bool is_sys_header;
    bool minimal_parse;
} PreContext;

typedef struct {
    PreContext * ctx;
    FileInfo * chunk_file;
    char * begin_chunk;
} PasteState;

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
    strputf(p_s, "%.*s\n", (int)(segment_end - highlight_end), highlight_end);
    for (int i=0; i < highlight_start - segment_start; i++) arrput(*p_s, ' ');
    for (int i=0; i < highlight_end - highlight_start; i++) arrput(*p_s, '~');
    strputf(p_s, "\n");
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

FileInfo *
get_line(LocationContext * lctx, char * buffer_begin, char ** o_pos, int * o_line, char ** o_start_of_line) {
    FileLoc pos_loc;
    int loc_index = -1;
    int max = (lctx->count)? lctx->count : arrlen(lctx->list);
    for (int i=lctx->index; i < max; i++) {
        FileLoc loc = lctx->list[i];
        ptrdiff_t offset = *o_pos - buffer_begin;
        if (offset < loc.offset) {
            lctx->index = i;
            loc_index = i-1;
            pos_loc = lctx->list[loc_index];
            break;
        } else {
            if (loc.file) lctx->file = loc.file;
            switch(loc.mode) {
            case LOC_NONE:
                break;
            case LOC_FILE:
            case LOC_MACRO:
                arrput(lctx->stack, i);
                break;
            case LOC_POP:
                assert(arrlen(lctx->stack) > 0);
                (void)arrpop(lctx->stack);
                int last_stack = lctx->stack[arrlen(lctx->stack) - 1];
                lctx->file = lctx->list[last_stack].file;
                continue;
            }
        }
    }
    if (loc_index < 0) return NULL;
    char * file_buffer = lctx->file->buffer;
    if (!file_buffer) {
        fprintf(stderr, "Internal error: failed to find file for error report.");
        exit(-1);
    }
    *o_pos = file_buffer + (pos_loc.file_offset + ((*o_pos - buffer_begin) - pos_loc.offset));
    assert(*o_pos < file_buffer + lctx->file->buffer_size);

    char * last_line;
    *o_line = count_newlines_in_range(file_buffer, *o_pos, &last_line);
    *o_start_of_line = last_line;
    return lctx->file;
}

static FileInfo *
find_origin(LocationContext * lctx, FileInfo * current, char * ptr) {
    int i = arrlen(lctx->file_buffers);
    FileInfo * file = (current)? current : lctx->file_buffers[--i];
    while (1) {
        if (ptr >= file->buffer && ptr < (file->buffer + file->buffer_size)) {
            return file;
        }

        if (--i >= 0) {
            file = lctx->file_buffers[i];
        } else {
            break;
        }
    }

    return NULL;
}

static void
message_internal(char * start_of_line, char * filename, int line, char * hl_start, char * hl_end, char * message, int message_type) {
    char * end_of_line = strchr(hl_end, '\n');
    if (!end_of_line) {
        end_of_line = hl_end;
    }
    char * s = NULL;
    const char * message_type_string = (message_type == 1)? "Warning" : "Error";
    const char * color = (message_type == 1)? BOLD_YELLOW : BOLD_RED;
    strputf(&s, "%s%s" WHITE " (" CYAN "%s:" BOLD_WHITE "%i" WHITE "): %s\n\n", color, message_type_string, filename, line, message);
    strput_code_segment(&s, start_of_line, end_of_line, hl_start, hl_end, color);
    fputs(s, stderr);
    arrfree(s);
}

void
parse_msg_internal(LocationContext * lctx, char * buffer, const Token * tk, char * message, int message_type) {
    char * start_of_line = NULL;
    char * filename = "?";
    char * hl_start = tk->start;
    int line_num = -1;
    FileInfo * file = get_line(lctx, buffer, &hl_start, &line_num, &start_of_line);
    filename = file->filename;
    assert(start_of_line != NULL);
    char * hl_end = hl_start + tk->length;
    message_internal(start_of_line, filename, line_num, hl_start, hl_end, message, message_type);
    for (int i=arrlen(lctx->stack) - 1; i >= 0; i--) {
        FileLoc l = lctx->list[lctx->stack[i]];
        if (l.mode == LOC_FILE) {
            fprintf(stderr, "In file: '%s'\n", l.file->filename);
        } else if (l.mode == LOC_MACRO) {
            hl_start = l.file->buffer + l.file_offset;
            hl_end = hl_start + strlen(l.macro_name);
            line_num = count_newlines_in_range(l.file->buffer, hl_start, &start_of_line);
            message_internal(start_of_line, filename, line_num, hl_start, hl_end, "In expansion", message_type);
        }
    }
}

static void
preprocess_message_internal(LocationContext * lctx, const Token * tk, char * message, int msg_type) {
    FileInfo * file = find_origin(lctx, lctx->file, tk->start);
    int line_num = 0;
    char * start_of_line = tk->start;
    char * filename = "__GENERATED__";
    if (file) {
        line_num = count_newlines_in_range(file->buffer, tk->start, &start_of_line);
        filename = file->filename;
    }
    message_internal(start_of_line, filename, line_num, tk->start, tk->start + tk->length, message, msg_type);
}

#define preprocess_error(tk, message)   preprocess_message_internal(&ctx->loc, tk, message, 0)
#define preprocess_warning(tk, message) preprocess_message_internal(&ctx->loc, tk, message, 1)

static Token
create_stringized(PreContext * ctx, Token * list) {
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
    strputf(&buf, "\"");

    char * str = copy_and_terminate(ctx->arena, buf, arrlen(buf));

    Token result = {
        .start = str,
        .length = arrlen(buf),
        .type = TK_STRING,
    };
    arrfree(buf);
    return result;
}

static void
ignore_section(PasteState * state, char * begin_ignored, char * end_ignored) {
    // save chunk location
    PreContext * ctx = state->ctx;

    if (state->begin_chunk != NULL) {
        ptrdiff_t signed_file_offset = state->begin_chunk - state->chunk_file->buffer;
        db_assert(signed_file_offset >= 0);
        FileLoc loc = {
            .offset      = arrlen(ctx->result_buffer),
            .file_offset = (size_t)signed_file_offset,
        };
        FileLoc * plast = &arrlast(ctx->loc.list);
        if ((loc.offset == plast->offset) && (plast->mode == LOC_NONE)) {
            *plast = loc;
        } else {
            arrput(ctx->loc.list, loc);
        }
        // paste chunk
        int size_chunk = begin_ignored - state->begin_chunk;
        if (size_chunk > 0) {
            char * out = arraddnptr(ctx->result_buffer, size_chunk);
            memcpy(out, state->begin_chunk, size_chunk);
        }
    }
    state->begin_chunk = end_ignored;
    state->chunk_file = ctx->current_file;
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
    int prev_len_list = arrlen(ctx->expand_ctx.list);
    Token l_paren = internal_macro_next_token(ctx, &i);
    if (l_paren.type == TK_END || *l_paren.start != '(') {
        arrsetlen(ctx->expand_ctx.list, prev_len_list);
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
        if (arrlen(arg_list[arg_i]) > 0) {
            arraddnptr(tks, arrlen(arg_list[arg_i]));
            memcpy(tks, arg_list[arg_i], arrlen(arg_list[arg_i]) * sizeof(*arg_list[arg_i]));
        }
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

    ptrdiff_t macro_index = shgeti(ctx->defines, terminated_tk);
    if (macro_index < 0) {
        return false;
    }
    Define * macro = &ctx->defines[macro_index];
    if (macro->special != MACRO_NOT_SPECIAL) {
        char * buf = NULL;
        int token_type = TK_STRING;
        switch(macro->special) {
        case MACRO_NOT_SPECIAL: break; // never reached

        case MACRO_defined: {
            bool preceding_space = macro_tk->preceding_space;
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
            char * replace = (shgeti(ctx->defines, defined_iden) >= 0)? "1" : "0";
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
                .preceding_space = preceding_space,
            };
            arrdeln(ctx->expand_ctx.list, macro_tk_index, 1 + (is_paren * 2));
            ctx->expand_ctx.list[macro_tk_index] = replace_tk;
            return true;
        }break;

        case MACRO_FILE: {
            strputf(&buf, "\"%s\"", ctx->current_file->filename);
        }break;

        case MACRO_LINE: {
            const FileInfo * current_file = find_origin(&ctx->loc, ctx->loc.file, macro_tk->start); // TODO: use parent macro if there is one
            int line_num = 0;
            if (current_file) {
                char * start_of_line_;
                line_num = count_newlines_in_range(current_file->buffer, macro_tk->start, &start_of_line_);
            }
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
            path_dir(static_buf_, ctx->current_file->filename, &filename);
            strputf(&buf, "\"%s\"", filename);
        }break;

        case MACRO_TIMESTAMP: {
            char static_buf [256];
            struct tm * date;

            date = localtime(&ctx->current_file->mtime);
            strftime(static_buf, sizeof(static_buf), "%a %b %d %H:%M:%S %Y", date);

            strputf(&buf, "\"%s\"", static_buf);
        }break;
        }

        Token replace_tk = {
            .start = copy_and_terminate(ctx->arena, buf, arrlen(buf)),
            .length = arrlen(buf),
            .type = token_type,
            .preceding_space = macro_tk->preceding_space,
        };
        arrfree(buf);
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
                            Token stringized = create_stringized(ctx, margs.unexpanded_list[param_i]);
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
            if (!(i > 0 && i+1 < arrlen(replace_list))) {
                preprocess_error(&tk, "Invalid concat operator.");
                exit(1);
            }
            Token result = {0};
            Token last_tk = replace_list[i-1];
            Token next_tk = replace_list[i+1];
            if (last_tk.type == TK_PLACEHOLDER && next_tk.type == TK_PLACEHOLDER) {
                result.type = TK_PLACEHOLDER;
            } else {
                char * buf = NULL;
                if (last_tk.type != TK_PLACEHOLDER) {
                    strputf(&buf, "%.*s", last_tk.length, last_tk.start);
                }
                if (next_tk.type != TK_PLACEHOLDER) {
                    strputf(&buf, "%.*s", next_tk.length, next_tk.start);
                }
                result.start = copy_and_terminate(ctx->arena, buf, arrlen(buf));
                result.length = arrlen(buf);
                result.preceding_space = last_tk.preceding_space;
                if (is_iden(result.start[0])) { // TODO: handle this more gracefully
                    result.type = TK_IDENTIFIER;
                }
                arrfree(buf);
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
            if (macro_scan(ctx, i)) {
                i--;
            }
        }
    }
    (void)arrpop(ctx->expand_ctx.macro_index_stack);

    ctx->expand_ctx.list[macro_tk_index].preceding_space = preceding_space;
    ctx->expand_ctx.last_macro_name = macro->key;

    if (free_replace_list) arrfree(replace_list);
    return true;
}

Token *
expand_line(PreContext * ctx, char ** o_s, bool is_include) {
    Token * ptks = NULL;
    arrsetcap(ptks, 16);
    ExpandContext prev_ctx = ctx->expand_ctx;
    while (1) {
        Token ptk = pre_next_token(o_s);
        if (ptk.type == TK_NEWLINE) {
            *o_s = ptk.start;
            ptk.type = TK_END;
            ptk.start -= 1; // highlight last character in errors
            arrput(ptks, ptk);
            break;
        } else if (ptk.type == TK_COMMENT) {
            continue;
        } else if (is_include && *ptk.start == '<') {
            char * closing = find_closing(ptk.start);
            if (!closing) {
                preprocess_error(&ptk, "No closing '>'.");
                exit(1);
            }
            *o_s = closing + 1;
            ptk.length = *o_s - ptk.start;
            ptk.type = TK_STRING;
            arrput(ptks, ptk);
        } else {
            int index = arrlen(ptks);
            arrput(ptks, ptk);
            if (ptk.type == TK_IDENTIFIER) {
                ctx->expand_ctx = (ExpandContext){
                    .macro_index_stack = NULL,
                    .list = ptks,
                    .o_s = o_s,
                };
                macro_scan(ctx, index);
                ptks = ctx->expand_ctx.list;
            }
        }
    }
    ctx->expand_ctx = prev_ctx;

    return ptks;
}

bool
parse_expression(PreContext * ctx, char ** o_s) {
    Token * tks = expand_line(ctx, o_s, false);
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
    arrfree(tks);

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

int preprocess_filename(PreContext * ctx, char * filename);

int
preprocess_buffer(PreContext * ctx) {
    FileInfo * file = ctx->current_file;
    char * file_buffer = file->buffer;
    char * filename = file->filename;

    char file_dir [1024];
    char * filename_nodir;
    (void) filename_nodir;
    path_dir(file_dir, filename, &filename_nodir);

    char * s = file_buffer;

    arrsetcap(ctx->expand_ctx.list, 64);
    arrsetlen(ctx->expand_ctx.list, 1);

    PasteState paste_ = {
        .ctx = ctx,
        .chunk_file = ctx->current_file,
        .begin_chunk = s,
    }, *paste = &paste_;

    FileLoc loc_push_file = {
        .offset = arrlen(ctx->result_buffer),
        .file_offset = 0,
        .file = file,
        .mode = LOC_FILE,
    };
    arrput(ctx->loc.list, loc_push_file);

    while (1) {
        char * start_of_line = s;
        struct {
            bool exists;
            bool is_quote;
            bool is_next;
            Token tk;
        } inc_file = {0};
        Token tk = pre_next_token(&s);
        bool def_forced = false;

        if (*tk.start == '#') {
            Token directive = pre_next_token(&s);
            while (directive.type == TK_COMMENT) directive = pre_next_token(&s);
            if (directive.type != TK_IDENTIFIER) {
                goto unknown_directive;
            }

            if (is_digit(*directive.start) || tk_equal(&directive, "line")) {
                // TODO
            } else if (tk_equal(&directive, "include") || (tk_equal(&directive, "include_next") && (inc_file.is_next = true))) {
                Token * expanded = expand_line(ctx, &s, true);
                Token next = expanded[0];
                // inclusion is deferred until after the last section gets pasted
                inc_file.exists = true;
                if (next.type == TK_STRING) {
                    inc_file.is_quote = (*next.start == '"');
                    inc_file.tk = next;
                } else {
                    preprocess_error(&next, "Unexpected token in include directive.");
                    return -1;
                }
                arrfree(expanded);
            } else if (tk_equal(&directive, "define") || (tk_equal(&directive, "define_forced") && (def_forced = true))) {
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
                            char * arg = copy_and_terminate(ctx->arena, tk.start, tk.length);
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
                def.key = copy_and_terminate(ctx->arena, macro_name.start, macro_name.length);
                def.replace_list = replace_list;
                def.forced = def_forced;
                if (is_func_like) {
                    def.arg_list = arg_list;
                    def.arg_count = arrlen(arg_list);
                    def.func_like = true;
                    def.variadic = variadic;
                }
                Define * prevdef = shgetp_null(ctx->defines, def.key);
                if (prevdef) {
                    if (prevdef->forced) {
                        preprocess_warning(&macro_name, "Attempted consesquent #define of forced define.");
                        goto nextline;
                    } else {
                        //preprocess_warning(&macro_name, "Macro redefinition.");
                        // NOTE: system headers would cause this to trigger a lot, though redefinitions
                        //       are against the C standard. So something is amok.
                    }
                }
                shputs(ctx->defines, def);
            } else if (tk_equal(&directive, "undef")) {
                Token def = pre_next_token(&s);
                STACK_TERMINATE(iden, def.start, def.length);
                Define * macro = shgetp(ctx->defines, iden);
                if (!macro->forced) {
                    (void) shdel(ctx->defines, iden);
                } else {
                    preprocess_warning(&def, "Attempted #undef of forced define.");
                }
            } else if (tk_equal(&directive, "if")) {
                bool expr_result = parse_expression(ctx, &s);
                if (!expr_result) {
                    pre_skip(ctx, &s, true);
                }
            } else if (tk_equal(&directive, "ifdef")) {
                tk = pre_next_token(&s);
                STACK_TERMINATE(name, tk.start, tk.length);
                int def_index = shgeti(ctx->defines, name);
                if (def_index < 0) {
                    pre_skip(ctx, &s, true);
                }
            } else if (tk_equal(&directive, "ifndef")) {
                tk = pre_next_token(&s);
                STACK_TERMINATE(name, tk.start, tk.length);
                int def_index = shgeti(ctx->defines, name);
                if (def_index >= 0) {
                    pre_skip(ctx, &s, true);
                }
            } else if (tk_equal(&directive, "elif")) {
                pre_skip(ctx, &s, false);
            } else if (tk_equal(&directive, "else")) {
                pre_skip(ctx, &s, false);
            } else if (tk_equal(&directive, "endif")) {
            } else if (tk_equal(&directive, "error")) {
                preprocess_error(&directive, "error directive.");
                return -1;
            } else if (tk_equal(&directive, "pragma")) {
                tk = pre_next_token(&s);
                if (tk_equal(&tk, "once")) {
                    ctx->current_file->once = true;
                }
            } else {
            unknown_directive:
                preprocess_error(&directive, "Unknown directive.");
                return -1;
            }

        nextline:
            // find next line
            while (tk.type != TK_NEWLINE && tk.type != TK_END) tk = pre_next_token(&s);

            if (!ctx->minimal_parse) ignore_section(paste, start_of_line, s);

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
                    char * inc_filepath_stored = copy_and_terminate(ctx->arena, inc_filename, strlen(inc_filename));
                    shputs(ctx->dependency_set, (NameSet){inc_filepath_stored});
                    continue;
                } else {
                    preprocess_error(&inc_file.tk, "File not found.");
                    return -1;
                }

            include_matched_file: ;
                char * inc_filepath_stored = copy_and_terminate(ctx->arena, inc_filepath, strlen(inc_filepath));
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

                int error = preprocess_filename(ctx, inc_filepath_stored);

                ctx->is_sys_header = prev;
                ctx->include_level--;
                ctx->current_file = file;

                if (error == RET_FILE_NOT_FOUND) {
                    preprocess_error(&inc_file.tk, "Failed to read file.");
                    return -1;
                }
                if (error < 0) return -1;
            }
        } else if (tk.type == TK_END) {
            if (!ctx->minimal_parse) ignore_section(paste, s, NULL);
            FileLoc pop_file = {
                .offset = arrlen(ctx->result_buffer),
                .mode = LOC_POP,
            };
            if (arrlast(ctx->loc.list).mode == LOC_FILE) {
                (void)arrpop(ctx->loc.list);
            } else {
                arrlast(ctx->loc.list) = pop_file;
            }
            break;
        } else {
            if (ctx->minimal_parse) {
                // find next line
                while (tk.type != TK_NEWLINE && tk.type != TK_END) tk = pre_next_token(&s);
            } else {
                if (tk.type == TK_COMMENT) {
                    ignore_section(paste, tk.start, s);
                } else if (tk.type == TK_IDENTIFIER) {
                    ctx->expand_ctx.o_s = &s;
                    ctx->expand_ctx.list[0] = tk;
                    if (macro_scan(ctx, 0)) {
                        const Token * list = ctx->expand_ctx.list;
                        ignore_section(paste, tk.start, s);

                        FileLoc loc_push_macro = {
                            .offset = arrlen(ctx->result_buffer),
                            .file_offset = tk.start - file_buffer,
                            .file = file,
                            .macro_name = ctx->expand_ctx.last_macro_name,
                            .mode = LOC_MACRO,
                        };
                        arrput(ctx->loc.list, loc_push_macro);

                        if (arrlen(list) > 0) {
                            FileInfo * last_origin = ctx->current_file;
                            ptrdiff_t last_file_offset = -1;
                            for (int i=0; i < arrlen(list); i++) {
                                Token mtk = list[i];
                                FileInfo * origin = find_origin(&ctx->loc, ctx->loc.file, mtk.start);
                                if (origin) {
                                    ptrdiff_t file_offset = mtk.start - origin->buffer;
                                    if (file_offset != last_file_offset || origin != last_origin) {
                                        FileLoc loc = {
                                            .offset = arrlen(ctx->result_buffer) + 1*mtk.preceding_space,
                                            .file_offset = file_offset,
                                        };
                                        if (origin) loc.file = origin;
                                        arrput(ctx->loc.list, loc);
                                        last_origin = origin;
                                        last_file_offset = file_offset;
                                    }
                                }
                                if (mtk.preceding_space) {
                                    arrput(ctx->result_buffer, ' ');
                                }
                                memcpy(arraddnptr(ctx->result_buffer, mtk.length), mtk.start, mtk.length);
                            }
                        }

                        FileLoc pop_macro = {
                            .offset = arrlen(ctx->result_buffer),
                            .mode = LOC_POP,
                        };
                        arrput(ctx->loc.list, pop_macro);

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
preprocess_filename(PreContext * ctx, char * filename) {
    char * file_buffer = NULL;
    size_t file_size;

    // search for buffer or create it if it doesn't exist
    for (int i=0; i < arrlen(ctx->loc.file_buffers); i++) {
        FileInfo * fb = ctx->loc.file_buffers[i];
        if (strcmp(filename, fb->filename) == 0) {
            if (fb->once) {
                return 0;
            }
            file_buffer = fb->buffer;
            file_size = fb->buffer_size;
            ctx->current_file = fb;
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
        FileInfo * new_buf = calloc(1, sizeof(*new_buf));
        new_buf->filename = filename;
        new_buf->buffer = file_buffer;
        new_buf->buffer_size = file_size;
        new_buf->mtime = mtime;
        if (!ctx->is_sys_header) {
            new_buf->gen = true;
        }
        ctx->current_file = new_buf;
        arrput(ctx->loc.file_buffers, new_buf);
    }

    if (ctx->include_level == 0) {
        ctx->base_file = filename;
    }

    return preprocess_buffer(ctx);
}

static char intro_defs [] =
"#define_forced __INTRO__ 1\n"

"#define_forced __unaligned \n"
"#if !defined __GNUC__\n"
"  #define_forced __forceinline inline\n"
"  #define_forced __THROW \n"
"#endif\n"
"#define_forced __inline inline\n"
"#define_forced __restrict restrict\n"

// MINGW
"#define _VA_LIST_DEFINED 1\n"

// GNU
"#if defined __GNUC__\n"
"#define_forced __restrict__ restrict\n"
"#define_forced __inline__ inline\n"
"#define_forced __attribute__(x) \n"
"#define_forced __extension__ \n"
"#define_forced __asm__(x) \n"
"#define_forced __volatile__(x) \n"
"#endif\n"
;

PreInfo
run_preprocessor(int argc, char ** argv) {
    // init pre context
    PreContext ctx_ = {0}, *ctx = &ctx_;
    ctx->arena = new_arena((1 << 20)); // 1MiB buckets
    ctx->expr_ctx = calloc(1, sizeof(*ctx->expr_ctx));
    ctx->expr_ctx->mode = MODE_PRE;
    ctx->expr_ctx->arena = new_arena(EXPR_BUCKET_CAP);

    PreInfo info = {0};

    sh_new_arena(ctx->defines);

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
        shputs(ctx->defines, special);
    }

    bool preprocess_only = false;
    bool no_sys = false;
    char * filepath = NULL;

    Token *const undef_replace_list = NULL; // reserve an address. remains unused
    Token * default_replace_list = NULL;
    arrput(default_replace_list, ((Token){.start = "1", .length = 1}));
    Define * deferred_defines = NULL;

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
                new_def.replace_list = default_replace_list;
                // TODO: define option
                arrput(deferred_defines, new_def);
            }break;

            case 'U': {
                Define undef;
                undef.key = ADJACENT();
                undef.replace_list = undef_replace_list;
                arrput(deferred_defines, undef);
            }break;

            case 'I': {
                const char * new_path = ADJACENT();
                arrput(include_paths, new_path);
            }break;

            case 'E': {
                preprocess_only = true;
            }break;

            case 'o': {
                info.output_filename = argv[++i];
            }break;

            case 0: {
                if (isatty(fileno(stdin))) {
                    fprintf(stderr, "Error: Cannot use terminal as file input.\n");
                    exit(1);
                }
                filepath = (char *)filename_stdin;
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
                    if (arg[3]) {
                        if (arg[3] == '_') {
                            ctx->m_options.target_mode = MT_SPACE;
                        } else if (arg[3] == 'n') {
                            ctx->m_options.target_mode = MT_NEWLINE;
                        } else {
                            goto unknown_option;
                        }
                        break;
                    } else {
                        ctx->m_options.target_mode = MT_NORMAL;
                        ctx->m_options.custom_target = argv[++i];
                    }
                }break;

                case 'F' :{
                    ctx->m_options.filename = argv[++i];
                }break;

                case 's': {
                    if (0==strcmp(arg+3, "ys")) { // -Msys
                        ctx->m_options.use_msys_path = true;
                    } else {
                        goto unknown_option;
                    }
                }break;

                case 'P': {
                    ctx->m_options.P = true;
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

    // option error checking
    if (ctx->m_options.enabled && ctx->m_options.target_mode == MT_NEWLINE) {
        struct stat out_stat;
        fstat(fileno(stdout), &out_stat);
        if (S_ISFIFO(out_stat.st_mode) && !isatty(fileno(stdout))) {
            fprintf(stderr, "WARNING: Newline separation '-MTn' may cause unexpected behavior. Consider using space separation '-MT_'.\n");
        }
    }
    if (ctx->m_options.target_mode != MT_NORMAL && ctx->m_options.P) {
        fprintf(stderr, "Error: Cannot use '-MT_' or -'MTn' with '-MP'.\n");
        exit(1);
    }

    if (!no_sys) {
        FileInfo temp_file = {0};

        ctx->minimal_parse = true;

        char program_dir [1024];
        char path [1024];
        strcpy(program_dir, argv[0]);
        path_normalize(program_dir);
        path_dir(program_dir, program_dir, NULL);

        const char * sys_inc_path = "intro.cfg";
        path_join(path, program_dir, sys_inc_path);
        char * cfg_buffer = intro_read_file(path, NULL);
        if (cfg_buffer) {
            Config cfg = load_config(cfg_buffer);
            ctx->sys_header_first = arrlen(include_paths);
            memcpy(arraddnptr(include_paths, arrlen(cfg.sys_include_paths)), cfg.sys_include_paths, arrlen(cfg.sys_include_paths) * sizeof(char *));
            ctx->sys_header_last = arrlen(include_paths) - 1;

            if (cfg.defines) {
                temp_file.buffer = cfg.defines;
                temp_file.filename = path;
                ctx->current_file = &temp_file;
                preprocess_buffer(ctx);
            }
        } else {
            fprintf(stderr, "No file at %s\n", path);
        }

        bool temp_minimal_parse = false;
        if (ctx->m_options.enabled && !ctx->m_options.D) {
            preprocess_only = true;
            temp_minimal_parse = true;
        } else {
            temp_file.buffer = intro_defs;
            temp_file.filename = "__INTRO_DEFS__";
            ctx->current_file = &temp_file;
            preprocess_buffer(ctx);
        }

        ctx->minimal_parse = temp_minimal_parse;
    }

    for (int i=0; i < arrlen(deferred_defines); i++) {
        Define def = deferred_defines[i];
        if (def.replace_list == undef_replace_list) {
            (void)shdel(ctx->defines, def.key);
        } else {
            shputs(ctx->defines, def);
        }
    }
    arrfree(deferred_defines);

    if (!filepath) {
        fputs("No filename given.\n", stderr);
        exit(1);
    }
    if (info.output_filename == NULL) {
        strputf(&info.output_filename, "%s.intro", filepath);
    }

    int error = preprocess_filename(ctx, filepath);
    if (error) {
        if (error == RET_FILE_NOT_FOUND) {
            fprintf(stderr, "File not found.\n");
        }
        info.ret = -1;
        return info;
    }
    arrput(ctx->result_buffer, 0);

    if (ctx->m_options.enabled) {
        char * ext = strrchr(filepath, '.');
        int len_basename = (ext)? ext - filepath : strlen(filepath);
        if (ctx->m_options.D && !ctx->m_options.filename) {
            char * dep_file = NULL;
            strputf(&dep_file, "%.*s.d", len_basename, filepath);
            ctx->m_options.filename = dep_file;
        }

        char * rule = NULL;
        char * dummy_rules = NULL;
        if (ctx->m_options.target_mode == MT_NORMAL) {
            if (ctx->m_options.custom_target) {
                strputf(&rule, "%s: ", ctx->m_options.custom_target);
            } else {
                strputf(&rule, "%.*s.o: ", len_basename, filepath);
            }
        }
        const char * sep = " ";
        if (ctx->m_options.target_mode == MT_NEWLINE) {
            sep = "\n";
        }
        strputf(&rule, "%s", filepath);
        for (int i=0; i < shlen(ctx->dependency_set); i++) {
            char buf [1024];
            const char * path = ctx->dependency_set[i].key;
            if (ctx->m_options.use_msys_path) {
                // NOTE: paths should already be using forward slashes at this point
                strcpy(buf, ctx->dependency_set[i].key);
                char drive_char = buf[0];
                if (is_alpha(drive_char) && buf[1] == ':' && buf[2] == '/') {
                    buf[0] = '/';
                    // captials work in msys, but lower case feels more idiomatic
                    if (drive_char < 'a') drive_char += ('a' - 'A');
                    buf[1] = drive_char;
                }
                path = buf;
            }
            strputf(&rule, "%s%s", sep, path);
            if (ctx->m_options.P) {
                strputf(&dummy_rules, "%s:\n", path);
            }
        }
        strputf(&rule, "\n");
        if (dummy_rules != NULL) {
            strputf(&rule, "%.*s", (int)arrlen(dummy_rules), dummy_rules);
        }

        if (preprocess_only && !ctx->m_options.filename) {
            fputs(rule, stdout);
            exit(0);
        }

        if (ctx->m_options.filename == NULL) {
            fprintf(stderr, "Somehow, intro cannot figure out what to do with the dependency information.\n"
                            "Congratulations, you confused not only the program, but also me personally. -cyman\n");
            exit(1);
        }
        int error = intro_dump_file(ctx->m_options.filename, rule, arrlen(rule));
        if (error < 0) {
            fprintf(stderr, "Failed to write dependencies to '%s'\n", ctx->m_options.filename);
            exit(1);
        }
    }
    if (preprocess_only) {
        fputs(ctx->result_buffer, stdout);
        exit(0);
    }

    for (int def_i=0; def_i < shlen(ctx->defines); def_i++) {
        Define def = ctx->defines[def_i];
        arrfree(def.arg_list);
        arrfree(def.replace_list);
    }
    shfree(ctx->defines);

    info.loc = ctx->loc;
    info.loc.index = 0;
    info.loc.count = arrlen(ctx->loc.list);
    info.result_buffer = ctx->result_buffer;
    return info;
}
