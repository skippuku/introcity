#include "lib/intro.h"
#include "lexer.c"
#include "global.h"

static const IntroType known_types [] = {
    {"void",     NULL, INTRO_UNKNOWN},
    {"uint8_t",  NULL, INTRO_U8 },
    {"uint16_t", NULL, INTRO_U16},
    {"uint32_t", NULL, INTRO_U32},
    {"uint64_t", NULL, INTRO_U64},
    {"int8_t",   NULL, INTRO_S8 },
    {"int16_t",  NULL, INTRO_S16},
    {"int32_t",  NULL, INTRO_S32},
    {"int64_t",  NULL, INTRO_S64},
    {"float",    NULL, INTRO_F32},
    {"double",   NULL, INTRO_F64},
    {"_Bool",    NULL, INTRO_U8 },
    {"va_list",  NULL, INTRO_VA_LIST},
};

static void
parse_error(ParseContext * ctx, Token * tk, char * message) {
    parse_msg_internal(&ctx->loc, ctx->buffer, tk, message, 0);
}

static void UNUSED
parse_warning(ParseContext * ctx, Token * tk, char * message) {
    parse_msg_internal(&ctx->loc, ctx->buffer, tk, message, 1);
}

static intmax_t parse_constant_expression(ParseContext * ctx, char ** o_s);

#include "attribute.c"

static const char *
cache_name(ParseContext * ctx, char * name) {
    ptrdiff_t index = shgeti(ctx->name_set, name);
    if (index < 0) {
        shputs(ctx->name_set, (NameSet){name});
        index = shtemp(ctx->name_set);
    }
    return ctx->name_set[index].key;
}

// NOTE: technically possible for a collision to happen, this should be changed
static IntroTypePtrList *
store_arg_type_list(ParseContext * ctx, IntroType ** list) {
    static const size_t hash_seed = 0xF58D6349C6431963; // this is just some random number

    size_t count_list_bytes = arrlen(list) * sizeof(list[0]);
    size_t hash = (list)? stbds_hash_bytes(list, count_list_bytes, hash_seed) : 0;
    IntroTypePtrList * stored = hmget(ctx->arg_list_by_hash, hash);
    if (!stored) {
        stored = malloc(sizeof(*stored) + count_list_bytes);
        stored->count = arrlen(list);
        memcpy(stored->types, list, count_list_bytes);
        stored = hmput(ctx->arg_list_by_hash, hash, stored);
    }

    return stored;
}

static IntroType *
store_type(ParseContext * ctx, const IntroType * type) {
    IntroType * stored = malloc(sizeof(*stored));
    memcpy(stored, type, sizeof(*stored));
    IntroType * original = NULL;
    if (stored->name) {
        ptrdiff_t index = shgeti(ctx->type_map, stored->name);
        if (index >= 0) {
            original = ctx->type_map[index].value;
        }
        shput(ctx->type_map, stored->name, stored);
        index = shtemp(ctx->type_map);
        stored->name = ctx->type_map[index].key;
    }
    if (original) {
        (void) hmdel(ctx->type_set, *original);
        free(original);
    }
    ptrdiff_t set_index = hmgeti(ctx->type_set, *stored);
    if (set_index < 0) {
        hmput(ctx->type_set, *stored, stored);
    } else {
        free(stored);
        stored = ctx->type_set[set_index].value;
    }
    // TODO: i am not a fan of this
    if (original && intro_is_complex(type)) {
        for (int i=LENGTH(known_types); i < hmlen(ctx->type_set); i++) {
            IntroType * t = ctx->type_set[i].value;
            if (t->parent == original) {
                if (t->category == INTRO_UNKNOWN) {
                    t->i_struct = stored->i_struct; // covers i_enum
                    t->category = stored->category;
                }
                t->parent = stored;
            }
        }
    }
    return stored;
}

int
maybe_expect_attribute(ParseContext * ctx, char ** o_s, int32_t i, Token * o_tk, AttributeSpecifier ** p_attribute_specifiers) {
    if (o_tk->type == TK_IDENTIFIER && tk_equal(o_tk, "I")) {
        Token paren = next_token(o_s);
        if (paren.type != TK_L_PARENTHESIS) {
            parse_error(ctx, &paren, "Expected '('.");
            return -1;
        }
        AttributeSpecifier spec;
        spec.location = paren.start;
        spec.i = i;
        arrput(*p_attribute_specifiers, spec);
        char * closing = find_closing(paren.start);
        if (!closing) {
            parse_error(ctx, &paren, "Missing closing ')'.");
            return -1;
        }
        *o_s = closing + 1;
        *o_tk = next_token(o_s);
    }
    return 0;
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
parse_constant_expression(ParseContext * ctx, char ** o_s) {
    Token * tks = NULL;
    Token tk;
    int depth = 0;
    while (1) {
        tk = next_token(o_s);
        if (tk.type == TK_END) {
            parse_error(ctx, &tk, "End reached unexpectedly.");
            exit(1);
        }
        if (tk.type == TK_L_PARENTHESIS) {
            depth += 1;
        } else if (tk.type == TK_R_PARENTHESIS) {
            depth -= 1;
            if (depth < 0) {
                *o_s = tk.start;
                break;
            }
        } else if (tk.type == TK_COMMA || tk.type == TK_R_BRACE || tk.type == TK_SEMICOLON || tk.type == TK_R_BRACKET) {
            *o_s = tk.start;
            break;
        }
        arrput(tks, tk);
    }
    ExprNode * tree = build_expression_tree(ctx->expr_ctx, tks, arrlen(tks), &tk);
    if (!tree) {
        parse_error(ctx, &tk, "Unknown value in expression.");
        exit(1);
    }
    ExprProcedure * expr = build_expression_procedure(tree);
    intmax_t result = run_expression(expr);

    free(expr);
    reset_arena(ctx->expr_ctx->arena);

    return result;
}

static int
parse_struct(ParseContext * ctx, char ** o_s) {
    Token tk = next_token(o_s), name_tk = {0};

    bool is_union;
    if (tk_equal(&tk, "struct")) {
        is_union = false;
    } else if (tk_equal(&tk, "union")) {
        is_union = true;
    } else {
        return -1;
    }

    char * complex_type_name = NULL;
    tk = next_token(o_s);
    if (tk.type == TK_IDENTIFIER) {
        name_tk = tk;
        tk = next_token(o_s);

        strputf(&complex_type_name, "%s %.*s",
                (is_union)? "union" : "struct", name_tk.length, name_tk.start);
        strputnull(complex_type_name);

        if (shgeti(ctx->type_map, complex_type_name) < 0) {
            IntroType temp_type = {0};
            temp_type.name = complex_type_name;
            store_type(ctx, &temp_type);
        }
    }

    if (tk.type != TK_L_BRACE) {
        if (tk.type == TK_IDENTIFIER || tk.type == TK_STAR || tk.type == TK_SEMICOLON) {
            return RET_NOT_DEFINITION;
        }
        parse_error(ctx, &tk, "Expected '{'.");
        return -1;
    }

    IntroMember * members = NULL;
    NestInfo * nests = NULL;
    DeclState decl = {.state = DECL_MEMBERS};
    while (1) {
        decl.member_index = arrlen(members);
        int ret = parse_declaration(ctx, o_s, &decl);
        if (ret != RET_DECL_CONTINUE) {
            if (ret == RET_DECL_FINISHED) {
                break;
            }
            if (ret >= 0 && decl.base_tk.start) {
                parse_error(ctx, &decl.base_tk, "Invalid.");
            }
            return -1;
        }
        if (decl.type->category == INTRO_UNKNOWN) {
            parse_error(ctx, &decl.base_tk, "Unknown type.");
            return -1;
        }

        IntroMember member = {0};
        if (!decl.name_tk.start) {
            if (decl.type->category == INTRO_STRUCT || decl.type->category == INTRO_UNION) {
                IntroStruct * s = decl.base->i_struct;
                for (int i=0; i < s->count_members; i++) {
                    arrput(members, s->members[i]);
                }
                continue;
            } else {
                parse_error(ctx, &tk, "Struct member has no name, or type is unknown.");
                return -1;
            }
        }

        member.name = copy_and_terminate(decl.name_tk.start, decl.name_tk.length);
        member.type = decl.type;
        member.bitfield = decl.bitfield;

        if (decl.base->name == NULL) {
            NestInfo info = {0};
            info.key = decl.base;
            info.member_index = arrlen(members);
            IntroType * tt = decl.type;
            while ((tt->category == INTRO_POINTER || tt->category == INTRO_ARRAY)) {
                tt = tt->parent;
                info.indirection_level++;
            }
            arrput(nests, info);
        }

        arrput(members, member);
    }

    IntroStruct * result = calloc(1, sizeof(IntroStruct) + sizeof(IntroMember) * arrlen(members));
    result->count_members = arrlen(members);
    result->is_union = is_union;
    memcpy(result->members, members, sizeof(IntroMember) * arrlen(members));
    arrfree(members);

    {
        IntroType type = {0};
        type.name = complex_type_name;
        type.category = (is_union)? INTRO_UNION : INTRO_STRUCT;
        type.i_struct = result;
        
        IntroType * stored = store_type(ctx, &type);
        arrfree(complex_type_name);

        for (int i=0; i < arrlen(nests); i++) {
            NestInfo info = nests[i];
            info.parent = stored;
            hmputs(ctx->nest_map, info);
        }
        arrfree(nests);
    }

    if (decl.attribute_specifiers) {
        IntroAttributeData * d;
        uint32_t count;
        for (int i=0; i < arrlen(decl.attribute_specifiers); i++) {
            int member_index = decl.attribute_specifiers[i].i;
            char * location = decl.attribute_specifiers[i].location;
            int error = parse_attributes(ctx, location, result, member_index, &d, &count);
            if (error) return error;
            result->members[member_index].attributes = d;
            result->members[member_index].count_attributes = count;
        }
        arrfree(decl.attribute_specifiers);

        handle_differed_defaults(ctx, result);
    }

    return 0;
}

static int
parse_enum(ParseContext * ctx, char ** o_s) {
    IntroEnum enum_ = {0};

    char * complex_type_name = NULL;
    Token tk = next_token(o_s), name_tk = {0};
    if (tk.type == TK_IDENTIFIER) {
        Token next = next_token(o_s);
        if (next.type != TK_L_PARENTHESIS) {
            name_tk = tk;
            strputf(&complex_type_name, "enum %.*s", name_tk.length, name_tk.start);
            strputnull(complex_type_name);

            if (shgeti(ctx->type_map, complex_type_name) < 0) {
                IntroType temp_type = {0};
                temp_type.name = complex_type_name;
                store_type(ctx, &temp_type);
            }
            tk = next;
        } else {
            *o_s = next.start;
        }
    }

    bool is_attribute = false;
    AttributeSpecifier * attribute_specifiers = NULL;
    if (tk.type == TK_IDENTIFIER && tk_equal(&tk, "I")) {
        tk = next_token(o_s);
        if (tk.type != TK_L_PARENTHESIS) {
            parse_error(ctx, &tk, "Expected '('.");
            return -1;
        }
        tk = next_token(o_s);
        if (tk.type == TK_IDENTIFIER && tk_equal(&tk, "attribute")) {
            is_attribute = true;
        } else {
            parse_error(ctx, &tk, "Invalid.");
            return -1;
        }
        tk = next_token(o_s);
        if (tk.type != TK_R_PARENTHESIS) {
            parse_error(ctx, &tk, "Expected ')'.");
            return -1;
        }
        tk = next_token(o_s);
    }
    if (tk.type != TK_L_BRACE) {
        if (tk.type == TK_IDENTIFIER || tk.type == TK_STAR || tk.type == TK_SEMICOLON) return RET_NOT_DEFINITION;
        parse_error(ctx, &tk, "Expected '{'.");
        return -1;
    }

    enum_.is_flags = true;
    enum_.is_sequential = true;
    IntroEnumValue * members = NULL;
    int next_int = 0;
    int mask = 0;
    while (1) {
        IntroEnumValue v = {0};
        Token name = next_token(o_s);
        if (name.type == TK_R_BRACE) {
            break;
        }
        if (name.type != TK_IDENTIFIER) {
            parse_error(ctx, &name, "Expected identifier.");
            return -1;
        }

        STACK_TERMINATE(new_name, name.start, name.length);
        if (shgeti(ctx->name_set, new_name) >= 0) {
            parse_error(ctx, &name, "Cannot define enumeration with reserved name.");
            return -1;
        }
        v.name = cache_name(ctx, new_name);

        tk = next_token(o_s);
        if (is_attribute) {
            int index = arrlen(attribute_specifiers);
            maybe_expect_attribute(ctx, o_s, arrlen(members), &tk, &attribute_specifiers);
            if (arrlen(attribute_specifiers) != index) {
                attribute_specifiers[index].tk = name;
            }
        }
        bool set = false;
        bool is_last = false;
        if (tk.type == TK_COMMA) {
            v.value = next_int++;
        } else if (tk.type == TK_EQUAL) {
            v.value = (int)parse_constant_expression(ctx, o_s);
            if (v.value != next_int) {
                enum_.is_sequential = false;
            }
            next_int = v.value + 1;
            set = true;
        } else if (tk.type == TK_R_BRACE) {
            v.value = next_int;
            is_last = true;
        } else {
            parse_error(ctx, &tk, "Unexpected symbol.");
            return -1;
        }

        if (mask & v.value) enum_.is_flags = false;
        mask |= v.value;

        arrput(members, v);
        shput(ctx->expr_ctx->constant_map, v.name, (intmax_t)v.value);

        if (is_last) break;

        if (set) {
            tk = next_token(o_s);
            if (tk.type == TK_COMMA) {
            } else if (tk.type == TK_R_BRACE) {
                break;
            } else {
                parse_error(ctx, &tk, "Unexpected symbol.");
                return -1;
            }
        }
    }
    enum_.count_members = arrlen(members);

    IntroEnum * result = malloc(sizeof(IntroEnum) + sizeof(*members) * arrlen(members));
    memcpy(result, &enum_, sizeof(IntroEnum));
    memcpy(result->members, members, sizeof(*members) * arrlen(members));
    arrfree(members);

    {
        IntroType type = {0};
        type.name = complex_type_name;
        type.category = INTRO_ENUM;
        type.i_enum = result;

        store_type(ctx, &type);
        arrfree(complex_type_name);
    }

    if (attribute_specifiers != NULL) {
        for (int i=0; i < arrlen(attribute_specifiers); i++) {
            AttributeSpecifier spec = attribute_specifiers[i];
            int error = parse_attribute_register(ctx, spec.location, spec.i, &spec.tk);
            if (error) return error;
        }
    }

    return 0;
}

int
parse_type_base(ParseContext * ctx, char ** o_s, DeclState * decl) {
    IntroType type = {0};
    char * type_name = NULL;
    bool is_typedef = false;
    int first_keyword = -1;

    Token first;
    while (1) {
        first = next_token(o_s);
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
    if (first.type != TK_IDENTIFIER) {
        if (decl->state == DECL_ARGS && first.type == TK_PERIOD) {
            for (int i=0; i < 2; i++) {
                Token next = next_token(o_s);
                if (next.type != TK_PERIOD) {
                    parse_error(ctx, &first, "Invalid symbol in parameter list.");
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
        *o_s = first.start;
        return RET_NOT_TYPE;
    }
    decl->base_tk = first;
    strputf(&type_name, "%.*s", first.length, first.start);

    if (first_keyword == KEYW_STRUCT
      ||first_keyword == KEYW_UNION
      ||first_keyword == KEYW_ENUM) {
        char * after_keyword = *o_s;
        int error;
        if (first_keyword == KEYW_STRUCT || first_keyword == KEYW_UNION) {
            *o_s = first.start;
            error = parse_struct(ctx, o_s);
        } else {
            error = parse_enum(ctx, o_s);
        }
        if (error == RET_NOT_DEFINITION) {
            *o_s = after_keyword;
            Token tk = next_token(o_s);
            strputf(&type_name, " %.*s", tk.length, tk.start);

            decl->base_tk.length = tk.start - first.start + tk.length;
        } else if (error != 0) {
            return -1;
        } else {
            ptrdiff_t last_index = hmtemp(ctx->type_set);
            decl->base = ctx->type_set[last_index].value;
            if (is_typedef) decl->state = DECL_TYPEDEF;
            return 0;
        }
    } else {
#define CHECK_INT(x) \
    if (x) { \
        parse_error(ctx, &tk, "Invalid."); \
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
                Token tk2 = next_token(o_s);
                if (tk_equal(&tk2, "long")) {
                    strputf(&type_name, " long");
                    tk = tk2;
                } else if (tk_equal(&tk2, "double")) {
                    type.category = INTRO_F128; // TODO
                    tk = tk2;
                    strputf(&type_name, " %.*s", tk.length, tk.start);
                    break;
                } else {
                    *o_s = tk2.start;
                }
                type.category |= 0x08;
            }break;

            case KEYW_MS_INT64: {
                CHECK_INT((type.category & 0x0f));
                type.category |= 0x08;
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

            case KEYW_MS_INT32:
            case KEYW_INT: {
                CHECK_INT((type.category & 0x0f) == 0x01);
                if ((type.category & 0x0f) == 0) {
                    type.category |= 0x04;
                }
                break_loop = true;
            }break;

            default: {
                if (add_to_name) *o_s = tk.start;
                tk = ltk;
                break_loop = true;
                add_to_name = false;
            }break;
            }
            if (add_to_name) strputf(&type_name, " %.*s", tk.length, tk.start);
            if (break_loop) break;
            ltk = tk;
            tk = next_token(o_s);
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
            type.parent = ctx->type_set[parent_index].value;
        }

        decl->base_tk.length = tk.start - first.start + tk.length;
    }

    if (is_typedef) decl->state = DECL_TYPEDEF;

    strputnull(type_name);
    IntroType * t = shget(ctx->type_map, type_name);
    if (t) {
        arrfree(type_name);
        decl->base = t;
    } else {
        if (type.category || is_typedef) {
            type.name = type_name;
            IntroType * stored = store_type(ctx, &type);
            arrfree(type_name);
            decl->base = stored;
        } else {
            parse_error(ctx, &decl->base_tk, "Undeclared type.");
            return -1;
        }
    }
    return 0;
}

static IntroType ** parse_function_arguments(ParseContext * ctx, char ** o_s, DeclState * parent_decl);

static int
parse_type_annex(ParseContext * ctx, char ** o_s, DeclState * decl) {
    int32_t * indirection = NULL;
    int32_t * temp = NULL;
    IntroType *** func_args_stack = NULL;
    char * paren;

    const int32_t POINTER = -1;
    const int32_t FUNCTION = -2;
    Token tk;
    char * end = *o_s;
    memset(&decl->name_tk, 0, sizeof(decl->name_tk));
    do {
        paren = NULL;

        int pointer_level = 0;
        while (1) {
            tk = next_token(o_s);
            if (tk.type == TK_STAR) {
                pointer_level += 1;
            } else if (is_ignored(get_keyword(ctx, tk))) {
                continue;
            } else {
                break;
            }
        }

        if (tk.type == TK_L_PARENTHESIS) {
            paren = tk.start + 1;
            *o_s = find_closing(tk.start) + 1;
            tk = next_token(o_s);
        }

        if (tk.type == TK_IDENTIFIER) {
            decl->name_tk = tk;
            tk = next_token(o_s);
        }

        arrsetlen(temp, 0);
        if (tk.type == TK_L_PARENTHESIS) {
            *o_s = tk.start;
            IntroType ** arg_types = parse_function_arguments(ctx, o_s, decl);
            arrpush(func_args_stack, arg_types);
            arrput(temp, FUNCTION);
            tk = next_token(o_s);
        }

        while (tk.type == TK_L_BRACKET) {
            char * closing_bracket = find_closing(tk.start);
            int32_t num;
            if (closing_bracket == tk.start + 1) {
                num = 0;
            } else {
                num = (int32_t)parse_constant_expression(ctx, o_s);
                if (num < 0) {
                    parse_error(ctx, &tk, "Invalid array size.");
                    return -1;
                }
            }
            arrput(temp, num);
            *o_s = closing_bracket + 1;
            tk = next_token(o_s);
        }

        if (tk.start > end) end = tk.start;

        for (int i=0; i < pointer_level; i++) {
            arrput(indirection, POINTER);
        }
        for (int i = arrlen(temp) - 1; i >= 0; i--) {
            arrput(indirection, temp[i]);
        }
    } while ((*o_s = paren) != NULL);
    arrfree(temp);

    IntroType * last_type = decl->base;
    for (int i=0; i < arrlen(indirection); i++) {
        int32_t it = indirection[i];
        IntroType new_type = {0};
        if (it == POINTER) {
            new_type.category = INTRO_POINTER;
            new_type.parent = last_type;
        } else if (it == FUNCTION) {
            IntroType ** arg_types = arrpop(func_args_stack);
            IntroTypePtrList * stored_args = store_arg_type_list(ctx, arg_types);
            arrfree(arg_types);

            new_type.parent = last_type; // return type
            new_type.category = INTRO_FUNCTION;
            new_type.args = stored_args;
        } else {
            new_type.category = INTRO_ARRAY;
            new_type.parent = last_type;
            new_type.array_size = it;
        }
        last_type = store_type(ctx, &new_type);
    }
    arrfree(indirection);
    arrfree(func_args_stack);

    *o_s = end;
    decl->type = last_type;
    return 0;
}

static int
parse_declaration(ParseContext * ctx, char ** o_s, DeclState * decl) {
    int ret = 0;
    if (!decl->reuse_base) ret = parse_type_base(ctx, o_s, decl);
    if (ret < 0 || ret == RET_FOUND_END) return ret;
    if (ret == RET_NOT_TYPE) {
        Token tk = next_token(o_s);
        if ((tk.type == TK_R_PARENTHESIS)
         || (tk.type == TK_SEMICOLON)
         || (tk.type == TK_R_BRACE)) {
            return RET_DECL_FINISHED;
        } else {
            parse_error(ctx, &tk, "Invalid type.");
            return -1;
        }
    } else if (ret == RET_DECL_VA_LIST) {
        Token tk = next_token(o_s);
        if (tk.type != TK_R_PARENTHESIS) {
            parse_error(ctx, &tk, "Expected ')' after va_list.");
            return -1;
        }
        return RET_DECL_FINISHED;
    }

    ret = parse_type_annex(ctx, o_s, decl);
    if (ret < 0) return -1;

    if (decl->state == DECL_TYPEDEF) {
        if (decl->name_tk.start == NULL) {
            parse_error(ctx, &decl->base_tk, "typedef has no name.");
            return -1;
        }
        char * name = copy_and_terminate(decl->name_tk.start, decl->name_tk.length);
        if (shgeti(ctx->ignore_typedefs, name) >= 0) {
            goto find_end;
        }
        IntroType * prev = shget(ctx->type_map, name);
        if (prev) {
            if (prev->parent != decl->type) {
                parse_error(ctx, &decl->name_tk, "Redefinition does not match previous definition.");
                return -1;
            }
        } else {
            IntroType new_type = *decl->type;
            new_type.name = name;
            bool new_type_is_indirect = new_type.category == INTRO_POINTER || new_type.category == INTRO_ARRAY;
            if (!new_type_is_indirect) {
                new_type.parent = decl->type;
            }
            store_type(ctx, &new_type);
        }
    }

    IntroFunction * func = NULL;
    if (decl->type->category == INTRO_FUNCTION) {
        STACK_TERMINATE(terminated_name, decl->name_tk.start, decl->name_tk.length);
        IntroFunction * prev = shget(ctx->function_map, terminated_name);
        int32_t count_args = decl->type->args->count;
        if (prev) {
            if (intro_origin(prev->type, 0) != intro_origin(decl->type, 0)) {
                parse_error(ctx, &decl->name_tk, "Function declaration does not match previous.");
                return -1;
            } else {
                //if (prev->has_body) {
                //    parse_warning(ctx, &decl->name_tk, "Function is redeclared after definition.");
                //}
                func = prev;
                for (int i=0; i < prev->type->args->count; i++) {
                    if (prev->arg_names[i]) free((void *)prev->arg_names[i]);
                }
            }
        } else {
            func = calloc(1, sizeof(*func) + count_args * sizeof(func->arg_names[0]));
            func->name = copy_and_terminate(decl->name_tk.start, decl->name_tk.length);
            func->type = decl->type;
            shput(ctx->function_map, func->name, func);
        }
        if (count_args > 0) {
            memcpy(func->arg_names, decl->arg_names, count_args * sizeof(func->arg_names[0]));
            arrfree(decl->arg_names);
        }
    }

find_end: ;
    decl->bitfield = 0;
    bool in_expr = false;
    while (1) {
        Token tk = next_token(o_s);
        if (decl->state == DECL_MEMBERS) {
            maybe_expect_attribute(ctx, o_s, decl->member_index, &tk, &decl->attribute_specifiers);
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
            intmax_t bitfield = parse_constant_expression(ctx, o_s);
            decl->bitfield = (uint8_t)bitfield;
            continue;
        } else {
            bool do_find_closing = false;
            bool func_body = false;
            if (in_expr) {
                if (tk.type == TK_L_BRACE || tk.type == TK_L_BRACKET || tk.type == TK_L_PARENTHESIS) {
                    do_find_closing = true;
                } else {
                    continue;
                }
            }
            if (tk.type == TK_L_BRACE && decl->type->category == INTRO_FUNCTION) {
                do_find_closing = true;
                func_body = true;
                assert(func != NULL);
                func->has_body = true;
            }
            if ((decl->state == DECL_CAST || decl->state == DECL_ARGS) && tk.type == TK_R_PARENTHESIS) {
                *o_s = tk.start;
                return RET_DECL_CONTINUE;
            }
            if (do_find_closing) {
                *o_s = find_closing(tk.start);
                if (!*o_s) {
                    if (func_body) {
                        parse_error(ctx, &tk, "No closing '}' for function body.");
                        if (decl->name_tk.start) {
                            parse_warning(ctx, &decl->name_tk, "Function name here.");
                        }
                    } else {
                        parse_error(ctx, &tk, "Parenthesis, bracket, or brace is not closed.");
                    }
                    return -1;
                }
                *o_s += 1;
                tk = next_token(o_s);
                if (tk.type == TK_END) {
                    return RET_FOUND_END;
                } else if (tk.type == TK_SEMICOLON || tk.type == TK_COMMA) {
                } else {
                    *o_s = tk.start;
                }
                memset(decl, 0, sizeof(*decl));
                *o_s = tk.start;
                return RET_DECL_CONTINUE;
            }
            parse_error(ctx, &tk, "Invalid symbol in declaration. Expected ',' or ';'.");
            return -1;
        }
    }
}

static IntroType **
parse_function_arguments(ParseContext * ctx, char ** o_s, DeclState * parent_decl) {
    Token open = next_token(o_s);
    assert(open.type == TK_L_PARENTHESIS);

    // TODO: get names
    IntroType ** arg_types = NULL;

    DeclState decl = {.state = DECL_ARGS};
    while (1) {
        int ret = parse_declaration(ctx, o_s, &decl);
        if (ret == RET_DECL_FINISHED) {
            break;
        } else if (ret < 0) {
            exit(1);
        }

        if (decl.type->category == INTRO_UNKNOWN && 0==strcmp(decl.type->name, "void")) {
            continue;
        }

        char * name = (decl.name_tk.start)
                       ? copy_and_terminate(decl.name_tk.start, decl.name_tk.length)
                       : NULL;

        arrput(arg_types, decl.type);
        arrput(parent_decl->arg_names, name);
    }

    return arg_types;
}

int
parse_preprocessed_text(PreInfo * pre_info, IntroInfo * o_info) {
    ParseContext * ctx = calloc(1, sizeof(ParseContext));
    ctx->buffer = pre_info->result_buffer;
    ctx->expr_ctx = calloc(1, sizeof(ExprContext));
    ctx->expr_ctx->arena = new_arena();
    ctx->expr_ctx->mode = MODE_PARSE;
    ctx->expr_ctx->ctx = ctx;
    ctx->loc = pre_info->loc;

    sh_new_arena(ctx->type_map);
    sh_new_arena(ctx->name_set);

    for (int i=0; i < LENGTH(known_types); i++) {
        store_type(ctx, &known_types[i]);
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
#if DEBUG
        assert(shgeti(ctx->keyword_set, keywords[i].key) == keywords[i].value);
#endif
    }

    create_initial_attributes();

    DeclState decl = {.state = DECL_GLOBAL};

    char * s = pre_info->result_buffer;
    while (1) {
        int ret = parse_declaration(ctx, &s, &decl);

        if (ret < 0) {
            return -1;
        } else if (ret == RET_FOUND_END) {
            break;
        }
    }

    IntroType ** type_list = NULL;
    arrsetcap(type_list, hmlen(ctx->type_set));
    IndexByPtrMap * index_by_ptr = NULL;
    for (int i=0; i < hmlen(ctx->type_set); i++) {
        IntroType * type_ptr = ctx->type_set[i].value;
        arrput(type_list, type_ptr);
        hmput(index_by_ptr, (void *)type_ptr, i);
    }

    uint32_t count_arg_lists = hmlen(ctx->arg_list_by_hash);
    IntroTypePtrList ** arg_lists = malloc(count_arg_lists * sizeof(void *));
    for (int i=0; i < count_arg_lists; i++) {
        arg_lists[i] = ctx->arg_list_by_hash[i].value;
    }

    uint32_t count_functions = shlen(ctx->function_map);
    IntroFunction ** functions = malloc(count_functions * sizeof(void *));
    for (int i=0; i < count_functions; i++) {
        functions[i] = ctx->function_map[i].value;
    }

    o_info->count_types = arrlen(type_list);
    o_info->types = type_list;
    o_info->index_by_ptr_map = index_by_ptr;
    o_info->nest_map = ctx->nest_map;
    o_info->value_buffer = ctx->value_buffer;
    o_info->count_arg_lists = count_arg_lists;
    o_info->arg_lists = arg_lists;
    o_info->count_functions = count_functions;
    o_info->functions = functions;
    return 0;
}
