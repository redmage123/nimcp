/**
 * @file nimcp_eligibility_immune_bridge.h
 * @brief Eligibility Trace-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and eligibility traces
 * WHY:  Biological evidence shows inflammation impairs temporal credit assignment and
 *       dopamine signaling. IL-1β affects reward learning. Essential for realistic RL.
 * HOW:  Cytokines shorten trace decay window, inflammation impairs credit assignment,
 *       immune activity modulates trace consolidation, recovery restores normal dynamics.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → ELIGIBILITY TRACE PATHWAYS:
 * -------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Impair dopamine signaling in striatum
 *    - Reduce D2 receptor availability → weakened reward learning
 *    - Shorten eligibility trace duration (accelerated decay)
 *    - IL-1β specifically blocks dopamine-dependent LTP
 *    - Reference: Felger & Treadway (2017) "Inflammation and reward processing"
 *
 * 2. Inflammation-Induced Trace Impairment:
 *    - Systemic inflammation shortens trace window (100-1000ms → 50-200ms)
 *    - Credit assignment becomes more local (distal rewards ignored)
 *    - Learning rate reduction during inflammatory states
 *    - "Sickness behavior" includes impaired temporal learning
 *    - Reference: Croxson et al. (2009) "Effort-based cost-benefit valuation"
 *
 * 3. Dopamine System Disruption:
 *    - Cytokines reduce dopamine synthesis (tyrosine hydroxylase inhibition)
 *    - Increased dopamine reuptake → lower tonic levels
 *    - Blunted phasic bursts → weakened reward prediction errors
 *    - Eligibility trace consolidation requires dopamine bursts
 *    - Reference: Felger et al. (2013) "Inflammation is associated with anhedonia"
 *
 * 4. Chronic Inflammation Effects:
 *    - Persistent trace decay acceleration (λ drops from 0.95 to 0.70-0.80)
 *    - Impaired long-term credit assignment (>500ms)
 *    - Motor learning deficits (delayed feedback tasks)
 *    - Reference: Harrison et al. (2016) "Inflammation and reward processing"
 *
 * 5. Anti-inflammatory Recovery (IL-10):
 *    - Restores normal trace dynamics
 *    - Enables dopamine system recovery
 *    - Gradually returns λ to baseline
 *    - Allows distal reward learning to resume
 *
 * ELIGIBILITY → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Learning Failure Stress:
 *    - Repeated reward prediction errors without resolution
 *    - Sustained negative reward signals → stress response
 *    - HPA axis activation → cortisol → immune modulation
 *    - Learned helplessness triggers inflammation
 *    - Reference: Maier & Watkins (2005) "Cytokines and learned helplessness"
 *
 * 2. Consolidation Failure Detection:
 *    - Traces accumulate but never consolidate (no dopamine bursts)
 *    - Frustration signal → immune activation
 *    - Represents "neural stress" from failed learning
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                ELIGIBILITY TRACE-IMMUNE BRIDGE                             ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE → ELIGIBILITY TRACE PATHWAYS                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.5 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.3 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.4 │         ├──→ Trace Decay Acceleration           │  ║
 * ║   │   │              │         │    (λ: 0.95 → 0.70-0.80)              │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │   ELIGIBILITY TRACE SYSTEM      │                             │  ║
 * ║   │   │  - Decay rate modulation        │                             │  ║
 * ║   │   │  - Learning rate reduction      │                             │  ║
 * ║   │   │  - Consolidation impairment     │                             │  ║
 * ║   │   │  - Credit assignment window     │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   +0.3       │     Recovery, Restore λ                         │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              ELIGIBILITY → IMMUNE PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ LEARNING     │ ──→ Sustained Negative Reward                   │  ║
 * ║   │   │ FAILURE      │ ──→ Stress Response → Inflammation              │  ║
 * ║   │   │              │                                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ CONSOLIDATION│ ──→ Trace Accumulation Without Bursts           │  ║
 * ║   │   │ FAILURE      │ ──→ Frustration → Immune Activation             │  ║
 * ║   │   └──────────────┘                                                 │  ║
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

#ifndef NIMCP_ELIGIBILITY_IMMUNE_BRIDGE_H
#define NIMCP_ELIGIBILITY_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine trace decay impact factors (accelerate decay) */
#define CYTOKINE_IL1_DECAY_IMPACT       -0.5f   /**< IL-1β → strong trace shortening */
#define CYTOKINE_IL6_DECAY_IMPACT       -0.3f   /**< IL-6 → moderate trace shortening */
#define CYTOKINE_TNF_DECAY_IMPACT       -0.4f   /**< TNF-α → strong trace shortening */
#define CYTOKINE_IL10_DECAY_IMPACT       0.3f   /**< IL-10 → restore normal traces */

/* Inflammation trace window mapping */
#define INFLAMMATION_TRACE_MULTIPLIER_NONE       1.00f   /**< Normal trace duration */
#define INFLAMMATION_TRACE_MULTIPLIER_LOCAL      0.90f   /**< Slight trace shortening */
#define INFLAMMATION_TRACE_MULTIPLIER_REGIONAL   0.80f   /**< Moderate trace shortening */
#define INFLAMMATION_TRACE_MULTIPLIER_SYSTEMIC   0.70f   /**< Severe trace shortening */
#define INFLAMMATION_TRACE_MULTIPLIER_STORM      0.60f   /**< Critical trace shortening */

/* Learning rate modulation during inflammation */
#define INFLAMMATION_LR_FACTOR_NONE              1.00f   /**< Normal learning */
#define INFLAMMATION_LR_FACTOR_LOCAL             0.95f   /**< Slight LR reduction */
#define INFLAMMATION_LR_FACTOR_REGIONAL          0.85f   /**< Moderate LR reduction */
#define INFLAMMATION_LR_FACTOR_SYSTEMIC          0.70f   /**< Severe LR reduction */
#define INFLAMMATION_LR_FACTOR_STORM             0.50f   /**< Critical LR reduction */

/* Learning failure stress thresholds */
#define LEARNING_FAILURE_STRESS_THRESHOLD        0.7f    /**< Sustained negative reward trigger */
#define CONSOLIDATION_FAILURE_THRESHOLD          100     /**< Accumulated traces without bursts */

/* Chronic inflammation duration (seconds) */
#define CHRONIC_INFLAMMATION_THRESHOLD_SEC       (86400.0f * 7)  /**< 7 days = chronic */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on eligibility traces
 *
 * Represents how cytokine levels modulate trace dynamics
 */
typedef struct {
    /* Pro-inflammatory effects (shorten traces) */
    float il1_trace_shortening;       /**< IL-1β induced trace decay acceleration */
    float il6_trace_shortening;       /**< IL-6 induced trace decay acceleration */
    float tnf_trace_shortening;       /**< TNF-α induced trace decay acceleration */

    /* Anti-inflammatory effects (restore traces) */
    float il10_trace_restoration;     /**< IL-10 trace recovery */

    /* Aggregate effects */
    float total_decay_modifier;       /**< Combined decay rate change */
    float learning_rate_modifier;     /**< Combined learning rate change */
    float consolidation_impairment;   /**< Burst consolidation weakening [0-1] */
    float credit_assignment_window_ms;/**< Effective credit assignment window */
} cytokine_trace_effects_t;

/**
 * @brief Inflammation effects on trace dynamics
 *
 * How inflammatory state affects eligibility trace system
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* Trace impacts */
    float decay_lambda_modifier;       /**< Multiplier for decay_lambda [0.6-1.0] */
    float learning_rate_factor;        /**< LR reduction factor [0.5-1.0] */
    float trace_window_ms;             /**< Effective trace duration */
    float distal_reward_impairment;    /**< Long-delay credit assignment [0-1] */

    /* Dopamine system disruption */
    float dopamine_synthesis_reduction;/**< Reduced DA production [0-1] */
    float burst_amplitude_reduction;   /**< Weakened phasic bursts [0-1] */
} inflammation_trace_state_t;

/**
 * @brief Learning failure stress detection
 *
 * How repeated learning failures trigger immune response
 */
typedef struct {
    /* Learning metrics */
    float cumulative_negative_reward;  /**< Running negative reward sum */
    uint32_t consecutive_failures;     /**< Failed learning attempts */
    float average_reward_error;        /**< Mean reward prediction error */

    /* Consolidation metrics */
    uint32_t unconsolidated_traces;    /**< Traces awaiting consolidation */
    uint64_t last_consolidation_ms;    /**< Last successful burst consolidation */
    float consolidation_frustration;   /**< Frustration level [0-1] */

    /* Stress indicators */
    bool learned_helplessness;         /**< Chronic failure detected */
    float stress_level;                /**< Overall stress [0-1] */
    bool immune_triggered;             /**< Immune system activated */
} learning_failure_stress_t;

/**
 * @brief Complete eligibility-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    eligibility_config_t* eligibility_config;

    /* Current state */
    cytokine_trace_effects_t cytokine_effects;
    inflammation_trace_state_t inflammation_state;
    learning_failure_stress_t learning_stress;

    /* Baseline parameters (for restoration) */
    float baseline_decay_lambda;       /**< Original decay_lambda */
    float baseline_learning_rate;      /**< Original learning_rate */
    float baseline_trace_window_ms;    /**< Original trace duration */

    /* Integration flags */
    bool enable_cytokine_trace_modulation;
    bool enable_inflammation_impairment;
    bool enable_learning_failure_detection;
    bool enable_consolidation_monitoring;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t trace_shortenings;
    uint32_t learning_failure_triggers;
    uint32_t consolidation_failures;
    } eligibility_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_trace_modulation;
    bool enable_inflammation_impairment;
    bool enable_learning_failure_detection;
    bool enable_consolidation_monitoring;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float stress_trigger_sensitivity;  /**< Learning failure trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float learning_failure_threshold;  /**< Negative reward to trigger immune [0.5-0.9] */
    uint32_t consolidation_failure_count; /**< Unconsolidated traces threshold */
} eligibility_immune_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int eligibility_immune_default_config(eligibility_immune_config_t* config);

/**
 * @brief Create eligibility-immune bridge
 *
 * WHAT: Initialize bidirectional eligibility-immune integration
 * WHY:  Enable realistic immune-trace coupling
 * HOW:  Allocate structure, link subsystems, store baselines
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param eligibility_config Eligibility trace configuration to modulate
 * @return New bridge or NULL on failure
 */
eligibility_immune_bridge_t* eligibility_immune_bridge_create(
    const eligibility_immune_config_t* config,
    brain_immune_system_t* immune_system,
    eligibility_config_t* eligibility_config
);

/**
 * @brief Destroy eligibility-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void eligibility_immune_bridge_destroy(eligibility_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Eligibility Trace API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to trace dynamics
 *
 * WHAT: Modulate decay_lambda and learning_rate based on cytokine levels
 * WHY:  Pro-inflammatory cytokines shorten trace windows
 * HOW:  Query immune system cytokines, adjust eligibility_config parameters
 *
 * @param bridge Eligibility-immune bridge
 * @return 0 on success
 */
int eligibility_immune_apply_cytokine_effects(eligibility_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to trace dynamics
 *
 * WHAT: Impair credit assignment from prolonged inflammation
 * WHY:  Chronic inflammation causes persistent learning deficits
 * HOW:  Check inflammation duration/level, reduce trace window and LR
 *
 * @param bridge Eligibility-immune bridge
 * @return 0 on success
 */
int eligibility_immune_apply_inflammation_effects(eligibility_immune_bridge_t* bridge);

/**
 * @brief Compute effective decay lambda
 *
 * WHAT: Calculate inflammation-modulated decay constant
 * WHY:  Inflammation accelerates trace decay
 * HOW:  Map inflammation level to decay multiplier [0.6-1.0]
 *
 * @param bridge Eligibility-immune bridge
 * @return Effective decay_lambda [0.6-0.95]
 */
float eligibility_immune_get_effective_lambda(const eligibility_immune_bridge_t* bridge);

/**
 * @brief Compute effective learning rate
 *
 * WHAT: Calculate inflammation-modulated learning rate
 * WHY:  Inflammation reduces learning effectiveness
 * HOW:  Map inflammation level to LR factor [0.5-1.0]
 *
 * @param bridge Eligibility-immune bridge
 * @return Effective learning rate factor [0.5-1.0]
 */
float eligibility_immune_get_lr_factor(const eligibility_immune_bridge_t* bridge);

/**
 * @brief Restore normal trace dynamics
 *
 * WHAT: Return decay_lambda and learning_rate to baseline
 * WHY:  Recovery from inflammation restores learning
 * HOW:  Gradually restore baseline parameters from IL-10
 *
 * @param bridge Eligibility-immune bridge
 * @return 0 on success
 */
int eligibility_immune_restore_baseline(eligibility_immune_bridge_t* bridge);

/* ============================================================================
 * Eligibility → Immune API
 * ============================================================================ */

/**
 * @brief Detect learning failure stress
 *
 * WHAT: Monitor for sustained negative rewards and trigger immune
 * WHY:  Learning failure creates stress-induced inflammation
 * HOW:  Track cumulative negative reward, trigger immune at threshold
 *
 * @param bridge Eligibility-immune bridge
 * @param reward Current reward signal
 * @return 0 on success
 */
int eligibility_immune_detect_learning_failure(
    eligibility_immune_bridge_t* bridge,
    float reward
);

/**
 * @brief Monitor consolidation failures
 *
 * WHAT: Detect traces accumulating without consolidation
 * WHY:  Failed consolidation represents neural frustration
 * HOW:  Count unconsolidated traces, trigger immune if excessive
 *
 * @param bridge Eligibility-immune bridge
 * @param num_active_traces Number of traces awaiting consolidation
 * @param burst_occurred Whether dopamine burst occurred
 * @return 0 on success
 */
int eligibility_immune_monitor_consolidation(
    eligibility_immune_bridge_t* bridge,
    uint32_t num_active_traces,
    bool burst_occurred
);

/**
 * @brief Trigger immune from learning stress
 *
 * WHAT: Activate immune system from chronic learning failures
 * WHY:  Learned helplessness triggers inflammation
 * HOW:  Present "learning failure antigen" to immune system
 *
 * @param bridge Eligibility-immune bridge
 * @return 0 on success
 */
int eligibility_immune_trigger_from_learning_stress(eligibility_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update eligibility-immune bridge (both directions)
 *
 * WHAT: Process all immune-eligibility interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, detect learning failures, monitor consolidation
 *
 * @param bridge Eligibility-immune bridge
 * @param delta_ms Time since last update
 * @param current_reward Current reward signal (for failure detection)
 * @param num_active_traces Number of active traces
 * @param burst_occurred Whether dopamine burst occurred this step
 * @return 0 on success
 */
int eligibility_immune_bridge_update(
    eligibility_immune_bridge_t* bridge,
    uint64_t delta_ms,
    float current_reward,
    uint32_t num_active_traces,
    bool burst_occurred
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine trace effects
 *
 * @param bridge Eligibility-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int eligibility_immune_get_cytokine_effects(
    const eligibility_immune_bridge_t* bridge,
    cytokine_trace_effects_t* effects
);

/**
 * @brief Get current inflammation trace state
 *
 * @param bridge Eligibility-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int eligibility_immune_get_inflammation_state(
    const eligibility_immune_bridge_t* bridge,
    inflammation_trace_state_t* state
);

/**
 * @brief Get learning failure stress level
 *
 * @param bridge Eligibility-immune bridge
 * @return Stress level [0-1]
 */
float eligibility_immune_get_stress_level(const eligibility_immune_bridge_t* bridge);

/**
 * @brief Check if experiencing trace impairment
 *
 * WHAT: Determine if inflammation impairing trace dynamics
 * WHY:  Trace impairment is distinct syndrome requiring intervention
 * HOW:  Check inflammation level and trace window reduction
 *
 * @param bridge Eligibility-immune bridge
 * @return true if traces significantly impaired
 */
bool eligibility_immune_is_trace_impaired(const eligibility_immune_bridge_t* bridge);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_ELIGIBILITY
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int eligibility_immune_connect_bio_async(eligibility_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int eligibility_immune_disconnect_bio_async(eligibility_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool eligibility_immune_is_bio_async_connected(const eligibility_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ELIGIBILITY_IMMUNE_BRIDGE_H */
