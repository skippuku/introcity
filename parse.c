#include "lib/intro.h"
#include "lexer.c"
#include "util.h"

static void
parse_error(ParseContext * ctx, Token * tk, char * message) {
    parse_msg_internal(ctx->buffer, tk, message, 0);
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
        for (int i=15; i < hmlen(ctx->type_set); i++) {
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

typedef struct {
    char * location;
    int32_t i;
    Token tk;
} AttributeSpecifier;

int
maybe_expect_attribute(ParseContext * ctx, char ** o_s, int32_t i, Token * o_tk, AttributeSpecifier ** p_attribute_specifiers) {
    if (o_tk->type == TK_IDENTIFIER && tk_equal(o_tk, "I")) {
        Token paren = next_token(o_s);
        if (paren.type != TK_L_PARENTHESIS) {
            parse_error(ctx, &paren, "Expected '('.");
            return 1;
        }
        AttributeSpecifier spec;
        spec.location = paren.start;
        spec.i = i;
        arrput(*p_attribute_specifiers, spec);
        char * closing = find_closing(paren.start);
        if (!closing) {
            parse_error(ctx, &paren, "Missing closing ')'.");
            return 1;
        }
        *o_s = closing + 1;
        *o_tk = next_token(o_s);
    }
    return 0;
}

static bool
is_ignored(Token * tk) {
    return tk_equal(tk, "const")
        || tk_equal(tk, "static")
        || tk_equal(tk, "volatile")
        || tk_equal(tk, "inline");
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
        if (tk.type == TK_IDENTIFIER || tk.type == TK_STAR || tk.type == TK_SEMICOLON) return 2;
        parse_error(ctx, &tk, "Expected '{'.");
        return -1;
    }

    IntroMember * members = NULL;
    AttributeSpecifier * attribute_specifiers = NULL;
    NestInfo * nests = NULL;
    while (1) {
        Token type_tk = {0};
        IntroType * base_type = parse_base_type(ctx, o_s, &type_tk, false);
        if (!base_type) {
            Token tk = next_token(o_s);
            if (tk.type == TK_R_BRACE) {
                break;
            } else {
                return 1;
            }
        }

        Token tk = next_token(o_s);
        if (tk.type == TK_SEMICOLON) {
            if (base_type->category == INTRO_STRUCT || base_type->category == INTRO_UNION) {
                IntroStruct * s = base_type->i_struct;
                for (int i=0; i < s->count_members; i++) {
                    arrput(members, s->members[i]);
                }
                continue;
            } else {
                parse_error(ctx, &tk, "Struct member has no name or type is unknown.");
                return 1;
            }
        } else {
            *o_s = tk.start;
            while (1) {
                IntroMember member = {0};

                Token name_tk;
                IntroType * type = parse_declaration(ctx, base_type, o_s, &name_tk);
                member.name = copy_and_terminate(name_tk.start, name_tk.length);
                if (!type) return 1;

                if (type->category == INTRO_UNKNOWN) {
                    parse_error(ctx, &type_tk, "Unknown type.");
                    return 1;
                }
                member.type = type;

                if (base_type->name == NULL) {
                    NestInfo info = {0};
                    info.key = base_type;
                    info.member_index = arrlen(members);
                    IntroType * tt = type;
                    while ((tt->category == INTRO_POINTER || tt->category == INTRO_ARRAY)) {
                        tt = tt->parent;
                        info.indirection_level++;
                    }
                    arrput(nests, info);
                }

                tk = next_token(o_s);
                int error = maybe_expect_attribute(ctx, o_s, arrlen(members), &tk, &attribute_specifiers);
                if (error) return error;

                if (tk.type == TK_COLON) {
                    intmax_t bitfield = parse_constant_expression(ctx, o_s);
                    member.bitfield = (uint8_t)bitfield;
                    tk = next_token(o_s);
                }

                arrput(members, member);

                if (tk.type == TK_SEMICOLON) {
                    break;
                } else if (tk.type == TK_COMMA) {
                } else {
                    parse_error(ctx, &tk, "Expected ';' or ','.");
                    return 1;
                }
            }
        }
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

    if (attribute_specifiers) {
        IntroAttributeData * d;
        uint32_t count;
        for (int i=0; i < arrlen(attribute_specifiers); i++) {
            int member_index = attribute_specifiers[i].i;
            char * location = attribute_specifiers[i].location;
            int error = parse_attributes(ctx, location, result, member_index, &d, &count);
            if (error) return error;
            result->members[member_index].attributes = d;
            result->members[member_index].count_attributes = count;
        }
        arrfree(attribute_specifiers);

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
        name_tk = tk;
        tk = next_token(o_s);

        strputf(&complex_type_name, "enum %.*s", name_tk.length, name_tk.start);
        strputnull(complex_type_name);

        if (shgeti(ctx->type_map, complex_type_name) < 0) {
            IntroType temp_type = {0};
            temp_type.name = complex_type_name;
            store_type(ctx, &temp_type);
        }
    }

    bool is_attribute = false;
    AttributeSpecifier * attribute_specifiers = NULL;
    if (tk.type == TK_IDENTIFIER && tk_equal(&tk, "I")) {
        tk = next_token(o_s);
        if (tk.type != TK_L_PARENTHESIS) {
            parse_error(ctx, &tk, "Expected '('.");
            return 1;
        }
        tk = next_token(o_s);
        if (tk.type == TK_IDENTIFIER && tk_equal(&tk, "attribute")) {
            is_attribute = true;
        } else {
            parse_error(ctx, &tk, "Invalid.");
            return 1;
        }
        tk = next_token(o_s);
        if (tk.type != TK_R_PARENTHESIS) {
            parse_error(ctx, &tk, "Expected ')'.");
            return 1;
        }
        tk = next_token(o_s);
    }
    if (tk.type != TK_L_BRACE) {
        if (tk.type == TK_IDENTIFIER || tk.type == TK_STAR || tk.type == TK_SEMICOLON) return 2;
        parse_error(ctx, &tk, "Expected '{'.");
        return 1;
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
            return 1;
        }

        STACK_TERMINATE(new_name, name.start, name.length);
        if (shgeti(ctx->name_set, new_name) >= 0) {
            parse_error(ctx, &name, "Cannot define enumeration with reserved name.");
            return 1;
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
            return 1;
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
                return 1;
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

static int
parse_typedef(ParseContext * ctx, char ** o_s) {
    Token type_tk = {0};
    (void) type_tk;
    IntroType * base = parse_base_type(ctx, o_s, &type_tk, true);
    if (!base) return 1;

    while (1) {
        Token name_tk = {0};
        IntroType * type = parse_declaration(ctx, base, o_s, &name_tk);
        if (!type) return 1;
        if (name_tk.start == NULL) {
            parse_error(ctx, &type_tk, "typedef has no name.");
            return 1;
        }
        char * name = copy_and_terminate(name_tk.start, name_tk.length);
        if (shgeti(ctx->ignore_typedefs, name) >= 0) {
            return 0;
        }
        IntroType * prev = shget(ctx->type_map, name);
        if (prev) {
            if (prev->parent != type) {
                parse_error(ctx, &name_tk, "Redefinition does not match previous definition.");
                return 1;
            }
        } else {
            IntroType new_type = *type;
            new_type.name = name;
            bool new_type_is_indirect = new_type.category == INTRO_POINTER || new_type.category == INTRO_ARRAY;
            if (!new_type_is_indirect) {
                new_type.parent = type;
            }
            store_type(ctx, &new_type);
        }
        Token tk = next_token(o_s);
        if (tk.type == TK_SEMICOLON) {
            break;
        } else if (tk.type == TK_COMMA) {
        } else {
            parse_error(ctx, &tk, "Expected ',' or ';'.");
            return 1;
        }
    }

    return 0;
}

static IntroType *
parse_base_type(ParseContext * ctx, char ** o_s, Token * o_tk, bool is_typedef) {
    IntroType type = {0};
    char * type_name = NULL;

    Token first;
    do {
        first = next_token(o_s);
    } while (first.type == TK_IDENTIFIER && is_ignored(&first));
    if (first.type != TK_IDENTIFIER) {
        *o_s = first.start;
        return NULL;
    }
    *o_tk = first;
    strputf(&type_name, "%.*s", first.length, first.start);

    bool is_struct = tk_equal(&first, "struct");
    bool is_union  = tk_equal(&first, "union");
    bool is_enum   = tk_equal(&first, "enum");
    if (is_struct || is_union || is_enum) {
        char * after_keyword = *o_s;
        int error;
        if (is_struct || is_union) {
            *o_s = first.start;
            error = parse_struct(ctx, o_s);
        } else {
            error = parse_enum(ctx, o_s);
        }
        if (error == 2) {
            *o_s = after_keyword;
            Token tk = next_token(o_s);
            strputf(&type_name, " %.*s", tk.length, tk.start);

            o_tk->length = tk.start - first.start + tk.length;
        } else if (error != 0) {
            return NULL;
        } else {
            ptrdiff_t last_index = hmtemp(ctx->type_set);
            return ctx->type_set[last_index].value;
        }
    } else {
#define CHECK_INT(x) \
    if (x) { \
        parse_error(ctx, &tk, "Invalid."); \
        return NULL; \
    }
        Token tk = first, ltk = first;
        while (1) {
            bool is_first = (tk.start == first.start);
            if (tk_equal(&tk, "unsigned")) {
                CHECK_INT((type.category & 0xf0));
                type.category |= INTRO_UNSIGNED;
            } else if (tk_equal(&tk, "signed")) {
                CHECK_INT((type.category & 0xf0));
                type.category |= INTRO_SIGNED;
            } else if (tk_equal(&tk, "long")) {
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
            } else if (tk_equal(&tk, "short")) {
                CHECK_INT((type.category & 0x0f));
                type.category |= 0x02;
            } else if (tk_equal(&tk, "char")) {
                CHECK_INT((type.category & 0x0f));
                type.category |= 0x01;
                if (!is_first) strputf(&type_name, " %.*s", tk.length, tk.start);
                break;
            } else if (tk_equal(&tk, "int")) {
                CHECK_INT((type.category & 0x0f) == 0x01);
                if ((type.category & 0x0f) == 0) {
                    type.category |= 0x04;
                }
                if (!is_first) strputf(&type_name, " %.*s", tk.length, tk.start);
                break;
            } else {
                if (!is_first) *o_s = tk.start;
                tk = ltk;
                break;
            }
            if (!is_first) strputf(&type_name, " %.*s", tk.length, tk.start);
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
        }

        o_tk->length = tk.start - first.start + tk.length;
    }

    strputnull(type_name);
    IntroType * t = shget(ctx->type_map, type_name);
    if (t) {
        arrfree(type_name);
        return t;
    } else {
        if (type.category || is_typedef) {
            type.name = type_name;
            IntroType * stored = store_type(ctx, &type);
            arrfree(type_name);
            return stored;
        } else {
            parse_error(ctx, o_tk, "Undeclared type.");
            return NULL;
        }
    }
}

static IntroType *
parse_declaration(ParseContext * ctx, IntroType * base_type, char ** o_s, Token * o_name_tk) {
    int32_t * indirection = NULL;
    int32_t * temp = NULL;
    char * paren;

    const int32_t POINTER = -1;
    Token tk;
    char * end = *o_s;
    do {
        paren = NULL;

        int pointer_level = 0;
        while (1) {
            tk = next_token(o_s);
            if (tk.type == TK_STAR) {
                pointer_level += 1;
            } else if (tk.type == TK_IDENTIFIER && is_ignored(&tk)) {
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
            *o_name_tk = tk;
            tk = next_token(o_s);
        }

        arrsetlen(temp, 0);
        while (tk.type == TK_L_BRACKET) {
            char * closing_bracket = find_closing(tk.start);
            int32_t num;
            if (closing_bracket == tk.start + 1) {
                num = 0;
            } else {
                num = (int32_t)parse_constant_expression(ctx, o_s);
                if (num < 0) {
                    parse_error(ctx, &tk, "Invalid array size.");
                    return NULL;
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

    IntroType * last_type = base_type;
    *o_s = end;
    tk = next_token(o_s);
    if (tk.type == TK_L_PARENTHESIS) {
        end = find_closing(tk.start);
        if (!end) {
            parse_error(ctx, &tk, "Failed to find closing ')'");
            return NULL;
        }
        end += 1;

        // make function type TODO: arguments
        IntroType func = {0};
        func.parent = base_type;
        func.category = INTRO_FUNCTION;

        last_type = store_type(ctx, &func);
    }
    
    for (int i=0; i < arrlen(indirection); i++) {
        int32_t it = indirection[i];
        IntroType new_type = {0};
        new_type.parent = last_type;
        if (it == POINTER) {
            new_type.category = INTRO_POINTER;
        } else {
            new_type.category = INTRO_ARRAY;
            new_type.array_size = it;
        }
        last_type = store_type(ctx, &new_type);
    }

    *o_s = end;
    return last_type;
}

void
find_declaration_end(char ** o_s) {
    Token tk;
    while (1) {
        if (tk.type != TK_SEMICOLON) {
            break;
        }
        tk = next_token(o_s);
    }
}

int
maybe_parse_function(ParseContext * ctx, char ** o_s) {
    Token type_tk, name_tk;
    IntroType * return_base = parse_base_type(ctx, o_s, &type_tk, false);
    if (!return_base) return -1;

    IntroType * return_type = parse_declaration(ctx, return_base, o_s, &name_tk);
    if (!return_type) return -1;

    if (return_type->category == INTRO_UNKNOWN) {
        parse_error(ctx, &type_tk, "Type is unknown.");
    }

    Token tk = next_token(o_s);
    if (tk.type != TK_L_PARENTHESIS) {
        find_declaration_end(o_s);
        return 0;
    }

    IntroArgument * arguments = NULL;
    tk = next_token(o_s);
    if (tk.type == TK_R_PARENTHESIS) {
        goto after_args;
    } else {
        *o_s = tk.start;
    }
    while (1) {
        Token arg_type_tk, arg_name_tk;
        IntroType * arg_base = parse_base_type(ctx, o_s, &arg_type_tk, false);
        if (!arg_base) return -1;

        IntroType * arg_type = parse_declaration(ctx, arg_base, o_s, &arg_name_tk);
        if (!arg_type) return -1;

        IntroArgument arg = {0};
        arg.name = copy_and_terminate(name_tk.start, name_tk.length);
        arg.type = arg_type;
        arrput(arguments, arg);

        tk = next_token(o_s);
        if (tk.type == TK_R_PARENTHESIS) {
            break;
        } else  if (tk.type != TK_COMMA) {
            parse_error(ctx, &tk, "Expected ','.");
            return -1;
        }
    }
after_args: ;
    
    IntroFunction * func = malloc(sizeof(*func) + arrlen(arguments) * sizeof(*arguments));
    memset(func, 0, sizeof(*func));
    memcpy(func->arguments, arguments, arrlen(arguments) * sizeof(*arguments));

    arrfree(arguments);

    func->return_type = return_type;
    func->count_arguments = arrlen(arguments);

    IntroType type = {0};
    type.name = copy_and_terminate(name_tk.start, name_tk.length);
    type.category = INTRO_FUNCTION;
    type.function = func;

    store_type(ctx, &type);

    find_declaration_end(o_s);

    return 0;
}

int
parse_preprocessed_text(char * buffer, IntroInfo * o_info) {
    ParseContext * ctx = calloc(1, sizeof(ParseContext));
    ctx->buffer = buffer;
    ctx->expr_ctx = calloc(1, sizeof(ExprContext));
    ctx->expr_ctx->arena = new_arena();
    ctx->expr_ctx->mode = MODE_PARSE;
    ctx->expr_ctx->ctx = ctx;

    sh_new_arena(ctx->type_map);
    sh_new_arena(ctx->name_set);

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
    };
    for (int i=0; i < LENGTH(known_types); i++) {
        store_type(ctx, &known_types[i]);
        shputs(ctx->ignore_typedefs, (NameSet){known_types[i].name});
    }

    create_initial_attributes();

    char * s = buffer;
    while (1) {
        Token tk = next_token(&s);

        if (tk.type == TK_END) {
            break;
        } else if (tk.type == TK_IDENTIFIER) {
            int error = 0;
            if (tk_equal(&tk, "struct") || tk_equal(&tk, "union")) {
                s = tk.start;
                error = parse_struct(ctx, &s);
                if (error == 2) error = 0;
            } else if (tk_equal(&tk, "enum")) {
                error = parse_enum(ctx, &s);
                if (error == 2) error = 0;
            } else if (tk_equal(&tk, "typedef")) {
                error = parse_typedef(ctx, &s);
            } else {
                //s = tk.start;
                //error = maybe_parse_function(ctx, &s);
            }

            if (error) {
                return error;
            }
        } else if (tk.type == TK_L_BRACE) {
            s = find_closing(tk.start);
            if (!s) {
                parse_error(ctx, &tk, "Failed to find closing '}'.");
                return -1;
            }
            s++;
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

    o_info->count_types = arrlen(type_list);
    o_info->types = type_list;
    o_info->index_by_ptr_map = index_by_ptr;
    o_info->nest_map = ctx->nest_map;
    o_info->value_buffer = ctx->value_buffer;
    return 0;
}
