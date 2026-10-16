/**
 * @file nimcp_jepa_fep_bridge.h
 * @brief JEPA - FEP Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bidirectional bridge between JEPA predictor and FEP orchestrator
 * WHY:  Enable free energy minimization through embedding prediction error,
 *       representation quality, and collapse penalty tracking
 * HOW:  Register with FEP orchestrator, compute free energy from JEPA metrics,
 *       update prediction errors based on latent space prediction quality
 *
 * THEORETICAL FOUNDATIONS:
 * - LeCun (2022): JEPA - Predicting in latent space, not pixel space
 * - Friston (2010): Free energy principle for hierarchical cognition
 * - Bardes et al. (2024): V-JEPA 2 representation learning
 *
 * BIOLOGICAL BASIS:
 * - Predictive coding in cortical hierarchies
 * - Embedding prediction error = mismatch in latent representations
 * - Representation collapse = degenerate solution avoidance
 * - Good predictive embeddings reduce free energy (prediction error)
 *
 * FEP INTEGRATION MODEL:
 * - Embedding prediction error contributes to free energy
 * - Representation collapse (low diversity) = high free energy penalty
 * - Successful predictions reduce free energy
 * - Precision tracks confidence in embedding predictions
 *
 * @see nimcp_jepa_predictor.h
 * @see nimcp_fep_orchestrator.h
 */

#ifndef NIMCP_JEPA_FEP_BRIDGE_H
#define NIMCP_JEPA_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

/** @brief FEP orchestrator handle */
typedef struct fep_orchestrator fep_orchestrator_t;

/** @brief JEPA predictor handle */
typedef struct jepa_predictor jepa_predictor_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Maximum embedding prediction error for normalization */
#define JEPA_FEP_MAX_PRED_ERROR         2.0f

/** @brief Free energy baseline for idle state */
#define JEPA_FEP_BASELINE_FREE_ENERGY   0.05f

/** @brief Maximum free energy ceiling */
#define JEPA_FEP_MAX_FREE_ENERGY        2.0f

/** @brief Default collapse penalty weight */
#define JEPA_FEP_COLLAPSE_PENALTY_WEIGHT 0.5f

/** @brief Default embedding prediction error weight */
#define JEPA_FEP_EMBEDDING_ERROR_WEIGHT  0.6f

/** @brief Default representation quality weight */
#define JEPA_FEP_REPRESENTATION_WEIGHT   0.4f

/** @brief Default update interval (ms) */
#define JEPA_FEP_DEFAULT_UPDATE_MS       25

/** @brief Bio-async module ID for JEPA FEP bridge */
#define BIO_MODULE_JEPA_FEP              0x0E60

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Bridge operational state
 */
typedef enum {
    JEPA_FEP_STATE_UNINITIALIZED = 0,  /**< Not yet initialized */
    JEPA_FEP_STATE_IDLE,               /**< Ready, no active processing */
    JEPA_FEP_STATE_ACTIVE,             /**< Processing embeddings, updating FEP */
    JEPA_FEP_STATE_DEGRADED,           /**< High free energy, reduced capacity */
    JEPA_FEP_STATE_ERROR               /**< Error state */
} jepa_fep_state_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP bridge configuration for JEPA
 */
typedef struct {
    /* Weighting parameters */
    float free_energy_weight;            /**< Overall FE contribution weight [0,1] */
    float embedding_prediction_error_weight; /**< Weight for embedding error */
    float representation_collapse_penalty;   /**< Weight for collapse penalty */

    /* Thresholds */
    float high_free_energy_threshold;    /**< Threshold for degraded mode */
    float collapse_detection_threshold;  /**< Threshold for detecting collapse */
    float prediction_quality_threshold;  /**< Min quality for "good" prediction */

    /* Behavior */
    bool enable_logging;                 /**< Enable debug logging */
    uint32_t update_interval_ms;         /**< Update interval in milliseconds */
    bool enable_adaptive_weights;        /**< Adjust weights based on state */
    bool enable_collapse_detection;      /**< Enable representation collapse detection */
} jepa_fep_config_t;

/*=============================================================================
 * STATISTICS STRUCTURES
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;              /**< Total FEP update cycles */
    uint64_t embedding_predictions;      /**< Total embedding predictions tracked */
    uint64_t representation_updates;     /**< Representation quality updates */
    uint64_t collapse_detections;        /**< Number of collapse events detected */
    float avg_update_time_us;            /**< Average update duration */
    float total_free_energy_contribution;/**< Cumulative FE contribution */
    float peak_free_energy;              /**< Peak free energy observed */
    float avg_embedding_error;           /**< Average embedding prediction error */
    float min_representation_quality;    /**< Minimum representation quality seen */
} jepa_fep_stats_t;

/*=============================================================================
 * OPAQUE BRIDGE HANDLE
 *===========================================================================*/

typedef struct jepa_fep_bridge jepa_fep_bridge_t;

/*=============================================================================
 * CALLBACKS
 *===========================================================================*/

/**
 * @brief High free energy callback
 *
 * Called when free energy exceeds threshold (entering degraded mode)
 *
 * @param bridge Bridge handle
 * @param free_energy Current free energy level
 * @param user_data User context
 */
typedef void (*jepa_fep_high_fe_callback_t)(
    jepa_fep_bridge_t* bridge,
    float free_energy,
    void* user_data
);

/**
 * @brief Representation collapse callback
 *
 * Called when representation collapse is detected
 *
 * @param bridge Bridge handle
 * @param collapse_severity Severity of collapse [0,1]
 * @param user_data User context
 */
typedef void (*jepa_fep_collapse_callback_t)(
    jepa_fep_bridge_t* bridge,
    float collapse_severity,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible parameters
 * HOW:  Set weights, thresholds, and behavior flags
 *
 * @return Default configuration structure
 */
jepa_fep_config_t jepa_fep_config_default(void);

/**
 * @brief Create FEP bridge for JEPA
 *
 * WHAT: Initialize FEP bridge infrastructure
 * WHY:  Enable FEP orchestrator integration for JEPA predictor
 * HOW:  Allocate state, configure parameters, prepare for registration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
jepa_fep_bridge_t* jepa_fep_bridge_create(const jepa_fep_config_t* config);

/**
 * @brief Destroy FEP bridge
 *
 * WHAT: Clean up FEP bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister from orchestrator if registered, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void jepa_fep_bridge_destroy(jepa_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset all metrics and state to initial values
 * WHY:  Recovery from error state or fresh start
 * HOW:  Clear metrics, reset counters, return to idle
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int jepa_fep_bridge_reset(jepa_fep_bridge_t* bridge);

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Register bridge with FEP orchestrator
 *
 * WHAT: Add JEPA to FEP coordination
 * WHY:  Enable system-wide free energy minimization
 * HOW:  Register update callback with orchestrator at JEPA timescale (25ms)
 *
 * @param bridge Bridge handle
 * @param orchestrator FEP orchestrator instance
 * @param predictor JEPA predictor instance
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int jepa_fep_bridge_register(
    jepa_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    jepa_predictor_t* predictor,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister bridge from FEP orchestrator
 *
 * WHAT: Remove JEPA from FEP coordination
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unregister from orchestrator, clear references
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int jepa_fep_bridge_unregister(jepa_fep_bridge_t* bridge);

/**
 * @brief Check if bridge is registered
 *
 * @param bridge Bridge handle
 * @return true if registered with orchestrator
 */
bool jepa_fep_bridge_is_registered(jepa_fep_bridge_t* bridge);

/**
 * @brief Get the FEP bridge ID assigned by orchestrator
 *
 * @param bridge Bridge handle
 * @return Bridge ID, or 0 if not registered
 */
uint32_t jepa_fep_bridge_get_id(jepa_fep_bridge_t* bridge);

/*=============================================================================
 * FEP UPDATE CALLBACK (Internal - used by FEP orchestrator)
 *===========================================================================*/

/**
 * @brief FEP update callback for JEPA
 *
 * WHAT: Called by FEP orchestrator during JEPA update cycle (25ms)
 * WHY:  Compute free energy from JEPA metrics, update predictions
 * HOW:  Query predictor stats, compute FE components, update metrics
 *
 * FREE ENERGY COMPUTATION:
 * - Embedding error contribution: prediction_error * embedding_weight
 * - Representation quality contribution: (1 - quality) * representation_weight
 * - Collapse penalty: collapse_severity * collapse_weight
 * - Total FE = baseline + sum of contributions
 *
 * @param handle Opaque handle (jepa_fep_bridge_t*)
 * @return 0 on success, -1 on error
 */
int jepa_fep_update_callback(void* handle);

/**
 * @brief FEP destroy callback for cleanup
 *
 * WHAT: Called by FEP orchestrator when unregistering
 * WHY:  Allow bridge-specific cleanup if needed
 * HOW:  Currently no-op (bridge destroyed separately)
 *
 * @param handle Opaque handle (jepa_fep_bridge_t*)
 */
void jepa_fep_destroy_callback(void* handle);

/*=============================================================================
 * MANUAL UPDATE OPERATIONS
 *===========================================================================*/

/**
 * @brief Manual update (for testing/debugging)
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int jepa_fep_bridge_update(jepa_fep_bridge_t* bridge);

/**
 * @brief Force an FEP update (for testing/debugging)
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int jepa_fep_bridge_force_update(jepa_fep_bridge_t* bridge);

/*=============================================================================
 * STATISTICS AND ACCESSORS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output: statistics
 * @return 0 on success, -1 on error
 */
int jepa_fep_bridge_get_stats(
    const jepa_fep_bridge_t* bridge,
    jepa_fep_stats_t* stats_out
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int jepa_fep_bridge_reset_stats(jepa_fep_bridge_t* bridge);

/**
 * @brief Get current free energy contribution
 *
 * @param bridge Bridge handle
 * @return Current free energy contribution, -1.0f on error
 */
float jepa_fep_bridge_get_free_energy_contribution(jepa_fep_bridge_t* bridge);

/**
 * @brief Get current embedding prediction error
 *
 * @param bridge Bridge handle
 * @return Current embedding prediction error, -1.0f on error
 */
float jepa_fep_bridge_get_embedding_prediction_error(jepa_fep_bridge_t* bridge);

/**
 * @brief Get current representation quality
 *
 * WHAT: Measure of how diverse/non-collapsed the representations are
 * WHY:  Representation collapse is a failure mode of self-supervised learning
 * HOW:  High quality (1.0) = diverse embeddings, low (0.0) = collapsed
 *
 * @param bridge Bridge handle
 * @return Representation quality [0,1], -1.0f on error
 */
float jepa_fep_bridge_get_representation_quality(jepa_fep_bridge_t* bridge);

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
jepa_fep_state_t jepa_fep_bridge_get_state(jepa_fep_bridge_t* bridge);

/**
 * @brief Check if in degraded mode
 *
 * @param bridge Bridge handle
 * @return true if free energy is above threshold
 */
bool jepa_fep_bridge_is_degraded(jepa_fep_bridge_t* bridge);

/*=============================================================================
 * INPUT FUNCTIONS
 *===========================================================================*/

/**
 * @brief Record an embedding prediction error
 *
 * WHAT: Input a new embedding prediction error measurement
 * WHY:  Track prediction errors for FEP integration
 * HOW:  Update running statistics and free energy contribution
 *
 * @param bridge Bridge handle
 * @param prediction_error Prediction error magnitude [0, inf)
 * @return 0 on success, -1 on error
 */
int jepa_fep_bridge_record_prediction_error(
    jepa_fep_bridge_t* bridge,
    float prediction_error
);

/**
 * @brief Record representation quality measurement
 *
 * WHAT: Input a new representation quality measurement
 * WHY:  Track representation diversity for collapse detection
 * HOW:  Update running statistics and collapse detection
 *
 * @param bridge Bridge handle
 * @param quality Representation quality [0,1]
 * @return 0 on success, -1 on error
 */
int jepa_fep_bridge_record_representation_quality(
    jepa_fep_bridge_t* bridge,
    float quality
);

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

/**
 * @brief Register high free energy callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on failure
 */
int jepa_fep_bridge_set_high_fe_callback(
    jepa_fep_bridge_t* bridge,
    jepa_fep_high_fe_callback_t callback,
    void* user_data
);

/**
 * @brief Register collapse detection callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on failure
 */
int jepa_fep_bridge_set_collapse_callback(
    jepa_fep_bridge_t* bridge,
    jepa_fep_collapse_callback_t callback,
    void* user_data
);

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Update bridge configuration
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on failure
 */
int jepa_fep_bridge_set_config(
    jepa_fep_bridge_t* bridge,
    const jepa_fep_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config_out Output: current configuration
 * @return 0 on success, -1 on failure
 */
int jepa_fep_bridge_get_config(
    const jepa_fep_bridge_t* bridge,
    jepa_fep_config_t* config_out
);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get state name as string
 *
 * @param state Bridge state
 * @return Human-readable state name
 */
const char* jepa_fep_state_name(jepa_fep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_FEP_BRIDGE_H */
