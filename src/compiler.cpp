#include "../include/compiler.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <array>
#include <filesystem>
#include <sys/stat.h>

namespace quartz {
namespace fs = std::filesystem;

// ============================================================
//  COMPILER CONSTRUCTOR
// ============================================================
Compiler::Compiler(CompileOptions opts) : opts_(std::move(opts)) {}

// ============================================================
//  TIMING HELPER
// ============================================================
double Compiler::ms_since(TP start) noexcept {
    auto now = Clock::now();
    return std::chrono::duration<double, std::milli>(now - start).count();
}

// ============================================================
//  TEMP FILE HELPERS
// ============================================================
std::string Compiler::make_temp_path(const std::string& suffix) const {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "/tmp/quartz_tmp_%d_%d", getpid(),
                  static_cast<int>(std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFF));
    return std::string(buf) + suffix;
}

void Compiler::cleanup_temp(const std::string& path) const noexcept {
    ::remove(path.c_str());
}

// ============================================================
//  TOOL AVAILABILITY CHECKS
// ============================================================
bool Compiler::nasm_available() noexcept {
    return ::system("nasm --version > /dev/null 2>&1") == 0;
}

bool Compiler::linker_available() noexcept {
    return ::system("ld --version > /dev/null 2>&1") == 0 ||
           ::system("cc --version > /dev/null 2>&1") == 0;
}

std::string Compiler::arch_string() noexcept {
    // Runtime detection via uname -m — works correctly on cross-compiled binaries
    FILE* p = ::popen("uname -m 2>/dev/null", "r");
    if (!p) return QUARTZ_ARCH_NAME;
    char buf[64] = {};
    if (::fgets(buf, sizeof(buf)-1, p)) {
        ::pclose(p);
        std::string s(buf);
        // strip newline
        while (!s.empty() && (s.back()=='\n'||s.back()=='\r'||s.back()==' ')) s.pop_back();
        return s;
    }
    ::pclose(p);
    return QUARTZ_ARCH_NAME;
}

// ============================================================
//  COMPILE FILE
// ============================================================
CompileResult Compiler::compile_file(const std::string& source_path) {
    std::ifstream f(source_path);
    if (!f.is_open()) {
        CompileResult res;
        res.errors.push_back("Cannot open source file: " + source_path);
        return res;
    }
    std::string src((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());

    std::string out_path = opts_.output_file;
    if (out_path == "a.out") {
        // Default: same dir, same name, no ext
        fs::path p(source_path);
        out_path = p.replace_extension("").string();
    }
    return compile_source(src, source_path, out_path);
}

// ============================================================
//  COMPILE SOURCE
// ============================================================
CompileResult Compiler::compile_source(const std::string& source,
                                        const std::string& filename,
                                        const std::string& output_path) {
    CompileResult res;
    auto t0 = Clock::now();

    if (opts_.verbose) {
        std::fprintf(stderr, "[Quartz Compiler v%s] Arch: %s\n",
            QUARTZ_VERSION_STR, arch_string().c_str());
        std::fprintf(stderr, "  Compiling: %s\n", filename.c_str());
    }

    // ---- Phase 1: Lex ----
    auto tokens = run_lexer(source, filename, res);
    if (!res.errors.empty() || tokens.empty()) return res;

    // ---- Phase 2: Parse ----
    auto ast = run_parser(std::move(tokens), res);
    if (!ast || !res.errors.empty()) return res;

    // ---- Phase 3: Codegen ----
    std::string asm_code = run_codegen(*ast, res);
    if (asm_code.empty() || !res.errors.empty()) return res;

    if (opts_.emit_asm) {
        res.asm_output = asm_code;
        // Also write .asm file
        std::string asm_path = output_path + ".asm";
        std::ofstream af(asm_path);
        if (af.is_open()) af << asm_code;
    }

    // ---- Phase 4: Assemble ----
    std::string obj_path = make_temp_path(".o");
    if (!run_nasm(asm_code, obj_path, res)) {
        cleanup_temp(obj_path);
        return res;
    }

    // ---- Phase 5: Link ----
    if (!run_linker(obj_path, output_path, res)) {
        cleanup_temp(obj_path);
        return res;
    }

    cleanup_temp(obj_path);

    res.total_ms = ms_since(t0);
    res.success  = true;
    res.output_path = output_path;

    if (opts_.verbose) {
        std::fprintf(stderr,
            "  Lex:      %.4fms\n"
            "  Parse:    %.4fms\n"
            "  Codegen:  %.4fms\n"
            "  Assemble: %.4fms\n"
            "  Link:     %.4fms\n"
            "  Total:    %.4fms\n"
            "  Output:   %s\n",
            res.lex_ms, res.parse_ms, res.codegen_ms,
            res.assemble_ms, res.link_ms, res.total_ms,
            output_path.c_str());
    }
    return res;
}

// ============================================================
//  EMIT ASM (no NASM required)
// ============================================================
std::string Compiler::emit_asm(const std::string& source, const std::string& filename) {
    CompileResult dummy;
    auto tokens = run_lexer(source, filename, dummy);
    if (tokens.empty()) return "";
    auto ast = run_parser(std::move(tokens), dummy);
    if (!ast) return "";
    return run_codegen(*ast, dummy);
}

// ============================================================
//  PIPELINE STAGES
// ============================================================
std::vector<Token> Compiler::run_lexer(const std::string& src,
                                         const std::string& fname,
                                         CompileResult& res) {
    auto t0 = Clock::now();
    Lexer lexer(src, fname);
    auto tokens = lexer.tokenize();
    res.lex_ms = ms_since(t0);
    if (lexer.has_error()) {
        res.errors.push_back(lexer.last_error());
    }
    if (opts_.verbose)
        std::fprintf(stderr, "  Lexed %zu tokens in %.4fms\n",
                     tokens.size(), res.lex_ms);
    return tokens;
}

std::unique_ptr<ast::ProgramNode> Compiler::run_parser(std::vector<Token> tokens,
                                                          CompileResult& res) {
    auto t0 = Clock::now();
    Parser parser(std::move(tokens));
    auto ast = parser.parse();
    res.parse_ms = ms_since(t0);
    if (parser.has_error()) {
        const auto& errs = parser.errors();
        res.errors.insert(res.errors.end(), errs.begin(), errs.end());
        return nullptr;
    }
    if (opts_.verbose)
        std::fprintf(stderr, "  Parsed in %.4fms\n", res.parse_ms);
    return ast;
}

std::string Compiler::run_codegen(const ast::ProgramNode& ast_root,
                                   CompileResult& res) {
    auto t0 = Clock::now();
    CodeGen gen(opts_.target);
    std::string code = gen.generate(ast_root);
    res.codegen_ms = ms_since(t0);
    if (gen.has_error()) {
        const auto& summary = gen.error_summary();
        res.errors.push_back(summary);
        return "";
    }
    if (opts_.verbose)
        std::fprintf(stderr, "  Codegen in %.4fms (%zu bytes)\n",
                     res.codegen_ms, code.size());
    return code;
}

// ---- Helper: run a shell command and capture output ----
static std::string run_cmd(const std::string& cmd, int& rc) {
    FILE* p = ::popen(cmd.c_str(), "r");
    if (!p) { rc = -1; return "popen failed: " + cmd; }
    char buf[256];
    std::string out;
    while (::fgets(buf, sizeof(buf), p)) out += buf;
    rc = ::pclose(p);
    return out;
}

// ---- Helper: find a tool in PATH ----
static std::string find_tool(std::initializer_list<const char*> names) {
    for (const char* n : names) {
        std::string cmd = std::string("which ") + n + " 2>/dev/null";
        FILE* p = ::popen(cmd.c_str(), "r");
        if (!p) continue;
        char buf[256] = {};
        bool found = ::fgets(buf, sizeof(buf)-1, p) != nullptr;
        ::pclose(p);
        if (found && buf[0] != '\0' && buf[0] != '\n') {
            std::string s(buf);
            while (!s.empty() && (s.back()=='\n'||s.back()=='\r'||s.back()==' ')) s.pop_back();
            if (!s.empty()) return s;
        }
    }
    return "";
}

// ---- Helper: detect dynamic linker path ----
static std::string find_dynamic_linker(AsmTarget target) {
    // List of candidate paths for dynamic linker
    std::vector<std::string> candidates;
    if (target == AsmTarget::X86_64) {
        candidates = {
            "/lib64/ld-linux-x86-64.so.2",
            "/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2",
            "/lib/ld-linux-x86-64.so.2",
        };
    } else {
        candidates = {
            "/lib/ld-linux-aarch64.so.1",
            "/lib/aarch64-linux-gnu/ld-linux-aarch64.so.1",
            "/lib64/ld-linux-aarch64.so.1",
            "/system/bin/linker64",   // Android / Recian
            "/usr/lib/aarch64-linux-gnu/ld-linux-aarch64.so.1",
        };
    }
    for (const auto& p : candidates) {
        if (fs::exists(p)) return p;
    }
    return "";
}

bool Compiler::run_nasm(const std::string& asm_code,
                          const std::string& obj_path,
                          CompileResult& res) {
    auto t0 = Clock::now();

    bool is_arm64 = (opts_.target == AsmTarget::ARM64_V8A ||
                     opts_.target == AsmTarget::ARM64_V9A);

    // Choose file extension: .s for GAS (ARM64), .asm for NASM (x86_64)
    std::string ext  = is_arm64 ? ".s" : ".asm";
    std::string asm_path = make_temp_path(ext);

    {
        std::ofstream f(asm_path);
        if (!f.is_open()) {
            res.errors.push_back("Cannot write temp asm file: " + asm_path);
            return false;
        }
        f << asm_code;
    }
    auto guard = make_guard([&]{ cleanup_temp(asm_path); });

    std::string cmd, assembler;
    int rc = 0;
    std::string asm_err;

    if (is_arm64) {
        // ARM64: use GNU as (GAS)
        assembler = find_tool({"aarch64-linux-gnu-as", "as"});
        if (assembler.empty()) {
            res.errors.push_back(
                "ARM64 assembler not found. Install with:\n"
                "  pkg install binutils  (Termux/Recian)\n"
                "  apt install binutils  (Debian/Ubuntu)");
            return false;
        }
        cmd = assembler + " -o " + obj_path + " " + asm_path + " 2>&1";
    } else {
        // x86_64: prefer NASM, fallback to GAS
        assembler = find_tool({"nasm"});
        if (!assembler.empty()) {
            cmd = assembler + " -f elf64 -o " + obj_path + " " + asm_path + " 2>&1";
        } else {
            // Fallback: GAS with AT&T syntax (needs different codegen - not supported yet)
            res.errors.push_back(
                "x86_64 assembler (nasm) not found. Install with:\n"
                "  apt install nasm\n"
                "  pkg install nasm");
            return false;
        }
    }

    if (opts_.verbose)
        std::fprintf(stderr, "  Assembler: %s\n", assembler.c_str());

    asm_err = run_cmd(cmd, rc);
    res.assemble_ms = ms_since(t0);

    if (rc != 0) {
        res.errors.push_back("Assembler error (cmd: " + cmd + "):\n" + asm_err);
        return false;
    }
    if (opts_.verbose)
        std::fprintf(stderr, "  Assembled in %.4fms\n", res.assemble_ms);
    return true;
}

bool Compiler::run_linker(const std::string& obj_path,
                            const std::string& out_path,
                            CompileResult& res) {
    auto t0 = Clock::now();

    bool is_arm64 = (opts_.target == AsmTarget::ARM64_V8A ||
                     opts_.target == AsmTarget::ARM64_V9A);

    // Find linker
    std::string ld;
    if (is_arm64) {
        ld = find_tool({"aarch64-linux-gnu-ld", "ld"});
    } else {
        ld = find_tool({"ld"});
    }

    if (ld.empty()) {
        res.errors.push_back("Linker (ld) not found. Install binutils.");
        return false;
    }

    std::string cmd = ld + " -o " + out_path + " " + obj_path;

    if (!opts_.static_link) {
        std::string dynlinker = find_dynamic_linker(opts_.target);
        if (!dynlinker.empty()) {
            cmd += " -dynamic-linker " + dynlinker;
        }
        // Find libc
        std::string libc;
        if (is_arm64) {
            std::vector<std::string> libc_paths = {
                "/lib/aarch64-linux-gnu/libc.so.6",
                "/lib/aarch64-linux-gnu/libc.so",
                "/usr/lib/aarch64-linux-gnu/libc.so.6",
                "/lib/libc.so.6",
                "/lib/libc.so",
            };
            for (const auto& p : libc_paths) {
                if (fs::exists(p)) { libc = p; break; }
            }
        } else {
            std::vector<std::string> libc_paths = {
                "/lib/x86_64-linux-gnu/libc.so.6",
                "/lib64/libc.so.6",
            };
            for (const auto& p : libc_paths) {
                if (fs::exists(p)) { libc = p; break; }
            }
        }
        if (!libc.empty()) cmd += " " + libc;
        else cmd += " -lc";  // hope it's in default search path
    } else {
        cmd += " -static";
    }
    cmd += " -lm 2>&1";

    if (opts_.verbose)
        std::fprintf(stderr, "  Linker:    %s\n", ld.c_str());

    int rc = 0;
    std::string link_err = run_cmd(cmd, rc);
    res.link_ms = ms_since(t0);

    if (rc != 0) {
        // Try fallback: use cc/gcc to link (simpler, handles everything)
        std::string cc = find_tool({"aarch64-linux-gnu-gcc", "gcc", "cc", "clang"});
        if (!cc.empty()) {
            if (opts_.verbose)
                std::fprintf(stderr, "  ld failed, trying cc: %s\n", cc.c_str());
            std::string cc_cmd = cc + " -nostartfiles -o " + out_path + " " + obj_path + " -lm 2>&1";
            int rc2 = 0;
            std::string cc_err = run_cmd(cc_cmd, rc2);
            if (rc2 == 0) {
                ::chmod(out_path.c_str(), 0755);
                if (opts_.verbose)
                    std::fprintf(stderr, "  Linked via cc in %.4fms\n", ms_since(t0));
                return true;
            }
            res.errors.push_back("Linker error (ld):\n" + link_err +
                                  "\nLinker error (cc):\n" + cc_err);
        } else {
            res.errors.push_back("Linker error:\n" + link_err);
        }
        return false;
    }

    ::chmod(out_path.c_str(), 0755);
    if (opts_.verbose)
        std::fprintf(stderr, "  Linked in %.4fms\n", res.link_ms);
    return true;
}

} // namespace quartz
