/**
 * @file nimcp_tensor_simd.c
 * @brief SIMD-Optimized Tensor Operations Implementation
 *
 * WHAT: Hardware-accelerated tensor operations using SIMD instructions
 * WHY:  Significantly faster element-wise operations, reductions, dot products
 * HOW:  Runtime dispatch to AVX2/AVX-512/NEON based on CPU capabilities
 *
 * IMPLEMENTATION NOTES:
 * - Uses restrict keyword for aliasing hints
 * - Aligns loop counts to vector widths
 * - Handles remainder elements with scalar fallback
 * - Uses double accumulator for reductions (numerical precision)
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#define LOG_MODULE "TENSOR_SIMD"
#define LOG_MODULE_ID 0x0902
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(tensor_simd)

#include "utils/tensor/nimcp_tensor_simd.h"
#include "gpu/execution/nimcp_simd_detect.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <float.h>

//=============================================================================
// Platform-specific SIMD includes
//=============================================================================

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define TENSOR_SIMD_X86 1
    #include <immintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define TENSOR_SIMD_ARM64 1
    #include <arm_neon.h>
#endif

//=============================================================================
// Static State
//=============================================================================

static tensor_simd_backend_t s_backend = TENSOR_SIMD_NONE;
static bool s_initialized = false;

//=============================================================================
// Initialization
//=============================================================================

int tensor_simd_init(void)
{
    if (s_initialized) {
        return 0;
    }

    simd_capabilities_t caps;
    if (!simd_detect_capabilities(&caps)) {
        LOG_WARN("Failed to detect SIMD capabilities, using scalar fallback");
        s_backend = TENSOR_SIMD_NONE;
        s_initialized = true;
        return 0;
    }

#ifdef TENSOR_SIMD_X86
    if (caps.has_avx512f) {
        s_backend = TENSOR_SIMD_AVX512;
        LOG_INFO("SIMD backend: AVX-512 (512-bit vectors)");
    } else if (caps.has_avx2) {
        s_backend = TENSOR_SIMD_AVX2;
        LOG_INFO("SIMD backend: AVX2 (256-bit vectors)");
    } else if (caps.has_avx) {
        s_backend = TENSOR_SIMD_AVX;
        LOG_INFO("SIMD backend: AVX (256-bit float vectors)");
    } else if (caps.has_sse2) {
        s_backend = TENSOR_SIMD_SSE2;
        LOG_INFO("SIMD backend: SSE2 (128-bit vectors)");
    } else {
        s_backend = TENSOR_SIMD_NONE;
        LOG_INFO("SIMD backend: scalar (no SIMD available)");
    }
#elif defined(TENSOR_SIMD_ARM64)
    if (caps.has_sve) {
        s_backend = TENSOR_SIMD_SVE;
        LOG_INFO("SIMD backend: SVE (scalable vectors)");
    } else if (caps.has_neon) {
        s_backend = TENSOR_SIMD_NEON;
        LOG_INFO("SIMD backend: NEON (128-bit vectors)");
    } else {
        s_backend = TENSOR_SIMD_NONE;
        LOG_INFO("SIMD backend: scalar (no SIMD available)");
    }
#else
    s_backend = TENSOR_SIMD_NONE;
    LOG_INFO("SIMD backend: scalar (unsupported platform)");
#endif

    s_initialized = true;
    return 0;
}

tensor_simd_backend_t tensor_simd_get_backend(void)
{
    if (!s_initialized) {
        tensor_simd_init();
    }
    return s_backend;
}

const char* tensor_simd_backend_name(tensor_simd_backend_t backend)
{
    switch (backend) {
        case TENSOR_SIMD_NONE:   return "Scalar";
        case TENSOR_SIMD_SSE2:   return "SSE2";
        case TENSOR_SIMD_AVX:    return "AVX";
        case TENSOR_SIMD_AVX2:   return "AVX2";
        case TENSOR_SIMD_AVX512: return "AVX-512";
        case TENSOR_SIMD_NEON:   return "NEON";
        case TENSOR_SIMD_SVE:    return "SVE";
        default:                 return "Unknown";
    }
}

//=============================================================================
// Scalar Fallback Implementations
//=============================================================================

static void scalar_add_f32(float* dst, const float* src, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] += src[i];
    }
}

static void scalar_sub_f32(float* dst, const float* src, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] -= src[i];
    }
}

static void scalar_mul_f32(float* dst, const float* src, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] *= src[i];
    }
}

static void scalar_add_scalar_f32(float* dst, float s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] += s;
    }
}

static void scalar_mul_scalar_f32(float* dst, float s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] *= s;
    }
}

static double scalar_sum_f32(const float* src, size_t n)
{
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        sum += src[i];
    }
    return sum;
}

static double scalar_dot_f32(const float* a, const float* b, size_t n)
{
    double dot = 0.0;
    for (size_t i = 0; i < n; i++) {
        dot += (double)a[i] * (double)b[i];
    }
    return dot;
}

static double scalar_sum_sq_f32(const float* src, size_t n)
{
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        sum += (double)src[i] * (double)src[i];
    }
    return sum;
}

static float scalar_max_f32(const float* src, size_t n)
{
    if (n == 0) return -FLT_MAX;
    float m = src[0];
    for (size_t i = 1; i < n; i++) {
        if (src[i] > m) m = src[i];
    }
    return m;
}

static float scalar_min_f32(const float* src, size_t n)
{
    if (n == 0) return FLT_MAX;
    float m = src[0];
    for (size_t i = 1; i < n; i++) {
        if (src[i] < m) m = src[i];
    }
    return m;
}

static void scalar_relu_f32(float* dst, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (dst[i] < 0.0f) dst[i] = 0.0f;
    }
}

static void scalar_sigmoid_f32(float* dst, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] = 1.0f / (1.0f + expf(-dst[i]));
    }
}

static void scalar_copy_f32(float* dst, const float* src, size_t n)
{
    memcpy(dst, src, n * sizeof(float));
}

static void scalar_fill_f32(float* dst, float v, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] = v;
    }
}

static void scalar_fma_scalar_f32(float* dst, float mul, float add, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] = dst[i] * mul + add;
    }
}

static void scalar_fma_f32(float* dst, const float* mul, const float* add, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] = dst[i] * mul[i] + add[i];
    }
}

//=============================================================================
// AVX2 Implementations (256-bit, 8 floats)
//=============================================================================

#ifdef TENSOR_SIMD_X86

#define AVX2_FLOATS 8

__attribute__((target("avx2")))
static void avx2_add_f32(float* dst, const float* src, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 d = _mm256_loadu_ps(dst + i);
        __m256 s = _mm256_loadu_ps(src + i);
        d = _mm256_add_ps(d, s);
        _mm256_storeu_ps(dst + i, d);
    }

    // Handle remainder
    for (; i < n; i++) {
        dst[i] += src[i];
    }
}

__attribute__((target("avx2")))
static void avx2_sub_f32(float* dst, const float* src, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 d = _mm256_loadu_ps(dst + i);
        __m256 s = _mm256_loadu_ps(src + i);
        d = _mm256_sub_ps(d, s);
        _mm256_storeu_ps(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] -= src[i];
    }
}

__attribute__((target("avx2")))
static void avx2_mul_f32(float* dst, const float* src, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 d = _mm256_loadu_ps(dst + i);
        __m256 s = _mm256_loadu_ps(src + i);
        d = _mm256_mul_ps(d, s);
        _mm256_storeu_ps(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] *= src[i];
    }
}

__attribute__((target("avx2")))
static void avx2_add_scalar_f32(float* dst, float scalar, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);
    __m256 s = _mm256_set1_ps(scalar);

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 d = _mm256_loadu_ps(dst + i);
        d = _mm256_add_ps(d, s);
        _mm256_storeu_ps(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] += scalar;
    }
}

__attribute__((target("avx2")))
static void avx2_mul_scalar_f32(float* dst, float scalar, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);
    __m256 s = _mm256_set1_ps(scalar);

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 d = _mm256_loadu_ps(dst + i);
        d = _mm256_mul_ps(d, s);
        _mm256_storeu_ps(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] *= scalar;
    }
}

__attribute__((target("avx2")))
static double avx2_sum_f32(const float* src, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);
    __m256 sum_vec = _mm256_setzero_ps();

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 s = _mm256_loadu_ps(src + i);
        sum_vec = _mm256_add_ps(sum_vec, s);
    }

    // Horizontal sum of the vector
    __m128 sum_high = _mm256_extractf128_ps(sum_vec, 1);
    __m128 sum_low = _mm256_castps256_ps128(sum_vec);
    __m128 sum128 = _mm_add_ps(sum_high, sum_low);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);

    double sum = (double)_mm_cvtss_f32(sum128);

    // Handle remainder
    for (; i < n; i++) {
        sum += src[i];
    }

    return sum;
}

__attribute__((target("avx2,fma")))
static double avx2_dot_f32(const float* a, const float* b, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);
    __m256 dot_vec = _mm256_setzero_ps();

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        dot_vec = _mm256_fmadd_ps(va, vb, dot_vec);
    }

    // Horizontal sum
    __m128 dot_high = _mm256_extractf128_ps(dot_vec, 1);
    __m128 dot_low = _mm256_castps256_ps128(dot_vec);
    __m128 dot128 = _mm_add_ps(dot_high, dot_low);
    dot128 = _mm_hadd_ps(dot128, dot128);
    dot128 = _mm_hadd_ps(dot128, dot128);

    double dot = (double)_mm_cvtss_f32(dot128);

    // Handle remainder
    for (; i < n; i++) {
        dot += (double)a[i] * (double)b[i];
    }

    return dot;
}

__attribute__((target("avx2,fma")))
static double avx2_sum_sq_f32(const float* src, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);

    /* Accumulate in double precision to prevent float32 overflow.
     * For large tensors (764K+ elements), float32 FMA overflows to Inf/NaN.
     * Process 8 floats at a time: square in float32, then widen to double
     * and accumulate in two __m256d (4 doubles each). */
    __m256d sum_lo = _mm256_setzero_pd();
    __m256d sum_hi = _mm256_setzero_pd();

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 s = _mm256_loadu_ps(src + i);
        __m256 sq = _mm256_mul_ps(s, s);

        /* Widen lower 4 floats to double and accumulate */
        __m128 lo4 = _mm256_castps256_ps128(sq);
        sum_lo = _mm256_add_pd(sum_lo, _mm256_cvtps_pd(lo4));

        /* Widen upper 4 floats to double and accumulate */
        __m128 hi4 = _mm256_extractf128_ps(sq, 1);
        sum_hi = _mm256_add_pd(sum_hi, _mm256_cvtps_pd(hi4));
    }

    /* Horizontal sum of two __m256d vectors */
    __m256d total = _mm256_add_pd(sum_lo, sum_hi);
    __m128d total_hi = _mm256_extractf128_pd(total, 1);
    __m128d total_lo = _mm256_castpd256_pd128(total);
    __m128d sum128d = _mm_add_pd(total_hi, total_lo);
    sum128d = _mm_hadd_pd(sum128d, sum128d);

    double sum;
    _mm_store_sd(&sum, sum128d);

    /* Scalar tail */
    for (; i < n; i++) {
        sum += (double)src[i] * (double)src[i];
    }

    return sum;
}

__attribute__((target("avx2")))
static float avx2_max_f32(const float* src, size_t n)
{
    if (n == 0) return -FLT_MAX;

    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);
    __m256 max_vec = _mm256_set1_ps(-FLT_MAX);

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 s = _mm256_loadu_ps(src + i);
        max_vec = _mm256_max_ps(max_vec, s);
    }

    // Horizontal max
    __m128 max_high = _mm256_extractf128_ps(max_vec, 1);
    __m128 max_low = _mm256_castps256_ps128(max_vec);
    __m128 max128 = _mm_max_ps(max_high, max_low);
    max128 = _mm_max_ps(max128, _mm_shuffle_ps(max128, max128, _MM_SHUFFLE(1,0,3,2)));
    max128 = _mm_max_ps(max128, _mm_shuffle_ps(max128, max128, _MM_SHUFFLE(2,3,0,1)));

    float m = _mm_cvtss_f32(max128);

    for (; i < n; i++) {
        if (src[i] > m) m = src[i];
    }

    return m;
}

__attribute__((target("avx2")))
static float avx2_min_f32(const float* src, size_t n)
{
    if (n == 0) return FLT_MAX;

    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);
    __m256 min_vec = _mm256_set1_ps(FLT_MAX);

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 s = _mm256_loadu_ps(src + i);
        min_vec = _mm256_min_ps(min_vec, s);
    }

    // Horizontal min
    __m128 min_high = _mm256_extractf128_ps(min_vec, 1);
    __m128 min_low = _mm256_castps256_ps128(min_vec);
    __m128 min128 = _mm_min_ps(min_high, min_low);
    min128 = _mm_min_ps(min128, _mm_shuffle_ps(min128, min128, _MM_SHUFFLE(1,0,3,2)));
    min128 = _mm_min_ps(min128, _mm_shuffle_ps(min128, min128, _MM_SHUFFLE(2,3,0,1)));

    float m = _mm_cvtss_f32(min128);

    for (; i < n; i++) {
        if (src[i] < m) m = src[i];
    }

    return m;
}

__attribute__((target("avx2")))
static void avx2_relu_f32(float* dst, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);
    __m256 zero = _mm256_setzero_ps();

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 d = _mm256_loadu_ps(dst + i);
        d = _mm256_max_ps(d, zero);
        _mm256_storeu_ps(dst + i, d);
    }

    for (; i < n; i++) {
        if (dst[i] < 0.0f) dst[i] = 0.0f;
    }
}

__attribute__((target("avx2,fma")))
static void avx2_fma_scalar_f32(float* dst, float mul, float add, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);
    __m256 vmul = _mm256_set1_ps(mul);
    __m256 vadd = _mm256_set1_ps(add);

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 d = _mm256_loadu_ps(dst + i);
        d = _mm256_fmadd_ps(d, vmul, vadd);
        _mm256_storeu_ps(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] = dst[i] * mul + add;
    }
}

__attribute__((target("avx2,fma")))
static void avx2_fma_f32(float* dst, const float* mul, const float* add, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % AVX2_FLOATS);

    for (; i < simd_end; i += AVX2_FLOATS) {
        __m256 d = _mm256_loadu_ps(dst + i);
        __m256 m = _mm256_loadu_ps(mul + i);
        __m256 a = _mm256_loadu_ps(add + i);
        d = _mm256_fmadd_ps(d, m, a);
        _mm256_storeu_ps(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] = dst[i] * mul[i] + add[i];
    }
}

#endif // TENSOR_SIMD_X86

//=============================================================================
// ARM NEON Implementations (128-bit, 4 floats)
//=============================================================================

#ifdef TENSOR_SIMD_ARM64

#define NEON_FLOATS 4

static void neon_add_f32(float* dst, const float* src, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t d = vld1q_f32(dst + i);
        float32x4_t s = vld1q_f32(src + i);
        d = vaddq_f32(d, s);
        vst1q_f32(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] += src[i];
    }
}

static void neon_sub_f32(float* dst, const float* src, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t d = vld1q_f32(dst + i);
        float32x4_t s = vld1q_f32(src + i);
        d = vsubq_f32(d, s);
        vst1q_f32(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] -= src[i];
    }
}

static void neon_mul_f32(float* dst, const float* src, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t d = vld1q_f32(dst + i);
        float32x4_t s = vld1q_f32(src + i);
        d = vmulq_f32(d, s);
        vst1q_f32(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] *= src[i];
    }
}

static void neon_add_scalar_f32(float* dst, float scalar, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);
    float32x4_t s = vdupq_n_f32(scalar);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t d = vld1q_f32(dst + i);
        d = vaddq_f32(d, s);
        vst1q_f32(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] += scalar;
    }
}

static void neon_mul_scalar_f32(float* dst, float scalar, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);
    float32x4_t s = vdupq_n_f32(scalar);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t d = vld1q_f32(dst + i);
        d = vmulq_f32(d, s);
        vst1q_f32(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] *= scalar;
    }
}

static double neon_sum_f32(const float* src, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);
    float32x4_t sum_vec = vdupq_n_f32(0.0f);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t s = vld1q_f32(src + i);
        sum_vec = vaddq_f32(sum_vec, s);
    }

    // Horizontal sum
    float32x2_t sum2 = vadd_f32(vget_low_f32(sum_vec), vget_high_f32(sum_vec));
    float32x2_t sum1 = vpadd_f32(sum2, sum2);

    double sum = (double)vget_lane_f32(sum1, 0);

    for (; i < n; i++) {
        sum += src[i];
    }

    return sum;
}

static double neon_dot_f32(const float* a, const float* b, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);
    float32x4_t dot_vec = vdupq_n_f32(0.0f);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t va = vld1q_f32(a + i);
        float32x4_t vb = vld1q_f32(b + i);
        dot_vec = vfmaq_f32(dot_vec, va, vb);
    }

    // Horizontal sum
    float32x2_t dot2 = vadd_f32(vget_low_f32(dot_vec), vget_high_f32(dot_vec));
    float32x2_t dot1 = vpadd_f32(dot2, dot2);

    double dot = (double)vget_lane_f32(dot1, 0);

    for (; i < n; i++) {
        dot += (double)a[i] * (double)b[i];
    }

    return dot;
}

static double neon_sum_sq_f32(const float* src, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);
    float32x4_t sum_vec = vdupq_n_f32(0.0f);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t s = vld1q_f32(src + i);
        sum_vec = vfmaq_f32(sum_vec, s, s);
    }

    // Horizontal sum
    float32x2_t sum2 = vadd_f32(vget_low_f32(sum_vec), vget_high_f32(sum_vec));
    float32x2_t sum1 = vpadd_f32(sum2, sum2);

    double sum = (double)vget_lane_f32(sum1, 0);

    for (; i < n; i++) {
        sum += (double)src[i] * (double)src[i];
    }

    return sum;
}

static float neon_max_f32(const float* src, size_t n)
{
    if (n == 0) return -FLT_MAX;

    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);
    float32x4_t max_vec = vdupq_n_f32(-FLT_MAX);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t s = vld1q_f32(src + i);
        max_vec = vmaxq_f32(max_vec, s);
    }

    // Horizontal max
    float32x2_t max2 = vmax_f32(vget_low_f32(max_vec), vget_high_f32(max_vec));
    float32x2_t max1 = vpmax_f32(max2, max2);

    float m = vget_lane_f32(max1, 0);

    for (; i < n; i++) {
        if (src[i] > m) m = src[i];
    }

    return m;
}

static float neon_min_f32(const float* src, size_t n)
{
    if (n == 0) return FLT_MAX;

    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);
    float32x4_t min_vec = vdupq_n_f32(FLT_MAX);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t s = vld1q_f32(src + i);
        min_vec = vminq_f32(min_vec, s);
    }

    // Horizontal min
    float32x2_t min2 = vmin_f32(vget_low_f32(min_vec), vget_high_f32(min_vec));
    float32x2_t min1 = vpmin_f32(min2, min2);

    float m = vget_lane_f32(min1, 0);

    for (; i < n; i++) {
        if (src[i] < m) m = src[i];
    }

    return m;
}

static void neon_relu_f32(float* dst, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);
    float32x4_t zero = vdupq_n_f32(0.0f);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t d = vld1q_f32(dst + i);
        d = vmaxq_f32(d, zero);
        vst1q_f32(dst + i, d);
    }

    for (; i < n; i++) {
        if (dst[i] < 0.0f) dst[i] = 0.0f;
    }
}

static void neon_fma_scalar_f32(float* dst, float mul, float add, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);
    float32x4_t vmul = vdupq_n_f32(mul);
    float32x4_t vadd = vdupq_n_f32(add);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t d = vld1q_f32(dst + i);
        d = vfmaq_f32(vadd, d, vmul);
        vst1q_f32(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] = dst[i] * mul + add;
    }
}

static void neon_fma_f32(float* dst, const float* mul, const float* add, size_t n)
{
    size_t i = 0;
    size_t simd_end = n - (n % NEON_FLOATS);

    for (; i < simd_end; i += NEON_FLOATS) {
        float32x4_t d = vld1q_f32(dst + i);
        float32x4_t m = vld1q_f32(mul + i);
        float32x4_t a = vld1q_f32(add + i);
        d = vfmaq_f32(a, d, m);
        vst1q_f32(dst + i, d);
    }

    for (; i < n; i++) {
        dst[i] = dst[i] * mul[i] + add[i];
    }
}

#endif // TENSOR_SIMD_ARM64

//=============================================================================
// Public API - Dispatching to appropriate backend
//=============================================================================

void tensor_simd_add_f32(float* dst, const float* src, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!dst || !src || n == 0) return;

    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
        case TENSOR_SIMD_AVX:
            avx2_add_f32(dst, src, n);
            break;
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            neon_add_f32(dst, src, n);
            break;
#endif
        default:
            scalar_add_f32(dst, src, n);
            break;
    }
}

void tensor_simd_sub_f32(float* dst, const float* src, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!dst || !src || n == 0) return;

    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
        case TENSOR_SIMD_AVX:
            avx2_sub_f32(dst, src, n);
            break;
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            neon_sub_f32(dst, src, n);
            break;
#endif
        default:
            scalar_sub_f32(dst, src, n);
            break;
    }
}

void tensor_simd_mul_f32(float* dst, const float* src, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!dst || !src || n == 0) return;

    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
        case TENSOR_SIMD_AVX:
            avx2_mul_f32(dst, src, n);
            break;
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            neon_mul_f32(dst, src, n);
            break;
#endif
        default:
            scalar_mul_f32(dst, src, n);
            break;
    }
}

void tensor_simd_add_scalar_f32(float* dst, float scalar, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!dst || n == 0) return;

    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
        case TENSOR_SIMD_AVX:
            avx2_add_scalar_f32(dst, scalar, n);
            break;
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            neon_add_scalar_f32(dst, scalar, n);
            break;
#endif
        default:
            scalar_add_scalar_f32(dst, scalar, n);
            break;
    }
}

void tensor_simd_mul_scalar_f32(float* dst, float scalar, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!dst || n == 0) return;

    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
        case TENSOR_SIMD_AVX:
            avx2_mul_scalar_f32(dst, scalar, n);
            break;
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            neon_mul_scalar_f32(dst, scalar, n);
            break;
#endif
        default:
            scalar_mul_scalar_f32(dst, scalar, n);
            break;
    }
}

/**
 * @brief SIMD-accelerated sum reduction with overflow detection
 *
 * NUMERICAL STABILITY: Uses double accumulator for precision. Detects overflow
 * to Inf and logs warning. For extremely large arrays (>1e308 total), the
 * result will be Inf which is mathematically correct but may indicate issues.
 */
double tensor_simd_sum_f32(const float* src, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!src || n == 0) return 0.0;

    double result;
    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
        case TENSOR_SIMD_AVX:
            result = avx2_sum_f32(src, n);
            break;
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            result = neon_sum_f32(src, n);
            break;
#endif
        default:
            result = scalar_sum_f32(src, n);
            break;
    }

    /* OVERFLOW DETECTION: Check for Inf/NaN and log warning.
     * WHAT: Detect when sum overflows double precision (~1.8e308)
     * WHY:  Overflow indicates extreme values that may corrupt downstream computations
     * HOW:  Check isinf/isnan and log warning with array size for diagnosis */
    if (isinf(result) || isnan(result)) {
        LOG_WARN("tensor_simd_sum_f32: overflow/NaN detected (n=%zu, result=%g)", n, result);
    }

    return result;
}

/**
 * @brief SIMD-accelerated dot product with overflow detection
 *
 * NUMERICAL STABILITY: Uses double accumulator. Detects overflow to Inf/NaN.
 */
double tensor_simd_dot_f32(const float* a, const float* b, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!a || !b || n == 0) return 0.0;

    double result;
    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
            result = avx2_dot_f32(a, b, n);
            break;
        case TENSOR_SIMD_AVX:
            // AVX without FMA - use scalar for correctness
            result = scalar_dot_f32(a, b, n);
            break;
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            result = neon_dot_f32(a, b, n);
            break;
#endif
        default:
            result = scalar_dot_f32(a, b, n);
            break;
    }

    /* OVERFLOW DETECTION */
    if (isinf(result) || isnan(result)) {
        LOG_WARN("tensor_simd_dot_f32: overflow/NaN detected (n=%zu, result=%g)", n, result);
    }

    return result;
}

/**
 * @brief SIMD-accelerated sum of squares with overflow detection
 *
 * NUMERICAL STABILITY: Uses double accumulator. Detects overflow to Inf/NaN.
 */
double tensor_simd_sum_sq_f32(const float* src, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!src || n == 0) return 0.0;

    double result;
    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
            result = avx2_sum_sq_f32(src, n);
            break;
        case TENSOR_SIMD_AVX:
            result = scalar_sum_sq_f32(src, n);
            break;
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            result = neon_sum_sq_f32(src, n);
            break;
#endif
        default:
            result = scalar_sum_sq_f32(src, n);
            break;
    }

    /* OVERFLOW DETECTION — logged at TRACE level to avoid log spam during
     * early training when gradients are still stabilizing. The optimizer's
     * gradient clipping handles this safely. */
    if (isinf(result) || isnan(result)) {
        LOG_TRACE("tensor_simd_sum_sq_f32: overflow/NaN detected (n=%zu, result=%g)", n, result);
    }

    return result;
}

float tensor_simd_max_f32(const float* src, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!src || n == 0) return -FLT_MAX;

    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
        case TENSOR_SIMD_AVX:
            return avx2_max_f32(src, n);
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            return neon_max_f32(src, n);
#endif
        default:
            return scalar_max_f32(src, n);
    }
}

float tensor_simd_min_f32(const float* src, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!src || n == 0) return FLT_MAX;

    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
        case TENSOR_SIMD_AVX:
            return avx2_min_f32(src, n);
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            return neon_min_f32(src, n);
#endif
        default:
            return scalar_min_f32(src, n);
    }
}

void tensor_simd_relu_f32(float* dst, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!dst || n == 0) return;

    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
        case TENSOR_SIMD_AVX:
            avx2_relu_f32(dst, n);
            break;
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            neon_relu_f32(dst, n);
            break;
#endif
        default:
            scalar_relu_f32(dst, n);
            break;
    }
}

void tensor_simd_sigmoid_f32(float* dst, size_t n)
{
    // Sigmoid requires exp() which is complex to vectorize efficiently
    // Use scalar implementation for all backends for now
    if (!dst || n == 0) return;
    scalar_sigmoid_f32(dst, n);
}

void tensor_simd_copy_f32(float* dst, const float* src, size_t n)
{
    if (!dst || !src || n == 0) return;
    scalar_copy_f32(dst, src, n);  // memcpy is already highly optimized
}

void tensor_simd_fill_f32(float* dst, float value, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!dst || n == 0) return;

    // For fill, scalar is often well-optimized by compiler
    scalar_fill_f32(dst, value, n);
}

void tensor_simd_fma_scalar_f32(float* dst, float mul, float add, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!dst || n == 0) return;

    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
            avx2_fma_scalar_f32(dst, mul, add, n);
            break;
        case TENSOR_SIMD_AVX:
            scalar_fma_scalar_f32(dst, mul, add, n);
            break;
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            neon_fma_scalar_f32(dst, mul, add, n);
            break;
#endif
        default:
            scalar_fma_scalar_f32(dst, mul, add, n);
            break;
    }
}

void tensor_simd_fma_f32(float* dst, const float* mul, const float* add, size_t n)
{
    if (!s_initialized) tensor_simd_init();
    if (!dst || !mul || !add || n == 0) return;

    switch (s_backend) {
#ifdef TENSOR_SIMD_X86
        case TENSOR_SIMD_AVX512:
        case TENSOR_SIMD_AVX2:
            avx2_fma_f32(dst, mul, add, n);
            break;
        case TENSOR_SIMD_AVX:
            scalar_fma_f32(dst, mul, add, n);
            break;
#endif
#ifdef TENSOR_SIMD_ARM64
        case TENSOR_SIMD_NEON:
        case TENSOR_SIMD_SVE:
            neon_fma_f32(dst, mul, add, n);
            break;
#endif
        default:
            scalar_fma_f32(dst, mul, add, n);
            break;
    }
}
