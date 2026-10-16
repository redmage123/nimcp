/**
 * @file nimcp_brain_recovery_integration.h
 * @brief Brain-Driven Intelligent Recovery - Cognitive Fault Tolerance Integration
 *
 * WHAT: Integrates NIMCP's cognitive capabilities with fault tolerance for intelligent recovery
 * WHY:  Enable the brain to participate in recovery decisions using reasoning and learning
 * HOW:  Connect executive function, working memory, and episodic learning to recovery system
 *
 * PHILOSOPHY:
 * Traditional fault tolerance: Fixed rules, static strategies
 * Brain-driven recovery: Learning from experience, adapting strategies, predicting outcomes
 *
 * COGNITIVE RECOVERY LOOP:
 * 1. Error Detected → Diagnostics run
 * 2. Brain analyzes failure pattern (Working Memory checks history)
 * 3. Brain reasons about root cause (Executive Function evaluates options)
 * 4. Brain selects recovery strategy (Confidence-based decision)
 * 5. Recovery executes (with brain monitoring)
 * 6. Brain learns outcome (Episodic Memory stores success/failure)
 * 7. Future decisions adapt (Learning from experience)
 *
 * KEY CONSTRAINT: NO CODE GENERATION
 * - All "self-healing" is runtime adaptation only
 * - Parameter adjustments, config changes, weight modifications
 * - NO compiler assumed in production environment
 *
 * @author NIMCP Team
 * @date 2025-11-19
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_RECOVERY_INTEGRATION_H
#define NIMCP_BRAIN_RECOVERY_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Import existing fault tolerance infrastructure
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "utils/fault_tolerance/nimcp_health_monitor.h"

// Import cognitive systems
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Brain Recovery Context
//=============================================================================

/**
 * @brief Opaque handle for brain recovery context
 *
 * WHAT: Maintains state for brain-driven recovery
 * WHY:  Store recovery history, learned patterns, decision metrics
 * HOW:  Connects to brain's cognitive systems (working memory, executive, etc.)
 */
typedef struct brain_recovery_context_internal* brain_recovery_context_t;

//=============================================================================
// Recovery Learning Structures
//=============================================================================

/**
 * @brief Recovery outcome for learning
 */
typedef struct {
    recovery_strategy_t* strategy;     /**< Strategy attempted */
    recovery_result_t* result;         /**< Actual result */
    diagnostic_result_t* diagnosis;    /**< Original diagnosis */

    uint64_t timestamp_us;             /**< When executed */
    uint32_t execution_time_us;        /**< How long it took */

    bool was_successful;               /**< Success or failure */
    float expected_success_prob;       /**< Brain's prediction */
    float actual_success;              /**< Actual outcome (1.0 or 0.0) */

    char learned_insight[512];         /**< What was learned */
} recovery_outcome_t;

/**
 * @brief Recovery pattern learned from history
 */
typedef struct {
    char failure_signature[256];       /**< Pattern fingerprint */
    recovery_tier_t best_tier;         /**< Most effective tier */
    recovery_action_t best_action;     /**< Most effective action */

    uint32_t occurrence_count;         /**< How many times seen */
    uint32_t success_count;            /**< How many successes */
    float success_rate;                /**< Success rate (0-1) */

    uint64_t first_seen_us;            /**< First occurrence */
    uint64_t last_seen_us;             /**< Most recent occurrence */

    float confidence;                  /**< Confidence in pattern (0-1) */
} recovery_pattern_t;

/**
 * @brief Brain's recovery decision with reasoning
 */
typedef struct {
    recovery_strategy_t* selected_strategy; /**< Strategy brain chose */

    float confidence;                  /**< Confidence in decision (0-1) */
    float predicted_success_prob;      /**< Predicted success probability */
    uint32_t estimated_time_us;        /**< Estimated recovery time */

    char reasoning[512];               /**< Why this strategy was chosen */
    char alternative_strategies[3][256]; /**< Top 3 alternatives considered */
    float alternative_scores[3];       /**< Scores of alternatives */

    bool requires_user_confirmation;   /**< High-risk decision? */
    bool is_novel_situation;           /**< Never seen before? */
    char failure_signature[256];       /**< Signature for pattern matching */
} brain_recovery_decision_t;

//=============================================================================
// Initialization & Lifecycle
//=============================================================================

/**
 * @brief Initialize brain-driven recovery system
 *
 * WHAT: Create recovery context connected to brain's cognitive systems
 * WHY:  Enable brain to participate in recovery decisions
 * HOW:  Integrate with executive function, working memory, episodic memory
 *
 * INTEGRATION POINTS:
 * - Executive Function: Decision making and strategy selection
 * - Working Memory: Track recent failure patterns
 * - Episodic Memory: Learn from recovery outcomes
 * - Knowledge System: Store recovery strategies
 * - Reasoning System: Analyze root causes
 *
 * @param brain Brain instance to integrate with
 * @return Recovery context handle, NULL on error
 *
 * COMPLEXITY: O(1) initialization
 * THREAD-SAFE: Yes (per-brain context)
 * MALLOC: Yes (context + history buffers)
 */
brain_recovery_context_t brain_recovery_init(brain_t brain);

/**
 * @brief Shutdown brain recovery system
 *
 * WHAT: Clean up resources, save learned patterns
 * WHY:  Graceful shutdown, preserve learning
 * HOW:  Write recovery history, free memory
 *
 * @param ctx Recovery context (can be NULL)
 *
 * COMPLEXITY: O(n) where n = number of learned patterns
 * THREAD-SAFE: No (caller must ensure exclusive access)
 */
void brain_recovery_shutdown(brain_recovery_context_t ctx);

//=============================================================================
// Brain-Driven Strategy Selection
//=============================================================================

/**
 * @brief Brain selects recovery strategy using cognitive reasoning
 *
 * WHAT: Brain analyzes failure and chooses best recovery strategy
 * WHY:  Leverage learned patterns and reasoning for better decisions
 * HOW:
 *   1. Working memory checks for similar past failures
 *   2. Executive function evaluates strategy options
 *   3. Reasoning system predicts success probability
 *   4. Knowledge system provides domain expertise
 *   5. Decision integrates all cognitive inputs
 *
 * COGNITIVE PROCESS:
 * - Pattern Matching: "Have I seen this before?"
 * - Causal Reasoning: "What likely caused this?"
 * - Predictive Modeling: "What strategy will work best?"
 * - Risk Assessment: "What could go wrong?"
 * - Learning Transfer: "Similar to situation X where Y worked"
 *
 * @param ctx Recovery context
 * @param diagnosis Diagnostic result from failure analysis
 * @param current_health Current system health status
 * @return Brain's recovery decision with reasoning
 *
 * COMPLEXITY: O(log n + k) where n=history size, k=strategy options
 * THREAD-SAFE: No
 * MALLOC: Yes (decision structure)
 */
brain_recovery_decision_t* brain_recovery_select_strategy(
    brain_recovery_context_t ctx,
    diagnostic_result_t* diagnosis,
    health_status_snapshot_t* current_health
);

/**
 * @brief Free recovery decision structure
 *
 * @param decision Decision to free (can be NULL)
 */
void brain_recovery_free_decision(brain_recovery_decision_t* decision);

//=============================================================================
// Learning from Recovery Outcomes
//=============================================================================

/**
 * @brief Brain learns from recovery outcome
 *
 * WHAT: Update brain's recovery knowledge based on outcome
 * WHY:  Improve future recovery decisions through learning
 * HOW:
 *   - Store outcome in episodic memory
 *   - Update strategy success rates
 *   - Adjust confidence in similar patterns
 *   - Extract generalizable insights
 *
 * LEARNING MECHANISMS:
 * - Reinforcement: Successful strategies reinforced
 * - Pattern Recognition: Extract failure signatures
 * - Prediction Error: Update success probability models
 * - Abstraction: Generalize to similar situations
 * - Forgetting: Decay outdated patterns
 *
 * @param ctx Recovery context
 * @param decision Original decision
 * @param result Actual recovery result
 *
 * COMPLEXITY: O(log n) for pattern update
 * THREAD-SAFE: No
 * SIDE-EFFECTS: Updates internal learning structures
 */
void brain_recovery_learn_outcome(
    brain_recovery_context_t ctx,
    brain_recovery_decision_t* decision,
    recovery_result_t* result
);

/**
 * @brief Get learned recovery patterns
 *
 * WHAT: Retrieve patterns brain has learned
 * WHY:  Inspect what brain has learned, export knowledge
 * HOW:  Return sorted list of patterns by confidence
 *
 * @param ctx Recovery context
 * @param patterns Output array for patterns
 * @param max_patterns Maximum patterns to return
 * @return Number of patterns returned
 *
 * COMPLEXITY: O(n log n) for sorting
 * THREAD-SAFE: Yes (read-only)
 */
uint32_t brain_recovery_get_patterns(
    brain_recovery_context_t ctx,
    recovery_pattern_t* patterns,
    uint32_t max_patterns
);

//=============================================================================
// Runtime Parameter Suggestions (No Code Generation!)
//=============================================================================

/**
 * @brief Parameter adjustment recommendation
 */
typedef struct {
    const char* parameter_name;        /**< Parameter to adjust */
    float current_value;               /**< Current value */
    float suggested_value;             /**< Suggested new value */
    float confidence;                  /**< Confidence (0-1) */
    char rationale[256];               /**< Why this adjustment */
} parameter_adjustment_t;

/**
 * @brief Brain suggests runtime parameter adjustments
 *
 * WHAT: Brain analyzes failure and suggests parameter changes
 * WHY:  Prevent recurrence through runtime adaptation
 * HOW:  Reason about failure cause → suggest parameter fixes
 *
 * ADJUSTABLE PARAMETERS (runtime only, no compilation):
 * - Learning rate: 0.0001 - 1.0
 * - Batch size: 1 - 1000
 * - Temperature: 0.1 - 10.0
 * - Dropout rate: 0.0 - 0.9
 * - Weight decay: 0.0 - 0.1
 * - Momentum: 0.0 - 0.99
 * - Gradient clipping: 0.1 - 10.0
 * - Activation threshold: -10.0 - 10.0
 * - Plasticity rate: 0.0 - 1.0
 * - Neuromodulation levels: 0.0 - 2.0
 *
 * EXAMPLE SCENARIOS:
 * - NaN detected → Reduce learning rate by 50%
 * - Memory pressure → Reduce batch size by 50%
 * - Gradient explosion → Enable gradient clipping
 * - Slow convergence → Increase learning rate by 20%
 * - Overfitting → Increase dropout rate
 *
 * @param ctx Recovery context
 * @param diagnosis Diagnostic result
 * @param adjustments Output array for recommendations
 * @param max_adjustments Maximum recommendations
 * @return Number of adjustments suggested
 *
 * COMPLEXITY: O(1) analysis
 * THREAD-SAFE: Yes
 */
uint32_t brain_recovery_suggest_parameters(
    brain_recovery_context_t ctx,
    diagnostic_result_t* diagnosis,
    parameter_adjustment_t* adjustments,
    uint32_t max_adjustments
);

//=============================================================================
// Success Probability Prediction
//=============================================================================

/**
 * @brief Brain predicts recovery strategy success probability
 *
 * WHAT: Estimate likelihood of strategy succeeding
 * WHY:  Avoid attempting strategies likely to fail
 * HOW:  Analyze:
 *   - Past success rate for this failure type
 *   - Current system health
 *   - Strategy complexity vs. available resources
 *   - Similar historical outcomes
 *
 * PREDICTION MODEL:
 * - Base rate: Historical success rate for strategy
 * - Similarity: How similar to past successes
 * - Health factor: Current system health impact
 * - Confidence: Uncertainty in prediction
 *
 * @param ctx Recovery context
 * @param strategy Strategy to evaluate
 * @param diagnosis Current diagnosis
 * @return Success probability (0.0-1.0), -1.0 on error
 *
 * COMPLEXITY: O(log n) for pattern lookup
 * THREAD-SAFE: Yes
 */
float brain_recovery_predict_success(
    brain_recovery_context_t ctx,
    recovery_strategy_t* strategy,
    diagnostic_result_t* diagnosis
);

//=============================================================================
// Recovery History & Analytics
//=============================================================================

/**
 * @brief Recovery history statistics
 */
typedef struct {
    uint32_t total_recoveries;         /**< Total recovery attempts */
    uint32_t successful_recoveries;    /**< Successful recoveries */
    uint32_t failed_recoveries;        /**< Failed recoveries */
    float success_rate;                /**< Overall success rate */

    uint32_t total_patterns_learned;   /**< Unique patterns */
    float avg_prediction_accuracy;     /**< How accurate predictions are */

    recovery_tier_t most_effective_tier; /**< Best performing tier */
    uint32_t avg_recovery_time_us;     /**< Average recovery time */

    uint32_t novel_failures;           /**< Never-seen-before failures */
    uint32_t recurring_failures;       /**< Repeated failures */
} recovery_history_stats_t;

/**
 * @brief Get recovery history statistics
 *
 * @param ctx Recovery context
 * @param stats Output statistics structure
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool brain_recovery_get_stats(
    brain_recovery_context_t ctx,
    recovery_history_stats_t* stats
);

/**
 * @brief Get recent recovery outcomes
 *
 * @param ctx Recovery context
 * @param outcomes Output array for recent outcomes
 * @param max_outcomes Maximum outcomes to return
 * @return Number of outcomes returned
 *
 * COMPLEXITY: O(n) where n = max_outcomes
 * THREAD-SAFE: Yes
 */
uint32_t brain_recovery_get_recent_outcomes(
    brain_recovery_context_t ctx,
    recovery_outcome_t* outcomes,
    uint32_t max_outcomes
);

//=============================================================================
// Integration with Cognitive Pipeline
//=============================================================================

/**
 * @brief Register recovery context with cognitive pipeline
 *
 * WHAT: Hook recovery into brain's cognitive processing
 * WHY:  Enable bidirectional information flow
 * HOW:  Register callbacks with executive function, working memory
 *
 * @param ctx Recovery context
 * @param brain Brain instance
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool brain_recovery_register_pipeline(
    brain_recovery_context_t ctx,
    brain_t brain
);

/**
 * @brief Unregister recovery context from pipeline
 *
 * @param ctx Recovery context
 * @param brain Brain instance
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void brain_recovery_unregister_pipeline(
    brain_recovery_context_t ctx,
    brain_t brain
);

//=============================================================================
// Persistence
//=============================================================================

/**
 * @brief Save recovery learning to file
 *
 * WHAT: Persist learned patterns and history
 * WHY:  Resume learning across sessions
 * HOW:  Serialize patterns, history, statistics
 *
 * @param ctx Recovery context
 * @param filepath Output file path
 * @return true on success
 *
 * COMPLEXITY: O(n) where n = number of patterns
 * THREAD-SAFE: Yes
 */
bool brain_recovery_save(
    brain_recovery_context_t ctx,
    const char* filepath
);

/**
 * @brief Load recovery learning from file
 *
 * @param brain Brain to associate with
 * @param filepath Input file path
 * @return Recovery context or NULL on error
 *
 * COMPLEXITY: O(n) where n = number of patterns
 * THREAD-SAFE: Yes
 */
brain_recovery_context_t brain_recovery_load(
    brain_t brain,
    const char* filepath
);

//=============================================================================
// Diagnostics & Debugging
//=============================================================================

/**
 * @brief Generate recovery learning report
 *
 * @param ctx Recovery context
 * @param output Output stream (stdout, file, etc.)
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
void brain_recovery_report(
    brain_recovery_context_t ctx,
    FILE* output
);

/**
 * @brief Export recovery data to JSON
 *
 * @param ctx Recovery context
 * @param json_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of bytes written, -1 on error
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
int32_t brain_recovery_export_json(
    brain_recovery_context_t ctx,
    char* json_buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_RECOVERY_INTEGRATION_H
