/**
 * @file nimcp_microglia_immune_bridge.h
 * @brief Microglia-Brain Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and microglia
 * WHY:  Microglia ARE the brain's resident immune cells - they perform phagocytosis,
 *       produce cytokines, present antigens, and undergo M1/M2 polarization. Critical
 *       for coordinating with the brain immune system abstraction.
 * HOW:  Brain immune system uses microglia for local immune response execution;
 *       microglia report threats as antigens and coordinate with systemic immunity.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * MICROGLIA AS BRAIN IMMUNE CELLS:
 * --------------------------------
 * Microglia are myeloid-lineage immune cells, analogous to macrophages:
 * - Ramified state: Surveillance mode, extended processes scanning tissue
 * - Activated state: Retracted processes, cytokine production, phagocytosis
 * - M1 polarization: Pro-inflammatory (IL-1β, TNF-α, IL-6, NO production)
 * - M2 polarization: Anti-inflammatory (IL-10, TGF-β, debris clearance, repair)
 * - Antigen presentation: Express MHC-II, present to T cells
 * - Phagocytosis: Engulf pathogens, debris, dying neurons, weak synapses
 *
 * IMMUNE → MICROGLIA PATHWAYS:
 * ---------------------------
 * 1. Cytokine Activation:
 *    - Pro-inflammatory cytokines → M1 polarization
 *    - IL-1β, TNF-α, IFN-γ → activated state
 *    - IL-10, TGF-β → M2 polarization, resolution
 *    - Reference: Cherry et al. (2014) "Neuroinflammation and M2 microglia:
 *      the good, the bad, and the inflamed"
 *
 * 2. Systemic Inflammation → Local Microglial Response:
 *    - Regional/systemic inflammation → mass microglial activation
 *    - Synchronized activation via ATP/purinergic signaling
 *    - Cytokine storm → microglial toxicity
 *    - Reference: Davalos et al. (2005) "ATP mediates rapid microglial response
 *      to local brain injury in vivo"
 *
 * 3. Helper T Cell Coordination:
 *    - Helper T cytokines → microglial state modulation
 *    - Th1 cytokines → M1 polarization
 *    - Th2 cytokines → M2 polarization
 *    - Reference: Ransohoff & Perry (2009) "Microglial physiology: unique
 *      stimuli, specialized responses"
 *
 * MICROGLIA → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Microglial Cytokine Production:
 *    - M1 microglia → IL-1β, IL-6, TNF-α release
 *    - M2 microglia → IL-10, TGF-β release
 *    - Amplify or dampen systemic immune response
 *    - Reference: Hanisch (2002) "Microglia as a source and target of cytokines"
 *
 * 2. Antigen Presentation to Brain Immune System:
 *    - Microglia detect local threats (pathogens, debris, misfolded proteins)
 *    - Present antigens via MHC-II
 *    - Trigger adaptive immune response
 *    - Reference: Carson et al. (2007) "Mature microglia resemble immature
 *      antigen-presenting cells"
 *
 * 3. Complement-Mediated Synapse Tagging:
 *    - C1q/C3 opsonization marks weak synapses
 *    - Microglial phagocytosis of tagged synapses
 *    - Report pruning activity to brain immune system
 *    - Reference: Stevens et al. (2007) "The classical complement cascade mediates
 *      CNS synapse elimination"
 *
 * 4. Damage-Associated Molecular Patterns (DAMPs):
 *    - Microglia detect DAMPs (ATP, HMGB1, heat shock proteins)
 *    - Report as antigens to brain immune system
 *    - Trigger sterile inflammation
 *    - Reference: Chen & Nuñez (2010) "Sterile inflammation: sensing and
 *      reacting to damage"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    MICROGLIA-IMMUNE BRIDGE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → MICROGLIA PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β/TNF-α  │  ───────┐                                       │  ║
 * ║   │   │ IFN-γ        │         ├──→ M1 Polarization (Pro-inflammatory) │  ║
 * ║   │   │              │         │                                       │  ║
 * ║   │   │ IL-10/TGF-β  │  ───────┼──→ M2 Polarization (Anti-inflammatory)│  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     MICROGLIA NETWORK           │                             │  ║
 * ║   │   │  - Activation state             │                             │  ║
 * ║   │   │  - M1/M2 polarization           │                             │  ║
 * ║   │   │  - Phagocytic activity          │                             │  ║
 * ║   │   │  - Cytokine production          │                             │  ║
 * ║   │   │  - Process morphology           │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION LEVEL     │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ LOCAL    → Targeted activation│                                │  ║
 * ║   │   │ REGIONAL → Mass activation    │                                │  ║
 * ║   │   │ SYSTEMIC → Network-wide M1    │                                │  ║
 * ║   │   │ STORM    → Toxic activation   │                                │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  MICROGLIA → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ M1 MICROGLIA     │ ──→ IL-1β, IL-6, TNF-α Release              │  ║
 * ║   │   │ (ACTIVATED)      │ ──→ Antigen Presentation                    │  ║
 * ║   │   │                  │ ──→ Phagocytosis Reports                    │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ M2 MICROGLIA     │ ──→ IL-10, TGF-β Release                    │  ║
 * ║   │   │ (RESOLUTION)     │ ──→ Debris Clearance                        │  ║
 * ║   │   │                  │ ──→ Repair Signaling                        │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ THREAT DETECTION │ ──→ Antigen Presentation (DAMPs)            │  ║
 * ║   │   │ COMPLEMENT TAG   │ ──→ Synapse Pruning Report                  │  ║
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

#ifndef NIMCP_MICROGLIA_IMMUNE_BRIDGE_H
#define NIMCP_MICROGLIA_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "glial/microglia/nimcp_microglia.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine microglial activation factors */
#define CYTOKINE_IL1_ACTIVATION_FACTOR      0.6f    /**< IL-1β → M1 activation */
#define CYTOKINE_IL6_ACTIVATION_FACTOR      0.4f    /**< IL-6 → M1 activation */
#define CYTOKINE_TNF_ACTIVATION_FACTOR      0.7f    /**< TNF-α → strong M1 */
#define CYTOKINE_IFN_GAMMA_ACTIVATION_FACTOR 0.8f   /**< IFN-γ → M1 polarization */
#define CYTOKINE_IL10_M2_FACTOR             0.6f    /**< IL-10 → M2 polarization */

/* Inflammation microglial state thresholds */
#define INFLAMMATION_NONE_ACTIVATION        0.0f    /**< Ramified state */
#define INFLAMMATION_LOCAL_ACTIVATION       0.3f    /**< Local activation */
#define INFLAMMATION_REGIONAL_ACTIVATION    0.6f    /**< Mass activation */
#define INFLAMMATION_SYSTEMIC_ACTIVATION    0.9f    /**< Network-wide M1 */
#define INFLAMMATION_STORM_TOXICITY         1.0f    /**< Toxic overactivation */

/* Microglial cytokine production thresholds */
#define M1_CYTOKINE_PRODUCTION_THRESHOLD    0.5f    /**< M1 state → cytokine release */
#define M2_CYTOKINE_PRODUCTION_THRESHOLD    0.4f    /**< M2 state → anti-inflam */
#define PHAGOCYTIC_ANTIGEN_THRESHOLD        0.7f    /**< Phagocytic → antigen present */

/* Complement synapse tagging thresholds */
#define COMPLEMENT_C1Q_ANTIGEN_SEVERITY     3       /**< C1q tag → low severity */
#define COMPLEMENT_C3_ANTIGEN_SEVERITY      5       /**< C3 tag → moderate severity */
#define PRUNING_ANTIGEN_REPORT_RATE         0.1f    /**< Report 10% of pruning events */

/* Microglial state modulation factors */
#define M1_POLARIZATION_THRESHOLD           0.6f    /**< Activation → M1 */
#define M2_POLARIZATION_THRESHOLD           0.4f    /**< IL-10 → M2 */
#define PROCESS_RETRACTION_ACTIVATION       0.5f    /**< Activation → retract */

/* M1/M2 polarization dominance ratios */
#define M1_M2_DOMINANCE_RATIO               1.5f    /**< M1 > M2 * this → M1 dominant */
#define M2_M1_DOMINANCE_RATIO               1.2f    /**< M2 > M1 * this → M2 dominant */
#define MIN_POLARIZATION_SIGNAL             0.1f    /**< Minimum signal for mixed state */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Microglial polarization state
 *
 * BIOLOGICAL BASIS:
 * M1: Classical activation (pro-inflammatory, cytotoxic)
 * M2: Alternative activation (anti-inflammatory, repair)
 */
typedef enum {
    MICROGLIA_POLARIZATION_NONE = 0,    /**< Resting/ramified */
    MICROGLIA_POLARIZATION_M1,          /**< Pro-inflammatory */
    MICROGLIA_POLARIZATION_M2,          /**< Anti-inflammatory/repair */
    MICROGLIA_POLARIZATION_MIXED        /**< Mixed M1/M2 markers */
} microglia_polarization_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine microglial effects
 *
 * Represents how cytokine levels affect microglial state
 */
typedef struct {
    /* M1 polarization drivers */
    float il1_m1_drive;                 /**< IL-1β → M1 */
    float il6_m1_drive;                 /**< IL-6 → M1 */
    float tnf_m1_drive;                 /**< TNF-α → M1 */
    float ifn_gamma_m1_drive;           /**< IFN-γ → M1 */

    /* M2 polarization drivers */
    float il10_m2_drive;                /**< IL-10 → M2 */

    /* Aggregate effects */
    float total_activation;             /**< Combined activation [0-1] */
    float m1_polarization_strength;     /**< M1 bias [0-1] */
    float m2_polarization_strength;     /**< M2 bias [0-1] */
    microglia_polarization_t polarization; /**< Current polarization state */

    /* Process morphology */
    float process_retraction;           /**< Process retraction [0-1] */
    float phagocytic_capacity;          /**< Phagocytosis rate [0-1] */
} cytokine_microglia_effects_t;

/**
 * @brief Inflammation microglial state
 *
 * How systemic inflammation affects microglial population
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;    /**< How long inflamed */

    /* Population activation */
    float activated_microglia_fraction; /**< Fraction activated [0-1] */
    float m1_fraction;                  /**< Fraction M1 polarized [0-1] */
    float m2_fraction;                  /**< Fraction M2 polarized [0-1] */

    /* Network-wide effects */
    float avg_cytokine_production;      /**< Average cytokine release rate */
    float avg_phagocytosis_rate;        /**< Average phagocytosis */
    bool cytokine_storm_toxicity;       /**< Storm → microglial toxicity */
} inflammation_microglia_state_t;

/**
 * @brief Microglia-driven immune modulation
 *
 * How microglial state affects brain immune function
 */
typedef struct {
    /* Microglial state */
    float avg_activation;               /**< Average activation across network */
    microglia_polarization_t dominant_polarization;
    float phagocytosis_activity;        /**< Network phagocytosis rate */

    /* Cytokine production */
    float m1_cytokine_production;       /**< IL-1β, IL-6, TNF-α production */
    float m2_cytokine_production;       /**< IL-10, TGF-β production */

    /* Antigen presentation */
    uint32_t antigens_presented;        /**< DAMPs/threats presented */
    uint32_t complement_tag_reports;    /**< Synapse pruning reports */
    uint32_t phagocytosis_reports;      /**< Phagocytosis event reports */

    /* Immune coordination */
    bool local_inflammation_trigger;    /**< Microglia → local inflammation */
    float immune_cell_recruitment;      /**< Cytokine → recruitment strength */
} microglia_immune_modulation_t;

/**
 * @brief Complete microglia-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    microglia_network_t* microglia_network;

    /* Current state */
    cytokine_microglia_effects_t cytokine_effects;
    inflammation_microglia_state_t inflammation_state;
    microglia_immune_modulation_t microglia_modulation;

    /* Integration flags */
    bool enable_cytokine_polarization;
    bool enable_inflammation_activation;
    bool enable_m1_cytokine_production;
    bool enable_m2_cytokine_production;
    bool enable_antigen_presentation;
    bool enable_complement_reporting;

    /* Timing */
    uint64_t last_update_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t m1_polarization_events;
    uint32_t m2_polarization_events;
    uint32_t antigens_presented;
    uint32_t cytokine_releases;
    uint32_t complement_reports;

    } microglia_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_polarization;
    bool enable_inflammation_activation;
    bool enable_m1_cytokine_production;
    bool enable_m2_cytokine_production;
    bool enable_antigen_presentation;
    bool enable_complement_reporting;

    /* Sensitivity tuning */
    float cytokine_sensitivity;         /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;     /**< Inflammation effect multiplier [0.5-2.0] */
    float polarization_sensitivity;     /**< Polarization response multiplier [0.5-2.0] */

    /* Thresholds */
    float m1_threshold;                 /**< M1 polarization threshold [0.5-0.7] */
    float m2_threshold;                 /**< M2 polarization threshold [0.3-0.5] */
    float phagocytic_threshold;         /**< Phagocytic state threshold [0.6-0.8] */
} microglia_immune_config_t;

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
int microglia_immune_default_config(microglia_immune_config_t* config);

/**
 * @brief Create microglia-immune bridge
 *
 * WHAT: Initialize bidirectional microglia-immune integration
 * WHY:  Enable realistic immune-microglia coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param microglia_network Microglia network
 * @return New bridge or NULL on failure
 */
microglia_immune_bridge_t* microglia_immune_bridge_create(
    const microglia_immune_config_t* config,
    brain_immune_system_t* immune_system,
    microglia_network_t* microglia_network
);

/**
 * @brief Destroy microglia-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void microglia_immune_bridge_destroy(microglia_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Microglia API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to microglia
 *
 * WHAT: Polarize microglia based on cytokine milieu
 * WHY:  Pro-inflammatory → M1, anti-inflammatory → M2
 * HOW:  Query immune system cytokines, adjust microglial polarization state
 *
 * @param bridge Microglia-immune bridge
 * @return 0 on success
 */
int microglia_immune_apply_cytokine_effects(microglia_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to microglia
 *
 * WHAT: Trigger mass microglial activation from systemic inflammation
 * WHY:  Regional/systemic inflammation → widespread activation
 * HOW:  Check inflammation level, activate microglia population proportionally
 *
 * @param bridge Microglia-immune bridge
 * @return 0 on success
 */
int microglia_immune_apply_inflammation_effects(microglia_immune_bridge_t* bridge);

/**
 * @brief Compute microglial activation from immune state
 *
 * WHAT: Calculate overall microglial activation given immune status
 * WHY:  Inflammation and cytokines drive activation
 * HOW:  Map cytokine levels and inflammation to activation factor [0-1]
 *
 * @param bridge Microglia-immune bridge
 * @return Activation factor [0-1] (0.0 = ramified, 1.0 = fully activated)
 */
float microglia_immune_compute_activation(const microglia_immune_bridge_t* bridge);

/**
 * @brief Determine microglial polarization state
 *
 * WHAT: Calculate M1 vs M2 polarization from cytokine balance
 * WHY:  Cytokine milieu determines functional phenotype
 * HOW:  Compare pro-inflammatory vs anti-inflammatory signals
 *
 * @param bridge Microglia-immune bridge
 * @return Polarization state
 */
microglia_polarization_t microglia_immune_compute_polarization(
    const microglia_immune_bridge_t* bridge
);

/* ============================================================================
 * Microglia → Immune API
 * ============================================================================ */

/**
 * @brief Release cytokines from M1 microglia
 *
 * WHAT: Trigger pro-inflammatory cytokine release from M1 microglia
 * WHY:  M1 microglia amplify inflammation
 * HOW:  Query M1 fraction, release IL-1β/IL-6/TNF-α proportionally
 *
 * @param bridge Microglia-immune bridge
 * @return 0 on success
 */
int microglia_immune_release_m1_cytokines(microglia_immune_bridge_t* bridge);

/**
 * @brief Release cytokines from M2 microglia
 *
 * WHAT: Trigger anti-inflammatory cytokine release from M2 microglia
 * WHY:  M2 microglia promote resolution and repair
 * HOW:  Query M2 fraction, release IL-10/TGF-β proportionally
 *
 * @param bridge Microglia-immune bridge
 * @return 0 on success
 */
int microglia_immune_release_m2_cytokines(microglia_immune_bridge_t* bridge);

/**
 * @brief Present antigens from DAMP detection
 *
 * WHAT: Report microglial threat detection as brain immune antigens
 * WHY:  Microglia are first responders, detect DAMPs and pathogens
 * HOW:  Monitor microglial activation events, present as antigens
 *
 * @param bridge Microglia-immune bridge
 * @return 0 on success
 */
int microglia_immune_present_damp_antigens(microglia_immune_bridge_t* bridge);

/**
 * @brief Report complement-tagged synapse pruning
 *
 * WHAT: Notify immune system of complement-mediated synapse elimination
 * WHY:  Complement tagging is part of adaptive immune response
 * HOW:  Query C1q/C3 tagged synapses, report as low-severity antigens
 *
 * @param bridge Microglia-immune bridge
 * @return 0 on success
 */
int microglia_immune_report_complement_pruning(microglia_immune_bridge_t* bridge);

/**
 * @brief Report phagocytosis events
 *
 * WHAT: Notify immune system of microglial phagocytosis activity
 * WHY:  Phagocytosis is core immune function
 * HOW:  Track phagocytosis rate, report as immune activity metric
 *
 * @param bridge Microglia-immune bridge
 * @return 0 on success
 */
int microglia_immune_report_phagocytosis(microglia_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update microglia-immune bridge (both directions)
 *
 * WHAT: Process all microglia-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from microglia, adjust parameters
 *
 * @param bridge Microglia-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int microglia_immune_bridge_update(
    microglia_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine microglial effects
 *
 * @param bridge Microglia-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int microglia_immune_get_cytokine_effects(
    const microglia_immune_bridge_t* bridge,
    cytokine_microglia_effects_t* effects
);

/**
 * @brief Get current inflammation microglial state
 *
 * @param bridge Microglia-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int microglia_immune_get_inflammation_state(
    const microglia_immune_bridge_t* bridge,
    inflammation_microglia_state_t* state
);

/**
 * @brief Check if experiencing cytokine storm toxicity
 *
 * WHAT: Determine if cytokine storm causing microglial dysfunction
 * WHY:  Detect pathological overactivation
 * HOW:  Check inflammation level and toxicity flag
 *
 * @param bridge Microglia-immune bridge
 * @return true if cytokine storm toxicity
 */
bool microglia_immune_has_storm_toxicity(const microglia_immune_bridge_t* bridge);

/**
 * @brief Get current microglial activation factor
 *
 * @param bridge Microglia-immune bridge
 * @return Activation factor [0-1]
 */
float microglia_immune_get_activation_factor(const microglia_immune_bridge_t* bridge);

/**
 * @brief Get current M1 polarization fraction
 *
 * @param bridge Microglia-immune bridge
 * @return M1 fraction [0-1]
 */
float microglia_immune_get_m1_fraction(const microglia_immune_bridge_t* bridge);

/**
 * @brief Get current M2 polarization fraction
 *
 * @param bridge Microglia-immune bridge
 * @return M2 fraction [0-1]
 */
float microglia_immune_get_m2_fraction(const microglia_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_MICROGLIA
 *
 * @param bridge Microglia-immune bridge
 * @return 0 on success, -1 on error
 */
int microglia_immune_connect_bio_async(microglia_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Microglia-immune bridge
 * @return 0 on success
 */
int microglia_immune_disconnect_bio_async(microglia_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Microglia-immune bridge
 * @return true if connected
 */
bool microglia_immune_is_bio_async_connected(const microglia_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MICROGLIA_IMMUNE_BRIDGE_H */
