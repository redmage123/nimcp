/**
 * @file nimcp_metaplasticity_immune_bridge.h
 * @brief Metaplasticity-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between brain immune system and extended metaplasticity
 * WHY:  Inflammation affects plasticity thresholds, making LTP harder to induce
 * HOW:  Cytokines shift modification thresholds upward, inflammation impairs threshold adaptation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → METAPLASTICITY PATHWAYS:
 * ----------------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Shift modification threshold (θ_m) UPWARD
 *    - Makes LTP harder to induce (activity must be higher to cross threshold)
 *    - Reduces threshold adaptation rate (slower homeostatic adjustment)
 *    - Result: Impaired ability to form new plasticity
 *    - Reference: Pickering & O'Connor (2007) "Inflammation and LTP impairment"
 *
 * 2. Inflammation Effects on Threshold:
 *    - NONE: θ_m = baseline (normal metaplasticity)
 *    - LOCAL: θ_m × 1.1 (10% higher threshold)
 *    - REGIONAL: θ_m × 1.3 (30% higher threshold)
 *    - SYSTEMIC: θ_m × 1.6 (60% higher threshold)
 *    - STORM: θ_m × 2.0 (100% higher threshold)
 *    - Reference: Yirmiya & Goshen (2011) "Immune modulation of learning"
 *
 * 3. Chronic Inflammation:
 *    - Sustained elevation > 7 days → persistent threshold elevation
 *    - Impaired threshold homeostasis (can't return to baseline)
 *    - Reduced metaplastic range (less adaptive capacity)
 *    - Long-term learning deficits
 *
 * 4. Anti-inflammatory Cytokines (IL-10):
 *    - Restore θ_m to baseline levels
 *    - Re-enable threshold adaptation
 *    - Recover metaplastic range
 *    - Reference: Rizzo et al. (2018) "IL-10 restores synaptic plasticity"
 *
 * 5. Neuromodulator Interaction:
 *    - Inflammation blocks dopamine's threshold-lowering effect
 *    - Cytokines override DA-mediated LTP facilitation
 *    - Fever + inflammation: additive threshold elevation
 *
 * METAPLASTICITY → IMMUNE PATHWAYS:
 * ----------------------------------
 * 1. Threshold Instability Detection:
 *    - Runaway threshold elevation → immune threat signal
 *    - Inability to reset thresholds → homeostatic failure
 *    - Triggers immune surveillance
 *
 * 2. Adaptive Failure Signaling:
 *    - Threshold stuck at extremes → pathological state
 *    - Loss of metaplastic range → system degradation
 *    - Supports neural integrity monitoring
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              METAPLASTICITY-IMMUNE BRIDGE                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE → METAPLASTICITY PATHWAYS                       │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → +30% │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → +25% │         │                                       │  ║
 * ║   │   │ TNF-α → +40% │         ├──→ Threshold Elevation                │  ║
 * ║   │   │              │         │                                       │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     θ_m MODIFICATION            │                             │  ║
 * ║   │   │  - Upward threshold shift       │                             │  ║
 * ║   │   │  - Reduced adaptation rate      │                             │  ║
 * ║   │   │  - Impaired baseline reset      │                             │  ║
 * ║   │   │  - Narrowed metaplastic range   │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │  Restoration │    Threshold Recovery                           │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │           METAPLASTICITY → IMMUNE PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  RUNAWAY θ_m │ ──→ Threshold Instability Alert                │  ║
 * ║   │   │  STUCK θ_m   │ ──→ Adaptive Failure Alert                     │  ║
 * ║   │   │  NO RESET    │ ──→ Homeostatic Threat                         │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  HEALTHY θ_m │ ──→ Anti-inflammatory Signaling                │  ║
 * ║   │   │  HOMEOSTASIS │ ──→ Neural Health                              │  ║
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
 * @reference Abraham & Bear (1996) "Metaplasticity: the plasticity of synaptic plasticity"
 */

#ifndef NIMCP_METAPLASTICITY_IMMUNE_BRIDGE_H
#define NIMCP_METAPLASTICITY_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/metaplasticity/nimcp_extended_metaplasticity.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine threshold elevation factors */
#define CYTOKINE_IL1_THRESHOLD_ELEVATION      1.30f   /**< IL-1β raises θ_m by 30% */
#define CYTOKINE_IL6_THRESHOLD_ELEVATION      1.25f   /**< IL-6 raises θ_m by 25% */
#define CYTOKINE_TNF_THRESHOLD_ELEVATION      1.40f   /**< TNF-α raises θ_m by 40% */
#define CYTOKINE_IFN_GAMMA_THRESHOLD_ELEVATION 1.20f  /**< IFN-γ raises θ_m by 20% */
#define CYTOKINE_IL10_THRESHOLD_RESTORATION   0.60f   /**< IL-10 lowers θ_m by 40% */

/* Inflammation-based threshold modulation */
#define INFLAMMATION_THETA_NONE               1.00f   /**< No change */
#define INFLAMMATION_THETA_LOCAL              1.10f   /**< 10% elevation */
#define INFLAMMATION_THETA_REGIONAL           1.30f   /**< 30% elevation */
#define INFLAMMATION_THETA_SYSTEMIC           1.60f   /**< 60% elevation */
#define INFLAMMATION_THETA_STORM              2.00f   /**< 100% elevation */

/* Adaptation rate impairment by inflammation */
#define INFLAMMATION_ADAPT_RATE_NONE          1.00f   /**< 100% adaptation */
#define INFLAMMATION_ADAPT_RATE_LOCAL         0.90f   /**< 90% adaptation */
#define INFLAMMATION_ADAPT_RATE_REGIONAL      0.70f   /**< 70% adaptation */
#define INFLAMMATION_ADAPT_RATE_SYSTEMIC      0.40f   /**< 40% adaptation */
#define INFLAMMATION_ADAPT_RATE_STORM         0.10f   /**< 10% adaptation */

/* Threshold instability detection */
#define METAPLASTICITY_THETA_RUNAWAY_THRESHOLD   3.0f  /**< Threshold elevation ratio */
#define METAPLASTICITY_THETA_STUCK_DURATION      300000 /**< 5 minutes stuck */
#define METAPLASTICITY_RESET_FAILURE_THRESHOLD   0.1f  /**< <10% reset is failure */

/* Chronic inflammation duration (seconds) */
#define CHRONIC_INFLAMMATION_THRESHOLD_SEC    (86400.0f * 7)  /**< 7 days = chronic */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on metaplasticity parameters
 *
 * How cytokine levels modulate threshold and adaptation
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_threshold_elevation;      /**< IL-1β θ_m increase factor */
    float il6_threshold_elevation;      /**< IL-6 θ_m increase factor */
    float tnf_threshold_elevation;      /**< TNF-α θ_m increase factor */
    float ifn_gamma_threshold_elevation; /**< IFN-γ θ_m increase factor */

    /* Anti-inflammatory effects */
    float il10_threshold_restoration;   /**< IL-10 θ_m restoration factor */

    /* Aggregate effects */
    float total_threshold_modulation;   /**< Combined θ_m scaling [1-3] */
    float total_adaptation_suppression; /**< Adaptation rate reduction [0-1] */
    float baseline_reset_impairment;    /**< Impaired sleep reset [0-1] */
} cytokine_metaplasticity_effects_t;

/**
 * @brief Inflammation effects on metaplasticity
 *
 * How chronic inflammation affects threshold dynamics
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;    /**< How long inflamed */
    bool is_chronic;                    /**< >= 7 days */

    /* Threshold impacts */
    float threshold_elevation;          /**< θ_m increase factor [1-3] */
    float adaptation_rate_suppression;  /**< Adaptation rate reduction [0-1] */
    float baseline_reset_impairment;    /**< Sleep reset impairment [0-1] */
    float metaplastic_range_reduction;  /**< Range compression [0-1] */

    /* Chronic effects */
    float threshold_homeostasis_loss;   /**< Inability to return to baseline */
    float adaptive_capacity_loss;       /**< Loss of metaplastic flexibility */
} inflammation_metaplasticity_state_t;

/**
 * @brief Metaplasticity instability detection
 *
 * Monitoring threshold health for immune alerting
 */
typedef struct {
    /* Threshold state */
    float current_theta_ratio;          /**< θ_effective / θ_baseline */
    float theta_stuck_duration_ms;      /**< Time at extreme value */
    float last_reset_magnitude;         /**< Last sleep reset strength */

    /* Instability flags */
    bool theta_runaway_detected;        /**< Excessive threshold elevation */
    bool theta_stuck_detected;          /**< Threshold frozen at extreme */
    bool reset_failure_detected;        /**< Sleep reset failed */
    bool homeostatic_threat;            /**< Overall adaptive failure */

    /* Severity */
    float instability_severity;         /**< Threat level [0-1] */
} metaplasticity_instability_state_t;

/**
 * @brief Metaplasticity modulation snapshot
 *
 * Current modulation state for threshold
 */
typedef struct {
    /* Current modulation factors */
    float threshold_modulation;         /**< θ_m multiplier [1-3] */
    float adaptation_rate_modulation;   /**< Adaptation rate [0-1] */
    float baseline_reset_modulation;    /**< Reset strength [0-1] */
    float neuromodulator_block;         /**< Block DA/NE effects [0-1] */

    /* Effective parameters */
    float effective_theta_baseline;
    float effective_adaptation_rate;
    float effective_reset_factor;
} metaplasticity_modulation_state_t;

/**
 * @brief Complete metaplasticity-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    metaplasticity_controller_t metaplasticity_controller;

    /* Current state */
    cytokine_metaplasticity_effects_t cytokine_effects;
    inflammation_metaplasticity_state_t inflammation_state;
    metaplasticity_instability_state_t instability_state;

    /* Base parameters (for restoration) */
    float base_theta_baseline;
    float base_adaptation_rate;
    float base_reset_factor;

    /* Integration flags */
    bool enable_cytokine_metaplasticity_modulation;
    bool enable_inflammation_impairment;
    bool enable_instability_detection;
    bool enable_homeostatic_feedback;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t instability_alerts;
    uint32_t threshold_restorations;

    } metaplasticity_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_metaplasticity_modulation;
    bool enable_inflammation_impairment;
    bool enable_instability_detection;
    bool enable_homeostatic_feedback;

    /* Sensitivity tuning */
    float cytokine_sensitivity;         /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;     /**< Inflammation effect multiplier [0.5-2.0] */
    float instability_sensitivity;      /**< Instability detection multiplier [0.5-2.0] */

    /* Base metaplasticity parameters */
    float base_theta_baseline;
    float base_adaptation_rate;
    float base_reset_factor;

    /* Thresholds */
    float theta_runaway_threshold;      /**< Threshold elevation ratio */
    float theta_stuck_duration_ms;      /**< Time stuck threshold */
    float reset_failure_threshold;      /**< Minimum successful reset */
} metaplasticity_immune_config_t;

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
int metaplasticity_immune_default_config(metaplasticity_immune_config_t* config);

/**
 * @brief Create metaplasticity-immune bridge
 *
 * WHAT: Initialize bidirectional metaplasticity-immune integration
 * WHY:  Enable realistic inflammation-threshold coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param metaplasticity_controller Metaplasticity controller
 * @return New bridge or NULL on failure
 */
metaplasticity_immune_bridge_t* metaplasticity_immune_bridge_create(
    const metaplasticity_immune_config_t* config,
    brain_immune_system_t* immune_system,
    metaplasticity_controller_t metaplasticity_controller
);

/**
 * @brief Destroy metaplasticity-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void metaplasticity_immune_bridge_destroy(metaplasticity_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Metaplasticity API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to threshold parameters
 *
 * WHAT: Modulate θ_m based on cytokine levels
 * WHY:  Pro-inflammatory cytokines elevate modification threshold
 * HOW:  Query immune system cytokines, adjust threshold parameters
 *
 * @param bridge Metaplasticity-immune bridge
 * @return 0 on success
 */
int metaplasticity_immune_apply_cytokine_effects(metaplasticity_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to metaplasticity
 *
 * WHAT: Elevate threshold and impair adaptation from inflammation
 * WHY:  Chronic inflammation causes persistent threshold dysfunction
 * HOW:  Check inflammation duration/level, suppress metaplastic parameters
 *
 * @param bridge Metaplasticity-immune bridge
 * @return 0 on success
 */
int metaplasticity_immune_apply_inflammation_effects(metaplasticity_immune_bridge_t* bridge);

/**
 * @brief Get inflammation-modulated threshold
 *
 * WHAT: Calculate effective θ_m with inflammation
 * WHY:  Inflammation raises threshold, impairing LTP induction
 * HOW:  Map inflammation level to threshold elevation factor
 *
 * @param bridge Metaplasticity-immune bridge
 * @param base_theta Original threshold
 * @return Effective threshold [base_theta - 3×base_theta]
 */
float metaplasticity_immune_get_effective_threshold(
    const metaplasticity_immune_bridge_t* bridge,
    float base_theta
);

/**
 * @brief Get modulation state
 *
 * WHAT: Compute current modulation factors for metaplasticity
 * WHY:  Need to apply inflammation/cytokine effects to thresholds
 * HOW:  Combine cytokine and inflammation effects
 *
 * @param bridge Metaplasticity-immune bridge
 * @param modulation Output modulation state
 * @return 0 on success
 */
int metaplasticity_immune_get_modulation_state(
    const metaplasticity_immune_bridge_t* bridge,
    metaplasticity_modulation_state_t* modulation
);

/**
 * @brief Restore metaplasticity after inflammation resolution
 *
 * WHAT: Return threshold parameters to baseline after recovery
 * WHY:  IL-10 and resolution restore metaplastic capacity
 * HOW:  Interpolate back to base parameters
 *
 * @param bridge Metaplasticity-immune bridge
 * @param recovery_factor Recovery progress [0-1]
 * @return 0 on success
 */
int metaplasticity_immune_restore_metaplasticity(
    metaplasticity_immune_bridge_t* bridge,
    float recovery_factor
);

/* ============================================================================
 * Metaplasticity → Immune API
 * ============================================================================ */

/**
 * @brief Detect metaplasticity instability
 *
 * WHAT: Check for unhealthy threshold patterns
 * WHY:  Runaway or stuck thresholds threaten homeostasis
 * HOW:  Monitor threshold ratio, stuck duration, reset failures
 *
 * @param bridge Metaplasticity-immune bridge
 * @return 0 on success
 */
int metaplasticity_immune_detect_instability(metaplasticity_immune_bridge_t* bridge);

/**
 * @brief Alert immune system of threshold instability
 *
 * WHAT: Notify immune system of metaplastic homeostatic threat
 * WHY:  Threshold dysfunction is threat to neural integrity
 * HOW:  Create antigen from instability signature
 *
 * @param bridge Metaplasticity-immune bridge
 * @param antigen_id Output: created antigen ID
 * @return 0 on success
 */
int metaplasticity_immune_alert_instability(
    metaplasticity_immune_bridge_t* bridge,
    uint32_t* antigen_id
);

/**
 * @brief Trigger anti-inflammatory signaling from healthy threshold
 *
 * WHAT: Signal healthy metaplastic function to immune system
 * WHY:  Homeostatic threshold indicates neural health
 * HOW:  Request IL-10 release when threshold is stable
 *
 * @param bridge Metaplasticity-immune bridge
 * @return 0 on success
 */
int metaplasticity_immune_signal_healthy_homeostasis(
    metaplasticity_immune_bridge_t* bridge
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update metaplasticity-immune bridge (both directions)
 *
 * WHAT: Process all metaplasticity-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine/inflammation effects, detect instabilities
 *
 * @param bridge Metaplasticity-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int metaplasticity_immune_bridge_update(
    metaplasticity_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine effects
 *
 * @param bridge Metaplasticity-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int metaplasticity_immune_get_cytokine_effects(
    const metaplasticity_immune_bridge_t* bridge,
    cytokine_metaplasticity_effects_t* effects
);

/**
 * @brief Get current inflammation state
 *
 * @param bridge Metaplasticity-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int metaplasticity_immune_get_inflammation_state(
    const metaplasticity_immune_bridge_t* bridge,
    inflammation_metaplasticity_state_t* state
);

/**
 * @brief Get current instability state
 *
 * @param bridge Metaplasticity-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int metaplasticity_immune_get_instability_state(
    const metaplasticity_immune_bridge_t* bridge,
    metaplasticity_instability_state_t* state
);

/**
 * @brief Check if metaplasticity is impaired
 *
 * WHAT: Determine if threshold adaptation is compromised
 * WHY:  Need to know if inflammation is affecting metaplasticity
 * HOW:  Check if adaptation rate < 1.0
 *
 * @param bridge Metaplasticity-immune bridge
 * @return true if impaired
 */
bool metaplasticity_immune_is_impaired(const metaplasticity_immune_bridge_t* bridge);

/**
 * @brief Get threshold elevation percentage
 *
 * @param bridge Metaplasticity-immune bridge
 * @return Elevation percentage [0-200]
 */
float metaplasticity_immune_get_threshold_elevation(
    const metaplasticity_immune_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_METAPLASTICITY
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int metaplasticity_immune_connect_bio_async(metaplasticity_immune_bridge_t* bridge);

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
int metaplasticity_immune_disconnect_bio_async(metaplasticity_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool metaplasticity_immune_is_bio_async_connected(
    const metaplasticity_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_METAPLASTICITY_IMMUNE_BRIDGE_H */
