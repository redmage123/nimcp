//=============================================================================
// nimcp_pink_noise.h - 1/f Pink Noise Generator for Neuromodulation
//=============================================================================
/**
 * @file nimcp_pink_noise.h
 * @brief 1/f^α noise generation for biologically realistic neuromodulation
 *
 * WHAT: Generates pink noise (1/f spectrum) for neural activity modulation
 * WHY: Real neurons exhibit 1/f noise in spike timing and synaptic strength
 *      - Multi-timescale learning (fast + slow components)
 *      - More robust learning than pure white noise
 *      - Matches biological neural variability (Bédard et al., 2006)
 *      - Critical for homeostatic plasticity
 * HOW: FFT-based spectral synthesis + fast Voss-McCartney approximation
 *
 * BIOLOGICAL MOTIVATION:
 * - Neural firing rates exhibit 1/f spectrum (Milstein et al., 2009)
 * - Membrane potential fluctuations are pink (Destexhe et al., 2003)
 * - Synaptic efficacy varies with 1/f statistics (Câteau & Reyes, 2006)
 * - Critical for balancing exploration vs exploitation in learning
 *
 * MATHEMATICAL BACKGROUND:
 * - White noise: S(f) = constant (all frequencies equal power)
 * - Pink noise: S(f) ∝ 1/f^α where α ≈ 1
 * - Red/Brownian: S(f) ∝ 1/f² (α = 2)
 * - Power distribution: More low frequencies → long-term dependencies
 *
 * PERFORMANCE:
 * - FFT method: O(N log N) for N samples, high quality
 * - Voss-McCartney: O(N) generation, good approximation
 * - Streaming mode: O(1) per sample, real-time suitable
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.7.0
 */

#ifndef NIMCP_PINK_NOISE_H
#define NIMCP_PINK_NOISE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Noise Configuration and Types
//=============================================================================

/**
 * @brief Noise generation algorithms
 *
 * WHAT: Different algorithms for generating 1/f^α noise
 * WHY: Trade-off between quality and performance
 */
typedef enum {
    PINK_NOISE_FFT,         /**< FFT-based spectral synthesis (highest quality) */
    PINK_NOISE_VOSS,        /**< Voss-McCartney algorithm (fast, good approximation) */
    PINK_NOISE_IIR,         /**< Infinite impulse response filter (streaming) */
    PINK_NOISE_WHITE        /**< White noise baseline for comparison */
} pink_noise_method_t;

/**
 * @brief Configuration for pink noise generation
 *
 * WHAT: Parameters controlling noise spectrum and characteristics
 * WHY: Flexibility to match different biological systems
 * HOW: Controls spectral slope, amplitude, and generation method
 */
typedef struct {
    float alpha;            /**< Spectral exponent (0=white, 1=pink, 2=red), typical: 0.8-1.2 */
    float amplitude;        /**< RMS amplitude of noise, typical: 0.01-0.1 */
    float min_frequency;    /**< Minimum frequency (Hz), typical: 0.1 */
    float max_frequency;    /**< Maximum frequency (Hz), typical: 100.0 */
    float sample_rate;      /**< Sampling rate (Hz), typical: 1000.0 */
    pink_noise_method_t method; /**< Generation algorithm to use */
    uint32_t seed;          /**< Random seed (0 = use time) */
} pink_noise_config_t;

/**
 * @brief Opaque handle for pink noise generator state
 *
 * WHAT: Maintains internal state for streaming noise generation
 * WHY: Allows efficient real-time generation without recomputation
 * HOW: Stores filter coefficients, RNG state, buffers
 */
typedef struct pink_noise_generator_internal_t* pink_noise_generator_t;

/**
 * @brief Statistics describing generated noise
 *
 * WHAT: Metrics validating noise properties
 * WHY: Verify generated noise matches expected 1/f spectrum
 * HOW: Computed via spectral analysis
 */
typedef struct {
    float measured_alpha;       /**< Fitted spectral exponent */
    float measured_amplitude;   /**< RMS amplitude */
    float spectral_fit_r2;      /**< R² for 1/f^α fit (0-1, >0.9 is good) */
    float mean;                 /**< Mean value (should be ~0) */
    float std_dev;              /**< Standard deviation */
    float min_value;            /**< Minimum sample value */
    float max_value;            /**< Maximum sample value */
} pink_noise_stats_t;

//=============================================================================
// Generator Lifecycle Functions
//=============================================================================

/**
 * @brief Create pink noise generator
 *
 * WHAT: Initializes generator with specified configuration
 * WHY: Prepare internal state for efficient noise generation
 * HOW: Allocate buffers, precompute filter coefficients, seed RNG
 *
 * ALGORITHM (FFT method):
 *   1. Generate white noise in frequency domain
 *   2. Apply 1/f^α envelope to magnitudes
 *   3. Randomize phases for each frequency bin
 *   4. Inverse FFT to get time-domain pink noise
 *
 * ALGORITHM (Voss-McCartney method):
 *   1. Initialize k octave generators (typically k=16)
 *   2. Each octave updates at rate 2^(-i) where i=0..k-1
 *   3. Sum all octaves to approximate 1/f spectrum
 *
 * @param config Generator configuration
 * @return Generator handle on success, NULL on failure
 *
 * @note Caller must call pink_noise_destroy() when done
 * @note Thread-safe if each generator is used by single thread
 *
 * EXAMPLE:
 * @code
 * pink_noise_config_t config = pink_noise_default_config();
 * config.alpha = 1.0f;  // True pink noise
 * config.amplitude = 0.05f;
 * pink_noise_generator_t gen = pink_noise_create(&config);
 * @endcode
 */
pink_noise_generator_t pink_noise_create(const pink_noise_config_t* config);

/**
 * @brief Destroy pink noise generator
 *
 * WHAT: Frees all resources associated with generator
 * WHY: Prevent memory leaks
 * HOW: Free buffers, clear state
 *
 * @param generator Generator to destroy (can be NULL)
 *
 * @note Safe to call with NULL generator
 * @note After calling, generator handle is invalid
 */
void pink_noise_destroy(pink_noise_generator_t generator);

//=============================================================================
// Noise Generation Functions
//=============================================================================

/**
 * @brief Generate batch of pink noise samples
 *
 * WHAT: Fills array with pink noise values
 * WHY: Efficient batch generation for precomputing noise sequences
 * HOW: Use configured algorithm to generate N samples
 *
 * @param generator Pink noise generator
 * @param samples Output array (must be allocated, size >= num_samples)
 * @param num_samples Number of samples to generate
 * @return true on success, false on failure
 *
 * @note Generated samples have zero mean and RMS amplitude as configured
 * @note Successive calls produce continuous correlated sequence
 *
 * USAGE:
 * @code
 * float noise[1000];
 * pink_noise_generate(gen, noise, 1000);
 * // Apply to neuromodulator: M(t) = M0 + noise[t]
 * @endcode
 */
bool pink_noise_generate(
    pink_noise_generator_t generator,
    float* samples,
    uint32_t num_samples
);

/**
 * @brief Generate single pink noise sample
 *
 * WHAT: Returns next sample in pink noise sequence
 * WHY: Streaming generation for real-time applications
 * HOW: Use internal state to produce next value
 *
 * @param generator Pink noise generator
 * @param sample Output sample value
 * @return true on success, false on failure
 *
 * @note O(1) time complexity for IIR/Voss methods
 * @note Maintains temporal correlation between successive calls
 *
 * USAGE:
 * @code
 * float noise_value;
 * while (running) {
 *     pink_noise_generate_sample(gen, &noise_value);
 *     apply_neuromodulation(network, noise_value);
 *     network_step(network);
 * }
 * @endcode
 */
bool pink_noise_generate_sample(
    pink_noise_generator_t generator,
    float* sample
);

/**
 * @brief Reset generator to initial state
 *
 * WHAT: Clears internal state and reseeds RNG
 * WHY: Restart noise sequence for reproducibility or new trial
 * HOW: Reset buffers, reseed with configured/new seed
 *
 * @param generator Pink noise generator
 * @param new_seed New random seed (0 = use configured seed)
 * @return true on success, false on failure
 */
bool pink_noise_reset(
    pink_noise_generator_t generator,
    uint32_t new_seed
);

//=============================================================================
// Analysis and Validation Functions
//=============================================================================

/**
 * @brief Compute statistics for generated noise
 *
 * WHAT: Analyzes noise samples and validates 1/f^α spectrum
 * WHY: Verify noise quality matches biological expectations
 * HOW: Spectral analysis via FFT, fit log-log slope
 *
 * ALGORITHM:
 *   1. Compute power spectrum: P(f) via FFT
 *   2. Log-log transform: log P vs log f
 *   3. Linear regression: slope = -α
 *   4. Compute R² goodness-of-fit
 *
 * @param samples Noise samples to analyze
 * @param num_samples Number of samples (should be power of 2 for FFT)
 * @param sample_rate Sampling rate (Hz)
 * @param stats Output statistics struct
 * @return true on success, false on failure
 *
 * @note Requires num_samples >= 64 for meaningful spectral analysis
 * @note Best results with num_samples as power of 2 (FFT optimization)
 */
bool pink_noise_compute_stats(
    const float* samples,
    uint32_t num_samples,
    float sample_rate,
    pink_noise_stats_t* stats
);

/**
 * @brief Validate noise matches expected 1/f^α spectrum
 *
 * WHAT: Tests if samples exhibit power-law spectrum within tolerance
 * WHY: Quality assurance for biological realism
 * HOW: Compute stats, check |measured_alpha - expected_alpha| < tolerance
 *
 * @param samples Noise samples to validate
 * @param num_samples Number of samples
 * @param sample_rate Sampling rate (Hz)
 * @param expected_alpha Expected spectral exponent
 * @param tolerance Acceptable deviation (typical: 0.1-0.2)
 * @return true if valid, false otherwise
 *
 * EXAMPLE:
 * @code
 * float noise[1024];
 * pink_noise_generate(gen, noise, 1024);
 * bool is_valid = pink_noise_validate(noise, 1024, 1000.0f, 1.0f, 0.15f);
 * assert(is_valid);  // Should be true for good pink noise
 * @endcode
 */
bool pink_noise_validate(
    const float* samples,
    uint32_t num_samples,
    float sample_rate,
    float expected_alpha,
    float tolerance
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Create default pink noise configuration
 *
 * WHAT: Returns sensible defaults for neural neuromodulation
 * WHY: Easy starting point without parameter tuning
 * HOW: Values based on empirical neuroscience measurements
 *
 * DEFAULT VALUES:
 * - alpha = 1.0 (true pink noise, 1/f spectrum)
 * - amplitude = 0.05 (5% modulation)
 * - min_frequency = 0.1 Hz (slow modulation ~10s timescale)
 * - max_frequency = 100 Hz (fast modulation ~10ms timescale)
 * - sample_rate = 1000 Hz (1ms resolution)
 * - method = PINK_NOISE_VOSS (fast, good quality)
 * - seed = 0 (time-based)
 *
 * @return Default configuration
 */
pink_noise_config_t pink_noise_default_config(void);

/**
 * @brief Validate pink noise configuration
 *
 * WHAT: Checks if config parameters are in valid ranges
 * WHY: Prevent invalid noise generation
 * HOW: Range checks on all parameters
 *
 * VALIDATION RULES:
 * - alpha ∈ [0, 3]
 * - amplitude > 0
 * - 0 < min_frequency < max_frequency
 * - sample_rate >= 2 * max_frequency (Nyquist)
 * - method is valid enum value
 *
 * @param config Configuration to validate
 * @return true if valid, false if invalid
 */
bool pink_noise_validate_config(const pink_noise_config_t* config);

//=============================================================================
// Persistence API (Save/Load)
//=============================================================================

/**
 * @brief Save pink noise generator state to file
 *
 * WHAT: Serialize pink noise generator state to binary file
 * WHY:  Enable persistence of noise generation state across sessions
 * HOW:  Write version marker, config, RNG state, and method-specific state
 *
 * Binary format:
 *   uint32_t version (1)
 *   pink_noise_config_t config
 *   uint32_t rng_state
 *   float voss_octaves[VOSS_NUM_OCTAVES]
 *   uint32_t voss_counter
 *   float iir_history[4]
 *   float iir_coeffs[5]
 *
 * @param generator Pink noise generator
 * @param file Open file handle for writing
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must ensure exclusive access)
 */
bool pink_noise_save(pink_noise_generator_t generator, FILE* file);

/**
 * @brief Load pink noise generator state from file
 *
 * WHAT: Deserialize pink noise generator state from binary file
 * WHY:  Restore saved noise generation state
 * HOW:  Read version marker, validate, reconstruct state
 *
 * @param file Open file handle for reading
 * @return Pink noise generator handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (creates new instance)
 */
pink_noise_generator_t pink_noise_load(FILE* file);

/**
 * @brief Get method name as string
 *
 * WHAT: Returns human-readable name for method
 * WHY: Debugging and logging
 * HOW: Simple enum-to-string mapping
 *
 * @param method Generation method
 * @return Method name (e.g., "FFT", "Voss", "IIR", "White")
 */
const char* pink_noise_method_name(pink_noise_method_t method);

//=============================================================================
// Neuromodulation Integration Functions
//=============================================================================

/**
 * @brief Apply pink noise modulation to neuromodulator
 *
 * WHAT: Modulates neuromodulator level with 1/f noise
 * WHY: Biological neuromodulators fluctuate with pink noise spectrum
 * HOW: M_new(t) = M_base + amplitude * pink_noise(t)
 *
 * @param generator Pink noise generator
 * @param base_level Baseline neuromodulator level
 * @param output Output modulated value
 * @return true on success, false on failure
 *
 * EXAMPLE:
 * @code
 * float dopamine_level;
 * pink_noise_modulate(gen, 0.5f, &dopamine_level);
 * network_set_neuromodulator(net, DOPAMINE, dopamine_level);
 * @endcode
 */
bool pink_noise_modulate(
    pink_noise_generator_t generator,
    float base_level,
    float* output
);

/**
 * @brief Apply multiplicative pink noise modulation
 *
 * WHAT: Modulates value multiplicatively: V_new = V * (1 + α*noise)
 * WHY: Synaptic strength often varies multiplicatively
 * HOW: noise ∈ [-1, 1], so V_new ∈ [V*(1-α), V*(1+α)]
 *
 * @param generator Pink noise generator
 * @param value Input value to modulate
 * @param modulation_strength Strength of modulation (typical: 0.05-0.2)
 * @param output Output modulated value
 * @return true on success, false on failure
 *
 * USAGE:
 * @code
 * // Modulate synaptic strength by ±10%
 * float weight_modulated;
 * pink_noise_modulate_multiplicative(gen, weight, 0.1f, &weight_modulated);
 * @endcode
 */
bool pink_noise_modulate_multiplicative(
    pink_noise_generator_t generator,
    float value,
    float modulation_strength,
    float* output
);

//=============================================================================
// Persistence (Save/Load)
//=============================================================================

/**
 * @brief Save pink noise generator state to file
 *
 * WHAT: Serialize generator state for later restoration
 * WHY: Enable model persistence across sessions
 * HOW: Binary write of internal state
 *
 * @param generator Pink noise generator to save
 * @param file Open file handle for writing
 * @return true on success, false on failure
 */
bool pink_noise_save(pink_noise_generator_t generator, FILE* file);

/**
 * @brief Load pink noise generator state from file
 *
 * WHAT: Restore generator state from saved file
 * WHY: Resume from saved model state
 * HOW: Binary read and reconstruction
 *
 * @param file Open file handle for reading
 * @return Restored generator or NULL on failure
 */
pink_noise_generator_t pink_noise_load(FILE* file);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last pink noise error message
 *
 * WHAT: Returns human-readable description of last error
 * WHY: Debugging and user-friendly error reporting
 * HOW: Thread-local error string storage
 *
 * @return Error message string (NULL if no error)
 */
const char* pink_noise_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PINK_NOISE_H
