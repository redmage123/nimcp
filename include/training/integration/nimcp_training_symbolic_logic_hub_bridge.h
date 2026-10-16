/**
 * @file nimcp_training_symbolic_logic_hub_bridge.h
 * @brief Symbolic Logic - Training Hub Integration Bridge
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Integration bridge connecting symbolic logic to the training integration hub
 * WHY:  Enable logic-guided training decisions, curriculum constraints, and rule learning
 * HOW:  Subscribe to training events, evaluate logical rules, publish recommendations
 *
 * KEY INTEGRATION SCENARIOS:
 * ==========================
 *
 * 1. CURRICULUM-GUIDED LEARNING:
 *    - Curriculum publishes DIFFICULTY_UPDATED events
 *    - Logic evaluates prerequisite rules: "prerequisite(topic) AND mastery > 0.8 -> allow_progression"
 *    - Logic publishes constraint validation back to curriculum
 *
 * 2. TRAINING FEEDBACK FOR RULE WEIGHTS:
 *    - Training publishes LOSS_COMPUTED events
 *    - Logic tracks which rules influenced recent decisions
 *    - Rule confidence increases if loss improved, decreases if diverged
 *
 * 3. SAFETY CONSTRAINTS:
 *    - Logic defines safety rules: "grad_norm > threshold -> must_clip"
 *    - Training events validated against constraints
 *    - Constraint violations published as events
 *
 * 4. META-LEARNING GUIDANCE:
 *    - Meta-learning queries: "What LR worked for similar tasks?"
 *    - Logic queries knowledge base for successful patterns
 *    - Returns logic-filtered hyperparameter suggestions
 *
 * 5. CONTINUAL LEARNING MEMORY:
 *    - Task switch triggers: "Which facts relate to new task?"
 *    - Logic returns relevance-ranked memories for replay
 *
 * SUPPORTED TRAINING RULES:
 * ========================
 * - Curriculum prerequisites: prerequisite_mastered(X) -> allow_progression
 * - LR safety: loss_stable AND grad_stable -> safe_lr_increase
 * - Difficulty constraints: difficulty < max_difficulty AND performance > threshold
 * - Checkpoint triggers: epochs_since_best > patience -> should_checkpoint
 * - Early stopping: validation_increasing(N) -> should_stop
 *
 * THREAD SAFETY: All functions are thread-safe.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TRAINING_SYMBOLIC_LOGIC_HUB_BRIDGE_H
#define NIMCP_TRAINING_SYMBOLIC_LOGIC_HUB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "training/integration/nimcp_training_integration_hub.h"
#include "training/integration/nimcp_training_event_types.h"
#include "cognitive/nimcp_symbolic_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Module ID for symbolic logic in training hub */
#define TRAINING_LOGIC_MODULE_ID          0x2020

/** Module name for registration */
#define TRAINING_LOGIC_MODULE_NAME        "training_symbolic_logic"

/** Maximum training rules */
#define TRAINING_LOGIC_MAX_RULES          128

/** Maximum rule tracking entries */
#define TRAINING_LOGIC_MAX_RULE_TRACKING  256

/** Confidence decay factor for rules not recently fired */
#define TRAINING_LOGIC_CONFIDENCE_DECAY   0.995f

/** Confidence boost for rules that improve loss */
#define TRAINING_LOGIC_CONFIDENCE_BOOST   0.05f

/** Confidence penalty for rules that worsen loss */
#define TRAINING_LOGIC_CONFIDENCE_PENALTY 0.1f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct training_logic_hub_bridge training_logic_hub_bridge_t;

/* ============================================================================
 * Training Rule Types
 * ============================================================================ */

/**
 * @brief Types of training rules that can be evaluated
 */
typedef enum {
    TRAINING_RULE_CURRICULUM_PREREQUISITE,   /**< Prerequisite for difficulty increase */
    TRAINING_RULE_LR_SAFETY,                 /**< Safe to adjust learning rate */
    TRAINING_RULE_DIFFICULTY_CONSTRAINT,     /**< Difficulty change constraint */
    TRAINING_RULE_CHECKPOINT_TRIGGER,        /**< Should create checkpoint */
    TRAINING_RULE_EARLY_STOP,                /**< Should stop training */
    TRAINING_RULE_GRADIENT_CLIP,             /**< Should clip gradients */
    TRAINING_RULE_BATCH_SIZE_ADJUST,         /**< Safe to adjust batch size */
    TRAINING_RULE_TASK_SWITCH,               /**< Should switch tasks */
    TRAINING_RULE_MEMORY_REPLAY,             /**< Should replay memories */
    TRAINING_RULE_HYPERPARAMETER_SUGGEST,    /**< Hyperparameter suggestion */
    TRAINING_RULE_CUSTOM,                    /**< User-defined rule */
    TRAINING_RULE_COUNT
} training_rule_type_t;

/**
 * @brief Training rule definition
 */
typedef struct {
    training_rule_type_t type;               /**< Rule type */
    char name[64];                           /**< Rule name */
    char condition[256];                     /**< Logical condition (FOL syntax) */
    float confidence;                        /**< Rule confidence [0,1] */
    float priority;                          /**< Rule priority [0,1] */
    bool is_safety_critical;                 /**< Cannot be overridden */
    uint64_t times_fired;                    /**< Times this rule fired */
    uint64_t times_correct;                  /**< Times outcome was correct */
    uint64_t last_fired_time;                /**< Last fire timestamp */
} training_logic_rule_t;

/**
 * @brief Rule evaluation result
 */
typedef struct {
    training_rule_type_t type;               /**< Rule type */
    bool satisfied;                          /**< Rule condition satisfied */
    float confidence;                        /**< Confidence in result */
    char explanation[256];                   /**< Human-readable explanation */
} training_rule_result_t;

/* ============================================================================
 * Training Metrics (for rule evaluation)
 * ============================================================================ */

/**
 * @brief Current training metrics for rule evaluation
 */
typedef struct {
    /* Loss metrics */
    float current_loss;                      /**< Current loss value */
    float previous_loss;                     /**< Previous loss value */
    float best_loss;                         /**< Best loss seen */
    float loss_trend;                        /**< Loss trend (-1 to 1) */
    bool loss_stable;                        /**< Loss is stable */

    /* Gradient metrics */
    float grad_norm;                         /**< Current gradient norm */
    float grad_norm_avg;                     /**< Average gradient norm */
    bool grad_stable;                        /**< Gradients are stable */
    bool grad_exploding;                     /**< Gradients exploding */
    bool grad_vanishing;                     /**< Gradients vanishing */

    /* Learning rate metrics */
    float learning_rate;                     /**< Current learning rate */
    float lr_min;                            /**< Minimum LR bound */
    float lr_max;                            /**< Maximum LR bound */

    /* Curriculum metrics */
    float difficulty;                        /**< Current difficulty [0,1] */
    float mastery;                           /**< Current mastery [0,1] */
    float performance;                       /**< Recent performance [0,1] */

    /* Progress metrics */
    uint32_t epoch;                          /**< Current epoch */
    uint32_t batch;                          /**< Current batch */
    uint32_t epochs_since_improvement;       /**< Epochs without improvement */
    uint32_t batches_since_checkpoint;       /**< Batches since last checkpoint */

    /* Validation metrics */
    float validation_loss;                   /**< Latest validation loss */
    float validation_accuracy;               /**< Latest validation accuracy */
    bool validation_improving;               /**< Validation is improving */
} training_logic_metrics_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Configuration for training logic hub bridge
 */
typedef struct {
    /* Event subscriptions */
    bool subscribe_loss_computed;            /**< Subscribe to loss events */
    bool subscribe_gradient_ready;           /**< Subscribe to gradient events */
    bool subscribe_difficulty_updated;       /**< Subscribe to curriculum events */
    bool subscribe_lr_adjusted;              /**< Subscribe to LR events */
    bool subscribe_epoch_complete;           /**< Subscribe to epoch events */
    bool subscribe_validation_complete;      /**< Subscribe to validation events */
    bool subscribe_task_switched;            /**< Subscribe to task switch events */

    /* Event publishing */
    bool publish_rule_results;               /**< Publish rule evaluation results */
    bool publish_constraint_violations;      /**< Publish constraint violations */
    bool publish_recommendations;            /**< Publish training recommendations */

    /* Rule learning */
    bool enable_rule_learning;               /**< Learn rule confidence from outcomes */
    float rule_learning_rate;                /**< Learning rate for rule confidence */
    float min_rule_confidence;               /**< Minimum rule confidence */

    /* Processing parameters */
    uint32_t max_rules_per_event;            /**< Max rules to evaluate per event */
    float inference_timeout_ms;              /**< Timeout for inference */
    bool enable_async_inference;             /**< Use async inference */

    /* Integration */
    bool connect_to_cognitive_logic;         /**< Connect to cognitive symbolic logic */
    bool share_knowledge_base;               /**< Share KB with cognitive logic */
} training_logic_hub_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Statistics for training logic hub bridge
 */
typedef struct {
    /* Event statistics */
    uint64_t events_received;                /**< Total events received */
    uint64_t events_processed;               /**< Events successfully processed */
    uint64_t events_published;               /**< Events published */

    /* Rule statistics */
    uint64_t rules_evaluated;                /**< Total rule evaluations */
    uint64_t rules_satisfied;                /**< Rules that evaluated true */
    uint64_t constraints_violated;           /**< Constraint violations detected */
    uint64_t recommendations_made;           /**< Recommendations published */

    /* Learning statistics */
    uint64_t rule_updates;                   /**< Rule confidence updates */
    float avg_rule_confidence;               /**< Average rule confidence */
    uint32_t rules_deprecated;               /**< Rules below min confidence */

    /* Performance statistics */
    float avg_inference_time_ms;             /**< Average inference time */
    float max_inference_time_ms;             /**< Maximum inference time */
    uint64_t inference_timeouts;             /**< Inference timeouts */

    /* Training outcome correlation */
    uint64_t predictions_correct;            /**< Correct rule predictions */
    uint64_t predictions_incorrect;          /**< Incorrect rule predictions */
    float prediction_accuracy;               /**< Overall prediction accuracy */
} training_logic_hub_stats_t;

/* ============================================================================
 * State
 * ============================================================================ */

/**
 * @brief State of training logic hub bridge
 */
typedef struct {
    bool is_registered;                      /**< Registered with hub */
    bool is_connected;                       /**< Connected to training hub */
    bool is_active;                          /**< Currently processing */
    uint32_t active_rules;                   /**< Number of active rules */
    uint64_t last_event_time;                /**< Last event timestamp */
    training_logic_metrics_t current_metrics;/**< Current training metrics */
} training_logic_hub_state_t;

/* ============================================================================
 * Constraint Violation Event
 * ============================================================================ */

/**
 * @brief Constraint violation event data (published when constraint violated)
 */
typedef struct {
    training_rule_type_t rule_type;          /**< Type of violated rule */
    char rule_name[64];                      /**< Name of violated rule */
    char violation_desc[256];                /**< Description of violation */
    float severity;                          /**< Severity [0,1] */
    bool is_safety_critical;                 /**< Safety-critical violation */
    training_logic_metrics_t metrics;        /**< Metrics at violation time */
} training_constraint_violation_t;

/**
 * @brief Training recommendation event data
 */
typedef struct {
    training_rule_type_t rule_type;          /**< Rule that generated recommendation */
    char action[64];                         /**< Recommended action */
    char rationale[256];                     /**< Reasoning for recommendation */
    float confidence;                        /**< Confidence in recommendation */
    float suggested_value;                   /**< Suggested parameter value (if applicable) */
} training_recommendation_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int training_logic_hub_default_config(training_logic_hub_config_t* config);

/**
 * @brief Create training logic hub bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on error
 */
training_logic_hub_bridge_t* training_logic_hub_create(
    const training_logic_hub_config_t* config);

/**
 * @brief Destroy training logic hub bridge
 * @param bridge Bridge to destroy
 */
void training_logic_hub_destroy(training_logic_hub_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to training integration hub
 * @param bridge Logic hub bridge
 * @param hub Training integration hub
 * @return 0 on success, -1 on error
 */
int training_logic_hub_connect(
    training_logic_hub_bridge_t* bridge,
    training_integration_hub_t hub);

/**
 * @brief Disconnect from training hub
 * @param bridge Logic hub bridge
 * @return 0 on success, -1 on error
 */
int training_logic_hub_disconnect(training_logic_hub_bridge_t* bridge);

/**
 * @brief Connect to cognitive symbolic logic system
 * @param bridge Logic hub bridge
 * @param logic Symbolic logic system
 * @return 0 on success, -1 on error
 *
 * NOTE: This enables sharing knowledge base and rules between
 * training logic and cognitive logic systems.
 */
int training_logic_hub_connect_cognitive_logic(
    training_logic_hub_bridge_t* bridge,
    symbolic_logic_t* logic);

/* ============================================================================
 * Rule Management API
 * ============================================================================ */

/**
 * @brief Add a training rule
 * @param bridge Logic hub bridge
 * @param rule Rule to add
 * @return Rule ID on success, -1 on error
 */
int training_logic_hub_add_rule(
    training_logic_hub_bridge_t* bridge,
    const training_logic_rule_t* rule);

/**
 * @brief Remove a training rule
 * @param bridge Logic hub bridge
 * @param rule_id Rule ID to remove
 * @return 0 on success, -1 on error
 */
int training_logic_hub_remove_rule(
    training_logic_hub_bridge_t* bridge,
    int rule_id);

/**
 * @brief Get a training rule by ID
 * @param bridge Logic hub bridge
 * @param rule_id Rule ID
 * @param rule Output rule structure
 * @return 0 on success, -1 on error
 */
int training_logic_hub_get_rule(
    training_logic_hub_bridge_t* bridge,
    int rule_id,
    training_logic_rule_t* rule);

/**
 * @brief Add default training rules
 * @param bridge Logic hub bridge
 * @return Number of rules added, -1 on error
 *
 * Adds standard rules for:
 * - LR safety (loss_stable AND grad_stable -> safe_lr_increase)
 * - Gradient clipping (grad_norm > threshold -> clip_gradients)
 * - Early stopping (epochs_without_improvement > patience -> stop)
 * - Checkpoint trigger (validation_improved -> checkpoint)
 * - Difficulty progression (mastery > threshold -> increase_difficulty)
 */
int training_logic_hub_add_default_rules(training_logic_hub_bridge_t* bridge);

/* ============================================================================
 * Metrics Update API
 * ============================================================================ */

/**
 * @brief Update training metrics for rule evaluation
 * @param bridge Logic hub bridge
 * @param metrics Current training metrics
 * @return 0 on success, -1 on error
 */
int training_logic_hub_update_metrics(
    training_logic_hub_bridge_t* bridge,
    const training_logic_metrics_t* metrics);

/* ============================================================================
 * Rule Evaluation API
 * ============================================================================ */

/**
 * @brief Evaluate all rules of a specific type
 * @param bridge Logic hub bridge
 * @param type Rule type to evaluate
 * @param results Output array of results
 * @param max_results Maximum results to return
 * @return Number of results, -1 on error
 */
int training_logic_hub_evaluate_rules(
    training_logic_hub_bridge_t* bridge,
    training_rule_type_t type,
    training_rule_result_t* results,
    uint32_t max_results);

/**
 * @brief Evaluate a specific rule by ID
 * @param bridge Logic hub bridge
 * @param rule_id Rule ID to evaluate
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int training_logic_hub_evaluate_rule(
    training_logic_hub_bridge_t* bridge,
    int rule_id,
    training_rule_result_t* result);

/**
 * @brief Check if a training action is safe
 * @param bridge Logic hub bridge
 * @param action Action description (e.g., "increase_lr", "increase_difficulty")
 * @param confidence Output confidence level
 * @return true if safe, false if constrained
 */
bool training_logic_hub_is_action_safe(
    training_logic_hub_bridge_t* bridge,
    const char* action,
    float* confidence);

/* ============================================================================
 * Rule Learning API
 * ============================================================================ */

/**
 * @brief Report training outcome for rule learning
 * @param bridge Logic hub bridge
 * @param loss_improved Whether loss improved since last evaluation
 * @param validation_improved Whether validation improved
 * @return 0 on success, -1 on error
 *
 * This updates rule confidence based on outcomes:
 * - Rules that fired before improvement get boosted
 * - Rules that fired before degradation get penalized
 */
int training_logic_hub_report_outcome(
    training_logic_hub_bridge_t* bridge,
    bool loss_improved,
    bool validation_improved);

/**
 * @brief Get current rule confidence for a rule
 * @param bridge Logic hub bridge
 * @param rule_id Rule ID
 * @return Confidence [0,1], -1 on error
 */
float training_logic_hub_get_rule_confidence(
    training_logic_hub_bridge_t* bridge,
    int rule_id);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Query for recommended learning rate
 * @param bridge Logic hub bridge
 * @param current_lr Current learning rate
 * @param suggested_lr Output suggested learning rate
 * @param confidence Output confidence in suggestion
 * @return 0 on success, -1 on error
 */
int training_logic_hub_query_lr(
    training_logic_hub_bridge_t* bridge,
    float current_lr,
    float* suggested_lr,
    float* confidence);

/**
 * @brief Query for recommended difficulty
 * @param bridge Logic hub bridge
 * @param current_difficulty Current difficulty
 * @param suggested_difficulty Output suggested difficulty
 * @param confidence Output confidence in suggestion
 * @return 0 on success, -1 on error
 */
int training_logic_hub_query_difficulty(
    training_logic_hub_bridge_t* bridge,
    float current_difficulty,
    float* suggested_difficulty,
    float* confidence);

/**
 * @brief Query if early stopping should be triggered
 * @param bridge Logic hub bridge
 * @param should_stop Output: true if should stop
 * @param confidence Output confidence
 * @return 0 on success, -1 on error
 */
int training_logic_hub_query_early_stop(
    training_logic_hub_bridge_t* bridge,
    bool* should_stop,
    float* confidence);

/* ============================================================================
 * State and Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge state
 * @param bridge Logic hub bridge
 * @param state Output state structure
 * @return 0 on success, -1 on error
 */
int training_logic_hub_get_state(
    training_logic_hub_bridge_t* bridge,
    training_logic_hub_state_t* state);

/**
 * @brief Get bridge statistics
 * @param bridge Logic hub bridge
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int training_logic_hub_get_stats(
    training_logic_hub_bridge_t* bridge,
    training_logic_hub_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Logic hub bridge
 * @return 0 on success, -1 on error
 */
int training_logic_hub_reset_stats(training_logic_hub_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_SYMBOLIC_LOGIC_HUB_BRIDGE_H */
