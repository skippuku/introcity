#include "lib/intro.h"
#include "lexer.c"
#include "global.c"

static const IntroType known_types [] = {
    {INTRO_UNKNOWN, 0, {}, 0, 0, "void"},
    {INTRO_U8, 0, {}, 0, 0,  "uint8_t",  0, 1, 1},
    {INTRO_U16, 0, {}, 0, 0, "uint16_t", 0, 2, 2},
    {INTRO_U32, 0, {}, 0, 0, "uint32_t", 0, 4, 4},
    {INTRO_U64, 0, {}, 0, 0, "uint64_t", 0, 8, 8},
    {INTRO_S8, 0, {}, 0, 0,  "int8_t",   0, 1, 1},
    {INTRO_S16, 0, {}, 0, 0, "int16_t",  0, 2, 2},
    {INTRO_S32, 0, {}, 0, 0, "int32_t",  0, 4, 4},
    {INTRO_S64, 0, {}, 0, 0, "int64_t",  0, 8, 8},
    {INTRO_F32, 0, {}, 0, 0, "float",    0, 4, 4},
    {INTRO_F64, 0, {}, 0, 0, "double",   0, 8, 8},
    {INTRO_U8, 0, {}, 0, 0,  "bool",     0, 1, 1},
    {INTRO_VA_LIST, 0, {}, 0, 0, "va_list"},
    {INTRO_VA_LIST, 0, {}, 0, 0, "__builtin_va_list"},
};

typedef struct {
    uint32_t id;
    uint32_t final_id;
    IntroType * type_ptr;
    IntroAttributeType type;
    bool builtin;
    bool global;
    bool repress;
    bool without_namespace;
    bool invalid_without_namespace;
    bool next_is_same;
} AttributeParseInfo;

enum SpecialMemberIndex {
    MIDX_TYPE = INT32_MIN,
    MIDX_ALL,
    MIDX_ALL_RECURSE,
};

typedef struct {
    uint32_t id;
    union {
        int32_t i;
        float f;
    } v;
} AttributeData;

typedef struct {
    IntroType * type;
    int32_t location;
    int32_t member_index;
    uint32_t count;
    AttributeData * attr_data;
} AttributeDirective;

typedef struct {
    IntroType * type;
    ptrdiff_t member_index;
} AttributeDataKey;

typedef struct {
    AttributeDataKey key;
    AttributeData * value;
} AttributeDataMap;

typedef struct {
    ptrdiff_t value_offset;
    void * data;
    size_t data_size;
} PtrStore;

typedef struct {
    IntroType * type;
    int32_t member_index, attr_id;
    uint32_t value;
} DeferredDefault;

struct ParseContext {
    Token * tk_list;
    MemArena * arena;
    NameSet * ignore_typedefs;
    struct{char * key; IntroType * value;} * type_map;
    struct{IntroType key; IntroType * value;} * type_set;
    NameSet * keyword_set;
    NameSet * enum_name_set;

    uint8_t * value_buffer;
    PtrStore * ptr_stores;

    DeferredDefault * deferred_length_defaults;

    ExprContext * expr_ctx;
    LocationContext loc;

    struct {size_t key; IntroTypePtrList * value;} * arg_list_by_hash;
    struct {char * key; IntroFunction * value;} * function_map;
    struct {IntroType * key; IntroType ** value;} * incomplete_typedefs;

    struct{ char * key; AttributeParseInfo value; } * attribute_map;
    struct{ char * key; int value; } * attribute_token_map;
    struct{ char * key; int value; } * builtin_map;
    AttributeData * attribute_globals;
    AttributeDirective * attribute_directives;
    AttributeDataMap * attribute_data_map;
    IntroBuiltinAttributeIds builtin;
    char ** string_set;
    uint32_t attribute_id_counter;
    ParseInfo * p_info;
};

static void
parse_error(ParseContext * ctx, Token tk, char * message) { // TODO
    parse_msg_internal(&ctx->loc, tk, message, 0);
}

static void UNUSED
parse_warning(ParseContext * ctx, Token tk, char * message) {
    parse_msg_internal(&ctx->loc, tk, message, 1);
}

static intmax_t parse_constant_expression(ParseContext * ctx, TokenIndex * tidx);

#include "attribute.c"

static const char *
register_enum_name(ParseContext * ctx, Token tk) {
    STACK_TERMINATE(name, tk.start, tk.length);
    ptrdiff_t index = shgeti(ctx->enum_name_set, name);
    if (index < 0) {
        shputs(ctx->enum_name_set, (NameSet){name});
        index = shtemp(ctx->enum_name_set);
        return ctx->enum_name_set[index].key;
    } else {
        parse_error(ctx, tk, "Enum name is reserved.");
        exit(1);
    }
}

static bool
funcs_are_equal(const IntroType * a, const IntroType * b) {
    if (intro_origin(a->of) != intro_origin(b->of)) return false;
    if (a->args->count != b->args->count) return false;
    for (int i=0; i < a->args->count; i++) {
        const IntroType * a_arg = intro_origin(a->args->types[i]);
        const IntroType * b_arg = intro_origin(b->args->types[i]);
        while (a_arg->of) {
            if (a_arg->category == b_arg->category) {
                a_arg = a_arg->of;
                b_arg = b_arg->of;
            } else {
                return false;
            }
            a_arg = intro_origin(a_arg);
            b_arg = intro_origin(b_arg);
        }
        if (a_arg != b_arg) {
            return false;
        }
    }
    return true;
}

static IntroTypePtrList *
store_arg_type_list(ParseContext * ctx, IntroType ** list) {
    static const size_t hash_seed = 0xF58D6349C6431963; // this is just some random number

    size_t count_bytes = arrlen(list) * sizeof(list[0]);
    size_t hash = (count_bytes > 0)? stbds_hash_bytes(list, count_bytes, hash_seed) : 0;
    IntroTypePtrList * stored = hmget(ctx->arg_list_by_hash, hash);
    if (stored) {
        // TODO: actually handle this somehow instead of aborting
        if (count_bytes > 0) {
            assert(0 == memcmp(stored->types, list, count_bytes));
        }
    } else {
        stored = arena_alloc(ctx->arena, sizeof(*stored) + count_bytes);
        stored->count = arrlen(list);
        if (count_bytes > 0) {
            memcpy(stored->types, list, count_bytes);
        }
        hmput(ctx->arg_list_by_hash, hash, stored);
    }

    return stored;
}

static IntroType *
store_type(ParseContext * ctx, IntroType type, char * pos) {
    IntroType * stored = NULL;
    bool replaced = false;

    if (pos) {
#if 0
        IntroLocation loc = {0};
        NoticeState notice;
        char * start_of_line;
        FileInfo * file = get_line(&ctx->loc, ctx->buffer, &pos, &loc.line, &start_of_line, &notice);
        if ((notice & NOTICE_ENABLED)) {
            type.flags |= INTRO_EXPLICITLY_GENERATED;
        }
        loc.path = file->filename;
        loc.column = pos - start_of_line;
        db_assert(loc.line >= 0);
        db_assert(loc.column >= 0);
        type.location = loc;
#endif
            type.flags |= INTRO_EXPLICITLY_GENERATED;
    }

    if (type.name) {
        ptrdiff_t index = shgeti(ctx->type_map, type.name);
        if (index >= 0) {
            stored = ctx->type_map[index].value;

            (void) hmdel(ctx->type_set, *stored);

            type.name = stored->name; // retain allocated key string
            replaced = true;
        }
    }

    ptrdiff_t set_index = hmgeti(ctx->type_set, type);
    if (set_index < 0) {
        if (!stored) {
            stored = arena_alloc(ctx->arena, sizeof(*stored));
            if (type.name) {
                shput(ctx->type_map, type.name, stored);
                type.name = ctx->type_map[shtemp(ctx->type_map)].key;
            }
        }
        hmput(ctx->type_set, type, stored);
        *stored = type;
    } else {
        stored = ctx->type_set[set_index].value;
    }

    if (replaced) {
        IntroType ** typedef_deps = hmget(ctx->incomplete_typedefs, stored);
        if (typedef_deps) {
            for (int i=0; i < stbds_header(typedef_deps)->length; i++) {
                IntroType * t = typedef_deps[i];
                t->category = type.category;
                t->i_struct = type.i_struct; // covers i_enum
                t->align = type.align;
                t->size = type.size;
            }
            arrfree(typedef_deps);
            (void) hmdel(ctx->incomplete_typedefs, stored);
        }
    }

    return stored;
}

bool
maybe_expect_attribute(ParseContext * ctx, TokenIndex * tidx, int32_t member_index, Token * o_tk) {
    bool had_application = false;
    while (o_tk->type == TK_IDENTIFIER && tk_equal(o_tk, "I")) {
        int32_t paren_index = tidx->index;
        Token paren = next_token(tidx);
        if (paren.type != TK_L_PARENTHESIS) {
            parse_error(ctx, paren, "Expected '('.");
            exit(1);
        }

        *o_tk = next_token(tidx);
        if (tk_equal(o_tk, "attribute") || tk_equal(o_tk, "apply_to")) {
            tidx->index = paren_index;
            parse_global_directive(ctx, tidx);
            *o_tk = next_token(tidx);
        } else {
            AttributeDirective directive = {
                .type = NULL,
                .location = paren_index,
                .member_index = member_index,
            };
            arrput(ctx->attribute_directives, directive);
            int32_t closing = find_closing((TokenIndex){.list = tidx->list, .index = paren_index});
            if (closing == 0) {
                parse_error(ctx, paren, "Missing closing ')'.");
                exit(1);
            }
            tidx->index = closing;
            *o_tk = next_token(tidx);

            had_application = true;
        }
    }
    return had_application;
}

static int
get_keyword(ParseContext * ctx, Token tk) {
    if (tk.type != TK_IDENTIFIER) return -1;
    STACK_TERMINATE(terminated, tk.start, tk.length);
    return shgeti(ctx->keyword_set, terminated);
}

static bool
is_ignored(int keyword) {
    switch(keyword) {
    case KEYW_STATIC:
    case KEYW_CONST:
    case KEYW_VOLATILE:
    case KEYW_EXTERN:
    case KEYW_INLINE:
    case KEYW_RESTRICT:
        return true;
    default:
        return false;
    }
}

static intmax_t
parse_constant_expression(ParseContext * ctx, TokenIndex * tidx) {
    Token * tks = NULL;
    Token tk;
    int depth = 0;
    while (1) {
        tk = next_token(tidx);
        if (tk.type == TK_END) {
            parse_error(ctx, tk, "End reached unexpectedly.");
            exit(1);
        }
        if (tk.type == TK_L_PARENTHESIS) {
            depth += 1;
        } else if (tk.type == TK_R_PARENTHESIS) {
            depth -= 1;
            if (depth < 0) {
                tidx->index--;
                break;
            }
        } else if (tk.type == TK_COMMA || tk.type == TK_R_BRACE || tk.type == TK_SEMICOLON || tk.type == TK_R_BRACKET) {
            tidx->index--;
            break;
        }
        arrput(tks, tk);
    }
    ExprNode * tree = build_expression_tree(ctx->expr_ctx, tks, arrlen(tks), &tk);
    arrfree(tks);
    if (!tree) {
        parse_error(ctx, tk, "Unknown value in expression.");
        exit(1);
    }
    ExprProcedure * expr = build_expression_procedure(tree);
    intmax_t result = run_expression(expr);

    free(expr);
    reset_arena(ctx->expr_ctx->arena);

    return result;
}

static int
parse_struct(ParseContext * ctx, TokenIndex * tidx) {
    Token tk = next_token(tidx), name_tk = {0};

    bool is_union;
    if (tk_equal(&tk, "struct")) {
        is_union = false;
    } else if (tk_equal(&tk, "union")) {
        is_union = true;
    } else {
        return -1;
    }
    char * position = tk.start;

    char * complex_type_name = NULL;
    tk = next_token(tidx);
    if (tk.type == TK_IDENTIFIER) {
        name_tk = tk;
        tk = next_token(tidx);

        strputf(&complex_type_name, "%s %.*s",
                (is_union)? "union" : "struct", name_tk.length, name_tk.start);

        IntroType * stored = shget(ctx->type_map, complex_type_name);
        if (!stored) {
            IntroType temp_type = {0};
            temp_type.name = complex_type_name;
            store_type(ctx, temp_type, NULL);
        }
    }

    if (tk.type != TK_L_BRACE) {
        if (tk.type == TK_IDENTIFIER || tk.type == TK_STAR || tk.type == TK_SEMICOLON) {
            arrfree(complex_type_name);
            return RET_NOT_DEFINITION;
        }
        parse_error(ctx, tk, "Expected '{'.");
        return -1;
    }

    int start_attribute_directives = arrlen(ctx->attribute_directives);
    IntroMember * members = NULL;
    DeclState decl = {
        .state = DECL_MEMBERS,
    };
    uint32_t total_size = 0, total_align = 1;
    while (1) {
        decl.member_index = arrlen(members);
        int ret = parse_declaration(ctx, tidx, &decl);
        if (ret != RET_DECL_CONTINUE) {
            if (ret == RET_DECL_FINISHED) {
                break;
            }
            if (ret >= 0 && decl.base_tk.start) {
                parse_error(ctx, decl.base_tk, "Invalid.");
            }
            return -1;
        }
        if (decl.type->category == INTRO_UNKNOWN) {
            parse_error(ctx, decl.base_tk, "Unknown type.");
            return -1;
        }

        IntroMember member = {0};
        member.type = decl.type;
        //member.bitfield = decl.bitfield; // TODO

        assert(member.type->align != 0);
        if (total_align < member.type->align) {
            total_align = member.type->align;
        }
        if (!is_union) {
            total_size += (member.type->align - (total_size % member.type->align)) % member.type->align;
            member.offset = total_size;
            total_size += member.type->size;
        } else {
            if (total_size < member.type->size) {
                total_size = member.type->size;
            }
            member.offset = 0;
        }

        if (decl.name_tk.start) {
            member.name = copy_and_terminate(ctx->arena, decl.name_tk.start, decl.name_tk.length);
        } else {
            if (decl.type->category == INTRO_STRUCT || decl.type->category == INTRO_UNION) {
                IntroStruct * s = decl.type->i_struct;
                total_size += (decl.type->align - (total_size % decl.type->align)) % decl.type->align;
                int member_index_offset = arrlen(members);
                for (int i=0; i < s->count_members; i++) {
                    IntroMember embed_member = s->members[i];
                    embed_member.offset = member.offset + embed_member.offset;
                    arrput(members, embed_member);
                }
                // correct member_index for attributes applied to members in the embedded type
                for (int i = start_attribute_directives; i < arrlen(ctx->attribute_directives); i++) {
                    AttributeDirective * p_directive = &ctx->attribute_directives[i];
                    if (p_directive->type == decl.type) {
                        p_directive->type = NULL;
                        p_directive->member_index += member_index_offset;
                    }
                }
                continue;
            } else {
                parse_error(ctx, tk, "Struct member has no name, or type is unknown.");
                return -1;
            }
        }

        arrput(members, member);
    }

    total_size += (total_align - (total_size % total_align)) % total_align;

    IntroStruct * result = arena_alloc(ctx->arena, sizeof(IntroStruct) + sizeof(IntroMember) * arrlen(members));
    result->count_members = arrlen(members);
    result->is_union = is_union;
    memcpy(result->members, members, sizeof(IntroMember) * arrlen(members));
    arrfree(members);

    IntroType * stored;
    {
        IntroType type = {0};
        type.name = complex_type_name;
        type.category = (is_union)? INTRO_UNION : INTRO_STRUCT;
        type.i_struct = result;
        type.size = total_size;
        type.align = total_align;
        
        stored = store_type(ctx, type, position);
        arrfree(complex_type_name);
    }

    if (arrlen(ctx->attribute_directives) > start_attribute_directives) {
        for (int i = start_attribute_directives; i < arrlen(ctx->attribute_directives); i++) {
            AttributeDirective * p_directive = &ctx->attribute_directives[i];
            if (p_directive->type == NULL) { // this will be set if it is from a nested struct definition
                p_directive->type = stored;
            }
        }
    }

    return 0;
}

static int
parse_enum(ParseContext * ctx, TokenIndex * tidx) {
    IntroEnum enum_ = {0};

    char * position = tk_at(tidx).start;
    char * complex_type_name = NULL;
    Token tk = next_token(tidx), name_tk = {0};
    if (tk.type == TK_IDENTIFIER) {
        Token next = next_token(tidx);
        if (next.type != TK_L_PARENTHESIS) {
            name_tk = tk;
            strputf(&complex_type_name, "enum %.*s", name_tk.length, name_tk.start);

            if (shgeti(ctx->type_map, complex_type_name) < 0) {
                IntroType temp_type = {0};
                temp_type.name = complex_type_name;
                store_type(ctx, temp_type, NULL);
            }
            tk = next;
        } else {
            tidx->index--;
        }
    }

    if (tk.type != TK_L_BRACE) {
        if (tk.type == TK_IDENTIFIER || tk.type == TK_STAR || tk.type == TK_SEMICOLON) return RET_NOT_DEFINITION;
        parse_error(ctx, tk, "Expected '{'.");
        return -1;
    }

    enum_.is_flags = true;
    enum_.is_sequential = true;
    IntroEnumValue * members = NULL;
    int next_int = 0;
    int mask = 0;
    while (1) {
        IntroEnumValue v = {0};
        Token name = next_token(tidx);
        if (name.type == TK_R_BRACE) {
            break;
        }
        if (name.type != TK_IDENTIFIER) {
            parse_error(ctx, name, "Expected identifier.");
            return -1;
        }

        v.name = register_enum_name(ctx, name);

        tk = next_token(tidx);
        bool set = false;
        bool is_last = false;
        if (tk.type == TK_COMMA) {
            v.value = next_int++;
        } else if (tk.type == TK_EQUAL) {
            v.value = (int)parse_constant_expression(ctx, tidx);
            if (v.value != next_int) {
                enum_.is_sequential = false;
            }
            next_int = (v.value < INT32_MAX)? v.value + 1 : 0;
            set = true;
        } else if (tk.type == TK_R_BRACE) {
            v.value = next_int;
            is_last = true;
        } else {
            parse_error(ctx, tk, "Unexpected symbol.");
            return -1;
        }

        if (mask & v.value) enum_.is_flags = false;
        mask |= v.value;

        arrput(members, v);
        shput(ctx->expr_ctx->constant_map, v.name, (intmax_t)v.value);

        if (is_last) break;

        if (set) {
            tk = next_token(tidx);
            if (tk.type == TK_COMMA) {
            } else if (tk.type == TK_R_BRACE) {
                break;
            } else {
                parse_error(ctx, tk, "Unexpected symbol.");
                return -1;
            }
        }
    }
    enum_.count_members = arrlen(members);

    IntroEnum * result = arena_alloc(ctx->arena, sizeof(IntroEnum) + sizeof(*members) * arrlen(members));
    memcpy(result, &enum_, sizeof(IntroEnum));
    memcpy(result->members, members, sizeof(*members) * arrlen(members));
    arrfree(members);

    {
        IntroType type = {0};
        type.name = complex_type_name;
        type.category = INTRO_ENUM;
        type.i_enum = result;
        type.size = 4; // TODO: this could be other things in C++
        type.align = type.size;

        store_type(ctx, type, position);
        arrfree(complex_type_name);
    }

    return 0;
}

int
parse_type_base(ParseContext * ctx, TokenIndex * tidx, DeclState * decl) {
    IntroType type = {0};
    char * type_name = NULL;
    bool is_typedef = false;
    int first_keyword = -1;

    Token first;
    while (1) {
        first = next_token(tidx);
        if (first.type == TK_IDENTIFIER) {
            first_keyword = get_keyword(ctx, first);
            if (!is_ignored(first_keyword)) {
                if (!is_typedef && first_keyword == KEYW_TYPEDEF) {
                    is_typedef = true;
                } else {
                    break;
                }
            }
        } else if (first.type == TK_END) {
            return RET_FOUND_END;
        } else {
            break;
        }
    }
    int32_t first_index = tidx->index - 1;
    if (first.type != TK_IDENTIFIER) {
        if (decl->state == DECL_ARGS && first.type == TK_PERIOD) {
            for (int i=0; i < 2; i++) {
                Token next = next_token(tidx);
                if (next.type != TK_PERIOD) {
                    parse_error(ctx, first, "Invalid symbol in parameter list.");
                    return -1;
                }
            }
            static const IntroType i_va_list = {
                .name = "...",
                .category = INTRO_VA_LIST,
            };
            decl->base = (IntroType *)&i_va_list;
            decl->base_tk.start = first.start;
            decl->base_tk.length = 3;
            return RET_DECL_VA_LIST;
        }
        tidx->index = first_index;
        return RET_NOT_TYPE;
    }
    decl->base_tk = first;
    strputf(&type_name, "%.*s", first.length, first.start);

    if (first_keyword == KEYW_STRUCT
      ||first_keyword == KEYW_UNION
      ||first_keyword == KEYW_ENUM) {
        int32_t after_keyword = tidx->index;
        int error;
        if (first_keyword == KEYW_STRUCT || first_keyword == KEYW_UNION) {
            tidx->index = first_index;
            error = parse_struct(ctx, tidx);
        } else {
            error = parse_enum(ctx, tidx);
        }
        if (error == RET_NOT_DEFINITION) {
            tidx->index = after_keyword;
            Token tk = next_token(tidx);
            strputf(&type_name, " %.*s", tk.length, tk.start);

            decl->base_tk.length = tk.start - first.start + tk.length;
        } else if (error != 0) {
            return -1;
        } else {
            arrfree(type_name);
            ptrdiff_t last_index = hmtemp(ctx->type_set);
            decl->base = ctx->type_set[last_index].value;
            if (is_typedef) decl->state = DECL_TYPEDEF;
            return 0;
        }
    } else {
#define CHECK_INT(x) \
    if (x) { \
        parse_error(ctx, tk, "Invalid."); \
        return -1; \
    }
        Token tk = first, ltk = first;
        while (1) {
            bool add_to_name = (tk.start != first.start);
            bool break_loop = false;
            int keyword = get_keyword(ctx, tk);
            switch(keyword) {
            case KEYW_UNSIGNED: {
                CHECK_INT((type.category & 0xf0));
                type.category |= INTRO_UNSIGNED;
            }break;

            case KEYW_SIGNED: {
                CHECK_INT((type.category & 0xf0));
                type.category |= INTRO_SIGNED;
            }break;

            case KEYW_LONG: {
                CHECK_INT((type.category & 0x0f));
                int32_t tk2_index = tidx->index;
                Token tk2 = next_token(tidx);
                if (tk_equal(&tk2, "long")) {
                    strputf(&type_name, " long");
                    tk = tk2;
                } else if (tk_equal(&tk2, "double")) {
                    type.category = INTRO_F128;
                    type.size = 16;
                    type.align = 16;
                    tk = tk2;
                    strputf(&type_name, " %.*s", tk.length, tk.start);
                    break;
                } else {
                    tidx->index = tk2_index;
                }
                type.category |= 0x08;
            }break;

            case KEYW_MS_INT64: {
                CHECK_INT((type.category & 0x0f));
                type.category |= 0x08;
            }break;

            case KEYW_MS_INT32: {
                CHECK_INT((type.category & 0x0f));
                type.category |= 0x04;
            }break;

            case KEYW_SHORT: {
                CHECK_INT((type.category & 0x0f));
                type.category |= 0x02;
            }break;

            case KEYW_CHAR: {
                CHECK_INT((type.category & 0x0f));
                type.category |= 0x01;
                break_loop = true;
            }break;

            case KEYW_INT: {
                CHECK_INT((type.category & 0x0f) == 0x01);
                if ((type.category & 0x0f) == 0) {
                    type.category |= 0x04;
                }
                break_loop = true;
            }break;

            default: {
                if (add_to_name) tidx->index--;
                tk = ltk;
                break_loop = true;
                add_to_name = false;
            }break;
            }
            if (add_to_name) strputf(&type_name, " %.*s", tk.length, tk.start);
            if (break_loop) break;
            ltk = tk;
            tk = next_token(tidx);
        }
#undef CHECK_INT

        if (type.category) {
            if ((type.category & 0xf0) == 0) {
                type.category |= 0x20;
            }
            if ((type.category & 0x0f) == 0) {
                type.category |= 0x04;
            }
            int parent_index = 0;
            switch(type.category) {
            case INTRO_U8:  parent_index = 1; break;
            case INTRO_U16: parent_index = 2; break;
            case INTRO_U32: parent_index = 3; break;
            case INTRO_U64: parent_index = 4; break;
            case INTRO_S8:  parent_index = 5; break;
            case INTRO_S16: parent_index = 6; break;
            case INTRO_S32: parent_index = 7; break;
            case INTRO_S64: parent_index = 8; break;
            default: break;
            }
            if (parent_index) {
                type.parent = ctx->type_set[parent_index].value;
                type.size  = type.parent->size;
                type.align = type.parent->align;
            }
        }

        decl->base_tk.length = tk.start - first.start + tk.length;
    }

    if (is_typedef) decl->state = DECL_TYPEDEF;

    IntroType * t = shget(ctx->type_map, type_name);
    if (t) {
        arrfree(type_name);
        decl->base = t;
    } else {
        if (type.category || is_typedef) {
            type.name = type_name;
            IntroType * stored = store_type(ctx, type, NULL);
            arrfree(type_name);
            decl->base = stored;
        } else {
            parse_error(ctx, decl->base_tk, "Undeclared type.");
            return -1;
        }
    }
    return 0;
}

static IntroType ** parse_function_arguments(ParseContext * ctx, TokenIndex * tidx, DeclState * parent_decl);

static int
parse_type_annex(ParseContext * ctx, TokenIndex * tidx, DeclState * decl) {
    int32_t * indirection = NULL;
    int32_t * temp = NULL;
    IntroType *** func_args_stack = NULL;
    int32_t paren;

    const int32_t POINTER = -1;
    const int32_t FUNCTION = -2;
    Token tk;
    int32_t end = tidx->index;
    memset(&decl->name_tk, 0, sizeof(decl->name_tk));
    do {
        paren = -1;

        int pointer_level = 0;
        while (1) {
            tk = next_token(tidx);
            if (tk.type == TK_STAR) {
                pointer_level += 1;
            } else if (is_ignored(get_keyword(ctx, tk))) {
                continue;
            } else {
                break;
            }
        }

        if (tk.type == TK_L_PARENTHESIS) {
            paren = tidx->index;
            tidx->index = find_closing((TokenIndex){.list = tidx->list, .index = tidx->index - 1});
            if (tidx->index == 0) {
                parse_error(ctx, tk, "No closing ')'.");
                return -1;
            }
            tk = next_token(tidx);
        }

        if (tk.type == TK_IDENTIFIER) {
            decl->name_tk = tk;
            tk = next_token(tidx);
        }

        arrsetlen(temp, 0);
        if (tk.type == TK_L_PARENTHESIS) {
            IntroType ** arg_types = parse_function_arguments(ctx, tidx, decl);
            arrpush(func_args_stack, arg_types);
            arrput(temp, FUNCTION);
            decl->func_specifies_args = true;
            tk = next_token(tidx);
        }

        while (tk.type == TK_L_BRACKET) {
            TokenIndex tempidx = {.list = tidx->list, .index = tidx->index - 1};
            int32_t closing_bracket = find_closing(tempidx) - 1;
            if (closing_bracket == 0) {
                parse_error(ctx, tk, "No closing ')'.");
                return -1;
            }
            int32_t num;
            if (closing_bracket == tidx->index) {
                num = 0;
            } else {
                num = (int32_t)parse_constant_expression(ctx, tidx);
                if (num < 0) {
                    parse_error(ctx, tk, "Invalid array size.");
                    return -1;
                }
            }
            arrput(temp, num);
            tidx->index = closing_bracket + 1;
            tk = next_token(tidx);
        }

        if (tidx->index > end) end = tidx->index;

        for (int i=0; i < pointer_level; i++) {
            arrput(indirection, POINTER);
        }
        for (int i = arrlen(temp) - 1; i >= 0; i--) {
            arrput(indirection, temp[i]);
        }
    } while ((tidx->index = paren) != -1);
    arrfree(temp);

    IntroType * last_type = decl->base;
    for (int i=0; i < arrlen(indirection); i++) {
        int32_t it = indirection[i];
        IntroType new_type;
        memset(&new_type, 0, sizeof(new_type));
        if (it == POINTER) {
            new_type.category = INTRO_POINTER;
            new_type.of = last_type;
            new_type.size = 8; // TODO: base on architecture
            new_type.align = 8;
        } else if (it == FUNCTION) {
            IntroType ** arg_types = arrpop(func_args_stack);
            IntroTypePtrList * stored_args = store_arg_type_list(ctx, arg_types);
            arrfree(arg_types);

            new_type.of = (IntroType *)intro_origin(last_type); // return type
            new_type.category = INTRO_FUNCTION;
            new_type.args = stored_args;
        } else {
            new_type.category = INTRO_ARRAY;
            new_type.of = last_type;
            new_type.array_size = it;
            new_type.size = last_type->size * new_type.array_size;
            new_type.align = last_type->align;
        }
        last_type = store_type(ctx, new_type, NULL);
    }
    arrfree(indirection);
    arrfree(func_args_stack);

    tidx->index = end - 1;
    decl->type = last_type;
    return 0;
}

static int
parse_declaration(ParseContext * ctx, TokenIndex * tidx, DeclState * decl) {
    IntroFunction * func = NULL;
    int ret = 0;
    bool attribute_at_start = false;

    if (decl->state != DECL_MEMBERS) {
        decl->member_index = MIDX_TYPE;
    }
    Token tk = next_token(tidx);
    attribute_at_start = maybe_expect_attribute(ctx, tidx, decl->member_index, &tk);

    tidx->index--;
    if (!decl->reuse_base) ret = parse_type_base(ctx, tidx, decl);
    if (ret < 0 || ret == RET_FOUND_END) return ret;
    if (ret == RET_NOT_TYPE) {
        tk = next_token(tidx);
        bool finished = (tk.type == TK_R_PARENTHESIS)
                     || (tk.type == TK_SEMICOLON)
                     || (tk.type == TK_R_BRACE);
        if (finished) {
            return RET_DECL_FINISHED;
        } else if (decl->state == DECL_CAST) {
            return RET_NOT_TYPE;
        } else {
            parse_error(ctx, tk, "Invalid type.");
            return -1;
        }
    } else if (ret == RET_DECL_VA_LIST) {
        tk = next_token(tidx);
        if (tk.type != TK_R_PARENTHESIS) {
            parse_error(ctx, tk, "Expected ')' after va_list.");
            return -1;
        }
        return RET_DECL_FINISHED;
    }

    ret = parse_type_annex(ctx, tidx, decl);
    if (ret < 0) return -1;

    IntroType * typedef_type = NULL;
    if (decl->state == DECL_TYPEDEF) {
        char * name;
        if (decl->name_tk.start) {
            name = copy_and_terminate(ctx->arena, decl->name_tk.start, decl->name_tk.length);
        } else {
            //parse_error(ctx, &decl->base_tk, "typedef has no name."); // apperantly this is actually fine?
            name = NULL;
            goto find_end;
        }
        if (shgeti(ctx->ignore_typedefs, name) >= 0) {
            goto find_end;
        }
        IntroType * prev = shget(ctx->type_map, name);
        if (prev) {
            const IntroType * og_prev = intro_origin(prev);
            const IntroType * og_this = intro_origin(decl->type);
            bool effectively_equal = og_prev->category == og_this->category
                                  && og_prev->of == og_this->of
                                  && og_prev->__data == og_this->__data;
            if (!effectively_equal) {
                parse_error(ctx, decl->name_tk, "Redefinition does not match previous definition.");
                if (prev->location.path) {
                    location_note(&ctx->loc, prev->location, "Previous definition here.");
                }
                return -1;
            }
        } else {
            IntroType new_type = *decl->type;
            new_type.name = name;
            new_type.parent = decl->type;
            IntroType * stored = store_type(ctx, new_type, decl->name_tk.start);
            if (decl->type->category == INTRO_UNKNOWN) {
                IntroType ** list = hmget(ctx->incomplete_typedefs, decl->type);
                arrput(list, stored);
                hmput(ctx->incomplete_typedefs, decl->type, list);
            }
            typedef_type = stored;
        }
    }

    if (decl->type->category == INTRO_FUNCTION) {
        STACK_TERMINATE(terminated_name, decl->name_tk.start, decl->name_tk.length);
        IntroFunction * prev = shget(ctx->function_map, terminated_name);
        int32_t count_args = decl->type->args->count;
        if (decl->func_specifies_args) {
            if (prev) {
                if (!funcs_are_equal(decl->type, prev->type)) {
                    parse_error(ctx, decl->name_tk, "Function declaration does not match previous.");
                    if (prev->location.path) {
                        location_note(&ctx->loc, prev->location, "Previous definition here.");
                    }
                    return -1;
                } else {
                    //if (prev->has_body) {
                    //    parse_warning(ctx, decl->name_tk, "Function is redeclared after definition.");
                    //}
                    func = prev;
                }
            } else {
                func = calloc(1, sizeof(*func) + count_args * sizeof(func->arg_names[0]));
                func->name = copy_and_terminate(ctx->arena, decl->name_tk.start, decl->name_tk.length);
                func->type = decl->type;
                {
#if 0
                    IntroLocation loc = {0};
                    char * pos = decl->name_tk.start;
                    char * start_of_line;
                    NoticeState notice;
                    FileInfo * file = get_line(&ctx->loc, ctx->buffer, &pos, &loc.line, &start_of_line, &notice);
                    if ((notice & NOTICE_ENABLED) && (notice & NOTICE_FUNCTIONS)) {
                        func->flags |= INTRO_EXPLICITLY_GENERATED;
                    }
                    loc.path = file->filename;
                    loc.column = pos - start_of_line;
                    assert(loc.line >= 0);
                    assert(loc.column >= 0);
                    func->location = loc;
#endif
                }
                shput(ctx->function_map, func->name, func);
            }
            if (count_args > 0 && decl->arg_names) {
                memcpy(func->arg_names, decl->arg_names, count_args * sizeof(func->arg_names[0]));
                arrfree(decl->arg_names);
            }
        } else {
            assert(decl->arg_names == NULL);
        }
    }

find_end: ;
    bool in_expr = false;
    while (1) {
        tk = next_token(tidx);
        if (decl->state == DECL_MEMBERS) {
            maybe_expect_attribute(ctx, tidx, decl->member_index, &tk);
        } else if (typedef_type) {
            if (maybe_expect_attribute(ctx, tidx, MIDX_TYPE, &tk) || attribute_at_start) {
                arrlast(ctx->attribute_directives).type = typedef_type;
            }
        }
        if (tk.type == TK_COMMA) {
            if (decl->state != DECL_ARGS) {
                decl->reuse_base = true;
            }
            return RET_DECL_CONTINUE;
        } else if (tk.type == TK_SEMICOLON) {
            decl->reuse_base = false;
            if (decl->state == DECL_TYPEDEF) decl->state = DECL_GLOBAL;
            return RET_DECL_CONTINUE;
        } else if (tk.type == TK_EQUAL) {
            in_expr = true;
        } else if (tk.type == TK_COLON && decl->state == DECL_MEMBERS) {
            UNUSED intmax_t bitfield = parse_constant_expression(ctx, tidx);
            continue;
        } else {
            bool do_find_closing = false;
            bool func_body = false;
            if ((decl->state == DECL_CAST || decl->state == DECL_ARGS) && tk.type == TK_R_PARENTHESIS) {
                tidx->index--;
                return RET_DECL_CONTINUE;
            }
            if (in_expr) {
                if (tk.type == TK_L_BRACE || tk.type == TK_L_BRACKET || tk.type == TK_L_PARENTHESIS) {
                    do_find_closing = true;
                } else {
                    continue;
                }
            }
            if (tk.type == TK_L_BRACE && func) {
                assert(func != NULL);
                do_find_closing = true;
                func_body = true;
                func->has_body = true;
            }
            if (do_find_closing) {
                tidx->index = find_closing((TokenIndex){.list = tidx->list, .index = tidx->index - 1});
                if (tidx->index == 0) {
                    if (func_body) {
                        parse_error(ctx, tk, "No closing '}' for function body.");
                        if (decl->name_tk.start) {
                            parse_warning(ctx, decl->name_tk, "Function name here.");
                        }
                    } else {
                        parse_error(ctx, tk, "Parenthesis, bracket, or brace is not closed.");
                    }
                    return -1;
                }
                tk = next_token(tidx);
                if (tk.type == TK_END) {
                    return RET_FOUND_END;
                } else if (tk.type == TK_SEMICOLON || tk.type == TK_COMMA) {
                } else {
                    tidx->index -= 1;
                }
                int state = decl->state;
                memset(decl, 0, sizeof(*decl));
                decl->state = state;
                return RET_DECL_CONTINUE;
            }
            parse_error(ctx, tk, "Invalid symbol in declaration. Expected ',' or ';'.");
            return -1;
        }
    }
}

static IntroType **
parse_function_arguments(ParseContext * ctx, TokenIndex * tidx, DeclState * parent_decl) {
    IntroType ** arg_types = NULL;

    DeclState decl = {.state = DECL_ARGS};
    while (1) {
        int ret = parse_declaration(ctx, tidx, &decl);
        if (ret == RET_DECL_FINISHED || ret == RET_FOUND_END) {
            break;
        } else if (ret < 0) {
            exit(1);
        }

        if (decl.type->category == INTRO_UNKNOWN && 0==strcmp(decl.type->name, "void")) {
            continue;
        }

        char * name = (decl.name_tk.start)
                       ? copy_and_terminate(ctx->arena, decl.name_tk.start, decl.name_tk.length)
                       : NULL;

        arrput(arg_types, decl.type);
        arrput(parent_decl->arg_names, name);
    }

    return arg_types;
}

static void
add_to_gen_info(ParseContext * ctx, ParseInfo * info, IntroType * type) {
    ptrdiff_t map_index = hmgeti(info->index_by_ptr_map, type);
    if (map_index < 0) {
        arrput(info->types, type);
        hmput(info->index_by_ptr_map, (void *)type, arrlen(info->types) - 1);

        if (type->parent) {
            add_to_gen_info(ctx, info, type->parent);
        }
        if (type->of) {
            add_to_gen_info(ctx, info, type->of);
        }
        if (type->category == INTRO_STRUCT || type->category == INTRO_UNION) {
            for (int mi=0; mi < type->i_struct->count_members; mi++) {
                const IntroMember * m = &type->i_struct->members[mi];
                add_to_gen_info(ctx, info, m->type);
            }
        } else if (type->category == INTRO_FUNCTION) {
            for (int arg_i=0; arg_i < type->args->count; arg_i++) {
                add_to_gen_info(ctx, info, type->args->types[arg_i]);
                arrput(info->arg_lists, type->args);
            }
        }
    }
}

int
parse_preprocessed_tokens(PreInfo * pre_info, ParseInfo * o_info) {
    ParseContext * ctx = calloc(1, sizeof(ParseContext));
    ctx->tk_list = pre_info->result_list;
    ctx->arena = new_arena((1 << 20)); // 1mb buckets
    ctx->expr_ctx = calloc(1, sizeof(ExprContext));
    ctx->expr_ctx->arena = new_arena(EXPR_BUCKET_CAP);
    ctx->expr_ctx->mode = MODE_PARSE;
    ctx->expr_ctx->ctx = ctx;
    ctx->loc = pre_info->loc;

    sh_new_arena(ctx->type_map);
    sh_new_arena(ctx->enum_name_set);

    for (int i=0; i < LENGTH(known_types); i++) {
        store_type(ctx, known_types[i], NULL);
        shputs(ctx->ignore_typedefs, (NameSet){known_types[i].name});
    }

    // NOTE: this must be in the same order as the enum definition in global.h
    static const struct{char * key; Keyword value;} keywords [] = {
        {"const",    KEYW_CONST},
        {"static",   KEYW_STATIC},
        {"struct",   KEYW_STRUCT},
        {"union",    KEYW_UNION},
        {"enum",     KEYW_ENUM},
        {"typedef",  KEYW_TYPEDEF},
        {"volatile", KEYW_VOLATILE},
        {"inline",   KEYW_INLINE},
        {"restrict", KEYW_RESTRICT},
        {"extern",   KEYW_EXTERN},

        {"unsigned", KEYW_UNSIGNED},
        {"signed",   KEYW_SIGNED},
        {"int",      KEYW_INT},
        {"long",     KEYW_LONG},
        {"char",     KEYW_CHAR},
        {"short",    KEYW_SHORT},
        {"float",    KEYW_FLOAT},
        {"double",   KEYW_DOUBLE},
        {"__int32",  KEYW_MS_INT32},
        {"__int64",  KEYW_MS_INT64},
    };
    for (int i=0; i < LENGTH(keywords); i++) {
        shputs(ctx->keyword_set, (NameSet){keywords[i].key});
        db_assert(shgeti(ctx->keyword_set, keywords[i].key) == keywords[i].value);
    }

    attribute_parse_init(ctx);

    DeclState decl = {.state = DECL_GLOBAL};

    TokenIndex _tidx = {.list = pre_info->result_list, .index = 0}, * tidx = &_tidx;
    while (1) {
        int ret = parse_declaration(ctx, tidx, &decl);

        if (ret == RET_FOUND_END) {
            break;
        } else if (ret < 0) {
            return -1;
        }
    }

    o_info->types = NULL;
    o_info->index_by_ptr_map = NULL;
    for (int i=0; i < hmlen(ctx->type_set); i++) {
        IntroType * type_ptr = ctx->type_set[i].value;
        if (i < LENGTH(known_types) || (type_ptr->flags & INTRO_EXPLICITLY_GENERATED)) {
            add_to_gen_info(ctx, o_info, type_ptr);
        }
    }

    uint32_t count_all_functions = shlen(ctx->function_map);
    uint32_t count_gen_functions = 0;
    IntroFunction ** functions = arena_alloc(ctx->arena, count_all_functions * sizeof(void *));
    for (int i=0; i < count_all_functions; i++) {
        IntroFunction * func = ctx->function_map[i].value;
        if ((func->flags & INTRO_EXPLICITLY_GENERATED)) {
            functions[count_gen_functions++] = func;
            add_to_gen_info(ctx, o_info, func->type);
        }
    }

    ctx->p_info = o_info;

    reset_location_context(&ctx->loc);
    handle_attributes(ctx, o_info);

    o_info->count_types = arrlen(o_info->types);
    o_info->value_buffer = ctx->value_buffer;
    o_info->count_arg_lists = arrlen(o_info->arg_lists);
    o_info->count_functions = count_gen_functions;
    o_info->functions = functions;
    o_info->string_set = ctx->string_set;
    return 0;
}
