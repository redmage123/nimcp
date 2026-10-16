/**
 * @file nimcp_dendritic_immune_bridge.h
 * @brief Dendritic Plasticity-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and dendritic plasticity
 * WHY:  Biological evidence shows neuroinflammation damages dendritic spines and
 *       cytokines cause structural changes. Essential for realistic neural modeling.
 * HOW:  Cytokines modulate spine density/complexity, abnormal pruning triggers immune responses,
 *       inflammation reduces dendritic growth, anti-inflammatory states promote recovery.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → DENDRITIC PATHWAYS:
 * ---------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Reduce dendritic spine density
 *    - Cause spine retraction and loss
 *    - Decrease dendritic complexity
 *    - Impair spine morphology (mushroom → stubby)
 *    - Reference: Pribiag & Stellwagen (2014) "TNF-α downregulates inhibitory neurotransmission"
 *
 * 2. IL-1β Specific Effects:
 *    - Reduces NMDA receptor trafficking
 *    - Decreases spine density by 20-30%
 *    - Affects spine stability
 *    - Impairs dendritic integration
 *    - Reference: Mishra et al. (2012) "Astrocytic IL-1β promotes synaptic changes"
 *
 * 3. Chronic Inflammation:
 *    - Progressive dendritic atrophy
 *    - Loss of dendritic complexity
 *    - Reduced dendritic length
 *    - Impaired dendritic spike generation
 *    - Reference: Eyre & Baune (2012) "Neuroplastic changes in depression"
 *
 * 4. Anti-inflammatory Cytokines (IL-10, IL-4):
 *    - Promote dendritic growth
 *    - Enhance spine formation
 *    - Restore dendritic complexity
 *    - Support structural plasticity
 *    - Reference: Yirmiya & Goshen (2011) "Immune modulation of learning"
 *
 * DENDRITIC → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Abnormal Spine Pruning:
 *    - Excessive spine loss triggers immune surveillance
 *    - Rapid dendritic retraction activates microglia
 *    - Synaptic debris removal by immune cells
 *    - Reference: Hong et al. (2016) "Complement and microglia mediate synapse loss"
 *
 * 2. Dendritic Damage Signals:
 *    - Calcium dysregulation → danger signals
 *    - Loss of dendritic integrity → immune activation
 *    - Structural instability → inflammatory response
 *    - Reference: Lively & Schlichter (2013) "Microglia responses to neuronal injury"
 *
 * 3. Synaptic Scaling Dysfunction:
 *    - Failed homeostatic plasticity → immune alert
 *    - Compensatory pruning triggers cytokine release
 *    - Reference: Turrigiano (2008) "Homeostatic plasticity"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    DENDRITIC-IMMUNE BRIDGE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → DENDRITIC PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.3 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.2 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.4 │         ├──→ Spine Loss                         │  ║
 * ║   │   │              │         │    Dendritic Atrophy                  │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     DENDRITIC SYSTEM            │                             │  ║
 * ║   │   │  - Spine density modulation     │                             │  ║
 * ║   │   │  - Complexity reduction         │                             │  ║
 * ║   │   │  - Integration impairment       │                             │  ║
 * ║   │   │  - Growth rate suppression      │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   +0.3       │     Recovery, Growth Promotion                  │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  DENDRITIC → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ RAPID PRUNING│ ──→ Immune Surveillance Trigger                 │  ║
 * ║   │   │ SPINE LOSS   │ ──→ Microglial Activation                       │  ║
 * ║   │   │ COMPLEXITY ↓ │ ──→ Cytokine Release                            │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  RECOVERY    │ ──→ Immune Resolution                           │  ║
 * ║   │   │  REGROWTH    │ ──→ IL-10 Release                               │  ║
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

#ifndef NIMCP_DENDRITIC_IMMUNE_BRIDGE_H
#define NIMCP_DENDRITIC_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/dendritic/nimcp_dendritic.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine spine density impact factors */
#define CYTOKINE_IL1_SPINE_IMPACT       -0.30f   /**< IL-1β → spine loss */
#define CYTOKINE_IL6_SPINE_IMPACT       -0.20f   /**< IL-6 → spine loss */
#define CYTOKINE_TNF_SPINE_IMPACT       -0.40f   /**< TNF-α → strong spine loss */
#define CYTOKINE_IFN_GAMMA_SPINE_IMPACT -0.15f   /**< IFN-γ → mild spine loss */
#define CYTOKINE_IL10_SPINE_IMPACT       0.30f   /**< IL-10 → spine growth */

/* Inflammation-dendritic complexity mapping */
#define INFLAMMATION_COMPLEXITY_THRESHOLD  0.5f   /**< Inflammation for complexity loss */
#define INFLAMMATION_ATROPHY_THRESHOLD     0.7f   /**< Threshold for dendritic atrophy */

/* Structural change immune trigger thresholds */
#define SPINE_LOSS_IMMUNE_THRESHOLD       0.3f   /**< Spine loss rate to trigger immune */
#define PRUNING_RATE_DANGER_THRESHOLD     0.5f   /**< Abnormal pruning rate */

/* Chronic inflammation duration (seconds) */
#define CHRONIC_INFLAMMATION_DENDRITIC_THRESHOLD    (86400.0f * 3)  /**< 3 days = chronic for dendrites */

/* Spine density ranges */
#define SPINE_DENSITY_MIN                 0.1f   /**< Minimum viable spine density */
#define SPINE_DENSITY_MAX                 1.0f   /**< Maximum spine density */
#define SPINE_DENSITY_HEALTHY             0.7f   /**< Healthy baseline */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine dendritic effects
 *
 * Represents how cytokine levels modulate dendritic structure
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_spine_loss;             /**< IL-1β induced spine loss */
    float il6_spine_loss;             /**< IL-6 induced spine loss */
    float tnf_spine_loss;             /**< TNF-α induced spine loss */
    float ifn_gamma_spine_loss;       /**< IFN-γ induced spine loss */

    /* Anti-inflammatory effects */
    float il10_growth_promotion;      /**< IL-10 growth promotion */

    /* Aggregate effects */
    float total_spine_density_change; /**< Combined density modulation */
    float complexity_reduction;       /**< Loss of dendritic complexity [0-1] */
    float integration_impairment;     /**< Reduced dendritic integration [0-1] */
    float growth_suppression;         /**< Reduced growth rate [0-1] */
} cytokine_dendritic_effects_t;

/**
 * @brief Inflammation dendritic state
 *
 * How chronic inflammation affects dendritic structure
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 3 days for dendrites */

    /* Structural impacts */
    float spine_density;               /**< Current spine density [0-1] */
    float spine_density_baseline;      /**< Baseline before inflammation */
    float complexity_loss;             /**< Reduction in complexity [0-1] */
    float atrophy_severity;            /**< Dendritic atrophy level [0-1] */

    /* Functional impacts */
    float nmda_trafficking_impairment; /**< Reduced NMDA receptors [0-1] */
    float spike_generation_impairment; /**< Reduced dendritic spikes [0-1] */
    float calcium_dysregulation;       /**< Calcium handling problems [0-1] */
} inflammation_dendritic_state_t;

/**
 * @brief Dendritic damage immune trigger
 *
 * How structural damage triggers immune activity
 */
typedef struct {
    /* Damage indicators */
    float spine_loss_rate;             /**< Rate of spine loss [spines/sec] */
    float pruning_rate;                /**< Pruning rate [0-1] */
    float complexity_reduction_rate;   /**< Loss of complexity rate */

    /* Immune triggers */
    bool microglial_surveillance_active; /**< Microglia activated */
    bool synaptic_debris_detected;     /**< Debris removal needed */
    float danger_signal_strength;      /**< DAMPs level [0-1] */

    /* Homeostatic failure */
    bool scaling_dysfunction;          /**< Failed homeostatic plasticity */
    float compensation_failure;        /**< Inability to compensate [0-1] */
} dendritic_immune_trigger_t;

/**
 * @brief Recovery and growth immune boost
 *
 * How healthy dendritic state supports immunity
 */
typedef struct {
    /* Structural health */
    float spine_density_health;        /**< Spine density normalized [0-1] */
    float complexity_health;           /**< Dendritic complexity [0-1] */
    float growth_rate;                 /**< Active growth rate */

    /* Immune benefits */
    float immune_support;              /**< Support to immune system [0-1] */
    float il10_release_trigger;        /**< Promote IL-10 release */
    float inflammation_clearance;      /**< Help clear inflammation */
} dendritic_immune_support_t;

/**
 * @brief Complete dendritic-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    dendritic_tree_t dendritic_tree;

    /* Current state */
    cytokine_dendritic_effects_t cytokine_effects;
    inflammation_dendritic_state_t inflammation_state;
    dendritic_immune_trigger_t damage_trigger;
    dendritic_immune_support_t recovery_support;

    /* Integration flags */
    bool enable_cytokine_dendritic_modulation;
    bool enable_inflammation_atrophy;
    bool enable_damage_immune_trigger;
    bool enable_recovery_immune_support;
    bool enable_spine_density_tracking;
    bool enable_complexity_tracking;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t damage_triggered_responses;
    uint32_t recovery_supports;
    float total_spine_loss;
    float total_spine_growth;
    } dendritic_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_dendritic_modulation;
    bool enable_inflammation_atrophy;
    bool enable_damage_immune_trigger;
    bool enable_recovery_immune_support;
    bool enable_spine_density_tracking;
    bool enable_complexity_tracking;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float damage_trigger_sensitivity;  /**< Damage trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float spine_loss_trigger_threshold; /**< Spine loss to trigger immune [0.2-0.5] */
    float inflammation_atrophy_threshold; /**< Inflammation for atrophy [0.5-0.9] */

    /* Baseline parameters */
    float baseline_spine_density;      /**< Healthy spine density [0.5-0.9] */
} dendritic_immune_config_t;

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
int dendritic_immune_default_config(dendritic_immune_config_t* config);

/**
 * @brief Create dendritic-immune bridge
 *
 * WHAT: Initialize bidirectional dendritic-immune integration
 * WHY:  Enable realistic immune-plasticity coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param dendritic_tree Dendritic tree system
 * @return New bridge or NULL on failure
 */
dendritic_immune_bridge_t* dendritic_immune_bridge_create(
    const dendritic_immune_config_t* config,
    brain_immune_system_t* immune_system,
    dendritic_tree_t dendritic_tree
);

/**
 * @brief Destroy dendritic-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void dendritic_immune_bridge_destroy(dendritic_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Dendritic API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to dendritic structure
 *
 * WHAT: Modulate spine density and complexity based on cytokine levels
 * WHY:  Pro-inflammatory cytokines cause spine loss and atrophy
 * HOW:  Query immune system cytokines, adjust dendritic parameters
 *
 * @param bridge Dendritic-immune bridge
 * @return 0 on success
 */
int dendritic_immune_apply_cytokine_effects(dendritic_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to dendritic structure
 *
 * WHAT: Induce dendritic atrophy and complexity loss from prolonged inflammation
 * WHY:  Chronic inflammation causes progressive dendritic damage
 * HOW:  Check inflammation duration/level, reduce spine density and complexity
 *
 * @param bridge Dendritic-immune bridge
 * @return 0 on success
 */
int dendritic_immune_apply_inflammation_effects(dendritic_immune_bridge_t* bridge);

/**
 * @brief Compute spine density reduction from inflammation
 *
 * WHAT: Calculate spine loss from immune state
 * WHY:  Inflammation reduces spine density and stability
 * HOW:  Map inflammation level/duration to spine density change
 *
 * @param bridge Dendritic-immune bridge
 * @return Spine density reduction [0-1]
 */
float dendritic_immune_compute_spine_loss(const dendritic_immune_bridge_t* bridge);

/**
 * @brief Compute complexity reduction from inflammation
 *
 * WHAT: Calculate dendritic complexity loss from immune state
 * WHY:  Chronic inflammation reduces dendritic arbor complexity
 * HOW:  Map inflammation level/duration to complexity reduction
 *
 * @param bridge Dendritic-immune bridge
 * @return Complexity reduction [0-1]
 */
float dendritic_immune_compute_complexity_loss(const dendritic_immune_bridge_t* bridge);

/* ============================================================================
 * Dendritic → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from abnormal spine loss
 *
 * WHAT: Activate immune system from rapid spine pruning
 * WHY:  Excessive spine loss activates microglial surveillance
 * HOW:  Check spine loss rate, trigger immune response if threshold exceeded
 *
 * @param bridge Dendritic-immune bridge
 * @return 0 on success
 */
int dendritic_immune_trigger_from_spine_loss(dendritic_immune_bridge_t* bridge);

/**
 * @brief Trigger immune response from dendritic damage
 *
 * WHAT: Activate immune system from structural damage signals
 * WHY:  Dendritic injury releases danger signals (DAMPs)
 * HOW:  Check calcium dysregulation and complexity loss, trigger if severe
 *
 * @param bridge Dendritic-immune bridge
 * @return 0 on success
 */
int dendritic_immune_trigger_from_damage(dendritic_immune_bridge_t* bridge);

/**
 * @brief Support immune resolution from healthy dendrites
 *
 * WHAT: Enhance immune recovery when dendritic health is good
 * WHY:  Healthy synaptic state supports anti-inflammatory processes
 * HOW:  Query dendritic health, promote IL-10 release and inflammation clearance
 *
 * @param bridge Dendritic-immune bridge
 * @return 0 on success
 */
int dendritic_immune_support_from_health(dendritic_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update dendritic-immune bridge (both directions)
 *
 * WHAT: Process all immune-dendritic interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from damage, support recovery
 *
 * @param bridge Dendritic-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int dendritic_immune_bridge_update(
    dendritic_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine dendritic effects
 *
 * @param bridge Dendritic-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int dendritic_immune_get_cytokine_effects(
    const dendritic_immune_bridge_t* bridge,
    cytokine_dendritic_effects_t* effects
);

/**
 * @brief Get current inflammation dendritic state
 *
 * @param bridge Dendritic-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int dendritic_immune_get_inflammation_state(
    const dendritic_immune_bridge_t* bridge,
    inflammation_dendritic_state_t* state
);

/**
 * @brief Check if experiencing dendritic atrophy
 *
 * WHAT: Determine if inflammation causing dendritic atrophy
 * WHY:  Atrophy is distinct pathological state
 * HOW:  Check inflammation level and atrophy severity
 *
 * @param bridge Dendritic-immune bridge
 * @return true if experiencing atrophy
 */
bool dendritic_immune_is_atrophy(const dendritic_immune_bridge_t* bridge);

/**
 * @brief Get current spine density
 *
 * @param bridge Dendritic-immune bridge
 * @return Spine density [0-1]
 */
float dendritic_immune_get_spine_density(const dendritic_immune_bridge_t* bridge);

/**
 * @brief Get complexity loss level
 *
 * @param bridge Dendritic-immune bridge
 * @return Complexity reduction [0-1]
 */
float dendritic_immune_get_complexity_loss(const dendritic_immune_bridge_t* bridge);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_DENDRITIC
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int dendritic_immune_connect_bio_async(dendritic_immune_bridge_t* bridge);

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
int dendritic_immune_disconnect_bio_async(dendritic_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool dendritic_immune_is_bio_async_connected(const dendritic_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DENDRITIC_IMMUNE_BRIDGE_H */
