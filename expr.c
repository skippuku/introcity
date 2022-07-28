#include "lexer.c"
#include "global.c"

#define EXPR_BUCKET_CAP (1<<12)

typedef enum {
    OP_NUMBER = 0x00,
    OP_OTHER,
    OP_INT = OP_NUMBER, // TODO: remove

    OP_MACCESS = 0x10,
    OP_PTR_MACCESS,
    OP_CONTAINER,
    OP_DEREF,
    OP_ADDRESS,

    OP_UNARY_ADD = 0x21,
    OP_UNARY_SUB,
    OP_BIT_NOT,
    OP_BOOL_NOT,
    OP_CAST,
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

    OP_PUSH = 0xf0, // intruction only
    OP_SET,
    OP_DONE,
} ExprOp;

typedef enum {
    I_INVALID = 0,
    I_RETURN = 1,

    I_LD,
    I_IMM,
    I_ZERO,

    I_CND_LD_TOP,

    I_NEGATE_I,
    I_NEGATE_F,
    I_BIT_NOT,
    I_NOT_ZERO,
    I_CVT_D_TO_I,
    I_CVT_F_TO_I,
    I_CVT_I_TO_D,
    I_CVT_F_TO_D,

    I_GREATER_POP = I_CVT_F_TO_D,

    I_ADDI,
    I_MULI,
    I_DIVI,
    I_MODI,
    I_L_SHIFT,
    I_R_SHIFT,

    I_BIT_AND,
    I_BIT_OR,
    I_BIT_XOR,

    I_CMP,

    I_ADDF,
    I_MULF,
    I_DIVF,

    I_COUNT
} InstrCode;

typedef enum {
    I_S8  = 0x00,
    I_S16 = 0x40,
    I_S32 = 0x80,
    I_S64 = 0xC0,
} InstrExt;

typedef union {
    uint64_t ui;
    int64_t  si;
    float    sf;
    double   df;
} RegisterData;

_Static_assert(I_COUNT <= (1 << 6), "Too many bytecode intructions to fit in 6 bits");

enum ExprOpTypes {
    OP_TYPE_MASK  = 0xf0,
    OP_VALUE_TYPE = 0x00,
    OP_UNARY_TYPE = 0x20,
};

struct ExprContext {
    struct{char * key; intmax_t value;} * constant_map;
    MemArena * arena;
    ParseContext * ctx;
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
    IntroType * type;
    Token tk;
};

typedef enum {
    REG_VALUE,
    REG_LAST_RESULT,
    REG_POP_STACK,
} ExprInstructionRegisterType; // TODO: remove

typedef struct {
    ExprOp op;
    ExprInstructionRegisterType left_type, right_type;
    intmax_t left_value, right_value;
} ExprInstruction; // TODO: remove

typedef struct {
    int stack_size;
    int count_instructions;
    ExprInstruction instructions [];
} ExprProcedure; // TODO: remove

static void UNUSED
free_expr_context(ExprContext * ectx) {
    free_arena(ectx->arena);
    shfree(ectx->constant_map);
}

#if 0
case TK_IDENTIFIER: {
    if (ectx->mode == MODE_PARSE && !is_digit(tk.start[0])) {
        if (tk_equal(&tk, "sizeof")) {
            if (tk_i + 3 >= count_tokens) {
                *o_error_tk = tk;
                return NULL;
            }
            Token tk1 = tokens[++tk_i];
            if (tk1.type != TK_L_PARENTHESIS) {
                *o_error_tk = tk1;
                return NULL;
            }

            tk1 = tokens[++tk_i];
            DeclState cast = {.state = DECL_CAST};
            TokenIndex eidx = {.list = tokens, .index = tk_i};
            int ret = parse_declaration(ectx->ctx, &eidx, &cast);
            if (ret == RET_DECL_FINISHED || ret == RET_DECL_CONTINUE) {
                node->value = cast.type->size;
            } else if (ret == RET_NOT_TYPE) {
                if (tk1.type == TK_STRING) {
                    node->value = tk1.length - 1;
                } else {
                    return NULL;
                }
            } else {
                return NULL;
            }

            tk_i = eidx.index;
        } else {
            STACK_TERMINATE(terminated, tk.start, tk.length);
            ptrdiff_t const_index = shgeti(ectx->constant_map, terminated);
            // NOTE: dumb hack to get around casts... if they are to int
            if (paren_depth > 0 && tk_equal(&tk, "int")) {
                while (1) {
                    if (++tk_i >= count_tokens) {
                        *o_error_tk = tk;
                        return NULL;
                    }
                    Token tk1 = tokens[tk_i];
                    if (tk1.type == TK_R_PARENTHESIS) {
                        paren_depth -= 1;
                        break;
                    }
                }
                continue;
            }
            if (const_index < 0) {
                *o_error_tk = tk;
                return NULL;
            }
            node->value = ectx->constant_map[const_index].value;
        }
    } else {
        node->value = (intmax_t)strtol(tk.start, NULL, 0);
    }
    node->op = OP_INT;
}break;
#endif

ExprNode *
build_expression_tree2(ExprContext * ectx, MemArena * arena, TokenIndex * tidx) {
    int paren_depth = 0;
    ExprNode * base = NULL;
    ExprNode * node = arena_alloc(arena, sizeof(*base));
    Token tk;
    bool last_was_value = false;
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
            paren_depth += 1;
            continue;
        }break;

        case TK_R_PARENTHESIS: {
            paren_depth -= 1;
            if (paren_depth < 0) {
                found_end = true;
            }
            continue;
        }break;

        case TK_IDENTIFIER: {
            if (ectx->mode == MODE_PARSE) {
                tidx->index -= 1;
                DeclState cast = {.state = DECL_CAST};
                int ret = parse_declaration(ectx->ctx, tidx, &cast);
                if (ret == RET_DECL_FINISHED || ret == RET_DECL_CONTINUE) {
                    node->op = OP_CAST;
                    node->type = cast.type;
                } else if (ret == RET_NOT_TYPE) {
                    if (tk_equal(&tk, "sizeof")) {
                        node->op = OP_SIZEOF;
                    } else if (tk_equal(&tk, "_Alignof")) {
                        node->op = OP_ALIGNOF;
                    } else {
                        node->op = OP_OTHER;
                    }
                } else {
                    return NULL;
                }
            } else {
                node->op = OP_NUMBER;
            }
        }break;

        case TK_NUMBER: {
            node->op = OP_NUMBER;
        }break;

        case TK_PLUS:          node->op = (last_was_value)? OP_ADD : OP_UNARY_ADD; break;
        case TK_HYPHEN:        node->op = (last_was_value)? OP_SUB : OP_UNARY_SUB; break;
        case TK_STAR:          node->op = (last_was_value)? OP_MUL : OP_DEREF; break;
        case TK_AND:           node->op = (last_was_value)? OP_BIT_AND : OP_ADDRESS; break;
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
            parse_error(ectx->ctx, tk, "Invalid token.");
            return NULL;
        }break;
        }

        if (found_end) break;

        if (node->op == OP_TERNARY_2) paren_depth -= 1;

        node->depth = paren_depth;
        node->tk = tk;

        if (node->op == OP_TERNARY_1) paren_depth += 1;

        last_was_value = ((node->op & 0xf0) <= OP_INT);

        ExprNode ** p_index = &base;
        while (1) {
            ExprNode * index = *p_index;

            if (
                (index == NULL)
              ||(node->depth < index->depth)
              ||(
                 (node->depth == index->depth && (node->op & OP_TYPE_MASK) >= (index->op & OP_TYPE_MASK))
               &&((node->op & OP_TYPE_MASK) != OP_UNARY_TYPE)
                )
               )
            {
                node->left = index;
                *p_index = node;
                break;
            }
            p_index = &index->right;
        }
        node = arena_alloc(arena, sizeof(*node));
    }

    return base;
}

static int32_t
get_member_offset(const IntroType * type, Token * p_name_tk) {
    for (int i=0; i < type->count; i++) {
        IntroMember member = type->members[i];
        if (member.name) {
            if (tk_equal(p_name_tk, member.name)) {
                return member.offset;
            }
        } else {
            if (intro_has_members(member.type)) {
                int32_t ret = get_member_offset(member.type, p_name_tk);
                if (ret >= 0) {
                    return ret + member.offset;
                }
            }
        }
    }
    return -1;
}

static void
put_imm_int(uint8_t ** pproc, intmax_t val) {
    int ext;
    if ((val & UINT8_MAX) == val) {
        ext = I_S8;
    } else if ((val & UINT16_MAX) == val) {
        ext = I_S16;
    } else if ((val & UINT32_MAX) == val) {
        ext = I_S32;
    } else {
        ext = I_S64;
    }
    bool is_negative = val < 0;
    if (is_negative) {
        val = -val;
    }
    arrput(*pproc, I_IMM | ext);
    size_t size = 1 << (ext >> 6);
    void * dest = arraddnptr(*pproc, size);
    memcpy(dest, &val, size);
    if (is_negative) {
        arrput(*pproc, I_NEGATE_I);
    }
}

uint8_t *
build_expression_procedure_internal(ExprContext * ectx, ExprNode * node, const IntroContainer * cont) {
    uint8_t * proc = NULL;

    if (node->left) {
        uint8_t * clip = build_expression_procedure_internal(ectx, node->left, cont);
        void * dest = arraddnptr(proc, arrlen(clip));
        memcpy(dest, clip, arrlen(clip));
        arrfree(clip);
        if (node->op == OP_BOOL_OR || node->op == OP_BOOL_AND) {
            arrput(proc, I_NOT_ZERO);
        }
    }
    if (node->right) {
        uint8_t * clip = build_expression_procedure_internal(ectx, node->right, cont);
        void * dest = arraddnptr(proc, arrlen(clip));
        memcpy(dest, clip, arrlen(clip));
        arrfree(clip);
    }

    switch(node->op) {
    case OP_NUMBER: {
        Token ntk = node->tk;
        if (memcmp(ntk.start, "0x", 2)!=0 && (memchr(ntk.start, '.', ntk.length))) {
            double val = strtod(ntk.start, NULL);
            arrput(proc, I_IMM | I_S64);
            void * dest = arraddnptr(proc, 8);
            memcpy(dest, &val, 8);
        } else {
            long val = strtol(ntk.start, NULL, 0);
            put_imm_int(&proc, val);
        }
    }break;

    case OP_OTHER: {
        STACK_TERMINATE(const_name, node->tk.start, node->tk.length);
        ptrdiff_t map_index = shgeti(ectx->constant_map, const_name);
        if (map_index >= 0) {
            intmax_t value = ectx->constant_map[map_index].value;
            put_imm_int(&proc, value);
        } else {
            parse_error(ectx->ctx, node->tk, "Unknown identifier.");
            exit(1);
        }
    }break;

    case OP_SIZEOF: {
        if (node->right->type) {
            size_t size = node->right->type->size;
            put_imm_int(&proc, size);
        } else {
            parse_error(ectx->ctx, node->tk, "Cannot determine size.");
            exit(1);
        }
    }break;

    case OP_CAST: {
    }break;

    case OP_UNARY_ADD: break;

    case OP_UNARY_SUB:
        arrput(proc, I_NEGATE_I);
        break;
    case OP_BIT_NOT:
        arrput(proc, I_BIT_NOT);
        break;
    case OP_BOOL_NOT:
        arrput(proc, I_NOT_ZERO);
        arrput(proc, I_IMM | I_S8);
        arrput(proc, 1);
        arrput(proc, I_BIT_XOR);
        break;

    case OP_MUL:
        arrput(proc, I_MULI);
        break;
    case OP_DIV:
        arrput(proc, I_DIVI);
        break;
    case OP_MOD:
        arrput(proc, I_MODI);
        break;

    case OP_ADD:
        arrput(proc, I_ADDI);
        break;
    case OP_SUB:
        arrput(proc, I_NEGATE_I);
        arrput(proc, I_ADDI);
        break;

    case OP_SHIFT_LEFT:
        arrput(proc, I_L_SHIFT);
        break;
    case OP_SHIFT_RIGHT: 
        arrput(proc, I_R_SHIFT);
        break;

    case OP_LESS:
        arrput(proc, I_CMP);
        arrput(proc, I_IMM | I_S8);
        arrput(proc, (1 << 1));
        arrput(proc, I_BIT_AND);
        arrput(proc, I_NOT_ZERO);
        break;
    case OP_LESS_OR_EQUAL:
        arrput(proc, I_CMP);
        arrput(proc, I_NOT_ZERO);
        break;
    case OP_GREATER:
        arrput(proc, I_CMP);
        arrput(proc, I_NOT_ZERO);
        arrput(proc, I_IMM | I_S8);
        arrput(proc, 1);
        arrput(proc, I_BIT_XOR);
        break;
    case OP_GREATER_OR_EQUAL:
        arrput(proc, I_CMP);
        arrput(proc, I_BIT_NOT);
        arrput(proc, I_IMM | I_S8);
        arrput(proc, (1 << 1));
        arrput(proc, I_BIT_AND);
        arrput(proc, I_NOT_ZERO);
        break;

    case OP_EQUAL:
        arrput(proc, I_CMP);
        arrput(proc, I_IMM | I_S8);
        arrput(proc, 1);
        arrput(proc, I_BIT_AND);
        break;
    case OP_NOT_EQUAL:
        arrput(proc, I_CMP);
        arrput(proc, I_BIT_NOT);
        arrput(proc, I_IMM | I_S8);
        arrput(proc, 1);
        arrput(proc, I_BIT_AND);
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
        arrput(proc, I_NOT_ZERO);
        arrput(proc, I_BIT_AND);
        break;
    case OP_BOOL_OR:
        arrput(proc, I_NOT_ZERO);
        arrput(proc, I_BIT_OR);
        break;

    case OP_TERNARY_1: break;

    case OP_TERNARY_2:
        arrput(proc, I_CND_LD_TOP);
        break;

    case OP_PUSH: case OP_SET: case OP_DONE: break;
    }

    return proc;
}

uint8_t *
build_expression_procedure2(ExprContext * ectx, ExprNode * tree, const IntroContainer * cont) {
    uint8_t * result = build_expression_procedure_internal(ectx, tree, cont);
    arrput(result, I_RETURN);
    return result;
}

RegisterData
intro_run_bytecode(uint8_t * code, uint8_t * data) {
    RegisterData stack [1024];
    RegisterData r0, r1, r2;
    size_t stack_idx = 0;
    size_t code_idx = 0;

    while (1) {
        uint8_t byte = code[code_idx++];
        uint8_t inst = byte & ~0xC0;
        uint8_t size = 1 << ((byte >> 6) & 0x03);

        if (inst > I_GREATER_POP) {
            r1 = r0;
            r0 = stack[--stack_idx];
        }

        switch ((InstrCode)inst) {
        case I_RETURN: return r0;

        case I_LD: {
            stack[stack_idx++] = r0;
            r0.ui = 0;
            memcpy(&r0, data + r0.ui, size);
        }break;

        case I_IMM: {
            stack[stack_idx++] = r0;
            r0.ui = 0;
            memcpy(&r0, &code[code_idx], size);
            code_idx += size;
        }break;

        case I_ZERO: {
            stack[stack_idx++] = r0;
            r0.ui = 0;
        }break;

        case I_CND_LD_TOP: {
            r1 = stack[--stack_idx]; // alternate value
            r2 = stack[--stack_idx]; // condition
            if (r2.ui) r0 = r1;
        }break;

        case I_NEGATE_I:   r0.si = -r0.si; break;
        case I_NEGATE_F:   r0.df = -r0.df; break;
        case I_BIT_NOT:    r0.ui = ~r0.ui; break;
        case I_NOT_ZERO:   r0.ui = !!(r0.ui); break;
        case I_CVT_D_TO_I: r0.si = (int64_t)r0.df; break;
        case I_CVT_F_TO_I: r0.si = (int64_t)r0.sf; break;
        case I_CVT_I_TO_D: r0.df = (double) r0.si; break;
        case I_CVT_F_TO_D: r0.df = (double) r0.sf; break;

        case I_ADDI: r0.si += r1.si; break;
        case I_MULI: r0.si *= r1.si; break;
        case I_DIVI: r0.si /= r1.si; break;
        case I_MODI: r0.si %= r1.si; break;

        case I_L_SHIFT: r0.ui <<= r1.ui; break;
        case I_R_SHIFT: r0.ui >>= r1.ui; break;

        case I_BIT_AND: r0.ui &= r1.ui; break;
        case I_BIT_OR:  r0.ui |= r1.ui; break;
        case I_BIT_XOR: r0.ui ^= r1.ui; break;

        case I_CMP: r0.ui = ((r0.si < r1.si) << 1) | (r0.si == r1.si); break;

        case I_ADDF: r0.df += r1.df; break;
        case I_MULF: r0.df *= r1.df; break;
        case I_DIVF: r0.df /= r1.df; break;

        case I_COUNT: case I_INVALID: assert(0);
        }
    }
}

void
expr_test() {
    char expr_buf [1024];
    MemArena * arena = new_arena(512);
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

        ExprNode * tree = build_expression_tree2(NULL, arena, &tidx);
        if (tree == NULL) {
            printf("invalid symbol\n");
            arrfree(tklist);
            continue;
        }
        uint8_t * procedure = build_expression_procedure2(NULL, tree, NULL);
        RegisterData ret = intro_run_bytecode(procedure, NULL);
        printf(" = %i    (expr size: %i)\n", ret.si, (int)arrlen(procedure));

        reset_arena(arena);
        arrfree(tklist);
        arrfree(procedure);
    }
    free_arena(arena);
}
