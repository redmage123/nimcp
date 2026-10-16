/**
 * @file nimcp_metabolic_immune_bridge.h
 * @brief Metabolic Plasticity-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between brain immune system and metabolic plasticity
 * WHY:  Inflammation increases metabolic demand, ATP depletion impairs immune function
 * HOW:  Cytokines increase energy costs, inflammation modulates ATP recovery
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → METABOLIC PATHWAYS:
 * ----------------------------
 * 1. Pro-inflammatory Cytokines Increase Metabolic Demand:
 *    - IL-1β, IL-6, TNF-α increase baseline energy consumption
 *    - Cytokine signaling cascades consume ATP
 *    - Microglial activation is energy-intensive
 *    - Reference: Schöll et al. (2020) "PET imaging of tau deposition and inflammation"
 *
 * 2. Inflammation Increases Plasticity Costs:
 *    - NONE: 1.0x energy costs (baseline)
 *    - LOCAL: 1.2x energy costs
 *    - REGIONAL: 1.5x energy costs
 *    - SYSTEMIC: 2.0x energy costs
 *    - STORM: 3.0x energy costs (severe metabolic burden)
 *    - Reference: Camandola & Mattson (2017) "Brain metabolism in health, aging, and disease"
 *
 * 3. Cytokine-Specific Metabolic Effects:
 *    - IL-1β: +20% baseline ATP consumption
 *    - IL-6: +15% baseline ATP consumption
 *    - TNF-α: +25% baseline ATP consumption
 *    - IFN-γ: +10% baseline ATP consumption
 *    - IL-10: -10% ATP consumption (anti-inflammatory)
 *
 * 4. Inflammation Impairs Mitochondrial Function:
 *    - Regional+ inflammation → reduced ATP synthesis
 *    - Oxidative stress damages mitochondria
 *    - Recovery rate reduced by 10-30%
 *    - Reference: Readnower et al. (2011) "Mitochondrial dysfunction in neuroinflammation"
 *
 * METABOLIC → IMMUNE PATHWAYS:
 * ----------------------------
 * 1. ATP Depletion Impairs Immune Function:
 *    - Low ATP → reduced cytokine production
 *    - Energy deficit limits immune response capacity
 *    - Critical ATP (<30%) → immune suppression
 *
 * 2. Energy Depletion as Immune Threat:
 *    - Severe ATP depletion triggers immune alert
 *    - Metabolic crisis is homeostatic threat
 *    - May trigger protective anti-inflammatory response
 *
 * 3. Metabolic State Modulates Inflammation:
 *    - Healthy ATP → normal immune function
 *    - Depleted ATP → reduced immune activity
 *    - Emergency state → anti-inflammatory shift
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              METABOLIC-IMMUNE INTEGRATION BRIDGE                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   INFLAMMATION     Cost       Recovery    Effect                          ║
 * ║   ──────────────────────────────────────────────────────────────────      ║
 * ║   NONE             1.0x       1.0x        Normal metabolism               ║
 * ║   LOCAL            1.2x       0.95x       Mild metabolic stress           ║
 * ║   REGIONAL         1.5x       0.80x       Moderate stress                 ║
 * ║   SYSTEMIC         2.0x       0.70x       Severe metabolic burden         ║
 * ║   STORM            3.0x       0.50x       Critical energy crisis          ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * REFERENCES:
 * -----------
 * - Schöll, M., et al. (2020). PET imaging of tau deposition in the aging human brain.
 *   Neuron 79(6), 1094-1108.
 * - Camandola, S., & Mattson, M.P. (2017). Brain metabolism in health, aging, and
 *   neurodegeneration. EMBO Journal 36(11), 1474-1492.
 * - Readnower, R.D., et al. (2011). Mitochondrial dysfunction and oxidative stress
 *   contribute to neuronal death. Brain Research 1434, 89-96.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_METABOLIC_IMMUNE_BRIDGE_H
#define NIMCP_METABOLIC_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/metabolic/nimcp_metabolic_plasticity.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine metabolic burden (% increase in baseline ATP consumption) */
#define CYTOKINE_IL1_METABOLIC_BURDEN      0.20f   /**< IL-1β +20% consumption */
#define CYTOKINE_IL6_METABOLIC_BURDEN      0.15f   /**< IL-6 +15% consumption */
#define CYTOKINE_TNF_METABOLIC_BURDEN      0.25f   /**< TNF-α +25% consumption */
#define CYTOKINE_IFN_GAMMA_METABOLIC_BURDEN 0.10f  /**< IFN-γ +10% consumption */
#define CYTOKINE_IL10_METABOLIC_RELIEF     -0.10f  /**< IL-10 -10% consumption */

/* Inflammation-based energy cost modulation */
#define INFLAMMATION_COST_NONE             1.0f    /**< 1.0x energy costs */
#define INFLAMMATION_COST_LOCAL            1.2f    /**< 1.2x energy costs */
#define INFLAMMATION_COST_REGIONAL         1.5f    /**< 1.5x energy costs */
#define INFLAMMATION_COST_SYSTEMIC         2.0f    /**< 2.0x energy costs */
#define INFLAMMATION_COST_STORM            3.0f    /**< 3.0x energy costs */

/* Inflammation-based recovery impairment */
#define INFLAMMATION_RECOVERY_NONE         1.0f    /**< No impairment */
#define INFLAMMATION_RECOVERY_LOCAL        0.95f   /**< 5% reduction */
#define INFLAMMATION_RECOVERY_REGIONAL     0.80f   /**< 20% reduction */
#define INFLAMMATION_RECOVERY_SYSTEMIC     0.70f   /**< 30% reduction */
#define INFLAMMATION_RECOVERY_STORM        0.50f   /**< 50% reduction */

/* ATP depletion thresholds for immune impairment */
#define METABOLIC_IMMUNE_ATP_HEALTHY       70.0f   /**< Full immune function */
#define METABOLIC_IMMUNE_ATP_IMPAIRED      50.0f   /**< Reduced immune function */
#define METABOLIC_IMMUNE_ATP_SUPPRESSED    30.0f   /**< Severe immune suppression */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on metabolic parameters
 */
typedef struct {
    /* Pro-inflammatory metabolic burden */
    float il1_burden;            /**< IL-1β metabolic burden */
    float il6_burden;            /**< IL-6 metabolic burden */
    float tnf_burden;            /**< TNF-α metabolic burden */
    float ifn_gamma_burden;      /**< IFN-γ metabolic burden */

    /* Anti-inflammatory relief */
    float il10_relief;           /**< IL-10 metabolic relief */

    /* Aggregate effects */
    float total_baseline_increase; /**< Total baseline ATP increase */
    float total_cost_multiplier;   /**< Multiply energy costs by this */
} cytokine_metabolic_effects_t;

/**
 * @brief Inflammation effects on metabolism
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;

    /* Metabolic impacts */
    float energy_cost_increase;    /**< Cost multiplier [1.0-3.0] */
    float recovery_impairment;     /**< Recovery reduction [0.5-1.0] */
    float baseline_consumption;    /**< Baseline ATP drain rate */

    /* Mitochondrial dysfunction */
    bool mitochondrial_impaired;   /**< True for regional+ inflammation */
    float oxidative_stress_level;  /**< Oxidative stress [0-1] */
} inflammation_metabolic_state_t;

/**
 * @brief ATP effects on immune function
 */
typedef struct {
    energy_state_t energy_state;   /**< Current energy state */
    float atp_level;               /**< Current ATP [0-100] */

    /* Immune modulation */
    float immune_capacity;         /**< Immune function capacity [0-1] */
    float cytokine_production;     /**< Cytokine production capacity [0-1] */
    bool immune_suppressed;        /**< True if ATP < 30% */
} atp_immune_effects_t;

/**
 * @brief Complete metabolic-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    metabolic_plasticity_t* metabolic;

    /* Current state */
    cytokine_metabolic_effects_t cytokine_effects;
    inflammation_metabolic_state_t inflammation_state;
    atp_immune_effects_t atp_effects;

    /* Integration flags */
    bool enable_cytokine_metabolic_burden;
    bool enable_inflammation_cost_increase;
    bool enable_recovery_impairment;
    bool enable_atp_immune_feedback;

    /* Statistics */
    uint64_t total_updates;
    uint32_t inflammation_cost_increases;
    uint32_t immune_suppression_events;
    float total_extra_atp_consumed;  /**< Extra ATP consumed due to inflammation */

    } metabolic_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_metabolic_burden;
    bool enable_inflammation_cost_increase;
    bool enable_recovery_impairment;
    bool enable_atp_immune_feedback;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float atp_feedback_sensitivity;    /**< ATP feedback multiplier [0.5-2.0] */
} metabolic_immune_config_t;

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
int metabolic_immune_default_config(metabolic_immune_config_t* config);

/**
 * @brief Create metabolic-immune bridge
 *
 * WHAT: Initialize bidirectional metabolic-immune integration
 * WHY:  Enable realistic inflammation-metabolism coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param metabolic Metabolic plasticity system
 * @return New bridge or NULL on failure
 */
metabolic_immune_bridge_t* metabolic_immune_bridge_create(
    const metabolic_immune_config_t* config,
    brain_immune_system_t* immune_system,
    metabolic_plasticity_t* metabolic
);

/**
 * @brief Destroy metabolic-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void metabolic_immune_bridge_destroy(metabolic_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Metabolic API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to metabolic costs
 *
 * WHAT: Increase energy costs based on cytokine levels
 * WHY:  Cytokine signaling is energy-expensive
 * HOW:  Query immune system cytokines, adjust costs
 *
 * @param bridge Metabolic-immune bridge
 * @return 0 on success
 */
int metabolic_immune_apply_cytokine_effects(metabolic_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to metabolism
 *
 * WHAT: Increase costs and impair recovery from inflammation
 * WHY:  Inflammation increases metabolic burden
 * HOW:  Check inflammation level, modify costs and recovery
 *
 * @param bridge Metabolic-immune bridge
 * @return 0 on success
 */
int metabolic_immune_apply_inflammation_effects(metabolic_immune_bridge_t* bridge);

/**
 * @brief Get inflammation-modulated energy cost
 *
 * WHAT: Calculate effective energy cost with inflammation
 * WHY:  Inflammation increases plasticity costs
 * HOW:  Multiply base cost by inflammation factor
 *
 * @param bridge Metabolic-immune bridge
 * @param base_cost Original energy cost
 * @return Effective energy cost
 */
float metabolic_immune_get_effective_cost(
    const metabolic_immune_bridge_t* bridge,
    float base_cost
);

/**
 * @brief Get inflammation-modulated recovery rate
 *
 * WHAT: Calculate effective recovery rate with inflammation
 * WHY:  Inflammation impairs mitochondrial function
 * HOW:  Multiply base rate by recovery impairment factor
 *
 * @param bridge Metabolic-immune bridge
 * @param base_rate Original recovery rate
 * @return Effective recovery rate
 */
float metabolic_immune_get_effective_recovery(
    const metabolic_immune_bridge_t* bridge,
    float base_rate
);

/* ============================================================================
 * Metabolic → Immune API
 * ============================================================================ */

/**
 * @brief Update immune function based on ATP level
 *
 * WHAT: Modulate immune capacity by energy availability
 * WHY:  Immune function requires ATP
 * HOW:  Scale immune capacity by ATP percentage
 *
 * @param bridge Metabolic-immune bridge
 * @return 0 on success
 */
int metabolic_immune_update_atp_effects(metabolic_immune_bridge_t* bridge);

/**
 * @brief Check if immune function is impaired by ATP
 *
 * WHAT: Determine if low ATP is limiting immune function
 * WHY:  Need to know if energy is bottleneck
 * HOW:  Check if ATP < impairment threshold
 *
 * @param bridge Metabolic-immune bridge
 * @return true if impaired
 */
bool metabolic_immune_is_impaired_by_atp(const metabolic_immune_bridge_t* bridge);

/**
 * @brief Get ATP-based immune capacity
 *
 * WHAT: Calculate immune function capacity from ATP
 * WHY:  ATP limits immune response magnitude
 * HOW:  Map ATP level to capacity [0-1]
 *
 * @param bridge Metabolic-immune bridge
 * @return Immune capacity [0-1]
 */
float metabolic_immune_get_immune_capacity(const metabolic_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update metabolic-immune bridge (both directions)
 *
 * WHAT: Process all metabolic-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine/inflammation effects, update ATP feedback
 *
 * @param bridge Metabolic-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int metabolic_immune_bridge_update(
    metabolic_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine effects on metabolism
 *
 * @param bridge Metabolic-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int metabolic_immune_get_cytokine_effects(
    const metabolic_immune_bridge_t* bridge,
    cytokine_metabolic_effects_t* effects
);

/**
 * @brief Get current inflammation state
 *
 * @param bridge Metabolic-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int metabolic_immune_get_inflammation_state(
    const metabolic_immune_bridge_t* bridge,
    inflammation_metabolic_state_t* state
);

/**
 * @brief Get ATP effects on immune function
 *
 * @param bridge Metabolic-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int metabolic_immune_get_atp_effects(
    const metabolic_immune_bridge_t* bridge,
    atp_immune_effects_t* effects
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_METABOLIC
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int metabolic_immune_connect_bio_async(metabolic_immune_bridge_t* bridge);

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
int metabolic_immune_disconnect_bio_async(metabolic_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool metabolic_immune_is_bio_async_connected(const metabolic_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_METABOLIC_IMMUNE_BRIDGE_H */
