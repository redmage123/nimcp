/**
 * @file nimcp_social_fep_bridge.h
 * @brief Social Cognition - FEP Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bidirectional bridge between social cognition and FEP orchestrator
 * WHY:  Enable free energy minimization across social processing, tracking
 *       prediction errors from social predictions, relationship uncertainty,
 *       and social norm violations
 * HOW:  Register with FEP orchestrator, compute free energy from social metrics,
 *       update prediction errors based on social behavior outcomes
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free energy principle for social cognition
 * - Kube et al. (2020): Social predictive processing
 * - Kilner et al. (2007): Predictive coding in social interaction
 *
 * BIOLOGICAL BASIS:
 * - Temporoparietal junction: Social prediction and mentalizing
 * - Medial prefrontal cortex: Social value and expectation
 * - Superior temporal sulcus: Biological motion and intention
 * - Amygdala: Social threat and reward prediction
 *
 * FEP INTEGRATION MODEL:
 * - Social cognition predicts others' behavior
 * - Social prediction error = mismatch between expected and actual behavior
 * - Relationship uncertainty = unpredictability of social bonds
 * - Social norm violations are high-surprise events (high free energy)
 * - Stable social relationships minimize social free energy
 *
 * @see nimcp_love_loyalty_friendship.h
 * @see nimcp_fep_orchestrator.h
 */

#ifndef NIMCP_SOCIAL_FEP_BRIDGE_H
#define NIMCP_SOCIAL_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Include the social module header for social_bond_system_t */
#include "cognitive/nimcp_love_loyalty_friendship.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

/** @brief FEP orchestrator handle */
typedef struct fep_orchestrator fep_orchestrator_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Free energy baseline for idle social state */
#define SOCIAL_FEP_BASELINE_FREE_ENERGY   0.1f

/** @brief Maximum free energy ceiling */
#define SOCIAL_FEP_MAX_FREE_ENERGY        2.0f

/** @brief Prediction error decay rate per update cycle */
#define SOCIAL_FEP_ERROR_DECAY_RATE       0.95f

/** @brief Social prediction error weight */
#define SOCIAL_FEP_PREDICTION_WEIGHT      0.4f

/** @brief Relationship uncertainty weight */
#define SOCIAL_FEP_UNCERTAINTY_WEIGHT     0.3f

/** @brief Social norm violation weight */
#define SOCIAL_FEP_NORM_WEIGHT            0.3f

/** @brief Bio-async module ID for social FEP bridge */
#define BIO_MODULE_SOCIAL_FEP             0x0D72

/** @brief Default update interval in milliseconds */
#define SOCIAL_FEP_DEFAULT_UPDATE_MS      50

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Bridge operational state
 */
typedef enum {
    SOCIAL_FEP_STATE_UNINITIALIZED = 0,  /**< Not yet initialized */
    SOCIAL_FEP_STATE_IDLE,               /**< Ready, no active processing */
    SOCIAL_FEP_STATE_ACTIVE,             /**< Processing social data, updating FEP */
    SOCIAL_FEP_STATE_DEGRADED,           /**< High free energy, reduced capacity */
    SOCIAL_FEP_STATE_ERROR               /**< Error state */
} social_fep_state_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief FEP bridge configuration
 *
 * WHAT: Configuration parameters for social FEP bridge
 * WHY:  Allow tuning of FEP computation for social cognition
 * HOW:  Weights, thresholds, and behavioral flags
 */
typedef struct {
    /* Behavior */
    bool enable_logging;                 /**< Enable debug logging */

    /* Timing */
    uint32_t update_interval_ms;         /**< Update interval in milliseconds */

    /* Weighting parameters */
    float free_energy_weight;            /**< Overall free energy contribution weight */
    float social_prediction_error_weight;/**< Weight for social prediction error */
    float relationship_uncertainty_weight;/**< Weight for relationship uncertainty */
    float norm_violation_weight;         /**< Weight for social norm violations */

    /* Thresholds */
    float high_free_energy_threshold;    /**< Threshold for degraded mode */
    float prediction_error_threshold;    /**< Threshold for surprise trigger */
    float uncertainty_threshold;         /**< Threshold for relationship uncertainty alert */

    /* Normalization */
    float baseline_free_energy;          /**< Baseline free energy (idle) */
    float max_free_energy;               /**< Maximum free energy ceiling */
    float error_decay_rate;              /**< Prediction error decay per cycle */

    /* Behavior flags */
    bool enable_adaptive_weights;        /**< Adjust weights based on state */
    bool enable_degraded_mode;           /**< Enable degraded mode on high FE */
    bool enable_surprise_callbacks;      /**< Trigger callbacks on surprise */
} social_fep_config_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 *
 * WHAT: Tracks performance and activity of social FEP bridge
 * WHY:  Monitoring and debugging of FEP integration
 * HOW:  Accumulated counters and averages
 */
typedef struct {
    uint64_t total_updates;              /**< Total FEP update cycles */
    uint64_t social_predictions;         /**< Total social predictions processed */
    uint64_t relationship_updates;       /**< Relationship state updates */
    uint64_t norm_violation_events;      /**< Social norm violations detected */
    uint64_t surprise_events;            /**< High-surprise events */
    uint64_t degraded_mode_entries;      /**< Times entered degraded mode */
    float avg_update_time_us;            /**< Average update duration (microseconds) */
    uint64_t total_update_time_us;       /**< Cumulative update time */
    float total_free_energy_contribution;/**< Cumulative free energy contribution */
    float peak_free_energy;              /**< Peak free energy observed */
    float avg_free_energy;               /**< Running average free energy */
    float avg_prediction_error;          /**< Running average prediction error */
} social_fep_stats_t;

/*=============================================================================
 * METRICS
 *===========================================================================*/

/**
 * @brief FEP metrics for social cognition
 *
 * WHAT: Tracks free energy and prediction error from social processing
 * WHY:  FEP orchestrator uses these to coordinate updates and detect anomalies
 * HOW:  Updated during FEP update cycle from social module statistics
 */
typedef struct {
    /* Core FEP metrics */
    float free_energy;                   /**< Current free energy estimate [0, MAX] */
    float prediction_error;              /**< Accumulated prediction error [0, 1] */
    float surprise;                      /**< Bayesian surprise from unexpected events */
    float entropy;                       /**< State uncertainty measure */

    /* Social-specific metrics */
    float social_prediction_error;       /**< Error in predicting others' behavior */
    float relationship_uncertainty;      /**< Unpredictability of social bonds */
    float norm_violation_surprise;       /**< Surprise from social norm violations */
    float trust_prediction_error;        /**< Error in trust predictions */
    float cooperation_prediction_error;  /**< Error in cooperation predictions */

    /* Relationship metrics */
    float avg_relationship_closeness;    /**< Average closeness across relationships */
    float avg_relationship_trust;        /**< Average trust across relationships */
    uint32_t active_relationships;       /**< Number of active relationships */
    uint32_t close_friends_count;        /**< Number of close friendships */

    /* Social state */
    float loneliness;                    /**< Current loneliness level */
    float oxytocin_level;                /**< Current oxytocin level */
    float social_warmth;                 /**< Current social warmth feeling */

    /* Timing */
    uint64_t last_update_time_ms;        /**< Last FEP update timestamp */
    uint32_t update_count;               /**< Total FEP updates performed */
} social_fep_metrics_t;

/*=============================================================================
 * OPAQUE BRIDGE HANDLE
 *===========================================================================*/

typedef struct social_fep_bridge social_fep_bridge_t;

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
typedef void (*social_fep_high_fe_callback_t)(
    social_fep_bridge_t* bridge,
    float free_energy,
    void* user_data
);

/**
 * @brief Surprise event callback
 *
 * Called when prediction error causes significant surprise
 *
 * @param bridge Bridge handle
 * @param surprise Surprise magnitude
 * @param source Source of surprise ("social_prediction", "relationship", "norm_violation")
 * @param user_data User context
 */
typedef void (*social_fep_surprise_callback_t)(
    social_fep_bridge_t* bridge,
    float surprise,
    const char* source,
    void* user_data
);

/**
 * @brief Metrics update callback
 *
 * Called after each FEP update cycle
 *
 * @param bridge Bridge handle
 * @param metrics Updated metrics
 * @param user_data User context
 */
typedef void (*social_fep_metrics_callback_t)(
    social_fep_bridge_t* bridge,
    const social_fep_metrics_t* metrics,
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
social_fep_config_t social_fep_config_default(void);

/**
 * @brief Create FEP bridge for social cognition
 *
 * WHAT: Initialize FEP bridge infrastructure
 * WHY:  Enable FEP orchestrator integration for social module
 * HOW:  Allocate state, configure parameters, prepare for registration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
social_fep_bridge_t* social_fep_bridge_create(const social_fep_config_t* config);

/**
 * @brief Destroy FEP bridge
 *
 * WHAT: Clean up FEP bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister from orchestrator if registered, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void social_fep_bridge_destroy(social_fep_bridge_t* bridge);

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
int social_fep_bridge_reset(social_fep_bridge_t* bridge);

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Register bridge with FEP orchestrator
 *
 * WHAT: Add social cognition to FEP coordination
 * WHY:  Enable system-wide free energy minimization
 * HOW:  Register update callback with orchestrator at cognitive timescale (50ms)
 *
 * @param bridge Bridge handle
 * @param orchestrator FEP orchestrator instance
 * @param social Social bond system
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int social_fep_bridge_register(
    social_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    social_bond_system_t* social,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister bridge from FEP orchestrator
 *
 * WHAT: Remove social cognition from FEP coordination
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unregister from orchestrator, clear references
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int social_fep_bridge_unregister(social_fep_bridge_t* bridge);

/**
 * @brief Check if bridge is registered
 *
 * @param bridge Bridge handle
 * @return true if registered with orchestrator
 */
bool social_fep_bridge_is_registered(social_fep_bridge_t* bridge);

/**
 * @brief Get the FEP bridge ID assigned by orchestrator
 *
 * @param bridge Bridge handle
 * @return Bridge ID, or 0 if not registered
 */
uint32_t social_fep_bridge_get_id(social_fep_bridge_t* bridge);

/*=============================================================================
 * FEP UPDATE FUNCTIONS
 *===========================================================================*/

/**
 * @brief FEP update callback for social cognition
 *
 * WHAT: Called by FEP orchestrator during cognitive update cycle (50ms)
 * WHY:  Compute free energy from social metrics, update predictions
 * HOW:  Query social stats, compute FE components, update metrics
 *
 * FREE ENERGY COMPUTATION:
 * - Social prediction error: mismatch between expected and actual behavior
 * - Relationship uncertainty: unpredictability of social bonds
 * - Norm violations: unexpected social behavior (high surprise)
 * - Total FE = baseline + weighted sum of contributions
 *
 * @param handle Opaque handle (social_fep_bridge_t*)
 * @return 0 on success, -1 on error
 */
int social_fep_update_callback(void* handle);

/**
 * @brief FEP destroy callback for cleanup
 *
 * WHAT: Called by FEP orchestrator when unregistering
 * WHY:  Allow bridge-specific cleanup if needed
 * HOW:  Currently no-op (bridge destroyed separately)
 *
 * @param handle Opaque handle (social_fep_bridge_t*)
 */
void social_fep_destroy_callback(void* handle);

/**
 * @brief Manual update trigger
 *
 * WHAT: Trigger FEP update outside normal cycle
 * WHY:  Testing or event-driven updates
 * HOW:  Call update logic directly
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int social_fep_bridge_update(social_fep_bridge_t* bridge);

/**
 * @brief Force update (for testing/debugging)
 *
 * WHAT: Force FEP update with minimal metrics
 * WHY:  Testing without full social system
 * HOW:  Update counters and invoke callbacks
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int social_fep_bridge_force_update(social_fep_bridge_t* bridge);

/*=============================================================================
 * METRICS AND STATISTICS
 *===========================================================================*/

/**
 * @brief Get current FEP metrics
 *
 * @param bridge Bridge handle
 * @param metrics_out Output: current metrics
 * @return 0 on success, -1 on error
 */
int social_fep_bridge_get_metrics(
    const social_fep_bridge_t* bridge,
    social_fep_metrics_t* metrics_out
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output: statistics
 * @return 0 on success, -1 on error
 */
int social_fep_bridge_get_stats(
    const social_fep_bridge_t* bridge,
    social_fep_stats_t* stats_out
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int social_fep_bridge_reset_stats(social_fep_bridge_t* bridge);

/*=============================================================================
 * FREE ENERGY ACCESSORS
 *===========================================================================*/

/**
 * @brief Get current free energy contribution
 *
 * @param bridge Bridge handle
 * @return Current free energy, -1.0f on error
 */
float social_fep_bridge_get_free_energy_contribution(social_fep_bridge_t* bridge);

/**
 * @brief Get current social prediction error
 *
 * @param bridge Bridge handle
 * @return Current social prediction error [0, 1], -1.0f on error
 */
float social_fep_bridge_get_social_prediction_error(social_fep_bridge_t* bridge);

/**
 * @brief Get current relationship uncertainty
 *
 * @param bridge Bridge handle
 * @return Current relationship uncertainty [0, 1], -1.0f on error
 */
float social_fep_bridge_get_relationship_uncertainty(social_fep_bridge_t* bridge);

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
social_fep_state_t social_fep_bridge_get_state(social_fep_bridge_t* bridge);

/**
 * @brief Check if in degraded mode
 *
 * @param bridge Bridge handle
 * @return true if free energy is above threshold
 */
bool social_fep_bridge_is_degraded(social_fep_bridge_t* bridge);

/**
 * @brief Get state name as string
 *
 * @param state Bridge state
 * @return Human-readable state name
 */
const char* social_fep_state_name(social_fep_state_t state);

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
int social_fep_bridge_set_high_fe_callback(
    social_fep_bridge_t* bridge,
    social_fep_high_fe_callback_t callback,
    void* user_data
);

/**
 * @brief Register surprise event callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on failure
 */
int social_fep_bridge_set_surprise_callback(
    social_fep_bridge_t* bridge,
    social_fep_surprise_callback_t callback,
    void* user_data
);

/**
 * @brief Register metrics update callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on failure
 */
int social_fep_bridge_set_metrics_callback(
    social_fep_bridge_t* bridge,
    social_fep_metrics_callback_t callback,
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
int social_fep_bridge_set_config(
    social_fep_bridge_t* bridge,
    const social_fep_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config_out Output: current configuration
 * @return 0 on success, -1 on failure
 */
int social_fep_bridge_get_config(
    const social_fep_bridge_t* bridge,
    social_fep_config_t* config_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOCIAL_FEP_BRIDGE_H */
