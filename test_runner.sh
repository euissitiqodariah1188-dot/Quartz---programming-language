#!/bin/bash
# ============================================================
#  QUARTZ TEST RUNNER v1.0
#  Tests semua file .qua di folder tests/
#  Usage: ./test_runner.sh
#  Usage: ./test_runner.sh --verbose
#  Usage: ./test_runner.sh --run   (compile + run, bukan cuma check)
# ============================================================

set -uo pipefail

# --- Colors ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

# --- Config ---
QUARTZ="./quartz"
TESTS_DIR="./tests"
VERBOSE=0
MODE="check"      # check | run | asm | bench
TIMEOUT=10        # seconds per test

# --- Parse args ---
for arg in "$@"; do
    case "$arg" in
        --verbose|-v)   VERBOSE=1 ;;
        --run|-r)       MODE="run" ;;
        --asm|-a)       MODE="asm" ;;
        --bench|-b)     MODE="bench" ;;
        --help|-h)
            echo "Usage: $0 [--verbose] [--run] [--asm] [--bench]"
            echo "  --verbose  : show full output per test"
            echo "  --run      : compile + execute (default: syntax check only)"
            echo "  --asm      : emit ARM64 assembly and check for bad mnemonics"
            echo "  --bench    : run compile-time benchmark on each file"
            exit 0
            ;;
    esac
done

# --- Check quartz binary ---
if [ ! -f "$QUARTZ" ]; then
    echo -e "${RED}ERROR: quartz binary not found at $QUARTZ${RESET}"
    echo "Run 'make' first."
    exit 1
fi

if [ ! -d "$TESTS_DIR" ]; then
    echo -e "${RED}ERROR: tests/ directory not found${RESET}"
    exit 1
fi

# --- Print header ---
echo ""
echo -e "${BOLD}╔══════════════════════════════════════════════════════╗${RESET}"
echo -e "${BOLD}║         QUARTZ LANGUAGE TEST RUNNER v1.0             ║${RESET}"
echo -e "${BOLD}╚══════════════════════════════════════════════════════╝${RESET}"
echo ""
echo -e "  Quartz:    $($QUARTZ version 2>/dev/null | head -1)"
echo -e "  Arch:      $($QUARTZ arch 2>/dev/null | head -1)"
echo -e "  Mode:      $MODE"
echo -e "  Tests dir: $TESTS_DIR"
echo -e "  Date:      $(date)"
echo ""
echo -e "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# --- Counters ---
PASS=0
FAIL=0
SKIP=0
TOTAL=0

# --- Results log ---
RESULTS=()
FAIL_DETAILS=()

# --- Bad ASM patterns to check (ARM64 specific) ---
BAD_PATTERNS=(
    "math op [0-9]"
    "sleep x"
    "split(csv)"
    "point nums"
    "unknown mnemonic"
    "; split"
    "; wait"
    "; point"
)

run_test() {
    local file="$1"
    local name
    name=$(basename "$file" .qua)
    TOTAL=$((TOTAL + 1))

    local output=""
    local exit_code=0
    local extra=""

    case "$MODE" in

        # ---- SYNTAX CHECK ----
        check)
            output=$(timeout $TIMEOUT "$QUARTZ" check "$file" 2>&1) || exit_code=$?
            if echo "$output" | grep -q "^OK"; then
                PASS=$((PASS + 1))
                RESULTS+=("PASS|$name|$output")
                echo -e "  ${GREEN}✅ PASS${RESET}  $name"
                [ $VERBOSE -eq 1 ] && echo -e "         ${output}"
            else
                FAIL=$((FAIL + 1))
                local err
                err=$(echo "$output" | grep "Error" | head -3 | tr '\n' ' ')
                RESULTS+=("FAIL|$name|$err")
                FAIL_DETAILS+=("$name: $err")
                echo -e "  ${RED}❌ FAIL${RESET}  $name"
                echo -e "         ${RED}${err}${RESET}"
                [ $VERBOSE -eq 1 ] && echo "$output"
            fi
            ;;

        # ---- COMPILE + RUN ----
        run)
            local tmp_bin
            tmp_bin="/tmp/qz_test_$(echo "$name" | tr -cd 'a-z0-9')_$$"

            # Step 1: compile
            local compile_out
            compile_out=$(timeout $TIMEOUT "$QUARTZ" compile -o "$tmp_bin" "$file" 2>&1) || exit_code=$?

            if [ $exit_code -ne 0 ]; then
                FAIL=$((FAIL + 1))
                local err
                err=$(echo "$compile_out" | grep -v "^$" | head -4 | tr '\n' ' ')
                RESULTS+=("FAIL|$name|compile error: $err")
                FAIL_DETAILS+=("$name [compile]: $err")
                echo -e "  ${RED}❌ FAIL${RESET}  $name  ${YELLOW}[compile error]${RESET}"
                echo -e "         ${RED}$(echo "$compile_out" | grep -i "error" | head -2)${RESET}"
                rm -f "$tmp_bin"
                return
            fi

            # Step 2: run
            local run_out
            run_out=$(timeout $TIMEOUT "$tmp_bin" 2>&1) || exit_code=$?
            rm -f "$tmp_bin"

            if [ $exit_code -ne 0 ] && [ $exit_code -ne 1 ]; then
                FAIL=$((FAIL + 1))
                RESULTS+=("FAIL|$name|runtime exit=$exit_code")
                FAIL_DETAILS+=("$name [runtime]: exit code $exit_code")
                echo -e "  ${RED}❌ FAIL${RESET}  $name  ${YELLOW}[runtime exit=$exit_code]${RESET}"
                [ $VERBOSE -eq 1 ] && echo "$run_out"
            else
                PASS=$((PASS + 1))
                local line_count
                line_count=$(echo "$run_out" | wc -l)
                RESULTS+=("PASS|$name|${line_count} lines output")
                echo -e "  ${GREEN}✅ PASS${RESET}  $name  ${CYAN}(${line_count} lines)${RESET}"
                if [ $VERBOSE -eq 1 ]; then
                    echo "$run_out" | sed 's/^/         /'
                fi
            fi
            ;;

        # ---- EMIT ASM + VALIDATE ----
        asm)
            local asm_out
            asm_out=$(timeout $TIMEOUT "$QUARTZ" emit-asm "$file" 2>&1) || exit_code=$?

            if [ $exit_code -ne 0 ]; then
                FAIL=$((FAIL + 1))
                RESULTS+=("FAIL|$name|emit-asm failed")
                FAIL_DETAILS+=("$name: emit-asm failed")
                echo -e "  ${RED}❌ FAIL${RESET}  $name  ${YELLOW}[emit-asm error]${RESET}"
                return
            fi

            # Check for bad patterns in generated ASM
            local bad_found=""
            for pat in "${BAD_PATTERNS[@]}"; do
                if echo "$asm_out" | grep -qE "$pat" 2>/dev/null; then
                    bad_found="$pat"
                    break
                fi
            done

            local asm_size
            asm_size=$(echo "$asm_out" | wc -c)

            if [ -n "$bad_found" ]; then
                FAIL=$((FAIL + 1))
                RESULTS+=("FAIL|$name|bad pattern: $bad_found")
                FAIL_DETAILS+=("$name: bad ASM pattern '$bad_found'")
                echo -e "  ${RED}❌ FAIL${RESET}  $name  ${YELLOW}[bad: '$bad_found']${RESET}"
            else
                PASS=$((PASS + 1))
                RESULTS+=("PASS|$name|${asm_size}B ASM")
                echo -e "  ${GREEN}✅ PASS${RESET}  $name  ${CYAN}(${asm_size} bytes ASM)${RESET}"
                if [ $VERBOSE -eq 1 ]; then
                    echo "$asm_out" | head -30 | sed 's/^/         /'
                fi
            fi
            ;;

        # ---- BENCHMARK ----
        bench)
            local bench_out
            bench_out=$(timeout 60 "$QUARTZ" bench "$file" 2>&1) || exit_code=$?

            if [ $exit_code -ne 0 ]; then
                FAIL=$((FAIL + 1))
                RESULTS+=("FAIL|$name|bench failed")
                FAIL_DETAILS+=("$name: bench failed")
                echo -e "  ${RED}❌ FAIL${RESET}  $name"
                return
            fi

            PASS=$((PASS + 1))
            local avg best
            avg=$(echo "$bench_out" | grep "Avg" | awk '{print $3}')
            best=$(echo "$bench_out" | grep "Best" | awk '{print $2}')
            RESULTS+=("PASS|$name|avg=${avg} best=${best}")
            echo -e "  ${GREEN}✅ PASS${RESET}  $name  ${CYAN}avg=${avg} best=${best}${RESET}"
            if [ $VERBOSE -eq 1 ]; then
                echo "$bench_out" | sed 's/^/         /'
            fi
            ;;
    esac
}

# --- Run all .qua files ---
shopt -s nullglob
qua_files=("$TESTS_DIR"/*.qua)
shopt -u nullglob

if [ ${#qua_files[@]} -eq 0 ]; then
    echo -e "${YELLOW}No .qua files found in $TESTS_DIR${RESET}"
    exit 0
fi

for f in "${qua_files[@]}"; do
    run_test "$f"
done

# --- Summary ---
echo ""
echo -e "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "  ${GREEN}${BOLD}🎉 ALL TESTS PASSED${RESET}"
else
    echo -e "  ${YELLOW}${BOLD}⚠  SOME TESTS FAILED${RESET}"
fi

echo ""
echo -e "  ${GREEN}✅ Passed:${RESET} $PASS"
echo -e "  ${RED}❌ Failed:${RESET} $FAIL"
echo -e "  ${CYAN}📁 Total: ${RESET} $TOTAL"
echo ""

if [ ${#FAIL_DETAILS[@]} -gt 0 ]; then
    echo -e "  ${RED}${BOLD}Failed tests:${RESET}"
    for d in "${FAIL_DETAILS[@]}"; do
        echo -e "    ${RED}• $d${RESET}"
    done
    echo ""
fi

# --- Save report to file ---
REPORT="test_report_$(date +%Y%m%d_%H%M%S).txt"
{
    echo "QUARTZ TEST REPORT"
    echo "Date:  $(date)"
    echo "Arch:  $($QUARTZ arch 2>/dev/null | head -1)"
    echo "Mode:  $MODE"
    echo "Total: $TOTAL  Pass: $PASS  Fail: $FAIL"
    echo ""
    echo "RESULTS:"
    for r in "${RESULTS[@]}"; do
        echo "  $r"
    done
    if [ ${#FAIL_DETAILS[@]} -gt 0 ]; then
        echo ""
        echo "FAILURES:"
        for d in "${FAIL_DETAILS[@]}"; do
            echo "  $d"
        done
    fi
} > "$REPORT"

echo -e "  📄 Report saved: ${CYAN}$REPORT${RESET}"
echo ""

# Exit with failure code if any test failed
[ $FAIL -eq 0 ] && exit 0 || exit 1
