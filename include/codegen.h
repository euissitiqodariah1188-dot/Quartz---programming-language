#pragma once
#ifndef QUARTZ_CODEGEN_H
#define QUARTZ_CODEGEN_H

#include "quartz.h"
#include "ast.h"
#include <sstream>
#include <set>

namespace quartz {

// ============================================================
//  QUARTZ CODE GENERATOR
//  AST → NASM Assembly (x86_64 or ARM64)
//  Output is passed directly to NASM assembler
// ============================================================

// Target architecture (auto-detected at runtime too)
enum class AsmTarget {
    X86_64,
    ARM64_V8A,
    ARM64_V9A
};

// Register allocation table
struct RegAllocState {
    static constexpr int NUM_REGS = 16;

#if defined(QUARTZ_ARCH_ARM64)
    // ARM64 general purpose registers
    static constexpr const char* GP_REGS[] = {
        "x0","x1","x2","x3","x4","x5","x6","x7",
        "x8","x9","x10","x11","x12","x13","x14","x15"
    };
    static constexpr const char* FP_REGS[] = {
        "d0","d1","d2","d3","d4","d5","d6","d7",
        "d8","d9","d10","d11","d12","d13","d14","d15"
    };
    static constexpr const char* SCRATCH1 = "x9";
    static constexpr const char* SCRATCH2 = "x10";
    static constexpr const char* SP_REG   = "sp";
    static constexpr const char* LR_REG   = "lr";
    static constexpr const char* FP_REG   = "fp";
    static constexpr const char* RET_REG  = "x0";
#else
    // x86_64 registers
    static constexpr const char* GP_REGS[] = {
        "rax","rcx","rdx","rbx","rsi","rdi","r8","r9",
        "r10","r11","r12","r13","r14","r15","rbp","rsp"
    };
    static constexpr const char* FP_REGS[] = {
        "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
        "xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15"
    };
    static constexpr const char* SCRATCH1 = "r10";
    static constexpr const char* SCRATCH2 = "r11";
    static constexpr const char* SP_REG   = "rsp";
    static constexpr const char* LR_REG   = "rip";
    static constexpr const char* FP_REG   = "rbp";
    static constexpr const char* RET_REG  = "rax";
#endif

    bool   used[NUM_REGS] = {};
    bool   fp_used[NUM_REGS] = {};

    int alloc_gp() noexcept {
        for (int i = 0; i < NUM_REGS - 2; ++i) {
            if (!used[i]) { used[i] = true; return i; }
        }
        return -1;  // spill needed
    }

    void free_gp(int i) noexcept {
        if (i >= 0 && i < NUM_REGS) used[i] = false;
    }

    int alloc_fp() noexcept {
        for (int i = 0; i < NUM_REGS; ++i) {
            if (!fp_used[i]) { fp_used[i] = true; return i; }
        }
        return -1;
    }

    void free_fp(int i) noexcept {
        if (i >= 0 && i < NUM_REGS) fp_used[i] = false;
    }

    void reset() noexcept {
        memset(used, 0, sizeof(used));
        memset(fp_used, 0, sizeof(fp_used));
    }
};

// Stack frame layout
struct StackFrame {
    std::unordered_map<std::string, int> local_offsets;  // var_name → rbp/fp offset
    int   current_offset = 0;
    int   frame_size     = 0;

    int   alloc_local(const std::string& name, int size = 8) {
        current_offset -= size;
        local_offsets[name] = current_offset;
        return current_offset;
    }

    int   get_offset(const std::string& name) const {
        auto it = local_offsets.find(name);
        return it != local_offsets.end() ? it->second : 0;
    }

    bool  has_local(const std::string& name) const {
        return local_offsets.count(name) > 0;
    }
};

// ============================================================
//  CODE GENERATOR CLASS
// ============================================================
class CodeGen {
public:
    explicit CodeGen(AsmTarget target = detect_target());
    ~CodeGen() = default;

    CodeGen(const CodeGen&) = delete;
    CodeGen& operator=(const CodeGen&) = delete;

    // Generate full NASM assembly from AST
    std::string generate(const ast::ProgramNode& program);

    // Architecture detection
    static AsmTarget detect_target() noexcept;

    bool        has_error()   const { return !errors_.empty(); }
    std::string error_summary() const;

private:
    AsmTarget   target_;
    std::ostringstream  out_;          // Output asm buffer
    std::ostringstream  data_sect_;    // .data section
    std::ostringstream  bss_sect_;     // .bss section
    std::ostringstream  text_sect_;    // .text section
    std::ostringstream  rodata_sect_;  // .rodata section

    RegAllocState       regs_;
    StackFrame          frame_;
    std::vector<StackFrame> frame_stack_;

    std::unordered_map<std::string, QuartzValue> globals_;
    std::unordered_map<std::string, std::string> func_labels_;
    std::set<std::string>                        string_pool_;

    int    label_cnt_ = 0;
    int    string_cnt_ = 0;
    std::vector<std::string> errors_;

    // --- Label generation ---
    std::string new_label(const char* prefix = ".L");
    std::string string_label(const std::string& s);

    // --- Section emitters ---
    void emit_preamble();
    void emit_data_section();
    void emit_text_section();
    void emit_epilogue();

    // --- Node dispatch ---
    void gen_node(const ast::Node& node);
    void gen_program(const ast::ProgramNode& node);
    void gen_main_decl(const ast::MainDeclNode& node);
    void gen_func_decl(const ast::FuncDeclNode& node);
    void gen_var_decl(const ast::VarDeclNode& node);
    void gen_print(const ast::PrintNode& node);
    void gen_util(const ast::UtilNode& node);
    void gen_if(const ast::IfNode& node);
    void gen_call(const ast::CallNode& node);
    void gen_mov(const ast::MovNode& node);
    void gen_add(const ast::AddNode& node);
    void gen_wait(const ast::WaitNode& node);
    void gen_split(const ast::SplitNode& node);
    void gen_point(const ast::PointNode& node);
    void gen_save(const ast::SaveNode& node);
    void gen_return(const ast::ReturnNode& node);
    void gen_math(const ast::MathNode& node);
    void gen_gpu_render(const ast::GpuRenderNode& node);
    void gen_hw_io(const ast::HwIoNode& node);
    void gen_asm(const ast::AsmNode& node);
    void gen_mem_alloc(const ast::MemAllocNode& node);

    // --- Expression code gen (loads result into register) ---
    std::string gen_expr(const ast::Node& node);  // returns reg name
    std::string gen_literal(const ast::LiteralNode& node);
    std::string gen_ident(const ast::IdentNode& node);
    std::string gen_binary(const ast::BinaryNode& node);
    std::string gen_unary(const ast::UnaryNode& node);
    std::string gen_array(const ast::ArrayNode& node);

    // --- Architecture-specific instruction emitters ---
    void emit_func_prologue(const std::string& name, int frame_size);
    void emit_func_epilogue();
    void emit_func_epilogue_sized(int frame_size);
    // Safe frame store/load - handles large offsets on ARM64
    void emit_frame_store(const std::string& val_reg, int offset);
    void emit_frame_load (const std::string& val_reg, int offset);
    void emit_call(const std::string& func);
    void emit_ret();
    void emit_mov_reg_imm(const std::string& reg, int64_t imm);
    void emit_mov_reg_reg(const std::string& dst, const std::string& src);
    void emit_load_mem(const std::string& reg, const std::string& base, int offset);
    void emit_store_mem(const std::string& base, int offset, const std::string& reg);
    void emit_add_reg(const std::string& dst, const std::string& src1, const std::string& src2);
    void emit_sub_reg(const std::string& dst, const std::string& src1, const std::string& src2);
    void emit_mul_reg(const std::string& dst, const std::string& src1, const std::string& src2);
    void emit_div_reg(const std::string& dst, const std::string& src);
    void emit_cmp(const std::string& a, const std::string& b);
    void emit_jmp(const std::string& label);
    void emit_jne(const std::string& label);
    void emit_je(const std::string& label);
    void emit_jlt(const std::string& label);
    void emit_jgt(const std::string& label);
    void emit_jle(const std::string& label);
    void emit_jge(const std::string& label);
    void emit_label(const std::string& label);
    void emit_push(const std::string& reg);
    void emit_pop(const std::string& reg);
    void emit_syscall_write(const std::string& str_label);
    void emit_syscall_exit(int code);
    void emit_syscall_mmap();
    void emit_syscall_munmap();
    void emit_fence();
    void emit_nop();
    void emit_cpu_pause();
    void emit_rdtsc(const std::string& reg);

    // --- Linux syscall numbers (arch-specific) ---
    int64_t syscall_write()  const noexcept;
    int64_t syscall_read()   const noexcept;
    int64_t syscall_exit()   const noexcept;
    int64_t syscall_mmap()   const noexcept;
    int64_t syscall_munmap() const noexcept;
    int64_t syscall_open()   const noexcept;
    int64_t syscall_close()  const noexcept;
    int64_t syscall_clock_nanosleep() const noexcept;

    // --- Math intrinsic generation ---
    void gen_math_intrinsic(TokenType op, const std::vector<std::string>& regs);
    void emit_int_to_fp(const std::string& int_reg, const std::string& fp_reg);
    void emit_fp_to_int(const std::string& fp_reg, const std::string& int_reg);
    void emit_load_math_arg(size_t i, const std::vector<std::string>& regs,
                            const std::string& fp_reg);
    void emit_math_call1(const std::string& fn, const std::vector<std::string>& regs);
    void emit_math_call2(const std::string& fn, const std::vector<std::string>& regs);

    // --- Runtime helper emitters ---
    void emit_x86_runtime_helpers();
    void emit_arm64_runtime_helpers();

    // --- Error ---
    void emit_error(const std::string& msg);

    // --- Helpers ---
    QZ_INLINE void emit(const std::string& line) { text_sect_ << "    " << line << "\n"; }
    QZ_INLINE void emit_raw(const std::string& line) { text_sect_ << line << "\n"; }
    QZ_INLINE void data(const std::string& line) { data_sect_ << line << "\n"; }
    QZ_INLINE void rodata(const std::string& line) { rodata_sect_ << line << "\n"; }
};

} // namespace quartz

#endif // QUARTZ_CODEGEN_H
