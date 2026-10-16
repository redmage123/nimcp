/**
 * @file nimcp_portia_attention_immune_bridge.h
 * @brief Portia Attention-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and Portia attention/resource allocation
 * WHY:  Biological evidence shows inflammation impairs resource management and allocation;
 *       resource depletion triggers metabolic immune suppression.
 * HOW:  Cytokines reduce resource budgets and reallocation efficiency; resource depletion
 *       triggers metabolic immune suppression and stress responses.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → RESOURCE ALLOCATION PATHWAYS:
 * -------------------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Reduce available cognitive resources
 *    - Impair efficient resource allocation
 *    - Bias toward threat-related resource allocation
 *    - Reduce flexibility in resource reallocation
 *    - Reference: Harrison et al. (2009) "Inflammation and cognitive performance"
 *
 * 2. Fever and Inflammation:
 *    - Overall resource budget reduction (energy conservation)
 *    - Slower reallocation dynamics
 *    - Impaired priority management
 *    - Sticky allocations (hysteresis increase)
 *
 * 3. Metabolic Immune Activity:
 *    - Immune activation consumes 20-30% of metabolic resources
 *    - Reduces available resources for other systems
 *    - Forces resource reallocation to immune function
 *    - Reference: Straub (2012) "Evolutionary medicine and chronic inflammatory
 *      state: known and new concepts in pathophysiology"
 *
 * RESOURCE ALLOCATION → IMMUNE PATHWAYS:
 * -------------------------------------
 * 1. Resource Depletion:
 *    - Low available resources → metabolic suppression
 *    - Immune system downregulation to conserve energy
 *    - Reduced immune surveillance
 *    - Increased infection vulnerability
 *
 * 2. Resource Competition:
 *    - High resource demands → stress response
 *    - IL-6 release (resource scarcity signaling)
 *    - Mild inflammatory state
 *    - Preparatory immune mobilization
 *
 * 3. Preemption Events:
 *    - Frequent resource preemption → chronic stress
 *    - Sustained low-level inflammation
 *    - Impaired immune regulation
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              PORTIA ATTENTION-IMMUNE BRIDGE                                ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                IMMUNE → RESOURCE ALLOCATION                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.15│  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.2 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.25│         ├──→ Resource Budget Reduction          │  ║
 * ║   │   │              │         │    Allocation Efficiency Loss         │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │   ATTENTION SYSTEM              │                             │  ║
 * ║   │   │  - Reduced resource budget      │                             │  ║
 * ║   │   │  - Slower reallocation          │                             │  ║
 * ║   │   │  - Increased hysteresis         │                             │  ║
 * ║   │   │  - Threat allocation bias       │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION LEVEL     │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ LOCAL    → -10% budget   │                                     │  ║
 * ║   │   │ REGIONAL → -20% budget   │                                     │  ║
 * ║   │   │ SYSTEMIC → -40% budget   │                                     │  ║
 * ║   │   │ STORM    → -60% budget   │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                RESOURCE ALLOCATION → IMMUNE                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │LOW RESOURCES │──→ Metabolic Immune Suppression                 │  ║
 * ║   │   │DEPLETION     │──→ Reduced Surveillance                         │  ║
 * ║   │   │SCARCITY      │──→ IL-6 Release (Scarcity Signal)               │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │HIGH PREEMPT  │──→ Chronic Stress Inflammation                  │  ║
 * ║   │   │COMPETITION   │──→ IL-1β Release                                │  ║
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

#ifndef NIMCP_PORTIA_ATTENTION_IMMUNE_BRIDGE_H
#define NIMCP_PORTIA_ATTENTION_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "portia/nimcp_portia_attention.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine resource budget impact factors */
#define CYTOKINE_IL1_BUDGET_IMPACT           -0.15f  /**< IL-1β → budget reduction */
#define CYTOKINE_IL6_BUDGET_IMPACT           -0.2f   /**< IL-6 → budget reduction */
#define CYTOKINE_TNF_BUDGET_IMPACT           -0.25f  /**< TNF-α → strong budget reduction */
#define CYTOKINE_IFN_GAMMA_BUDGET_IMPACT     -0.1f   /**< IFN-γ → mild budget reduction */
#define CYTOKINE_IL10_BUDGET_RECOVERY        0.1f    /**< IL-10 → budget recovery */

/* Inflammation resource budget reduction */
#define INFLAMMATION_NONE_BUDGET_FACTOR      1.0f    /**< No reduction */
#define INFLAMMATION_LOCAL_BUDGET_FACTOR     0.9f    /**< -10% budget */
#define INFLAMMATION_REGIONAL_BUDGET_FACTOR  0.8f    /**< -20% budget */
#define INFLAMMATION_SYSTEMIC_BUDGET_FACTOR  0.6f    /**< -40% budget */
#define INFLAMMATION_STORM_BUDGET_FACTOR     0.4f    /**< -60% budget */

/* Inflammation allocation efficiency reduction */
#define INFLAMMATION_EFFICIENCY_BASE         0.05f   /**< Base efficiency loss */
#define INFLAMMATION_EFFICIENCY_PER_LEVEL    0.08f   /**< Per inflammation level */

/* Resource-triggered immune thresholds */
#define RESOURCE_DEPLETION_THRESHOLD         0.2f    /**< Low resources → suppression */
#define RESOURCE_SCARCITY_THRESHOLD          0.4f    /**< Scarcity → stress */
#define RESOURCE_PREEMPTION_THRESHOLD        5       /**< Preemptions → inflammation */

/* Resource immune response rates */
#define RESOURCE_DEPLETION_SUPPRESSION       0.3f    /**< Depletion → suppression */
#define RESOURCE_SCARCITY_IL6_RELEASE        0.1f    /**< Scarcity → IL-6 */
#define RESOURCE_PREEMPTION_IL1_RELEASE      0.08f   /**< Preemption → IL-1β */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine resource allocation effects
 *
 * How cytokine levels impair resource management
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_budget_reduction;          /**< IL-1β budget loss */
    float il6_budget_reduction;          /**< IL-6 budget loss */
    float tnf_budget_reduction;          /**< TNF-α budget loss */
    float ifn_gamma_budget_reduction;    /**< IFN-γ budget loss */

    /* Anti-inflammatory effects */
    float il10_budget_recovery;          /**< IL-10 budget recovery */

    /* Aggregate effects */
    float total_budget_factor;           /**< Combined budget multiplier [0-1] */
    float allocation_efficiency_loss;    /**< Allocation efficiency loss [0-1] */
    float reallocation_delay;            /**< Reallocation slowdown [0-1] */
    float hysteresis_increase;           /**< Hysteresis factor increase [0-1] */
} cytokine_attention_effects_t;

/**
 * @brief Inflammation resource allocation state
 *
 * How chronic inflammation affects resource allocation
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;     /**< How long inflamed */
    bool is_chronic;                     /**< >= threshold */

    /* Resource impacts */
    float budget_factor;                 /**< Overall budget multiplier [0-1] */
    float efficiency_loss;               /**< Allocation efficiency loss [0-1] */
    float priority_impairment;           /**< Priority management deficit [0-1] */
    float reallocation_slowdown;         /**< Reallocation speed loss [0-1] */

    /* Allocation biases */
    float threat_allocation_bias;        /**< Bias toward threat resources [0-1] */
    float flexibility_reduction;         /**< Reduced reallocation flexibility [0-1] */
} inflammation_attention_state_t;

/**
 * @brief Resource-driven immune modulation
 *
 * How resource allocation state affects immune function
 */
typedef struct {
    /* Resource state */
    float available_resources;           /**< Current resource availability [0-1] */
    float resource_utilization;          /**< Resource usage ratio [0-1] */
    uint32_t recent_preemptions;         /**< Recent preemption count */
    float resource_competition_level;    /**< Competition level [0-1] */

    /* Immune effects */
    float depletion_immune_suppression;  /**< Depletion → suppression [0-1] */
    float scarcity_stress_level;         /**< Scarcity stress [0-1] */
    float preemption_stress;             /**< Preemption stress [0-1] */

    /* Cytokine releases */
    float il6_release_from_scarcity;     /**< IL-6 from scarcity */
    float il1_release_from_preemption;   /**< IL-1β from preemption */
    bool chronic_preemption_inflammation; /**< Chronic competition inflammation */
} attention_immune_modulation_t;

/**
 * @brief Attention-immune bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_budget_reduction;
    bool enable_inflammation_allocation_impairment;
    bool enable_resource_depletion_suppression;
    bool enable_resource_scarcity_stress;
    bool enable_preemption_inflammation;

    /* Sensitivity tuning */
    float cytokine_sensitivity;          /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;      /**< Inflammation effect multiplier [0.5-2.0] */
    float resource_immune_sensitivity;   /**< Resource→immune multiplier [0.5-2.0] */

    /* Thresholds */
    float depletion_threshold;           /**< Depletion threshold [0.1-0.3] */
    float scarcity_threshold;            /**< Scarcity threshold [0.3-0.5] */
    uint32_t preemption_threshold;       /**< Preemption count threshold [3-10] */
} portia_attention_immune_config_t;

/**
 * @brief Complete attention-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    portia_attention_state_t attention_system;

    /* Configuration */
    portia_attention_immune_config_t config;

    /* Current state */
    cytokine_attention_effects_t cytokine_effects;
    inflammation_attention_state_t inflammation_state;
    attention_immune_modulation_t attention_modulation;

    /* Timing */
    uint64_t last_update_time;
    uint32_t preemption_counter;         /**< Recent preemption count */

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_impairments;
    uint32_t depletion_suppressions;
    uint32_t scarcity_stresses;
    uint32_t preemption_inflammations;

    } portia_attention_immune_bridge_t;

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
int portia_attention_immune_default_config(portia_attention_immune_config_t* config);

/**
 * @brief Create attention-immune bridge
 *
 * WHAT: Initialize bidirectional attention-immune integration
 * WHY:  Enable realistic immune-resource allocation coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param attention_system Portia attention system
 * @return New bridge or NULL on failure
 */
portia_attention_immune_bridge_t* portia_attention_immune_create(
    const portia_attention_immune_config_t* config,
    brain_immune_system_t* immune_system,
    portia_attention_state_t attention_system
);

/**
 * @brief Destroy attention-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void portia_attention_immune_destroy(portia_attention_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Attention API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to resource allocation
 *
 * WHAT: Reduce resource budget and efficiency based on cytokines
 * WHY:  Immune activation reduces available cognitive resources
 * HOW:  Query immune system cytokines, adjust resource budget
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success
 */
int portia_attention_immune_apply_cytokine_effects(portia_attention_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to resource allocation
 *
 * WHAT: Reduce budget and allocation efficiency from inflammation
 * WHY:  Inflammation consumes metabolic resources
 * HOW:  Check inflammation level, adjust allocation parameters
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success
 */
int portia_attention_immune_apply_inflammation_effects(portia_attention_immune_bridge_t* bridge);

/**
 * @brief Compute resource budget factor from immune state
 *
 * WHAT: Calculate budget multiplier given immune status
 * WHY:  Immune activation reduces available resources
 * HOW:  Map inflammation level to budget factor [0-1]
 *
 * @param bridge Attention-immune bridge
 * @return Budget factor [0-1] (1.0 = normal, 0.0 = depleted)
 */
float portia_attention_immune_compute_budget_factor(
    const portia_attention_immune_bridge_t* bridge
);

/**
 * @brief Compute allocation efficiency loss from inflammation
 *
 * WHAT: Calculate how much inflammation impairs allocation efficiency
 * WHY:  Inflammation reduces cognitive flexibility
 * HOW:  Map inflammation level to efficiency loss
 *
 * @param bridge Attention-immune bridge
 * @return Efficiency loss [0-1] (0 = no loss, 1 = complete impairment)
 */
float portia_attention_immune_compute_efficiency_loss(
    const portia_attention_immune_bridge_t* bridge
);

/* ============================================================================
 * Attention → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune suppression from resource depletion
 *
 * WHAT: Suppress immune activity from low available resources
 * WHY:  Resource depletion forces metabolic conservation
 * HOW:  Check resource availability, suppress immune if depleted
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success
 */
int portia_attention_immune_trigger_depletion_suppression(
    portia_attention_immune_bridge_t* bridge
);

/**
 * @brief Trigger stress response from resource scarcity
 *
 * WHAT: Activate stress immune response from resource competition
 * WHY:  Resource scarcity activates stress cascade
 * HOW:  Check resource utilization, release IL-6 if scarce
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success
 */
int portia_attention_immune_trigger_scarcity_stress(portia_attention_immune_bridge_t* bridge);

/**
 * @brief Trigger inflammation from repeated preemptions
 *
 * WHAT: Activate chronic inflammation from resource competition
 * WHY:  Repeated preemption → chronic stress → inflammation
 * HOW:  Track preemptions, trigger inflammation if threshold exceeded
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success
 */
int portia_attention_immune_trigger_preemption_inflammation(
    portia_attention_immune_bridge_t* bridge
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update attention-immune bridge (both directions)
 *
 * WHAT: Process all attention-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from resources, adjust parameters
 *
 * @param bridge Attention-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int portia_attention_immune_update(portia_attention_immune_bridge_t* bridge, uint64_t delta_ms);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine attention effects
 *
 * @param bridge Attention-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int portia_attention_immune_get_cytokine_effects(
    const portia_attention_immune_bridge_t* bridge,
    cytokine_attention_effects_t* effects
);

/**
 * @brief Get current inflammation attention state
 *
 * @param bridge Attention-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int portia_attention_immune_get_inflammation_state(
    const portia_attention_immune_bridge_t* bridge,
    inflammation_attention_state_t* state
);

/**
 * @brief Check if experiencing resource deficit from inflammation
 *
 * WHAT: Determine if inflammation causing significant resource reduction
 * WHY:  Detect clinically significant resource impacts
 * HOW:  Check budget reduction threshold
 *
 * @param bridge Attention-immune bridge
 * @return true if significant deficit (>20% budget loss)
 */
bool portia_attention_immune_has_resource_deficit(
    const portia_attention_immune_bridge_t* bridge
);

/**
 * @brief Get current resource budget factor
 *
 * @param bridge Attention-immune bridge
 * @return Budget factor [0-1]
 */
float portia_attention_immune_get_budget_factor(const portia_attention_immune_bridge_t* bridge);

/**
 * @brief Get current allocation efficiency loss
 *
 * @param bridge Attention-immune bridge
 * @return Efficiency loss [0-1]
 */
float portia_attention_immune_get_efficiency_loss(const portia_attention_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_PORTIA_ATTENTION
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success, -1 on error
 */
int portia_attention_immune_connect_bio_async(portia_attention_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success
 */
int portia_attention_immune_disconnect_bio_async(portia_attention_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Attention-immune bridge
 * @return true if connected
 */
bool portia_attention_immune_is_bio_async_connected(
    const portia_attention_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PORTIA_ATTENTION_IMMUNE_BRIDGE_H */
