/**
 * @file nimcp_omni_logic_bridge.h
 * @brief Omnidirectional Inference to Logic Gate Bridge
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Bridge integrating omnidirectional inference with neural logic gates
 * WHY:  Enable logical inference direction alignment (forward/backward chaining),
 *       abductive reasoning via backward prediction, deductive via forward
 * HOW:  Prediction direction maps to inference direction, logic gate outputs
 *       influence inference strategy selection
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * LOGICAL INFERENCE AND PREDICTION DIRECTION:
 * -------------------------------------------
 *
 *   1. FORWARD CHAINING (Modus Ponens):
 *      - Given premises, derive conclusions
 *      - Maps to forward prediction: x_t → x_{t+1}
 *      - "If A then B, A is true → B is true"
 *
 *   2. BACKWARD CHAINING (Modus Tollens):
 *      - Given goal, find supporting premises
 *      - Maps to backward prediction: x_t → x_{t-1}
 *      - "If A then B, B is false → A is false"
 *
 *   3. ABDUCTIVE REASONING:
 *      - Find best explanation for observation
 *      - Uses backward inference to hypothesize causes
 *      - "B is observed → A is most likely cause"
 *
 *   4. LATERAL REASONING:
 *      - Cross-modal inference and analogy
 *      - Uses lateral prediction between domains
 *      - "A in domain X → A' in domain Y"
 *
 * LOGIC GATE INTEGRATION:
 * -----------------------
 *   Logic Gate Type     →  Inference Mapping
 *   ─────────────────────────────────────────────
 *   AND gate            →  Conjunction of predictions
 *   OR gate             →  Disjunction of predictions
 *   NOT gate            →  Negation of prediction
 *   IMPLIES gate        →  Conditional prediction
 *   XOR gate            →  Exclusive prediction paths
 *
 * INFERENCE STRATEGY SELECTION:
 * -----------------------------
 * Logic gates control which inference direction to use:
 *
 *   - High forward confidence + goal unknown → forward chaining
 *   - Goal specified + causes unknown → backward chaining
 *   - Cross-domain query → lateral inference
 *   - Conflicting evidence → weighted combination
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_LOGIC_BRIDGE_H
#define NIMCP_OMNI_LOGIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct omni_logic_bridge omni_logic_bridge_t;
typedef struct jepa_bidirectional jepa_bidirectional_t;
typedef struct predictive_hierarchy predictive_hierarchy_t;
typedef struct neural_logic_network neural_logic_network_t;
typedef struct neural_logic_gate neural_logic_gate_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for omni-logic bridge */
#define BIO_MODULE_OMNI_LOGIC_BRIDGE           0x0E52

/** @brief Maximum number of inference rules */
#define OMNI_LOGIC_MAX_RULES                   64

/** @brief Default confidence threshold for inference */
#define OMNI_LOGIC_DEFAULT_CONFIDENCE_THRESHOLD 0.7f

/** @brief Default logic gate IDs */
#define OMNI_LOGIC_GATE_FORWARD_OK             1
#define OMNI_LOGIC_GATE_BACKWARD_OK            2
#define OMNI_LOGIC_GATE_SWITCH_DIRECTION       3
#define OMNI_LOGIC_GATE_USE_LATERAL            4
#define OMNI_LOGIC_GATE_COMBINE_DIRECTIONS     5

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Logical inference type
 */
typedef enum {
    OMNI_LOGIC_DEDUCTIVE = 0,        /**< Deductive (forward chaining) */
    OMNI_LOGIC_ABDUCTIVE,            /**< Abductive (backward chaining) */
    OMNI_LOGIC_INDUCTIVE,            /**< Inductive (lateral generalization) */
    OMNI_LOGIC_ANALOGICAL,           /**< Analogical (cross-domain) */
    OMNI_LOGIC_COMBINED              /**< Combined inference */
} omni_logic_type_t;

/**
 * @brief Inference direction recommendation
 */
typedef enum {
    OMNI_INFER_FORWARD = 0,          /**< Use forward prediction */
    OMNI_INFER_BACKWARD,             /**< Use backward prediction */
    OMNI_INFER_LATERAL,              /**< Use lateral prediction */
    OMNI_INFER_HIERARCHICAL_UP,      /**< Use hierarchical up */
    OMNI_INFER_HIERARCHICAL_DOWN,    /**< Use hierarchical down */
    OMNI_INFER_WEIGHTED_COMBINE,     /**< Weighted combination */
    OMNI_INFER_SEQUENTIAL            /**< Sequential multi-direction */
} omni_infer_direction_t;

/**
 * @brief Logic condition for inference
 */
typedef enum {
    OMNI_COND_GOAL_SPECIFIED = 0,    /**< Goal/target is specified */
    OMNI_COND_CAUSES_KNOWN,          /**< Causes/premises are known */
    OMNI_COND_FORWARD_CONFIDENT,     /**< Forward prediction confident */
    OMNI_COND_BACKWARD_CONFIDENT,    /**< Backward prediction confident */
    OMNI_COND_LATERAL_RELEVANT,      /**< Lateral modality relevant */
    OMNI_COND_CONFLICTING_EVIDENCE,  /**< Evidence conflicts */
    OMNI_COND_TIME_CONSTRAINED,      /**< Time constraint active */
    OMNI_COND_COUNT
} omni_logic_condition_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Omni inference effects on logic
 */
typedef struct {
    float forward_confidence;        /**< Confidence in forward prediction */
    float backward_confidence;       /**< Confidence in backward prediction */
    float lateral_confidence;        /**< Confidence in lateral prediction */
    float hierarchical_confidence;   /**< Confidence in hierarchical */
    omni_logic_type_t suggested_type; /**< Suggested logic type */
    omni_infer_direction_t recommended_dir; /**< Recommended direction */
} omni_to_logic_effects_t;

/**
 * @brief Logic effects on omni inference
 */
typedef struct {
    omni_infer_direction_t direction; /**< Direction to use */
    float direction_weight[5];        /**< Weights per direction */
    bool force_direction;             /**< Force specific direction */
    uint32_t max_iterations;          /**< Max inference iterations */
    float confidence_threshold;       /**< Min confidence threshold */
    bool enable_chaining;             /**< Enable inference chaining */
} logic_to_omni_effects_t;

/**
 * @brief Inference rule
 */
typedef struct {
    uint32_t rule_id;                /**< Rule identifier */
    char name[64];                   /**< Rule name */
    omni_logic_type_t type;          /**< Inference type */
    uint32_t gate_id;                /**< Associated logic gate */
    float confidence_threshold;      /**< Required confidence */
    bool active;                     /**< Rule is active */
} omni_logic_rule_t;

/**
 * @brief Condition state for logic evaluation
 */
typedef struct {
    bool goal_specified;             /**< Goal is specified */
    bool causes_known;               /**< Causes are known */
    bool forward_confident;          /**< Forward confident */
    bool backward_confident;         /**< Backward confident */
    bool lateral_relevant;           /**< Lateral relevant */
    bool conflicting_evidence;       /**< Conflicting evidence */
    bool time_constrained;           /**< Time constrained */

    /* Numeric values */
    float forward_pe;                /**< Forward prediction error */
    float backward_pe;               /**< Backward prediction error */
    float lateral_pe;                /**< Lateral prediction error */
    float available_time_ms;         /**< Available time */
} omni_logic_conditions_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Confidence thresholds */
    float confidence_threshold;      /**< Default confidence threshold */
    float forward_confidence_min;    /**< Min forward confidence */
    float backward_confidence_min;   /**< Min backward confidence */

    /* Direction weights */
    float default_forward_weight;    /**< Default forward weight */
    float default_backward_weight;   /**< Default backward weight */
    float default_lateral_weight;    /**< Default lateral weight */

    /* Chaining configuration */
    bool enable_chaining;            /**< Enable inference chaining */
    uint32_t max_chain_depth;        /**< Maximum chain depth */
    uint32_t max_iterations;         /**< Maximum iterations */

    /* Gate configuration */
    bool create_default_gates;       /**< Create default logic gates */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_logging;             /**< Enable logging */
} omni_logic_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total bridge updates */
    uint64_t direction_decisions;    /**< Direction decisions made */
    uint64_t forward_selected;       /**< Forward direction count */
    uint64_t backward_selected;      /**< Backward direction count */
    uint64_t lateral_selected;       /**< Lateral direction count */
    uint64_t combined_selected;      /**< Combined direction count */
    uint64_t gate_evaluations;       /**< Total gate evaluations */
    uint64_t chain_inferences;       /**< Chained inferences */
    float avg_forward_confidence;    /**< Average forward confidence */
    float avg_backward_confidence;   /**< Average backward confidence */
} omni_logic_stats_t;

/**
 * @brief Omni-logic bridge structure
 */
struct omni_logic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge */

    omni_logic_config_t config;      /**< Configuration */

    /* Connected systems */
    jepa_bidirectional_t* jepa;      /**< Bidirectional JEPA */
    predictive_hierarchy_t* pred_hier; /**< Predictive hierarchy */
    neural_logic_network_t* logic_net; /**< Neural logic network */

    /* Logic gates for inference control */
    neural_logic_gate_t* forward_gate;  /**< Gate for forward decision */
    neural_logic_gate_t* backward_gate; /**< Gate for backward decision */
    neural_logic_gate_t* switch_gate;   /**< Gate for direction switch */
    neural_logic_gate_t* lateral_gate;  /**< Gate for lateral decision */
    neural_logic_gate_t* combine_gate;  /**< Gate for combining */

    /* Inference rules */
    omni_logic_rule_t* rules;        /**< Array of rules */
    uint32_t num_rules;              /**< Number of rules */
    uint32_t max_rules;              /**< Maximum rules */

    /* Current conditions */
    omni_logic_conditions_t conditions; /**< Current conditions */

    /* Computed effects */
    omni_to_logic_effects_t omni_effects;   /**< Omni → logic */
    logic_to_omni_effects_t logic_effects;  /**< Logic → omni */

    /* Statistics */
    omni_logic_stats_t stats;

    /* Bio-async integration */
    void* bio_context;               /**< Bio-async module context */
    bool bio_async_connected;        /**< Bio-async connection state */

    /* Thread safety */
    void* mutex;
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_default_config(omni_logic_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create omni-logic bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
omni_logic_bridge_t* omni_logic_bridge_create(
    const omni_logic_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void omni_logic_bridge_destroy(omni_logic_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to bidirectional JEPA
 *
 * @param bridge Bridge
 * @param jepa Bidirectional JEPA (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_connect_jepa(omni_logic_bridge_t* bridge,
                             jepa_bidirectional_t* jepa);

/**
 * @brief Connect to predictive hierarchy
 *
 * @param bridge Bridge
 * @param pred_hier Predictive hierarchy (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_connect_pred_hier(omni_logic_bridge_t* bridge,
                                  predictive_hierarchy_t* pred_hier);

/**
 * @brief Connect to neural logic network
 *
 * @param bridge Bridge
 * @param logic_net Neural logic network (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_connect_logic_network(omni_logic_bridge_t* bridge,
                                      neural_logic_network_t* logic_net);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge (compute bidirectional effects)
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_update(omni_logic_bridge_t* bridge);

/**
 * @brief Apply omni effects to logic gates
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_apply_to_logic(omni_logic_bridge_t* bridge);

/**
 * @brief Apply logic effects to omni inference
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_apply_to_omni(omni_logic_bridge_t* bridge);

/* ============================================================================
 * Inference Direction API
 * ============================================================================ */

/**
 * @brief Get recommended inference direction
 *
 * WHAT: Query logic gates for direction recommendation
 * WHY:  Select optimal inference strategy
 * HOW:  Evaluate gates based on current conditions
 *
 * @param bridge Bridge
 * @param direction Output direction
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_get_direction(omni_logic_bridge_t* bridge,
                              omni_infer_direction_t* direction);

/**
 * @brief Set condition for inference
 *
 * @param bridge Bridge
 * @param condition Condition to set
 * @param value Value to set
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_set_condition(omni_logic_bridge_t* bridge,
                              omni_logic_condition_t condition,
                              bool value);

/**
 * @brief Get current conditions
 *
 * @param bridge Bridge
 * @param conditions Output conditions
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_get_conditions(omni_logic_bridge_t* bridge,
                               omni_logic_conditions_t* conditions);

/**
 * @brief Check if forward chaining is appropriate
 *
 * @param bridge Bridge
 * @return true if forward chaining recommended
 */
bool omni_logic_should_forward_chain(omni_logic_bridge_t* bridge);

/**
 * @brief Check if backward chaining is appropriate
 *
 * @param bridge Bridge
 * @return true if backward chaining recommended
 */
bool omni_logic_should_backward_chain(omni_logic_bridge_t* bridge);

/**
 * @brief Check if direction switch is needed
 *
 * @param bridge Bridge
 * @return true if direction switch recommended
 */
bool omni_logic_should_switch_direction(omni_logic_bridge_t* bridge);

/* ============================================================================
 * Rule Management API
 * ============================================================================ */

/**
 * @brief Add inference rule
 *
 * @param bridge Bridge
 * @param name Rule name
 * @param type Inference type
 * @param gate_id Associated gate (0 for none)
 * @param rule_id Output rule ID
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_add_rule(omni_logic_bridge_t* bridge,
                         const char* name,
                         omni_logic_type_t type,
                         uint32_t gate_id,
                         uint32_t* rule_id);

/**
 * @brief Remove inference rule
 *
 * @param bridge Bridge
 * @param rule_id Rule to remove
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_remove_rule(omni_logic_bridge_t* bridge,
                            uint32_t rule_id);

/**
 * @brief Evaluate specific rule
 *
 * @param bridge Bridge
 * @param rule_id Rule to evaluate
 * @param result Output: rule fires (true/false)
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_evaluate_rule(omni_logic_bridge_t* bridge,
                              uint32_t rule_id,
                              bool* result);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current omni-to-logic effects
 *
 * @param bridge Bridge
 * @param effects Output effects
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_get_omni_effects(omni_logic_bridge_t* bridge,
                                 omni_to_logic_effects_t* effects);

/**
 * @brief Get current logic-to-omni effects
 *
 * @param bridge Bridge
 * @param effects Output effects
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_get_logic_effects(omni_logic_bridge_t* bridge,
                                  logic_to_omni_effects_t* effects);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_get_stats(omni_logic_bridge_t* bridge,
                          omni_logic_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_reset_stats(omni_logic_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_connect_bio_async(omni_logic_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_logic_disconnect_bio_async(omni_logic_bridge_t* bridge);

/**
 * @brief Check if bio-async connected
 *
 * @param bridge Bridge
 * @return true if connected
 */
bool omni_logic_is_bio_async_connected(const omni_logic_bridge_t* bridge);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert logic type to string
 *
 * @param type Logic type
 * @return String representation
 */
const char* omni_logic_type_to_string(omni_logic_type_t type);

/**
 * @brief Convert infer direction to string
 *
 * @param direction Infer direction
 * @return String representation
 */
const char* omni_logic_direction_to_string(omni_infer_direction_t direction);

/**
 * @brief Convert condition to string
 *
 * @param condition Condition
 * @return String representation
 */
const char* omni_logic_condition_to_string(omni_logic_condition_t condition);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_LOGIC_BRIDGE_H */
