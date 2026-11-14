#include "../include/codegen.h"
#include <sstream>
#include <cmath>

namespace quartz {

using namespace ast;

// ============================================================
//  CODEGEN CONSTRUCTOR
// ============================================================
CodeGen::CodeGen(AsmTarget target) : target_(target) {}

AsmTarget CodeGen::detect_target() noexcept {
    // Runtime detection via uname — critical for cross-compiled binaries
    // e.g. binary built on x86_64 but running on ARM64 (Recian)
    FILE* p = ::popen("uname -m 2>/dev/null", "r");
    if (p) {
        char buf[64] = {};
        if (::fgets(buf, sizeof(buf)-1, p)) {
            ::pclose(p);
            std::string arch(buf);
            // Strip trailing whitespace/newline
            while (!arch.empty() &&
                   (arch.back()=='\n'||arch.back()=='\r'||arch.back()==' '))
                arch.pop_back();
            if (arch == "aarch64" || arch == "arm64" ||
                arch == "armv8"   || arch == "armv9")
                return AsmTarget::ARM64_V8A;
            return AsmTarget::X86_64;
        }
        ::pclose(p);
    }
    // Fallback: compile-time macro
#if defined(QUARTZ_ARCH_ARM64)
    return AsmTarget::ARM64_V8A;
#else
    return AsmTarget::X86_64;
#endif
}

std::string CodeGen::error_summary() const {
    std::string s;
    for (const auto& e : errors_) s += e + "\n";
    return s;
}

// ============================================================
//  LABEL / STRING POOL
// ============================================================
std::string CodeGen::new_label(const char* prefix) {
    return std::string(prefix) + std::to_string(label_cnt_++);
}

std::string CodeGen::string_label(const std::string& s) {
    std::string lbl = "__qzstr_" + std::to_string(string_cnt_++);
    if (target_ == AsmTarget::X86_64) {
        rodata_sect_ << "section .rodata\n";
        rodata_sect_ << lbl << ": db `" << s << "`, 0\n";
        rodata_sect_ << lbl << "_len: equ $ - " << lbl << "\n";
    } else {
        // ARM64 GAS - goes into .rodata
        rodata_sect_ << "\n    .section .rodata\n";
        rodata_sect_ << "    .align 3\n";
        rodata_sect_ << lbl << ":\n";
        rodata_sect_ << "    .asciz \"" << s << "\"\n";
        rodata_sect_ << "    .equ " << lbl << "_len, . - " << lbl << "\n";
    }
    return lbl;
}

// ============================================================
//  SYSCALL NUMBERS
// ============================================================
int64_t CodeGen::syscall_write()          const noexcept { return target_ == AsmTarget::X86_64 ? 1 : 64; }
int64_t CodeGen::syscall_read()           const noexcept { return target_ == AsmTarget::X86_64 ? 0 : 63; }
int64_t CodeGen::syscall_exit()           const noexcept { return target_ == AsmTarget::X86_64 ? 60 : 93; }
int64_t CodeGen::syscall_mmap()           const noexcept { return target_ == AsmTarget::X86_64 ? 9 : 222; }
int64_t CodeGen::syscall_munmap()         const noexcept { return target_ == AsmTarget::X86_64 ? 11 : 215; }
int64_t CodeGen::syscall_open()           const noexcept { return target_ == AsmTarget::X86_64 ? 2 : 56; }
int64_t CodeGen::syscall_close()          const noexcept { return target_ == AsmTarget::X86_64 ? 3 : 57; }
int64_t CodeGen::syscall_clock_nanosleep()const noexcept { return target_ == AsmTarget::X86_64 ? 230 : 115; }

// ============================================================
//  GENERATE ENTRY POINT
// ============================================================
std::string CodeGen::generate(const ProgramNode& program) {
    emit_preamble();
    emit_data_section();   // fills data_sect_ and bss_sect_
    emit_text_section();   // fills text_sect_ with print/str helpers
    gen_program(program);  // appends _start + user code into text_sect_
    emit_epilogue();       // ← MUST be called here: appends runtime helpers

    // Assemble final output — section ordering matters!
    std::ostringstream final_out;
    if (target_ == AsmTarget::X86_64) {
        // NASM: rodata → data → bss → text
        final_out << out_.str();
        final_out << rodata_sect_.str();
        final_out << data_sect_.str();
        final_out << bss_sect_.str();
        final_out << text_sect_.str();
    } else {
        // ARM64 GAS: text first (_start + helpers), then data/rodata/bss
        final_out << text_sect_.str();
        final_out << rodata_sect_.str();
        final_out << data_sect_.str();
        final_out << bss_sect_.str();
    }
    return final_out.str();
}

// ============================================================
//  PREAMBLE
// ============================================================
void CodeGen::emit_preamble() {
    if (target_ == AsmTarget::X86_64) {
        out_ << "; Quartz Language - Generated NASM Assembly (x86_64 Linux)\n";
        out_ << "; Architecture: x86_64\n";
        out_ << "bits 64\n";
        out_ << "default rel\n\n";
        out_ << "section .rodata\n";
        out_ << "__qz_newline: db 10\n";
        out_ << "__qz_newline_len: equ 1\n";
        out_ << "__qz_true_str: db \"true\", 0\n";
        out_ << "__qz_false_str: db \"false\", 0\n";
        out_ << "__qz_nil_str: db \"nil\", 0\n";
        out_ << "__qz_nan_str: db \"NaN\", 0\n\n";
    } else {
        // ARM64 GAS - rodata section for string literals
        // Note: section .text with _start MUST come before data sections
        // We build text first, then data, so preamble is empty here.
        // String literals go into rodata_sect_ via string_label()
        out_ << "";  // intentionally empty - ARM64 uses different ordering
    }
}

void CodeGen::emit_data_section() {
    if (target_ == AsmTarget::X86_64) {
        data_sect_ << "\nsection .data\n";
        bss_sect_  << "\nsection .bss\n";
        bss_sect_ << "__qz_print_buf: resb 128\n";
        bss_sect_ << "__qz_int_buf: resb 32\n";
    } else {
        // ARM64 GAS: .bss section
        bss_sect_  << "\n    .section .bss\n";
        bss_sect_  << "    .align 3\n";
        bss_sect_  << "__qz_print_buf:\n    .space 128\n";
        bss_sect_  << "__qz_int_buf:\n    .space 32\n";
        // .rodata section for string literals
        data_sect_ << "\n    .section .rodata\n";
        data_sect_ << "    .align 3\n";
        data_sect_ << "__qz_newline:\n    .byte 10\n";
        data_sect_ << "__qz_true_str:\n    .asciz \"true\"\n";
        data_sect_ << "__qz_false_str:\n    .asciz \"false\"\n";
        data_sect_ << "__qz_nil_str:\n    .asciz \"nil\"\n";
        data_sect_ << "__qz_nan_str:\n    .asciz \"NaN\"\n";
    }
}

void CodeGen::emit_text_section() {
    if (target_ == AsmTarget::X86_64) {
        text_sect_ << "\nsection .text\n";
        text_sect_ << "global _start\n\n";
        // x86_64 helpers remain unchanged below

        // __qz_print_int: prints integer in rdi
        text_sect_ <<
        "__qz_print_int:\n"
        "    push rbp\n"
        "    mov rbp, rsp\n"
        "    push rbx\n"
        "    push r12\n"
        "    push r13\n"
        "    lea r12, [__qz_int_buf + 31]\n"
        "    mov byte [r12], 10\n"      // newline
        "    mov r13, 1\n"
        "    test rdi, rdi\n"
        "    jns .positive\n"
        "    neg rdi\n"
        "    mov byte [r12-1], 45\n"    // '-'
        "    dec r12\n"
        "    inc r13\n"
        ".positive:\n"
        "    mov rax, rdi\n"
        "    mov rbx, 10\n"
        ".loop:\n"
        "    xor rdx, rdx\n"
        "    div rbx\n"
        "    add dl, 48\n"
        "    dec r12\n"
        "    mov [r12], dl\n"
        "    inc r13\n"
        "    test rax, rax\n"
        "    jnz .loop\n"
        "    mov rax, 1\n"              // sys_write
        "    mov rdi, 1\n"             // stdout
        "    mov rsi, r12\n"
        "    mov rdx, r13\n"
        "    syscall\n"
        "    pop r13\n"
        "    pop r12\n"
        "    pop rbx\n"
        "    pop rbp\n"
        "    ret\n\n";

        // __qz_print_str: prints null-terminated string in rdi
        text_sect_ <<
        "__qz_print_str:\n"
        "    push rbp\n"
        "    mov rbp, rsp\n"
        "    push rbx\n"
        "    mov rbx, rdi\n"
        "    xor rdx, rdx\n"
        ".strlen_loop:\n"
        "    cmp byte [rbx + rdx], 0\n"
        "    je .strlen_done\n"
        "    inc rdx\n"
        "    jmp .strlen_loop\n"
        ".strlen_done:\n"
        "    test rdx, rdx\n"
        "    jz .skip_write\n"
        "    mov rax, 1\n"             // sys_write
        "    mov rdi, 1\n"             // stdout
        "    mov rsi, rbx\n"
        "    syscall\n"
        ".skip_write:\n"
        "    ; print newline\n"
        "    mov rax, 1\n"
        "    mov rdi, 1\n"
        "    lea rsi, [__qz_newline]\n"
        "    mov rdx, 1\n"
        "    syscall\n"
        "    pop rbx\n"
        "    pop rbp\n"
        "    ret\n\n";

    } else {
        // ARM64 GAS - correct format for aarch64-linux-gnu-as
        text_sect_ << "\n    .text\n";
        text_sect_ << "    .arch armv8-a\n";
        text_sect_ << "    .global _start\n";
        text_sect_ << "    .type _start, %function\n\n";

        // __qz_print_int: prints signed integer in x0
        text_sect_ <<
        "    .global __qz_print_int\n"
        "    .type __qz_print_int, %function\n"
        "__qz_print_int:\n"
        "    stp x29, x30, [sp, #-64]!\n"
        "    mov x29, sp\n"
        "    stp x19, x20, [sp, #16]\n"
        "    stp x21, x22, [sp, #32]\n"
        "    stp x23, x24, [sp, #48]\n"
        "    // x19 = buf pointer, x20 = end of buf, x21 = length, x22 = value\n"
        "    adrp x19, __qz_int_buf\n"
        "    add x19, x19, :lo12:__qz_int_buf\n"
        "    add x20, x19, #31\n"
        "    mov w23, #10\n"
        "    strb w23, [x20]\n"
        "    mov x21, #1\n"
        "    mov x22, x0\n"
        "    tst x22, x22\n"
        "    bpl .Lprint_pos\n"
        "    neg x22, x22\n"
        "    mov w23, #45\n"
        "    sub x20, x20, #1\n"
        "    strb w23, [x20]\n"
        "    add x21, x21, #1\n"
        ".Lprint_pos:\n"
        "    mov x1, #10\n"
        ".Lprint_loop:\n"
        "    udiv x2, x22, x1\n"
        "    msub x3, x2, x1, x22\n"
        "    add w3, w3, #48\n"
        "    sub x20, x20, #1\n"
        "    strb w3, [x20]\n"
        "    add x21, x21, #1\n"
        "    mov x22, x2\n"
        "    cbnz x22, .Lprint_loop\n"
        "    mov x8, #64\n"
        "    mov x0, #1\n"
        "    mov x1, x20\n"
        "    mov x2, x21\n"
        "    svc #0\n"
        "    ldp x23, x24, [sp, #48]\n"
        "    ldp x21, x22, [sp, #32]\n"
        "    ldp x19, x20, [sp, #16]\n"
        "    ldp x29, x30, [sp], #64\n"
        "    ret\n\n";

        // __qz_print_str: prints null-terminated string in x0, then newline
        text_sect_ <<
        "    .global __qz_print_str\n"
        "    .type __qz_print_str, %function\n"
        "__qz_print_str:\n"
        "    stp x29, x30, [sp, #-32]!\n"
        "    mov x29, sp\n"
        "    stp x19, x20, [sp, #16]\n"
        "    mov x19, x0\n"
        "    // compute strlen\n"
        "    mov x20, #0\n"
        ".Lstrlen_loop:\n"
        "    ldrb w2, [x19, x20]\n"
        "    cbz w2, .Lstrlen_done\n"
        "    add x20, x20, #1\n"
        "    b .Lstrlen_loop\n"
        ".Lstrlen_done:\n"
        "    cbz x20, .Lskip_str\n"
        "    mov x8, #64\n"
        "    mov x0, #1\n"
        "    mov x1, x19\n"
        "    mov x2, x20\n"
        "    svc #0\n"
        ".Lskip_str:\n"
        "    // print newline\n"
        "    adrp x1, __qz_newline\n"
        "    add x1, x1, :lo12:__qz_newline\n"
        "    mov x8, #64\n"
        "    mov x0, #1\n"
        "    mov x2, #1\n"
        "    svc #0\n"
        "    ldp x19, x20, [sp, #16]\n"
        "    ldp x29, x30, [sp], #32\n"
        "    ret\n\n";
    }
}

// ============================================================
//  RUNTIME HELPERS — written directly, no static cache
//  (static cache caused cross-arch bugs when binary runs on
//   different arch than it was compiled on)
// ============================================================

void CodeGen::emit_epilogue() {
    if (target_ == AsmTarget::X86_64) {
        text_sect_ <<
        "\n; === Quartz Runtime Helpers (x86_64) ===\n\n"
        "__qz_wait:\n"
        "    push rbp\n    mov rbp, rsp\n    sub rsp, 32\n"
        "    xor rax, rax\n    mov [rsp], rax\n"
        "    imul rdi, rdi, 1000000\n    mov [rsp+8], rdi\n"
        "    mov rax, 35\n    mov rdi, rsp\n    xor rsi, rsi\n    syscall\n"
        "    mov rsp, rbp\n    pop rbp\n    ret\n\n"
        "__qz_split:\n    mov rax, rdi\n    ret\n\n"
        "__qz_point:\n    mov rax, rdi\n    ret\n\n"
        "__qz_gcd:\n"
        "    mov rax, rdi\n    mov rcx, rsi\n"
        ".Lgcd_lp:\n    test rcx, rcx\n    jz .Lgcd_dn\n"
        "    xor rdx, rdx\n    div rcx\n    mov rax, rcx\n    mov rcx, rdx\n    jmp .Lgcd_lp\n"
        ".Lgcd_dn:\n    ret\n\n"
        "__qz_factorial:\n"
        "    mov rax, 1\n    cmp rdi, 0\n    jle .Lft_dn\n    mov rcx, rdi\n"
        ".Lft_lp:\n    imul rax, rcx\n    dec rcx\n    jnz .Lft_lp\n"
        ".Lft_dn:\n    ret\n\n"
        "__qz_fibonacci:\n"
        "    cmp rdi, 0\n    je .Lfb0\n    cmp rdi, 1\n    je .Lfb1\n"
        "    mov r8, 0\n    mov r9, 1\n    mov rcx, rdi\n    dec rcx\n"
        ".Lfb_lp:\n    mov rax, r8\n    add rax, r9\n    mov r8, r9\n    mov r9, rax\n"
        "    dec rcx\n    jnz .Lfb_lp\n    mov rax, r9\n    ret\n"
        ".Lfb0:\n    xor rax, rax\n    ret\n"
        ".Lfb1:\n    mov rax, 1\n    ret\n\n"
        "__qz_isprime:\n"
        "    cmp rdi, 2\n    jl .Lpr_no\n    je .Lpr_yes\n"
        "    test rdi, 1\n    jz .Lpr_no\n    mov rcx, 3\n"
        ".Lpr_lp:\n    mov rax, rcx\n    imul rax, rcx\n    cmp rax, rdi\n    jg .Lpr_yes\n"
        "    mov rax, rdi\n    xor rdx, rdx\n    div rcx\n    test rdx, rdx\n    jz .Lpr_no\n"
        "    add rcx, 2\n    jmp .Lpr_lp\n"
        ".Lpr_yes:\n    mov rax, 1\n    ret\n"
        ".Lpr_no:\n    xor rax, rax\n    ret\n\n"
        "__qz_math_stub:\n    xor rax, rax\n    ret\n\n"
        "__qz_lerp_stub:\n    mov rax, rdi\n    ret\n\n"
        "__qz_hw_dispatch:\n    xor rax, rax\n    ret\n\n"
        "__qz_gpu_render_stub:\n    xor rax, rax\n    ret\n\n"
        "__qz_mem_alloc:\n"
        "    mov rsi, rdi\n    xor rdi, rdi\n    mov rdx, 3\n    mov r10, 34\n"
        "    mov r8, -1\n    xor r9, r9\n    mov rax, 9\n    syscall\n    ret\n\n"
        "__qz_split_internal:\n    xor rax, rax\n    ret\n\n"
        "__reverse__:\n    xor rax, rax\n    ret\n\n"
        "__from__:\n    xor rax, rax\n    ret\n\n";
    } else {
        // ARM64 GAS helpers — unique labels per file to avoid multi-file conflicts
        text_sect_ <<
        "\n// === Quartz Runtime Helpers (ARM64) ===\n\n"
        "    .global __qz_wait\n"
        "    .type __qz_wait, %function\n"
        "__qz_wait:\n"
        "    stp x29, x30, [sp, #-48]!\n"
        "    mov x29, sp\n"
        "    str xzr, [sp, #16]\n"
        "    mov x1, #15259\n"           // 1000000 = 0xF4240, build with movk
        "    movk x1, #0xf, lsl #16\n"
        "    mul x0, x0, x1\n"
        "    str x0, [sp, #24]\n"
        "    mov x8, #101\n"
        "    add x0, sp, #16\n"
        "    mov x1, #0\n"
        "    svc #0\n"
        "    ldp x29, x30, [sp], #48\n"
        "    ret\n\n"
        "    .global __qz_split\n"
        "    .type __qz_split, %function\n"
        "__qz_split:\n"
        "    ret\n\n"
        "    .global __qz_point\n"
        "    .type __qz_point, %function\n"
        "__qz_point:\n"
        "    ret\n\n"
        "    .global __qz_gcd\n"
        "    .type __qz_gcd, %function\n"
        "__qz_gcd:\n"
        "    cbz x1, .Lqz_gcd_done\n"
        ".Lqz_gcd_loop:\n"
        "    udiv x2, x0, x1\n"
        "    msub x2, x2, x1, x0\n"
        "    mov x0, x1\n"
        "    mov x1, x2\n"
        "    cbnz x1, .Lqz_gcd_loop\n"
        ".Lqz_gcd_done:\n"
        "    ret\n\n"
        "    .global __qz_factorial\n"
        "    .type __qz_factorial, %function\n"
        "__qz_factorial:\n"
        "    stp x29, x30, [sp, #-16]!\n"
        "    mov x29, sp\n"
        "    mov x2, x0\n"
        "    mov x0, #1\n"
        "    cmp x2, #0\n"
        "    ble .Lqz_fact_done\n"
        ".Lqz_fact_loop:\n"
        "    mul x0, x0, x2\n"
        "    sub x2, x2, #1\n"
        "    cbnz x2, .Lqz_fact_loop\n"
        ".Lqz_fact_done:\n"
        "    ldp x29, x30, [sp], #16\n"
        "    ret\n\n"
        "    .global __qz_fibonacci\n"
        "    .type __qz_fibonacci, %function\n"
        "__qz_fibonacci:\n"
        "    stp x29, x30, [sp, #-16]!\n"
        "    mov x29, sp\n"
        "    cmp x0, #0\n"
        "    beq .Lqz_fib_zero\n"
        "    cmp x0, #1\n"
        "    beq .Lqz_fib_one\n"
        "    mov x1, #0\n"
        "    mov x2, #1\n"
        "    mov x3, x0\n"
        "    sub x3, x3, #1\n"
        ".Lqz_fib_loop:\n"
        "    add x4, x1, x2\n"
        "    mov x1, x2\n"
        "    mov x2, x4\n"
        "    sub x3, x3, #1\n"
        "    cbnz x3, .Lqz_fib_loop\n"
        "    mov x0, x2\n"
        "    ldp x29, x30, [sp], #16\n"
        "    ret\n"
        ".Lqz_fib_zero:\n"
        "    mov x0, #0\n"
        "    ldp x29, x30, [sp], #16\n"
        "    ret\n"
        ".Lqz_fib_one:\n"
        "    mov x0, #1\n"
        "    ldp x29, x30, [sp], #16\n"
        "    ret\n\n"
        "    .global __qz_isprime\n"
        "    .type __qz_isprime, %function\n"
        "__qz_isprime:\n"
        "    cmp x0, #2\n"
        "    blt .Lqz_prime_no\n"
        "    beq .Lqz_prime_yes\n"
        "    tst x0, #1\n"
        "    beq .Lqz_prime_no\n"
        "    mov x1, #3\n"
        ".Lqz_prime_loop:\n"
        "    mul x2, x1, x1\n"
        "    cmp x2, x0\n"
        "    bgt .Lqz_prime_yes\n"
        "    udiv x2, x0, x1\n"
        "    msub x2, x2, x1, x0\n"
        "    cbz x2, .Lqz_prime_no\n"
        "    add x1, x1, #2\n"
        "    b .Lqz_prime_loop\n"
        ".Lqz_prime_yes:\n"
        "    mov x0, #1\n"
        "    ret\n"
        ".Lqz_prime_no:\n"
        "    mov x0, #0\n"
        "    ret\n\n"
        "    .global __qz_math_stub\n"
        "    .type __qz_math_stub, %function\n"
        "__qz_math_stub:\n"
        "    mov x0, #0\n"
        "    ret\n\n"
        "    .global __qz_lerp_stub\n"
        "    .type __qz_lerp_stub, %function\n"
        "__qz_lerp_stub:\n"
        "    ret\n\n"
        "    .global __qz_hw_dispatch\n"
        "    .type __qz_hw_dispatch, %function\n"
        "__qz_hw_dispatch:\n"
        "    mov x0, #0\n"
        "    ret\n\n"
        "    .global __qz_gpu_render_stub\n"
        "    .type __qz_gpu_render_stub, %function\n"
        "__qz_gpu_render_stub:\n"
        "    mov x0, #0\n"
        "    ret\n\n"
        "    .global __qz_mem_alloc\n"
        "    .type __qz_mem_alloc, %function\n"
        "__qz_mem_alloc:\n"
        "    stp x29, x30, [sp, #-16]!\n"
        "    mov x29, sp\n"
        "    mov x1, x0\n"
        "    mov x0, #0\n"
        "    mov x2, #3\n"
        "    mov x3, #34\n"
        "    mov x4, #-1\n"
        "    mov x5, #0\n"
        "    mov x8, #222\n"
        "    svc #0\n"
        "    ldp x29, x30, [sp], #16\n"
        "    ret\n\n"
        "    .global __reverse__\n"
        "    .type __reverse__, %function\n"
        "__reverse__:\n"
        "    mov x0, #0\n"
        "    ret\n\n"
        "    .global __from__\n"
        "    .type __from__, %function\n"
        "__from__:\n"
        "    mov x0, #0\n"
        "    ret\n\n";
    }
}

// ============================================================
//  x86_64 Runtime Helpers (NASM)
// ============================================================

// ============================================================
//  NODE DISPATCH
// ============================================================
void CodeGen::gen_node(const Node& node) {
    switch (node.type) {
        case NodeType::PROGRAM:      gen_program(static_cast<const ProgramNode&>(node)); break;
        case NodeType::MAIN_DECL:    gen_main_decl(static_cast<const MainDeclNode&>(node)); break;
        case NodeType::FUNC_DECL:    gen_func_decl(static_cast<const FuncDeclNode&>(node)); break;
        case NodeType::VAR_DECL:     gen_var_decl(static_cast<const VarDeclNode&>(node)); break;
        case NodeType::PRINT_STMT:   gen_print(static_cast<const PrintNode&>(node)); break;
        case NodeType::UTIL_STMT:    gen_util(static_cast<const UtilNode&>(node)); break;
        case NodeType::IF_STMT:      gen_if(static_cast<const IfNode&>(node)); break;
        case NodeType::CALL_EXPR:    gen_call(static_cast<const CallNode&>(node)); break;
        case NodeType::MOV_STMT:     gen_mov(static_cast<const MovNode&>(node)); break;
        case NodeType::ADD_STMT:     gen_add(static_cast<const AddNode&>(node)); break;
        case NodeType::WAIT_STMT:    gen_wait(static_cast<const WaitNode&>(node)); break;
        case NodeType::SPLIT_STMT:   gen_split(static_cast<const SplitNode&>(node)); break;
        case NodeType::POINT_STMT:   gen_point(static_cast<const PointNode&>(node)); break;
        case NodeType::SAVE_STMT:    gen_save(static_cast<const SaveNode&>(node)); break;
        case NodeType::RETURN_STMT:  gen_return(static_cast<const ReturnNode&>(node)); break;
        case NodeType::MATH_EXPR:    gen_math(static_cast<const MathNode&>(node)); break;
        case NodeType::GPU_RENDER_STMT: gen_gpu_render(static_cast<const GpuRenderNode&>(node)); break;
        case NodeType::HW_IO_STMT:   gen_hw_io(static_cast<const HwIoNode&>(node)); break;
        case NodeType::ASM_STMT:     gen_asm(static_cast<const AsmNode&>(node)); break;
        case NodeType::MEM_ALLOC_STMT: gen_mem_alloc(static_cast<const MemAllocNode&>(node)); break;
        default: break;
    }
}

void CodeGen::gen_program(const ProgramNode& node) {
    for (const auto& stmt : node.body) {
        if (stmt) gen_node(*stmt);
    }
}

void CodeGen::gen_main_decl(const MainDeclNode& node) {
    emit_label("_start");

    // Push fresh frame for main scope
    frame_stack_.push_back(frame_);
    frame_ = StackFrame{};

    static constexpr int MAIN_FRAME = 512;

    if (target_ == AsmTarget::X86_64) {
        emit("push rbp");
        emit("mov rbp, rsp");
        emit("sub rsp, " + std::to_string(MAIN_FRAME));
    } else {
        emit("stp x29, x30, [sp, #-" + std::to_string(MAIN_FRAME) + "]!");
        emit("mov x29, sp");
    }

    for (const auto& stmt : node.body) {
        if (stmt) gen_node(*stmt);
    }

    if (!frame_stack_.empty()) {
        frame_ = frame_stack_.back();
        frame_stack_.pop_back();
    }

    emit_syscall_exit(0);
}

void CodeGen::gen_func_decl(const FuncDeclNode& node) {
    func_labels_[node.name] = node.name;

    // Jump OVER the function body so execution doesn't fall into it
    std::string skip_lbl = new_label(".Lskip_fn_");
    emit_jmp(skip_lbl);

    if (target_ != AsmTarget::X86_64) {
        text_sect_ << "    .global " << node.name << "\n";
        text_sect_ << "    .type " << node.name << ", %function\n";
    }
    emit_label(node.name);

    // Push fresh isolated frame for this function
    frame_stack_.push_back(frame_);
    frame_ = StackFrame{};

    static constexpr int FUNC_FRAME = 384;
    emit_func_prologue(node.name, FUNC_FRAME);

    // Store parameters
    for (size_t i = 0; i < node.params.size() && i < 8; ++i) {
        int off = frame_.alloc_local(node.params[i], 8);
        if (target_ == AsmTarget::X86_64) {
            const char* pregs[] = {"rdi","rsi","rdx","rcx","r8","r9","r10","r11"};
            std::string sign = (off >= 0) ? "+" : "";
            emit("mov qword [rbp" + sign + std::to_string(off) + "], " + std::string(pregs[i]));
        } else {
            emit_frame_store("x" + std::to_string(i), off);
        }
    }

    for (const auto& stmt : node.body) {
        if (stmt) gen_node(*stmt);
    }

    // Default return 0 if no explicit return
    if (target_ == AsmTarget::X86_64) emit("xor rax, rax");
    else                               emit("mov x0, #0");
    emit_func_epilogue_sized(FUNC_FRAME);
    emit_ret();

    // Label after function — execution continues here
    emit_label(skip_lbl);

    // Pop frame state
    if (!frame_stack_.empty()) {
        frame_ = frame_stack_.back();
        frame_stack_.pop_back();
    }
}

void CodeGen::gen_var_decl(const VarDeclNode& node) {
    int off = frame_.alloc_local(node.name, 8);
    if (node.init) {
        std::string reg = gen_expr(*node.init);
        if (target_ == AsmTarget::X86_64) {
            std::string sign = (off >= 0) ? "+" : "";
            emit("mov qword [rbp" + sign + std::to_string(off) + "], " + reg);
        } else {
            emit_frame_store(reg, off);
        }
    }
}

void CodeGen::gen_print(const PrintNode& node) {
    // Detect if the expression is a string literal
    bool is_str = false;
    if (node.expr->type == NodeType::LITERAL_EXPR) {
        const auto& lit = static_cast<const LiteralNode&>(*node.expr);
        is_str = lit.value.is_string();
    }

    std::string reg = gen_expr(*node.expr);

    if (target_ == AsmTarget::X86_64) {
        if (reg != "rdi") emit("mov rdi, " + reg);
        emit(is_str ? "call __qz_print_str" : "call __qz_print_int");
    } else {
        // ARM64: gen_expr for string returns "x0" (adrp+add already done)
        if (reg != "x0") emit("mov x0, " + reg);
        emit(is_str ? "bl __qz_print_str" : "bl __qz_print_int");
    }
}

void CodeGen::gen_util(const UtilNode& node) {
    // util:: just evaluates expression and loads result
    std::string reg = gen_expr(*node.expr);
    // Result stays in register (can be used by surrounding code)
    (void)reg;
}

void CodeGen::gen_if(const IfNode& node) {
    std::string else_lbl = new_label(".Lelse");
    std::string end_lbl  = new_label(".Lend");

    std::string cond_reg = gen_expr(*node.condition);
    if (target_ == AsmTarget::X86_64) {
        emit("test " + cond_reg + ", " + cond_reg);
        emit("jz " + else_lbl);
    } else {
        emit("cbz " + cond_reg + ", " + else_lbl);
    }

    for (const auto& s : node.then_body) if (s) gen_node(*s);

    if (!node.else_body.empty()) {
        emit_jmp(end_lbl);
        emit_label(else_lbl);
        for (const auto& s : node.else_body) if (s) gen_node(*s);
    } else {
        emit_label(else_lbl);
    }
    emit_label(end_lbl);
}

void CodeGen::gen_call(const CallNode& node) {
    size_t nargs = std::min(node.args.size(), size_t(8));

    if (nargs == 0) {
        emit_call(node.callee);
        return;
    }

    // Evaluate all args and push to stack to avoid register clobber
    if (target_ == AsmTarget::X86_64) {
        // Evaluate each arg → push on stack (in reverse order keeps ABI)
        for (int i = static_cast<int>(nargs) - 1; i >= 0; --i) {
            std::string r = gen_expr(*node.args[static_cast<size_t>(i)]);
            if (r != "rax") emit("mov rax, " + r);
            emit("push rax");
        }
        // Pop into correct param registers (left to right)
        const char* pregs[] = {"rdi","rsi","rdx","rcx","r8","r9","r10","r11"};
        for (size_t i = 0; i < nargs; ++i)
            emit("pop " + std::string(pregs[i]));
    } else {
        // ARM64: save args on stack, then load into x0..x7
        // Allocate 16-byte aligned space
        size_t stack_space = ((nargs * 8 + 15) & ~15UL);
        emit("sub sp, sp, #" + std::to_string(stack_space));
        // Evaluate each arg and store to stack slot
        for (size_t i = 0; i < nargs; ++i) {
            std::string r = gen_expr(*node.args[i]);
            if (r != "x0") emit("mov x0, " + r);
            emit("str x0, [sp, #" + std::to_string(i * 8) + "]");
        }
        // Load from stack into param registers
        for (size_t i = 0; i < nargs; ++i)
            emit("ldr x" + std::to_string(i) + ", [sp, #" + std::to_string(i * 8) + "]");
        // Restore stack
        emit("add sp, sp, #" + std::to_string(stack_space));
    }

    emit_call(node.callee);
}

void CodeGen::gen_mov(const MovNode& node) {
    std::string src_reg = gen_expr(*node.src);
    int off = frame_.get_offset(node.dest);
    if (off != 0) {
        if (target_ == AsmTarget::X86_64)
            emit("mov qword [rbp" + std::to_string(off) + "], " + src_reg);
        else
            emit_frame_store(src_reg, off);
    } else {
        // Global or new variable
        frame_.alloc_local(node.dest, 8);
        int new_off = frame_.get_offset(node.dest);
        if (target_ == AsmTarget::X86_64)
            emit("mov qword [rbp" + std::to_string(new_off) + "], " + src_reg);
        else
            emit_frame_store(src_reg, new_off);
    }
}

void CodeGen::gen_add(const AddNode& node) {
    std::string l = gen_expr(*node.left);
    std::string r = gen_expr(*node.right);
    if (target_ == AsmTarget::X86_64)
        emit("add " + l + ", " + r);
    else
        emit("add " + l + ", " + l + ", " + r);
}

void CodeGen::gen_wait(const WaitNode& node) {
    // wait N → sleep N milliseconds
    // For simplicity, emit a __qz_wait(ms) runtime call
    std::string reg = gen_expr(*node.duration);
    if (target_ == AsmTarget::X86_64) {
        if (reg != "rdi") emit("mov rdi, " + reg);
        emit("call __qz_wait");
    } else {
        if (reg != "x0") emit("mov x0, " + reg);
        emit("bl __qz_wait");
    }
}

void CodeGen::gen_split(const SplitNode& node) {
    // split(var) — load address of var into arg register and call __qz_split
    int off = frame_.get_offset(node.target);
    if (target_ == AsmTarget::X86_64) {
        if (off != 0) {
            std::string sign = (off >= 0) ? "+" : "";
            emit("lea rdi, [rbp" + sign + std::to_string(off) + "]");
        } else {
            emit("xor rdi, rdi");
        }
        emit("call __qz_split");
    } else {
        if (off != 0) {
            emit("add x0, x29, #" + std::to_string(off < 0 ? -off : off));
            if (off < 0) emit("sub x0, x29, #" + std::to_string(-off));
        } else {
            emit("mov x0, #0");
        }
        emit("bl __qz_split");
    }
}

void CodeGen::gen_point(const PointNode& node) {
    // point var = idx — get element at index from split variable
    std::string idx_reg = gen_expr(*node.index);
    int off = frame_.get_offset(node.target);
    if (target_ == AsmTarget::X86_64) {
        if (idx_reg != "rdi") emit("mov rdi, " + idx_reg);
        if (off != 0) {
            std::string sign = (off >= 0) ? "+" : "";
            emit("lea rsi, [rbp" + sign + std::to_string(off) + "]");
        } else {
            emit("xor rsi, rsi");
        }
        emit("call __qz_point");
    } else {
        if (idx_reg != "x0") emit("mov x0, " + idx_reg);
        if (off != 0) {
            int abs_off = off < 0 ? -off : off;
            emit((off < 0 ? "sub" : "add") + std::string(" x1, x29, #") + std::to_string(abs_off));
        } else {
            emit("mov x1, #0");
        }
        emit("bl __qz_point");
    }
}

void CodeGen::gen_save(const SaveNode& node) {
    // save::dest = key = src  →  copy src value into dest
    int src_off  = frame_.get_offset(node.src);
    int dest_off = frame_.alloc_local(node.dest, 8);
    if (target_ == AsmTarget::X86_64) {
        if (src_off != 0) {
            std::string s = (src_off >= 0) ? "+" : "";
            emit("mov rax, qword [rbp" + s + std::to_string(src_off) + "]");
        } else {
            emit("xor rax, rax");
        }
        std::string d = (dest_off >= 0) ? "+" : "";
        emit("mov qword [rbp" + d + std::to_string(dest_off) + "], rax");
    } else {
        if (src_off != 0) {
            emit_frame_load("x0", src_off);
        } else {
            emit("mov x0, #0");
        }
        emit_frame_store("x0", dest_off);
    }
}

void CodeGen::gen_return(const ReturnNode& node) {
    if (node.value) {
        std::string reg = gen_expr(*node.value);
        if (target_ == AsmTarget::X86_64) {
            if (reg != "rax") emit("mov rax, " + reg);
        } else {
            if (reg != "x0") emit("mov x0, " + reg);
        }
    } else {
        // Return 0 by default
        if (target_ == AsmTarget::X86_64) emit("xor rax, rax");
        else                               emit("mov x0, #0");
    }
    // Use FUNC_FRAME size (must match prologue)
    emit_func_epilogue_sized(384);
    emit_ret();
}

void CodeGen::gen_math(const MathNode& node) {
    // Collect args
    std::vector<std::string> arg_regs;
    for (const auto& a : node.operands)
        arg_regs.push_back(gen_expr(*a));
    gen_math_intrinsic(node.operation, arg_regs);
}

// Convert integer register value → double (for math calls)
void CodeGen::emit_int_to_fp(const std::string& int_reg, const std::string& fp_reg) {
    if (target_ == AsmTarget::X86_64) {
        emit("cvtsi2sd " + fp_reg + ", " + int_reg);
    } else {
        emit("scvtf " + fp_reg + ", " + int_reg);
    }
}

// Convert double result → integer register
void CodeGen::emit_fp_to_int(const std::string& fp_reg, const std::string& int_reg) {
    if (target_ == AsmTarget::X86_64) {
        emit("cvttsd2si " + int_reg + ", " + fp_reg);
    } else {
        emit("fcvtzs " + int_reg + ", " + fp_reg);
    }
}

// Load arg[i] into fp register ready for a math call
void CodeGen::emit_load_math_arg(size_t i, const std::vector<std::string>& regs,
                                  const std::string& fp_reg) {
    if (i >= regs.size()) return;
    if (target_ == AsmTarget::X86_64) {
        emit("mov rdi, " + regs[i]);
        emit("cvtsi2sd " + fp_reg + ", rdi");
    } else {
        // ARM64: arg goes in x0/x1, then scvtf to d0/d1
        std::string int_reg = "x" + std::to_string(i);
        if (regs[i] != int_reg) emit("mov " + int_reg + ", " + regs[i]);
        emit("scvtf " + fp_reg + ", " + int_reg);
    }
}

// Emit a 1-arg libc math call: arg in x0/rdi (int), result in x0/rax (int via float)
void CodeGen::emit_math_call1(const std::string& fn, const std::vector<std::string>& regs) {
    if (target_ == AsmTarget::X86_64) {
        if (!regs.empty() && regs[0] != "rdi") emit("mov rdi, " + regs[0]);
        emit("cvtsi2sd xmm0, rdi");
        emit("call " + fn);
        emit("cvttsd2si rax, xmm0");
    } else {
        if (!regs.empty() && regs[0] != "x0") emit("mov x0, " + regs[0]);
        emit("scvtf d0, x0");
        emit("bl " + fn);
        emit("fcvtzs x0, d0");
    }
}

// Emit a 2-arg libc math call
void CodeGen::emit_math_call2(const std::string& fn, const std::vector<std::string>& regs) {
    if (target_ == AsmTarget::X86_64) {
        if (!regs.empty() && regs[0] != "rdi") emit("mov rdi, " + regs[0]);
        emit("cvtsi2sd xmm0, rdi");
        if (regs.size() > 1 && regs[1] != "rsi") emit("mov rsi, " + regs[1]);
        emit("cvtsi2sd xmm1, rsi");
        emit("call " + fn);
        emit("cvttsd2si rax, xmm0");
    } else {
        if (!regs.empty() && regs[0] != "x0") emit("mov x0, " + regs[0]);
        emit("scvtf d0, x0");
        if (regs.size() > 1 && regs[1] != "x1") emit("mov x1, " + regs[1]);
        emit("scvtf d1, x1");
        emit("bl " + fn);
        emit("fcvtzs x0, d0");
    }
}

void CodeGen::gen_math_intrinsic(TokenType op, const std::vector<std::string>& regs) {
    bool is_x86 = (target_ == AsmTarget::X86_64);

    switch (op) {
        // ---- Single-arg float math → libc ----
        case TokenType::TK_SQRT:  emit_math_call1("sqrt",  regs); break;
        case TokenType::TK_SIN:   emit_math_call1("sin",   regs); break;
        case TokenType::TK_COS:   emit_math_call1("cos",   regs); break;
        case TokenType::TK_TAN:   emit_math_call1("tan",   regs); break;
        case TokenType::TK_ASIN:  emit_math_call1("asin",  regs); break;
        case TokenType::TK_ACOS:  emit_math_call1("acos",  regs); break;
        case TokenType::TK_ATAN:  emit_math_call1("atan",  regs); break;
        case TokenType::TK_EXP:   emit_math_call1("exp",   regs); break;
        case TokenType::TK_LOG:   emit_math_call1("log",   regs); break;
        case TokenType::TK_LOG2:  emit_math_call1("log2",  regs); break;
        case TokenType::TK_LOG10: emit_math_call1("log10", regs); break;
        case TokenType::TK_CEIL:  emit_math_call1("ceil",  regs); break;
        case TokenType::TK_FLOOR: emit_math_call1("floor", regs); break;
        case TokenType::TK_ROUND: emit_math_call1("round", regs); break;
        case TokenType::TK_TANH_ACT: emit_math_call1("tanh", regs); break;
        case TokenType::TK_SIGMOID: {
            // sigmoid(x) = 1 / (1 + exp(-x))
            // Step 1: load x, convert to float, negate, call exp
            if (!regs.empty()) {
                if (is_x86) {
                    if (regs[0] != "rdi") emit("mov rdi, " + regs[0]);
                    emit("cvtsi2sd xmm0, rdi");
                    // negate float: flip sign bit
                    emit("xorpd xmm1, xmm1");
                    emit("subsd xmm1, xmm0");   // xmm1 = -x
                    emit("movsd xmm0, xmm1");
                    emit("call exp");
                    // xmm0 = exp(-x); compute 1/(1+exp(-x))
                    // Store result, load 1.0 from rodata
                    std::string one_lbl = "__qz_fp_one_" + std::to_string(label_cnt_++);
                    rodata_sect_ << "section .rodata\n"
                                 << one_lbl << ": dq 0x3FF0000000000000\n";
                    emit("movsd xmm1, [rel " + one_lbl + "]");
                    emit("addsd xmm0, xmm1");   // xmm0 = 1 + exp(-x)
                    emit("divsd xmm1, xmm0");   // xmm1 = 1 / (1 + exp(-x))
                    emit("cvttsd2si rax, xmm1");
                } else {
                    // ARM64: negate integer x first, then convert, call exp
                    // Load 1.0 constant safely via integer register
                    // 1.0 IEEE754 double = 0x3FF0000000000000
                    std::string one_lbl = "__qz_fp_one_" + std::to_string(label_cnt_++);
                    rodata_sect_ << "\n    .section .rodata\n"
                                 << "    .align 3\n"
                                 << one_lbl << ":\n"
                                 << "    .8byte 0x3FF0000000000000\n";
                    if (regs[0] != "x0") emit("mov x0, " + regs[0]);
                    emit("neg x0, x0");
                    emit("scvtf d0, x0");
                    emit("bl exp");              // d0 = exp(-x)
                    emit("adrp x9, " + one_lbl);
                    emit("add x9, x9, :lo12:" + one_lbl);
                    emit("ldr d1, [x9]");        // d1 = 1.0
                    emit("fadd d2, d0, d1");     // d2 = 1 + exp(-x)
                    emit("fdiv d0, d1, d2");     // d0 = 1 / (1 + exp(-x))
                    emit("fcvtzs x0, d0");
                }
            } else {
                if (is_x86) emit("xor rax, rax");
                else        emit("mov x0, #0");
            }
            break;
        }
        case TokenType::TK_RELU: {
            // relu(x) = max(x, 0)
            if (!regs.empty()) {
                if (is_x86) {
                    emit("mov rax, " + regs[0]);
                    emit("test rax, rax");
                    emit("cmovl rax, [rel .Lzero_i]");
                } else {
                    if (regs[0] != "x0") emit("mov x0, " + regs[0]);
                    emit("cmp x0, #0");
                    emit("csel x0, x0, xzr, gt");
                }
            }
            break;
        }

        // ---- ABS — integer absolute value ----
        case TokenType::TK_ABS: {
            if (!regs.empty()) {
                if (is_x86) {
                    if (regs[0] != "rax") emit("mov rax, " + regs[0]);
                    emit("cqo");                // sign-extend rax → rdx:rax
                    emit("xor rax, rdx");       // rax = rax ^ rdx
                    emit("sub rax, rdx");       // rax = rax - rdx (= abs)
                } else {
                    // ARM64: no abs for int64, use eor+sub idiom
                    if (regs[0] != "x0") emit("mov x0, " + regs[0]);
                    emit("asr x1, x0, #63");    // x1 = sign mask (0 or -1)
                    emit("eor x0, x0, x1");     // x0 = x0 ^ x1
                    emit("sub x0, x0, x1");     // x0 = x0 - x1 = abs(x0)
                }
            }
            break;
        }

        // ---- MIN / MAX ----
        case TokenType::TK_MIN: {
            if (regs.size() >= 2) {
                if (is_x86) {
                    emit("mov rax, " + regs[0]);
                    emit("cmp rax, " + regs[1]);
                    emit("cmovg rax, " + regs[1]);
                } else {
                    if (regs[0] != "x0") emit("mov x0, " + regs[0]);
                    if (regs[1] != "x1") emit("mov x1, " + regs[1]);
                    emit("cmp x0, x1");
                    emit("csel x0, x1, x0, gt");
                }
            }
            break;
        }
        case TokenType::TK_MAX: {
            if (regs.size() >= 2) {
                if (is_x86) {
                    emit("mov rax, " + regs[0]);
                    emit("cmp rax, " + regs[1]);
                    emit("cmovl rax, " + regs[1]);
                } else {
                    if (regs[0] != "x0") emit("mov x0, " + regs[0]);
                    if (regs[1] != "x1") emit("mov x1, " + regs[1]);
                    emit("cmp x0, x1");
                    emit("csel x0, x1, x0, lt");
                }
            }
            break;
        }

        // ---- CLAMP ----
        case TokenType::TK_CLAMP: {
            // clamp(x, lo, hi)
            if (regs.size() >= 3) {
                if (is_x86) {
                    emit("mov rax, " + regs[0]);
                    emit("cmp rax, " + regs[1]);
                    emit("cmovl rax, " + regs[1]);  // rax = max(x, lo)
                    emit("cmp rax, " + regs[2]);
                    emit("cmovg rax, " + regs[2]);  // rax = min(rax, hi)
                } else {
                    if (regs[0] != "x0") emit("mov x0, " + regs[0]);
                    if (regs[1] != "x1") emit("mov x1, " + regs[1]);
                    if (regs[2] != "x2") emit("mov x2, " + regs[2]);
                    emit("cmp x0, x1");
                    emit("csel x0, x1, x0, lt");   // x0 = max(x0, x1)
                    emit("cmp x0, x2");
                    emit("csel x0, x2, x0, gt");   // x0 = min(x0, x2)
                }
            }
            break;
        }

        // ---- LERP ----
        case TokenType::TK_LERP: {
            // lerp(a, b, t) → a + t*(b-a)  (integer approximation)
            emit_math_call1("__qz_lerp_stub", regs);
            break;
        }

        // ---- Two-arg float ops ----
        case TokenType::TK_POW:   emit_math_call2("pow",   regs); break;
        case TokenType::TK_ATAN2: emit_math_call2("atan2", regs); break;
        case TokenType::TK_FMOD:  emit_math_call2("fmod",  regs); break;

        // ---- GCD (Euclidean — pure integer) ----
        case TokenType::TK_GCD: {
            if (regs.size() >= 2) {
                // Call __qz_gcd runtime helper
                if (is_x86) {
                    if (regs[0] != "rdi") emit("mov rdi, " + regs[0]);
                    if (regs[1] != "rsi") emit("mov rsi, " + regs[1]);
                    emit("call __qz_gcd");
                } else {
                    if (regs[0] != "x0") emit("mov x0, " + regs[0]);
                    if (regs[1] != "x1") emit("mov x1, " + regs[1]);
                    emit("bl __qz_gcd");
                }
            }
            break;
        }

        // ---- FACTORIAL ----
        case TokenType::TK_FACTORIAL: {
            if (!regs.empty()) {
                if (is_x86) {
                    if (regs[0] != "rdi") emit("mov rdi, " + regs[0]);
                    emit("call __qz_factorial");
                } else {
                    if (regs[0] != "x0") emit("mov x0, " + regs[0]);
                    emit("bl __qz_factorial");
                }
            }
            break;
        }

        // ---- FIBONACCI ----
        case TokenType::TK_FIBONACCI: {
            if (!regs.empty()) {
                if (is_x86) {
                    if (regs[0] != "rdi") emit("mov rdi, " + regs[0]);
                    emit("call __qz_fibonacci");
                } else {
                    if (regs[0] != "x0") emit("mov x0, " + regs[0]);
                    emit("bl __qz_fibonacci");
                }
            }
            break;
        }

        // ---- ISPRIME ----
        case TokenType::TK_PRIME_CHECK: {
            if (!regs.empty()) {
                if (is_x86) {
                    if (regs[0] != "rdi") emit("mov rdi, " + regs[0]);
                    emit("call __qz_isprime");
                } else {
                    if (regs[0] != "x0") emit("mov x0, " + regs[0]);
                    emit("bl __qz_isprime");
                }
            }
            break;
        }

        // ---- SOFTMAX (stub — returns input for scalar case) ----
        case TokenType::TK_SOFTMAX: {
            emit_math_call1("exp", regs);  // scalar approximation
            break;
        }

        // ---- MEAN / VARIANCE / STD ----
        case TokenType::TK_MEAN:
        case TokenType::TK_VARIANCE:
        case TokenType::TK_STD_DEV:
        case TokenType::TK_MEDIAN:
        case TokenType::TK_PEARSON:
        case TokenType::TK_COVARIANCE:
        case TokenType::TK_ENTROPY:
        case TokenType::TK_COMBINATION:
        case TokenType::TK_PERMUTATION:
        case TokenType::TK_DOT_PROD:
        case TokenType::TK_CROSS:
        case TokenType::TK_MAGNITUDE:
        case TokenType::TK_NORMALIZE:
        case TokenType::TK_DISTANCE:
        case TokenType::TK_MATRIX_MUL:
        case TokenType::TK_FFT:
        case TokenType::TK_IFFT:
        case TokenType::TK_DERIVATIVE:
        case TokenType::TK_INTEGRAL:
        case TokenType::TK_LCM:
        case TokenType::TK_EIGEN: {
            // These require runtime library — emit stub call
            const char* fn = "__qz_math_stub";
            if (!regs.empty()) {
                if (is_x86) {
                    if (regs[0] != "rdi") emit("mov rdi, " + regs[0]);
                    emit("call " + std::string(fn));
                } else {
                    if (regs[0] != "x0") emit("mov x0, " + regs[0]);
                    emit("bl " + std::string(fn));
                }
            } else {
                if (is_x86) emit("xor rax, rax");
                else        emit("mov x0, #0");
            }
            break;
        }

        default: {
            // Unknown op — emit 0 safely
            if (is_x86) emit("xor rax, rax");
            else        emit("mov x0, #0");
            break;
        }
    }
}

void CodeGen::gen_gpu_render(const GpuRenderNode& node) {
    // Load up to 4 args into registers
    for (size_t i = 0; i < node.args.size() && i < 4; ++i) {
        std::string reg = gen_expr(*node.args[i]);
        if (target_ == AsmTarget::X86_64) {
            const char* rs[] = {"rdi","rsi","rdx","rcx"};
            if (reg != std::string(rs[i])) emit("mov " + std::string(rs[i]) + ", " + reg);
        } else {
            std::string dest = "x" + std::to_string(i);
            if (reg != dest) emit("mov " + dest + ", " + reg);
        }
    }
    emit_call("__qz_gpu_render_stub");
}

void CodeGen::gen_hw_io(const HwIoNode& node) {
    // Load arguments into registers
    for (size_t i = 0; i < node.args.size() && i < 4; ++i) {
        std::string reg = gen_expr(*node.args[i]);
        if (target_ == AsmTarget::X86_64) {
            const char* rs[] = {"rdi","rsi","rdx","rcx"};
            if (reg != std::string(rs[i])) emit("mov " + std::string(rs[i]) + ", " + reg);
        } else {
            std::string dest = "x" + std::to_string(i);
            if (reg != dest) emit("mov " + dest + ", " + reg);
        }
    }
    // CPU control keywords → emit inline or call stub
    switch (node.hw_op) {
        case TokenType::TK_CPU_NOP:
        case TokenType::TK_CPU_PAUSE:
        case TokenType::TK_CPU_YIELD:
            emit_cpu_pause();
            break;
        case TokenType::TK_CPU_FENCE:
        case TokenType::TK_CPU_MFENCE:
        case TokenType::TK_CPU_BARRIER:
        case TokenType::TK_MEM_FENCE_M:
        case TokenType::TK_MEM_BARRIER_M:
            emit_fence();
            break;
        case TokenType::TK_CPU_RDTSC:
            if (target_ == AsmTarget::X86_64) {
                emit("rdtsc");
                emit("shl rdx, 32");
                emit("or rax, rdx");
            } else {
                emit("mrs x0, cntvct_el0");
            }
            break;
        default:
            // All other HW/GPU/mem/file keywords → safe stub call
            emit_call("__qz_hw_dispatch");
            break;
    }
}

void CodeGen::gen_asm(const AsmNode& node) {
    emit_raw(node.asm_code);
}

void CodeGen::gen_mem_alloc(const MemAllocNode& node) {
    std::string size_reg = gen_expr(*node.size_expr);
    if (target_ == AsmTarget::X86_64) {
        emit("mov rdi, " + size_reg);
        emit("mov rsi, " + std::to_string(node.alignment));
    } else {
        emit("mov x0, " + size_reg);
        emit("mov x1, #" + std::to_string(node.alignment));
    }
    emit_call("__qz_mem_alloc");
    // Store result pointer
    int off = frame_.alloc_local(node.target, 8);
    if (target_ == AsmTarget::X86_64)
        emit("mov qword [rbp" + std::to_string(off) + "], rax");
    else
        emit_frame_store("x0", off);
}

// ============================================================
//  EXPRESSION CODE GEN
// ============================================================
std::string CodeGen::gen_expr(const Node& node) {
    switch (node.type) {
        case NodeType::LITERAL_EXPR: return gen_literal(static_cast<const LiteralNode&>(node));
        case NodeType::IDENT_EXPR:   return gen_ident(static_cast<const IdentNode&>(node));
        case NodeType::BINARY_EXPR:  return gen_binary(static_cast<const BinaryNode&>(node));
        case NodeType::UNARY_EXPR:   return gen_unary(static_cast<const UnaryNode&>(node));
        case NodeType::ARRAY_EXPR:   return gen_array(static_cast<const ArrayNode&>(node));
        case NodeType::MATH_EXPR: {
            gen_math(static_cast<const MathNode&>(node));
            return target_ == AsmTarget::X86_64 ? "rax" : "x0";
        }
        case NodeType::CALL_EXPR: {
            gen_call(static_cast<const CallNode&>(node));
            return target_ == AsmTarget::X86_64 ? "rax" : "x0";
        }
        default:
            gen_node(node);
            return target_ == AsmTarget::X86_64 ? "rax" : "x0";
    }
}

std::string CodeGen::gen_literal(const LiteralNode& node) {
    if (target_ == AsmTarget::X86_64) {
        if (node.value.is_int()) {
            emit("mov rax, " + std::to_string(node.value.data.i));
            return "rax";
        } else if (node.value.is_float()) {
            uint64_t bits;
            double   d = node.value.data.f;
            memcpy(&bits, &d, 8);
            emit("mov rax, 0x" + [](uint64_t v){
                char buf[32]; snprintf(buf, sizeof(buf), "%016llx",
                    (unsigned long long)v); return std::string(buf); }(bits));
            emit("movq xmm0, rax");
            emit("cvttsd2si rax, xmm0");  // convert float bits to int for uniform handling
            return "rax";
        } else if (node.value.is_bool()) {
            emit("mov rax, " + std::string(node.value.data.b ? "1" : "0"));
            return "rax";
        } else if (node.value.is_string()) {
            std::string lbl = string_label(node.value.str_data);
            emit("lea rax, [" + lbl + "]");
            return "rax";
        } else {
            emit("xor rax, rax");
            return "rax";
        }
    } else {
        // ARM64 GAS
        if (node.value.is_int()) {
            int64_t v = node.value.data.i;
            if (v == 0) {
                emit("mov x0, #0");
            } else if (v > 0 && v <= 65535) {
                emit("mov x0, #" + std::to_string(v));
            } else if (v < 0 && v >= -65536) {
                // negative small: load unsigned and trust arithmetic
                emit("mov x0, #" + std::to_string(v & 0xFFFF));
                emit("movk x0, #" + std::to_string((v >> 16) & 0xFFFF) + ", lsl #16");
                emit("movk x0, #" + std::to_string((v >> 32) & 0xFFFF) + ", lsl #32");
                emit("movk x0, #" + std::to_string((v >> 48) & 0xFFFF) + ", lsl #48");
            } else {
                // Build 64-bit constant with movz + movk
                emit("movz x0, #" + std::to_string(v & 0xFFFF));
                if ((v >> 16) & 0xFFFF) emit("movk x0, #" + std::to_string((v>>16)&0xFFFF) + ", lsl #16");
                if ((v >> 32) & 0xFFFF) emit("movk x0, #" + std::to_string((v>>32)&0xFFFF) + ", lsl #32");
                if ((v >> 48) & 0xFFFF) emit("movk x0, #" + std::to_string((v>>48)&0xFFFF) + ", lsl #48");
            }
            return "x0";
        } else if (node.value.is_float()) {
            // Store float constant in rodata and load it
            std::string flbl = "__qzflt_" + std::to_string(string_cnt_++);
            uint64_t bits; double d = node.value.data.f; memcpy(&bits, &d, 8);
            rodata_sect_ << "\n    .section .rodata\n    .align 3\n";
            rodata_sect_ << flbl << ":\n    .8byte 0x";
            char hbuf[32]; snprintf(hbuf, sizeof(hbuf), "%016llx", (unsigned long long)bits);
            rodata_sect_ << hbuf << "\n";
            emit("adrp x0, " + flbl);
            emit("add x0, x0, :lo12:" + flbl);
            emit("ldr d0, [x0]");
            // Convert float bits to integer register for uniform handling
            emit("fmov x0, d0");
            return "x0";
        } else if (node.value.is_bool()) {
            emit("mov x0, #" + std::string(node.value.data.b ? "1" : "0"));
            return "x0";
        } else if (node.value.is_string()) {
            std::string lbl = string_label(node.value.str_data);
            emit("adrp x0, " + lbl);
            emit("add x0, x0, :lo12:" + lbl);
            return "x0";
        } else {
            emit("mov x0, #0");
            return "x0";
        }
    }
}

std::string CodeGen::gen_ident(const IdentNode& node) {
    int off = frame_.get_offset(node.name);
    if (target_ == AsmTarget::X86_64) {
        if (off != 0) emit("mov rax, qword [rbp" + std::to_string(off) + "]");
        else emit("xor rax, rax ; undefined: " + node.name);
        return "rax";
    } else {
        if (off != 0) emit_frame_load("x0", off);
        else emit("mov x0, #0 // undefined: " + node.name);
        return "x0";
    }
}

std::string CodeGen::gen_binary(const BinaryNode& node) {
    std::string lreg = gen_expr(*node.left);
    std::string saved_l;
    // Save left result before evaluating right
    if (target_ == AsmTarget::X86_64) {
        emit("push rax");
        std::string rreg = gen_expr(*node.right);
        emit("mov rcx, rax");
        emit("pop rax");

        const std::string& op = node.op;
        if (op == "+")  emit("add rax, rcx");
        else if (op == "-") emit("sub rax, rcx");
        else if (op == "*") emit("imul rax, rcx");
        else if (op == "/") { emit("cqo"); emit("idiv rcx"); }
        else if (op == "%") { emit("cqo"); emit("idiv rcx"); emit("mov rax, rdx"); }
        else if (op == "==") { emit("cmp rax, rcx"); emit("sete al"); emit("movzx rax, al"); }
        else if (op == "!=") { emit("cmp rax, rcx"); emit("setne al"); emit("movzx rax, al"); }
        else if (op == "<")  { emit("cmp rax, rcx"); emit("setl al"); emit("movzx rax, al"); }
        else if (op == ">")  { emit("cmp rax, rcx"); emit("setg al"); emit("movzx rax, al"); }
        else if (op == "<=") { emit("cmp rax, rcx"); emit("setle al"); emit("movzx rax, al"); }
        else if (op == ">=") { emit("cmp rax, rcx"); emit("setge al"); emit("movzx rax, al"); }
        else if (op == "and" || op == "&&") { emit("and rax, rcx"); }
        else if (op == "or"  || op == "||") { emit("or rax, rcx"); }
        return "rax";
    } else {
        // ARM64: save x0 (left result) on stack, eval right into x0, then operate
        emit("str x0, [sp, #-16]!");   // 16-byte aligned push
        std::string rreg = gen_expr(*node.right);
        emit("mov x1, x0");            // right → x1
        emit("ldr x0, [sp], #16");     // restore left → x0

        const std::string& op = node.op;
        if      (op == "+")              emit("add x0, x0, x1");
        else if (op == "-")              emit("sub x0, x0, x1");
        else if (op == "*")              emit("mul x0, x0, x1");
        else if (op == "/")              emit("sdiv x0, x0, x1");
        else if (op == "%")            { emit("sdiv x2, x0, x1"); emit("msub x0, x2, x1, x0"); }
        else if (op == "==")           { emit("cmp x0, x1"); emit("cset x0, eq"); }
        else if (op == "!=")           { emit("cmp x0, x1"); emit("cset x0, ne"); }
        else if (op == "<")            { emit("cmp x0, x1"); emit("cset x0, lt"); }
        else if (op == ">")            { emit("cmp x0, x1"); emit("cset x0, gt"); }
        else if (op == "<=")           { emit("cmp x0, x1"); emit("cset x0, le"); }
        else if (op == ">=")           { emit("cmp x0, x1"); emit("cset x0, ge"); }
        else if (op == "and" || op == "&&") emit("and x0, x0, x1");
        else if (op == "or"  || op == "||") emit("orr x0, x0, x1");
        return "x0";
    }
}

std::string CodeGen::gen_unary(const UnaryNode& node) {
    std::string reg = gen_expr(*node.operand);
    if (target_ == AsmTarget::X86_64) {
        if (node.op == "-") emit("neg rax");
        else if (node.op == "!" || node.op == "not") {
            emit("test rax, rax");
            emit("setz al");
            emit("movzx rax, al");
        }
        return "rax";
    } else {
        if (node.op == "-") emit("neg x0, x0");
        else if (node.op == "!" || node.op == "not") {
            emit("cmp x0, #0");
            emit("cset x0, eq");
        }
        return "x0";
    }
}

std::string CodeGen::gen_array(const ArrayNode& node) {
    emit("; array literal [" + std::to_string(node.elements.size()) + " elements]");
    // Allocate stack space for array elements
    size_t total = node.elements.size() * 8;
    if (target_ == AsmTarget::X86_64) {
        emit("sub rsp, " + std::to_string(total + 8));
        emit("mov rax, rsp");
        for (size_t i = 0; i < node.elements.size(); ++i) {
            std::string ereg = gen_expr(*node.elements[i]);
            emit("mov qword [rsp+" + std::to_string(i*8) + "], " + ereg);
        }
        return "rax";
    } else {
        // ARM64: round to 16-byte aligned size
        size_t aligned = (total + 15) & ~15UL;
        emit("sub sp, sp, #" + std::to_string(aligned));
        emit("mov x0, sp");
        for (size_t i = 0; i < node.elements.size(); ++i) {
            std::string ereg = gen_expr(*node.elements[i]);
            if (ereg != "x0") emit("mov x0, " + ereg);
            emit("str x0, [sp, #" + std::to_string(i*8) + "]");
        }
        emit("mov x0, sp");
        return "x0";
    }
}

// ============================================================
//  ARCH-SPECIFIC INSTRUCTION EMITTERS
// ============================================================
void CodeGen::emit_func_prologue(const std::string&, int frame_size) {
    int aligned = (frame_size + 15) & ~15;
    if (target_ == AsmTarget::X86_64) {
        emit("push rbp");
        emit("mov rbp, rsp");
        emit("sub rsp, " + std::to_string(aligned));
    } else {
        emit("stp x29, x30, [sp, #-" + std::to_string(aligned) + "]!");
        emit("mov x29, sp");
    }
}

void CodeGen::emit_func_epilogue() {
    emit_func_epilogue_sized(384);
}

// Safe frame store: handles offset range > ±255 on ARM64
void CodeGen::emit_frame_store(const std::string& val_reg, int offset) {
    if (target_ == AsmTarget::X86_64) {
        std::string sign = (offset >= 0) ? "+" : "";
        emit("mov qword [rbp" + sign + std::to_string(offset) + "], " + val_reg);
    } else {
        // ARM64: str Xn, [x29, #imm] valid for imm in -256..255 (unscaled)
        // or 0..32760 (scaled, multiple of 8). Use x10 as scratch for large offsets.
        if (offset >= -256 && offset <= 255) {
            emit("str " + val_reg + ", [x29, #" + std::to_string(offset) + "]");
        } else if (offset >= 0 && offset <= 32760 && (offset % 8 == 0)) {
            emit("str " + val_reg + ", [x29, #" + std::to_string(offset) + "]");
        } else {
            // Large or unaligned offset: use scratch register x10
            if (offset > 0) {
                emit("add x10, x29, #" + std::to_string(offset));
            } else {
                emit("sub x10, x29, #" + std::to_string(-offset));
            }
            emit("str " + val_reg + ", [x10]");
        }
    }
}

void CodeGen::emit_frame_load(const std::string& val_reg, int offset) {
    if (target_ == AsmTarget::X86_64) {
        std::string sign = (offset >= 0) ? "+" : "";
        emit("mov " + val_reg + ", qword [rbp" + sign + std::to_string(offset) + "]");
    } else {
        if (offset >= -256 && offset <= 255) {
            emit("ldr " + val_reg + ", [x29, #" + std::to_string(offset) + "]");
        } else if (offset >= 0 && offset <= 32760 && (offset % 8 == 0)) {
            emit("ldr " + val_reg + ", [x29, #" + std::to_string(offset) + "]");
        } else {
            if (offset > 0) {
                emit("add x10, x29, #" + std::to_string(offset));
            } else {
                emit("sub x10, x29, #" + std::to_string(-offset));
            }
            emit("ldr " + val_reg + ", [x10]");
        }
    }
}

void CodeGen::emit_func_epilogue_sized(int frame_size) {
    int aligned = (frame_size + 15) & ~15;
    if (target_ == AsmTarget::X86_64) {
        emit("mov rsp, rbp");
        emit("pop rbp");
    } else {
        emit("mov sp, x29");
        emit("ldp x29, x30, [sp], #" + std::to_string(aligned));
    }
}

void CodeGen::emit_call(const std::string& func) {
    if (target_ == AsmTarget::X86_64) emit("call " + func);
    else                               emit("bl " + func);
}

void CodeGen::emit_ret() {
    emit("ret");   // same mnemonic for both x86_64 and ARM64
}

void CodeGen::emit_mov_reg_imm(const std::string& reg, int64_t imm) {
    if (target_ == AsmTarget::X86_64)
        emit("mov " + reg + ", " + std::to_string(imm));
    else
        emit("mov " + reg + ", #" + std::to_string(imm));
}

void CodeGen::emit_mov_reg_reg(const std::string& dst, const std::string& src) {
    if (dst != src) emit("mov " + dst + ", " + src);
}

void CodeGen::emit_jmp(const std::string& label) {
    if (target_ == AsmTarget::X86_64) emit("jmp " + label);
    else                               emit("b " + label);
}

void CodeGen::emit_jne(const std::string& label) {
    if (target_ == AsmTarget::X86_64) emit("jne " + label);
    else                               emit("bne " + label);
}

void CodeGen::emit_je(const std::string& label) {
    if (target_ == AsmTarget::X86_64) emit("je " + label);
    else                               emit("beq " + label);
}

void CodeGen::emit_jlt(const std::string& label) {
    if (target_ == AsmTarget::X86_64) emit("jl " + label);
    else                               emit("blt " + label);
}

void CodeGen::emit_jgt(const std::string& label) {
    if (target_ == AsmTarget::X86_64) emit("jg " + label);
    else                               emit("bgt " + label);
}

void CodeGen::emit_jle(const std::string& label) {
    if (target_ == AsmTarget::X86_64) emit("jle " + label);
    else                               emit("ble " + label);
}

void CodeGen::emit_jge(const std::string& label) {
    if (target_ == AsmTarget::X86_64) emit("jge " + label);
    else                               emit("bge " + label);
}

void CodeGen::emit_label(const std::string& label) {
    // ARM64 GAS: local labels start with .L, global labels are plain
    text_sect_ << label << ":\n";
}

void CodeGen::emit_push(const std::string& reg) {
    if (target_ == AsmTarget::X86_64)
        emit("push " + reg);
    else
        emit("str " + reg + ", [sp, #-8]!");
}

void CodeGen::emit_pop(const std::string& reg) {
    if (target_ == AsmTarget::X86_64)
        emit("pop " + reg);
    else
        emit("ldr " + reg + ", [sp], #8");
}

void CodeGen::emit_syscall_write(const std::string& str_label) {
    if (target_ == AsmTarget::X86_64) {
        emit("mov rax, 1");
        emit("mov rdi, 1");
        emit("lea rsi, [" + str_label + "]");
        emit("mov rdx, [" + str_label + "_len]");
        emit("syscall");
    } else {
        emit("mov x8, #64");
        emit("mov x0, #1");
        emit("adrp x1, " + str_label);
        emit("add x1, x1, :lo12:" + str_label);
        emit("adrp x2, " + str_label + "_len");
        emit("ldr x2, [x2, :lo12:" + str_label + "_len]");
        emit("svc #0");
    }
}

void CodeGen::emit_syscall_exit(int code) {
    if (target_ == AsmTarget::X86_64) {
        emit("mov rax, " + std::to_string(syscall_exit()));
        emit("mov rdi, " + std::to_string(code));
        emit("syscall");
    } else {
        // ARM64 Linux: exit_group = 94, exit = 93
        emit("mov x8, #93");
        emit("mov x0, #" + std::to_string(code));
        emit("svc #0");
        // Safety: loop in case svc returns
        emit("b . - 4");
    }
}

void CodeGen::emit_fence() {
    if (target_ == AsmTarget::X86_64) emit("mfence");
    else                               emit("dmb ish");
}

void CodeGen::emit_nop() {
    emit("nop");
}

void CodeGen::emit_cpu_pause() {
    if (target_ == AsmTarget::X86_64) emit("pause");
    else                               emit("yield");
}

void CodeGen::emit_rdtsc(const std::string& reg) {
    if (target_ == AsmTarget::X86_64) {
        emit("rdtsc");
        emit("shl rdx, 32");
        emit("or " + reg + ", rdx");
    } else {
        emit("mrs " + reg + ", cntvct_el0");
    }
}

void CodeGen::emit_error(const std::string& msg) {
    errors_.push_back(msg);
}

} // namespace quartz
