    .intel_syntax noprefix
    .global intro__vm
    .text
intro__vm:
    push rbp
    mov  rbp, rsp
    xor  rax,rax
.loop:
    mov  r8b, byte ptr [rdi]
    add  rdi, 1

    mov  cl, r8b
    and  rcx, 0x3f
    shr  r8b, 6

    cmp  cl, 0xd
    jle  .skip_pop

    mov  rdx, rax
    pop  rax

.skip_pop:
    cmp    cl, 23
    jg     .I_INVALID
    mov    rcx, .jmptbl[0 + rcx*8]
    jmp    rcx

#define INST(name) .quad .name
    .align 8
.jmptbl:
    INST(I_INVALID)
    INST(I_RETURN)
    INST(I_LD)
    INST(I_IMM)
    INST(I_ZERO)
    INST(I_CND_LD_TOP)
    INST(I_NEGATE_I)
    INST(I_INVALID)
    INST(I_BIT_NOT)
    INST(I_NOT_ZERO)
    INST(I_INVALID)
    INST(I_INVALID)
    INST(I_INVALID)
    INST(I_INVALID)
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
#undef INST
    .text

.I_LD:
    lea rcx, [rsi + rax]
    xor rax, rax
    mov al, byte ptr [rcx]

    cmp r8b, 1
    jb .loop
    mov ax, word ptr [rcx]

    cmp r8b, 2
    jb .loop
    mov eax, dword ptr [rcx]

    je .loop
    mov rax, qword ptr [rcx]
    jmp .loop

.I_IMM:
    push   rax
    xor rax, rax

    mov al, byte ptr [rdi]
    lea rdi, [rdi + 1]

    cmp r8b, 1
    jb .loop

    mov ah, byte ptr [rdi]
    lea rdi, [rdi + 1]

    cmp r8b, 2
    jb .loop

    mov cl,  byte ptr [rdi + 0]
    mov ch,  byte ptr [rdi + 1]
    shl ecx, 16
    or  eax, ecx
    lea rdi, [rdi + 2]

    cmp r8b, 3
    jb .loop

    mov cl, byte ptr [rdi + 2]
    mov ch, byte ptr [rdi + 3]
    shl ecx, 16
    mov al, byte ptr [rdi + 0]
    mov ah, byte ptr [rdi + 1]
    shl rcx, 32
    or  rax, rcx
    lea rdi, [rdi + 4]
    jmp .loop

.I_ZERO:
    push   rax
    xor    rax,rax
    jmp    .loop

.I_CND_LD_TOP:
    pop    rdx
    pop    r8
    test   r8,r8
    cmovnz rax,rdx
    jmp    .loop

.I_NEGATE_I:
    neg    rax
    jmp    .loop

.I_BIT_NOT:
    not    rax
    jmp    .loop

.I_NOT_ZERO:
    xor    rdx, rdx
    test   rax,rax
    setnz  dl
    mov    rax, rdx
    jmp    .loop

.I_ADDI:
    add    rax,rdx
    jmp    .loop

.I_MULI:
    imul   rdx
    jmp    .loop

.I_DIVI:
    xchg   rcx, rdx
    xor    rdx, rdx
    idiv   rcx
    jmp    .loop

.I_MODI:
    xchg   rcx, rdx
    xor    rdx, rdx
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

.I_CMP:
    cmp    rax, rdx
    mov    rax, 0
    setl   al
    sete   cl
    shl    al, 1
    or     al, cl
    jmp    .loop

.I_RETURN:
    mov rsp, rbp
    pop rbp
    ret

.I_INVALID:
    sub rsp, 16
    and rsp, -16
#ifdef _WIN32
    lea    rcx, .invalid_msg
    xor    rdx, rdx
    mov    dl, byte ptr[rdi - 1]
    call   printf

    mov    rcx, 1
    call   exit
#else
    lea    rdi, .invalid_msg
    xor    rsi, rsi
    mov    sil, byte ptr[rdi - 1]
    call   printf

    mov    rdi, 1
    call   exit
#endif

.invalid_msg:
    .asciz "Invalid instruction: %i.\n"
