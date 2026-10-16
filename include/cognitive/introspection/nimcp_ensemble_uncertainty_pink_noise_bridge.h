//=============================================================================
// nimcp_ensemble_uncertainty_pink_noise_bridge.h
//=============================================================================
/**
 * @file nimcp_ensemble_uncertainty_pink_noise_bridge.h
 * @brief Pink Noise - Ensemble Uncertainty Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between pink noise and ensemble uncertainty estimation
 * WHY:  Real neural systems exhibit 1/f noise in ensemble dynamics:
 *       - Neural populations show pink noise correlations (Churchland et al., 2010)
 *       - Uncertainty estimates benefit from biologically realistic variability
 *       - Pink noise in model predictions matches cortical ensemble dynamics
 *       - Prevents overconfident ensemble consensus via natural fluctuations
 *
 * HOW:  Pink noise modulates ensemble model predictions, adding realistic
 *       temporal correlations to uncertainty estimates. Ensemble uncertainty
 *       metrics inform noise parameters (high uncertainty → increase noise).
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Cortical populations exhibit 1/f noise in firing rate correlations
 * - Neural ensembles show pink noise in trial-to-trial variability (Churchland, 2010)
 * - Uncertainty is encoded as variance in population activity (Ma et al., 2006)
 * - Noise correlation structure affects information capacity (Moreno-Bote et al., 2014)
 * - Pink noise in neural ensembles reflects integration across timescales
 *
 * PINK NOISE → ENSEMBLE PATHWAYS:
 * ===============================
 * - Add 1/f noise to individual model predictions (realistic variability)
 * - Modulate model weights with pink noise (ensemble diversity)
 * - Inject noise into feature inputs (data augmentation)
 * - Temporal correlation preserves realistic dynamics
 *
 * ENSEMBLE → PINK NOISE PATHWAYS:
 * ===============================
 * - High epistemic uncertainty → increase noise amplitude (explore more)
 * - High aleatoric uncertainty → reduce noise (inherent variability present)
 * - Model agreement → decrease noise (consensus reached)
 * - Divergent predictions → increase noise diversity
 *
 * MATHEMATICAL MODEL:
 * ==================
 * For ensemble model i at time t:
 *   prediction_i(t) = base_prediction_i(t) + α * pink_noise_i(t)
 *
 * Where:
 *   α = amplitude modulated by epistemic/aleatoric ratio
 *   pink_noise_i ~ 1/f^β with temporal correlation τ
 *
 * Uncertainty metrics:
 *   epistemic = Var(prediction_1, ..., prediction_N)
 *   aleatoric = E[H(prediction_i)]
 *
 * Noise adaptation:
 *   α(t+1) = α(t) + λ * (epistemic(t) - epistemic_target)
 *   β(t+1) = β_base + η * (aleatoric(t) / total(t))
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ENSEMBLE_UNCERTAINTY_PINK_NOISE_BRIDGE_H
#define NIMCP_ENSEMBLE_UNCERTAINTY_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/introspection/nimcp_ensemble_uncertainty.h"
#include "plasticity/noise/nimcp_pink_noise.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define ENSEMBLE_PINK_MAX_MODELS          32    /**< Maximum ensemble models */
#define ENSEMBLE_PINK_DEFAULT_AMPLITUDE   0.05f /**< Default noise amplitude (5%) */
#define ENSEMBLE_PINK_DEFAULT_ALPHA       1.0f  /**< Default spectral exponent */
#define ENSEMBLE_PINK_MIN_AMPLITUDE       0.001f /**< Minimum noise amplitude */
#define ENSEMBLE_PINK_MAX_AMPLITUDE       0.5f  /**< Maximum noise amplitude */
#define ENSEMBLE_PINK_ADAPTATION_RATE     0.01f /**< Noise adaptation learning rate */

//=============================================================================
// Types and Enumerations
//=============================================================================

/**
 * @brief Pink noise injection mode
 *
 * WHAT: Where to inject pink noise into ensemble
 * WHY:  Different injection points model different biological mechanisms
 */
typedef enum {
    ENSEMBLE_PINK_INJECT_PREDICTIONS,   /**< Add noise to model outputs (default) */
    ENSEMBLE_PINK_INJECT_FEATURES,      /**< Add noise to input features (data augmentation) */
    ENSEMBLE_PINK_INJECT_WEIGHTS,       /**< Modulate model weights (synaptic noise) */
    ENSEMBLE_PINK_INJECT_COMBINED       /**< Combination of above */
} ensemble_pink_injection_mode_t;

/**
 * @brief Noise adaptation strategy
 *
 * WHAT: How noise parameters adapt to uncertainty
 * WHY:  Different strategies for different learning scenarios
 */
typedef enum {
    ENSEMBLE_PINK_ADAPT_NONE,           /**< Fixed noise parameters */
    ENSEMBLE_PINK_ADAPT_EPISTEMIC,      /**< Adapt based on epistemic uncertainty */
    ENSEMBLE_PINK_ADAPT_ALEATORIC,      /**< Adapt based on aleatoric uncertainty */
    ENSEMBLE_PINK_ADAPT_COMBINED,       /**< Adapt based on total uncertainty */
    ENSEMBLE_PINK_ADAPT_RATIO           /**< Adapt based on epistemic/aleatoric ratio */
} ensemble_pink_adaptation_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for ensemble-pink noise bridge
 *
 * WHAT: Parameters controlling noise injection and adaptation
 * WHY:  Flexible configuration for different ensemble types and tasks
 * HOW:  Configure injection mode, adaptation strategy, and noise parameters
 */
typedef struct {
    // Noise generation parameters
    float base_amplitude;                   /**< Base noise amplitude (default: 0.05) */
    float base_alpha;                       /**< Base spectral exponent (default: 1.0) */
    float sample_rate;                      /**< Sampling rate Hz (default: 1000.0) */
    pink_noise_method_t noise_method;       /**< Generation method (default: VOSS) */

    // Injection parameters
    ensemble_pink_injection_mode_t injection_mode;  /**< Where to inject noise */
    bool per_model_noise;                   /**< Separate noise generator per model */
    float feature_noise_scale;              /**< Scale for feature noise (0-1) */
    float weight_noise_scale;               /**< Scale for weight noise (0-1) */
    float prediction_noise_scale;           /**< Scale for prediction noise (0-1) */

    // Adaptation parameters
    ensemble_pink_adaptation_t adaptation_mode;     /**< Adaptation strategy */
    float adaptation_rate;                  /**< Learning rate for adaptation */
    float epistemic_target;                 /**< Target epistemic uncertainty */
    float aleatoric_target;                 /**< Target aleatoric uncertainty */
    float amplitude_min;                    /**< Minimum amplitude after adaptation */
    float amplitude_max;                    /**< Maximum amplitude after adaptation */

    // Temporal parameters
    float correlation_time_ms;              /**< Pink noise correlation timescale */
    bool enable_temporal_smoothing;         /**< Smooth noise transitions */
    float smoothing_alpha;                  /**< Exponential smoothing factor */

    // Feature flags
    bool enable_noise_injection;            /**< Master enable for noise */
    bool enable_adaptation;                 /**< Enable adaptive noise parameters */
    bool enable_feedback;                   /**< Enable ensemble→noise feedback */
    bool enable_logging;                    /**< Log noise and uncertainty metrics */

} ensemble_pink_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Current pink noise state for ensemble
 *
 * WHAT: Runtime state of noise generators and parameters
 * WHY:  Track current noise values and adaptation state
 */
typedef struct {
    float current_amplitude;                /**< Current effective amplitude */
    float current_alpha;                    /**< Current effective spectral exponent */
    float* noise_samples;                   /**< Current noise samples per model */
    uint32_t num_noise_samples;             /**< Number of noise samples */
    uint64_t samples_generated;             /**< Total samples generated */
    float temporal_smoothing_state;         /**< State for temporal smoothing */
} ensemble_pink_state_t;

/**
 * @brief Uncertainty metrics from ensemble
 *
 * WHAT: Current uncertainty estimates from ensemble
 * WHY:  Used to adapt noise parameters
 */
typedef struct {
    float epistemic;                        /**< Current epistemic uncertainty */
    float aleatoric;                        /**< Current aleatoric uncertainty */
    float total;                            /**< Current total uncertainty */
    float confidence;                       /**< Current confidence (1 - total) */
    float epistemic_aleatoric_ratio;        /**< Ratio epistemic/aleatoric */
} ensemble_pink_uncertainty_t;

/**
 * @brief Effects computed from uncertainty on noise
 *
 * WHAT: How uncertainty modulates noise parameters
 * WHY:  Adaptive noise based on ensemble state
 */
typedef struct {
    float amplitude_modifier;               /**< Multiplicative amplitude adjustment */
    float alpha_modifier;                   /**< Additive alpha adjustment */
    float effective_amplitude;              /**< Final amplitude after modulation */
    float effective_alpha;                  /**< Final alpha after modulation */
    bool adaptation_triggered;              /**< Whether adaptation occurred */
} ensemble_pink_effects_t;

/**
 * @brief Statistics for monitoring
 *
 * WHAT: Aggregate statistics for bridge operation
 * WHY:  Monitor performance and adaptation behavior
 */
typedef struct {
    uint64_t total_injections;              /**< Total noise injections performed */
    uint64_t total_adaptations;             /**< Total adaptation steps */
    float avg_amplitude;                    /**< Average amplitude over time */
    float avg_alpha;                        /**< Average alpha over time */
    float avg_epistemic;                    /**< Average epistemic uncertainty */
    float avg_aleatoric;                    /**< Average aleatoric uncertainty */
    float min_uncertainty;                  /**< Minimum uncertainty observed */
    float max_uncertainty;                  /**< Maximum uncertainty observed */
    uint64_t update_count;                  /**< Number of updates */
} ensemble_pink_stats_t;

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Ensemble uncertainty pink noise bridge
 *
 * WHAT: Main bridge structure connecting ensemble and pink noise
 * WHY:  Encapsulate state and manage integration
 * HOW:  Store configuration, state, generators, and connections
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    ensemble_pink_config_t config;          /**< Configuration */

    // Module connections
    ensemble_context_t ensemble;            /**< Connected ensemble context */
    pink_noise_generator_t* generators;     /**< Array of noise generators */
    uint32_t num_generators;                /**< Number of generators (1 or N models) */

    // Runtime state
    ensemble_pink_state_t state;            /**< Current noise state */
    ensemble_pink_uncertainty_t uncertainty; /**< Current uncertainty metrics */
    ensemble_pink_effects_t effects;        /**< Computed effects */
    ensemble_pink_stats_t stats;            /**< Statistics */

    // Bio-async integration
    // Thread safety

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
} ensemble_pink_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Get default configuration for ensemble-pink bridge
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Return pre-configured struct with biological parameters
 *
 * DEFAULTS:
 * - base_amplitude: 0.05 (5% noise)
 * - base_alpha: 1.0 (true pink noise)
 * - injection_mode: PREDICTIONS (add to outputs)
 * - adaptation_mode: EPISTEMIC (adapt to model uncertainty)
 * - per_model_noise: true (independent noise per model)
 * - enable_noise_injection: true
 * - enable_adaptation: true
 *
 * @param config Output configuration struct
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int ensemble_pink_default_config(ensemble_pink_config_t* config);

/**
 * WHAT: Create ensemble pink noise bridge
 * WHY:  Initialize bridge for ensemble-noise integration
 * HOW:  Allocate structure, create noise generators, initialize state
 *
 * ALGORITHM:
 * 1. Allocate bridge structure
 * 2. Copy configuration (or use defaults)
 * 3. Create mutex for thread safety
 * 4. Initialize state and statistics
 * 5. Generators created on connect to ensemble
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * ERRORS:
 * - Returns NULL if allocation fails
 * - Returns NULL if mutex creation fails
 *
 * MEMORY: Caller must call ensemble_pink_destroy() when done
 *
 * COMPLEXITY: O(1) (generators created on connect)
 * THREAD-SAFE: Yes
 */
ensemble_pink_bridge_t* ensemble_pink_create(
    const ensemble_pink_config_t* config
);

/**
 * WHAT: Destroy ensemble pink noise bridge
 * WHY:  Free all resources and prevent memory leaks
 * HOW:  Destroy generators, free buffers, destroy mutex
 *
 * @param bridge Bridge to destroy (NULL-safe)
 *
 * SAFETY: Safe to call with NULL
 * SIDE EFFECTS: Disconnects bio-async if connected
 *
 * COMPLEXITY: O(N) where N = num_generators
 * THREAD-SAFE: Yes (caller must ensure no concurrent access)
 */
void ensemble_pink_destroy(ensemble_pink_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * WHAT: Connect bridge to ensemble uncertainty context
 * WHY:  Enable noise injection into ensemble predictions
 * HOW:  Store ensemble reference, create noise generators per model
 *
 * ALGORITHM:
 * 1. Validate bridge and ensemble not NULL
 * 2. Lock mutex
 * 3. Store ensemble reference
 * 4. Query ensemble size (number of models)
 * 5. Create noise generators (1 shared or N per-model)
 * 6. Allocate noise sample buffers
 * 7. Unlock mutex
 *
 * @param bridge Ensemble pink bridge
 * @param ensemble Ensemble uncertainty context
 * @return 0 on success, negative on error
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge or ensemble is NULL
 * - NIMCP_ERROR_OPERATION_FAILED if generator creation fails
 *
 * COMPLEXITY: O(N) where N = num_models (if per_model_noise)
 * THREAD-SAFE: Yes
 */
int ensemble_pink_connect_ensemble(
    ensemble_pink_bridge_t* bridge,
    ensemble_context_t ensemble
);

/**
 * WHAT: Disconnect bridge from ensemble
 * WHY:  Release resources and allow reconnection
 * HOW:  Destroy generators, free buffers, clear reference
 *
 * @param bridge Ensemble pink bridge
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(N) where N = num_generators
 * THREAD-SAFE: Yes
 */
int ensemble_pink_disconnect_ensemble(
    ensemble_pink_bridge_t* bridge
);

/**
 * WHAT: Check if bridge is connected to ensemble
 * WHY:  Verify connection state before operations
 * HOW:  Check ensemble reference is non-NULL
 *
 * @param bridge Ensemble pink bridge
 * @return true if connected, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool ensemble_pink_is_connected(const ensemble_pink_bridge_t* bridge);

//=============================================================================
// Noise Injection Functions
//=============================================================================

/**
 * WHAT: Generate pink noise samples for ensemble models
 * WHY:  Prepare noise for injection into predictions
 * HOW:  Generate samples from each noise generator
 *
 * ALGORITHM:
 * 1. For each noise generator (1 or N):
 *    - Generate single sample
 *    - Apply current amplitude and alpha modulation
 *    - Store in state.noise_samples array
 * 2. Update samples_generated counter
 *
 * @param bridge Ensemble pink bridge
 * @return 0 on success, negative on error
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge is NULL
 * - NIMCP_ERROR_INVALID_STATE if not connected to ensemble
 *
 * COMPLEXITY: O(N) where N = num_generators
 * THREAD-SAFE: Yes
 */
int ensemble_pink_generate_noise(ensemble_pink_bridge_t* bridge);

/**
 * WHAT: Inject pink noise into ensemble predictions
 * WHY:  Add biologically realistic variability to model outputs
 * HOW:  Modulate predictions with generated noise samples
 *
 * ALGORITHM:
 * 1. Generate noise samples if needed
 * 2. For each model prediction:
 *    - Get corresponding noise sample (shared or per-model)
 *    - Apply injection mode (additive, multiplicative, etc.)
 *    - Update prediction in-place
 * 3. Update injection counter
 *
 * @param bridge Ensemble pink bridge
 * @param predictions Array of ensemble predictions (modified in-place)
 * @param num_predictions Number of predictions
 * @return 0 on success, negative on error
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if bridge or predictions is NULL
 * - NIMCP_ERROR_INVALID_STATE if not connected or noise disabled
 *
 * NOTE: Modifies predictions in-place
 *
 * COMPLEXITY: O(N * D) where N = models, D = prediction dimension
 * THREAD-SAFE: Yes
 */
int ensemble_pink_inject_noise(
    ensemble_pink_bridge_t* bridge,
    ensemble_prediction_t* predictions,
    uint32_t num_predictions
);

/**
 * WHAT: Apply pink noise to ensemble features (inputs)
 * WHY:  Data augmentation via biologically realistic input noise
 * HOW:  Add pink noise to feature vectors before ensemble prediction
 *
 * @param bridge Ensemble pink bridge
 * @param features Feature vector (modified in-place)
 * @param num_features Number of features
 * @return 0 on success, negative on error
 *
 * NOTE: Modifies features in-place
 *
 * COMPLEXITY: O(F) where F = num_features
 * THREAD-SAFE: Yes
 */
int ensemble_pink_inject_feature_noise(
    ensemble_pink_bridge_t* bridge,
    float* features,
    uint32_t num_features
);

//=============================================================================
// Adaptation Functions
//=============================================================================

/**
 * WHAT: Update uncertainty metrics from ensemble
 * WHY:  Keep bridge synchronized with ensemble state
 * HOW:  Query ensemble for latest uncertainty estimates
 *
 * @param bridge Ensemble pink bridge
 * @param uncertainty Current uncertainty result from ensemble
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int ensemble_pink_update_uncertainty(
    ensemble_pink_bridge_t* bridge,
    const ensemble_uncertainty_result_t* uncertainty
);

/**
 * WHAT: Compute noise parameter adaptations from uncertainty
 * WHY:  Adapt noise to ensemble state (high uncertainty → more noise)
 * HOW:  Apply adaptation strategy to modulate amplitude/alpha
 *
 * ALGORITHM (EPISTEMIC adaptation):
 * 1. Compute error: e = epistemic - epistemic_target
 * 2. Update amplitude: α += adaptation_rate * e
 * 3. Clamp to [amplitude_min, amplitude_max]
 * 4. Update effects structure
 *
 * ALGORITHM (RATIO adaptation):
 * 1. Compute ratio: r = epistemic / (epistemic + aleatoric)
 * 2. If r > 0.5 (model uncertain): increase noise
 * 3. If r < 0.5 (data noisy): decrease noise
 *
 * @param bridge Ensemble pink bridge
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int ensemble_pink_adapt_noise(ensemble_pink_bridge_t* bridge);

/**
 * WHAT: Full update cycle for bridge
 * WHY:  Convenience function for complete update
 * HOW:  Generate noise, adapt parameters, update statistics
 *
 * TYPICAL USAGE:
 * ```c
 * // In training loop
 * ensemble_uncertainty_result_t unc = ensemble_compute_uncertainty(ens, features, n);
 * ensemble_pink_update_uncertainty(bridge, &unc);
 * ensemble_pink_update(bridge, delta_ms);
 * // Next prediction will use adapted noise
 * ```
 *
 * @param bridge Ensemble pink bridge
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(N) where N = num_generators
 * THREAD-SAFE: Yes
 */
int ensemble_pink_update(
    ensemble_pink_bridge_t* bridge,
    uint64_t delta_ms
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * WHAT: Get current effective noise amplitude
 * WHY:  Monitor current noise level
 * HOW:  Return effective_amplitude from effects
 *
 * @param bridge Ensemble pink bridge
 * @return Current effective amplitude
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float ensemble_pink_get_amplitude(const ensemble_pink_bridge_t* bridge);

/**
 * WHAT: Get current effective spectral exponent
 * WHY:  Monitor current noise spectrum
 * HOW:  Return effective_alpha from effects
 *
 * @param bridge Ensemble pink bridge
 * @return Current effective alpha
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float ensemble_pink_get_alpha(const ensemble_pink_bridge_t* bridge);

/**
 * WHAT: Get current noise samples
 * WHY:  Inspect noise values for debugging
 * HOW:  Return pointer to state.noise_samples
 *
 * @param bridge Ensemble pink bridge
 * @param num_samples Output: number of samples
 * @return Pointer to noise samples array (read-only)
 *
 * NOTE: Returned pointer is valid until next ensemble_pink_generate_noise()
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
const float* ensemble_pink_get_noise_samples(
    const ensemble_pink_bridge_t* bridge,
    uint32_t* num_samples
);

/**
 * WHAT: Get bridge statistics
 * WHY:  Monitor performance and adaptation behavior
 * HOW:  Copy stats structure
 *
 * @param bridge Ensemble pink bridge
 * @param stats Output statistics
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int ensemble_pink_get_stats(
    const ensemble_pink_bridge_t* bridge,
    ensemble_pink_stats_t* stats
);

/**
 * WHAT: Get current uncertainty metrics
 * WHY:  Access last updated uncertainty values
 * HOW:  Copy uncertainty structure
 *
 * @param bridge Ensemble pink bridge
 * @param uncertainty Output uncertainty metrics
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int ensemble_pink_get_uncertainty(
    const ensemble_pink_bridge_t* bridge,
    ensemble_pink_uncertainty_t* uncertainty
);

//=============================================================================
// Control Functions
//=============================================================================

/**
 * WHAT: Enable or disable noise injection
 * WHY:  Toggle noise on/off without destroying bridge
 * HOW:  Set config.enable_noise_injection flag
 *
 * @param bridge Ensemble pink bridge
 * @param enable true to enable, false to disable
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int ensemble_pink_set_enabled(
    ensemble_pink_bridge_t* bridge,
    bool enable
);

/**
 * WHAT: Enable or disable adaptation
 * WHY:  Toggle adaptive noise on/off
 * HOW:  Set config.enable_adaptation flag
 *
 * @param bridge Ensemble pink bridge
 * @param enable true to enable, false to disable
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int ensemble_pink_set_adaptation_enabled(
    ensemble_pink_bridge_t* bridge,
    bool enable
);

/**
 * WHAT: Reset bridge state
 * WHY:  Clear statistics and reset to initial state
 * HOW:  Reset generators, clear stats, reinitialize state
 *
 * @param bridge Ensemble pink bridge
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(N) where N = num_generators
 * THREAD-SAFE: Yes
 */
int ensemble_pink_reset(ensemble_pink_bridge_t* bridge);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Connect bridge to bio-async router
 * WHY:  Enable inter-module messaging for ensemble-noise integration
 * HOW:  Register with bio-async router
 *
 * @param bridge Ensemble pink bridge
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int ensemble_pink_connect_bio_async(ensemble_pink_bridge_t* bridge);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Cleanup bio-async resources
 * HOW:  Unregister from router
 *
 * @param bridge Ensemble pink bridge
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int ensemble_pink_disconnect_bio_async(ensemble_pink_bridge_t* bridge);

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Verify bio-async connection state
 * HOW:  Return bio_async_enabled flag
 *
 * @param bridge Ensemble pink bridge
 * @return true if connected, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool ensemble_pink_is_bio_async_connected(
    const ensemble_pink_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENSEMBLE_UNCERTAINTY_PINK_NOISE_BRIDGE_H */
