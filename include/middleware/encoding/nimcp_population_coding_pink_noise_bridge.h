//=============================================================================
// nimcp_population_coding_pink_noise_bridge.h - Population Coding Pink Noise Integration
//=============================================================================
/**
 * @file nimcp_population_coding_pink_noise_bridge.h
 * @brief Integration of pink noise modulation with population coding
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Apply 1/f pink noise to population neural activity for biological realism
 * WHY:  Real neural populations exhibit pink noise in firing rates:
 *       - Enhances coding robustness via stochastic resonance
 *       - Maintains multi-timescale population dynamics
 *       - Prevents pathological synchrony in population codes
 *       - Models spontaneous neural variability
 *
 * HOW:  Pink noise generator modulates population firing rates;
 *       Noise can be applied per-neuron or globally to population activity.
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Population activity exhibits 1/f spectrum (Miller et al., 2009)
 * - Spontaneous variability crucial for exploration (Faisal et al., 2008)
 * - Noise improves signal detection via stochastic resonance
 * - Multi-scale noise supports hierarchical temporal integration
 * - Place cell populations show correlated noise (Fenton & Muller, 1998)
 *
 * APPLICATION MODES:
 * ==================
 * 1. PER-NEURON NOISE: Independent noise per neuron
 *    - Models uncorrelated spontaneous activity
 *    - Each neuron gets different noise realization
 *    - Reduces synchrony, increases population code dimensionality
 *
 * 2. GLOBAL NOISE: Correlated noise across population
 *    - Models shared neuromodulatory fluctuations
 *    - All neurons share same noise signal
 *    - Preserves relative firing rate structure
 *
 * 3. HYBRID: Correlated + independent components
 *    - Global noise models shared neuromodulation
 *    - Local noise models intrinsic variability
 *    - correlation_factor controls mix
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_POPULATION_CODING_PINK_NOISE_BRIDGE_H
#define NIMCP_POPULATION_CODING_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>
#include "middleware/encoding/nimcp_population_coding.h"
#include "plasticity/noise/nimcp_pink_noise.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of neurons for per-neuron noise */
#define POPULATION_PINK_MAX_NEURONS    10000

/** Default correlation factor for hybrid mode */
#define POPULATION_PINK_DEFAULT_CORRELATION   0.3f

//=============================================================================
// Noise Application Modes
//=============================================================================

/**
 * @brief Mode for applying pink noise to population
 */
typedef enum {
    POPULATION_PINK_MODE_GLOBAL,        /**< All neurons share same noise */
    POPULATION_PINK_MODE_PER_NEURON,    /**< Independent noise per neuron */
    POPULATION_PINK_MODE_HYBRID         /**< Mix of global + local noise */
} population_pink_noise_mode_t;

/**
 * @brief Target of noise modulation
 */
typedef enum {
    POPULATION_PINK_TARGET_RATES,       /**< Modulate firing rates directly */
    POPULATION_PINK_TARGET_TUNING,      /**< Modulate tuning curve widths */
    POPULATION_PINK_TARGET_POSITIONS,   /**< Modulate position encodings */
    POPULATION_PINK_TARGET_ALL          /**< Modulate all aspects */
} population_pink_noise_target_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Pink noise bridge configuration
 */
typedef struct {
    // Noise characteristics
    float alpha;                        /**< Spectral exponent (0=white, 1=pink, 2=red) */
    float amplitude;                    /**< Base noise amplitude (RMS) */
    float min_frequency;                /**< Minimum frequency (Hz) */
    float max_frequency;                /**< Maximum frequency (Hz) */
    float sample_rate;                  /**< Sampling rate (Hz) */

    // Application mode
    population_pink_noise_mode_t mode;  /**< How noise is applied */
    population_pink_noise_target_t target; /**< What to modulate */

    // Hybrid mode parameters
    float correlation_factor;           /**< [0-1] global vs local mix (0=all local, 1=all global) */

    // Rate modulation parameters
    float rate_modulation_strength;     /**< Strength of rate modulation [0-1] */
    bool multiplicative_rate_noise;     /**< If true: rate *= (1 + noise), else: rate += noise */
    float max_rate_change;              /**< Max fractional rate change per step */

    // Tuning modulation parameters
    float tuning_modulation_strength;   /**< Strength of tuning width modulation [0-1] */
    float min_tuning_width;             /**< Minimum tuning width (radians) */
    float max_tuning_width;             /**< Maximum tuning width (radians) */

    // Position modulation parameters
    float position_modulation_strength; /**< Strength of position jitter [0-1] */

    // Pink noise generator config
    pink_noise_method_t method;         /**< Generation algorithm */
    uint32_t seed;                      /**< Random seed (0 = time-based) */

    // Control flags
    bool enable_noise;                  /**< Master enable/disable */
    bool clamp_rates;                   /**< Clamp rates to [0, max_rate] */
} population_pink_config_t;

//=============================================================================
// State Structure
//=============================================================================

/**
 * @brief Statistics for noise modulation
 */
typedef struct {
    float avg_noise_level;              /**< Average noise amplitude */
    float max_noise_level;              /**< Maximum noise seen */
    float rate_variance_increase;       /**< Increase in rate variance due to noise */
    uint64_t samples_generated;         /**< Total noise samples generated */
} population_pink_stats_t;

/**
 * @brief Population pink noise bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    population_pink_config_t config;
    population_coding_encoder_t encoder;
    pink_noise_generator_t global_generator;
    pink_noise_generator_t* per_neuron_generators;  /**< Array for per-neuron mode */
    uint32_t num_neurons;

    // Current modulation state
    float* current_noise_values;        /**< Current noise per neuron */
    float global_noise_value;           /**< Shared noise for global/hybrid mode */

    // Statistics
    population_pink_stats_t stats;
    uint64_t update_count;

    // Thread safety
} population_pink_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create population coding pink noise bridge
 *
 * WHAT: Initialize bridge for pink noise modulation of population coding
 * WHY:  Enable biologically realistic noise in population responses
 * HOW:  Create noise generators based on mode, allocate state
 *
 * ALGORITHM:
 * 1. Validate configuration
 * 2. Create global noise generator
 * 3. If per-neuron mode, create generator array
 * 4. Allocate noise value storage
 * 5. Initialize statistics
 *
 * @param config Bridge configuration (NULL = defaults)
 * @param num_neurons Number of neurons in population
 * @return Bridge handle or NULL on failure
 *
 * COMPLEXITY: O(n) where n = num_neurons (per-neuron mode)
 */
population_pink_bridge_t* population_pink_bridge_create(
    const population_pink_config_t* config,
    uint32_t num_neurons
);

/**
 * @brief Destroy population pink noise bridge
 *
 * WHAT: Clean up all bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free generators, arrays, mutex
 *
 * @param bridge Bridge to destroy (NULL-safe)
 *
 * COMPLEXITY: O(n) for per-neuron mode
 */
void population_pink_bridge_destroy(population_pink_bridge_t* bridge);

/**
 * @brief Get default configuration
 *
 * WHAT: Return sensible defaults for population pink noise
 * WHY:  Easy initialization without parameter tuning
 *
 * DEFAULTS:
 * - alpha = 1.0 (true pink noise)
 * - amplitude = 0.05 (5% RMS modulation)
 * - min_frequency = 0.1 Hz
 * - max_frequency = 100 Hz
 * - sample_rate = 1000 Hz
 * - mode = POPULATION_PINK_MODE_HYBRID
 * - target = POPULATION_PINK_TARGET_RATES
 * - correlation_factor = 0.3 (30% shared, 70% independent)
 * - rate_modulation_strength = 0.1 (10% modulation)
 * - multiplicative_rate_noise = true
 * - method = PINK_NOISE_VOSS
 * - enable_noise = true
 * - clamp_rates = true
 *
 * @return Default configuration
 */
population_pink_config_t population_pink_bridge_default_config(void);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect to population coding encoder
 *
 * WHAT: Associate bridge with population encoder
 * WHY:  Enable coordinated noise application
 *
 * @param bridge Pink noise bridge
 * @param encoder Population coding encoder
 * @return 0 on success, negative on error
 */
int population_pink_bridge_connect_encoder(
    population_pink_bridge_t* bridge,
    population_coding_encoder_t encoder
);

/**
 * @brief Disconnect from encoder
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative on error
 */
int population_pink_bridge_disconnect(population_pink_bridge_t* bridge);

//=============================================================================
// Noise Generation Functions
//=============================================================================

/**
 * @brief Update noise values for all neurons
 *
 * WHAT: Generate fresh noise samples for population
 * WHY:  Maintain temporal correlation structure
 * HOW:  Generate global + per-neuron noise based on mode
 *
 * ALGORITHM (HYBRID mode):
 * 1. Generate global noise sample
 * 2. For each neuron i:
 *    - Generate local noise sample
 *    - noise[i] = correlation * global + (1-correlation) * local[i]
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(n) where n = num_neurons
 */
int population_pink_bridge_update_noise(population_pink_bridge_t* bridge);

/**
 * @brief Get current noise value for specific neuron
 *
 * @param bridge Pink noise bridge
 * @param neuron_idx Neuron index [0, num_neurons)
 * @return Noise value or 0.0 if invalid
 */
float population_pink_bridge_get_noise(
    const population_pink_bridge_t* bridge,
    uint32_t neuron_idx
);

/**
 * @brief Get global noise value
 *
 * @param bridge Pink noise bridge
 * @return Global noise value (shared component)
 */
float population_pink_bridge_get_global_noise(
    const population_pink_bridge_t* bridge
);

//=============================================================================
// Modulation Functions
//=============================================================================

/**
 * @brief Apply pink noise to population firing rates
 *
 * WHAT: Modulate input rates with current noise values
 * WHY:  Add biological variability to population responses
 * HOW:  Apply additive or multiplicative noise per neuron
 *
 * ALGORITHM (multiplicative):
 * 1. For each neuron i:
 *    - modulated_rate[i] = rate[i] * (1 + strength * noise[i])
 *    - Clamp to [0, max_rate] if clamp_rates enabled
 *
 * ALGORITHM (additive):
 * 1. For each neuron i:
 *    - modulated_rate[i] = rate[i] + strength * amplitude * noise[i]
 *    - Clamp if enabled
 *
 * @param bridge Pink noise bridge
 * @param rates_in Input firing rates [num_neurons]
 * @param num_neurons Number of neurons
 * @param rates_out Modulated rates [num_neurons] (can alias rates_in)
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(n)
 */
int population_pink_bridge_modulate_rates(
    population_pink_bridge_t* bridge,
    const float* rates_in,
    uint32_t num_neurons,
    float* rates_out
);

/**
 * @brief Apply pink noise to tuning curves
 *
 * WHAT: Modulate tuning curve widths with noise
 * WHY:  Model dynamic tuning changes due to neural variability
 * HOW:  Multiplicatively modulate tuning width
 *
 * @param bridge Pink noise bridge
 * @param tuning_in Input tuning curves [num_neurons]
 * @param num_neurons Number of neurons
 * @param tuning_out Modulated tuning curves [num_neurons] (can alias)
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(n)
 */
int population_pink_bridge_modulate_tuning(
    population_pink_bridge_t* bridge,
    const tuning_curve_t* tuning_in,
    uint32_t num_neurons,
    tuning_curve_t* tuning_out
);

/**
 * @brief Apply pink noise to neuron positions
 *
 * WHAT: Add positional jitter based on pink noise
 * WHY:  Model spatial variability in neural organization
 * HOW:  Small additive perturbations to position vectors
 *
 * @param bridge Pink noise bridge
 * @param positions_in Input positions [num_neurons]
 * @param num_neurons Number of neurons
 * @param positions_out Jittered positions [num_neurons] (can alias)
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(n)
 */
int population_pink_bridge_modulate_positions(
    population_pink_bridge_t* bridge,
    const vector3d_t* positions_in,
    uint32_t num_neurons,
    vector3d_t* positions_out
);

//=============================================================================
// Control Functions
//=============================================================================

/**
 * @brief Enable noise modulation
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative on error
 */
int population_pink_bridge_enable(population_pink_bridge_t* bridge);

/**
 * @brief Disable noise modulation
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative on error
 */
int population_pink_bridge_disable(population_pink_bridge_t* bridge);

/**
 * @brief Check if noise is enabled
 *
 * @param bridge Pink noise bridge
 * @return true if enabled, false otherwise
 */
bool population_pink_bridge_is_enabled(const population_pink_bridge_t* bridge);

/**
 * @brief Reset noise generators
 *
 * WHAT: Reseed generators and clear state
 * WHY:  Start fresh noise sequence
 *
 * @param bridge Pink noise bridge
 * @param new_seed New random seed (0 = use configured seed)
 * @return 0 on success, negative on error
 */
int population_pink_bridge_reset(
    population_pink_bridge_t* bridge,
    uint32_t new_seed
);

//=============================================================================
// Configuration Update Functions
//=============================================================================

/**
 * @brief Set noise amplitude
 *
 * @param bridge Pink noise bridge
 * @param amplitude New amplitude (RMS)
 * @return 0 on success, negative on error
 */
int population_pink_bridge_set_amplitude(
    population_pink_bridge_t* bridge,
    float amplitude
);

/**
 * @brief Set spectral exponent
 *
 * @param bridge Pink noise bridge
 * @param alpha New spectral exponent [0-3]
 * @return 0 on success, negative on error
 */
int population_pink_bridge_set_alpha(
    population_pink_bridge_t* bridge,
    float alpha
);

/**
 * @brief Set correlation factor (hybrid mode)
 *
 * @param bridge Pink noise bridge
 * @param correlation Correlation factor [0-1]
 * @return 0 on success, negative on error
 */
int population_pink_bridge_set_correlation(
    population_pink_bridge_t* bridge,
    float correlation
);

/**
 * @brief Set rate modulation strength
 *
 * @param bridge Pink noise bridge
 * @param strength Modulation strength [0-1]
 * @return 0 on success, negative on error
 */
int population_pink_bridge_set_rate_modulation(
    population_pink_bridge_t* bridge,
    float strength
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Pink noise bridge
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int population_pink_bridge_get_stats(
    const population_pink_bridge_t* bridge,
    population_pink_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative on error
 */
int population_pink_bridge_reset_stats(population_pink_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_POPULATION_CODING_PINK_NOISE_BRIDGE_H
