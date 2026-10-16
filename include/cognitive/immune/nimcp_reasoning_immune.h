/**
 * @file nimcp_reasoning_immune.h
 * @brief Reasoning-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and reasoning/logic processing
 * WHY:  Pro-inflammatory cytokines impair executive function and logical reasoning (biological evidence).
 *       Logical contradictions/failures may indicate corrupted cognitive state requiring immune response.
 * HOW:  Cytokines modulate reasoning speed/accuracy, chronic inflammation degrades logical processing.
 *       Reasoning failures (contradictions, proof failures, unification errors) trigger immune investigation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → REASONING PATHWAYS:
 * ---------------------------
 * 1. Pro-inflammatory Cytokines Impair Executive Function:
 *    - IL-1β, IL-6, TNF-α cross blood-brain barrier
 *    - Impair prefrontal cortex function (reasoning, planning, logic)
 *    - Reduce processing speed (psychomotor slowing)
 *    - Increase errors in logical tasks
 *    - Impair working memory capacity (critical for multi-step reasoning)
 *    - Reference: Dantzer et al. (2008), "Cytokine-induced sickness behavior"
 *    - Reference: Harrison et al. (2009), "Inflammation causes mood changes through effects on neural function"
 *
 * 2. Chronic Inflammation and Cognitive Deficits:
 *    - Sustained elevation → reasoning deficits
 *    - Impaired abstract thinking
 *    - Reduced problem-solving efficiency
 *    - Difficulty with complex logical chains
 *    - Reference: McAfoose & Baune (2009), "Evidence for a cytokine model of cognitive function"
 *
 * 3. Cytokine Storm → Severe Cognitive Impairment:
 *    - Delirium-like state
 *    - Inability to maintain logical coherence
 *    - Confusion, disorganized thinking
 *    - Reference: Girard et al. (2010), "Delirium as a predictor of long-term cognitive impairment"
 *
 * 4. Anti-inflammatory Cytokines (IL-10) → Cognitive Recovery:
 *    - Promotes resolution of cognitive impairment
 *    - Restores reasoning capacity
 *    - Enables return to baseline logical performance
 *    - Reference: Maes et al. (2012), "Anti-inflammatory cytokines and cognitive function"
 *
 * REASONING → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Logical Contradictions as Corruption Signal:
 *    - Contradictions indicate potentially corrupted knowledge base
 *    - May result from Byzantine node, data corruption, adversarial input
 *    - Triggers immune investigation (B cell activation)
 *    - Reference: Fault-tolerant computing literature (immune-inspired algorithms)
 *
 * 2. Repeated Proof Failures:
 *    - Multiple failed inferences suggest systemic issue
 *    - Could indicate damaged reasoning module or corrupted rules
 *    - Escalates inflammation for diagnostic attention
 *    - Biological analog: Pattern of failures triggers stress response
 *
 * 3. Unification Failures:
 *    - Pattern matching errors may indicate corrupted data structures
 *    - Triggers antigen presentation for immune analysis
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    REASONING-IMMUNE BRIDGE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → REASONING PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │   CYTOKINES      │                                             │  ║
 * ║   │   │ ──────────────   │                                             │  ║
 * ║   │   │ IL-1β → -30% spd │  ───────┐                                   │  ║
 * ║   │   │ IL-6  → -20% spd │         │                                   │  ║
 * ║   │   │ TNF-α → -40% spd │         ├──→ Reasoning Impairment           │  ║
 * ║   │   │                  │         │    (Speed ↓, Accuracy ↓)          │  ║
 * ║   │   └──────────────────┘         │                                   │  ║
 * ║   │                                ▼                                   │  ║
 * ║   │   ┌─────────────────────────────────────┐                         │  ║
 * ║   │   │     REASONING SYSTEM                │                         │  ║
 * ║   │   │  - Forward/Backward Chaining        │                         │  ║
 * ║   │   │  - Unification Engine               │                         │  ║
 * ║   │   │  - Knowledge Base                   │                         │  ║
 * ║   │   │  - Symbolic Logic                   │                         │  ║
 * ║   │   │                                     │                         │  ║
 * ║   │   │  Modulation Effects:                │                         │  ║
 * ║   │   │  - Max iterations reduced           │                         │  ║
 * ║   │   │  - Inference timeout shortened      │                         │  ║
 * ║   │   │  - Confidence thresholds raised     │                         │  ║
 * ║   │   │  - Working memory capacity reduced  │                         │  ║
 * ║   │   └─────────────────────────────────────┘                         │  ║
 * ║   │                                ▲                                   │  ║
 * ║   │   ┌──────────────────┐         │                                   │  ║
 * ║   │   │   INFLAMMATION   │         │                                   │  ║
 * ║   │   │ ──────────────   │         │                                   │  ║
 * ║   │   │ LOCAL    → -10%  │         │                                   │  ║
 * ║   │   │ REGIONAL → -25%  │  ───────┘                                   │  ║
 * ║   │   │ SYSTEMIC → -50%  │     Cumulative Impairment                   │  ║
 * ║   │   │ STORM    → -80%  │                                             │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │   IL-10          │ ──→ Cognitive Recovery                      │  ║
 * ║   │   │ Anti-inflammatory│     (Restore baseline)                      │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  REASONING → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌────────────────────┐                                           │  ║
 * ║   │   │  CONTRADICTION     │ ──→ Immune Investigation                  │  ║
 * ║   │   │   DETECTED         │     (Antigen Presentation)                │  ║
 * ║   │   └────────────────────┘                                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌────────────────────┐                                           │  ║
 * ║   │   │  PROOF FAILURES    │ ──→ Inflammation Trigger                  │  ║
 * ║   │   │   (Repeated)       │     (Diagnostic Attention)                │  ║
 * ║   │   └────────────────────┘                                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌────────────────────┐                                           │  ║
 * ║   │   │  UNIFICATION       │ ──→ Corruption Signal                     │  ║
 * ║   │   │   ERRORS           │     (B Cell Activation)                   │  ║
 * ║   │   └────────────────────┘                                           │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * CYTOKINE IMPACT MAPPING:
 * -----------------------
 * | Cytokine | Speed Impact | Accuracy Impact | WM Capacity Impact |
 * |----------|--------------|-----------------|-------------------|
 * | IL-1β    | -30%         | -15%            | -1 item           |
 * | IL-6     | -20%         | -10%            | -1 item           |
 * | TNF-α    | -40%         | -20%            | -2 items          |
 * | IFN-γ    | -15%         | -5%             | 0 items           |
 * | IL-10    | +10%         | +5%             | +1 item (recovery)|
 *
 * INFLAMMATION LEVEL IMPACT:
 * -------------------------
 * | Level    | Speed Penalty | Accuracy Penalty | Max Iterations | Timeout  |
 * |----------|---------------|------------------|----------------|----------|
 * | NONE     | 0%            | 0%               | 100%           | 100%     |
 * | LOCAL    | -10%          | -5%              | -10%           | -10%     |
 * | REGIONAL | -25%          | -15%             | -30%           | -25%     |
 * | SYSTEMIC | -50%          | -30%             | -50%           | -50%     |
 * | STORM    | -80%          | -60%             | -80%           | -80%     |
 *
 * REASONING FAILURE THRESHOLDS:
 * ----------------------------
 * | Event                    | Immune Response                                |
 * |--------------------------|------------------------------------------------|
 * | Contradiction detected   | Antigen presentation (MEDIUM severity)         |
 * | 3+ proof failures (10s)  | Inflammation trigger (LOCAL)                   |
 * | 10+ proof failures (10s) | Inflammation escalation (REGIONAL)             |
 * | 5+ unification errors    | B cell activation (pattern investigation)      |
 * | Persistent errors (>60s) | Cytokine release (IL-1β, IL-6)                 |
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

#ifndef NIMCP_REASONING_IMMUNE_H
#define NIMCP_REASONING_IMMUNE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/reasoning/nimcp_reasoning_integration.h"
#include "cognitive/reasoning/nimcp_forward_chaining.h"
#include "cognitive/reasoning/nimcp_backward_chaining.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine reasoning impact factors (speed modulation) */
#define CYTOKINE_IL1_REASONING_SPEED_IMPACT      -0.30f   /**< IL-1β → -30% speed */
#define CYTOKINE_IL6_REASONING_SPEED_IMPACT      -0.20f   /**< IL-6 → -20% speed */
#define CYTOKINE_TNF_REASONING_SPEED_IMPACT      -0.40f   /**< TNF-α → -40% speed */
#define CYTOKINE_IFN_GAMMA_REASONING_SPEED_IMPACT -0.15f  /**< IFN-γ → -15% speed */
#define CYTOKINE_IL10_REASONING_SPEED_IMPACT      0.10f   /**< IL-10 → +10% recovery */

/* Cytokine accuracy impact factors */
#define CYTOKINE_IL1_REASONING_ACCURACY_IMPACT    -0.15f  /**< IL-1β → -15% accuracy */
#define CYTOKINE_IL6_REASONING_ACCURACY_IMPACT    -0.10f  /**< IL-6 → -10% accuracy */
#define CYTOKINE_TNF_REASONING_ACCURACY_IMPACT    -0.20f  /**< TNF-α → -20% accuracy */
#define CYTOKINE_IFN_GAMMA_REASONING_ACCURACY_IMPACT -0.05f /**< IFN-γ → -5% accuracy */
#define CYTOKINE_IL10_REASONING_ACCURACY_IMPACT    0.05f  /**< IL-10 → +5% recovery */

/* Inflammation-based reasoning degradation */
#define INFLAMMATION_LOCAL_REASONING_PENALTY      0.10f   /**< LOCAL → -10% performance */
#define INFLAMMATION_REGIONAL_REASONING_PENALTY   0.25f   /**< REGIONAL → -25% performance */
#define INFLAMMATION_SYSTEMIC_REASONING_PENALTY   0.50f   /**< SYSTEMIC → -50% performance */
#define INFLAMMATION_STORM_REASONING_PENALTY      0.80f   /**< STORM → -80% performance (delirium) */

/* Reasoning failure thresholds for immune trigger */
#define REASONING_CONTRADICTION_SEVERITY          0.6f    /**< Antigen severity for contradiction */
#define REASONING_PROOF_FAILURE_THRESHOLD         3       /**< Failures in window → LOCAL inflammation */
#define REASONING_PROOF_FAILURE_ESCALATION        10      /**< Failures in window → REGIONAL inflammation */
#define REASONING_UNIFICATION_ERROR_THRESHOLD     5       /**< Errors → B cell investigation */
#define REASONING_FAILURE_WINDOW_SEC              10.0f   /**< Time window for failure counting */
#define REASONING_PERSISTENT_ERROR_DURATION_SEC   60.0f   /**< Duration for cytokine release */

/* Working memory capacity impact (Miller's 7±2) */
#define CYTOKINE_IL1_WM_CAPACITY_REDUCTION        1       /**< IL-1β → -1 WM slot */
#define CYTOKINE_IL6_WM_CAPACITY_REDUCTION        1       /**< IL-6 → -1 WM slot */
#define CYTOKINE_TNF_WM_CAPACITY_REDUCTION        2       /**< TNF-α → -2 WM slots */
#define CYTOKINE_IL10_WM_CAPACITY_INCREASE        1       /**< IL-10 → +1 WM slot (recovery) */

/* ============================================================================
 * Types and Structures
 * ============================================================================ */

/**
 * @brief Reasoning-immune integration configuration
 *
 * WHAT: Configuration for bidirectional reasoning-immune coupling
 * WHY:  Allow customization of integration behavior
 * HOW:  Enable/disable features, set sensitivity parameters
 */
typedef struct {
    /* Integration enable flags */
    bool enable_cytokine_reasoning_modulation;   /**< Enable cytokine → reasoning effects */
    bool enable_inflammation_cognitive_slowing;  /**< Enable inflammation → slowing */
    bool enable_reasoning_failure_immune_trigger; /**< Enable reasoning → immune signals */
    bool enable_working_memory_modulation;       /**< Enable WM capacity modulation */
    bool enable_contradiction_immune_alert;      /**< Enable contradiction → antigen presentation */

    /* Cytokine sensitivity */
    float cytokine_sensitivity;                  /**< Global cytokine effect multiplier [0.0, 2.0] */
    float inflammation_sensitivity;              /**< Inflammation effect multiplier [0.0, 2.0] */

    /* Reasoning modulation parameters */
    float max_speed_reduction;                   /**< Maximum allowed speed penalty [0.0, 1.0] */
    float max_accuracy_reduction;                /**< Maximum allowed accuracy penalty [0.0, 1.0] */
    uint32_t min_max_iterations;                 /**< Minimum allowed max_iterations (safety) */

    /* Immune trigger thresholds */
    uint32_t proof_failure_threshold;            /**< Failures for LOCAL inflammation */
    uint32_t proof_failure_escalation;           /**< Failures for REGIONAL inflammation */
    uint32_t unification_error_threshold;        /**< Errors for B cell activation */
    float failure_window_sec;                    /**< Time window for failure counting */
    float contradiction_antigen_severity;        /**< Severity for contradiction antigens */
} reasoning_immune_config_t;

/**
 * @brief Reasoning impairment state
 *
 * WHAT: Current reasoning performance modulation from immune activity
 * WHY:  Track how cytokines/inflammation affect reasoning capacity
 * HOW:  Compute cumulative effects from all immune factors
 */
typedef struct {
    float speed_multiplier;                      /**< Effective speed: 1.0=normal, 0.5=half-speed */
    float accuracy_multiplier;                   /**< Effective accuracy: 1.0=normal */
    uint32_t effective_max_iterations;           /**< Reduced max iterations */
    uint32_t effective_wm_capacity;              /**< Reduced WM capacity (items) */
    float timeout_multiplier;                    /**< Inference timeout multiplier */

    /* Contributing factors */
    float cytokine_speed_impact;                 /**< Speed impact from cytokines */
    float cytokine_accuracy_impact;              /**< Accuracy impact from cytokines */
    float inflammation_penalty;                  /**< Performance penalty from inflammation */
    brain_inflammation_level_t max_inflammation; /**< Highest inflammation level */
} reasoning_impairment_t;

/**
 * @brief Reasoning failure tracking
 *
 * WHAT: Tracks reasoning failures for immune trigger detection
 * WHY:  Identify patterns indicating corrupted state
 * HOW:  Count failures in sliding window, escalate to immune
 */
typedef struct {
    uint32_t proof_failures_recent;              /**< Proof failures in window */
    uint32_t unification_errors_recent;          /**< Unification errors in window */
    uint32_t contradictions_detected;            /**< Contradictions detected (lifetime) */
    uint64_t first_failure_time_ms;              /**< First failure in current window */
    uint64_t last_failure_time_ms;               /**< Most recent failure */
    bool persistent_error_state;                 /**< True if errors > duration threshold */
    uint64_t error_state_start_ms;               /**< When persistent errors began */
} reasoning_failure_state_t;

/**
 * @brief Reasoning-immune bridge statistics
 */
typedef struct {
    uint64_t total_cytokine_modulations;         /**< Total cytokine modulations applied */
    uint64_t total_inflammation_modulations;     /**< Total inflammation modulations */
    uint64_t total_immune_triggers;              /**< Total immune responses triggered */
    uint64_t contradictions_reported;            /**< Contradictions reported to immune */
    uint64_t proof_failures_reported;            /**< Proof failures reported to immune */
    uint64_t unification_errors_reported;        /**< Unification errors reported to immune */
    float avg_speed_reduction;                   /**< Average speed reduction from immune */
    float avg_accuracy_reduction;                /**< Average accuracy reduction from immune */
    float max_speed_reduction_observed;          /**< Maximum speed reduction observed */
    float max_accuracy_reduction_observed;       /**< Maximum accuracy reduction observed */
} reasoning_immune_stats_t;

/**
 * @brief Reasoning-immune bridge instance (opaque)
 */
typedef struct reasoning_immune_bridge reasoning_immune_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Populate config with biologically-based defaults
 * WHY:  Ensure safe, realistic default behavior
 * HOW:  Set defaults from literature-based constants
 *
 * @param config Configuration to populate (non-NULL)
 * @return 0 on success, -1 on NULL parameter
 */
int reasoning_immune_default_config(reasoning_immune_config_t* config);

/**
 * @brief Create reasoning-immune bridge
 *
 * WHAT: Create bidirectional integration between reasoning and immune systems
 * WHY:  Enable cytokine modulation of reasoning, reasoning failure immune triggers
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~2KB for bridge structure
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system (non-NULL)
 * @param reasoning_integration Reasoning integration handle (non-NULL)
 * @return Bridge handle, NULL on failure
 *
 * @note Both systems must remain valid for bridge lifetime
 * @note Thread-safe after creation (uses mutex)
 */
reasoning_immune_bridge_t* reasoning_immune_bridge_create(
    const reasoning_immune_config_t* config,
    brain_immune_system_t* immune_system,
    reasoning_integration_t* reasoning_integration
);

/**
 * @brief Destroy reasoning-immune bridge
 *
 * WHAT: Release bridge resources
 * WHY:  Clean shutdown, prevent leaks
 * HOW:  Free allocations, NULL-safe
 *
 * COMPLEXITY: O(1)
 *
 * @param bridge Bridge handle (NULL-safe)
 */
void reasoning_immune_bridge_destroy(reasoning_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Reasoning Modulation Functions
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to reasoning
 *
 * WHAT: Modulate reasoning performance based on current cytokine levels
 * WHY:  Model biological cytokine-induced cognitive slowing
 * HOW:  Query immune system, compute speed/accuracy multipliers, update reasoning config
 *
 * ALGORITHM:
 * 1. Get current cytokine levels from immune system
 * 2. Compute speed impact: sum(cytokine_level[i] * SPEED_IMPACT[i])
 * 3. Compute accuracy impact: sum(cytokine_level[i] * ACCURACY_IMPACT[i])
 * 4. Clamp to configured max reductions
 * 5. Update reasoning integration configuration
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: <20μs
 *
 * @param bridge Bridge handle (non-NULL)
 * @return 0 on success, -1 on error
 *
 * @note Should be called periodically (e.g., every inference or on cytokine change)
 * @note Effects are reversible when cytokines clear
 */
int reasoning_immune_apply_cytokine_effects(reasoning_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to reasoning
 *
 * WHAT: Reduce reasoning capacity based on inflammation level
 * WHY:  Model chronic inflammation cognitive deficits
 * HOW:  Query max inflammation, apply corresponding penalty
 *
 * EFFECTS:
 * - LOCAL: -10% speed, -5% accuracy
 * - REGIONAL: -25% speed, -15% accuracy
 * - SYSTEMIC: -50% speed, -30% accuracy
 * - STORM: -80% speed, -60% accuracy (delirium)
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: <15μs
 *
 * @param bridge Bridge handle (non-NULL)
 * @return 0 on success, -1 on error
 *
 * @note Inflammation effects stack with cytokine effects
 * @note STORM level represents delirium-like state
 */
int reasoning_immune_apply_inflammation_effects(reasoning_immune_bridge_t* bridge);

/**
 * @brief Compute current reasoning impairment
 *
 * WHAT: Calculate cumulative reasoning performance degradation
 * WHY:  Provide visibility into immune-induced impairment
 * HOW:  Combine cytokine and inflammation effects
 *
 * @param bridge Bridge handle (non-NULL)
 * @param impairment Output impairment state (non-NULL)
 * @return 0 on success, -1 on error
 *
 * @note Result is snapshot; call apply functions to actually modify reasoning
 */
int reasoning_immune_get_impairment(
    const reasoning_immune_bridge_t* bridge,
    reasoning_impairment_t* impairment
);

/* ============================================================================
 * Reasoning → Immune Trigger Functions
 * ============================================================================ */

/**
 * @brief Report contradiction to immune system
 *
 * WHAT: Signal logical contradiction as potential corruption
 * WHY:  Contradictions may indicate Byzantine node, data corruption, adversarial input
 * HOW:  Present contradiction as antigen for immune investigation
 *
 * IMMUNE RESPONSE:
 * - Antigen presentation with MEDIUM severity
 * - B cell activation if cross-reactive with known patterns
 * - May trigger IL-1β release if novel
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: <30μs
 *
 * @param bridge Bridge handle (non-NULL)
 * @param contradiction_description Text description of contradiction (non-NULL)
 * @return 0 on success, -1 on error
 *
 * @note Called from reasoning integration on EVENT_CONTRADICTION_DETECTED
 */
int reasoning_immune_report_contradiction(
    reasoning_immune_bridge_t* bridge,
    const char* contradiction_description
);

/**
 * @brief Report proof failure to immune system
 *
 * WHAT: Track proof failures, escalate if threshold exceeded
 * WHY:  Repeated failures may indicate systemic issue
 * HOW:  Count failures in sliding window, trigger inflammation if threshold exceeded
 *
 * ESCALATION LOGIC:
 * - 1-2 failures: Track only (no immune response)
 * - 3+ failures in 10s: Trigger LOCAL inflammation
 * - 10+ failures in 10s: Escalate to REGIONAL inflammation
 * - Persistent failures (>60s): Release IL-1β, IL-6
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: <25μs
 *
 * @param bridge Bridge handle (non-NULL)
 * @param goal_description Text description of failed goal (NULL allowed)
 * @return 0 on success, -1 on error
 *
 * @note Called from reasoning integration on EVENT_PROOF_FAILED
 * @note Window resets after successful proof
 */
int reasoning_immune_report_proof_failure(
    reasoning_immune_bridge_t* bridge,
    const char* goal_description
);

/**
 * @brief Report unification error to immune system
 *
 * WHAT: Track unification errors, investigate if threshold exceeded
 * WHY:  Pattern matching failures may indicate corrupted data structures
 * HOW:  Count errors, activate B cell investigation at threshold
 *
 * IMMUNE RESPONSE:
 * - Track errors in sliding window
 * - 5+ errors: B cell activation (pattern investigation)
 * - 10+ errors: Antigen presentation
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: <20μs
 *
 * @param bridge Bridge handle (non-NULL)
 * @param error_description Text description of error (NULL allowed)
 * @return 0 on success, -1 on error
 *
 * @note Called from reasoning integration on EVENT_UNIFICATION_FAILED
 */
int reasoning_immune_report_unification_error(
    reasoning_immune_bridge_t* bridge,
    const char* error_description
);

/**
 * @brief Clear reasoning failure tracking
 *
 * WHAT: Reset failure counters (e.g., after successful proof)
 * WHY:  Prevent spurious immune triggers from isolated failures
 * HOW:  Zero failure counts, reset window timer
 *
 * @param bridge Bridge handle (non-NULL)
 * @return 0 on success, -1 on error
 *
 * @note Called on successful proof or after immune response resolves issue
 */
int reasoning_immune_clear_failure_tracking(reasoning_immune_bridge_t* bridge);

/* ============================================================================
 * Query Functions
 * ============================================================================ */

/**
 * @brief Get current failure state
 *
 * @param bridge Bridge handle (non-NULL)
 * @param failure_state Output failure state (non-NULL)
 * @return 0 on success, -1 on error
 */
int reasoning_immune_get_failure_state(
    const reasoning_immune_bridge_t* bridge,
    reasoning_failure_state_t* failure_state
);

/**
 * @brief Get configuration
 *
 * @param bridge Bridge handle (non-NULL)
 * @param config Output configuration (non-NULL)
 * @return 0 on success, -1 on error
 */
int reasoning_immune_get_config(
    const reasoning_immune_bridge_t* bridge,
    reasoning_immune_config_t* config
);

/**
 * @brief Update configuration
 *
 * @param bridge Bridge handle (non-NULL)
 * @param config New configuration (non-NULL, validated)
 * @return 0 on success, -1 on invalid config
 */
int reasoning_immune_set_config(
    reasoning_immune_bridge_t* bridge,
    const reasoning_immune_config_t* config
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle (non-NULL)
 * @param stats Output statistics (non-NULL)
 * @return 0 on success, -1 on error
 */
int reasoning_immune_get_stats(
    const reasoning_immune_bridge_t* bridge,
    reasoning_immune_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Bridge handle (non-NULL)
 * @return 0 on success, -1 on error
 */
int reasoning_immune_reset_stats(reasoning_immune_bridge_t* bridge);

/* ============================================================================
 * Integration API (for reasoning module)
 * ============================================================================ */

/**
 * @brief Connect reasoning system to immune system
 *
 * WHAT: Establish bidirectional reasoning-immune integration
 * WHY:  Single API for reasoning module to enable immune coupling
 * HOW:  Create bridge, register callbacks with reasoning integration
 *
 * USAGE:
 * ```c
 * brain_immune_system_t* immune = brain_immune_create(&immune_config);
 * reasoning_integration_t* reasoning = reasoning_integration_create(event_bus);
 *
 * reasoning_immune_bridge_t* bridge =
 *     reasoning_connect_immune(reasoning, immune, NULL);
 * ```
 *
 * @param reasoning_integration Reasoning integration handle (non-NULL)
 * @param immune_system Brain immune system (non-NULL)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle, NULL on failure
 *
 * @note This is the primary integration API
 * @note Bridge must be destroyed before systems
 */
reasoning_immune_bridge_t* reasoning_connect_immune(
    reasoning_integration_t* reasoning_integration,
    brain_immune_system_t* immune_system,
    const reasoning_immune_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_REASONING_IMMUNE_H
