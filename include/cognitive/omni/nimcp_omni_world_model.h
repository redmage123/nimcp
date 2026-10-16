/**
 * @file nimcp_omni_world_model.h
 * @brief Omnidirectional Generative World Model for Mental Simulation
 * @version 1.0.0
 * @date 2026-01-04
 *
 * WHAT: Generative world model for counterfactual reasoning and mental simulation
 * WHY:  Enable "what if" queries and policy evaluation through internal simulation
 * HOW:  Learn transition dynamics in all directions for state prediction
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * GENERATIVE MODELS (Friston, 2010):
 * ----------------------------------
 * The brain maintains an internal generative model of the world:
 *
 *   p(o, s, a) = p(o|s) * p(s|s', a) * p(a)
 *
 * Where:
 *   o = observations
 *   s = hidden states
 *   a = actions
 *   p(o|s) = likelihood (how states generate observations)
 *   p(s|s',a) = transition dynamics (how actions change states)
 *
 * OMNIDIRECTIONAL WORLD MODEL:
 * ----------------------------
 * Standard world models predict forward in time.
 * Omnidirectional world models add:
 *
 *   1. FORWARD DYNAMICS: s_t+1 = f(s_t, a_t)
 *      - Predict future states given actions
 *      - Used for planning and policy evaluation
 *
 *   2. BACKWARD DYNAMICS: s_t-1, a_t-1 = g(s_t)
 *      - Infer past states and actions
 *      - Used for learning from outcomes
 *
 *   3. LATERAL DYNAMICS: s_other = h(s_self, context)
 *      - Cross-modal state prediction
 *      - Used for multi-modal coordination
 *
 *   4. HIERARCHICAL DYNAMICS: s_abstract <-> s_concrete
 *      - Abstract state guides concrete predictions
 *      - Concrete states ground abstract models
 *
 * COUNTERFACTUAL SIMULATION:
 * --------------------------
 * Given current state s and hypothetical action a':
 *   1. Roll out: s' = f(s, a'), s'' = f(s', a''), ...
 *   2. Evaluate: What observations would result?
 *   3. Compare: Is this better than actual trajectory?
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |              OMNIDIRECTIONAL GENERATIVE WORLD MODEL                      |
 * +=========================================================================+
 * |                                                                          |
 * |   +------------------------------------------------------------------+   |
 * |   |                    STATE SPACE                                   |   |
 * |   |   +----------+    +----------+    +----------+    +----------+   |   |
 * |   |   | s_t-2    | -> | s_t-1    | -> | s_t      | -> | s_t+1    |   |   |
 * |   |   +----------+    +----------+    +----------+    +----------+   |   |
 * |   |        ^              ^               ^               ^          |   |
 * |   |        |              |               |               |          |   |
 * |   |   +----+----+    +----+----+    +----+----+    +----+----+       |   |
 * |   |   | a_t-2   |    | a_t-1   |    | a_t     |    | a_t+1   |       |   |
 * |   |   +---------+    +---------+    +---------+    +---------+       |   |
 * |   +------------------------------------------------------------------+   |
 * |                                                                          |
 * |   +------------------+  +------------------+  +------------------+       |
 * |   | FORWARD DYNAMICS |  | BACKWARD DYNAMICS|  | LATERAL DYNAMICS |       |
 * |   | s' = f(s, a)     |  | s,a = g(s')      |  | s_b = h(s_a)     |       |
 * |   +------------------+  +------------------+  +------------------+       |
 * |                                                                          |
 * |   +------------------------------------------------------------------+   |
 * |   |                 COUNTERFACTUAL ENGINE                            |   |
 * |   |   Query: "What if action a' instead of a?"                       |   |
 * |   |   Rollout: s -> s'_1 -> s'_2 -> ... -> s'_n                      |   |
 * |   |   Evaluate: Compare predicted vs preferred outcomes              |   |
 * |   +------------------------------------------------------------------+   |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * INTEGRATION WITH ACTIVE INFERENCE:
 * ----------------------------------
 * The world model enables policy evaluation:
 *   G(pi) = sum_t E_q[log q(s_t) - log p(o_t, s_t | pi)]
 *
 * By simulating trajectories under each policy, we compute expected
 * free energy without actually taking actions.
 *
 * KEY RESEARCH INSIGHTS (Sutskever, LeCun, Hafner 2024-2026):
 * ===========================================================
 *
 * ILYA SUTSKEVER (NeurIPS 2024, SSI):
 * -----------------------------------
 * - "The more AI reasons, the more unpredictable it becomes"
 * - Pre-training creates statistical projection, not true understanding
 * - LLMs lack internal "intuition" about reasoning direction
 * - Need to move beyond pattern matching to genuine world understanding
 * - "Data is the fossil fuel of AI" - we've hit peak data
 *
 * YANN LECUN (JEPA Architecture):
 * -------------------------------
 * - Predict internal representations, not pixels/tokens
 * - Joint Embedding Predictive Architecture for world models
 * - Key modules: Configurator, Perception, World Model, Cost, Memory, Actor
 * - "If you are interested in human-level AI, don't work on LLMs"
 * - V-JEPA 2: Self-supervised video world model with zero-shot planning
 *
 * DREAMERV3 (Hafner et al., Nature 2025):
 * ---------------------------------------
 * - Learn world model from experience, improve by imagining futures
 * - RSSM: Recurrent State Space Model (deterministic + stochastic)
 * - Symlog transformation for reward normalization across scales
 * - Single configuration works across 150+ diverse tasks
 * - First to collect diamonds in Minecraft without human data
 *
 * IMPLEMENTATION APPROACH:
 * ------------------------
 * 1. LATENT STATE SPACE: Compressed representation (not raw observations)
 * 2. RSSM DYNAMICS: h_t = f(h_{t-1}, z_{t-1}, a_{t-1}) deterministic
 *                   z_t ~ p(z_t | h_t) stochastic
 * 3. MDN PREDICTIONS: Mixture density for distribution of outcomes
 * 4. SYMLOG REWARDS: symlog(x) = sign(x) * ln(|x| + 1) for scale invariance
 * 5. DREAMING: Offline simulation for experience replay and consolidation
 */

#ifndef NIMCP_OMNI_WORLD_MODEL_H
#define NIMCP_OMNI_WORLD_MODEL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum state dimensionality */
#define OMNI_WM_MAX_STATE_DIM          256

/** Maximum action dimensionality */
#define OMNI_WM_MAX_ACTION_DIM         64

/** Maximum observation dimensionality */
#define OMNI_WM_MAX_OBS_DIM            256

/** Maximum rollout horizon */
#define OMNI_WM_MAX_HORIZON            32

/** Maximum number of counterfactual queries */
#define OMNI_WM_MAX_COUNTERFACTUALS    16

/** Maximum hierarchical levels */
#define OMNI_WM_MAX_LEVELS             8

/** Maximum latent dimensionality (compressed representation) */
#define OMNI_WM_MAX_LATENT_DIM         128

/** Maximum number of MDN mixture components */
#define OMNI_WM_MAX_MDN_COMPONENTS     8

/** Maximum experience replay buffer size */
#define OMNI_WM_MAX_REPLAY_SIZE        10000

/** Maximum dream episode length */
#define OMNI_WM_MAX_DREAM_LENGTH       50

/** Bio-async module ID for world model */
#define BIO_MODULE_OMNI_WORLD_MODEL    0x0E62

/** Message types */
#define BIO_MSG_OMNI_WM_PREDICT        0x6201
#define BIO_MSG_OMNI_WM_COUNTERFACTUAL 0x6202
#define BIO_MSG_OMNI_WM_UPDATE         0x6203
#define BIO_MSG_OMNI_WM_ROLLOUT        0x6204

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief World model dynamics direction
 */
typedef enum {
    OMNI_WM_DIR_FORWARD     = 0,  /**< Forward dynamics: predict future */
    OMNI_WM_DIR_BACKWARD    = 1,  /**< Backward dynamics: infer past */
    OMNI_WM_DIR_LATERAL     = 2,  /**< Lateral dynamics: cross-modal */
    OMNI_WM_DIR_HIERARCHICAL = 3  /**< Hierarchical: abstract <-> concrete */
} omni_wm_direction_t;

/**
 * @brief Model learning mode
 */
typedef enum {
    OMNI_WM_LEARN_ONLINE    = 0,  /**< Update on each observation */
    OMNI_WM_LEARN_BATCH     = 1,  /**< Batch updates */
    OMNI_WM_LEARN_REPLAY    = 2,  /**< Experience replay */
    OMNI_WM_LEARN_DREAMING  = 3   /**< Offline simulation (dreaming) */
} omni_wm_learn_mode_t;

/**
 * @brief Counterfactual query type
 */
typedef enum {
    OMNI_WM_CF_ACTION       = 0,  /**< What if different action? */
    OMNI_WM_CF_STATE        = 1,  /**< What if different state? */
    OMNI_WM_CF_CONTEXT      = 2,  /**< What if different context? */
    OMNI_WM_CF_GOAL         = 3   /**< What if different goal? */
} omni_wm_counterfactual_type_t;

/**
 * @brief Prediction type for stochastic vs deterministic
 */
typedef enum {
    OMNI_WM_PRED_DETERMINISTIC = 0,  /**< Single point prediction */
    OMNI_WM_PRED_STOCHASTIC    = 1,  /**< Sample from distribution */
    OMNI_WM_PRED_MIXTURE       = 2   /**< MDN mixture density */
} omni_wm_prediction_type_t;

/* ============================================================================
 * RSSM (Recurrent State Space Model) - DreamerV3 inspired
 * ============================================================================ */

/**
 * @brief RSSM state: deterministic (h) + stochastic (z)
 *
 * Following DreamerV3/Dreamer architecture:
 *   h_t = f(h_{t-1}, z_{t-1}, a_{t-1})  [deterministic, captures history]
 *   z_t ~ p(z_t | h_t)                   [stochastic, captures uncertainty]
 *   s_t = concat(h_t, z_t)              [full latent state]
 */
typedef struct {
    float* h;                   /**< Deterministic recurrent state */
    uint32_t h_dim;             /**< Deterministic state dimension */
    float* z;                   /**< Stochastic latent state */
    uint32_t z_dim;             /**< Stochastic state dimension */
    float* z_mean;              /**< Mean of stochastic distribution */
    float* z_std;               /**< Std dev of stochastic distribution */
    double timestamp;           /**< When this state was computed */
} omni_wm_rssm_state_t;

/**
 * @brief MDN (Mixture Density Network) component
 *
 * Models distribution of possible outcomes rather than point predictions.
 * Essential for handling stochastic environments.
 */
typedef struct {
    float weight;               /**< Mixture weight (pi_k) */
    float* mean;                /**< Component mean (mu_k) */
    float* std;                 /**< Component std dev (sigma_k) */
    uint32_t dim;               /**< Dimensionality */
} omni_wm_mdn_component_t;

/**
 * @brief MDN prediction (distribution of outcomes)
 */
typedef struct {
    omni_wm_mdn_component_t* components;  /**< Mixture components */
    uint32_t num_components;              /**< Number of components */
    uint32_t dim;                         /**< Output dimensionality */
} omni_wm_mdn_prediction_t;

/**
 * @brief Latent representation (compressed observation)
 *
 * Following JEPA: predict in latent space, not pixel space.
 * More efficient and focuses on semantically meaningful features.
 */
typedef struct {
    float* embedding;           /**< Latent embedding vector */
    uint32_t dim;               /**< Embedding dimensionality */
    float reconstruction_error; /**< Autoencoder reconstruction loss */
    float information_content;  /**< Bits of information (entropy) */
} omni_wm_latent_t;

/**
 * @brief Experience tuple for replay buffer
 */
typedef struct {
    omni_wm_rssm_state_t* state;    /**< State before action */
    float* action;                   /**< Action taken */
    uint32_t action_dim;             /**< Action dimensionality */
    float reward;                    /**< Reward received */
    float symlog_reward;             /**< Symlog-transformed reward */
    omni_wm_rssm_state_t* next_state; /**< Resulting state */
    float* observation;              /**< Raw observation */
    uint32_t obs_dim;                /**< Observation dimensionality */
    bool terminal;                   /**< Episode terminated? */
    double timestamp;                /**< When this experience occurred */
} omni_wm_experience_t;

/**
 * @brief World state representation (simplified interface)
 */
typedef struct {
    float* values;              /**< State vector */
    uint32_t dim;               /**< State dimensionality */
    float uncertainty;          /**< State uncertainty (entropy) */
    double timestamp;           /**< When this state was estimated */
    uint32_t level;             /**< Hierarchical level (0 = concrete) */
} omni_wm_state_t;

/**
 * @brief State transition (dynamics model output)
 */
typedef struct {
    omni_wm_state_t* next_state;  /**< Predicted next state */
    float* action_taken;          /**< Action that caused transition */
    uint32_t action_dim;          /**< Action dimensionality */
    float log_prob;               /**< Log probability of transition */
    float prediction_error;       /**< Prediction error magnitude */
    omni_wm_direction_t direction; /**< Which dynamics were used */
} omni_wm_transition_t;

/**
 * @brief Counterfactual query
 */
typedef struct {
    omni_wm_counterfactual_type_t type;  /**< Query type */
    omni_wm_state_t* initial_state;      /**< Starting state */
    float* hypothetical_action;           /**< Hypothetical action */
    uint32_t action_dim;                  /**< Action dimensionality */
    uint32_t horizon;                     /**< How far to simulate */
    float* context;                       /**< Optional context */
    uint32_t context_dim;                 /**< Context dimensionality */
} omni_wm_counterfactual_query_t;

/**
 * @brief Counterfactual result
 */
typedef struct {
    omni_wm_state_t** trajectory;  /**< Simulated state trajectory */
    uint32_t trajectory_len;       /**< Number of states in trajectory */
    float expected_reward;         /**< Expected cumulative reward */
    float divergence;              /**< How different from actual? */
    float confidence;              /**< Confidence in prediction */
    float* predicted_obs;          /**< Predicted observations */
    uint32_t obs_dim;              /**< Observation dimensionality */
} omni_wm_counterfactual_result_t;

/**
 * @brief Rollout trajectory for policy evaluation
 */
typedef struct {
    omni_wm_state_t** states;      /**< State sequence */
    float** actions;               /**< Action sequence */
    float** observations;          /**< Observation sequence */
    float* rewards;                /**< Reward sequence */
    uint32_t length;               /**< Trajectory length */
    float total_reward;            /**< Cumulative reward */
    float expected_free_energy;    /**< EFE of this trajectory */
} omni_wm_rollout_t;

/**
 * @brief World model statistics
 */
typedef struct {
    uint64_t forward_predictions;    /**< Forward predictions made */
    uint64_t backward_inferences;    /**< Backward inferences made */
    uint64_t lateral_predictions;    /**< Lateral predictions made */
    uint64_t counterfactual_queries; /**< Counterfactual queries processed */
    uint64_t rollouts_completed;     /**< Policy rollouts completed */
    uint64_t model_updates;          /**< Model parameter updates */
    float mean_prediction_error;     /**< Average prediction error */
    float mean_counterfactual_divergence; /**< Average CF divergence */
} omni_wm_stats_t;

/**
 * @brief World model configuration
 */
typedef struct {
    /* Dimensionality settings */
    uint32_t state_dim;            /**< State dimensionality */
    uint32_t action_dim;           /**< Action dimensionality */
    uint32_t obs_dim;              /**< Observation dimensionality */
    uint32_t hidden_dim;           /**< Hidden layer size for dynamics */
    uint32_t num_levels;           /**< Hierarchical levels */

    /* RSSM configuration (DreamerV3-inspired) */
    uint32_t rssm_h_dim;           /**< Deterministic state dimension */
    uint32_t rssm_z_dim;           /**< Stochastic state dimension */
    uint32_t latent_dim;           /**< Latent embedding dimension */

    /* MDN configuration */
    uint32_t mdn_components;       /**< Number of mixture components */
    omni_wm_prediction_type_t pred_type; /**< Prediction type */

    /* Learning settings */
    float learning_rate;           /**< Learning rate for dynamics */
    float discount_factor;         /**< Discount for future rewards */
    float kl_weight;               /**< KL divergence weight for VAE */
    float reward_scale;            /**< Reward scaling factor */
    omni_wm_learn_mode_t learn_mode; /**< Learning mode */

    /* Experience replay */
    uint32_t replay_buffer_size;   /**< Experience replay buffer size */
    uint32_t batch_size;           /**< Training batch size */
    float priority_exponent;       /**< Prioritized replay exponent */

    /* Dreaming/imagination settings */
    uint32_t dream_horizon;        /**< Imagination rollout length */
    uint32_t dream_episodes;       /**< Episodes per dream cycle */
    float imagination_noise;       /**< Noise for exploration in dreams */

    /* Feature flags */
    bool enable_lateral;           /**< Enable lateral dynamics */
    bool enable_hierarchical;      /**< Enable hierarchical dynamics */
    bool enable_dreaming;          /**< Enable offline simulation */
    bool use_symlog_rewards;       /**< Use symlog for reward normalization */
    bool use_rssm;                 /**< Use RSSM vs simple dynamics */
    bool use_mdn;                  /**< Use MDN for stochastic predictions */
} omni_wm_config_t;

/* Forward declaration */
typedef struct omni_world_model omni_world_model_t;

/* ============================================================================
 * Core API
 * ============================================================================ */

/**
 * @brief Create world model with configuration
 * @param config Configuration (NULL for defaults)
 * @return World model or NULL on failure
 */
omni_world_model_t* omni_wm_create(const omni_wm_config_t* config);

/**
 * @brief Create world model with dimensions
 * @param state_dim State dimensionality
 * @param action_dim Action dimensionality
 * @param obs_dim Observation dimensionality
 * @return World model or NULL on failure
 */
omni_world_model_t* omni_wm_create_simple(uint32_t state_dim,
                                           uint32_t action_dim,
                                           uint32_t obs_dim);

/**
 * @brief Destroy world model
 * @param wm World model to destroy
 */
void omni_wm_destroy(omni_world_model_t* wm);

/**
 * @brief Get default configuration
 * @param config Configuration to fill
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_get_default_config(omni_wm_config_t* config);

/* ============================================================================
 * State Management
 * ============================================================================ */

/**
 * @brief Create a state
 * @param dim State dimensionality
 * @return State or NULL on failure
 */
omni_wm_state_t* omni_wm_state_create(uint32_t dim);

/**
 * @brief Create state from values
 * @param values State values
 * @param dim Dimensionality
 * @return State or NULL on failure
 */
omni_wm_state_t* omni_wm_state_from_values(const float* values, uint32_t dim);

/**
 * @brief Clone a state
 * @param state State to clone
 * @return Cloned state or NULL on failure
 */
omni_wm_state_t* omni_wm_state_clone(const omni_wm_state_t* state);

/**
 * @brief Destroy a state
 * @param state State to destroy
 */
void omni_wm_state_destroy(omni_wm_state_t* state);

/**
 * @brief Set current state
 * @param wm World model
 * @param state Current state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_set_state(omni_world_model_t* wm,
                                 const omni_wm_state_t* state);

/**
 * @brief Get current state
 * @param wm World model
 * @return Current state (do not free) or NULL
 */
const omni_wm_state_t* omni_wm_get_state(const omni_world_model_t* wm);

/* ============================================================================
 * Dynamics Prediction
 * ============================================================================ */

/**
 * @brief Predict next state (forward dynamics)
 * @param wm World model
 * @param action Action to take
 * @param action_dim Action dimensionality
 * @param result Transition result (caller allocates)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_predict_forward(omni_world_model_t* wm,
                                       const float* action,
                                       uint32_t action_dim,
                                       omni_wm_transition_t* result);

/**
 * @brief Infer previous state and action (backward dynamics)
 * @param wm World model
 * @param current_state Current state
 * @param result Transition result (caller allocates)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_infer_backward(omni_world_model_t* wm,
                                      const omni_wm_state_t* current_state,
                                      omni_wm_transition_t* result);

/**
 * @brief Predict lateral state (cross-modal)
 * @param wm World model
 * @param source_state Source modality state
 * @param target_modality Target modality ID
 * @param result Predicted state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_predict_lateral(omni_world_model_t* wm,
                                       const omni_wm_state_t* source_state,
                                       uint32_t target_modality,
                                       omni_wm_state_t* result);

/**
 * @brief Predict hierarchical state
 * @param wm World model
 * @param state Input state
 * @param target_level Target hierarchical level
 * @param result Predicted state at target level
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_predict_hierarchical(omni_world_model_t* wm,
                                            const omni_wm_state_t* state,
                                            uint32_t target_level,
                                            omni_wm_state_t* result);

/* ============================================================================
 * Counterfactual Reasoning
 * ============================================================================ */

/**
 * @brief Create counterfactual query
 * @param type Query type
 * @param initial_state Starting state
 * @param hypothetical_action Hypothetical action
 * @param action_dim Action dimensionality
 * @param horizon Simulation horizon
 * @return Query or NULL on failure
 */
omni_wm_counterfactual_query_t* omni_wm_cf_query_create(
    omni_wm_counterfactual_type_t type,
    const omni_wm_state_t* initial_state,
    const float* hypothetical_action,
    uint32_t action_dim,
    uint32_t horizon);

/**
 * @brief Destroy counterfactual query
 * @param query Query to destroy
 */
void omni_wm_cf_query_destroy(omni_wm_counterfactual_query_t* query);

/**
 * @brief Execute counterfactual query
 * @param wm World model
 * @param query Counterfactual query
 * @param result Result (caller allocates)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_counterfactual(omni_world_model_t* wm,
                                      const omni_wm_counterfactual_query_t* query,
                                      omni_wm_counterfactual_result_t* result);

/**
 * @brief Simple counterfactual: what if this action?
 * @param wm World model
 * @param action Hypothetical action
 * @param action_dim Action dimensionality
 * @param horizon Simulation horizon
 * @param result Result (caller allocates)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_what_if(omni_world_model_t* wm,
                               const float* action,
                               uint32_t action_dim,
                               uint32_t horizon,
                               omni_wm_counterfactual_result_t* result);

/**
 * @brief Destroy counterfactual result
 * @param result Result to destroy
 */
void omni_wm_cf_result_destroy(omni_wm_counterfactual_result_t* result);

/* ============================================================================
 * Policy Rollouts
 * ============================================================================ */

/**
 * @brief Create rollout structure
 * @param max_length Maximum trajectory length
 * @param state_dim State dimensionality
 * @param action_dim Action dimensionality
 * @param obs_dim Observation dimensionality
 * @return Rollout or NULL on failure
 */
omni_wm_rollout_t* omni_wm_rollout_create(uint32_t max_length,
                                           uint32_t state_dim,
                                           uint32_t action_dim,
                                           uint32_t obs_dim);

/**
 * @brief Destroy rollout
 * @param rollout Rollout to destroy
 */
void omni_wm_rollout_destroy(omni_wm_rollout_t* rollout);

/**
 * @brief Execute policy rollout
 * @param wm World model
 * @param policy Policy function (state -> action)
 * @param horizon Rollout horizon
 * @param rollout Result (caller allocates)
 * @param user_data User data for policy function
 * @return NIMCP_SUCCESS or error code
 */
typedef void (*omni_wm_policy_fn)(const omni_wm_state_t* state,
                                   float* action,
                                   void* user_data);

nimcp_error_t omni_wm_rollout(omni_world_model_t* wm,
                               omni_wm_policy_fn policy,
                               uint32_t horizon,
                               omni_wm_rollout_t* rollout,
                               void* user_data);

/**
 * @brief Evaluate expected free energy of rollout
 * @param wm World model
 * @param rollout Rollout to evaluate
 * @param preferred_obs Preferred observations (goal)
 * @param obs_dim Observation dimensionality
 * @return Expected free energy
 */
float omni_wm_evaluate_efe(omni_world_model_t* wm,
                            const omni_wm_rollout_t* rollout,
                            const float* preferred_obs,
                            uint32_t obs_dim);

/* ============================================================================
 * Learning
 * ============================================================================ */

/**
 * @brief Update model with observation
 * @param wm World model
 * @param state Current state
 * @param action Action taken
 * @param action_dim Action dimensionality
 * @param next_state Resulting state
 * @param reward Reward received
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_update(omni_world_model_t* wm,
                              const omni_wm_state_t* state,
                              const float* action,
                              uint32_t action_dim,
                              const omni_wm_state_t* next_state,
                              float reward);

/**
 * @brief Run dreaming (offline simulation)
 * @param wm World model
 * @param num_episodes Number of dream episodes
 * @param episode_length Length of each episode
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_dream(omni_world_model_t* wm,
                             uint32_t num_episodes,
                             uint32_t episode_length);

/**
 * @brief Generate a dream sequence into a rollout structure
 *
 * Produces a single imagined trajectory by rolling out the world model
 * with stochastic noise injection.  If dream_length is 0, falls back
 * to config.dream_horizon.
 *
 * @param wm World model
 * @param dream_length Number of steps (0 = use config.dream_horizon)
 * @param noise_scale  Noise amplitude for stochastic exploration
 * @param rollout      Pre-allocated rollout (via omni_wm_rollout_create)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_generate_dream(omni_world_model_t* wm,
                                      uint32_t dream_length,
                                      float noise_scale,
                                      omni_wm_rollout_t* rollout);

/**
 * @brief Set learning rate
 * @param wm World model
 * @param learning_rate New learning rate
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_set_learning_rate(omni_world_model_t* wm,
                                         float learning_rate);

/* ============================================================================
 * Active Inference Integration
 * ============================================================================ */

/**
 * @brief Connect to active inference system
 * @param wm World model
 * @param ai Active inference context
 * @return NIMCP_SUCCESS or error code
 */
struct omni_active_inference;
nimcp_error_t omni_wm_connect_active_inference(omni_world_model_t* wm,
                                                struct omni_active_inference* ai);

/**
 * @brief Evaluate policy using world model simulation
 * @param wm World model
 * @param policy_actions Policy action sequence
 * @param horizon Policy horizon
 * @param preferred_obs Preferred observations
 * @param obs_dim Observation dimensionality
 * @return Expected free energy of policy
 */
float omni_wm_evaluate_policy(omni_world_model_t* wm,
                               const float* policy_actions,
                               uint32_t horizon,
                               const float* preferred_obs,
                               uint32_t obs_dim);

/* ============================================================================
 * Observation Prediction
 * ============================================================================ */

/**
 * @brief Predict observations from state
 * @param wm World model
 * @param state Hidden state
 * @param predicted_obs Output: predicted observations
 * @param obs_dim Observation dimensionality
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_predict_observations(omni_world_model_t* wm,
                                            const omni_wm_state_t* state,
                                            float* predicted_obs,
                                            uint32_t obs_dim);

/**
 * @brief Infer state from observations
 * @param wm World model
 * @param observations Observations
 * @param obs_dim Observation dimensionality
 * @param inferred_state Output: inferred state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_infer_state(omni_world_model_t* wm,
                                   const float* observations,
                                   uint32_t obs_dim,
                                   omni_wm_state_t* inferred_state);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Get world model statistics
 * @param wm World model
 * @param stats Output: statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_get_stats(const omni_world_model_t* wm,
                                 omni_wm_stats_t* stats);

/**
 * @brief Reset statistics
 * @param wm World model
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_reset_stats(omni_world_model_t* wm);

/* ============================================================================
 * RSSM API (Recurrent State Space Model)
 * ============================================================================ */

/**
 * @brief Create RSSM state
 * @param h_dim Deterministic state dimension
 * @param z_dim Stochastic state dimension
 * @return RSSM state or NULL on failure
 */
omni_wm_rssm_state_t* omni_wm_rssm_state_create(uint32_t h_dim, uint32_t z_dim);

/**
 * @brief Clone RSSM state
 * @param state State to clone
 * @return Cloned state or NULL on failure
 */
omni_wm_rssm_state_t* omni_wm_rssm_state_clone(const omni_wm_rssm_state_t* state);

/**
 * @brief Destroy RSSM state
 * @param state State to destroy
 */
void omni_wm_rssm_state_destroy(omni_wm_rssm_state_t* state);

/**
 * @brief RSSM forward step: predict next state
 * @param wm World model
 * @param state Current RSSM state
 * @param action Action taken
 * @param action_dim Action dimensionality
 * @param next_state Output: predicted next state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_rssm_step(omni_world_model_t* wm,
                                 const omni_wm_rssm_state_t* state,
                                 const float* action,
                                 uint32_t action_dim,
                                 omni_wm_rssm_state_t* next_state);

/**
 * @brief RSSM imagine: rollout in latent space without observations
 * @param wm World model
 * @param initial_state Starting state
 * @param actions Action sequence
 * @param horizon Imagination horizon
 * @param trajectory Output: imagined trajectory
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_rssm_imagine(omni_world_model_t* wm,
                                    const omni_wm_rssm_state_t* initial_state,
                                    const float* const* actions,
                                    uint32_t horizon,
                                    omni_wm_rssm_state_t** trajectory);

/**
 * @brief Get current RSSM state
 * @param wm World model
 * @return Current RSSM state or NULL
 */
const omni_wm_rssm_state_t* omni_wm_get_rssm_state(const omni_world_model_t* wm);

/**
 * @brief Set current RSSM state
 * @param wm World model
 * @param state New RSSM state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_set_rssm_state(omni_world_model_t* wm,
                                      const omni_wm_rssm_state_t* state);

/* ============================================================================
 * Latent Encoding API (JEPA-inspired)
 * ============================================================================ */

/**
 * @brief Create latent representation
 * @param dim Embedding dimensionality
 * @return Latent or NULL on failure
 */
omni_wm_latent_t* omni_wm_latent_create(uint32_t dim);

/**
 * @brief Destroy latent representation
 * @param latent Latent to destroy
 */
void omni_wm_latent_destroy(omni_wm_latent_t* latent);

/**
 * @brief Encode observation to latent space
 * @param wm World model
 * @param observation Raw observation
 * @param obs_dim Observation dimensionality
 * @param latent Output: latent representation
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_encode(omni_world_model_t* wm,
                              const float* observation,
                              uint32_t obs_dim,
                              omni_wm_latent_t* latent);

/**
 * @brief Decode latent to observation space
 * @param wm World model
 * @param latent Latent representation
 * @param observation Output: reconstructed observation
 * @param obs_dim Observation dimensionality
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_decode(omni_world_model_t* wm,
                              const omni_wm_latent_t* latent,
                              float* observation,
                              uint32_t obs_dim);

/**
 * @brief Predict in latent space (JEPA-style)
 * @param wm World model
 * @param latent Current latent state
 * @param action Action to take
 * @param action_dim Action dimensionality
 * @param predicted_latent Output: predicted latent
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_predict_latent(omni_world_model_t* wm,
                                      const omni_wm_latent_t* latent,
                                      const float* action,
                                      uint32_t action_dim,
                                      omni_wm_latent_t* predicted_latent);

/* ============================================================================
 * MDN API (Mixture Density Network)
 * ============================================================================ */

/**
 * @brief Create MDN prediction
 * @param num_components Number of mixture components
 * @param dim Output dimensionality
 * @return MDN prediction or NULL on failure
 */
omni_wm_mdn_prediction_t* omni_wm_mdn_create(uint32_t num_components,
                                              uint32_t dim);

/**
 * @brief Destroy MDN prediction
 * @param pred Prediction to destroy
 */
void omni_wm_mdn_destroy(omni_wm_mdn_prediction_t* pred);

/**
 * @brief Predict with MDN (distribution of outcomes)
 * @param wm World model
 * @param state Current state
 * @param action Action to take
 * @param action_dim Action dimensionality
 * @param pred Output: MDN prediction
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_predict_mdn(omni_world_model_t* wm,
                                   const omni_wm_state_t* state,
                                   const float* action,
                                   uint32_t action_dim,
                                   omni_wm_mdn_prediction_t* pred);

/**
 * @brief Sample from MDN prediction
 * @param pred MDN prediction
 * @param sample Output: sampled value
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_mdn_sample(const omni_wm_mdn_prediction_t* pred,
                                  float* sample);

/**
 * @brief Get most likely component from MDN
 * @param pred MDN prediction
 * @param mode Output: mode (most likely value)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_mdn_mode(const omni_wm_mdn_prediction_t* pred,
                                float* mode);

/**
 * @brief Compute log probability under MDN
 * @param pred MDN prediction
 * @param value Value to evaluate
 * @return Log probability
 */
float omni_wm_mdn_log_prob(const omni_wm_mdn_prediction_t* pred,
                            const float* value);

/* ============================================================================
 * Experience Replay API
 * ============================================================================ */

/**
 * @brief Create experience tuple
 * @param state_dim State dimensionality
 * @param action_dim Action dimensionality
 * @param obs_dim Observation dimensionality
 * @return Experience or NULL on failure
 */
omni_wm_experience_t* omni_wm_experience_create(uint32_t state_dim,
                                                  uint32_t action_dim,
                                                  uint32_t obs_dim);

/**
 * @brief Destroy experience tuple
 * @param exp Experience to destroy
 */
void omni_wm_experience_destroy(omni_wm_experience_t* exp);

/**
 * @brief Add experience to replay buffer
 * @param wm World model
 * @param exp Experience to add
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_add_experience(omni_world_model_t* wm,
                                      const omni_wm_experience_t* exp);

/**
 * @brief Sample batch from replay buffer
 * @param wm World model
 * @param batch Output: sampled experiences
 * @param batch_size Number of experiences to sample
 * @return Actual number of experiences returned
 */
uint32_t omni_wm_sample_experiences(omni_world_model_t* wm,
                                     omni_wm_experience_t** batch,
                                     uint32_t batch_size);

/**
 * @brief Get replay buffer size
 * @param wm World model
 * @return Current number of experiences in buffer
 */
uint32_t omni_wm_get_replay_size(const omni_world_model_t* wm);

/**
 * @brief Clear replay buffer
 * @param wm World model
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_clear_replay(omni_world_model_t* wm);

/* ============================================================================
 * Symlog Transformation API (DreamerV3)
 * ============================================================================ */

/**
 * @brief Symlog transformation: sign(x) * ln(|x| + 1)
 *
 * Used for normalizing rewards across vastly different scales.
 * Preserves sign while compressing magnitude.
 *
 * @param x Value to transform
 * @return Symlog-transformed value
 */
float omni_wm_symlog(float x);

/**
 * @brief Inverse symlog: sign(x) * (exp(|x|) - 1)
 * @param x Symlog-transformed value
 * @return Original value
 */
float omni_wm_symexp(float x);

/**
 * @brief Apply symlog to array
 * @param input Input array
 * @param output Output array
 * @param size Array size
 */
void omni_wm_symlog_array(const float* input, float* output, uint32_t size);

/**
 * @brief Apply symexp to array
 * @param input Input array
 * @param output Output array
 * @param size Array size
 */
void omni_wm_symexp_array(const float* input, float* output, uint32_t size);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert direction to string
 * @param dir Direction
 * @return String representation
 */
const char* omni_wm_direction_to_string(omni_wm_direction_t dir);

/**
 * @brief Convert learn mode to string
 * @param mode Learn mode
 * @return String representation
 */
const char* omni_wm_learn_mode_to_string(omni_wm_learn_mode_t mode);

/**
 * @brief Convert counterfactual type to string
 * @param type Counterfactual type
 * @return String representation
 */
const char* omni_wm_cf_type_to_string(omni_wm_counterfactual_type_t type);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect world model to bio-async system
 * @param wm World model
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_connect_bio_async(omni_world_model_t* wm);

/**
 * @brief Disconnect from bio-async system
 * @param wm World model
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_disconnect_bio_async(omni_world_model_t* wm);

/* ============================================================================
 * Serialization / Persistence API
 * ============================================================================ */

/** Magic number for world model serialization format: 'OMWM' */
#define OMNI_WM_SERIAL_MAGIC        0x4F4D574D

/** Current serialization format version */
#define OMNI_WM_SERIAL_VERSION      1

/** Serialization flags */
#define OMNI_WM_SERIAL_FLAG_COMPRESSED    0x01
#define OMNI_WM_SERIAL_FLAG_HAS_REPLAY    0x02
#define OMNI_WM_SERIAL_FLAG_HAS_RSSM      0x04
#define OMNI_WM_SERIAL_FLAG_HAS_DYNAMICS  0x08

/**
 * @brief Serialize world model to buffer
 *
 * Converts the world model state to a portable binary format suitable for
 * checkpointing and persistence. Uses big-endian format for cross-platform
 * compatibility.
 *
 * SERIALIZATION FORMAT (Version 1):
 * - Magic number: uint32_t (OMNI_WM_SERIAL_MAGIC)
 * - Version: uint8_t
 * - Flags: uint8_t
 * - Config section
 * - Current state section
 * - RSSM state section (if flag set)
 * - Replay buffer section (if flag set)
 * - Dynamics weights section (if flag set)
 * - Statistics section
 * - Checksum: uint32_t CRC32
 *
 * @param wm World model to serialize
 * @param buffer Output buffer (NULL to query required size)
 * @param buffer_size Buffer size
 * @return Required/used buffer size, or 0 on error
 */
size_t omni_wm_serialize(const omni_world_model_t* wm,
                          uint8_t* buffer,
                          size_t buffer_size);

/**
 * @brief Deserialize world model from buffer
 *
 * Reconstructs a world model from its serialized binary representation.
 * Validates magic number, version, and checksum before restoration.
 *
 * @param buffer Input buffer containing serialized data
 * @param buffer_size Buffer size
 * @return World model or NULL on error
 */
omni_world_model_t* omni_wm_deserialize(const uint8_t* buffer,
                                         size_t buffer_size);

/**
 * @brief Save world model to file
 *
 * Convenience function that serializes the world model and writes it
 * to a file. Handles file I/O and compression automatically.
 *
 * @param wm World model to save
 * @param filepath File path (created or overwritten)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_save(const omni_world_model_t* wm,
                            const char* filepath);

/**
 * @brief Load world model from file
 *
 * Convenience function that reads a file and deserializes the world model.
 *
 * @param filepath File path to load from
 * @return World model or NULL on error
 */
omni_world_model_t* omni_wm_load(const char* filepath);

/**
 * @brief Checkpoint data for in-memory snapshots
 */
typedef struct {
    uint64_t id;              /**< Unique checkpoint ID */
    uint8_t* data;            /**< Serialized snapshot data */
    size_t data_size;         /**< Size of serialized data */
    double timestamp;         /**< When checkpoint was created */
    char description[64];     /**< Optional description */
} omni_wm_checkpoint_t;

/**
 * @brief Create checkpoint of current state
 *
 * Creates an in-memory snapshot of the world model's current state.
 * Checkpoints are stored internally and can be restored later.
 *
 * @param wm World model
 * @return Checkpoint ID (non-zero) or 0 on error
 */
uint64_t omni_wm_checkpoint(omni_world_model_t* wm);

/**
 * @brief Restore from checkpoint
 *
 * Restores the world model to a previously checkpointed state.
 *
 * @param wm World model
 * @param checkpoint_id Checkpoint ID returned by omni_wm_checkpoint
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_restore_checkpoint(omni_world_model_t* wm,
                                          uint64_t checkpoint_id);

/**
 * @brief Delete a checkpoint
 *
 * Frees memory associated with a checkpoint.
 *
 * @param wm World model
 * @param checkpoint_id Checkpoint ID to delete
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_delete_checkpoint(omni_world_model_t* wm,
                                         uint64_t checkpoint_id);

/**
 * @brief Get number of stored checkpoints
 *
 * @param wm World model
 * @return Number of checkpoints
 */
uint32_t omni_wm_get_checkpoint_count(const omni_world_model_t* wm);

/**
 * @brief Clear all checkpoints
 *
 * @param wm World model
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_clear_checkpoints(omni_world_model_t* wm);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WORLD_MODEL_H */
