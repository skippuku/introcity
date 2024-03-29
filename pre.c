#include <time.h>

#include "global.c"
#include "lexer.c"

#ifdef __SSE2__
#include <immintrin.h>
#endif

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
    MACRO_has_include,
    MACRO_has_include_next,
    MACRO_COUNT
} SpecialMacro;

typedef struct {
    char * key;
    Token * replace_list;
    char ** arg_list;

    FileInfo * file;
    size_t file_offset;

    int32_t arg_count;
    SpecialMacro special;
    bool is_defined;
    bool func_like;
    bool variadic;
    bool forced;
} Define;

typedef struct {
    int * macro_index_stack;
    Token * list;
    TokenIndex * tidx;
    char * last_macro_name;
    bool in_expression;
} ExpandContext;

typedef struct {
    Token * result_list;
    FileInfo * current_file;
    Define * defines;
    const char ** include_paths;
    MemArena * arena;
    NameSet * dependency_set;
    ExprContext * expr_ctx;
    ExpandContext expand_ctx;
    char * expansion_site;
    LocationContext loc;

    const Config * cfg;

    NoticeState * notice_stack;
    NoticeState notice;

    const char * base_file;
    int counter;
    int include_level;

    int sys_header_first;
    int sys_header_last;
    bool is_sys_header;
    bool minimal_parse;
    bool preprocess_only;
} PreContext;

typedef struct {
    PreContext * ctx;
    FileInfo * chunk_file;
    int32_t begin_chunk;
    NoticeState notice;
} PasteState;

#define BOLD_RED "\e[1;31m"
#define BOLD_YELLOW "\e[1;33m"
#define CYAN "\e[0;36m"
#define WHITE "\e[0;37m"
#define BOLD_WHITE "\e[1;37m"
static void
strput_code_segment(char ** p_s, char * segment_start, char * segment_end, char * highlight_start, char * highlight_end, const char * highlight_color) {
    strputf(p_s, "\n%.*s", (int)(highlight_start - segment_start), segment_start);
    strputf(p_s, "%s%.*s" WHITE, highlight_color, (int)(highlight_end - highlight_start), highlight_start);
    strputf(p_s, "%.*s\n", (int)(segment_end - highlight_end), highlight_end);
    for (int i=0; i < highlight_start - segment_start; i++) {
        if (segment_start[i] == '\t') {
            arrput(*p_s, '\t');
        } else {
            arrput(*p_s, ' ');
        }
    }
    for (int i=0; i < highlight_end - highlight_start; i++) arrput(*p_s, '~');
    strputf(p_s, "\n");
}

static int
count_newlines_unaligned(char * start, int count) {
    int result = 0;
    char * s = start;
    while (s < start + count) {
        if (*s == '\n') result += 1;
        s++;
    }
    return result;
}

static int
count_newlines_in_range(char * start, char * end, char ** o_last_line) {
#ifdef __SSE2__
    char * s = start;
    int result = 1;
    int count_to_aligned = (16 - ((uintptr_t)s & 15)) & 15;
    _assume(end > s);
    result += count_newlines_unaligned(s, MIN(count_to_aligned, end - s));
    if (s < end) {
        while (s+16 < end) {
            const __m128i newlines = _mm_set1_epi8('\n');
            const __m128i mask = _mm_set1_epi8(1);
            __m128i line = _mm_load_si128((void *)s);
            __m128i cmp = _mm_cmpeq_epi8(line, newlines);
            cmp = _mm_and_si128(cmp, mask);
            __m128i vsum = _mm_sad_epu8(cmp, _mm_setzero_si128());
            int isum = _mm_cvtsi128_si32(vsum) + _mm_extract_epi16(vsum, 4);
            result += isum;
            s += 16;
        }
        _assume(end - s >= 0);
        result += count_newlines_unaligned(s, end - s);
    }
#else
    int result = 1 + count_newlines_unaligned(start, end - start);
#endif
    while (end >= start && *--end != '\n');
    end++;
    *o_last_line = end;
    return result;
}

FileInfo *
get_location_info(LocationContext * lctx, int32_t tk_index, NoticeState * o_notice) {
    int loc_index = -1;
    int max = (lctx->count)? lctx->count : arrlen(lctx->list);
    for (int i=lctx->index; i < max; i++) {
        FileLoc loc = lctx->list[i];
        if (tk_index < loc.offset) {
            lctx->index = i;
            loc_index = i-1;
            break;
        } else {
            lctx->notice = loc.notice;
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
                break;
            }
        }
    }
    if (loc_index < 0) return NULL;
    char * file_buffer = lctx->file->buffer;
    if (!file_buffer) {
        fprintf(stderr, "Internal error: failed to find file for error report.");
        exit(-1);
    }

    if (o_notice) *o_notice = lctx->notice;
    return lctx->file;
}

static FileInfo *
find_origin(LocationContext * lctx, FileInfo * current, char * ptr) {
    int i = arrlen(lctx->file_buffers);
    if (i == 0) return NULL;

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
message_internal(char * start_of_line, const char * filename, int line, char * hl_start, char * hl_end, const char * message, int message_type) {
    char * end_of_line = strchr(hl_end, '\n');
    if (!end_of_line) {
        end_of_line = hl_end;
    }
    char * s = NULL;
    const char * message_type_string = (message_type == 1)? "Warning" : "Error";
    const char * color = (message_type == 1)? BOLD_YELLOW : BOLD_RED;
    strputf(&s, "%s%s" WHITE " (" CYAN "%s:" BOLD_WHITE "%i" WHITE "): %s\n", color, message_type_string, filename, line, message);
    strput_code_segment(&s, start_of_line, end_of_line, hl_start, hl_end, color);
    fputs(s, stderr);
    //db_break();
    arrfree(s);
}

void
parse_msg_internal(LocationContext * lctx, int32_t tk_index, char * message, int message_type) {
    Token tk = lctx->tk_list[tk_index];
    char * start_of_line = NULL;

    FileInfo * file = get_location_info(lctx, tk_index, NULL);
    int line_num = count_newlines_in_range(file->buffer, tk.start, &start_of_line);
    char * filename = file->filename;
    assert(start_of_line != NULL);

    char * hl_start = tk.start;
    char * hl_end = hl_start + tk.length;
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

void
location_note(LocationContext * lctx, IntroLocation location, const char * msg) {
    FileInfo * file = NULL;
    for (int i=0; i < lctx->count; i++) {
        FileInfo * f = lctx->file_buffers[i];
        if (0==strcmp(f->filename, location.path)) {
            file = f;
            break;
        }
    }
    if (file == NULL) {
        fprintf(stderr, "Could not find location for note.\n");
        return;
    }
    char * s = file->buffer + location.offset;
    char * start_of_line;
    int line = count_newlines_in_range(file->buffer, s, &start_of_line);

    Token tk = pre_next_token(&s);
    message_internal(start_of_line, location.path, line, tk.start, tk.start + tk.length, msg, 1);
}

void
strput_tokens(char ** p_str, Token * list) {
    for (int i=0; i < arrlen(list); i++) {
        Token tk = list[i];
        if (tk.preceding_space) {
            arrput(*p_str, ' ');
        }
        char * dst = arraddnptr(*p_str, tk.length);
        memcpy(dst, tk.start, tk.length);
    }
    arrsetcap(*p_str, arrlen(*p_str) + 1);
    (*p_str)[arrlen(*p_str)] = 0;
}

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
ignore_section(PasteState * state, int32_t begin_ignored, int32_t end_ignored) {
    PreContext * ctx = state->ctx;
    if (state->begin_chunk != -1) {
        ptrdiff_t signed_file_offset = state->chunk_file->tk_list[state->begin_chunk].start - state->chunk_file->buffer;
        _assume(signed_file_offset >= 0);
        FileLoc loc = {
            .offset      = arrlen(ctx->result_list),
            .file_offset = (size_t)signed_file_offset,
            .notice = state->notice,
        };
        FileLoc * plast = &arrlast(ctx->loc.list);
        if ((loc.offset == plast->offset) && (plast->mode == LOC_NONE)) {
            *plast = loc;
        } else {
            arrput(ctx->loc.list, loc);
        }
        // paste chunk
        int len_chunk = begin_ignored - state->begin_chunk;
        if (len_chunk > 0) {
            if (!state->ctx->preprocess_only) {
                for (int i=0; i < len_chunk; i++) {
                    Token tk = state->chunk_file->tk_list[state->begin_chunk + i];
                    if (tk.type != TK_NEWLINE && tk.type != TK_COMMENT) {
                        arrput(ctx->result_list, tk);
                    }
                }
            } else {
                Token last_tk = {0};
                for (int i=0; i < len_chunk; i++) {
                    Token tk = state->chunk_file->tk_list[state->begin_chunk + i];
                    if (tk.type != TK_COMMENT) {
                        if (tk.type == TK_IDENTIFIER && last_tk.type == TK_IDENTIFIER) {
                            tk.preceding_space = true;
                        }
                        arrput(ctx->result_list, tk);
                        last_tk = tk;
                    }
                }
            }
        }
    }
    state->begin_chunk = end_ignored;
    state->chunk_file = ctx->current_file;
    state->notice = ctx->notice;
}

void
tk_cat(Token ** p_list, const Token * ext, bool space) {
    Token * dest = arraddnptr(*p_list, arrlen(ext));
    for (int i=0; i < arrlen(ext); i++) {
        dest[i] = ext[i];
    }
    dest[0].preceding_space = space;
}

typedef struct {
    Token tk;
    char * name;
    bool exists;
    bool is_quote;
    bool is_next;
    bool test_only;
} IncludeFile;

bool
pre_get_include_path(PreContext * ctx, const char * file_dir, IncludeFile inc_file, char * o_path) {
    int start_search_index = 0;
    if (inc_file.is_next) {
        for (int i=0; i < arrlen(ctx->include_paths); i++) {
            const char * include_path = ctx->include_paths[i];
            if (0==strcmp(file_dir, include_path)) {
                start_search_index = i + 1;
                break;
            }
        }
    } else {
        if (inc_file.is_quote) {
            path_join(o_path, file_dir, inc_file.name);
            if (access(o_path, F_OK) == 0) {
                ctx->is_sys_header = false;
                return true;
            }
        }
    }

    for (int i = start_search_index; i < arrlen(ctx->include_paths); i++) {
        const char * include_path = ctx->include_paths[i];
        path_join(o_path, include_path, inc_file.name);
        if (access(o_path, F_OK) == 0) {
            if (!inc_file.test_only) {
                ctx->is_sys_header = (i >= ctx->sys_header_first && i <= ctx->sys_header_last);
            }
            return true;
        }
    }

    return false;
}

static bool macro_scan(PreContext * ctx, int macro_tk_index);

static Token
internal_macro_next_token(PreContext * ctx, int * o_index) {
    ++(*o_index);
    if (*o_index < arrlen(ctx->expand_ctx.list)) {
        return ctx->expand_ctx.list[*o_index];
    } else {
        Token tk;
        if (!ctx->expand_ctx.tidx) {
            tk = (Token){.type = TK_END};
            arrput(ctx->expand_ctx.list, tk);
            return tk;
        }
        do {
            tk = next_token(ctx->expand_ctx.tidx);
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
    int32_t prev_index = 0;
    if (ctx->expand_ctx.tidx) prev_index = ctx->expand_ctx.tidx->index;
    int prev_len_list = arrlen(ctx->expand_ctx.list);
    Token l_paren = internal_macro_next_token(ctx, &i);
    if (l_paren.type == TK_END || *l_paren.start != '(') {
        arrsetlen(ctx->expand_ctx.list, prev_len_list);
        if (ctx->expand_ctx.tidx) ctx->expand_ctx.tidx->index = prev_index;
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
        if (paren_level == 1 && tk.type == TK_COMMA) {
            arrput(arg_list, arg_tks);
            arg_tks = NULL;
            continue;
        }
        if (tk.type == TK_L_PARENTHESIS) {
            paren_level += 1;
        } else if (tk.type == TK_R_PARENTHESIS) {
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

    ExpandContext prev_ctx = ctx->expand_ctx;
    for (int arg_i=0; arg_i < arrlen(arg_list); arg_i++) {
        for (int tk_i=0; tk_i < arrlen(arg_list[arg_i]); tk_i++) {
            if (arg_list[arg_i][tk_i].type == TK_IDENTIFIER) {
                ctx->expand_ctx = (ExpandContext){
                    .macro_index_stack = ctx->expand_ctx.macro_index_stack,
                    .list = arg_list[arg_i],
                    .tidx = ctx->expand_ctx.tidx,
                };
                macro_scan(ctx, tk_i);
                arg_list[arg_i] = ctx->expand_ctx.list;
            }
        }
    }
    prev_ctx.macro_index_stack = ctx->expand_ctx.macro_index_stack;
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
    char temp [1024];
    TERMINATE(temp, macro_tk->start, macro_tk->length);

    ptrdiff_t macro_index = shgeti(ctx->defines, temp);
    if (macro_index < 0) {
        return false;
    }
    Define * macro = &ctx->defines[macro_index];
    if (!macro->is_defined) return false;

    if (macro->special != MACRO_NOT_SPECIAL) {
        char * buf = NULL;
        int token_type = TK_STRING;
        switch(macro->special) {
        case MACRO_NOT_SPECIAL: _assume(0);

        case MACRO_defined: {
            if (!ctx->expand_ctx.in_expression) {
                return false;
            }
            bool preceding_space = macro_tk->preceding_space;
            int index = macro_tk_index;
            Token tk = internal_macro_next_token(ctx, &index);
            bool is_paren = false;
            if (tk.type == TK_L_PARENTHESIS) {
                is_paren = true;
                tk = internal_macro_next_token(ctx, &index);
            }
            if (tk.type != TK_IDENTIFIER) {
                preprocess_error(&tk, "Expected identifier.");
                exit(1);
            }
            TERMINATE(temp, tk.start, tk.length);
            Define def = shgets(ctx->defines, temp);
            char * replace = (def.is_defined)? "1" : "0";
            if (is_paren) {
                tk = internal_macro_next_token(ctx, &index);
                if (tk.type != TK_R_PARENTHESIS) {
                    preprocess_error(&tk, "Expected ')'.");
                    exit(1);
                }
            }
            Token replace_tk = (Token){
                .start = replace,
                .length = 1,
                .type = TK_NUMBER,
                .preceding_space = preceding_space,
            };
            arrdeln(ctx->expand_ctx.list, macro_tk_index, index - macro_tk_index);
            ctx->expand_ctx.list[macro_tk_index] = replace_tk;
            return true;
        }break;

        case MACRO_FILE: {
            strputf(&buf, "\"%s\"", ctx->current_file->filename);
        }break;

        case MACRO_LINE: {
            const FileInfo * current_file = find_origin(&ctx->loc, ctx->loc.file, ctx->expansion_site);
            int line_num = 0;
            if (current_file) {
                char * start_of_line_;
                line_num = count_newlines_in_range(current_file->buffer, ctx->expansion_site, &start_of_line_);
            }
            strputf(&buf, "%i", line_num);
            token_type = TK_NUMBER;
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

        case MACRO_COUNT: {
            preprocess_warning(macro_tk, "sus");
            return false;
        }break;

        case MACRO_has_include_next:
        case MACRO_has_include: {
            if (!ctx->expand_ctx.in_expression) {
                return false;
            }

            IncludeFile inc_file = {0};
            inc_file.is_next = (macro->special == MACRO_has_include_next);
            inc_file.test_only = true;

            bool preceding_space = macro_tk->preceding_space;
            int index = macro_tk_index;
            Token tk = internal_macro_next_token(ctx, &index);

            bool is_paren = false;
            if (tk.type == TK_L_PARENTHESIS) {
                is_paren = true;
                tk = internal_macro_next_token(ctx, &index);
            }

            const char * start;
            const char * end;

            if (tk.type == TK_STRING) {
                start = tk.start + 1;
                end = tk.start + tk.length - 1;
                inc_file.is_quote = true;
            } else if (tk.type == TK_L_ANGLE) {
                start = tk.start + 1;
                Token opening = tk;
                while (1) {
                    tk = internal_macro_next_token(ctx, &index);
                    if (tk.type == TK_R_ANGLE) {
                        break;
                    }
                    if (tk.type == TK_END) {
                        preprocess_error(&opening, "No closing '>'.");
                        exit(1);
                    }
                }
                end = tk.start;
            } else {
                preprocess_error(&tk, "Expected include file.");
                exit(1);
            }

            char inc_filename [1024];
            char file_dir [1024];
            char _static_buf [1024];
            char * _filename;

            TERMINATE(inc_filename, start, end - start);
            inc_file.name = inc_filename;

            path_dir(file_dir, ctx->current_file->filename, &_filename);

            bool found_file = pre_get_include_path(ctx, file_dir, inc_file, _static_buf);

            char * replace = (found_file)? "1" : "0";

            if (is_paren) {
                tk = internal_macro_next_token(ctx, &index);
                if (tk.type != TK_R_PARENTHESIS) {
                    preprocess_error(&tk, "Expected ')'.");
                    exit(1);
                }
            }

            Token replace_tk = (Token){
                .start = replace,
                .length = 1,
                .type = TK_NUMBER,
                .preceding_space = preceding_space,
            };
            arrdeln(ctx->expand_ctx.list, macro_tk_index, index - macro_tk_index);
            ctx->expand_ctx.list[macro_tk_index] = replace_tk;

            return true;
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
                char * s = result.start;
                Token result_eval_tk = pre_next_token(&s);
                result.type = result_eval_tk.type;
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
expand_line(PreContext * ctx, TokenIndex * tidx, bool is_include) {
    Token * ptks = NULL;
    arrsetcap(ptks, 16);
    ExpandContext prev_ctx = ctx->expand_ctx;
    while (1) {
        Token ptk = next_token(tidx);
        int32_t prev_index = tidx->index - 1;
        if (ptk.type == TK_NEWLINE) {
            tidx->index = prev_index;
            ptk.type = TK_END;
            ptk.start -= 1; // highlight last character in errors
            arrput(ptks, ptk);
            break;
        } else if (ptk.type == TK_COMMENT) {
            continue;
        } else if (is_include && ptk.type == TK_L_ANGLE) {
            int32_t closing = find_closing((TokenIndex){.list = tidx->list, .index = prev_index});
            if (!closing) {
                preprocess_error(&ptk, "No closing '>'.");
                exit(1);
            }
            tidx->index = closing;
            ptk.length = tidx->list[tidx->index].start - ptk.start;
            ptk.type = TK_STRING;
            arrput(ptks, ptk);
        } else {
            int index = arrlen(ptks);
            arrput(ptks, ptk);
            if (ptk.type == TK_IDENTIFIER) {
                ctx->expand_ctx = (ExpandContext){
                    .macro_index_stack = NULL,
                    .list = ptks,
                    .in_expression = !is_include,
                    .tidx = tidx,
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
parse_expression(PreContext * ctx, TokenIndex * tidx) {
    Token * tks = expand_line(ctx, tidx, false);
    TokenIndex etidx = {.list = tks, .index = 0};
    ExprNode * tree = build_expression_tree2(ctx->expr_ctx, &etidx);
    if (!tree) {
        preprocess_error(&etidx.list[etidx.index - 1], "Problem token.");
        exit(1);
    }
    uint8_t * bytecode = build_expression_procedure2(ctx->expr_ctx, tree, NULL);
    intmax_t result = intro_run_bytecode(bytecode, NULL).si;

    arrfree(bytecode);
    reset_arena(ctx->expr_ctx->arena);
    arrfree(tks);

    return !!result;
}

static void
pre_skip(PreContext * ctx, TokenIndex * tidx, bool elif_ok) {
    int depth = 1;
    while (1) {
        Token tk = next_token(tidx);
        if (tk.length == 1 && tk.start[0] == '#') {
            tk = next_token(tidx);
            if (tk.type == TK_IDENTIFIER) {
                if (tk_equal(&tk, "if")
                 || tk_equal(&tk, "ifdef")
                 || tk_equal(&tk, "ifndef"))
                {
                    depth++;
                } else if (tk_equal(&tk, "endif")) {
                    depth--;
                } else if (tk_equal(&tk, "else")) {
                    if (elif_ok && depth == 1) {
                        depth = 0;
                    }
                } else if (tk_equal(&tk, "elif")) {
                    if (elif_ok && depth == 1) {
                        bool expr_result = parse_expression(ctx, tidx);
                        if (expr_result) {
                            depth = 0;
                        }
                    }
                }
            }
        }
        while (tk.type != TK_NEWLINE && tk.type != TK_END) {
            tk = next_token(tidx);
        }
        if (depth == 0 || tk.type == TK_END) {
            tidx->index--;
            return;
        }
    }
}

int
pre_handle_intro_pragma(PreContext * ctx, TokenIndex * tidx, Token * o_tk) {
#define SET_BITS(var, mask, state) (var = (state)? var | mask : var & ~mask)
    static const struct{const char * key; NoticeState value;} elements [] = {
        {"functions", NOTICE_FUNCTIONS},
        {"macros", NOTICE_MACROS},
        {"all", NOTICE_ALL},
        {"includes", NOTICE_INCLUDES},
        {"system", NOTICE_SYS_HEADERS},
    };
    Token tk;
    while (1) {
        tk = next_token(tidx);
        bool is_disable = false, found_match = false;
        if (tk_equal(&tk, "enable") || (is_disable = tk_equal(&tk, "disable"))) {
            tk = next_token(tidx);
            if (tk.type == TK_IDENTIFIER) {
                for (int element_i=0; element_i < LENGTH(elements); element_i++) {
                    if (tk_equal(&tk, elements[element_i].key)) {
                        int bit = elements[element_i].value;
                        SET_BITS(ctx->notice, bit, !is_disable);
                        found_match = true;
                        break;
                    }
                }
                if (!found_match) {
                    preprocess_error(&tk, "Invalid element.");
                    return -1;
                }
            } else {
                SET_BITS(ctx->notice, NOTICE_ENABLED, !is_disable);
                tidx->index--;
            }
        } else if (tk_equal(&tk, "push")) {
            arrpush(ctx->notice_stack, ctx->notice);
        } else if (tk_equal(&tk, "pop")) {
            if (arrlen(ctx->notice_stack) < 1) {
                preprocess_error(&tk, "attempt to pop stack of 0");
                return -1;
            }
            ctx->notice = arrpop(ctx->notice_stack);
        } else if (tk.type == TK_COMMA) {
            continue;
        } else if (tk.type == TK_NEWLINE) {
            break;
        } else {
            preprocess_error(&tk, "Unknown intro directive.");
            return -1;
        }
    }
    if (o_tk) *o_tk = tk;
    return 0;
}

int
preprocess_set_file(PreContext * ctx, char * filename) {
    char * file_buffer = NULL;
    size_t file_size;

    // search for buffer or create it if it doesn't exist
    for (int i=0; i < arrlen(ctx->loc.file_buffers); i++) {
        FileInfo * fb = ctx->loc.file_buffers[i];
        if (strcmp(filename, fb->filename) == 0) {
            if (fb->once) {
                return RET_ALREADY_INCLUDED;
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
            g_metrics.pre_time += nanointerval();
            file_buffer = intro_read_file(filename, &file_size);
            struct stat file_stat;
            stat(filename, &file_stat);
            mtime = file_stat.st_mtime;
            g_metrics.file_access_time += nanointerval();
        }
        if (!file_buffer) {
            return RET_FILE_NOT_FOUND;
        }

        FileInfo * new_buf = calloc(1, sizeof(*new_buf));
        new_buf->filename = filename;
        new_buf->buffer = file_buffer;

        g_metrics.pre_time += nanointerval();
        new_buf->tk_list = create_token_list(file_buffer);
        g_metrics.lex_time += nanointerval();

        if (ctx->cfg->show_metrics) {
            for (int i=0; i < arrlen(new_buf->tk_list); i++) {
                if (new_buf->tk_list[i].type == TK_NEWLINE) {
                    g_metrics.count_pre_lines++;
                }
            }
            g_metrics.count_pre_tokens += arrlen(new_buf->tk_list);
            g_metrics.count_pre_file_bytes += file_size;
        }

        new_buf->buffer_size = file_size;
        new_buf->mtime = mtime;
        ctx->current_file = new_buf;
        arrput(ctx->loc.file_buffers, new_buf);
    }

    if (ctx->include_level == 0) {
        ctx->base_file = filename;
    }

    return 0;
}

int
preprocess_file(PreContext * ctx) {
    FileInfo * file = ctx->current_file;
    char * filename = file->filename;

    TokenIndex _tidx = {0}, * tidx = &_tidx;
    tidx->list = file->tk_list;
    tidx->index = 0;

    char file_dir [1024];
    char * filename_nodir;
    (void) filename_nodir;
    path_dir(file_dir, filename, &filename_nodir);

    arrsetcap(ctx->expand_ctx.list, 64);
    arrsetlen(ctx->expand_ctx.list, 1);

    PasteState paste_ = {
        .ctx = ctx,
        .chunk_file = ctx->current_file,
        .begin_chunk = 0,
    }, *paste = &paste_;

    FileLoc loc_push_file = {
        .offset = arrlen(ctx->result_list),
        .file_offset = 0,
        .file = file,
        .mode = LOC_FILE,
        .notice = ctx->notice,
    };
    arrput(ctx->loc.list, loc_push_file);

    while (1) {
        int32_t start_of_line = tidx->index;
        ctx->expansion_site = tk_at(tidx).start;

        IncludeFile inc_file = {0};
        Token tk = next_token(tidx);
        bool def_forced = false;

        if (tk.type == TK_HASH) {
            Token directive = next_token(tidx);
            while (directive.type == TK_COMMENT) directive = next_token(tidx);
            if (directive.type != TK_IDENTIFIER) {
                goto unknown_directive;
            }

            if (is_digit(*directive.start) || tk_equal(&directive, "line")) {
                // TODO: line directives
            } else if (tk_equal(&directive, "include") || (tk_equal(&directive, "include_next") && (inc_file.is_next = true))) {
                Token * expanded = expand_line(ctx, tidx, true);
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
                TokenIndex _midx = *tidx, * midx = &_midx;
                Token macro_name = next_token(midx);
                if (macro_name.type != TK_IDENTIFIER) {
                    preprocess_error(&macro_name, "Expected identifier.");
                    return -1;
                }

                char ** arg_list = NULL;
                bool is_func_like = tk_at(midx).type == TK_L_PARENTHESIS && !tk_at(midx).preceding_space;
                bool variadic = false;
                if (is_func_like) {
                    midx->index++;
                    while (1) {
                        Token tk = next_token(midx);
                        while (tk.type == TK_COMMENT) tk = next_token(midx);
                        if (tk.type == TK_IDENTIFIER) {
                            char * arg = copy_and_terminate(ctx->arena, tk.start, tk.length);
                            arrput(arg_list, arg);
                        } else if (tk.type == TK_R_PARENTHESIS) {
                            break;
                        } else if (memcmp(tk.start, "...", 3) == 0) {
                            variadic = true;
                            midx->index += 2;
                        } else {
                            preprocess_error(&tk, "Invalid symbol.");
                            return -1;
                        }

                        tk = next_token(midx);
                        while (tk.type == TK_COMMENT) tk = next_token(midx);
                        if (tk.type == TK_R_PARENTHESIS) {
                            break;
                        } else if (tk.type == TK_COMMA) {
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

                *tidx = *midx;
                Token * replace_list = NULL;
                while (1) {
                    Token tk = next_token(tidx);
                    if (tk.type == TK_NEWLINE || tk.type == TK_END) {
                        tidx->index--;
                        break;
                    }
                    if (tk.type != TK_COMMENT) {
                        if (replace_list == NULL) tk.preceding_space = false;
                        arrput(replace_list, tk);
                    }
                }

                // create macro
                Define def = {0};
                def.is_defined = true;
                def.key = copy_and_terminate(ctx->arena, macro_name.start, macro_name.length);
                def.replace_list = replace_list;
                def.forced = def_forced;
                if (is_func_like) {
                    def.arg_list = arg_list;
                    def.arg_count = arrlen(arg_list);
                    def.func_like = true;
                    def.variadic = variadic;
                }

                bool gen = (ctx->notice & NOTICE_ENABLED)
                        && (ctx->notice & NOTICE_MACROS)
                        && !(ctx->is_sys_header && !(ctx->notice & NOTICE_SYS_HEADERS));
                if (gen) {
                    def.file = ctx->current_file;
                    def.file_offset = macro_name.start - ctx->current_file->buffer;
                }

                Define * prevdef = shgetp_null(ctx->defines, def.key);
                if (prevdef && prevdef->is_defined) {
                    if (prevdef->forced) {
                        if (!ctx->is_sys_header) {
                            preprocess_warning(&macro_name, "Attempted consesquent #define of forced define.");
                        }
                        goto nextline;
                    } else {
                        // preprocess_warning(&macro_name, "Macro redefinition.");
                        // TODO: check that definitions are equal
                    }
                }
                if (!prevdef) {
                    shputs(ctx->defines, def);
                } else {
                    *prevdef = def;
                }
            } else if (tk_equal(&directive, "undef")) {
                Token def = next_token(tidx);
                STACK_TERMINATE(iden, def.start, def.length);
                Define * macro = shgetp(ctx->defines, iden);
                if (macro && macro->is_defined) {
                    if (!macro->forced) {
                        macro->is_defined = false;
                    } else {
                        preprocess_warning(&def, "Attempted #undef of forced define.");
                    }
                }
            } else if (tk_equal(&directive, "if")) {
                bool expr_result = parse_expression(ctx, tidx);
                if (!expr_result) {
                    pre_skip(ctx, tidx, true);
                }
            } else if (tk_equal(&directive, "ifdef")) {
                tk = next_token(tidx);
                STACK_TERMINATE(name, tk.start, tk.length);
                int def_index = shgeti(ctx->defines, name);
                if (def_index < 0) {
                    pre_skip(ctx, tidx, true);
                }
            } else if (tk_equal(&directive, "ifndef")) {
                tk = next_token(tidx);
                STACK_TERMINATE(name, tk.start, tk.length);
                int def_index = shgeti(ctx->defines, name);
                if (def_index >= 0) {
                    pre_skip(ctx, tidx, true);
                }
            } else if (tk_equal(&directive, "elif")) {
                pre_skip(ctx, tidx, false);
            } else if (tk_equal(&directive, "else")) {
                pre_skip(ctx, tidx, false);
            } else if (tk_equal(&directive, "endif")) {
            } else if (tk_equal(&directive, "error")) {
                preprocess_error(&directive, "error directive.");
                return -1;
            } else if (tk_equal(&directive, "pragma")) {
                tk = next_token(tidx);
                if (tk_equal(&tk, "once")) {
                    ctx->current_file->once = true;
                } else if (tk_equal(&tk, "intro")) {
                    int ret = pre_handle_intro_pragma(ctx, tidx, &tk);
                    if (ret < 0) return ret;
                } else if (tk_equal(&tk, "pack")) {
                } else if (tk_equal(&tk, "GCC")) {
                } else if (tk_equal(&tk, "clang")) {
                } else if (tk_equal(&tk, "push_macro")) {
                } else if (tk_equal(&tk, "pop_macro")) {
                } else if (tk_equal(&tk, "message")) {
                    preprocess_warning(&tk, "Message pragma.");
                } else {
                    if (!ctx->is_sys_header) {
                        preprocess_warning(&tk, "Unknown pragma, ignoring.");
                    }
                }
            } else {
            unknown_directive:
                preprocess_error(&directive, "Unknown directive.");
                return -1;
            }

          nextline:
            // find next line
            while (tk.type != TK_NEWLINE && tk.type != TK_END) tk = next_token(tidx);

            if (!ctx->minimal_parse) ignore_section(paste, start_of_line, tidx->index);

            if (inc_file.exists) {
                char inc_filename [1024];
                TERMINATE(inc_filename, inc_file.tk.start + 1, inc_file.tk.length - 2);
                char ext_buf [128];
                if (0==strcmp(".intro", path_extension(ext_buf, inc_filename))) {
                    goto skip_and_add_include_dep;
                }
                char inc_filepath [1024];

                bool prev = ctx->is_sys_header;

                inc_file.name = inc_filename;
                bool file_found = pre_get_include_path(ctx, file_dir, inc_file, inc_filepath);

                if (!file_found) {
                    if (ctx->cfg->m_options.G) {
                      skip_and_add_include_dep: ;
                        char * inc_filepath_stored = copy_and_terminate(ctx->arena, inc_filename, strlen(inc_filename));
                        shputs(ctx->dependency_set, (NameSet){inc_filepath_stored});
                        continue;
                    } else {
                        preprocess_error(&inc_file.tk, "File not found.");
                        return -1;
                    }
                }

                char * inc_filepath_stored = copy_and_terminate(ctx->arena, inc_filepath, strlen(inc_filepath));
                if (!(ctx->cfg->m_options.no_sys && ctx->is_sys_header)) {
                    shputs(ctx->dependency_set, (NameSet){inc_filepath_stored});
                } else {
                    if (ctx->minimal_parse) {
                        continue;
                    }
                }

                ctx->include_level++;

                arrpush(ctx->notice_stack, ctx->notice);
                bool enable_notice = (ctx->notice & NOTICE_INCLUDES) && !(ctx->is_sys_header && !(ctx->notice & NOTICE_SYS_HEADERS));
                SET_BITS(ctx->notice, NOTICE_ENABLED, enable_notice);

                int ret = preprocess_set_file(ctx, inc_filepath_stored);
                if (ret == RET_FILE_NOT_FOUND) {
                    preprocess_error(&inc_file.tk, "Failed to read file.");
                    return -1;
                } else if (ret == RET_ALREADY_INCLUDED) {
                } else if (ret == 0) {
                    ret = preprocess_file(ctx);
                    if (ret < 0) return -1;
                } else {
                    return -1;
                }

                ctx->notice = arrpop(ctx->notice_stack);

                ctx->is_sys_header = prev;
                ctx->include_level--;
                ctx->current_file = file;

            }
        } else if (tk.type == TK_END) {
            if (!ctx->minimal_parse) ignore_section(paste, tidx->index - 1, -1);
            FileLoc pop_file = {
                .offset = arrlen(ctx->result_list),
                .mode = LOC_POP,
                .notice = ctx->notice,
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
                while (tk.type != TK_NEWLINE && tk.type != TK_END) tk = next_token(tidx);
            } else {
                if (tk.type == TK_COMMENT) {
                    ignore_section(paste, tidx->index, tidx->index);
                } else if (tk.type == TK_IDENTIFIER) {
                    ctx->expand_ctx.tidx = tidx;
                    ctx->expand_ctx.list[0] = tk;
                    int32_t start_index = tidx->index - 1;

                    bool has_space = tk.preceding_space;
                    if (macro_scan(ctx, 0)) {

                        Token * list = ctx->expand_ctx.list;
                        list[0].preceding_space = has_space;
                        ignore_section(paste, start_index, tidx->index);

                        FileLoc loc_push_macro = {
                            .offset = arrlen(ctx->result_list),
                            .file_offset = tk.start - file->buffer,
                            .file = file,
                            .macro_name = ctx->expand_ctx.last_macro_name,
                            .mode = LOC_MACRO,
                            .notice = ctx->notice,
                        };
                        arrput(ctx->loc.list, loc_push_macro);

                        if (arrlen(list) > 0) {
                            Token mtk;
                            for (int i=0; i < arrlen(list); i++) {
                                mtk = list[i];
                                if (mtk.type == TK_PLACEHOLDER) continue;
                                else if (mtk.type == TK_DISABLED) mtk.type = TK_IDENTIFIER;
                                arrput(ctx->result_list, mtk);
                            }
                        }

                        FileLoc pop_macro = {
                            .offset = arrlen(ctx->result_list),
                            .mode = LOC_POP,
                            .notice = ctx->notice,
                        };
                        arrput(ctx->loc.list, pop_macro);

                        arrsetlen(ctx->expand_ctx.list, 1);
                        arrsetlen(ctx->expand_ctx.macro_index_stack, 0);

                        g_metrics.count_macro_expansions += 1;
                    }
                }
            }
        }
    }

    return 0;
}

static char intro_defs [] =
"#ifndef __INTRO_MINIMAL__\n"
"#define __INTRO__ 1\n"
"#endif\n"

"#if !defined __GNUC__\n"
"#define_forced __forceinline inline\n"
"#define_forced __THROW \n"
"#endif\n"

"#define __inline inline\n"
"#define __restrict restrict\n"
"#define _Complex \n" // TODO: ... uhg

// MINGW
"#define _VA_LIST_DEFINED 1\n"

"#if defined __GNUC__\n"
"#define_forced __restrict__ restrict\n"
"#define_forced __inline__ inline\n"
"#define_forced __attribute__(x) \n"
"#define_forced __extension__ \n"
"#define_forced __asm__(x) \n"
"#define_forced __volatile__(x) \n"
"#endif\n"

"#define_forced _Static_assert _Static_assert\n"
"#define_forced bool bool\n"
"typedef bool _Bool;\n"
;

PreInfo
run_preprocessor(Config * cfg) {
    // init pre context
    PreContext ctx_ = {0}, *ctx = &ctx_;
    ctx->arena = new_arena((1 << 20)); // 1MiB buckets
    ctx->expr_ctx = calloc(1, sizeof(*ctx->expr_ctx));
    ctx->expr_ctx->mode = MODE_PRE;
    ctx->expr_ctx->arena = new_arena(EXPR_BUCKET_CAP);
    ctx->expr_ctx->ploc = &ctx->loc;

    ctx->notice = NOTICE_DEFAULT;
    ctx->notice_stack = NULL;

    ctx->cfg = cfg;
    ctx->preprocess_only = cfg->pre_only;

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
        {"__has_include", MACRO_has_include},
        {"__has_include_next", MACRO_has_include_next},
        {"imposter", MACRO_COUNT},
    };
    for (int i=0; i < LENGTH(special_macros); i++) {
        Define special = {0};
        special.key = special_macros[i].name;
        special.special = special_macros[i].value;
        special.is_defined = true;
        special.forced = true;
        shputs(ctx->defines, special);
    }

    if (cfg->pragma) {
        Token * list = create_token_list((char *)cfg->pragma);
        TokenIndex pragma_idx = {.list = list, .index = 0};
        pre_handle_intro_pragma(ctx, &pragma_idx, NULL);
        arrfree(list);
    }

    ctx->minimal_parse = false;
    ctx->is_sys_header = true;

    if (cfg->defines) {
        FileInfo * config_file = calloc(1, sizeof(*config_file));
        config_file->buffer = cfg->defines;
        config_file->tk_list = create_token_list((char *)cfg->defines);
        config_file->filename = copy_and_terminate(ctx->arena, cfg->config_filename, strlen(cfg->config_filename));
        arrput(ctx->loc.file_buffers, config_file);
        ctx->current_file = config_file;
        int ret = preprocess_file(ctx);
        if (ret < 0) {
            info.ret = -1;
            return info;
        }
    }

    if (arrlen(cfg->sys_include_paths) > 0) {
        ctx->sys_header_first = arrlen(ctx->include_paths);
        const char ** dest = arraddnptr(ctx->include_paths, arrlen(cfg->sys_include_paths));
        memcpy(dest, cfg->sys_include_paths, arrlen(cfg->sys_include_paths) * sizeof(char *));
        ctx->sys_header_last = arrlen(ctx->include_paths) - 1;
    }

    if (arrlen(cfg->include_paths) > 0) {
        const char ** dest = arraddnptr(ctx->include_paths, arrlen(cfg->include_paths));
        memcpy(dest, cfg->include_paths, arrlen(cfg->include_paths) * sizeof(char *));
    }

    bool temp_minimal_parse = false;
    if (cfg->m_options.enabled && !cfg->m_options.D) {
        ctx->preprocess_only = true;
        temp_minimal_parse = true;
        Define def = {
            .key = "__INTRO_MINIMAL__",
            .is_defined = true,
        };
        shputs(ctx->defines, def);
    }

    FileInfo * intro_defs_file = calloc(1, sizeof(*intro_defs_file));
    intro_defs_file->buffer_size = strlen(intro_defs);
    intro_defs_file->buffer = intro_defs;
    intro_defs_file->tk_list = create_token_list(intro_defs);
    intro_defs_file->filename = "__INTRO_DEFS__";
    arrput(ctx->loc.file_buffers, intro_defs_file);
    ctx->current_file = intro_defs_file;
    int ret = preprocess_file(ctx);
    if (ret < 0) {
        info.ret = -1;
        return info;
    }

    ctx->minimal_parse = temp_minimal_parse;
    ctx->is_sys_header = false;

    Token * default_replace_list = NULL;
    arrput(default_replace_list, ((Token){.start = "1", .length = 1, .type = TK_NUMBER}));

    for (int preop_i=0; preop_i < arrlen(cfg->pre_options); preop_i++) {
        PreOption pre_op = cfg->pre_options[preop_i];
        switch (pre_op.type) {
        case PRE_OP_DEFINE: {
            Define new_def = {.is_defined = true};
            Token * tklist = create_token_list(pre_op.string);
            if (arrlen(tklist) > 2) {
                if (!(arrlen(tklist) >= 4 && tklist[1].type == TK_EQUAL)) {
                    fprintf(stderr, "Invalid -D option.\n");
                    exit(1);
                }

                (void) arrpop(tklist); // remove TK_END token

                new_def.key = copy_and_terminate(ctx->arena, tklist[0].start, tklist[0].length);

                arrdeln(tklist, 0, 2); // remove NAME = tokens
                new_def.replace_list = tklist;
            } else {
                new_def.key = pre_op.string;
                new_def.replace_list = default_replace_list;
                arrfree(tklist);
            }
            shputs(ctx->defines, new_def);
        }break;

        case PRE_OP_UNDEFINE: {
            (void) shdel(ctx->defines, pre_op.string);
        }break;

        case PRE_OP_INPUT_FILE: {
            int ret = preprocess_set_file(ctx, pre_op.string);
            if (ret) {
                if (ret == RET_FILE_NOT_FOUND) {
                    fprintf(stderr, "File not found.\n");
                }
                info.ret = -1;
                return info;
            }
            ret = preprocess_file(ctx);
            if (ret) {
                info.ret = ret;
                return info;
            }
        }break;
        }
    }

    Token endtk = {.type = TK_END};
    arrput(ctx->result_list, endtk);

    Define lib_version_macro = shgets(ctx->defines, "INTRO_LIB_VERSION");
    if (lib_version_macro.is_defined) {
        Token lib_version_tk = lib_version_macro.replace_list[0];
        long input_lib_version = strtol(lib_version_tk.start, NULL, 10);
        long our_lib_version = INTRO_LIB_VERSION;

        if (our_lib_version / 100 != input_lib_version / 100) {
            preprocess_warning(&lib_version_tk, "intro.h version does not match generator version.");
            fprintf(stderr, "generator version: %s\n", VERSION);
        } else if (our_lib_version < input_lib_version) {
            preprocess_warning(&lib_version_tk, "intro.h has a lower patch level than the generator.");
            fprintf(stderr, "generator version: %s\n", VERSION);
        }
    }

    if (cfg->m_options.enabled) {
        const char * filepath = cfg->first_input_filename;
        char * ext = strrchr(filepath, '.');
        int len_basename = (ext)? ext - filepath : strlen(filepath);
        if (cfg->m_options.D && !cfg->m_options.filename) {
            char * dep_file = NULL;
            strputf(&dep_file, "%.*s.d", len_basename, filepath);
            cfg->m_options.filename = dep_file;
        }

        char * rule = NULL;
        char * dummy_rules = NULL;
        if (cfg->m_options.target_mode == MT_NORMAL) {
            if (cfg->m_options.custom_target) {
                strputf(&rule, "%s: ", cfg->m_options.custom_target);
            } else {
                strputf(&rule, "%.*s.o: ", len_basename, filepath);
            }
        }
        const char * sep = " ";
        if (cfg->m_options.target_mode == MT_NEWLINE) {
            sep = "\n";
        }
        strputf(&rule, "%s", filepath);
        for (int i=0; i < shlen(ctx->dependency_set); i++) {
            char buf [1024];
            const char * path = ctx->dependency_set[i].key;
            if (cfg->m_options.use_msys_path) {
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
            if (cfg->m_options.P) {
                strputf(&dummy_rules, "%s:\n", path);
            }
        }
        strputf(&rule, "\n");
        if (dummy_rules != NULL) {
            strputf(&rule, "%.*s", (int)arrlen(dummy_rules), dummy_rules);
        }

        if (ctx->preprocess_only && !cfg->m_options.filename) {
            fputs(rule, stdout);
            exit(0);
        }

        if (cfg->m_options.filename == NULL) {
            fprintf(stderr, "Somehow, intro cannot figure out what to do with the dependency information.\n");
            exit(1);
        }
        int error = intro_dump_file(cfg->m_options.filename, rule, arrlen(rule));
        if (error < 0) {
            fprintf(stderr, "Failed to write dependencies to '%s'\n", cfg->m_options.filename);
            exit(1);
        }
    }

    g_metrics.count_parse_tokens = arrlen(ctx->result_list);
    g_metrics.pre_time += nanointerval();
    g_metrics.count_pre_files = arrlen(ctx->loc.file_buffers);

    if (ctx->preprocess_only) {
        char * pretext = NULL;
        arrsetcap(pretext, 1 << 12);
        for (int i=0; i < arrlen(ctx->result_list); i++) {
            Token tk = ctx->result_list[i];
            if (tk.preceding_space) arrput(pretext, ' ');
            char * out = arraddnptr(pretext, tk.length);
            memcpy(out, tk.start, tk.length);
        }
        fwrite(pretext, 1, arrlen(pretext), stdout);

        g_metrics.gen_time += nanointerval();
        if (cfg->show_metrics) {
            show_metrics();
        }

        exit(0);
    }

    for (int def_i=0; def_i < shlen(ctx->defines); def_i++) {
        Define def = ctx->defines[def_i];
        if (def.file) {
            char * replace_text = NULL;
            if (def.replace_list) {
                strput_tokens(&replace_text, def.replace_list);
            }
            IntroMacro macro = {
                .name = def.key,
                .parameters = (const char **)def.arg_list,
                .count_parameters = def.arg_count,
                .replace = replace_text,
                .location = {.path = def.file->filename, .offset = def.file_offset},
            };
            arrput(info.macros, macro);
        }
    }

    for (int i=0; i < arrlen(ctx->result_list); i++) {
        ctx->result_list[i].index = i;
    }

    info.loc = ctx->loc;
    info.loc.index = 0;
    info.loc.count = arrlen(ctx->loc.list);
    info.result_list = ctx->result_list;
    g_metrics.pre_time += nanointerval();
    return info;
}
