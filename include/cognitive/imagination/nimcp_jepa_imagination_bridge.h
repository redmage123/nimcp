/**
 * @file nimcp_jepa_imagination_bridge.h
 * @brief JEPA-Imagination Bidirectional Bridge
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Bidirectional bridge connecting JEPA (predictive model) with imagination engine
 * WHY:  JEPA provides world model predictions; imagination uses them for counterfactual simulation
 * HOW:  Full bridge pattern with effects in both directions
 *
 * BIOLOGICAL BASIS:
 * The predictive coding framework and imagination are deeply intertwined:
 * - World models generate predictions about future states
 * - Imagination simulates counterfactual outcomes using the world model
 * - Imagined outcomes provide training signal for improving predictions
 * - Latent space predictions constrain imagination to plausible scenarios
 *
 * ARCHITECTURE:
 * ```
 * +----------------------+                    +----------------------+
 * |        JEPA          |                    |  IMAGINATION ENGINE  |
 * |                      |                    |                      |
 * | * Latent predictor   |<-- latent preds -->| * Scenario manager   |
 * | * Context encoder    |    world queries   | * Latent space       |
 * | * World model        |                    | * World model        |
 * | * Masking            |<-- training sig -->| * Visual generation  |
 * | * Multimodal fusion  |    pred targets    | * Prospective sim    |
 * |                      |                    |                      |
 * +----------------------+                    +----------------------+
 *           |                                           |
 *           +---------------- BRIDGE -------------------+
 *                    (bidirectional effects)
 * ```
 *
 * EFFECTS:
 * - JEPA -> Imagination:
 *   * Latent predictions provide future state estimates for simulation
 *   * World model queries return expected outcomes of imagined actions
 *   * Counterfactual constraints limit imagination to plausible scenarios
 *
 * - Imagination -> JEPA:
 *   * Training signal from imagined outcomes improves prediction accuracy
 *   * Prediction targets from simulation scenarios
 *   * Novelty signals identify where world model needs improvement
 *
 * USAGE:
 * ```c
 * jepa_imagination_bridge_t* bridge = jepa_imagination_bridge_create(NULL);
 * jepa_imagination_connect_jepa(bridge, jepa_system);
 * jepa_imagination_connect_imagination(bridge, imagination_engine);
 *
 * // In update loop:
 * jepa_imagination_update(bridge, delta_time);
 *
 * // Request prediction-constrained imagination
 * jepa_imagination_request_predicted_imagination(bridge, context, &goal);
 * ```
 */

#ifndef NIMCP_JEPA_IMAGINATION_BRIDGE_H
#define NIMCP_JEPA_IMAGINATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/tensor/nimcp_tensor.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct jepa_latent;
struct jepa_predictor;
struct imagination_engine;
struct imagination_scenario;
struct imagination_goal;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum latent predictions to track */
#define JEPA_IMAG_MAX_PREDICTIONS           8

/** Maximum active counterfactual scenarios */
#define JEPA_IMAG_MAX_COUNTERFACTUALS       4

/** Default prediction confidence threshold */
#define JEPA_IMAG_DEFAULT_CONFIDENCE_THRESHOLD  0.4f

/** Default learning rate from imagined outcomes */
#define JEPA_IMAG_DEFAULT_LEARNING_RATE     0.01f

/*=============================================================================
 * EFFECTS STRUCTURES
 *===========================================================================*/

/**
 * @brief Effects flowing from JEPA to imagination
 *
 * WHAT: Prediction-derived constraints and content for imagination
 * WHY:  Imagination should be grounded in world model predictions
 */
typedef struct {
    /* Latent prediction effects */
    float prediction_confidence;         /**< Confidence in latent predictions [0.0-1.0] */
    float world_model_coherence;         /**< World model consistency score */
    float counterfactual_plausibility;   /**< Plausibility of counterfactuals [0.0-1.0] */

    /* Predicted latent content */
    uint32_t num_predictions;            /**< Number of active predictions */
    nimcp_tensor_t* predicted_latents;   /**< Stack of predicted latent states */
    float prediction_horizons[JEPA_IMAG_MAX_PREDICTIONS]; /**< Time horizon per prediction */
    float prediction_variances[JEPA_IMAG_MAX_PREDICTIONS]; /**< Uncertainty per prediction */

    /* World model query results */
    float action_outcome_likelihood;     /**< Likelihood of imagined action outcome */
    nimcp_tensor_t* outcome_embedding;   /**< Predicted outcome in latent space */

    /* Constraint signals */
    bool constrain_to_predictions;       /**< Whether to constrain imagination */
    float constraint_strength;           /**< How strongly to enforce constraints [0.0-1.0] */
} jepa_to_imagination_effects_t;

/**
 * @brief Effects flowing from imagination to JEPA
 *
 * WHAT: Imagination-derived training signals for JEPA
 * WHY:  Imagined scenarios provide self-supervised learning targets
 */
typedef struct {
    /* Training signals */
    float learning_strength;             /**< Strength of learning signal [0.0-1.0] */
    float prediction_error;              /**< Error between predicted and imagined [0.0-1.0] */
    bool update_world_model;             /**< Should update JEPA world model */

    /* Prediction targets */
    uint32_t scenario_id;                /**< Source scenario ID */
    nimcp_tensor_t* target_embedding;    /**< Target latent from imagination */
    nimcp_tensor_t* context_embedding;   /**< Context that led to this target */

    /* Novelty and exploration */
    float novelty_score;                 /**< How novel the imagined content is */
    float exploration_value;             /**< Value of exploring this prediction space */

    /* Counterfactual feedback */
    bool is_counterfactual;              /**< Whether from counterfactual simulation */
    float counterfactual_divergence;     /**< How far from predicted reality */
} imagination_to_jepa_effects_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Prediction parameters */
    float confidence_threshold;          /**< Minimum confidence for predictions [0.0-1.0] */
    uint32_t max_prediction_horizon;     /**< Maximum steps to predict ahead */
    float prediction_decay;              /**< Confidence decay per step [0.0-1.0] */

    /* Training parameters */
    float learning_rate;                 /**< Learning rate from imagined outcomes */
    float novelty_bonus;                 /**< Bonus for novel predictions [0.0-1.0] */
    bool enable_counterfactual_training; /**< Train on counterfactual scenarios */

    /* Constraint parameters */
    bool enable_prediction_constraints;  /**< Constrain imagination to predictions */
    float constraint_softness;           /**< How soft the constraints are [0.0-1.0] */

    /* Update frequency */
    float update_interval_ms;            /**< Minimum time between updates */

    /* Bio-async */
    bool enable_bio_async;               /**< Enable bio-async messaging */
} jepa_imagination_config_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Prediction stats */
    uint64_t predictions_generated;      /**< Total predictions generated */
    uint64_t predictions_used;           /**< Predictions used by imagination */
    float avg_prediction_confidence;     /**< Average prediction confidence */

    /* Training stats */
    uint64_t training_signals_sent;      /**< Training signals to JEPA */
    uint64_t world_model_updates;        /**< World model update triggers */
    float avg_prediction_error;          /**< Average prediction error */

    /* Counterfactual stats */
    uint64_t counterfactuals_simulated;  /**< Counterfactual scenarios run */
    float avg_counterfactual_divergence; /**< Average divergence from prediction */

    /* Timing */
    uint64_t total_updates;              /**< Total update calls */
    float avg_update_time_ms;            /**< Average update time */
} jepa_imagination_stats_t;

/*=============================================================================
 * MAIN BRIDGE STRUCTURE
 *===========================================================================*/

/**
 * @brief JEPA-Imagination bridge
 *
 * Coordinates bidirectional communication between JEPA and imagination.
 */
typedef struct jepa_imagination_bridge {
    bridge_base_t base;                  /**< MUST be first - base bridge infrastructure */

    /* Connected systems (typed for convenience, also in base) */
    struct jepa_predictor* jepa;
    struct imagination_engine* imagination;

    /* Bidirectional effects */
    jepa_to_imagination_effects_t jepa_to_imag;
    imagination_to_jepa_effects_t imag_to_jepa;

    /* Configuration */
    jepa_imagination_config_t config;

    /* State tracking */
    uint32_t active_counterfactuals[JEPA_IMAG_MAX_COUNTERFACTUALS];
    uint32_t num_active_counterfactuals;

    /* Pending operations */
    bool prediction_request_pending;
    nimcp_tensor_t* pending_context;

    /* Statistics */
    jepa_imagination_stats_t stats;

    /* Timing */
    uint64_t last_update_time_ms;
} jepa_imagination_bridge_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int jepa_imagination_default_config(jepa_imagination_config_t* config);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int jepa_imagination_validate_config(const jepa_imagination_config_t* config);

/**
 * @brief Create bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on error
 */
jepa_imagination_bridge_t* jepa_imagination_bridge_create(
    const jepa_imagination_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void jepa_imagination_bridge_destroy(jepa_imagination_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * Clears effects and pending requests, keeps connections and config.
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int jepa_imagination_reset(jepa_imagination_bridge_t* bridge);

/*=============================================================================
 * CONNECTION API
 *===========================================================================*/

/**
 * @brief Connect JEPA predictor
 *
 * @param bridge Bridge
 * @param jepa JEPA predictor to connect
 * @return 0 on success, -1 on error
 */
int jepa_imagination_connect_jepa(
    jepa_imagination_bridge_t* bridge,
    struct jepa_predictor* jepa);

/**
 * @brief Connect imagination engine
 *
 * @param bridge Bridge
 * @param imagination Imagination engine to connect
 * @return 0 on success, -1 on error
 */
int jepa_imagination_connect_imagination(
    jepa_imagination_bridge_t* bridge,
    struct imagination_engine* imagination);

/**
 * @brief Disconnect JEPA
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int jepa_imagination_disconnect_jepa(jepa_imagination_bridge_t* bridge);

/**
 * @brief Disconnect imagination
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int jepa_imagination_disconnect_imagination(jepa_imagination_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge
 * @return true if both systems connected
 */
bool jepa_imagination_is_connected(const jepa_imagination_bridge_t* bridge);

/*=============================================================================
 * UPDATE API
 *===========================================================================*/

/**
 * @brief Main update function
 *
 * Computes and applies effects in both directions.
 *
 * @param bridge Bridge
 * @param delta_time_ms Time since last update in milliseconds
 * @return 0 on success, -1 on error
 */
int jepa_imagination_update(
    jepa_imagination_bridge_t* bridge,
    float delta_time_ms);

/**
 * @brief Compute JEPA -> imagination effects only
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int jepa_imagination_compute_jepa_effects(jepa_imagination_bridge_t* bridge);

/**
 * @brief Compute imagination -> JEPA effects only
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int jepa_imagination_compute_imag_effects(jepa_imagination_bridge_t* bridge);

/**
 * @brief Apply all computed effects to connected systems
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int jepa_imagination_apply_effects(jepa_imagination_bridge_t* bridge);

/*=============================================================================
 * JEPA-IMAGINATION INTEGRATION API
 *===========================================================================*/

/**
 * @brief Request prediction-constrained imagination
 *
 * Uses JEPA predictions to constrain imagination to plausible scenarios.
 *
 * @param bridge Bridge
 * @param context Current context for prediction
 * @param goal Goal for the imagination scenario
 * @return Scenario ID on success, 0 on failure
 */
uint32_t jepa_imagination_request_predicted_imagination(
    jepa_imagination_bridge_t* bridge,
    const nimcp_tensor_t* context,
    struct imagination_goal* goal);

/**
 * @brief Request counterfactual simulation
 *
 * Imagines alternative outcomes that diverge from JEPA predictions.
 *
 * @param bridge Bridge
 * @param context Current context
 * @param action Alternative action to simulate
 * @return Scenario ID on success, 0 on failure
 */
uint32_t jepa_imagination_request_counterfactual(
    jepa_imagination_bridge_t* bridge,
    const nimcp_tensor_t* context,
    const nimcp_tensor_t* action);

/**
 * @brief Query world model for action outcome
 *
 * Uses JEPA to predict outcome of an imagined action.
 *
 * @param bridge Bridge
 * @param context Current context
 * @param action Action to evaluate
 * @param outcome Output predicted outcome embedding
 * @return 0 on success, -1 on error
 */
int jepa_imagination_query_world_model(
    jepa_imagination_bridge_t* bridge,
    const nimcp_tensor_t* context,
    const nimcp_tensor_t* action,
    nimcp_tensor_t** outcome);

/**
 * @brief Provide training signal from imagined scenario
 *
 * Sends imagined outcome as training target for JEPA.
 *
 * @param bridge Bridge
 * @param scenario Imagination scenario to learn from
 * @param emotional_weight Importance of this learning [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int jepa_imagination_provide_training_signal(
    jepa_imagination_bridge_t* bridge,
    const struct imagination_scenario* scenario,
    float emotional_weight);

/**
 * @brief Get current JEPA contribution to imagination
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int jepa_imagination_get_jepa_effects(
    const jepa_imagination_bridge_t* bridge,
    jepa_to_imagination_effects_t* effects);

/**
 * @brief Get current imagination contribution to JEPA
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int jepa_imagination_get_imagination_effects(
    const jepa_imagination_bridge_t* bridge,
    imagination_to_jepa_effects_t* effects);

/*=============================================================================
 * QUERY API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int jepa_imagination_get_stats(
    const jepa_imagination_bridge_t* bridge,
    jepa_imagination_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int jepa_imagination_reset_stats(jepa_imagination_bridge_t* bridge);

/**
 * @brief Get number of active counterfactual simulations
 *
 * @param bridge Bridge
 * @return Number of active counterfactuals
 */
uint32_t jepa_imagination_get_counterfactual_count(
    const jepa_imagination_bridge_t* bridge);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int jepa_imagination_connect_bio_async(jepa_imagination_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int jepa_imagination_disconnect_bio_async(jepa_imagination_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge
 * @return true if connected
 */
bool jepa_imagination_is_bio_async_connected(
    const jepa_imagination_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge
 * @return Number of messages processed
 */
int jepa_imagination_process_messages(jepa_imagination_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_IMAGINATION_BRIDGE_H */
