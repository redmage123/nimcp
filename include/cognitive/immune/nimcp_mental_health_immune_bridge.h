/**
 * @file nimcp_mental_health_immune_bridge.h
 * @brief Mental Health-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and mental health monitoring
 * WHY:  Biological evidence shows strong immune-mental health coupling (cytokine theory
 *       of depression). Essential for realistic brain modeling of mental disorders.
 * HOW:  Cytokines modulate neurotransmitters and disorder risk, chronic inflammation
 *       causes depression/anxiety, mental disorders trigger inflammatory responses,
 *       recovery/treatment reduces inflammation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → MENTAL HEALTH PATHWAYS:
 * ---------------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Reduce serotonin synthesis → Depression
 *    - Reduce dopamine → Anhedonia, amotivation
 *    - Activate HPA axis → Anxiety, stress response
 *    - Reference: Miller & Raison (2016) "The role of inflammation in depression"
 *
 * 2. Chronic Inflammation:
 *    - Sustained elevation → Major Depression (MDD)
 *    - Increases risk of anxiety disorders
 *    - Impairs executive function
 *    - Reference: Dantzer et al. (2008) "From inflammation to sickness and depression"
 *
 * 3. Cytokine Storm:
 *    - Can trigger acute psychosis
 *    - Severe anxiety/panic
 *    - Cognitive dysfunction
 *    - Reference: Mehta et al. (2020) "COVID-19: cytokine storm and neuropsychiatric effects"
 *
 * MENTAL HEALTH → IMMUNE PATHWAYS:
 * ---------------------------------
 * 1. Depression:
 *    - Elevated IL-6, TNF-α, CRP
 *    - Dysregulated immune function
 *    - Increased inflammation
 *    - Reference: Howren et al. (2009) "Depression and inflammatory markers meta-analysis"
 *
 * 2. Anxiety:
 *    - HPA axis activation → Cortisol → Immune suppression initially
 *    - Followed by inflammatory rebound
 *    - Reference: Rohleder (2019) "Stress and inflammation"
 *
 * 3. PTSD:
 *    - Chronic inflammatory state
 *    - Elevated pro-inflammatory cytokines
 *    - Reference: Passos et al. (2015) "Inflammatory markers in PTSD"
 *
 * 4. Recovery/Treatment:
 *    - Successful treatment reduces inflammation
 *    - Anti-inflammatory drugs show antidepressant effects
 *    - Reference: Köhler et al. (2014) "NSAIDs and depression treatment"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║               MENTAL HEALTH-IMMUNE BRIDGE                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │             IMMUNE → MENTAL HEALTH PATHWAYS                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β        │  ───────┐                                       │  ║
 * ║   │   │ IL-6         │         │                                       │  ║
 * ║   │   │ TNF-α        │         ├──→ Depression Risk ↑                  │  ║
 * ║   │   │ IFN-γ        │         │    Anxiety Risk ↑                     │  ║
 * ║   │   └──────────────┘         │    Serotonin/Dopamine ↓               │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │  MENTAL HEALTH SYSTEM           │                             │  ║
 * ║   │   │  - Disorder risk modulation     │                             │  ║
 * ║   │   │  - Neurotransmitter levels      │                             │  ║
 * ║   │   │  - Behavioral marker updates    │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │              │     Recovery, Risk Reduction                    │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │          MENTAL HEALTH → IMMUNE PATHWAYS                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  DEPRESSION  │ ──→ ↑ IL-6, TNF-α, Chronic Inflammation        │  ║
 * ║   │   │  ANXIETY     │ ──→ ↑ Cortisol → Immune Suppression/Rebound    │  ║
 * ║   │   │  PTSD        │ ──→ ↑ Pro-inflammatory State                   │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  RECOVERY    │ ──→ ↓ Inflammation, ↑ IL-10                    │  ║
 * ║   │   │  TREATMENT   │ ──→ Immune Normalization                       │  ║
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

#ifndef NIMCP_MENTAL_HEALTH_IMMUNE_BRIDGE_H
#define NIMCP_MENTAL_HEALTH_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_mental_health.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine mental health impact factors (disorder risk modulation) */
#define CYTOKINE_IL1_DEPRESSION_RISK      0.3f   /**< IL-1β → depression risk increase */
#define CYTOKINE_IL6_DEPRESSION_RISK      0.25f  /**< IL-6 → depression risk increase */
#define CYTOKINE_TNF_DEPRESSION_RISK      0.4f   /**< TNF-α → strong depression risk */
#define CYTOKINE_IFN_GAMMA_DEPRESSION_RISK 0.2f  /**< IFN-γ → mild depression risk */
#define CYTOKINE_IL10_RECOVERY_BENEFIT    -0.3f  /**< IL-10 → recovery (negative = reduces risk) */

/* Cytokine anxiety risk factors */
#define CYTOKINE_IL1_ANXIETY_RISK         0.25f  /**< IL-1β → anxiety risk increase */
#define CYTOKINE_IL6_ANXIETY_RISK         0.3f   /**< IL-6 → anxiety risk increase */
#define CYTOKINE_TNF_ANXIETY_RISK         0.2f   /**< TNF-α → anxiety risk increase */

/* Inflammation-disorder mapping */
#define INFLAMMATION_DEPRESSION_THRESHOLD  0.5f   /**< Inflammation level for depression risk onset */
#define INFLAMMATION_ANXIETY_THRESHOLD     0.4f   /**< Inflammation level for anxiety risk onset */
#define INFLAMMATION_PSYCHOSIS_THRESHOLD   0.8f   /**< Inflammation level for psychosis risk (storm) */

/* Mental disorder immune trigger thresholds */
#define DEPRESSION_IMMUNE_TRIGGER_THRESHOLD  0.6f  /**< Depression severity to trigger immune response */
#define ANXIETY_IMMUNE_TRIGGER_THRESHOLD     0.5f  /**< Anxiety severity to trigger immune response */
#define PTSD_IMMUNE_TRIGGER_THRESHOLD        0.7f  /**< PTSD severity to trigger immune response */

/* Chronic inflammation duration (seconds) */
#define CHRONIC_MH_INFLAMMATION_THRESHOLD    (86400.0f * 14)  /**< 14 days = chronic (depression risk) */

/* Recovery immune benefit */
#define RECOVERY_IL10_BOOST                  0.4f   /**< IL-10 release on successful intervention */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine mental health effects
 *
 * Represents how cytokine levels modulate mental disorder risk
 */
typedef struct {
    /* Pro-inflammatory effects on depression */
    float il1_depression_risk;        /**< IL-1β induced depression risk */
    float il6_depression_risk;        /**< IL-6 induced depression risk */
    float tnf_depression_risk;        /**< TNF-α induced depression risk */
    float ifn_gamma_depression_risk;  /**< IFN-γ induced depression risk */

    /* Pro-inflammatory effects on anxiety */
    float il1_anxiety_risk;           /**< IL-1β induced anxiety risk */
    float il6_anxiety_risk;           /**< IL-6 induced anxiety risk */
    float tnf_anxiety_risk;           /**< TNF-α induced anxiety risk */

    /* Anti-inflammatory effects */
    float il10_recovery_benefit;      /**< IL-10 recovery/resilience effect */

    /* Aggregate effects */
    float total_depression_risk_shift; /**< Combined depression risk modulation */
    float total_anxiety_risk_shift;    /**< Combined anxiety risk modulation */
    float neurotransmitter_suppression; /**< Overall serotonin/dopamine suppression [0-1] */
} cytokine_mental_health_effects_t;

/**
 * @brief Inflammation mental health state
 *
 * How chronic inflammation affects mental health disorder risk
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 14 days */

    /* Mental health impacts */
    float depression_risk_multiplier;  /**< Depression risk multiplier [1.0-3.0] */
    float anxiety_risk_multiplier;     /**< Anxiety risk multiplier [1.0-2.5] */
    float psychosis_risk;              /**< Psychosis risk from cytokine storm [0-1] */
    float cognitive_impairment;        /**< Executive function impairment [0-1] */
    float serotonin_suppression;       /**< Serotonin reduction [0-1] */
    float dopamine_suppression;        /**< Dopamine reduction [0-1] */
} inflammation_mental_health_state_t;

/**
 * @brief Mental disorder immune response
 *
 * How mental disorders trigger immune activity
 */
typedef struct {
    /* Disorder indicators */
    float depression_severity;         /**< Current depression severity [0-1] */
    float anxiety_severity;            /**< Current anxiety severity [0-1] */
    float ptsd_severity;               /**< Current PTSD severity [0-1] */

    /* Immune triggers */
    bool depression_triggered;         /**< Depression triggered immune response */
    bool anxiety_triggered;            /**< Anxiety triggered immune response */
    bool ptsd_triggered;               /**< PTSD triggered immune response */

    /* Immune effects */
    float cortisol_activation;         /**< HPA axis activation [0-1] */
    float immune_suppression;          /**< Initial immune suppression [0-1] */
    float inflammatory_rebound;        /**< Post-stress inflammation [0-1] */
    float chronic_inflammation_level;  /**< Sustained inflammation [0-1] */
} mental_disorder_immune_trigger_t;

/**
 * @brief Recovery immune enhancement
 *
 * How successful interventions/recovery boost immunity
 */
typedef struct {
    /* Recovery state */
    bool recent_intervention;          /**< Intervention occurred recently */
    uint64_t intervention_time;        /**< When intervention occurred */
    float intervention_effectiveness;  /**< How effective was intervention [0-1] */

    /* Immune benefits */
    float immune_normalization;        /**< Immune function restoration [0-1] */
    float il10_release_boost;          /**< Anti-inflammatory boost */
    float inflammation_reduction;      /**< Reduced inflammation [0-1] */
    float stress_response_reduction;   /**< Reduced HPA activation [0-1] */
} recovery_immune_enhancement_t;

/**
 * @brief Complete mental health-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    mental_health_monitor_t* mental_health_monitor;

    /* Current state */
    cytokine_mental_health_effects_t cytokine_effects;
    inflammation_mental_health_state_t inflammation_state;
    mental_disorder_immune_trigger_t disorder_trigger;
    recovery_immune_enhancement_t recovery_boost;

    /* Integration flags */
    bool enable_cytokine_disorder_modulation;
    bool enable_inflammation_depression;
    bool enable_inflammation_anxiety;
    bool enable_disorder_immune_trigger;
    bool enable_recovery_immune_boost;
    bool enable_neurotransmitter_modulation;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t disorder_triggered_responses;
    uint32_t recovery_boosts;
    uint32_t depression_triggers;
    uint32_t anxiety_triggers;
    uint32_t ptsd_triggers;
    } mental_health_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_disorder_modulation;
    bool enable_inflammation_depression;
    bool enable_inflammation_anxiety;
    bool enable_disorder_immune_trigger;
    bool enable_recovery_immune_boost;
    bool enable_neurotransmitter_modulation;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float disorder_trigger_sensitivity; /**< Disorder trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float depression_trigger_threshold;  /**< Depression severity to trigger immune [0.5-0.9] */
    float anxiety_trigger_threshold;     /**< Anxiety severity to trigger immune [0.4-0.8] */
    float inflammation_depression_threshold; /**< Inflammation for depression risk [0.4-0.7] */
    float inflammation_anxiety_threshold;    /**< Inflammation for anxiety risk [0.3-0.6] */
} mental_health_immune_config_t;

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
int mental_health_immune_default_config(mental_health_immune_config_t* config);

/**
 * @brief Create mental health-immune bridge
 *
 * WHAT: Initialize bidirectional mental health-immune integration
 * WHY:  Enable realistic immune-mental health coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param mental_health_monitor Mental health monitoring system
 * @return New bridge or NULL on failure
 */
mental_health_immune_bridge_t* mental_health_immune_bridge_create(
    const mental_health_immune_config_t* config,
    brain_immune_system_t* immune_system,
    mental_health_monitor_t* mental_health_monitor
);

/**
 * @brief Destroy mental health-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void mental_health_immune_bridge_destroy(mental_health_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Mental Health API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to mental health disorder risk
 *
 * WHAT: Modulate disorder risk based on cytokine levels
 * WHY:  Pro-inflammatory cytokines increase depression/anxiety risk
 * HOW:  Query immune system cytokines, adjust disorder risk scores
 *
 * @param bridge Mental health-immune bridge
 * @return 0 on success
 */
int mental_health_immune_apply_cytokine_effects(mental_health_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to mental health state
 *
 * WHAT: Increase depression/anxiety risk from prolonged inflammation
 * WHY:  Chronic inflammation is major depression risk factor
 * HOW:  Check inflammation duration/level, increase disorder risk
 *
 * @param bridge Mental health-immune bridge
 * @return 0 on success
 */
int mental_health_immune_apply_inflammation_effects(mental_health_immune_bridge_t* bridge);

/**
 * @brief Modulate neurotransmitters based on inflammation
 *
 * WHAT: Reduce serotonin/dopamine levels from cytokine effects
 * WHY:  Cytokines impair neurotransmitter synthesis
 * HOW:  Map inflammation level to neurotransmitter suppression
 *
 * @param bridge Mental health-immune bridge
 * @return 0 on success
 */
int mental_health_immune_modulate_neurotransmitters(mental_health_immune_bridge_t* bridge);

/**
 * @brief Compute depression risk from inflammation
 *
 * WHAT: Calculate depression risk multiplier from immune state
 * WHY:  Inflammation is validated depression risk factor
 * HOW:  Map inflammation level/duration to risk multiplier [1.0-3.0]
 *
 * @param bridge Mental health-immune bridge
 * @return Depression risk multiplier [1.0-3.0]
 */
float mental_health_immune_compute_depression_risk(const mental_health_immune_bridge_t* bridge);

/**
 * @brief Compute anxiety risk from inflammation
 *
 * WHAT: Calculate anxiety risk multiplier from immune state
 * WHY:  Inflammation increases anxiety via multiple pathways
 * HOW:  Map inflammation level to risk multiplier [1.0-2.5]
 *
 * @param bridge Mental health-immune bridge
 * @return Anxiety risk multiplier [1.0-2.5]
 */
float mental_health_immune_compute_anxiety_risk(const mental_health_immune_bridge_t* bridge);

/* ============================================================================
 * Mental Health → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from depression
 *
 * WHAT: Activate immune system from severe depression
 * WHY:  Depression increases pro-inflammatory cytokines
 * HOW:  Check depression severity, trigger IL-6, TNF-α release
 *
 * @param bridge Mental health-immune bridge
 * @return 0 on success
 */
int mental_health_immune_trigger_from_depression(mental_health_immune_bridge_t* bridge);

/**
 * @brief Trigger immune response from anxiety
 *
 * WHAT: Activate immune system from severe anxiety
 * WHY:  Anxiety activates HPA axis and inflammatory response
 * HOW:  Check anxiety severity, trigger cortisol and inflammatory rebound
 *
 * @param bridge Mental health-immune bridge
 * @return 0 on success
 */
int mental_health_immune_trigger_from_anxiety(mental_health_immune_bridge_t* bridge);

/**
 * @brief Trigger immune response from PTSD
 *
 * WHAT: Activate immune system from PTSD
 * WHY:  PTSD creates chronic inflammatory state
 * HOW:  Check PTSD severity, trigger sustained inflammation
 *
 * @param bridge Mental health-immune bridge
 * @return 0 on success
 */
int mental_health_immune_trigger_from_ptsd(mental_health_immune_bridge_t* bridge);

/**
 * @brief Boost immune function from successful intervention
 *
 * WHAT: Enhance immunity from successful mental health intervention
 * WHY:  Recovery/treatment reduces inflammation
 * HOW:  Detect intervention, release IL-10, reduce inflammation
 *
 * @param bridge Mental health-immune bridge
 * @return 0 on success
 */
int mental_health_immune_boost_from_recovery(mental_health_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update mental health-immune bridge (both directions)
 *
 * WHAT: Process all immune-mental health interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from disorders, boost from recovery
 *
 * @param bridge Mental health-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int mental_health_immune_bridge_update(
    mental_health_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine mental health effects
 *
 * @param bridge Mental health-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int mental_health_immune_get_cytokine_effects(
    const mental_health_immune_bridge_t* bridge,
    cytokine_mental_health_effects_t* effects
);

/**
 * @brief Get current inflammation mental health state
 *
 * @param bridge Mental health-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int mental_health_immune_get_inflammation_state(
    const mental_health_immune_bridge_t* bridge,
    inflammation_mental_health_state_t* state
);

/**
 * @brief Check if experiencing cytokine-induced depression
 *
 * WHAT: Determine if cytokines are inducing depressive symptoms
 * WHY:  Cytokine-induced depression is distinct clinical entity
 * HOW:  Check cytokine levels and depression risk score
 *
 * @param bridge Mental health-immune bridge
 * @return true if experiencing cytokine-induced depression
 */
bool mental_health_immune_is_cytokine_depression(const mental_health_immune_bridge_t* bridge);

/**
 * @brief Get neurotransmitter suppression level
 *
 * @param bridge Mental health-immune bridge
 * @return Neurotransmitter suppression [0-1]
 */
float mental_health_immune_get_neurotransmitter_suppression(const mental_health_immune_bridge_t* bridge);

/**
 * @brief Get statistics
 *
 * @param bridge Mental health-immune bridge
 * @param total_updates Output: total updates
 * @param depression_triggers Output: depression-triggered immune responses
 * @param anxiety_triggers Output: anxiety-triggered immune responses
 * @return 0 on success
 */
int mental_health_immune_get_stats(
    const mental_health_immune_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* depression_triggers,
    uint32_t* anxiety_triggers
);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_MENTAL_HEALTH
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int mental_health_immune_connect_bio_async(mental_health_immune_bridge_t* bridge);

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
int mental_health_immune_disconnect_bio_async(mental_health_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool mental_health_immune_is_bio_async_connected(const mental_health_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MENTAL_HEALTH_IMMUNE_BRIDGE_H */
