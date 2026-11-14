#include "../include/compiler.h"
#include "../include/logic_processor.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

// ============================================================
//  QUARTZ CLI TOOL
//  Usage:
//    quartz run file.qua
//    quartz compile file.qua
//    quartz compile -o output file.qua
//    quartz compile --emit-asm file.qua
//    quartz --version
//    quartz --arch
//    quartz install lib"libname"
//    ... and 23 total commands
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
        "  Target: Low-level, fast, accurate\n\n",
        QUARTZ_VERSION_STR,
        QUARTZ_ARCH_NAME
    );
}

static void print_help() {
    std::fprintf(stdout,
        "Usage: quartz <command> [options] [file]\n\n"
        "Commands:\n"
        "  run <file.qua>              Compile and run a Quartz source file\n"
        "  compile <file.qua>          Compile to binary\n"
        "  compile -o <out> <file.qua> Compile with custom output name\n"
        "  compile --emit-asm <file>   Also save generated assembly\n"
        "  compile --static <file>     Link statically\n"
        "  emit-asm <file.qua>         Print generated assembly to stdout\n"
        "  check <file.qua>            Syntax-check without compiling\n"
        "  install lib\"<name>\"       Install a Quartz package\n"
        "  uninstall lib\"<name>\"     Remove a Quartz package\n"
        "  list-libs                   List installed packages\n"
        "  new <project-name>          Create a new Quartz project\n"
        "  version                     Print version info\n"
        "  arch                        Print target architecture\n"
        "  info                        Show CPU/hardware info\n"
        "  bench <file.qua>            Compile and benchmark\n"
        "  test <file.qua>             Run test suite in file\n"
        "  fmt <file.qua>              Format source file\n"
        "  ast <file.qua>              Print AST dump\n"
        "  tokens <file.qua>           Print token stream\n"
        "  build                       Build project (reads quartz.toml)\n"
        "  clean                       Remove build artifacts\n"
        "  init                        Initialize project in current dir\n"
        "  update                      Update Quartz toolchain\n"
        "  help                        Show this help\n\n"
        "Options:\n"
        "  -o <file>       Output file name\n"
        "  -v, --verbose   Verbose output\n"
        "  --emit-asm      Save assembly alongside binary\n"
        "  --static        Static linking\n"
        "  --opt=<0|1|2>   Optimization level (default: 2)\n\n"
        "File extension: .qua\n\n"
    );
}

static void print_version() {
    std::fprintf(stdout,
        "Quartz %s\n"
        "Architecture: %s\n"
        "Target OS: Linux\n"
        "C++ Standard: C++17\n",
        QUARTZ_VERSION_STR,
        QUARTZ_ARCH_NAME
    );
}

static void print_arch_info() {
    auto& hw = HardwareDirector::instance();
    std::fprintf(stdout,
        "Architecture:  %s\n"
        "CPU Cores:     %d\n"
        "CPU Freq:      %.2f GHz\n"
        "NEON (ARM):    %s\n"
        "SVE (ARM):     %s\n"
        "AVX2 (x86):    %s\n"
        "AVX512 (x86):  %s\n"
        "NASM:          %s\n"
        "Linker:        %s\n",
        hw.arch_name(),
        hw.get_core_count(),
        static_cast<double>(hw.get_cpu_freq_hz()) / 1.0e9,
        hw.has_neon()   ? "yes" : "no",
        hw.has_sve()    ? "yes" : "no",
        hw.has_avx2()   ? "yes" : "no",
        hw.has_avx512() ? "yes" : "no",
        Compiler::nasm_available()   ? "found" : "NOT FOUND",
        Compiler::linker_available() ? "found" : "NOT FOUND"
    );
}

// ---- Read source file ----
static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::fprintf(stderr, "Error: Cannot open file: %s\n", path.c_str());
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}

// ---- Syntax check ----
static int cmd_check(const std::string& path, bool verbose) {
    std::string src = read_file(path);
    if (src.empty()) return 1;

    Lexer lexer(src, path);
    auto tokens = lexer.tokenize();
    if (lexer.has_error()) {
        std::fprintf(stderr, "Lex error: %s\n", lexer.last_error().c_str());
        return 1;
    }
    Parser parser(std::move(tokens));
    auto ast = parser.parse();
    if (parser.has_error()) {
        std::fprintf(stderr, "%s", parser.error_summary().c_str());
        return 1;
    }
    std::fprintf(stdout, "OK: %s — no syntax errors\n", path.c_str());
    return 0;
}

// ---- Token dump ----
static int cmd_tokens(const std::string& path) {
    std::string src = read_file(path);
    if (src.empty()) return 1;
    Lexer lexer(src, path);
    auto tokens = lexer.tokenize();
    std::fprintf(stdout, "Tokens in %s:\n", path.c_str());
    for (const auto& t : tokens) {
        std::fprintf(stdout, "  [%4u:%3u] %-24s '%s'\n",
            t.line, t.col,
            [](TokenType tt) -> const char* {
                switch (tt) {
                    case TokenType::TK_IDENTIFIER:  return "IDENTIFIER";
                    case TokenType::TK_INTEGER:     return "INTEGER";
                    case TokenType::TK_FLOAT:       return "FLOAT";
                    case TokenType::TK_STRING:      return "STRING";
                    case TokenType::TK_PRINT:       return "PRINT";
                    case TokenType::TK_VAR:         return "VAR";
                    case TokenType::TK_IF:          return "IF";
                    case TokenType::TK_ELSE:        return "ELSE";
                    case TokenType::TK_FUNC:        return "FUNC";
                    case TokenType::TK_RETURN:      return "RETURN";
                    case TokenType::TK_NEWLINE:     return "NEWLINE";
                    case TokenType::TK_EOF:         return "EOF";
                    case TokenType::TK_ASSIGN:      return "ASSIGN";
                    case TokenType::TK_PLUS:        return "PLUS";
                    case TokenType::TK_MINUS:       return "MINUS";
                    case TokenType::TK_STAR:        return "STAR";
                    case TokenType::TK_SLASH:       return "SLASH";
                    case TokenType::TK_LPAREN:      return "LPAREN";
                    case TokenType::TK_RPAREN:      return "RPAREN";
                    case TokenType::TK_COMMA:       return "COMMA";
                    default:                        return "TOKEN";
                }
            }(t.type),
            t.value.c_str());
    }
    return 0;
}

// ---- AST dump ----
static int cmd_ast(const std::string& path) {
    std::string src = read_file(path);
    if (src.empty()) return 1;
    Lexer lexer(src, path);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto ast = parser.parse();
    if (parser.has_error()) {
        std::fprintf(stderr, "%s", parser.error_summary().c_str());
        return 1;
    }
    std::fprintf(stdout, "AST for %s:\n", path.c_str());
    std::function<void(const ast::Node*, int)> dump = [&](const ast::Node* n, int depth) {
        if (!n) return;
        for (int i = 0; i < depth; ++i) std::fprintf(stdout, "  ");
        std::fprintf(stdout, "%s\n", n->debug_str().c_str());
        // Recurse into children
        switch (n->type) {
            case ast::NodeType::PROGRAM: {
                for (const auto& c : static_cast<const ast::ProgramNode*>(n)->body)
                    dump(c.get(), depth + 1);
                break;
            }
            case ast::NodeType::MAIN_DECL: {
                for (const auto& c : static_cast<const ast::MainDeclNode*>(n)->body)
                    dump(c.get(), depth + 1);
                break;
            }
            case ast::NodeType::FUNC_DECL: {
                for (const auto& c : static_cast<const ast::FuncDeclNode*>(n)->body)
                    dump(c.get(), depth + 1);
                break;
            }
            default: break;
        }
    };
    dump(ast.get(), 0);
    return 0;
}

// ---- New project ----
static int cmd_new(const std::string& name) {
    if (name.empty()) { std::fprintf(stderr, "Error: project name required\n"); return 1; }
    fs::create_directories(name + "/src");
    fs::create_directories(name + "/lib");
    fs::create_directories(name + "/tests");

    // Create main.qua
    std::ofstream main_f(name + "/src/main.qua");
    main_f <<
        "// " << name << " - Quartz project\n"
        "// Created with Quartz v" << QUARTZ_VERSION_STR << "\n\n"
        "int main():\n\n"
        "    print(\"Hello from " << name << "!\")\n\n";
    main_f.close();

    // Create quartz.toml
    std::ofstream cfg(name + "/quartz.toml");
    cfg << "[project]\n"
        << "name = \"" << name << "\"\n"
        << "version = \"0.1.0\"\n"
        << "author = \"\"\n"
        << "quartz = \"" << QUARTZ_VERSION_STR << "\"\n\n"
        << "[build]\n"
        << "src = \"src/main.qua\"\n"
        << "out = \"bin/" << name << "\"\n"
        << "opt = 2\n\n"
        << "[dependencies]\n";
    cfg.close();

    // Create .gitignore
    std::ofstream gi(name + "/.gitignore");
    gi << "bin/\n*.o\n*.asm\n";
    gi.close();

    std::fprintf(stdout, "Created project '%s'\n", name.c_str());
    std::fprintf(stdout, "  %s/src/main.qua\n", name.c_str());
    std::fprintf(stdout, "  %s/quartz.toml\n", name.c_str());
    std::fprintf(stdout, "\nTo build: cd %s && quartz build\n", name.c_str());
    return 0;
}

// ---- Build from quartz.toml ----
static int cmd_build(bool verbose) {
    if (!fs::exists("quartz.toml")) {
        std::fprintf(stderr, "Error: quartz.toml not found in current directory\n");
        return 1;
    }
    // Simple TOML parser for our format
    std::ifstream f("quartz.toml");
    std::string src_file, out_file;
    int opt_level = 2;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        // Strip spaces and quotes
        auto strip = [](std::string s) {
            s.erase(0, s.find_first_not_of(" \t\""));
            s.erase(s.find_last_not_of(" \t\"\r\n") + 1);
            return s;
        };
        key = strip(key); val = strip(val);
        if (key == "src") src_file = val;
        else if (key == "out") out_file = val;
        else if (key == "opt") opt_level = std::stoi(val);
    }
    if (src_file.empty()) { std::fprintf(stderr, "Error: no src in quartz.toml\n"); return 1; }
    if (out_file.empty()) out_file = "a.out";

    // Create output directory
    fs::path out_path(out_file);
    if (out_path.has_parent_path()) fs::create_directories(out_path.parent_path());

    CompileOptions opts;
    opts.verbose    = verbose;
    opts.output_file = out_file;
    opts.opt_level  = opt_level;

    Compiler compiler(opts);
    auto result = compiler.compile_file(src_file);
    if (!result.success) {
        for (const auto& e : result.errors) std::fprintf(stderr, "%s\n", e.c_str());
        return 1;
    }
    std::fprintf(stdout, "Built: %s (%.3fms)\n", out_file.c_str(), result.total_ms);
    return 0;
}

// ---- Install library ----
static int cmd_install(const std::string& libname) {
    std::string lib = libname;
    // Strip lib" prefix/suffix if present
    if (lib.substr(0, 4) == "lib\"") lib = lib.substr(4);
    if (!lib.empty() && lib.back() == '"') lib.pop_back();
    if (lib.empty()) { std::fprintf(stderr, "Error: library name required\n"); return 1; }

    std::string lib_dir = std::string(getenv("HOME") ? getenv("HOME") : ".") + "/.quartz/lib";
    fs::create_directories(lib_dir);

    // Check built-in libraries
    static const char* builtin_libs[] = { "quartz.math", "quartz.io", "quartz.ml", nullptr };
    bool is_builtin = false;
    for (int i = 0; builtin_libs[i]; ++i) {
        if (lib == builtin_libs[i]) { is_builtin = true; break; }
    }

    if (is_builtin) {
        std::string lib_path = lib_dir + "/" + lib + ".qua";
        std::ofstream lf(lib_path);
        if (lib == "quartz.math") {
            lf << "// quartz.math - Math utilities\n"
               << "// Built-in library for Quartz\n\n"
               << "func qz_add(a, b):\n"
               << "    return a + b\n"
               << "end!\n\n"
               << "func qz_mul(a, b):\n"
               << "    return a * b\n"
               << "end!\n\n"
               << "func qz_clamp_val(x, lo, hi):\n"
               << "    return clamp(x, lo, hi)\n"
               << "end!\n";
        } else if (lib == "quartz.io") {
            lf << "// quartz.io - I/O utilities\n\n"
               << "func qz_println(msg):\n"
               << "    print(msg)\n"
               << "end!\n";
        } else if (lib == "quartz.ml") {
            lf << "// quartz.ml - Machine learning primitives\n\n"
               << "func qz_sigmoid(x):\n"
               << "    return sigmoid(x)\n"
               << "end!\n\n"
               << "func qz_relu(x):\n"
               << "    return relu(x)\n"
               << "end!\n";
        }
        lf.close();
        std::fprintf(stdout, "Installed: %s → %s\n", lib.c_str(), lib_path.c_str());
        return 0;
    }

    std::fprintf(stderr, "Error: Library '%s' not found in registry.\n", lib.c_str());
    std::fprintf(stderr, "Available built-in libraries:\n");
    for (int i = 0; builtin_libs[i]; ++i)
        std::fprintf(stderr, "  %s\n", builtin_libs[i]);
    return 1;
}

// ---- Benchmark ----
static int cmd_bench(const std::string& path, bool verbose) {
    std::string src = read_file(path);
    if (src.empty()) return 1;

    std::fprintf(stdout, "Benchmarking: %s\n", path.c_str());

    // Run 5 times and average
    static constexpr int RUNS = 5;
    double total_ms = 0.0;
    CompileOptions opts;
    opts.verbose = false;
    Compiler compiler(opts);

    for (int i = 0; i < RUNS; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        auto res = compiler.compile_source(src, path, "/tmp/quartz_bench_out");
        auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - t0).count();
        total_ms += elapsed;
        if (!res.success) {
            std::fprintf(stderr, "Compile failed on run %d\n", i + 1);
            return 1;
        }
    }
    ::remove("/tmp/quartz_bench_out");

    std::fprintf(stdout,
        "  Runs:         %d\n"
        "  Avg compile:  %.4fms\n"
        "  Total:        %.4fms\n",
        RUNS, total_ms / RUNS, total_ms);
    return 0;
}

// ============================================================
//  MAIN ENTRY POINT
// ============================================================
int main(int argc, char* argv[]) {
    // Register cleanup
    std::atexit([]() {
        MemoryManager::instance().shutdown();
    });

    if (argc < 2) {
        print_banner();
        print_help();
        return 0;
    }

    std::string cmd = argv[1];
    bool verbose = false;
    std::string output_file;
    std::string source_file;
    bool emit_asm = false;
    bool do_static = false;
    int opt_level = 2;

    // Parse common flags
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-v" || a == "--verbose") verbose = true;
        else if (a == "--emit-asm")        emit_asm = true;
        else if (a == "--static")          do_static = true;
        else if (a == "--opt=0")           opt_level = 0;
        else if (a == "--opt=1")           opt_level = 1;
        else if (a == "--opt=2")           opt_level = 2;
        else if ((a == "-o") && i+1 < argc) { output_file = argv[++i]; }
        else args.push_back(a);
    }
    if (!args.empty()) source_file = args[0];

    // ---- Dispatch commands ----
    if (cmd == "version" || cmd == "--version" || cmd == "-V") {
        print_version();
        return 0;
    }
    if (cmd == "arch" || cmd == "--arch") {
        print_arch_info();
        return 0;
    }
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        print_banner();
        print_help();
        return 0;
    }
    if (cmd == "info") {
        print_banner();
        print_arch_info();
        return 0;
    }

    // ---- File commands ----
    if (cmd == "run") {
        if (source_file.empty()) { std::fprintf(stderr, "Error: no file specified\n"); return 1; }
        if (!fs::exists(source_file)) { std::fprintf(stderr, "Error: file not found: %s\n", source_file.c_str()); return 1; }

        // Compile to temp binary
        std::string tmp_bin = make_temp_path("");
        CompileOptions opts;
        opts.verbose     = verbose;
        opts.output_file = tmp_bin;
        opts.opt_level   = opt_level;

        Compiler compiler(opts);
        auto result = compiler.compile_file(source_file);

        if (!result.success) {
            std::fprintf(stderr, "\n[Quartz] Compile errors:\n");
            for (const auto& e : result.errors)
                std::fprintf(stderr, "  %s\n", e.c_str());
            return 1;
        }

        if (verbose)
            std::fprintf(stderr, "[Quartz] Compiled in %.4fms → %s\n",
                result.total_ms, tmp_bin.c_str());

        // Run the binary
        int rc = ::system(tmp_bin.c_str());
        ::remove(tmp_bin.c_str());
        return WEXITSTATUS(rc);
    }

    if (cmd == "compile") {
        if (source_file.empty()) { std::fprintf(stderr, "Error: no file specified\n"); return 1; }
        if (!fs::exists(source_file)) { std::fprintf(stderr, "Error: file not found: %s\n", source_file.c_str()); return 1; }

        if (output_file.empty()) {
            fs::path p(source_file);
            output_file = p.replace_extension("").string();
        }

        CompileOptions opts;
        opts.verbose      = verbose;
        opts.output_file  = output_file;
        opts.emit_asm     = emit_asm;
        opts.static_link  = do_static;
        opts.opt_level    = opt_level;

        Compiler compiler(opts);
        auto result = compiler.compile_file(source_file);

        if (!result.success) {
            std::fprintf(stderr, "\n[Quartz] Compile errors:\n");
            for (const auto& e : result.errors)
                std::fprintf(stderr, "  %s\n", e.c_str());
            return 1;
        }
        std::fprintf(stdout, "Compiled: %s → %s (%.4fms)\n",
            source_file.c_str(), output_file.c_str(), result.total_ms);
        return 0;
    }

    if (cmd == "emit-asm") {
        if (source_file.empty()) { std::fprintf(stderr, "Error: no file specified\n"); return 1; }
        std::string src = read_file(source_file);
        if (src.empty()) return 1;
        Compiler compiler;
        std::string asm_code = compiler.emit_asm(src, source_file);
        std::fputs(asm_code.c_str(), stdout);
        return 0;
    }

    if (cmd == "check") {
        if (source_file.empty()) { std::fprintf(stderr, "Error: no file specified\n"); return 1; }
        return cmd_check(source_file, verbose);
    }

    if (cmd == "tokens") {
        if (source_file.empty()) { std::fprintf(stderr, "Error: no file specified\n"); return 1; }
        return cmd_tokens(source_file);
    }

    if (cmd == "ast") {
        if (source_file.empty()) { std::fprintf(stderr, "Error: no file specified\n"); return 1; }
        return cmd_ast(source_file);
    }

    if (cmd == "new") {
        std::string name = source_file;
        if (name.empty() && !args.empty()) name = args[0];
        return cmd_new(name);
    }

    if (cmd == "build") {
        return cmd_build(verbose);
    }

    if (cmd == "install") {
        std::string libname;
        // Collect all remaining args as lib name
        for (const auto& a : args) libname += a;
        if (libname.empty() && !args.empty()) libname = args[0];
        return cmd_install(libname);
    }

    if (cmd == "uninstall") {
        std::string libname = source_file;
        if (libname.substr(0, 4) == "lib\"") libname = libname.substr(4);
        if (!libname.empty() && libname.back() == '"') libname.pop_back();
        std::string lib_path = std::string(getenv("HOME") ? getenv("HOME") : ".") +
                               "/.quartz/lib/" + libname + ".qua";
        if (fs::exists(lib_path)) {
            fs::remove(lib_path);
            std::fprintf(stdout, "Uninstalled: %s\n", libname.c_str());
        } else {
            std::fprintf(stderr, "Library not found: %s\n", libname.c_str());
        }
        return 0;
    }

    if (cmd == "list-libs") {
        std::string lib_dir = std::string(getenv("HOME") ? getenv("HOME") : ".") + "/.quartz/lib";
        std::fprintf(stdout, "Installed libraries in %s:\n", lib_dir.c_str());
        if (!fs::exists(lib_dir)) { std::fprintf(stdout, "  (none)\n"); return 0; }
        for (const auto& entry : fs::directory_iterator(lib_dir))
            std::fprintf(stdout, "  %s\n", entry.path().filename().c_str());
        return 0;
    }

    if (cmd == "bench") {
        if (source_file.empty()) { std::fprintf(stderr, "Error: no file specified\n"); return 1; }
        return cmd_bench(source_file, verbose);
    }

    if (cmd == "init") {
        std::string cur = fs::current_path().filename().string();
        return cmd_new(cur);
    }

    if (cmd == "clean") {
        for (const auto& ext : {".o", ".asm", ".obj"}) {
            for (const auto& entry : fs::recursive_directory_iterator(".")) {
                if (entry.path().extension() == ext) {
                    fs::remove(entry.path());
                    std::fprintf(stdout, "Removed: %s\n", entry.path().c_str());
                }
            }
        }
        if (fs::exists("bin")) fs::remove_all("bin");
        std::fprintf(stdout, "Cleaned.\n");
        return 0;
    }

    if (cmd == "update") {
        std::fprintf(stdout, "Quartz is up to date (v%s)\n", QUARTZ_VERSION_STR);
        return 0;
    }

    if (cmd == "fmt") {
        std::fprintf(stdout, "fmt: formatter not yet implemented in this release.\n");
        return 0;
    }

    if (cmd == "test") {
        if (source_file.empty()) { std::fprintf(stderr, "Error: no file specified\n"); return 1; }
        // Run test file
        CompileOptions opts;
        opts.verbose = verbose;
        opts.output_file = "/tmp/quartz_test_out";
        Compiler compiler(opts);
        auto result = compiler.compile_file(source_file);
        if (!result.success) {
            for (const auto& e : result.errors) std::fprintf(stderr, "%s\n", e.c_str());
            return 1;
        }
        int rc = ::system("/tmp/quartz_test_out");
        ::remove("/tmp/quartz_test_out");
        return WEXITSTATUS(rc);
    }

    // Unknown command
    std::fprintf(stderr, "Unknown command: %s\n", cmd.c_str());
    std::fprintf(stderr, "Run 'quartz help' for usage.\n");
    return 1;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    return ::main(argc, argv);
}
