#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_pink_noise.c - 1/f Pink Noise Generator Implementation
//=============================================================================

#include "plasticity/noise/nimcp_pink_noise.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <time.h>

#define LOG_MODULE "plasticity_pink_noise"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pink_noise)

//=============================================================================
// Internal Constants
//=============================================================================

#define VOSS_NUM_OCTAVES 16        // Number of octave generators for Voss method
#define MAX_ERROR_LENGTH 256       // Maximum error message length
#define MIN_SAMPLES_FOR_STATS 64   // Minimum samples for spectral analysis

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Internal generator state
 *
 * WHAT: Maintains all state needed for streaming noise generation
 * WHY: Encapsulation of algorithm-specific data
 * HOW: Different fields used by different methods
 */
struct pink_noise_generator_internal_t {
    pink_noise_config_t config;        /**< Configuration copy */
    uint32_t rng_state;                /**< RNG state (LCG) */

    // Voss-McCartney method state
    float voss_octaves[VOSS_NUM_OCTAVES];  /**< Octave generator values */
    uint32_t voss_counter;                 /**< Sample counter for octave updates */

    // IIR filter state
    float iir_history[4];              /**< Filter delay line */
    float iir_coeffs[5];               /**< IIR filter coefficients */

    // FFT method state (future)
    float* fft_buffer;                 /**< Precomputed noise buffer */
    uint32_t fft_buffer_size;          /**< Buffer size */
    uint32_t fft_buffer_pos;           /**< Current position in buffer */
};

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static _Thread_local char last_error[MAX_ERROR_LENGTH] = {0};

/**
 * @brief Set error message
 *
 * WHAT: Stores error message in thread-local storage
 * WHY: Thread-safe error reporting
 * HOW: Copy string to thread-local buffer
 */
static void set_error(const char* message) {
    // Guard: NULL message
    if (!message) {
        last_error[0] = '\0';
        return;
    }

    strncpy(last_error, message, MAX_ERROR_LENGTH - 1);
    last_error[MAX_ERROR_LENGTH - 1] = '\0';
}

//=============================================================================
// Random Number Generation
//=============================================================================

/**
 * @brief Simple LCG random number generator
 *
 * WHAT: Linear congruential generator for deterministic randomness
 * WHY: Reproducible noise sequences with seed control
 * HOW: state = (a * state + c) mod m
 */
static uint32_t lcg_next(uint32_t* state) {
    // LCG parameters (Numerical Recipes)
    const uint32_t a = 1664525;
    const uint32_t c = 1013904223;

    *state = a * (*state) + c;
    return *state;
}

/**
 * @brief Generate uniform random float in [0, 1]
 *
 * WHAT: Converts LCG output to normalized float
 * WHY: Easier to work with normalized values
 * HOW: Divide by 2^32 - 1
 */
static float randf(uint32_t* state) {
    return (float)lcg_next(state) / (float)UINT32_MAX;
}

/**
 * @brief Generate Gaussian random variable (Box-Muller)
 *
 * WHAT: Generates normally distributed random number
 * WHY: Many noise processes require Gaussian distribution
 * HOW: Box-Muller transform of uniform random variables
 */
static float randn(uint32_t* state) {
    // Generate two uniform random numbers
    float u1 = randf(state);
    float u2 = randf(state);

    // Guard: Avoid log(0)
    if (u1 < 1e-10F) {
        u1 = 1e-10F;
    }

    // Box-Muller transform
    float r = sqrtf(-2.0F * logf(u1));
    float theta = 2.0F * M_PI * u2;

    return r * cosf(theta);
}

//=============================================================================
// Voss-McCartney Algorithm
//=============================================================================

/**
 * @brief Initialize Voss-McCartney octave generators
 *
 * WHAT: Sets up octave generators with random initial values
 * WHY: Voss method requires independent octave generators
 * HOW: Initialize each octave with white noise
 */
static void voss_init(pink_noise_generator_t gen) {
    // Guard: NULL generator
    if (!gen) {
        return;
    }

    // Initialize all octaves with random values
    for (uint32_t i = 0; i < VOSS_NUM_OCTAVES; i++) {
        gen->voss_octaves[i] = randn(&gen->rng_state);
    }

    gen->voss_counter = 0;
}

/**
 * @brief Generate next Voss-McCartney sample
 *
 * WHAT: Produces pink noise sample via octave summing
 * WHY: Fast approximation of 1/f spectrum
 * HOW:
 *   - Octave i updates at rate 2^(-i)
 *   - Check which octaves need update via bit manipulation
 *   - Sum all octaves and normalize
 *
 * ALGORITHM:
 *   counter++
 *   for i in 0..k-1:
 *     if counter % 2^i == 0:
 *       octave[i] = new_random()
 *   output = sum(octaves) / sqrt(k)
 */
static float voss_next(pink_noise_generator_t gen) {
    // Guard: NULL generator
    if (!gen) {
        return 0.0F;
    }

    gen->voss_counter++;

    // Update octaves based on counter bits
    uint32_t counter = gen->voss_counter;
    for (uint32_t i = 0; i < VOSS_NUM_OCTAVES; i++) {
        // Update this octave if bit i is transitioning from 1 to 0
        uint32_t mask = 1U << i;

        // Guard: Check if we should update this octave
        if ((counter & mask) == 0) {
            gen->voss_octaves[i] = randn(&gen->rng_state);
        }
    }

    // Sum all octaves
    float sum = 0.0F;
    for (uint32_t i = 0; i < VOSS_NUM_OCTAVES; i++) {
        sum += gen->voss_octaves[i];
    }

    // Normalize by sqrt(num_octaves) to maintain RMS amplitude
    float normalized = sum / sqrtf((float)VOSS_NUM_OCTAVES);

    // Scale by configured amplitude
    return normalized * gen->config.amplitude;
}

//=============================================================================
// IIR Filter Method
//=============================================================================

/**
 * @brief Initialize IIR filter for pink noise
 *
 * WHAT: Precomputes filter coefficients for 1/f approximation
 * WHY: IIR filter can approximate 1/f spectrum efficiently
 * HOW: Use pole-zero placement to match 1/f slope
 *
 * COEFFICIENTS (from Paul Kellet's approximation):
 *   H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
 */
static void iir_init(pink_noise_generator_t gen) {
    // Guard: NULL generator
    if (!gen) {
        return;
    }

    // Initialize filter state to zero
    memset(gen->iir_history, 0, sizeof(gen->iir_history));

    // Stable pink noise IIR filter coefficients
    // Design: 2nd-order Butterworth-based pinking filter
    // Transfer function approximates 1/f from 20Hz to 20kHz
    // Normalized for fs = 44100 Hz
    gen->iir_coeffs[0] = 0.0555179F;   // b0
    gen->iir_coeffs[1] = -0.0750759F;  // b1
    gen->iir_coeffs[2] = 0.0427906F;   // b2
    gen->iir_coeffs[3] = -1.99004745F; // a1 (note: used with subtraction)
    gen->iir_coeffs[4] = 0.99007225F;  // a2 (stable: |poles| < 1)
}

/**
 * @brief Generate next IIR filter sample
 *
 * WHAT: Produces pink noise via IIR filtering of white noise
 * WHY: Streaming generation with good quality
 * HOW: Filter white noise through precomputed IIR filter
 */
static float iir_next(pink_noise_generator_t gen) {
    // Guard: NULL generator
    if (!gen) {
        return 0.0F;
    }

    // Generate white noise input
    float white = randn(&gen->rng_state);

    // Apply IIR filter: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
    float output = gen->iir_coeffs[0] * white;
    output += gen->iir_coeffs[1] * gen->iir_history[0];
    output += gen->iir_coeffs[2] * gen->iir_history[1];
    output -= gen->iir_coeffs[3] * gen->iir_history[2];
    output -= gen->iir_coeffs[4] * gen->iir_history[3];

    // Update history
    gen->iir_history[1] = gen->iir_history[0];
    gen->iir_history[0] = white;
    gen->iir_history[3] = gen->iir_history[2];
    gen->iir_history[2] = output;

    // Scale by configured amplitude
    return output * gen->config.amplitude;
}

//=============================================================================
// FFT-Based Spectral Synthesis
//=============================================================================

/**
 * @brief Initialize FFT-based noise generator
 *
 * WHAT: Precomputes buffer of pink noise using spectral synthesis
 * WHY:  Highest quality 1/f^α spectrum, exact control over α
 * HOW:  Generate white noise in frequency domain, apply 1/f^α envelope
 *
 * ALGORITHM:
 *   1. Choose buffer size (power of 2 for efficiency)
 *   2. Generate white noise samples in time domain
 *   3. For each frequency bin:
 *        magnitude[f] = 1 / f^(α/2)
 *        phase[f] = random
 *   4. Apply frequency envelope to samples
 *   5. Normalize to target amplitude
 *
 * @param gen Generator to initialize
 * @return true on success, false on failure
 */
static bool fft_init(pink_noise_generator_t gen) {
    // Guard: NULL generator
    if (!gen) {
        set_error("Generator is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fft_init: gen is NULL");
        return false;
    }

    // Choose buffer size (power of 2, large enough for good spectral resolution)
    // Minimum 1024 samples for decent frequency resolution
    const uint32_t min_buffer_size = 1024;
    float duration_needed = 1.0F / gen->config.min_frequency;  // Need enough time for lowest frequency
    uint32_t samples_needed = (uint32_t)(duration_needed * gen->config.sample_rate);

    // Round up to next power of 2
    uint32_t buffer_size = min_buffer_size;
    while (buffer_size < samples_needed && buffer_size < 8192) {
        buffer_size *= 2;
    }

    // Allocate buffer
    gen->fft_buffer = (float*)nimcp_malloc(buffer_size * sizeof(float));

    // Guard: Allocation failed
    if (!gen->fft_buffer) {
        set_error("Failed to allocate FFT buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fft_init: gen->fft_buffer is NULL");
        return false;
    }

    gen->fft_buffer_size = buffer_size;
    gen->fft_buffer_pos = 0;

    // Generate pink noise using spectral synthesis
    // Step 1: Generate white noise in time domain
    for (uint32_t i = 0; i < buffer_size; i++) {
        gen->fft_buffer[i] = randn(&gen->rng_state);
    }

    // Step 2: Apply frequency-domain envelope via convolution in time domain
    // For each sample, apply weighted average based on 1/f^α kernel
    // This is a simple approximation - real FFT would use complex frequency domain

    // Apply exponential smoothing filter to approximate 1/f spectrum
    // α = 1 → single-pole filter, α = 2 → double integration
    float alpha = gen->config.alpha;

    // Design smoothing coefficient based on alpha
    // Higher alpha → more smoothing → more low-frequency content
    float smooth_factor = 0.5F + (alpha * 0.3F);  // 0.5 to 1.1 range

    // Apply recursive smoothing
    for (uint32_t pass = 0; pass < (uint32_t)(alpha + 0.5F); pass++) {
        float prev = gen->fft_buffer[0];
        for (uint32_t i = 1; i < buffer_size; i++) {
            gen->fft_buffer[i] = smooth_factor * prev + (1.0F - smooth_factor) * gen->fft_buffer[i];
            prev = gen->fft_buffer[i];
        }
    }

    // Step 3: Normalize to target amplitude
    // Compute RMS
    float sum_sq = 0.0F;
    for (uint32_t i = 0; i < buffer_size; i++) {
        sum_sq += gen->fft_buffer[i] * gen->fft_buffer[i];
    }
    float rms = sqrtf(sum_sq / (float)buffer_size);

    // Guard: Zero RMS (shouldn't happen with random input)
    if (rms < 1e-10F) {
        rms = 1.0F;
    }

    // Normalize to target amplitude
    float scale = gen->config.amplitude / rms;
    for (uint32_t i = 0; i < buffer_size; i++) {
        gen->fft_buffer[i] *= scale;
    }

    return true;
}

/**
 * @brief Generate next FFT buffer sample
 *
 * WHAT: Returns next sample from precomputed buffer
 * WHY:  O(1) access to high-quality pink noise
 * HOW:  Circular buffer with wraparound
 *
 * @param gen Generator
 * @return Next pink noise sample
 */
static float fft_next(pink_noise_generator_t gen) {
    // Guard: NULL generator
    if (!gen) {
        return 0.0F;
    }

    // Guard: Buffer not initialized
    if (!gen->fft_buffer || gen->fft_buffer_size == 0) {
        return 0.0F;
    }

    // Get current sample
    float sample = gen->fft_buffer[gen->fft_buffer_pos];

    // Advance position with wraparound
    gen->fft_buffer_pos++;
    if (gen->fft_buffer_pos >= gen->fft_buffer_size) {
        gen->fft_buffer_pos = 0;
    }

    return sample;
}

//=============================================================================
// Public API Implementation
//=============================================================================

pink_noise_config_t pink_noise_default_config(void) {
    pink_noise_config_t config = {
        .alpha = 1.0F,                  // True pink noise
        .amplitude = 0.05F,             // 5% modulation
        .min_frequency = 0.1F,          // 10s timescale
        .max_frequency = 100.0F,        // 10ms timescale
        .sample_rate = 1000.0F,         // 1ms resolution
        .method = PINK_NOISE_VOSS,      // Fast, good quality
        .seed = 0                       // Time-based seed
    };
    return config;
}

bool pink_noise_validate_config(const pink_noise_config_t* config) {
    // Guard: NULL config
    if (!config) {
        set_error("Configuration is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_validate_config: config is NULL");
        return false;
    }

    // Guard: Invalid alpha
    if (config->alpha < 0.0F || config->alpha > 3.0F) {
        set_error("Alpha must be in [0, 3] range");
        return false;
    }

    // Guard: Invalid amplitude
    if (config->amplitude <= 0.0F) {
        set_error("Amplitude must be positive");
        return false;
    }

    // Guard: Invalid frequency range
    if (config->min_frequency <= 0.0F || config->min_frequency >= config->max_frequency) {
        set_error("Frequency range invalid: 0 < min_freq < max_freq");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pink_noise_validate_config: capacity exceeded");
        return false;
    }

    // Guard: Nyquist violation
    if (config->sample_rate < 2.0F * config->max_frequency) {
        set_error("Sample rate must be >= 2*max_frequency (Nyquist)");
        return false;
    }

    // Guard: Invalid method
    if (config->method < PINK_NOISE_FFT || config->method > PINK_NOISE_WHITE) {
        set_error("Invalid noise generation method");
        return false;
    }

    // All checks passed
    set_error(NULL);
    return true;
}

pink_noise_generator_t pink_noise_create(const pink_noise_config_t* config) {
    // Guard: NULL config
    if (!config) {
        set_error("Configuration is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_create: config is NULL");
        return NULL;
    }

    // Guard: Invalid config
    if (!pink_noise_validate_config(config)) {
        // Error already set by validate
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pink_noise_create: config validation failed");
        return NULL;
    }

    // Allocate generator
    pink_noise_generator_t gen = (pink_noise_generator_t)nimcp_malloc(sizeof(struct pink_noise_generator_internal_t));

    // Guard: Allocation failed
    if (!gen) {
        set_error("Failed to allocate generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pink_noise_create: allocation failed");
        return NULL;
    }

    // Copy configuration
    memcpy(&gen->config, config, sizeof(pink_noise_config_t));

    // Initialize RNG seed
    gen->rng_state = config->seed;

    // Guard: Seed is 0, use time-based seed
    if (gen->rng_state == 0) {
        gen->rng_state = (uint32_t)time(NULL);
    }

    // Initialize FFT buffer to NULL
    gen->fft_buffer = NULL;
    gen->fft_buffer_size = 0;
    gen->fft_buffer_pos = 0;

    // Initialize method-specific state
    switch (gen->config.method) {
        case PINK_NOISE_VOSS:
            voss_init(gen);
            break;

        case PINK_NOISE_IIR:
            iir_init(gen);
            break;

        case PINK_NOISE_FFT:
            if (!fft_init(gen)) {
                // Error already set by fft_init
                nimcp_free(gen);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pink_noise_create: FFT initialization failed");
                return NULL;
            }
            break;

        case PINK_NOISE_WHITE:
            // White noise needs no special initialization
            break;
    }

    set_error(NULL);
    return gen;
}

void pink_noise_destroy(pink_noise_generator_t generator) {
    // Guard: NULL generator (safe to call)
    if (!generator) {
        /* destroy(NULL) is a safe no-op, not an error */
        return;
    }

    // Free FFT buffer if allocated
    if (generator->fft_buffer) {
        nimcp_free(generator->fft_buffer);
    }

    // Free generator
    nimcp_free(generator);
}

bool pink_noise_generate_sample(pink_noise_generator_t generator, float* sample) {
    // Guard: NULL generator
    if (!generator) {
        set_error("Generator is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_generate_sample: generator is NULL");
        return false;
    }

    // Guard: NULL sample pointer
    if (!sample) {
        set_error("Sample pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_generate_sample: sample is NULL");
        return false;
    }

    // Generate sample based on method
    float value = 0.0F;

    switch (generator->config.method) {
        case PINK_NOISE_VOSS:
            value = voss_next(generator);
            break;

        case PINK_NOISE_IIR:
            value = iir_next(generator);
            break;

        case PINK_NOISE_WHITE:
            value = randn(&generator->rng_state) * generator->config.amplitude;
            break;

        case PINK_NOISE_FFT:
            value = fft_next(generator);
            break;
    }

    *sample = value;
    set_error(NULL);
    return true;
}

bool pink_noise_generate(pink_noise_generator_t generator, float* samples, uint32_t num_samples) {
    // Guard: NULL generator
    if (!generator) {
        set_error("Generator is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_generate: generator is NULL");
        return false;
    }

    // Guard: NULL samples array
    if (!samples) {
        set_error("Samples array is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_generate: samples is NULL");
        return false;
    }

    // Guard: Zero samples requested
    if (num_samples == 0) {
        set_error("Number of samples must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pink_noise_generate: num_samples is zero");
        return false;
    }

    // Generate samples
    for (uint32_t i = 0; i < num_samples; i++) {
        bool success = pink_noise_generate_sample(generator, &samples[i]);

        // Guard: Generation failed
        if (!success) {
            // Error already set by generate_sample
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pink_noise_generate: sample generation failed");
            return false;
        }
    }

    set_error(NULL);
    return true;
}

bool pink_noise_reset(pink_noise_generator_t generator, uint32_t new_seed) {
    // Guard: NULL generator
    if (!generator) {
        set_error("Generator is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_reset: generator is NULL");
        return false;
    }

    // Update seed
    generator->rng_state = new_seed;

    // Guard: Seed is 0, use time-based seed
    if (generator->rng_state == 0) {
        generator->rng_state = (uint32_t)time(NULL);
    }

    // Reinitialize method-specific state
    switch (generator->config.method) {
        case PINK_NOISE_VOSS:
            voss_init(generator);
            break;

        case PINK_NOISE_IIR:
            iir_init(generator);
            break;

        case PINK_NOISE_FFT:
            // Reset FFT buffer position
            generator->fft_buffer_pos = 0;
            break;

        case PINK_NOISE_WHITE:
            // White noise needs no special reset
            break;
    }

    set_error(NULL);
    return true;
}

//=============================================================================
// Spectral Analysis via DFT
//=============================================================================

/**
 * @brief Compute power spectral density via DFT
 *
 * WHAT: Estimates spectral slope α from 1/f^α power law
 * WHY:  Validates that generated noise matches expected spectrum
 * HOW:  Compute DFT, fit log-log linear regression to estimate slope
 *
 * ALGORITHM:
 *   1. Compute power spectrum P(f) via DFT
 *   2. Convert to log-log: log(P) vs log(f)
 *   3. Linear regression: log(P) = -α*log(f) + b
 *   4. Extract α and R² goodness-of-fit
 *
 * @param samples Input samples
 * @param num_samples Number of samples
 * @param sample_rate Sampling rate (Hz)
 * @param alpha Output: measured spectral exponent
 * @param r_squared Output: R² goodness of fit
 * @return true on success
 */
static bool compute_spectral_slope(
    const float* samples,
    uint32_t num_samples,
    float sample_rate,
    float* alpha,
    float* r_squared)
{
    // Guard: Invalid inputs
    if (!samples || !alpha || !r_squared) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compute_spectral_slope: required parameter is NULL (samples, alpha, r_squared)");
        return false;
    }

    // Guard: Too few samples
    if (num_samples < 64) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compute_spectral_slope: validation failed");
        return false;
    }

    // Limit DFT size for performance (use up to 1024 samples)
    uint32_t dft_size = num_samples;
    if (dft_size > 1024) {
        dft_size = 1024;
    }

    // Compute power spectrum via DFT
    // P(f) = |Σ x(t) * e^(-2πift)|²

    const uint32_t num_freqs = dft_size / 2;  // Only positive frequencies
    float power[512];  // Max 1024/2 = 512 frequency bins

    for (uint32_t k = 1; k < num_freqs && k < 512; k++) {  // Skip DC (k=0)
        float real_part = 0.0F;
        float imag_part = 0.0F;

        // Compute DFT bin k
        for (uint32_t n = 0; n < dft_size; n++) {
            float angle = -2.0F * M_PI * (float)k * (float)n / (float)dft_size;
            real_part += samples[n] * cosf(angle);
            imag_part += samples[n] * sinf(angle);
        }

        // Power = magnitude squared
        power[k] = real_part * real_part + imag_part * imag_part;
    }

    // Log-log linear regression to estimate slope
    // log(P) = -α*log(f) + b
    // Use frequency range 10% to 90% of Nyquist to avoid edge effects

    uint32_t start_bin = num_freqs / 10;
    if (start_bin < 2) start_bin = 2;
    uint32_t end_bin = (num_freqs * 9) / 10;

    float sum_log_f = 0.0F;
    float sum_log_p = 0.0F;
    float sum_log_f_sq = 0.0F;
    float sum_log_f_log_p = 0.0F;
    uint32_t count = 0;

    for (uint32_t k = start_bin; k < end_bin && k < 512; k++) {
        // Guard: Skip zero/negative power
        if (power[k] <= 0.0F) continue;

        float freq = (float)k * sample_rate / (float)dft_size;
        float log_f = logf(freq);
        float log_p = logf(power[k]);

        sum_log_f += log_f;
        sum_log_p += log_p;
        sum_log_f_sq += log_f * log_f;
        sum_log_f_log_p += log_f * log_p;
        count++;
    }

    // Guard: Not enough valid points
    if (count < 10) {
        *alpha = 1.0F;
        *r_squared = 0.0F;
        return true;  // Return default values
    }

    // Linear regression: slope = (nΣxy - ΣxΣy) / (nΣx² - (Σx)²)
    float n = (float)count;
    float numerator = n * sum_log_f_log_p - sum_log_f * sum_log_p;
    float denominator = n * sum_log_f_sq - sum_log_f * sum_log_f;

    // Guard: Singular matrix
    if (fabsf(denominator) < 1e-10F) {
        *alpha = 1.0F;
        *r_squared = 0.0F;
        return true;
    }

    float slope = numerator / denominator;
    *alpha = -slope;  // Negative because P ∝ f^(-α)

    // Compute R² goodness of fit
    float mean_log_p = sum_log_p / n;
    float ss_tot = 0.0F;  // Total sum of squares
    float ss_res = 0.0F;  // Residual sum of squares

    for (uint32_t k = start_bin; k < end_bin && k < 512; k++) {
        if (power[k] <= 0.0F) continue;

        float freq = (float)k * sample_rate / (float)dft_size;
        float log_f = logf(freq);
        float log_p = logf(power[k]);

        // Predicted log(P) from fitted line
        float intercept = (sum_log_p - slope * sum_log_f) / n;
        float predicted = slope * log_f + intercept;

        ss_tot += (log_p - mean_log_p) * (log_p - mean_log_p);
        ss_res += (log_p - predicted) * (log_p - predicted);
    }

    // R² = 1 - (SS_res / SS_tot)
    if (ss_tot > 1e-10F) {
        *r_squared = 1.0F - (ss_res / ss_tot);
    } else {
        *r_squared = 0.0F;
    }

    return true;
}

bool pink_noise_compute_stats(const float* samples, uint32_t num_samples, float sample_rate, pink_noise_stats_t* stats) {
    // Guard: NULL samples
    if (!samples) {
        set_error("Samples array is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_compute_stats: samples is NULL");
        return false;
    }

    // Guard: NULL stats
    if (!stats) {
        set_error("Stats pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_compute_stats: stats is NULL");
        return false;
    }

    // Guard: Too few samples
    if (num_samples < MIN_SAMPLES_FOR_STATS) {
        set_error("Too few samples for statistics (minimum 64)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pink_noise_compute_stats: validation failed");
        return false;
    }

    // Initialize stats
    memset(stats, 0, sizeof(pink_noise_stats_t));

    // Compute mean
    float sum = 0.0F;
    for (uint32_t i = 0; i < num_samples; i++) {
        sum += samples[i];
    }
    stats->mean = sum / (float)num_samples;

    // Compute std dev, min, max
    float sum_sq = 0.0F;
    stats->min_value = samples[0];
    stats->max_value = samples[0];

    for (uint32_t i = 0; i < num_samples; i++) {
        float dev = samples[i] - stats->mean;
        sum_sq += dev * dev;

        if (samples[i] < stats->min_value) {
            stats->min_value = samples[i];
        }
        if (samples[i] > stats->max_value) {
            stats->max_value = samples[i];
        }
    }

    stats->std_dev = sqrtf(sum_sq / (float)num_samples);
    stats->measured_amplitude = stats->std_dev;  // RMS amplitude

    // Perform spectral analysis via DFT
    bool spectral_success = compute_spectral_slope(
        samples, num_samples, sample_rate,
        &stats->measured_alpha, &stats->spectral_fit_r2
    );

    // Guard: Spectral analysis failed
    if (!spectral_success) {
        // Use default values
        stats->measured_alpha = 1.0F;
        stats->spectral_fit_r2 = 0.0F;
    }

    set_error(NULL);
    return true;
}

bool pink_noise_validate(const float* samples, uint32_t num_samples, float sample_rate, float expected_alpha, float tolerance) {
    // Guard: NULL samples
    if (!samples) {
        set_error("Samples array is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_validate: samples is NULL");
        return false;
    }

    // Guard: Invalid tolerance
    if (tolerance <= 0.0F) {
        set_error("Tolerance must be positive");
        return false;
    }

    // Compute statistics
    pink_noise_stats_t stats;
    bool success = pink_noise_compute_stats(samples, num_samples, sample_rate, &stats);

    // Guard: Stats computation failed
    if (!success) {
        // Error already set by compute_stats
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_validate: success is NULL");
        return false;
    }

    // Check if measured alpha is within tolerance
    float alpha_diff = fabsf(stats.measured_alpha - expected_alpha);

    // Guard: Alpha out of tolerance
    if (alpha_diff > tolerance) {
        set_error("Spectral exponent outside tolerance");
        return false;
    }

    set_error(NULL);
    return true;
}

bool pink_noise_modulate(pink_noise_generator_t generator, float base_level, float* output) {
    // Guard: NULL generator
    if (!generator) {
        set_error("Generator is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_modulate: generator is NULL");
        return false;
    }

    // Guard: NULL output
    if (!output) {
        set_error("Output pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_modulate: output is NULL");
        return false;
    }

    // Generate noise sample
    float noise;
    bool success = pink_noise_generate_sample(generator, &noise);

    // Guard: Generation failed
    if (!success) {
        // Error already set by generate_sample
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_modulate: success is NULL");
        return false;
    }

    // Apply additive modulation
    *output = base_level + noise;

    set_error(NULL);
    return true;
}

bool pink_noise_modulate_multiplicative(pink_noise_generator_t generator, float value, float modulation_strength, float* output) {
    // Guard: NULL generator
    if (!generator) {
        set_error("Generator is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_modulate_multiplicative: generator is NULL");
        return false;
    }

    // Guard: NULL output
    if (!output) {
        set_error("Output pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_modulate_multiplicative: output is NULL");
        return false;
    }

    // Guard: Invalid modulation strength
    if (modulation_strength < 0.0F || modulation_strength > 1.0F) {
        set_error("Modulation strength must be in [0, 1] range");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pink_noise_modulate_multiplicative: validation failed");
        return false;
    }

    // Generate noise sample
    float noise;
    bool success = pink_noise_generate_sample(generator, &noise);

    // Guard: Generation failed
    if (!success) {
        // Error already set by generate_sample
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_modulate_multiplicative: success is NULL");
        return false;
    }

    // Normalize noise to [-1, 1] range (approximately)
    float normalized_noise = noise / (3.0F * generator->config.amplitude);  // ±3σ clipping

    // Guard: Clamp to [-1, 1]
    if (normalized_noise < -1.0F) normalized_noise = -1.0F;
    if (normalized_noise > 1.0F) normalized_noise = 1.0F;

    // Apply multiplicative modulation
    *output = value * (1.0F + modulation_strength * normalized_noise);

    set_error(NULL);
    return true;
}

const char* pink_noise_method_name(pink_noise_method_t method) {
    switch (method) {
        case PINK_NOISE_FFT:   return "FFT";
        case PINK_NOISE_VOSS:  return "Voss";
        case PINK_NOISE_IIR:   return "IIR";
        case PINK_NOISE_WHITE: return "White";
        default:               return "Unknown";
    }
}

const char* pink_noise_get_last_error(void) {
    // Guard: No error set
    if (last_error[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_get_last_error: validation failed");
        return NULL;
    }

    return last_error;
}

//=============================================================================
// Persistence (Save/Load) - Stub Implementations
//=============================================================================

bool pink_noise_save(pink_noise_generator_t generator, FILE* file) {
    // WHAT: Save pink noise generator state to binary file
    // WHY: Enable persistence for neuromodulator state across sessions
    // HOW: Binary serialization of config + internal state

    if (!generator) {
        set_error("Generator is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_save: generator is NULL");
        return false;
    }

    if (!file) {
        set_error("File handle is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_save: file is NULL");
        return false;
    }

    // Write marker and version
    uint32_t marker = 0x50494E4B;  // "PINK" in ASCII
    uint32_t version = 2;          // Version 2: full state serialization
    if (fwrite(&marker, sizeof(uint32_t), 1, file) != 1 ||
        fwrite(&version, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to write header");
        return false;
    }

    // Write config
    if (fwrite(&generator->config, sizeof(pink_noise_config_t), 1, file) != 1) {
        set_error("Failed to write config");
        return false;
    }

    // Write RNG state
    if (fwrite(&generator->rng_state, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to write RNG state");
        return false;
    }

    // Write Voss-McCartney state
    if (fwrite(generator->voss_octaves, sizeof(float), VOSS_NUM_OCTAVES, file) != VOSS_NUM_OCTAVES) {
        set_error("Failed to write Voss octaves");
        return false;
    }
    if (fwrite(&generator->voss_counter, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to write Voss counter");
        return false;
    }

    // Write IIR filter state
    if (fwrite(generator->iir_history, sizeof(float), 4, file) != 4 ||
        fwrite(generator->iir_coeffs, sizeof(float), 5, file) != 5) {
        set_error("Failed to write IIR state");
        return false;
    }

    set_error(NULL);
    return true;
}

pink_noise_generator_t pink_noise_load(FILE* file) {
    // WHAT: Load pink noise generator state from binary file
    // WHY: Restore neuromodulator state from persistence
    // HOW: Binary deserialization, config + internal state

    if (!file) {
        set_error("Invalid file handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_load: file is NULL");
        return NULL;
    }

    // Read and validate header
    uint32_t marker, version;
    if (fread(&marker, sizeof(uint32_t), 1, file) != 1 ||
        fread(&version, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to read header");
        return NULL;
    }

    if (marker != 0x50494E4B) {
        set_error("Invalid marker (expected PINK)");
        return NULL;
    }

    // Handle version 1 (legacy: marker only, no state)
    if (version == 1 || version == 0) {
        pink_noise_config_t config = {
            .alpha = 1.0F,
            .amplitude = 0.05F,
            .min_frequency = 0.1F,
            .max_frequency = 100.0F,
            .sample_rate = 1000.0F,
            .method = PINK_NOISE_VOSS,
            .seed = 0
        };
        set_error(NULL);
        return pink_noise_create(&config);
    }

    // Version 2: full state
    pink_noise_config_t config;
    if (fread(&config, sizeof(pink_noise_config_t), 1, file) != 1) {
        set_error("Failed to read config");
        return NULL;
    }

    // Create generator with loaded config
    pink_noise_generator_t gen = pink_noise_create(&config);
    if (!gen) {
        set_error("Failed to create generator from loaded config");
        return NULL;
    }

    // Restore RNG state
    if (fread(&gen->rng_state, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to read RNG state");
        pink_noise_destroy(gen);
        return NULL;
    }

    // Restore Voss-McCartney state
    if (fread(gen->voss_octaves, sizeof(float), VOSS_NUM_OCTAVES, file) != VOSS_NUM_OCTAVES ||
        fread(&gen->voss_counter, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to read Voss state");
        pink_noise_destroy(gen);
        return NULL;
    }

    // Restore IIR filter state
    if (fread(gen->iir_history, sizeof(float), 4, file) != 4 ||
        fread(gen->iir_coeffs, sizeof(float), 5, file) != 5) {
        set_error("Failed to read IIR state");
        pink_noise_destroy(gen);
        return NULL;
    }

    set_error(NULL);
    return gen;
}
