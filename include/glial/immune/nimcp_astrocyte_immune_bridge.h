/**
 * @file nimcp_astrocyte_immune_bridge.h
 * @brief Astrocyte-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and astrocytes
 * WHY:  Astrocytes are critical for blood-brain barrier maintenance, inflammatory
 *       response modulation, and neuroinflammation. They undergo reactive changes
 *       during immune activation and regulate cytokine levels.
 * HOW:  Cytokines modulate astrocyte reactivity, calcium signaling, and glutamate
 *       clearance; reactive astrocytes release cytokines and modulate BBB permeability.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → ASTROCYTE PATHWAYS:
 * ---------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Trigger astrocyte reactive state (astrogliosis)
 *    - Increase calcium wave frequency and amplitude
 *    - Impair glutamate clearance (excitotoxicity risk)
 *    - Reduce metabolic support to neurons
 *    - Disrupt BBB tight junctions
 *    - Reference: Sofroniew & Vinters (2010) "Astrocytes: biology and pathology"
 *    - Reference: Liberto et al. (2004) "Pro-regenerative properties of cytokine-
 *      activated astrocytes"
 *
 * 2. Chronic Inflammation and Reactive Astrocytes:
 *    - Prolonged cytokine exposure → persistent reactivity
 *    - Glial scar formation (physical barrier)
 *    - Release of additional pro-inflammatory mediators
 *    - Impaired synaptic support
 *    - Reference: Burda & Sofroniew (2014) "Reactive gliosis and the multicellular
 *      response to CNS damage and disease"
 *
 * 3. Anti-inflammatory Cytokines (IL-10):
 *    - Promote resolution of astrocyte reactivity
 *    - Restore normal calcium dynamics
 *    - Enhance glutamate clearance
 *    - Restore BBB integrity
 *    - Reference: Lobo-Silva et al. (2016) "Balancing the immune response in the
 *      brain: IL-10 and its regulation"
 *
 * ASTROCYTE → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Reactive Astrocyte Cytokine Production:
 *    - Activated astrocytes release IL-1β, IL-6, TNF-α
 *    - Amplify local inflammatory response
 *    - Recruit microglia to injury site
 *    - Modulate T cell infiltration
 *    - Reference: Farina et al. (2007) "Astrocytes are active players in cerebral
 *      innate immunity"
 *
 * 2. Astrocyte-BBB Inflammation Coupling:
 *    - Astrocyte end-feet disruption → BBB permeability
 *    - Increased immune cell infiltration
 *    - Enhanced antigen presentation
 *    - Reference: Abbott et al. (2006) "Astrocyte-endothelial interactions at the
 *      blood-brain barrier"
 *
 * 3. Astrocyte Calcium Waves and Immune Signaling:
 *    - Calcium waves propagate inflammatory signals
 *    - Coordinate gliotransmitter release
 *    - Modulate local immune microenvironment
 *    - Reference: Cornell-Bell et al. (1990) "Glutamate induces calcium waves in
 *      cultured astrocytes: long-range glial signaling"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    ASTROCYTE-IMMUNE BRIDGE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → ASTROCYTE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → +0.5 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → +0.3 │         │                                       │  ║
 * ║   │   │ TNF-α → +0.6 │         ├──→ Astrocyte Reactivity               │  ║
 * ║   │   │ IL-10 → -0.4 │         │    (Astrogliosis, Calcium ↑)         │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     ASTROCYTE NETWORK           │                             │  ║
 * ║   │   │  - Reactivity state             │                             │  ║
 * ║   │   │  - Calcium wave frequency       │                             │  ║
 * ║   │   │  - Glutamate clearance          │                             │  ║
 * ║   │   │  - Metabolic support            │                             │  ║
 * ║   │   │  - BBB permeability             │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION LEVEL     │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ LOCAL    → +20% reactivity│                                    │  ║
 * ║   │   │ REGIONAL → +50% reactivity│                                    │  ║
 * ║   │   │ SYSTEMIC → +80% reactivity│                                    │  ║
 * ║   │   │ STORM    → Astrogliosis   │                                    │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  ASTROCYTE → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ REACTIVE STATE   │ ──→ IL-1β, IL-6, TNF-α Release              │  ║
 * ║   │   │ CALCIUM WAVES    │ ──→ Propagate Inflammatory Signals          │  ║
 * ║   │   │ BBB DISRUPTION   │ ──→ Enhanced Immune Cell Infiltration       │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ RESTING STATE    │ ──→ IL-10 Release (Homeostasis)             │  ║
 * ║   │   │ NORMAL CALCIUM   │ ──→ Regulatory Signaling                    │  ║
 * ║   │   └──────────────────┘                                             │  ║
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

#ifndef NIMCP_ASTRO_NETWORK_IMMUNE_BRIDGE_H
#define NIMCP_ASTRO_NETWORK_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine astrocyte reactivity impact factors */
#define CYTOKINE_IL1_REACTIVITY_IMPACT      0.5f    /**< IL-1β → reactivity boost */
#define CYTOKINE_IL6_REACTIVITY_IMPACT      0.3f    /**< IL-6 → reactivity boost */
#define CYTOKINE_TNF_REACTIVITY_IMPACT      0.6f    /**< TNF-α → strong reactivity */
#define CYTOKINE_IFN_GAMMA_REACTIVITY_IMPACT 0.25f  /**< IFN-γ → moderate reactivity */
#define CYTOKINE_IL10_REACTIVITY_IMPACT     -0.4f   /**< IL-10 → resolution */

/* Cytokine calcium modulation */
#define CYTOKINE_IL1_CALCIUM_BOOST          0.4f    /**< IL-1β → calcium ↑ */
#define CYTOKINE_IL6_CALCIUM_BOOST          0.25f   /**< IL-6 → calcium ↑ */
#define CYTOKINE_TNF_CALCIUM_BOOST          0.5f    /**< TNF-α → strong calcium ↑ */
#define CYTOKINE_IL10_CALCIUM_REDUCTION     -0.3f   /**< IL-10 → calcium ↓ */

/* Cytokine glutamate clearance impairment */
#define CYTOKINE_IL1_GLUTAMATE_IMPAIRMENT   -0.3f   /**< IL-1β → clearance ↓ */
#define CYTOKINE_IL6_GLUTAMATE_IMPAIRMENT   -0.2f   /**< IL-6 → clearance ↓ */
#define CYTOKINE_TNF_GLUTAMATE_IMPAIRMENT   -0.4f   /**< TNF-α → clearance ↓ */
#define CYTOKINE_IL10_GLUTAMATE_RECOVERY    0.3f    /**< IL-10 → clearance ↑ */

/* Inflammation astrocyte reactivity factors */
#define INFLAMMATION_NONE_REACTIVITY_FACTOR     0.0f    /**< No reactivity */
#define INFLAMMATION_LOCAL_REACTIVITY_FACTOR    0.2f    /**< +20% reactivity */
#define INFLAMMATION_REGIONAL_REACTIVITY_FACTOR 0.5f    /**< +50% reactivity */
#define INFLAMMATION_SYSTEMIC_REACTIVITY_FACTOR 0.8f    /**< +80% reactivity */
#define INFLAMMATION_STORM_REACTIVITY_FACTOR    1.0f    /**< Full astrogliosis */

/* Inflammation BBB permeability factors */
#define INFLAMMATION_BBB_PERMEABILITY_BASE      0.05f   /**< Base permeability increase */
#define INFLAMMATION_BBB_PERMEABILITY_PER_LEVEL 0.15f   /**< Per inflammation level */

/* Reactive astrocyte cytokine production thresholds */
#define REACTIVE_ASTROCYTE_THRESHOLD        0.4f    /**< Reactivity for cytokine release */
#define ASTROGLIOSIS_THRESHOLD              0.7f    /**< Severe reactivity threshold */
#define CALCIUM_WAVE_IMMUNE_THRESHOLD       2.5f    /**< Calcium (µM) for immune signal */

/* Astrocyte metabolic support impairment */
#define REACTIVE_METABOLIC_IMPAIRMENT       -0.25f  /**< Reactivity → ATP support ↓ */
#define INFLAMMATION_METABOLIC_FACTOR       0.15f   /**< Per inflammation level */

/* Reactive astrocyte cytokine release ratios */
#define CYTOKINE_IL1B_RELEASE_RATIO         0.4f    /**< IL-1β release strength ratio */
#define CYTOKINE_IL6_RELEASE_RATIO          0.3f    /**< IL-6 release strength ratio */
#define CYTOKINE_TNFA_RELEASE_RATIO         0.5f    /**< TNF-α release strength (severe) */

/* Chronic inflammation detection threshold */
#define CHRONIC_INFLAMMATION_UPDATE_THRESHOLD 30.0f /**< Updates before chronic state */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine astrocyte effects
 *
 * Represents how cytokine levels affect astrocyte function
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_reactivity_boost;         /**< IL-1β induced reactivity */
    float il6_reactivity_boost;         /**< IL-6 induced reactivity */
    float tnf_reactivity_boost;         /**< TNF-α induced reactivity */
    float ifn_gamma_reactivity_boost;   /**< IFN-γ induced reactivity */

    /* Anti-inflammatory effects */
    float il10_reactivity_reduction;    /**< IL-10 resolution effect */

    /* Calcium modulation */
    float calcium_wave_frequency_change; /**< Change in Ca²⁺ wave rate */
    float calcium_amplitude_change;      /**< Change in Ca²⁺ amplitude */

    /* Glutamate clearance */
    float glutamate_clearance_factor;    /**< Clearance efficiency [0-1] */

    /* Metabolic support */
    float atp_support_factor;            /**< ATP delivery to neurons [0-1] */

    /* BBB effects */
    float bbb_permeability_increase;     /**< BBB leakiness [0-1] */

    /* Aggregate effects */
    float total_reactivity;              /**< Combined reactivity [0-1] */
    bool is_reactive;                    /**< Above reactive threshold */
    bool is_astrogliosis;                /**< Severe reactivity */
} cytokine_astro_network_effects_t;

/**
 * @brief Inflammation astrocyte state
 *
 * How chronic inflammation affects astrocyte population
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;    /**< How long inflamed */
    bool is_chronic;                    /**< >= threshold */

    /* Astrocyte population impacts */
    float reactive_astrocyte_fraction;  /**< Fraction in reactive state [0-1] */
    float avg_calcium_elevation;        /**< Average Ca²⁺ elevation (µM) */
    float glutamate_clearance_deficit;  /**< Clearance impairment [0-1] */
    float metabolic_support_deficit;    /**< ATP support loss [0-1] */
    float bbb_permeability;             /**< BBB integrity loss [0-1] */

    /* Glial scar formation */
    bool glial_scar_forming;            /**< Chronic reactivity → scarring */
    float scar_progression;             /**< Scar formation progress [0-1] */
} inflammation_astro_network_state_t;

/**
 * @brief Astrocyte-driven immune modulation
 *
 * How astrocyte state affects immune function
 */
typedef struct {
    /* Astrocyte state */
    float avg_reactivity;               /**< Average reactivity across network */
    float calcium_wave_activity;        /**< Calcium wave frequency */
    float bbb_disruption_level;         /**< BBB compromise level [0-1] */

    /* Immune effects */
    float cytokine_production_rate;     /**< Reactive astrocyte cytokine release */
    float immune_cell_recruitment;      /**< BBB permeability → infiltration */
    float local_inflammation_amplification; /**< Inflammatory feedback gain */

    /* Resolution signaling */
    bool homeostatic_il10_release;      /**< Resting astrocytes → IL-10 */
    float anti_inflammatory_signal;     /**< IL-10 production rate */

    /* Calcium wave immune propagation */
    bool calcium_wave_immune_trigger;   /**< High Ca²⁺ → immune alert */
    uint32_t calcium_triggered_alerts;  /**< Count of Ca²⁺-triggered alerts */
} astro_network_modulation_t;

/**
 * @brief Complete astrocyte-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    astrocyte_network_t* astrocyte_network;

    /* Current state */
    cytokine_astro_network_effects_t cytokine_effects;
    inflammation_astro_network_state_t inflammation_state;
    astro_network_modulation_t astrocyte_modulation;

    /* Integration flags */
    bool enable_cytokine_reactivity;
    bool enable_inflammation_astrogliosis;
    bool enable_reactive_cytokine_production;
    bool enable_calcium_immune_signaling;
    bool enable_bbb_inflammation_coupling;

    /* Timing */
    uint64_t last_update_time;
    float chronic_inflammation_accumulator; /**< Accumulated inflammation time */

    /* Statistics */
    uint64_t total_updates;
    uint32_t reactivity_events;
    uint32_t cytokine_releases;
    uint32_t calcium_immune_triggers;
    uint32_t bbb_disruption_events;

    } astro_network_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_reactivity;
    bool enable_inflammation_astrogliosis;
    bool enable_reactive_cytokine_production;
    bool enable_calcium_immune_signaling;
    bool enable_bbb_inflammation_coupling;

    /* Sensitivity tuning */
    float cytokine_sensitivity;         /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;     /**< Inflammation effect multiplier [0.5-2.0] */
    float reactivity_sensitivity;       /**< Reactivity response multiplier [0.5-2.0] */

    /* Thresholds */
    float reactive_threshold;           /**< Reactivity for cytokine production [0.3-0.5] */
    float astrogliosis_threshold;       /**< Severe reactivity threshold [0.6-0.8] */
    float calcium_immune_threshold;     /**< Calcium (µM) for immune alert [2.0-3.0] */
} astro_network_config_t;

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
int astro_network_default_config(astro_network_config_t* config);

/**
 * @brief Create astrocyte-immune bridge
 *
 * WHAT: Initialize bidirectional astrocyte-immune integration
 * WHY:  Enable realistic immune-astrocyte coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param astrocyte_network Astrocyte network
 * @return New bridge or NULL on failure
 */
astro_network_bridge_t* astro_network_bridge_create(
    const astro_network_config_t* config,
    brain_immune_system_t* immune_system,
    astrocyte_network_t* astrocyte_network
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
void astro_network_bridge_destroy(astro_network_bridge_t* bridge);

/* ============================================================================
 * Immune → Astrocyte API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to astrocytes
 *
 * WHAT: Modulate astrocyte reactivity based on cytokine levels
 * WHY:  Pro-inflammatory cytokines trigger reactive state
 * HOW:  Query immune system cytokines, adjust astrocyte reactivity/calcium
 *
 * @param bridge Astrocyte-immune bridge
 * @return 0 on success
 */
int astro_network_apply_cytokine_effects(astro_network_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to astrocytes
 *
 * WHAT: Trigger astrogliosis from chronic inflammation
 * WHY:  Sustained inflammation causes persistent reactive state
 * HOW:  Check inflammation level/duration, adjust astrocyte population state
 *
 * @param bridge Astrocyte-immune bridge
 * @return 0 on success
 */
int astro_network_apply_inflammation_effects(astro_network_bridge_t* bridge);

/**
 * @brief Compute astrocyte reactivity from immune state
 *
 * WHAT: Calculate overall astrocyte reactivity given immune status
 * WHY:  Inflammation drives astrocyte activation
 * HOW:  Map cytokine levels and inflammation to reactivity factor [0-1]
 *
 * @param bridge Astrocyte-immune bridge
 * @return Reactivity factor [0-1] (0.0 = resting, 1.0 = full astrogliosis)
 */
float astro_network_compute_reactivity(const astro_network_bridge_t* bridge);

/**
 * @brief Compute glutamate clearance efficiency from inflammation
 *
 * WHAT: Calculate how inflammation impairs glutamate uptake
 * WHY:  Reactive astrocytes have reduced clearance → excitotoxicity risk
 * HOW:  Map inflammation and cytokines to clearance factor
 *
 * @param bridge Astrocyte-immune bridge
 * @return Clearance efficiency [0-1] (1.0 = normal, 0.0 = complete failure)
 */
float astro_network_compute_glutamate_clearance(const astro_network_bridge_t* bridge);

/* ============================================================================
 * Astrocyte → Immune API
 * ============================================================================ */

/**
 * @brief Release cytokines from reactive astrocytes
 *
 * WHAT: Trigger pro-inflammatory cytokine release from reactive astrocytes
 * WHY:  Reactive astrocytes amplify local inflammation
 * HOW:  Query astrocyte reactivity, release IL-1β/IL-6/TNF-α if above threshold
 *
 * @param bridge Astrocyte-immune bridge
 * @return 0 on success
 */
int astro_network_release_cytokines_from_reactive(astro_network_bridge_t* bridge);

/**
 * @brief Trigger immune alert from calcium waves
 *
 * WHAT: Send immune alert when astrocyte calcium waves exceed threshold
 * WHY:  High calcium propagates inflammatory signals across network
 * HOW:  Monitor calcium wave activity, trigger bio-async immune alert
 *
 * @param bridge Astrocyte-immune bridge
 * @return 0 on success
 */
int astro_network_trigger_calcium_immune_alert(astro_network_bridge_t* bridge);

/**
 * @brief Increase BBB permeability from astrocyte dysfunction
 *
 * WHAT: Disrupt BBB integrity when astrocytes reactive
 * WHY:  Astrocyte end-feet retraction → BBB leakiness → immune infiltration
 * HOW:  Map reactivity to BBB permeability, notify immune system
 *
 * @param bridge Astrocyte-immune bridge
 * @return 0 on success
 */
int astro_network_modulate_bbb_permeability(astro_network_bridge_t* bridge);

/**
 * @brief Release IL-10 from resting astrocytes
 *
 * WHAT: Trigger anti-inflammatory response from healthy astrocytes
 * WHY:  Resting astrocytes maintain homeostasis and promote resolution
 * HOW:  Detect low reactivity, release IL-10 cytokine
 *
 * @param bridge Astrocyte-immune bridge
 * @return 0 on success
 */
int astro_network_release_il10_homeostatic(astro_network_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update astrocyte-immune bridge (both directions)
 *
 * WHAT: Process all astrocyte-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from astrocytes, adjust parameters
 *
 * @param bridge Astrocyte-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int astro_network_bridge_update(
    astro_network_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine astrocyte effects
 *
 * @param bridge Astrocyte-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int astro_network_get_cytokine_effects(
    const astro_network_bridge_t* bridge,
    cytokine_astro_network_effects_t* effects
);

/**
 * @brief Get current inflammation astrocyte state
 *
 * @param bridge Astrocyte-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int astro_network_get_inflammation_state(
    const astro_network_bridge_t* bridge,
    inflammation_astro_network_state_t* state
);

/**
 * @brief Check if experiencing astrogliosis
 *
 * WHAT: Determine if astrocytes in severe reactive state
 * WHY:  Detect clinically significant glial activation
 * HOW:  Check reactivity threshold
 *
 * @param bridge Astrocyte-immune bridge
 * @return true if astrogliosis (reactivity > threshold)
 */
bool astro_network_has_astrogliosis(const astro_network_bridge_t* bridge);

/**
 * @brief Get current astrocyte reactivity factor
 *
 * @param bridge Astrocyte-immune bridge
 * @return Reactivity factor [0-1]
 */
float astro_network_get_reactivity_factor(const astro_network_bridge_t* bridge);

/**
 * @brief Get current BBB permeability
 *
 * @param bridge Astrocyte-immune bridge
 * @return BBB permeability [0-1] (0 = intact, 1 = fully compromised)
 */
float astro_network_get_bbb_permeability(const astro_network_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_GLIAL_ASTROCYTE
 *
 * @param bridge Astrocyte-immune bridge
 * @return 0 on success, -1 on error
 */
int astro_network_connect_bio_async(astro_network_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Astrocyte-immune bridge
 * @return 0 on success
 */
int astro_network_disconnect_bio_async(astro_network_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Astrocyte-immune bridge
 * @return true if connected
 */
bool astro_network_is_bio_async_connected(const astro_network_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ASTRO_NETWORK_IMMUNE_BRIDGE_H */
