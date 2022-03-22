static struct attribute_map_s {
    char * key;
    int32_t type;
    int32_t value_type;
} * attribute_map = NULL;

static char ** note_set = NULL;

void
create_initial_attributes() {
    const struct attribute_map_s initial [] = {
        {"id",      INTRO_ATTR_ID,      INTRO_V_INT}, // NOTE: maybe this should be part of IntroMember since it is common?
        {"default", INTRO_ATTR_DEFAULT, INTRO_V_VALUE},
        {"length",  INTRO_ATTR_LENGTH,  INTRO_V_MEMBER},
        {"switch",  INTRO_ATTR_SWITCH,  INTRO_V_CONDITION},
        {"type",    INTRO_ATTR_TYPE,    INTRO_V_NONE},
        {"note",    INTRO_ATTR_NOTE,    INTRO_V_STRING},
    };
    // NOTE: might need to do this later:
    //sh_new_arena(attribute_map);
    for (int i=0; i < LENGTH(initial); i++) {
        shputs(attribute_map, initial[i]);
    }
}

int
parse_attribute_register(ParseContext * ctx, char * s, int type, Token * type_tk) {
    const struct { char * key; int value_type; } value_type_lookup [] = {
        {"none",      INTRO_V_NONE},
        {"int",       INTRO_V_INT},
        {"float",     INTRO_V_FLOAT},
        {"value",     INTRO_V_VALUE}, // TODO
        {"condition", INTRO_V_CONDITION}, // TODO
        {"member",    INTRO_V_MEMBER},
        {"string",    INTRO_V_STRING},
    };

    Token tk0 = next_token(&s), tk1;
    if (tk0.type != TK_L_PARENTHESIS) {
        parse_error(ctx, &tk0, "Expected '('.");
        return 1;
    }

    tk0 = next_token(&s);
    if (tk0.type != TK_IDENTIFIER) {
        parse_error(ctx, &tk0, "Expected identifier.");
        return 1;
    }

    int value_type = INTRO_V_NONE;
    char * name = NULL;
    Token * name_ref = NULL;

    tk1 = next_token(&s);
    if (tk1.type == TK_IDENTIFIER) {
        bool matched = false;
        for (int i=0; i < LENGTH(value_type_lookup); i++) {
            if (tk_equal(&tk0, value_type_lookup[i].key)) {
                value_type = value_type_lookup[i].value_type;
                matched = true;
                break;
            }
        }
        if (!matched) {
            parse_error(ctx, &tk0, "Unknown attribute value type.");
            return 1;
        }
        name = copy_and_terminate(tk1.start, tk1.length);
        name_ref = &tk1;
    } else if (tk1.type == TK_R_PARENTHESIS) {
        name = copy_and_terminate(tk0.start, tk0.length);
        name_ref = &tk0;
    } else {
        parse_error(ctx, &tk1, "Expected identifier or ')'.");
        return 1;
    }

    int map_index = shgeti(attribute_map, name);
    if (map_index >= 0) {
        parse_error(ctx, name_ref, "Attribute name is reserved.");
        return 1;
    }

    for (int i=0; i < shlen(attribute_map); i++) {
        if (attribute_map[i].type == type) {
            char * msg = NULL;
            strputf(&msg, "Attribute type (%i) is reserved by attribute '%s'.", type, attribute_map[i].key);
            strputnull(msg);
            parse_error(ctx, type_tk, msg);
            arrfree(msg);
            return 2;
        }
    }

    struct attribute_map_s entry;
    entry.key = name;
    entry.type = type;
    entry.value_type = value_type;
    shputs(attribute_map, entry);

    return 0;
}

bool
check_id_valid(const IntroStruct * i_struct, int id) {
    for (int member_index = 0; member_index < i_struct->count_members; member_index++) {
        const IntroMember * member = &i_struct->members[member_index];
        for (int attr_index = 0; attr_index < member->count_attributes; attr_index++) {
            const IntroAttributeData * attr = &member->attributes[attr_index];
            if (attr->type == INTRO_ATTR_ID && attr->v.i == id) {
                return false;
            }
        }
    }
    return true;
}

int
parse_attribute(ParseContext * ctx, char ** o_s, IntroStruct * i_struct, int member_index, IntroAttributeData * o_result) {
    IntroAttributeData data = {0};
    Token tk = next_token(o_s);
    if (tk.type == TK_IDENTIFIER) {
        char * terminated_name = copy_and_terminate(tk.start, tk.length);
        int map_index = shgeti(attribute_map, terminated_name);
        free(terminated_name);
        if (map_index < 0) {
            parse_error(ctx, &tk, "No such attribute.");
            return 1;
        }

        data.type = attribute_map[map_index].type;
        data.value_type = attribute_map[map_index].value_type; // NOTE: you could just lookup the attribute's value type

        switch(data.value_type) {
        case INTRO_V_NONE: {
            if (data.type == INTRO_ATTR_TYPE) {
                IntroType * type = i_struct->members[member_index].type;
                if (!(type->name && strcmp(type->name, "IntroType") == 0)) {
                    parse_error(ctx, &tk, "Member must be of type 'IntroType' to have type attribute.");
                    return 1;
                }
            }
            data.v.i = 0;
        } break;

        case INTRO_V_INT: {
            tk = next_token(o_s);
            long result = strtol(tk.start, o_s, 0);
            if (*o_s == tk.start) {
                parse_error(ctx, &tk, "Invalid integer.");
                return 1;
            }
            data.v.i = (int32_t)result;
            if (data.type == INTRO_ATTR_ID && !check_id_valid(i_struct, data.v.i)) {
                parse_error(ctx, &tk, "This ID is reserved.");
                return 1;
            }
        } break;

        case INTRO_V_FLOAT: {
            tk = next_token(o_s);
            float result = strtof(tk.start, o_s);
            if (*o_s == tk.start) {
                parse_error(ctx, &tk, "Invalid floating point number.");
                return 1;
            }
            data.v.f = result;
        } break;

        case INTRO_V_STRING: {
            tk = next_token(o_s);
            if (tk.type != TK_STRING) {
                parse_error(ctx, &tk, "Expected string.");
                return 1;
            }
            char * result = copy_and_terminate(tk.start+1, tk.length-2); // TODO: parse escape codes
            int32_t index = arrlen(note_set);
            arrput(note_set, result);
            data.v.i = index;
        } break;

        case INTRO_V_VALUE: {
            parse_error(ctx, &tk, "Value attributes are not currently supported.");
            return 1;
        } break;

        case INTRO_V_MEMBER: {
            tk = next_token(o_s);
            if (tk.type != TK_IDENTIFIER || is_digit(tk.start[0])) {
                parse_error(ctx, &tk, "Expected member name.");
                return 1;
            }
            bool success = false;
            for (int mi=0; mi < i_struct->count_members; mi++) {
                if (tk_equal(&tk, i_struct->members[mi].name)) {
                    if (data.type == INTRO_ATTR_LENGTH) {
                        IntroType * type = i_struct->members[mi].type;
                        uint32_t category_no_size = type->category & 0xff0;
                        if (category_no_size != INTRO_SIGNED && category_no_size != INTRO_UNSIGNED) {
                            parse_error(ctx, &tk, "Length defining member must be an integer type.");
                            return 1;
                        }
                    }
                    data.v.i = mi;
                    success = true;
                    break;
                }
            }
            if (!success) {
                parse_error(ctx, &tk, "No such member.");
                return 1;
            }
        } break;

        case INTRO_V_CONDITION: {
            parse_error(ctx, &tk, "Condition attributes are not currently supported.");
            return 1;
        } break;
        }
    }

    if (o_result) *o_result = data;
    return 0;
}

int
parse_attributes(ParseContext * ctx, char * s, IntroStruct * i_struct, int member_index, IntroAttributeData ** o_result, uint32_t * o_count_attributes) {
    IntroAttributeData * attributes = NULL;

    Token tk = next_token(&s);
    if (tk.type != TK_L_PARENTHESIS) {
        parse_error(ctx, &tk, "Expected '('.");
        return 1;
    }
    while (1) {
        tk = next_token(&s);
        IntroAttributeData data;
        if (tk.type == TK_IDENTIFIER) {
            if (is_digit(tk.start[0])) {
                data.type = INTRO_ATTR_ID;
                data.value_type = INTRO_V_INT;
                // @copy from above
                long result = strtol(tk.start, &s, 0);
                if (s == tk.start) {
                    parse_error(ctx, &tk, "Invalid integer.");
                    return 1;
                }
                data.v.i = (int32_t)result;
                if (!check_id_valid(i_struct, data.v.i)) {
                    parse_error(ctx, &tk, "This ID is reserved.");
                    return 1;
                }
            } else {
                s = tk.start;
                int error = parse_attribute(ctx, &s, i_struct, member_index, &data);
                if (error) return 1;
            }
            arrput(attributes, data);
        } else if (tk.type == TK_R_PARENTHESIS) {
            break;
        } else {
            parse_error(ctx, &tk, "Invalid symbol.");
            return 1;
        }

        tk = next_token(&s);
        if (tk.type == TK_COMMA) {
        } else if (tk.type == TK_R_PARENTHESIS) {
            break;
        } else {
            parse_error(ctx, &tk, "Expected ',' or ')'.");
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
