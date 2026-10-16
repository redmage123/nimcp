/**
 * @file nimcp_mirror_omni_bridge.h
 * @brief Mirror Neurons - Omnidirectional Cognitive Integration Bridge
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Bidirectional integration between mirror neurons and omnidirectional cognitive modules
 * WHY:  Mirror neurons provide social learning and action understanding that feeds into
 *       omnidirectional world models and active inference systems
 * HOW:  Mirror neuron observations inform agent state predictions in world models;
 *       mirror confidence modulates active inference precision; counterfactual
 *       simulation evaluates imitation outcomes
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * MIRROR NEURONS AND PREDICTIVE COGNITION:
 * ----------------------------------------
 * Mirror neurons in premotor and parietal cortex activate both during action
 * execution and observation. This dual representation provides:
 *
 * 1. Agent State Prediction:
 *    - Observe action -> predict agent's internal state
 *    - Feed predictions to world model for trajectory planning
 *    - Enable theory of mind through state inference
 *
 * 2. Action Prior Formation:
 *    - Observation history -> action likelihood priors
 *    - Inform active inference policy evaluation
 *    - Reduce search space for imitation learning
 *
 * 3. Imitation Cost Evaluation:
 *    - Active inference queries mirror neurons for imitation feasibility
 *    - Compare expected cost of imitation vs. exploration
 *    - Precision-weighted policy selection
 *
 * 4. Counterfactual Imitation Simulation:
 *    - World model simulates outcomes of potential imitation
 *    - Mirror neurons validate action-goal consistency
 *    - Support offline imitation learning through mental simulation
 *
 * OMNIDIRECTIONAL INFERENCE:
 * --------------------------
 * Standard inference is forward-looking (predict future).
 * Omnidirectional inference adds:
 *
 *   - FORWARD: What will happen if I imitate this action?
 *   - BACKWARD: What action led to this observed outcome?
 *   - LATERAL: How does this action affect other agents/modalities?
 *
 * ARCHITECTURE:
 * ```
 * +============================================================================+
 * |                    MIRROR-OMNI BRIDGE                                       |
 * +============================================================================+
 * |                                                                             |
 * |   +--------------------------------------------------------------------+   |
 * |   |                  MIRROR -> WORLD MODEL                              |   |
 * |   |                                                                     |   |
 * |   |   Mirror Observations -> Agent State Predictions                   |   |
 * |   |   Action Recognition -> State Transition Priors                    |   |
 * |   |   Goal Inference -> Hierarchical State Updates                     |   |
 * |   +--------------------------------------------------------------------+   |
 * |                                                                             |
 * |   +--------------------------------------------------------------------+   |
 * |   |                  MIRROR -> ACTIVE INFERENCE                         |   |
 * |   |                                                                     |   |
 * |   |   Observation History -> Action Priors P(a)                        |   |
 * |   |   Mirror Confidence -> Inference Precision                         |   |
 * |   |   Imitation Likelihood -> Policy Weighting                         |   |
 * |   +--------------------------------------------------------------------+   |
 * |                                                                             |
 * |   +--------------------------------------------------------------------+   |
 * |   |                  ACTIVE INFERENCE -> MIRROR                         |   |
 * |   |                                                                     |   |
 * |   |   Policy Queries -> Imitation Cost Evaluation                      |   |
 * |   |   Goal-Action Queries -> Motor Program Retrieval                   |   |
 * |   |   Precision Updates -> Mirror Gain Modulation                      |   |
 * |   +--------------------------------------------------------------------+   |
 * |                                                                             |
 * |   +--------------------------------------------------------------------+   |
 * |   |                  WORLD MODEL -> MIRROR                              |   |
 * |   |                                                                     |   |
 * |   |   Counterfactual Queries -> Imitation Outcome Simulation           |   |
 * |   |   State Predictions -> Action Expectation Updates                  |   |
 * |   |   Trajectory Rollouts -> Motor Sequence Validation                 |   |
 * |   +--------------------------------------------------------------------+   |
 * |                                                                             |
 * +============================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MIRROR_OMNI_BRIDGE_H
#define NIMCP_MIRROR_OMNI_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "cognitive/omni/nimcp_omni_active_inference.h"
#include "cognitive/omni/nimcp_omni_world_model.h"

/* Forward declaration for health agent */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Note: BIO_MODULE_MIRROR_OMNI_BRIDGE (0x0279) is defined in nimcp_bio_messages.h */

/** @brief Bio-async message types */
#define BIO_MSG_MIRROR_OMNI_AGENT_STATE    0x2791  /**< Agent state update */
#define BIO_MSG_MIRROR_OMNI_ACTION_PRIOR   0x2792  /**< Action prior update */
#define BIO_MSG_MIRROR_OMNI_IMITATION_COST 0x2793  /**< Imitation cost query */
#define BIO_MSG_MIRROR_OMNI_COUNTERFACTUAL 0x2794  /**< Counterfactual result */
#define BIO_MSG_MIRROR_OMNI_PRECISION      0x2795  /**< Precision modulation */

/* Mirror-to-WorldModel coupling constants */
#define MIRROR_OMNI_STATE_COUPLING_RATE    0.4f    /**< State prediction rate */
#define MIRROR_OMNI_STATE_CONFIDENCE_MIN   0.3f    /**< Min confidence for state update */

/* Mirror-to-ActiveInference coupling constants */
#define MIRROR_OMNI_PRIOR_COUPLING_RATE    0.3f    /**< Action prior update rate */
#define MIRROR_OMNI_PRIOR_DECAY_RATE       0.1f    /**< Prior decay per timestep */
#define MIRROR_OMNI_PRECISION_GAIN_MIN     0.5f    /**< Min precision gain */
#define MIRROR_OMNI_PRECISION_GAIN_MAX     1.5f    /**< Max precision gain */

/* ActiveInference-to-Mirror coupling constants */
#define MIRROR_OMNI_IMITATION_THRESHOLD    0.6f    /**< Min imitation cost threshold */
#define MIRROR_OMNI_POLICY_WEIGHT_MIN      0.1f    /**< Min policy weight */

/* WorldModel-to-Mirror coupling constants */
#define MIRROR_OMNI_CF_HORIZON_DEFAULT     5       /**< Default counterfactual horizon */
#define MIRROR_OMNI_CF_DIVERGENCE_MAX      0.8f    /**< Max acceptable divergence */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Omnidirectional inference direction
 */
typedef enum {
    MIRROR_OMNI_DIR_FORWARD = 0,     /**< Forward: predict future from action */
    MIRROR_OMNI_DIR_BACKWARD,        /**< Backward: infer action from outcome */
    MIRROR_OMNI_DIR_LATERAL,         /**< Lateral: cross-agent/modal effects */
    MIRROR_OMNI_DIR_COUNT
} mirror_omni_direction_t;

/**
 * @brief Imitation evaluation result
 */
typedef enum {
    MIRROR_OMNI_IMITATE_SUCCESS = 0, /**< Imitation recommended */
    MIRROR_OMNI_IMITATE_COSTLY,      /**< High cost, consider alternatives */
    MIRROR_OMNI_IMITATE_INFEASIBLE,  /**< Cannot imitate this action */
    MIRROR_OMNI_IMITATE_UNKNOWN      /**< Insufficient data */
} mirror_omni_imitation_result_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mirror_omni_bridge mirror_omni_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for mirror-omni bridge
 */
typedef struct {
    /* Mirror -> World Model coupling */
    float state_coupling_rate;           /**< Agent state prediction rate */
    float state_confidence_threshold;    /**< Min confidence for state updates */
    bool enable_hierarchical_states;     /**< Update hierarchical states */

    /* Mirror -> Active Inference coupling */
    float prior_coupling_rate;           /**< Action prior update rate */
    float prior_decay_rate;              /**< Prior decay per timestep */
    float precision_gain_min;            /**< Minimum precision gain */
    float precision_gain_max;            /**< Maximum precision gain */
    bool enable_confidence_precision;    /**< Mirror confidence -> AI precision */

    /* Active Inference -> Mirror coupling */
    float imitation_threshold;           /**< Min imitation cost threshold */
    float policy_weight_min;             /**< Minimum policy weight */
    bool enable_imitation_queries;       /**< Allow imitation cost queries */

    /* World Model -> Mirror coupling */
    uint32_t counterfactual_horizon;     /**< Counterfactual simulation steps */
    float divergence_threshold;          /**< Max acceptable CF divergence */
    bool enable_counterfactual;          /**< Enable CF imitation simulation */

    /* General settings */
    bool enable_omnidirectional;         /**< Enable all inference directions */
    bool enable_bio_async;               /**< Enable bio-async messaging */
} mirror_omni_config_t;

/**
 * @brief Agent state prediction from mirror observations
 */
typedef struct {
    uint32_t agent_id;                   /**< Observed agent ID */
    float* predicted_state;              /**< Predicted internal state */
    uint32_t state_dim;                  /**< State dimensionality */
    float confidence;                    /**< Prediction confidence */
    uint64_t timestamp;                  /**< When prediction was made */
    uint32_t action_id;                  /**< Action being performed */
    float goal_probability;              /**< Inferred goal probability */
} mirror_omni_agent_state_t;

/**
 * @brief Action prior from observation history
 */
typedef struct {
    uint32_t action_id;                  /**< Action identifier */
    float prior_probability;             /**< P(action) from observations */
    float observation_count;             /**< Times this action observed */
    float recency_weight;                /**< Recent observation weighting */
    float success_rate;                  /**< Observed success rate */
} mirror_omni_action_prior_t;

/**
 * @brief Imitation cost evaluation
 */
typedef struct {
    uint32_t action_id;                  /**< Action to evaluate */
    float motor_cost;                    /**< Motor execution cost */
    float learning_cost;                 /**< Learning difficulty */
    float expected_reward;               /**< Expected reward from imitation */
    float risk;                          /**< Risk of failed imitation */
    float total_cost;                    /**< Combined cost score */
    mirror_omni_imitation_result_t result; /**< Evaluation result */
} mirror_omni_imitation_cost_t;

/**
 * @brief Counterfactual imitation simulation result
 */
typedef struct {
    uint32_t action_id;                  /**< Simulated action */
    uint32_t horizon;                    /**< Simulation horizon */
    float expected_reward;               /**< Expected cumulative reward */
    float divergence;                    /**< Divergence from observed */
    float goal_achievement;              /**< Goal achievement probability */
    float* predicted_trajectory;         /**< Predicted state trajectory */
    uint32_t trajectory_length;          /**< Length of trajectory */
    bool feasible;                       /**< Whether imitation is feasible */
} mirror_omni_cf_result_t;

/**
 * @brief Effects of mirror-omni coupling
 */
typedef struct {
    /* Mirror -> World Model effects */
    float agent_state_prediction_strength;
    float state_update_magnitude;
    uint32_t agents_tracked;

    /* Mirror -> Active Inference effects */
    float action_prior_strength;
    float precision_modulation;
    uint32_t priors_active;

    /* Active Inference -> Mirror effects */
    float imitation_query_rate;
    float avg_imitation_cost;

    /* World Model -> Mirror effects */
    float counterfactual_utilization;
    float avg_cf_divergence;
} mirror_omni_effects_t;

/**
 * @brief Current state of mirror-omni interaction
 */
typedef struct {
    /* Connection state */
    bool mirror_connected;
    bool world_model_connected;
    bool active_inference_connected;

    /* Mirror neuron state */
    uint32_t active_mirror_neurons;
    float max_observation_activation;
    uint32_t observed_agents;

    /* Coupling state */
    uint32_t state_predictions_made;
    uint32_t action_priors_generated;
    uint32_t imitation_queries_handled;
    uint32_t counterfactuals_simulated;

    /* Directional inference state */
    float forward_inference_weight;
    float backward_inference_weight;
    float lateral_inference_weight;

    /* Quality metrics */
    float avg_prediction_confidence;
    float avg_imitation_success;
} mirror_omni_state_t;

/**
 * @brief Statistics for mirror-omni bridge
 */
typedef struct {
    /* Coupling events */
    uint64_t total_state_predictions;
    uint64_t total_prior_updates;
    uint64_t total_imitation_queries;
    uint64_t total_counterfactuals;

    /* Direction-specific stats */
    uint64_t forward_inferences;
    uint64_t backward_inferences;
    uint64_t lateral_inferences;

    /* Quality metrics */
    float avg_state_confidence;
    float avg_prior_strength;
    float avg_imitation_cost;
    float avg_cf_divergence;

    /* Success metrics */
    float imitation_success_rate;
    float prediction_accuracy;

    /* Performance */
    float avg_update_latency_ms;
    uint64_t bio_async_messages_sent;
    uint64_t bio_async_messages_received;
} mirror_omni_stats_t;

/* Forward declaration for health agent (B22 Upgrade) */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/**
 * @brief Mirror-omni bridge state
 */
struct mirror_omni_bridge {
    bridge_base_t base;                  /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    mirror_omni_config_t config;

    /* Connected systems */
    mirror_neurons_t mirror_system;      /**< Mirror neuron system */
    omni_world_model_t* world_model;     /**< Omnidirectional world model */
    omni_active_inference_t* active_inference; /**< Active inference system */

    /* Cached state */
    mirror_omni_agent_state_t* agent_states;  /**< Tracked agent states */
    uint32_t num_agent_states;
    uint32_t agent_states_capacity;

    mirror_omni_action_prior_t* action_priors; /**< Action prior cache */
    uint32_t num_action_priors;
    uint32_t action_priors_capacity;

    /* Current effects */
    mirror_omni_effects_t effects;
    mirror_omni_state_t state;

    /* Statistics */
    mirror_omni_stats_t stats;

    /* Health agent (instance-level) */
    nimcp_health_agent_t* health_agent;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default mirror-omni configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int mirror_omni_bridge_default_config(mirror_omni_config_t* config);

/**
 * @brief Create mirror-omni bridge
 *
 * WHAT: Initialize mirror-omni integration bridge
 * WHY:  Enable bidirectional mirror-omni interaction
 * HOW:  Allocate bridge, initialize caches, setup state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
mirror_omni_bridge_t* mirror_omni_bridge_create(
    const mirror_omni_config_t* config
);

/**
 * @brief Destroy mirror-omni bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free caches, release memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void mirror_omni_bridge_destroy(mirror_omni_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset statistics and caches, preserve connections
 * WHY:  Allow bridge reuse without reconnection
 * HOW:  Clear caches, reset stats, maintain connections
 *
 * @param bridge Mirror-omni bridge
 * @return 0 on success, -1 on error
 */
int mirror_omni_bridge_reset(mirror_omni_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect mirror neuron system
 *
 * @param bridge Mirror-omni bridge
 * @param mirror Mirror neuron system
 * @return 0 on success, -1 on error
 */
int mirror_omni_bridge_connect_mirror(
    mirror_omni_bridge_t* bridge,
    mirror_neurons_t mirror
);

/**
 * @brief Connect world model
 *
 * @param bridge Mirror-omni bridge
 * @param world_model Omnidirectional world model
 * @return 0 on success, -1 on error
 */
int mirror_omni_bridge_connect_world_model(
    mirror_omni_bridge_t* bridge,
    omni_world_model_t* world_model
);

/**
 * @brief Connect active inference system
 *
 * @param bridge Mirror-omni bridge
 * @param active_inference Active inference system
 * @return 0 on success, -1 on error
 */
int mirror_omni_bridge_connect_active_inference(
    mirror_omni_bridge_t* bridge,
    omni_active_inference_t* active_inference
);

/**
 * @brief Check if all systems are connected
 *
 * @param bridge Mirror-omni bridge
 * @return true if all systems connected
 */
bool mirror_omni_bridge_is_fully_connected(
    const mirror_omni_bridge_t* bridge
);

/* ============================================================================
 * Mirror -> World Model API
 * ============================================================================ */

/**
 * @brief Feed agent state predictions from mirror observations
 *
 * WHAT: Use mirror neuron observations to predict agent internal states
 * WHY:  Enable world model to track other agents' states
 * HOW:  Extract observation features, infer state, update world model
 *
 * @param bridge Mirror-omni bridge
 * @param agent_id Agent being observed
 * @return 0 on success, -1 on error
 */
int mirror_omni_feed_agent_state(
    mirror_omni_bridge_t* bridge,
    uint32_t agent_id
);

/**
 * @brief Update world model with mirror-derived state transitions
 *
 * WHAT: Provide state transition priors from observation
 * WHY:  Improve world model predictions for social dynamics
 * HOW:  Convert action observations to transition probabilities
 *
 * @param bridge Mirror-omni bridge
 * @return 0 on success, -1 on error
 */
int mirror_omni_update_state_transitions(
    mirror_omni_bridge_t* bridge
);

/**
 * @brief Get predicted agent state
 *
 * @param bridge Mirror-omni bridge
 * @param agent_id Agent to query
 * @param state Output state (caller allocates)
 * @return 0 on success, -1 on error
 */
int mirror_omni_get_agent_state(
    const mirror_omni_bridge_t* bridge,
    uint32_t agent_id,
    mirror_omni_agent_state_t* state
);

/* ============================================================================
 * Mirror -> Active Inference API
 * ============================================================================ */

/**
 * @brief Provide action priors from observation history
 *
 * WHAT: Generate action probability priors from observed actions
 * WHY:  Inform active inference policy evaluation
 * HOW:  Aggregate observation history into prior distribution
 *
 * @param bridge Mirror-omni bridge
 * @return 0 on success, -1 on error
 */
int mirror_omni_provide_action_priors(
    mirror_omni_bridge_t* bridge
);

/**
 * @brief Modulate active inference precision from mirror confidence
 *
 * WHAT: Use mirror neuron confidence to modulate inference precision
 * WHY:  High confidence in observations -> precise inference
 * HOW:  Map mirror confidence to precision weights
 *
 * @param bridge Mirror-omni bridge
 * @return 0 on success, -1 on error
 */
int mirror_omni_modulate_precision(
    mirror_omni_bridge_t* bridge
);

/**
 * @brief Get action prior for specific action
 *
 * @param bridge Mirror-omni bridge
 * @param action_id Action to query
 * @param prior Output prior (caller allocates)
 * @return 0 on success, -1 on error
 */
int mirror_omni_get_action_prior(
    const mirror_omni_bridge_t* bridge,
    uint32_t action_id,
    mirror_omni_action_prior_t* prior
);

/* ============================================================================
 * Active Inference -> Mirror API
 * ============================================================================ */

/**
 * @brief Evaluate imitation cost for policy
 *
 * WHAT: Query mirror neurons for imitation feasibility and cost
 * WHY:  Include imitation cost in policy evaluation
 * HOW:  Assess motor cost, learning difficulty, expected reward
 *
 * @param bridge Mirror-omni bridge
 * @param action_id Action to evaluate
 * @param cost Output cost evaluation (caller allocates)
 * @return 0 on success, -1 on error
 */
int mirror_omni_evaluate_imitation_cost(
    mirror_omni_bridge_t* bridge,
    uint32_t action_id,
    mirror_omni_imitation_cost_t* cost
);

/**
 * @brief Query mirror neurons for goal-action mapping
 *
 * WHAT: Retrieve motor programs associated with a goal
 * WHY:  Support goal-directed imitation
 * HOW:  Query mirror hierarchy for goal-motor bindings
 *
 * @param bridge Mirror-omni bridge
 * @param goal_id Goal to achieve
 * @param actions Output action IDs (caller allocates)
 * @param max_actions Maximum actions to return
 * @param num_actions Output: actual number of actions
 * @return 0 on success, -1 on error
 */
int mirror_omni_query_goal_actions(
    mirror_omni_bridge_t* bridge,
    uint32_t goal_id,
    uint32_t* actions,
    uint32_t max_actions,
    uint32_t* num_actions
);

/* ============================================================================
 * World Model -> Mirror API
 * ============================================================================ */

/**
 * @brief Simulate counterfactual imitation outcomes
 *
 * WHAT: Use world model to simulate what-if imitation scenarios
 * WHY:  Evaluate imitation before committing
 * HOW:  Roll out world model with hypothetical imitation action
 *
 * @param bridge Mirror-omni bridge
 * @param action_id Action to simulate imitating
 * @param horizon Simulation horizon
 * @param result Output simulation result (caller allocates)
 * @return 0 on success, -1 on error
 */
int mirror_omni_simulate_counterfactual(
    mirror_omni_bridge_t* bridge,
    uint32_t action_id,
    uint32_t horizon,
    mirror_omni_cf_result_t* result
);

/**
 * @brief Update action expectations from world model predictions
 *
 * WHAT: Use world model state predictions to update mirror expectations
 * WHY:  Improve action anticipation through world modeling
 * HOW:  Feed predicted states back to mirror neuron system
 *
 * @param bridge Mirror-omni bridge
 * @return 0 on success, -1 on error
 */
int mirror_omni_update_action_expectations(
    mirror_omni_bridge_t* bridge
);

/**
 * @brief Validate motor sequence with trajectory rollout
 *
 * WHAT: Use world model to validate mirror-derived motor sequences
 * WHY:  Ensure imitation plans are feasible
 * HOW:  Roll out motor sequence in world model, check constraints
 *
 * @param bridge Mirror-omni bridge
 * @param action_ids Motor sequence to validate
 * @param num_actions Number of actions in sequence
 * @param feasible Output: whether sequence is feasible
 * @param expected_reward Output: expected reward (can be NULL)
 * @return 0 on success, -1 on error
 */
int mirror_omni_validate_motor_sequence(
    mirror_omni_bridge_t* bridge,
    const uint32_t* action_ids,
    uint32_t num_actions,
    bool* feasible,
    float* expected_reward
);

/* ============================================================================
 * Omnidirectional Inference API
 * ============================================================================ */

/**
 * @brief Perform omnidirectional inference update
 *
 * WHAT: Run inference in all enabled directions
 * WHY:  Comprehensive state/action understanding
 * HOW:  Forward + backward + lateral inference
 *
 * @param bridge Mirror-omni bridge
 * @return 0 on success, -1 on error
 */
int mirror_omni_omni_inference(
    mirror_omni_bridge_t* bridge
);

/**
 * @brief Perform forward inference
 *
 * WHAT: Predict future states/actions from current observations
 * WHY:  Anticipate agent behavior
 * HOW:  Use mirror observations to predict forward trajectory
 *
 * @param bridge Mirror-omni bridge
 * @param agent_id Agent to predict
 * @return 0 on success, -1 on error
 */
int mirror_omni_forward_inference(
    mirror_omni_bridge_t* bridge,
    uint32_t agent_id
);

/**
 * @brief Perform backward inference
 *
 * WHAT: Infer past actions from current state
 * WHY:  Understand action causality
 * HOW:  Use world model backward dynamics with mirror evidence
 *
 * @param bridge Mirror-omni bridge
 * @param agent_id Agent to analyze
 * @return 0 on success, -1 on error
 */
int mirror_omni_backward_inference(
    mirror_omni_bridge_t* bridge,
    uint32_t agent_id
);

/**
 * @brief Perform lateral inference
 *
 * WHAT: Infer cross-agent/modal action effects
 * WHY:  Understand social dynamics and multi-agent coordination
 * HOW:  Use lateral world model dynamics with mirror social context
 *
 * @param bridge Mirror-omni bridge
 * @return 0 on success, -1 on error
 */
int mirror_omni_lateral_inference(
    mirror_omni_bridge_t* bridge
);

/**
 * @brief Set inference direction weights
 *
 * WHAT: Adjust relative importance of inference directions
 * WHY:  Task-specific emphasis on forward/backward/lateral
 * HOW:  Normalize and apply weights to inference outputs
 *
 * @param bridge Mirror-omni bridge
 * @param forward_weight Weight for forward inference [0,1]
 * @param backward_weight Weight for backward inference [0,1]
 * @param lateral_weight Weight for lateral inference [0,1]
 * @return 0 on success, -1 on error
 */
int mirror_omni_set_direction_weights(
    mirror_omni_bridge_t* bridge,
    float forward_weight,
    float backward_weight,
    float lateral_weight
);

/* ============================================================================
 * Update Cycle API
 * ============================================================================ */

/**
 * @brief Main update loop for mirror-omni bridge
 *
 * WHAT: Update all mirror-omni couplings
 * WHY:  Keep systems synchronized
 * HOW:  Run all direction-specific updates
 *
 * @param bridge Mirror-omni bridge
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int mirror_omni_bridge_update(
    mirror_omni_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Mirror-omni bridge
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int mirror_omni_bridge_get_state(
    const mirror_omni_bridge_t* bridge,
    mirror_omni_state_t* state
);

/**
 * @brief Get current effects
 *
 * @param bridge Mirror-omni bridge
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int mirror_omni_bridge_get_effects(
    const mirror_omni_bridge_t* bridge,
    mirror_omni_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Mirror-omni bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int mirror_omni_bridge_get_stats(
    const mirror_omni_bridge_t* bridge,
    mirror_omni_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Mirror-omni bridge
 * @return 0 on success, -1 on error
 */
int mirror_omni_bridge_reset_stats(
    mirror_omni_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Mirror-omni bridge
 * @return 0 on success
 */
int mirror_omni_bridge_connect_bio_async(
    mirror_omni_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Mirror-omni bridge
 * @return 0 on success
 */
int mirror_omni_bridge_disconnect_bio_async(
    mirror_omni_bridge_t* bridge
);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Mirror-omni bridge
 * @return true if connected
 */
bool mirror_omni_bridge_is_bio_async_connected(
    const mirror_omni_bridge_t* bridge
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Convert direction to string
 *
 * @param direction Inference direction
 * @return String representation
 */
const char* mirror_omni_direction_to_string(mirror_omni_direction_t direction);

/**
 * @brief Convert imitation result to string
 *
 * @param result Imitation result
 * @return String representation
 */
const char* mirror_omni_imitation_result_to_string(mirror_omni_imitation_result_t result);

/**
 * @brief Free counterfactual result
 *
 * @param result Result to free (NULL safe)
 */
void mirror_omni_cf_result_free(mirror_omni_cf_result_t* result);

/**
 * @brief Free agent state
 *
 * @param state State to free (NULL safe)
 */
void mirror_omni_agent_state_free(mirror_omni_agent_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_OMNI_BRIDGE_H */
