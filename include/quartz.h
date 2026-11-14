#pragma once
#ifndef QUARTZ_H
#define QUARTZ_H

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <variant>
#include <optional>
#include <stdexcept>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>

// ============================================================
//  QUARTZ LANGUAGE ENGINE v1.0
//  Target: Linux ARM64 (v8a/v9a) + x86_64
//  Built with C++ (90%) + ASM (10%)
// ============================================================

// --- Architecture Detection ---
#if defined(__aarch64__) || defined(__arm64__)
    #define QUARTZ_ARCH_ARM64 1
    #define QUARTZ_ARCH_NAME "ARM64"
#elif defined(__x86_64__) || defined(__amd64__)
    #define QUARTZ_ARCH_X86_64 1
    #define QUARTZ_ARCH_NAME "x86_64"
#else
    #error "Quartz: Unsupported architecture. Only ARM64 and x86_64 are supported."
#endif

// --- Version ---
#define QUARTZ_VERSION_MAJOR 1
#define QUARTZ_VERSION_MINOR 0
#define QUARTZ_VERSION_PATCH 0
#define QUARTZ_VERSION_STR   "1.0.0"

// --- Performance macros ---
#define QZ_INLINE          __attribute__((always_inline)) inline
#define QZ_NOINLINE        __attribute__((noinline))
#define QZ_HOT             __attribute__((hot))
#define QZ_COLD            __attribute__((cold))
#define QZ_LIKELY(x)       __builtin_expect(!!(x), 1)
#define QZ_UNLIKELY(x)     __builtin_expect(!!(x), 0)
#define QZ_ALIGNED(n)      __attribute__((aligned(n)))
#define QZ_PACKED          __attribute__((packed))
#define QZ_UNREACHABLE()   __builtin_unreachable()
#define QZ_PREFETCH(p)     __builtin_prefetch((p), 0, 3)
#define QZ_CACHE_LINE      64

// --- Error handling ---
#define QZ_ASSERT(cond, msg) \
    do { if (QZ_UNLIKELY(!(cond))) { \
        ::quartz::internal::fatal_error(__FILE__, __LINE__, __func__, msg); \
    }} while(0)

namespace quartz {
namespace internal {
    [[noreturn]] void fatal_error(const char* file, int line, const char* func, const char* msg) noexcept;
}

// ============================================================
//  TOKEN TYPES
// ============================================================
enum class TokenType : uint32_t {
    // Literals
    TK_INTEGER,
    TK_FLOAT,
    TK_STRING,
    TK_IDENTIFIER,
    TK_BOOL_TRUE,
    TK_BOOL_FALSE,
    TK_NAN,

    // Core keywords
    TK_INT_MAIN,    // int main():
    TK_PRINT,       // print()
    TK_VAR,         // var
    TK_SPLIT,       // split()
    TK_INPUT,       // input
    TK_IF,          // if
    TK_ELSE,        // else
    TK_THEN,        // then
    TK_FROM,        // from
    TK_WHEN,        // when
    TK_WAIT,        // wait
    TK_REVERSE,     // reverse
    TK_AND,         // and
    TK_OR,          // or
    TK_NOT,         // not
    TK_FUNC,        // func
    TK_END,         // end!
    TK_RETURN,      // return
    TK_POINT,       // point
    TK_SAVE,        // save::
    TK_MOV,         // MOV
    TK_IMPORT,      // import
    TK_USING,       // using()
    TK_GPU_RENDER,  // GPURender::
    TK_ADD,         // add
    TK_UTIL,        // util::

    // Math/Logic keywords (56)
    TK_ABS,         TK_SQRT,        TK_POW,         TK_LOG,
    TK_LOG2,        TK_LOG10,       TK_EXP,         TK_SIN,
    TK_COS,         TK_TAN,         TK_ASIN,        TK_ACOS,
    TK_ATAN,        TK_ATAN2,       TK_CEIL,        TK_FLOOR,
    TK_ROUND,       TK_MIN,         TK_MAX,         TK_CLAMP,
    TK_LERP,        TK_FMOD,        TK_MOD,         TK_DOT_PROD,
    TK_CROSS,       TK_NORMALIZE,   TK_MAGNITUDE,   TK_DISTANCE,
    TK_MATRIX_MUL,  TK_MATRIX_INV,  TK_MATRIX_TRANS,TK_DETERMINANT,
    TK_EIGEN,       TK_CONVOLUTION, TK_FFT,         TK_IFFT,
    TK_SIGMOID,     TK_RELU,        TK_SOFTMAX,     TK_TANH_ACT,
    TK_DERIVATIVE,  TK_INTEGRAL,    TK_GCD,         TK_LCM,
    TK_PRIME_CHECK, TK_FACTORIAL,   TK_FIBONACCI,   TK_COMBINATION,
    TK_PERMUTATION, TK_MEDIAN,      TK_MEAN,        TK_VARIANCE,
    TK_STD_DEV,     TK_COVARIANCE,  TK_PEARSON,     TK_ENTROPY,

    // String keywords (45)
    TK_STR_LEN,     TK_STR_UPPER,   TK_STR_LOWER,   TK_STR_TRIM,
    TK_STR_LTRIM,   TK_STR_RTRIM,   TK_STR_CONCAT,  TK_STR_SUBSTR,
    TK_STR_FIND,    TK_STR_RFIND,   TK_STR_REPLACE, TK_STR_SPLIT,
    TK_STR_JOIN,    TK_STR_STARTS,  TK_STR_ENDS,    TK_STR_CONTAINS,
    TK_STR_COUNT,   TK_STR_REPEAT,  TK_STR_REV,     TK_STR_PAD_L,
    TK_STR_PAD_R,   TK_STR_STRIP,   TK_STR_FORMAT,  TK_STR_ENCODE,
    TK_STR_DECODE,  TK_STR_HASH,    TK_STR_TO_INT,  TK_STR_TO_FLOAT,
    TK_STR_TO_BOOL, TK_STR_FROM_INT,TK_STR_FROM_FLT,TK_STR_COMPARE,
    TK_STR_REGEX,   TK_STR_MATCH,   TK_STR_ESCAPE,  TK_STR_UNESCAPE,
    TK_STR_BASE64E, TK_STR_BASE64D, TK_STR_HEX,     TK_STR_FROM_HEX,
    TK_STR_CHAR_AT, TK_STR_BYTES,   TK_STR_UNICODE, TK_STR_INSERT,
    TK_STR_DELETE,

    // CPU control keywords (34)
    TK_CPU_PAUSE,   TK_CPU_YIELD,   TK_CPU_BARRIER, TK_CPU_FENCE,
    TK_CPU_MFENCE,  TK_CPU_LFENCE,  TK_CPU_SFENCE,  TK_CPU_CLFLUSH,
    TK_CPU_PREFETCH,TK_CPU_CPUID,   TK_CPU_RDTSC,   TK_CPU_RDTSCP,
    TK_CPU_GETCORES,TK_CPU_GETFREQ, TK_CPU_SETAFF,  TK_CPU_GETAFF,
    TK_CPU_SLEEP_NS,TK_CPU_SPIN,    TK_CPU_ATOMIC_L,TK_CPU_ATOMIC_S,
    TK_CPU_CAS,     TK_CPU_FETCHADD,TK_CPU_XCHG,    TK_CPU_LOCK,
    TK_CPU_UNLOCK,  TK_CPU_NOP,     TK_CPU_HLT,     TK_CPU_INT,
    TK_CPU_CLI,     TK_CPU_STI,     TK_CPU_PUSHF,   TK_CPU_POPF,
    TK_CPU_SYSCALL, TK_CPU_SYSRET,

    // Hardware keywords (59)
    TK_HW_MMAP,     TK_HW_MUNMAP,   TK_HW_IOREAD8,  TK_HW_IOREAD16,
    TK_HW_IOREAD32, TK_HW_IOREAD64, TK_HW_IOWRITE8, TK_HW_IOWRITE16,
    TK_HW_IOWRITE32,TK_HW_IOWRITE64,TK_HW_DMA_ALLOC,TK_HW_DMA_FREE,
    TK_HW_DMA_SYNC, TK_HW_IRQ_REG, TK_HW_IRQ_UNREG,TK_HW_IRQ_EN,
    TK_HW_IRQ_DIS,  TK_HW_TIMER_NS, TK_HW_TIMER_US, TK_HW_TIMER_MS,
    TK_HW_PORT_IN,  TK_HW_PORT_OUT, TK_HW_DEVOPEN,  TK_HW_DEVCLOSE,
    TK_HW_DEVREAD,  TK_HW_DEVWRITE, TK_HW_DEVIOCTL, TK_HW_DEVMAP,
    TK_HW_DEVSYNC,  TK_HW_DEVAIO,   TK_HW_NETOPEN,  TK_HW_NETCLOSE,
    TK_HW_NETSEND,  TK_HW_NETRECV,  TK_HW_NETBIND,  TK_HW_NETLISTEN,
    TK_HW_NETACCEPT,TK_HW_NETCONN,  TK_HW_NETPOLL,  TK_HW_NETFLUSH,
    TK_HW_SOUNDOPEN,TK_HW_SOUNDPLAY,TK_HW_SOUNDSTOP,TK_HW_SOUNDVOL,
    TK_HW_DISPOPEN, TK_HW_DISPCLOSE,TK_HW_DISPFLUSH,TK_HW_DISPMODE,
    TK_HW_KBDREAD,  TK_HW_MOUSEREAD,TK_HW_GAMEPAD,  TK_HW_CAMERA,
    TK_HW_SENSOR,   TK_HW_GPIO_SET, TK_HW_GPIO_GET, TK_HW_GPIO_MODE,
    TK_HW_SPI_XFER, TK_HW_I2C_XFER, TK_HW_UART_SEND,

    // Data keywords (33)
    TK_ARRAY,       TK_DICT,        TK_SET,         TK_QUEUE,
    TK_STACK,       TK_LIST,        TK_TUPLE,       TK_MAP,
    TK_PUSH,        TK_POP,         TK_PEEK,        TK_INSERT,
    TK_REMOVE,      TK_CLEAR,       TK_SIZE,        TK_EMPTY,
    TK_FRONT,       TK_BACK,        TK_SORT,        TK_RSORT,
    TK_FILTER,      TK_REDUCE,      TK_MAP_FN,      TK_FOREACH,
    TK_FIND_IDX,    TK_SLICE,       TK_CONCAT_ARR,  TK_UNIQUE,
    TK_FLATTEN,     TK_ZIP,         TK_ENUMERATE,   TK_REVERSE_ARR,
    TK_CONTAINS_VAL,

    // Text manipulation keywords (66)
    TK_TEXT_WRAP,   TK_TEXT_CENTER, TK_TEXT_LEFT,   TK_TEXT_RIGHT,
    TK_TEXT_BOLD,   TK_TEXT_ITALIC, TK_TEXT_COLOR,  TK_TEXT_BLINK,
    TK_TEXT_UNDER,  TK_TEXT_STRIKE, TK_TEXT_RESET,  TK_TEXT_WIDTH,
    TK_TEXT_COLS,   TK_TEXT_ROWS,   TK_TEXT_CURSOR, TK_TEXT_CLEAR,
    TK_TEXT_SCROLL, TK_TEXT_BOX,    TK_TEXT_TABLE,  TK_TEXT_BORDER,
    TK_TEXT_HEADER, TK_TEXT_FOOTER, TK_TEXT_INDENT, TK_TEXT_DEDENT,
    TK_TEXT_LINE,   TK_TEXT_NEWLINE,TK_TEXT_TAB,    TK_TEXT_SPACE,
    TK_TEXT_PRINTF, TK_TEXT_SCANF,  TK_TEXT_PUTS,   TK_TEXT_GETS,
    TK_TEXT_PUTCHAR,TK_TEXT_GETCHAR,TK_TEXT_FWRITE, TK_TEXT_FREAD,
    TK_TEXT_SSCANF, TK_TEXT_SNPRINTF,TK_TEXT_VPRINTF,TK_TEXT_VSPRINTF,
    TK_TEXT_STRTOK, TK_TEXT_STRSPN, TK_TEXT_STRCSPN,TK_TEXT_STRPBRK,
    TK_TEXT_MEMCPY, TK_TEXT_MEMMOVE,TK_TEXT_MEMSET, TK_TEXT_MEMCMP,
    TK_TEXT_MEMCHR, TK_TEXT_STRCAT, TK_TEXT_STRNCAT,TK_TEXT_STRCMP,
    TK_TEXT_STRNCMP,TK_TEXT_STRCPY, TK_TEXT_STRNCPY,TK_TEXT_STRLEN,
    TK_TEXT_SPRINTF,TK_TEXT_ATOI,   TK_TEXT_ATOF,   TK_TEXT_ITOA,
    TK_TEXT_FTOA,   TK_TEXT_DTOA,   TK_TEXT_LTRIM_WS,TK_TEXT_RTRIM_WS,
    TK_TEXT_PRINT_HEX, TK_TEXT_PRINT_BIN,

    // File/Folder keywords (77)
    TK_FILE_OPEN,   TK_FILE_CLOSE,  TK_FILE_READ,   TK_FILE_WRITE,
    TK_FILE_APPEND, TK_FILE_SEEK,   TK_FILE_TELL,   TK_FILE_SIZE,
    TK_FILE_TRUNC,  TK_FILE_FLUSH,  TK_FILE_SYNC,   TK_FILE_EOF,
    TK_FILE_EXISTS, TK_FILE_DELETE, TK_FILE_RENAME, TK_FILE_COPY,
    TK_FILE_MOVE,   TK_FILE_CHMOD,  TK_FILE_CHOWN,  TK_FILE_STAT,
    TK_FILE_READALL,TK_FILE_WRITEALL,TK_FILE_READLINE,TK_FILE_WRITELINE,
    TK_FILE_READBIN,TK_FILE_WRITEBIN,TK_FILE_LOCK,  TK_FILE_UNLOCK,
    TK_FILE_MMAP,   TK_FILE_MUNMAP, TK_FILE_TEMPNAME,TK_FILE_TEMPOPEN,
    TK_DIR_OPEN,    TK_DIR_CLOSE,   TK_DIR_READ,    TK_DIR_CREATE,
    TK_DIR_DELETE,  TK_DIR_EXISTS,  TK_DIR_LIST,    TK_DIR_CWD,
    TK_DIR_CHDIR,   TK_DIR_MKDIR_P, TK_DIR_RMDIR_R, TK_DIR_GLOB,
    TK_DIR_WATCH,   TK_PATH_JOIN,   TK_PATH_SPLIT,  TK_PATH_BASENAME,
    TK_PATH_DIRNAME,TK_PATH_EXTNAME,TK_PATH_ABS,    TK_PATH_REL,
    TK_PATH_NORM,   TK_PATH_ISABS,  TK_PATH_ISREL,  TK_FS_SPACE,
    TK_FS_USED,     TK_FS_FREE,     TK_FS_TYPE,     TK_FS_MOUNT,
    TK_FS_UMOUNT,   TK_FS_SYNC,     TK_PIPE_OPEN,   TK_PIPE_CLOSE,
    TK_PIPE_READ,   TK_PIPE_WRITE,  TK_PROC_EXEC,   TK_PROC_WAIT,
    TK_PROC_KILL,   TK_PROC_PID,    TK_PROC_ENV_GET,TK_PROC_ENV_SET,
    TK_ENV_ARGS,    TK_SYM_LINK,    TK_HARD_LINK,   TK_READ_LINK,

    // ASM keywords (12)
    TK_ASM_MOV,     TK_ASM_ADD,     TK_ASM_SUB,     TK_ASM_MUL,
    TK_ASM_DIV,     TK_ASM_AND,     TK_ASM_OR,      TK_ASM_XOR,
    TK_ASM_SHL,     TK_ASM_SHR,     TK_ASM_CMP,     TK_ASM_JMP,

    // GPU control keywords (54)
    TK_GPU_INIT,    TK_GPU_SHUTDOWN,TK_GPU_ALLOC,   TK_GPU_FREE,
    TK_GPU_UPLOAD,  TK_GPU_DOWNLOAD,TK_GPU_COPY,    TK_GPU_MEMSET,
    TK_GPU_SYNC,    TK_GPU_ASYNC,   TK_GPU_STREAM,  TK_GPU_EVENT,
    TK_GPU_KERNEL,  TK_GPU_LAUNCH,  TK_GPU_GRID,    TK_GPU_BLOCK,
    TK_GPU_THREADS, TK_GPU_WARP,    TK_GPU_SHARED,  TK_GPU_LOCAL,
    TK_GPU_GLOBAL,  TK_GPU_ATOMIC,  TK_GPU_BARRIER_G,TK_GPU_REDUCE,
    TK_GPU_SCAN,    TK_GPU_SORT_G,  TK_GPU_MATMUL,  TK_GPU_CONV2D,
    TK_GPU_MAXPOOL, TK_GPU_AVGPOOL, TK_GPU_RELU_G,  TK_GPU_SOFTMAX_G,
    TK_GPU_BATCHNORM,TK_GPU_DROPOUT,TK_GPU_EMBED,   TK_GPU_ATTENTION,
    TK_GPU_FFT_G,   TK_GPU_IFFT_G,  TK_GPU_HIST,    TK_GPU_FILTER_G,
    TK_GPU_WARP_RED,TK_GPU_LDMATRIX,TK_GPU_TENSOR,  TK_GPU_WMMA,
    TK_GPU_MMA,     TK_GPU_PERF,    TK_GPU_OCCUPANCY,TK_GPU_REG,
    TK_GPU_L1,      TK_GPU_L2,      TK_GPU_BANDWIDTH,TK_GPU_FLOPS,
    TK_GPU_INFO,    TK_GPU_DEVICE,

    // GPU render keywords (67)
    TK_GR_VERTEX,   TK_GR_FRAGMENT, TK_GR_COMPUTE,  TK_GR_GEOMETRY,
    TK_GR_TESS,     TK_GR_MESH,     TK_GR_RAY,      TK_GR_PIPELINE,
    TK_GR_FRAMEBUF, TK_GR_RENDERBUF,TK_GR_TEXTURE,  TK_GR_SAMPLER,
    TK_GR_UNIFORM,  TK_GR_SSBO,     TK_GR_VAO,      TK_GR_VBO,
    TK_GR_IBO,      TK_GR_DRAW,     TK_GR_DRAWIDX,  TK_GR_DRAWINST,
    TK_GR_DRAWINDIR,TK_GR_BLIT,     TK_GR_RESOLVE,  TK_GR_CLEAR_G,
    TK_GR_VIEWPORT, TK_GR_SCISSOR,  TK_GR_DEPTH,    TK_GR_STENCIL,
    TK_GR_BLEND,    TK_GR_CULL,     TK_GR_WINDING,  TK_GR_POLYMODE,
    TK_GR_LINE_W,   TK_GR_POINT_S,  TK_GR_MSAA,     TK_GR_ANISO,
    TK_GR_MIP,      TK_GR_LOD,      TK_GR_SHADOW,   TK_GR_SHADOWMAP,
    TK_GR_SSAO,     TK_GR_BLOOM,    TK_GR_DOF,      TK_GR_MOTION_BL,
    TK_GR_HDR,      TK_GR_TONEMAP,  TK_GR_GAMMA,    TK_GR_COLOR_SP,
    TK_GR_POST_FX,  TK_GR_PRESENT,  TK_GR_SWAP,     TK_GR_VSYNC,
    TK_GR_WIREFRAME,TK_GR_SKYBOX,   TK_GR_CUBEMAP,  TK_GR_PROBE,
    TK_GR_GI,       TK_GR_REFLEC,   TK_GR_REFRAC,   TK_GR_CAUSTIC,
    TK_GR_SUBSURFACE,TK_GR_PARALLAX,TK_GR_DISPLACE, TK_GR_MORPH,
    TK_GR_BONES,    TK_GR_SKIN,     TK_GR_PARTICLES,

    // Memory RAM keywords (45)
    TK_MEM_ALLOC,   TK_MEM_FREE,    TK_MEM_REALLOC, TK_MEM_CALLOC,
    TK_MEM_ALIGN,   TK_MEM_ZERO,    TK_MEM_COPY,    TK_MEM_MOVE,
    TK_MEM_SET,     TK_MEM_COMPARE, TK_MEM_FIND,    TK_MEM_POOL,
    TK_MEM_ARENA,   TK_MEM_STACK_A, TK_MEM_PAGE,    TK_MEM_HUGE,
    TK_MEM_LOCK_P,  TK_MEM_UNLOCK_P,TK_MEM_PROTECT, TK_MEM_ADVISE,
    TK_MEM_SYNC_M,  TK_MEM_ASYNC_M, TK_MEM_BARRIER_M,TK_MEM_FENCE_M,
    TK_MEM_PREFETCH_M,TK_MEM_FLUSH_M,TK_MEM_INVALIDATE,TK_MEM_WRITEBACK,
    TK_MEM_ATOMIC_A,TK_MEM_ATOMIC_S,TK_MEM_CAS_M,  TK_MEM_FETCHADD_M,
    TK_MEM_XCHG_M,  TK_MEM_REF_CNT, TK_MEM_WEAK_REF,TK_MEM_SHARED,
    TK_MEM_UNIQUE,  TK_MEM_BORROW,  TK_MEM_LIFETIME,TK_MEM_SCOPE,
    TK_MEM_RAII,    TK_MEM_GC_MARK, TK_MEM_GC_SWEEP,TK_MEM_COMPACT,
    TK_MEM_DEFRAG,

    // Operators
    TK_PLUS,        TK_MINUS,       TK_STAR,        TK_SLASH,
    TK_PERCENT,     TK_CARET,       TK_AMPERSAND,   TK_PIPE,
    TK_TILDE,       TK_BANG,        TK_QUESTION,    TK_AT,
    TK_HASH,        TK_DOLLAR,      TK_UNDERSCORE,

    // Comparisons
    TK_EQ,          TK_NEQ,         TK_LT,          TK_GT,
    TK_LE,          TK_GE,          TK_ASSIGN,

    // Delimiters
    TK_LPAREN,      TK_RPAREN,      TK_LBRACE,      TK_RBRACE,
    TK_LBRACKET,    TK_RBRACKET,    TK_COMMA,       TK_DOT,
    TK_COLON,       TK_SEMICOLON,   TK_NEWLINE,     TK_INDENT,
    TK_DEDENT,      TK_EOF,

    // Special
    TK_ANNOTATION,  TK_DOUBLE_COLON,TK_ARROW,       TK_DOUBLE_ARROW,
    TK_COMMENT,     TK_UNKNOWN
};

// ============================================================
//  TOKEN STRUCTURE
// ============================================================
struct QZ_ALIGNED(32) Token {
    TokenType   type;
    uint32_t    line;
    uint32_t    col;
    uint32_t    len;
    std::string value;

    Token() noexcept : type(TokenType::TK_UNKNOWN), line(0), col(0), len(0) {}
    Token(TokenType t, std::string v, uint32_t l, uint32_t c)
        : type(t), line(l), col(c), len(static_cast<uint32_t>(v.size())), value(std::move(v)) {}
};

// ============================================================
//  VALUE TYPES
// ============================================================
struct QuartzValue {
    enum class Kind : uint8_t {
        NIL, BOOL, INT64, FLOAT64, STRING, ARRAY, DICT, POINTER, FUNCTION
    } kind;

    union {
        bool        b;
        int64_t     i;
        double      f;
        void*       ptr;
    } data;
    std::string              str_data;
    std::vector<QuartzValue> arr_data;
    std::unordered_map<std::string, QuartzValue> dict_data;

    QuartzValue() noexcept : kind(Kind::NIL) { data.i = 0; }
    explicit QuartzValue(bool v)    noexcept : kind(Kind::BOOL)    { data.b = v; }
    explicit QuartzValue(int64_t v) noexcept : kind(Kind::INT64)   { data.i = v; }
    explicit QuartzValue(double v)  noexcept : kind(Kind::FLOAT64) { data.f = v; }
    explicit QuartzValue(std::string v) : kind(Kind::STRING), str_data(std::move(v)) { data.i = 0; }
    explicit QuartzValue(void* v)   noexcept : kind(Kind::POINTER) { data.ptr = v; }

    bool   is_nil()     const noexcept { return kind == Kind::NIL; }
    bool   is_bool()    const noexcept { return kind == Kind::BOOL; }
    bool   is_int()     const noexcept { return kind == Kind::INT64; }
    bool   is_float()   const noexcept { return kind == Kind::FLOAT64; }
    bool   is_string()  const noexcept { return kind == Kind::STRING; }
    bool   is_array()   const noexcept { return kind == Kind::ARRAY; }
    bool   is_dict()    const noexcept { return kind == Kind::DICT; }
    bool   is_pointer() const noexcept { return kind == Kind::POINTER; }

    std::string to_string() const;
    double      to_numeric() const noexcept;
    bool        to_bool()   const noexcept;
};

} // namespace quartz

#endif // QUARTZ_H
