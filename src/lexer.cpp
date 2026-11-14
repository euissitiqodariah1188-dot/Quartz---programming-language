#include "../include/lexer.h"
#include <cctype>
#include <stdexcept>

namespace quartz {

// ============================================================
//  KEYWORD MAP — all Quartz keywords
// ============================================================
static const std::unordered_map<std::string, TokenType>& keyword_map() {
    static const std::unordered_map<std::string, TokenType> kmap = {
        // Core
        {"main",        TokenType::TK_INT_MAIN},
        {"print",       TokenType::TK_PRINT},
        {"var",         TokenType::TK_VAR},
        {"split",       TokenType::TK_SPLIT},
        {"input",       TokenType::TK_INPUT},
        {"if",          TokenType::TK_IF},
        {"else",        TokenType::TK_ELSE},
        {"then",        TokenType::TK_THEN},
        {"from",        TokenType::TK_FROM},
        {"when",        TokenType::TK_WHEN},
        {"wait",        TokenType::TK_WAIT},
        {"reverse",     TokenType::TK_REVERSE},
        {"false",       TokenType::TK_BOOL_FALSE},
        {"true",        TokenType::TK_BOOL_TRUE},
        {"Nan",         TokenType::TK_NAN},
        {"NaN",         TokenType::TK_NAN},
        {"and",         TokenType::TK_AND},
        {"or",          TokenType::TK_OR},
        {"not",         TokenType::TK_NOT},
        {"func",        TokenType::TK_FUNC},
        {"end!",        TokenType::TK_END},
        {"end",         TokenType::TK_END},
        {"return",      TokenType::TK_RETURN},
        {"point",       TokenType::TK_POINT},
        {"MOV",         TokenType::TK_MOV},
        {"import",      TokenType::TK_IMPORT},
        {"using",       TokenType::TK_USING},
        {"add",         TokenType::TK_ADD},
        {"save",        TokenType::TK_SAVE},
        {"GPURender",   TokenType::TK_GPU_RENDER},
        {"util",        TokenType::TK_UTIL},
        // Math/Logic (56)
        {"abs",         TokenType::TK_ABS},
        {"sqrt",        TokenType::TK_SQRT},
        {"pow",         TokenType::TK_POW},
        {"log",         TokenType::TK_LOG},
        {"log2",        TokenType::TK_LOG2},
        {"log10",       TokenType::TK_LOG10},
        {"exp",         TokenType::TK_EXP},
        {"sin",         TokenType::TK_SIN},
        {"cos",         TokenType::TK_COS},
        {"tan",         TokenType::TK_TAN},
        {"asin",        TokenType::TK_ASIN},
        {"acos",        TokenType::TK_ACOS},
        {"atan",        TokenType::TK_ATAN},
        {"atan2",       TokenType::TK_ATAN2},
        {"ceil",        TokenType::TK_CEIL},
        {"floor",       TokenType::TK_FLOOR},
        {"round",       TokenType::TK_ROUND},
        {"min",         TokenType::TK_MIN},
        {"max",         TokenType::TK_MAX},
        {"clamp",       TokenType::TK_CLAMP},
        {"lerp",        TokenType::TK_LERP},
        {"fmod",        TokenType::TK_FMOD},
        {"mod",         TokenType::TK_MOD},
        {"dot",         TokenType::TK_DOT_PROD},
        {"cross",       TokenType::TK_CROSS},
        {"normalize",   TokenType::TK_NORMALIZE},
        {"magnitude",   TokenType::TK_MAGNITUDE},
        {"distance",    TokenType::TK_DISTANCE},
        {"matmul",      TokenType::TK_MATRIX_MUL},
        {"matinv",      TokenType::TK_MATRIX_INV},
        {"mattrans",    TokenType::TK_MATRIX_TRANS},
        {"det",         TokenType::TK_DETERMINANT},
        {"eigen",       TokenType::TK_EIGEN},
        {"conv",        TokenType::TK_CONVOLUTION},
        {"fft",         TokenType::TK_FFT},
        {"ifft",        TokenType::TK_IFFT},
        {"sigmoid",     TokenType::TK_SIGMOID},
        {"relu",        TokenType::TK_RELU},
        {"softmax",     TokenType::TK_SOFTMAX},
        {"tanh_act",    TokenType::TK_TANH_ACT},
        {"derivative",  TokenType::TK_DERIVATIVE},
        {"integral",    TokenType::TK_INTEGRAL},
        {"gcd",         TokenType::TK_GCD},
        {"lcm",         TokenType::TK_LCM},
        {"isprime",     TokenType::TK_PRIME_CHECK},
        {"factorial",   TokenType::TK_FACTORIAL},
        {"fibonacci",   TokenType::TK_FIBONACCI},
        {"comb",        TokenType::TK_COMBINATION},
        {"perm",        TokenType::TK_PERMUTATION},
        {"median",      TokenType::TK_MEDIAN},
        {"mean",        TokenType::TK_MEAN},
        {"variance",    TokenType::TK_VARIANCE},
        {"stddev",      TokenType::TK_STD_DEV},
        {"covariance",  TokenType::TK_COVARIANCE},
        {"pearson",     TokenType::TK_PEARSON},
        {"entropy",     TokenType::TK_ENTROPY},
        // String (45)
        {"strlen",      TokenType::TK_STR_LEN},
        {"strupper",    TokenType::TK_STR_UPPER},
        {"strlower",    TokenType::TK_STR_LOWER},
        {"strtrim",     TokenType::TK_STR_TRIM},
        {"strltrim",    TokenType::TK_STR_LTRIM},
        {"strrtrim",    TokenType::TK_STR_RTRIM},
        {"strcat",      TokenType::TK_STR_CONCAT},
        {"substr",      TokenType::TK_STR_SUBSTR},
        {"strfind",     TokenType::TK_STR_FIND},
        {"strrfind",    TokenType::TK_STR_RFIND},
        {"strreplace",  TokenType::TK_STR_REPLACE},
        {"strsplit",    TokenType::TK_STR_SPLIT},
        {"strjoin",     TokenType::TK_STR_JOIN},
        {"strstarts",   TokenType::TK_STR_STARTS},
        {"strends",     TokenType::TK_STR_ENDS},
        {"strcontains", TokenType::TK_STR_CONTAINS},
        {"strcount",    TokenType::TK_STR_COUNT},
        {"strrepeat",   TokenType::TK_STR_REPEAT},
        {"strrev",      TokenType::TK_STR_REV},
        {"strpadl",     TokenType::TK_STR_PAD_L},
        {"strpadr",     TokenType::TK_STR_PAD_R},
        {"strstrip",    TokenType::TK_STR_STRIP},
        {"strformat",   TokenType::TK_STR_FORMAT},
        {"strencode",   TokenType::TK_STR_ENCODE},
        {"strdecode",   TokenType::TK_STR_DECODE},
        {"strhash",     TokenType::TK_STR_HASH},
        {"strtoint",    TokenType::TK_STR_TO_INT},
        {"strtofloat",  TokenType::TK_STR_TO_FLOAT},
        {"strtobool",   TokenType::TK_STR_TO_BOOL},
        {"strfromint",  TokenType::TK_STR_FROM_INT},
        {"strfromfloat",TokenType::TK_STR_FROM_FLT},
        {"strcmp",      TokenType::TK_STR_COMPARE},
        {"strregex",    TokenType::TK_STR_REGEX},
        {"strmatch",    TokenType::TK_STR_MATCH},
        {"strescape",   TokenType::TK_STR_ESCAPE},
        {"strunescape", TokenType::TK_STR_UNESCAPE},
        {"strb64enc",   TokenType::TK_STR_BASE64E},
        {"strb64dec",   TokenType::TK_STR_BASE64D},
        {"strhex",      TokenType::TK_STR_HEX},
        {"strfromhex",  TokenType::TK_STR_FROM_HEX},
        {"charat",      TokenType::TK_STR_CHAR_AT},
        {"strbytes",    TokenType::TK_STR_BYTES},
        {"strunicode",  TokenType::TK_STR_UNICODE},
        {"strinsert",   TokenType::TK_STR_INSERT},
        {"strdelete",   TokenType::TK_STR_DELETE},
        // CPU control (34)
        {"cpu_pause",   TokenType::TK_CPU_PAUSE},
        {"cpu_yield",   TokenType::TK_CPU_YIELD},
        {"cpu_barrier", TokenType::TK_CPU_BARRIER},
        {"cpu_fence",   TokenType::TK_CPU_FENCE},
        {"cpu_mfence",  TokenType::TK_CPU_MFENCE},
        {"cpu_lfence",  TokenType::TK_CPU_LFENCE},
        {"cpu_sfence",  TokenType::TK_CPU_SFENCE},
        {"cpu_clflush", TokenType::TK_CPU_CLFLUSH},
        {"cpu_prefetch",TokenType::TK_CPU_PREFETCH},
        {"cpuid",       TokenType::TK_CPU_CPUID},
        {"rdtsc",       TokenType::TK_CPU_RDTSC},
        {"rdtscp",      TokenType::TK_CPU_RDTSCP},
        {"getcores",    TokenType::TK_CPU_GETCORES},
        {"getfreq",     TokenType::TK_CPU_GETFREQ},
        {"setaff",      TokenType::TK_CPU_SETAFF},
        {"getaff",      TokenType::TK_CPU_GETAFF},
        {"sleep_ns",    TokenType::TK_CPU_SLEEP_NS},
        {"cpu_spin",    TokenType::TK_CPU_SPIN},
        {"atomic_load", TokenType::TK_CPU_ATOMIC_L},
        {"atomic_store",TokenType::TK_CPU_ATOMIC_S},
        {"cas",         TokenType::TK_CPU_CAS},
        {"fetchadd",    TokenType::TK_CPU_FETCHADD},
        {"xchg",        TokenType::TK_CPU_XCHG},
        {"cpu_lock",    TokenType::TK_CPU_LOCK},
        {"cpu_unlock",  TokenType::TK_CPU_UNLOCK},
        {"nop",         TokenType::TK_CPU_NOP},
        {"hlt",         TokenType::TK_CPU_HLT},
        {"cpu_int",     TokenType::TK_CPU_INT},
        {"cli",         TokenType::TK_CPU_CLI},
        {"sti",         TokenType::TK_CPU_STI},
        {"pushf",       TokenType::TK_CPU_PUSHF},
        {"popf",        TokenType::TK_CPU_POPF},
        {"syscall",     TokenType::TK_CPU_SYSCALL},
        {"sysret",      TokenType::TK_CPU_SYSRET},
        // Hardware (59)
        {"hw_mmap",     TokenType::TK_HW_MMAP},
        {"hw_munmap",   TokenType::TK_HW_MUNMAP},
        {"ioread8",     TokenType::TK_HW_IOREAD8},
        {"ioread16",    TokenType::TK_HW_IOREAD16},
        {"ioread32",    TokenType::TK_HW_IOREAD32},
        {"ioread64",    TokenType::TK_HW_IOREAD64},
        {"iowrite8",    TokenType::TK_HW_IOWRITE8},
        {"iowrite16",   TokenType::TK_HW_IOWRITE16},
        {"iowrite32",   TokenType::TK_HW_IOWRITE32},
        {"iowrite64",   TokenType::TK_HW_IOWRITE64},
        {"dma_alloc",   TokenType::TK_HW_DMA_ALLOC},
        {"dma_free",    TokenType::TK_HW_DMA_FREE},
        {"dma_sync",    TokenType::TK_HW_DMA_SYNC},
        {"irq_reg",     TokenType::TK_HW_IRQ_REG},
        {"irq_unreg",   TokenType::TK_HW_IRQ_UNREG},
        {"irq_en",      TokenType::TK_HW_IRQ_EN},
        {"irq_dis",     TokenType::TK_HW_IRQ_DIS},
        {"timer_ns",    TokenType::TK_HW_TIMER_NS},
        {"timer_us",    TokenType::TK_HW_TIMER_US},
        {"timer_ms",    TokenType::TK_HW_TIMER_MS},
        {"port_in",     TokenType::TK_HW_PORT_IN},
        {"port_out",    TokenType::TK_HW_PORT_OUT},
        {"devopen",     TokenType::TK_HW_DEVOPEN},
        {"devclose",    TokenType::TK_HW_DEVCLOSE},
        {"devread",     TokenType::TK_HW_DEVREAD},
        {"devwrite",    TokenType::TK_HW_DEVWRITE},
        {"devioctl",    TokenType::TK_HW_DEVIOCTL},
        {"devmap",      TokenType::TK_HW_DEVMAP},
        {"devsync",     TokenType::TK_HW_DEVSYNC},
        {"devaio",      TokenType::TK_HW_DEVAIO},
        {"netopen",     TokenType::TK_HW_NETOPEN},
        {"netclose",    TokenType::TK_HW_NETCLOSE},
        {"netsend",     TokenType::TK_HW_NETSEND},
        {"netrecv",     TokenType::TK_HW_NETRECV},
        {"netbind",     TokenType::TK_HW_NETBIND},
        {"netlisten",   TokenType::TK_HW_NETLISTEN},
        {"netaccept",   TokenType::TK_HW_NETACCEPT},
        {"netconn",     TokenType::TK_HW_NETCONN},
        {"netpoll",     TokenType::TK_HW_NETPOLL},
        {"netflush",    TokenType::TK_HW_NETFLUSH},
        {"soundopen",   TokenType::TK_HW_SOUNDOPEN},
        {"soundplay",   TokenType::TK_HW_SOUNDPLAY},
        {"soundstop",   TokenType::TK_HW_SOUNDSTOP},
        {"soundvol",    TokenType::TK_HW_SOUNDVOL},
        {"dispopen",    TokenType::TK_HW_DISPOPEN},
        {"dispclose",   TokenType::TK_HW_DISPCLOSE},
        {"dispflush",   TokenType::TK_HW_DISPFLUSH},
        {"dispmode",    TokenType::TK_HW_DISPMODE},
        {"kbdread",     TokenType::TK_HW_KBDREAD},
        {"mouseread",   TokenType::TK_HW_MOUSEREAD},
        {"gamepad",     TokenType::TK_HW_GAMEPAD},
        {"camera",      TokenType::TK_HW_CAMERA},
        {"sensor",      TokenType::TK_HW_SENSOR},
        {"gpio_set",    TokenType::TK_HW_GPIO_SET},
        {"gpio_get",    TokenType::TK_HW_GPIO_GET},
        {"gpio_mode",   TokenType::TK_HW_GPIO_MODE},
        {"spi_xfer",    TokenType::TK_HW_SPI_XFER},
        {"i2c_xfer",    TokenType::TK_HW_I2C_XFER},
        {"uart_send",   TokenType::TK_HW_UART_SEND},
        // Data (33)
        {"array",       TokenType::TK_ARRAY},
        {"dict",        TokenType::TK_DICT},
        {"set",         TokenType::TK_SET},
        {"queue",       TokenType::TK_QUEUE},
        {"stack",       TokenType::TK_STACK},
        {"list",        TokenType::TK_LIST},
        {"tuple",       TokenType::TK_TUPLE},
        {"map",         TokenType::TK_MAP},
        {"push",        TokenType::TK_PUSH},
        {"pop",         TokenType::TK_POP},
        {"peek",        TokenType::TK_PEEK},
        {"insert",      TokenType::TK_INSERT},
        {"remove",      TokenType::TK_REMOVE},
        {"clear",       TokenType::TK_CLEAR},
        {"size",        TokenType::TK_SIZE},
        {"empty",       TokenType::TK_EMPTY},
        {"front",       TokenType::TK_FRONT},
        {"back",        TokenType::TK_BACK},
        {"sort",        TokenType::TK_SORT},
        {"rsort",       TokenType::TK_RSORT},
        {"filter",      TokenType::TK_FILTER},
        {"reduce",      TokenType::TK_REDUCE},
        {"mapfn",       TokenType::TK_MAP_FN},
        {"foreach",     TokenType::TK_FOREACH},
        {"findidx",     TokenType::TK_FIND_IDX},
        {"slice",       TokenType::TK_SLICE},
        {"concatarr",   TokenType::TK_CONCAT_ARR},
        {"unique",      TokenType::TK_UNIQUE},
        {"flatten",     TokenType::TK_FLATTEN},
        {"zip",         TokenType::TK_ZIP},
        {"enumerate",   TokenType::TK_ENUMERATE},
        {"reversearr",  TokenType::TK_REVERSE_ARR},
        {"containsval", TokenType::TK_CONTAINS_VAL},
        // File/Folder (77)
        {"file_open",   TokenType::TK_FILE_OPEN},
        {"file_close",  TokenType::TK_FILE_CLOSE},
        {"file_read",   TokenType::TK_FILE_READ},
        {"file_write",  TokenType::TK_FILE_WRITE},
        {"file_append", TokenType::TK_FILE_APPEND},
        {"file_seek",   TokenType::TK_FILE_SEEK},
        {"file_tell",   TokenType::TK_FILE_TELL},
        {"file_size",   TokenType::TK_FILE_SIZE},
        {"file_trunc",  TokenType::TK_FILE_TRUNC},
        {"file_flush",  TokenType::TK_FILE_FLUSH},
        {"file_sync",   TokenType::TK_FILE_SYNC},
        {"file_eof",    TokenType::TK_FILE_EOF},
        {"file_exists", TokenType::TK_FILE_EXISTS},
        {"file_delete", TokenType::TK_FILE_DELETE},
        {"file_rename", TokenType::TK_FILE_RENAME},
        {"file_copy",   TokenType::TK_FILE_COPY},
        {"file_move",   TokenType::TK_FILE_MOVE},
        {"file_chmod",  TokenType::TK_FILE_CHMOD},
        {"file_chown",  TokenType::TK_FILE_CHOWN},
        {"file_stat",   TokenType::TK_FILE_STAT},
        {"file_readall",TokenType::TK_FILE_READALL},
        {"file_writeall",TokenType::TK_FILE_WRITEALL},
        {"file_readline",TokenType::TK_FILE_READLINE},
        {"file_writeln",TokenType::TK_FILE_WRITELINE},
        {"file_readbin",TokenType::TK_FILE_READBIN},
        {"file_writebin",TokenType::TK_FILE_WRITEBIN},
        {"file_lock",   TokenType::TK_FILE_LOCK},
        {"file_unlock", TokenType::TK_FILE_UNLOCK},
        {"file_mmap",   TokenType::TK_FILE_MMAP},
        {"file_munmap", TokenType::TK_FILE_MUNMAP},
        {"tempname",    TokenType::TK_FILE_TEMPNAME},
        {"tempopen",    TokenType::TK_FILE_TEMPOPEN},
        {"dir_open",    TokenType::TK_DIR_OPEN},
        {"dir_close",   TokenType::TK_DIR_CLOSE},
        {"dir_read",    TokenType::TK_DIR_READ},
        {"dir_create",  TokenType::TK_DIR_CREATE},
        {"dir_delete",  TokenType::TK_DIR_DELETE},
        {"dir_exists",  TokenType::TK_DIR_EXISTS},
        {"dir_list",    TokenType::TK_DIR_LIST},
        {"cwd",         TokenType::TK_DIR_CWD},
        {"chdir",       TokenType::TK_DIR_CHDIR},
        {"mkdir_p",     TokenType::TK_DIR_MKDIR_P},
        {"rmdir_r",     TokenType::TK_DIR_RMDIR_R},
        {"glob",        TokenType::TK_DIR_GLOB},
        {"dir_watch",   TokenType::TK_DIR_WATCH},
        {"path_join",   TokenType::TK_PATH_JOIN},
        {"path_split",  TokenType::TK_PATH_SPLIT},
        {"basename",    TokenType::TK_PATH_BASENAME},
        {"dirname",     TokenType::TK_PATH_DIRNAME},
        {"extname",     TokenType::TK_PATH_EXTNAME},
        {"abspath",     TokenType::TK_PATH_ABS},
        {"relpath",     TokenType::TK_PATH_REL},
        {"normpath",    TokenType::TK_PATH_NORM},
        {"isabs",       TokenType::TK_PATH_ISABS},
        {"isrel",       TokenType::TK_PATH_ISREL},
        {"fs_space",    TokenType::TK_FS_SPACE},
        {"fs_used",     TokenType::TK_FS_USED},
        {"fs_free",     TokenType::TK_FS_FREE},
        {"fs_type",     TokenType::TK_FS_TYPE},
        {"fs_mount",    TokenType::TK_FS_MOUNT},
        {"fs_umount",   TokenType::TK_FS_UMOUNT},
        {"fs_sync",     TokenType::TK_FS_SYNC},
        {"pipe_open",   TokenType::TK_PIPE_OPEN},
        {"pipe_close",  TokenType::TK_PIPE_CLOSE},
        {"pipe_read",   TokenType::TK_PIPE_READ},
        {"pipe_write",  TokenType::TK_PIPE_WRITE},
        {"proc_exec",   TokenType::TK_PROC_EXEC},
        {"proc_wait",   TokenType::TK_PROC_WAIT},
        {"proc_kill",   TokenType::TK_PROC_KILL},
        {"getpid",      TokenType::TK_PROC_PID},
        {"env_get",     TokenType::TK_PROC_ENV_GET},
        {"env_set",     TokenType::TK_PROC_ENV_SET},
        {"env_args",    TokenType::TK_ENV_ARGS},
        {"symlink",     TokenType::TK_SYM_LINK},
        {"hardlink",    TokenType::TK_HARD_LINK},
        {"readlink",    TokenType::TK_READ_LINK},
        // Memory (45)
        {"mem_alloc",   TokenType::TK_MEM_ALLOC},
        {"mem_free",    TokenType::TK_MEM_FREE},
        {"mem_realloc", TokenType::TK_MEM_REALLOC},
        {"mem_calloc",  TokenType::TK_MEM_CALLOC},
        {"mem_align",   TokenType::TK_MEM_ALIGN},
        {"mem_zero",    TokenType::TK_MEM_ZERO},
        {"mem_copy",    TokenType::TK_MEM_COPY},
        {"mem_move",    TokenType::TK_MEM_MOVE},
        {"mem_set",     TokenType::TK_MEM_SET},
        {"mem_cmp",     TokenType::TK_MEM_COMPARE},
        {"mem_find",    TokenType::TK_MEM_FIND},
        {"mem_pool",    TokenType::TK_MEM_POOL},
        {"mem_arena",   TokenType::TK_MEM_ARENA},
        {"mem_salloc",  TokenType::TK_MEM_STACK_A},
        {"mem_page",    TokenType::TK_MEM_PAGE},
        {"mem_huge",    TokenType::TK_MEM_HUGE},
        {"mem_lock",    TokenType::TK_MEM_LOCK_P},
        {"mem_unlock",  TokenType::TK_MEM_UNLOCK_P},
        {"mem_protect", TokenType::TK_MEM_PROTECT},
        {"mem_advise",  TokenType::TK_MEM_ADVISE},
        {"mem_sync",    TokenType::TK_MEM_SYNC_M},
        {"mem_async",   TokenType::TK_MEM_ASYNC_M},
        {"mem_barrier", TokenType::TK_MEM_BARRIER_M},
        {"mem_fence",   TokenType::TK_MEM_FENCE_M},
        {"mem_prefetch",TokenType::TK_MEM_PREFETCH_M},
        {"mem_flush",   TokenType::TK_MEM_FLUSH_M},
        {"mem_inv",     TokenType::TK_MEM_INVALIDATE},
        {"mem_wb",      TokenType::TK_MEM_WRITEBACK},
        {"mem_atomic_add",TokenType::TK_MEM_ATOMIC_A},
        {"mem_atomic_store",TokenType::TK_MEM_ATOMIC_S},
        {"mem_cas",     TokenType::TK_MEM_CAS_M},
        {"mem_fadd",    TokenType::TK_MEM_FETCHADD_M},
        {"mem_xchg",    TokenType::TK_MEM_XCHG_M},
        {"refcnt",      TokenType::TK_MEM_REF_CNT},
        {"weakref",     TokenType::TK_MEM_WEAK_REF},
        {"shared_mem",  TokenType::TK_MEM_SHARED},
        {"unique_mem",  TokenType::TK_MEM_UNIQUE},
        {"borrow",      TokenType::TK_MEM_BORROW},
        {"lifetime",    TokenType::TK_MEM_LIFETIME},
        {"scope",       TokenType::TK_MEM_SCOPE},
        {"raii",        TokenType::TK_MEM_RAII},
        {"gc_mark",     TokenType::TK_MEM_GC_MARK},
        {"gc_sweep",    TokenType::TK_MEM_GC_SWEEP},
        {"compact",     TokenType::TK_MEM_COMPACT},
        {"defrag",      TokenType::TK_MEM_DEFRAG},
        // GPU (54)
        {"gpu_init",    TokenType::TK_GPU_INIT},
        {"gpu_shutdown",TokenType::TK_GPU_SHUTDOWN},
        {"gpu_alloc",   TokenType::TK_GPU_ALLOC},
        {"gpu_free",    TokenType::TK_GPU_FREE},
        {"gpu_upload",  TokenType::TK_GPU_UPLOAD},
        {"gpu_download",TokenType::TK_GPU_DOWNLOAD},
        {"gpu_copy",    TokenType::TK_GPU_COPY},
        {"gpu_memset",  TokenType::TK_GPU_MEMSET},
        {"gpu_sync",    TokenType::TK_GPU_SYNC},
        {"gpu_async",   TokenType::TK_GPU_ASYNC},
        {"gpu_stream",  TokenType::TK_GPU_STREAM},
        {"gpu_event",   TokenType::TK_GPU_EVENT},
        {"gpu_kernel",  TokenType::TK_GPU_KERNEL},
        {"gpu_launch",  TokenType::TK_GPU_LAUNCH},
        {"gpu_grid",    TokenType::TK_GPU_GRID},
        {"gpu_block",   TokenType::TK_GPU_BLOCK},
        {"gpu_threads", TokenType::TK_GPU_THREADS},
        {"gpu_warp",    TokenType::TK_GPU_WARP},
        {"gpu_shared",  TokenType::TK_GPU_SHARED},
        {"gpu_local",   TokenType::TK_GPU_LOCAL},
        {"gpu_global",  TokenType::TK_GPU_GLOBAL},
        {"gpu_atomic",  TokenType::TK_GPU_ATOMIC},
        {"gpu_barrier", TokenType::TK_GPU_BARRIER_G},
        {"gpu_reduce",  TokenType::TK_GPU_REDUCE},
        {"gpu_scan",    TokenType::TK_GPU_SCAN},
        {"gpu_sort",    TokenType::TK_GPU_SORT_G},
        {"gpu_matmul",  TokenType::TK_GPU_MATMUL},
        {"gpu_conv2d",  TokenType::TK_GPU_CONV2D},
        {"gpu_maxpool", TokenType::TK_GPU_MAXPOOL},
        {"gpu_avgpool", TokenType::TK_GPU_AVGPOOL},
        {"gpu_relu",    TokenType::TK_GPU_RELU_G},
        {"gpu_softmax", TokenType::TK_GPU_SOFTMAX_G},
        {"batchnorm",   TokenType::TK_GPU_BATCHNORM},
        {"dropout",     TokenType::TK_GPU_DROPOUT},
        {"embed",       TokenType::TK_GPU_EMBED},
        {"attention",   TokenType::TK_GPU_ATTENTION},
        {"gpu_fft",     TokenType::TK_GPU_FFT_G},
        {"gpu_ifft",    TokenType::TK_GPU_IFFT_G},
        {"gpu_hist",    TokenType::TK_GPU_HIST},
        {"gpu_filter",  TokenType::TK_GPU_FILTER_G},
        {"warp_reduce", TokenType::TK_GPU_WARP_RED},
        {"ldmatrix",    TokenType::TK_GPU_LDMATRIX},
        {"gpu_tensor",  TokenType::TK_GPU_TENSOR},
        {"wmma",        TokenType::TK_GPU_WMMA},
        {"mma",         TokenType::TK_GPU_MMA},
        {"gpu_perf",    TokenType::TK_GPU_PERF},
        {"gpu_occ",     TokenType::TK_GPU_OCCUPANCY},
        {"gpu_reg",     TokenType::TK_GPU_REG},
        {"gpu_l1",      TokenType::TK_GPU_L1},
        {"gpu_l2",      TokenType::TK_GPU_L2},
        {"gpu_bw",      TokenType::TK_GPU_BANDWIDTH},
        {"gpu_flops",   TokenType::TK_GPU_FLOPS},
        {"gpu_info",    TokenType::TK_GPU_INFO},
        {"gpu_device",  TokenType::TK_GPU_DEVICE},
        // GPU Render (67)
        {"gr_vertex",   TokenType::TK_GR_VERTEX},
        {"gr_fragment", TokenType::TK_GR_FRAGMENT},
        {"gr_compute",  TokenType::TK_GR_COMPUTE},
        {"gr_geometry", TokenType::TK_GR_GEOMETRY},
        {"gr_tess",     TokenType::TK_GR_TESS},
        {"gr_mesh",     TokenType::TK_GR_MESH},
        {"gr_ray",      TokenType::TK_GR_RAY},
        {"gr_pipeline", TokenType::TK_GR_PIPELINE},
        {"gr_framebuf", TokenType::TK_GR_FRAMEBUF},
        {"gr_renderbuf",TokenType::TK_GR_RENDERBUF},
        {"gr_texture",  TokenType::TK_GR_TEXTURE},
        {"gr_sampler",  TokenType::TK_GR_SAMPLER},
        {"gr_uniform",  TokenType::TK_GR_UNIFORM},
        {"gr_ssbo",     TokenType::TK_GR_SSBO},
        {"gr_vao",      TokenType::TK_GR_VAO},
        {"gr_vbo",      TokenType::TK_GR_VBO},
        {"gr_ibo",      TokenType::TK_GR_IBO},
        {"gr_draw",     TokenType::TK_GR_DRAW},
        {"gr_drawidx",  TokenType::TK_GR_DRAWIDX},
        {"gr_drawinst", TokenType::TK_GR_DRAWINST},
        {"gr_blit",     TokenType::TK_GR_BLIT},
        {"gr_resolve",  TokenType::TK_GR_RESOLVE},
        {"gr_clear",    TokenType::TK_GR_CLEAR_G},
        {"gr_viewport", TokenType::TK_GR_VIEWPORT},
        {"gr_scissor",  TokenType::TK_GR_SCISSOR},
        {"gr_depth",    TokenType::TK_GR_DEPTH},
        {"gr_stencil",  TokenType::TK_GR_STENCIL},
        {"gr_blend",    TokenType::TK_GR_BLEND},
        {"gr_cull",     TokenType::TK_GR_CULL},
        {"gr_winding",  TokenType::TK_GR_WINDING},
        {"gr_polymode", TokenType::TK_GR_POLYMODE},
        {"gr_linew",    TokenType::TK_GR_LINE_W},
        {"gr_points",   TokenType::TK_GR_POINT_S},
        {"gr_msaa",     TokenType::TK_GR_MSAA},
        {"gr_aniso",    TokenType::TK_GR_ANISO},
        {"gr_mip",      TokenType::TK_GR_MIP},
        {"gr_lod",      TokenType::TK_GR_LOD},
        {"gr_shadow",   TokenType::TK_GR_SHADOW},
        {"gr_shadowmap",TokenType::TK_GR_SHADOWMAP},
        {"ssao",        TokenType::TK_GR_SSAO},
        {"bloom",       TokenType::TK_GR_BLOOM},
        {"dof",         TokenType::TK_GR_DOF},
        {"motionblur",  TokenType::TK_GR_MOTION_BL},
        {"hdr",         TokenType::TK_GR_HDR},
        {"tonemap",     TokenType::TK_GR_TONEMAP},
        {"gamma",       TokenType::TK_GR_GAMMA},
        {"colorspace",  TokenType::TK_GR_COLOR_SP},
        {"postfx",      TokenType::TK_GR_POST_FX},
        {"gr_present",  TokenType::TK_GR_PRESENT},
        {"gr_swap",     TokenType::TK_GR_SWAP},
        {"vsync",       TokenType::TK_GR_VSYNC},
        {"wireframe",   TokenType::TK_GR_WIREFRAME},
        {"skybox",      TokenType::TK_GR_SKYBOX},
        {"cubemap",     TokenType::TK_GR_CUBEMAP},
        {"envprobe",    TokenType::TK_GR_PROBE},
        {"gi",          TokenType::TK_GR_GI},
        {"reflection",  TokenType::TK_GR_REFLEC},
        {"refraction",  TokenType::TK_GR_REFRAC},
        {"caustic",     TokenType::TK_GR_CAUSTIC},
        {"subsurface",  TokenType::TK_GR_SUBSURFACE},
        {"parallax",    TokenType::TK_GR_PARALLAX},
        {"displace",    TokenType::TK_GR_DISPLACE},
        {"morph",       TokenType::TK_GR_MORPH},
        {"bones",       TokenType::TK_GR_BONES},
        {"skin",        TokenType::TK_GR_SKIN},
        {"particles",   TokenType::TK_GR_PARTICLES},
        // Text manipulation (66)
        {"text_wrap",   TokenType::TK_TEXT_WRAP},
        {"text_center", TokenType::TK_TEXT_CENTER},
        {"text_left",   TokenType::TK_TEXT_LEFT},
        {"text_right",  TokenType::TK_TEXT_RIGHT},
        {"text_bold",   TokenType::TK_TEXT_BOLD},
        {"text_italic", TokenType::TK_TEXT_ITALIC},
        {"text_color",  TokenType::TK_TEXT_COLOR},
        {"text_blink",  TokenType::TK_TEXT_BLINK},
        {"text_under",  TokenType::TK_TEXT_UNDER},
        {"text_strike", TokenType::TK_TEXT_STRIKE},
        {"text_reset",  TokenType::TK_TEXT_RESET},
        {"text_width",  TokenType::TK_TEXT_WIDTH},
        {"text_cols",   TokenType::TK_TEXT_COLS},
        {"text_rows",   TokenType::TK_TEXT_ROWS},
        {"text_cursor", TokenType::TK_TEXT_CURSOR},
        {"text_clear",  TokenType::TK_TEXT_CLEAR},
        {"text_scroll", TokenType::TK_TEXT_SCROLL},
        {"text_box",    TokenType::TK_TEXT_BOX},
        {"text_table",  TokenType::TK_TEXT_TABLE},
        {"text_border", TokenType::TK_TEXT_BORDER},
        {"text_header", TokenType::TK_TEXT_HEADER},
        {"text_footer", TokenType::TK_TEXT_FOOTER},
        {"text_indent", TokenType::TK_TEXT_INDENT},
        {"text_dedent", TokenType::TK_TEXT_DEDENT},
        {"text_line",   TokenType::TK_TEXT_LINE},
        {"text_newline",TokenType::TK_TEXT_NEWLINE},
        {"text_tab",    TokenType::TK_TEXT_TAB},
        {"text_space",  TokenType::TK_TEXT_SPACE},
        {"printf",      TokenType::TK_TEXT_PRINTF},
        {"scanf",       TokenType::TK_TEXT_SCANF},
        {"puts",        TokenType::TK_TEXT_PUTS},
        {"gets",        TokenType::TK_TEXT_GETS},
        {"putchar",     TokenType::TK_TEXT_PUTCHAR},
        {"getchar",     TokenType::TK_TEXT_GETCHAR},
        {"fwrite",      TokenType::TK_TEXT_FWRITE},
        {"fread",       TokenType::TK_TEXT_FREAD},
        {"sscanf",      TokenType::TK_TEXT_SSCANF},
        {"snprintf",    TokenType::TK_TEXT_SNPRINTF},
        {"vprintf",     TokenType::TK_TEXT_VPRINTF},
        {"vsprintf",    TokenType::TK_TEXT_VSPRINTF},
        {"strtok",      TokenType::TK_TEXT_STRTOK},
        {"strspn",      TokenType::TK_TEXT_STRSPN},
        {"strcspn",     TokenType::TK_TEXT_STRCSPN},
        {"strpbrk",     TokenType::TK_TEXT_STRPBRK},
        {"memcpy",      TokenType::TK_TEXT_MEMCPY},
        {"memmove",     TokenType::TK_TEXT_MEMMOVE},
        {"memset",      TokenType::TK_TEXT_MEMSET},
        {"memcmp",      TokenType::TK_TEXT_MEMCMP},
        {"memchr",      TokenType::TK_TEXT_MEMCHR},
        {"strncat",     TokenType::TK_TEXT_STRNCAT},
        {"strncmp",     TokenType::TK_TEXT_STRNCMP},
        {"strcpy",      TokenType::TK_TEXT_STRCPY},
        {"strncpy",     TokenType::TK_TEXT_STRNCPY},
        {"sprintf",     TokenType::TK_TEXT_SPRINTF},
        {"atoi",        TokenType::TK_TEXT_ATOI},
        {"atof",        TokenType::TK_TEXT_ATOF},
        {"itoa",        TokenType::TK_TEXT_ITOA},
        {"ftoa",        TokenType::TK_TEXT_FTOA},
        {"dtoa",        TokenType::TK_TEXT_DTOA},
        {"ltrimws",     TokenType::TK_TEXT_LTRIM_WS},
        {"rtrimws",     TokenType::TK_TEXT_RTRIM_WS},
        {"printhex",    TokenType::TK_TEXT_PRINT_HEX},
        {"printbin",    TokenType::TK_TEXT_PRINT_BIN},
        // ASM inline (12)
        {"asm_mov",     TokenType::TK_ASM_MOV},
        {"asm_add",     TokenType::TK_ASM_ADD},
        {"asm_sub",     TokenType::TK_ASM_SUB},
        {"asm_mul",     TokenType::TK_ASM_MUL},
        {"asm_div",     TokenType::TK_ASM_DIV},
        {"asm_and",     TokenType::TK_ASM_AND},
        {"asm_or",      TokenType::TK_ASM_OR},
        {"asm_xor",     TokenType::TK_ASM_XOR},
        {"asm_shl",     TokenType::TK_ASM_SHL},
        {"asm_shr",     TokenType::TK_ASM_SHR},
        {"asm_cmp",     TokenType::TK_ASM_CMP},
        {"asm_jmp",     TokenType::TK_ASM_JMP},
    };
    return kmap;
}

// ============================================================
//  LEXER IMPLEMENTATION
// ============================================================

Lexer::Lexer(std::string source, std::string filename)
    : src_(std::move(source)), filename_(std::move(filename))
{
    indent_stack_.push_back(0);
}

QZ_INLINE char Lexer::current() const noexcept {
    return pos_ < src_.size() ? src_[pos_] : '\0';
}

QZ_INLINE char Lexer::lookahead(int offset) const noexcept {
    size_t p = pos_ + static_cast<size_t>(offset);
    return p < src_.size() ? src_[p] : '\0';
}

QZ_INLINE char Lexer::advance() noexcept {
    char c = src_[pos_++];
    if (c == '\n') { ++line_; col_ = 1; }
    else           { ++col_; }
    return c;
}

QZ_INLINE Token Lexer::make_token(TokenType t, std::string val) const noexcept {
    return Token(t, std::move(val), line_, col_);
}

void Lexer::skip_whitespace_no_newline() {
    while (pos_ < src_.size()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else {
            break;
        }
    }
}

void Lexer::skip_comment() {
    // Single-line comments: //
    if (current() == '/' && lookahead() == '/') {
        while (pos_ < src_.size() && current() != '\n') advance();
    }
    // Block comments: /* ... */
    else if (current() == '/' && lookahead() == '*') {
        advance(); advance();
        while (pos_ < src_.size()) {
            if (current() == '*' && lookahead() == '/') {
                advance(); advance();
                break;
            }
            advance();
        }
    }
}

Token Lexer::read_number() {
    size_t start = pos_;
    uint32_t l = line_, c = col_;
    bool is_float = false;

    // Hex: 0x...
    if (current() == '0' && (lookahead() == 'x' || lookahead() == 'X')) {
        advance(); advance();
        while (pos_ < src_.size() && std::isxdigit(current())) advance();
        return Token(TokenType::TK_INTEGER, src_.substr(start, pos_ - start), l, c);
    }
    // Binary: 0b...
    if (current() == '0' && (lookahead() == 'b' || lookahead() == 'B')) {
        advance(); advance();
        while (pos_ < src_.size() && (current() == '0' || current() == '1')) advance();
        return Token(TokenType::TK_INTEGER, src_.substr(start, pos_ - start), l, c);
    }
    // Decimal
    while (pos_ < src_.size() && std::isdigit(current())) advance();
    if (pos_ < src_.size() && current() == '.' && std::isdigit(lookahead())) {
        is_float = true;
        advance();
        while (pos_ < src_.size() && std::isdigit(current())) advance();
    }
    // Exponent
    if (pos_ < src_.size() && (current() == 'e' || current() == 'E')) {
        is_float = true;
        advance();
        if (pos_ < src_.size() && (current() == '+' || current() == '-')) advance();
        while (pos_ < src_.size() && std::isdigit(current())) advance();
    }
    // Suffix f/F for float
    if (pos_ < src_.size() && (current() == 'f' || current() == 'F')) {
        is_float = true;
        advance();
    }
    std::string val = src_.substr(start, pos_ - start);
    return Token(is_float ? TokenType::TK_FLOAT : TokenType::TK_INTEGER, val, l, c);
}

Token Lexer::read_string(char quote) {
    uint32_t l = line_, c = col_;
    advance();  // skip opening quote
    std::string val;
    while (pos_ < src_.size() && current() != quote) {
        char ch = current();
        if (ch == '\\') {
            advance();
            switch (current()) {
                case 'n':  val += '\n'; break;
                case 't':  val += '\t'; break;
                case 'r':  val += '\r'; break;
                case '0':  val += '\0'; break;
                case '\\': val += '\\'; break;
                case '"':  val += '"';  break;
                case '\'': val += '\''; break;
                case 'a':  val += '\a'; break;
                case 'b':  val += '\b'; break;
                default:   val += current(); break;
            }
        } else {
            val += ch;
        }
        advance();
    }
    if (pos_ < src_.size()) advance();  // skip closing quote
    return Token(TokenType::TK_STRING, val, l, c);
}

Token Lexer::read_identifier_or_keyword() {
    uint32_t l = line_, c = col_;
    size_t start = pos_;
    while (pos_ < src_.size() &&
           (std::isalnum(current()) || current() == '_')) {
        advance();
    }
    // Check for end! 
    std::string word = src_.substr(start, pos_ - start);
    if (word == "end" && pos_ < src_.size() && current() == '!') {
        advance();
        word += '!';
    }
    TokenType t = lookup_keyword(word);
    return Token(t, word, l, c);
}

Token Lexer::read_operator() {
    char ch = advance();
    switch (ch) {
        case '+': return make_token(TokenType::TK_PLUS, "+");
        case '-': {
            if (current() == '>') { advance(); return make_token(TokenType::TK_ARROW, "->"); }
            return make_token(TokenType::TK_MINUS, "-");
        }
        case '*': return make_token(TokenType::TK_STAR, "*");
        case '/': {
            if (current() == '/' || current() == '*') {
                --pos_; --col_;  // put back, skip_comment will handle
                return make_token(TokenType::TK_COMMENT, "");
            }
            return make_token(TokenType::TK_SLASH, "/");
        }
        case '%': return make_token(TokenType::TK_PERCENT, "%");
        case '^': return make_token(TokenType::TK_CARET, "^");
        case '&': return make_token(TokenType::TK_AMPERSAND, "&");
        case '|': return make_token(TokenType::TK_PIPE, "|");
        case '~': return make_token(TokenType::TK_TILDE, "~");
        case '!': {
            if (current() == '=') { advance(); return make_token(TokenType::TK_NEQ, "!="); }
            return make_token(TokenType::TK_BANG, "!");
        }
        case '?': return make_token(TokenType::TK_QUESTION, "?");
        case '@': return make_token(TokenType::TK_AT, "@");
        case '#': return make_token(TokenType::TK_HASH, "#");
        case '$': return make_token(TokenType::TK_DOLLAR, "$");
        case '_': return make_token(TokenType::TK_UNDERSCORE, "_");
        case '=': {
            if (current() == '=') { advance(); return make_token(TokenType::TK_EQ, "=="); }
            if (current() == '>') { advance(); return make_token(TokenType::TK_DOUBLE_ARROW, "=>"); }
            return make_token(TokenType::TK_ASSIGN, "=");
        }
        case '<': {
            if (current() == '=') { advance(); return make_token(TokenType::TK_LE, "<="); }
            return make_token(TokenType::TK_LT, "<");
        }
        case '>': {
            if (current() == '=') { advance(); return make_token(TokenType::TK_GE, ">="); }
            return make_token(TokenType::TK_GT, ">");
        }
        case '(': return make_token(TokenType::TK_LPAREN, "(");
        case ')': return make_token(TokenType::TK_RPAREN, ")");
        case '{': return make_token(TokenType::TK_LBRACE, "{");
        case '}': return make_token(TokenType::TK_RBRACE, "}");
        case '[': return make_token(TokenType::TK_LBRACKET, "[");
        case ']': return make_token(TokenType::TK_RBRACKET, "]");
        case ',': return make_token(TokenType::TK_COMMA, ",");
        case '.': return make_token(TokenType::TK_DOT, ".");
        case ':': {
            if (current() == ':') { advance(); return make_token(TokenType::TK_DOUBLE_COLON, "::"); }
            return make_token(TokenType::TK_COLON, ":");
        }
        case ';': return make_token(TokenType::TK_SEMICOLON, ";");
        default:
            return make_token(TokenType::TK_UNKNOWN, std::string(1, ch));
    }
}

Token Lexer::next_token() {
    while (pos_ < src_.size()) {
        // Skip spaces/tabs (not newlines)
        skip_whitespace_no_newline();
        if (pos_ >= src_.size()) break;

        char c = current();

        // Comments
        if (c == '/' && (lookahead() == '/' || lookahead() == '*')) {
            skip_comment();
            continue;
        }

        // Newline
        if (c == '\n') {
            uint32_t l = line_, col = col_;
            advance();
            return Token(TokenType::TK_NEWLINE, "\\n", l, col);
        }

        // Numbers (only positive literals; minus sign handled by parser as unary)
        if (std::isdigit(c)) {
            return read_number();
        }

        // Strings
        if (c == '"' || c == '\'') {
            return read_string(c);
        }

        // Identifiers / keywords
        if (std::isalpha(c) || c == '_') {
            return read_identifier_or_keyword();
        }

        // Operators and punctuation
        return read_operator();
    }
    return Token(TokenType::TK_EOF, "", line_, col_);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    tokens.reserve(1024);

    while (true) {
        Token t = next_token();
        // Skip comment tokens
        if (t.type == TokenType::TK_COMMENT) continue;
        tokens.push_back(t);
        if (t.type == TokenType::TK_EOF) break;
    }
    return tokens;
}

TokenType Lexer::lookup_keyword(const std::string& word) noexcept {
    const auto& km = keyword_map();
    auto it = km.find(word);
    if (it != km.end()) return it->second;
    return TokenType::TK_IDENTIFIER;
}

Token Lexer::error_token(const std::string& msg) {
    error_ = filename_ + ":" + std::to_string(line_) + ":" + std::to_string(col_) + ": " + msg;
    return Token(TokenType::TK_UNKNOWN, msg, line_, col_);
}

} // namespace quartz
