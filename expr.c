#include "lexer.c"
#include "global.c"

#include <ctype.h>

#define EXPR_BUCKET_CAP (1<<12)

typedef enum {
    OP_NUMBER = 0x01,
    OP_LITERAL,
    OP_OTHER,
    OP_MACCESS,
    OP_PTR_MACCESS,
    OP_CONTAINER,

    OP_UNARY_ADD = 0x20,
    OP_UNARY_SUB,
    OP_BIT_NOT,
    OP_BOOL_NOT,
    OP_CAST,
    OP_DEREF,
    OP_ADDRESS,
    OP_SIZEOF,
    OP_ALIGNOF,

    OP_MUL = 0x30,
    OP_DIV,
    OP_MOD,

    OP_ADD = 0x40,
    OP_SUB,

    OP_SHIFT_LEFT = 0x50,
    OP_SHIFT_RIGHT,

    OP_LESS = 0x60,
    OP_LESS_OR_EQUAL,
    OP_GREATER,
    OP_GREATER_OR_EQUAL,

    OP_EQUAL = 0x70,
    OP_NOT_EQUAL,

    OP_BIT_AND  = 0x80,
    OP_BIT_XOR  = 0x90,
    OP_BIT_OR   = 0xa0,
    OP_BOOL_AND = 0xb0,
    OP_BOOL_OR  = 0xc0,

    OP_TERNARY_1 = 0xd0,
    OP_TERNARY_2,
} ExprOp;

enum ExprOpTypes {
    OP_TYPE_MASK  = 0xf0,
    OP_VALUE_TYPE = 0x00,
    OP_UNARY_TYPE = 0x20,
};

struct ExprContext {
    struct{char * key; intmax_t value;} * constant_map;
    MemArena * arena;
    ParseContext * ctx;
    LocationContext * ploc;
    enum {
        MODE_PRE,
        MODE_PARSE,
    } mode;
};

typedef struct ExprNode ExprNode;
struct ExprNode {
    ExprNode * left;
    ExprNode * right;
    int depth;
    ExprOp op;
    intmax_t value;
    const IntroType * type;
    Token tk;
};

static void UNUSED
free_expr_context(ExprContext * ectx) {
    free_arena(ectx->arena);
    shfree(ectx->constant_map);
}

static void
insert_node_into_tree(ExprNode ** p_base, ExprNode * node) {
    ExprNode ** p_slot = p_base;
    while (1) {
        ExprNode * slot = *p_slot;

        if (
            (slot == NULL)
          ||(node->depth < slot->depth)
          ||(
              (node->depth == slot->depth)
            &&((node->op & OP_TYPE_MASK) >= (slot->op & OP_TYPE_MASK))
            )
           )
        {
            node->left = slot;
            *p_slot = node;
            break;
        }
        p_slot = &slot->right;
    }
}

char *
parse_escaped_string(Token * str_tk, size_t * o_length) {
    char * result = malloc(str_tk->length);
    char * src = str_tk->start + 1;
    char * dst = result;
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
            *dst++ = c;
        } else {
            *dst++ = *src;
        }
        src++;
    }
    *dst++ = 0;
    *o_length = dst - result;
    return result;
}

char *
create_escaped_string(const char * str) {
    const char * src = str - 1;
    const char *const end = str + strlen(str);
    char * result = NULL;
    arrsetcap(result, end - src + 5);

    arrput(result, '"');
    while (++src < end) {
        char esc;

        switch (*src) {
        case '\n': esc = 'n'; break;
        case '\t': esc = 't'; break;
        case '\r': esc = 'r'; break;
        case '\b': esc = 'b'; break;
        case '\a': esc = 'a'; break;
        case '\f': esc = 'f'; break;
        case '\v': esc = 'v'; break;
        case '\\': esc = '\\'; break;
        case '\"': esc = '\"'; break;
        default: {
            if (isprint(*src)) {
                arrput(result, *src);
                continue;
            } else {
                strputf(&result, "\\x%02x", (unsigned int)*src);
                continue;
            }
        }break;
        }

        char * dst = arraddnptr(result, 2);
        dst[0] = '\\';
        dst[1] = esc;
    }
    arrput(result, '"');
    arrput(result, '\0');

    return result;
}

ExprNode *
build_expression_tree2(ExprContext * ectx, TokenIndex * tidx) {
    int paren_depth = 0;
    bool next_is_unary = true;
    ExprNode * base = NULL;
    ExprNode * branch = NULL;
    ExprNode * last = NULL;
    ExprNode * node = arena_alloc(ectx->arena, sizeof(*base));
    Token tk;
    while (1) {
        bool found_end = false;
        tk = next_token(tidx);
        switch (tk.type) {
        case TK_COMMA:
        case TK_R_BRACKET:
        case TK_R_BRACE:
        case TK_SEMICOLON:
        case TK_END: {
            tidx->index -= 1;
            found_end = true;
        }break;

        case TK_L_PARENTHESIS: {
            if (ectx->mode == MODE_PARSE && tk_at(tidx).type == TK_IDENTIFIER) {
                size_t last_index = tidx->index;
                DeclState cast = {.state = DECL_CAST};
                int ret = parse_declaration(ectx->ctx, tidx, &cast);
                if (ret == RET_DECL_FINISHED || ret == RET_DECL_CONTINUE) {
                    node->op = OP_CAST;
                    node->type = cast.type;
                    break;
                } else {
                    tidx->index = last_index;
                }
            }
            paren_depth += 1;
            continue;
        }break;

        case TK_R_PARENTHESIS: {
            paren_depth -= 1;
            if (paren_depth < 0) {
                tidx->index -= 1;
                found_end = true;
                break;
            }
            continue;
        }break;

        case TK_IDENTIFIER: {
            if (ectx->mode == MODE_PARSE) {
                if (tk_equal(&tk, "sizeof")) {
                    node->op = OP_SIZEOF;
                } else if (tk_equal(&tk, "_Alignof")) {
                    node->op = OP_ALIGNOF;
                } else if (tk_equal(&tk, "I")) {
                    if (tk_at(tidx).type == TK_L_PARENTHESIS) {
                        found_end = true;
                        tidx->index -= 1;
                    } else {
                        node->op = OP_OTHER;
                    }
                } else {
                    node->op = OP_OTHER;
                }
            } else {
                node->op = OP_NUMBER;
            }
        }break;

        case TK_NUMBER: {
            node->op = OP_NUMBER;
        }break;

        case TK_STRING:        node->op = OP_LITERAL; break;

        case TK_PLUS:          node->op = (next_is_unary)? OP_UNARY_ADD : OP_ADD; break;
        case TK_HYPHEN:        node->op = (next_is_unary)? OP_UNARY_SUB : OP_SUB; break;
        case TK_STAR:          node->op = (next_is_unary)? OP_DEREF : OP_MUL; break;
        case TK_AND:           node->op = (next_is_unary)? OP_ADDRESS : OP_BIT_AND; break;
        case TK_FORSLASH:      node->op = OP_DIV; break;
        case TK_MOD:           node->op = OP_MOD; break;
        case TK_D_EQUAL:       node->op = OP_EQUAL; break;
        case TK_NOT_EQUAL:     node->op = OP_NOT_EQUAL; break;
        case TK_D_AND:         node->op = OP_BOOL_AND; break;
        case TK_D_BAR:         node->op = OP_BOOL_OR; break;
        case TK_L_ANGLE:       node->op = OP_LESS; break;
        case TK_LESS_EQUAL:    node->op = OP_LESS_OR_EQUAL; break;
        case TK_R_ANGLE:       node->op = OP_GREATER; break;
        case TK_GREATER_EQUAL: node->op = OP_GREATER_OR_EQUAL; break;
        case TK_BAR:           node->op = OP_BIT_OR; break;
        case TK_CARET:         node->op = OP_BIT_XOR; break;
        case TK_BANG:          node->op = OP_BOOL_NOT; break;
        case TK_TILDE:         node->op = OP_BIT_NOT; break;
        case TK_LEFT_SHIFT:    node->op = OP_SHIFT_LEFT; break;
        case TK_RIGHT_SHIFT:   node->op = OP_SHIFT_RIGHT; break;
        case TK_QUESTION_MARK: node->op = OP_TERNARY_1; break;
        case TK_COLON:         node->op = OP_TERNARY_2; break;
        case TK_PERIOD:        node->op = OP_MACCESS; break;
        case TK_L_ARROW:       node->op = OP_CONTAINER; break;
        case TK_R_ARROW:       node->op = OP_PTR_MACCESS; break;

        default: {
            char * msg = "Invalid token in expression.";
            if (ectx->mode == MODE_PARSE) {
                parse_error(ectx->ctx, tk, msg);
            } else {
                preprocess_message_internal(ectx->ploc, &tk, msg, 0);
            }
            return NULL;
        }break;
        }

        if (found_end) {
            if (branch) {
                insert_node_into_tree(&base, branch);
            }
            break;
        }

        if (node->op == OP_TERNARY_2) paren_depth -= 1;

        node->depth = paren_depth;
        node->tk = tk;

        if (node->op == OP_TERNARY_1) paren_depth += 1;

        next_is_unary = false;
        if ((node->op & OP_TYPE_MASK) <= OP_UNARY_TYPE) {
            if (last) {
                last->right = node;
                if (node->op == OP_CAST && (last->op == OP_SIZEOF || last->op == OP_ALIGNOF)) {
                    insert_node_into_tree(&base, branch);
                    branch = NULL;
                    goto alloc_next_node;
                }
            }
            last = node;
            if (!branch) branch = node;
            if ((node->op & OP_TYPE_MASK) == OP_UNARY_TYPE) {
                next_is_unary = true;
            }
        } else {
            if (branch) {
                insert_node_into_tree(&base, branch);
                branch = NULL;
            }
            last = NULL;
            insert_node_into_tree(&base, node);
            next_is_unary = true;
        }

      alloc_next_node:
        node = arena_alloc(ectx->arena, sizeof(*node));
    }

    return base;
}

static int32_t
get_member_offset(const IntroType * type, const Token * p_name_tk, const IntroType ** o_member_type) {
    for (int i=0; i < type->count; i++) {
        IntroMember member = type->members[i];
        if (member.name) {
            if (tk_equal(p_name_tk, member.name)) {
                *o_member_type = member.type;
                return member.offset;
            }
        } else {
            if (intro_has_members(member.type)) {
                int32_t ret = get_member_offset(member.type, p_name_tk, o_member_type);
                if (ret >= 0) {
                    return ret + member.offset;
                }
            }
        }
    }
    return -1;
}

static void
put_imm_int(uint8_t ** pproc, uint64_t val) {
    int size;
    if ((val & UINT8_MAX) == val) {
        size = 1;
        arrput(*pproc, I_IMM8);
    } else if ((val & UINT16_MAX) == val) {
        size = 2;
        arrput(*pproc, I_IMM16);
    } else if ((val & UINT32_MAX) == val) {
        size = 4;
        arrput(*pproc, I_IMM32);
    } else {
        size = 8;
        arrput(*pproc, I_IMM64);
    }
    void * dest = arraddnptr(*pproc, size);
    memcpy(dest, &val, size);
}

uint8_t *
build_expression_procedure_internal(ExprContext * ectx, ExprNode * node, const IntroContainer * cont) {
    uint8_t * proc = NULL;
    bool use_float_expr = false;

    if (node->op == OP_MACCESS || node->op == OP_OTHER || node->op == OP_CONTAINER) {
        if (!cont) {
            goto match_constant;
        }

        size_t offset = 0;
        const IntroContainer * top_level = cont;
        while (top_level->parent) {
            if (top_level->index == top_level->parent->type->count) {
                offset += top_level->parent->type->size;
            } else {
                offset += top_level->parent->type->members[top_level->index].offset;
            }
            top_level = top_level->parent;
        }

        ExprNode * base_node = node;
        while (1) {
            switch (node->op) {
            case OP_CONTAINER:
                if (!cont->parent) {
                    parse_error(ectx->ctx, node->tk, "No container to access.");
                }
                offset -= cont->parent->type->members[cont->index].offset;
                cont = cont->parent;
                node->type = cont->type;
                // FALLTHROUGH
            case OP_MACCESS:
                if (!node->right) {
                    parse_error(ectx->ctx, node->tk, "What are you accessing.");
                    exit(1);
                }
                node = node->right;
                // FALLTHROUGH
            case OP_OTHER: {
                int32_t moff = get_member_offset(cont->type, &node->tk, &node->type);
                if (moff < 0) {
                    if (!base_node->right) {
                    match_constant: ;
                        STACK_TERMINATE(name, node->tk.start, node->tk.length);
                        ptrdiff_t map_index = shgeti(ectx->constant_map, name);
                        if (map_index >= 0) {
                            put_imm_int(&proc, ectx->constant_map[map_index].value);
                            node->type = parse_get_known(ectx->ctx, 8);
                            return proc;
                        }
                    }
                    parse_error(ectx->ctx, node->tk, "Identifier is not member or constant.");
                    exit(1);
                }
                offset += moff;
                IntroContainer * next_cont = arena_alloc(ectx->arena, sizeof(*next_cont));
                next_cont->parent = cont;
                next_cont->type = node->type;
                cont = next_cont;
            }break;

            case OP_PTR_MACCESS: {
                if (node->right <= 0) {
                    parse_error(ectx->ctx, node->tk, "What are you accessing.");
                    exit(1);
                }
                node = node->right;

                assert_msg(0, "Unimplemented."); // TODO
            }break;

            default:
                _assume(0);
            }

            if (node->right) {
                node = node->right;
            } else {
                break;
            }
        }

        if (!(intro_is_scalar(node->type) || node->type->category == INTRO_ENUM)) {
            parse_error(ectx->ctx, node->tk, "Cannot use non-scalar here.");
            exit(1);
        }
        size_t read_size = node->type->size;
        uint8_t inst;
        switch(read_size) {
        case 1: inst = I_LD8;  break;
        case 2: inst = I_LD16; break;
        case 4: inst = I_LD32; break;
        case 8: inst = I_LD64; break;
        default: _assume(0), inst = 0;
        }

        put_imm_int(&proc, offset);
        arrput(proc, inst);

        base_node->type = node->type;

        return proc;
    } else if (node->op != OP_SIZEOF && node->op != OP_ALIGNOF) {
        if (node->left) {
            uint8_t * clip = build_expression_procedure_internal(ectx, node->left, cont);
            if (clip) {
                void * dest = arraddnptr(proc, arrlen(clip));
                memcpy(dest, clip, arrlen(clip));
                arrfree(clip);
                if (node->op == OP_BOOL_OR || node->op == OP_BOOL_AND) {
                    arrput(proc, I_BOOL);
                }
            }

            if (ectx->ctx && intro_is_floating(node->left->type)) {
                use_float_expr = true;
                if (node->left->type->category == INTRO_F32) {
                    arrput(proc, I_CVT_F_TO_D);
                }
            }
        }

        if (node->right) {
            uint8_t * clip = build_expression_procedure_internal(ectx, node->right, cont);
            if (ectx->ctx && intro_is_floating(node->right->type)) {
                if (node->left && !use_float_expr) {
                    use_float_expr = true;
                    arrput(proc, I_CVT_I_TO_D);
                }
            }

            if (clip) {
                void * dest = arraddnptr(proc, arrlen(clip));
                memcpy(dest, clip, arrlen(clip));
                arrfree(clip);
            }

            if (ectx->ctx && use_float_expr) {
                if (intro_is_int(node->right->type)) {
                    arrput(proc, I_CVT_I_TO_D);
                } else if (node->right->type->category == INTRO_F32) {
                    arrput(proc, I_CVT_F_TO_D);
                }
            }
        }

        if (ectx->ctx) {
            if (use_float_expr) {
                node->type = parse_get_known(ectx->ctx, 10); // F64
            } else {
                node->type = parse_get_known(ectx->ctx, 8); // S64
            }
        }
    }

    switch(node->op) {
    case OP_NUMBER: {
        Token ntk = node->tk;
        if (memcmp(ntk.start, "0x", 2)!=0 && memchr(ntk.start, '.', ntk.length)) {
            union {
                double d;
                uint64_t u;
            } val;
            val.d = strtod(ntk.start, NULL);
            put_imm_int(&proc, val.u);
            if (ectx->ctx) {
                node->type = parse_get_known(ectx->ctx, 10); // F64
            }
        } else {
            long long val = strtoll(ntk.start, NULL, 0);
            put_imm_int(&proc, val);
            if (ectx->ctx) {
                node->type = parse_get_known(ectx->ctx, 8); // S64
            }
        }
    }break;

    case OP_LITERAL: {
        if (node->tk.type == TK_STRING) {
            put_imm_int(&proc, 1);
            // TODO
        }
    }break;

    case OP_MACCESS:
    case OP_PTR_MACCESS:
    case OP_CONTAINER:
    case OP_OTHER:
        _assume(0);

    case OP_DEREF: {
        // TODO
    }break;

    case OP_ADDRESS: {
        // TODO
    }break;

    case OP_CAST: {
        if (intro_is_int(node->type)) {
            if (intro_is_int(node->right->type)) {
            } else if (node->right->type->category == INTRO_F32) {
                arrput(proc, I_CVT_F_TO_I);
            } else if (node->right->type->category == INTRO_F64) {
                arrput(proc, I_CVT_D_TO_I);
            }
            if (node->type->size != 8) {
                put_imm_int(&proc, (1 << node->type->size) - 1);
                arrput(proc, I_BIT_AND);
            }
        } else if ((node->type->category & 0xf0) == INTRO_FLOATING && intro_is_int(node->right->type)) {
            arrput(proc, I_CVT_I_TO_D);
        }
    }break;

    case OP_SIZEOF: {
        if (node->right->type) {
            size_t size = node->right->type->size;
            put_imm_int(&proc, size);
        } else if (node->right->tk.type == TK_STRING) {
            size_t size = node->right->tk.length - 1;
            put_imm_int(&proc, size);
        } else {
            parse_error(ectx->ctx, node->tk, "Cannot determine size.");
            exit(1);
        }
        node->type = parse_get_known(ectx->ctx, 8);
    }break;

    case OP_ALIGNOF: {
        if (node->right->type) {
            size_t alignment = node->right->type->align;
            put_imm_int(&proc, alignment);
        } else {
            parse_error(ectx->ctx, node->tk, "Cannot determine alignment.");
            exit(1);
        }
        node->type = parse_get_known(ectx->ctx, 8);
    }break;

    case OP_UNARY_ADD: break;

    case OP_UNARY_SUB:
        arrput(proc, I_NEGATE_I);
        break;
    case OP_BIT_NOT:
        arrput(proc, I_BIT_NOT);
        break;
    case OP_BOOL_NOT:
        arrput(proc, I_BOOL_NOT);
        break;

    case OP_MUL:
        if (use_float_expr) {
            arrput(proc, I_MULF);
        } else {
            arrput(proc, I_MULI);
        }
        break;
    case OP_DIV:
        if (use_float_expr) {
            arrput(proc, I_DIVF);
        } else {
            arrput(proc, I_DIVI);
        }
        break;
    case OP_MOD:
        arrput(proc, I_MODI);
        break;

    case OP_ADD:
        if (use_float_expr) {
            arrput(proc, I_ADDF);
        } else {
            arrput(proc, I_ADDI);
        }
        break;
    case OP_SUB:
        if (use_float_expr) {
            arrput(proc, I_NEGATE_F);
            arrput(proc, I_ADDF);
        } else {
            arrput(proc, I_NEGATE_I);
            arrput(proc, I_ADDI);
        }
        break;

    case OP_SHIFT_LEFT:
        arrput(proc, I_L_SHIFT);
        break;
    case OP_SHIFT_RIGHT: 
        arrput(proc, I_R_SHIFT);
        break;

    case OP_LESS:
        arrput(proc, (use_float_expr)? I_CMP : I_CMP_F);
        arrput(proc, I_SETL);
        break;
    case OP_LESS_OR_EQUAL:
        arrput(proc, (use_float_expr)? I_CMP : I_CMP_F);
        arrput(proc, I_SETLE);
        break;
    case OP_GREATER:
        arrput(proc, (use_float_expr)? I_CMP : I_CMP_F);
        arrput(proc, I_SETLE);
        arrput(proc, I_BOOL_NOT);
        break;
    case OP_GREATER_OR_EQUAL:
        arrput(proc, (use_float_expr)? I_CMP : I_CMP_F);
        arrput(proc, I_SETL);
        arrput(proc, I_BOOL_NOT);
        break;

    case OP_EQUAL:
        arrput(proc, (use_float_expr)? I_CMP : I_CMP_F);
        arrput(proc, I_SETE);
        break;
    case OP_NOT_EQUAL:
        arrput(proc, (use_float_expr)? I_CMP : I_CMP_F);
        arrput(proc, I_SETE);
        arrput(proc, I_BOOL_NOT);
        break;

    case OP_BIT_AND:
        arrput(proc, I_BIT_AND);
        break;
    case OP_BIT_XOR:
        arrput(proc, I_BIT_XOR);
        break;
    case OP_BIT_OR:
        arrput(proc, I_BIT_OR);
        break;
    case OP_BOOL_AND:
        arrput(proc, I_BOOL);
        arrput(proc, I_BIT_AND);
        break;
    case OP_BOOL_OR:
        arrput(proc, I_BOOL);
        arrput(proc, I_BIT_OR);
        break;

    case OP_TERNARY_1: break;

    case OP_TERNARY_2:
        arrput(proc, I_CND_LD_TOP);
        break;
    }

    return proc;
}

uint8_t *
build_expression_procedure2(ExprContext * ectx, ExprNode * tree, const IntroContainer * cont) {
    uint8_t * result = build_expression_procedure_internal(ectx, tree, cont);
    arrput(result, I_RETURN);
    return result;
}

void
interactive_calculator() {
    char expr_buf [1024];
    ExprContext expr_ctx = {0};
    expr_ctx.arena = new_arena(512);
    while (1) {
        printf("expr> ");
        fgets(expr_buf, sizeof(expr_buf), stdin);
        char * endl = strchr(expr_buf, '\n');
        if (endl) *endl = '\0';

        if (0==strcmp(expr_buf, "q")) {
            break;
        }

        Token * tklist = create_token_list(expr_buf);

        TokenIndex tidx = {.list = tklist, .index = 0};

        ExprNode * tree = build_expression_tree2(&expr_ctx, &tidx);
        if (tree == NULL) {
            printf("invalid symbol\n");
            arrfree(tklist);
            continue;
        }
        uint8_t * procedure = build_expression_procedure2(&expr_ctx, tree, NULL);
        union IntroRegisterData ret = intro_run_bytecode(procedure, NULL);
        printf(" = %i    (expr size: %i)\n", (int)ret.si, (int)arrlen(procedure));

        reset_arena(expr_ctx.arena);
        arrfree(tklist);
        arrfree(procedure);
    }
    free_arena(expr_ctx.arena);
}
