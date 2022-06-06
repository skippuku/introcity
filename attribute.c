enum AttributeToken {
    ATTR_TK_GLOBAL = INTRO_AT_COUNT + 1,
    ATTR_TK_INHERIT,
    ATTR_TK_ATTRIBUTE,
    ATTR_TK_APPLY_TO,
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
        {"string",  INTRO_AT_STRING},
        {"type",    INTRO_AT_TYPE},
        {"expr",    INTRO_AT_EXPR},
        {"__remove",INTRO_AT_REMOVE},
        {"global",  ATTR_TK_GLOBAL},
        {"inherit", ATTR_TK_INHERIT},
        {"attribute", ATTR_TK_ATTRIBUTE},
        {"apply_to", ATTR_TK_APPLY_TO},
    };
    shdefault(ctx->attribute_token_map, ATTR_TK_INVALID);
    for (int i=0; i < LENGTH(attribute_keywords); i++) {
        shput(ctx->attribute_token_map, attribute_keywords[i].key, attribute_keywords[i].value);
    }

    static const struct { const char * key; int value; } builtin_attributes [] = {
        {"i_id",       INTRO_ATTR_ID},
        {"i_bitfield", INTRO_ATTR_BITFIELD},
        {"i_default",  INTRO_ATTR_DEFAULT},
        {"i_length",   INTRO_ATTR_LENGTH},
        {"i_alias",    INTRO_ATTR_ALIAS},
        {"i_city",     INTRO_ATTR_CITY},
        {"i_cstring",  INTRO_ATTR_CSTRING},
        {"i_type",     INTRO_ATTR_TYPE},
        {"i_remove",   INTRO_ATTR_REMOVE},
    };
    shdefault(ctx->builtin_map, -1);
    for (int i=0; i < LENGTH(builtin_attributes); i++) {
        shput(ctx->builtin_map, builtin_attributes[i].key, builtin_attributes[i].value);
    }

    sh_new_arena(ctx->attribute_map);
    ctx->attribute_id_counter = INTRO_ATTR_FIRST_UNRESERVED;
    ctx->attribute_flag_id_counter = UINT32_MAX / 2;
}

int
parse_global_directive(ParseContext * ctx, char ** o_s) {
    Token tk = next_token(o_s);
    char * opening_paren;
    if (tk.type != TK_L_PARENTHESIS) {
        parse_error(ctx, &tk, "Expected '('.");
        return -1;
    }
    opening_paren = tk.start;

    tk = next_token(o_s);
    char temp [1024];
    memcpy(temp, tk.start, tk.length);
    temp[tk.length] = 0;
    int a_tk = shget(ctx->attribute_token_map, temp);
    if (a_tk == ATTR_TK_ATTRIBUTE) {
        tk = next_token(o_s);
        char * namespace = NULL;
        if (tk.type == TK_IDENTIFIER) {
            namespace = copy_and_terminate(ctx->arena, tk.start, tk.length);
            tk = next_token(o_s);
        }
        if (tk.type != TK_L_PARENTHESIS) {
            parse_error(ctx, &tk, "Expected. '('.");
            return -1;
        }

        while (1) {
            AttributeParseInfo info = {0};

            tk = next_token(o_s);
            if (tk.type == TK_R_PARENTHESIS) {
                break;
            } else if (tk.type != TK_IDENTIFIER) {
                parse_error(ctx, &tk, "Expected identifier for attribute name.");
            }
            char namespaced [1024];
            char unspaced [1024];
            memcpy(unspaced, tk.start, tk.length);
            unspaced[tk.length] = 0;
            strcpy(namespaced, namespace);
            strcat(namespaced, unspaced);
            int builtin = shget(ctx->builtin_map, namespaced);
            if (shgeti(ctx->attribute_map, namespaced) >= 0) {
                parse_error(ctx, &tk, "Attribute name is reserved.");
                return -1;
            }
            tk = next_token(o_s);
            if (tk.type != TK_COLON) {
                parse_error(ctx, &tk, "Expected ':'.");
                return -1;
            }

            tk = next_token(o_s);
            memcpy(temp, tk.start, tk.length);
            temp[tk.length] = 0;
            int a_tk = shget(ctx->attribute_token_map, temp);
            if (a_tk >= INTRO_AT_COUNT) {
                parse_error(ctx, &tk, "Invalid attribute type.");
                return -1;
            }
            info.type = a_tk;
            if (builtin >= 0) {
                info.id = builtin;
                info.builtin = true;
            } else if (info.type == INTRO_AT_FLAG) {
                info.id = ctx->attribute_flag_id_counter++;
            } else {
                info.id = ctx->attribute_id_counter++;
            }

            tk = next_token(o_s);
            if (tk.type == TK_L_PARENTHESIS && a_tk == INTRO_AT_VALUE) {
                tk = next_token(o_s);
                if (tk.type == TK_AT) {
                    tk = next_token(o_s);
                    if (!tk_equal(&tk, "inherit")) {
                        parse_error(ctx, &tk, "Invalid trait.");
                        return -1;
                    }
                    tk = next_token(o_s);
                    info.type_ptr = NULL;
                } else {
                    *o_s = tk.start;
                    DeclState decl = {.state = DECL_CAST};
                    int ret = parse_declaration(ctx, o_s, &decl);
                    if (ret == RET_DECL_FINISHED || ret == RET_DECL_CONTINUE) {
                        info.type_ptr = decl.type;
                    } else if (ret == RET_NOT_TYPE) {
                        parse_error(ctx, &decl.base_tk, "Not a type.");
                        return -1;
                    } else {
                        return -1;
                    }
                    tk = next_token(o_s);
                }
                if (tk.type != TK_R_PARENTHESIS) {
                    parse_error(ctx, &tk, "Expected ')'.");
                    return -1;
                }
                tk = next_token(o_s);
            }

            if (tk.type == TK_AT) {
                tk = next_token(o_s);
                if (tk_equal(&tk, "global")) {
                    if (info.type != INTRO_AT_FLAG) {
                        parse_error(ctx, &tk, "Only flag attributes can have the trait global.");
                        return -1;
                    }
                    info.global = true;
                } else {
                    parse_error(ctx, &tk, "Invalid trait.");
                    return -1;
                }
                tk = next_token(o_s);
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
                parse_error(ctx, &tk, "Expected ',' or ')'.");
                return -1;
            }
        }
    } else if (a_tk == ATTR_TK_APPLY_TO) {
        parse_warning(ctx, &tk, "'apply_to' is not yet implemented. Ignoring.");
        *o_s = find_closing(opening_paren);
    } else {
        parse_error(ctx, &tk, "Invalid. Expected 'attribute' or 'apply_to'.");
        return -1;
    }

    tk = next_token(o_s);
    if (tk.type != TK_R_PARENTHESIS) {
        parse_error(ctx, &tk, "Missing ')'.");
        return -1;
    }
    return 0;
}

static bool
check_id_valid(const IntroStruct * i_struct, int id) {
#if 0
    if (id < 0 || ((id & 0xFFFF) != id)) {
        return false;
    }
    for (int member_index = 0; member_index < i_struct->count_members; member_index++) {
        const IntroMember * member = &i_struct->members[member_index];
        for (int attr_index = 0; attr_index < member->count_attributes; attr_index++) {
            const AttributeData * attr = &member->attributes[attr_index];
            if (attr->type == INTRO_ATTR_ID && attr->v.i == id) {
                return false;
            }
        }
    }
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

ptrdiff_t parse_array_value(ParseContext * ctx, const IntroType * type, char ** o_s, uint32_t * o_count);

// TODO: change to parse_expression
ptrdiff_t
parse_value(ParseContext * ctx, IntroType * type, char ** o_s, uint32_t * o_count) {
    if ((type->category >= INTRO_U8 && type->category <= INTRO_S64) || type->category == INTRO_ENUM) {
        intmax_t result = parse_constant_expression(ctx, o_s);
        int size = (type->category != INTRO_ENUM)? type->category & 0x0f : sizeof(int);
        return store_value(ctx, &result, size);
    } else if (type->category == INTRO_F32) {
        float result = strtof(*o_s, o_s);
        return store_value(ctx, &result, 4);
    } else if (type->category == INTRO_F64) {
        double result = strtod(*o_s, o_s);
        return store_value(ctx, &result, 8);
    } else if (type->category == INTRO_POINTER) {
        Token tk = next_token(o_s);
        if (tk.type == TK_STRING) {
            if (type->parent->category == INTRO_S8 && 0==strcmp(type->parent->name, "char")) {
                size_t length;
                char * str = parse_escaped_string(&tk, &length);
                if (!str) {
                    parse_error(ctx, &tk, "Invalid string.");
                    return -1;
                }
                ptrdiff_t result = store_ptr(ctx, str, length);
                return result;
            }
        } else {
            *o_s = tk.start;
            ptrdiff_t array_value_offset = parse_array_value(ctx, type, o_s, o_count);
            ptrdiff_t pointer_offset = store_value(ctx, &array_value_offset, sizeof(size_t));
            return pointer_offset;
        }
    } else if (type->category == INTRO_ARRAY) {
        ptrdiff_t result = parse_array_value(ctx, type, o_s, NULL);
        return result;
    }
    return -1;
}

ptrdiff_t
parse_array_value(ParseContext * ctx, const IntroType * type, char ** o_s, uint32_t * o_count) {
    Token tk = next_token(o_s);

    ptrdiff_t result = arrlen(ctx->value_buffer);
    size_t array_element_size = intro_size(type->parent);
    uint32_t count = 0;

    if (tk.type == TK_L_BRACE) {
        while (1) {
            tk = next_token(o_s);
            if (tk.type == TK_COMMA) {
                parse_error(ctx, &tk, "Invalid symbol.");
                return -1;
            } else if (tk.type == TK_R_BRACE) {
                break;
            }
            *o_s = tk.start;
            parse_value(ctx, type->parent, o_s, NULL);
            count++;

            tk = next_token(o_s);
            if (tk.type == TK_COMMA) {
            } else if (tk.type == TK_R_BRACE) {
                break;
            } else {
                parse_error(ctx, &tk, "Invalid symbol.");
                return -1;
            }
        }
        if (type->category == INTRO_ARRAY) {
            int left = array_element_size * (type->array_size - count);
            if (count < type->array_size) {
                memset(arraddnptr(ctx->value_buffer, left), 0, left);
            }
        }
        if (o_count) *o_count = count;
    } else {
        parse_error(ctx, &tk, "Expected '{'.");
        return -1;
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
handle_value_attribute(ParseContext * ctx, char ** o_s, IntroType * type, int member_index, AttributeData * data, Token * p_tk) {
    IntroType * m_type = type->i_struct->members[member_index].type;
    assert(arrlen(ctx->ptr_stores) == 0);
    uint32_t length_value = 0;
    ptrdiff_t value_offset = parse_value(ctx, m_type, o_s, &length_value);
    if (value_offset < 0) {
        parse_error(ctx, p_tk, "Error parsing value attribute.");
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

int
parse_attribute(ParseContext * ctx, char ** o_s, IntroType * type, int member_index, AttributeData * o_result) {
    IntroStruct * i_struct = type->i_struct;
    AttributeData data = {0};
    Token tk = next_token(o_s);
    if (tk.type == TK_IDENTIFIER) {
        STACK_TERMINATE(terminated_name, tk.start, tk.length);
        int map_index = shgeti(ctx->attribute_map, terminated_name);
        if (map_index < 0) {
            parse_error(ctx, &tk, "No such attribute.");
            return -1;
        }

        AttributeParseInfo attr_info = ctx->attribute_map[map_index].value;
        if (attr_info.invalid_without_namespace) {
            parse_error(ctx, &tk, "Name matches more than one namespace.");
            return -1;
        }
        data.id = attr_info.final_id;
        IntroAttributeType attribute_type = attr_info.type;

        switch(attribute_type) {
        default: break;
        case INTRO_AT_FLAG: {
            if (data.id == INTRO_ATTR_TYPE) {
                IntroType * type = i_struct->members[member_index].type;
                if (!(type->category == INTRO_POINTER && strcmp(type->parent->name, "IntroType") == 0)) {
                    parse_error(ctx, &tk, "Member must be of type 'IntroType *' to have type attribute.");
                    char typename [1024];
                    intro_sprint_type_name(typename, type);
                    fprintf(stderr, "member type is %s\n", typename);
                    return -1;
                }
            }
            data.v.i = 0;
        } break;

        case INTRO_AT_INT: {
            tk = next_token(o_s);
            long result = strtol(tk.start, o_s, 0);
            if (*o_s == tk.start) {
                parse_error(ctx, &tk, "Invalid integer.");
                return -1;
            }
            data.v.i = (int32_t)result;
            if (data.id == INTRO_ATTR_ID) {
                if (!check_id_valid(i_struct, data.v.i)) {
                    parse_error(ctx, &tk, "This ID is reserved.");
                    return -1;
                }
            }
        } break;

        case INTRO_AT_FLOAT: {
            tk = next_token(o_s);
            float result = strtof(tk.start, o_s);
            if (*o_s == tk.start) {
                parse_error(ctx, &tk, "Invalid floating point number.");
                return -1;
            }
            data.v.f = result;
        } break;

        case INTRO_AT_STRING: {
            tk = next_token(o_s);
            char * result = NULL;
            if (data.id == INTRO_ATTR_ALIAS && tk.type == TK_IDENTIFIER) {
                result = copy_and_terminate(ctx->arena, tk.start, tk.length);
            } else {
                if (tk.type != TK_STRING) {
                    parse_error(ctx, &tk, "Expected string.");
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
            if (handle_value_attribute(ctx, o_s, type, member_index, &data, &tk)) return -1;
        } break;

        case INTRO_AT_MEMBER: {
            tk = next_token(o_s);
            if (tk.type != TK_IDENTIFIER || is_digit(tk.start[0])) {
                parse_error(ctx, &tk, "Expected member name.");
                return -1;
            }
            bool success = false;
            for (int mi=0; mi < i_struct->count_members; mi++) {
                if (tk_equal(&tk, i_struct->members[mi].name)) {
                    if (data.id == INTRO_ATTR_LENGTH) {
                        IntroType * type = i_struct->members[mi].type;
                        uint32_t category_no_size = type->category & 0xf0;
                        if (category_no_size != INTRO_SIGNED && category_no_size != INTRO_UNSIGNED) {
                            parse_error(ctx, &tk, "Length defining member must be of an integer type.");
                            return -1;
                        }
                    }
                    data.v.i = mi;
                    success = true;
                    break;
                }
            }
            if (!success) {
                parse_error(ctx, &tk, "No such member.");
                return -1;
            }
        } break;
        }
    }

    if (o_result) *o_result = data;
    return 0;
}

int
parse_attributes(ParseContext * ctx, char * s, IntroType * type, int member_index, AttributeData ** o_result, uint32_t * o_count_attributes) {
    IntroStruct * i_struct = type->i_struct;
    AttributeData * attributes = NULL;

    Token tk = next_token(&s);
    if (tk.type != TK_L_PARENTHESIS) {
        parse_error(ctx, &tk, "Expected '('.");
        return -1;
    }
    while (1) {
        tk = next_token(&s);
        AttributeData data;
        if (tk.type == TK_IDENTIFIER) {
            if (is_digit(tk.start[0])) {
                data.id = INTRO_ATTR_ID;
                // @copy from above
                long result = strtol(tk.start, &s, 0);
                if (s == tk.start) {
                    parse_error(ctx, &tk, "Invalid integer.");
                    return -1;
                }
                data.v.i = (int32_t)result;
                if (!check_id_valid(i_struct, data.v.i)) {
                    parse_error(ctx, &tk, "This ID is reserved.");
                    return -1;
                }
            } else {
                s = tk.start;
                int error = parse_attribute(ctx, &s, type, member_index, &data);
                if (error) return -1;
            }
            arrput(attributes, data);
        } else if (tk.type == TK_EQUAL) {
            data.id = INTRO_ATTR_DEFAULT;
            if (handle_value_attribute(ctx, &s, type, member_index, &data, &tk)) return -1;

            arrput(attributes, data);
        } else if (tk.type == TK_R_PARENTHESIS) {
            break;
        } else {
            parse_error(ctx, &tk, "Invalid symbol.");
            return -1;
        }

        tk = next_token(&s);
        if (tk.type == TK_COMMA) {
        } else if (tk.type == TK_R_PARENTHESIS) {
            break;
        } else {
            parse_error(ctx, &tk, "Expected ',' or ')'.");
            return -1;
        }
    }

    if (o_result && o_count_attributes) {
        *o_count_attributes = arrlenu(attributes);
        *o_result = attributes;
    } else {
        arrfree(attributes);
    }
    return 0;
}

static void
add_attribute(IntroInfo * o_info, AttributeParseInfo * info, AttributeParseInfo * next_info, char * name) {
    IntroAttribute attribute = {
        .name = name,
        .attr_type = info->type,
        .type_id = hmget(o_info->index_by_ptr_map, info->type_ptr),
    };
    int final_id = arrlen(o_info->attr.available);
    if (info->builtin) {
        final_id = info->id;
        if (info->type != INTRO_AT_FLAG && info->type != INTRO_AT_REMOVE) {
            o_info->attr.available[info->id] = attribute;
        }
    } else {
        arrput(o_info->attr.available, attribute);
    }
    info->final_id = final_id;
    if (next_info) next_info->final_id = final_id;
}

int
attribute_data_sort_callback(const void * p_a, const void * p_b) {
    const AttributeData a = *(AttributeData *)p_a;
    const AttributeData b = *(AttributeData *)p_b;
    return a.id - b.id;
}

void
add_attributes_to_member(ParseContext * ctx, IntroType * type, int32_t member_index, AttributeData * data, int32_t count) {
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
        if (data[i].id == INTRO_ATTR_REMOVE) {
            for (int j=0; j < arrlen(pcontent->value); j++) {
                if (pcontent->value[j].id == data[i].v.i) {
                    arrdelswap(pcontent->value, j);
                }
            }
        } else {
            arrput(pcontent->value, data[i]);
        }
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
            if (pcontent->value[a].id == INTRO_ATTR_LENGTH) {
                length_member_index = pcontent->value[a].v.i;
                length_member = &def.type->i_struct->members[length_member_index];
                break;
            }
        }
        if (!length_member) continue;

        ptrdiff_t value_offset = store_value(ctx, &def.value, intro_size(length_member->type));
        AttributeData data = {
            .id = def.attr_id,
            .v.i = value_offset,
        };

        add_attributes_to_member(ctx, def.type, length_member_index, &data, 1);
    }
    arrsetlen(ctx->deferred_length_defaults, 0);
}

static void
handle_attributes(ParseContext * ctx, IntroInfo * o_info) {
    int * flags = NULL;
    arrsetcap(flags, 16);
    arrsetcap(o_info->attr.available, 32);
    arrsetlen(o_info->attr.available, INTRO_ATTR_FIRST_UNRESERVED - 1);

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
        add_attribute(o_info, info, next_info, ctx->attribute_map[index].key);
    }

    o_info->attr.first_flag = arrlen(o_info->attr.available);
    for (int i=0; i < arrlen(flags); i++) {
        int index = flags[i];
        char * name = ctx->attribute_map[index].key;
        AttributeParseInfo * info = &ctx->attribute_map[index].value;
        AttributeParseInfo * next_info = (info->next_is_same)? &ctx->attribute_map[index+1].value : NULL;
        add_attribute(o_info, info, next_info, name);
    }
    arrfree(flags);

    IntroAttributeSpec * empty = arraddnptr(o_info->attr.spec_buffer, 1);
    memset(empty, 0, sizeof(*empty));

    for (int directive_i=0; directive_i < arrlen(ctx->attribute_directives); directive_i++) {
        AttributeDirective directive = ctx->attribute_directives[directive_i];
        // TODO: just pass &directive
        int ret = parse_attributes(ctx, directive.location, directive.type, directive.member_index, &directive.attr_data, &directive.count);
        if (ret) exit(1);

        add_attributes_to_member(ctx, directive.type, directive.member_index, directive.attr_data, directive.count);
    }

    handle_deferred_defaults(ctx);

    for (int i=0; i < hmlen(ctx->attribute_data_map); i++) {
        AttributeDataMap content = ctx->attribute_data_map[i];
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
                memcpy(&spec->value_offsets[i], &attr_data[i].v, sizeof(uint32_t));
            }
        }

        switch(member_index) {
        case MIDX_TYPE: {
            type->attr = spec_index;
        }break;

        default: {
            type->i_struct->members[member_index].attr = spec_index;
        }break;
        }
    }

    o_info->attr.count_available = arrlen(o_info->attr.available);
}
