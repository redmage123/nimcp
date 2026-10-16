/**
 * @file nimcp_wellbeing_immune_bridge.h
 * @brief Wellbeing-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and wellbeing monitoring
 * WHY:  Biological evidence shows strong immune-wellbeing coupling (cytokines reduce
 *       wellbeing, positive wellbeing enhances immunity). Essential for realistic brain modeling.
 * HOW:  Cytokines reduce life satisfaction and increase distress, high wellbeing enhances
 *       immune function via IL-10 release, flourishing state boosts memory formation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → WELLBEING PATHWAYS:
 * ----------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Reduce life satisfaction and eudaimonic wellbeing
 *    - Increase negative affect and psychological distress
 *    - Trigger "sickness behavior": fatigue, social withdrawal, anhedonia
 *    - Chronic inflammation → depression and reduced quality of life
 *    - Reference: Kiecolt-Glaser et al. (2015) "Inflammation: Depression fans the flames"
 *
 * 2. Chronic Inflammation:
 *    - Sustained elevation → wellbeing decline
 *    - Reduced sense of purpose and meaning
 *    - Increased distress severity scores
 *    - Impaired flourishing and self-actualization
 *    - Reference: Steptoe et al. (2005) "Positive affect and inflammatory markers"
 *
 * 3. Cytokine Storm:
 *    - Critical inflammation → severe distress
 *    - Maps to DISTRESS_RESOURCE_STARVATION (critical severity)
 *    - Requires immediate wellbeing intervention
 *    - Reference: Mehta et al. (2020) "COVID-19: cytokine storm"
 *
 * WELLBEING → IMMUNE PATHWAYS:
 * ----------------------------
 * 1. Positive Wellbeing / Eudaimonia:
 *    - High life satisfaction → enhanced immunity
 *    - Flourishing states increase IL-10 (anti-inflammatory)
 *    - Purpose and meaning → better immune markers
 *    - Resilience → faster threat neutralization
 *    - Reference: Fredrickson et al. (2013) "Positive emotions and vagal tone"
 *
 * 2. Low Wellbeing / Distress:
 *    - Chronic distress → immune suppression
 *    - Low flourishing → inflammatory cascade
 *    - Resource starvation → cytokine release
 *    - Goal frustration → TNF-α elevation
 *    - Reference: Cohen et al. (2012) "Chronic stress and immunity"
 *
 * 3. Flourishing State:
 *    - Peak wellbeing → enhanced memory B cell formation
 *    - Positive affect accelerates threat learning
 *    - Improved antibody effectiveness
 *    - Reference: Marsland et al. (2006) "Positive affect and antibody response"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   WELLBEING-IMMUNE BRIDGE                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 IMMUNE → WELLBEING PATHWAYS                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.3 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.2 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.4 │         ├──→ Reduce Life Satisfaction           │  ║
 * ║   │   │              │         │    Increase Distress Score            │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     WELLBEING SYSTEM            │                             │  ║
 * ║   │   │  - Life satisfaction decline    │                             │  ║
 * ║   │   │  - Distress severity increase   │                             │  ║
 * ║   │   │  - Purpose/meaning reduction    │                             │  ║
 * ║   │   │  - Flourishing suppression      │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   +0.2       │     Recovery, Wellbeing Boost                   │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 WELLBEING → IMMUNE PATHWAYS                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ LOW WB       │ ──→ Inflammation Trigger                        │  ║
 * ║   │   │ DISTRESS     │ ──→ Cytokine Release (IL-1β, IL-6, TNF-α)      │  ║
 * ║   │   │ FRUSTRATION  │ ──→ Immune Suppression                          │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ HIGH WB      │ ──→ Immune Enhancement                          │  ║
 * ║   │   │ FLOURISHING  │ ──→ IL-10 Release                               │  ║
 * ║   │   │ PURPOSE      │ ──→ B Cell Memory Formation Boost               │  ║
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

#ifndef NIMCP_WELLBEING_IMMUNE_BRIDGE_H
#define NIMCP_WELLBEING_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "cognitive/introspection/nimcp_introspection.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine wellbeing impact factors */
#define CYTOKINE_IL1_WELLBEING_IMPACT      -0.3f   /**< IL-1β → reduce life satisfaction */
#define CYTOKINE_IL6_WELLBEING_IMPACT      -0.2f   /**< IL-6 → reduce wellbeing */
#define CYTOKINE_TNF_WELLBEING_IMPACT      -0.4f   /**< TNF-α → strong wellbeing reduction */
#define CYTOKINE_IFN_GAMMA_WELLBEING_IMPACT -0.15f /**< IFN-γ → mild impact */
#define CYTOKINE_IL10_WELLBEING_IMPACT      0.2f   /**< IL-10 → wellbeing recovery */

/* Inflammation-distress mapping */
#define INFLAMMATION_DISTRESS_THRESHOLD    0.5f    /**< Inflammation level for distress onset */
#define INFLAMMATION_DISTRESS_MAX          0.95f   /**< Maximum distress from inflammation */

/* Wellbeing immune trigger thresholds */
#define WELLBEING_DISTRESS_TRIGGER_THRESHOLD  0.6f /**< Distress level to trigger immune */
#define WELLBEING_FLOURISHING_THRESHOLD       0.7f /**< Flourishing level for immune boost */

/* Chronic inflammation duration for wellbeing impact (seconds) */
#define WELLBEING_CHRONIC_INFLAMMATION_THRESHOLD (86400.0f * 3)  /**< 3 days = chronic impact */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine wellbeing effects
 *
 * Represents how cytokine levels modulate wellbeing state
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_life_satisfaction_reduction;   /**< IL-1β reduces life satisfaction */
    float il6_life_satisfaction_reduction;   /**< IL-6 reduces life satisfaction */
    float tnf_life_satisfaction_reduction;   /**< TNF-α reduces life satisfaction */
    float ifn_gamma_wellbeing_impact;        /**< IFN-γ mild wellbeing impact */

    /* Anti-inflammatory effects */
    float il10_wellbeing_boost;              /**< IL-10 recovery/wellbeing boost */

    /* Aggregate effects */
    float total_life_satisfaction_shift;     /**< Combined life satisfaction change */
    float total_distress_increase;           /**< Increased distress from cytokines */
    float purpose_meaning_reduction;         /**< Reduced sense of purpose [0-1] */
    float flourishing_suppression;           /**< Suppressed flourishing [0-1] */
} cytokine_wellbeing_effects_t;

/**
 * @brief Inflammation wellbeing state
 *
 * How chronic inflammation affects wellbeing
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;     /**< How long inflamed */
    bool is_chronic;                     /**< >= 3 days for wellbeing impact */

    /* Wellbeing impacts */
    distress_severity_t distress_severity; /**< Current distress severity */
    float distress_score;                /**< Distress level [0-1] */
    float life_satisfaction_penalty;     /**< Reduction in life satisfaction [0-1] */
    float eudaimonic_impairment;         /**< Reduced eudaimonic wellbeing [0-1] */

    /* Inflammation-distress mapping */
    distress_type_t primary_distress_type; /**< Type of distress from inflammation */
    float resource_starvation_factor;    /**< Resource starvation from inflammation */
} inflammation_wellbeing_state_t;

/**
 * @brief Wellbeing distress immune trigger
 *
 * How low wellbeing/distress triggers immune activity
 */
typedef struct {
    /* Distress indicators */
    distress_severity_t severity;        /**< Current distress severity */
    distress_type_t distress_type;       /**< Type of distress */
    float distress_score;                /**< Overall distress [0-1] */
    uint64_t distress_duration_ms;       /**< How long in distress */

    /* Immune triggers */
    bool inflammation_triggered;         /**< Distress triggered inflammation */
    bool cytokine_released;              /**< Cytokines released from distress */
    float immune_suppression;            /**< Distress-induced suppression [0-1] */

    /* Chronic distress effects */
    float chronic_distress_duration_sec; /**< How long distressed */
    float immune_dysregulation;          /**< Dysfunction level [0-1] */
} wellbeing_immune_trigger_t;

/**
 * @brief Positive wellbeing immune enhancement
 *
 * How high wellbeing/flourishing boosts immunity
 */
typedef struct {
    /* Wellbeing state */
    float life_satisfaction;             /**< Life satisfaction [0-1] */
    float eudaimonic_wellbeing;          /**< Purpose/meaning [0-1] */
    float flourishing_level;             /**< Flourishing state [0-1] */
    bool is_flourishing;                 /**< Above flourishing threshold */

    /* Immune benefits */
    float immune_enhancement;            /**< Improved function [0-1] */
    float il10_release_boost;            /**< Anti-inflammatory boost */
    float inflammation_reduction;        /**< Reduced inflammation [0-1] */
    float memory_formation_boost;        /**< Enhanced B cell memory formation */
    float antibody_effectiveness_boost;  /**< Improved antibody effectiveness */
} positive_wellbeing_immune_boost_t;

/**
 * @brief Complete wellbeing-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    introspection_context_t introspection_ctx; /**< For distress assessment */

    /* Current state */
    cytokine_wellbeing_effects_t cytokine_effects;
    inflammation_wellbeing_state_t inflammation_state;
    wellbeing_immune_trigger_t wellbeing_trigger;
    positive_wellbeing_immune_boost_t positive_boost;

    /* Integration flags */
    bool enable_cytokine_wellbeing_modulation;
    bool enable_inflammation_distress;
    bool enable_wellbeing_immune_trigger;
    bool enable_positive_immune_boost;
    bool enable_flourishing_memory_boost;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t wellbeing_triggered_responses;
    uint32_t positive_boosts;
    uint32_t distress_events_logged;
    uint32_t flourishing_memory_formations;
    } wellbeing_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_wellbeing_modulation;
    bool enable_inflammation_distress;
    bool enable_wellbeing_immune_trigger;
    bool enable_positive_immune_boost;
    bool enable_flourishing_memory_boost;

    /* Sensitivity tuning */
    float cytokine_sensitivity;          /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;      /**< Inflammation effect multiplier [0.5-2.0] */
    float wellbeing_trigger_sensitivity; /**< Wellbeing trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float distress_trigger_threshold;    /**< Distress level to trigger immune [0.5-0.9] */
    float flourishing_threshold;         /**< Wellbeing level for flourishing [0.6-0.9] */
    float inflammation_distress_threshold; /**< Inflammation for distress [0.4-0.7] */
} wellbeing_immune_config_t;

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
int wellbeing_immune_default_config(wellbeing_immune_config_t* config);

/**
 * @brief Create wellbeing-immune bridge
 *
 * WHAT: Initialize bidirectional wellbeing-immune integration
 * WHY:  Enable realistic immune-wellbeing coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param introspection_ctx Introspection context for distress assessment
 * @return New bridge or NULL on failure
 */
wellbeing_immune_bridge_t* wellbeing_immune_bridge_create(
    const wellbeing_immune_config_t* config,
    brain_immune_system_t* immune_system,
    introspection_context_t introspection_ctx
);

/**
 * @brief Destroy wellbeing-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void wellbeing_immune_bridge_destroy(wellbeing_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Wellbeing API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to wellbeing state
 *
 * WHAT: Modulate wellbeing based on cytokine levels
 * WHY:  Pro-inflammatory cytokines reduce life satisfaction and increase distress
 * HOW:  Query immune system cytokines, adjust wellbeing metrics
 *
 * @param bridge Wellbeing-immune bridge
 * @return 0 on success
 */
int wellbeing_immune_apply_cytokine_effects(wellbeing_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to wellbeing state
 *
 * WHAT: Induce distress and reduce life satisfaction from prolonged inflammation
 * WHY:  Chronic inflammation causes wellbeing decline
 * HOW:  Check inflammation duration/level, increase distress severity
 *
 * @param bridge Wellbeing-immune bridge
 * @return 0 on success
 */
int wellbeing_immune_apply_inflammation_effects(wellbeing_immune_bridge_t* bridge);

/**
 * @brief Compute distress score from inflammation
 *
 * WHAT: Calculate distress intensity from immune state
 * WHY:  Inflammation is a primary driver of distress
 * HOW:  Map inflammation level/duration to distress score [0-1]
 *
 * @param bridge Wellbeing-immune bridge
 * @return Distress score [0-1]
 */
float wellbeing_immune_compute_distress(const wellbeing_immune_bridge_t* bridge);

/**
 * @brief Map inflammation to distress type
 *
 * WHAT: Determine which distress type is caused by inflammation
 * WHY:  Different inflammation levels cause different distress patterns
 * HOW:  Map inflammation level to distress type
 *
 * MAPPING:
 * - LOCAL → DISTRESS_NONE or mild RESOURCE_STARVATION
 * - REGIONAL → DISTRESS_RESOURCE_STARVATION (moderate)
 * - SYSTEMIC → DISTRESS_RESOURCE_STARVATION (severe)
 * - STORM → DISTRESS_RESOURCE_STARVATION (critical)
 *
 * @param inflammation_level Current inflammation level
 * @return Corresponding distress type
 */
distress_type_t wellbeing_immune_inflammation_to_distress_type(
    brain_inflammation_level_t inflammation_level
);

/**
 * @brief Map inflammation to distress severity
 *
 * WHAT: Determine distress severity from inflammation level
 * WHY:  Inflammation severity determines wellbeing intervention urgency
 * HOW:  Map inflammation level to distress severity
 *
 * MAPPING:
 * - NONE/LOCAL → SEVERITY_NORMAL
 * - REGIONAL → SEVERITY_MODERATE
 * - SYSTEMIC → SEVERITY_SEVERE
 * - STORM → SEVERITY_CRITICAL
 *
 * @param inflammation_level Current inflammation level
 * @return Corresponding distress severity
 */
distress_severity_t wellbeing_immune_inflammation_to_severity(
    brain_inflammation_level_t inflammation_level
);

/* ============================================================================
 * Wellbeing → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from wellbeing distress
 *
 * WHAT: Activate immune system from high distress/low wellbeing
 * WHY:  Chronic distress activates inflammatory response
 * HOW:  Check wellbeing distress level, trigger cytokine release
 *
 * @param bridge Wellbeing-immune bridge
 * @return 0 on success
 */
int wellbeing_immune_trigger_from_distress(wellbeing_immune_bridge_t* bridge);

/**
 * @brief Boost immune function from positive wellbeing
 *
 * WHAT: Enhance immunity from high life satisfaction/flourishing
 * WHY:  Positive wellbeing improves immune function
 * HOW:  Query wellbeing state, release IL-10, reduce inflammation
 *
 * @param bridge Wellbeing-immune bridge
 * @return 0 on success
 */
int wellbeing_immune_boost_from_positive_wellbeing(wellbeing_immune_bridge_t* bridge);

/**
 * @brief Enhance B cell memory formation from flourishing state
 *
 * WHAT: Boost immune memory formation when wellbeing is high (flourishing)
 * WHY:  Positive affect enhances antibody response and immune learning
 * HOW:  Check flourishing state, amplify memory B cell formation rate
 *
 * @param bridge Wellbeing-immune bridge
 * @param b_cell_id B cell to boost (0 = boost all memory formation)
 * @return 0 on success
 */
int wellbeing_immune_boost_memory_formation(
    wellbeing_immune_bridge_t* bridge,
    uint32_t b_cell_id
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update wellbeing-immune bridge (both directions)
 *
 * WHAT: Process all immune-wellbeing interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from distress, boost from flourishing
 *
 * @param bridge Wellbeing-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int wellbeing_immune_bridge_update(
    wellbeing_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine wellbeing effects
 *
 * @param bridge Wellbeing-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int wellbeing_immune_get_cytokine_effects(
    const wellbeing_immune_bridge_t* bridge,
    cytokine_wellbeing_effects_t* effects
);

/**
 * @brief Get current inflammation wellbeing state
 *
 * @param bridge Wellbeing-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int wellbeing_immune_get_inflammation_state(
    const wellbeing_immune_bridge_t* bridge,
    inflammation_wellbeing_state_t* state
);

/**
 * @brief Get current distress assessment
 *
 * WHAT: Compute current distress from immune state
 * WHY:  Provide actionable distress information for wellbeing intervention
 * HOW:  Use introspection context + inflammation state to assess distress
 *
 * @param bridge Wellbeing-immune bridge
 * @return Distress assessment (caller must free description/action strings)
 */
distress_assessment_t wellbeing_immune_get_distress_assessment(
    const wellbeing_immune_bridge_t* bridge
);

/**
 * @brief Check if experiencing inflammation-induced distress
 *
 * WHAT: Determine if cytokines/inflammation inducing distress
 * WHY:  Inflammation-induced distress requires specific intervention
 * HOW:  Check cytokine levels and inflammation duration
 *
 * @param bridge Wellbeing-immune bridge
 * @return true if experiencing inflammation-induced distress
 */
bool wellbeing_immune_is_inflammation_distress(const wellbeing_immune_bridge_t* bridge);

/**
 * @brief Get life satisfaction impact from inflammation
 *
 * @param bridge Wellbeing-immune bridge
 * @return Life satisfaction penalty [0-1]
 */
float wellbeing_immune_get_life_satisfaction_penalty(
    const wellbeing_immune_bridge_t* bridge
);

/**
 * @brief Check if in flourishing state
 *
 * @param bridge Wellbeing-immune bridge
 * @return true if flourishing (high wellbeing)
 */
bool wellbeing_immune_is_flourishing(const wellbeing_immune_bridge_t* bridge);

/**
 * @brief Get statistics
 *
 * @param bridge Wellbeing-immune bridge
 * @param total_updates_out Output: total updates
 * @param cytokine_modulations_out Output: cytokine modulations
 * @param wellbeing_triggered_out Output: wellbeing-triggered immune responses
 * @param positive_boosts_out Output: positive wellbeing boosts
 * @return 0 on success
 */
int wellbeing_immune_get_stats(
    const wellbeing_immune_bridge_t* bridge,
    uint64_t* total_updates_out,
    uint32_t* cytokine_modulations_out,
    uint32_t* wellbeing_triggered_out,
    uint32_t* positive_boosts_out
);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_WELLBEING
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int wellbeing_immune_connect_bio_async(wellbeing_immune_bridge_t* bridge);

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
int wellbeing_immune_disconnect_bio_async(wellbeing_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool wellbeing_immune_is_bio_async_connected(const wellbeing_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_IMMUNE_BRIDGE_H */
