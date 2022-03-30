#include "intro.h"
#include "util.c"
#include "lexer.c"

typedef struct {
    char * key;
} NameSet;

typedef struct {
    char * buffer;
    struct{char * key; IntroType * value;} * type_map;
    struct{IntroType key; IntroType * value;} * type_set;
    NameSet * name_set;
    NestInfo * nest_map;
} ParseContext;

void
parse_error(ParseContext * ctx, Token * tk, char * message) {
    parse_error_internal(ctx->buffer, tk, message);
}

#include "attribute.c"

static char *
cache_name(ParseContext * ctx, char * name) {
    ptrdiff_t index = shgeti(ctx->name_set, name);
    if (index < 0) {
        shputs(ctx->name_set, (NameSet){name});
        index = shtemp(ctx->name_set);
    }
    return ctx->name_set[index].key;
}

static bool
is_complex(int category) {
    return (category == INTRO_STRUCT
         || category == INTRO_UNION
         || category == INTRO_ENUM);
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
    if (original && is_complex(type->category)) {
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

static IntroType * parse_base_type(ParseContext *, char **, Token *);
static IntroType * parse_declaration(ParseContext *, IntroType *, char **, char **);

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
        *o_s = find_closing(paren.start) + 1;
        *o_tk = next_token(o_s);
    }
    return 0;
}

static bool
is_ignored(Token * tk) {
    return tk_equal(tk, "const") || tk_equal(tk, "static");
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

    tk = next_token(o_s);
    if (tk.type == TK_IDENTIFIER) {
        name_tk = tk;
        tk = next_token(o_s);
    }

    if (tk.type != TK_L_BRACE) {
        if (tk.type == TK_IDENTIFIER || tk.type == TK_STAR) return 2;
        parse_error(ctx, &tk, "Expected '{'.");
        return -1;
    }

    IntroMember * members = NULL;
    AttributeSpecifier * attribute_specifiers = NULL;
    NestInfo * nests = NULL;
    while (1) {
        Token type_tk = {0};
        IntroType * base_type = parse_base_type(ctx, o_s, &type_tk);
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
            if (base_type->category == INTRO_STRUCT) {
                IntroStruct * s = base_type->i_struct;
                for (int i=0; i < s->count_members; i++) {
                    arrput(members, s->members[i]);
                }
                #if 0 // TODO: how should this be handled?
                if (decl.is_nested) {
                    (void)arrpop(structs);
                }
                #endif
                continue;
            } else {
                parse_error(ctx, &tk, "Struct member has no name or type is unknown.");
                return 1;
            }
        } else {
            *o_s = tk.start;
            while (1) {
                IntroMember member = {0};

                IntroType * type = parse_declaration(ctx, base_type, o_s, &member.name);
                if (!type) return 1;

                if (type->category == INTRO_UNKNOWN) {
                    parse_error(ctx, &type_tk, "Unknown type.");
                    return 1;
                }
                member.type = type;

                if (base_type->name == NULL && is_complex(base_type->category)) {
                    NestInfo info = {0};
                    info.key = base_type; // covers i_enum
                    info.member_index = arrlen(members);
                    arrput(nests, info);
                }

                tk = next_token(o_s);
                int error = maybe_expect_attribute(ctx, o_s, arrlen(members), &tk, &attribute_specifiers);
                if (error) return error;

                arrput(members, member);

                if (tk.type == TK_SEMICOLON) {
                    break;
                } else if (tk.type == TK_COMMA) {
                } else {
                    parse_error(ctx, &tk, "Invalid symbol in struct member declaration.");
                    return 1;
                }
            }
        }
    }

    IntroStruct * result = malloc(sizeof(IntroStruct) + sizeof(IntroMember) * arrlen(members));
    result->count_members = arrlen(members);
    result->is_union = is_union;
    memcpy(result->members, members, sizeof(IntroMember) * arrlen(members));
    arrfree(members);

    {
        char * struct_type_name = NULL;
        if (name_tk.type == TK_IDENTIFIER) {
            strputf(&struct_type_name, "%s %.*s", 
                    (is_union)? "union" : "struct", name_tk.length, name_tk.start);
            strputnull(struct_type_name);
        }

        IntroType type = {0};
        type.name = struct_type_name;
        type.category = INTRO_STRUCT;
        type.i_struct = result;
        
        IntroType * stored = store_type(ctx, &type);
        arrfree(struct_type_name);

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
    }

    return 0;
}

static int
parse_enum(ParseContext * ctx, char ** o_s) {
    IntroEnum enum_ = {0};

    Token tk = next_token(o_s), name_tk = {0};
    if (tk.type == TK_IDENTIFIER) {
        name_tk = tk;
        tk = next_token(o_s);
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
        if (tk.type == TK_IDENTIFIER || tk.type == TK_STAR) return 2;
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

        char * new_name = copy_and_terminate(name.start, name.length);
        if (shgeti(ctx->name_set, new_name) >= 0) {
            parse_error(ctx, &name, "Cannot define enumeration with reserved name.");
            return 1;
        }
        v.name = cache_name(ctx, new_name);
        free(new_name);

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
            char * prev_loc = *o_s;
            long num = strtol(*o_s, o_s, 0);
            if (*o_s == prev_loc) {
                parse_error(ctx, &tk, "Unable to parse enumeration value.");
                return 1;
            }
            v.value = (int)num;
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
        char * enum_type_name = NULL;
        if (name_tk.type == TK_IDENTIFIER) {
            strputf(&enum_type_name, "enum %.*s", name_tk.length, name_tk.start);
            strputnull(enum_type_name);
        }

        IntroType type;
        type.name = enum_type_name;
        type.category = INTRO_ENUM;
        type.i_enum = result;

        store_type(ctx, &type);
        arrfree(enum_type_name);
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
parse_typedef(ParseContext * ctx, char ** o_s) { // TODO: store base somehow
    Token type_tk = {0};
    (void) type_tk;
    IntroType * base = parse_base_type(ctx, o_s, &type_tk);
    if (!base) return 1;

    char * name = NULL;
    IntroType * type = parse_declaration(ctx, base, o_s, &name);
    if (!type) return 1;
    if (name == NULL) {
        parse_error(ctx, NULL, "typedef has no name."); // TODO: token
        return 1;
    }

    IntroType new_type = *type;
    new_type.name = name;
    bool base_is_complex = is_complex(type->category) || type->category == INTRO_UNKNOWN;
    bool new_type_is_indirect = new_type.category == INTRO_POINTER || new_type.category == INTRO_ARRAY;
    if (base_is_complex && !new_type_is_indirect) {
        new_type.parent = type;
    }
    if (shgeti(ctx->type_map, name) >= 0) {
        parse_error(ctx, NULL, "type is redefined."); // TODO: token
        return 1;
    }
    store_type(ctx, &new_type);

    return 0;
}

static IntroType *
parse_base_type(ParseContext * ctx, char ** o_s, Token * o_tk) {
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
    strputf(&type_name, "%.*s", first.length, first.start);

    bool is_nested = false;
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
        } else if (error != 0) {
            return NULL;
        } else {
            is_nested = true; // TODO: does this matter?
            (void) is_nested;
            ptrdiff_t last_index = hmtemp(ctx->type_set);
            return ctx->type_set[last_index].value;
        }
    } else {
        Token tk, ltk = first;
        if (tk_equal(&first, "unsigned")) {
            type.category |= INTRO_UNSIGNED;
        } else if (tk_equal(&first, "signed")) {
            type.category |= INTRO_SIGNED;
        }

        if (type.category) {
            tk = next_token(o_s);
        } else {
            tk = first;
        }

        bool can_be_followed_by_int = false;
        if (tk_equal(&tk, "long")) {
            Token tk2 = next_token(o_s);
            if (tk_equal(&tk2, "long")) {
                strputf(&type_name, " long");
                tk = tk2;
            } else if (tk_equal(&tk2, "double")) {
                parse_error(ctx, &tk2, "long double is not supported.");
                return NULL;
            }
            type.category |= 0x08;
            can_be_followed_by_int = true;
        } else if (tk_equal(&tk, "short")) {
            type.category |= 0x02;
            can_be_followed_by_int = true;
        } else if (tk_equal(&tk, "char")) {
            type.category |= 0x01;
        } else if (tk_equal(&tk, "int")) {
            type.category |= 0x04;
        }

        if ((type.category & 0x0f)) {
            if ((type.category & 0xf0) == 0) {
                type.category |= 0x20;
            } else {
                strputf(&type_name, " %.*s", tk.length, tk.start);
            }
            ltk = tk;
            tk = next_token(o_s);
        }
        if (type.category && (type.category & 0x0f) == 0) {
            type.category |= 0x04;
        }

        if (can_be_followed_by_int) {
            if (tk_equal(&tk, "int")) {
                strputf(&type_name, " int");
                ltk = tk;
            }
        }

        o_tk->start = first.start;
        o_tk->type = TK_IDENTIFIER;
        o_tk->length = ltk.start - first.start + ltk.length;
        *o_s = ltk.start + ltk.length;
    }

    strputnull(type_name);
    IntroType * t = shget(ctx->type_map, type_name);
    if (t) {
        arrfree(type_name);
        return t;
    } else {
        type.name = type_name;
        IntroType * stored = store_type(ctx, &type);
        arrfree(type_name);
        return stored;
    }
}

static IntroType *
parse_declaration(ParseContext * ctx, IntroType * base_type, char ** o_s, char ** o_name) {
    int32_t * indirection = NULL;
    int32_t * temp = NULL;
    char * paren;

    const int32_t POINTER = -1;
    Token tk;
    char * end = *o_s;
    do {
        paren = NULL;

        int pointer_level = 0;
        while ((tk = next_token(o_s)).type == TK_STAR) {
            pointer_level += 1;
        }

        if (tk.type == TK_L_PARENTHESIS) {
            paren = tk.start + 1;
            *o_s = find_closing(tk.start) + 1;
            tk = next_token(o_s);
        }

        if (tk.type == TK_IDENTIFIER) {
            // TODO: leak?
            *o_name = copy_and_terminate(tk.start, tk.length);
            tk = next_token(o_s);
        }

        arrsetlen(temp, 0);
        while (tk.type == TK_L_BRACKET) {
            tk = next_token(o_s);
            if (tk.type == TK_IDENTIFIER) {
                long num = strtol(tk.start, NULL, 0);
                if (num <= 0) {
                    parse_error(ctx, &tk, "Invalid array size.");
                    return NULL;
                }
                arrput(temp, (uint32_t)num);
                tk = next_token(o_s);
                if (tk.type != TK_R_BRACKET) {
                    parse_error(ctx, &tk, "Invalid symbol. Expected closing bracket ']'.");
                    return NULL;
                }
            } else if (tk.type == TK_R_BRACKET) {
                arrput(temp, 0);
            } else {
                parse_error(ctx, &tk, "Invalid symbol. Expected array size or closing bracket ']'.");
                return NULL;
            }
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

int
parse_preprocessed_text(char * buffer, IntroInfo * o_info) {
    ParseContext * ctx = malloc(sizeof(ParseContext));
    memset(ctx, 0, sizeof(*ctx));
    ctx->buffer = buffer;

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
        {"size_t",   NULL, INTRO_U64}, // TODO
        {"ptrdiff_t",NULL, INTRO_S64},
        {"bool",     NULL, INTRO_U8 },
    };
    for (int i=0; i < LENGTH(known_types); i++) {
        store_type(ctx, &known_types[i]);
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
            } else if (tk_equal(&tk, "enum")) {
                error = parse_enum(ctx, &s);
            } else if (tk_equal(&tk, "typedef")) {
                error = parse_typedef(ctx, &s);
            }

            if (error) {
                return error;
            }
        } else if (tk.type == TK_L_BRACE) {
            s = find_closing(tk.start);
            if (!s) {
                // TODO(print error)
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
    return 0;
}
