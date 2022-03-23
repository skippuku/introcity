#include <string.h>
#include <assert.h>

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
} ParseContext;

void
parse_error(ParseContext * ctx, Token * tk, char * message) { // TODO
    (void) ctx;
    (void) tk;
    if (tk) {
        fprintf(stderr, "Error(tk: %.*s): %s\n", tk->length, tk->start, message);
    } else {
        fprintf(stderr, "Error: %s\n", message);
    }
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
    if (hmgeti(ctx->type_set, *stored) < 0) {
        hmput(ctx->type_set, *stored, stored);
    }
    // TODO: i am not a fan of this
    if (type->category == INTRO_STRUCT
     || type->category == INTRO_UNION
     || type->category == INTRO_ENUM)
    {
        for (int i=0; i < hmlen(ctx->type_set); i++) {
            IntroType * t = ctx->type_set[i].value;
            if (t->category == INTRO_UNKNOWN) {
                if (t->parent == original) {
                    t->i_struct = stored->i_struct; // covers i_enum
                    t->category = stored->category;
                    t->parent = stored;
                }
            }
        }
    }
    return stored;
}

static IntroType * parse_base_type(ParseContext *, char **);
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
    while (1) {
        IntroType * base_type = parse_base_type(ctx, o_s);
        if (!base_type) {
            Token tk = next_token(o_s);
            if (tk.type == TK_R_BRACE) {
                break;
            } else {
                parse_error(ctx, NULL, "Failed to parse base type.");
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
                    parse_error(ctx, NULL, "Unknown type."); // TODO: token
                    return 1;
                }
                member.type = type;

                #if 0 // TODO: handle this
                if (decl.is_nested) {
                    struct nested_info_s info = {0};
                    info.key = type.i_struct;
                    info.struct_index = struct_index;
                    info.member_index = arrlen(members);
                    hmputs(nested_info, info);
                }
                #endif

                tk = next_token(o_s);
                int error = maybe_expect_attribute(ctx, o_s, arrlen(members), &tk, &attribute_specifiers);
                if (error) return error;

                arrput(members, member);

                if (tk.type == TK_SEMICOLON) {
                    break;
                } else if (tk.type == TK_COMMA) {
                } else {
                    parse_error(ctx, &tk, "Cannot parse symbol in member declaration. Expected ';' or ','.");
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
        strputf(&struct_type_name, "%s ", is_union ? "union" : "struct");
        if (name_tk.type == TK_IDENTIFIER) {
            strputf(&struct_type_name, "%.*s", name_tk.length, name_tk.start);
        } else {
            static int anon_index = 0; // NOTE: not sure about this...
            strputf(&struct_type_name, "Anon_%i", anon_index++); 
        }
        strputnull(struct_type_name);

        IntroType type;
        type.name = struct_type_name;
        type.category = INTRO_STRUCT;
        type.i_struct = result;
        
        store_type(ctx, &type);
        arrfree(struct_type_name);
#if 0 // TODO
        if (prev != NULL) {
            if (prev->category == INTRO_UNKNOWN) {
                for (int i=0; i < arrlen(prev->forward_list); i++) {
                    KnownType * ft = &known_types[prev->forward_list[i]];
                    ft->category = INTRO_STRUCT;
                    ft->i_struct = result;
                }
                arrfree(prev->forward_list);
            } else {
                parse_error(ctx, &name_tk, "Redefinition of type.");
                return 1;
            }
        }
        shputs(known_types, struct_type);
#endif
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
        strputf(&enum_type_name, "enum ");
        if (name_tk.type == TK_IDENTIFIER) {
            strputf(&enum_type_name, "%.*s", name_tk.length, name_tk.start);
        } else {
            static int anon_index = 0; // NOTE: hmm
            strputf(&enum_type_name, "Anon_%i", anon_index++);
        }
        strputnull(enum_type_name);

        IntroType type;
        type.name = enum_type_name;
        type.category = INTRO_ENUM;
        type.i_enum = result;

        store_type(ctx, &type);
        arrfree(enum_type_name);

#if 0 // TODO
        if (prev != NULL) {
            if (prev->category == INTRO_UNKNOWN) {
                for (int i=0; i < arrlen(prev->forward_list); i++) {
                    KnownType * ft = &known_types[prev->forward_list[i]];
                    ft->category = INTRO_ENUM;
                    ft->i_enum = result;
                }
                arrfree(prev->forward_list);
            } else {
                parse_error(&name_tk, "Redefinition of type.");
                return 1;
            }
        }
        shputs(known_types, enum_type);
#endif
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
    IntroType * base = parse_base_type(ctx, o_s);
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
    if (new_type.category == INTRO_STRUCT
     || new_type.category == INTRO_UNION
     || new_type.category == INTRO_ENUM)
    {
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
parse_base_type(ParseContext * ctx, char ** o_s) {
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

    struct { // TODO: not sure what to do with this
        bool is_nested;
    } result = {0};
    Token error_tk = {0};
    error_tk.start = first.start;
    error_tk.type = TK_IDENTIFIER;

    bool is_struct = tk_equal(&first, "struct");
    bool is_union  = tk_equal(&first, "union");
    bool is_enum   = tk_equal(&first, "enum");
    if (is_struct || is_union || is_enum) {
        char * after_keyword = *o_s;
        int error;
        //strputf(&type_name, "%.*s", first.length, first.start);
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
            result.is_nested = true; // TODO: does this matter?
            ptrdiff_t last_index = shlen(ctx->type_map) - 1;
            return ctx->type_map[last_index].value;
        }
    } else {
        Token tk, ltk = first;
        if (tk_equal(&first, "unsigned")) {
            type.category = INTRO_UNSIGNED;
        } else if (tk_equal(&first, "signed")) {
            type.category = INTRO_SIGNED;
        }

        if (type.category) {
            //strputf(&type_name, "%.*s", first.length, first.start);
            tk = next_token(o_s);
        } else {
            tk = first;
        }

        bool can_be_followed_by_int = false;
        if (tk_equal(&tk, "long")) {
            Token tk2 = next_token(o_s);
            if (tk_equal(&tk2, "long")) {
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
            ltk = tk;
            tk = next_token(o_s);
            if ((type.category & 0xf0) == 0) {
                type.category |= 0x20;
            } else {
                strputf(&type_name, " %.*s", tk.length, tk.start);
            }
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

        error_tk.length = ltk.start - first.start + ltk.length;
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

    IntroType * type_list = NULL;
    arrsetcap(type_list, hmlen(ctx->type_set));
    for (int i=0; i < hmlen(ctx->type_set); i++) {
        arrput(type_list, *ctx->type_set[i].value);
    }

    o_info->count_types = arrlen(type_list);
    o_info->types = type_list;
    return 0;
}
