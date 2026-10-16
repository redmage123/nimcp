/**
 * @file nimcp_structural_immune_bridge.h
 * @brief Structural Plasticity-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between brain immune system and structural plasticity
 * WHY:  Microglia mediate synaptic pruning through complement tagging and engulfment
 * HOW:  Immune signals trigger complement tagging; microglia eliminate tagged spines
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → STRUCTURAL PLASTICITY PATHWAYS:
 * ----------------------------------------
 * 1. Microglia-Mediated Synaptic Pruning:
 *    - Microglia actively prune synapses during development and plasticity
 *    - CR3 receptor on microglia binds C3-tagged synapses
 *    - Engulfment removes synaptic material
 *    - Critical for circuit refinement
 *    - Reference: Schafer et al. (2012) "Microglia sculpt postnatal neural
 *      circuits in an activity and complement-dependent manner"
 *
 * 2. Complement Tagging (C1q/C3):
 *    - Weak or inactive synapses tagged by complement C1q
 *    - C3 deposition marks synapses for elimination
 *    - Activity-dependent: low activity → tagging
 *    - Reference: Stevens et al. (2007) "The classical complement cascade
 *      mediates CNS synapse elimination"
 *
 * 3. Inflammation Effects on Structural Plasticity:
 *    - Pro-inflammatory cytokines (IL-1β, TNF-α) impair spine formation
 *    - IL-6 reduces spine density
 *    - Chronic inflammation → excessive pruning
 *    - Reference: Mishra et al. (2012) "Astrocyte-mediated long-term depression"
 *
 * 4. Anti-Inflammatory Protection:
 *    - IL-10 protects spines from elimination
 *    - Reduces complement deposition
 *    - Promotes spine stabilization
 *    - Reference: Lim & Yang (2017) "Neuroinflammation-dependent synapse loss"
 *
 * STRUCTURAL PLASTICITY → IMMUNE PATHWAYS:
 * ----------------------------------------
 * 1. Synapse Elimination Signals:
 *    - Dying spines release "eat me" signals
 *    - Phosphatidylserine exposure on pruning synapses
 *    - Attracts microglia for engulfment
 *    - Reference: Lehrman et al. (2018) "CD47 protects synapses from excess
 *      microglia-mediated pruning during development"
 *
 * 2. Spine Density Homeostasis:
 *    - High spine density → immune pruning increase
 *    - Low spine density → reduced pruning
 *    - Maintains optimal connectivity
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              STRUCTURAL PLASTICITY-IMMUNE BRIDGE                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → STRUCTURAL PATHWAYS                       │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -40% │  ───────┐                                       │  ║
 * ║   │   │ TNF-α → -50% │         │                                       │  ║
 * ║   │   │ IL-6  → -30% │         ├──→ Impaired Spine Formation           │  ║
 * ║   │   │ IL-10 → +20% │         │    Reduced Stabilization             │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │  MICROGLIA PRUNING              │                             │  ║
 * ║   │   │  - Complement tagging           │                             │  ║
 * ║   │   │  - CR3-mediated engulfment      │                             │  ║
 * ║   │   │  - Selective elimination        │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  STRUCTURAL → IMMUNE PATHWAYS                       │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────┐                                         │  ║
 * ║   │   │  PRUNING SYNAPSES    │ ──→ "Eat Me" Signals                    │  ║
 * ║   │   │  WEAK SPINES         │ ──→ Complement Tagging Request          │  ║
 * ║   │   │  HIGH DENSITY        │ ──→ Immune Pruning Increase             │  ║
 * ║   │   └──────────────────────┘                                         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_STRUCTURAL_IMMUNE_BRIDGE_H
#define NIMCP_STRUCTURAL_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine structural plasticity impact factors */
#define CYTOKINE_IL1_FORMATION_IMPACT      -0.4f  /**< IL-1β → formation deficit */
#define CYTOKINE_IL6_FORMATION_IMPACT      -0.3f  /**< IL-6 → formation deficit */
#define CYTOKINE_TNF_FORMATION_IMPACT      -0.5f  /**< TNF-α → strong deficit */
#define CYTOKINE_IL10_FORMATION_IMPACT      0.2f  /**< IL-10 → protection */

/* Cytokine pruning impact */
#define CYTOKINE_IL1_PRUNING_BOOST          0.3f  /**< IL-1β → increased pruning */
#define CYTOKINE_IL6_PRUNING_BOOST          0.2f  /**< IL-6 → increased pruning */
#define CYTOKINE_TNF_PRUNING_BOOST          0.4f  /**< TNF-α → strong pruning */
#define CYTOKINE_IL10_PRUNING_REDUCTION    -0.3f  /**< IL-10 → reduced pruning */

/* Inflammation structural effects */
#define INFLAMMATION_NONE_FORMATION_FACTOR     1.0f   /**< No effect */
#define INFLAMMATION_LOCAL_FORMATION_FACTOR    0.8f   /**< -20% formation */
#define INFLAMMATION_REGIONAL_FORMATION_FACTOR 0.6f   /**< -40% formation */
#define INFLAMMATION_SYSTEMIC_FORMATION_FACTOR 0.3f   /**< -70% formation */
#define INFLAMMATION_STORM_FORMATION_FACTOR    0.1f   /**< -90% formation */

/* Microglia pruning rates */
#define MICROGLIA_BASELINE_PRUNING_RATE     0.01f  /**< Baseline pruning */
#define MICROGLIA_ACTIVE_PRUNING_RATE       0.05f  /**< Active immune state */
#define MICROGLIA_INFLAMMATION_PRUNING_RATE 0.1f   /**< During inflammation */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine structural plasticity effects
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_formation_impairment;   /**< IL-1β impairs formation */
    float il6_formation_impairment;   /**< IL-6 impairs formation */
    float tnf_formation_impairment;   /**< TNF-α impairs formation */

    /* Anti-inflammatory effects */
    float il10_formation_boost;       /**< IL-10 protects formation */

    /* Pruning modulation */
    float il1_pruning_boost;          /**< IL-1β increases pruning */
    float il6_pruning_boost;          /**< IL-6 increases pruning */
    float tnf_pruning_boost;          /**< TNF-α increases pruning */
    float il10_pruning_reduction;     /**< IL-10 reduces pruning */

    /* Aggregate effects */
    float total_formation_factor;     /**< Combined formation multiplier */
    float total_pruning_factor;       /**< Combined pruning multiplier */
    float stabilization_impairment;   /**< Stabilization deficit [0-1] */
} cytokine_structural_effects_t;

/**
 * @brief Microglia pruning state
 */
typedef struct {
    /* Microglia activity */
    float pruning_rate;               /**< Current pruning rate */
    float complement_tagging_rate;    /**< Rate of complement tagging */
    uint32_t tagged_spine_count;      /**< Number of tagged spines */

    /* Pruning statistics */
    uint32_t spines_pruned_today;     /**< Recent pruning activity */
    uint32_t total_complement_tags;   /**< Lifetime tags */
    uint32_t total_engulfments;       /**< Lifetime engulfments */
} microglia_pruning_state_t;

/**
 * @brief Structural-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    structural_plasticity_system_t* structural_system;

    /* Current state */
    cytokine_structural_effects_t cytokine_effects;
    microglia_pruning_state_t microglia_state;

    /* Integration flags */
    bool enable_cytokine_modulation;
    bool enable_microglia_pruning;
    bool enable_complement_tagging;
    bool enable_inflammation_effects;

    /* Timing */
    uint64_t last_update_time;
    uint64_t last_pruning_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_impairments;
    uint32_t microglia_prunings;
    uint32_t complement_tags_applied;

    } structural_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_modulation;
    bool enable_microglia_pruning;
    bool enable_complement_tagging;
    bool enable_inflammation_effects;

    /* Sensitivity tuning */
    float cytokine_sensitivity;       /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;   /**< Inflammation effect multiplier [0.5-2.0] */
    float pruning_sensitivity;        /**< Pruning rate multiplier [0.5-2.0] */

    /* Thresholds */
    float weak_spine_threshold;       /**< Activity below this → tagging */
    float high_density_threshold;     /**< Density above this → pruning boost */
} structural_immune_config_t;

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
int structural_immune_default_config(structural_immune_config_t* config);

/**
 * @brief Create structural-immune bridge
 *
 * WHAT: Initialize bidirectional structural-immune integration
 * WHY:  Enable realistic immune-mediated pruning
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param structural_system Structural plasticity system
 * @return New bridge or NULL on failure
 */
structural_immune_bridge_t* structural_immune_bridge_create(
    const structural_immune_config_t* config,
    brain_immune_system_t* immune_system,
    structural_plasticity_system_t* structural_system
);

/**
 * @brief Destroy structural-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void structural_immune_bridge_destroy(structural_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Structural API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to structural plasticity
 *
 * WHAT: Modulate spine formation/pruning based on cytokine levels
 * WHY:  Pro-inflammatory cytokines impair spine formation
 * HOW:  Query immune system cytokines, reduce formation rate
 *
 * @param bridge Structural-immune bridge
 * @return 0 on success
 */
int structural_immune_apply_cytokine_effects(structural_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to structural plasticity
 *
 * WHAT: Reduce spine formation during inflammation
 * WHY:  Chronic inflammation impairs structural plasticity
 * HOW:  Check inflammation level, adjust formation rate
 *
 * @param bridge Structural-immune bridge
 * @return 0 on success
 */
int structural_immune_apply_inflammation_effects(structural_immune_bridge_t* bridge);

/**
 * @brief Perform microglia-mediated synaptic pruning
 *
 * WHAT: Tag and eliminate weak synapses via complement system
 * WHY:  Microglia actively prune inactive synapses
 * HOW:  Identify weak spines, tag with complement, eliminate
 *
 * @param bridge Structural-immune bridge
 * @return 0 on success
 */
int structural_immune_microglia_prune(structural_immune_bridge_t* bridge);

/**
 * @brief Tag weak spines with complement (C1q/C3)
 *
 * WHAT: Mark inactive synapses for elimination
 * WHY:  Complement tagging targets spines for microglia
 * HOW:  Identify low-activity spines, apply complement tag
 *
 * @param bridge Structural-immune bridge
 * @return Number of spines tagged
 */
uint32_t structural_immune_tag_weak_spines(structural_immune_bridge_t* bridge);

/* ============================================================================
 * Structural → Immune API
 * ============================================================================ */

/**
 * @brief Signal spine density to immune system
 *
 * WHAT: Inform immune system of current spine density
 * WHY:  High density triggers increased pruning
 * HOW:  Compute density, send signal to immune system
 *
 * @param bridge Structural-immune bridge
 * @return 0 on success
 */
int structural_immune_signal_density(structural_immune_bridge_t* bridge);

/**
 * @brief Request immune pruning for specific synapse
 *
 * WHAT: Ask immune system to prune a specific synapse
 * WHY:  Structural system identifies candidates for elimination
 * HOW:  Send complement tagging request to immune system
 *
 * @param bridge Structural-immune bridge
 * @param synapse_id Synapse to prune
 * @return 0 on success
 */
int structural_immune_request_pruning(
    structural_immune_bridge_t* bridge,
    uint32_t synapse_id
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update structural-immune bridge (both directions)
 *
 * WHAT: Process all structural-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, perform microglia pruning, signal density
 *
 * @param bridge Structural-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int structural_immune_bridge_update(
    structural_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine structural effects
 *
 * @param bridge Structural-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int structural_immune_get_cytokine_effects(
    const structural_immune_bridge_t* bridge,
    cytokine_structural_effects_t* effects
);

/**
 * @brief Get current microglia pruning state
 *
 * @param bridge Structural-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int structural_immune_get_microglia_state(
    const structural_immune_bridge_t* bridge,
    microglia_pruning_state_t* state
);

/**
 * @brief Get formation impairment from inflammation
 *
 * @param bridge Structural-immune bridge
 * @return Formation factor [0-1] (1.0 = normal, 0.0 = blocked)
 */
float structural_immune_get_formation_factor(
    const structural_immune_bridge_t* bridge
);

/**
 * @brief Get pruning boost from inflammation
 *
 * @param bridge Structural-immune bridge
 * @return Pruning factor [1.0+] (1.0 = baseline)
 */
float structural_immune_get_pruning_factor(
    const structural_immune_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_STRUCTURAL
 *
 * @param bridge Structural-immune bridge
 * @return 0 on success, -1 on error
 */
int structural_immune_connect_bio_async(structural_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Structural-immune bridge
 * @return 0 on success
 */
int structural_immune_disconnect_bio_async(structural_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Structural-immune bridge
 * @return true if connected
 */
bool structural_immune_is_bio_async_connected(
    const structural_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STRUCTURAL_IMMUNE_BRIDGE_H */
