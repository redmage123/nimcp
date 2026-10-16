/**
 * @file nimcp_substrate_immune_bridge.h
 * @brief Neural Substrate-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and neural substrate
 * WHY:  Inflammation affects metabolic/physical substrate (fever, ATP depletion,
 *       membrane damage). Substrate stress can trigger immune responses.
 * HOW:  Cytokines modulate temperature/energy/membrane; substrate alerts trigger immune.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → SUBSTRATE PATHWAYS:
 * ----------------------------
 * 1. Cytokine Effects on Metabolism:
 *    - IL-1β increases metabolic rate (sickness behavior)
 *    - TNF-α impairs mitochondrial function → ATP depletion
 *    - IFN-γ increases oxygen consumption
 *    - Reference: Tracey (2002) "The inflammatory reflex"
 *
 * 2. Fever Response:
 *    - IL-1β, IL-6, TNF-α are pyrogens
 *    - Temperature increase affects all substrate processes
 *    - Hyperthermia threshold protection
 *    - Reference: Blatteis (2006) "Endotoxic fever"
 *
 * 3. Membrane Damage:
 *    - Complement system (MAC) damages membranes
 *    - Reactive oxygen species (ROS) cause lipid peroxidation
 *    - Ion channel dysfunction
 *    - Reference: Bhakdi & Tranum-Jensen (1991) "Complement lysis"
 *
 * 4. Ion Dysregulation:
 *    - Inflammation disrupts Na+/K+-ATPase function
 *    - Ca2+ overload from excitotoxicity
 *    - Reference: Bhardwaj et al. (2016) "Ion homeostasis"
 *
 * SUBSTRATE → IMMUNE PATHWAYS:
 * ----------------------------
 * 1. Metabolic Stress as Danger Signal:
 *    - ATP depletion releases DAMPs (damage-associated molecular patterns)
 *    - Hypoxia activates HIF-1α → immune gene expression
 *    - Reference: Zhang et al. (2010) "Circulating mitochondrial DAMPs"
 *
 * 2. Membrane Damage Recognition:
 *    - Exposed phosphatidylserine triggers phagocytosis
 *    - Leaked intracellular contents activate innate immunity
 *    - Reference: Ravichandran (2011) "Engulfment of dying cells"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SUBSTRATE-IMMUNE INTEGRATION BRIDGE                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               IMMUNE → SUBSTRATE PATHWAYS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐          │  ║
 * ║   │   │   IL-1β      │   │   TNF-α      │   │   IL-6       │          │  ║
 * ║   │   │ ──────────── │   │ ──────────── │   │ ──────────── │          │  ║
 * ║   │   │ +Temp        │   │ -ATP         │   │ +Temp        │          │  ║
 * ║   │   │ +Metabolic   │   │ -Membrane    │   │ +Metabolic   │          │  ║
 * ║   │   └──────┬───────┘   └──────┬───────┘   └──────┬───────┘          │  ║
 * ║   │          └──────────────────┴──────────────────┘                   │  ║
 * ║   │                             ↓                                      │  ║
 * ║   │   ┌─────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │              SUBSTRATE MODULATION                           │ │  ║
 * ║   │   │  • Temperature increase (fever)                             │ │  ║
 * ║   │   │  • ATP depletion (metabolic burden)                         │ │  ║
 * ║   │   │  • Membrane integrity loss (cytotoxicity)                   │ │  ║
 * ║   │   │  • Ion imbalance (pump dysfunction)                         │ │  ║
 * ║   │   └─────────────────────────────────────────────────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               SUBSTRATE → IMMUNE PATHWAYS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐          │  ║
 * ║   │   │ LOW_ATP      │   │ MEMBRANE_DMG │   │ ION_IMBAL    │          │  ║
 * ║   │   │ Alert        │   │ Alert        │   │ Alert        │          │  ║
 * ║   │   └──────┬───────┘   └──────┬───────┘   └──────┬───────┘          │  ║
 * ║   │          └──────────────────┴──────────────────┘                   │  ║
 * ║   │                             ↓                                      │  ║
 * ║   │   ┌─────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │             IMMUNE RESPONSE TRIGGER                         │ │  ║
 * ║   │   │  • Present DAMP as antigen                                  │ │  ║
 * ║   │   │  • Activate innate immunity                                 │ │  ║
 * ║   │   │  • Initiate repair response                                 │ │  ║
 * ║   │   └─────────────────────────────────────────────────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SUBSTRATE_IMMUNE_BRIDGE_H
#define NIMCP_SUBSTRATE_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Thread utilities */
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine metabolic effects */
#define CYTOKINE_IL1_TEMP_INCREASE          0.5f    /**< IL-1β: +0.5°C per unit */
#define CYTOKINE_IL1_METABOLIC_INCREASE     0.15f   /**< IL-1β: +15% metabolic rate */
#define CYTOKINE_IL6_TEMP_INCREASE          0.4f    /**< IL-6: +0.4°C per unit */
#define CYTOKINE_IL6_METABOLIC_INCREASE     0.10f   /**< IL-6: +10% metabolic rate */
#define CYTOKINE_TNF_ATP_DEPLETION          0.20f   /**< TNF-α: -20% ATP per unit */
#define CYTOKINE_TNF_MEMBRANE_DAMAGE        0.10f   /**< TNF-α: -10% membrane */
#define CYTOKINE_IFN_O2_CONSUMPTION         0.15f   /**< IFN-γ: +15% O2 consumption */

/* Inflammation level effects */
#define INFLAMMATION_LOCAL_TEMP             0.3f    /**< Local: +0.3°C */
#define INFLAMMATION_REGIONAL_TEMP          0.8f    /**< Regional: +0.8°C */
#define INFLAMMATION_SYSTEMIC_TEMP          1.5f    /**< Systemic: +1.5°C */
#define INFLAMMATION_STORM_TEMP             2.5f    /**< Storm: +2.5°C */

/* IL-10 recovery effects */
#define IL10_TEMPERATURE_REDUCTION          0.3f    /**< IL-10 cooling effect */
#define IL10_ATP_RECOVERY_BOOST             0.10f   /**< IL-10 ATP recovery boost */
#define IL10_MEMBRANE_REPAIR_BOOST          0.05f   /**< IL-10 membrane repair boost */

/* Substrate alert thresholds for immune trigger */
#define SUBSTRATE_ALERT_IMMUNE_THRESHOLD    2       /**< 2 alerts trigger immune */
#define SUBSTRATE_DAMP_SEVERITY_LOW_ATP     7       /**< ATP depletion severity */
#define SUBSTRATE_DAMP_SEVERITY_MEMBRANE    8       /**< Membrane damage severity */
#define SUBSTRATE_DAMP_SEVERITY_ION         5       /**< Ion imbalance severity */

/* Bio-async module ID */
#define BIO_MODULE_IMMUNE_SUBSTRATE         0x0D21  /**< Bio-async module ID */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine substrate effects
 */
typedef struct {
    /* Temperature effects */
    float il1_temp_effect;        /**< IL-1β temperature increase */
    float il6_temp_effect;        /**< IL-6 temperature increase */
    float total_temp_increase;    /**< Combined temperature effect */

    /* Metabolic effects */
    float il1_metabolic_effect;   /**< IL-1β metabolic increase */
    float il6_metabolic_effect;   /**< IL-6 metabolic increase */
    float tnf_atp_effect;         /**< TNF-α ATP depletion */
    float ifn_o2_effect;          /**< IFN-γ O2 consumption */

    /* Damage effects */
    float tnf_membrane_effect;    /**< TNF-α membrane damage */
    float ion_imbalance_effect;   /**< Cytokine-induced ion imbalance */

    /* Composite scores */
    float fever_intensity;        /**< Overall fever [0-1] */
    float metabolic_burden;       /**< Metabolic load [0-1] */
    float damage_severity;        /**< Cellular damage [0-1] */
} cytokine_substrate_effects_t;

/**
 * @brief Substrate stress immune trigger state
 */
typedef struct {
    /* Alert tracking */
    uint32_t consecutive_alerts;  /**< Consecutive alert cycles */
    bool immune_triggered;        /**< Has immune been triggered */
    uint32_t antigen_id;          /**< ID of presented DAMP */

    /* Alert types present */
    bool atp_alert;               /**< Low ATP alert active */
    bool membrane_alert;          /**< Membrane damage alert */
    bool ion_alert;               /**< Ion imbalance alert */
    bool hyperthermia_alert;      /**< Hyperthermia alert */

    /* Severity assessment */
    uint32_t computed_severity;   /**< Severity for immune [1-10] */
} substrate_immune_trigger_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint32_t fever_cycles;
    uint32_t atp_depletions;
    uint32_t membrane_damages;
    uint32_t immune_triggers;
    uint32_t il10_recoveries;
    float max_temperature;
    float min_atp_level;
} substrate_immune_stats_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_fever_response;
    bool enable_metabolic_effects;
    bool enable_damage_effects;
    bool enable_substrate_immune_trigger;
    bool enable_il10_recovery;
    bool enable_bio_async;

    /* Sensitivity multipliers */
    float temperature_sensitivity;
    float metabolic_sensitivity;
    float damage_sensitivity;

    /* Thresholds */
    uint32_t alert_persistence_threshold;
    float max_fever_temperature;
} substrate_immune_config_t;

/**
 * @brief Complete substrate-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    neural_substrate_t* substrate;
    brain_immune_system_t* immune_system;

    /* Current effects */
    cytokine_substrate_effects_t cytokine_effects;
    substrate_immune_trigger_t trigger_state;

    /* Configuration */
    substrate_immune_config_t config;

    /* Statistics */
    substrate_immune_stats_t stats;

    } substrate_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int substrate_immune_default_config(substrate_immune_config_t* config);

/**
 * @brief Create substrate-immune bridge
 *
 * @param config Configuration (NULL for defaults)
 * @param substrate Neural substrate
 * @param immune_system Brain immune system
 * @return New bridge or NULL on failure
 */
substrate_immune_bridge_t* substrate_immune_bridge_create(
    const substrate_immune_config_t* config,
    neural_substrate_t* substrate,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy substrate-immune bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void substrate_immune_bridge_destroy(substrate_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Substrate immune bridge
 * @return 0 on success, -1 on error
 */
int substrate_immune_connect_bio_async(substrate_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Substrate immune bridge
 * @return 0 on success, -1 on error
 */
int substrate_immune_disconnect_bio_async(substrate_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Substrate immune bridge
 * @return true if connected
 */
bool substrate_immune_is_bio_async_connected(const substrate_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Substrate API
 * ============================================================================ */

/**
 * @brief Apply fever response
 *
 * WHAT: Increase substrate temperature based on cytokines
 * WHY:  IL-1β, IL-6, TNF-α are endogenous pyrogens
 * HOW:  Query cytokine levels, compute temperature increase
 *
 * @param bridge Substrate immune bridge
 * @return 0 on success
 */
int substrate_immune_apply_fever(substrate_immune_bridge_t* bridge);

/**
 * @brief Apply metabolic effects
 *
 * WHAT: Modulate ATP/O2/glucose based on immune activity
 * WHY:  Inflammation has metabolic cost
 * HOW:  Deplete ATP proportional to cytokine levels
 *
 * @param bridge Substrate immune bridge
 * @return 0 on success
 */
int substrate_immune_apply_metabolic_effects(substrate_immune_bridge_t* bridge);

/**
 * @brief Apply damage effects
 *
 * WHAT: Apply membrane/ion damage from inflammation
 * WHY:  Cytotoxic immune responses damage substrate
 * HOW:  Reduce membrane integrity, disrupt ions
 *
 * @param bridge Substrate immune bridge
 * @return 0 on success
 */
int substrate_immune_apply_damage(substrate_immune_bridge_t* bridge);

/**
 * @brief Apply IL-10 recovery effects
 *
 * WHAT: Restore substrate with anti-inflammatory IL-10
 * WHY:  IL-10 promotes recovery from inflammation
 * HOW:  Reduce temperature, boost recovery rates
 *
 * @param bridge Substrate immune bridge
 * @param il10_concentration IL-10 level [0-1]
 * @return 0 on success
 */
int substrate_immune_apply_il10_recovery(
    substrate_immune_bridge_t* bridge,
    float il10_concentration
);

/* ============================================================================
 * Substrate → Immune API
 * ============================================================================ */

/**
 * @brief Check substrate alerts for immune trigger
 *
 * WHAT: Monitor substrate alerts for immune activation
 * WHY:  Substrate stress releases DAMPs
 * HOW:  Check alert history, trigger if persistent
 *
 * @param bridge Substrate immune bridge
 * @return true if immune should be triggered
 */
bool substrate_immune_check_stress(substrate_immune_bridge_t* bridge);

/**
 * @brief Trigger immune response from substrate stress
 *
 * WHAT: Present DAMP to immune system
 * WHY:  Cellular damage should activate immunity
 * HOW:  Create epitope from substrate state, present to immune
 *
 * @param bridge Substrate immune bridge
 * @return 0 on success, -1 on error
 */
int substrate_immune_trigger_response(substrate_immune_bridge_t* bridge);

/**
 * @brief Compute stress severity for immune system
 *
 * @param bridge Substrate immune bridge
 * @return Severity score [1-10]
 */
uint32_t substrate_immune_compute_severity(const substrate_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update substrate-immune bridge (both directions)
 *
 * WHAT: Process all substrate-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply immune effects, check for triggers
 *
 * @param bridge Substrate immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int substrate_immune_bridge_update(
    substrate_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine effects on substrate
 *
 * @param bridge Substrate immune bridge
 * @param effects Output effects
 * @return 0 on success
 */
int substrate_immune_get_cytokine_effects(
    const substrate_immune_bridge_t* bridge,
    cytokine_substrate_effects_t* effects
);

/**
 * @brief Get substrate immune trigger state
 *
 * @param bridge Substrate immune bridge
 * @param trigger Output trigger state
 * @return 0 on success
 */
int substrate_immune_get_trigger_state(
    const substrate_immune_bridge_t* bridge,
    substrate_immune_trigger_t* trigger
);

/**
 * @brief Check if substrate is under immune modulation
 *
 * @param bridge Substrate immune bridge
 * @return true if modulated
 */
bool substrate_immune_is_modulated(const substrate_immune_bridge_t* bridge);

/**
 * @brief Get fever intensity
 *
 * @param bridge Substrate immune bridge
 * @return Fever intensity [0-1]
 */
float substrate_immune_get_fever_intensity(const substrate_immune_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Substrate immune bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int substrate_immune_get_stats(
    const substrate_immune_bridge_t* bridge,
    substrate_immune_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SUBSTRATE_IMMUNE_BRIDGE_H */
