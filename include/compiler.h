#pragma once
#ifndef QUARTZ_COMPILER_H
#define QUARTZ_COMPILER_H

#include "quartz.h"
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "memory_manager.h"
#include "logic_processor.h"
#include <chrono>
#include <filesystem>

namespace quartz {

// ============================================================
//  QUARTZ COMPILER
//  .qua source → NASM ASM → NASM object → ELF binary
//  Pipeline: Lexer → Parser → CodeGen → NASM → LD
//  Target compile speed: 0.01ms for small programs
// ============================================================

struct CompileOptions {
    bool        optimize     = true;
    bool        debug_info   = false;
    bool        verbose      = false;
    bool        emit_asm     = false;    // save .asm file alongside
    bool        static_link  = false;
    std::string output_file  = "a.out";
    AsmTarget   target       = CodeGen::detect_target();
    int         opt_level    = 2;        // 0=none 1=basic 2=full
};

struct CompileResult {
    bool        success      = false;
    double      lex_ms       = 0.0;
    double      parse_ms     = 0.0;
    double      codegen_ms   = 0.0;
    double      assemble_ms  = 0.0;
    double      link_ms      = 0.0;
    double      total_ms     = 0.0;
    std::string output_path;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::string asm_output;      // generated ASM if requested
};

class Compiler {
public:
    explicit Compiler(CompileOptions opts = {});
    ~Compiler() = default;

    Compiler(const Compiler&) = delete;
    Compiler& operator=(const Compiler&) = delete;

    // Compile a .qua file → binary
    CompileResult compile_file(const std::string& source_path);

    // Compile source string → binary
    CompileResult compile_source(const std::string& source,
                                  const std::string& filename = "<source>",
                                  const std::string& output_path = "a.out");

    // Just lex + parse + codegen → returns ASM string (no NASM needed)
    std::string   emit_asm(const std::string& source,
                            const std::string& filename = "<source>");

    // Check if NASM is available
    static bool   nasm_available() noexcept;

    // Check if linker (ld/cc) is available
    static bool   linker_available() noexcept;

    // Arch info
    static std::string arch_string() noexcept;

private:
    CompileOptions opts_;

    // Pipeline stages
    std::vector<Token>              run_lexer   (const std::string& src,
                                                  const std::string& fname,
                                                  CompileResult& res);
    std::unique_ptr<ast::ProgramNode> run_parser(std::vector<Token> tokens,
                                                  CompileResult& res);
    std::string                     run_codegen (const ast::ProgramNode& ast,
                                                  CompileResult& res);
    bool                            run_nasm    (const std::string& asm_code,
                                                  const std::string& obj_path,
                                                  CompileResult& res);
    bool                            run_linker  (const std::string& obj_path,
                                                  const std::string& out_path,
                                                  CompileResult& res);

    // Temp file helpers
    std::string make_temp_path(const std::string& suffix) const;
    void        cleanup_temp  (const std::string& path)   const noexcept;

    // Timing helpers
    using Clock = std::chrono::high_resolution_clock;
    using TP    = std::chrono::time_point<Clock>;
    static double ms_since(TP start) noexcept;
};

} // namespace quartz

#endif // QUARTZ_COMPILER_H
