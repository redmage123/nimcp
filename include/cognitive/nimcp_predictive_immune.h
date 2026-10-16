/**
 * @file nimcp_predictive_immune.h
 * @brief Predictive Processing - Brain Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Integration layer connecting predictive coding and brain immune system
 * WHY:  Model interoceptive prediction of immune state and immune modulation of
 *       predictive processing
 * HOW:  Bidirectional coupling:
 *       - Immune → Predictive: Inflammation increases prediction error tolerance
 *       - Predictive → Immune: Large prediction errors trigger immune detection
 *
 * BIOLOGICAL BASIS:
 * ```
 * PREDICTIVE CODING FRAMEWORK INCLUDES INTEROCEPTION:
 *
 * 1. Interoceptive Prediction:
 *    - Brain predicts internal bodily states (immune state, inflammation)
 *    - Prediction errors drive sickness behavior and homeostatic regulation
 *    - Active inference: actions selected to minimize predicted immune threats
 *
 * 2. Immune Modulation of Prediction:
 *    - Inflammation reduces precision of predictions (increased uncertainty)
 *    - Cytokines (IL-1β, IL-6, TNFα) modulate neural gain and error sensitivity
 *    - Adaptive: during infection, focus on survival not precise prediction
 *
 * 3. Prediction Errors as Immune Triggers:
 *    - Large prediction errors may indicate corrupted neural processing
 *    - Persistent errors suggest potential threat (adversarial input, corruption)
 *    - Free energy spikes trigger immune surveillance
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                PREDICTIVE-IMMUNE INTEGRATION                          ║
 * ╠═══════════════════════════════════════════════════════════════════════╣
 * ║                                                                        ║
 * ║   ┌────────────────────────────────────────────────────────────────┐  ║
 * ║   │           INTEROCEPTIVE PREDICTION OF IMMUNE STATE             │  ║
 * ║   │   ┌────────────┐        ┌──────────────┐       ┌────────────┐ │  ║
 * ║   │   │  Predict   │  -->   │  Prediction  │  -->  │  Sickness  │ │  ║
 * ║   │   │ Cytokine   │        │    Error     │       │  Behavior  │ │  ║
 * ║   │   │   State    │        │              │       │  Response  │ │  ║
 * ║   │   └────────────┘        └──────────────┘       └────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────┘  ║
 * ║                                 ↕                                     ║
 * ║   ┌────────────────────────────────────────────────────────────────┐  ║
 * ║   │         IMMUNE MODULATION OF PREDICTION PRECISION              │  ║
 * ║   │   ┌────────────┐        ┌──────────────┐       ┌────────────┐ │  ║
 * ║   │   │Inflammation│  -->   │   Reduce     │  -->  │  Increase  │ │  ║
 * ║   │   │   Level    │        │  Precision   │       │   Error    │ │  ║
 * ║   │   │            │        │   Weights    │       │ Tolerance  │ │  ║
 * ║   │   └────────────┘        └──────────────┘       └────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────┘  ║
 * ║                                 ↕                                     ║
 * ║   ┌────────────────────────────────────────────────────────────────┐  ║
 * ║   │        PREDICTION ERROR AS IMMUNE THREAT INDICATOR             │  ║
 * ║   │   ┌────────────┐        ┌──────────────┐       ┌────────────┐ │  ║
 * ║   │   │   Large    │  -->   │   Antigen    │  -->  │   Immune   │ │  ║
 * ║   │   │Prediction  │        │ Presentation │       │  Response  │ │  ║
 * ║   │   │   Error    │        │              │       │            │ │  ║
 * ║   │   └────────────┘        └──────────────┘       └────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                        ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Mediator: Coordinates predictive and immune modules
 * - Observer: Callbacks for cross-module events
 * - Strategy: Pluggable prediction-immune coupling strategies
 *
 * REFERENCES:
 * - Friston (2012): "Embodied Inference and the Immune System"
 * - Barrett & Simmons (2015): "Interoceptive predictions in the brain"
 * - Stephan et al. (2016): "Allostatic Self-efficacy"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PREDICTIVE_IMMUNE_H
#define NIMCP_PREDICTIVE_IMMUNE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Core dependencies */
#include "cognitive/nimcp_predictive.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain_regions/nimcp_brain_region_predictive.h"
#include "async/nimcp_bio_messages.h"
#include "utils/validation/nimcp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PREDICTIVE_IMMUNE_MAX_INTEROCEPTIVE_DIMS  32   /**< Max interoceptive state dims */
#define PREDICTIVE_IMMUNE_BASELINE_PRECISION      1.0f /**< Baseline precision */
#define PREDICTIVE_IMMUNE_MIN_PRECISION          0.1f  /**< Min precision during inflammation */
#define PREDICTIVE_IMMUNE_ERROR_THRESHOLD        3.0f  /**< Prediction error threshold for immune alert */
#define PREDICTIVE_IMMUNE_FREE_ENERGY_THRESHOLD  10.0f /**< Free energy threshold for threat detection */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Interoceptive prediction modes
 *
 * WHAT: Different strategies for predicting internal immune state
 * WHY:  Different prediction granularities based on need
 */
typedef enum {
    INTERO_PREDICT_NONE = 0,        /**< No interoceptive prediction */
    INTERO_PREDICT_INFLAMMATION,    /**< Predict inflammation level only */
    INTERO_PREDICT_CYTOKINES,       /**< Predict cytokine concentrations */
    INTERO_PREDICT_FULL_STATE       /**< Predict full immune state */
} interoceptive_prediction_mode_t;

/**
 * @brief Immune modulation strategy
 *
 * WHAT: How inflammation affects predictive processing
 * WHY:  Different coupling strengths between systems
 */
typedef enum {
    IMMUNE_MOD_NONE = 0,           /**< No modulation */
    IMMUNE_MOD_PRECISION_ONLY,     /**< Only reduce precision weights */
    IMMUNE_MOD_LEARNING_RATE,      /**< Modulate learning rate */
    IMMUNE_MOD_FULL                /**< Full coupling (precision + LR + convergence) */
} immune_modulation_strategy_t;

/**
 * @brief Prediction error response mode
 *
 * WHAT: How prediction errors trigger immune responses
 * WHY:  Control sensitivity to prediction anomalies
 */
typedef enum {
    PRED_ERROR_RESPONSE_NONE = 0,     /**< Don't trigger immune on errors */
    PRED_ERROR_RESPONSE_THRESHOLD,    /**< Trigger on threshold crossing */
    PRED_ERROR_RESPONSE_CUMULATIVE,   /**< Trigger on cumulative error */
    PRED_ERROR_RESPONSE_ADAPTIVE      /**< Adaptive threshold based on history */
} prediction_error_response_mode_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Predictive-immune integration configuration
 */
typedef struct {
    /* Interoceptive prediction */
    interoceptive_prediction_mode_t intero_mode;  /**< Prediction mode */
    uint32_t intero_state_dim;                    /**< Interoceptive state dimensionality */
    float intero_learning_rate;                   /**< Learning rate for intero predictions */
    bool enable_sickness_behavior;                /**< Enable sickness behavior on errors */

    /* Immune modulation of prediction */
    immune_modulation_strategy_t modulation_strategy; /**< How immune affects prediction */
    float precision_reduction_factor;             /**< Precision reduction per inflammation level */
    float learning_rate_reduction_factor;         /**< LR reduction during inflammation */
    float max_precision_reduction;                /**< Max precision reduction (0-1) */

    /* Prediction error detection */
    prediction_error_response_mode_t error_response_mode; /**< Error response strategy */
    float prediction_error_threshold;             /**< Threshold for immune alert */
    float free_energy_threshold;                  /**< Free energy threshold for threat */
    uint32_t cumulative_error_window;             /**< Window for cumulative error (timesteps) */
    float adaptive_threshold_alpha;               /**< Adaptive threshold smoothing */

    /* Cytokine-specific effects */
    float il1_precision_effect;                   /**< IL-1 effect on precision */
    float il6_precision_effect;                   /**< IL-6 effect on precision */
    float tnf_precision_effect;                   /**< TNF-α effect on precision */
    float il10_recovery_boost;                    /**< IL-10 recovery effect */

    /* Bio-async integration */
    bool enable_bio_async;                        /**< Enable bio-async messaging */
    bool broadcast_intero_predictions;            /**< Broadcast interoceptive predictions */
} predictive_immune_config_t;

/* ============================================================================
 * State Structures
 * ============================================================================ */

/**
 * @brief Interoceptive state representation
 *
 * WHAT: Current and predicted internal immune state
 * WHY:  Track predictions and errors for interoception
 */
typedef struct {
    /* Current state (sensed) */
    float inflammation_level;                     /**< Current inflammation (0-1) */
    float cytokine_concentrations[BRAIN_CYTOKINE_COUNT]; /**< Cytokine levels */
    float immune_activation;                      /**< Overall immune activation (0-1) */
    uint32_t active_threats;                      /**< Active antigen count */

    /* Predicted state */
    float predicted_inflammation;                 /**< Predicted inflammation */
    float predicted_cytokines[BRAIN_CYTOKINE_COUNT];    /**< Predicted cytokine levels */
    float predicted_activation;                   /**< Predicted activation */

    /* Prediction errors */
    float inflammation_error;                     /**< Inflammation prediction error */
    float cytokine_error[BRAIN_CYTOKINE_COUNT];         /**< Cytokine prediction errors */
    float activation_error;                       /**< Activation prediction error */
    float total_interoceptive_error;              /**< Total interoceptive error */
} interoceptive_state_t;

/**
 * @brief Immune-modulated precision state
 *
 * WHAT: Track how immune state affects prediction precision
 * WHY:  Monitor immune modulation effects
 */
typedef struct {
    float baseline_precision;                     /**< Baseline precision (no inflammation) */
    float current_precision;                      /**< Current modulated precision */
    float inflammation_factor;                    /**< Inflammation reduction factor */
    float cytokine_factor;                        /**< Cytokine reduction factor */
    float total_reduction;                        /**< Total precision reduction */
} immune_modulated_precision_t;

/**
 * @brief Prediction error immune trigger state
 *
 * WHAT: Track prediction errors for immune threat detection
 * WHY:  Detect corrupted processing via prediction anomalies
 */
typedef struct {
    float current_error;                          /**< Current prediction error */
    float error_threshold;                        /**< Current threshold (adaptive) */
    float cumulative_error;                       /**< Cumulative error in window */
    uint32_t error_spike_count;                   /**< Count of threshold crossings */
    uint64_t last_trigger_time;                   /**< Last immune trigger time */
    bool triggered;                               /**< Currently triggered */
} prediction_error_trigger_t;

/**
 * @brief Predictive-immune integration statistics
 */
typedef struct {
    /* Interoceptive prediction */
    uint64_t interoceptive_updates;               /**< Total intero updates */
    float avg_interoceptive_error;                /**< Average intero error */
    float max_interoceptive_error;                /**< Max intero error seen */
    uint32_t sickness_behavior_triggers;          /**< Sickness behavior count */

    /* Immune modulation */
    float avg_precision_reduction;                /**< Average precision reduction */
    float max_precision_reduction;                /**< Max precision reduction */
    uint64_t modulation_events;                   /**< Modulation event count */

    /* Prediction error triggers */
    uint32_t immune_triggers;                     /**< Immune triggers from pred errors */
    uint32_t false_positives;                     /**< False positive triggers */
    float trigger_accuracy;                       /**< Accuracy of triggers (0-1) */
} predictive_immune_stats_t;

/* ============================================================================
 * Main Integration Structure
 * ============================================================================ */

/**
 * @brief Predictive-immune integration system (opaque handle)
 */
typedef struct predictive_immune_system predictive_immune_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for predictive-immune integration
 * WHY:  Easy initialization
 * HOW:  Return struct with balanced parameters
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t predictive_immune_default_config(predictive_immune_config_t* config);

/**
 * @brief Create predictive-immune integration system
 *
 * WHAT: Initialize integration between predictive and immune systems
 * WHY:  Enable bidirectional coupling
 * HOW:  Allocate state, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param predictive_net Predictive network to integrate
 * @param immune_system Brain immune system to integrate
 * @return Integration system handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
predictive_immune_system_t* predictive_immune_create(
    const predictive_immune_config_t* config,
    predictive_network_t predictive_net,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy predictive-immune integration
 *
 * WHAT: Clean up integration resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister callbacks, free state
 *
 * @param system Integration system
 *
 * COMPLEXITY: O(1)
 */
void predictive_immune_destroy(predictive_immune_system_t* system);

/**
 * @brief Start integration
 *
 * WHAT: Activate integration monitoring and coupling
 * WHY:  Begin predictive-immune interaction
 * HOW:  Register callbacks with both systems
 *
 * @param system Integration system
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t predictive_immune_start(predictive_immune_system_t* system);

/**
 * @brief Stop integration
 *
 * WHAT: Deactivate integration
 * WHY:  Graceful shutdown
 * HOW:  Unregister callbacks
 *
 * @param system Integration system
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t predictive_immune_stop(predictive_immune_system_t* system);

/* ============================================================================
 * Interoceptive Prediction API
 * ============================================================================ */

/**
 * @brief Update interoceptive prediction of immune state
 *
 * WHAT: Generate prediction of current immune state
 * WHY:  Core of interoceptive inference
 * HOW:  Use predictive network to predict inflammation, cytokines
 *
 * BIOLOGICAL BASIS:
 * - Anterior insula represents interoceptive predictions
 * - Prediction errors drive autonomic adjustments and sickness behavior
 * - Active inference: brain infers causes of immune signals
 *
 * @param system Integration system
 * @param dt Time step (ms)
 * @return NIMCP_SUCCESS on success
 *
 * SIDE EFFECTS:
 * - Updates interoceptive state predictions
 * - Computes prediction errors
 * - May trigger sickness behavior if errors are large
 *
 * COMPLEXITY: O(intero_state_dim × network_complexity)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_update_interoception(
    predictive_immune_system_t* system,
    float dt
);

/**
 * @brief Get current interoceptive state
 *
 * WHAT: Query interoceptive predictions and errors
 * WHY:  Monitor interoceptive inference
 * HOW:  Return current state snapshot
 *
 * @param system Integration system
 * @param state Output interoceptive state
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_get_interoceptive_state(
    predictive_immune_system_t* system,
    interoceptive_state_t* state
);

/**
 * @brief Trigger sickness behavior response
 *
 * WHAT: Execute sickness behavior from interoceptive prediction error
 * WHY:  Model behavioral response to immune state mismatch
 * HOW:  Reduce activity, increase rest, modulate attention
 *
 * BIOLOGICAL BASIS:
 * - Sickness behavior conserves energy during infection
 * - Driven by cytokines (IL-1β, IL-6) and prediction errors
 * - Reduces exploratory behavior, increases recuperative behavior
 *
 * @param system Integration system
 * @param intero_error Interoceptive prediction error magnitude
 * @return NIMCP_SUCCESS on success
 *
 * SIDE EFFECTS:
 * - May reduce learning rates
 * - May broadcast sickness behavior via bio-async
 * - Updates statistics
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_trigger_sickness_behavior(
    predictive_immune_system_t* system,
    float intero_error
);

/* ============================================================================
 * Immune Modulation of Prediction API
 * ============================================================================ */

/**
 * @brief Apply immune modulation to prediction precision
 *
 * WHAT: Reduce prediction precision based on inflammation state
 * WHY:  Inflammation increases uncertainty, reduces precision
 * HOW:  Scale precision weights by inflammation level
 *
 * BIOLOGICAL BASIS:
 * - Cytokines (IL-1β, IL-6, TNFα) reduce synaptic efficacy
 * - Inflammation increases neural noise, reducing signal precision
 * - Adaptive: during threat, focus on gross features not fine details
 *
 * FORMULA: precision_modulated = precision_base × (1 - inflammation × factor)
 *
 * @param system Integration system
 * @param region Predictive region to modulate (NULL for all)
 * @return NIMCP_SUCCESS on success
 *
 * SIDE EFFECTS:
 * - Updates region precision weights
 * - May broadcast precision modulation via bio-async
 * - Updates modulation statistics
 *
 * COMPLEXITY: O(region_neurons)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_apply_immune_modulation(
    predictive_immune_system_t* system,
    brain_region_t* region
);

/**
 * @brief Compute cytokine effect on precision
 *
 * WHAT: Calculate precision reduction from cytokine concentrations
 * WHY:  Different cytokines have different effects
 * HOW:  Weighted sum of cytokine effects
 *
 * BIOLOGICAL BASIS:
 * - IL-1β: Reduces LTP, increases neural noise
 * - IL-6: Modulates neurotransmitter release
 * - TNFα: Scales synaptic strength
 * - IL-10: Anti-inflammatory, restores precision
 *
 * @param system Integration system
 * @param cytokine_levels Cytokine concentrations [CYTOKINE_COUNT]
 * @param precision_out Output: modulated precision factor (0-1)
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(CYTOKINE_COUNT)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_compute_cytokine_precision_effect(
    predictive_immune_system_t* system,
    const float* cytokine_levels,
    float* precision_out
);

/**
 * @brief Get immune-modulated precision for region
 *
 * WHAT: Query current precision modulation state
 * WHY:  Monitor immune effects on prediction
 * HOW:  Return precision state snapshot
 *
 * @param system Integration system
 * @param region Predictive region to query
 * @param precision_state Output precision state
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_get_precision_modulation(
    predictive_immune_system_t* system,
    brain_region_t* region,
    immune_modulated_precision_t* precision_state
);

/* ============================================================================
 * Prediction Error Detection API
 * ============================================================================ */

/**
 * @brief Monitor prediction error for immune threat detection
 *
 * WHAT: Check if prediction errors indicate corrupted processing
 * WHY:  Large errors may signal adversarial input or neural corruption
 * HOW:  Compare errors to adaptive threshold, trigger immune if exceeded
 *
 * BIOLOGICAL BASIS:
 * - Persistent prediction errors indicate model failure
 * - May result from corrupted sensory input or compromised processing
 * - Immune system activated as protective response
 *
 * @param system Integration system
 * @param region Predictive region to monitor
 * @param dt Time step (ms)
 * @return NIMCP_SUCCESS on success
 *
 * SIDE EFFECTS:
 * - Updates error trigger state
 * - May present antigen to immune system if threshold exceeded
 * - Updates trigger statistics
 *
 * COMPLEXITY: O(region_neurons)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_monitor_prediction_errors(
    predictive_immune_system_t* system,
    brain_region_t* region,
    float dt
);

/**
 * @brief Trigger immune response from prediction error
 *
 * WHAT: Present prediction error as antigen to immune system
 * WHY:  Large errors treated as potential threats
 * HOW:  Create antigen from error signature, present to immune
 *
 * @param system Integration system
 * @param region Source region with error
 * @param error_magnitude Error magnitude
 * @param antigen_id Output: created antigen ID
 * @return NIMCP_SUCCESS on success
 *
 * SIDE EFFECTS:
 * - Creates antigen in immune system
 * - May trigger immune response cascade
 * - Broadcasts alert via bio-async
 *
 * COMPLEXITY: O(BRAIN_IMMUNE_EPITOPE_SIZE)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_trigger_error_response(
    predictive_immune_system_t* system,
    brain_region_t* region,
    float error_magnitude,
    uint32_t* antigen_id
);

/**
 * @brief Get prediction error trigger state
 *
 * WHAT: Query current error monitoring state
 * WHY:  Monitor threat detection sensitivity
 * HOW:  Return trigger state snapshot
 *
 * @param system Integration system
 * @param region Region to query
 * @param trigger_state Output trigger state
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_get_error_trigger_state(
    predictive_immune_system_t* system,
    brain_region_t* region,
    prediction_error_trigger_t* trigger_state
);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Update predictive-immune integration
 *
 * WHAT: Execute full integration cycle
 * WHY:  Maintain bidirectional coupling
 * HOW:
 *   1. Update interoceptive predictions
 *   2. Apply immune modulation to precision
 *   3. Monitor prediction errors for threats
 *
 * @param system Integration system
 * @param dt Time step (ms)
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(total_neurons + intero_dim)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_update(
    predictive_immune_system_t* system,
    float dt
);

/**
 * @brief Get integration statistics
 *
 * WHAT: Query performance metrics
 * WHY:  Monitor integration effectiveness
 * HOW:  Return statistics snapshot
 *
 * @param system Integration system
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_get_stats(
    predictive_immune_system_t* system,
    predictive_immune_stats_t* stats
);

/**
 * @brief Reset integration state
 *
 * WHAT: Clear all integration state
 * WHY:  Start fresh integration cycle
 * HOW:  Reset predictions, errors, modulation
 *
 * @param system Integration system
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_reset(predictive_immune_system_t* system);

/* ============================================================================
 * Region-Specific Integration API
 * ============================================================================ */

/**
 * @brief Connect predictive region to immune integration
 *
 * WHAT: Enable immune coupling for specific brain region
 * WHY:  Region-specific predictive-immune interaction
 * HOW:  Register region, apply modulation
 *
 * @param system Integration system
 * @param region Predictive region to connect
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_connect_region(
    predictive_immune_system_t* system,
    brain_region_t* region
);

/**
 * @brief Disconnect predictive region from immune integration
 *
 * WHAT: Disable immune coupling for region
 * WHY:  Remove region from integration
 * HOW:  Unregister region, restore baseline precision
 *
 * @param system Integration system
 * @param region Predictive region to disconnect
 * @return NIMCP_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
nimcp_result_t predictive_immune_disconnect_region(
    predictive_immune_system_t* system,
    brain_region_t* region
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_IMMUNE_H */
