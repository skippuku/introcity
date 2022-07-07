enum AttributeToken {
    ATTR_TK_GLOBAL = INTRO_AT_COUNT + 1,
    ATTR_TK_INHERIT,
    ATTR_TK_ATTRIBUTE,
    ATTR_TK_APPLY_TO,
    ATTR_TK_REPRESS,
    ATTR_TK_INVALID
};

#define EXPECT(x) \
    tk = next_token(tidx); \
    if (tk.start[0] != x) { \
        parse_error(ctx, tk, "Expected " #x "."); \
        return -1; \
    }

void
attribute_parse_init(ParseContext * ctx) {
    static const struct { const char * key; int value; } attribute_keywords [] = {
        {"flag",    INTRO_AT_FLAG},
        {"int",     INTRO_AT_INT},
        {"float",   INTRO_AT_FLOAT},
        {"value",   INTRO_AT_VALUE},
        {"member",  INTRO_AT_MEMBER},
        {"string",  INTRO_AT_STRING},
        {"type",    INTRO_AT_TYPE},
        {"expr",    INTRO_AT_EXPR},
        {"__remove",INTRO_AT_REMOVE},
        {"global",  ATTR_TK_GLOBAL},
        {"inherit", ATTR_TK_INHERIT},
        {"attribute", ATTR_TK_ATTRIBUTE},
        {"apply_to", ATTR_TK_APPLY_TO},
        {"repress", ATTR_TK_APPLY_TO},
    };
    shdefault(ctx->attribute_token_map, ATTR_TK_INVALID);
    for (int i=0; i < LENGTH(attribute_keywords); i++) {
        shput(ctx->attribute_token_map, attribute_keywords[i].key, attribute_keywords[i].value);
    }

    shdefault(ctx->builtin_map, -1);
    for (int i=0; i < LENGTH(g_builtin_attributes); i++) {
        shput(ctx->builtin_map, g_builtin_attributes[i].key, g_builtin_attributes[i].value);
    }

    sh_new_arena(ctx->attribute_map);
    ctx->attribute_id_counter = 0;
}

int
parse_global_directive(ParseContext * ctx, TokenIndex * tidx) {
    Token tk = next_token(tidx);
    if (tk.type != TK_L_PARENTHESIS) {
        parse_error(ctx, tk, "Expected '('.");
        return -1;
    }

    tk = next_token(tidx);
    char temp [1024];
    memcpy(temp, tk.start, tk.length);
    temp[tk.length] = 0;
    int a_tk = shget(ctx->attribute_token_map, temp);
    if (a_tk == ATTR_TK_ATTRIBUTE) {
        tk = next_token(tidx);
        char * namespace = NULL;
        if (tk.type == TK_IDENTIFIER) {
            namespace = copy_and_terminate(ctx->arena, tk.start, tk.length);
            tk = next_token(tidx);
        }
        if (tk.type != TK_L_PARENTHESIS) {
            parse_error(ctx, tk, "Expected. '('.");
            return -1;
        }

        while (1) {
            AttributeParseInfo info = {0};

            tk = next_token(tidx);
            if (tk.type == TK_R_PARENTHESIS) {
                break;
            } else if (tk.type != TK_IDENTIFIER) {
                parse_error(ctx, tk, "Expected identifier for attribute name.");
            }
            char namespaced [1024];
            char unspaced [1024];
            memcpy(unspaced, tk.start, tk.length);
            unspaced[tk.length] = 0;
            strcpy(namespaced, namespace);
            strcat(namespaced, unspaced);
            if (shgeti(ctx->attribute_map, namespaced) >= 0) {
                parse_error(ctx, tk, "Attribute name is reserved.");
                return -1;
            }
            tk = next_token(tidx);
            if (tk.type != TK_COLON) {
                parse_error(ctx, tk, "Expected ':'.");
                return -1;
            }

            tk = next_token(tidx);
            memcpy(temp, tk.start, tk.length);
            temp[tk.length] = 0;
            int a_tk = shget(ctx->attribute_token_map, temp);
            if (a_tk >= INTRO_AT_COUNT) {
                parse_error(ctx, tk, "Invalid attribute type.");
                return -1;
            }
            info.type = a_tk;

            if (info.type != INTRO_AT_FLAG) {
                info.id = ctx->attribute_id_counter++;
            }

            tk = next_token(tidx);
            if (tk.type == TK_L_PARENTHESIS && a_tk == INTRO_AT_VALUE) {
                tk = next_token(tidx);
                if (tk.type == TK_AT) {
                    tk = next_token(tidx);
                    if (!tk_equal(&tk, "inherit")) {
                        parse_error(ctx, tk, "Invalid trait.");
                        return -1;
                    }
                    tk = next_token(tidx);
                    info.type_ptr = NULL;
                } else {
                    tidx->index--;
                    DeclState decl = {.state = DECL_CAST};
                    int ret = parse_declaration(ctx, tidx, &decl);
                    if (ret == RET_DECL_FINISHED || ret == RET_DECL_CONTINUE) {
                        info.type_ptr = decl.type;
                    } else if (ret == RET_NOT_TYPE) {
                        parse_error(ctx, decl.base_tk, "Not a type.");
                        return -1;
                    } else {
                        return -1;
                    }
                    tk = next_token(tidx);
                }
                if (tk.type != TK_R_PARENTHESIS) {
                    parse_error(ctx, tk, "Expected ')'.");
                    return -1;
                }
                tk = next_token(tidx);
            }

            if (tk.type == TK_AT) {
                tk = next_token(tidx);
                if (tk_equal(&tk, "global")) {
                    if (info.type != INTRO_AT_FLAG) {
                        parse_error(ctx, tk, "Only flag attributes can have the trait global.");
                        return -1;
                    }
                    info.global = true;
                } else if (tk_equal(&tk, "repress")) {
                    info.repress = true;
                } else {
                    parse_error(ctx, tk, "Invalid trait.");
                    return -1;
                }
                tk = next_token(tidx);
            }

            shput(ctx->attribute_map, namespaced, info);
            ptrdiff_t unspaced_index = shgeti(ctx->attribute_map, unspaced);
            if (unspaced_index < 0) {
                info.without_namespace = true;
                shput(ctx->attribute_map, unspaced, info);
            } else {
                ctx->attribute_map[unspaced_index].value.invalid_without_namespace = true;
            }

            if (tk.type == TK_COMMA) {
                continue;
            } else if (tk.type == TK_R_PARENTHESIS) {
                break;
            } else {
                parse_error(ctx, tk, "Expected ',' or ')'.");
                return -1;
            }
        }
    } else if (a_tk == ATTR_TK_APPLY_TO) {
        EXPECT('(');
        DeclState decl = {.state = DECL_CAST};
        int ret = parse_declaration(ctx, tidx, &decl);
        if (ret < 0) return -1;
        EXPECT(')');
        EXPECT('(');
        tidx->index--;
        int32_t start_directive_index = tidx->index;
        tidx->index = find_closing(*tidx);
        AttributeDirective directive = {
            .type = decl.type,
            .location = start_directive_index,
            .member_index = MIDX_TYPE,
        };
        arrput(ctx->attribute_directives, directive);
    } else {
        parse_error(ctx, tk, "Invalid. Expected 'attribute' or 'apply_to'.");
        return -1;
    }

    tk = next_token(tidx);
    if (tk.type != TK_R_PARENTHESIS) {
        parse_error(ctx, tk, "Missing ')'.");
        return -1;
    }
    return 0;
}

static bool
check_id_valid(const IntroType * type, int id) {
#if 0 // TODO
    if (id < 0 || ((id & 0xFFFF) != id)) {
        return false;
    }
    // Not sure how to do this yet
#endif
    return true;
}

static char *
parse_escaped_string(Token * str_tk, size_t * o_length) {
    char * result = NULL;
    char * src = str_tk->start + 1;
    while (src < str_tk->start + str_tk->length - 1) {
        if (*src == '\\') {
            src++;
            char c;
            if (*src >= '0' && *src <= '9') {
                char * end;
                long num = strtol(src, &end, 8);
                if (end - src > 3) return NULL;
                src = end - 1;
                c = (char)num;
            } else {
                switch(*src) {
                case 'n':  c = '\n'; break;
                case 't':  c = '\t'; break;
                case '\\': c = '\\'; break;
                case '\'': c = '\''; break;
                case '\"': c = '\"'; break;
                case 'b':  c = '\b'; break;
                case 'v':  c = '\v'; break;
                case 'r':  c = '\r'; break;
                case 'f':  c = '\f'; break;
                case '?':  c = '?' ; break;
                case 'x': {
                    char * end;
                    src++;
                    long num = strtol(src, &end, 16);
                    if (end - src != 2) return NULL;
                    src = end - 1;
                    c = (char)num;
                }break;
                default: {
                    return NULL;
                }break;
                }
            }
            arrput(result, c);
        } else {
            arrput(result, *src);
        }
        src++;
    }
    arrput(result, 0);

    char * ret = malloc(arrlen(result));
    memcpy(ret, result, arrlen(result));
    *o_length = arrlen(result);
    arrfree(result);
    return ret;
}

// NOTE: value storing is fairly similar to some of what the city implementation does,
// maybe some code can be resued between those two systems
ptrdiff_t
store_value(ParseContext * ctx, const void * value, size_t value_size) {
    void * storage = arraddnptr(ctx->value_buffer, value_size);
    memcpy(storage, value, value_size); // NOTE: this is only correct for LE
    return (storage - (void *)ctx->value_buffer);
}

ptrdiff_t
store_ptr(ParseContext * ctx, void * data, size_t size) {
    static const uint8_t nothing [sizeof(size_t)] = {0};
    ptrdiff_t offset = store_value(ctx, &nothing, sizeof(size_t));
    PtrStore ptr_store = {0};
    ptr_store.value_offset = offset;
    ptr_store.data = data;
    ptr_store.data_size = size;
    arrput(ctx->ptr_stores, ptr_store);
    return offset;
}

ptrdiff_t parse_array_value(ParseContext * ctx, const IntroType * type, TokenIndex * tidx, uint32_t * o_count);
ptrdiff_t parse_struct_value(ParseContext * ctx, const IntroType * type, TokenIndex * tidx);

// TODO: change to parse_expression
ptrdiff_t
parse_value(ParseContext * ctx, IntroType * type, TokenIndex * tidx, uint32_t * o_count) {
    char * end;
    if ((type->category >= INTRO_U8 && type->category <= INTRO_S64) || type->category == INTRO_ENUM) {
        intmax_t result = parse_constant_expression(ctx, tidx);
        return store_value(ctx, &result, type->size);
    } else if (type->category == INTRO_F32) {
        float result = strtof(tk_at(tidx).start, &end);
        advance_to(tidx, end);
        return store_value(ctx, &result, 4);
    } else if (type->category == INTRO_F64) {
        double result = strtod(tk_at(tidx).start, &end);
        advance_to(tidx, end);
        return store_value(ctx, &result, 8);
    } else if (type->category == INTRO_POINTER) {
        Token tk = next_token(tidx);
        if (tk.type == TK_STRING) {
            if (type->of->category == INTRO_S8 && 0==strcmp(type->of->name, "char")) {
                size_t length;
                char * str = parse_escaped_string(&tk, &length);
                if (!str) {
                    parse_error(ctx, tk, "Invalid string.");
                    return -1;
                }
                ptrdiff_t result = store_ptr(ctx, str, length);
                return result;
            }
        } else {
            tidx->index--;
            ptrdiff_t array_value_offset = parse_array_value(ctx, type, tidx, o_count);
            ptrdiff_t pointer_offset = store_value(ctx, &array_value_offset, sizeof(size_t));
            return pointer_offset;
        }
    } else if (type->category == INTRO_ARRAY) {
        if (type->of->category == INTRO_S8 && 0==strcmp(type->of->name, "char")) { // NOTE: this should probably check attributes instead
            Token tk = next_token(tidx);
            int32_t tk_index = tidx->index;
            if (tk.type == TK_STRING) {
                size_t str_length;
                char * str = parse_escaped_string(&tk, &str_length);
                if (!str) {
                    parse_error(ctx, tk, "Invalid string.");
                    return -1;
                }
                ptrdiff_t result = arrlen(ctx->value_buffer);
                char * dest = (char *)arraddnptr(ctx->value_buffer, intro_size(type));
                memset(dest, 0, intro_size(type));
                strcpy(dest, str);
                free(str);

                return result;
            }
            tidx->index = tk_index;
        }
        ptrdiff_t result = parse_array_value(ctx, type, tidx, NULL);
        return result;
    } else if (intro_has_fields(type)) {
        ptrdiff_t result = parse_struct_value(ctx, type, tidx);
        return result;
    }
    return -1;
}

ptrdiff_t
parse_array_value(ParseContext * ctx, const IntroType * type, TokenIndex * tidx, uint32_t * o_count) {
    Token tk;

    uint8_t * prev_buf = ctx->value_buffer;
    ctx->value_buffer = NULL;
    arrsetcap(ctx->value_buffer, type->size);
    memset(ctx->value_buffer, 0, type->size);

    int count_ptrs_last = arrlen(ctx->ptr_stores);

    size_t array_element_size = type->of->size;
    intmax_t index = 0;
    intmax_t highest_index = 0;

    EXPECT('{');
    while (1) {
        int32_t tk_index = tidx->index;
        tk = next_token(tidx);
        if (tk.type == TK_L_BRACKET) {
            index = parse_constant_expression(ctx, tidx);
            if (index < 0 || index >= type->count) {
                parse_error(ctx, tk, "Invalid index.");
                return -1;
            }
            EXPECT(']');
            EXPECT('=');
        } else if (tk.type == TK_COMMA) {
            parse_error(ctx, tk, "Invalid symbol.");
            return -1;
        } else if (tk.type == TK_R_BRACE) {
            break;
        } else {
            tidx->index = tk_index;
        }
        if (highest_index < index) highest_index = index;
        arrsetlen(ctx->value_buffer, index * array_element_size);
        ptrdiff_t parse_ret = parse_value(ctx, type->of, tidx, NULL);
        if (parse_ret < 0) {
            return -1;
        }

        tk = next_token(tidx);
        if (tk.type == TK_COMMA) {
            index++;
        } else if (tk.type == TK_R_BRACE) {
            break;
        } else {
            parse_error(ctx, tk, "Invalid symbol.");
            return -1;
        }
    }
    uint32_t count = highest_index + 1;
    if (type->category == INTRO_ARRAY && type->count > 0) {
        arrsetlen(ctx->value_buffer, type->size);
    } else {
        arrsetlen(ctx->value_buffer, count * array_element_size);
    }

    uint8_t * temp_buf = ctx->value_buffer;
    ctx->value_buffer = prev_buf;
    ptrdiff_t result = store_value(ctx, temp_buf, arrlen(temp_buf));
    arrfree(temp_buf);

    for (int i = count_ptrs_last; i < arrlen(ctx->ptr_stores); i++) {
        ctx->ptr_stores[i].value_offset += result;
    }

    if (o_count) *o_count = count;
    return result;
}

ptrdiff_t
parse_struct_value(ParseContext * ctx, const IntroType * type, TokenIndex * tidx) {
    Token tk;
    EXPECT('{');

    uint8_t * prev_buf = ctx->value_buffer;
    ctx->value_buffer = NULL;
    arrsetcap(ctx->value_buffer, type->size);
    memset(ctx->value_buffer, 0, type->size);

    int count_ptrs_last = arrlen(ctx->ptr_stores);

    int member_index = 0;
    while (1) {
        tk = next_token(tidx);
        if (tk.type == TK_PERIOD) {
            tk = next_token(tidx);
            if (tk.type != TK_IDENTIFIER) {
                parse_error(ctx, tk, "Expected identifier.");
                return -1;
            }
            bool found_match = false;
            for (int i=0; i < type->count; i++) {
                IntroMember check = type->members[i];
                if (tk_equal(&tk, check.name)) {
                    found_match = true;
                    member_index = i;
                    break;
                }
            }
            if (!found_match) {
                char buf [1024];
                if (type->name) {
                    stbsp_sprintf(buf, "Not a member of %s.", type->name);
                } else {
                    strcpy(buf, "Invalid member name.");
                }
                parse_error(ctx, tk, buf);
                return -1;
            }
            EXPECT('=');
        } else if (tk.type == TK_R_BRACE) {
            break;
        } else {
            tidx->index--;
        }

        IntroMember member = type->members[member_index];
        arrsetlen(ctx->value_buffer, member.offset);
        ptrdiff_t parse_ret = parse_value(ctx, member.type, tidx, NULL);
        if (parse_ret < 0) {
            return -1;
        }

        tk = next_token(tidx);
        if (tk.type == TK_COMMA) {
            member_index += 1;
        } else if (tk.type == TK_R_BRACE) {
            break;
        } else {
            parse_error(ctx, tk, "Expected ',' or '}'.");
            return -1;
        }
    }

    uint8_t * struct_buf = ctx->value_buffer;
    ctx->value_buffer = prev_buf;
    ptrdiff_t result = store_value(ctx, struct_buf, arrlen(struct_buf));
    arrfree(struct_buf);

    for (int i = count_ptrs_last; i < arrlen(ctx->ptr_stores); i++) {
        ctx->ptr_stores[i].value_offset += result;
    }

    return result;
}

void
store_deferred_ptrs(ParseContext * ctx) {
    for (int i=0; i < arrlen(ctx->ptr_stores); i++) {
        PtrStore ptr_store = ctx->ptr_stores[i];
        store_value(ctx, &ptr_store.data_size, 4);
        size_t offset = store_value(ctx, ptr_store.data, ptr_store.data_size);
        size_t * o_offset = (size_t *)(ctx->value_buffer + ptr_store.value_offset);
        memcpy(o_offset, &offset, sizeof(offset));
        free(ptr_store.data);
    }
    arrsetlen(ctx->ptr_stores, 0);
}

int
handle_value_attribute(ParseContext * ctx, TokenIndex * tidx, IntroType * type, int member_index, AttributeData * data, Token * p_tk) {
    IntroAttribute attr_info = ctx->p_info->attr.available[data->id];
    IntroType * v_type;
    if (attr_info.type_id == 0) {
        v_type = type->members[member_index].type;
    } else {
        v_type = ctx->p_info->types[attr_info.type_id];
    }
    assert(arrlen(ctx->ptr_stores) == 0);
    uint32_t length_value = 0;
    ptrdiff_t value_offset = parse_value(ctx, v_type, tidx, &length_value);
    if (value_offset < 0) {
        parse_error(ctx, *p_tk, "Error parsing value attribute.");
        return -1;
    }
    if (length_value) {
        DeferredDefault def = {
            .type = type,
            .member_index = member_index,
            .attr_id = data->id,
            .value = length_value,
        };
        arrput(ctx->deferred_length_defaults, def);
    }
    store_deferred_ptrs(ctx);
    data->v.i = value_offset;
    return 0;
}

int32_t
parse_attribute_id(ParseContext * ctx, TokenIndex * tidx) {
    Token tk = next_token(tidx);
    STACK_TERMINATE(term, tk.start, tk.length);
    int map_index = shgeti(ctx->attribute_map, term);
    if (map_index < 0) {
        parse_error(ctx, tk, "No such attribute.");
        return -1;
    }

    AttributeParseInfo attr_info = ctx->attribute_map[map_index].value;
    if (attr_info.invalid_without_namespace) {
        parse_error(ctx, tk, "Name matches more than one namespace.");
        return -1;
    }
    uint32_t id = attr_info.final_id;
    return id;
}

int
parse_attribute(ParseContext * ctx, TokenIndex * tidx, IntroType * type, int member_index, AttributeData * o_result) {
    AttributeData data = {0};

    Token tk = next_token(tidx);
    if (tk.type != TK_IDENTIFIER) {
        parse_error(ctx, tk, "Expected identifier.");
        return -1;
    }

    STACK_TERMINATE(terminated_name, tk.start, tk.length);
    int map_index = shgeti(ctx->attribute_map, terminated_name);
    if (map_index < 0) {
        parse_error(ctx, tk, "No such attribute.");
        return -1;
    }

    AttributeParseInfo attr_info = ctx->attribute_map[map_index].value;
    if (attr_info.invalid_without_namespace) {
        parse_error(ctx, tk, "Name matches more than one namespace.");
        return -1;
    }
    data.id = attr_info.final_id;
    IntroAttributeType attribute_type = attr_info.type;

    char * end;
    switch(attribute_type) {
    default: break;
    case INTRO_AT_FLAG: {
        if (data.id == ctx->builtin.i_type) {
            IntroType * mtype = type->members[member_index].type;
            if (!(mtype->category == INTRO_POINTER && strcmp(mtype->of->name, "IntroType") == 0)) {
                parse_error(ctx, tk, "Member must be of type 'IntroType *' to have type attribute.");
                char typename [1024];
                intro_sprint_type_name(typename, mtype);
                fprintf(stderr, "member type is %s\n", typename);
                return -1;
            }
        }
        data.v.i = 0;
    } break;

    case INTRO_AT_INT: {
        tk = next_token(tidx);
        long result = strtol(tk.start, &end, 0);
        if (end == tk.start) {
            parse_error(ctx, tk, "Invalid integer.");
            return -1;
        }
        data.v.i = (int32_t)result;
        if (data.id == ctx->builtin.i_id) {
            if (!check_id_valid(type, data.v.i)) {
                parse_error(ctx, tk, "This ID is reserved.");
                return -1;
            }
        }
    } break;

    case INTRO_AT_FLOAT: {
        tk = next_token(tidx);
        float result = strtof(tk.start, &end);
        advance_to(tidx, end);
        if (end == tk.start) {
            parse_error(ctx, tk, "Invalid floating point number.");
            return -1;
        }
        data.v.f = result;
    } break;

    case INTRO_AT_STRING: {
        tk = next_token(tidx);
        char * result = NULL;
        if (data.id == ctx->builtin.i_alias && tk.type == TK_IDENTIFIER) {
            result = copy_and_terminate(ctx->arena, tk.start, tk.length);
        } else {
            if (tk.type != TK_STRING) {
                parse_error(ctx, tk, "Expected string.");
                return -1;
            }
            result = copy_and_terminate(ctx->arena, tk.start+1, tk.length-2);
        }
        int32_t index = arrlen(ctx->string_set);
        arrput(ctx->string_set, result);
        data.v.i = index;
    } break;

    // TODO: error check: member used as length can't have a value if the member it is the length of has a value
    // TODO: error check: if multiple members use the same member for length, values can't have different lengths
    // NOTE: the previous 2 todo's do not apply if the values are for different attributes
    case INTRO_AT_VALUE: {
        if (handle_value_attribute(ctx, tidx, type, member_index, &data, &tk)) return -1;
    } break;

    case INTRO_AT_MEMBER: {
        tk = next_token(tidx);
        if (tk.type != TK_IDENTIFIER || is_digit(tk.start[0])) {
            parse_error(ctx, tk, "Expected member name.");
            return -1;
        }
        bool success = false;
        for (int mi=0; mi < type->count; mi++) {
            if (tk_equal(&tk, type->members[mi].name)) {
                if (data.id == ctx->builtin.i_length) {
                    IntroType * mtype = type->members[mi].type;
                    uint32_t category_no_size = mtype->category & 0xf0;
                    if (category_no_size != INTRO_SIGNED && category_no_size != INTRO_UNSIGNED) {
                        parse_error(ctx, tk, "Length defining member must be of an integer type.");
                        return -1;
                    }
                }
                data.v.i = mi;
                success = true;
                break;
            }
        }
        if (!success) {
            parse_error(ctx, tk, "No such member.");
            return -1;
        }
    } break;

    case INTRO_AT_REMOVE: {
        int32_t id = parse_attribute_id(ctx, tidx);
        if (id < 0) {
            return -1;
        }
        data.v.i = id;
    }break;
    }

    if (o_result) *o_result = data;
    return 0;
}

int
parse_attributes(ParseContext * ctx, AttributeDirective * directive) {
    TokenIndex _tidx, * tidx = &_tidx;
    tidx->list = ctx->tk_list;
    tidx->index = directive->location;
    AttributeData * attributes = NULL;

    Token tk = next_token(tidx);
    if (tk.type != TK_L_PARENTHESIS) {
        parse_error(ctx, tk, "Expected '('.");
        return -1;
    }
    while (1) {
        tk = next_token(tidx);
        AttributeData data;
        if (tk.type == TK_IDENTIFIER) {
            if (is_digit(tk.start[0])) {
                data.id = ctx->builtin.i_id;
                // @copy from above
                char * end;
                long result = strtol(tk.start, &end, 0);
                advance_to(tidx, end);
                if (end == tk.start) {
                    parse_error(ctx, tk, "Invalid integer.");
                    return -1;
                }
                data.v.i = (int32_t)result;
                if (!check_id_valid(directive->type, data.v.i)) {
                    parse_error(ctx, tk, "This ID is reserved.");
                    return -1;
                }
            } else {
                tidx->index--;
                int error = parse_attribute(ctx, tidx, directive->type, directive->member_index, &data);
                if (error) return -1;
            }
            arrput(attributes, data);
        } else if (tk.type == TK_EQUAL) {
            data.id = ctx->builtin.i_default;
            if (handle_value_attribute(ctx, tidx, directive->type, directive->member_index, &data, &tk)) return -1;

            arrput(attributes, data);
        } else if (tk.type == TK_TILDE) {
            data.id = ctx->builtin.i_remove;
            int32_t remove_id = parse_attribute_id(ctx, tidx);
            if (remove_id < 0) return -1;
            data.v.i = remove_id;

            arrput(attributes, data);
        } else if (tk.type == TK_R_PARENTHESIS) {
            break;
        } else {
            parse_error(ctx, tk, "Invalid symbol.");
            return -1;
        }

        tk = next_token(tidx);
        if (tk.type == TK_COMMA) {
        } else if (tk.type == TK_R_PARENTHESIS) {
            break;
        } else {
            parse_error(ctx, tk, "Expected ',' or ')'.");
            return -1;
        }
    }

    directive->count = arrlenu(attributes);
    directive->attr_data = attributes;
    return 0;
}

static void
add_attribute(ParseContext * ctx, ParseInfo * o_info, AttributeParseInfo * info, AttributeParseInfo * next_info, char * name) {
    IntroAttribute attribute = {
        .name = name,
        .attr_type = info->type,
        .type_id = hmget(o_info->index_by_ptr_map, info->type_ptr),
    };
    int final_id = arrlen(o_info->attr.available);
    arrput(o_info->attr.available, attribute);
    info->final_id = final_id;
    if (next_info) next_info->final_id = final_id;
    int builtin_offset = shget(ctx->builtin_map, name);
    if (builtin_offset >= 0) {
        *((uint8_t *)&ctx->builtin + builtin_offset) = (uint8_t)final_id;
    }
    if (info->global) {
        AttributeData data = {.id = final_id};
        arrput(ctx->attribute_globals, data);
    }
}

int
attribute_data_sort_callback(const void * p_a, const void * p_b) {
    const AttributeData a = *(AttributeData *)p_a;
    const AttributeData b = *(AttributeData *)p_b;
    return a.id - b.id;
}

void
apply_attributes_to_member(ParseContext * ctx, IntroType * type, int32_t member_index, AttributeData * data, int32_t count) {
    AttributeDataKey key = {
        .type = type,
        .member_index = member_index,
    };
    AttributeDataMap * pcontent = hmgetp_null(ctx->attribute_data_map, key);
    if (!pcontent) {
        hmput(ctx->attribute_data_map, key, NULL);
        pcontent = hmgetp_null(ctx->attribute_data_map, key);
        assert(pcontent != NULL);
    }
    for (int i=0; i < count; i++) {
        bool do_remove = data[i].id == ctx->builtin.i_remove;
        uint32_t check_id = (do_remove)? data[i].v.i : data[i].id;
        for (int j=0; j < arrlen(pcontent->value); j++) {
            if (pcontent->value[j].id == check_id) {
                arrdelswap(pcontent->value, j);
                break;
            }
        }
        if (!do_remove) arrput(pcontent->value, data[i]);
    }
}

static void
handle_deferred_defaults(ParseContext * ctx) {
    for (int i=0; i < arrlen(ctx->deferred_length_defaults); i++) {
        DeferredDefault def = ctx->deferred_length_defaults[i];
        AttributeDataKey key = {
            .type = def.type,
            .member_index = def.member_index,
        };
        AttributeDataMap * pcontent = hmgetp_null(ctx->attribute_data_map, key);
        assert(pcontent != NULL);

        IntroMember * length_member = NULL;
        int32_t length_member_index = -1;
        for (int a=0; a < arrlen(pcontent->value); a++) {
            if (pcontent->value[a].id == ctx->builtin.i_length) {
                length_member_index = pcontent->value[a].v.i;
                length_member = &def.type->members[length_member_index];
                break;
            }
        }
        if (!length_member) continue;

        ptrdiff_t value_offset = store_value(ctx, &def.value, intro_size(length_member->type));
        AttributeData data = {
            .id = def.attr_id,
            .v.i = value_offset,
        };

        apply_attributes_to_member(ctx, def.type, length_member_index, &data, 1);
    }
    arrsetlen(ctx->deferred_length_defaults, 0);
}

static void
handle_attributes(ParseContext * ctx, ParseInfo * o_info) {
    int * flags = NULL;
    arrsetcap(flags, 16);
    arrsetcap(o_info->attr.available, 32);
    arrsetcap(ctx->value_buffer, (1 << 16));

    for (int i=0; i < hmlen(ctx->attribute_map); i++) {
        AttributeParseInfo * info = &ctx->attribute_map[i].value;
        AttributeParseInfo * next_info = NULL;
        int index = i;
        if (i+1 < hmlen(ctx->attribute_map)) {
            next_info = &ctx->attribute_map[i+1].value;
            if (next_info->id != info->id) {
                next_info = NULL;
            } else {
                i++;
            }
        }
        if (info->type == INTRO_AT_FLAG) {
            arrput(flags, index);
            if (next_info) {
                info->next_is_same = true;
            }
            continue;
        }
        add_attribute(ctx, o_info, info, next_info, ctx->attribute_map[index].key);
    }

    o_info->attr.first_flag = arrlen(o_info->attr.available);
    for (int i=0; i < arrlen(flags); i++) {
        int index = flags[i];
        char * name = ctx->attribute_map[index].key;
        AttributeParseInfo * info = &ctx->attribute_map[index].value;
        AttributeParseInfo * next_info = (info->next_is_same)? &ctx->attribute_map[index+1].value : NULL;
        add_attribute(ctx, o_info, info, next_info, name);
    }
    arrfree(flags);

    IntroAttributeSpec * empty = arraddnptr(o_info->attr.spec_buffer, 1);
    memset(empty, 0, sizeof(*empty));

    for (int directive_i=0; directive_i < arrlen(ctx->attribute_directives); directive_i++) {
        AttributeDirective directive = ctx->attribute_directives[directive_i];
        int ret = parse_attributes(ctx, &directive);
        if (ret) exit(1);

        // add global flags
        apply_attributes_to_member(ctx, directive.type, directive.member_index, ctx->attribute_globals, arrlen(ctx->attribute_globals));

        // add type attributes
        AttributeDataKey key = {0};
        key.member_index = MIDX_TYPE;
        if (directive.member_index >= 0) {
            const IntroMember member = directive.type->members[directive.member_index];
            key.type = member.type;
        } else {
            if (directive.type->parent) {
                key.type = directive.type->parent;
            }
        }
        if (key.type != NULL) {
            AttributeData * type_attr_data = hmget(ctx->attribute_data_map, key);
            if (arrlen(type_attr_data) > 0) {
                apply_attributes_to_member(ctx, directive.type, directive.member_index, type_attr_data, arrlen(type_attr_data));
            }
        }

        // add attributes from directive
        apply_attributes_to_member(ctx, directive.type, directive.member_index, directive.attr_data, directive.count);
    }

    handle_deferred_defaults(ctx);

    for (int data_i=0; data_i < hmlen(ctx->attribute_data_map); data_i++) {
        AttributeDataMap content = ctx->attribute_data_map[data_i];
        IntroType * type = content.key.type;
        int32_t member_index = content.key.member_index;
        AttributeData * attr_data = content.value;

        uint32_t spec_index = arrlen(o_info->attr.spec_buffer);
        int32_t count = arrlen(attr_data);

        qsort(attr_data, count, sizeof(attr_data[0]), &attribute_data_sort_callback);

        int count_without_flags = 0;
        for (int i=0; i < count; i++) {
            if (attr_data[i].id < o_info->attr.first_flag) {
                count_without_flags++;
            }
        }
        int count_16_byte_sections_needed = 1 + ((count_without_flags * sizeof(uint32_t)) + 15) / 16;
        IntroAttributeSpec * spec = arraddnptr(o_info->attr.spec_buffer, count_16_byte_sections_needed);
        memset(spec, 0, count_16_byte_sections_needed * sizeof(*spec));

        for (int i=0; i < count; i++) {
            uint32_t bitset_index = attr_data[i].id >> 5; 
            uint32_t bit_index = attr_data[i].id & 31;
            uint32_t attr_bit = 1 << bit_index;
            spec->bitset[bitset_index] |= attr_bit;

            if (attr_data[i].id < o_info->attr.first_flag) {
                uint32_t * value_offsets = (uint32_t *)(spec + 1);
                memcpy(&value_offsets[i], &attr_data[i].v, sizeof(uint32_t));
            }
        }

        switch(member_index) {
        case MIDX_TYPE: {
            type->attr = spec_index;
        }break;

        default: {
            type->members[member_index].attr = spec_index;
        }break;
        }

        arrfree(attr_data);
    }
    hmfree(ctx->attribute_data_map);

    for (int type_i=0; type_i < arrlen(o_info->types); type_i++) {
        IntroType * type = o_info->types[type_i];
        if (type->attr == 0 && type->parent && type->parent->attr != 0) {
            type->attr = type->parent->attr;
        }
        if (type->category == INTRO_STRUCT || type->category == INTRO_UNION) {
            for (int mi=0; mi < type->count; mi++) {
                IntroMember * member = &type->members[mi];
                if (member->attr == 0 && member->type->attr != 0) {
                    member->attr = member->type->attr;
                }
            }
        }
    }

    for (int i=0; i < arrlen(ctx->attribute_globals); i++) {
        uint32_t id = ctx->attribute_globals[i].id;
        uint32_t bitset_index = id >> 5; 
        uint32_t bit_index = id & 31;
        uint32_t attr_bit = 1 << bit_index;
        o_info->attr.spec_buffer[0].bitset[bitset_index] |= attr_bit;
    }

    o_info->attr.count_available = arrlen(o_info->attr.available);
    o_info->attr.builtin = ctx->builtin;
}
