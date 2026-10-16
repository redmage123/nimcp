/**
 * @file nimcp_stdp_immune_bridge.h
 * @brief STDP Plasticity-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and STDP plasticity
 * WHY:  Biological evidence shows pro-inflammatory cytokines impair LTP/LTD,
 *       inflammation narrows STDP timing windows, and fever reduces synaptic learning.
 * HOW:  Cytokines modulate STDP learning rates and timing windows, chronic inflammation
 *       causes synaptic dysfunction, IL-10 restores plasticity capacity.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → STDP PATHWAYS:
 * ----------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Impair hippocampal LTP (long-term potentiation)
 *    - Reduce LTP magnitude by 20-50% depending on concentration
 *    - Narrow STDP timing window (τ reduced by 30-60%)
 *    - Increase LTD susceptibility
 *    - Reference: Pickering & O'Connor (2007) "Pro-inflammatory cytokines and cognitive function"
 *
 * 2. Inflammation Levels → Learning Rate:
 *    - NONE: 100% learning rate (baseline)
 *    - LOCAL: 90% learning rate (slight impairment)
 *    - REGIONAL: 70% learning rate (moderate impairment)
 *    - SYSTEMIC: 40% learning rate (severe impairment)
 *    - STORM: 10% learning rate (critical dysfunction)
 *    - Reference: Barrientos et al. (2009) "Inflammation impairs memory consolidation"
 *
 * 3. Chronic Inflammation:
 *    - Sustained elevation > 7 days → persistent LTP deficits
 *    - Synaptic scaling dysfunction
 *    - Reduced spine density and turnover
 *    - Impaired memory consolidation
 *    - Reference: Yirmiya & Goshen (2011) "Immune modulation of learning and memory"
 *
 * 4. Anti-inflammatory Cytokines (IL-10):
 *    - Restore LTP capacity
 *    - Normalize STDP timing windows
 *    - Protect against inflammation-induced plasticity deficits
 *    - Reference: Rizzo et al. (2018) "IL-10 restores synaptic plasticity"
 *
 * 5. Fever Effects:
 *    - Body temperature >38.5°C → reduced learning efficiency
 *    - Synaptic plasticity suppressed to conserve energy
 *    - Similar to systemic inflammation effects
 *    - Reference: Schultzberg et al. (1999) "Fever and learning deficits"
 *
 * STDP → IMMUNE PATHWAYS:
 * ----------------------
 * 1. Learning Instability Detection:
 *    - Excessive LTP without LTD → runaway excitation
 *    - Excessive LTD without LTP → synaptic failure
 *    - Rapid weight changes → homeostatic threat
 *    - Triggers immune surveillance of neural health
 *
 * 2. Synaptic Homeostasis Feedback:
 *    - Healthy LTP/LTD balance → anti-inflammatory signaling
 *    - Imbalanced plasticity → pro-inflammatory signaling
 *    - Supports neural integrity monitoring
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    STDP-IMMUNE BRIDGE                                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → STDP PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -30% │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -25% │         │                                       │  ║
 * ║   │   │ TNF-α → -40% │         ├──→ LTP Magnitude Reduction            │  ║
 * ║   │   │              │         │                                       │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     STDP PARAMETERS             │                             │  ║
 * ║   │   │  - Learning rate modulation     │                             │  ║
 * ║   │   │  - Timing window narrowing      │                             │  ║
 * ║   │   │  - LTP/LTD amplitude scaling    │                             │  ║
 * ║   │   │  - Trace decay acceleration     │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   +40%       │     Recovery, Plasticity Restoration            │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  STDP → IMMUNE PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  RUNAWAY LTP │ ──→ Excitation Threat Alert                    │  ║
 * ║   │   │  RUNAWAY LTD │ ──→ Synaptic Failure Alert                     │  ║
 * ║   │   │  INSTABILITY │ ──→ Homeostatic Threat                         │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  BALANCED    │ ──→ Anti-inflammatory Signaling                │  ║
 * ║   │   │  PLASTICITY  │ ──→ Synaptic Health                            │  ║
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

#ifndef NIMCP_STDP_IMMUNE_BRIDGE_H
#define NIMCP_STDP_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/stdp/nimcp_stdp.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine LTP impairment factors */
#define CYTOKINE_IL1_LTP_IMPAIRMENT      0.70f   /**< IL-1β reduces LTP to 70% */
#define CYTOKINE_IL6_LTP_IMPAIRMENT      0.75f   /**< IL-6 reduces LTP to 75% */
#define CYTOKINE_TNF_LTP_IMPAIRMENT      0.60f   /**< TNF-α reduces LTP to 60% */
#define CYTOKINE_IFN_GAMMA_LTP_IMPAIRMENT 0.80f  /**< IFN-γ reduces LTP to 80% */
#define CYTOKINE_IL10_LTP_RESTORATION    1.40f   /**< IL-10 restores LTP by 40% */

/* Inflammation-based learning rate modulation */
#define INFLAMMATION_LR_NONE             1.00f   /**< 100% learning rate */
#define INFLAMMATION_LR_LOCAL            0.90f   /**< 90% learning rate */
#define INFLAMMATION_LR_REGIONAL         0.70f   /**< 70% learning rate */
#define INFLAMMATION_LR_SYSTEMIC         0.40f   /**< 40% learning rate */
#define INFLAMMATION_LR_STORM            0.10f   /**< 10% learning rate */

/* STDP timing window narrowing factors */
#define INFLAMMATION_TAU_NARROWING_LOCAL    0.95f  /**< 5% narrowing */
#define INFLAMMATION_TAU_NARROWING_REGIONAL 0.70f  /**< 30% narrowing */
#define INFLAMMATION_TAU_NARROWING_SYSTEMIC 0.50f  /**< 50% narrowing */
#define INFLAMMATION_TAU_NARROWING_STORM    0.40f  /**< 60% narrowing */

/* STDP instability detection thresholds */
#define STDP_LTP_RUNAWAY_THRESHOLD       10.0f   /**< Total LTP for runaway detection */
#define STDP_LTD_RUNAWAY_THRESHOLD       10.0f   /**< Total LTD for runaway detection */
#define STDP_LTP_LTD_BALANCE_THRESHOLD   3.0f    /**< Ratio threshold for imbalance */
#define STDP_WEIGHT_CHANGE_RATE_THRESHOLD 0.5f   /**< Weight change per update threshold */

/* Chronic inflammation duration (seconds) */
#define CHRONIC_INFLAMMATION_THRESHOLD_SEC    (86400.0f * 7)  /**< 7 days = chronic */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on STDP parameters
 *
 * How cytokine levels modulate STDP learning and timing
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_ltp_impairment;        /**< IL-1β LTP reduction factor */
    float il6_ltp_impairment;        /**< IL-6 LTP reduction factor */
    float tnf_ltp_impairment;        /**< TNF-α LTP reduction factor */
    float ifn_gamma_ltp_impairment;  /**< IFN-γ LTP reduction factor */

    /* Anti-inflammatory effects */
    float il10_ltp_restoration;      /**< IL-10 LTP restoration factor */

    /* Aggregate effects */
    float total_ltp_modulation;      /**< Combined LTP scaling [0-2] */
    float total_ltd_modulation;      /**< Combined LTD scaling [0-2] */
    float learning_rate_factor;      /**< Overall LR multiplier [0-1] */
    float timing_window_factor;      /**< Tau scaling [0-1] */
} cytokine_stdp_effects_t;

/**
 * @brief Inflammation effects on STDP
 *
 * How chronic inflammation affects synaptic plasticity
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* STDP impacts */
    float ltp_capacity_reduction;      /**< LTP magnitude reduction [0-1] */
    float ltd_enhancement;             /**< LTD susceptibility increase [0-1] */
    float timing_window_narrowing;     /**< Tau reduction [0-1] */
    float learning_rate_suppression;   /**< LR reduction [0-1] */

    /* Chronic effects */
    float spine_density_loss;          /**< Synaptic loss [0-1] */
    float consolidation_impairment;    /**< Memory consolidation deficit [0-1] */
} inflammation_stdp_state_t;

/**
 * @brief STDP instability detection
 *
 * Monitoring synaptic plasticity health for immune alerting
 */
typedef struct {
    /* Plasticity state */
    float total_ltp_recent;            /**< Recent cumulative LTP */
    float total_ltd_recent;            /**< Recent cumulative LTD */
    float ltp_ltd_ratio;               /**< LTP/LTD balance */
    float weight_change_rate;          /**< Rate of weight changes */

    /* Instability flags */
    bool ltp_runaway_detected;         /**< Excessive LTP without LTD */
    bool ltd_runaway_detected;         /**< Excessive LTD without LTP */
    bool homeostatic_threat;           /**< Rapid weight changes */
    bool balanced_plasticity;          /**< Healthy LTP/LTD balance */

    /* Severity */
    float instability_severity;        /**< Threat level [0-1] */
} stdp_instability_state_t;

/**
 * @brief STDP modulation snapshot
 *
 * Current modulation state for STDP synapse
 */
typedef struct {
    /* Current modulation factors */
    float learning_rate_modulation;    /**< LR multiplier [0-1] */
    float a_plus_modulation;           /**< LTP amplitude scaling [0-2] */
    float a_minus_modulation;          /**< LTD amplitude scaling [0-2] */
    float tau_plus_modulation;         /**< LTP time constant scaling [0-1] */
    float tau_minus_modulation;        /**< LTD time constant scaling [0-1] */

    /* Effective parameters (original * modulation) */
    float effective_learning_rate;
    float effective_a_plus;
    float effective_a_minus;
    float effective_tau_plus;
    float effective_tau_minus;
} stdp_modulation_state_t;

/**
 * @brief Complete STDP-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    stdp_synapse_t* stdp_synapses;     /**< Array of STDP synapses */
    size_t num_synapses;               /**< Number of synapses */
    size_t synapse_capacity;           /**< Capacity of synapse array */

    /* Current state */
    cytokine_stdp_effects_t cytokine_effects;
    inflammation_stdp_state_t inflammation_state;
    stdp_instability_state_t instability_state;

    /* Base parameters (for restoration) */
    float base_learning_rate;
    float base_a_plus;
    float base_a_minus;
    float base_tau_plus;
    float base_tau_minus;

    /* Integration flags */
    bool enable_cytokine_stdp_modulation;
    bool enable_inflammation_impairment;
    bool enable_instability_detection;
    bool enable_homeostatic_feedback;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t instability_alerts;
    uint32_t plasticity_restorations;
    } stdp_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_stdp_modulation;
    bool enable_inflammation_impairment;
    bool enable_instability_detection;
    bool enable_homeostatic_feedback;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float instability_sensitivity;     /**< Instability detection multiplier [0.5-2.0] */

    /* Base STDP parameters */
    float base_learning_rate;
    float base_a_plus;
    float base_a_minus;
    float base_tau_plus;
    float base_tau_minus;

    /* Thresholds */
    float ltp_runaway_threshold;       /**< LTP accumulation threshold */
    float ltd_runaway_threshold;       /**< LTD accumulation threshold */
    float balance_threshold;           /**< LTP/LTD ratio threshold */
} stdp_immune_config_t;

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
int stdp_immune_default_config(stdp_immune_config_t* config);

/**
 * @brief Create STDP-immune bridge
 *
 * WHAT: Initialize bidirectional STDP-immune integration
 * WHY:  Enable realistic inflammation-plasticity coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param synapses Array of STDP synapses to manage
 * @param num_synapses Number of synapses
 * @return New bridge or NULL on failure
 */
stdp_immune_bridge_t* stdp_immune_bridge_create(
    const stdp_immune_config_t* config,
    brain_immune_system_t* immune_system,
    stdp_synapse_t* synapses,
    size_t num_synapses
);

/**
 * @brief Destroy STDP-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void stdp_immune_bridge_destroy(stdp_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → STDP API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to STDP parameters
 *
 * WHAT: Modulate STDP based on cytokine levels
 * WHY:  Pro-inflammatory cytokines impair LTP/LTD
 * HOW:  Query immune system cytokines, adjust STDP parameters
 *
 * @param bridge STDP-immune bridge
 * @return 0 on success
 */
int stdp_immune_apply_cytokine_effects(stdp_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to STDP
 *
 * WHAT: Reduce learning capacity from prolonged inflammation
 * WHY:  Chronic inflammation causes persistent LTP deficits
 * HOW:  Check inflammation duration/level, suppress STDP parameters
 *
 * @param bridge STDP-immune bridge
 * @return 0 on success
 */
int stdp_immune_apply_inflammation_effects(stdp_immune_bridge_t* bridge);

/**
 * @brief Get inflammation-modulated learning rate
 *
 * WHAT: Calculate effective learning rate with inflammation
 * WHY:  Fever/inflammation reduces synaptic learning efficiency
 * HOW:  Map inflammation level to LR reduction factor
 *
 * @param bridge STDP-immune bridge
 * @param base_lr Original learning rate
 * @return Effective learning rate [0-base_lr]
 */
float stdp_immune_get_effective_learning_rate(
    const stdp_immune_bridge_t* bridge,
    float base_lr
);

/**
 * @brief Get modulation state for synapse
 *
 * WHAT: Compute current modulation factors for STDP parameters
 * WHY:  Need to apply inflammation/cytokine effects to learning
 * HOW:  Combine cytokine and inflammation effects
 *
 * @param bridge STDP-immune bridge
 * @param modulation Output modulation state
 * @return 0 on success
 */
int stdp_immune_get_modulation_state(
    const stdp_immune_bridge_t* bridge,
    stdp_modulation_state_t* modulation
);

/**
 * @brief Apply modulation to STDP synapse
 *
 * WHAT: Update STDP synapse parameters with current modulation
 * WHY:  Realize immune effects on plasticity
 * HOW:  Scale LR, a_plus, a_minus, tau_plus, tau_minus by factors
 *
 * @param bridge STDP-immune bridge
 * @param synapse Synapse to modulate
 * @return 0 on success
 */
int stdp_immune_apply_modulation_to_synapse(
    stdp_immune_bridge_t* bridge,
    stdp_synapse_t* synapse
);

/**
 * @brief Restore STDP parameters after inflammation resolution
 *
 * WHAT: Return STDP parameters to baseline after recovery
 * WHY:  IL-10 and resolution restore plasticity capacity
 * HOW:  Interpolate back to base parameters
 *
 * @param bridge STDP-immune bridge
 * @param recovery_factor Recovery progress [0-1]
 * @return 0 on success
 */
int stdp_immune_restore_plasticity(
    stdp_immune_bridge_t* bridge,
    float recovery_factor
);

/* ============================================================================
 * STDP → Immune API
 * ============================================================================ */

/**
 * @brief Detect STDP instability (runaway LTP/LTD)
 *
 * WHAT: Check for unhealthy plasticity patterns
 * WHY:  Excessive LTP or LTD threatens synaptic homeostasis
 * HOW:  Monitor LTP/LTD accumulation and balance
 *
 * @param bridge STDP-immune bridge
 * @return 0 on success
 */
int stdp_immune_detect_instability(stdp_immune_bridge_t* bridge);

/**
 * @brief Alert immune system of plasticity instability
 *
 * WHAT: Notify immune system of synaptic homeostatic threat
 * WHY:  Runaway plasticity is threat to neural integrity
 * HOW:  Create antigen from instability signature
 *
 * @param bridge STDP-immune bridge
 * @param antigen_id Output: created antigen ID
 * @return 0 on success
 */
int stdp_immune_alert_instability(
    stdp_immune_bridge_t* bridge,
    uint32_t* antigen_id
);

/**
 * @brief Trigger anti-inflammatory signaling from balanced plasticity
 *
 * WHAT: Signal healthy synaptic function to immune system
 * WHY:  Balanced LTP/LTD indicates neural health
 * HOW:  Request IL-10 release when plasticity is balanced
 *
 * @param bridge STDP-immune bridge
 * @return 0 on success
 */
int stdp_immune_signal_balanced_plasticity(stdp_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update STDP-immune bridge (both directions)
 *
 * WHAT: Process all STDP-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine/inflammation effects, detect instabilities
 *
 * @param bridge STDP-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int stdp_immune_bridge_update(
    stdp_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine effects on STDP
 *
 * @param bridge STDP-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int stdp_immune_get_cytokine_effects(
    const stdp_immune_bridge_t* bridge,
    cytokine_stdp_effects_t* effects
);

/**
 * @brief Get current inflammation state
 *
 * @param bridge STDP-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int stdp_immune_get_inflammation_state(
    const stdp_immune_bridge_t* bridge,
    inflammation_stdp_state_t* state
);

/**
 * @brief Get current instability state
 *
 * @param bridge STDP-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int stdp_immune_get_instability_state(
    const stdp_immune_bridge_t* bridge,
    stdp_instability_state_t* state
);

/**
 * @brief Check if plasticity is impaired by inflammation
 *
 * WHAT: Determine if learning capacity is reduced
 * WHY:  Need to know if inflammation is affecting learning
 * HOW:  Check if learning rate factor < 1.0
 *
 * @param bridge STDP-immune bridge
 * @return true if impaired
 */
bool stdp_immune_is_plasticity_impaired(const stdp_immune_bridge_t* bridge);

/**
 * @brief Get LTP capacity reduction percentage
 *
 * @param bridge STDP-immune bridge
 * @return Reduction percentage [0-100]
 */
float stdp_immune_get_ltp_capacity_reduction(const stdp_immune_bridge_t* bridge);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_STDP
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int stdp_immune_connect_bio_async(stdp_immune_bridge_t* bridge);

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
int stdp_immune_disconnect_bio_async(stdp_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool stdp_immune_is_bio_async_connected(const stdp_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STDP_IMMUNE_BRIDGE_H */
