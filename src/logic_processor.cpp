#include "../include/logic_processor.h"
#include <algorithm>
#include <numeric>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#if defined(QUARTZ_ARCH_ARM64)
  #if __has_include(<sys/auxv.h>)
    #include <sys/auxv.h>
  #endif
  #if __has_include(<asm/hwcap.h>)
    #include <asm/hwcap.h>
  #endif
  #ifndef HWCAP_SVE
    #define HWCAP_SVE (1UL << 22)
  #endif
  #ifndef AT_HWCAP
    #define AT_HWCAP 16
  #endif
#endif

// ARM64 NEON intrinsics
#if defined(QUARTZ_ARCH_ARM64)
  #include <arm_neon.h>
#endif
// x86 SSE/AVX intrinsics
#if defined(QUARTZ_ARCH_X86_64)
  #include <immintrin.h>
#endif

namespace quartz {

// ============================================================
//  LogicProcessor constructor — record init TSC
// ============================================================
LogicProcessor::LogicProcessor() noexcept {
    init_tsc_ = rdtsc();
}

// ============================================================
//  TIMING
// ============================================================
uint64_t LogicProcessor::rdtsc() const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
#else
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#endif
}

double LogicProcessor::elapsed_ms() const noexcept {
    uint64_t delta = rdtsc() - init_tsc_;
#if defined(QUARTZ_ARCH_ARM64)
    // ARM64: use virtual timer frequency
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return static_cast<double>(delta) / static_cast<double>(freq) * 1000.0;
#else
    // x86: estimate ~3GHz (use rdtscp + calibration in production)
    return static_cast<double>(delta) / 3.0e6;
#endif
}

// ============================================================
//  SAFE DIVISION / MOD / POW
// ============================================================
QZ_INLINE double LogicProcessor::op_div(double a, double b) const noexcept {
    if (QZ_UNLIKELY(b == 0.0)) return std::numeric_limits<double>::infinity();
    return a / b;
}

QZ_INLINE int64_t LogicProcessor::op_mod(int64_t a, int64_t b) const noexcept {
    if (QZ_UNLIKELY(b == 0)) return 0;
    return a % b;
}

QZ_INLINE double LogicProcessor::op_pow(double a, double b) const noexcept {
    return std::pow(a, b);
}

// ============================================================
//  GELU
// ============================================================
QZ_INLINE double LogicProcessor::op_gelu(double x) const noexcept {
    // GELU approx: x * 0.5 * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
    static constexpr double SQRT_2_PI = 0.7978845608028654;
    double c = SQRT_2_PI * (x + 0.044715 * x * x * x);
    return x * 0.5 * (1.0 + std::tanh(c));
}

// ============================================================
//  SOFTMAX
// ============================================================
void LogicProcessor::op_softmax(const double* in, double* out, size_t n) const noexcept {
    if (n == 0) return;
    double maxv = *std::max_element(in, in + n);
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        out[i] = std::exp(in[i] - maxv);
        sum += out[i];
    }
    if (sum > 0.0) {
        double inv = 1.0 / sum;
        for (size_t i = 0; i < n; ++i) out[i] *= inv;
    }
}

// ============================================================
//  VECTOR MATH
// ============================================================
double LogicProcessor::op_dot(const double* a, const double* b, size_t n) const noexcept {
    double s = 0.0;
#if defined(QUARTZ_ARCH_ARM64)
    size_t i = 0;
    float64x2_t acc = vdupq_n_f64(0.0);
    for (; i + 2 <= n; i += 2) {
        float64x2_t va = vld1q_f64(a + i);
        float64x2_t vb = vld1q_f64(b + i);
        acc = vfmaq_f64(acc, va, vb);
    }
    s = vgetq_lane_f64(acc, 0) + vgetq_lane_f64(acc, 1);
    for (; i < n; ++i) s += a[i] * b[i];
#elif defined(QUARTZ_ARCH_X86_64) && defined(__AVX2__)
    size_t i = 0;
    __m256d acc = _mm256_setzero_pd();
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        acc = _mm256_fmadd_pd(va, vb, acc);
    }
    __m128d lo = _mm256_castpd256_pd128(acc);
    __m128d hi = _mm256_extractf128_pd(acc, 1);
    lo = _mm_add_pd(lo, hi);
    lo = _mm_hadd_pd(lo, lo);
    s = _mm_cvtsd_f64(lo);
    for (; i < n; ++i) s += a[i] * b[i];
#else
    for (size_t i = 0; i < n; ++i) s += a[i] * b[i];
#endif
    return s;
}

void LogicProcessor::op_cross3(const double a[3], const double b[3], double out[3]) const noexcept {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

double LogicProcessor::op_magnitude(const double* v, size_t n) const noexcept {
    return std::sqrt(op_dot(v, v, n));
}

void LogicProcessor::op_normalize(const double* v, double* out, size_t n) const noexcept {
    double mag = op_magnitude(v, n);
    if (mag < 1e-15) { std::memset(out, 0, n * sizeof(double)); return; }
    double inv = 1.0 / mag;
    for (size_t i = 0; i < n; ++i) out[i] = v[i] * inv;
}

double LogicProcessor::op_distance(const double* a, const double* b, size_t n) const noexcept {
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double d = a[i] - b[i];
        s += d * d;
    }
    return std::sqrt(s);
}

// ============================================================
//  MATRIX MATH
// ============================================================
void LogicProcessor::op_matrix_mul(const double* a, const double* b,
                                    double* out, size_t dim) const noexcept {
    std::memset(out, 0, dim * dim * sizeof(double));
    for (size_t i = 0; i < dim; ++i) {
        for (size_t k = 0; k < dim; ++k) {
            double aik = a[i * dim + k];
            if (aik == 0.0) continue;
            for (size_t j = 0; j < dim; ++j) {
                out[i * dim + j] += aik * b[k * dim + j];
            }
        }
    }
}

void LogicProcessor::op_matrix_trans(const double* m, double* out,
                                      size_t rows, size_t cols) const noexcept {
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            out[j * rows + i] = m[i * cols + j];
}

double LogicProcessor::op_determinant(const double* m, size_t dim) const noexcept {
    if (dim == 1) return m[0];
    if (dim == 2) return m[0]*m[3] - m[1]*m[2];
    // LU with partial pivoting
    std::vector<double> tmp(m, m + dim * dim);
    std::vector<size_t> piv(dim);
    std::iota(piv.begin(), piv.end(), 0);
    double det = 1.0;
    for (size_t col = 0; col < dim; ++col) {
        size_t maxrow = col;
        double maxval = std::fabs(tmp[col * dim + col]);
        for (size_t row = col + 1; row < dim; ++row) {
            double v = std::fabs(tmp[row * dim + col]);
            if (v > maxval) { maxval = v; maxrow = row; }
        }
        if (maxrow != col) {
            std::swap_ranges(tmp.begin() + col * dim,
                             tmp.begin() + col * dim + dim,
                             tmp.begin() + maxrow * dim);
            det = -det;
        }
        double pivot = tmp[col * dim + col];
        if (std::fabs(pivot) < 1e-14) return 0.0;
        det *= pivot;
        for (size_t row = col + 1; row < dim; ++row) {
            double factor = tmp[row * dim + col] / pivot;
            for (size_t j = col; j < dim; ++j)
                tmp[row * dim + j] -= factor * tmp[col * dim + j];
        }
    }
    return det;
}

void LogicProcessor::op_matrix_inv(const double* m, double* out, size_t dim) const noexcept {
    // Gauss-Jordan elimination
    std::vector<double> aug(dim * dim * 2, 0.0);
    for (size_t i = 0; i < dim; ++i) {
        for (size_t j = 0; j < dim; ++j) aug[i * dim * 2 + j] = m[i * dim + j];
        aug[i * dim * 2 + dim + i] = 1.0;
    }
    for (size_t col = 0; col < dim; ++col) {
        size_t maxrow = col;
        double maxval = std::fabs(aug[col * dim * 2 + col]);
        for (size_t row = col + 1; row < dim; ++row) {
            double v = std::fabs(aug[row * dim * 2 + col]);
            if (v > maxval) { maxval = v; maxrow = row; }
        }
        if (maxrow != col) {
            for (size_t j = 0; j < dim * 2; ++j)
                std::swap(aug[col * dim * 2 + j], aug[maxrow * dim * 2 + j]);
        }
        double pivot = aug[col * dim * 2 + col];
        if (std::fabs(pivot) < 1e-14) continue;
        for (size_t j = 0; j < dim * 2; ++j) aug[col * dim * 2 + j] /= pivot;
        for (size_t row = 0; row < dim; ++row) {
            if (row == col) continue;
            double factor = aug[row * dim * 2 + col];
            for (size_t j = 0; j < dim * 2; ++j)
                aug[row * dim * 2 + j] -= factor * aug[col * dim * 2 + j];
        }
    }
    for (size_t i = 0; i < dim; ++i)
        for (size_t j = 0; j < dim; ++j)
            out[i * dim + j] = aug[i * dim * 2 + dim + j];
}

// ============================================================
//  FFT  (Cooley-Tukey, in-place, complex interleaved)
// ============================================================
void LogicProcessor::fft_core(double* data, size_t n, bool inverse) const noexcept {
    if (n <= 1) return;
    // Bit-reversal permutation
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(data[2*i],   data[2*j]);
            std::swap(data[2*i+1], data[2*j+1]);
        }
    }
    // FFT butterfly
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = 2.0 * M_PI / static_cast<double>(len) * (inverse ? 1.0 : -1.0);
        double wRe = std::cos(ang), wIm = std::sin(ang);
        for (size_t i = 0; i < n; i += len) {
            double curRe = 1.0, curIm = 0.0;
            for (size_t j = 0; j < len / 2; ++j) {
                double uRe = data[2*(i+j)],       uIm = data[2*(i+j)+1];
                double vRe = data[2*(i+j+len/2)], vIm = data[2*(i+j+len/2)+1];
                double tRe = curRe*vRe - curIm*vIm;
                double tIm = curRe*vIm + curIm*vRe;
                data[2*(i+j)]         = uRe + tRe;
                data[2*(i+j)+1]       = uIm + tIm;
                data[2*(i+j+len/2)]   = uRe - tRe;
                data[2*(i+j+len/2)+1] = uIm - tIm;
                double newRe = curRe*wRe - curIm*wIm;
                curIm = curRe*wIm + curIm*wRe;
                curRe = newRe;
            }
        }
    }
    if (inverse) {
        double inv = 1.0 / static_cast<double>(n);
        for (size_t i = 0; i < 2*n; ++i) data[i] *= inv;
    }
}

void LogicProcessor::op_fft(double* data, size_t n) const noexcept  { fft_core(data, n, false); }
void LogicProcessor::op_ifft(double* data, size_t n) const noexcept { fft_core(data, n, true); }

// ============================================================
//  CONVOLUTION
// ============================================================
void LogicProcessor::op_conv1d(const double* in, size_t in_len,
                                const double* kernel, size_t k_len,
                                double* out) const noexcept {
    for (size_t i = 0; i < in_len; ++i) {
        double s = 0.0;
        for (size_t k = 0; k < k_len && k <= i; ++k)
            s += in[i - k] * kernel[k];
        out[i] = s;
    }
}

void LogicProcessor::op_conv2d(const double* in, size_t iH, size_t iW,
                                const double* kernel, size_t kH, size_t kW,
                                double* out) const noexcept {
    for (size_t i = 0; i < iH; ++i) {
        for (size_t j = 0; j < iW; ++j) {
            double s = 0.0;
            for (size_t ki = 0; ki < kH; ++ki) {
                for (size_t kj = 0; kj < kW; ++kj) {
                    int si = static_cast<int>(i) - static_cast<int>(kH/2) + static_cast<int>(ki);
                    int sj = static_cast<int>(j) - static_cast<int>(kW/2) + static_cast<int>(kj);
                    if (si >= 0 && si < (int)iH && sj >= 0 && sj < (int)iW)
                        s += in[si * iW + sj] * kernel[ki * kW + kj];
                }
            }
            out[i * iW + j] = s;
        }
    }
}

// ============================================================
//  STATISTICS
// ============================================================
double LogicProcessor::op_mean(const double* data, size_t n) const noexcept {
    if (n == 0) return 0.0;
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) s += data[i];
    return s / static_cast<double>(n);
}

double LogicProcessor::op_variance(const double* data, size_t n) const noexcept {
    if (n < 2) return 0.0;
    double m = op_mean(data, n);
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) { double d = data[i] - m; s += d * d; }
    return s / static_cast<double>(n - 1);  // Bessel's correction
}

double LogicProcessor::op_std_dev(const double* data, size_t n) const noexcept {
    return std::sqrt(op_variance(data, n));
}

double LogicProcessor::op_median(double* data, size_t n) const noexcept {
    if (n == 0) return 0.0;
    std::sort(data, data + n);
    if (n % 2 == 1) return data[n / 2];
    return (data[n/2 - 1] + data[n/2]) * 0.5;
}

double LogicProcessor::op_covariance(const double* a, const double* b, size_t n) const noexcept {
    if (n < 2) return 0.0;
    double ma = op_mean(a, n), mb = op_mean(b, n);
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) s += (a[i] - ma) * (b[i] - mb);
    return s / static_cast<double>(n - 1);
}

double LogicProcessor::op_pearson(const double* a, const double* b, size_t n) const noexcept {
    double cov = op_covariance(a, b, n);
    double sa = op_std_dev(a, n), sb = op_std_dev(b, n);
    if (sa < 1e-15 || sb < 1e-15) return 0.0;
    return cov / (sa * sb);
}

double LogicProcessor::op_entropy(const double* probs, size_t n) const noexcept {
    double h = 0.0;
    for (size_t i = 0; i < n; ++i) {
        if (probs[i] > 1e-15) h -= probs[i] * std::log2(probs[i]);
    }
    return h;
}

// ============================================================
//  NUMBER THEORY
// ============================================================
int64_t LogicProcessor::op_gcd(int64_t a, int64_t b) const noexcept {
    a = std::abs(a); b = std::abs(b);
    while (b) { int64_t t = b; b = a % b; a = t; }
    return a;
}

int64_t LogicProcessor::op_lcm(int64_t a, int64_t b) const noexcept {
    if (a == 0 || b == 0) return 0;
    return std::abs(a / op_gcd(a, b) * b);
}

bool LogicProcessor::op_is_prime(int64_t n) const noexcept {
    if (n < 2) return false;
    if (n < 4) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int64_t i = 5; i * i <= n; i += 6)
        if (n % i == 0 || n % (i + 2) == 0) return false;
    return true;
}

int64_t LogicProcessor::op_factorial(int64_t n) const noexcept {
    if (n < 0) return -1;
    if (n > 20) return std::numeric_limits<int64_t>::max();  // overflow guard
    int64_t r = 1;
    for (int64_t i = 2; i <= n; ++i) r *= i;
    return r;
}

int64_t LogicProcessor::op_fibonacci(int64_t n) const noexcept {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    int64_t a = 0, b = 1;
    for (int64_t i = 2; i <= n; ++i) {
        int64_t c = a + b; a = b; b = c;
    }
    return b;
}

int64_t LogicProcessor::op_combination(int64_t n, int64_t k) const noexcept {
    if (k < 0 || k > n) return 0;
    if (k == 0 || k == n) return 1;
    if (k > n - k) k = n - k;
    int64_t r = 1;
    for (int64_t i = 0; i < k; ++i) {
        r = r * (n - i) / (i + 1);
    }
    return r;
}

int64_t LogicProcessor::op_permutation(int64_t n, int64_t k) const noexcept {
    if (k < 0 || k > n) return 0;
    int64_t r = 1;
    for (int64_t i = 0; i < k; ++i) r *= (n - i);
    return r;
}

// ============================================================
//  CALCULUS APPROXIMATIONS
// ============================================================
double LogicProcessor::op_derivative(std::function<double(double)> f, double x) const noexcept {
    // 5-point stencil for high accuracy
    static constexpr double h = 1e-5;
    return (-f(x + 2*h) + 8*f(x + h) - 8*f(x - h) + f(x - 2*h)) / (12.0 * h);
}

double LogicProcessor::op_integral(std::function<double(double)> f,
                                    double a, double b) const noexcept {
    // Simpson's 3/8 adaptive rule
    static constexpr int N = 1000;
    double h = (b - a) / static_cast<double>(N);
    double s = f(a) + f(b);
    for (int i = 1; i < N; ++i) {
        double x = a + i * h;
        s += (i % 2 == 0 ? 2.0 : 4.0) * f(x);
    }
    return s * h / 3.0;
}

// ============================================================
//  BATCH SIMD OPS
// ============================================================
void LogicProcessor::batch_add(const double* a, const double* b,
                                double* out, size_t n) const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    size_t i = 0;
    for (; i + 2 <= n; i += 2) {
        float64x2_t va = vld1q_f64(a + i);
        float64x2_t vb = vld1q_f64(b + i);
        vst1q_f64(out + i, vaddq_f64(va, vb));
    }
    for (; i < n; ++i) out[i] = a[i] + b[i];
#elif defined(QUARTZ_ARCH_X86_64) && defined(__AVX2__)
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        _mm256_storeu_pd(out + i, _mm256_add_pd(va, vb));
    }
    for (; i < n; ++i) out[i] = a[i] + b[i];
#else
    for (size_t i = 0; i < n; ++i) out[i] = a[i] + b[i];
#endif
}

void LogicProcessor::batch_mul(const double* a, const double* b,
                                double* out, size_t n) const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    size_t i = 0;
    for (; i + 2 <= n; i += 2) {
        float64x2_t va = vld1q_f64(a + i);
        float64x2_t vb = vld1q_f64(b + i);
        vst1q_f64(out + i, vmulq_f64(va, vb));
    }
    for (; i < n; ++i) out[i] = a[i] * b[i];
#elif defined(QUARTZ_ARCH_X86_64) && defined(__AVX2__)
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        _mm256_storeu_pd(out + i, _mm256_mul_pd(va, vb));
    }
    for (; i < n; ++i) out[i] = a[i] * b[i];
#else
    for (size_t i = 0; i < n; ++i) out[i] = a[i] * b[i];
#endif
}

void LogicProcessor::batch_sigmoid(const double* in, double* out, size_t n) const noexcept {
    for (size_t i = 0; i < n; ++i) out[i] = op_sigmoid(in[i]);
}

void LogicProcessor::batch_relu(const double* in, double* out, size_t n) const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    size_t i = 0;
    float64x2_t zero = vdupq_n_f64(0.0);
    for (; i + 2 <= n; i += 2) {
        float64x2_t v = vld1q_f64(in + i);
        vst1q_f64(out + i, vmaxq_f64(v, zero));
    }
    for (; i < n; ++i) out[i] = in[i] > 0.0 ? in[i] : 0.0;
#else
    for (size_t i = 0; i < n; ++i) out[i] = in[i] > 0.0 ? in[i] : 0.0;
#endif
}

void LogicProcessor::batch_softmax(const double* in, double* out, size_t n) const noexcept {
    op_softmax(in, out, n);
}

// ============================================================
//  HARDWARE DIRECTOR IMPLEMENTATION
// ============================================================
HardwareDirector::HardwareDirector() noexcept {}

uint64_t HardwareDirector::rdtsc() const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val) :: "memory");
    return val;
#else
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi) :: "memory");
    return (static_cast<uint64_t>(hi) << 32) | lo;
#endif
}

void HardwareDirector::cpuid(uint32_t leaf,
                              uint32_t& eax, uint32_t& ebx,
                              uint32_t& ecx, uint32_t& edx) const noexcept {
#if defined(QUARTZ_ARCH_X86_64)
    asm volatile("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(leaf), "c"(0));
#else
    eax = ebx = ecx = edx = 0;
#endif
}

int HardwareDirector::get_core_count() const noexcept {
    return static_cast<int>(::sysconf(_SC_NPROCESSORS_ONLN));
}

uint64_t HardwareDirector::get_cpu_freq_hz() const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
#else
    // Try reading from /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq
    int fd = ::open("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", O_RDONLY);
    if (fd < 0) return 3000000000ULL;
    char buf[32] = {};
    // Read CPU frequency - ignore return value intentionally
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-result"
    ::read(fd, buf, sizeof(buf) - 1);
    #pragma GCC diagnostic pop
    ::close(fd);
    uint64_t khz = static_cast<uint64_t>(::atoll(buf));
    return khz * 1000ULL;
#endif
}

void HardwareDirector::mem_fence() const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    asm volatile("dmb ish" ::: "memory");
#else
    asm volatile("mfence" ::: "memory");
#endif
}

void HardwareDirector::load_fence() const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    asm volatile("dmb ishld" ::: "memory");
#else
    asm volatile("lfence" ::: "memory");
#endif
}

void HardwareDirector::store_fence() const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    asm volatile("dmb ishst" ::: "memory");
#else
    asm volatile("sfence" ::: "memory");
#endif
}

void HardwareDirector::cache_flush(const void* addr) const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    asm volatile("dc civac, %0" :: "r"(addr) : "memory");
#else
    asm volatile("clflush (%0)" :: "r"(addr) : "memory");
#endif
}

void HardwareDirector::prefetch_r(const void* addr) const noexcept {
    __builtin_prefetch(addr, 0, 3);
}

void HardwareDirector::prefetch_w(const void* addr) const noexcept {
    __builtin_prefetch(addr, 1, 3);
}

int64_t HardwareDirector::atomic_add(std::atomic<int64_t>& target, int64_t val) noexcept {
    return target.fetch_add(val, std::memory_order_acq_rel);
}

int64_t HardwareDirector::atomic_cas(std::atomic<int64_t>& target,
                                      int64_t expected, int64_t desired) noexcept {
    target.compare_exchange_strong(expected, desired, std::memory_order_acq_rel);
    return expected;
}

int64_t HardwareDirector::atomic_xchg(std::atomic<int64_t>& target, int64_t val) noexcept {
    return target.exchange(val, std::memory_order_acq_rel);
}

void HardwareDirector::sleep_ns(uint64_t ns) const noexcept {
    struct timespec ts;
    ts.tv_sec  = static_cast<time_t>(ns / 1000000000ULL);
    ts.tv_nsec = static_cast<long>(ns % 1000000000ULL);
    ::clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
}

void HardwareDirector::cpu_pause() const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    asm volatile("yield" ::: "memory");
#else
    asm volatile("pause" ::: "memory");
#endif
}

uint8_t  HardwareDirector::port_in8 (uint16_t port) const noexcept {
#if defined(QUARTZ_ARCH_X86_64)
    uint8_t v;
    asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
#else
    (void)port; return 0;
#endif
}

uint16_t HardwareDirector::port_in16(uint16_t port) const noexcept {
#if defined(QUARTZ_ARCH_X86_64)
    uint16_t v;
    asm volatile("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
#else
    (void)port; return 0;
#endif
}

uint32_t HardwareDirector::port_in32(uint16_t port) const noexcept {
#if defined(QUARTZ_ARCH_X86_64)
    uint32_t v;
    asm volatile("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
#else
    (void)port; return 0;
#endif
}

void HardwareDirector::port_out8 (uint16_t port, uint8_t  val) const noexcept {
#if defined(QUARTZ_ARCH_X86_64)
    asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
#else
    (void)port; (void)val;
#endif
}

void HardwareDirector::port_out16(uint16_t port, uint16_t val) const noexcept {
#if defined(QUARTZ_ARCH_X86_64)
    asm volatile("outw %0, %1" :: "a"(val), "Nd"(port));
#else
    (void)port; (void)val;
#endif
}

void HardwareDirector::port_out32(uint16_t port, uint32_t val) const noexcept {
#if defined(QUARTZ_ARCH_X86_64)
    asm volatile("outl %0, %1" :: "a"(val), "Nd"(port));
#else
    (void)port; (void)val;
#endif
}

void* HardwareDirector::mmap_phys(uint64_t phys_addr, size_t size) const noexcept {
    if (fd_mem_ < 0) {
        int fd = ::open("/dev/mem", O_RDWR | O_SYNC);
        if (fd < 0) return nullptr;
        const_cast<HardwareDirector*>(this)->fd_mem_ = fd;
    }
    void* p = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                     fd_mem_, static_cast<off_t>(phys_addr));
    return p == MAP_FAILED ? nullptr : p;
}

void HardwareDirector::munmap_phys(void* virt_addr, size_t size) const noexcept {
    ::munmap(virt_addr, size);
}

bool HardwareDirector::is_arm64()  const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    return true;
#else
    return false;
#endif
}

bool HardwareDirector::is_x86_64() const noexcept {
#if defined(QUARTZ_ARCH_X86_64)
    return true;
#else
    return false;
#endif
}

bool HardwareDirector::has_neon() const noexcept {
#if defined(QUARTZ_ARCH_ARM64)
    return true;  // All ARMv8-A have NEON
#else
    return false;
#endif
}

bool HardwareDirector::has_sve() const noexcept {
#if defined(QUARTZ_ARCH_ARM64) && defined(AT_HWCAP) && defined(HWCAP_SVE)
    unsigned long hwcap = ::getauxval(AT_HWCAP);
    return (hwcap & static_cast<unsigned long>(HWCAP_SVE)) != 0;
#else
    return false;
#endif
}

bool HardwareDirector::has_avx2() const noexcept {
#if defined(QUARTZ_ARCH_X86_64)
    uint32_t eax, ebx, ecx, edx;
    cpuid(7, eax, ebx, ecx, edx);
    return (ebx & (1U << 5)) != 0;
#else
    return false;
#endif
}

bool HardwareDirector::has_avx512() const noexcept {
#if defined(QUARTZ_ARCH_X86_64)
    uint32_t eax, ebx, ecx, edx;
    cpuid(7, eax, ebx, ecx, edx);
    return (ebx & (1U << 16)) != 0;
#else
    return false;
#endif
}

const char* HardwareDirector::arch_name() const noexcept {
    return QUARTZ_ARCH_NAME;
}

} // namespace quartz
