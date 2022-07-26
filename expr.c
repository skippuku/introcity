#include "lexer.c"
#include "global.c"

#define EXPR_BUCKET_CAP (1<<12)

typedef enum {
    OP_LITERAL = 0x00,
    OP_MEMBER,
    OP_INT = OP_LITERAL, // TODO: remove

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
    I_RETURN,

    I_LD_R0,
    I_LD_R1,
    I_CLD_R0,
    I_CLD_R1,
    I_IMM_R0,
    I_IMM_R1,

    I_ADDI,
    I_SUBI,
    I_MULI,
    I_DIVI,
    I_L_SHIFT,
    I_R_SHIFT,

    I_BIT_AND,
    I_BIT_OR,
    I_BIT_XOR,
    I_BIT_NOT,

    I_CMP,

    I_ADDF,
    I_SUBF,
    I_MULF,
    I_DIVF,

    I_CVT_D_TO_I,
    I_CVT_F_TO_I,
    I_CVT_I_TO_D,
    I_CVT_F_TO_D,

    I_PUSH,
    I_POP,
    I_SWAP,
    I_ZERO,

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
    intmax_t value; // TODO: remove
    Token tk;
};

typedef enum {
    REG_VALUE,
    REG_LAST_RESULT,
    REG_POP_STACK,
} ExprInstructionRegisterType;

typedef struct {
    ExprOp op;
    ExprInstructionRegisterType left_type, right_type;
    intmax_t left_value, right_value;
} ExprInstruction;

typedef struct {
    int stack_size;
    int count_instructions;
    ExprInstruction instructions [];
} ExprProcedure;

static void UNUSED
free_expr_context(ExprContext * ectx) {
    free_arena(ectx->arena);
    shfree(ectx->constant_map);
}

ExprNode *
build_expression_tree(ExprContext * ectx, Token * tokens, int count_tokens, Token * o_error_tk) {
    ExprNode * base = NULL;

    int paren_depth = 0;
    ExprNode * node = arena_alloc(ectx->arena, sizeof(*node));
    bool last_was_value = false;
    if (count_tokens == 0) count_tokens = INT32_MAX;
    for (int tk_i=0; tk_i < count_tokens; tk_i++) {
        Token tk = tokens[tk_i];

        switch(tk.type) {
        case TK_END: {
            tk_i = count_tokens; // break loop
            continue;
        }break;

        case TK_L_PARENTHESIS: {
            paren_depth += 1;
            continue;
        }break;

        case TK_R_PARENTHESIS: {
            paren_depth -= 1;
            if (paren_depth < 0) {
                tk_i = count_tokens; // break loop
            }
            continue;
        }break;

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

        case TK_NUMBER: {
            node->value = (intmax_t)strtol(tk.start, NULL, 0);
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
            if (o_error_tk) *o_error_tk = tk;
            return NULL;
        }break;
        }

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
        node = arena_alloc(ectx->arena, sizeof(*node));
    }

    return base;
}

ExprNode *
build_expression_tree2(ParseContext * ctx, MemArena * arena, TokenIndex * tidx) {
    int paren_depth = 0;
    ExprNode * base = NULL;
    ExprNode * node = arena_alloc(arena, sizeof(*base));
    Token tk;
    bool last_was_value = false;
    while (1) {
        bool found_end = false;
        tk = next_token(tidx);
        switch (tk.type) {
        case TK_END: {
            found_end = true;
        }break;

        case TK_L_PARENTHESIS: {
            paren_depth += 1;
        }break;

        case TK_R_PARENTHESIS: {
            paren_depth -= 1;
            if (paren_depth < 0) {
                found_end = true;
            }
        }break;

        case TK_IDENTIFIER: {
            DeclState cast = {.state = DECL_CAST};
            int ret = parse_declaration(ctx, tidx, &cast);
            if (ret == RET_DECL_FINISHED || ret == RET_DECL_CONTINUE) {
                node->op = OP_CAST;
            } else if (ret == RET_NOT_TYPE) {
                if (tk_equal(&tk, "sizeof")) {
                    node->op = OP_SIZEOF;
                } else if (tk_equal(&tk, "_Alignof")) {
                    node->op = OP_ALIGNOF;
                } else {
                    node->op = OP_MEMBER;
                }
            } else {
                return NULL;
            }
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
            parse_error(ctx, tk, "Invalid token.");
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

uint8_t *
build_expression_procedure2(ExprNode * tree, const IntroContainer * cont) {
}

ExprProcedure *
build_expression_procedure(ExprNode * tree) {
    ExprInstruction * list = NULL;

    ExprNode ** node_stack = NULL;
    ExprNode * node = tree;
    int max_stack_size = 0;

    if (!node || node->op == OP_INT) {
        ExprInstruction set = {
            .op = OP_SET,
            .left_type = REG_VALUE,
            .right_type = REG_VALUE,
            .right_value = (node)? node->value : 0,
        };
        arrput(list, set);
        goto post_reverse;
    }

    while (1) {
        assert(node->right != NULL);
        ExprInstruction ins = {0};
        ins.op = node->op;
        bool left_is_op = node->left && node->left->op;
        bool right_is_op = node->right->op;
        bool go_left = false;
        bool take_from_node_stack = false;
        if (node->op == OP_TERNARY_1) {
            (void)arrpop(node_stack);
        }
        if (node->op == OP_TERNARY_2) {
            arrput(node_stack, NULL); // dummy node to get correct stack size
        }
        if (left_is_op) {
            if (right_is_op) {
                ins.left_type = REG_POP_STACK;
                ins.right_type = REG_LAST_RESULT;
                arrput(node_stack, node->left);
            } else {
                ins.left_type = REG_LAST_RESULT;
                ins.right_type = REG_VALUE;
                go_left = true;
            }
        } else {
            if (right_is_op) {
                ins.left_type = REG_VALUE;
                ins.right_type = REG_LAST_RESULT;
            } else {
                ins.left_type = REG_VALUE;
                ins.right_type = REG_VALUE;
                take_from_node_stack = true;
            }
        }
        if (arrlen(node_stack) > max_stack_size) {
            max_stack_size = arrlen(node_stack);
        }

        if (ins.left_type == REG_VALUE) {
            if (node->left) {
                ins.left_value = node->left->value;
            } else {
                ins.left_value = 0;
            }
        }
        if (ins.right_type == REG_VALUE) {
            ins.right_value = node->right->value;
        }
        arrput(list, ins);

        if (take_from_node_stack) {
            if (arrlen(node_stack) > 0) {
                static const ExprInstruction push = {.op = OP_PUSH};
                arrput(list, push);
                node = arrpop(node_stack);
            } else {
                break;
            }
        } else {
            if (go_left) {
                node = node->left;
            } else {
                node = node->right;
            }
        }
    }

    // reverse list
    for (int i=0; i < arrlen(list) / 2; i++) {
        int opposite_index = arrlen(list) - i - 1;
        ExprInstruction temp = list[opposite_index];
        list[opposite_index] = list[i];
        list[i] = temp;
    }

post_reverse: ;
    static const ExprInstruction done = {.op = OP_DONE};
    arrput(list, done);

    ExprProcedure * proc = malloc(sizeof(*proc) + sizeof(*list) * arrlen(list));
    proc->stack_size = max_stack_size,
    proc->count_instructions = arrlen(list),
    memcpy(proc->instructions, list, sizeof(*list) * arrlen(list));

    arrfree(list);

    return proc;
}

intmax_t // TODO: remove
run_expression(ExprProcedure * proc) {
    intmax_t stack [proc->stack_size + 1]; // +1: no undefined behavior
    intmax_t stack_index = 0;
    intmax_t result = 0;
    intmax_t left = 0, right = 0;
    for (int i=0; i < proc->count_instructions; i++) {
        ExprInstruction ins = proc->instructions[i];

        switch(ins.left_type) {
        case REG_VALUE: left = ins.left_value; break;
        case REG_LAST_RESULT: left = result; break;
        case REG_POP_STACK: left = stack[--stack_index]; break;
        }

        switch(ins.right_type) {
        case REG_VALUE: right = ins.right_value; break;
        case REG_LAST_RESULT: right = result; break;
        case REG_POP_STACK: break; // never reached
        }

        switch(ins.op) {
        case OP_DONE: return result;
        case OP_PUSH: stack[stack_index++] = result; break;

        case OP_UNARY_ADD: result = +right; break;
        case OP_UNARY_SUB: result = -right; break;
        case OP_BOOL_NOT:  result = !right; break;
        case OP_BIT_NOT:   result = ~right; break;

        case OP_ADD: result = left + right; break;
        case OP_SUB: result = left - right; break;
        case OP_MUL: result = left * right; break;
        case OP_DIV: result = left / right; break;
        case OP_MOD: result = left % right; break;

        case OP_EQUAL:            result = left == right; break;
        case OP_NOT_EQUAL:        result = left != right; break;
        case OP_LESS:             result = left <  right; break;
        case OP_GREATER:          result = left >  right; break;
        case OP_LESS_OR_EQUAL:    result = left <= right; break;
        case OP_GREATER_OR_EQUAL: result = left >= right; break;
        case OP_BOOL_AND:         result = left && right; break;
        case OP_BOOL_OR:          result = left || right; break;
        case OP_SHIFT_LEFT:       result = left << right; break;
        case OP_SHIFT_RIGHT:      result = left >> right; break;
        
        case OP_BIT_AND: result = left & right; break;
        case OP_BIT_OR:  result = left | right; break;
        case OP_BIT_XOR: result = left ^ right; break;

        case OP_SET: result = right; break;

        case OP_TERNARY_1:
            stack[stack_index++] = !!left;
            result = right;
            break;

        case OP_TERNARY_2:
            result = (stack[--stack_index])? left : right;
            break;

        case OP_INT: break; // never reached
        }
    }

    return result;
}

RegisterData
intro_run_bytecode(uint8_t * code, uint8_t * data) {
    RegisterData stack [1024];
    RegisterData r0, r1;
    size_t stack_idx = 0;
    size_t code_idx = 0;

    while (1) {
        uint8_t byte = code[code_idx++];
        uint8_t inst = byte & ~0xC0;
        uint8_t size = 1 << ((byte >> 6) & 0x03);

        switch ((InstrCode)inst) {
        case I_RETURN: return r0;

        case I_LD_R0: {
            r0.ui = 0;
            memcpy(&r0, data + r0.ui, size);
        }break;

        case I_LD_R1: {
            r1.ui = 0;
            memcpy(&r1, data + r0.ui, size);
        }break;

        case I_CLD_R0: {
            r0.ui = 0;
            if (r1.ui != 0) memcpy(&r0, data + r0.ui, size);
        }break;

        case I_CLD_R1: {
            r1.ui = 0;
            if (r1.ui != 0) memcpy(&r1, data + r0.ui, size);
        }break;

        case I_IMM_R0: {
            r0.ui = 0;
            memcpy(&r0, &code[code_idx], size);
            code_idx += size;
        }break;

        case I_IMM_R1: {
            r1.ui = 0;
            memcpy(&r1, &code[code_idx], size);
            code_idx += size;
        }break;

        case I_ADDI: r0.si += r1.si; break;
        case I_SUBI: r0.si -= r1.si; break;
        case I_MULI: r0.si *= r1.si; break;
        case I_DIVI: {
            int64_t top = r0.si;
            int64_t bot = r1.si;
            r0.si = top / bot;
            r1.si = top % bot;
        }break;

        case I_L_SHIFT: r0.ui <<= r1.ui; break;
        case I_R_SHIFT: r0.ui >>= r1.ui; break;

        case I_BIT_AND: r0.ui &= r1.ui; break;
        case I_BIT_OR:  r0.ui |= r1.ui; break;
        case I_BIT_XOR: r0.ui ^= r1.ui; break;
        case I_BIT_NOT: r0.ui = ~r0.ui; break;

        case I_CMP: r0.ui = ((r0.si < r1.si) << 1) | (r0.si == r1.si); break;

        case I_ADDF: r0.df += r1.df; break;
        case I_SUBF: r0.df -= r1.df; break;
        case I_MULF: r0.df *= r1.df; break;
        case I_DIVF: r0.df /= r1.df; break;

        case I_CVT_D_TO_I: r0.si = (ssize_t)r0.df; break;
        case I_CVT_F_TO_I: r0.si = (ssize_t)r0.sf; break;
        case I_CVT_I_TO_D: r0.df = (double) r0.si; break;
        case I_CVT_F_TO_D: r0.df = (double) r0.sf; break;

        case I_PUSH: {
            assert(stack_idx < LENGTH(stack));
            stack[stack_idx++] = r0;
        }break;

        case I_POP: {
            assert(stack_idx > 0);
            r0 = stack[--stack_idx];
        }break;

        case I_SWAP: {
            RegisterData temp = r0;
            r0 = r1;
            r1 = temp;
        }break;

        case I_ZERO: {
            r0.ui = 0;
        }break;

        default: assert(0);
        }
    }
}
