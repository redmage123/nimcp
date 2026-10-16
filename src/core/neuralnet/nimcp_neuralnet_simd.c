/**
 * @file nimcp_neuralnet_simd.c
 * @brief SIMD-accelerated neural network operations implementation
 * @version 2.7.0
 * @date 2025-11-04
 */

#include "core/neuralnet/nimcp_neuralnet_simd.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include <math.h>
#include <string.h>

// Include SIMD intrinsics based on available features
#ifdef __AVX512F__
#include <immintrin.h>
#define HAS_AVX512
#endif

#ifdef __AVX2__
#include <immintrin.h>
#define HAS_AVX2
#endif

#ifdef __AVX__
#include <immintrin.h>
#define HAS_AVX
#endif

#ifdef __SSE2__
#include <emmintrin.h>
#define HAS_SSE2
#endif

//=============================================================================
// CPU Capability Detection
//=============================================================================

static nimcp_cpu_caps_t g_cpu_caps = {0};
static bool g_cpu_caps_initialized = false;

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

static void cpuid(int info[4], int function_id) {
    #if defined(_MSC_VER)
        __cpuid(info, function_id);
    #elif defined(__GNUC__) || defined(__clang__)
        __asm__ __volatile__ (
            "cpuid"
            : "=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3])
            : "a" (function_id), "c" (0)
        );
    #endif
}

static void cpuidex(int info[4], int function_id, int subfunction_id) {
    #if defined(_MSC_VER)
        __cpuidex(info, function_id, subfunction_id);
    #elif defined(__GNUC__) || defined(__clang__)
        __asm__ __volatile__ (
            "cpuid"
            : "=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3])
            : "a" (function_id), "c" (subfunction_id)
        );
    #endif
}

#endif  // x86/x64

bool nimcp_cpu_detect(nimcp_cpu_caps_t* caps) {
    if (!caps) {
        return false;
    }

    memset(caps, 0, sizeof(nimcp_cpu_caps_t));

    #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        int info[4];

        // Get CPU brand string
        int brand_string[12];
        cpuid(brand_string, 0x80000002);
        cpuid(brand_string + 4, 0x80000003);
        cpuid(brand_string + 8, 0x80000004);
        caps->cpu_brand = "x86-64 CPU";  // Simplified - could parse brand_string

        // Check feature flags
        cpuid(info, 1);

        // EDX flags
        caps->has_sse2 = (info[3] & (1 << 26)) != 0;

        // ECX flags
        caps->has_avx = (info[2] & (1 << 28)) != 0;
        caps->has_fma = (info[2] & (1 << 12)) != 0;

        // Check AVX2
        cpuidex(info, 7, 0);
        caps->has_avx2 = (info[1] & (1 << 5)) != 0;

        // Check AVX-512
        caps->has_avx512f = (info[1] & (1 << 16)) != 0;
        caps->has_avx512bw = (info[1] & (1 << 30)) != 0;

        // Determine best level
        if (caps->has_avx512f) {
            caps->best_level = NIMCP_SIMD_AVX512;
        } else if (caps->has_avx2) {
            caps->best_level = NIMCP_SIMD_AVX2;
        } else if (caps->has_avx) {
            caps->best_level = NIMCP_SIMD_AVX;
        } else if (caps->has_sse2) {
            caps->best_level = NIMCP_SIMD_SSE2;
        } else {
            caps->best_level = NIMCP_SIMD_NONE;
        }

    #else
        // Non-x86 architecture - no SIMD support
        caps->cpu_brand = "Unknown CPU";
        caps->best_level = NIMCP_SIMD_NONE;
    #endif

    // Cache results
    g_cpu_caps = *caps;
    g_cpu_caps_initialized = true;

    return true;
}

const nimcp_cpu_caps_t* nimcp_cpu_get_caps(void) {
    if (!g_cpu_caps_initialized) {
        nimcp_cpu_detect(&g_cpu_caps);
    }
    return &g_cpu_caps;
}

//=============================================================================
// Scalar Fallback Implementations
//=============================================================================

void nimcp_simd_matvec_mul_scalar(const float* matrix, const float* input,
                                   float* output, uint32_t m, uint32_t n) {
    for (uint32_t i = 0; i < m; i++) {
        float sum = 0.0f;
        const float* row = matrix + i * n;
        for (uint32_t j = 0; j < n; j++) {
            sum += row[j] * input[j];
        }
        output[i] = sum;
    }
}

static float dot_product_scalar(const float* a, const float* b, uint32_t n) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

static void vector_add_scalar(const float* a, const float* b, float* result, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        result[i] = a[i] + b[i];
    }
}

static void vector_mul_scalar(const float* a, const float* b, float* result, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        result[i] = a[i] * b[i];
    }
}

static void relu_scalar(const float* input, float* output, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        output[i] = (input[i] > 0.0f) ? input[i] : 0.0f;
    }
}

static void sigmoid_scalar(const float* input, float* output, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        output[i] = 1.0f / (1.0f + expf(-input[i]));
    }
}

//=============================================================================
// AVX2 Implementations (8-wide float)
//=============================================================================

#ifdef HAS_AVX2

void nimcp_simd_matvec_mul_avx2(const float* matrix, const float* input,
                                 float* output, uint32_t m, uint32_t n) {
    const uint32_t simd_width = 8;
    const uint32_t n_simd = (n / simd_width) * simd_width;

    for (uint32_t i = 0; i < m; i++) {
        const float* row = matrix + i * n;
        __m256 sum_vec = _mm256_setzero_ps();

        // Vectorized loop
        for (uint32_t j = 0; j < n_simd; j += simd_width) {
            __m256 row_vec = _mm256_loadu_ps(row + j);
            __m256 inp_vec = _mm256_loadu_ps(input + j);
            sum_vec = _mm256_fmadd_ps(row_vec, inp_vec, sum_vec);  // FMA: sum += row * input
        }

        // Horizontal sum
        __m128 sum_high = _mm256_extractf128_ps(sum_vec, 1);
        __m128 sum_low = _mm256_castps256_ps128(sum_vec);
        sum_low = _mm_add_ps(sum_low, sum_high);
        sum_low = _mm_hadd_ps(sum_low, sum_low);
        sum_low = _mm_hadd_ps(sum_low, sum_low);
        float sum = _mm_cvtss_f32(sum_low);

        // Scalar tail
        for (uint32_t j = n_simd; j < n; j++) {
            sum += row[j] * input[j];
        }

        output[i] = sum;
    }
}

static float dot_product_avx2(const float* a, const float* b, uint32_t n) {
    const uint32_t simd_width = 8;
    const uint32_t n_simd = (n / simd_width) * simd_width;

    __m256 sum_vec = _mm256_setzero_ps();

    for (uint32_t i = 0; i < n_simd; i += simd_width) {
        __m256 a_vec = _mm256_loadu_ps(a + i);
        __m256 b_vec = _mm256_loadu_ps(b + i);
        sum_vec = _mm256_fmadd_ps(a_vec, b_vec, sum_vec);
    }

    // Horizontal sum
    __m128 sum_high = _mm256_extractf128_ps(sum_vec, 1);
    __m128 sum_low = _mm256_castps256_ps128(sum_vec);
    sum_low = _mm_add_ps(sum_low, sum_high);
    sum_low = _mm_hadd_ps(sum_low, sum_low);
    sum_low = _mm_hadd_ps(sum_low, sum_low);
    float sum = _mm_cvtss_f32(sum_low);

    // Scalar tail
    for (uint32_t i = n_simd; i < n; i++) {
        sum += a[i] * b[i];
    }

    return sum;
}

static void vector_add_avx2(const float* a, const float* b, float* result, uint32_t n) {
    const uint32_t simd_width = 8;
    const uint32_t n_simd = (n / simd_width) * simd_width;

    for (uint32_t i = 0; i < n_simd; i += simd_width) {
        __m256 a_vec = _mm256_loadu_ps(a + i);
        __m256 b_vec = _mm256_loadu_ps(b + i);
        __m256 result_vec = _mm256_add_ps(a_vec, b_vec);
        _mm256_storeu_ps(result + i, result_vec);
    }

    // Scalar tail
    for (uint32_t i = n_simd; i < n; i++) {
        result[i] = a[i] + b[i];
    }
}

static void vector_mul_avx2(const float* a, const float* b, float* result, uint32_t n) {
    const uint32_t simd_width = 8;
    const uint32_t n_simd = (n / simd_width) * simd_width;

    for (uint32_t i = 0; i < n_simd; i += simd_width) {
        __m256 a_vec = _mm256_loadu_ps(a + i);
        __m256 b_vec = _mm256_loadu_ps(b + i);
        __m256 result_vec = _mm256_mul_ps(a_vec, b_vec);
        _mm256_storeu_ps(result + i, result_vec);
    }

    // Scalar tail
    for (uint32_t i = n_simd; i < n; i++) {
        result[i] = a[i] * b[i];
    }
}

static void relu_avx2(const float* input, float* output, uint32_t n) {
    const uint32_t simd_width = 8;
    const uint32_t n_simd = (n / simd_width) * simd_width;
    __m256 zero = _mm256_setzero_ps();

    for (uint32_t i = 0; i < n_simd; i += simd_width) {
        __m256 input_vec = _mm256_loadu_ps(input + i);
        __m256 result_vec = _mm256_max_ps(input_vec, zero);
        _mm256_storeu_ps(output + i, result_vec);
    }

    // Scalar tail
    for (uint32_t i = n_simd; i < n; i++) {
        output[i] = (input[i] > 0.0f) ? input[i] : 0.0f;
    }
}

#endif  // HAS_AVX2

//=============================================================================
// AVX-512 Implementations (16-wide float)
//=============================================================================

#ifdef HAS_AVX512

void nimcp_simd_matvec_mul_avx512(const float* matrix, const float* input,
                                   float* output, uint32_t m, uint32_t n) {
    const uint32_t simd_width = 16;
    const uint32_t n_simd = (n / simd_width) * simd_width;

    for (uint32_t i = 0; i < m; i++) {
        const float* row = matrix + i * n;
        __m512 sum_vec = _mm512_setzero_ps();

        // Vectorized loop
        for (uint32_t j = 0; j < n_simd; j += simd_width) {
            __m512 row_vec = _mm512_loadu_ps(row + j);
            __m512 inp_vec = _mm512_loadu_ps(input + j);
            sum_vec = _mm512_fmadd_ps(row_vec, inp_vec, sum_vec);
        }

        // Horizontal sum
        float sum = _mm512_reduce_add_ps(sum_vec);

        // Scalar tail
        for (uint32_t j = n_simd; j < n; j++) {
            sum += row[j] * input[j];
        }

        output[i] = sum;
    }
}

static float dot_product_avx512(const float* a, const float* b, uint32_t n) {
    const uint32_t simd_width = 16;
    const uint32_t n_simd = (n / simd_width) * simd_width;

    __m512 sum_vec = _mm512_setzero_ps();

    for (uint32_t i = 0; i < n_simd; i += simd_width) {
        __m512 a_vec = _mm512_loadu_ps(a + i);
        __m512 b_vec = _mm512_loadu_ps(b + i);
        sum_vec = _mm512_fmadd_ps(a_vec, b_vec, sum_vec);
    }

    float sum = _mm512_reduce_add_ps(sum_vec);

    // Scalar tail
    for (uint32_t i = n_simd; i < n; i++) {
        sum += a[i] * b[i];
    }

    return sum;
}

#endif  // HAS_AVX512

//=============================================================================
// Auto-Dispatching SIMD Operations
//=============================================================================

float nimcp_simd_dot_product(const float* a, const float* b, uint32_t n) {
    if (!a || !b || n == 0) {
        return 0.0f;
    }

    const nimcp_cpu_caps_t* caps = nimcp_cpu_get_caps();

    #ifdef HAS_AVX512
    if (caps->best_level >= NIMCP_SIMD_AVX512) {
        return dot_product_avx512(a, b, n);
    }
    #endif

    #ifdef HAS_AVX2
    if (caps->best_level >= NIMCP_SIMD_AVX2) {
        return dot_product_avx2(a, b, n);
    }
    #endif

    return dot_product_scalar(a, b, n);
}

void nimcp_simd_vector_add(const float* a, const float* b, float* result, uint32_t n) {
    if (!a || !b || !result || n == 0) {
        return;
    }

    const nimcp_cpu_caps_t* caps = nimcp_cpu_get_caps();

    #ifdef HAS_AVX2
    if (caps->best_level >= NIMCP_SIMD_AVX2) {
        vector_add_avx2(a, b, result, n);
        return;
    }
    #endif

    vector_add_scalar(a, b, result, n);
}

void nimcp_simd_vector_mul(const float* a, const float* b, float* result, uint32_t n) {
    if (!a || !b || !result || n == 0) {
        return;
    }

    const nimcp_cpu_caps_t* caps = nimcp_cpu_get_caps();

    #ifdef HAS_AVX2
    if (caps->best_level >= NIMCP_SIMD_AVX2) {
        vector_mul_avx2(a, b, result, n);
        return;
    }
    #endif

    vector_mul_scalar(a, b, result, n);
}

void nimcp_simd_relu(const float* input, float* output, uint32_t n) {
    if (!input || !output || n == 0) {
        return;
    }

    const nimcp_cpu_caps_t* caps = nimcp_cpu_get_caps();

    #ifdef HAS_AVX2
    if (caps->best_level >= NIMCP_SIMD_AVX2) {
        relu_avx2(input, output, n);
        return;
    }
    #endif

    relu_scalar(input, output, n);
}

void nimcp_simd_sigmoid(const float* input, float* output, uint32_t n) {
    if (!input || !output || n == 0) {
        return;
    }

    // Sigmoid is complex - use scalar for now
    // Could be optimized with polynomial approximation + SIMD
    sigmoid_scalar(input, output, n);
}

void nimcp_simd_matvec_mul(const float* matrix, const float* input,
                           float* output, uint32_t m, uint32_t n) {
    if (!matrix || !input || !output || m == 0 || n == 0) {
        return;
    }

    const nimcp_cpu_caps_t* caps = nimcp_cpu_get_caps();

    #ifdef HAS_AVX512
    if (caps->best_level >= NIMCP_SIMD_AVX512) {
        nimcp_simd_matvec_mul_avx512(matrix, input, output, m, n);
        return;
    }
    #endif

    #ifdef HAS_AVX2
    if (caps->best_level >= NIMCP_SIMD_AVX2) {
        nimcp_simd_matvec_mul_avx2(matrix, input, output, m, n);
        return;
    }
    #endif

    nimcp_simd_matvec_mul_scalar(matrix, input, output, m, n);
}

void nimcp_simd_matmat_mul(const float* a, const float* b, float* c,
                           uint32_t m, uint32_t k, uint32_t n) {
    if (!a || !b || !c || m == 0 || k == 0 || n == 0) {
        return;
    }

    // Simple implementation - could be optimized with blocking/tiling
    for (uint32_t i = 0; i < m; i++) {
        for (uint32_t j = 0; j < n; j++) {
            float sum = 0.0f;
            for (uint32_t p = 0; p < k; p++) {
                sum += a[i * k + p] * b[p * n + j];
            }
            c[i * n + j] = sum;
        }
    }
}

//=============================================================================
// SIMD-Optimized Neural Network Forward Pass
//=============================================================================

uint32_t neural_network_forward_simd(neural_network_t network,
                                      const float* input, uint32_t input_size,
                                      float* output, uint32_t output_size) {
    if (!network || !input || !output) {
        return 0;
    }

    // This is a placeholder - full implementation would integrate with
    // neural_network_t structure and use SIMD for weight multiplications
    // For now, just use the non-SIMD version
    return neural_network_forward(network, input, input_size, output, output_size);
}

//=============================================================================
// Benchmarking
//=============================================================================

float nimcp_simd_benchmark(uint32_t matrix_size, uint32_t iterations) {
    if (matrix_size == 0 || iterations == 0) {
        return 0.0f;
    }

    // Allocate test matrices
    float* matrix = (float*)nimcp_malloc(matrix_size * matrix_size * sizeof(float));
    float* input = (float*)nimcp_malloc(matrix_size * sizeof(float));
    float* output = (float*)nimcp_malloc(matrix_size * sizeof(float));

    if (!matrix || !input || !output) {
        nimcp_free(matrix);
        nimcp_free(input);
        nimcp_free(output);
        return 0.0f;
    }

    // Initialize with random data
    for (uint32_t i = 0; i < matrix_size * matrix_size; i++) {
        matrix[i] = (float)rand() / RAND_MAX;
    }
    for (uint32_t i = 0; i < matrix_size; i++) {
        input[i] = (float)rand() / RAND_MAX;
    }

    // Benchmark
    uint64_t start_ms = nimcp_time_get_ms();

    for (uint32_t iter = 0; iter < iterations; iter++) {
        nimcp_simd_matvec_mul(matrix, input, output, matrix_size, matrix_size);
    }

    uint64_t end_ms = nimcp_time_get_ms();
    float elapsed_s = (end_ms - start_ms) / 1000.0f;

    // Calculate GFLOPS
    // Each matvec: matrix_size * matrix_size multiply-adds = 2 * matrix_size^2 FLOPs
    uint64_t total_flops = (uint64_t)iterations * 2 * matrix_size * matrix_size;
    float gflops = (total_flops / 1e9f) / elapsed_s;

    // Cleanup
    nimcp_free(matrix);
    nimcp_free(input);
    nimcp_free(output);

    return gflops;
}
