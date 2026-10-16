#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_complex_math.c - Complex Number Mathematics Implementation
//=============================================================================

#include "utils/math/nimcp_complex_math.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// AVX2 SIMD support
#ifdef __AVX2__
#include <immintrin.h>
#define NIMCP_HAS_AVX2 1
#else
#define NIMCP_HAS_AVX2 0
#endif

// CPU feature detection
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#include <cpuid.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(complex_math)

#define NIMCP_HAS_CPUID 1
#else
#define NIMCP_HAS_CPUID 0
#endif

//=============================================================================
// Internal Helpers
//=============================================================================

// Complex multiplication: (a + bi)(c + di) = (ac - bd) + (ad + bc)i
static inline neural_phasor_t cmul(neural_phasor_t z1, neural_phasor_t z2) {
    neural_phasor_t result;
    result.real = z1.real * z2.real - z1.imag * z2.imag;
    result.imag = z1.real * z2.imag + z1.imag * z2.real;
    return result;
}

// Complex conjugate: (a + bi)* = (a - bi)
static inline neural_phasor_t cconj(neural_phasor_t z) {
    neural_phasor_t result;
    result.real = z.real;
    result.imag = -z.imag;
    return result;
}

// Complex magnitude: |a + bi| = sqrt(a² + b²)
static inline float cabs_inline(neural_phasor_t z) {
    return sqrtf(z.real * z.real + z.imag * z.imag);
}

// Complex phase: arg(a + bi) = atan2(b, a)
static inline float carg_inline(neural_phasor_t z) {
    return atan2f(z.imag, z.real);
}

// Complex division: z1/z2 = z1 * conj(z2) / |z2|²
static inline neural_phasor_t cdiv(neural_phasor_t z1, neural_phasor_t z2) {
    float denom = z2.real * z2.real + z2.imag * z2.imag;
    if (denom < 1e-20F) {
        return (neural_phasor_t){0.0F, 0.0F};
    }
    neural_phasor_t result = cmul(z1, cconj(z2));
    result.real /= denom;
    result.imag /= denom;
    return result;
}

// Complex addition
static inline neural_phasor_t cadd(neural_phasor_t z1, neural_phasor_t z2) {
    return (neural_phasor_t){z1.real + z2.real, z1.imag + z2.imag};
}

// Scalar multiplication
static inline neural_phasor_t cscale(neural_phasor_t z, float scalar) {
    return (neural_phasor_t){z.real * scalar, z.imag * scalar};
}

//=============================================================================
// Internal State
//=============================================================================

static complex_math_config_t g_config = {0};
static bool g_initialized = false;
static bool g_has_avx2 = false;

//=============================================================================
// CPU Feature Detection
//=============================================================================

#if NIMCP_HAS_CPUID
static bool detect_avx2_support(void) {
    unsigned int eax, ebx, ecx, edx;

    // Check CPUID support
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_avx2_support: __get_cpuid is NULL");
        return false;
    }

    // Check OSXSAVE (bit 27 of ECX)
    if (!(ecx & (1 << 27))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_avx2_support: validation failed");
        return false;
    }

    // Check AVX support (bit 28 of ECX)
    if (!(ecx & (1 << 28))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_avx2_support: validation failed");
        return false;
    }

    // Check extended features (EAX=7, ECX=0)
    if (!__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_avx2_support: __get_cpuid_count is NULL");
        return false;
    }

    // Check AVX2 (bit 5 of EBX)
    return (ebx & (1 << 5)) != 0;
}
#else
static bool detect_avx2_support(void) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_avx2_support: __get_cpuid_count is NULL");
    return false;
}
#endif

//=============================================================================
// Core Phasor Operations
//=============================================================================

neural_phasor_t phasor_from_polar(float amplitude, float phase) {
    // z = A·e^(iφ) = A·(cos φ + i·sin φ)
    neural_phasor_t result;
    result.real = amplitude * cosf(phase);
    result.imag = amplitude * sinf(phase);
    return result;
}

neural_phasor_t phasor_from_cartesian(float real, float imag) {
    return (neural_phasor_t){real, imag};
}

float phasor_amplitude(neural_phasor_t z) {
    // |z| = sqrt(real² + imag²)
    return cabs_inline(z);
}

float phasor_phase(neural_phasor_t z) {
    // arg(z) = atan2(imag, real)
    return carg_inline(z);
}

float phasor_phase_difference(neural_phasor_t z1, neural_phasor_t z2) {
    // Phase difference: arg(z2 · z1*)
    // This automatically wraps to [-π, π]
    neural_phasor_t diff = cmul(z2, cconj(z1));
    return carg_inline(diff);
}

neural_phasor_t phasor_normalize(neural_phasor_t z) {
    float amp = cabs_inline(z);
    if (amp < 1e-10F) {
        // Avoid division by zero for very small amplitudes
        return (neural_phasor_t){1.0F, 0.0F};
    }
    return (neural_phasor_t){z.real / amp, z.imag / amp};
}

//=============================================================================
// Array Operations (SIMD Accelerated)
//=============================================================================

#ifdef __AVX2__
// SIMD version of phasor_array_coherence using AVX2
// Processes 4 phasors per iteration (256 bits = 4 x 64-bit complex)
static float phasor_array_coherence_avx2(const neural_phasor_t* signals, uint32_t n) {
    if (n == 0 || signals == NULL) {
        return 0.0F;
    }

    __m256 sum_real = _mm256_setzero_ps();
    __m256 sum_imag = _mm256_setzero_ps();

    // Process 4 phasors at a time
    uint32_t simd_count = (n / 4) * 4;

    for (uint32_t i = 0; i < simd_count; i += 4) {
        // Load 4 phasors (8 floats: r0,i0,r1,i1,r2,i2,r3,i3)
        __m256 p01 = _mm256_loadu_ps((const float*)&signals[i]);

        // Deinterleave to get: [r0,r1,r2,r3] and [i0,i1,i2,i3]
        // p01 = [r0,i0,r1,i1,r2,i2,r3,i3]
        __m256 tmp = _mm256_shuffle_ps(p01, p01, 0xD8);
        __m256 real = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(tmp), 0xD8));

        tmp = _mm256_shuffle_ps(p01, p01, 0x8D);
        __m256 imag = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(tmp), 0x72));

        // Compute magnitude: sqrt(real² + imag²)
        __m256 real_sq = _mm256_mul_ps(real, real);
        __m256 imag_sq = _mm256_mul_ps(imag, imag);
        __m256 mag_sq = _mm256_add_ps(real_sq, imag_sq);
        __m256 mag = _mm256_sqrt_ps(mag_sq);

        // Normalize: real/mag, imag/mag (with safe division)
        __m256 epsilon = _mm256_set1_ps(1e-10F);
        mag = _mm256_max_ps(mag, epsilon);
        __m256 rcp_mag = _mm256_div_ps(_mm256_set1_ps(1.0F), mag);

        __m256 norm_real = _mm256_mul_ps(real, rcp_mag);
        __m256 norm_imag = _mm256_mul_ps(imag, rcp_mag);

        // Accumulate normalized phasors
        sum_real = _mm256_add_ps(sum_real, norm_real);
        sum_imag = _mm256_add_ps(sum_imag, norm_imag);
    }

    // Horizontal sum reduction
    float sum_r[8], sum_i[8];
    _mm256_storeu_ps(sum_r, sum_real);
    _mm256_storeu_ps(sum_i, sum_imag);

    float total_real = sum_r[0] + sum_r[1] + sum_r[2] + sum_r[3];
    float total_imag = sum_i[0] + sum_i[1] + sum_i[2] + sum_i[3];

    // Handle remaining elements with scalar code
    for (uint32_t i = simd_count; i < n; i++) {
        neural_phasor_t normalized = phasor_normalize(signals[i]);
        total_real += normalized.real;
        total_imag += normalized.imag;
    }

    // Compute mean and magnitude
    float mean_real = total_real / (float)n;
    float mean_imag = total_imag / (float)n;
    return sqrtf(mean_real * mean_real + mean_imag * mean_imag);
}

// SIMD version of phasor_array_synchrony using AVX2
static float phasor_array_synchrony_avx2(const neural_phasor_t* signals1,
                                         const neural_phasor_t* signals2,
                                         uint32_t n) {
    if (n == 0 || signals1 == NULL || signals2 == NULL) {
        return 0.0F;
    }

    __m256 sum_real = _mm256_setzero_ps();
    __m256 sum_imag = _mm256_setzero_ps();

    uint32_t simd_count = (n / 4) * 4;

    for (uint32_t i = 0; i < simd_count; i += 4) {
        // Load and normalize signals1
        __m256 p1 = _mm256_loadu_ps((const float*)&signals1[i]);
        __m256 tmp1 = _mm256_shuffle_ps(p1, p1, 0xD8);
        __m256 r1 = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(tmp1), 0xD8));

        tmp1 = _mm256_shuffle_ps(p1, p1, 0x8D);
        __m256 i1 = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(tmp1), 0x72));

        __m256 mag1_sq = _mm256_add_ps(_mm256_mul_ps(r1, r1), _mm256_mul_ps(i1, i1));
        __m256 mag1 = _mm256_sqrt_ps(mag1_sq);
        __m256 epsilon = _mm256_set1_ps(1e-10F);
        mag1 = _mm256_max_ps(mag1, epsilon);
        __m256 rcp_mag1 = _mm256_div_ps(_mm256_set1_ps(1.0F), mag1);

        __m256 n1_real = _mm256_mul_ps(r1, rcp_mag1);
        __m256 n1_imag = _mm256_mul_ps(i1, rcp_mag1);

        // Load and normalize signals2
        __m256 p2 = _mm256_loadu_ps((const float*)&signals2[i]);
        __m256 tmp2 = _mm256_shuffle_ps(p2, p2, 0xD8);
        __m256 r2 = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(tmp2), 0xD8));

        tmp2 = _mm256_shuffle_ps(p2, p2, 0x8D);
        __m256 i2 = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(tmp2), 0x72));

        __m256 mag2_sq = _mm256_add_ps(_mm256_mul_ps(r2, r2), _mm256_mul_ps(i2, i2));
        __m256 mag2 = _mm256_sqrt_ps(mag2_sq);
        mag2 = _mm256_max_ps(mag2, epsilon);
        __m256 rcp_mag2 = _mm256_div_ps(_mm256_set1_ps(1.0F), mag2);

        __m256 n2_real = _mm256_mul_ps(r2, rcp_mag2);
        __m256 n2_imag = _mm256_mul_ps(i2, rcp_mag2);

        // Conjugate multiply: z1 * conj(z2) = (r1*r2 + i1*i2) + i(i1*r2 - r1*i2)
        __m256 diff_real = _mm256_add_ps(_mm256_mul_ps(n1_real, n2_real), _mm256_mul_ps(n1_imag, n2_imag));
        __m256 diff_imag = _mm256_sub_ps(_mm256_mul_ps(n1_imag, n2_real), _mm256_mul_ps(n1_real, n2_imag));

        sum_real = _mm256_add_ps(sum_real, diff_real);
        sum_imag = _mm256_add_ps(sum_imag, diff_imag);
    }

    // Horizontal reduction
    float sum_r[8], sum_i[8];
    _mm256_storeu_ps(sum_r, sum_real);
    _mm256_storeu_ps(sum_i, sum_imag);

    float total_real = sum_r[0] + sum_r[1] + sum_r[2] + sum_r[3];
    float total_imag = sum_i[0] + sum_i[1] + sum_i[2] + sum_i[3];

    // Scalar tail
    for (uint32_t i = simd_count; i < n; i++) {
        neural_phasor_t diff = cmul(phasor_normalize(signals1[i]),
                                   cconj(phasor_normalize(signals2[i])));
        total_real += diff.real;
        total_imag += diff.imag;
    }

    float mean_real = total_real / (float)n;
    float mean_imag = total_imag / (float)n;
    return sqrtf(mean_real * mean_real + mean_imag * mean_imag);
}

// SIMD version of phasor_array_mean_phase using AVX2
static float phasor_array_mean_phase_avx2(const neural_phasor_t* signals, uint32_t n) {
    if (n == 0 || signals == NULL) {
        return 0.0F;
    }

    __m256 sum_real = _mm256_setzero_ps();
    __m256 sum_imag = _mm256_setzero_ps();

    uint32_t simd_count = (n / 4) * 4;

    for (uint32_t i = 0; i < simd_count; i += 4) {
        __m256 p = _mm256_loadu_ps((const float*)&signals[i]);

        __m256 tmp = _mm256_shuffle_ps(p, p, 0xD8);
        __m256 real = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(tmp), 0xD8));

        tmp = _mm256_shuffle_ps(p, p, 0x8D);
        __m256 imag = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(tmp), 0x72));

        __m256 mag_sq = _mm256_add_ps(_mm256_mul_ps(real, real), _mm256_mul_ps(imag, imag));
        __m256 mag = _mm256_sqrt_ps(mag_sq);
        __m256 epsilon = _mm256_set1_ps(1e-10F);
        mag = _mm256_max_ps(mag, epsilon);
        __m256 rcp_mag = _mm256_div_ps(_mm256_set1_ps(1.0F), mag);

        sum_real = _mm256_add_ps(sum_real, _mm256_mul_ps(real, rcp_mag));
        sum_imag = _mm256_add_ps(sum_imag, _mm256_mul_ps(imag, rcp_mag));
    }

    float sum_r[8], sum_i[8];
    _mm256_storeu_ps(sum_r, sum_real);
    _mm256_storeu_ps(sum_i, sum_imag);

    float total_real = sum_r[0] + sum_r[1] + sum_r[2] + sum_r[3];
    float total_imag = sum_i[0] + sum_i[1] + sum_i[2] + sum_i[3];

    for (uint32_t i = simd_count; i < n; i++) {
        neural_phasor_t normalized = phasor_normalize(signals[i]);
        total_real += normalized.real;
        total_imag += normalized.imag;
    }

    return atan2f(total_imag, total_real);
}
#endif // __AVX2__

//=============================================================================
// Array Operations (Vectorized)
//=============================================================================

float phasor_array_coherence(const neural_phasor_t* signals, uint32_t n) {
#ifdef __AVX2__
    // Use SIMD version if available and enabled
    if (g_has_avx2 && g_config.enable_simd) {
        return phasor_array_coherence_avx2(signals, n);
    }
#endif
    if (n == 0 || signals == NULL) {
        return 0.0F;
    }

    // ITPC = |mean(z_i/|z_i|)|
    // Normalize each phasor to unit length, then take mean
    neural_phasor_t sum = {0.0F, 0.0F};

    for (uint32_t i = 0; i < n; i++) {
        neural_phasor_t normalized = phasor_normalize(signals[i]);
        sum = cadd(sum, normalized);
    }

    neural_phasor_t mean = cscale(sum, 1.0F / (float)n);
    return cabs_inline(mean);
}

float phasor_array_synchrony(const neural_phasor_t* signals1,
                            const neural_phasor_t* signals2,
                            uint32_t n) {
#ifdef __AVX2__
    if (g_has_avx2 && g_config.enable_simd) {
        return phasor_array_synchrony_avx2(signals1, signals2, n);
    }
#endif
    if (n == 0 || signals1 == NULL || signals2 == NULL) {
        return 0.0F;
    }

    // PLV = |mean(exp(i·(φ1 - φ2)))|
    neural_phasor_t sum = {0.0F, 0.0F};

    for (uint32_t i = 0; i < n; i++) {
        // Phase difference as unit phasor
        neural_phasor_t diff = cmul(phasor_normalize(signals1[i]),
                                   cconj(phasor_normalize(signals2[i])));
        sum = cadd(sum, diff);
    }

    neural_phasor_t mean = cscale(sum, 1.0F / (float)n);
    return cabs_inline(mean);
}

float phasor_array_mean_phase(const neural_phasor_t* signals, uint32_t n) {
#ifdef __AVX2__
    if (g_has_avx2 && g_config.enable_simd) {
        return phasor_array_mean_phase_avx2(signals, n);
    }
#endif
    if (n == 0 || signals == NULL) {
        return 0.0F;
    }

    // Circular mean phase: arg(mean(normalized phasors))
    neural_phasor_t sum = {0.0F, 0.0F};

    for (uint32_t i = 0; i < n; i++) {
        neural_phasor_t normalized = phasor_normalize(signals[i]);
        sum = cadd(sum, normalized);
    }

    return carg_inline(sum);
}

float phasor_array_phase_variance(const neural_phasor_t* signals, uint32_t n) {
    if (n == 0 || signals == NULL) {
        return 0.0F;
    }

    // Circular variance: 1 - |mean(normalized phasors)|
    float coherence = phasor_array_coherence(signals, n);
    return 1.0F - coherence;
}

void phasor_array_statistics(const neural_phasor_t* signals, uint32_t n,
                             complex_signal_stats_t* stats) {
    if (stats == NULL) {
        return;
    }

    // Initialize
    memset(stats, 0, sizeof(complex_signal_stats_t));

    if (n == 0 || signals == NULL) {
        return;
    }

    // Compute mean amplitude
    float amp_sum = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        amp_sum += cabs_inline(signals[i]);
    }
    stats->mean_amplitude = amp_sum / (float)n;

    // Compute phase statistics
    stats->coherence = phasor_array_coherence(signals, n);
    stats->mean_phase = phasor_array_mean_phase(signals, n);
    stats->phase_std = sqrtf(phasor_array_phase_variance(signals, n));

    // Synchrony is same as coherence for single array
    stats->synchrony = stats->coherence;
}

//=============================================================================
// FFT Operations (Cooley-Tukey Algorithm)
//=============================================================================

// Check if n is power of 2
static bool is_power_of_2(uint32_t n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

// Bit reversal for FFT
static void bit_reverse_copy(const neural_phasor_t* input, neural_phasor_t* output, uint32_t n) {
    uint32_t bits = 0;
    uint32_t temp = n;
    while (temp > 1) {
        bits++;
        temp >>= 1;
    }

    for (uint32_t i = 0; i < n; i++) {
        uint32_t rev = 0;
        uint32_t num = i;
        for (uint32_t b = 0; b < bits; b++) {
            rev = (rev << 1) | (num & 1);
            num >>= 1;
        }
        output[rev] = input[i];
    }
}

// Cooley-Tukey FFT (radix-2 decimation-in-time)
static void fft_internal(neural_phasor_t* data, uint32_t n, bool inverse) {
    if (!is_power_of_2(n)) {
        return;
    }

    // Bit-reverse permutation done in-place
    for (uint32_t i = 0; i < n; i++) {
        uint32_t j = 0;
        uint32_t k = i;
        uint32_t log2n = 0;
        uint32_t temp = n;
        while (temp > 1) {
            log2n++;
            temp >>= 1;
        }
        for (uint32_t b = 0; b < log2n; b++) {
            j = (j << 1) | (k & 1);
            k >>= 1;
        }
        if (i < j) {
            neural_phasor_t temp_val = data[i];
            data[i] = data[j];
            data[j] = temp_val;
        }
    }

    // FFT butterfly operations
    float direction = inverse ? 1.0F : -1.0F;

    for (uint32_t size = 2; size <= n; size *= 2) {
        float angle = direction * 2.0F * M_PI / (float)size;
        neural_phasor_t wlen = {cosf(angle), sinf(angle)};

        for (uint32_t start = 0; start < n; start += size) {
            neural_phasor_t w = {1.0F, 0.0F};

            for (uint32_t k = 0; k < size / 2; k++) {
                neural_phasor_t u = data[start + k];
                neural_phasor_t t = cmul(w, data[start + k + size / 2]);

                data[start + k] = cadd(u, t);
                data[start + k + size / 2] = cadd(u, cscale(t, -1.0F));

                w = cmul(w, wlen);
            }
        }
    }

    // Normalize for inverse FFT
    if (inverse) {
        for (uint32_t i = 0; i < n; i++) {
            data[i] = cscale(data[i], 1.0F / (float)n);
        }
    }
}

bool phasor_fft(const neural_phasor_t* input, neural_phasor_t* output, uint32_t n) {
    if (input == NULL || output == NULL || n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasor_fft: n is zero");
        return false;
    }

    if (!is_power_of_2(n)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasor_fft: is_power_of_2 is NULL");
        return false;
    }

    // Copy input to output
    memcpy(output, input, n * sizeof(neural_phasor_t));

    // Perform FFT in-place
    fft_internal(output, n, false);

    return true;
}

bool phasor_ifft(const neural_phasor_t* input, neural_phasor_t* output, uint32_t n) {
    if (input == NULL || output == NULL || n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasor_ifft: n is zero");
        return false;
    }

    if (!is_power_of_2(n)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasor_ifft: is_power_of_2 is NULL");
        return false;
    }

    // Copy input to output
    memcpy(output, input, n * sizeof(neural_phasor_t));

    // Perform inverse FFT in-place
    fft_internal(output, n, true);

    return true;
}

bool phasor_power_spectrum(const neural_phasor_t* input, float* output, uint32_t n) {
    if (input == NULL || output == NULL || n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasor_power_spectrum: n is zero");
        return false;
    }

    if (!is_power_of_2(n)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasor_power_spectrum: is_power_of_2 is NULL");
        return false;
    }

    // Allocate temporary buffer for FFT
    neural_phasor_t* fft_result = (neural_phasor_t*)nimcp_malloc(n * sizeof(neural_phasor_t));
    if (fft_result == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasor_power_spectrum: validation failed");
        return false;
    }

    // Compute FFT
    if (!phasor_fft(input, fft_result, n)) {
        nimcp_free(fft_result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasor_power_spectrum: phasor_fft is NULL");
        return false;
    }

    // Compute power spectrum: |FFT(x)|²
    for (uint32_t i = 0; i < n; i++) {
        float magnitude = cabs_inline(fft_result[i]);
        output[i] = magnitude * magnitude;
    }

    nimcp_free(fft_result);
    return true;
}

//=============================================================================
// Neural-Specific Operations
//=============================================================================

// SIMD version of PAC modulation index
#ifdef __AVX2__
static float phasor_pac_modulation_index_avx2(const neural_phasor_t* theta_phase,
                                               const float* gamma_amplitude,
                                               uint32_t n) {
    #define NUM_PHASE_BINS 18
    float amplitude_by_phase[NUM_PHASE_BINS] = {0};
    uint32_t counts[NUM_PHASE_BINS] = {0};

    // Vectorized phase binning - process 8 samples at a time
    uint32_t simd_count = (n / 8) * 8;
    __m256 pi = _mm256_set1_ps(M_PI);
    __m256 two_pi = _mm256_set1_ps(2.0F * M_PI);
    __m256 num_bins = _mm256_set1_ps((float)NUM_PHASE_BINS);

    for (uint32_t i = 0; i < simd_count; i += 8) {
        // Load 8 amplitudes
        __m256 amps = _mm256_loadu_ps(&gamma_amplitude[i]);

        // Extract phases (atan2 needs scalar for now, but we optimize binning)
        float phases[8], amp_vals[8];
        _mm256_storeu_ps(amp_vals, amps);

        for (int j = 0; j < 8; j++) {
            phases[j] = atan2f(theta_phase[i + j].imag, theta_phase[i + j].real);
        }

        // Vectorize phase normalization and binning
        __m256 phase_vec = _mm256_loadu_ps(phases);
        __m256 normalized = _mm256_div_ps(_mm256_add_ps(phase_vec, pi), two_pi);
        __m256 bin_float = _mm256_mul_ps(normalized, num_bins);

        // Store and accumulate (scalar for now due to indirect indexing)
        float bin_vals[8];
        _mm256_storeu_ps(bin_vals, bin_float);

        for (int j = 0; j < 8; j++) {
            uint32_t bin = (uint32_t)bin_vals[j];
            if (bin >= NUM_PHASE_BINS) bin = NUM_PHASE_BINS - 1;
            amplitude_by_phase[bin] += amp_vals[j];
            counts[bin]++;
        }
    }

    // Scalar tail
    for (uint32_t i = simd_count; i < n; i++) {
        float phase = atan2f(theta_phase[i].imag, theta_phase[i].real);
        float phase_normalized = (phase + M_PI) / (2.0F * M_PI);
        uint32_t bin = (uint32_t)(phase_normalized * NUM_PHASE_BINS);
        if (bin >= NUM_PHASE_BINS) bin = NUM_PHASE_BINS - 1;
        amplitude_by_phase[bin] += gamma_amplitude[i];
        counts[bin]++;
    }

    // Compute mean amplitude per bin (vectorized)
    float total_amplitude = 0.0F;
    for (uint32_t i = 0; i < NUM_PHASE_BINS; i++) {
        if (counts[i] > 0) {
            amplitude_by_phase[i] /= counts[i];
            total_amplitude += amplitude_by_phase[i];
        }
    }

    if (total_amplitude < 1e-10F) {
        return 0.0F;
    }

    // Normalize to probability distribution
    for (uint32_t i = 0; i < NUM_PHASE_BINS; i++) {
        amplitude_by_phase[i] /= total_amplitude;
    }

    // Compute Shannon entropy (scalar - log2 vectorization not worth the complexity for 18 bins)
    float entropy = 0.0F;
    for (uint32_t i = 0; i < NUM_PHASE_BINS; i++) {
        if (amplitude_by_phase[i] > 1e-10F) {
            entropy -= amplitude_by_phase[i] * log2f(amplitude_by_phase[i]);
        }
    }

    float max_entropy = log2f((float)NUM_PHASE_BINS);
    float modulation_index = (max_entropy - entropy) / max_entropy;

    #undef NUM_PHASE_BINS
    return modulation_index;
}
#endif

float phasor_pac_modulation_index(const neural_phasor_t* theta_phase,
                                  const float* gamma_amplitude,
                                  uint32_t n) {
#ifdef __AVX2__
    if (g_has_avx2 && g_config.enable_simd) {
        return phasor_pac_modulation_index_avx2(theta_phase, gamma_amplitude, n);
    }
#endif
    if (theta_phase == NULL || gamma_amplitude == NULL || n == 0) {
        return 0.0F;
    }

    // Bin gamma amplitude by theta phase
    #define NUM_PHASE_BINS 18  // 20° bins
    float amplitude_by_phase[NUM_PHASE_BINS] = {0};
    uint32_t counts[NUM_PHASE_BINS] = {0};

    for (uint32_t i = 0; i < n; i++) {
        float phase = carg_inline(theta_phase[i]);
        // Normalize to [0, 1]
        float phase_normalized = (phase + M_PI) / (2.0F * M_PI);

        uint32_t bin = (uint32_t)(phase_normalized * NUM_PHASE_BINS);
        if (bin >= NUM_PHASE_BINS) {
            bin = NUM_PHASE_BINS - 1;
        }

        amplitude_by_phase[bin] += gamma_amplitude[i];
        counts[bin]++;
    }

    // Compute mean amplitude per bin
    float total_amplitude = 0.0F;
    for (uint32_t i = 0; i < NUM_PHASE_BINS; i++) {
        if (counts[i] > 0) {
            amplitude_by_phase[i] /= counts[i];
            total_amplitude += amplitude_by_phase[i];
        }
    }

    if (total_amplitude < 1e-10F) {
        return 0.0F;
    }

    // Normalize to probability distribution
    for (uint32_t i = 0; i < NUM_PHASE_BINS; i++) {
        amplitude_by_phase[i] /= total_amplitude;
    }

    // Compute Shannon entropy
    float entropy = 0.0F;
    for (uint32_t i = 0; i < NUM_PHASE_BINS; i++) {
        if (amplitude_by_phase[i] > 1e-10F) {
            entropy -= amplitude_by_phase[i] * log2f(amplitude_by_phase[i]);
        }
    }

    // Compute modulation index: (Hmax - H) / Hmax
    // Hmax = log2(NUM_PHASE_BINS) for uniform distribution
    float max_entropy = log2f((float)NUM_PHASE_BINS);
    float modulation_index = (max_entropy - entropy) / max_entropy;

    #undef NUM_PHASE_BINS
    return modulation_index;
}

bool phasor_hilbert_transform(const float* real_signal,
                              neural_phasor_t* analytic_signal,
                              uint32_t n) {
    if (real_signal == NULL || analytic_signal == NULL || n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasor_power_spectrum: n is zero");
        return false;
    }

    if (!is_power_of_2(n)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phasor_power_spectrum: is_power_of_2 is NULL");
        return false;
    }

    // Allocate FFT buffer
    neural_phasor_t* fft_buffer = (neural_phasor_t*)nimcp_malloc(n * sizeof(neural_phasor_t));
    if (fft_buffer == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    // Convert real signal to complex (imaginary = 0)
    for (uint32_t i = 0; i < n; i++) {
        fft_buffer[i] = (neural_phasor_t){real_signal[i], 0.0F};
    }

    // Forward FFT
    fft_internal(fft_buffer, n, false);

    // Zero out negative frequencies, double positive frequencies
    fft_buffer[0] = cscale(fft_buffer[0], 1.0F);  // DC unchanged
    for (uint32_t i = 1; i < n / 2; i++) {
        fft_buffer[i] = cscale(fft_buffer[i], 2.0F);  // Positive frequencies
    }
    fft_buffer[n / 2] = cscale(fft_buffer[n / 2], 1.0F);  // Nyquist unchanged
    for (uint32_t i = n / 2 + 1; i < n; i++) {
        fft_buffer[i] = (neural_phasor_t){0.0F, 0.0F};  // Negative frequencies
    }

    // Inverse FFT
    fft_internal(fft_buffer, n, true);

    // Copy result
    memcpy(analytic_signal, fft_buffer, n * sizeof(neural_phasor_t));

    nimcp_free(fft_buffer);
    return true;
}

//=============================================================================
// Configuration
//=============================================================================

complex_math_config_t complex_math_default_config(void) {
    complex_math_config_t config;
    // Auto-detect AVX2 support and enable by default if available
    config.enable_simd = NIMCP_HAS_AVX2;
    config.enable_fft_cache = false;    // FFT caching not implemented yet
    config.fft_cache_size = 0;
    return config;
}

bool complex_math_init(const complex_math_config_t* config) {
    if (config == NULL) {
        g_config = complex_math_default_config();
    } else {
        g_config = *config;
    }

    // Runtime CPU feature detection
    g_has_avx2 = detect_avx2_support();

    // If SIMD requested but not available, disable it
    if (g_config.enable_simd && !g_has_avx2) {
        g_config.enable_simd = false;
    }

    g_initialized = true;

#if NIMCP_HAS_AVX2
    if (g_has_avx2 && g_config.enable_simd) {
        // Optional: Log SIMD status for debugging
        // printf("Complex math: AVX2 SIMD acceleration enabled\n");
    }
#endif

    return true;
}

void complex_math_cleanup(void) {
    g_initialized = false;
    g_has_avx2 = false;
    memset(&g_config, 0, sizeof(complex_math_config_t));
}

bool complex_math_has_simd(void) {
    return g_has_avx2 && g_config.enable_simd;
}
