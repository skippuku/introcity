enum AttributeToken {
    ATTR_TK_GLOBAL = INTRO_AT_COUNT + 1,
    ATTR_TK_INHERIT,
    ATTR_TK_ATTRIBUTE,
    ATTR_TK_APPLY_TO,
    ATTR_TK_PROPAGATE,
    ATTR_TK_INVALID
};

void
attribute_parse_init(ParseContext * ctx) {
    static const struct { const char * key; int value; } attribute_keywords [] = {
        {"flag",    INTRO_AT_FLAG},
        {"int",     INTRO_AT_INT},
        {"float",   INTRO_AT_FLOAT},
        {"value",   INTRO_AT_VALUE},
        {"member",  INTRO_AT_MEMBER},
        {"type",    INTRO_AT_TYPE},
        {"expr",    INTRO_AT_EXPR},
        {"__remove",INTRO_AT_REMOVE},
        {"attribute", ATTR_TK_ATTRIBUTE},
        {"apply_to", ATTR_TK_APPLY_TO},
        {"global",  ATTR_TK_GLOBAL},
        {"inherit", ATTR_TK_INHERIT},
        {"propagate", ATTR_TK_PROPAGATE},
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
    ctx->flag_temp_id_counter = 0;
}

int
parse_global_directive(ParseContext * ctx, TokenIndex * tidx) {
    Token tk;
    EXPECT('(');

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
            shputs(ctx->attribute_namespace_set, (NameSet){namespace});
            tk = next_token(tidx);
        } else if (tk.type == TK_AT) {
            tk = next_token(tidx);
            memcpy(temp, tk.start, tk.length);
            temp[tk.length] = 0;
            a_tk = shget(ctx->attribute_token_map, temp);
            if (a_tk != ATTR_TK_GLOBAL) {
                parse_error(ctx, tk, "The only attribute trait allowed here is 'global'.");
                return -1;
            }
            namespace = NULL;
            tk = next_token(tidx);
        } else {
            parse_error(ctx, tk, "Expected namespace or @global before '('.");
            return -1;
        }

        if (tk.type != TK_L_PARENTHESIS) {
            parse_error(ctx, tk, "Expected '('.");
            return -1;
        }

        while (1) {
            AttributeParseInfo info = {0};

            tk = next_token(tidx);
            if (tk.type == TK_R_PARENTHESIS) {
                break;
            } else if (tk.type != TK_IDENTIFIER) {
                parse_error(ctx, tk, "Expected an identifier for an attribute name.");
            }
            char namespaced [1024];
            stbsp_snprintf(namespaced, sizeof namespaced, "%s%.*s", namespace ?: "", tk.length, tk.start);
            if (shgeti(ctx->attribute_map, namespaced) >= 0) {
                parse_error(ctx, tk, "This attribute name is reserved.");
                return -1;
            }
            EXPECT(':');

            tk = next_token(tidx);
            memcpy(temp, tk.start, tk.length);
            temp[tk.length] = 0;
            int a_tk = shget(ctx->attribute_token_map, temp);
            if (a_tk >= INTRO_AT_COUNT) {
                parse_error(ctx, tk, "Invalid attribute type.");
                return -1;
            }
            info.type = a_tk;

            if (info.type == INTRO_AT_FLAG) {
                info.id = ctx->flag_temp_id_counter++;
            } else {
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
                        parse_error(ctx, decl.base_tk, "Type must be defined before attribute definition.");
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

            while (tk.type == TK_AT) {
                tk = next_token(tidx);
                memcpy(temp, tk.start, tk.length);
                temp[tk.length] = 0;
                a_tk = shget(ctx->attribute_token_map, temp);
                if (a_tk == ATTR_TK_GLOBAL) {
                    if (info.type != INTRO_AT_FLAG) {
                        parse_error(ctx, tk, "Only flag attributes can have the trait global.");
                        return -1;
                    }
                    info.global = true;
                    info.propagate = true;
                } else if (a_tk == ATTR_TK_PROPAGATE) {
                    info.propagate = true;
                } else {
                    parse_error(ctx, tk, "Invalid trait.");
                    return -1;
                }
                tk = next_token(tidx);
            }

            shput(ctx->attribute_map, namespaced, info);

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

ptrdiff_t
store_value(ParseContext * ctx, const void * value, size_t value_size) {
    void * storage = arraddnptr(ctx->value_buffer, value_size);
    memcpy(storage, value, value_size); // NOTE: this is only correct for LE
    return (storage - (void *)ctx->value_buffer);
}

ptrdiff_t
store_ptr(ParseContext * ctx, void * data, size_t size) {
    static const uint8_t nothing [sizeof(void *)] = {0};
    ptrdiff_t offset = store_value(ctx, &nothing, sizeof(void *));
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
                char * dest = (char *)arraddnptr(ctx->value_buffer, type->size);
                memset(dest, 0, type->size);
                strcpy(dest, str);
                free(str);

                return result;
            }
            tidx->index = tk_index;
        }
        ptrdiff_t result = parse_array_value(ctx, type, tidx, NULL);
        return result;
    } else if (intro_has_members(type)) {
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

bool
parse_attribute_name(ParseContext * ctx, TokenIndex * tidx, AttributeParseInfo * o_info) {
    ptrdiff_t map_index = -1;
    char name [1024];
    Token tk;

    EXPECT_IDEN();

    if (ctx->current_namespace) {
        stbsp_snprintf(name, sizeof name, "%s%.*s", ctx->current_namespace, tk.length, tk.start);
        map_index = shgeti(ctx->attribute_map, name);
    }

    if (map_index < 0) {
        memcpy(name, tk.start, tk.length);
        name[tk.length] = 0;
        map_index = shgeti(ctx->attribute_map, name);
    }

    if (map_index < 0) {
        parse_error(ctx, tk, "No such attribute.");
        return false;
    }

    *o_info = ctx->attribute_map[map_index].value;
    return true;
}

int
parse_attribute(ParseContext * ctx, TokenIndex * tidx, IntroType * type, int member_index, AttributeData * o_result) {
    AttributeData data = {0};
    AttributeParseInfo attr_info;
    Token tk;

    if (!parse_attribute_name(ctx, tidx, &attr_info)) {
        return -1;
    }
    data.id = attr_info.final_id;
    IntroAttributeType attribute_type = attr_info.type;

    char * end;
    switch(attribute_type) {
    case INTRO_AT_FLAG: {
        if (data.id == ctx->builtin.type) {
            IntroType * mtype = type->members[member_index].type;
            if (!(mtype->category == INTRO_POINTER && strcmp(mtype->of->name, "IntroType") == 0)) {
                parse_error(ctx, tk_last(tidx), "Member must be of type 'IntroType *' to have type attribute.");
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
        advance_to(tidx, end);
        data.v.i = (int32_t)result;
    } break;

    case INTRO_AT_FLOAT: {
        tk = next_token(tidx);
        float result = strtof(tk.start, &end);
        advance_to(tidx, end);
        if (end == tk.start) {
            parse_error(ctx, tk, "Invalid floating point number.");
            return -1;
        }
        advance_to(tidx, end);
        data.v.f = result;
    } break;

    // TODO: error check: member used as length can't have a value if the member it is the length of has a value
    // TODO: error check: if multiple members use the same member for length, values can't have different lengths
    // NOTE: the previous 2 todo's do not apply if the values are for different attributes
    case INTRO_AT_VALUE: {
        if (data.id == ctx->builtin.alias && tk_at(tidx).type == TK_IDENTIFIER) {
            tk = next_token(tidx);
            char * name = malloc(tk.length + 1);
            memcpy(name, tk.start, tk.length);
            name[tk.length] = 0;
            ptrdiff_t result = store_ptr(ctx, name, tk.length + 1);
            store_deferred_ptrs(ctx);
            data.v.i = result;
        } else {
            if (handle_value_attribute(ctx, tidx, type, member_index, &data, &tk)) return -1;
        }
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
                if (data.id == ctx->builtin.length) {
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

    case INTRO_AT_EXPR: {
        ExprNode * tree = build_expression_tree2(ctx->expr_ctx, tidx);
        if (!tree) {
            return -1; 
        }

        IntroContainer base_cont = {.type = type};
        IntroContainer * last_cont = &base_cont;
        ContainerMapValue v;
        while ((v = hmget(ctx->container_map, type)).type != NULL) {
            IntroContainer * next = arena_alloc(ctx->expr_ctx->arena, sizeof(*next));
            next->type = v.type;
            last_cont->index = v.index;
            last_cont->parent = next;
            last_cont = next;
            type = v.type;
        }

        uint8_t * bytecode = build_expression_procedure2(ctx->expr_ctx, tree, &base_cont);

        size_t value_buf_offset = arraddnindex(ctx->value_buffer, arrlen(bytecode));
        memcpy(ctx->value_buffer + value_buf_offset, bytecode, arrlen(bytecode));

        arrfree(bytecode);
        reset_arena(ctx->expr_ctx->arena);

        data.v.i = value_buf_offset;
    }break;

    case INTRO_AT_REMOVE: {
        AttributeParseInfo rm_attr_info;
        if (!parse_attribute_name(ctx, tidx, &rm_attr_info)) return -1;
        data.v.i = (int32_t)rm_attr_info.final_id;
    }break;

    case INTRO_AT_TYPE: {
        parse_error(ctx, tk_last(tidx), "Not implemented.");
    }break;

    case INTRO_AT_COUNT: assert(0);
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

    ctx->current_namespace = NULL;

    while (1) {
        tk = next_token(tidx);
        AttributeData data;
        if (tk.type == TK_IDENTIFIER) {
            if (tk_at(tidx).type == TK_COLON) {
                char temp_namespace [1024];
                memcpy(temp_namespace, tk.start, tk.length);
                temp_namespace[tk.length] = 0;
                ctx->current_namespace = shgets(ctx->attribute_namespace_set, temp_namespace).key;
                if (!ctx->current_namespace) {
                    strcat(temp_namespace, "_");
                    ctx->current_namespace = shgets(ctx->attribute_namespace_set, temp_namespace).key;
                    if (!ctx->current_namespace) {
                        parse_error(ctx, tk, "Namespace does not exist.");
                        return -1;
                    }
                }
                tidx->index += 1;
                continue;
            } else {
                tidx->index--;
                int error = parse_attribute(ctx, tidx, directive->type, directive->member_index, &data);
                if (error) return -1;
                arrput(attributes, data);
            }
        } else if (tk.type == TK_AT) {
            if (tk_equal(&tidx->list[tidx->index], "global") && tidx->list[tidx->index + 1].type == TK_COLON) {
                ctx->current_namespace = NULL;
                tidx->index += 2;
                continue;
            } else {
                goto invalid_symbol;
            }
        } else if (tk.type == TK_NUMBER) {
            data.id = ctx->builtin.id;
            // @copy from above
            char * end;
            long result = strtol(tk.start, &end, 0);
            advance_to(tidx, end);
            if (end == tk.start) {
                parse_error(ctx, tk, "Invalid integer.");
                return -1;
            }
            data.v.i = (int32_t)result;
            arrput(attributes, data);
        } else if (tk.type == TK_EQUAL) {
            data.id = ctx->builtin.fallback;
            if (handle_value_attribute(ctx, tidx, directive->type, directive->member_index, &data, &tk)) return -1;

            arrput(attributes, data);
        } else if (tk.type == TK_TILDE) {
            data.id = ctx->builtin.remove;

            AttributeParseInfo rm_attr_info;
            if (!parse_attribute_name(ctx, tidx, &rm_attr_info)) return -1;
            data.v.i = (int32_t)rm_attr_info.final_id;

            arrput(attributes, data);
        } else if (tk.type == TK_R_PARENTHESIS) {
            break;
        } else {
          invalid_symbol: ;
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
add_attribute(ParseContext * ctx, ParseInfo * o_info, AttributeParseInfo * info, char * name) {
    IntroAttribute attribute = {
        .name = name,
        .attr_type = info->type,
        .type_id = hmget(o_info->index_by_ptr_map, info->type_ptr),
        .propagated = info->propagate,
    };
    int final_id = arrlen(o_info->attr.available);
    arrput(o_info->attr.available, attribute);
    info->final_id = final_id;
    int builtin_offset = shget(ctx->builtin_map, name);
    if (builtin_offset >= 0) {
        *((uint8_t *)&ctx->builtin + builtin_offset) = (uint8_t)final_id;
    }
    if (info->global) {
        AttributeData data = {
            .id = final_id,
        };
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
        bool propagated = ctx->p_info->attr.available[data[i].id].propagated;
        if (member_index == MIDX_TYPE_PROPAGATED && !propagated) {
            continue;
        }
        bool do_remove = data[i].id == ctx->builtin.remove;
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
    // TODO: not sure how to handle this now that length is an expression...
#if 0
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
            if (pcontent->value[a].id == ctx->builtin.length) {
                length_member_index = pcontent->value[a].v.i;
                length_member = &def.type->members[length_member_index];
                break;
            }
        }
        if (!length_member) continue;

        ptrdiff_t value_offset = store_value(ctx, &def.value, length_member->type->size);
        AttributeData data = {
            .id = def.attr_id,
            .v.i = value_offset,
        };

        apply_attributes_to_member(ctx, def.type, length_member_index, &data, 1);
    }
    arrsetlen(ctx->deferred_length_defaults, 0);
#endif
}

static void
handle_attributes(ParseContext * ctx, ParseInfo * o_info) {
    int * flags = NULL;
    arrsetcap(flags, 16);
    arrsetcap(o_info->attr.available, 32);
    arrsetcap(ctx->value_buffer, (1 << 16));

    for (int i=0; i < shlen(ctx->attribute_map); i++) {
        AttributeParseInfo * info = &ctx->attribute_map[i].value;
        if (info->type == INTRO_AT_FLAG) {
            arrput(flags, i);
            continue;
        }
        add_attribute(ctx, o_info, info, ctx->attribute_map[i].key);
    }

    o_info->attr.first_flag = arrlen(o_info->attr.available);
    for (int i=0; i < arrlen(flags); i++) {
        int index = flags[i];
        char * name = ctx->attribute_map[index].key;
        AttributeParseInfo * info = &ctx->attribute_map[index].value;
        add_attribute(ctx, o_info, info, name);
    }
    arrfree(flags);

    IntroAttributeSpec * empty = arraddnptr(o_info->attr.spec_buffer, 1);
    memset(empty, 0, sizeof(*empty));

    for (int directive_i=0; directive_i < arrlen(ctx->attribute_directives); directive_i++) {
        AttributeDirective directive = ctx->attribute_directives[directive_i];
        if (!directive.attr_data) {
            int ret = parse_attributes(ctx, &directive);
            if (ret) exit(1);
        } else {
            for (int attr_i=0; attr_i < directive.count; attr_i++) {
                uint32_t old_id = directive.attr_data[attr_i].id;
                directive.attr_data[attr_i].id = shget(ctx->attribute_map, g_builtin_attributes[old_id].key).final_id;
            }
        }

        // add propagated type attributes
        bool found_inheritance = false;
        AttributeDataKey key = {0};
        key.member_index = MIDX_TYPE_PROPAGATED;
        if (directive.member_index >= 0) {
            if (intro_has_members(directive.type)) {
                IntroMember member = directive.type->members[directive.member_index];
                key.type = member.type;
            } else if (directive.type->category == INTRO_ENUM) {
                key.type = directive.type; // NOTE: maybe this should be set to null instead
            } else {
                assert(0 /* Vibe check failed. */);
            }
        } else {
            if (directive.type->parent) {
                key.type = directive.type->parent;
            }
        }
        if (key.type != NULL) {
            AttributeData * type_attr_data = NULL;
            while (1) {
                type_attr_data = hmget(ctx->attribute_data_map, key);
                if (!type_attr_data && key.type->parent) {
                    key.type = key.type->parent;
                } else {
                    break;
                }
            }
            if (type_attr_data && arrlen(type_attr_data) > 0) {
                apply_attributes_to_member(ctx, directive.type, directive.member_index, type_attr_data, arrlen(type_attr_data));
                found_inheritance = true;
            }
        }

        // add global flags if nothing could be inherited
        if (!found_inheritance) {
            apply_attributes_to_member(ctx, directive.type, directive.member_index, ctx->attribute_globals, arrlen(ctx->attribute_globals));
        }

        // add attributes from directive
        apply_attributes_to_member(ctx, directive.type, directive.member_index, directive.attr_data, directive.count);

        // add to heritable (propagated) attributes if this is for a type
        if (directive.member_index == MIDX_TYPE) {
            AttributeDataKey key = {.type = directive.type, .member_index = directive.member_index};
            AttributeData * all_data = hmget(ctx->attribute_data_map, key);

            if (arrlen(all_data) > 0) {
                // function ignores non-propagated attributes
                apply_attributes_to_member(ctx, directive.type, MIDX_TYPE_PROPAGATED, all_data, arrlen(all_data));
            }
        }
    }

    handle_deferred_defaults(ctx);

    struct {IntroType * key; uint32_t value;} * propagated_map = NULL;

    HashTable * attr_spec_set = new_table(1024);

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
        size_t prev_len = arrlen(o_info->attr.spec_buffer);
        IntroAttributeSpec * spec = arraddnptr(o_info->attr.spec_buffer, count_16_byte_sections_needed);
        size_t data_size = count_16_byte_sections_needed * sizeof(*spec);
        memset(spec, 0, data_size);

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

        HashEntry entry = {
            .key_data = spec,
            .key_size = data_size,
        };
        
        uint32_t lookup_index = table_get(attr_spec_set, &entry);
        if (lookup_index != TABLE_INVALID_INDEX) {
            spec_index = entry.value;
            arrsetlen(o_info->attr.spec_buffer, prev_len);
        } else {
            entry.value = spec_index;
            table_set(attr_spec_set, entry);
        }

        switch(member_index) {
        case MIDX_TYPE: {
            type->attr = spec_index;
        }break;

        case MIDX_TYPE_PROPAGATED: {
            hmput(propagated_map, type, spec_index);
        }break;

        default: {
            if (intro_has_members(type)) {
                type->members[member_index].attr = spec_index;
            } else if (type->category == INTRO_ENUM) {
                type->values[member_index].attr = spec_index;
            } else {
                assert(0 /* Vibe check failed */);
            }
        }break;
        }

        arrfree(attr_data);
    }
    hmfree(ctx->attribute_data_map);
    free_table(attr_spec_set);

    // set inhertited attribute data for declarations without attribute directives
    for (int type_i=0; type_i < arrlen(o_info->types); type_i++) {
        IntroType * type = o_info->types[type_i];
        if (type->attr == 0 && type->parent && type->parent->attr != 0) {
            type->attr = hmget(propagated_map, type->parent);
            hmput(propagated_map, type, type->attr);
        }
        if (intro_has_members(type)) {
            for (int mi=0; mi < type->count; mi++) {
                IntroMember * member = &type->members[mi];
                if (member->attr == 0 && member->type->attr != 0) {
                    member->attr = hmget(propagated_map, member->type);
                }
            }
        }
    }
    hmfree(propagated_map);

    // apply global flags to default attribute spec (0)
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
