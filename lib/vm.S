// NOTE: this doesn't work yet because i don't understand how to make assembly position-independent
    .intel_syntax noprefix
    .global intro__vm
    .text
intro__vm:
    push rbp
    mov  rbp, rsp
    xor  eax, eax
.loop:
    xor  ecx, ecx
    mov  cl, byte ptr [rdi]
    add  rdi, 1

    cmp  cl, 23
    jle  .skip_pop

    mov  rdx, rax
    pop  rax

.skip_pop:
    cmp    cl, 37
    ja     .I_INVALID
    lea    r9, .jmptbl[rip]
    mov    ecx, dword ptr [r9 + rcx*4]
    add    rcx, r9
    jmp    rcx

#define INST(NAME) .long .NAME-.jmptbl
    .align 4
.jmptbl:
    INST(I_INVALID)
    INST(I_RETURN)
    INST(I_LD8)
    INST(I_LD16)
    INST(I_LD32)
    INST(I_LD64)
    INST(I_IMM8)
    INST(I_IMM16)
    INST(I_IMM32)
    INST(I_IMM64)
    INST(I_ZERO)
    INST(I_CND_LD_TOP)
    INST(I_NEGATE_I)
    INST(I_INVALID)
    INST(I_BIT_NOT)
    INST(I_BOOL)
    INST(I_BOOL_NOT)
    INST(I_SETL)
    INST(I_SETE)
    INST(I_SETLE)
    INST(I_CVT_D_TO_I)
    INST(I_CVT_F_TO_I)
    INST(I_CVT_I_TO_D)
    INST(I_CVT_F_TO_D)
    INST(I_ADDI)
    INST(I_MULI)
    INST(I_DIVI)
    INST(I_MODI)
    INST(I_L_SHIFT)
    INST(I_R_SHIFT)
    INST(I_BIT_AND)
    INST(I_BIT_OR)
    INST(I_BIT_XOR)
    INST(I_CMP)
    INST(I_CMP_F)
    INST(I_ADDF)
    INST(I_MULF)
    INST(I_DIVF)
#undef INST
    .text

.I_LD8:
    lea rcx, [rsi + rax]
    xor eax, eax
    mov al, byte ptr [rcx]
    jmp    .loop

.I_LD16:
    lea rcx, [rsi + rax]
    xor eax, eax
    mov ax, word ptr [rcx]
    jmp    .loop

.I_LD32:
    mov eax, dword ptr [rsi + rax]
    jmp    .loop

.I_LD64:
    mov rax, qword ptr [rsi + rax]
    jmp    .loop

.I_IMM8:
    push rax
    xor  eax, eax
    mov  al, byte ptr [rdi]
    add  rdi, 1
    jmp    .loop

.I_IMM16:
    push rax
    xor  eax, eax
    mov  ax, word ptr [rdi]
    add  rdi, 2
    jmp    .loop

.I_IMM32:
    push rax
    mov  eax, dword ptr [rdi]
    add  rdi, 4
    jmp    .loop

.I_IMM64:
    push rax
    mov  rax, qword ptr [rdi]
    add  rdi, 8
    jmp    .loop

.I_ZERO:
    push   rax
    xor    eax, eax
    jmp    .loop

.I_CND_LD_TOP:
    pop    rdx
    pop    rcx
    test   rcx,rcx
    cmovnz rax,rdx
    jmp    .loop

.I_NEGATE_I:
    neg    rax
    jmp    .loop

.I_CMP:
    cmp    rax, rdx
    pushf
    jmp    .loop

.I_SETL:
    xor   eax, eax
    popf
    setl  al
    jmp   .loop

.I_SETE:
    xor   eax, eax
    popf
    sete  al
    jmp   .loop

.I_SETLE:
    xor   eax, eax
    popf
    setle al
    jmp   .loop

.I_BIT_NOT:
    not    rax
    jmp    .loop

.I_BOOL:
    test   rax, rax
    setnz  al
    and    eax, 1
    jmp    .loop

.I_BOOL_NOT:
    test   rax, rax
    setz   al
    and    eax, 1
    jmp    .loop

.I_ADDI:
    add    rax,rdx
    jmp    .loop

.I_MULI:
    imul   rdx
    jmp    .loop

.I_DIVI:
    xchg   rcx, rdx
    xor    edx, edx
    idiv   rcx
    jmp    .loop

.I_MODI:
    xchg   rcx, rdx
    xor    edx, edx
    idiv   rcx
    mov    rax, rdx
    jmp    .loop

.I_L_SHIFT:
    mov    cl,dl
    shl    rax,cl
    jmp    .loop

.I_R_SHIFT:
    mov    cl,dl
    shr    rax,cl
    jmp    .loop

.I_BIT_AND:
    and    rax,rdx
    jmp    .loop

.I_BIT_OR:
    or     rax,rdx
    jmp    .loop

.I_BIT_XOR:
    xor    rax,rdx
    jmp    .loop

.I_CMP_F:
    movq   xmm1, rdx
    comisd xmm0, xmm1
    pushf
    jmp    .loop

.I_CVT_D_TO_I:
    cvtsd2si rax, xmm0
    jmp .loop

.I_CVT_F_TO_I:
    cvtss2si rax, xmm0
    jmp .loop

.I_CVT_I_TO_D:
    cvtsi2sd xmm0, rax
    movq     rax, xmm0
    jmp .loop

.I_CVT_F_TO_D:
    cvtss2sd xmm0, xmm0
    movq     rax, xmm0
    jmp .loop

.I_NEGATE_F:
    movq  xmm1, xmm0
    xorps xmm0, xmm0
    subps xmm0, xmm1
    movq  rax, xmm0
    jmp   .loop

.I_ADDF:
    movq xmm1, rdx
    addps xmm0, xmm1
    movq rax, xmm0
    jmp .loop

.I_MULF:
    movq xmm1, rdx
    mulps xmm0, xmm1
    movq rax, xmm0
    jmp .loop

.I_DIVF:
    movq xmm1, rdx
    divps xmm0, xmm1
    movq rax, xmm0
    jmp .loop

.I_RETURN:
    mov rsp, rbp
    pop rbp
    ret

.I_INVALID:
    sub rsp, 16
    and rsp, -16
#ifdef _WIN32
    lea    rcx, .invalid_msg[rip]
    xor    edx, edx
    mov    dl, byte ptr[rdi - 1]
    call   printf

    mov    rcx, 1
    call   exit
#else
    lea    rdi, .invalid_msg[rip]
    xor    esi, esi
    mov    sil, byte ptr[rdi - 1]
    call   printf

    mov    rdi, 1
    call   exit
#endif
    ret # shouldn't be reached

.invalid_msg:
    .asciz "VM: Invalid bytecode instruction: %i.\n"
