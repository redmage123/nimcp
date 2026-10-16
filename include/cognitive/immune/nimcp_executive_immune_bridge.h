/**
 * @file nimcp_executive_immune_bridge.h
 * @brief Executive Function-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and executive functions
 * WHY:  Biological evidence shows inflammation impairs prefrontal cortex function.
 *       Essential for realistic cognitive modeling under immune challenge.
 * HOW:  Cytokines modulate executive capacity, task switching, inhibition, and planning.
 *       Executive overload/stress triggers inflammatory responses.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → EXECUTIVE PATHWAYS:
 * ----------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Cross blood-brain barrier
 *    - Reduce prefrontal cortex activation
 *    - Impair working memory capacity (7±2 → 3-4 items)
 *    - Increase task switching costs (200ms → 400-600ms)
 *    - Reduce inhibitory control (impulse control failures)
 *    - Simplify planning (reduce depth from 10 → 3-5 steps)
 *    - Reference: Harrison et al. (2009) "Inflammation causes mood changes through alterations in subgenual cingulate activity"
 *
 * 2. Cognitive Fog/Sickness Behavior:
 *    - Reduced attention and focus
 *    - Slowed processing speed
 *    - Impaired decision-making
 *    - Increased perseveration (reduced flexibility)
 *    - Reference: Dantzer et al. (2008) "Cytokine-induced sickness behavior"
 *
 * 3. IL-6 Effects on Cognitive Flexibility:
 *    - Impairs task switching and set-shifting
 *    - Increases errors on Wisconsin Card Sorting Test
 *    - Reduces ability to update strategies
 *    - Reference: Marsland et al. (2006) "IL-6 covaries inversely with cognitive performance"
 *
 * 4. Chronic Inflammation:
 *    - Progressive executive dysfunction
 *    - Reduced planning horizon
 *    - Impaired goal maintenance
 *    - Reference: Gimeno et al. (2009) "Inflammation and cognitive function"
 *
 * EXECUTIVE → IMMUNE PATHWAYS:
 * ----------------------------
 * 1. Cognitive Overload/Executive Stress:
 *    - Activates HPA axis (cortisol release)
 *    - Initially suppresses immune function
 *    - Followed by inflammatory rebound
 *    - High task load → elevated IL-6
 *    - Reference: Segerstrom & Miller (2004) "Psychological stress and immune system"
 *
 * 2. Chronic Executive Demands:
 *    - Sustained cognitive load → chronic inflammation
 *    - Burnout associated with elevated CRP, IL-6
 *    - Exhaustion → immune dysregulation
 *    - Reference: Oosterholt et al. (2015) "Burnout and cortisol"
 *
 * 3. Goal Frustration:
 *    - Failed planning → stress response
 *    - Task switching failures → frustration → inflammation
 *    - Inhibition failures (ethical violations) → guilt → immune activation
 *    - Reference: Dickerson & Kemeny (2004) "Acute stressors and cortisol responses"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    EXECUTIVE-IMMUNE BRIDGE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → EXECUTIVE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.3 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.4 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.5 │         ├──→ Cognitive Impairment               │  ║
 * ║   │   │              │         │    (Fog, Slowness)                    │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     EXECUTIVE FUNCTIONS         │                             │  ║
 * ║   │   │  - Capacity reduction (100% → 10%)                            │  ║
 * ║   │   │  - Switch cost increase (200ms → 600ms)                       │  ║
 * ║   │   │  - Inhibition impairment (0.7 → 0.95)                         │  ║
 * ║   │   │  - Planning depth reduction (10 → 3)                          │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   +0.2       │     Recovery, Restoration                       │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  EXECUTIVE → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ OVERLOAD     │ ──→ Inflammation Trigger                        │  ║
 * ║   │   │ FRUSTRATION  │ ──→ Cortisol → Immune Suppression               │  ║
 * ║   │   │ BURNOUT      │ ──→ IL-6 Release                                │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ SUCCESS      │ ──→ Immune Enhancement                          │  ║
 * ║   │   │ COMPLETION   │ ──→ IL-10 Release                               │  ║
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

#ifndef NIMCP_EXECUTIVE_IMMUNE_BRIDGE_H
#define NIMCP_EXECUTIVE_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_executive.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine executive impact factors */
#define CYTOKINE_IL1_CAPACITY_IMPACT      -0.3f   /**< IL-1β → capacity reduction */
#define CYTOKINE_IL6_CAPACITY_IMPACT      -0.4f   /**< IL-6 → capacity reduction */
#define CYTOKINE_TNF_CAPACITY_IMPACT      -0.5f   /**< TNF-α → strong capacity reduction */
#define CYTOKINE_IFN_GAMMA_CAPACITY_IMPACT -0.2f  /**< IFN-γ → mild capacity reduction */
#define CYTOKINE_IL10_CAPACITY_IMPACT      0.2f   /**< IL-10 → recovery boost */

/* Inflammation-executive mapping */
#define INFLAMMATION_CAPACITY_FLOOR       0.1f    /**< Minimum capacity (10%) during cytokine storm */
#define INFLAMMATION_SWITCH_COST_MULT     2.0f    /**< Switch cost multiplier at max inflammation */
#define INFLAMMATION_INHIBITION_PENALTY   0.25f   /**< Inhibition threshold increase at max inflammation */
#define INFLAMMATION_PLANNING_REDUCTION   0.7f    /**< Planning depth reduction at max inflammation */

/* Executive stress immune trigger thresholds */
#define EXECUTIVE_OVERLOAD_THRESHOLD      0.85f   /**< Cognitive load to trigger immune response */
#define EXECUTIVE_FRUSTRATION_MULTIPLIER  1.3f    /**< Failed tasks amplify inflammation */
#define EXECUTIVE_BURNOUT_THRESHOLD       0.9f    /**< Sustained overload = burnout */

/* Chronic overload duration (seconds) */
#define CHRONIC_OVERLOAD_THRESHOLD        (3600.0f * 4)  /**< 4 hours = chronic overload */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on executive functions
 *
 * Represents how cytokine levels impair executive control
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_capacity_reduction;       /**< IL-1β induced capacity loss */
    float il6_capacity_reduction;       /**< IL-6 induced capacity loss */
    float tnf_capacity_reduction;       /**< TNF-α induced capacity loss */
    float ifn_gamma_capacity_reduction; /**< IFN-γ induced capacity loss */

    /* Anti-inflammatory effects */
    float il10_capacity_recovery;       /**< IL-10 recovery boost */

    /* Aggregate effects */
    float total_capacity_impact;        /**< Combined capacity modulation */
    float cognitive_fog_level;          /**< Overall cognitive fog [0-1] */
    float processing_slowdown;          /**< Processing speed reduction [0-1] */
    float flexibility_impairment;       /**< Task switching impairment [0-1] */
} cytokine_executive_effects_t;

/**
 * @brief Inflammation effects on executive state
 *
 * How chronic inflammation affects executive functions
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;    /**< How long inflamed */
    bool is_chronic;                    /**< >= threshold */

    /* Executive impacts */
    float capacity_reduction;           /**< Capacity loss [0-1] */
    float switch_cost_increase;         /**< Switch cost multiplier [1-3] */
    float inhibition_impairment;        /**< Inhibition threshold increase [0-0.25] */
    float planning_depth_reduction;     /**< Planning depth reduction [0-1] */

    /* Cognitive function decline */
    float working_memory_impairment;    /**< WM capacity loss [0-1] */
    float perseveration_increase;       /**< Reduced flexibility [0-1] */
} inflammation_executive_state_t;

/**
 * @brief Executive stress immune response
 *
 * How executive overload triggers immune activity
 */
typedef struct {
    /* Executive stress indicators */
    float cognitive_load;               /**< Current load [0-1] */
    float overload_duration_sec;        /**< How long overloaded */
    uint32_t failed_tasks;              /**< Recently failed tasks */
    uint32_t task_switches;             /**< Recent switches (high = stress) */

    /* Immune triggers */
    bool cortisol_triggered;            /**< HPA axis activated */
    bool inflammatory_rebound;          /**< Post-stress inflammation */
    float immune_suppression;           /**< Stress-induced suppression [0-1] */

    /* Burnout state */
    float burnout_level;                /**< Burnout severity [0-1] */
    bool chronic_overload;              /**< Sustained overload */
    float immune_dysregulation;         /**< Dysfunction level [0-1] */
} executive_immune_trigger_t;

/**
 * @brief Executive success immune enhancement
 *
 * How task completion and success boost immunity
 */
typedef struct {
    /* Success indicators */
    uint32_t completed_tasks;           /**< Recently completed */
    float success_rate;                 /**< Completion ratio [0-1] */
    float goal_achievement;             /**< Goal progress [0-1] */

    /* Immune benefits */
    float immune_enhancement;           /**< Improved function [0-1] */
    float il10_release_boost;           /**< Anti-inflammatory boost */
    float inflammation_reduction;       /**< Reduced inflammation [0-1] */
    float stress_recovery;              /**< Stress reduction [0-1] */
} executive_success_immune_boost_t;

/**
 * @brief Complete executive-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    executive_controller_t* executive_controller;

    /* Current state */
    cytokine_executive_effects_t cytokine_effects;
    inflammation_executive_state_t inflammation_state;
    executive_immune_trigger_t executive_trigger;
    executive_success_immune_boost_t success_boost;

    /* Integration flags */
    bool enable_cytokine_executive_modulation;
    bool enable_inflammation_impairment;
    bool enable_executive_immune_trigger;
    bool enable_success_immune_boost;
    bool enable_overload_monitoring;

    /* Tracking */
    uint64_t last_update_time;          /**< Last update timestamp */
    float prev_cognitive_load;          /**< Previous load for delta tracking */
    uint32_t prev_failed_tasks;         /**< Previous failures */

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t executive_triggered_responses;
    uint32_t success_boosts;
    uint32_t overload_events;
    uint32_t burnout_events;
    } executive_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_executive_modulation;
    bool enable_inflammation_impairment;
    bool enable_executive_immune_trigger;
    bool enable_success_immune_boost;
    bool enable_overload_monitoring;

    /* Sensitivity tuning */
    float cytokine_sensitivity;         /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;     /**< Inflammation effect multiplier [0.5-2.0] */
    float overload_trigger_sensitivity; /**< Overload trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float overload_trigger_threshold;   /**< Cognitive load to trigger immune [0.7-0.95] */
    float burnout_threshold;            /**< Sustained load for burnout [0.8-1.0] */
    float capacity_floor;               /**< Minimum capacity during inflammation [0.05-0.2] */
} executive_immune_config_t;

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
int executive_immune_default_config(executive_immune_config_t* config);

/**
 * @brief Create executive-immune bridge
 *
 * WHAT: Initialize bidirectional executive-immune integration
 * WHY:  Enable realistic immune-executive coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param executive_controller Executive controller
 * @return New bridge or NULL on failure
 */
executive_immune_bridge_t* executive_immune_bridge_create(
    const executive_immune_config_t* config,
    brain_immune_system_t* immune_system,
    executive_controller_t* executive_controller
);

/**
 * @brief Destroy executive-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void executive_immune_bridge_destroy(executive_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Executive API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to executive functions
 *
 * WHAT: Modulate executive capacity based on cytokine levels
 * WHY:  Pro-inflammatory cytokines impair prefrontal function
 * HOW:  Query immune system cytokines, adjust capacity/switch cost/inhibition
 *
 * @param bridge Executive-immune bridge
 * @return 0 on success
 */
int executive_immune_apply_cytokine_effects(executive_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to executive functions
 *
 * WHAT: Induce cognitive fog and executive dysfunction from prolonged inflammation
 * WHY:  Chronic inflammation causes progressive executive impairment
 * HOW:  Check inflammation duration/level, reduce capacity and planning depth
 *
 * @param bridge Executive-immune bridge
 * @return 0 on success
 */
int executive_immune_apply_inflammation_effects(executive_immune_bridge_t* bridge);

/**
 * @brief Compute capacity reduction from inflammation
 *
 * WHAT: Calculate executive capacity loss from immune state
 * WHY:  Inflammation reduces prefrontal activation
 * HOW:  Map inflammation level/duration to capacity reduction [0-1]
 *
 * @param bridge Executive-immune bridge
 * @return Capacity reduction [0-1] (0=no reduction, 1=complete loss)
 */
float executive_immune_compute_capacity_reduction(const executive_immune_bridge_t* bridge);

/**
 * @brief Compute switch cost increase from inflammation
 *
 * WHAT: Calculate task switching cost increase from inflammation
 * WHY:  Cytokines increase perseveration and cognitive rigidity
 * HOW:  Map inflammation level to switch cost multiplier [1-3]
 *
 * @param bridge Executive-immune bridge
 * @return Switch cost multiplier [1-3]
 */
float executive_immune_compute_switch_cost_increase(const executive_immune_bridge_t* bridge);

/**
 * @brief Compute inhibition impairment from inflammation
 *
 * WHAT: Calculate inhibitory control loss from inflammation
 * WHY:  Cytokines impair prefrontal inhibition
 * HOW:  Map inflammation level to inhibition threshold increase [0-0.25]
 *
 * @param bridge Executive-immune bridge
 * @return Inhibition threshold increase [0-0.25]
 */
float executive_immune_compute_inhibition_impairment(const executive_immune_bridge_t* bridge);

/**
 * @brief Compute planning depth reduction from inflammation
 *
 * WHAT: Calculate planning horizon reduction from inflammation
 * WHY:  Cytokines simplify goal hierarchies
 * HOW:  Map inflammation level to depth reduction [0-1]
 *
 * @param bridge Executive-immune bridge
 * @return Planning depth reduction factor [0-1]
 */
float executive_immune_compute_planning_reduction(const executive_immune_bridge_t* bridge);

/* ============================================================================
 * Executive → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from executive overload
 *
 * WHAT: Activate immune system from high cognitive load
 * WHY:  Executive stress activates HPA axis and inflammatory response
 * HOW:  Check executive load, trigger cytokine release
 *
 * @param bridge Executive-immune bridge
 * @return 0 on success
 */
int executive_immune_trigger_from_overload(executive_immune_bridge_t* bridge);

/**
 * @brief Amplify inflammation from task failures
 *
 * WHAT: Increase inflammatory response from goal frustration
 * WHY:  Frustration and failure intensify stress response
 * HOW:  Query failed tasks, scale immune inflammation level
 *
 * @param bridge Executive-immune bridge
 * @return 0 on success
 */
int executive_immune_amplify_from_frustration(executive_immune_bridge_t* bridge);

/**
 * @brief Detect and respond to burnout state
 *
 * WHAT: Identify sustained executive overload (burnout)
 * WHY:  Chronic overload → chronic inflammation
 * HOW:  Track overload duration, trigger sustained immune activation
 *
 * @param bridge Executive-immune bridge
 * @return 0 on success
 */
int executive_immune_detect_burnout(executive_immune_bridge_t* bridge);

/**
 * @brief Boost immune function from task success
 *
 * WHAT: Enhance immunity from goal achievement and task completion
 * WHY:  Success reduces stress and promotes recovery
 * HOW:  Query completed tasks, release IL-10, reduce inflammation
 *
 * @param bridge Executive-immune bridge
 * @return 0 on success
 */
int executive_immune_boost_from_success(executive_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update executive-immune bridge (both directions)
 *
 * WHAT: Process all immune-executive interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from overload, boost from success
 *
 * @param bridge Executive-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int executive_immune_bridge_update(
    executive_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine executive effects
 *
 * @param bridge Executive-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int executive_immune_get_cytokine_effects(
    const executive_immune_bridge_t* bridge,
    cytokine_executive_effects_t* effects
);

/**
 * @brief Get current inflammation executive state
 *
 * @param bridge Executive-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int executive_immune_get_inflammation_state(
    const executive_immune_bridge_t* bridge,
    inflammation_executive_state_t* state
);

/**
 * @brief Check if experiencing cognitive fog
 *
 * WHAT: Determine if cytokines inducing cognitive fog
 * WHY:  Cognitive fog is distinct impairment state
 * HOW:  Check cytokine levels and fog score
 *
 * @param bridge Executive-immune bridge
 * @return true if experiencing cognitive fog
 */
bool executive_immune_is_cognitive_fog(const executive_immune_bridge_t* bridge);

/**
 * @brief Get cognitive fog severity
 *
 * @param bridge Executive-immune bridge
 * @return Cognitive fog level [0-1]
 */
float executive_immune_get_cognitive_fog_severity(const executive_immune_bridge_t* bridge);

/**
 * @brief Check if in burnout state
 *
 * @param bridge Executive-immune bridge
 * @return true if burnout detected
 */
bool executive_immune_is_burnout(const executive_immune_bridge_t* bridge);

/**
 * @brief Get burnout severity
 *
 * @param bridge Executive-immune bridge
 * @return Burnout level [0-1]
 */
float executive_immune_get_burnout_severity(const executive_immune_bridge_t* bridge);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_EXECUTIVE
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int executive_immune_connect_bio_async(executive_immune_bridge_t* bridge);

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
int executive_immune_disconnect_bio_async(executive_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool executive_immune_is_bio_async_connected(const executive_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXECUTIVE_IMMUNE_BRIDGE_H */
