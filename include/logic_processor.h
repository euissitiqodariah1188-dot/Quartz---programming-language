#pragma once
#ifndef QUARTZ_LOGIC_PROCESSOR_H
#define QUARTZ_LOGIC_PROCESSOR_H

#include "quartz.h"
#include <cmath>
#include <complex>

namespace quartz {

// ============================================================
//  QUARTZ LOGIC PROCESSOR
//  High-accuracy mathematical + logical computation engine
//  Uses CPU + (optionally) GPU for math
//  Target: 0.1ms first run
// ============================================================

class LogicProcessor {
public:
    static LogicProcessor& instance() noexcept {
        static LogicProcessor inst;
        return inst;
    }

    ~LogicProcessor() = default;
    LogicProcessor(const LogicProcessor&) = delete;
    LogicProcessor& operator=(const LogicProcessor&) = delete;

    // ---- Core arithmetic ----
    QZ_INLINE QZ_HOT double  op_add   (double a, double b) const noexcept { return a + b; }
    QZ_INLINE QZ_HOT double  op_sub   (double a, double b) const noexcept { return a - b; }
    QZ_INLINE QZ_HOT double  op_mul   (double a, double b) const noexcept { return a * b; }
    QZ_INLINE QZ_HOT double  op_div   (double a, double b) const noexcept;
    QZ_INLINE QZ_HOT int64_t op_mod   (int64_t a, int64_t b) const noexcept;
    QZ_INLINE QZ_HOT double  op_pow   (double a, double b) const noexcept;

    // ---- Trigonometry ----
    QZ_INLINE double op_sin  (double x) const noexcept { return std::sin(x); }
    QZ_INLINE double op_cos  (double x) const noexcept { return std::cos(x); }
    QZ_INLINE double op_tan  (double x) const noexcept { return std::tan(x); }
    QZ_INLINE double op_asin (double x) const noexcept { return std::asin(x); }
    QZ_INLINE double op_acos (double x) const noexcept { return std::acos(x); }
    QZ_INLINE double op_atan (double x) const noexcept { return std::atan(x); }
    QZ_INLINE double op_atan2(double y, double x) const noexcept { return std::atan2(y, x); }

    // ---- Exponential / Logarithm ----
    QZ_INLINE double op_exp   (double x) const noexcept { return std::exp(x); }
    QZ_INLINE double op_log   (double x) const noexcept { return std::log(x); }
    QZ_INLINE double op_log2  (double x) const noexcept { return std::log2(x); }
    QZ_INLINE double op_log10 (double x) const noexcept { return std::log10(x); }
    QZ_INLINE double op_sqrt  (double x) const noexcept { return std::sqrt(x); }
    QZ_INLINE double op_cbrt  (double x) const noexcept { return std::cbrt(x); }

    // ---- Rounding ----
    QZ_INLINE double  op_abs   (double x)  const noexcept { return std::fabs(x); }
    QZ_INLINE double  op_ceil  (double x)  const noexcept { return std::ceil(x); }
    QZ_INLINE double  op_floor (double x)  const noexcept { return std::floor(x); }
    QZ_INLINE double  op_round (double x)  const noexcept { return std::round(x); }
    QZ_INLINE double  op_trunc (double x)  const noexcept { return std::trunc(x); }
    QZ_INLINE double  op_fmod  (double a, double b) const noexcept { return std::fmod(a, b); }

    // ---- Min/Max/Clamp ----
    QZ_INLINE double op_min   (double a, double b) const noexcept { return a < b ? a : b; }
    QZ_INLINE double op_max   (double a, double b) const noexcept { return a > b ? a : b; }
    QZ_INLINE double op_clamp (double x, double lo, double hi) const noexcept {
        return x < lo ? lo : (x > hi ? hi : x);
    }
    QZ_INLINE double op_lerp  (double a, double b, double t) const noexcept {
        return a + t * (b - a);
    }

    // ---- Vector math ----
    double op_dot      (const double* a, const double* b, size_t n) const noexcept;
    void   op_cross3   (const double a[3], const double b[3], double out[3]) const noexcept;
    double op_magnitude(const double* v, size_t n) const noexcept;
    void   op_normalize(const double* v, double* out, size_t n) const noexcept;
    double op_distance (const double* a, const double* b, size_t n) const noexcept;

    // ---- Matrix math ----
    // Matrix stored row-major, dim = rows = cols (square)
    void   op_matrix_mul (const double* a, const double* b, double* out, size_t dim) const noexcept;
    void   op_matrix_inv (const double* m, double* out, size_t dim) const noexcept;
    void   op_matrix_trans(const double* m, double* out, size_t rows, size_t cols) const noexcept;
    double op_determinant(const double* m, size_t dim) const noexcept;

    // ---- FFT ----
    // in-place complex FFT (interleaved real/imag pairs)
    void op_fft  (double* data, size_t n) const noexcept;
    void op_ifft (double* data, size_t n) const noexcept;

    // ---- ML activation functions ----
    QZ_INLINE double op_sigmoid(double x) const noexcept {
        return 1.0 / (1.0 + std::exp(-x));
    }
    QZ_INLINE double op_relu(double x) const noexcept {
        return x > 0.0 ? x : 0.0;
    }
    QZ_INLINE double op_leaky_relu(double x, double alpha = 0.01) const noexcept {
        return x > 0.0 ? x : alpha * x;
    }
    void   op_softmax(const double* in, double* out, size_t n) const noexcept;
    QZ_INLINE double op_tanh_act(double x) const noexcept { return std::tanh(x); }
    QZ_INLINE double op_swish(double x) const noexcept { return x * op_sigmoid(x); }
    QZ_INLINE double op_gelu(double x) const noexcept;
    QZ_INLINE double op_elu(double x, double alpha = 1.0) const noexcept {
        return x >= 0.0 ? x : alpha * (std::exp(x) - 1.0);
    }

    // ---- Convolution ----
    void op_conv1d(const double* in, size_t in_len,
                   const double* kernel, size_t k_len,
                   double* out) const noexcept;
    void op_conv2d(const double* in, size_t iH, size_t iW,
                   const double* kernel, size_t kH, size_t kW,
                   double* out) const noexcept;

    // ---- Statistics ----
    double op_mean      (const double* data, size_t n) const noexcept;
    double op_variance  (const double* data, size_t n) const noexcept;
    double op_std_dev   (const double* data, size_t n) const noexcept;
    double op_median    (double* data, size_t n) const noexcept;  // sorts in place
    double op_covariance(const double* a, const double* b, size_t n) const noexcept;
    double op_pearson   (const double* a, const double* b, size_t n) const noexcept;
    double op_entropy   (const double* probs, size_t n) const noexcept;

    // ---- Number theory ----
    int64_t  op_gcd      (int64_t a, int64_t b) const noexcept;
    int64_t  op_lcm      (int64_t a, int64_t b) const noexcept;
    bool     op_is_prime (int64_t n) const noexcept;
    int64_t  op_factorial(int64_t n) const noexcept;
    int64_t  op_fibonacci(int64_t n) const noexcept;
    int64_t  op_combination (int64_t n, int64_t k) const noexcept;
    int64_t  op_permutation (int64_t n, int64_t k) const noexcept;

    // ---- Calculus approximations ----
    double op_derivative(std::function<double(double)> f, double x) const noexcept;
    double op_integral  (std::function<double(double)> f, double a, double b) const noexcept;

    // ---- Timing ----
    uint64_t rdtsc() const noexcept;
    double   elapsed_ms() const noexcept;  // since LogicProcessor init

    // ---- Batch SIMD ops (uses NEON on ARM64, AVX2 on x86) ----
    void batch_add    (const double* a, const double* b, double* out, size_t n) const noexcept;
    void batch_mul    (const double* a, const double* b, double* out, size_t n) const noexcept;
    void batch_sigmoid(const double* in, double* out, size_t n) const noexcept;
    void batch_relu   (const double* in, double* out, size_t n) const noexcept;
    void batch_softmax(const double* in, double* out, size_t n) const noexcept;

private:
    LogicProcessor() noexcept;

    uint64_t init_tsc_ = 0;

    // FFT helper
    void fft_core(double* data, size_t n, bool inverse) const noexcept;
    // Gaussian elimination helper
    bool gauss_elim(double* m, double* out, size_t dim) const noexcept;
};

// ============================================================
//  HARDWARE DIRECTOR
//  Provides direct hardware access via inline ASM
//  10% ASM portion of Quartz
// ============================================================
class HardwareDirector {
public:
    static HardwareDirector& instance() noexcept {
        static HardwareDirector inst;
        return inst;
    }

    ~HardwareDirector() = default;
    HardwareDirector(const HardwareDirector&) = delete;
    HardwareDirector& operator=(const HardwareDirector&) = delete;

    // ---- CPU info ----
    uint64_t rdtsc() const noexcept;
    void     cpuid(uint32_t leaf, uint32_t& eax, uint32_t& ebx,
                   uint32_t& ecx, uint32_t& edx) const noexcept;
    int      get_core_count() const noexcept;
    uint64_t get_cpu_freq_hz() const noexcept;

    // ---- Memory barriers ----
    void mem_fence()   const noexcept;
    void load_fence()  const noexcept;
    void store_fence() const noexcept;

    // ---- Cache control ----
    void cache_flush  (const void* addr) const noexcept;
    void prefetch_r   (const void* addr) const noexcept;
    void prefetch_w   (const void* addr) const noexcept;

    // ---- Atomic ops (using C++ atomics for portability) ----
    int64_t atomic_add  (std::atomic<int64_t>& target, int64_t val) noexcept;
    int64_t atomic_cas  (std::atomic<int64_t>& target, int64_t expected, int64_t desired) noexcept;
    int64_t atomic_xchg (std::atomic<int64_t>& target, int64_t val) noexcept;

    // ---- Sleep ----
    void sleep_ns(uint64_t ns) const noexcept;
    void cpu_pause() const noexcept;

    // ---- I/O port access (Linux /dev/port or mmap) ----
    uint8_t  port_in8 (uint16_t port) const noexcept;
    uint16_t port_in16(uint16_t port) const noexcept;
    uint32_t port_in32(uint16_t port) const noexcept;
    void     port_out8 (uint16_t port, uint8_t  val) const noexcept;
    void     port_out16(uint16_t port, uint16_t val) const noexcept;
    void     port_out32(uint16_t port, uint32_t val) const noexcept;

    // ---- Memory-mapped I/O ----
    void* mmap_phys(uint64_t phys_addr, size_t size) const noexcept;
    void  munmap_phys(void* virt_addr, size_t size) const noexcept;

    // ---- Architecture info ----
    bool is_arm64() const noexcept;
    bool is_x86_64() const noexcept;
    bool has_neon() const noexcept;
    bool has_sve() const noexcept;   // ARM64 SVE
    bool has_avx2() const noexcept;  // x86 AVX2
    bool has_avx512() const noexcept;

    // ---- Architecture name ----
    const char* arch_name() const noexcept;

private:
    HardwareDirector() noexcept;
    int    fd_port_ = -1;    // /dev/port fd if opened
    int    fd_mem_  = -1;    // /dev/mem fd if opened
};

} // namespace quartz

#endif // QUARTZ_LOGIC_PROCESSOR_H
