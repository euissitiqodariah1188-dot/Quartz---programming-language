#include "../include/compiler.h"
#include "../include/codegen.h"
#include "../include/logic_processor.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <sys/wait.h>

// ============================================================
//  QUARTZ CLI TOOL  — quartz_main.cpp
//  Semua command masuk sini
// ============================================================

namespace fs = std::filesystem;
using namespace quartz;

static void print_banner() {
    std::fprintf(stdout,
        "\n"
        " ██████╗ ██╗   ██╗ █████╗ ██████╗ ████████╗███████╗\n"
        "██╔═══██╗██║   ██║██╔══██╗██╔══██╗╚══██╔══╝╚════██║\n"
        "██║   ██║██║   ██║███████║██████╔╝   ██║       ██╔╝\n"
        "██║▄▄ ██║██║   ██║██╔══██║██╔══██╗   ██║      ██╔╝ \n"
        "╚██████╔╝╚██████╔╝██║  ██║██║  ██║   ██║      ██║  \n"
        " ╚══▀▀═╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝      ╚═╝  \n"
        "\n"
        "  Quartz Programming Language v%s\n"
        "  Architecture: %s | Linux\n"
        "  Designed for: Logic, Math, ML, AI, Simulation, Games\n\n",
        QUARTZ_VERSION_STR,
        QUARTZ_ARCH_NAME
    );
}

static void print_help() {
    std::fprintf(stdout,
        "Usage: quartz <command> [options] [file]\n\n"
        "Commands:\n"
        "  run <file.qua>                  Compile and run\n"
        "  compile <file.qua>              Compile to binary\n"
        "  compile -o <out> <file.qua>     Compile with output name\n"
        "  compile --emit-asm <file.qua>   Save .asm alongside binary\n"
        "  compile --static <file.qua>     Static link\n"
        "  emit-asm <file.qua>             Print assembly to stdout\n"
        "  check <file.qua>                Syntax check only\n"
        "  tokens <file.qua>               Dump token stream\n"
        "  ast <file.qua>                  Dump AST\n"
        "  bench <file.qua>                Compile-time benchmark\n"
        "  test <file.qua>                 Run test suite\n"
        "  fmt <file.qua>                  Format source\n"
        "  new <name>                      Create new project\n"
        "  init                            Init project in cwd\n"
        "  build                           Build from quartz.toml\n"
        "  clean                           Remove build artifacts\n"
        "  install lib\"<name>\"             Install package\n"
        "  uninstall lib\"<name>\"           Remove package\n"
        "  list-libs                       List installed packages\n"
        "  update                          Update toolchain\n"
        "  version                         Print version\n"
        "  arch                            Print arch + CPU info\n"
        "  info                            Full system info\n"
        "  help                            Show this help\n\n"
        "Options:\n"
        "  -o <file>       Output binary name\n"
        "  -v, --verbose   Verbose output\n"
        "  --emit-asm      Save .asm file alongside binary\n"
        "  --static        Static linking\n"
        "  --opt=<0|1|2>   Optimization level (default 2)\n\n"
        "File extension: .qua\n"
        "Built-in libraries: quartz.math  quartz.io  quartz.ml\n\n"
    );
}

static void print_version() {
    std::fprintf(stdout,
        "Quartz Programming Language\n"
        "Version:   %s\n"
        "Arch:      %s\n"
        "OS:        Linux\n"
        "C++ std:   C++17\n"
        "Built:     %s %s\n",
        QUARTZ_VERSION_STR, QUARTZ_ARCH_NAME,
        __DATE__, __TIME__
    );
}

static void print_arch_info() {
    auto& hw = HardwareDirector::instance();
    std::fprintf(stdout,
        "Architecture:   %s\n"
        "CPU Cores:      %d\n"
        "CPU Frequency:  %.3f GHz\n"
        "NEON (ARM64):   %s\n"
        "SVE  (ARM64):   %s\n"
        "AVX2 (x86_64):  %s\n"
        "AVX512(x86_64): %s\n"
        "NASM Assembler: %s\n"
        "Linker (ld):    %s\n",
        hw.arch_name(),
        hw.get_core_count(),
        static_cast<double>(hw.get_cpu_freq_hz()) / 1.0e9,
        hw.has_neon()   ? "yes" : "no",
        hw.has_sve()    ? "yes" : "no",
        hw.has_avx2()   ? "yes" : "no",
        hw.has_avx512() ? "yes" : "no",
        Compiler::nasm_available()   ? "found" : "not found",
        Compiler::linker_available() ? "found" : "not found"
    );
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::fprintf(stderr, "Error: Cannot open: %s\n", path.c_str());
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}

static std::string make_tmp(const std::string& suffix) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "/tmp/qz_%d_%lld%s",
        getpid(),
        (long long)std::chrono::steady_clock::now().time_since_epoch().count(),
        suffix.c_str());
    return buf;
}

// ---- check ----
static int cmd_check(const std::string& path) {
    std::string src = read_file(path);
    if (src.empty()) return 1;
    Lexer lex(src, path);
    auto toks = lex.tokenize();
    if (lex.has_error()) { std::fprintf(stderr, "%s\n", lex.last_error().c_str()); return 1; }
    Parser par(std::move(toks));
    par.parse();
    if (par.has_error()) { std::fprintf(stderr, "%s", par.error_summary().c_str()); return 1; }
    std::fprintf(stdout, "OK — no syntax errors in %s\n", path.c_str());
    return 0;
}

// ---- tokens ----
static int cmd_tokens(const std::string& path) {
    std::string src = read_file(path);
    if (src.empty()) return 1;
    Lexer lex(src, path);
    auto toks = lex.tokenize();
    std::fprintf(stdout, "%-6s %-4s  %-28s  %s\n", "LINE","COL","TYPE","VALUE");
    std::fprintf(stdout, "%s\n", std::string(70,'-').c_str());
    for (const auto& t : toks) {
        const char* tname = "TOKEN";
        switch (t.type) {
            case TokenType::TK_IDENTIFIER: tname="IDENTIFIER"; break;
            case TokenType::TK_INTEGER:    tname="INTEGER"; break;
            case TokenType::TK_FLOAT:      tname="FLOAT"; break;
            case TokenType::TK_STRING:     tname="STRING"; break;
            case TokenType::TK_PRINT:      tname="PRINT"; break;
            case TokenType::TK_VAR:        tname="VAR"; break;
            case TokenType::TK_FUNC:       tname="FUNC"; break;
            case TokenType::TK_IF:         tname="IF"; break;
            case TokenType::TK_ELSE:       tname="ELSE"; break;
            case TokenType::TK_RETURN:     tname="RETURN"; break;
            case TokenType::TK_INT_MAIN:   tname="MAIN"; break;
            case TokenType::TK_NEWLINE:    tname="NEWLINE"; break;
            case TokenType::TK_EOF:        tname="EOF"; break;
            case TokenType::TK_ASSIGN:     tname="ASSIGN"; break;
            case TokenType::TK_PLUS:       tname="PLUS"; break;
            case TokenType::TK_MINUS:      tname="MINUS"; break;
            case TokenType::TK_STAR:       tname="STAR"; break;
            case TokenType::TK_SLASH:      tname="SLASH"; break;
            case TokenType::TK_LPAREN:     tname="LPAREN"; break;
            case TokenType::TK_RPAREN:     tname="RPAREN"; break;
            case TokenType::TK_COMMA:      tname="COMMA"; break;
            case TokenType::TK_DOUBLE_COLON: tname="DOUBLE_COLON"; break;
            case TokenType::TK_BOOL_TRUE:  tname="TRUE"; break;
            case TokenType::TK_BOOL_FALSE: tname="FALSE"; break;
            case TokenType::TK_WAIT:       tname="WAIT"; break;
            case TokenType::TK_SPLIT:      tname="SPLIT"; break;
            case TokenType::TK_POINT:      tname="POINT"; break;
            case TokenType::TK_SAVE:       tname="SAVE"; break;
            case TokenType::TK_MOV:        tname="MOV"; break;
            case TokenType::TK_ABS:        tname="ABS"; break;
            case TokenType::TK_SQRT:       tname="SQRT"; break;
            case TokenType::TK_SIN:        tname="SIN"; break;
            case TokenType::TK_COS:        tname="COS"; break;
            default: break;
        }
        std::string val = t.value;
        if (val == "\n" || val == "\\n") val = "\\n";
        if (val.size() > 40) val = val.substr(0,37) + "...";
        std::fprintf(stdout, "%-6u %-4u  %-28s  %s\n",
            t.line, t.col, tname, val.c_str());
    }
    return 0;
}

// ---- ast ----
static int cmd_ast(const std::string& path) {
    std::string src = read_file(path);
    if (src.empty()) return 1;
    Lexer lex(src, path);
    auto toks = lex.tokenize();
    Parser par(std::move(toks));
    auto tree = par.parse();
    if (par.has_error()) { std::fprintf(stderr, "%s", par.error_summary().c_str()); return 1; }

    std::function<void(const ast::Node*, int)> dump = [&](const ast::Node* n, int d) {
        if (!n) return;
        std::string pad(d * 2, ' ');
        std::fprintf(stdout, "%s%s\n", pad.c_str(), n->debug_str().c_str());
        switch (n->type) {
            case ast::NodeType::PROGRAM:
                for (auto& c : static_cast<const ast::ProgramNode*>(n)->body)   { dump(c.get(), d+1); } break;
            case ast::NodeType::MAIN_DECL:
                for (auto& c : static_cast<const ast::MainDeclNode*>(n)->body)  { dump(c.get(), d+1); } break;
            case ast::NodeType::FUNC_DECL:
                for (auto& c : static_cast<const ast::FuncDeclNode*>(n)->body)  { dump(c.get(), d+1); } break;
            case ast::NodeType::IF_STMT: {
                auto* nd = static_cast<const ast::IfNode*>(n);
                dump(nd->condition.get(), d+1);
                for (auto& c : nd->then_body) dump(c.get(), d+2);
                for (auto& c : nd->else_body) dump(c.get(), d+2);
                break;
            }
            case ast::NodeType::VAR_DECL: {
                auto* nd = static_cast<const ast::VarDeclNode*>(n);
                if (nd->init) dump(nd->init.get(), d+1);
                break;
            }
            case ast::NodeType::PRINT_STMT: {
                auto* nd = static_cast<const ast::PrintNode*>(n);
                dump(nd->expr.get(), d+1);
                break;
            }
            default: break;
        }
    };
    dump(tree.get(), 0);
    return 0;
}

// ---- new project ----
static int cmd_new(const std::string& name) {
    if (name.empty()) { std::fprintf(stderr, "Error: project name required\n"); return 1; }
    fs::create_directories(name + "/src");
    fs::create_directories(name + "/lib");
    fs::create_directories(name + "/tests");
    fs::create_directories(name + "/bin");

    std::ofstream mf(name + "/src/main.qua");
    mf << "// " << name << " - Quartz project\n"
       << "// Quartz v" << QUARTZ_VERSION_STR << "\n\n"
       << "int main():\n\n"
       << "    print(\"Hello from " << name << "!\")\n\n";
    mf.close();

    std::ofstream tf(name + "/quartz.toml");
    tf << "[project]\n"
       << "name = \"" << name << "\"\n"
       << "version = \"0.1.0\"\n"
       << "quartz = \"" << QUARTZ_VERSION_STR << "\"\n\n"
       << "[build]\n"
       << "src = \"src/main.qua\"\n"
       << "out = \"bin/" << name << "\"\n"
       << "opt = 2\n\n"
       << "[dependencies]\n";
    tf.close();

    std::ofstream gi(name + "/.gitignore");
    gi << "bin/\n*.o\n*.asm\n";
    gi.close();

    std::fprintf(stdout, "Created project '%s'\n", name.c_str());
    std::fprintf(stdout, "  %s/src/main.qua\n", name.c_str());
    std::fprintf(stdout, "  %s/quartz.toml\n", name.c_str());
    std::fprintf(stdout, "\n  cd %s && quartz build\n\n", name.c_str());
    return 0;
}

// ---- build from quartz.toml ----
static int cmd_build(bool verbose) {
    if (!fs::exists("quartz.toml")) {
        std::fprintf(stderr, "Error: quartz.toml not found\n"); return 1;
    }
    std::ifstream f("quartz.toml");
    std::string src_file, out_file;
    int opt_level = 2;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        auto strip = [](std::string s) {
            size_t a = s.find_first_not_of(" \t\"");
            size_t b = s.find_last_not_of(" \t\"\r\n");
            if (a == std::string::npos) return std::string();
            return s.substr(a, b - a + 1);
        };
        key = strip(key); val = strip(val);
        if (key == "src") src_file = val;
        else if (key == "out") out_file = val;
        else if (key == "opt") { try { opt_level = std::stoi(val); } catch(...){} }
    }
    if (src_file.empty()) { std::fprintf(stderr, "Error: no 'src' in quartz.toml\n"); return 1; }
    if (out_file.empty()) out_file = "a.out";

    fs::path op(out_file);
    if (op.has_parent_path()) fs::create_directories(op.parent_path());

    CompileOptions opts;
    opts.verbose     = verbose;
    opts.output_file = out_file;
    opts.opt_level   = opt_level;
    Compiler compiler(opts);
    auto result = compiler.compile_file(src_file);
    if (!result.success) {
        for (const auto& e : result.errors) std::fprintf(stderr, "  %s\n", e.c_str());
        return 1;
    }
    std::fprintf(stdout, "Built: %s (%.4fms)\n", out_file.c_str(), result.total_ms);
    return 0;
}

// ---- install library ----
static int cmd_install(const std::string& libname) {
    std::string lib = libname;
    // Strip all quote variants
    while (!lib.empty() && (lib.front() == '"' || lib.front() == '\'')) lib = lib.substr(1);
    while (!lib.empty() && (lib.back()  == '"' || lib.back()  == '\'')) lib.pop_back();
    // Strip lib prefix (from "lib\"quartz.math\"" → "quartz.math")
    if (lib.size() > 3 && lib.substr(0,3) == "lib") {
        std::string rest = lib.substr(3);
        while (!rest.empty() && (rest.front() == '"' || rest.front() == '\'')) rest = rest.substr(1);
        while (!rest.empty() && (rest.back()  == '"' || rest.back()  == '\'')) rest.pop_back();
        if (!rest.empty()) lib = rest;
    }
    if (lib.empty()) { std::fprintf(stderr, "Usage: quartz install lib\"name\"\n"); return 1; }

    const char* HOME = getenv("HOME");
    std::string lib_dir = std::string(HOME ? HOME : "/tmp") + "/.quartz/lib";
    fs::create_directories(lib_dir);

    // Built-in package definitions
    struct BuiltinLib { const char* name; const char* src; };
    static const BuiltinLib builtins[] = {
        {"quartz.math",
            "// quartz.math — math utilities\n\n"
            "func qz_add(a, b):\n    return a + b\nend!\n\n"
            "func qz_sub(a, b):\n    return a - b\nend!\n\n"
            "func qz_mul(a, b):\n    return a * b\nend!\n\n"
            "func qz_div(a, b):\n    return a / b\nend!\n\n"
            "func qz_clamp(x, lo, hi):\n    return clamp(x, lo, hi)\nend!\n\n"
            "func qz_lerp(a, b, t):\n    return lerp(a, b, t)\nend!\n\n"
            "func qz_abs(x):\n    return abs(x)\nend!\n\n"
            "func qz_sqrt(x):\n    return sqrt(x)\nend!\n\n"
            "func qz_pow(b, e):\n    return pow(b, e)\nend!\n"
        },
        {"quartz.io",
            "// quartz.io — I/O utilities\n\n"
            "func qz_print(msg):\n    print(msg)\nend!\n\n"
            "func qz_read():\n    var v = input(\"\")\n    return v\nend!\n"
        },
        {"quartz.ml",
            "// quartz.ml — machine learning primitives\n\n"
            "func qz_sigmoid(x):\n    return sigmoid(x)\nend!\n\n"
            "func qz_relu(x):\n    return relu(x)\nend!\n\n"
            "func qz_tanh(x):\n    return tanh_act(x)\nend!\n\n"
            "func qz_softmax_1d(arr):\n    return softmax(arr)\nend!\n\n"
            "func qz_mse(pred, target):\n    return pow(pred - target, 2)\nend!\n\n"
            "func qz_cross_entropy(p, q):\n    return entropy(q)\nend!\n"
        },
        {nullptr, nullptr}
    };

    for (int i = 0; builtins[i].name; ++i) {
        if (lib == builtins[i].name) {
            std::string dest = lib_dir + "/" + lib + ".qua";
            std::ofstream lf(dest);
            lf << builtins[i].src;
            lf.close();
            std::fprintf(stdout, "Installed: %s\n  → %s\n", lib.c_str(), dest.c_str());
            return 0;
        }
    }
    std::fprintf(stderr, "Library not found: %s\n", lib.c_str());
    std::fprintf(stderr, "Available: quartz.math  quartz.io  quartz.ml\n");
    return 1;
}

// ---- bench ----
static int cmd_bench(const std::string& path, bool verbose) {
    std::string src = read_file(path);
    if (src.empty()) return 1;
    std::fprintf(stdout, "Benchmarking (lex+parse+codegen): %s\n", path.c_str());

    static const int RUNS = 100;
    double sum_lex = 0, sum_parse = 0, sum_cg = 0, sum_total = 0;
    double best = 1e18, worst = 0.0;
    size_t asm_size = 0;

    for (int i = 0; i < RUNS; ++i) {
        // Phase 1: Lex
        auto t0 = std::chrono::high_resolution_clock::now();
        Lexer lex(src, path);
        auto toks = lex.tokenize();
        double lex_ms = std::chrono::duration<double,std::milli>(
            std::chrono::high_resolution_clock::now() - t0).count();

        // Phase 2: Parse
        auto t1 = std::chrono::high_resolution_clock::now();
        Parser par(std::move(toks));
        auto ast = par.parse();
        double parse_ms = std::chrono::duration<double,std::milli>(
            std::chrono::high_resolution_clock::now() - t1).count();

        if (par.has_error()) {
            std::fprintf(stderr, "Parse error on run %d\n", i+1);
            return 1;
        }

        // Phase 3: Codegen
        auto t2 = std::chrono::high_resolution_clock::now();
        CodeGen gen;
        std::string code = gen.generate(*ast);
        double cg_ms = std::chrono::duration<double,std::milli>(
            std::chrono::high_resolution_clock::now() - t2).count();
        asm_size = code.size();

        double total = lex_ms + parse_ms + cg_ms;
        sum_lex   += lex_ms;
        sum_parse += parse_ms;
        sum_cg    += cg_ms;
        sum_total += total;
        if (total < best)  best  = total;
        if (total > worst) worst = total;
    }

    std::fprintf(stdout,
        "  Runs:         %d\n"
        "  Avg total:    %.4fms  (lex=%.4f parse=%.4f codegen=%.4f)\n"
        "  Best:         %.4fms\n"
        "  Worst:        %.4fms\n"
        "  ASM output:   %zu bytes\n",
        RUNS,
        sum_total/RUNS, sum_lex/RUNS, sum_parse/RUNS, sum_cg/RUNS,
        best, worst, asm_size);
    return 0;
}

// ============================================================
//  MAIN
// ============================================================
int main(int argc, char* argv[]) {
    std::atexit([]{ MemoryManager::instance().shutdown(); });

    if (argc < 2) { print_banner(); print_help(); return 0; }

    std::string cmd = argv[1];

    // Parse flags & args
    bool verbose   = false;
    bool emit_asm  = false;
    bool do_static = false;
    int  opt_level = 2;
    std::string output_file;
    std::string source_file;

    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-v" || a == "--verbose") { verbose = true; continue; }
        if (a == "--emit-asm")             { emit_asm = true; continue; }
        if (a == "--static")               { do_static = true; continue; }
        if (a == "--opt=0")                { opt_level = 0; continue; }
        if (a == "--opt=1")                { opt_level = 1; continue; }
        if (a == "--opt=2")                { opt_level = 2; continue; }
        if (a == "-o" && i+1 < argc)      { output_file = argv[++i]; continue; }
        if (source_file.empty())           { source_file = a; }
    }

    // ── Simple informational commands ──────────────────────
    if (cmd == "version" || cmd == "--version" || cmd == "-V") { print_version(); return 0; }
    if (cmd == "help"    || cmd == "--help"    || cmd == "-h") { print_banner(); print_help(); return 0; }
    if (cmd == "arch")   { print_arch_info(); return 0; }
    if (cmd == "info")   { print_banner(); print_arch_info(); return 0; }
    if (cmd == "update") { std::fprintf(stdout,"Quartz v%s — up to date\n",QUARTZ_VERSION_STR); return 0; }
    if (cmd == "fmt")    { std::fprintf(stdout,"quartz fmt: not yet in this release\n"); return 0; }

    // ── Project commands ───────────────────────────────────
    if (cmd == "new")  { return cmd_new(source_file); }
    if (cmd == "init") { return cmd_new(fs::current_path().filename().string()); }
    if (cmd == "build"){ return cmd_build(verbose); }

    if (cmd == "clean") {
        for (auto& ext : {".o",".asm",".obj"})
            for (auto& e : fs::recursive_directory_iterator("."))
                if (e.path().extension() == ext)
                    { fs::remove(e.path()); std::fprintf(stdout,"Removed: %s\n",e.path().c_str()); }
        if (fs::exists("bin")) fs::remove_all("bin");
        std::fprintf(stdout,"Cleaned.\n");
        return 0;
    }

    if (cmd == "install") {
        // Grab everything after "install" as the library name
        std::string libname;
        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (!a.empty() && a[0] != '-') libname += (libname.empty() ? "" : " ") + a;
        }
        // Strip surrounding lib" ... " if present
        if (libname.size() > 4 && libname.substr(0,4) == "lib\"") libname = libname.substr(4);
        if (!libname.empty() && libname.back() == '"') libname.pop_back();
        // Also strip plain lib prefix without quotes
        if (libname.size() > 3 && libname.substr(0,3) == "lib") {
            std::string stripped = libname.substr(3);
            // only strip if what follows is a known lib name start
            if (!stripped.empty() && (stripped[0] == '"' || stripped[0] == '.'))
                libname = stripped;
        }
        return cmd_install(libname);
    }

    if (cmd == "uninstall") {
        std::string lib = source_file;
        if (lib.substr(0,4) == "lib\"") lib = lib.substr(4);
        if (!lib.empty() && lib.back() == '"') lib.pop_back();
        const char* HOME = getenv("HOME");
        std::string p = std::string(HOME?HOME:"/tmp") + "/.quartz/lib/" + lib + ".qua";
        if (fs::exists(p)) { fs::remove(p); std::fprintf(stdout,"Uninstalled: %s\n",lib.c_str()); }
        else std::fprintf(stderr,"Not installed: %s\n",lib.c_str());
        return 0;
    }

    if (cmd == "list-libs") {
        const char* HOME = getenv("HOME");
        std::string dir = std::string(HOME?HOME:"/tmp") + "/.quartz/lib";
        std::fprintf(stdout, "Installed libraries (%s):\n", dir.c_str());
        if (!fs::exists(dir)) { std::fprintf(stdout,"  (none)\n"); return 0; }
        for (auto& e : fs::directory_iterator(dir))
            std::fprintf(stdout,"  %s\n", e.path().filename().c_str());
        return 0;
    }

    // ── Analysis commands ──────────────────────────────────
    if (cmd == "check")  { if (source_file.empty()){std::fprintf(stderr,"Error: no file\n");return 1;} return cmd_check(source_file); }
    if (cmd == "tokens") { if (source_file.empty()){std::fprintf(stderr,"Error: no file\n");return 1;} return cmd_tokens(source_file); }
    if (cmd == "ast")    { if (source_file.empty()){std::fprintf(stderr,"Error: no file\n");return 1;} return cmd_ast(source_file); }
    if (cmd == "bench")  { if (source_file.empty()){std::fprintf(stderr,"Error: no file\n");return 1;} return cmd_bench(source_file, verbose); }

    if (cmd == "emit-asm") {
        if (source_file.empty()) { std::fprintf(stderr,"Error: no file\n"); return 1; }
        std::string src = read_file(source_file);
        if (src.empty()) return 1;
        Compiler compiler;
        std::string code = compiler.emit_asm(src, source_file);
        std::fputs(code.c_str(), stdout);
        return 0;
    }

    // ── Compile command ────────────────────────────────────
    if (cmd == "compile") {
        if (source_file.empty()) { std::fprintf(stderr,"Error: no source file\n"); return 1; }
        if (!fs::exists(source_file)) { std::fprintf(stderr,"Error: not found: %s\n",source_file.c_str()); return 1; }
        if (output_file.empty()) {
            fs::path p(source_file);
            output_file = p.replace_extension("").string();
        }
        CompileOptions opts;
        opts.verbose     = verbose;
        opts.output_file = output_file;
        opts.emit_asm    = emit_asm;
        opts.static_link = do_static;
        opts.opt_level   = opt_level;
        Compiler compiler(opts);
        auto res = compiler.compile_file(source_file);
        if (!res.success) {
            std::fprintf(stderr,"\n[Quartz] Compile errors in %s:\n", source_file.c_str());
            for (const auto& e : res.errors) std::fprintf(stderr,"  %s\n", e.c_str());
            return 1;
        }
        std::fprintf(stdout, "Compiled: %s\n"
                             "  Output: %s\n"
                             "  Time:   %.4fms (lex=%.3f parse=%.3f codegen=%.3f asm=%.3f link=%.3f)\n",
            source_file.c_str(), output_file.c_str(), res.total_ms,
            res.lex_ms, res.parse_ms, res.codegen_ms, res.assemble_ms, res.link_ms);
        if (emit_asm)
            std::fprintf(stdout, "  ASM:    %s.asm\n", output_file.c_str());
        return 0;
    }

    // ── Run command ────────────────────────────────────────
    if (cmd == "run") {
        if (source_file.empty()) { std::fprintf(stderr,"Error: no source file\n"); return 1; }
        if (!fs::exists(source_file)) { std::fprintf(stderr,"Error: not found: %s\n",source_file.c_str()); return 1; }

        std::string tmp_bin = make_tmp("");
        CompileOptions opts;
        opts.verbose     = verbose;
        opts.output_file = tmp_bin;
        opts.opt_level   = opt_level;
        Compiler compiler(opts);
        auto res = compiler.compile_file(source_file);
        if (!res.success) {
            std::fprintf(stderr,"\n[Quartz] Compile errors in %s:\n", source_file.c_str());
            for (const auto& e : res.errors) std::fprintf(stderr,"  %s\n", e.c_str());
            return 1;
        }
        if (verbose)
            std::fprintf(stderr,"[Quartz] Compiled in %.4fms → running...\n", res.total_ms);

        int rc = ::system(tmp_bin.c_str());
        ::remove(tmp_bin.c_str());
        return WEXITSTATUS(rc);
    }

    // ── Test command ───────────────────────────────────────
    if (cmd == "test") {
        if (source_file.empty()) { std::fprintf(stderr,"Error: no file\n"); return 1; }
        std::string tmp = make_tmp("");
        CompileOptions opts;
        opts.verbose     = verbose;
        opts.output_file = tmp;
        Compiler compiler(opts);
        auto res = compiler.compile_file(source_file);
        if (!res.success) {
            for (const auto& e : res.errors) std::fprintf(stderr,"%s\n",e.c_str());
            return 1;
        }
        std::fprintf(stdout, "Running tests: %s\n", source_file.c_str());
        int rc = ::system(tmp.c_str());
        ::remove(tmp.c_str());
        return WEXITSTATUS(rc);
    }

    // Unknown
    std::fprintf(stderr,"Unknown command: '%s'\nRun 'quartz help' for usage.\n", cmd.c_str());
    return 1;
}
