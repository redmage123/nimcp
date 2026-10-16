/**
 * @file nimcp_harm_prevention.h
 * @brief Asimov's First Law - Harm Prevention System
 * @version 1.0.0
 * @date 2025-12-16
 *
 * WHAT: Implements Asimov's First Law of Robotics - preventing harm to humans
 *       through action evaluation and inaction detection
 * WHY:  Highest-priority safety check ensuring no action causes harm directly
 *       or through inaction. Essential for safe autonomous systems.
 * HOW:  Evaluates all actions against harm classifier, blocks/warns/escalates
 *       based on harm score, detects harmful inaction scenarios
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HARM AVOIDANCE IN BIOLOGICAL SYSTEMS:
 * -------------------------------------
 * 1. Amygdala Threat Detection:
 *    - Rapid evaluation of stimuli for potential harm
 *    - Triggers defensive responses before conscious awareness
 *    - Prevents harmful actions through automatic inhibition
 *    - Reference: LeDoux (1996) "The Emotional Brain"
 *
 * 2. Prefrontal Cortex Harm Assessment:
 *    - Deliberate evaluation of action consequences
 *    - Models future harm through mental simulation
 *    - Inhibits prepotent responses that could cause harm
 *    - Reference: Damasio (1994) "Descartes' Error"
 *
 * 3. Moral Decision-Making Networks:
 *    - Ventromedial PFC: harm aversion and moral emotions
 *    - Temporoparietal junction: considering others' perspectives
 *    - Anterior cingulate: conflict monitoring for harmful outcomes
 *    - Reference: Greene et al. (2001) "Neural bases of moral judgment"
 *
 * 4. Inaction Detection (Sins of Omission):
 *    - Default mode network monitors for required actions
 *    - Guilt/regret signals when failing to prevent harm
 *    - Prospective memory for harm-preventing obligations
 *    - Reference: Spreng et al. (2009) "Default network and social cognition"
 *
 * ASIMOV'S FIRST LAW IMPLEMENTATION:
 * ----------------------------------
 * "A robot may not injure a human being or, through inaction,
 *  allow a human being to come to harm."
 *
 * 1. Direct Harm Prevention:
 *    - All actions evaluated BEFORE execution
 *    - Harm scores computed by specialized classifier
 *    - Blocking threshold prevents high-harm actions
 *    - Warning threshold alerts on moderate harm risk
 *
 * 2. Inaction Detection:
 *    - Context monitoring for situations requiring intervention
 *    - Detection of harm that will occur without action
 *    - Escalation when inaction would allow harm
 *    - Models "duty to rescue" moral obligations
 *
 * 3. Harm Score Calculation:
 *    - Physical harm: injury, death, health damage
 *    - Psychological harm: trauma, distress, manipulation
 *    - Social harm: reputation damage, relationships
 *    - Economic harm: property damage, financial loss
 *    - Calibrated from 0.0 (no harm) to 1.0 (severe harm)
 *
 * 4. Decision Hierarchy:
 *    - BLOCK (harm > 0.1): Action prevented, logged, escalated
 *    - WARN (harm > 0.05): Action allowed with warning
 *    - ESCALATE: Human review required for edge cases
 *    - ALLOW (harm ≤ 0.05): Action proceeds normally
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    HARM PREVENTION SYSTEM                                  ║
 * ║                     (Asimov's First Law)                                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              DIRECT HARM PREVENTION PATHWAY                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   ACTION REQUEST                                                    │  ║
 * ║   │        │                                                            │  ║
 * ║   │        ▼                                                            │  ║
 * ║   │   ┌─────────────────┐                                              │  ║
 * ║   │   │ Harm Classifier │ ──→ Compute harm score [0.0-1.0]            │  ║
 * ║   │   └─────────────────┘                                              │  ║
 * ║   │        │                                                            │  ║
 * ║   │        ▼                                                            │  ║
 * ║   │   ┌─────────────────────────────────┐                              │  ║
 * ║   │   │    DECISION LOGIC               │                              │  ║
 * ║   │   │  ─────────────────────────────  │                              │  ║
 * ║   │   │  harm > 0.1  → BLOCK            │                              │  ║
 * ║   │   │  harm > 0.05 → WARN             │                              │  ║
 * ║   │   │  harm ≤ 0.05 → ALLOW            │                              │  ║
 * ║   │   │  edge case   → ESCALATE         │                              │  ║
 * ║   │   └─────────────────────────────────┘                              │  ║
 * ║   │        │                                                            │  ║
 * ║   │        ▼                                                            │  ║
 * ║   │   [ACTION EXECUTED] or [BLOCKED]                                   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              INACTION HARM DETECTION                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   CONTEXT MONITORING                                                │  ║
 * ║   │        │                                                            │  ║
 * ║   │        ▼                                                            │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │ Detect Harm Without      │                                     │  ║
 * ║   │   │ Intervention             │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   │        │                                                            │  ║
 * ║   │        ▼                                                            │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │ Identify Required Action │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   │        │                                                            │  ║
 * ║   │        ▼                                                            │  ║
 * ║   │   ESCALATE TO HUMAN                                                │  ║
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
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HARM_PREVENTION_H
#define NIMCP_HARM_PREVENTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Default harm thresholds */
#define HARM_PREVENTION_DEFAULT_BLOCK_THRESHOLD   0.1f  /**< Block if harm > 0.1 */
#define HARM_PREVENTION_DEFAULT_WARN_THRESHOLD    0.05f /**< Warn if harm > 0.05 */

/* Harm score ranges */
#define HARM_SCORE_NONE       0.0f   /**< No harm detected */
#define HARM_SCORE_MINIMAL    0.01f  /**< Minimal/negligible harm */
#define HARM_SCORE_LOW        0.05f  /**< Low harm (warning threshold) */
#define HARM_SCORE_MODERATE   0.1f   /**< Moderate harm (block threshold) */
#define HARM_SCORE_HIGH       0.5f   /**< High harm */
#define HARM_SCORE_SEVERE     1.0f   /**< Severe/catastrophic harm */

/* String length limits */
#define HARM_REASON_MAX_LEN   512    /**< Max length of harm reason string */

/* Statistics tracking */
#define HARM_STATS_HISTORY_SIZE 100  /**< Number of recent decisions to track */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Harm prevention decision types
 *
 * WHAT: Decision outcomes from harm evaluation
 * WHY:  Different actions require different handling
 * HOW:  Ordered by severity (ALLOW < WARN < BLOCK < ESCALATE)
 */
typedef enum {
    HARM_DECISION_ALLOW = 0,     /**< Action safe to proceed */
    HARM_DECISION_WARN,          /**< Proceed with warning */
    HARM_DECISION_BLOCK,         /**< Action blocked (harmful) */
    HARM_DECISION_ESCALATE       /**< Escalate to human review */
} harm_decision_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Harm prevention evaluation result
 *
 * WHAT: Complete result of harm evaluation for an action
 * WHY:  Captures decision, score, reasoning, and metadata
 * HOW:  Returned by evaluation functions for logging/auditing
 */
typedef struct {
    harm_decision_t decision;        /**< Final decision */
    float harm_score;                /**< Computed harm score [0.0-1.0] */
    char reason[HARM_REASON_MAX_LEN]; /**< Human-readable reason */
    uint64_t timestamp_ms;           /**< When evaluation occurred */
    bool first_law_violated;         /**< Would violate First Law */
    bool requires_human_review;      /**< Needs human escalation */
} harm_prevention_result_t;

/**
 * @brief Harm classifier function type
 *
 * WHAT: Function signature for external harm classification
 * WHY:  Allows pluggable harm detection algorithms
 * HOW:  Takes action description/data, returns harm score [0.0-1.0]
 *
 * @param action_desc Human-readable action description
 * @param action_data Serialized action data (can be NULL)
 * @param action_data_len Length of action data
 * @param user_data Opaque user data passed to classifier
 * @return Harm score [0.0-1.0], or negative on error
 */
typedef float (*harm_classifier_fn)(
    const char* action_desc,
    const void* action_data,
    size_t action_data_len,
    void* user_data
);

/**
 * @brief Escalation callback function type
 *
 * WHAT: Callback invoked when action requires human review
 * WHY:  Allows system to notify operators of critical decisions
 * HOW:  Called with evaluation result and user data
 *
 * @param result Harm prevention result that triggered escalation
 * @param user_data Opaque user data passed to callback
 */
typedef void (*harm_escalation_callback_fn)(
    const harm_prevention_result_t* result,
    void* user_data
);

/**
 * @brief Harm prevention system configuration
 *
 * WHAT: Configuration parameters for harm prevention system
 * WHY:  Allows tuning thresholds and behavior
 * HOW:  Passed to harm_prevention_create()
 */
typedef struct {
    float block_threshold;           /**< Harm score threshold for blocking */
    float warn_threshold;            /**< Harm score threshold for warning */
    bool enable_human_escalation;    /**< Enable escalation to humans */
    bool enable_inaction_detection;  /**< Detect harmful inaction */
    harm_escalation_callback_fn escalation_callback; /**< Escalation callback */
    void* callback_user_data;        /**< User data for callback */
} harm_prevention_config_t;

/**
 * @brief Harm prevention system statistics
 *
 * WHAT: Runtime statistics about harm prevention decisions
 * WHY:  Monitor system behavior and audit trail
 * HOW:  Updated on each evaluation
 */
typedef struct {
    uint64_t total_evaluations;      /**< Total actions evaluated */
    uint64_t allowed_count;          /**< Actions allowed */
    uint64_t warned_count;           /**< Actions warned */
    uint64_t blocked_count;          /**< Actions blocked */
    uint64_t escalated_count;        /**< Actions escalated */
    uint64_t inaction_detections;    /**< Harmful inaction detected */
    float avg_harm_score;            /**< Average harm score */
    float max_harm_score;            /**< Maximum harm score seen */
    uint64_t first_law_violations;   /**< Count of First Law violations */
} harm_prevention_stats_t;

/**
 * @brief Harm prevention system (opaque)
 *
 * WHAT: Main harm prevention management structure
 * WHY:  Encapsulates all state for thread-safe harm checking
 * HOW:  Created by harm_prevention_create()
 */
typedef struct harm_prevention_system harm_prevention_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with safe defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, negative on error
 */
int harm_prevention_default_config(harm_prevention_config_t* config);

/**
 * @brief Create harm prevention system
 *
 * WHAT: Initialize harm prevention system with classifier
 * WHY:  Enable First Law enforcement
 * HOW:  Allocate structure, configure thresholds, link classifier
 *
 * @param config Configuration (NULL for defaults)
 * @param classifier Harm classifier function (required)
 * @param classifier_user_data User data passed to classifier
 * @return New system or NULL on failure
 */
harm_prevention_system_t* harm_prevention_create(
    const harm_prevention_config_t* config,
    harm_classifier_fn classifier,
    void* classifier_user_data
);

/**
 * @brief Destroy harm prevention system
 *
 * WHAT: Clean up harm prevention resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure and internal buffers
 *
 * @param system System to destroy
 */
void harm_prevention_destroy(harm_prevention_system_t* system);

/* ============================================================================
 * Action Evaluation API
 * ============================================================================ */

/**
 * @brief Evaluate action for harm
 *
 * WHAT: Assess whether action would cause harm
 * WHY:  Prevent harmful actions (First Law enforcement)
 * HOW:  Invoke classifier, compare to thresholds, decide
 *
 * @param system Harm prevention system
 * @param action_desc Human-readable action description
 * @param action_data Serialized action data (can be NULL)
 * @param action_data_len Length of action data
 * @param result Output evaluation result
 * @return 0 on success, negative on error
 */
int harm_prevention_evaluate_action(
    harm_prevention_system_t* system,
    const char* action_desc,
    const void* action_data,
    size_t action_data_len,
    harm_prevention_result_t* result
);

/**
 * @brief Evaluate inaction for harm
 *
 * WHAT: Detect harm that would occur without intervention
 * WHY:  Prevent harm through inaction (First Law second clause)
 * HOW:  Assess context, check if action required to prevent harm
 *
 * @param system Harm prevention system
 * @param context_desc Description of current context/situation
 * @param required_action Description of action that should be taken
 * @param result Output evaluation result
 * @return 0 on success, negative on error
 */
int harm_prevention_evaluate_inaction(
    harm_prevention_system_t* system,
    const char* context_desc,
    const char* required_action,
    harm_prevention_result_t* result
);

/**
 * @brief Block action and record reason
 *
 * WHAT: Explicitly block an action with reason
 * WHY:  Manual blocking for policy-based prevention
 * HOW:  Add to blocked actions list, log reason
 *
 * @param system Harm prevention system
 * @param action_id Unique action identifier
 * @param reason Human-readable reason for blocking
 * @return 0 on success, negative on error
 */
int harm_prevention_block_action(
    harm_prevention_system_t* system,
    uint32_t action_id,
    const char* reason
);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Register escalation callback
 *
 * WHAT: Set callback for human escalation events
 * WHY:  Enable real-time notification of critical decisions
 * HOW:  Store callback and user data in system
 *
 * @param system Harm prevention system
 * @param callback Escalation callback function
 * @param user_data User data passed to callback
 * @return 0 on success, negative on error
 */
int harm_prevention_register_escalation_callback(
    harm_prevention_system_t* system,
    harm_escalation_callback_fn callback,
    void* user_data
);

/**
 * @brief Update harm thresholds
 *
 * WHAT: Change block/warn thresholds dynamically
 * WHY:  Allow runtime tuning of harm sensitivity
 * HOW:  Update thresholds with validation
 *
 * @param system Harm prevention system
 * @param block_threshold New block threshold [0.0-1.0]
 * @param warn_threshold New warn threshold [0.0-1.0]
 * @return 0 on success, negative on error
 */
int harm_prevention_update_thresholds(
    harm_prevention_system_t* system,
    float block_threshold,
    float warn_threshold
);

/**
 * @brief Enable or disable inaction detection
 *
 * WHAT: Toggle inaction harm detection
 * WHY:  Control whether system monitors for harmful inaction
 * HOW:  Set enable flag in system
 *
 * @param system Harm prevention system
 * @param enable true to enable, false to disable
 * @return 0 on success, negative on error
 */
int harm_prevention_enable_inaction_detection(
    harm_prevention_system_t* system,
    bool enable
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get blocked action count
 *
 * WHAT: Retrieve number of actions blocked
 * WHY:  Monitor harm prevention effectiveness
 * HOW:  Return blocked counter
 *
 * @param system Harm prevention system
 * @return Blocked action count
 */
uint64_t harm_prevention_get_blocked_count(
    const harm_prevention_system_t* system
);

/**
 * @brief Get harm prevention statistics
 *
 * WHAT: Retrieve comprehensive statistics
 * WHY:  Audit trail and system monitoring
 * HOW:  Copy current stats to output
 *
 * @param system Harm prevention system
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
int harm_prevention_get_stats(
    const harm_prevention_system_t* system,
    harm_prevention_stats_t* stats
);

/**
 * @brief Check if action would be blocked
 *
 * WHAT: Quick check if action would be blocked (no side effects)
 * WHY:  Pre-check before expensive action preparation
 * HOW:  Evaluate action but don't update statistics
 *
 * @param system Harm prevention system
 * @param action_desc Action description
 * @param action_data Action data (can be NULL)
 * @param action_data_len Data length
 * @return true if would be blocked, false otherwise
 */
bool harm_prevention_would_block(
    const harm_prevention_system_t* system,
    const char* action_desc,
    const void* action_data,
    size_t action_data_len
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register system as bio-async module
 * WHY:  Enable inter-module messaging for harm signals
 * HOW:  Register with bio_router using BIO_MODULE_HARM_PREVENTION
 *
 * @param system Harm prevention system
 * @return 0 on success, negative on error
 */
int harm_prevention_connect_bio_async(
    harm_prevention_system_t* system
);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param system Harm prevention system
 * @return 0 on success, negative on error
 */
int harm_prevention_disconnect_bio_async(
    harm_prevention_system_t* system
);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Verify messaging capability before sending
 * HOW:  Check bio_async_enabled flag
 *
 * @param system Harm prevention system
 * @return true if connected, false otherwise
 */
bool harm_prevention_is_bio_async_connected(
    const harm_prevention_system_t* system
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HARM_PREVENTION_H */
