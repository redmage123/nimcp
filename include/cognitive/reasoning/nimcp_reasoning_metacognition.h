/**
 * @file nimcp_reasoning_metacognition.h
 * @brief Metacognitive Controller — adaptive strategy selection for reasoning
 *
 * WHAT: Pre-classifier that estimates query complexity and selects the optimal
 *       reasoning strategy (trivial/sequential/concurrent/convergent)
 * WHY:  Convergent reasoning is expensive; trivial queries should skip it.
 *       The brain doesn't apply maximum cognitive effort to every stimulus.
 * HOW:  Heuristic complexity estimation from query features + outcome learning
 *       via exponential moving average threshold adaptation
 *
 * BIOLOGICAL BASIS:
 * Models the anterior cingulate cortex (ACC) and dorsolateral prefrontal cortex
 * (dlPFC) allocating cognitive resources based on perceived task difficulty.
 * The ACC monitors conflict and effort demands, while the dlPFC adjusts the
 * depth of processing accordingly.
 *
 * INTEGRATION:
 * query -> assess() -> metacognitive_assessment_t -> dispatch strategy
 * outcome -> record_outcome() -> threshold adaptation (EMA learning)
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#ifndef NIMCP_REASONING_METACOGNITION_H
#define NIMCP_REASONING_METACOGNITION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct reasoning_engine;
typedef struct reasoning_engine reasoning_engine_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Number of strategy types */
#define REASONING_NUM_STRATEGIES 4

/** Default complexity threshold for simple queries */
#define REASONING_METACOG_DEFAULT_SIMPLE_THRESHOLD 0.10f

/** Default complexity threshold for moderate queries */
#define REASONING_METACOG_DEFAULT_MODERATE_THRESHOLD 0.30f

/** Default complexity threshold for hard queries */
#define REASONING_METACOG_DEFAULT_HARD_THRESHOLD 0.50f

/** Default learning rate for threshold adaptation */
#define REASONING_METACOG_DEFAULT_LEARNING_RATE 0.05f

/** Default outcome history size */
#define REASONING_METACOG_DEFAULT_HISTORY_SIZE 64

/** Maximum outcome history entries */
#define REASONING_METACOG_MAX_HISTORY_SIZE 256

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Reasoning strategy selected by metacognitive assessment
 *
 * WHAT: Which reasoning pipeline to use for a given query
 * WHY:  Different complexity levels benefit from different strategies
 */
typedef enum {
    REASONING_STRATEGY_TRIVIAL = 0,    /**< Skip to synthesis (minimal) */
    REASONING_STRATEGY_SEQUENTIAL,      /**< 9-phase sequential pipeline */
    REASONING_STRATEGY_CONCURRENT,      /**< Thread-pool parallel pipeline */
    REASONING_STRATEGY_CONVERGENT       /**< Full convergent evidence accumulation */
} reasoning_strategy_t;

/**
 * @brief Estimated query complexity level
 *
 * WHAT: Categorical complexity estimate for a query
 * WHY:  Maps to strategy selection via thresholds
 */
typedef enum {
    REASONING_COMPLEXITY_TRIVIAL = 0,  /**< Very simple factual lookup */
    REASONING_COMPLEXITY_SIMPLE,        /**< Single-clause, no logic */
    REASONING_COMPLEXITY_MODERATE,      /**< Multi-clause, some logic */
    REASONING_COMPLEXITY_COMPLEX,       /**< Multiple logical operators */
    REASONING_COMPLEXITY_HARD           /**< Nested logic, counterfactuals */
} query_complexity_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/* Forward declaration for engine config (defined in nimcp_reasoning_chain.h) */
struct reasoning_engine_config_struct;

/**
 * @brief Continuous resource budget computed from complexity score
 *
 * WHAT: Continuous parameters that scale reasoning resources proportionally
 * WHY:  Avoids hard strategy cutoffs — resource allocation is a smooth gradient
 *       from minimal (score=0) to maximum (score=1)
 *
 * BIOLOGICAL BASIS:
 * The brain doesn't discretely switch between "modes" — it continuously
 * adjusts the number of cortical regions activated, the depth of processing,
 * and the threshold for accepting a conclusion.
 */
typedef struct {
    float parallelism_factor;       /**< 0.0=sequential, 0.5=concurrent, 1.0=convergent */
    uint32_t max_contributors;      /**< Scaled from score: [1, config.max_convergent_contributors] */
    uint32_t max_steps;             /**< Scaled from score: [3, config.max_steps] */
    float convergence_threshold;    /**< Tighter at low scores, looser at high: [0.05, 0.001] */
    float confidence_target;        /**< Min confidence before stopping: [0.9, 0.5] */
    float timeout_factor;           /**< Scales timeout: [0.2, 1.0] */
    bool use_thread_pool;           /**< True when parallelism_factor > 0.1 */
} reasoning_resource_budget_t;

/**
 * @brief Result of metacognitive assessment
 *
 * WHAT: Continuous resource budget + discrete labels for logging/stats
 * WHY:  Dispatch uses the continuous budget; enums are for human readability
 */
typedef struct {
    /* ── Continuous resource budget (used for dispatch) ── */
    reasoning_resource_budget_t budget;  /**< Continuous resource allocation */

    /* ── Discrete labels (for logging/stats/backward compatibility) ── */
    query_complexity_t complexity;              /**< Categorical label (not used for dispatch) */
    reasoning_strategy_t recommended_strategy;  /**< Categorical label (not used for dispatch) */

    /* ── Metadata ── */
    uint32_t estimated_steps;                   /**< Predicted step count */
    float estimated_time_us;                    /**< Predicted execution time */
    float confidence_in_assessment;             /**< How confident the assessment is [0-1] */
    float complexity_score;                     /**< Raw complexity score [0-1] */
} metacognitive_assessment_t;

/**
 * @brief Configuration for the metacognitive controller
 *
 * WHAT: Tuneable parameters for complexity estimation and strategy selection
 * WHY:  Allow adaptation of metacognitive behavior
 */
typedef struct {
    bool enable_metacognition;           /**< Master enable (default true) */
    float complexity_threshold_simple;   /**< Score below this = TRIVIAL (default 0.25) */
    float complexity_threshold_moderate; /**< Score below this = SIMPLE (default 0.50) */
    float complexity_threshold_hard;     /**< Score below this = MODERATE/COMPLEX (default 0.75) */
    float learning_rate;                 /**< EMA rate for threshold adaptation (default 0.05) */
    uint32_t history_size;               /**< Max outcome history entries (default 64) */
} metacognitive_config_t;

/**
 * @brief Aggregate statistics for the metacognitive controller
 *
 * WHAT: Counters and averages for monitoring metacognitive performance
 * WHY:  Track how well strategy selection matches actual query difficulty
 */
typedef struct {
    uint32_t total_assessments;                         /**< Total assessments made */
    uint32_t strategy_counts[REASONING_NUM_STRATEGIES]; /**< Per-strategy usage counts */
    float avg_assessment_time_us;                       /**< Average assessment time */
    float accuracy;                                     /**< Calibrated accuracy [0-1] */
} metacognitive_stats_t;

/**
 * @brief Opaque metacognitive controller instance
 */
typedef struct reasoning_metacognition reasoning_metacognition_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Get default metacognitive configuration
 *
 * WHAT: Return configuration with sensible defaults
 * WHY:  Simplify controller creation
 *
 * @return Default configuration struct
 */
metacognitive_config_t reasoning_metacognition_default_config(void);

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create a metacognitive controller
 *
 * WHAT: Allocate and initialize a metacognitive controller instance
 * WHY:  Required before any assessment operations
 *
 * @param config Configuration (NULL for defaults)
 * @return Controller instance or NULL on allocation failure
 */
reasoning_metacognition_t* reasoning_metacognition_create(
    const metacognitive_config_t* config);

/**
 * @brief Destroy a metacognitive controller
 *
 * WHAT: Free all controller resources
 * WHY:  Prevent memory leaks
 *
 * @param mc Controller to destroy (NULL safe)
 */
void reasoning_metacognition_destroy(reasoning_metacognition_t* mc);

/*=============================================================================
 * CORE ASSESSMENT
 *===========================================================================*/

/**
 * @brief Assess query complexity and recommend a strategy
 *
 * WHAT: Estimate query complexity from linguistic features and select strategy
 * WHY:  Core metacognitive function — route queries to appropriate pipeline
 * HOW:  Heuristic scoring: length, logical operators, causal language,
 *       counterfactuals, nested structure → complexity score → strategy
 *
 * @param mc Metacognitive controller (non-NULL)
 * @param query Natural language query string (non-NULL)
 * @param engine_config Engine configuration for override checks (may be NULL)
 * @return Assessment result (zeroed on error)
 */
metacognitive_assessment_t reasoning_metacognition_assess(
    reasoning_metacognition_t* mc,
    const char* query,
    const void* engine_config);

/*=============================================================================
 * OUTCOME LEARNING
 *===========================================================================*/

/**
 * @brief Record reasoning outcome for threshold adaptation
 *
 * WHAT: Feed back actual reasoning results to calibrate future assessments
 * WHY:  Enable online learning — if "simple" queries consistently need many
 *       steps, the simple threshold should shift
 *
 * @param mc Metacognitive controller (non-NULL)
 * @param used Strategy that was actually used
 * @param actual_confidence Achieved confidence [0-1]
 * @param actual_time_us Actual execution time in microseconds
 * @param actual_steps Actual number of reasoning steps
 * @return 0 on success, -1 on error
 */
int reasoning_metacognition_record_outcome(
    reasoning_metacognition_t* mc,
    reasoning_strategy_t used,
    float actual_confidence,
    float actual_time_us,
    uint32_t actual_steps);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get metacognitive statistics
 *
 * @param mc Metacognitive controller (non-NULL)
 * @param stats Output statistics struct (non-NULL)
 * @return 0 on success, -1 on error
 */
int reasoning_metacognition_get_stats(
    const reasoning_metacognition_t* mc,
    metacognitive_stats_t* stats);

/*=============================================================================
 * UTILITY
 *===========================================================================*/

/**
 * @brief Get strategy name as string
 *
 * @param strategy Strategy type
 * @return Static string name (never NULL)
 */
const char* reasoning_metacognition_get_strategy_name(reasoning_strategy_t strategy);

/**
 * @brief Get complexity name as string
 *
 * @param complexity Complexity level
 * @return Static string name (never NULL)
 */
const char* reasoning_metacognition_get_complexity_name(query_complexity_t complexity);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_METACOGNITION_H */
