# ============================================================
#  Quartz Language Makefile
#  Supports: ARM64 (v8a/v9a) + x86_64, Linux
# ============================================================

CXX      := g++
CXXFLAGS := -std=c++17 -O3 -pipe -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable
LDFLAGS  := -lm -lpthread

# Architecture-specific flags
ARCH := $(shell uname -m)
ifeq ($(ARCH),aarch64)
    CXXFLAGS  += -march=armv8-a+simd -mtune=native -ffast-math
    ARCH_NAME := ARM64
else ifeq ($(ARCH),arm64)
    CXXFLAGS  += -march=armv8-a+simd -mtune=native -ffast-math
    ARCH_NAME := ARM64
else
    CXXFLAGS  += -march=native -mavx2 -mfma -ffast-math
    ARCH_NAME := x86_64
endif

# Check if stdc++fs is a separate library (GCC < 9) or built-in (GCC >= 9)
GCC_VER := $(shell $(CXX) -dumpversion | cut -d. -f1)
ifeq ($(shell test $(GCC_VER) -lt 9 2>/dev/null && echo old),old)
    LDFLAGS += -lstdc++fs
endif

INCLUDES := -Iinclude

CORE_SRCS := \
    src/runtime.cpp \
    src/lexer.cpp \
    src/parser.cpp \
    src/codegen.cpp \
    src/compiler.cpp \
    src/memory_manager.cpp \
    src/logic_processor.cpp

CORE_OBJS := $(CORE_SRCS:.cpp=.o)

BIN      := quartz
LIB      := libquartz_core.a

.PHONY: all clean install test bench

all: $(BIN)
	@echo ""
	@echo "  Quartz v1.0.0 built for $(ARCH_NAME)"
	@echo "  Binary: ./$(BIN)"
	@echo ""

# Build core library
$(LIB): $(CORE_OBJS)
	@echo "[AR] $@"
	@ar rcs $@ $^

# Build CLI tool
$(BIN): tools/quartz_main.cpp $(LIB)
	@echo "[CXX] quartz"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ -L. -lquartz_core $(LDFLAGS)
	@echo "[OK]  ./quartz"

# Compile .cpp → .o
%.o: %.cpp
	@echo "[CXX] $<"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# ---- Tests ----
test: $(BIN)
	@echo ""
	@echo "=== Running Quartz Tests ==="
	@echo ""
	@echo "[TEST] Syntax check: test_hello.qua"
	@./$(BIN) check tests/test_hello.qua
	@echo "[TEST] Syntax check: test_vars.qua"
	@./$(BIN) check tests/test_vars.qua
	@echo "[TEST] Syntax check: test_math.qua"
	@./$(BIN) check tests/test_math.qua
	@echo "[TEST] Syntax check: test_features.qua"
	@./$(BIN) check tests/test_features.qua
	@echo "[TEST] Syntax check: test_ml.qua"
	@./$(BIN) check tests/test_ml.qua
	@echo ""
	@echo "[TEST] Token dump: test_hello.qua"
	@./$(BIN) tokens tests/test_hello.qua
	@echo ""
	@echo "[TEST] AST dump: test_hello.qua"
	@./$(BIN) ast tests/test_hello.qua
	@echo ""
	@echo "[TEST] ASM emit: test_hello.qua"
	@./$(BIN) emit-asm tests/test_hello.qua | head -40
	@echo ""
	@echo "=== All syntax tests PASSED ==="
	@echo ""

# ---- Benchmark ----
bench: $(BIN)
	@echo ""
	@echo "=== Compile-time Benchmarks ==="
	@./$(BIN) bench tests/test_hello.qua
	@./$(BIN) bench tests/test_math.qua
	@./$(BIN) bench tests/test_ml.qua

# ---- Show arch info ----
info: $(BIN)
	@./$(BIN) info

# ---- Install ----
install: $(BIN)
	@echo "Installing quartz to /usr/local/bin/"
	@install -m 755 $(BIN) /usr/local/bin/quartz
	@mkdir -p /usr/local/share/quartz/stdlib
	@cp stdlib/*.qua /usr/local/share/quartz/stdlib/
	@echo "Installed: quartz → /usr/local/bin/quartz"
	@echo "Stdlib:    → /usr/local/share/quartz/stdlib/"

# ---- Uninstall ----
uninstall:
	@rm -f /usr/local/bin/quartz
	@rm -rf /usr/local/share/quartz
	@echo "Uninstalled quartz"

# ---- Clean ----
clean:
	@rm -f $(CORE_OBJS) $(LIB) $(BIN)
	@echo "Cleaned"

# Debug build
debug: CXXFLAGS := -std=c++17 -O0 -g3 -fsanitize=address,undefined -Wall -Wextra -Wno-unused-parameter
debug: clean all
