#include "../include/parser.h"
#include <sstream>

namespace quartz {

using namespace ast;

// ============================================================
//  CONSTRUCTOR
// ============================================================
Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens)), pos_(0) {}

// ============================================================
//  HELPERS
// ============================================================
static const Token EOF_TOKEN = Token(TokenType::TK_EOF, "", 0, 0);

QZ_INLINE const Token& Parser::current() const noexcept {
    return pos_ < tokens_.size() ? tokens_[pos_] : EOF_TOKEN;
}

QZ_INLINE const Token& Parser::peek(int offset) const noexcept {
    size_t idx = pos_ + static_cast<size_t>(offset);
    return idx < tokens_.size() ? tokens_[idx] : EOF_TOKEN;
}

QZ_INLINE const Token& Parser::advance() noexcept {
    if (pos_ < tokens_.size()) ++pos_;
    return tokens_[pos_ - 1];
}

QZ_INLINE bool Parser::at_end() const noexcept {
    return current().type == TokenType::TK_EOF;
}

QZ_INLINE bool Parser::check(TokenType t) const noexcept {
    return current().type == t;
}

QZ_INLINE bool Parser::match(TokenType t) noexcept {
    if (check(t)) { advance(); return true; }
    return false;
}

Token Parser::consume(TokenType t, const char* msg) {
    if (check(t)) return advance();
    emit_error(std::string(msg) + " (got '" + current().value + "' at line " +
               std::to_string(current().line) + ")");
    return current();
}

void Parser::skip_newlines() noexcept {
    while (check(TokenType::TK_NEWLINE)) advance();
}

void Parser::emit_error(const std::string& msg) {
    errors_.push_back("Error at line " + std::to_string(current().line) +
                      ", col " + std::to_string(current().col) + ": " + msg);
}

void Parser::synchronize() {
    // Skip to next statement boundary
    while (!at_end()) {
        if (check(TokenType::TK_NEWLINE)) { advance(); return; }
        switch (current().type) {
            case TokenType::TK_VAR:
            case TokenType::TK_FUNC:
            case TokenType::TK_IF:
            case TokenType::TK_RETURN:
            case TokenType::TK_PRINT:
            case TokenType::TK_EOF:
                return;
            default:
                advance();
        }
    }
}

std::string Parser::error_summary() const {
    std::string s;
    for (const auto& e : errors_) s += e + "\n";
    return s;
}

// ============================================================
//  KEYWORD CATEGORY HELPERS
// ============================================================
static bool is_gpu_keyword(TokenType t) {
    switch (t) {
        case TokenType::TK_GPU_INIT:    case TokenType::TK_GPU_SHUTDOWN:
        case TokenType::TK_GPU_ALLOC:   case TokenType::TK_GPU_FREE:
        case TokenType::TK_GPU_UPLOAD:  case TokenType::TK_GPU_DOWNLOAD:
        case TokenType::TK_GPU_COPY:    case TokenType::TK_GPU_MEMSET:
        case TokenType::TK_GPU_SYNC:    case TokenType::TK_GPU_ASYNC:
        case TokenType::TK_GPU_STREAM:  case TokenType::TK_GPU_EVENT:
        case TokenType::TK_GPU_KERNEL:  case TokenType::TK_GPU_LAUNCH:
        case TokenType::TK_GPU_GRID:    case TokenType::TK_GPU_BLOCK:
        case TokenType::TK_GPU_THREADS: case TokenType::TK_GPU_WARP:
        case TokenType::TK_GPU_SHARED:  case TokenType::TK_GPU_LOCAL:
        case TokenType::TK_GPU_GLOBAL:  case TokenType::TK_GPU_ATOMIC:
        case TokenType::TK_GPU_BARRIER_G: case TokenType::TK_GPU_REDUCE:
        case TokenType::TK_GPU_SCAN:    case TokenType::TK_GPU_SORT_G:
        case TokenType::TK_GPU_MATMUL:  case TokenType::TK_GPU_CONV2D:
        case TokenType::TK_GPU_MAXPOOL: case TokenType::TK_GPU_AVGPOOL:
        case TokenType::TK_GPU_RELU_G:  case TokenType::TK_GPU_SOFTMAX_G:
        case TokenType::TK_GPU_BATCHNORM: case TokenType::TK_GPU_DROPOUT:
        case TokenType::TK_GPU_EMBED:   case TokenType::TK_GPU_ATTENTION:
        case TokenType::TK_GPU_FFT_G:   case TokenType::TK_GPU_IFFT_G:
        case TokenType::TK_GPU_HIST:    case TokenType::TK_GPU_FILTER_G:
        case TokenType::TK_GPU_WARP_RED: case TokenType::TK_GPU_LDMATRIX:
        case TokenType::TK_GPU_TENSOR:  case TokenType::TK_GPU_WMMA:
        case TokenType::TK_GPU_MMA:     case TokenType::TK_GPU_PERF:
        case TokenType::TK_GPU_OCCUPANCY: case TokenType::TK_GPU_REG:
        case TokenType::TK_GPU_L1:      case TokenType::TK_GPU_L2:
        case TokenType::TK_GPU_BANDWIDTH: case TokenType::TK_GPU_FLOPS:
        case TokenType::TK_GPU_INFO:    case TokenType::TK_GPU_DEVICE:
        // GPU render
        case TokenType::TK_GR_VERTEX:   case TokenType::TK_GR_FRAGMENT:
        case TokenType::TK_GR_COMPUTE:  case TokenType::TK_GR_GEOMETRY:
        case TokenType::TK_GR_TESS:     case TokenType::TK_GR_MESH:
        case TokenType::TK_GR_RAY:      case TokenType::TK_GR_PIPELINE:
        case TokenType::TK_GR_FRAMEBUF: case TokenType::TK_GR_RENDERBUF:
        case TokenType::TK_GR_TEXTURE:  case TokenType::TK_GR_SAMPLER:
        case TokenType::TK_GR_UNIFORM:  case TokenType::TK_GR_SSBO:
        case TokenType::TK_GR_VAO:      case TokenType::TK_GR_VBO:
        case TokenType::TK_GR_IBO:      case TokenType::TK_GR_DRAW:
        case TokenType::TK_GR_DRAWIDX:  case TokenType::TK_GR_DRAWINST:
        case TokenType::TK_GR_BLIT:     case TokenType::TK_GR_RESOLVE:
        case TokenType::TK_GR_CLEAR_G:  case TokenType::TK_GR_VIEWPORT:
        case TokenType::TK_GR_SCISSOR:  case TokenType::TK_GR_DEPTH:
        case TokenType::TK_GR_STENCIL:  case TokenType::TK_GR_BLEND:
        case TokenType::TK_GR_CULL:     case TokenType::TK_GR_WINDING:
        case TokenType::TK_GR_POLYMODE: case TokenType::TK_GR_LINE_W:
        case TokenType::TK_GR_POINT_S:  case TokenType::TK_GR_MSAA:
        case TokenType::TK_GR_ANISO:    case TokenType::TK_GR_MIP:
        case TokenType::TK_GR_LOD:      case TokenType::TK_GR_SHADOW:
        case TokenType::TK_GR_SHADOWMAP: case TokenType::TK_GR_SSAO:
        case TokenType::TK_GR_BLOOM:    case TokenType::TK_GR_DOF:
        case TokenType::TK_GR_MOTION_BL: case TokenType::TK_GR_HDR:
        case TokenType::TK_GR_TONEMAP:  case TokenType::TK_GR_GAMMA:
        case TokenType::TK_GR_COLOR_SP: case TokenType::TK_GR_POST_FX:
        case TokenType::TK_GR_PRESENT:  case TokenType::TK_GR_SWAP:
        case TokenType::TK_GR_VSYNC:    case TokenType::TK_GR_WIREFRAME:
        case TokenType::TK_GR_SKYBOX:   case TokenType::TK_GR_CUBEMAP:
        case TokenType::TK_GR_PROBE:    case TokenType::TK_GR_GI:
        case TokenType::TK_GR_REFLEC:   case TokenType::TK_GR_REFRAC:
        case TokenType::TK_GR_CAUSTIC:  case TokenType::TK_GR_SUBSURFACE:
        case TokenType::TK_GR_PARALLAX: case TokenType::TK_GR_DISPLACE:
        case TokenType::TK_GR_MORPH:    case TokenType::TK_GR_BONES:
        case TokenType::TK_GR_SKIN:     case TokenType::TK_GR_PARTICLES:
            return true;
        default: return false;
    }
}

static bool is_hw_keyword(TokenType t) {
    switch (t) {
        // HW device/io
        case TokenType::TK_HW_MMAP:     case TokenType::TK_HW_MUNMAP:
        case TokenType::TK_HW_IOREAD8:  case TokenType::TK_HW_IOREAD16:
        case TokenType::TK_HW_IOREAD32: case TokenType::TK_HW_IOREAD64:
        case TokenType::TK_HW_IOWRITE8: case TokenType::TK_HW_IOWRITE16:
        case TokenType::TK_HW_IOWRITE32: case TokenType::TK_HW_IOWRITE64:
        case TokenType::TK_HW_DMA_ALLOC: case TokenType::TK_HW_DMA_FREE:
        case TokenType::TK_HW_DMA_SYNC: case TokenType::TK_HW_IRQ_REG:
        case TokenType::TK_HW_IRQ_UNREG: case TokenType::TK_HW_IRQ_EN:
        case TokenType::TK_HW_IRQ_DIS:  case TokenType::TK_HW_TIMER_NS:
        case TokenType::TK_HW_TIMER_US: case TokenType::TK_HW_TIMER_MS:
        case TokenType::TK_HW_PORT_IN:  case TokenType::TK_HW_PORT_OUT:
        case TokenType::TK_HW_DEVOPEN:  case TokenType::TK_HW_DEVCLOSE:
        case TokenType::TK_HW_DEVREAD:  case TokenType::TK_HW_DEVWRITE:
        case TokenType::TK_HW_DEVIOCTL: case TokenType::TK_HW_DEVMAP:
        case TokenType::TK_HW_DEVSYNC:  case TokenType::TK_HW_DEVAIO:
        case TokenType::TK_HW_NETOPEN:  case TokenType::TK_HW_NETCLOSE:
        case TokenType::TK_HW_NETSEND:  case TokenType::TK_HW_NETRECV:
        case TokenType::TK_HW_NETBIND:  case TokenType::TK_HW_NETLISTEN:
        case TokenType::TK_HW_NETACCEPT: case TokenType::TK_HW_NETCONN:
        case TokenType::TK_HW_NETPOLL:  case TokenType::TK_HW_NETFLUSH:
        case TokenType::TK_HW_SOUNDOPEN: case TokenType::TK_HW_SOUNDPLAY:
        case TokenType::TK_HW_SOUNDSTOP: case TokenType::TK_HW_SOUNDVOL:
        case TokenType::TK_HW_DISPOPEN: case TokenType::TK_HW_DISPCLOSE:
        case TokenType::TK_HW_DISPFLUSH: case TokenType::TK_HW_DISPMODE:
        case TokenType::TK_HW_KBDREAD:  case TokenType::TK_HW_MOUSEREAD:
        case TokenType::TK_HW_GAMEPAD:  case TokenType::TK_HW_CAMERA:
        case TokenType::TK_HW_SENSOR:   case TokenType::TK_HW_GPIO_SET:
        case TokenType::TK_HW_GPIO_GET: case TokenType::TK_HW_GPIO_MODE:
        case TokenType::TK_HW_SPI_XFER: case TokenType::TK_HW_I2C_XFER:
        case TokenType::TK_HW_UART_SEND:
        // CPU control
        case TokenType::TK_CPU_PAUSE:   case TokenType::TK_CPU_YIELD:
        case TokenType::TK_CPU_BARRIER: case TokenType::TK_CPU_FENCE:
        case TokenType::TK_CPU_MFENCE:  case TokenType::TK_CPU_LFENCE:
        case TokenType::TK_CPU_SFENCE:  case TokenType::TK_CPU_CLFLUSH:
        case TokenType::TK_CPU_PREFETCH: case TokenType::TK_CPU_CPUID:
        case TokenType::TK_CPU_RDTSC:   case TokenType::TK_CPU_RDTSCP:
        case TokenType::TK_CPU_GETCORES: case TokenType::TK_CPU_GETFREQ:
        case TokenType::TK_CPU_SETAFF:  case TokenType::TK_CPU_GETAFF:
        case TokenType::TK_CPU_SLEEP_NS: case TokenType::TK_CPU_SPIN:
        case TokenType::TK_CPU_ATOMIC_L: case TokenType::TK_CPU_ATOMIC_S:
        case TokenType::TK_CPU_CAS:     case TokenType::TK_CPU_FETCHADD:
        case TokenType::TK_CPU_XCHG:    case TokenType::TK_CPU_LOCK:
        case TokenType::TK_CPU_UNLOCK:  case TokenType::TK_CPU_NOP:
        case TokenType::TK_CPU_HLT:     case TokenType::TK_CPU_INT:
        case TokenType::TK_CPU_CLI:     case TokenType::TK_CPU_STI:
        case TokenType::TK_CPU_PUSHF:   case TokenType::TK_CPU_POPF:
        case TokenType::TK_CPU_SYSCALL: case TokenType::TK_CPU_SYSRET:
        // Memory
        case TokenType::TK_MEM_FREE:    case TokenType::TK_MEM_REALLOC:
        case TokenType::TK_MEM_CALLOC:  case TokenType::TK_MEM_ALIGN:
        case TokenType::TK_MEM_ZERO:    case TokenType::TK_MEM_COPY:
        case TokenType::TK_MEM_MOVE:    case TokenType::TK_MEM_SET:
        case TokenType::TK_MEM_COMPARE: case TokenType::TK_MEM_FIND:
        case TokenType::TK_MEM_POOL:    case TokenType::TK_MEM_ARENA:
        case TokenType::TK_MEM_STACK_A: case TokenType::TK_MEM_PAGE:
        case TokenType::TK_MEM_HUGE:    case TokenType::TK_MEM_LOCK_P:
        case TokenType::TK_MEM_UNLOCK_P: case TokenType::TK_MEM_PROTECT:
        case TokenType::TK_MEM_ADVISE:  case TokenType::TK_MEM_SYNC_M:
        case TokenType::TK_MEM_ASYNC_M: case TokenType::TK_MEM_BARRIER_M:
        case TokenType::TK_MEM_FENCE_M: case TokenType::TK_MEM_PREFETCH_M:
        case TokenType::TK_MEM_FLUSH_M: case TokenType::TK_MEM_INVALIDATE:
        case TokenType::TK_MEM_WRITEBACK: case TokenType::TK_MEM_ATOMIC_A:
        case TokenType::TK_MEM_ATOMIC_S: case TokenType::TK_MEM_CAS_M:
        case TokenType::TK_MEM_FETCHADD_M: case TokenType::TK_MEM_XCHG_M:
        case TokenType::TK_MEM_REF_CNT: case TokenType::TK_MEM_WEAK_REF:
        case TokenType::TK_MEM_SHARED:  case TokenType::TK_MEM_UNIQUE:
        case TokenType::TK_MEM_BORROW:  case TokenType::TK_MEM_LIFETIME:
        case TokenType::TK_MEM_SCOPE:   case TokenType::TK_MEM_RAII:
        case TokenType::TK_MEM_GC_MARK: case TokenType::TK_MEM_GC_SWEEP:
        case TokenType::TK_MEM_COMPACT: case TokenType::TK_MEM_DEFRAG:
        // File/dir
        case TokenType::TK_FILE_OPEN:   case TokenType::TK_FILE_CLOSE:
        case TokenType::TK_FILE_READ:   case TokenType::TK_FILE_WRITE:
        case TokenType::TK_FILE_APPEND: case TokenType::TK_FILE_SEEK:
        case TokenType::TK_FILE_TELL:   case TokenType::TK_FILE_SIZE:
        case TokenType::TK_FILE_TRUNC:  case TokenType::TK_FILE_FLUSH:
        case TokenType::TK_FILE_SYNC:   case TokenType::TK_FILE_EOF:
        case TokenType::TK_FILE_EXISTS: case TokenType::TK_FILE_DELETE:
        case TokenType::TK_FILE_RENAME: case TokenType::TK_FILE_COPY:
        case TokenType::TK_FILE_MOVE:   case TokenType::TK_FILE_CHMOD:
        case TokenType::TK_FILE_CHOWN:  case TokenType::TK_FILE_STAT:
        case TokenType::TK_FILE_READALL: case TokenType::TK_FILE_WRITEALL:
        case TokenType::TK_FILE_READLINE: case TokenType::TK_FILE_WRITELINE:
        case TokenType::TK_FILE_READBIN: case TokenType::TK_FILE_WRITEBIN:
        case TokenType::TK_FILE_LOCK:   case TokenType::TK_FILE_UNLOCK:
        case TokenType::TK_FILE_MMAP:   case TokenType::TK_FILE_MUNMAP:
        case TokenType::TK_FILE_TEMPNAME: case TokenType::TK_FILE_TEMPOPEN:
        case TokenType::TK_DIR_OPEN:    case TokenType::TK_DIR_CLOSE:
        case TokenType::TK_DIR_READ:    case TokenType::TK_DIR_CREATE:
        case TokenType::TK_DIR_DELETE:  case TokenType::TK_DIR_EXISTS:
        case TokenType::TK_DIR_LIST:    case TokenType::TK_DIR_CWD:
        case TokenType::TK_DIR_CHDIR:   case TokenType::TK_DIR_MKDIR_P:
        case TokenType::TK_DIR_RMDIR_R: case TokenType::TK_DIR_GLOB:
        case TokenType::TK_DIR_WATCH:   case TokenType::TK_PATH_JOIN:
        case TokenType::TK_PATH_SPLIT:  case TokenType::TK_PATH_BASENAME:
        case TokenType::TK_PATH_DIRNAME: case TokenType::TK_PATH_EXTNAME:
        case TokenType::TK_PATH_ABS:    case TokenType::TK_PATH_REL:
        case TokenType::TK_PATH_NORM:   case TokenType::TK_PATH_ISABS:
        case TokenType::TK_PATH_ISREL:  case TokenType::TK_FS_SPACE:
        case TokenType::TK_FS_USED:     case TokenType::TK_FS_FREE:
        case TokenType::TK_FS_TYPE:     case TokenType::TK_FS_MOUNT:
        case TokenType::TK_FS_UMOUNT:   case TokenType::TK_FS_SYNC:
        case TokenType::TK_PIPE_OPEN:   case TokenType::TK_PIPE_CLOSE:
        case TokenType::TK_PIPE_READ:   case TokenType::TK_PIPE_WRITE:
        case TokenType::TK_PROC_EXEC:   case TokenType::TK_PROC_WAIT:
        case TokenType::TK_PROC_KILL:   case TokenType::TK_PROC_PID:
        case TokenType::TK_PROC_ENV_GET: case TokenType::TK_PROC_ENV_SET:
        case TokenType::TK_ENV_ARGS:    case TokenType::TK_SYM_LINK:
        case TokenType::TK_HARD_LINK:   case TokenType::TK_READ_LINK:
            return true;
        default: return false;
    }
}

// ============================================================
//  PROGRAM ENTRY
// ============================================================
std::unique_ptr<ProgramNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>();
    skip_newlines();
    while (!at_end()) {
        try {
            auto node = parse_statement();
            if (node) program->body.push_back(std::move(node));
        } catch (...) {
            synchronize();
        }
        skip_newlines();
    }
    return program;
}

// ============================================================
//  STATEMENT DISPATCH
// ============================================================
NodePtr Parser::parse_statement() {
    skip_newlines();
    if (at_end()) return nullptr;

    const Token& tok = current();

    // int main():
    if (tok.type == TokenType::TK_IDENTIFIER && tok.value == "int" &&
        peek().type == TokenType::TK_INT_MAIN) {
        return parse_main_decl();
    }
    if (tok.type == TokenType::TK_INT_MAIN) {
        // 'main' standalone  
        return parse_main_decl();
    }
    if (tok.type == TokenType::TK_FUNC)         return parse_func_decl();
    if (tok.type == TokenType::TK_VAR)          return parse_var_decl();
    if (tok.type == TokenType::TK_IMPORT)       return parse_import();
    if (tok.type == TokenType::TK_USING)        return parse_using();
    if (tok.type == TokenType::TK_REVERSE) {
        uint32_t l = current().line, c = current().col;
        advance();
        return std::make_unique<ast::CallNode>("__reverse__", ast::NodeList{}, l, c);
    }
    if (tok.type == TokenType::TK_PRINT)        return parse_print_stmt();
    if (tok.type == TokenType::TK_INPUT)        return parse_input_stmt();
    if (tok.type == TokenType::TK_SPLIT)        return parse_split_stmt();
    if (tok.type == TokenType::TK_POINT)        return parse_point_stmt();
    if (tok.type == TokenType::TK_SAVE)         return parse_save_stmt();
    // save:: as identifier (in case keyword map missed it)
    if (tok.type == TokenType::TK_IDENTIFIER && tok.value == "save" &&
        peek().type == TokenType::TK_DOUBLE_COLON) return parse_save_stmt();
    if (tok.type == TokenType::TK_MOV)          return parse_mov_stmt();
    if (tok.type == TokenType::TK_ADD)          return parse_add_stmt();
    if (tok.type == TokenType::TK_WAIT)         return parse_wait_stmt();
    if (tok.type == TokenType::TK_IDENTIFIER && tok.value == "util") return parse_util_stmt();
    if (tok.type == TokenType::TK_UTIL)         return parse_util_stmt();
    if (tok.type == TokenType::TK_GPU_RENDER)   return parse_gpu_render_stmt();
    if (tok.type == TokenType::TK_RETURN)       return parse_return_stmt();
    if (tok.type == TokenType::TK_IF)           return parse_if_stmt();
    if (tok.type == TokenType::TK_WHEN)         return parse_when_stmt();
    if (tok.type == TokenType::TK_FROM)         return parse_from_stmt();
    if (tok.type == TokenType::TK_MEM_ALLOC)    return parse_mem_stmt();
    // GPU keywords → parse_gpu_stmt
    if (is_gpu_keyword(tok.type))  return parse_gpu_stmt();
    // HW/CPU/mem keywords → parse_hw_stmt
    if (is_hw_keyword(tok.type))   return parse_hw_stmt();

    // Fallthrough: expression statement or function call
    return parse_expr_stmt();
}

// ============================================================
//  MAIN DECL:  int main():
// ============================================================
NodePtr Parser::parse_main_decl() {
    uint32_t l = current().line, c = current().col;
    // consume 'int' if present
    if (current().value == "int") advance();
    // consume 'main'
    advance();
    // consume '(' ')'
    if (match(TokenType::TK_LPAREN)) match(TokenType::TK_RPAREN);
    // consume ':'
    if (check(TokenType::TK_COLON)) advance();
    // consume newline
    skip_newlines();
    // Parse body until end of file or end!
    NodeList body = parse_block();
    return std::make_unique<MainDeclNode>(std::move(body), l, c);
}

// ============================================================
//  FUNC DECL:  func name:  ... end!
// ============================================================
NodePtr Parser::parse_func_decl() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'func'
    std::string name = consume(TokenType::TK_IDENTIFIER, "Expected function name").value;
    // params: optional (p1, p2, ...)
    std::vector<std::string> params;
    if (match(TokenType::TK_LPAREN)) {
        while (!check(TokenType::TK_RPAREN) && !at_end()) {
            if (check(TokenType::TK_IDENTIFIER)) {
                params.push_back(advance().value);
            }
            if (!match(TokenType::TK_COMMA)) break;
        }
        consume(TokenType::TK_RPAREN, "Expected ')' after params");
    }
    if (check(TokenType::TK_COLON)) advance();
    skip_newlines();
    NodeList body = parse_block();
    // consume end!
    if (check(TokenType::TK_END)) advance();
    return std::make_unique<FuncDeclNode>(name, std::move(params), std::move(body), l, c);
}

// ============================================================
//  VAR DECL:  var name = value
// ============================================================
NodePtr Parser::parse_var_decl() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'var'
    std::string name = consume(TokenType::TK_IDENTIFIER, "Expected variable name").value;
    NodePtr init;
    if (match(TokenType::TK_ASSIGN)) {
        init = parse_expr();
    }
    return std::make_unique<VarDeclNode>(name, std::move(init), l, c);
}

// ============================================================
//  IMPORT:  import "path"
// ============================================================
NodePtr Parser::parse_import() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'import'
    std::string path = consume(TokenType::TK_STRING, "Expected import path").value;
    std::string alias;
    if (match(TokenType::TK_IDENTIFIER)) alias = tokens_[pos_-1].value;
    return std::make_unique<ImportNode>(path, alias, l, c);
}

// ============================================================
//  USING:  using("path")
// ============================================================
NodePtr Parser::parse_using() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'using'
    consume(TokenType::TK_LPAREN, "Expected '(' after using");
    std::string path = consume(TokenType::TK_STRING, "Expected path string").value;
    consume(TokenType::TK_RPAREN, "Expected ')' after using path");
    return std::make_unique<UsingNode>(path, l, c);
}

// ============================================================
//  PRINT:  print("text")  or  print(var)
// ============================================================
NodePtr Parser::parse_print_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'print'
    consume(TokenType::TK_LPAREN, "Expected '(' after print");
    NodePtr expr = parse_expr();
    consume(TokenType::TK_RPAREN, "Expected ')' after print argument");
    return std::make_unique<PrintNode>(std::move(expr), l, c);
}

// ============================================================
//  INPUT:  var x = input(prompt)
//  Standalone: input(prompt)
// ============================================================
NodePtr Parser::parse_input_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'input'
    consume(TokenType::TK_LPAREN, "Expected '(' after input");
    NodePtr prompt = parse_expr();
    consume(TokenType::TK_RPAREN, "Expected ')' after input prompt");
    // Reuse CallNode for input
    NodeList args;
    args.push_back(std::move(prompt));
    return std::make_unique<CallNode>("input", std::move(args), l, c);
}

// ============================================================
//  SPLIT:  split(varname)
// ============================================================
NodePtr Parser::parse_split_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'split'
    consume(TokenType::TK_LPAREN, "Expected '(' after split");
    std::string target = consume(TokenType::TK_IDENTIFIER, "Expected variable name").value;
    consume(TokenType::TK_RPAREN, "Expected ')' after split argument");
    return std::make_unique<SplitNode>(target, l, c);
}

// ============================================================
//  POINT:  point varname = index
// ============================================================
NodePtr Parser::parse_point_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'point'
    std::string target = consume(TokenType::TK_IDENTIFIER, "Expected variable name").value;
    consume(TokenType::TK_ASSIGN, "Expected '=' after point variable");
    NodePtr index = parse_expr();
    return std::make_unique<PointNode>(target, std::move(index), l, c);
}

// ============================================================
//  SAVE::  save::dest = key = src
// ============================================================
NodePtr Parser::parse_save_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'save' or TK_SAVE
    // consume '::'
    if (check(TokenType::TK_DOUBLE_COLON)) advance();
    else if (check(TokenType::TK_COLON)) { advance(); if (check(TokenType::TK_COLON)) advance(); }
    else { emit_error("Expected '::' after save"); return nullptr; }
    std::string dest = consume(TokenType::TK_IDENTIFIER, "Expected destination variable").value;
    consume(TokenType::TK_ASSIGN, "Expected '=' after save destination");
    std::string key = consume(TokenType::TK_IDENTIFIER, "Expected key name").value;
    consume(TokenType::TK_ASSIGN, "Expected '=' after key");
    std::string src = consume(TokenType::TK_IDENTIFIER, "Expected source variable").value;
    return std::make_unique<SaveNode>(dest, key, src, l, c);
}

// ============================================================
//  MOV:  MOV dest src
// ============================================================
NodePtr Parser::parse_mov_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'MOV'
    std::string dest = consume(TokenType::TK_IDENTIFIER, "Expected destination for MOV").value;
    NodePtr src = parse_expr();
    return std::make_unique<MovNode>(dest, std::move(src), l, c);
}

// ============================================================
//  ADD:  add a b   (adds b into a)
// ============================================================
NodePtr Parser::parse_add_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'add'
    NodePtr left = parse_expr();
    NodePtr right = parse_expr();
    return std::make_unique<AddNode>(std::move(left), std::move(right), l, c);
}

// ============================================================
//  WAIT:  wait 100   (milliseconds)
// ============================================================
NodePtr Parser::parse_wait_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'wait'
    NodePtr dur = parse_expr();
    return std::make_unique<WaitNode>(std::move(dur), l, c);
}

// ============================================================
//  UTIL::  util::expr
// ============================================================
NodePtr Parser::parse_util_stmt() {
    uint32_t l = current().line, c = current().col;
    // consume 'util' keyword or identifier
    advance();
    // consume '::'
    if (check(TokenType::TK_DOUBLE_COLON)) advance();
    else if (check(TokenType::TK_COLON)) { advance(); if (check(TokenType::TK_COLON)) advance(); }
    else { emit_error("Expected '::' after util"); return nullptr; }
    // Collect raw expression string until newline
    size_t start_pos = pos_;
    std::string expr_str;
    while (!at_end() && !check(TokenType::TK_NEWLINE)) {
        expr_str += current().value;
        advance();
    }
    // Re-parse as expression (re-tokenize sub-string)
    pos_ = start_pos;
    NodePtr expr = parse_expr();
    return std::make_unique<UtilNode>(expr_str, std::move(expr), l, c);
}

// ============================================================
//  GPURender::
// ============================================================
NodePtr Parser::parse_gpu_render_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'GPURender'
    consume(TokenType::TK_DOUBLE_COLON, "Expected '::' after GPURender");
    TokenType op = current().type;
    advance();
    NodeList args;
    if (match(TokenType::TK_LPAREN)) {
        while (!check(TokenType::TK_RPAREN) && !at_end()) {
            args.push_back(parse_expr());
            if (!match(TokenType::TK_COMMA)) break;
        }
        consume(TokenType::TK_RPAREN, "Expected ')' after GPURender args");
    }
    return std::make_unique<GpuRenderNode>(op, std::move(args), l, c);
}

// ============================================================
//  RETURN
// ============================================================
NodePtr Parser::parse_return_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'return'
    NodePtr val;
    if (!check(TokenType::TK_NEWLINE) && !at_end()) {
        val = parse_expr();
    }
    return std::make_unique<ReturnNode>(std::move(val), l, c);
}

// ============================================================
//  IF / ELSE
// ============================================================
NodePtr Parser::parse_if_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'if'
    NodePtr cond = parse_expr();
    if (check(TokenType::TK_COLON)) advance();
    skip_newlines();
    NodeList then_body = parse_block();
    NodeList else_body;
    if (check(TokenType::TK_ELSE)) {
        advance();
        if (check(TokenType::TK_COLON)) advance();
        skip_newlines();
        else_body = parse_block();
    }
    if (check(TokenType::TK_END)) advance();
    return std::make_unique<IfNode>(std::move(cond), std::move(then_body), std::move(else_body), l, c);
}

// ============================================================
//  WHEN
// ============================================================
NodePtr Parser::parse_when_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'when'
    NodePtr cond = parse_expr();
    if (check(TokenType::TK_COLON)) advance();
    skip_newlines();
    NodeList body = parse_block();
    if (check(TokenType::TK_END)) advance();
    return std::make_unique<IfNode>(std::move(cond), std::move(body), NodeList{}, l, c);
}

// ============================================================
//  FROM
// ============================================================
NodePtr Parser::parse_from_stmt() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume 'from'
    NodePtr start_expr = parse_expr();
    if (check(TokenType::TK_COLON)) advance();
    skip_newlines();
    NodeList body = parse_block();
    if (check(TokenType::TK_END)) advance();
    NodeList args;
    args.push_back(std::move(start_expr));
    return std::make_unique<CallNode>("__from__", std::move(args), l, c);
}

// ============================================================
//  MEM STATEMENT:  mem_alloc(var, size, align)
// ============================================================
NodePtr Parser::parse_mem_stmt() {
    uint32_t l = current().line, c = current().col;
    advance(); // consume mem keyword
    consume(TokenType::TK_LPAREN, "Expected '(' after memory keyword");
    std::string target = consume(TokenType::TK_IDENTIFIER, "Expected variable name").value;
    consume(TokenType::TK_COMMA, "Expected ',' after variable name");
    NodePtr size_expr = parse_expr();
    size_t align = 8;
    if (match(TokenType::TK_COMMA)) {
        // alignment argument
        if (check(TokenType::TK_INTEGER)) {
            align = static_cast<size_t>(std::stoll(current().value));
            advance();
        }
    }
    consume(TokenType::TK_RPAREN, "Expected ')' after mem_alloc args");
    return std::make_unique<MemAllocNode>(target, std::move(size_expr), align, l, c);
}

// ============================================================
//  HW STATEMENT
// ============================================================
NodePtr Parser::parse_hw_stmt() {
    uint32_t l = current().line, c = current().col;
    TokenType op = current().type;
    advance();
    NodeList args;
    if (match(TokenType::TK_LPAREN)) {
        while (!check(TokenType::TK_RPAREN) && !at_end()) {
            args.push_back(parse_expr());
            if (!match(TokenType::TK_COMMA)) break;
        }
        consume(TokenType::TK_RPAREN, "Expected ')' after hw args");
    }
    return std::make_unique<HwIoNode>(op, std::move(args), l, c);
}

// ============================================================
//  GPU STATEMENT
// ============================================================
NodePtr Parser::parse_gpu_stmt() {
    uint32_t l = current().line, c = current().col;
    TokenType op = current().type;
    advance();
    NodeList args;
    if (match(TokenType::TK_LPAREN)) {
        while (!check(TokenType::TK_RPAREN) && !at_end()) {
            args.push_back(parse_expr());
            if (!match(TokenType::TK_COMMA)) break;
        }
        consume(TokenType::TK_RPAREN, "Expected ')' after gpu args");
    }
    return std::make_unique<GpuRenderNode>(op, std::move(args), l, c);
}

// ============================================================
//  EXPRESSION STATEMENT (function call or assignment)
// ============================================================
NodePtr Parser::parse_expr_stmt() {
    uint32_t l = current().line, c = current().col;
    // Check if it's an identifier followed by assignment
    if (check(TokenType::TK_IDENTIFIER) && peek().type == TokenType::TK_ASSIGN) {
        std::string name = advance().value;
        advance();  // consume '='
        NodePtr val = parse_expr();
        return std::make_unique<VarDeclNode>(name, std::move(val), l, c);
    }
    return parse_expr();
}

// ============================================================
//  BLOCK PARSER  (until end!/else/EOF)
// ============================================================
NodeList Parser::parse_block() {
    NodeList stmts;
    while (!at_end()) {
        skip_newlines();
        if (at_end()) break;
        // Block terminators
        if (check(TokenType::TK_END)  ||
            check(TokenType::TK_ELSE)) break;
        auto stmt = parse_statement();
        if (stmt) stmts.push_back(std::move(stmt));
    }
    return stmts;
}

// ============================================================
//  EXPRESSION PARSERS
// ============================================================
NodePtr Parser::parse_expr() {
    return parse_or_expr();
}

NodePtr Parser::parse_or_expr() {
    uint32_t l = current().line, c = current().col;
    auto left = parse_and_expr();
    while (check(TokenType::TK_OR)) {
        std::string op = advance().value;
        auto right = parse_and_expr();
        left = std::make_unique<BinaryNode>(op, std::move(left), std::move(right), l, c);
    }
    return left;
}

NodePtr Parser::parse_and_expr() {
    uint32_t l = current().line, c = current().col;
    auto left = parse_equality_expr();
    while (check(TokenType::TK_AND)) {
        std::string op = advance().value;
        auto right = parse_equality_expr();
        left = std::make_unique<BinaryNode>(op, std::move(left), std::move(right), l, c);
    }
    return left;
}

NodePtr Parser::parse_equality_expr() {
    uint32_t l = current().line, c = current().col;
    auto left = parse_relational_expr();
    while (check(TokenType::TK_EQ) || check(TokenType::TK_NEQ)) {
        std::string op = advance().value;
        auto right = parse_relational_expr();
        left = std::make_unique<BinaryNode>(op, std::move(left), std::move(right), l, c);
    }
    return left;
}

NodePtr Parser::parse_relational_expr() {
    uint32_t l = current().line, c = current().col;
    auto left = parse_additive_expr();
    while (check(TokenType::TK_LT)  || check(TokenType::TK_GT) ||
           check(TokenType::TK_LE)  || check(TokenType::TK_GE)) {
        std::string op = advance().value;
        auto right = parse_additive_expr();
        left = std::make_unique<BinaryNode>(op, std::move(left), std::move(right), l, c);
    }
    return left;
}

NodePtr Parser::parse_additive_expr() {
    uint32_t l = current().line, c = current().col;
    auto left = parse_multiplicative_expr();
    while (check(TokenType::TK_PLUS) || check(TokenType::TK_MINUS)) {
        std::string op = advance().value;
        auto right = parse_multiplicative_expr();
        left = std::make_unique<BinaryNode>(op, std::move(left), std::move(right), l, c);
    }
    return left;
}

NodePtr Parser::parse_multiplicative_expr() {
    uint32_t l = current().line, c = current().col;
    auto left = parse_unary_expr();
    while (check(TokenType::TK_STAR) || check(TokenType::TK_SLASH) ||
           check(TokenType::TK_PERCENT)) {
        std::string op = advance().value;
        auto right = parse_unary_expr();
        left = std::make_unique<BinaryNode>(op, std::move(left), std::move(right), l, c);
    }
    return left;
}

NodePtr Parser::parse_unary_expr() {
    uint32_t l = current().line, c = current().col;

    // Only parse unary minus if followed by a literal or identifier (not a keyword that is a function)
    if (check(TokenType::TK_BANG) || check(TokenType::TK_NOT)) {
        std::string op = advance().value;
        auto operand = parse_unary_expr();
        return std::make_unique<UnaryNode>(op, std::move(operand), l, c);
    }

    // Unary minus: only when followed by a number literal or identifier
    if (check(TokenType::TK_MINUS)) {
        TokenType next = peek().type;
        bool is_unary = (next == TokenType::TK_INTEGER  ||
                         next == TokenType::TK_FLOAT    ||
                         next == TokenType::TK_IDENTIFIER ||
                         next == TokenType::TK_LPAREN);
        if (is_unary) {
            std::string op = advance().value;
            auto operand = parse_primary_expr();
            return std::make_unique<UnaryNode>(op, std::move(operand), l, c);
        }
    }

    return parse_primary_expr();
}

NodePtr Parser::parse_primary_expr() {
    uint32_t l = current().line, c = current().col;
    const Token& tok = current();

    // Math functions
    switch (tok.type) {
        case TokenType::TK_ABS: case TokenType::TK_SQRT: case TokenType::TK_POW:
        case TokenType::TK_SIN: case TokenType::TK_COS:  case TokenType::TK_TAN:
        case TokenType::TK_LOG: case TokenType::TK_LOG2: case TokenType::TK_LOG10:
        case TokenType::TK_EXP: case TokenType::TK_CEIL: case TokenType::TK_FLOOR:
        case TokenType::TK_ROUND: case TokenType::TK_MIN: case TokenType::TK_MAX:
        case TokenType::TK_CLAMP: case TokenType::TK_LERP: case TokenType::TK_SIGMOID:
        case TokenType::TK_RELU: case TokenType::TK_SOFTMAX: case TokenType::TK_TANH_ACT:
        case TokenType::TK_MEAN: case TokenType::TK_MEDIAN: case TokenType::TK_VARIANCE:
        case TokenType::TK_STD_DEV: case TokenType::TK_GCD: case TokenType::TK_LCM:
        case TokenType::TK_FACTORIAL: case TokenType::TK_FIBONACCI:
        case TokenType::TK_FMOD:  case TokenType::TK_MOD:
        case TokenType::TK_DOT_PROD: case TokenType::TK_CROSS: case TokenType::TK_MAGNITUDE:
        case TokenType::TK_NORMALIZE: case TokenType::TK_DISTANCE:
        case TokenType::TK_MATRIX_MUL: case TokenType::TK_FFT: case TokenType::TK_IFFT:
        case TokenType::TK_DERIVATIVE: case TokenType::TK_INTEGRAL:
        case TokenType::TK_PEARSON: case TokenType::TK_ENTROPY:
        case TokenType::TK_COVARIANCE: case TokenType::TK_COMBINATION:
        case TokenType::TK_PERMUTATION: case TokenType::TK_PRIME_CHECK:
        {
            return parse_math_func(tok.type);
        }
        default: break;
    }

    // Integer literal
    if (tok.type == TokenType::TK_INTEGER) {
        advance();
        int64_t v;
        const std::string& s = tok.value;
        if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
            v = std::stoll(s, nullptr, 16);
        else if (s.size() > 2 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B'))
            v = std::stoll(s.substr(2), nullptr, 2);
        else
            v = std::stoll(s);
        return std::make_unique<LiteralNode>(QuartzValue(v), l, c);
    }
    // Float literal
    if (tok.type == TokenType::TK_FLOAT) {
        advance();
        double v = std::stod(tok.value);
        return std::make_unique<LiteralNode>(QuartzValue(v), l, c);
    }
    // String literal
    if (tok.type == TokenType::TK_STRING) {
        advance();
        return std::make_unique<LiteralNode>(QuartzValue(tok.value), l, c);
    }
    // Bool literals
    if (tok.type == TokenType::TK_BOOL_TRUE) {
        advance();
        return std::make_unique<LiteralNode>(QuartzValue(true), l, c);
    }
    if (tok.type == TokenType::TK_BOOL_FALSE) {
        advance();
        return std::make_unique<LiteralNode>(QuartzValue(false), l, c);
    }
    // NaN
    if (tok.type == TokenType::TK_NAN) {
        advance();
        return std::make_unique<LiteralNode>(QuartzValue(std::numeric_limits<double>::quiet_NaN()), l, c);
    }
    // Array literal: [1, 2, 3]
    if (tok.type == TokenType::TK_LBRACKET) {
        return parse_array_literal();
    }
    // Grouped expression
    if (tok.type == TokenType::TK_LPAREN) {
        advance();
        auto expr = parse_expr();
        consume(TokenType::TK_RPAREN, "Expected ')' to close grouped expression");
        return expr;
    }
    // Identifier or function call
    if (tok.type == TokenType::TK_IDENTIFIER) {
        std::string name = advance().value;
        if (check(TokenType::TK_LPAREN)) {
            return parse_call_expr(name);
        }
        return std::make_unique<IdentNode>(name, l, c);
    }
    // Unrecognized — return empty literal
    emit_error("Unexpected token '" + tok.value + "' in expression");
    advance();
    return std::make_unique<LiteralNode>(QuartzValue(), l, c);
}

NodePtr Parser::parse_call_expr(std::string callee) {
    uint32_t l = current().line, c = current().col;
    consume(TokenType::TK_LPAREN, "Expected '(' for function call");
    NodeList args;
    while (!check(TokenType::TK_RPAREN) && !at_end()) {
        args.push_back(parse_expr());
        if (!match(TokenType::TK_COMMA)) break;
    }
    consume(TokenType::TK_RPAREN, "Expected ')' to close function call");
    return std::make_unique<CallNode>(callee, std::move(args), l, c);
}

NodePtr Parser::parse_math_func(TokenType op) {
    uint32_t l = current().line, c = current().col;
    advance();  // consume function name
    consume(TokenType::TK_LPAREN, "Expected '(' after math function");
    NodeList args;
    while (!check(TokenType::TK_RPAREN) && !at_end()) {
        args.push_back(parse_expr());
        if (!match(TokenType::TK_COMMA)) break;
    }
    consume(TokenType::TK_RPAREN, "Expected ')' after math function args");
    return std::make_unique<MathNode>(op, std::move(args), l, c);
}

NodePtr Parser::parse_array_literal() {
    uint32_t l = current().line, c = current().col;
    advance();  // consume '['
    NodeList elements;
    while (!check(TokenType::TK_RBRACKET) && !at_end()) {
        elements.push_back(parse_expr());
        if (!match(TokenType::TK_COMMA)) break;
    }
    consume(TokenType::TK_RBRACKET, "Expected ']' to close array");
    return std::make_unique<ArrayNode>(std::move(elements), l, c);
}

} // namespace quartz
