/**
 * @file nimcp_reciprocity_eval.h
 * @brief Golden Rule Reciprocity Evaluation Module
 * @version 1.0.0
 * @date 2025-12-16
 *
 * WHAT: Evaluates actions using the Golden Rule principle - "Treat others as you would want to be treated"
 * WHY:  Universal ethical principle that tests action acceptability through perspective reversal;
 *       fundamental to moral reasoning across cultures and philosophies
 * HOW:  Reverse perspectives, compute symmetry, check if action would be acceptable if roles reversed
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * THEORY OF MIND AND PERSPECTIVE TAKING:
 * --------------------------------------
 * 1. Temporo-Parietal Junction (TPJ):
 *    - Enables perspective taking and role reversal
 *    - Allows simulation of "what if I were the target?"
 *    - Critical for empathy and moral judgment
 *    - Reference: Decety & Sommerville (2003) "Shared representations between self and other"
 *
 * 2. Ventromedial Prefrontal Cortex (vmPFC):
 *    - Evaluates moral acceptability
 *    - Integrates emotional and rational considerations
 *    - Represents value of actions from multiple perspectives
 *    - Reference: Koenigs et al. (2007) "Damage to ventromedial prefrontal cortex"
 *
 * 3. Mirror Neuron System:
 *    - Simulates experiences of others
 *    - Enables "feeling what they would feel"
 *    - Provides embodied understanding of action impact
 *    - Reference: Rizzolatti & Craighero (2004) "The mirror-neuron system"
 *
 * RECIPROCITY DETECTION:
 * ----------------------
 * 1. Symmetry Assessment:
 *    - Compare action A→B with hypothetical B→A
 *    - Detect asymmetries that violate fairness
 *    - Measure alignment between "give" and "willing to receive"
 *
 * 2. Role Reversal Simulation:
 *    - Simulate self as target of action
 *    - Query emotional/rational response to reversed action
 *    - Check if reversed action would be acceptable
 *
 * 3. Fairness Computation:
 *    - Integrate symmetry and acceptability
 *    - Weight by action impact and context
 *    - Generate pass/fail/warn verdict
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                     RECIPROCITY EVALUATOR                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  RECIPROCITY EVALUATION PIPELINE                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   Step 1: ACTION DESCRIPTION                                       │  ║
 * ║   │   ┌──────────────────────────────────────┐                         │  ║
 * ║   │   │ Action: Agent A → Agent B            │                         │  ║
 * ║   │   │ Description: "Share B's location"    │                         │  ║
 * ║   │   └──────────────────────────────────────┘                         │  ║
 * ║   │                      ↓                                              │  ║
 * ║   │   Step 2: PERSPECTIVE REVERSAL (TPJ)                               │  ║
 * ║   │   ┌──────────────────────────────────────┐                         │  ║
 * ║   │   │ Reversed: Agent B → Agent A          │                         │  ║
 * ║   │   │ "Would I accept my location shared?" │                         │  ║
 * ║   │   └──────────────────────────────────────┘                         │  ║
 * ║   │                      ↓                                              │  ║
 * ║   │   Step 3: SYMMETRY COMPUTATION                                     │  ║
 * ║   │   ┌──────────────────────────────────────┐                         │  ║
 * ║   │   │ Compare: A→B vs B→A                  │                         │  ║
 * ║   │   │ Symmetry Score: 0.0-1.0              │                         │  ║
 * ║   │   │ (1.0 = perfectly symmetric)          │                         │  ║
 * ║   │   └──────────────────────────────────────┘                         │  ║
 * ║   │                      ↓                                              │  ║
 * ║   │   Step 4: ACCEPTABILITY CHECK (vmPFC)                              │  ║
 * ║   │   ┌──────────────────────────────────────┐                         │  ║
 * ║   │   │ Query: "Would I accept this if      │                         │  ║
 * ║   │   │        done to me?"                  │                         │  ║
 * ║   │   │ Response: YES / NO / UNCERTAIN       │                         │  ║
 * ║   │   └──────────────────────────────────────┘                         │  ║
 * ║   │                      ↓                                              │  ║
 * ║   │   Step 5: VERDICT                                                  │  ║
 * ║   │   ┌──────────────────────────────────────┐                         │  ║
 * ║   │   │ Symmetry ≥ threshold AND             │                         │  ║
 * ║   │   │ Would Accept = YES                   │                         │  ║
 * ║   │   │   → PASS (Golden Rule satisfied)     │                         │  ║
 * ║   │   │                                       │                         │  ║
 * ║   │   │ Otherwise → FAIL/WARN                │                         │  ║
 * ║   │   └──────────────────────────────────────┘                         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   EXAMPLE EVALUATIONS:                                                    ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │ 1. "Share B's location"                                            │  ║
 * ║   │    Reversed: "Share my location"                                   │  ║
 * ║   │    Symmetry: 1.0 (identical actions)                               │  ║
 * ║   │    Accept?: NO (privacy violation)                                 │  ║
 * ║   │    Verdict: FAIL                                                   │  ║
 * ║   │                                                                     │  ║
 * ║   │ 2. "Provide helpful advice to B"                                   │  ║
 * ║   │    Reversed: "Receive helpful advice"                              │  ║
 * ║   │    Symmetry: 0.9 (slightly different contexts)                     │  ║
 * ║   │    Accept?: YES (beneficial)                                       │  ║
 * ║   │    Verdict: PASS                                                   │  ║
 * ║   │                                                                     │  ║
 * ║   │ 3. "Access B's private files for security audit"                   │  ║
 * ║   │    Reversed: "Allow access to my private files"                    │  ║
 * ║   │    Symmetry: 0.4 (power asymmetry)                                 │  ║
 * ║   │    Accept?: UNCERTAIN (depends on trust/context)                   │  ║
 * ║   │    Verdict: WARN                                                   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - NIMCP_LOGGING_* for logging
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RECIPROCITY_EVAL_H
#define NIMCP_RECIPROCITY_EVAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default symmetry threshold for passing */
#define RECIPROCITY_DEFAULT_SYMMETRY_THRESHOLD  0.7f

/** Maximum description length */
#define RECIPROCITY_MAX_DESCRIPTION_LEN         256

/** Maximum entity name length */
#define RECIPROCITY_MAX_ENTITY_LEN              64

/** Maximum explanation length */
#define RECIPROCITY_MAX_EXPLANATION_LEN         256

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Reciprocity evaluation result
 */
typedef enum {
    RECIPROCITY_PASS = 0,       /**< Action passes Golden Rule test */
    RECIPROCITY_FAIL,           /**< Action fails Golden Rule test */
    RECIPROCITY_WARN,           /**< Uncertain - requires human judgment */
    RECIPROCITY_UNKNOWN         /**< Unable to evaluate */
} reciprocity_result_t;

/**
 * @brief Action perspective for role reversal
 *
 * Describes an action and its role-reversed equivalent
 */
typedef struct {
    char action_description[RECIPROCITY_MAX_DESCRIPTION_LEN];  /**< Original action */
    char target_entity[RECIPROCITY_MAX_ENTITY_LEN];            /**< Who is affected */
    char equivalent_action[RECIPROCITY_MAX_DESCRIPTION_LEN];   /**< Same action if roles reversed */
} action_perspective_t;

/**
 * @brief Reciprocity evaluation result
 *
 * Complete evaluation with verdict, scores, and explanation
 */
typedef struct {
    reciprocity_result_t result;                               /**< Overall verdict */
    float symmetry_score;                                      /**< How symmetric (0.0-1.0) */
    bool would_accept_reversed;                                /**< Would we accept reversed action? */
    char explanation[RECIPROCITY_MAX_EXPLANATION_LEN];         /**< Human-readable explanation */
} reciprocity_evaluation_t;

/**
 * @brief Reciprocity evaluator configuration
 */
typedef struct {
    float symmetry_threshold;       /**< Minimum symmetry to pass (default 0.7) */
    bool strict_mode;               /**< Fail on any asymmetry */
    bool enable_perspective_taking; /**< Consider others' viewpoints */
} reciprocity_config_t;

/**
 * @brief Reciprocity evaluator statistics
 */
typedef struct {
    uint64_t total_evaluations;     /**< Total evaluations performed */
    uint64_t passes;                /**< Actions that passed */
    uint64_t failures;              /**< Actions that failed */
    uint64_t warnings;              /**< Uncertain actions */
    float average_symmetry;         /**< Average symmetry score */
} reciprocity_stats_t;

/**
 * @brief Reciprocity evaluator (opaque handle)
 */
typedef struct reciprocity_evaluator_struct* reciprocity_evaluator_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with evidence-based defaults
 * HOW:  Return struct with predefined values
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int reciprocity_eval_default_config(reciprocity_config_t* config);

/**
 * @brief Create reciprocity evaluator
 *
 * WHAT: Initialize Golden Rule evaluator
 * WHY:  Enable perspective-taking and symmetry-based ethical evaluation
 * HOW:  Allocate structure, initialize statistics
 *
 * @param config Configuration (NULL for defaults)
 * @return New evaluator or NULL on failure
 */
reciprocity_evaluator_t reciprocity_eval_create(const reciprocity_config_t* config);

/**
 * @brief Destroy reciprocity evaluator
 *
 * WHAT: Clean up evaluator resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure and internal resources
 *
 * @param evaluator Evaluator to destroy
 */
void reciprocity_eval_destroy(reciprocity_evaluator_t evaluator);

/* ============================================================================
 * Evaluation API
 * ============================================================================ */

/**
 * @brief Evaluate action using Golden Rule
 *
 * WHAT: Test if action satisfies "treat others as you'd want to be treated"
 * WHY:  Fundamental moral principle for action acceptability
 * HOW:  Reverse perspective, compute symmetry, check acceptability
 *
 * @param evaluator Reciprocity evaluator
 * @param action Action description
 * @param target Who is affected by action
 * @param evaluation Output: evaluation result
 * @return 0 on success, -1 on error
 */
int reciprocity_eval_check(
    reciprocity_evaluator_t evaluator,
    const char* action,
    const char* target,
    reciprocity_evaluation_t* evaluation
);

/**
 * @brief Reverse perspective for action
 *
 * WHAT: Generate role-reversed version of action
 * WHY:  Core of Golden Rule - "what if it were done to me?"
 * HOW:  Swap roles, preserve action semantics
 *
 * @param evaluator Reciprocity evaluator
 * @param action Original action description
 * @param reversed_action Output: role-reversed action (buffer size ≥ RECIPROCITY_MAX_DESCRIPTION_LEN)
 * @return 0 on success, -1 on error
 */
int reciprocity_eval_reverse_perspective(
    reciprocity_evaluator_t evaluator,
    const char* action,
    char* reversed_action
);

/**
 * @brief Check if would accept action if received
 *
 * WHAT: Query whether reversed action would be acceptable
 * WHY:  Core acceptability test for Golden Rule
 * HOW:  Evaluate action from target's perspective
 *
 * @param evaluator Reciprocity evaluator
 * @param action_if_received Action as it would be received
 * @return true if acceptable, false otherwise
 */
bool reciprocity_eval_would_accept(
    reciprocity_evaluator_t evaluator,
    const char* action_if_received
);

/**
 * @brief Compute symmetry score between action and reversed action
 *
 * WHAT: Calculate how symmetric action is under role reversal
 * WHY:  Asymmetries indicate unfairness or exploitation
 * HOW:  Compare action features before/after reversal
 *
 * @param evaluator Reciprocity evaluator
 * @param action Original action
 * @param target Target entity
 * @return Symmetry score [0.0-1.0] (1.0 = perfectly symmetric)
 */
float reciprocity_eval_get_symmetry_score(
    reciprocity_evaluator_t evaluator,
    const char* action,
    const char* target
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get evaluator statistics
 *
 * WHAT: Retrieve evaluation statistics
 * WHY:  Monitor evaluator usage and performance
 * HOW:  Copy internal statistics to output
 *
 * @param evaluator Reciprocity evaluator
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int reciprocity_eval_get_stats(
    reciprocity_evaluator_t evaluator,
    reciprocity_stats_t* stats
);

/**
 * @brief Reset evaluator statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Start fresh measurement period
 * HOW:  Zero all statistic fields
 *
 * @param evaluator Reciprocity evaluator
 * @return 0 on success, -1 on error
 */
int reciprocity_eval_reset_stats(reciprocity_evaluator_t evaluator);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect evaluator to bio-async router
 *
 * WHAT: Register evaluator as bio-async module
 * WHY:  Enable inter-module messaging for distributed ethical reasoning
 * HOW:  Register with bio_router using BIO_MODULE_RECIPROCITY_EVAL
 *
 * @param evaluator Reciprocity evaluator
 * @return 0 on success, -1 on error
 */
int reciprocity_eval_connect_bio_async(reciprocity_evaluator_t evaluator);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister evaluator from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param evaluator Reciprocity evaluator
 * @return 0 on success, -1 on error
 */
int reciprocity_eval_disconnect_bio_async(reciprocity_evaluator_t evaluator);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Verify messaging capability
 * HOW:  Check bio_async_enabled flag
 *
 * @param evaluator Reciprocity evaluator
 * @return true if connected, false otherwise
 */
bool reciprocity_eval_is_bio_async_connected(reciprocity_evaluator_t evaluator);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RECIPROCITY_EVAL_H */
