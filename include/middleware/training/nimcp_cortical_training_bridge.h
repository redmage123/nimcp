//=============================================================================
// nimcp_cortical_training_bridge.h - Cortical-Training Bridge Integration
//=============================================================================
//
// WHAT: Bidirectional bridge integrating cortical modules (predictive coding,
//       dendritic computation, cortical columns) with training pipeline.
//
// WHY: Models how cortical dynamics affect learning. Predictive coding's
//      free energy guides learning intensity, dendritic bursts signal stable
//      representations, column competition indicates feature quality.
//
// HOW: Cortical → Training: Modulates LR, gradient confidence, precision weights
//      Training → Cognitive: Consolidates predictions, adjusts PE gains
//      Integrates with cognitive-training, logic-training, immune-training bridges
//
// BIOLOGICAL BASIS:
// - Predictive coding: Free energy minimization guides learning (high FE → learn more)
// - Dendritic bursts: BAC firing signals successful prediction → stable learning
// - Cortical columns: Winner confidence indicates representation quality
// - Precision weighting: Attention-like modulation of prediction errors
//
//=============================================================================

#ifndef NIMCP_CORTICAL_TRAINING_BRIDGE_H
#define NIMCP_CORTICAL_TRAINING_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct cortical_training_bridge cortical_training_bridge_t;
typedef struct predictive_coding_context predictive_coding_context_t;
typedef struct dendritic_compartment dendritic_compartment_t;
typedef struct hypercolumn hypercolumn_t;
typedef struct cognitive_training_bridge cognitive_training_bridge_t;
typedef struct training_logic_bridge training_logic_bridge_t;
typedef struct training_immune_system training_immune_system_t;
typedef struct training_plasticity_bridge training_plasticity_bridge_t;

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define CORTICAL_TRAINING_MODULE_NAME      "cortical_training_bridge"
#define CORTICAL_TRAINING_MODULE_VERSION   "1.0.0"

/* Bio-async module ID defined in nimcp_bio_messages.h as BIO_MODULE_CORTICAL_TRAINING (0x0524) */

/** Default configuration values */
#define CORTICAL_TRAINING_DEFAULT_UPDATE_INTERVAL_MS    100
#define CORTICAL_TRAINING_DEFAULT_LR_MIN_FACTOR         0.3f
#define CORTICAL_TRAINING_DEFAULT_LR_MAX_FACTOR         1.15f
#define CORTICAL_TRAINING_DEFAULT_GRADIENT_MIN_CONF     0.5f
#define CORTICAL_TRAINING_DEFAULT_GRADIENT_MAX_CONF     1.0f

/** Limits */
#define CORTICAL_TRAINING_MAX_LAYERS                    16
#define CORTICAL_TRAINING_MAX_PRECISION_WEIGHTS         1024

/** Free energy thresholds */
#define CORTICAL_TRAINING_FE_EXPLOSION_THRESHOLD        100.0f
#define CORTICAL_TRAINING_FE_HIGH_THRESHOLD             10.0f

/** Burst rate thresholds */
#define CORTICAL_TRAINING_BURST_RATE_COLLAPSE_THRESHOLD 0.1f
#define CORTICAL_TRAINING_BURST_RATE_LOW_THRESHOLD      0.3f
#define CORTICAL_TRAINING_BURST_RATE_HIGH_THRESHOLD     0.7f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Types of cortical modulation on training
 *
 * WHAT: Different ways cortical state affects training parameters
 * WHY: Each modulation type targets specific training aspect
 * HOW: Computed from cortical effects, applied to training
 */
typedef enum {
    CORTICAL_TRAINING_MODULATION_LR = 0,              /**< Learning rate modulation */
    CORTICAL_TRAINING_MODULATION_GRADIENT_CONFIDENCE, /**< Gradient trust/confidence */
    CORTICAL_TRAINING_MODULATION_PRECISION,           /**< Per-layer precision weights */
    CORTICAL_TRAINING_MODULATION_CONSOLIDATION,       /**< Consolidation triggering */
    CORTICAL_TRAINING_MODULATION_COUNT
} cortical_training_modulation_t;

/**
 * @brief Feedback events from training to cortical modules
 *
 * WHAT: Training events that trigger cortical responses
 * WHY: Training progress affects cortical state (strengthen predictions, reset precision)
 * HOW: Each event updates cortical modules via API calls
 */
typedef enum {
    CORTICAL_TRAINING_FEEDBACK_STRENGTHEN_PREDICTIONS = 0, /**< Loss improved → strengthen predictions */
    CORTICAL_TRAINING_FEEDBACK_INCREASE_PE_GAIN,           /**< Loss plateaued → increase PE gain */
    CORTICAL_TRAINING_FEEDBACK_RESET_PRECISION,            /**< Divergence → reset precision weights */
    CORTICAL_TRAINING_FEEDBACK_CONSOLIDATE,                /**< Good convergence → consolidate representations */
    CORTICAL_TRAINING_FEEDBACK_COUNT
} cortical_training_feedback_t;

/**
 * @brief Operation mode for cortical-training bridge
 *
 * WHAT: How the bridge processes and applies modulations
 * WHY: Different use cases need different automation levels
 * HOW: Modes control whether modulations are advisory or automatic
 */
typedef enum {
    CORTICAL_TRAINING_MODE_DISABLED = 0,     /**< Bridge disabled */
    CORTICAL_TRAINING_MODE_MONITOR_ONLY,     /**< Monitor but don't modulate */
    CORTICAL_TRAINING_MODE_ADVISORY,         /**< Provide recommendations */
    CORTICAL_TRAINING_MODE_AUTOMATIC,        /**< Automatically apply modulations */
    CORTICAL_TRAINING_MODE_COORDINATED       /**< Coordinate with other bridges */
} cortical_training_mode_t;

//=============================================================================
// Cortical Effects (Cortical → Training)
//=============================================================================

/**
 * @brief Cortical effects on training parameters
 *
 * WHAT: Aggregated cortical state that modulates training
 * WHY: Captures predictive coding, dendritic, and columnar influences on learning
 * HOW: Updated from connected cortical modules each cycle
 *
 * BIOLOGICAL ANALOGY: Cortical feedback signals regulate learning intensity
 * and confidence. High prediction errors (free energy) indicate learning
 * opportunity, while dendritic bursts confirm stable representations.
 */
typedef struct {
    /* === Predictive Coding Effects === */
    float free_energy;               /**< Total free energy [0-∞], high → more learning */
    float prediction_error_mag;      /**< Average prediction error magnitude [0-∞] */
    float convergence_rate;          /**< Rate of free energy decrease [0-1] */
    float* precision_weights;        /**< Per-layer precision weights [num_layers] */
    uint32_t num_layers;             /**< Number of hierarchical layers */

    /* === Dendritic Effects === */
    float burst_rate;                /**< Dendritic burst rate [0-1], high → stable */
    float bac_success_rate;          /**< BAC firing success rate [0-1] */
    float calcium_spikes;            /**< Calcium spike rate [0-∞] */

    /* === Cortical Column Effects === */
    float winner_confidence;         /**< Confidence of winning minicolumn [0-1] */
    float population_entropy;        /**< Entropy of column activations [0-log(N)] */
    float inhibition_strength;       /**< Lateral inhibition strength [0-1] */

    /* === Computed Modulations === */
    float lr_factor;                 /**< Computed LR multiplier [0.3-1.15] */
    float gradient_confidence;       /**< Trust in gradient direction [0.5-1.0] */
    bool predictions_stable;         /**< Whether predictions have converged */
    bool should_consolidate;         /**< Consolidation recommended */

    /* === Metadata === */
    uint64_t last_update_ms;         /**< When effects were last updated */
    bool valid;                      /**< Whether effects are current */
} cortical_training_effects_t;

//=============================================================================
// Training Effects (Training → Cortical)
//=============================================================================

/**
 * @brief Training feedback to cortical modules
 *
 * WHAT: Training state that triggers cortical responses
 * WHY: Learning progress affects cortical state (consolidate predictions)
 * HOW: Signals sent to cortical modules after metric updates
 *
 * BIOLOGICAL ANALOGY: Learning success modulates prediction strength and
 * precision. Successful learning consolidates predictions, while failures
 * increase prediction error gain.
 */
typedef struct {
    /* === Loss Metrics === */
    float loss_current;              /**< Current loss value */
    float loss_delta;                /**< Change from previous step */
    float loss_improvement_rate;     /**< Rate of improvement [0-1] */
    bool convergence_detected;       /**< Loss converged */

    /* === Gradient Metrics === */
    float gradient_norm;             /**< Current gradient norm */
    float gradient_stability;        /**< Gradient stability [0-1] */
    bool gradient_aligned;           /**< Gradients align with predictions */

    /* === Prediction Metrics === */
    float prediction_accuracy;       /**< Prediction accuracy [0-1] */
    bool predictions_improving;      /**< Predictions getting better */
    uint32_t steps_since_improvement;/**< Steps without improvement */

    /* === Metadata === */
    uint64_t timestamp_ms;           /**< When metrics were captured */
    bool valid;                      /**< Whether metrics are current */
} training_cortical_effects_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for cortical-training bridge
 *
 * WHAT: All configurable parameters for the bridge
 * WHY: Allow customization for different training scenarios
 * HOW: Passed to create function, copied internally
 */
typedef struct {
    cortical_training_mode_t mode;          /**< Operation mode */

    /* === Cortical Module Enables === */
    bool enable_predictive_coding;          /**< Enable predictive coding integration */
    bool enable_dendritic;                  /**< Enable dendritic computation integration */
    bool enable_columns;                    /**< Enable cortical columns integration */

    /* === Modulation Strengths === */
    float predictive_strength;              /**< Predictive coding modulation weight [0-1] */
    float dendritic_strength;               /**< Dendritic modulation weight [0-1] */
    float columns_strength;                 /**< Columns modulation weight [0-1] */

    /* === LR Modulation Parameters === */
    float lr_min_factor;                    /**< Minimum LR multiplier (default: 0.3) */
    float lr_max_factor;                    /**< Maximum LR multiplier (default: 1.15) */
    float lr_fe_scale;                      /**< How much free energy affects LR */
    float lr_burst_scale;                   /**< How much burst rate affects LR */

    /* === Gradient Confidence Parameters === */
    float gradient_min_confidence;          /**< Minimum gradient confidence */
    float gradient_max_confidence;          /**< Maximum gradient confidence */
    float confidence_burst_weight;          /**< Weight of burst rate on confidence */
    float confidence_winner_weight;         /**< Weight of winner confidence */

    /* === Precision Modulation === */
    bool enable_precision_modulation;       /**< Use precision weights for gradient scaling */
    float precision_min_weight;             /**< Minimum precision weight */
    float precision_max_weight;             /**< Maximum precision weight */

    /* === Consolidation === */
    float consolidation_burst_threshold;    /**< Burst rate to trigger consolidation */
    float consolidation_fe_threshold;       /**< Max free energy for consolidation */
    float consolidation_convergence_threshold; /**< Min convergence rate for consolidation */

    /* === Free Energy Thresholds === */
    float fe_explosion_threshold;           /**< Free energy explosion (danger) */
    float fe_high_threshold;                /**< High free energy (boost learning) */
    float burst_collapse_threshold;         /**< Burst rate collapse (instability) */

    /* === Integration Flags === */
    bool enable_cognitive_training;         /**< Integrate with cognitive-training bridge */
    bool enable_training_logic;             /**< Integrate with training-logic bridge */
    bool enable_training_immune;            /**< Integrate with training-immune system */
    bool enable_training_plasticity;        /**< Integrate with training-plasticity bridge */
    bool enable_bio_async;                  /**< Enable bio-async messaging */

    /* === Update Settings === */
    uint32_t update_interval_ms;            /**< Update interval (default: 100ms) */
    bool disable_auto_update;               /**< Disable automatic updates (testing) */

    /* === Safety Limits === */
    float max_modulation_change_per_step;   /**< Max modulation change per step */
    bool enable_emergency_override;         /**< Allow emergency LR reduction */
} cortical_training_config_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Statistics for cortical-training bridge
 *
 * WHAT: Tracking of bridge activity and performance
 * WHY: Monitoring and debugging
 * HOW: Accumulated during operation, queryable via API
 */
typedef struct {
    /* === Modulation Counts === */
    uint64_t total_modulations;
    uint64_t modulations_by_type[CORTICAL_TRAINING_MODULATION_COUNT];
    uint64_t lr_increases;
    uint64_t lr_decreases;
    uint64_t consolidations_triggered;

    /* === Feedback Counts === */
    uint64_t total_feedback_events;
    uint64_t feedback_by_type[CORTICAL_TRAINING_FEEDBACK_COUNT];

    /* === Average Effects === */
    float avg_free_energy;
    float avg_burst_rate;
    float avg_winner_confidence;
    float avg_prediction_error;

    /* === Modulation Factors === */
    float avg_lr_factor;
    float avg_gradient_confidence;
    float min_lr_factor;
    float max_lr_factor;

    /* === Integration Status === */
    bool predictive_coding_connected;
    bool dendritic_connected;
    bool columns_connected;
    bool cognitive_training_connected;
    bool training_logic_connected;
    bool training_immune_connected;
    bool training_plasticity_connected;
    bool bio_async_connected;

    /* === Timing === */
    uint64_t total_update_calls;
    float avg_update_time_us;
    float max_update_time_us;
    uint64_t last_update_ms;

    /* === Current State === */
    cortical_training_mode_t current_mode;
    uint64_t current_training_step;
} cortical_training_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize configuration with default values
 *
 * WHAT: Populates config struct with sensible defaults
 * WHY: Ensure all fields have valid initial values
 * HOW: Sets mode to AUTOMATIC, enables all modules, balanced weights
 *
 * @param config Configuration to initialize (must not be NULL)
 */
void cortical_training_default_config(cortical_training_config_t* config);

/**
 * @brief Create a new cortical-training bridge
 *
 * WHAT: Allocates and initializes bridge structure
 * WHY: Entry point for using the bridge
 * HOW: Allocates effects structures, initializes state
 *
 * @param config Configuration (NULL uses defaults)
 * @return Bridge handle or NULL on failure
 */
cortical_training_bridge_t* cortical_training_create(
    const cortical_training_config_t* config
);

/**
 * @brief Destroy a cortical-training bridge
 *
 * WHAT: Frees all resources associated with bridge
 * WHY: Proper cleanup
 * HOW: Disconnects integrations, frees memory, destroys mutex
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void cortical_training_destroy(cortical_training_bridge_t* bridge);

/**
 * @brief Start the cortical-training bridge
 *
 * WHAT: Activates the bridge for operation
 * WHY: Allows deferred start after configuration
 * HOW: Connects bio-async if enabled, initializes update timer
 *
 * @param bridge Bridge to start
 * @return 0 on success, negative on error
 */
int cortical_training_start(cortical_training_bridge_t* bridge);

/**
 * @brief Stop the cortical-training bridge
 *
 * WHAT: Deactivates the bridge
 * WHY: Pause operation without destroying
 * HOW: Disconnects bio-async, preserves state
 *
 * @param bridge Bridge to stop
 * @return 0 on success, negative on error
 */
int cortical_training_stop(cortical_training_bridge_t* bridge);

//=============================================================================
// Cortical Module Connection API
//=============================================================================

/**
 * @brief Connect to predictive coding module
 *
 * WHAT: Links bridge to predictive coding (free energy principle)
 * WHY: Free energy guides learning intensity and precision
 * HOW: Queries free energy, prediction errors, precision weights, convergence
 *
 * BIOLOGICAL BASIS: Predictive coding minimizes free energy (surprise).
 * High free energy indicates learning opportunity, while low free energy
 * suggests stable predictions.
 *
 * @param bridge Bridge to connect
 * @param predictive_coding Predictive coding context (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cortical_training_connect_predictive_coding(
    cortical_training_bridge_t* bridge,
    predictive_coding_context_t* predictive_coding
);

/**
 * @brief Connect to dendritic computation module
 *
 * WHAT: Links bridge to dendritic compartments (bursts, BAC firing)
 * WHY: Dendritic bursts signal successful prediction and stable learning
 * HOW: Queries burst rate, BAC success rate, calcium spikes
 *
 * BIOLOGICAL BASIS: Dendritic bursts (BAC firing) occur when bottom-up
 * input matches top-down prediction, signaling stable representation.
 *
 * @param bridge Bridge to connect
 * @param dendritic Dendritic compartment (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cortical_training_connect_dendritic(
    cortical_training_bridge_t* bridge,
    dendritic_compartment_t* dendritic
);

/**
 * @brief Connect to cortical columns module
 *
 * WHAT: Links bridge to cortical columns (competition, winner confidence)
 * WHY: Column competition indicates representation quality
 * HOW: Queries winner confidence, population entropy, inhibition strength
 *
 * BIOLOGICAL BASIS: Cortical columns compete via lateral inhibition.
 * High winner confidence indicates good feature representation, while
 * high entropy suggests uncertain/distributed representation.
 *
 * @param bridge Bridge to connect
 * @param columns Cortical columns (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cortical_training_connect_columns(
    cortical_training_bridge_t* bridge,
    hypercolumn_t* columns
);

//=============================================================================
// Training Integration API
//=============================================================================

/**
 * @brief Connect to cognitive-training bridge
 *
 * WHAT: Links bridge to cognitive-training for coordination
 * WHY: Combine cortical and cognitive signals for unified modulation
 * HOW: Syncs epistemic uncertainty, cognitive load
 *
 * @param bridge Bridge to connect
 * @param cognitive_training Cognitive-training bridge (may be NULL)
 * @return 0 on success, negative on error
 */
int cortical_training_connect_cognitive_training(
    cortical_training_bridge_t* bridge,
    cognitive_training_bridge_t* cognitive_training
);

/**
 * @brief Connect to training-logic bridge
 *
 * WHAT: Links bridge to logic-based training control
 * WHY: Cortical conditions feed into logic gates
 * HOW: Provides cortical state as logic conditions
 *
 * @param bridge Bridge to connect
 * @param training_logic Training-logic bridge (may be NULL)
 * @return 0 on success, negative on error
 */
int cortical_training_connect_training_logic(
    cortical_training_bridge_t* bridge,
    training_logic_bridge_t* training_logic
);

/**
 * @brief Connect to training-immune system
 *
 * WHAT: Links bridge to immune-based training control
 * WHY: Free energy explosion and burst collapse are threats
 * HOW: Reports cortical anomalies as antigens
 *
 * @param bridge Bridge to connect
 * @param training_immune Training-immune system (may be NULL)
 * @return 0 on success, negative on error
 */
int cortical_training_connect_training_immune(
    cortical_training_bridge_t* bridge,
    training_immune_system_t* training_immune
);

/**
 * @brief Connect to training-plasticity bridge
 *
 * WHAT: Links bridge to plasticity-based training control
 * WHY: Cortical state affects plasticity mechanisms
 * HOW: Modulates STDP, BCM based on cortical effects
 *
 * @param bridge Bridge to connect
 * @param training_plasticity Training-plasticity bridge (may be NULL)
 * @return 0 on success, negative on error
 */
int cortical_training_connect_training_plasticity(
    cortical_training_bridge_t* bridge,
    training_plasticity_bridge_t* training_plasticity
);

//=============================================================================
// Cortical → Training: Modulation API
//=============================================================================

/**
 * @brief Get current cortical effects
 *
 * WHAT: Retrieve aggregated cortical state
 * WHY: Query cortical modulations for training
 * HOW: Copies internal effects to output
 *
 * @param bridge Bridge to query
 * @param effects Output effects structure
 * @return 0 on success, negative on error
 */
int cortical_training_get_effects(
    const cortical_training_bridge_t* bridge,
    cortical_training_effects_t* effects
);

/**
 * @brief Get modulated learning rate
 *
 * WHAT: Apply cortical modulation to base LR
 * WHY: Automatic LR adjustment based on cortical state
 * HOW: base_lr × cortical_effects.lr_factor
 *
 * BIOLOGICAL BASIS: High burst rate → stable predictions → maintain/increase LR.
 * High free energy → learning opportunity → boost LR.
 * Free energy explosion → danger → drastically reduce LR.
 *
 * @param bridge Bridge to query
 * @param base_lr Base learning rate
 * @return Modulated learning rate
 */
float cortical_training_get_modulated_lr(
    const cortical_training_bridge_t* bridge,
    float base_lr
);

/**
 * @brief Get gradient confidence factor
 *
 * WHAT: Confidence in gradient direction based on cortical state
 * WHY: Scale gradient updates by confidence
 * HOW: Combines burst rate and winner confidence
 *
 * BIOLOGICAL BASIS: High burst rate and winner confidence indicate
 * stable representations, increasing trust in gradients.
 *
 * @param bridge Bridge to query
 * @return Gradient confidence [0.5-1.0]
 */
float cortical_training_get_gradient_confidence(
    const cortical_training_bridge_t* bridge
);

/**
 * @brief Check if predictions are stable
 *
 * WHAT: Determine if cortical predictions have converged
 * WHY: Trigger consolidation or reduce exploration
 * HOW: Returns cortical_effects.predictions_stable
 *
 * BIOLOGICAL BASIS: Low free energy and high burst rate indicate
 * stable predictive model.
 *
 * @param bridge Bridge to query
 * @return true if predictions stable, false otherwise
 */
bool cortical_training_are_predictions_stable(
    const cortical_training_bridge_t* bridge
);

/**
 * @brief Get per-layer precision weights
 *
 * WHAT: Precision-based gradient scaling for hierarchical layers
 * WHY: Focus learning on uncertain layers (low precision)
 * HOW: weights[i] = cortical_effects.precision_weights[i]
 *
 * BIOLOGICAL BASIS: Precision (inverse variance) modulates prediction
 * error signals. Low precision → high uncertainty → more learning.
 *
 * @param bridge Bridge to query
 * @param weights Output precision weights [num_layers]
 * @param num_layers Number of layers
 * @return 0 on success, negative on error
 */
int cortical_training_get_precision_weights(
    const cortical_training_bridge_t* bridge,
    float* weights,
    uint32_t num_layers
);

//=============================================================================
// Training → Cortical: Feedback API
//=============================================================================

/**
 * @brief Update training metrics
 *
 * WHAT: Reports current training state to cortical modules
 * WHY: Training progress triggers cortical responses
 * HOW: Updates internal state, may trigger feedback events
 *
 * @param bridge Bridge to update
 * @param loss Current loss value
 * @param grad_norm Current gradient norm
 * @param lr Current learning rate
 * @param step Current training step
 * @return 0 on success, negative on error
 */
int cortical_training_update_metrics(
    cortical_training_bridge_t* bridge,
    float loss,
    float grad_norm,
    float lr,
    uint64_t step
);

/**
 * @brief Signal a training feedback event
 *
 * WHAT: Notify cortical modules of training event
 * WHY: Events trigger cortical adaptations
 * HOW: Calls appropriate cortical module APIs
 *
 * EXAMPLES:
 * - STRENGTHEN_PREDICTIONS: Loss improved → reinforce predictions
 * - INCREASE_PE_GAIN: Plateau → increase prediction error gain
 * - RESET_PRECISION: Divergence → reset precision weights
 * - CONSOLIDATE: Convergence → consolidate representations
 *
 * @param bridge Bridge to signal
 * @param event Feedback event type
 * @param magnitude Event magnitude [0-1]
 * @return 0 on success, negative on error
 */
int cortical_training_signal_event(
    cortical_training_bridge_t* bridge,
    cortical_training_feedback_t event,
    float magnitude
);

//=============================================================================
// Update Cycle API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Main update cycle for bridge
 * WHY: Refresh cortical state and compute modulations
 * HOW: Queries cortical modules, computes effects, applies modulations
 *
 * CALL FREQUENCY: Every training step or every N milliseconds
 *
 * @param bridge Bridge to update
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success, negative on error
 */
int cortical_training_update(
    cortical_training_bridge_t* bridge,
    uint64_t delta_ms
);

//=============================================================================
// Bio-Async API
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async messaging system
 * WHY: Enable inter-module communication
 * HOW: Registers module, creates inbox
 *
 * @param bridge Bridge to connect
 * @return 0 on success, negative on error
 */
int cortical_training_connect_bio_async(
    cortical_training_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async system
 * WHY: Clean shutdown
 * HOW: Unregisters module
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, negative on error
 */
int cortical_training_disconnect_bio_async(
    cortical_training_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY: Determine if messaging is available
 * HOW: Returns internal flag
 *
 * @param bridge Bridge to query
 * @return true if connected, false otherwise
 */
bool cortical_training_is_bio_async_connected(
    const cortical_training_bridge_t* bridge
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve accumulated statistics
 * WHY: Monitoring and debugging
 * HOW: Copies internal stats to output
 *
 * @param bridge Bridge to query
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int cortical_training_get_stats(
    const cortical_training_bridge_t* bridge,
    cortical_training_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY: Start fresh measurement period
 * HOW: Zeros all stat counters
 *
 * @param bridge Bridge to reset
 * @return 0 on success, negative on error
 */
int cortical_training_reset_stats(
    cortical_training_bridge_t* bridge
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Convert modulation type enum to string
 *
 * @param modulation Modulation type to convert
 * @return String name of modulation type
 */
const char* cortical_training_modulation_to_string(
    cortical_training_modulation_t modulation
);

/**
 * @brief Convert feedback event enum to string
 *
 * @param event Feedback event to convert
 * @return String name of event
 */
const char* cortical_training_feedback_to_string(
    cortical_training_feedback_t event
);

/**
 * @brief Convert mode enum to string
 *
 * @param mode Mode to convert
 * @return String name of mode
 */
const char* cortical_training_mode_to_string(
    cortical_training_mode_t mode
);

/**
 * @brief Dump bridge state for debugging
 *
 * @param bridge Bridge to dump
 */
void cortical_training_dump_state(
    const cortical_training_bridge_t* bridge
);

//=============================================================================
// Test API (for unit/integration testing without real cortical modules)
//=============================================================================

/**
 * @brief Set cortical effects directly for testing
 *
 * WHAT: Injects cortical effects without connected modules
 * WHY: Enables testing of modulation behavior without full system
 * HOW: Copies provided effects struct into bridge
 *
 * NOTE: This function is intended for testing only. In production,
 * use the proper cortical module connection APIs.
 *
 * @param bridge Bridge to update
 * @param effects Effects to set
 * @return 0 on success, negative on error
 */
int cortical_training_set_effects_for_testing(
    cortical_training_bridge_t* bridge,
    const cortical_training_effects_t* effects
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_TRAINING_BRIDGE_H */
