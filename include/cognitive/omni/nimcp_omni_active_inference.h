/**
 * @file nimcp_omni_active_inference.h
 * @brief Omnidirectional Active Inference for Action Selection
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Active inference with omnidirectional policy evaluation
 * WHY:  Select actions that minimize expected free energy bidirectionally
 * HOW:  Combine precision-weighted predictions with goal-directed inference
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * ACTIVE INFERENCE (Friston et al., 2017):
 * ----------------------------------------
 * Agents minimize expected free energy (EFE) when selecting actions:
 *
 *   G(π) = E_q(o,s|π)[ln q(s|π) - ln p(o,s|π)]
 *        = Risk + Ambiguity
 *
 * Where:
 *   G(π) = Expected free energy for policy π
 *   Risk = KL[q(o|π)||p(o)] = Deviation from preferred observations
 *   Ambiguity = E_q[H[p(o|s)]] = Expected uncertainty
 *
 * OMNIDIRECTIONAL EXTENSION:
 * --------------------------
 * Standard active inference is forward-looking (predict future states).
 * Omnidirectional active inference adds:
 *
 *   1. BACKWARD INFERENCE: Infer what actions led to current state
 *      - "What must have happened to reach this observation?"
 *      - Useful for learning from outcomes
 *
 *   2. LATERAL INFERENCE: Cross-modal action effects
 *      - "How does this action affect other modalities?"
 *      - Enables coordinated multi-modal actions
 *
 *   3. HIERARCHICAL INFERENCE: Multi-scale planning
 *      - Abstract policies guide concrete actions
 *      - Concrete outcomes refine abstract goals
 *
 * PRECISION-WEIGHTED POLICY EVALUATION:
 * -------------------------------------
 * Policy selection uses precision-weighted EFE:
 *
 *   P(π) ∝ exp(-Π_π * G(π))
 *
 * Where Π_π is the precision of policy evaluation.
 * High precision = exploit known good policies
 * Low precision = explore new policies
 *
 * ARCHITECTURE:
 * ```
 * ╔══════════════════════════════════════════════════════════════════════╗
 * ║              OMNIDIRECTIONAL ACTIVE INFERENCE                         ║
 * ╠══════════════════════════════════════════════════════════════════════╣
 * ║                                                                       ║
 * ║   ┌─────────────────────────────────────────────────────────────┐    ║
 * ║   │                    POLICY SPACE                              │    ║
 * ║   │   ┌───────┐  ┌───────┐  ┌───────┐  ┌───────┐               │    ║
 * ║   │   │ π₁    │  │ π₂    │  │ π₃    │  │ πₙ    │               │    ║
 * ║   │   │ G(π₁) │  │ G(π₂) │  │ G(π₃) │  │ G(πₙ) │               │    ║
 * ║   │   └───┬───┘  └───┬───┘  └───┬───┘  └───┬───┘               │    ║
 * ║   └───────┼──────────┼──────────┼──────────┼────────────────────┘    ║
 * ║           │          │          │          │                          ║
 * ║           ▼          ▼          ▼          ▼                          ║
 * ║   ┌─────────────────────────────────────────────────────────────┐    ║
 * ║   │              PRECISION-WEIGHTED SOFTMAX                      │    ║
 * ║   │                P(π) ∝ exp(-Π * G(π))                        │    ║
 * ║   └─────────────────────────┬───────────────────────────────────┘    ║
 * ║                             │                                         ║
 * ║                             ▼                                         ║
 * ║   ┌─────────────────────────────────────────────────────────────┐    ║
 * ║   │                  ACTION SELECTION                            │    ║
 * ║   │  Forward: a → min G_forward(π)                               │    ║
 * ║   │  Backward: infer a given outcome                             │    ║
 * ║   │  Lateral: coordinate multi-modal actions                     │    ║
 * ║   └─────────────────────────────────────────────────────────────┘    ║
 * ║                                                                       ║
 * ╚══════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_ACTIVE_INFERENCE_H
#define NIMCP_OMNI_ACTIVE_INFERENCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct omni_active_inference omni_active_inference_t;
typedef struct omni_precision_ctx omni_precision_ctx_t;
typedef struct omni_kg_sync omni_kg_sync_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum policies to evaluate */
#define OMNI_AI_MAX_POLICIES               32

/** @brief Maximum action dimensions */
#define OMNI_AI_MAX_ACTION_DIM             64

/** @brief Maximum planning horizon (time steps) */
#define OMNI_AI_MAX_HORIZON                16

/** @brief Default policy precision (temperature) */
#define OMNI_AI_DEFAULT_PRECISION          1.0f

/** @brief Default exploration rate */
#define OMNI_AI_DEFAULT_EXPLORATION        0.1f

/** @brief Bio-async module ID */
#define BIO_MODULE_OMNI_ACTIVE_INFERENCE   0x0E61

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Inference direction for action selection
 */
typedef enum {
    OMNI_AI_DIR_FORWARD = 0,     /**< Forward: predict future from action */
    OMNI_AI_DIR_BACKWARD,        /**< Backward: infer action from outcome */
    OMNI_AI_DIR_LATERAL,         /**< Lateral: cross-modal action effects */
    OMNI_AI_DIR_HIERARCHICAL,    /**< Hierarchical: abstract/concrete */
    OMNI_AI_DIR_COUNT
} omni_ai_direction_t;

/**
 * @brief Policy selection modes
 */
typedef enum {
    OMNI_AI_SELECT_SOFTMAX = 0,  /**< Softmax over -G(π) */
    OMNI_AI_SELECT_GREEDY,       /**< Greedy: min G(π) */
    OMNI_AI_SELECT_THOMPSON,     /**< Thompson sampling */
    OMNI_AI_SELECT_UCB           /**< Upper confidence bound */
} omni_ai_select_mode_t;

/**
 * @brief EFE component weighting modes
 */
typedef enum {
    OMNI_AI_EFE_BALANCED = 0,    /**< Equal risk/ambiguity */
    OMNI_AI_EFE_RISK_SEEKING,    /**< Minimize risk (exploit) */
    OMNI_AI_EFE_CURIOUS,         /**< Minimize ambiguity (explore) */
    OMNI_AI_EFE_ADAPTIVE         /**< Adapt based on precision */
} omni_ai_efe_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Single policy representation
 */
typedef struct {
    uint32_t policy_id;              /**< Policy identifier */
    float* actions;                  /**< Action sequence [horizon x action_dim] */
    uint32_t horizon;                /**< Planning horizon */
    uint32_t action_dim;             /**< Action dimensionality */

    /* EFE components */
    float efe_total;                 /**< Total expected free energy G(π) */
    float efe_risk;                  /**< Risk component */
    float efe_ambiguity;             /**< Ambiguity component */
    float efe_intrinsic;             /**< Intrinsic value (info gain) */
    float efe_extrinsic;             /**< Extrinsic value (goal) */

    /* Direction-specific EFE */
    float efe_forward;               /**< Forward EFE */
    float efe_backward;              /**< Backward EFE (hindsight) */
    float efe_lateral;               /**< Lateral EFE (cross-modal) */

    /* Selection probability */
    float probability;               /**< P(π) after softmax */
    float precision;                 /**< Policy-specific precision */
} omni_ai_policy_t;

/**
 * @brief Goal specification
 */
typedef struct {
    float* preferred_obs;            /**< Preferred observations */
    uint32_t obs_dim;                /**< Observation dimension */
    float goal_precision;            /**< Goal precision (importance) */
    bool active;                     /**< Goal is active */
} omni_ai_goal_t;

/**
 * @brief Action result from inference
 */
typedef struct {
    float* action;                   /**< Selected action vector */
    uint32_t action_dim;             /**< Action dimension */
    uint32_t selected_policy;        /**< Selected policy index */
    float confidence;                /**< Selection confidence */
    omni_ai_direction_t direction;   /**< Direction used */
    float efe;                       /**< EFE of selected policy */
} omni_ai_action_result_t;

/**
 * @brief Active inference configuration
 */
typedef struct {
    /* Policy evaluation */
    omni_ai_select_mode_t select_mode;
    omni_ai_efe_mode_t efe_mode;
    float policy_precision;          /**< Softmax temperature */
    float exploration_rate;          /**< Epsilon for exploration */

    /* Planning */
    uint32_t max_horizon;            /**< Maximum planning steps */
    uint32_t num_policies;           /**< Number of policies to evaluate */

    /* EFE weights */
    float risk_weight;               /**< Weight for risk component */
    float ambiguity_weight;          /**< Weight for ambiguity */
    float intrinsic_weight;          /**< Weight for info gain */
    float extrinsic_weight;          /**< Weight for goal achievement */

    /* Direction weights */
    float forward_weight;            /**< Forward inference weight */
    float backward_weight;           /**< Backward inference weight */
    float lateral_weight;            /**< Lateral inference weight */

    /* Integration */
    bool use_precision_context;      /**< Use omni precision */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_logging;             /**< Enable debug logging */
} omni_ai_config_t;

/**
 * @brief Active inference statistics
 */
typedef struct {
    uint64_t total_inferences;       /**< Total inference calls */
    uint64_t forward_selections;     /**< Forward action selections */
    uint64_t backward_inferences;    /**< Backward inferences */
    uint64_t lateral_inferences;     /**< Lateral inferences */
    float avg_efe;                   /**< Average EFE */
    float avg_confidence;            /**< Average selection confidence */
    float min_efe;                   /**< Minimum EFE observed */
    float exploration_rate;          /**< Actual exploration rate */
    uint32_t explorations;           /**< Exploration actions taken */
    uint32_t exploitations;          /**< Exploitation actions taken */
} omni_ai_stats_t;

/**
 * @brief Active inference context
 */
struct omni_active_inference {
    /* Configuration */
    omni_ai_config_t config;

    /* Policy space */
    omni_ai_policy_t* policies;
    uint32_t num_policies;
    uint32_t policy_capacity;

    /* Goal state */
    omni_ai_goal_t* goals;
    uint32_t num_goals;
    uint32_t goal_capacity;

    /* Current state */
    float* current_obs;              /**< Current observations */
    uint32_t obs_dim;
    float* current_belief;           /**< Current belief state */
    uint32_t belief_dim;

    /* External integrations */
    omni_precision_ctx_t* precision_ctx;
    omni_kg_sync_t* kg_sync;
    void* fep_system;                /**< FEP system (fep_system_t*) */

    /* Bio-async */
    void* bio_context;
    bool bio_async_connected;

    /* Statistics */
    omni_ai_stats_t stats;

    /* Thread safety */
    void* mutex;
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default active inference configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int omni_ai_default_config(omni_ai_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create active inference context
 *
 * @param config Configuration (NULL for defaults)
 * @param action_dim Action dimensionality
 * @param obs_dim Observation dimensionality
 * @return New context or NULL on failure
 */
omni_active_inference_t* omni_ai_create(
    const omni_ai_config_t* config,
    uint32_t action_dim,
    uint32_t obs_dim);

/**
 * @brief Destroy active inference context
 */
void omni_ai_destroy(omni_active_inference_t* ai);

/**
 * @brief Reset to initial state
 */
int omni_ai_reset(omni_active_inference_t* ai);

/* ============================================================================
 * Policy API
 * ============================================================================ */

/**
 * @brief Add policy to evaluation set
 *
 * @param ai Active inference context
 * @param actions Action sequence [horizon x action_dim]
 * @param horizon Planning horizon
 * @param action_dim Action dimension
 * @return Policy index or -1 on failure
 */
int omni_ai_add_policy(omni_active_inference_t* ai,
                        const float* actions,
                        uint32_t horizon,
                        uint32_t action_dim);

/**
 * @brief Generate random policies for exploration
 *
 * @param ai Active inference context
 * @param num_policies Number to generate
 * @param horizon Planning horizon
 * @return Number actually generated
 */
int omni_ai_generate_random_policies(omni_active_inference_t* ai,
                                      uint32_t num_policies,
                                      uint32_t horizon);

/**
 * @brief Clear all policies
 */
int omni_ai_clear_policies(omni_active_inference_t* ai);

/**
 * @brief Get policy by index
 */
int omni_ai_get_policy(omni_active_inference_t* ai,
                        uint32_t index,
                        omni_ai_policy_t* policy);

/* ============================================================================
 * Goal API
 * ============================================================================ */

/**
 * @brief Set preferred observations (goal)
 *
 * @param ai Active inference context
 * @param preferred Preferred observation vector
 * @param obs_dim Observation dimension
 * @param precision Goal precision
 * @return Goal index or -1 on failure
 */
int omni_ai_set_goal(omni_active_inference_t* ai,
                      const float* preferred,
                      uint32_t obs_dim,
                      float precision);

/**
 * @brief Add additional goal
 */
int omni_ai_add_goal(omni_active_inference_t* ai,
                      const float* preferred,
                      uint32_t obs_dim,
                      float precision);

/**
 * @brief Clear all goals
 */
int omni_ai_clear_goals(omni_active_inference_t* ai);

/**
 * @brief Activate/deactivate goal
 */
int omni_ai_set_goal_active(omni_active_inference_t* ai,
                             uint32_t goal_index,
                             bool active);

/* ============================================================================
 * Observation API
 * ============================================================================ */

/**
 * @brief Update current observations
 *
 * @param ai Active inference context
 * @param obs Observation vector
 * @param obs_dim Observation dimension
 * @return NIMCP_SUCCESS on success
 */
int omni_ai_update_observation(omni_active_inference_t* ai,
                                const float* obs,
                                uint32_t obs_dim);

/**
 * @brief Update current belief state
 */
int omni_ai_update_belief(omni_active_inference_t* ai,
                           const float* belief,
                           uint32_t belief_dim);

/* ============================================================================
 * Inference API
 * ============================================================================ */

/**
 * @brief Evaluate all policies (compute EFE)
 *
 * WHAT: Compute expected free energy for each policy
 * WHY:  Prerequisite for action selection
 * HOW:  Forward simulate, compute risk+ambiguity
 *
 * @param ai Active inference context
 * @return NIMCP_SUCCESS on success
 */
int omni_ai_evaluate_policies(omni_active_inference_t* ai);

/**
 * @brief Select action using forward inference
 *
 * WHAT: Choose action minimizing forward EFE
 * WHY:  Standard active inference
 * HOW:  Softmax over -G_forward(π)
 *
 * @param ai Active inference context
 * @param result Output action result
 * @return NIMCP_SUCCESS on success
 */
int omni_ai_select_action_forward(omni_active_inference_t* ai,
                                   omni_ai_action_result_t* result);

/**
 * @brief Infer action using backward inference
 *
 * WHAT: Infer what action led to current state
 * WHY:  Learning from outcomes
 * HOW:  Minimize backward EFE
 *
 * @param ai Active inference context
 * @param outcome Observed outcome
 * @param outcome_dim Outcome dimension
 * @param result Output action result
 * @return NIMCP_SUCCESS on success
 */
int omni_ai_infer_action_backward(omni_active_inference_t* ai,
                                   const float* outcome,
                                   uint32_t outcome_dim,
                                   omni_ai_action_result_t* result);

/**
 * @brief Select action considering all directions
 *
 * WHAT: Omnidirectional action selection
 * WHY:  Combine forward, backward, lateral inference
 * HOW:  Weighted combination of directional EFE
 *
 * @param ai Active inference context
 * @param result Output action result
 * @return NIMCP_SUCCESS on success
 */
int omni_ai_select_action_omni(omni_active_inference_t* ai,
                                omni_ai_action_result_t* result);

/**
 * @brief Get EFE for specific policy
 */
float omni_ai_get_policy_efe(omni_active_inference_t* ai,
                              uint32_t policy_index,
                              omni_ai_direction_t direction);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to precision context
 */
int omni_ai_connect_precision(omni_active_inference_t* ai,
                               omni_precision_ctx_t* precision_ctx);

/**
 * @brief Connect to FEP system
 */
int omni_ai_connect_fep(omni_active_inference_t* ai,
                         void* fep_system);

/**
 * @brief Connect to KG sync
 */
int omni_ai_connect_kg_sync(omni_active_inference_t* ai,
                             omni_kg_sync_t* kg_sync);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 */
int omni_ai_connect_bio_async(omni_active_inference_t* ai);

/**
 * @brief Disconnect from bio-async router
 */
int omni_ai_disconnect_bio_async(omni_active_inference_t* ai);

/**
 * @brief Check bio-async connection
 */
bool omni_ai_is_bio_async_connected(const omni_active_inference_t* ai);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get statistics
 */
int omni_ai_get_stats(omni_active_inference_t* ai,
                       omni_ai_stats_t* stats);

/**
 * @brief Reset statistics
 */
int omni_ai_reset_stats(omni_active_inference_t* ai);

/**
 * @brief Get best policy index
 */
int omni_ai_get_best_policy(omni_active_inference_t* ai);

/**
 * @brief Get policy probabilities
 */
int omni_ai_get_policy_probs(const omni_active_inference_t* ai,
                              float* probs,
                              uint32_t max_policies);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Compute softmax over negative EFE
 *
 * P(π) = exp(-precision * G(π)) / Z
 *
 * @param efe EFE values
 * @param probs Output probabilities
 * @param n Number of policies
 * @param precision Softmax precision
 * @return NIMCP_SUCCESS on success
 */
int omni_ai_softmax_efe(const float* efe,
                         float* probs,
                         uint32_t n,
                         float precision);

/**
 * @brief Sample policy from distribution
 */
int omni_ai_sample_policy(const float* probs,
                           uint32_t n);

/**
 * @brief Allocate action result
 */
omni_ai_action_result_t* omni_ai_action_result_create(uint32_t action_dim);

/**
 * @brief Free action result
 */
void omni_ai_action_result_destroy(omni_ai_action_result_t* result);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_ai_direction_to_string(omni_ai_direction_t direction);
const char* omni_ai_select_mode_to_string(omni_ai_select_mode_t mode);
const char* omni_ai_efe_mode_to_string(omni_ai_efe_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_ACTIVE_INFERENCE_H */
