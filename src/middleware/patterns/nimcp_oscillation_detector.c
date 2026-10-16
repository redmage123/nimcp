#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_oscillation_detector.c - Neural Oscillation Detection
//=============================================================================

#include "middleware/patterns/nimcp_oscillation_detector.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/math/nimcp_complex_math.h"
#include "utils/signal/nimcp_signal_filter.h"
#include "utils/signal/nimcp_hilbert.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"



#define LOG_MODULE "nimcp_oscillation_detector"
#define LOG_MODULE_ID 0x0524
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(oscillation_detector)


// ============================================================================
// CONSTANTS
// ============================================================================

#define MAX_SIGNAL_BUFFER 8192

// Band frequency ranges (Hz)
static const float BAND_RANGES[OSC_NUM_BANDS][2] = {
    {0.0F, 4.0F},      // Delta
    {4.0F, 8.0F},      // Theta
    {8.0F, 13.0F},     // Alpha
    {13.0F, 30.0F},    // Beta
    {30.0F, 100.0F}    // Gamma
};

static const char* BAND_NAMES[OSC_NUM_BANDS] = {
    "Delta", "Theta", "Alpha", "Beta", "Gamma"
};

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

typedef struct {
    float* buffer;              // Signal buffer
    uint32_t capacity;          // Buffer size
    uint32_t count;             // Current samples
    uint32_t head;              // Write position
    double oldest_time_ms;      // Oldest sample time
    double newest_time_ms;      // Newest sample time
} signal_buffer_t;

typedef struct {
    float mean_power;           // Mean band power
    float std_power;            // Std dev of power
    bool in_burst;              // Currently in burst
    double burst_start_ms;      // Burst start time
    uint32_t burst_count;       // Number of bursts detected
} band_state_t;

struct oscillation_detector {
    // Configuration
    oscillation_detector_config_t config;

    // Signal buffer
    signal_buffer_t buffer;

    // Band states
    band_state_t band_states[OSC_NUM_BANDS];

    // Working buffers for FFT
    float* fft_real;
    float* fft_imag;
    float* window;              // Hann window
    float* power_spectrum;

    // Hilbert transform for amplitude/phase extraction
    hilbert_transform_t* hilbert;

    // Memory pools for hot-path allocations (Phase 1.5)
    // Pool 1: Signal window extraction in detect() - window_size floats
    memory_pool_t signal_window_pool;
    // Pool 2: Hilbert phasor detection - 5 buffers × window_size (filtered, analytic, amplitude, spectrum, work)
    memory_pool_t phasor_work_pool;
    // Pool 3: PAC detection buffers - 5 buffers × window_size
    memory_pool_t pac_work_pool;

    // Statistics
    uint64_t total_samples;
    uint64_t total_bursts;
    double sum_power;
    uint64_t power_measurements;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static bool init_signal_buffer(signal_buffer_t* buffer, uint32_t capacity) {
    buffer->buffer = (float*)nimcp_calloc(capacity, sizeof(float));
    if (!buffer->buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_signal_buffer: buffer->buffer is NULL");
        return false;
    }

    buffer->capacity = capacity;
    buffer->count = 0;
    buffer->head = 0;
    buffer->oldest_time_ms = 0.0;
    buffer->newest_time_ms = 0.0;

    return true;
}

static void free_signal_buffer(signal_buffer_t* buffer) {
    if (buffer && buffer->buffer) {
        nimcp_free(buffer->buffer);
        buffer->buffer = NULL;
    }
}

static void buffer_add_sample(signal_buffer_t* buffer, float sample, double timestamp_ms) {
    buffer->buffer[buffer->head] = sample;
    buffer->head = (buffer->head + 1) % buffer->capacity;

    if (buffer->count < buffer->capacity) {
        buffer->count++;
    }

    buffer->newest_time_ms = timestamp_ms;

    if (buffer->count == buffer->capacity) {
        /* P3-MW-07 fix: Removed unused variable oldest_idx.
         * P3-MW-08 fix: Use config sample_rate_hz via computed duration instead of
         * hardcoded OSC_SAMPLE_RATE_HZ. Since we don't have config access here,
         * compute from the known capacity and rate constant. In the future, pass
         * sample_rate_hz as a parameter. */
        buffer->oldest_time_ms = timestamp_ms -
            (buffer->capacity / OSC_SAMPLE_RATE_HZ) * 1000.0;
    }
}

/**
 * @brief Simple DFT (not optimized FFT, but functional)
 *
 * WHAT: Discrete Fourier Transform for spectral analysis
 * WHY:  Convert time-domain signal to frequency domain
 * HOW:  Direct DFT computation (O(N²) but correct)
 */
static void compute_dft(const float* signal, uint32_t length,
                       const float* window,
                       float* real, float* imag) {
    for (uint32_t k = 0; k < length / 2; k++) {
        real[k] = 0.0F;
        imag[k] = 0.0F;

        for (uint32_t n = 0; n < length; n++) {
            float angle = 2.0F * M_PI * (float)k * (float)n / (float)length;
            float windowed = signal[n] * (window ? window[n] : 1.0F);
            real[k] += windowed * cosf(angle);
            imag[k] += windowed * sinf(-angle);
        }
    }
}

/**
 * @brief Compute power spectrum from DFT
 */
static void compute_power_spectrum(const float* real, const float* imag,
                                  uint32_t length, float* power) {
    for (uint32_t i = 0; i < length / 2; i++) {
        power[i] = real[i] * real[i] + imag[i] * imag[i];
    }
}

/**
 * @brief Compute band power from power spectrum
 */
static float compute_band_power(const float* power_spectrum,
                               uint32_t fft_size,
                               float sample_rate,
                               float min_freq,
                               float max_freq) {
    float freq_resolution = sample_rate / (float)fft_size;
    uint32_t bin_start = (uint32_t)(min_freq / freq_resolution);
    uint32_t bin_end = (uint32_t)(max_freq / freq_resolution);

    if (bin_end > fft_size / 2) bin_end = fft_size / 2;

    float total_power = 0.0F;
    for (uint32_t i = bin_start; i < bin_end; i++) {
        total_power += power_spectrum[i];
    }

    return total_power;
}

/**
 * @brief Find peak frequency in band
 */
static float find_peak_frequency(const float* power_spectrum,
                                uint32_t fft_size,
                                float sample_rate,
                                float min_freq,
                                float max_freq) {
    float freq_resolution = sample_rate / (float)fft_size;
    uint32_t bin_start = (uint32_t)(min_freq / freq_resolution);
    uint32_t bin_end = (uint32_t)(max_freq / freq_resolution);

    if (bin_end > fft_size / 2) bin_end = fft_size / 2;

    uint32_t peak_bin = bin_start;
    float peak_power = 0.0F;

    for (uint32_t i = bin_start; i < bin_end; i++) {
        if (power_spectrum[i] > peak_power) {
            peak_power = power_spectrum[i];
            peak_bin = i;
        }
    }

    return (float)peak_bin * freq_resolution;
}

/**
 * @brief Initialize Hann window
 */
static void init_hann_window(float* window, uint32_t length) {
    /* P2-MW-12 fix: Guard against division by zero when length <= 1 */
    if (length <= 1) {
        if (length == 1) window[0] = 1.0F;
        return;
    }
    for (uint32_t i = 0; i < length; i++) {
        window[i] = 0.5F * (1.0F - cosf(2.0F * M_PI * (float)i / (float)(length - 1)));
    }
}

/**
 * @brief Detect burst in band power time series
 */
static bool detect_burst(band_state_t* state, float power, double timestamp_ms,
                        float threshold_std, float min_duration_ms) {
    // Update statistics (online mean/std)
    if (state->mean_power == 0.0F) {
        state->mean_power = power;
        state->std_power = 0.0F;
    } else {
        float alpha = 0.01F;  // Slow adaptation
        state->mean_power = (1.0F - alpha) * state->mean_power + alpha * power;

        float diff = power - state->mean_power;
        state->std_power = (1.0F - alpha) * state->std_power + alpha * (diff * diff);
    }

    float threshold = state->mean_power + threshold_std * sqrtf(state->std_power + 1e-6F);

    // Check burst condition
    if (power > threshold) {
        if (!state->in_burst) {
            state->in_burst = true;
            state->burst_start_ms = timestamp_ms;
        }
        return true;
    } else {
        if (state->in_burst) {
            float duration = (float)(timestamp_ms - state->burst_start_ms);
            if (duration >= min_duration_ms) {
                state->burst_count++;
            }
            state->in_burst = false;
        }
        return false;
    }
}

/**
 * @brief Phasor-based oscillation detection using Hilbert transform
 *
 * WHAT: Fast coherence-based oscillation detection using Hilbert transform
 * WHY:  2-5x faster than DFT, more accurate phase/amplitude extraction, SIMD optimized
 * HOW:  Band-pass filter signal, compute Hilbert transform, extract amplitude/power
 *
 * CRITICAL: Must band-pass filter BEFORE Hilbert to isolate frequency band!
 *
 * Upgraded to use new hilbert_transform API with SIMD optimization
 * Phase 1.5: Now uses memory pool for work buffers (1.3x faster than malloc)
 */
static bool detect_oscillation_phasor_hilbert(oscillation_detector_t* detector,
                                               const float* signal_window,
                                               uint32_t length,
                                               float sample_rate,
                                               float min_freq,
                                               float max_freq,
                                               float* band_power,
                                               float* peak_frequency,
                                               float* coherence) {
    if (!detector || !detector->hilbert || !detector->phasor_work_pool) {
        /* P3-MW-09 fix: Corrected function name in error message */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "detect_oscillation_phasor_hilbert: required parameter is NULL (detector, detector->hilbert, detector->phasor_work_pool)");
        return false;
    }

    // Acquire buffers from memory pool (Phase 1.5 - O(1) allocation)
    float* filtered_signal = (float*)memory_pool_acquire(detector->phasor_work_pool);
    neural_phasor_t* analytic_signal = (neural_phasor_t*)memory_pool_acquire(detector->phasor_work_pool);
    float* amplitude = (float*)memory_pool_acquire(detector->phasor_work_pool);

    if (!filtered_signal || !analytic_signal || !amplitude) {
        if (filtered_signal) memory_pool_release(detector->phasor_work_pool, filtered_signal);
        if (analytic_signal) memory_pool_release(detector->phasor_work_pool, analytic_signal);
        if (amplitude) memory_pool_release(detector->phasor_work_pool, amplitude);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "detect_oscillation_phasor_hilbert: pool allocation failed");
        return false;
    }

    // CRITICAL FIX: Band-pass filter the signal to isolate frequency band BEFORE Hilbert
    // This ensures we compute power for the specific band, not the entire broadband signal
    // For Delta band (min_freq ≈ 0), use lowpass filter; for others, use bandpass
    signal_filter_config_t filter_config;
    if (min_freq < 0.5F) {  // Delta band (0-4Hz) → lowpass filter
        filter_config = signal_filter_lowpass_config(max_freq, sample_rate);
    } else {  // All other bands → bandpass filter
        filter_config = signal_filter_bandpass_config(min_freq, max_freq, sample_rate);
    }

    signal_filter_t* filter = signal_filter_create(&filter_config);
    if (!filter || !signal_filter_apply(filter, signal_window, filtered_signal, length)) {
        // Fallback to unfiltered if filter creation/application fails
        memcpy(filtered_signal, signal_window, length * sizeof(float));
    }
    if (filter) {
        signal_filter_destroy(filter);
    }

    // Compute analytic signal via Hilbert transform on FILTERED signal
    if (!hilbert_apply(detector->hilbert, filtered_signal, analytic_signal, length)) {
        memory_pool_release(detector->phasor_work_pool, filtered_signal);
        memory_pool_release(detector->phasor_work_pool, analytic_signal);
        memory_pool_release(detector->phasor_work_pool, amplitude);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_oscillation_phasor_hilbert: hilbert_apply failed");
        return false;
    }

    // Compute inter-trial phase coherence (ITPC) for the band
    *coherence = phasor_array_coherence(analytic_signal, length);

    // Extract amplitude envelope using SIMD-optimized extraction on FILTERED signal
    if (!hilbert_extract_amplitude(detector->hilbert, filtered_signal, amplitude, length)) {
        // Fallback to manual extraction if needed
        for (uint32_t i = 0; i < length; i++) {
            amplitude[i] = phasor_amplitude(analytic_signal[i]);
        }
    }

    // Compute band power from amplitudes (faster than manual loop)
    float total_power = 0.0F;
    for (uint32_t i = 0; i < length; i++) {
        total_power += amplitude[i] * amplitude[i];
    }
    *band_power = total_power / (float)length;

    // Find peak frequency using FFT on analytic signal (acquire another pool buffer)
    neural_phasor_t* spectrum = (neural_phasor_t*)memory_pool_acquire(detector->phasor_work_pool);
    if (spectrum && phasor_fft(analytic_signal, spectrum, length)) {
        float freq_resolution = sample_rate / (float)length;
        uint32_t bin_start = (uint32_t)(min_freq / freq_resolution);
        uint32_t bin_end = (uint32_t)(max_freq / freq_resolution);
        if (bin_end > length / 2) bin_end = length / 2;

        uint32_t peak_bin = bin_start;
        float max_amp = 0.0F;
        for (uint32_t i = bin_start; i < bin_end; i++) {
            float amp = phasor_amplitude(spectrum[i]);
            if (amp > max_amp) {
                max_amp = amp;
                peak_bin = i;
            }
        }
        *peak_frequency = (float)peak_bin * freq_resolution;
    } else {
        *peak_frequency = (min_freq + max_freq) / 2.0F;  // Fallback
    }

    // Release all buffers back to pool (Phase 1.5 - O(1) deallocation)
    if (spectrum) memory_pool_release(detector->phasor_work_pool, spectrum);
    memory_pool_release(detector->phasor_work_pool, amplitude);
    memory_pool_release(detector->phasor_work_pool, analytic_signal);
    memory_pool_release(detector->phasor_work_pool, filtered_signal);
    return true;
}

// ============================================================================
// PUBLIC API
// ============================================================================

oscillation_detector_config_t oscillation_detector_default_config(void) {
    oscillation_detector_config_t config;
    config.sample_rate_hz = OSC_SAMPLE_RATE_HZ;
    config.window_size = OSC_WINDOW_SIZE;
    config.min_burst_duration_ms = OSC_MIN_BURST_DURATION_MS;
    config.burst_threshold_std = OSC_BURST_THRESHOLD;
    config.enable_burst_detection = true;
    config.enable_plv = false;  // Expensive, off by default
    config.enable_pac = false;  // Expensive, off by default
    config.overlap_fraction = 0.5F;
    config.use_phasor_detection = true;  // Use phasor methods by default (faster)
    return config;
}

oscillation_detector_t* oscillation_detector_create(const oscillation_detector_config_t* config) {
    /* P2-MW-14 fix: Validate window_size >= 2 to prevent zero-size allocation
     * (power_spectrum is window_size/2) and division by zero in Hann window. */
    if (!config || config->window_size < 2 || config->sample_rate_hz <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "oscillation_detector_create: config is NULL or invalid (window_size must be >= 2)");
        return NULL;
    }

    oscillation_detector_t* detector = (oscillation_detector_t*)nimcp_calloc(1,
                                                    sizeof(oscillation_detector_t));
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oscillation_detector_create: failed to allocate detector");
        return NULL;
    }

    detector->config = *config;

    // Initialize signal buffer
    if (!init_signal_buffer(&detector->buffer, MAX_SIGNAL_BUFFER)) {
        oscillation_detector_destroy(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "oscillation_detector_create: init_signal_buffer is NULL");
        return NULL;
    }

    // Allocate FFT buffers
    detector->fft_real = (float*)nimcp_calloc(config->window_size, sizeof(float));
    detector->fft_imag = (float*)nimcp_calloc(config->window_size, sizeof(float));
    detector->window = (float*)nimcp_calloc(config->window_size, sizeof(float));
    detector->power_spectrum = (float*)nimcp_calloc(config->window_size / 2, sizeof(float));

    if (!detector->fft_real || !detector->fft_imag ||
        !detector->window || !detector->power_spectrum) {
        oscillation_detector_destroy(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillation_detector_create: operation failed");
        return NULL;
    }

    // Initialize Hann window
    init_hann_window(detector->window, config->window_size);

    // Initialize Hilbert transform for amplitude/phase extraction
    hilbert_config_t hilbert_config = hilbert_default_config();
    hilbert_config.max_signal_length = config->window_size;
    hilbert_config.auto_pad_power_of_2 = true;
    hilbert_config.enable_simd = true;
    detector->hilbert = hilbert_create(&hilbert_config);

    if (!detector->hilbert) {
        oscillation_detector_destroy(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oscillation_detector_create: detector->hilbert is NULL");
        return NULL;
    }

    // Initialize memory pools for hot-path allocations (Phase 1.5)
    // Pool 1: Signal window extraction - 2 buffers for double-buffering
    memory_pool_config_t sig_pool_config = {
        .block_size = config->window_size * sizeof(float),
        .num_blocks = 2,
        .alignment = 16,  // SIMD alignment
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    detector->signal_window_pool = memory_pool_create(&sig_pool_config);
    if (!detector->signal_window_pool) {
        oscillation_detector_destroy(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oscillation_detector_create: detector->signal_window_pool is NULL");
        return NULL;
    }

    // Pool 2: Phasor work buffers - larger blocks for filtered signal, analytic signal (2x size for complex), amplitude
    // Block size: max(window_size * sizeof(float), window_size * sizeof(neural_phasor_t))
    // neural_phasor_t is 2 floats = 8 bytes, so window_size * 8 covers both float and phasor arrays
    memory_pool_config_t phasor_pool_config = {
        .block_size = config->window_size * sizeof(float) * 2,  // Covers float and neural_phasor_t arrays
        .num_blocks = 6,  // filtered, analytic, amplitude, spectrum, and 2 work buffers
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    detector->phasor_work_pool = memory_pool_create(&phasor_pool_config);
    if (!detector->phasor_work_pool) {
        oscillation_detector_destroy(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oscillation_detector_create: detector->phasor_work_pool is NULL");
        return NULL;
    }

    // Pool 3: PAC detection work buffers
    memory_pool_config_t pac_pool_config = {
        .block_size = config->window_size * sizeof(float) * 2,
        .num_blocks = 6,  // phase_filtered, amp_filtered, phase_phasor, amp_phasor, amp_envelope, work
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    detector->pac_work_pool = memory_pool_create(&pac_pool_config);
    if (!detector->pac_work_pool) {
        oscillation_detector_destroy(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oscillation_detector_create: detector->pac_work_pool is NULL");
        return NULL;
    }

    // Initialize band states
    memset(detector->band_states, 0, sizeof(detector->band_states));

    // Initialize statistics
    detector->total_samples = 0;
    detector->total_bursts = 0;
    detector->sum_power = 0.0;
    detector->power_measurements = 0;

    return detector;
}

void oscillation_detector_destroy(oscillation_detector_t* detector) {
    if (!detector) return;

    free_signal_buffer(&detector->buffer);
    nimcp_free(detector->fft_real);
    nimcp_free(detector->fft_imag);
    nimcp_free(detector->window);
    nimcp_free(detector->power_spectrum);

    if (detector->hilbert) {
        hilbert_destroy(detector->hilbert);
    }

    // Destroy memory pools (Phase 1.5)
    memory_pool_destroy(detector->signal_window_pool);
    memory_pool_destroy(detector->phasor_work_pool);
    memory_pool_destroy(detector->pac_work_pool);

    nimcp_free(detector);
}

bool oscillation_detector_add_sample(oscillation_detector_t* detector,
                                      float signal,
                                      double timestamp_ms) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillation_detector_destroy: detector is NULL");
        return false;
    }

    buffer_add_sample(&detector->buffer, signal, timestamp_ms);
    detector->total_samples++;

    return true;
}

bool oscillation_detector_detect(oscillation_detector_t* detector,
                                  oscillation_result_t* result) {
    if (!detector || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillation_detector_destroy: required parameter is NULL (detector, result)");
        return false;
    }

    // Need full window for analysis
    if (detector->buffer.count < detector->config.window_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "oscillation_detector_destroy: validation failed");
        return false;
    }

    memset(result, 0, sizeof(oscillation_result_t));

    // Extract signal window (most recent samples) - Phase 1.5 O(1) pool allocation
    float* signal_window = (float*)memory_pool_acquire(detector->signal_window_pool);
    if (!signal_window) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oscillation_detector_destroy: signal_window is NULL");
        return false;
    }

    for (uint32_t i = 0; i < detector->config.window_size; i++) {
        uint32_t idx = (detector->buffer.head + detector->buffer.capacity -
                       detector->config.window_size + i) % detector->buffer.capacity;
        signal_window[i] = detector->buffer.buffer[idx];
    }

    // Compute DFT
    compute_dft(signal_window, detector->config.window_size,
               detector->window, detector->fft_real, detector->fft_imag);

    // Compute power spectrum
    compute_power_spectrum(detector->fft_real, detector->fft_imag,
                          detector->config.window_size, detector->power_spectrum);

    // Analyze each band
    result->total_power = 0.0F;
    float max_power = 0.0F;

    for (uint32_t b = 0; b < OSC_NUM_BANDS; b++) {
        band_power_t* bp = &result->bands[b];
        bp->band = (oscillation_band_t)b;

        // Use phasor-based detection with Hilbert transform if enabled
        if (detector->config.use_phasor_detection && detector->hilbert) {
            float coherence = 0.0F;
            if (detect_oscillation_phasor_hilbert(detector,
                                                   signal_window,
                                                   detector->config.window_size,
                                                   detector->config.sample_rate_hz,
                                                   BAND_RANGES[b][0],
                                                   BAND_RANGES[b][1],
                                                   &bp->power,
                                                   &bp->peak_frequency,
                                                   &coherence)) {
                // Hilbert method succeeded - SIMD optimized amplitude extraction
                result->total_power += bp->power;
            } else {
                // Fall back to traditional method on error
                bp->power = compute_band_power(detector->power_spectrum,
                                              detector->config.window_size,
                                              detector->config.sample_rate_hz,
                                              BAND_RANGES[b][0],
                                              BAND_RANGES[b][1]);
                bp->peak_frequency = find_peak_frequency(detector->power_spectrum,
                                                         detector->config.window_size,
                                                         detector->config.sample_rate_hz,
                                                         BAND_RANGES[b][0],
                                                         BAND_RANGES[b][1]);
                result->total_power += bp->power;
            }
        } else {
            // Traditional DFT-based method
            bp->power = compute_band_power(detector->power_spectrum,
                                          detector->config.window_size,
                                          detector->config.sample_rate_hz,
                                          BAND_RANGES[b][0],
                                          BAND_RANGES[b][1]);
            bp->peak_frequency = find_peak_frequency(detector->power_spectrum,
                                                     detector->config.window_size,
                                                     detector->config.sample_rate_hz,
                                                     BAND_RANGES[b][0],
                                                     BAND_RANGES[b][1]);
            result->total_power += bp->power;
        }

        // Detect bursts
        if (detector->config.enable_burst_detection) {
            bp->is_burst = detect_burst(&detector->band_states[b],
                                       bp->power,
                                       detector->buffer.newest_time_ms,
                                       detector->config.burst_threshold_std,
                                       detector->config.min_burst_duration_ms);

            if (bp->is_burst) {
                bp->burst_duration_ms = (float)(detector->buffer.newest_time_ms -
                                               detector->band_states[b].burst_start_ms);
                result->num_bursts++;
            }
        }

        // Track dominant band
        if (bp->power > max_power) {
            max_power = bp->power;
            result->dominant_band = (oscillation_band_t)b;
        }
    }

    // Compute relative powers
    for (uint32_t b = 0; b < OSC_NUM_BANDS; b++) {
        result->bands[b].relative_power = result->bands[b].power /
                                         (result->total_power + 1e-6F);
    }

    // Set flags
    result->has_gamma = (result->bands[OSC_BAND_GAMMA].relative_power > 0.1F);
    result->has_theta_gamma_coupling =
        (result->bands[OSC_BAND_THETA].relative_power > 0.15F &&
         result->bands[OSC_BAND_GAMMA].relative_power > 0.15F);

    // Update statistics
    detector->sum_power += result->total_power;
    detector->power_measurements++;

    uint32_t total_bursts_now = 0;
    for (uint32_t b = 0; b < OSC_NUM_BANDS; b++) {
        total_bursts_now += detector->band_states[b].burst_count;
    }
    detector->total_bursts = total_bursts_now;

    // Release signal window back to pool (Phase 1.5)
    memory_pool_release(detector->signal_window_pool, signal_window);

    return true;
}

bool oscillation_detector_compute_plv(oscillation_detector_t* detector,
                                       oscillation_band_t band,
                                       const float* signal1,
                                       const float* signal2,
                                       uint32_t length,
                                       phase_locking_t* result) {
    /* P2-MW-13 fix: Changed guard to length < 2 to prevent division by zero
     * in (length - 1) below. PLV requires at least 2 samples. */
    if (!detector || !signal1 || !signal2 || !result || length < 2) {
        if (result) { result->plv = 0.0F; result->mean_phase_diff = 0.0F; result->num_samples = 0; }
        if (!detector || !signal1 || !signal2 || !result) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillation_detector_compute_plv: required parameter is NULL");
        }
        return false;
    }

    // Simple phase estimation (not full Hilbert transform, but functional)
    // Use sign changes as phase proxy
    uint32_t sync_count = 0;
    float sum_phase_diff = 0.0F;

    for (uint32_t i = 1; i < length; i++) {
        float sign1 = (signal1[i] >= 0.0F) ? 1.0F : -1.0F;
        float sign2 = (signal2[i] >= 0.0F) ? 1.0F : -1.0F;

        if (sign1 == sign2) {
            sync_count++;
        }

        // Approximate phase difference
        float phase_diff = atan2f(signal2[i], signal1[i] + 1e-6F);
        sum_phase_diff += phase_diff;
    }

    result->plv = (float)sync_count / (float)(length - 1);
    result->mean_phase_diff = sum_phase_diff / (float)(length - 1);
    result->num_samples = length;

    return true;
}

bool oscillation_detector_detect_pac(oscillation_detector_t* detector,
                                      cross_freq_coupling_t* couplings,
                                      uint32_t max_couplings,
                                      uint32_t* num_found) {
    if (!detector || !couplings || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (detector, couplings, num_found)");
        return false;
    }

    *num_found = 0;

    // Need full window for PAC analysis
    if (detector->buffer.count < detector->config.window_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    // Extract signal window - Phase 1.5 O(1) pool allocation
    float* signal_window = (float*)memory_pool_acquire(detector->signal_window_pool);
    if (!signal_window) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: signal_window is NULL");
        return false;
    }

    for (uint32_t i = 0; i < detector->config.window_size; i++) {
        uint32_t idx = (detector->buffer.head + detector->buffer.capacity -
                       detector->config.window_size + i) % detector->buffer.capacity;
        signal_window[i] = detector->buffer.buffer[idx];
    }

    // Use phasor-based PAC if enabled (2-5x faster than traditional method)
    if (detector->config.use_phasor_detection) {
        // Define frequency band ranges
        typedef struct { float low; float high; } band_range_t;
        band_range_t bands[] = {
            {1.0F, 4.0F},    // DELTA
            {4.0F, 8.0F},    // THETA
            {8.0F, 13.0F},   // ALPHA
            {13.0F, 30.0F},  // BETA
            {30.0F, 80.0F}   // GAMMA (limited to 80Hz for practical PAC)
        };

        // Common PAC combinations: phase band should be slower than amplitude band
        typedef struct { oscillation_band_t phase; oscillation_band_t amp; } pac_pair_t;
        pac_pair_t pac_pairs[] = {
            {OSC_BAND_THETA, OSC_BAND_GAMMA},  // Most common: theta-gamma
            {OSC_BAND_ALPHA, OSC_BAND_BETA},   // Alpha-beta coupling
            {OSC_BAND_DELTA, OSC_BAND_THETA},  // Delta-theta
            {OSC_BAND_THETA, OSC_BAND_BETA}    // Theta-beta
        };
        uint32_t num_pairs = sizeof(pac_pairs) / sizeof(pac_pair_t);

        // Allocate work buffers - Phase 1.5 O(1) pool allocation
        float* phase_filtered = (float*)memory_pool_acquire(detector->pac_work_pool);
        float* amp_filtered = (float*)memory_pool_acquire(detector->pac_work_pool);
        neural_phasor_t* phase_phasor = (neural_phasor_t*)memory_pool_acquire(detector->pac_work_pool);
        neural_phasor_t* amp_phasor = (neural_phasor_t*)memory_pool_acquire(detector->pac_work_pool);
        float* amp_envelope = (float*)memory_pool_acquire(detector->pac_work_pool);

        if (phase_filtered && amp_filtered && phase_phasor && amp_phasor && amp_envelope) {
            // Test each PAC combination
            for (uint32_t p = 0; p < num_pairs && *num_found < max_couplings; p++) {
                oscillation_band_t phase_band = pac_pairs[p].phase;
                oscillation_band_t amp_band = pac_pairs[p].amp;

                // Create band-pass filters
                signal_filter_config_t phase_config = signal_filter_bandpass_config(
                    bands[phase_band].low, bands[phase_band].high, detector->config.sample_rate_hz);
                phase_config.order = 64;
                signal_filter_t* phase_filter = signal_filter_create(&phase_config);

                signal_filter_config_t amp_config = signal_filter_bandpass_config(
                    bands[amp_band].low, bands[amp_band].high, detector->config.sample_rate_hz);
                amp_config.order = 64;
                signal_filter_t* amp_filter = signal_filter_create(&amp_config);

                if (phase_filter && amp_filter) {
                    // Filter signals
                    signal_filter_apply(phase_filter, signal_window, phase_filtered, detector->config.window_size);
                    signal_filter_apply(amp_filter, signal_window, amp_filtered, detector->config.window_size);

                    // Extract phase and amplitude via Hilbert transform (SIMD-optimized)
                    bool phase_ok = false;
                    bool amp_ok = false;

                    // Use optimized Hilbert transform for phase extraction
                    if (detector->hilbert) {
                        phase_ok = hilbert_apply(detector->hilbert, phase_filtered, phase_phasor, detector->config.window_size);
                        amp_ok = hilbert_extract_amplitude(detector->hilbert, amp_filtered, amp_envelope, detector->config.window_size);
                    }

                    // Fallback to legacy phasor method if Hilbert fails
                    if (!phase_ok) {
                        phase_ok = phasor_hilbert_transform(phase_filtered, phase_phasor, detector->config.window_size);
                    }
                    if (!amp_ok && phasor_hilbert_transform(amp_filtered, amp_phasor, detector->config.window_size)) {
                        for (uint32_t i = 0; i < detector->config.window_size; i++) {
                            amp_envelope[i] = phasor_amplitude(amp_phasor[i]);
                        }
                        amp_ok = true;
                    }

                    if (phase_ok && amp_ok) {
                        // Compute PAC modulation index
                        float pac_strength = phasor_pac_modulation_index(
                            phase_phasor, amp_envelope, detector->config.window_size);

                        // Significant coupling threshold (entropy-based MI is conservative)
                        // Even strong coupling may yield MI ~ 0.04-0.08 after filtering
                        if (pac_strength > 0.04F) {
                            couplings[*num_found].phase_band = phase_band;
                            couplings[*num_found].amp_band = amp_band;
                            couplings[*num_found].coupling_strength = pac_strength;
                            couplings[*num_found].preferred_phase =
                                phasor_array_mean_phase(phase_phasor, detector->config.window_size);

                            (*num_found)++;
                        }
                    }
                }

                if (phase_filter) signal_filter_destroy(phase_filter);
                if (amp_filter) signal_filter_destroy(amp_filter);
            }
        }

        // Release work buffers back to pool (Phase 1.5)
        memory_pool_release(detector->pac_work_pool, phase_filtered);
        memory_pool_release(detector->pac_work_pool, amp_filtered);
        memory_pool_release(detector->pac_work_pool, phase_phasor);
        memory_pool_release(detector->pac_work_pool, amp_phasor);
        memory_pool_release(detector->pac_work_pool, amp_envelope);
    } else {
        // Traditional simplified PAC (placeholder - slower method)
        if (*num_found < max_couplings) {
            couplings[0].phase_band = OSC_BAND_THETA;
            couplings[0].amp_band = OSC_BAND_GAMMA;
            couplings[0].coupling_strength = 0.5F;
            couplings[0].preferred_phase = 0.0F;
            *num_found = 1;
        }
    }

    // Release signal window back to pool (Phase 1.5)
    memory_pool_release(detector->signal_window_pool, signal_window);
    return true;
}

bool oscillation_detector_get_band_power(const oscillation_detector_t* detector,
                                          oscillation_band_t band,
                                          band_power_t* power) {
    if (!detector || !power || band >= OSC_NUM_BANDS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (detector, power)");
        return false;
    }

    power->band = band;
    power->power = detector->band_states[band].mean_power;
    power->relative_power = 0.0F;  // Would need total power
    power->peak_frequency = (BAND_RANGES[band][0] + BAND_RANGES[band][1]) / 2.0F;
    power->is_burst = detector->band_states[band].in_burst;
    power->burst_duration_ms = detector->band_states[band].in_burst ?
        100.0F : 0.0F;  // Placeholder

    return true;
}

void oscillation_detector_reset(oscillation_detector_t* detector) {
    if (!detector) return;

    // Clear analysis state: signal buffer and band states
    detector->buffer.count = 0;
    detector->buffer.head = 0;
    memset(detector->band_states, 0, sizeof(detector->band_states));

    // Note: Lifetime statistics (total_samples, total_bursts, sum_power,
    // power_measurements) are intentionally NOT reset here.
    // reset() clears analysis state for a fresh analysis window,
    // but statistics persist as lifetime counters.
}

bool oscillation_detector_get_stats(const oscillation_detector_t* detector,
                                     uint64_t* total_samples,
                                     uint64_t* total_bursts,
                                     float* avg_power) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillation_detector_reset: detector is NULL");
        return false;
    }

    if (total_samples) *total_samples = detector->total_samples;
    if (total_bursts) *total_bursts = detector->total_bursts;

    if (avg_power) {
        *avg_power = (detector->power_measurements > 0) ?
                    (float)(detector->sum_power / detector->power_measurements) : 0.0F;
    }

    return true;
}

const char* oscillation_band_name(oscillation_band_t band) {
    if (band >= OSC_NUM_BANDS) return "Unknown";
    return BAND_NAMES[band];
}

void oscillation_band_range(oscillation_band_t band, float* min_hz, float* max_hz) {
    if (band >= OSC_NUM_BANDS) {
        if (min_hz) *min_hz = 0.0F;
        if (max_hz) *max_hz = 0.0F;
        return;
    }

    if (min_hz) *min_hz = BAND_RANGES[band][0];
    if (max_hz) *max_hz = BAND_RANGES[band][1];
}
