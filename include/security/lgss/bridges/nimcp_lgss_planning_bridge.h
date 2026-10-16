/**
 * @file nimcp_lgss_planning_bridge.h
 * @brief LGSS Planning Safety Bridge - Component A6
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Safety validation for planning outputs (MCTS trees, action sequences)
 * WHY:  Plans may contain hidden harmful action sequences or compound risks
 * HOW:  Recursively evaluate plan nodes, aggregate harm probabilities
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PREFRONTAL PLANNING SAFETY:
 * ----------------------------
 * - Prefrontal cortex performs mental simulation before action
 * - OFC evaluates expected outcomes at each decision point
 * - ACC monitors for conflict between subgoals
 * - Planning bridge = safety evaluation of simulated futures
 *
 * This bridge validates:
 * 1. Individual plan nodes/actions
 * 2. Action sequences for compound harm
 * 3. Full plan trees for hidden risks
 *
 * ARCHITECTURE:
 * ```
 * +------------------------------------------------------------------+
 * |                   PLANNING SAFETY BRIDGE                          |
 * +==================================================================+
 * |                                                                  |
 * |  ┌──────────────────────────────────────────────────────────┐   |
 * |  │                    PLAN TREE VALIDATOR                    │   |
 * |  │                                                           │   |
 * |  │  ┌─────────┐      ┌─────────┐      ┌─────────┐          │   |
 * |  │  │  ROOT   │─────►│  NODE   │─────►│  LEAF   │          │   |
 * |  │  │ p_harm  │      │ p_harm  │      │ p_harm  │          │   |
 * |  │  └────┬────┘      └────┬────┘      └─────────┘          │   |
 * |  │       │                │                                  │   |
 * |  │       ▼                ▼                                  │   |
 * |  │  ┌─────────┐      ┌─────────┐                            │   |
 * |  │  │ CHILD 1 │      │ CHILD 2 │                            │   |
 * |  │  └─────────┘      └─────────┘                            │   |
 * |  │                                                           │   |
 * |  │  Aggregate: p_total = 1 - PRODUCT(1 - p_harm_i)          │   |
 * |  └──────────────────────────────────────────────────────────┘   |
 * |                                                                  |
 * |  ┌──────────────────────────────────────────────────────────┐   |
 * |  │                   HARM ESTIMATION                         │   |
 * |  │  - Direct harm from action                                │   |
 * |  │  - Indirect/cascading effects                            │   |
 * |  │  - Reversibility analysis                                │   |
 * |  │  - Context-dependent risk                                │   |
 * |  └──────────────────────────────────────────────────────────┘   |
 * |                                                                  |
 * +------------------------------------------------------------------+
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LGSS_PLANNING_BRIDGE_H
#define NIMCP_LGSS_PLANNING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "security/lgss/bridges/nimcp_lgss_executive_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum action description length */
#define LGSS_PLAN_MAX_ACTION_LEN         128

/** Maximum outcome description length */
#define LGSS_PLAN_MAX_OUTCOME_LEN        128

/** Maximum plan tree depth for validation */
#define LGSS_PLAN_MAX_DEPTH              20

/** Maximum child nodes per plan node */
#define LGSS_PLAN_MAX_CHILDREN           16

/** Magic number for validation */
#define LGSS_PLAN_BRIDGE_MAGIC           0x4C475042  /* 'LGPB' */

/** Default compound harm threshold */
#define LGSS_PLAN_DEFAULT_COMPOUND_HARM_THRESHOLD 0.5f

/** Default max acceptable aggregate harm */
#define LGSS_PLAN_DEFAULT_MAX_AGGREGATE_HARM 0.7f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct planning_safety_bridge planning_safety_bridge_t;
typedef struct action_interceptor action_interceptor_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Plan node for safety evaluation
 *
 * WHAT: Represents a single action/decision point in a plan
 * WHY:  Each node must be evaluated for harm potential
 * HOW:  Contains action details and safety-relevant metrics
 */
typedef struct plan_node {
    char action[LGSS_PLAN_MAX_ACTION_LEN];      /**< Action description */
    char expected_outcome[LGSS_PLAN_MAX_OUTCOME_LEN]; /**< Expected outcome */
    float p_harm;                               /**< Harm probability [0,1] */
    float reversibility;                        /**< Reversibility index [0,1] */

    /* Node metadata */
    uint32_t node_id;                           /**< Unique node identifier */
    uint32_t depth;                             /**< Depth in plan tree */
    bool is_critical;                           /**< Must succeed for plan */
    bool is_terminal;                           /**< Is this a leaf node */

    /* Safety-specific fields */
    lgss_safety_domain_t domain;                /**< Safety domain */
    char target_type[64];                       /**< Target of action */
    float estimated_cost;                       /**< Resource cost */
    float estimated_value;                      /**< Expected value */

    /* Children (for tree validation) */
    struct plan_node** children;                /**< Child nodes (NULL if leaf) */
    uint32_t num_children;                      /**< Number of children */
} plan_node_t;

/**
 * @brief Plan validation result
 *
 * WHAT: Detailed result of plan tree validation
 * WHY:  Provides actionable feedback on plan safety
 * HOW:  Aggregates node evaluations with explanations
 */
typedef struct {
    lgss_result_t result;                       /**< Overall result */
    bool is_safe;                               /**< Plan is safe to execute */

    /* Aggregate metrics */
    float aggregate_p_harm;                     /**< Total harm probability */
    float min_reversibility;                    /**< Minimum reversibility in plan */
    float avg_reversibility;                    /**< Average reversibility */

    /* Problem identification */
    uint32_t num_dangerous_nodes;               /**< Nodes with p_harm > threshold */
    uint32_t dangerous_node_ids[16];            /**< IDs of dangerous nodes */
    uint32_t most_dangerous_node_id;            /**< Highest p_harm node */
    float most_dangerous_p_harm;                /**< Highest p_harm value */

    /* Evaluation metadata */
    uint32_t nodes_evaluated;                   /**< Total nodes evaluated */
    uint32_t max_depth_reached;                 /**< Deepest node evaluated */
    uint64_t evaluation_time_us;                /**< Total evaluation time */

    /* Explanation */
    char explanation[256];                      /**< Human-readable explanation */
    char recommended_action[128];               /**< Recommended action */
} plan_validation_result_t;

/**
 * @brief Single action validation result
 */
typedef struct {
    lgss_result_t result;                       /**< Validation result */
    float p_harm;                               /**< Evaluated harm probability */
    float reversibility;                        /**< Evaluated reversibility */
    bool is_safe;                               /**< Action is safe */
    char matched_rule_id[64];                   /**< Triggering rule (if any) */
    char explanation[128];                      /**< Explanation */
} action_validation_result_t;

/**
 * @brief Harm estimation parameters
 */
typedef struct {
    char action[LGSS_PLAN_MAX_ACTION_LEN];      /**< Action to estimate */
    lgss_safety_domain_t domain;                /**< Safety domain */
    char target_type[64];                       /**< Target of action */
    float context_risk_factor;                  /**< Environmental risk [0,2] */
    bool consider_indirect_effects;             /**< Include cascading effects */
    bool consider_uncertainty;                  /**< Add uncertainty margin */
} harm_estimation_params_t;

/**
 * @brief Harm estimation result
 */
typedef struct {
    float direct_p_harm;                        /**< Direct harm probability */
    float indirect_p_harm;                      /**< Indirect/cascading harm */
    float total_p_harm;                         /**< Combined harm probability */
    float confidence;                           /**< Confidence in estimate */
    float uncertainty_margin;                   /**< Uncertainty range +/- */
    char risk_factors[256];                     /**< Identified risk factors */
} harm_estimation_result_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Planning safety bridge configuration
 */
typedef struct {
    /* Thresholds */
    float node_harm_threshold;                  /**< Block nodes with p_harm > threshold (default: 0.3) */
    float compound_harm_threshold;              /**< Escalate if compound harm > threshold (default: 0.5) */
    float max_aggregate_harm;                   /**< Block if aggregate > threshold (default: 0.7) */
    float min_acceptable_reversibility;         /**< Block if reversibility < threshold (default: 0.2) */

    /* Validation depth */
    uint32_t max_validation_depth;              /**< Maximum depth to evaluate (default: 20) */
    bool validate_all_branches;                 /**< Validate all branches (default: true) */
    bool stop_on_first_violation;               /**< Stop at first dangerous node (default: false) */

    /* Harm estimation */
    bool include_indirect_effects;              /**< Consider cascading effects (default: true) */
    bool include_uncertainty;                   /**< Add uncertainty margin (default: true) */
    float base_uncertainty_margin;              /**< Base uncertainty [0,1] (default: 0.1) */

    /* Fail-safe behavior */
    bool fail_safe_on_unknown;                  /**< Deny unknown actions (default: true) */
    float default_unknown_p_harm;               /**< Default p_harm for unknown actions (default: 0.5) */

    /* Sensitivity */
    float safety_sensitivity;                   /**< Safety effect scaling [0.5-2.0] (default: 1.0) */
} planning_safety_config_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief Planning safety bridge statistics
 */
typedef struct {
    /* Plan validations */
    uint64_t plans_validated;                   /**< Total plans validated */
    uint64_t plans_allowed;                     /**< Plans allowed */
    uint64_t plans_denied;                      /**< Plans denied */
    uint64_t plans_escalated;                   /**< Plans escalated */

    /* Node validations */
    uint64_t nodes_validated;                   /**< Total nodes validated */
    uint64_t nodes_dangerous;                   /**< Nodes flagged as dangerous */
    float avg_node_p_harm;                      /**< Average node harm probability */

    /* Action validations */
    uint64_t actions_validated;                 /**< Total actions validated */
    uint64_t actions_allowed;                   /**< Actions allowed */
    uint64_t actions_denied;                    /**< Actions denied */

    /* Harm estimations */
    uint64_t harm_estimates;                    /**< Total harm estimations */
    float avg_estimated_harm;                   /**< Average estimated harm */

    /* Performance */
    float avg_validation_time_us;               /**< Average validation time */
    uint64_t total_inference_steps;             /**< Total inference steps */
} planning_safety_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Planning safety bridge state
 */
struct planning_safety_bridge {
    bridge_base_t base;                         /**< MUST be first: base bridge */

    /* Magic for validation */
    uint32_t magic;                             /**< Magic number for validation */

    /* Configuration */
    planning_safety_config_t config;

    /* Connected systems */
    action_interceptor_t* aix;                  /**< Action interceptor */

    /* Statistics */
    planning_safety_stats_t stats;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration structure
 * @return 0 on success, error code on failure
 */
int planning_safety_default_config(planning_safety_config_t* config);

/**
 * @brief Create planning safety bridge
 *
 * WHAT: Creates planning validation bridge
 * WHY:  Enable safety validation of plan trees
 * HOW:  Allocate, initialize, connect to AIx
 *
 * @return Bridge instance or NULL on failure
 */
planning_safety_bridge_t* planning_safety_bridge_create(void);

/**
 * @brief Create planning safety bridge with custom config
 *
 * @param config Custom configuration
 * @return Bridge instance or NULL on failure
 */
planning_safety_bridge_t* planning_safety_bridge_create_custom(
    const planning_safety_config_t* config
);

/**
 * @brief Destroy planning safety bridge
 *
 * @param bridge Bridge instance (NULL safe)
 */
void planning_safety_bridge_destroy(planning_safety_bridge_t* bridge);

/* ============================================================================
 * Core Validation API
 * ============================================================================ */

/**
 * @brief Validate entire plan tree
 *
 * WHAT: Recursively validate all nodes in a plan tree
 * WHY:  Plans may contain hidden harmful sequences
 * HOW:  DFS traversal, aggregate harm probabilities, check thresholds
 *
 * ALGORITHM:
 * 1. Validate root node
 * 2. Recursively validate children
 * 3. Aggregate harm: p_total = 1 - PRODUCT(1 - p_harm_i)
 * 4. Check compound harm threshold
 * 5. Return overall result
 *
 * @param bridge Bridge instance
 * @param root Root node of plan tree
 * @param result Output validation result
 * @return 0 on success, error code on failure
 */
int planning_safety_validate_plan_tree(
    planning_safety_bridge_t* bridge,
    const plan_node_t* root,
    plan_validation_result_t* result
);

/**
 * @brief Validate single action
 *
 * WHAT: Evaluate safety of a single action
 * WHY:  Quick validation without full tree traversal
 * HOW:  Convert to context, evaluate via AIx
 *
 * @param bridge Bridge instance
 * @param action Action description
 * @param domain Safety domain
 * @param target_type Target of action
 * @param result Output validation result
 * @return 0 on success, error code on failure
 */
int planning_safety_validate_action(
    planning_safety_bridge_t* bridge,
    const char* action,
    lgss_safety_domain_t domain,
    const char* target_type,
    action_validation_result_t* result
);

/**
 * @brief Estimate harm probability for an action
 *
 * WHAT: Estimate harm probability using safety rules
 * WHY:  Enable risk assessment before planning
 * HOW:  Apply safety rules, consider context, add uncertainty
 *
 * @param bridge Bridge instance
 * @param params Estimation parameters
 * @param result Output estimation result
 * @return 0 on success, error code on failure
 */
int planning_safety_estimate_harm(
    planning_safety_bridge_t* bridge,
    const harm_estimation_params_t* params,
    harm_estimation_result_t* result
);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Calculate aggregate harm probability
 *
 * WHAT: Combine harm probabilities from multiple nodes
 * WHY:  Sequence of low-harm actions can be high-harm overall
 * HOW:  p_total = 1 - PRODUCT(1 - p_harm_i)
 *
 * @param p_harm_values Array of harm probabilities
 * @param count Number of values
 * @return Aggregate harm probability [0,1]
 */
float planning_safety_aggregate_harm(
    const float* p_harm_values,
    uint32_t count
);

/**
 * @brief Create plan node
 *
 * @param action Action description
 * @param p_harm Harm probability
 * @param reversibility Reversibility index
 * @return Plan node or NULL on failure
 */
plan_node_t* planning_safety_create_node(
    const char* action,
    float p_harm,
    float reversibility
);

/**
 * @brief Destroy plan node (and children recursively)
 *
 * @param node Node to destroy (NULL safe)
 */
void planning_safety_destroy_node(plan_node_t* node);

/**
 * @brief Add child to plan node
 *
 * @param parent Parent node
 * @param child Child node to add
 * @return 0 on success, error code on failure
 */
int planning_safety_add_child(plan_node_t* parent, plan_node_t* child);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int planning_safety_get_stats(
    const planning_safety_bridge_t* bridge,
    planning_safety_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int planning_safety_reset_stats(planning_safety_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect action interceptor
 *
 * @param bridge Bridge instance
 * @param aix Action interceptor
 * @return 0 on success, error code on failure
 */
int planning_safety_connect_aix(
    planning_safety_bridge_t* bridge,
    action_interceptor_t* aix
);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge instance
 * @return true if AIx is connected
 */
bool planning_safety_is_connected(const planning_safety_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int planning_safety_connect_bio_async(planning_safety_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int planning_safety_disconnect_bio_async(planning_safety_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool planning_safety_is_bio_async_connected(const planning_safety_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_PLANNING_BRIDGE_H */
