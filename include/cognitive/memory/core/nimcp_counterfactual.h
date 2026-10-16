//=============================================================================
// nimcp_counterfactual.h - Counterfactual Reasoning System for Prime Resonant Memory
//=============================================================================
/**
 * @file nimcp_counterfactual.h
 * @brief "What if" thinking and alternative past reasoning for cognitive memory
 *
 * WHAT: Counterfactual reasoning generates alternative scenarios based on
 *       mutations to past events, computing emotional impact and causal chains
 * WHY:  Counterfactual thinking is essential for learning, planning, and
 *       emotional regulation - imagining alternatives to understand cause/effect
 * HOW:  Mutates memory elements, propagates changes through entanglement graph,
 *       and computes affect changes (regret/relief) from outcome differences
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Counterfactual Reasoning in the Brain:
 *   +-----------------------------------------------------------------------+
 *   |  Counterfactual thinking activates distinct neural networks:          |
 *   |                                                                       |
 *   |  Prefrontal Cortex (PFC):                                             |
 *   |  - Dorsolateral PFC: Working memory for alternative scenarios         |
 *   |  - Ventromedial PFC: Affective valuation of alternatives              |
 *   |  - Anterior cingulate: Conflict detection (actual vs. possible)       |
 *   |                                                                       |
 *   |  Default Mode Network (DMN):                                          |
 *   |  - Mental simulation and scene construction                           |
 *   |  - Self-referential processing ("What if I had...")                   |
 *   |                                                                       |
 *   |  Amygdala:                                                            |
 *   |  - Regret computation for upward counterfactuals                      |
 *   |  - Relief computation for downward counterfactuals                    |
 *   |                                                                       |
 *   |  Hippocampus:                                                         |
 *   |  - Episodic memory retrieval for basis events                         |
 *   |  - Recombination of elements into novel scenarios                     |
 *   +-----------------------------------------------------------------------+
 *
 *   Types of Counterfactual Thinking:
 *   +-----------------------------------------------------------------------+
 *   |  Upward Counterfactuals: "Things could have been better"              |
 *   |  - Trigger: Negative outcomes                                         |
 *   |  - Emotion: Regret, disappointment                                    |
 *   |  - Function: Learning, behavior change                                |
 *   |                                                                       |
 *   |  Downward Counterfactuals: "Things could have been worse"             |
 *   |  - Trigger: Near misses, close calls                                  |
 *   |  - Emotion: Relief, gratitude                                         |
 *   |  - Function: Emotional comfort, coping                                |
 *   |                                                                       |
 *   |  Additive vs. Subtractive:                                            |
 *   |  - Additive: "If only I had done X" (adding an action)                |
 *   |  - Subtractive: "If only I hadn't done X" (removing an action)        |
 *   +-----------------------------------------------------------------------+
 *
 *   Mutability Heuristics (Kahneman & Miller, 1986):
 *   +-----------------------------------------------------------------------+
 *   |  Some events are mentally "easier" to change than others:             |
 *   |                                                                       |
 *   |  HIGH MUTABILITY (easy to mentally change):                           |
 *   |  - Actions over inactions ("Action effect")                           |
 *   |  - Exceptional events over routine events                             |
 *   |  - Recent events over distant events                                  |
 *   |  - Controllable events over uncontrollable                            |
 *   |  - First events in sequence over later events                         |
 *   |                                                                       |
 *   |  LOW MUTABILITY (hard to mentally change):                            |
 *   |  - Physical laws, gravity, etc.                                       |
 *   |  - Character traits ("She's just like that")                          |
 *   |  - Background vs. foreground events                                   |
 *   +-----------------------------------------------------------------------+
 *
 *   Causal Reasoning Model:
 *   +-----------------------------------------------------------------------+
 *   |  For counterfactuals to be meaningful, we need causal structure:      |
 *   |                                                                       |
 *   |  1. Temporal precedence: Cause must precede effect                    |
 *   |  2. Covariation: Cause and effect vary together                       |
 *   |  3. Mechanism: Plausible pathway from cause to effect                 |
 *   |  4. Counterfactual dependence: No cause => no effect                  |
 *   |                                                                       |
 *   |  Causal matrix stored in counterfactual_system_t tracks learned       |
 *   |  cause-effect relationships from experience.                          |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Counterfactual generation: ~10us per mutation
 * - Outcome evaluation: ~50us (depends on entanglement graph size)
 * - Affect computation: ~1us
 * - Causal extraction: ~100us for dense graphs
 *
 * MEMORY:
 * - counterfactual_t: ~128 bytes
 * - counterfactual_analysis_t: ~256 bytes + arrays
 * - counterfactual_system_t: ~50KB base + O(N^2) causal matrix
 *
 * INTEGRATION:
 * - Core: PR Memory Nodes, Prime Signatures, Entanglement Graph
 * - Middleware: Emotional processing, Decision making
 * - API: Query "what if" scenarios, Get regret/relief scores
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_COUNTERFACTUAL_H
#define NIMCP_COUNTERFACTUAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_entanglement.h"
#include "cognitive/memory/core/nimcp_resonance.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of causal factors to extract */
#define CF_MAX_CAUSES                       64

/** Maximum counterfactuals per analysis */
#define CF_MAX_COUNTERFACTUALS              32

/** Default causal matrix dimension */
#define CF_DEFAULT_CAUSAL_DIM               256

/** Maximum analyses to cache */
#define CF_DEFAULT_MAX_ANALYSES             1024

/** Default minimum causal strength to consider */
#define CF_MIN_CAUSAL_STRENGTH              0.1f

/** Default regret/relief decay over time (per day) */
#define CF_AFFECT_DECAY_RATE                0.05f

/** Epsilon for floating-point comparisons */
#define CF_EPSILON                          1e-6f

/** Default mutability for action mutations */
#define CF_DEFAULT_ACTION_MUTABILITY        0.8f

/** Default mutability for event mutations */
#define CF_DEFAULT_EVENT_MUTABILITY         0.4f

/** Default mutability for timing mutations */
#define CF_DEFAULT_TIMING_MUTABILITY        0.6f

/** Threshold for considering an outcome significantly better */
#define CF_SIGNIFICANT_IMPROVEMENT          0.2f

/** Threshold for considering an outcome significantly worse */
#define CF_SIGNIFICANT_DEGRADATION          -0.2f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Counterfactual direction (upward vs downward thinking)
 *
 * WHAT: Whether the alternative is better or worse than reality
 * WHY:  Different directions serve different psychological functions
 */
typedef enum {
    COUNTER_UPWARD = 0,     /**< Things could have been better (triggers regret) */
    COUNTER_DOWNWARD,       /**< Things could have been worse (triggers relief) */
    COUNTER_ADDITIVE,       /**< Adding something that was absent */
    COUNTER_SUBTRACTIVE,    /**< Removing something that was present */
    COUNTER_TYPE_COUNT      /**< Number of counterfactual types */
} counterfactual_type_t;

/**
 * @brief Mutation type - what aspect of the event is changed
 *
 * WHAT: The category of mental change applied to create alternative
 * WHY:  Different mutations have different psychological implications
 */
typedef enum {
    MUTATE_ACTION = 0,      /**< "If I had done X instead of Y" */
    MUTATE_INACTION,        /**< "If I had done nothing" */
    MUTATE_PERSON,          /**< "If person P had been different" */
    MUTATE_EVENT,           /**< "If event E hadn't happened" */
    MUTATE_TIMING,          /**< "If I had acted sooner/later" */
    MUTATE_CONTEXT,         /**< "If the situation had been different" */
    MUTATE_TYPE_COUNT       /**< Number of mutation types */
} mutation_type_t;

/**
 * @brief Single counterfactual scenario
 *
 * WHAT: A specific "what if" alternative to an actual event
 * WHY:  Captures the mutation (antecedent) and predicted consequence
 * HOW:  Links original memory to mutated element and predicted outcome
 *
 * Memory layout: ~128 bytes
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Antecedent (what was changed)
    //-------------------------------------------------------------------------
    pr_memory_node_t* original_memory;  /**< Reference to the actual event */
    mutation_type_t mutation_type;      /**< Type of mental change applied */
    prime_signature_t original_element; /**< Signature of what was changed */
    prime_signature_t mutated_element;  /**< Signature of the alternative */

    //-------------------------------------------------------------------------
    // Consequent (what would have happened)
    //-------------------------------------------------------------------------
    prime_signature_t alternate_outcome; /**< Predicted alternative outcome */
    float outcome_probability;           /**< How likely this outcome [0, 1] */
    float outcome_valence;               /**< How good/bad the outcome [-1, +1] */

    //-------------------------------------------------------------------------
    // Emotional Impact
    //-------------------------------------------------------------------------
    float affect_change;                 /**< How much better/worse [-1, +1] */
    float regret_intensity;              /**< Regret if upward [0, 1] */
    float relief_intensity;              /**< Relief if downward [0, 1] */

    //-------------------------------------------------------------------------
    // Metadata
    //-------------------------------------------------------------------------
    counterfactual_type_t direction;     /**< Upward, downward, additive, subtractive */
    uint64_t created_time_ms;            /**< When this counterfactual was generated */
    float salience;                      /**< How attention-grabbing [0, 1] */
    bool is_controllable;                /**< Was the original controllable? */

} counterfactual_t;

/**
 * @brief Complete counterfactual analysis of a memory
 *
 * WHAT: All counterfactual reasoning about a single event
 * WHY:  Captures causal structure and multiple alternative scenarios
 * HOW:  Extracts causes, generates counterfactuals, finds most impactful
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Target Memory
    //-------------------------------------------------------------------------
    pr_memory_node_t* memory;            /**< The memory being analyzed */
    uint64_t memory_id;                  /**< Memory ID for tracking */

    //-------------------------------------------------------------------------
    // Causal Factors
    //-------------------------------------------------------------------------
    prime_signature_t* causes;           /**< Array of causal signatures */
    float* causal_strengths;             /**< Strength of each cause [0, 1] */
    size_t num_causes;                   /**< Number of causes identified */
    size_t max_causes;                   /**< Capacity of causes array */

    //-------------------------------------------------------------------------
    // Counterfactual Scenarios
    //-------------------------------------------------------------------------
    counterfactual_t* counterfactuals;   /**< Array of counterfactual scenarios */
    size_t num_counterfactuals;          /**< Number generated */
    size_t max_counterfactuals;          /**< Capacity of array */

    //-------------------------------------------------------------------------
    // Most Impactful Mutation
    //-------------------------------------------------------------------------
    counterfactual_t* most_impactful;    /**< Pointer to highest |affect_change| */
    counterfactual_t* most_regretted;    /**< Highest regret (upward, negative) */
    counterfactual_t* most_relieving;    /**< Highest relief (downward, positive) */

    //-------------------------------------------------------------------------
    // Mutability Scores
    //-------------------------------------------------------------------------
    float action_mutability;             /**< How easy to change actions [0, 1] */
    float event_mutability;              /**< How easy to change events [0, 1] */
    float timing_mutability;             /**< How easy to change timing [0, 1] */
    float person_mutability;             /**< How easy to change people [0, 1] */
    float overall_mutability;            /**< Weighted average mutability */

    //-------------------------------------------------------------------------
    // Metadata
    //-------------------------------------------------------------------------
    uint64_t analysis_time_ms;           /**< When analysis was performed */
    float memory_recency;                /**< How recent (affects mutability) */
    float memory_exceptionality;         /**< How exceptional (affects mutability) */

} counterfactual_analysis_t;

/**
 * @brief Configuration for counterfactual system
 */
typedef struct {
    size_t causal_dim;                   /**< Dimension of causal matrix */
    size_t max_analyses;                 /**< Maximum cached analyses */
    size_t max_counterfactuals;          /**< Max counterfactuals per analysis */
    size_t max_causes;                   /**< Max causes per analysis */

    float min_causal_strength;           /**< Minimum strength to consider cause */
    float affect_decay_rate;             /**< Decay rate for regret/relief */

    float action_mutability_weight;      /**< Weight for action mutability */
    float event_mutability_weight;       /**< Weight for event mutability */
    float timing_mutability_weight;      /**< Weight for timing mutability */
    float recency_weight;                /**< How much recency affects mutability */
    float exceptionality_weight;         /**< How much exceptionality affects mutability */

    bool enable_downward_counterfactuals; /**< Generate "could be worse" scenarios */
    bool enable_causal_learning;         /**< Update causal matrix from experience */
    bool enable_affect_computation;      /**< Compute regret/relief scores */

} counterfactual_config_t;

/**
 * @brief Statistics for counterfactual system
 */
typedef struct {
    uint64_t total_analyses;             /**< Total analyses performed */
    uint64_t total_counterfactuals;      /**< Total counterfactuals generated */
    uint64_t upward_count;               /**< Upward counterfactuals generated */
    uint64_t downward_count;             /**< Downward counterfactuals generated */
    float mean_regret;                   /**< Average regret intensity */
    float mean_relief;                   /**< Average relief intensity */
    float max_regret;                    /**< Maximum regret observed */
    float max_relief;                    /**< Maximum relief observed */
    size_t cached_analyses;              /**< Currently cached analyses */
    size_t causal_links_learned;         /**< Causal relationships learned */
    uint64_t computation_time_ns;        /**< Total computation time */
} counterfactual_stats_t;

/**
 * @brief Counterfactual reasoning system handle (opaque)
 *
 * Internal structure contains:
 * - Entanglement graph reference for causal inference
 * - Node manager reference for memory access
 * - Causal matrix (learned cause-effect relationships)
 * - Analysis cache
 * - Configuration and statistics
 */
typedef struct counterfactual_system_struct* counterfactual_system_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default counterfactual system configuration
 *
 * WHAT: Returns sensible defaults for counterfactual reasoning
 * WHY:  Provides starting point for most use cases
 *
 * @return Default configuration with:
 *         - causal_dim: 256
 *         - max_analyses: 1024
 *         - max_counterfactuals: 32
 *         - max_causes: 64
 *         - All features enabled
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT counterfactual_config_t counterfactual_config_default(void);

/**
 * @brief Validate counterfactual configuration
 *
 * WHAT: Ensures configuration values are valid
 * WHY:  Prevent invalid configs causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool counterfactual_config_validate(const counterfactual_config_t* config);

//=============================================================================
// System Lifecycle Functions
//=============================================================================

/**
 * @brief Create a counterfactual reasoning system
 *
 * WHAT: Allocates and initializes counterfactual system
 * WHY:  Entry point for "what if" reasoning capability
 * HOW:  Creates causal matrix, initializes cache, links to graph
 *
 * @param entanglement Entanglement graph for causal structure
 * @param node_manager Node manager for memory access
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on failure
 *
 * Performance: O(causal_dim^2) for matrix initialization
 * Memory: ~50KB base + O(causal_dim^2) for matrix
 *
 * Example:
 *   counterfactual_system_t cf = counterfactual_create(graph, mgr, NULL);
 *   if (!cf) {
 *       fprintf(stderr, "Failed: %s\n", pr_counterfactual_get_last_error());
 *   }
 */
NIMCP_EXPORT counterfactual_system_t counterfactual_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const counterfactual_config_t* config
);

/**
 * @brief Destroy counterfactual system and free all resources
 *
 * WHAT: Deallocates system and all cached analyses
 * WHY:  Resource cleanup
 *
 * @param system System to destroy (NULL safe)
 *
 * Performance: O(cached_analyses)
 */
NIMCP_EXPORT void counterfactual_destroy(counterfactual_system_t system);

/**
 * @brief Clear cached analyses without destroying system
 *
 * WHAT: Clears analysis cache, keeps causal matrix
 * WHY:  Reset cache without losing learned causal knowledge
 *
 * @param system System to clear
 * @return true on success, false if system is NULL
 */
NIMCP_EXPORT bool counterfactual_clear_cache(counterfactual_system_t system);

/**
 * @brief Reset causal matrix to initial state
 *
 * WHAT: Clears all learned causal relationships
 * WHY:  Start causal learning from scratch
 *
 * @param system System to reset
 * @return true on success, false if system is NULL
 */
NIMCP_EXPORT bool counterfactual_reset_causal_matrix(counterfactual_system_t system);

//=============================================================================
// Main Analysis Functions
//=============================================================================

/**
 * @brief Analyze a memory for counterfactuals
 *
 * WHAT: Main entry point - comprehensive counterfactual analysis
 * WHY:  Generates all relevant "what if" scenarios for a memory
 * HOW:  Extracts causes, applies mutations, computes outcomes and affect
 *
 * ALGORITHM:
 *   1. Extract causal factors from entanglement graph
 *   2. Compute mutability scores based on heuristics
 *   3. Generate upward counterfactuals (better alternatives)
 *   4. Generate downward counterfactuals (worse alternatives) if enabled
 *   5. Evaluate outcome probabilities via graph propagation
 *   6. Compute regret/relief for each scenario
 *   7. Identify most impactful mutation
 *
 * @param system Counterfactual system
 * @param memory Memory to analyze
 * @param analysis Output analysis structure (caller-allocated)
 * @return true on success, false on error
 *
 * Performance: ~100us for typical memory with moderate connections
 *
 * Example:
 *   counterfactual_analysis_t analysis;
 *   if (counterfactual_analyze(cf, memory, &analysis)) {
 *       printf("Found %zu counterfactuals\n", analysis.num_counterfactuals);
 *       if (analysis.most_impactful) {
 *           printf("Most impactful: affect=%.2f\n",
 *                  analysis.most_impactful->affect_change);
 *       }
 *   }
 */
NIMCP_EXPORT bool counterfactual_analyze(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    counterfactual_analysis_t* analysis
);

/**
 * @brief Generate a specific counterfactual mutation
 *
 * WHAT: Create single counterfactual by mutating specific element
 * WHY:  Targeted "what if" for specific alternative
 * HOW:  Applies mutation, propagates through graph, computes outcome
 *
 * @param system Counterfactual system
 * @param memory Memory to mutate
 * @param mutation_type Type of mutation to apply
 * @param original Original element being changed
 * @param mutated What it becomes in the alternative
 * @param result Output counterfactual (caller-allocated)
 * @return true on success, false on error
 *
 * Performance: ~10us
 *
 * Example:
 *   counterfactual_t cf;
 *   if (counterfactual_generate(system, memory, MUTATE_ACTION,
 *                               &original_sig, &alternative_sig, &cf)) {
 *       printf("Alternative outcome valence: %.2f\n", cf.outcome_valence);
 *   }
 */
NIMCP_EXPORT bool counterfactual_generate(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    mutation_type_t mutation_type,
    const prime_signature_t* original,
    const prime_signature_t* mutated,
    counterfactual_t* result
);

//=============================================================================
// Mutation Functions
//=============================================================================

/**
 * @brief Generate "what if I had done X instead" counterfactual
 *
 * WHAT: Action replacement counterfactual
 * WHY:  Most common type - "I should have done X"
 * HOW:  Replaces action signature with alternative
 *
 * @param system Counterfactual system
 * @param memory Memory containing the action
 * @param action_taken Signature of actual action
 * @param action_alternative Signature of alternative action
 * @param result Output counterfactual
 * @return true on success, false on error
 *
 * Performance: ~10us
 */
NIMCP_EXPORT bool counterfactual_mutate_action(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    const prime_signature_t* action_taken,
    const prime_signature_t* action_alternative,
    counterfactual_t* result
);

/**
 * @brief Generate "what if I had done nothing" counterfactual
 *
 * WHAT: Inaction counterfactual
 * WHY:  "I shouldn't have done that" reasoning
 * HOW:  Removes action entirely from scenario
 *
 * @param system Counterfactual system
 * @param memory Memory containing the action
 * @param action_taken Signature of action to remove
 * @param result Output counterfactual
 * @return true on success, false on error
 *
 * Performance: ~10us
 */
NIMCP_EXPORT bool counterfactual_mutate_inaction(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    const prime_signature_t* action_taken,
    counterfactual_t* result
);

/**
 * @brief Generate "what if I had acted sooner/later" counterfactual
 *
 * WHAT: Timing mutation counterfactual
 * WHY:  "If only I had done it earlier/later"
 * HOW:  Adjusts temporal context of action
 *
 * @param system Counterfactual system
 * @param memory Memory containing the timed action
 * @param action Signature of the action
 * @param time_delta Time shift in milliseconds (negative=earlier, positive=later)
 * @param result Output counterfactual
 * @return true on success, false on error
 *
 * Performance: ~15us (includes temporal propagation)
 */
NIMCP_EXPORT bool counterfactual_mutate_timing(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    const prime_signature_t* action,
    int64_t time_delta,
    counterfactual_t* result
);

/**
 * @brief Generate "what if event E hadn't happened" counterfactual
 *
 * WHAT: Event removal counterfactual
 * WHY:  Understanding impact of external events
 * HOW:  Removes event from causal chain
 *
 * @param system Counterfactual system
 * @param memory Memory containing the event
 * @param event_sig Signature of event to remove
 * @param result Output counterfactual
 * @return true on success, false on error
 *
 * Performance: ~15us
 */
NIMCP_EXPORT bool counterfactual_mutate_event(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    const prime_signature_t* event_sig,
    counterfactual_t* result
);

//=============================================================================
// Outcome Evaluation Functions
//=============================================================================

/**
 * @brief Evaluate predicted outcome for a counterfactual scenario
 *
 * WHAT: Propagates mutation through causal structure to predict outcome
 * WHY:  "What would have happened" inference
 * HOW:  Uses causal matrix and entanglement graph for propagation
 *
 * @param system Counterfactual system
 * @param cf Counterfactual to evaluate
 * @param outcome Output: predicted outcome signature
 * @param probability Output: probability of this outcome [0, 1]
 * @return true on success, false on error
 *
 * Performance: ~50us (depends on graph density)
 */
NIMCP_EXPORT bool counterfactual_evaluate_outcome(
    counterfactual_system_t system,
    const counterfactual_t* cf,
    prime_signature_t* outcome,
    float* probability
);

/**
 * @brief Compute affect change (regret/relief) for counterfactual
 *
 * WHAT: Calculate emotional impact of the alternative
 * WHY:  Regret/relief are key outputs of counterfactual thinking
 * HOW:  Compares actual vs. alternative outcome valences
 *
 * @param system Counterfactual system
 * @param cf Counterfactual (modified in place with affect values)
 * @return true on success, false on error
 *
 * ALGORITHM:
 *   affect_change = alternative_valence - actual_valence
 *   if affect_change > 0:  // Alternative was better
 *       regret = affect_change * controllability * mutability
 *       relief = 0
 *   else:  // Alternative was worse
 *       regret = 0
 *       relief = -affect_change * controllability * mutability
 *
 * Performance: ~1us
 */
NIMCP_EXPORT bool counterfactual_compute_affect(
    counterfactual_system_t system,
    counterfactual_t* cf
);

//=============================================================================
// Causal Reasoning Functions
//=============================================================================

/**
 * @brief Extract causal factors for a memory
 *
 * WHAT: Identify what caused this memory/event
 * WHY:  Causal structure is foundation for meaningful counterfactuals
 * HOW:  Analyzes incoming entanglement edges, temporal patterns
 *
 * @param system Counterfactual system
 * @param memory Memory to analyze
 * @param causes Output array for causal signatures
 * @param strengths Output array for causal strengths
 * @param max_causes Maximum causes to extract
 * @param num_causes Output: actual number extracted
 * @return true on success, false on error
 *
 * CRITERIA for causation:
 * 1. Temporal precedence (cause before effect)
 * 2. Entanglement strength above threshold
 * 3. Causal edge type in entanglement graph
 * 4. Covariation pattern in causal matrix
 *
 * Performance: ~100us for dense connections
 */
NIMCP_EXPORT bool counterfactual_extract_causes(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    prime_signature_t* causes,
    float* strengths,
    size_t max_causes,
    size_t* num_causes
);

/**
 * @brief Update causal model from observed cause-effect pair
 *
 * WHAT: Learn cause-effect relationship from experience
 * WHY:  Build causal knowledge for better counterfactual inference
 * HOW:  Updates causal matrix with observed co-occurrence
 *
 * @param system Counterfactual system
 * @param cause Signature of the cause
 * @param effect Signature of the effect
 * @param strength Observed strength of relationship [0, 1]
 * @return true on success, false on error
 *
 * ALGORITHM:
 *   causal_matrix[cause_idx][effect_idx] =
 *       learning_rate * strength + (1 - learning_rate) * old_value
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool counterfactual_update_causal_model(
    counterfactual_system_t system,
    const prime_signature_t* cause,
    const prime_signature_t* effect,
    float strength
);

/**
 * @brief Get causal strength between two signatures
 *
 * WHAT: Query learned causal relationship
 * WHY:  Used in outcome evaluation and explanation
 *
 * @param system Counterfactual system
 * @param cause Potential cause signature
 * @param effect Potential effect signature
 * @return Causal strength [0, 1], or -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT float counterfactual_get_causal_strength(
    counterfactual_system_t system,
    const prime_signature_t* cause,
    const prime_signature_t* effect
);

//=============================================================================
// Mutability Functions
//=============================================================================

/**
 * @brief Get most mentally mutable element in memory
 *
 * WHAT: Find what's easiest to imagine differently
 * WHY:  High mutability elements are natural focus of counterfactuals
 * HOW:  Applies mutability heuristics to memory components
 *
 * @param system Counterfactual system
 * @param memory Memory to analyze
 * @param element Output: most mutable element signature
 * @param mutability Output: mutability score [0, 1]
 * @param type Output: type of mutation most natural
 * @return true on success, false on error
 *
 * Mutability heuristics:
 * - Actions > inactions (action effect)
 * - Recent > distant (recency effect)
 * - Exceptional > routine (normality effect)
 * - Controllable > uncontrollable (controllability effect)
 *
 * Performance: ~10us
 */
NIMCP_EXPORT bool counterfactual_get_most_mutable(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    prime_signature_t* element,
    float* mutability,
    mutation_type_t* type
);

/**
 * @brief Compute mutability score for specific element
 *
 * WHAT: How easy is it to mentally change this element?
 * WHY:  Used to weight counterfactual salience
 *
 * @param system Counterfactual system
 * @param memory Memory context
 * @param element Element to evaluate
 * @param type Type of mutation being considered
 * @return Mutability score [0, 1]
 *
 * Performance: ~1us
 */
NIMCP_EXPORT float counterfactual_compute_mutability(
    counterfactual_system_t system,
    pr_memory_node_t* memory,
    const prime_signature_t* element,
    mutation_type_t type
);

//=============================================================================
// Comparison Functions
//=============================================================================

/**
 * @brief Compare upward vs downward counterfactual thinking
 *
 * WHAT: Analyze balance of "better" vs "worse" alternatives
 * WHY:  Ratio indicates psychological state (depression vs resilience)
 * HOW:  Counts and weights upward vs downward counterfactuals
 *
 * @param system Counterfactual system
 * @param analysis Analysis to evaluate
 * @param upward_strength Output: total strength of upward CFs
 * @param downward_strength Output: total strength of downward CFs
 * @param ratio Output: upward / (upward + downward)
 * @return true on success, false on error
 *
 * Interpretation:
 * - ratio > 0.7: Excessive upward focus (may indicate rumination)
 * - ratio < 0.3: Excessive downward focus (may indicate denial)
 * - ratio ~0.5: Balanced counterfactual thinking
 *
 * Performance: O(num_counterfactuals)
 */
NIMCP_EXPORT bool counterfactual_compare_updown(
    counterfactual_system_t system,
    const counterfactual_analysis_t* analysis,
    float* upward_strength,
    float* downward_strength,
    float* ratio
);

/**
 * @brief Find counterfactual most likely to change behavior
 *
 * WHAT: Which alternative would actually lead to different choices?
 * WHY:  Functional counterfactuals promote learning
 * HOW:  Combines affect, controllability, and actionability
 *
 * @param system Counterfactual system
 * @param analysis Analysis to search
 * @param result Output: most actionable counterfactual (or NULL if none)
 * @return true if found actionable CF, false if none or error
 *
 * Performance: O(num_counterfactuals)
 */
NIMCP_EXPORT bool counterfactual_find_most_actionable(
    counterfactual_system_t system,
    const counterfactual_analysis_t* analysis,
    counterfactual_t** result
);

//=============================================================================
// Analysis Memory Management
//=============================================================================

/**
 * @brief Initialize analysis structure
 *
 * WHAT: Prepare analysis for use
 * WHY:  Must be called before counterfactual_analyze
 *
 * @param analysis Analysis to initialize
 * @param max_causes Maximum causes to allocate for
 * @param max_counterfactuals Maximum counterfactuals to allocate for
 * @return true on success, false on allocation failure
 *
 * Performance: O(max_causes + max_counterfactuals)
 */
NIMCP_EXPORT bool counterfactual_analysis_init(
    counterfactual_analysis_t* analysis,
    size_t max_causes,
    size_t max_counterfactuals
);

/**
 * @brief Free analysis internal arrays
 *
 * WHAT: Deallocate arrays inside analysis
 * WHY:  Resource cleanup
 *
 * @param analysis Analysis to clean up (NULL safe)
 */
NIMCP_EXPORT void counterfactual_analysis_cleanup(counterfactual_analysis_t* analysis);

/**
 * @brief Deep copy an analysis
 *
 * WHAT: Create independent copy of analysis
 * WHY:  Cache or store analysis separately
 *
 * @param dest Destination analysis (will be initialized)
 * @param src Source analysis
 * @return true on success, false on error
 */
NIMCP_EXPORT bool counterfactual_analysis_copy(
    counterfactual_analysis_t* dest,
    const counterfactual_analysis_t* src
);

//=============================================================================
// Statistics and Utility Functions
//=============================================================================

/**
 * @brief Get counterfactual system statistics
 *
 * @param system System to query
 * @param stats Output statistics structure
 * @return true on success, false if system is NULL
 */
NIMCP_EXPORT bool pr_counterfactual_get_stats(
    counterfactual_system_t system,
    counterfactual_stats_t* stats
);

/**
 * @brief Reset counterfactual system statistics
 *
 * @param system System to reset
 */
NIMCP_EXPORT void pr_counterfactual_reset_stats(counterfactual_system_t system);

/**
 * @brief Get last error message
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* pr_counterfactual_get_last_error(void);

/**
 * @brief Get counterfactual type name as string
 *
 * @param type Counterfactual type
 * @return Static string name
 */
NIMCP_EXPORT const char* counterfactual_type_name(counterfactual_type_t type);

/**
 * @brief Get mutation type name as string
 *
 * @param type Mutation type
 * @return Static string name
 */
NIMCP_EXPORT const char* counterfactual_mutation_name(mutation_type_t type);

/**
 * @brief Print counterfactual to stdout for debugging
 *
 * @param cf Counterfactual to print
 */
NIMCP_EXPORT void counterfactual_print(const counterfactual_t* cf);

/**
 * @brief Print analysis summary to stdout
 *
 * @param analysis Analysis to print
 */
NIMCP_EXPORT void counterfactual_analysis_print(const counterfactual_analysis_t* analysis);

/**
 * @brief Generate human-readable explanation of counterfactual
 *
 * WHAT: Natural language description of the "what if"
 * WHY:  Explainable AI - understand counterfactual reasoning
 *
 * @param cf Counterfactual to explain
 * @param buf Output buffer
 * @param size Buffer size
 * @return Characters written (excluding null terminator)
 *
 * Example output:
 *   "If [action] had been [alternative] instead, the outcome would likely
 *    have been better (probability: 0.72). Regret intensity: 0.65."
 */
NIMCP_EXPORT size_t counterfactual_explain(
    const counterfactual_t* cf,
    char* buf,
    size_t size
);

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Check if counterfactual represents significant improvement
 *
 * @param cf Counterfactual to check
 * @return true if affect_change > CF_SIGNIFICANT_IMPROVEMENT
 */
static inline bool counterfactual_is_significant_improvement(const counterfactual_t* cf) {
    return cf && cf->affect_change > CF_SIGNIFICANT_IMPROVEMENT;
}

/**
 * @brief Check if counterfactual represents significant degradation
 *
 * @param cf Counterfactual to check
 * @return true if affect_change < CF_SIGNIFICANT_DEGRADATION
 */
static inline bool counterfactual_is_significant_degradation(const counterfactual_t* cf) {
    return cf && cf->affect_change < CF_SIGNIFICANT_DEGRADATION;
}

/**
 * @brief Check if counterfactual is upward (better alternative)
 *
 * @param cf Counterfactual to check
 * @return true if direction is COUNTER_UPWARD
 */
static inline bool counterfactual_is_upward(const counterfactual_t* cf) {
    return cf && cf->direction == COUNTER_UPWARD;
}

/**
 * @brief Check if counterfactual is downward (worse alternative)
 *
 * @param cf Counterfactual to check
 * @return true if direction is COUNTER_DOWNWARD
 */
static inline bool counterfactual_is_downward(const counterfactual_t* cf) {
    return cf && cf->direction == COUNTER_DOWNWARD;
}

/**
 * @brief Get dominant emotion from counterfactual
 *
 * @param cf Counterfactual to check
 * @return Positive for relief, negative for regret, 0 if neutral
 */
static inline float counterfactual_dominant_affect(const counterfactual_t* cf) {
    if (!cf) return 0.0f;
    if (cf->relief_intensity > cf->regret_intensity) {
        return cf->relief_intensity;
    }
    return -cf->regret_intensity;
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_COUNTERFACTUAL_H
