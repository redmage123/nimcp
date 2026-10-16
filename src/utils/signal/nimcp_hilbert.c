#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_hilbert.c - Hilbert Transform Implementation
//=============================================================================

#include "utils/signal/nimcp_hilbert.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread_pool.h"
#include <string.h>
#include <math.h>

// AVX2 SIMD support
#ifdef __AVX2__
#include <immintrin.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hilbert)

#define NIMCP_HAS_AVX2 1
#else
#define NIMCP_HAS_AVX2 0
#endif

//=============================================================================
// Internal Structures
//=============================================================================

struct hilbert_transform_t {
    hilbert_config_t config;
    neural_phasor_t* work_buffer;    ///< Working buffer for FFT
    uint32_t work_buffer_size;       ///< Size of working buffer
};

//=============================================================================
// Utility Functions
//=============================================================================

static inline bool is_power_of_2(uint32_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

static inline uint32_t next_power_of_2(uint32_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

//=============================================================================
// Configuration
//=============================================================================

hilbert_config_t hilbert_default_config(void) {
    hilbert_config_t config = {
        .auto_pad_power_of_2 = true,
        .enable_simd = true,
        .max_signal_length = 4096
    };
    return config;
}

bool hilbert_validate_config(const hilbert_config_t* config) {
    if (config == NULL) {
        return false;
    }
    if (config->max_signal_length == 0 || config->max_signal_length > 1000000) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_validate_config: config->max_signal_length is zero");
        return false;
    }
    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

hilbert_transform_t* hilbert_create(const hilbert_config_t* config) {
    // Use defaults if config is NULL
    hilbert_config_t default_config = hilbert_default_config();
    const hilbert_config_t* cfg = config ? config : &default_config;

    if (!hilbert_validate_config(cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hilbert_create: hilbert_validate_config is NULL");
        return NULL;
    }

    hilbert_transform_t* ht = (hilbert_transform_t*)nimcp_calloc(1, sizeof(hilbert_transform_t));
    if (ht == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ht is NULL");

        return NULL;
    }

    ht->config = *cfg;

    // Pre-allocate work buffer for max signal length (rounded up to power of 2)
    uint32_t buffer_size = cfg->max_signal_length;
    if (cfg->auto_pad_power_of_2) {
        buffer_size = next_power_of_2(buffer_size);
    }

    ht->work_buffer = (neural_phasor_t*)nimcp_calloc(buffer_size, sizeof(neural_phasor_t));
    if (ht->work_buffer == NULL) {
        nimcp_free(ht);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hilbert_create: validation failed");
        return NULL;
    }
    ht->work_buffer_size = buffer_size;

    return ht;
}

void hilbert_destroy(hilbert_transform_t* ht) {
    if (ht == NULL) {
        return;
    }

    if (ht->work_buffer != NULL) {
        nimcp_free(ht->work_buffer);
    }

    nimcp_free(ht);
}

//=============================================================================
// Core Transform Functions
//=============================================================================

bool hilbert_apply(hilbert_transform_t* ht,
                   const float* signal,
                   neural_phasor_t* analytic,
                   uint32_t n) {
    if (ht == NULL || signal == NULL || analytic == NULL || n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: n is zero");
        return false;
    }

    // Determine FFT size
    uint32_t fft_size = n;
    bool needs_padding = false;

    if (!is_power_of_2(n)) {
        if (ht->config.auto_pad_power_of_2) {
            fft_size = next_power_of_2(n);
            needs_padding = true;
        } else {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: validation failed");
            return false;  // Require power of 2 if auto-padding disabled
        }
    }

    if (fft_size > ht->work_buffer_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: validation failed");
        return false;  // Signal too large for pre-allocated buffer
    }

    // Use existing phasor_hilbert_transform for power-of-2 case
    if (!needs_padding) {
        return phasor_hilbert_transform(signal, analytic, n);
    }

    // Handle non-power-of-2 with padding
    // Zero-pad signal to FFT size
    for (uint32_t i = 0; i < n; i++) {
        ht->work_buffer[i].real = signal[i];
        ht->work_buffer[i].imag = 0.0F;
    }
    for (uint32_t i = n; i < fft_size; i++) {
        ht->work_buffer[i].real = 0.0F;
        ht->work_buffer[i].imag = 0.0F;
    }

    // Compute Hilbert transform on padded signal
    if (!phasor_hilbert_transform((float*)ht->work_buffer, ht->work_buffer, fft_size)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: phasor_hilbert_transform is NULL");
        return false;
    }

    // Copy non-padded portion to output
    memcpy(analytic, ht->work_buffer, n * sizeof(neural_phasor_t));

    return true;
}

bool hilbert_extract_amplitude(hilbert_transform_t* ht,
                                const float* signal,
                                float* amplitude,
                                uint32_t n) {
    if (ht == NULL || signal == NULL || amplitude == NULL || n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: n is zero");
        return false;
    }

    // Allocate temporary buffer for analytic signal
    neural_phasor_t* analytic = (neural_phasor_t*)nimcp_malloc(n * sizeof(neural_phasor_t));
    if (analytic == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: validation failed");
        return false;
    }

    // Compute analytic signal
    if (!hilbert_apply(ht, signal, analytic, n)) {
        nimcp_free(analytic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: hilbert_apply is NULL");
        return false;
    }

    // Extract amplitude with SIMD optimization if available
#if NIMCP_HAS_AVX2
    if (ht->config.enable_simd && n >= 8) {
        uint32_t simd_end = (n / 8) * 8;

        for (uint32_t i = 0; i < simd_end; i += 8) {
            // Load 8 complex numbers (16 floats)
            __m256 real = _mm256_set_ps(
                analytic[i+7].real, analytic[i+6].real, analytic[i+5].real, analytic[i+4].real,
                analytic[i+3].real, analytic[i+2].real, analytic[i+1].real, analytic[i].real
            );
            __m256 imag = _mm256_set_ps(
                analytic[i+7].imag, analytic[i+6].imag, analytic[i+5].imag, analytic[i+4].imag,
                analytic[i+3].imag, analytic[i+2].imag, analytic[i+1].imag, analytic[i].imag
            );

            // Compute magnitude: sqrt(real² + imag²)
            __m256 real_sq = _mm256_mul_ps(real, real);
            __m256 imag_sq = _mm256_mul_ps(imag, imag);
            __m256 sum = _mm256_add_ps(real_sq, imag_sq);
            __m256 mag = _mm256_sqrt_ps(sum);

            // Store result
            _mm256_storeu_ps(&amplitude[i], mag);
        }

        // Handle remainder
        for (uint32_t i = simd_end; i < n; i++) {
            amplitude[i] = phasor_amplitude(analytic[i]);
        }
    } else
#endif
    {
        // Scalar fallback
        for (uint32_t i = 0; i < n; i++) {
            amplitude[i] = phasor_amplitude(analytic[i]);
        }
    }

    nimcp_free(analytic);
    return true;
}

bool hilbert_extract_phase(hilbert_transform_t* ht,
                            const float* signal,
                            float* phase,
                            uint32_t n) {
    if (ht == NULL || signal == NULL || phase == NULL || n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: n is zero");
        return false;
    }

    // Allocate temporary buffer for analytic signal
    neural_phasor_t* analytic = (neural_phasor_t*)nimcp_malloc(n * sizeof(neural_phasor_t));
    if (analytic == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: validation failed");
        return false;
    }

    // Compute analytic signal
    if (!hilbert_apply(ht, signal, analytic, n)) {
        nimcp_free(analytic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: hilbert_apply is NULL");
        return false;
    }

    // Extract phase (atan2 not easily vectorized, use scalar)
    for (uint32_t i = 0; i < n; i++) {
        phase[i] = phasor_phase(analytic[i]);
    }

    nimcp_free(analytic);
    return true;
}

bool hilbert_extract_amplitude_phase(hilbert_transform_t* ht,
                                      const float* signal,
                                      float* amplitude,
                                      float* phase,
                                      uint32_t n) {
    if (ht == NULL || signal == NULL || amplitude == NULL || phase == NULL || n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: n is zero");
        return false;
    }

    // Allocate temporary buffer for analytic signal
    neural_phasor_t* analytic = (neural_phasor_t*)nimcp_malloc(n * sizeof(neural_phasor_t));
    if (analytic == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: validation failed");
        return false;
    }

    // Compute analytic signal once
    if (!hilbert_apply(ht, signal, analytic, n)) {
        nimcp_free(analytic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_destroy: hilbert_apply is NULL");
        return false;
    }

    // Extract both amplitude and phase
    // Use SIMD for amplitude, scalar for phase
#if NIMCP_HAS_AVX2
    if (ht->config.enable_simd && n >= 8) {
        uint32_t simd_end = (n / 8) * 8;

        for (uint32_t i = 0; i < simd_end; i += 8) {
            // Load and compute amplitude (SIMD)
            __m256 real = _mm256_set_ps(
                analytic[i+7].real, analytic[i+6].real, analytic[i+5].real, analytic[i+4].real,
                analytic[i+3].real, analytic[i+2].real, analytic[i+1].real, analytic[i].real
            );
            __m256 imag = _mm256_set_ps(
                analytic[i+7].imag, analytic[i+6].imag, analytic[i+5].imag, analytic[i+4].imag,
                analytic[i+3].imag, analytic[i+2].imag, analytic[i+1].imag, analytic[i].imag
            );

            __m256 real_sq = _mm256_mul_ps(real, real);
            __m256 imag_sq = _mm256_mul_ps(imag, imag);
            __m256 sum = _mm256_add_ps(real_sq, imag_sq);
            __m256 mag = _mm256_sqrt_ps(sum);
            _mm256_storeu_ps(&amplitude[i], mag);

            // Compute phase (scalar - atan2 not vectorizable)
            for (uint32_t j = i; j < i + 8; j++) {
                phase[j] = phasor_phase(analytic[j]);
            }
        }

        // Handle remainder
        for (uint32_t i = simd_end; i < n; i++) {
            amplitude[i] = phasor_amplitude(analytic[i]);
            phase[i] = phasor_phase(analytic[i]);
        }
    } else
#endif
    {
        // Scalar fallback
        for (uint32_t i = 0; i < n; i++) {
            amplitude[i] = phasor_amplitude(analytic[i]);
            phase[i] = phasor_phase(analytic[i]);
        }
    }

    nimcp_free(analytic);
    return true;
}

//=============================================================================
// Batch Processing with Thread Pool Parallelization
//=============================================================================

/** Minimum channels to use parallel processing (overhead threshold) */
#define HILBERT_PARALLEL_THRESHOLD 4

/** Default number of worker threads */
#define HILBERT_DEFAULT_THREADS 4

/**
 * @brief Task context for parallel Hilbert transform
 */
typedef struct {
    hilbert_transform_t* ht;      /**< Hilbert transform instance */
    const float* signal;          /**< Input signal for this channel */
    neural_phasor_t* analytic;    /**< Output analytic signal */
    uint32_t n;                   /**< Signal length */
    volatile bool success;        /**< Result flag (atomic write) */
} hilbert_apply_task_t;

/**
 * @brief Task context for parallel amplitude extraction
 */
typedef struct {
    hilbert_transform_t* ht;      /**< Hilbert transform instance */
    const float* signal;          /**< Input signal */
    float* amplitude;             /**< Output amplitude */
    uint32_t n;                   /**< Signal length */
    volatile bool success;        /**< Result flag */
} hilbert_amplitude_task_t;

/**
 * @brief Task context for parallel phase extraction
 */
typedef struct {
    hilbert_transform_t* ht;      /**< Hilbert transform instance */
    const float* signal;          /**< Input signal */
    float* phase;                 /**< Output phase */
    uint32_t n;                   /**< Signal length */
    volatile bool success;        /**< Result flag */
} hilbert_phase_task_t;

/**
 * @brief Thread pool task wrapper for hilbert_apply
 */
static void hilbert_apply_task_fn(void* arg) {
    hilbert_apply_task_t* task = (hilbert_apply_task_t*)arg;
    task->success = hilbert_apply(task->ht, task->signal, task->analytic, task->n);
}

/**
 * @brief Thread pool task wrapper for hilbert_extract_amplitude
 */
static void hilbert_amplitude_task_fn(void* arg) {
    hilbert_amplitude_task_t* task = (hilbert_amplitude_task_t*)arg;
    task->success = hilbert_extract_amplitude(task->ht, task->signal,
                                               task->amplitude, task->n);
}

/**
 * @brief Thread pool task wrapper for hilbert_extract_phase
 */
static void hilbert_phase_task_fn(void* arg) {
    hilbert_phase_task_t* task = (hilbert_phase_task_t*)arg;
    task->success = hilbert_extract_phase(task->ht, task->signal,
                                           task->phase, task->n);
}

bool hilbert_apply_batch(hilbert_transform_t* ht,
                          const float** signals,
                          neural_phasor_t** analytic,
                          uint32_t n,
                          uint32_t num_channels) {
    if (ht == NULL || signals == NULL || analytic == NULL || n == 0 || num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_phase_task_fn: n is zero");
        return false;
    }

    // Use sequential processing for small channel counts (avoid thread pool overhead)
    if (num_channels < HILBERT_PARALLEL_THRESHOLD) {
        for (uint32_t ch = 0; ch < num_channels; ch++) {
            if (!hilbert_apply(ht, signals[ch], analytic[ch], n)) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_phase_task_fn: hilbert_apply is NULL");
                return false;
            }
        }
        return true;
    }

    // Parallel processing using thread pool
    nimcp_thread_pool_t* pool = nimcp_pool_create(HILBERT_DEFAULT_THREADS);
    if (pool == NULL) {
        // Fall back to sequential processing
        for (uint32_t ch = 0; ch < num_channels; ch++) {
            if (!hilbert_apply(ht, signals[ch], analytic[ch], n)) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_phase_task_fn: hilbert_apply is NULL");
                return false;
            }
        }
        return true;
    }

    // Allocate task contexts
    hilbert_apply_task_t* tasks = (hilbert_apply_task_t*)nimcp_calloc(
        num_channels, sizeof(hilbert_apply_task_t));
    if (tasks == NULL) {
        nimcp_pool_destroy(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_phase_task_fn: validation failed");
        return false;
    }

    // Submit tasks for each channel
    for (uint32_t ch = 0; ch < num_channels; ch++) {
        tasks[ch].ht = ht;
        tasks[ch].signal = signals[ch];
        tasks[ch].analytic = analytic[ch];
        tasks[ch].n = n;
        tasks[ch].success = true;

        nimcp_result_t rc = nimcp_pool_submit(pool, hilbert_apply_task_fn, &tasks[ch]);
        if (rc != NIMCP_OK) {
            tasks[ch].success = false;
        }
    }

    // Wait for all tasks to complete
    nimcp_pool_wait(pool);

    // Check for any failures
    bool all_success = true;
    for (uint32_t ch = 0; ch < num_channels; ch++) {
        if (!tasks[ch].success) {
            all_success = false;
            break;
        }
    }

    nimcp_free(tasks);
    nimcp_pool_destroy(pool);

    return all_success;
}

bool hilbert_extract_amplitude_batch(hilbert_transform_t* ht,
                                      const float** signals,
                                      float** amplitudes,
                                      uint32_t n,
                                      uint32_t num_channels) {
    if (ht == NULL || signals == NULL || amplitudes == NULL || n == 0 || num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_phase_task_fn: n is zero");
        return false;
    }

    // Use sequential processing for small channel counts
    if (num_channels < HILBERT_PARALLEL_THRESHOLD) {
        for (uint32_t ch = 0; ch < num_channels; ch++) {
            if (!hilbert_extract_amplitude(ht, signals[ch], amplitudes[ch], n)) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_phase_task_fn: hilbert_extract_amplitude is NULL");
                return false;
            }
        }
        return true;
    }

    // Parallel processing using thread pool
    nimcp_thread_pool_t* pool = nimcp_pool_create(HILBERT_DEFAULT_THREADS);
    if (pool == NULL) {
        // Fall back to sequential
        for (uint32_t ch = 0; ch < num_channels; ch++) {
            if (!hilbert_extract_amplitude(ht, signals[ch], amplitudes[ch], n)) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_phase_task_fn: hilbert_extract_amplitude is NULL");
                return false;
            }
        }
        return true;
    }

    hilbert_amplitude_task_t* tasks = (hilbert_amplitude_task_t*)nimcp_calloc(
        num_channels, sizeof(hilbert_amplitude_task_t));
    if (tasks == NULL) {
        nimcp_pool_destroy(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_phase_task_fn: validation failed");
        return false;
    }

    for (uint32_t ch = 0; ch < num_channels; ch++) {
        tasks[ch].ht = ht;
        tasks[ch].signal = signals[ch];
        tasks[ch].amplitude = amplitudes[ch];
        tasks[ch].n = n;
        tasks[ch].success = true;

        nimcp_result_t rc = nimcp_pool_submit(pool, hilbert_amplitude_task_fn, &tasks[ch]);
        if (rc != NIMCP_OK) {
            tasks[ch].success = false;
        }
    }

    nimcp_pool_wait(pool);

    bool all_success = true;
    for (uint32_t ch = 0; ch < num_channels; ch++) {
        if (!tasks[ch].success) {
            all_success = false;
            break;
        }
    }

    nimcp_free(tasks);
    nimcp_pool_destroy(pool);

    return all_success;
}

bool hilbert_extract_phase_batch(hilbert_transform_t* ht,
                                  const float** signals,
                                  float** phases,
                                  uint32_t n,
                                  uint32_t num_channels) {
    if (ht == NULL || signals == NULL || phases == NULL || n == 0 || num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_phase_task_fn: n is zero");
        return false;
    }

    // Use sequential processing for small channel counts
    if (num_channels < HILBERT_PARALLEL_THRESHOLD) {
        for (uint32_t ch = 0; ch < num_channels; ch++) {
            if (!hilbert_extract_phase(ht, signals[ch], phases[ch], n)) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_phase_task_fn: hilbert_extract_phase is NULL");
                return false;
            }
        }
        return true;
    }

    // Parallel processing using thread pool
    nimcp_thread_pool_t* pool = nimcp_pool_create(HILBERT_DEFAULT_THREADS);
    if (pool == NULL) {
        // Fall back to sequential
        for (uint32_t ch = 0; ch < num_channels; ch++) {
            if (!hilbert_extract_phase(ht, signals[ch], phases[ch], n)) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_phase_task_fn: hilbert_extract_phase is NULL");
                return false;
            }
        }
        return true;
    }

    hilbert_phase_task_t* tasks = (hilbert_phase_task_t*)nimcp_calloc(
        num_channels, sizeof(hilbert_phase_task_t));
    if (tasks == NULL) {
        nimcp_pool_destroy(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_phase_task_fn: validation failed");
        return false;
    }

    for (uint32_t ch = 0; ch < num_channels; ch++) {
        tasks[ch].ht = ht;
        tasks[ch].signal = signals[ch];
        tasks[ch].phase = phases[ch];
        tasks[ch].n = n;
        tasks[ch].success = true;

        nimcp_result_t rc = nimcp_pool_submit(pool, hilbert_phase_task_fn, &tasks[ch]);
        if (rc != NIMCP_OK) {
            tasks[ch].success = false;
        }
    }

    nimcp_pool_wait(pool);

    bool all_success = true;
    for (uint32_t ch = 0; ch < num_channels; ch++) {
        if (!tasks[ch].success) {
            all_success = false;
            break;
        }
    }

    nimcp_free(tasks);
    nimcp_pool_destroy(pool);

    return all_success;
}

//=============================================================================
// Result Management
//=============================================================================

hilbert_result_t* hilbert_result_create(uint32_t n) {
    if (n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hilbert_result_create: n is zero");
        return NULL;
    }

    hilbert_result_t* result = (hilbert_result_t*)nimcp_calloc(1, sizeof(hilbert_result_t));
    if (result == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;
    }

    result->analytic = (neural_phasor_t*)nimcp_calloc(n, sizeof(neural_phasor_t));
    result->amplitude = (float*)nimcp_calloc(n, sizeof(float));
    result->phase = (float*)nimcp_calloc(n, sizeof(float));
    result->length = n;
    result->owns_memory = true;

    if (result->analytic == NULL || result->amplitude == NULL || result->phase == NULL) {
        hilbert_result_destroy(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hilbert_result_create: validation failed");
        return NULL;
    }

    return result;
}

void hilbert_result_destroy(hilbert_result_t* result) {
    if (result == NULL) {
        return;
    }

    if (result->owns_memory) {
        if (result->analytic != NULL) {
            nimcp_free(result->analytic);
        }
        if (result->amplitude != NULL) {
            nimcp_free(result->amplitude);
        }
        if (result->phase != NULL) {
            nimcp_free(result->phase);
        }
    }

    nimcp_free(result);
}

hilbert_result_t* hilbert_compute_full(hilbert_transform_t* ht,
                                        const float* signal,
                                        uint32_t n) {
    if (ht == NULL || signal == NULL || n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_result_destroy: n is zero");
        return NULL;
    }

    hilbert_result_t* result = hilbert_result_create(n);
    if (result == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;
    }

    // Compute analytic signal
    if (!hilbert_apply(ht, signal, result->analytic, n)) {
        hilbert_result_destroy(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hilbert_result_destroy: hilbert_apply is NULL");
        return NULL;
    }

    // Extract amplitude and phase
    if (!hilbert_extract_amplitude_phase(ht, signal, result->amplitude, result->phase, n)) {
        hilbert_result_destroy(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hilbert_result_destroy: hilbert_extract_amplitude_phase is NULL");
        return NULL;
    }

    return result;
}

//=============================================================================
// Utility Functions
//=============================================================================

bool hilbert_instantaneous_frequency(const float* phase,
                                      float* frequency,
                                      uint32_t n,
                                      float sample_rate) {
    if (phase == NULL || frequency == NULL || n < 3 || sample_rate <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_result_destroy: validation failed");
        return false;
    }

    // Unwrap phase to handle 2π discontinuities
    float* unwrapped = (float*)nimcp_malloc(n * sizeof(float));
    if (unwrapped == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hilbert_result_destroy: validation failed");
        return false;
    }

    unwrapped[0] = phase[0];
    for (uint32_t i = 1; i < n; i++) {
        float diff = phase[i] - phase[i-1];
        // Wrap difference to [-π, π]
        while (diff > M_PI) diff -= 2.0F * M_PI;
        while (diff < -M_PI) diff += 2.0F * M_PI;
        unwrapped[i] = unwrapped[i-1] + diff;
    }

    // Compute derivative using central differences
    // dφ/dt ≈ (φ[i+1] - φ[i-1]) / (2·Δt)
    float dt = 1.0F / sample_rate;

    // First point (forward difference)
    frequency[0] = (unwrapped[1] - unwrapped[0]) / dt / (2.0F * M_PI);

    // Central points (central difference)
    for (uint32_t i = 1; i < n - 1; i++) {
        frequency[i] = (unwrapped[i+1] - unwrapped[i-1]) / (2.0F * dt) / (2.0F * M_PI);
    }

    // Last point (backward difference)
    frequency[n-1] = (unwrapped[n-1] - unwrapped[n-2]) / dt / (2.0F * M_PI);

    nimcp_free(unwrapped);
    return true;
}
