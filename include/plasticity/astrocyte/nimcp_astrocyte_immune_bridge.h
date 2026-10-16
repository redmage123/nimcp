/**
 * @file nimcp_astrocyte_immune_bridge.h
 * @brief Astrocyte-Immune System Integration Bridge (Reactive Astrogliosis)
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between brain immune system and astrocyte plasticity
 * WHY:  Pro-inflammatory cytokines trigger reactive astrogliosis, impairing synaptic function.
 *       Anti-inflammatory signals restore neuroprotective astrocyte phenotype.
 * HOW:  Cytokines modulate astrocyte reactive state (A1/A2), affecting D-serine release,
 *       glutamate uptake, and gliotransmitter production. Astrocytes signal immune activation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → ASTROCYTE PATHWAYS (REACTIVE ASTROGLIOSIS):
 * -----------------------------------------------------
 * 1. Pro-inflammatory Cytokines Induce A1 Reactive State:
 *    - IL-1β, TNF-α, IL-6 activate astrocytes to A1 neurotoxic phenotype
 *    - A1 astrocytes: ↓D-serine production (40-60% reduction)
 *    - A1 astrocytes: ↓Glutamate uptake (50% impairment)
 *    - A1 astrocytes: Release neurotoxic factors
 *    - Chronic A1 state → synaptic loss and neurodegeneration
 *    Reference: Liddelow et al. (2017) "Neurotoxic reactive astrocytes"
 *               Vezzani et al. (2011) "Astrocyte activation in epilepsy"
 *
 * 2. IL-1β-Specific Effects:
 *    - Reduces D-serine release by 50%
 *    - Impairs NMDA-dependent LTP by 40%
 *    - Increases gliotransmitter release variability
 *    - Disrupts tripartite synapse function
 *    Reference: Sama et al. (2008) "IL-1β modulates astrocyte function"
 *
 * 3. TNF-α Effects on Astrocytes:
 *    - Reduces glutamate transporter (GLT-1) expression
 *    - Impairs glutamate uptake by 60%
 *    - Increases risk of excitotoxicity
 *    - Promotes astrocyte proliferation (gliosis)
 *    Reference: Bezzi et al. (2001) "TNF-α triggers glutamate release"
 *
 * 4. IL-6 and Astrocyte Activation:
 *    - Moderate A1 activation
 *    - Increases astrocyte calcium oscillations
 *    - Alters ATP/adenosine balance
 *    Reference: Gruol (2016) "IL-6 regulation of synaptic function"
 *
 * 5. Anti-inflammatory IL-10 Induces A2 Neuroprotective State:
 *    - IL-10 promotes A2 reactive phenotype
 *    - A2 astrocytes: Support synaptogenesis
 *    - A2 astrocytes: Enhanced glutamate uptake
 *    - A2 astrocytes: Normal or elevated D-serine
 *    - Counteracts A1 neurotoxicity
 *    Reference: Norden et al. (2016) "IL-10 and neuroprotection"
 *
 * 6. IFN-γ (BFT Quarantine Cytokine):
 *    - Activates astrocytes to mixed A1/A2 state
 *    - Enhances antigen presentation
 *    - Moderate glutamate uptake impairment
 *    - Involved in neuroinflammatory quarantine
 *
 * ASTROCYTE → IMMUNE PATHWAYS:
 * -----------------------------
 * 1. Astrocyte Calcium Waves Signal Network Distress:
 *    - Excessive calcium waves indicate hyperactivity
 *    - Triggers immune surveillance
 *    - ATP release propagates danger signals
 *
 * 2. Glutamate Uptake Failure Detection:
 *    - Severely impaired uptake (<50%) → alert immune system
 *    - Risk of excitotoxicity
 *    - Trigger neuroprotective A2 response
 *
 * 3. D-Serine Depletion Alert:
 *    - Critical D-serine levels (<0.4) → synaptic dysfunction risk
 *    - Signal need for anti-inflammatory intervention
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║            ASTROCYTE-IMMUNE BRIDGE (REACTIVE ASTROGLIOSIS)                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE → ASTROCYTE PATHWAYS                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -50% D-serine, -30% uptake                             │  ║
 * ║   │   │ TNF-α → -60% uptake, A1 activation                             │  ║
 * ║   │   │ IL-6  → -25% D-serine, Ca²⁺ dysregulation                      │  ║
 * ║   │   │ IFN-γ → -40% uptake (quarantine)                               │  ║
 * ║   │   │              │                                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │          │                                                         │  ║
 * ║   │          ▼                                                         │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     REACTIVE STATE TRANSITION   │                             │  ║
 * ║   │   │  - RESTING → A1 (neurotoxic)    │                             │  ║
 * ║   │   │  - RESTING → A2 (neuroprotect)  │                             │  ║
 * ║   │   │  - A1 ←→ A2 (IL-10 transition)  │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │          │                                                         │  ║
 * ║   │          ▼                                                         │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │   IL-10      │                                                 │  ║
 * ║   │   │ Anti-inflam  │  ───────┐                                       │  ║
 * ║   │   │   +20% D-serine, +15% uptake    │                             │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              ASTROCYTE → IMMUNE PATHWAYS                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────┐                                         │  ║
 * ║   │   │  EXCESSIVE Ca²⁺ WAVES│ ──→ Network Hyperactivity Alert         │  ║
 * ║   │   │  GLU UPTAKE < 50%    │ ──→ Excitotoxicity Risk Alert           │  ║
 * ║   │   │  D-SERINE < 0.4      │ ──→ Synaptic Dysfunction Alert          │  ║
 * ║   │   └──────────────────────┘                                         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   INFLAMMATION LEVEL → ASTROCYTE EFFECTS:                                 ║
 * ║   ────────────────────────────────────────────────────────────────────    ║
 * ║   NONE:      D-serine 1.0x,  Glu uptake 1.0x   (Resting)                 ║
 * ║   LOCAL:     D-serine 0.9x,  Glu uptake 0.95x  (Mild activation)         ║
 * ║   REGIONAL:  D-serine 0.7x,  Glu uptake 0.75x  (A1 transition)           ║
 * ║   SYSTEMIC:  D-serine 0.5x,  Glu uptake 0.50x  (Strong A1)               ║
 * ║   STORM:     D-serine 0.3x,  Glu uptake 0.30x  (Severe A1)               ║
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

#ifndef NIMCP_ASTROCYTE_IMMUNE_BRIDGE_H
#define NIMCP_ASTROCYTE_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/astrocyte/nimcp_astrocyte_plasticity.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine effects on D-serine production */
#define CYTOKINE_IL1_D_SERINE_REDUCTION     0.50f   /**< IL-1β reduces D-serine to 50% */
#define CYTOKINE_IL6_D_SERINE_REDUCTION     0.75f   /**< IL-6 reduces D-serine to 75% */
#define CYTOKINE_TNF_D_SERINE_REDUCTION     0.60f   /**< TNF-α reduces D-serine to 60% */
#define CYTOKINE_IFN_GAMMA_D_SERINE_REDUCTION 0.70f /**< IFN-γ reduces D-serine to 70% */
#define CYTOKINE_IL10_D_SERINE_RESTORATION  1.20f   /**< IL-10 restores D-serine to 120% */

/* Cytokine effects on glutamate uptake */
#define CYTOKINE_IL1_GLU_UPTAKE_IMPAIRMENT  0.70f   /**< IL-1β reduces uptake to 70% */
#define CYTOKINE_IL6_GLU_UPTAKE_IMPAIRMENT  0.80f   /**< IL-6 reduces uptake to 80% */
#define CYTOKINE_TNF_GLU_UPTAKE_IMPAIRMENT  0.40f   /**< TNF-α reduces uptake to 40% */
#define CYTOKINE_IFN_GAMMA_GLU_UPTAKE_IMPAIRMENT 0.60f /**< IFN-γ reduces uptake to 60% */
#define CYTOKINE_IL10_GLU_UPTAKE_RESTORATION 1.15f  /**< IL-10 restores uptake to 115% */

/* Inflammation-based astrocyte modulation */
#define INFLAMMATION_ASTRO_NONE_D_SERINE      1.00f  /**< No inflammation */
#define INFLAMMATION_ASTRO_LOCAL_D_SERINE     0.90f  /**< Local: slight reduction */
#define INFLAMMATION_ASTRO_REGIONAL_D_SERINE  0.70f  /**< Regional: A1 transition */
#define INFLAMMATION_ASTRO_SYSTEMIC_D_SERINE  0.50f  /**< Systemic: strong A1 */
#define INFLAMMATION_ASTRO_STORM_D_SERINE     0.30f  /**< Storm: severe A1 */

#define INFLAMMATION_ASTRO_NONE_GLU_UPTAKE     1.00f
#define INFLAMMATION_ASTRO_LOCAL_GLU_UPTAKE    0.95f
#define INFLAMMATION_ASTRO_REGIONAL_GLU_UPTAKE 0.75f
#define INFLAMMATION_ASTRO_SYSTEMIC_GLU_UPTAKE 0.50f
#define INFLAMMATION_ASTRO_STORM_GLU_UPTAKE    0.30f

/* Astrocyte dysfunction detection thresholds */
#define ASTROCYTE_GLU_UPTAKE_CRITICAL_THRESHOLD  0.50f  /**< <50% uptake → alert */
#define ASTROCYTE_D_SERINE_CRITICAL_THRESHOLD    0.40f  /**< <0.4 D-serine → alert */
#define ASTROCYTE_CA_WAVE_EXCESSIVE_THRESHOLD    0.80f  /**< >0.8 calcium → alert */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on astrocyte function
 *
 * How cytokine levels modulate gliotransmitter production and uptake
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_d_serine_reduction;      /**< IL-1β D-serine impairment */
    float il6_d_serine_reduction;      /**< IL-6 D-serine impairment */
    float tnf_d_serine_reduction;      /**< TNF-α D-serine impairment */
    float ifn_gamma_d_serine_reduction; /**< IFN-γ D-serine impairment */

    float il1_glu_uptake_impairment;   /**< IL-1β uptake impairment */
    float il6_glu_uptake_impairment;   /**< IL-6 uptake impairment */
    float tnf_glu_uptake_impairment;   /**< TNF-α uptake impairment */
    float ifn_gamma_glu_uptake_impairment; /**< IFN-γ uptake impairment */

    /* Anti-inflammatory effects */
    float il10_d_serine_restoration;   /**< IL-10 D-serine restoration */
    float il10_glu_uptake_restoration; /**< IL-10 uptake restoration */

    /* Aggregate effects */
    float total_d_serine_modulation;   /**< Combined D-serine scaling [0-1.5] */
    float total_glu_uptake_modulation; /**< Combined uptake scaling [0-1.5] */
    float reactive_state_intensity;    /**< A1/A2 intensity [0-1] */
} cytokine_astrocyte_effects_t;

/**
 * @brief Inflammation effects on astrocyte function
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;
    bool is_chronic;

    /* Astrocyte impacts */
    float d_serine_reduction;          /**< D-serine production reduction [0-1] */
    float glu_uptake_impairment;       /**< Uptake efficiency impairment [0-1] */
    float calcium_dysregulation;       /**< Calcium signaling disruption [0-1] */
    float atp_release_alteration;      /**< ATP/adenosine balance change [-1,1] */

    /* Reactive state */
    astrocyte_reactive_state_t target_state;
    float a1_transition_progress;      /**< Progress to A1 state [0-1] */
    float a2_transition_progress;      /**< Progress to A2 state [0-1] */
} inflammation_astrocyte_state_t;

/**
 * @brief Astrocyte dysfunction detection
 *
 * Monitoring astrocyte health for immune alerting
 */
typedef struct {
    /* Function state */
    float current_glu_uptake;          /**< Current uptake rate */
    float current_d_serine;            /**< Current D-serine level */
    float calcium_wave_frequency;      /**< Current Ca²⁺ wave frequency */

    /* Dysfunction flags */
    bool glu_uptake_critical;          /**< Severe uptake impairment */
    bool d_serine_depleted;            /**< Critical D-serine depletion */
    bool calcium_excessive;            /**< Excessive calcium activity */
    bool excitotoxicity_risk;          /**< High excitotoxicity risk */

    /* Severity */
    float dysfunction_severity;        /**< Overall dysfunction [0-1] */
} astrocyte_dysfunction_state_t;

/**
 * @brief Astrocyte-immune bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_modulation;
    bool enable_inflammation_effects;
    bool enable_dysfunction_detection;
    bool enable_reactive_state_control;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float dysfunction_sensitivity;     /**< Dysfunction detection multiplier [0.5-2.0] */

    /* Thresholds */
    float glu_uptake_critical_threshold;
    float d_serine_critical_threshold;
    float ca_wave_excessive_threshold;
} astrocyte_immune_config_t;

/**
 * @brief Complete astrocyte-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    astrocyte_plasticity_t astrocyte_system;

    /* Current state */
    cytokine_astrocyte_effects_t cytokine_effects;
    inflammation_astrocyte_state_t inflammation_state;
    astrocyte_dysfunction_state_t dysfunction_state;

    /* Configuration */
    astrocyte_immune_config_t config;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t dysfunction_alerts;
    uint32_t reactive_state_transitions;

    } astrocyte_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default astrocyte-immune bridge configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int astrocyte_immune_default_config(astrocyte_immune_config_t* config);

/**
 * @brief Create astrocyte-immune bridge
 *
 * WHAT: Initialize bidirectional astrocyte-immune integration
 * WHY:  Enable realistic reactive astrogliosis modeling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param astrocyte_system Astrocyte plasticity system
 * @return New bridge or NULL on failure
 */
astrocyte_immune_bridge_t* astrocyte_immune_bridge_create(
    const astrocyte_immune_config_t* config,
    brain_immune_system_t* immune_system,
    astrocyte_plasticity_t astrocyte_system
);

/**
 * @brief Destroy astrocyte-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void astrocyte_immune_bridge_destroy(astrocyte_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Astrocyte API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to astrocyte function
 *
 * WHAT: Modulate astrocytes based on cytokine levels
 * WHY:  Pro-inflammatory cytokines induce A1 reactive state
 * HOW:  Query immune cytokines, adjust D-serine and uptake
 *
 * @param bridge Astrocyte-immune bridge
 * @return 0 on success
 */
int astrocyte_immune_apply_cytokine_effects(astrocyte_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to astrocytes
 *
 * WHAT: Trigger reactive astrogliosis from inflammation
 * WHY:  Chronic inflammation causes persistent A1 state
 * HOW:  Check inflammation level/duration, transition reactive state
 *
 * @param bridge Astrocyte-immune bridge
 * @return 0 on success
 */
int astrocyte_immune_apply_inflammation_effects(astrocyte_immune_bridge_t* bridge);

/**
 * @brief Transition astrocyte to reactive state based on cytokines
 *
 * WHAT: Set astrocyte A1/A2 reactive phenotype
 * WHY:  IL-1β/TNF-α → A1, IL-10 → A2
 * HOW:  Determine dominant cytokine, call astrocyte_plasticity_set_reactive_state
 *
 * @param bridge Astrocyte-immune bridge
 * @param astrocyte_id Which astrocyte to transition
 * @return 0 on success
 */
int astrocyte_immune_transition_reactive_state(
    astrocyte_immune_bridge_t* bridge,
    uint32_t astrocyte_id
);

/* ============================================================================
 * Astrocyte → Immune API
 * ============================================================================ */

/**
 * @brief Detect astrocyte dysfunction
 *
 * WHAT: Check for critical astrocyte function impairment
 * WHY:  Severe uptake failure or D-serine depletion threatens neurons
 * HOW:  Monitor glutamate uptake, D-serine, calcium waves
 *
 * @param bridge Astrocyte-immune bridge
 * @return 0 on success
 */
int astrocyte_immune_detect_dysfunction(astrocyte_immune_bridge_t* bridge);

/**
 * @brief Alert immune system of astrocyte dysfunction
 *
 * WHAT: Notify immune system of astrocyte failure
 * WHY:  Dysfunction threatens neuronal health
 * HOW:  Create antigen from dysfunction signature
 *
 * @param bridge Astrocyte-immune bridge
 * @param antigen_id Output: created antigen ID
 * @return 0 on success
 */
int astrocyte_immune_alert_dysfunction(
    astrocyte_immune_bridge_t* bridge,
    uint32_t* antigen_id
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update astrocyte-immune bridge (both directions)
 *
 * WHAT: Process all astrocyte-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine/inflammation effects, detect dysfunction
 *
 * @param bridge Astrocyte-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int astrocyte_immune_bridge_update(
    astrocyte_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine effects on astrocytes
 *
 * @param bridge Astrocyte-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int astrocyte_immune_get_cytokine_effects(
    const astrocyte_immune_bridge_t* bridge,
    cytokine_astrocyte_effects_t* effects
);

/**
 * @brief Get current inflammation state effects
 *
 * @param bridge Astrocyte-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int astrocyte_immune_get_inflammation_state(
    const astrocyte_immune_bridge_t* bridge,
    inflammation_astrocyte_state_t* state
);

/**
 * @brief Get current dysfunction state
 *
 * @param bridge Astrocyte-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int astrocyte_immune_get_dysfunction_state(
    const astrocyte_immune_bridge_t* bridge,
    astrocyte_dysfunction_state_t* state
);

/**
 * @brief Check if astrocyte function is impaired by inflammation
 *
 * @param bridge Astrocyte-immune bridge
 * @return true if impaired
 */
bool astrocyte_immune_is_function_impaired(const astrocyte_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_ASTROCYTE
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int astrocyte_immune_connect_bio_async(astrocyte_immune_bridge_t* bridge);

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
int astrocyte_immune_disconnect_bio_async(astrocyte_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool astrocyte_immune_is_bio_async_connected(const astrocyte_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ASTROCYTE_IMMUNE_BRIDGE_H */
