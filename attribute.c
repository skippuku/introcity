static struct attribute_map_s {
    char * key;
    int32_t type;
    int32_t value_type;
} * attribute_map = NULL;

void
create_initial_attributes() {
    const struct attribute_map_s initial [] = {
        {"id",      INTRO_ATTR_ID,      INTRO_V_INT}, // NOTE: maybe this should be part of IntroMember since it is common?
        {"default", INTRO_ATTR_DEFAULT, INTRO_V_VALUE},
        {"length",  INTRO_ATTR_LENGTH,  INTRO_V_MEMBER},
        {"switch",  INTRO_ATTR_SWITCH,  INTRO_V_CONDITION},
        {"type",    INTRO_ATTR_TYPE,    INTRO_V_NONE},
    };
    // NOTE: might need to do this later:
    //sh_new_arena(attribute_map);
    for (int i=0; i < LENGTH(initial); i++) {
        shputs(attribute_map, initial[i]);
    }
}

int
register_attribute_type(int type, int value_type, char * identifier) {
    int map_index = shgeti(attribute_map, identifier);
    if (map_index >= 0) return 1;

    for (int i=0; i < shlen(attribute_map); i++) {
        if (attribute_map[i].type == type) return 2;
    }

    struct attribute_map_s entry;
    entry.key = identifier;
    entry.type = type;
    entry.value_type = value_type;
    shputs(attribute_map, entry);

    return 0;
}

int
parse_attribute(char * buffer, char ** o_s, IntroStruct * i_struct, int member_index, IntroAttributeData * o_result) {
    IntroAttributeData data = {0};
    Token tk = next_token(o_s);
    if (tk.type == TK_IDENTIFIER) {
        char * terminated_name = copy_and_terminate(tk.start, tk.length);
        int map_index = shgeti(attribute_map, terminated_name);
        free(terminated_name);
        if (map_index < 0) {
            parse_error(&tk, "No such attribute.");
            return 1;
        }

        data.type = attribute_map[map_index].type;
        data.value_type = attribute_map[map_index].value_type; // NOTE: you could just lookup the attribute's value type

        switch(data.value_type) {
        case INTRO_V_NONE: {
            if (data.type == INTRO_ATTR_TYPE) {
                IntroType * type = i_struct->members[member_index].type;
                if (type->indirection_level != 0 || strcmp(type->name, "IntroType") != 0) {
                    parse_error(&tk, "Member must be of type 'IntroType' to have type attribute.");
                    return 1;
                }
            }
            data.v.i = 0;
        } break;

        case INTRO_V_INT: {
            tk = next_token(o_s);
            long result = strtol(tk.start, o_s, 0);
            if (*o_s == tk.start) {
                parse_error(&tk, "Invalid integer.");
                return 1;
            }
            data.v.i = (int32_t)result;
        } break;

        case INTRO_V_FLOAT: {
            tk = next_token(o_s);
            float result = strtof(tk.start, o_s);
            if (*o_s == tk.start) {
                parse_error(&tk, "Invalid floating point number.");
                return 1;
            }
            data.v.f = result;
        } break;

        case INTRO_V_VALUE: {
            parse_error(&tk, "Value attributes are not currently supported.");
            return 1;
        } break;

        case INTRO_V_MEMBER: {
            tk = next_token(o_s);
            if (tk.type != TK_IDENTIFIER || isdigit(tk.start[0])) {
                parse_error(&tk, "Expected member name.");
                return 1;
            }
            bool success = false;
            for (int mi=0; mi < i_struct->count_members; mi++) {
                if (tk_equal(&tk, i_struct->members[mi].name)) {
                    if (data.type == INTRO_ATTR_LENGTH) {
                        IntroType * type = i_struct->members[mi].type;
                        if (type->indirection_level > 0 || (type->category != INTRO_SIGNED && type->category != INTRO_UNSIGNED)) {
                            parse_error(&tk, "Length defining member must be an integer type.");
                            return 1;
                        }
                    }
                    data.v.member_index = mi;
                    success = true;
                    break;
                }
            }
            if (!success) {
                parse_error(&tk, "No such member.");
                return 1;
            }
        } break;

        case INTRO_V_CONDITION: {
            parse_error(&tk, "Condition attributes are not currently supported.");
            return 1;
        } break;
        }
    }

    if (o_result) *o_result = data;
    return 0;
}

int
parse_attributes(char * buffer, char * s, IntroStruct * i_struct, int member_index, IntroAttributeData ** o_result, uint32_t * o_count_attributes) {
    IntroAttributeData * attributes = NULL;

    Token tk = next_token(&s);
    if (!(tk.type == TK_PARENTHESIS && tk.is_open)) {
        parse_error(&tk, "Expected '('.");
        return 1;
    }
    while (1) {
        tk = next_token(&s);
        IntroAttributeData attr = {0};
        if (tk.type == TK_IDENTIFIER) {
            IntroAttributeData data;
            if (isdigit(tk.start[0])) {
                data.type = INTRO_ATTR_ID;
                data.value_type = INTRO_V_INT;
                // copied from above
                long result = strtol(tk.start, &s, 0);
                if (s == tk.start) {
                    parse_error(&tk, "Invalid integer.");
                    return 1;
                }
                data.v.i = (int32_t)result;
            } else {
                s = tk.start;
                int error = parse_attribute(buffer, &s, i_struct, member_index, &data);
                if (error) return 1;
            }
            arrput(attributes, data);
        } else if (tk.type == TK_PARENTHESIS && !tk.is_open) {
            break;
        } else {
            parse_error(&tk, "Invalid symbol.");
            return 1;
        }
        if (attr.type != 0) arrput(attributes, attr);

        tk = next_token(&s);
        if (tk.type == TK_COMMA) {
        } else if (tk.type == TK_PARENTHESIS && !tk.is_open) {
            break;
        } else {
            parse_error(&tk, "Expected ',' or ')'.");
            return 1;
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
