/**
 * @file nimcp_calcium_immune_bridge.h
 * @brief Calcium Dynamics-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between brain immune system and calcium dynamics
 * WHY:  Pro-inflammatory cytokines impair NMDA calcium currents, inflammation
 *       disrupts calcium homeostasis, and calcium dysregulation triggers immune response
 * HOW:  Cytokines modulate NMDA influx and calcium clearance, calcium instability
 *       alerts immune system
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → CALCIUM PATHWAYS:
 * --------------------------
 * 1. Pro-inflammatory Cytokines Reduce NMDA Calcium Currents:
 *    - IL-1β: Reduces NMDA calcium influx by 20-30%
 *    - TNF-α: Reduces NMDA currents by 30-40%
 *    - IL-6: Reduces calcium influx by 15-25%
 *    - IFN-γ: Minimal effect on calcium (~5-10% reduction)
 *    - Reference: Viviani et al. (2003) "Interleukin-1β enhances NMDA
 *      receptor-mediated intracellular calcium increase through activation
 *      of the Src family of kinases"
 *
 * 2. Inflammation Impairs Calcium Homeostasis:
 *    - NONE: Normal calcium dynamics
 *    - LOCAL: Slight reduction in calcium clearance (90% efficiency)
 *    - REGIONAL: Moderate impairment (70% efficiency)
 *    - SYSTEMIC: Severe impairment (40% efficiency)
 *    - STORM: Critical dysfunction (10% efficiency)
 *    - Reference: Sama & Norris (2013) "Calcium dysregulation and
 *      neuroinflammation"
 *
 * 3. Anti-inflammatory Cytokines Restore Calcium Dynamics:
 *    - IL-10: Restores NMDA calcium currents by 30-40%
 *    - Normalizes calcium clearance mechanisms
 *    - Protects against inflammation-induced calcium dysregulation
 *    - Reference: Rizzo et al. (2018) "IL-10 modulates neuronal calcium signaling"
 *
 * 4. Chronic Inflammation:
 *    - Sustained elevation >7 days → persistent calcium dysregulation
 *    - Impaired calcium buffering capacity
 *    - Reduced pump expression (PMCA, NCX)
 *    - Mitochondrial calcium overload
 *    - Reference: Gleichmann & Mattson (2011) "Neuronal calcium homeostasis
 *      and dysregulation"
 *
 * CALCIUM → IMMUNE PATHWAYS:
 * --------------------------
 * 1. Calcium Instability Detection:
 *    - Sustained high [Ca²⁺] (>1.5 μM for >100 ms) → excitotoxicity threat
 *    - Oscillatory calcium (frequency >10 Hz) → network instability
 *    - Prolonged low [Ca²⁺] (<0.05 μM for >1 s) → synaptic failure
 *    - Triggers immune surveillance of neural health
 *
 * 2. Calcium Homeostasis Feedback:
 *    - Healthy calcium dynamics → anti-inflammatory signaling
 *    - Dysregulated calcium → pro-inflammatory signaling
 *    - Supports neural integrity monitoring
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              CALCIUM-IMMUNE INTEGRATION BRIDGE                             ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → CALCIUM PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -25% │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -20% │         │                                       │  ║
 * ║   │   │ TNF-α → -35% │         ├──→ NMDA Ca²⁺ Influx Reduction         │  ║
 * ║   │   │ IL-10 → +35% │         │                                       │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     CALCIUM PARAMETERS          │                             │  ║
 * ║   │   │  - NMDA influx modulation       │                             │  ║
 * ║   │   │  - Pump rate impairment         │                             │  ║
 * ║   │   │  - Buffer capacity reduction    │                             │  ║
 * ║   │   │  - Threshold shifts             │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   INFLAMMATION LEVEL → CALCIUM CLEARANCE                           │  ║
 * ║   │   ─────────────────────────────────────────                        │  ║
 * ║   │   NONE       → 100% clearance efficiency                           │  ║
 * ║   │   LOCAL      → 90% clearance                                       │  ║
 * ║   │   REGIONAL   → 70% clearance                                       │  ║
 * ║   │   SYSTEMIC   → 40% clearance                                       │  ║
 * ║   │   STORM      → 10% clearance                                       │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  CALCIUM → IMMUNE PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ HIGH [Ca²⁺]  │ ──→ Excitotoxicity Alert (>1.5 μM)             │  ║
 * ║   │   │ OSCILLATORY  │ ──→ Network Instability Alert (>10 Hz)         │  ║
 * ║   │   │ LOW [Ca²⁺]   │ ──→ Synaptic Failure Alert (<0.05 μM)          │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  HEALTHY Ca  │ ──→ Anti-inflammatory Signaling                │  ║
 * ║   │   │  DYNAMICS    │ ──→ Calcium Homeostasis                        │  ║
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

#ifndef NIMCP_CALCIUM_IMMUNE_BRIDGE_H
#define NIMCP_CALCIUM_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/calcium/nimcp_calcium_dynamics.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine NMDA calcium influx impairment factors */
#define CYTOKINE_IL1_CALCIUM_IMPAIRMENT      0.75f   /**< IL-1β reduces influx to 75% */
#define CYTOKINE_IL6_CALCIUM_IMPAIRMENT      0.80f   /**< IL-6 reduces influx to 80% */
#define CYTOKINE_TNF_CALCIUM_IMPAIRMENT      0.65f   /**< TNF-α reduces influx to 65% */
#define CYTOKINE_IFN_GAMMA_CALCIUM_IMPAIRMENT 0.90f  /**< IFN-γ reduces influx to 90% */
#define CYTOKINE_IL10_CALCIUM_RESTORATION    1.35f   /**< IL-10 restores influx by 35% */

/* Inflammation-based calcium clearance impairment */
#define INFLAMMATION_CLEARANCE_NONE          1.00f   /**< 100% clearance efficiency */
#define INFLAMMATION_CLEARANCE_LOCAL         0.90f   /**< 90% efficiency */
#define INFLAMMATION_CLEARANCE_REGIONAL      0.70f   /**< 70% efficiency */
#define INFLAMMATION_CLEARANCE_SYSTEMIC      0.40f   /**< 40% efficiency */
#define INFLAMMATION_CLEARANCE_STORM         0.10f   /**< 10% efficiency */

/* Calcium instability detection thresholds */
#define CALCIUM_EXCITOTOXICITY_THRESHOLD     1.5f    /**< μM, sustained high [Ca²⁺] */
#define CALCIUM_EXCITOTOXICITY_DURATION_MS   100.0f  /**< ms, duration threshold */
#define CALCIUM_SYNAPTIC_FAILURE_THRESHOLD   0.05f   /**< μM, prolonged low [Ca²⁺] */
#define CALCIUM_SYNAPTIC_FAILURE_DURATION_MS 1000.0f /**< ms, duration threshold */
#define CALCIUM_OSCILLATION_FREQ_THRESHOLD   10.0f   /**< Hz, high frequency oscillations */

/* Chronic inflammation duration (seconds) */
#define CHRONIC_INFLAMMATION_THRESHOLD_SEC   (86400.0f * 7)  /**< 7 days = chronic */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on calcium dynamics
 *
 * How cytokine levels modulate calcium signaling
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_influx_impairment;         /**< IL-1β influx reduction factor */
    float il6_influx_impairment;         /**< IL-6 influx reduction factor */
    float tnf_influx_impairment;         /**< TNF-α influx reduction factor */
    float ifn_gamma_influx_impairment;   /**< IFN-γ influx reduction factor */

    /* Anti-inflammatory effects */
    float il10_influx_restoration;       /**< IL-10 influx restoration factor */

    /* Aggregate effects */
    float total_influx_modulation;       /**< Combined influx scaling [0-2] */
    float total_clearance_modulation;    /**< Combined clearance scaling [0-1] */
    float total_buffer_modulation;       /**< Buffer capacity scaling [0-1] */
} cytokine_calcium_effects_t;

/**
 * @brief Inflammation effects on calcium homeostasis
 *
 * How chronic inflammation affects calcium dynamics
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;     /**< How long inflamed */
    bool is_chronic;                     /**< >= 7 days */

    /* Calcium impacts */
    float clearance_impairment;          /**< Pump/extrusion reduction [0-1] */
    float buffer_capacity_loss;          /**< Buffering capacity loss [0-1] */
    float nmda_sensitivity_reduction;    /**< NMDA receptor sensitivity [0-1] */

    /* Chronic effects */
    float pump_expression_loss;          /**< PMCA/NCX expression loss [0-1] */
    float mitochondrial_dysfunction;     /**< Mitochondrial Ca²⁺ handling [0-1] */
} inflammation_calcium_state_t;

/**
 * @brief Calcium instability detection
 *
 * Monitoring calcium dynamics for immune alerting
 */
typedef struct {
    /* Calcium state */
    float max_ca_recent;                 /**< Maximum [Ca²⁺] in recent window */
    float min_ca_recent;                 /**< Minimum [Ca²⁺] in recent window */
    float time_above_excitotoxic_ms;     /**< Time spent >1.5 μM */
    float time_below_failure_ms;         /**< Time spent <0.05 μM */
    float oscillation_frequency_hz;      /**< Detected oscillation frequency */

    /* Instability flags */
    bool excitotoxicity_detected;        /**< Sustained high [Ca²⁺] */
    bool synaptic_failure_detected;      /**< Prolonged low [Ca²⁺] */
    bool oscillatory_instability;        /**< High frequency oscillations */
    bool healthy_dynamics;               /**< Normal calcium homeostasis */

    /* Severity */
    float instability_severity;          /**< Threat level [0-1] */
} calcium_instability_state_t;

/**
 * @brief Calcium modulation snapshot
 *
 * Current modulation state for calcium system
 */
typedef struct {
    /* Current modulation factors */
    float influx_modulation;             /**< NMDA influx multiplier [0-2] */
    float pump_modulation;               /**< Pump rate multiplier [0-2] */
    float buffer_modulation;             /**< Buffer capacity multiplier [0-1] */
    float decay_modulation;              /**< Decay time constant multiplier [0.5-2] */

    /* Effective parameters (original * modulation) */
    float effective_influx_alpha;
    float effective_pump_rate;
    float effective_buffer_capacity;
    float effective_decay_tau_ms;
} calcium_modulation_state_t;

/**
 * @brief Complete calcium-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    calcium_dynamics_t calcium;

    /* Current state */
    cytokine_calcium_effects_t cytokine_effects;
    inflammation_calcium_state_t inflammation_state;
    calcium_instability_state_t instability_state;

    /* Base parameters (for restoration) */
    float base_influx_alpha;
    float base_pump_rate;
    float base_buffer_capacity;
    float base_decay_tau_ms;

    /* Integration flags */
    bool enable_cytokine_calcium_modulation;
    bool enable_inflammation_impairment;
    bool enable_instability_detection;
    bool enable_homeostatic_feedback;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t instability_alerts;
    uint32_t calcium_restorations;

    } calcium_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_calcium_modulation;
    bool enable_inflammation_impairment;
    bool enable_instability_detection;
    bool enable_homeostatic_feedback;

    /* Sensitivity tuning */
    float cytokine_sensitivity;          /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;      /**< Inflammation effect multiplier [0.5-2.0] */
    float instability_sensitivity;       /**< Instability detection multiplier [0.5-2.0] */

    /* Base calcium parameters */
    float base_influx_alpha;
    float base_pump_rate;
    float base_buffer_capacity;
    float base_decay_tau_ms;

    /* Thresholds */
    float excitotoxicity_threshold_um;   /**< [Ca²⁺] threshold for excitotoxicity */
    float synaptic_failure_threshold_um; /**< [Ca²⁺] threshold for failure */
    float oscillation_freq_threshold_hz; /**< Oscillation frequency threshold */
} calcium_immune_config_t;

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
int calcium_immune_default_config(calcium_immune_config_t* config);

/**
 * @brief Create calcium-immune bridge
 *
 * WHAT: Initialize bidirectional calcium-immune integration
 * WHY:  Enable realistic inflammation-calcium coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param calcium Calcium dynamics system
 * @return New bridge or NULL on failure
 */
calcium_immune_bridge_t* calcium_immune_bridge_create(
    const calcium_immune_config_t* config,
    brain_immune_system_t* immune_system,
    calcium_dynamics_t calcium
);

/**
 * @brief Destroy calcium-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void calcium_immune_bridge_destroy(calcium_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Calcium API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to calcium dynamics
 *
 * WHAT: Modulate calcium based on cytokine levels
 * WHY:  Pro-inflammatory cytokines impair NMDA calcium currents
 * HOW:  Query immune system cytokines, adjust calcium parameters
 *
 * @param bridge Calcium-immune bridge
 * @return 0 on success
 */
int calcium_immune_apply_cytokine_effects(calcium_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to calcium homeostasis
 *
 * WHAT: Reduce calcium clearance from prolonged inflammation
 * WHY:  Chronic inflammation causes persistent calcium dysregulation
 * HOW:  Check inflammation duration/level, impair pumps and buffers
 *
 * @param bridge Calcium-immune bridge
 * @return 0 on success
 */
int calcium_immune_apply_inflammation_effects(calcium_immune_bridge_t* bridge);

/**
 * @brief Get inflammation-modulated calcium influx factor
 *
 * WHAT: Calculate effective NMDA influx with inflammation
 * WHY:  Cytokines reduce calcium entry efficiency
 * HOW:  Map cytokine levels and inflammation to influx reduction
 *
 * @param bridge Calcium-immune bridge
 * @return Influx factor [0-1]
 */
float calcium_immune_get_effective_influx(const calcium_immune_bridge_t* bridge);

/**
 * @brief Get modulation state
 *
 * WHAT: Compute current modulation factors for calcium parameters
 * WHY:  Need to apply inflammation/cytokine effects to dynamics
 * HOW:  Combine cytokine and inflammation effects
 *
 * @param bridge Calcium-immune bridge
 * @param modulation Output modulation state
 * @return 0 on success
 */
int calcium_immune_get_modulation_state(
    const calcium_immune_bridge_t* bridge,
    calcium_modulation_state_t* modulation
);

/**
 * @brief Restore calcium dynamics after inflammation resolution
 *
 * WHAT: Return calcium parameters to baseline after recovery
 * WHY:  IL-10 and resolution restore calcium homeostasis
 * HOW:  Interpolate back to base parameters
 *
 * @param bridge Calcium-immune bridge
 * @param recovery_factor Recovery progress [0-1]
 * @return 0 on success
 */
int calcium_immune_restore_dynamics(
    calcium_immune_bridge_t* bridge,
    float recovery_factor
);

/* ============================================================================
 * Calcium → Immune API
 * ============================================================================ */

/**
 * @brief Detect calcium instability (excitotoxicity, failure, oscillations)
 *
 * WHAT: Check for unhealthy calcium dynamics
 * WHY:  Calcium dysregulation threatens neural integrity
 * HOW:  Monitor [Ca²⁺] extremes and oscillations
 *
 * @param bridge Calcium-immune bridge
 * @return 0 on success
 */
int calcium_immune_detect_instability(calcium_immune_bridge_t* bridge);

/**
 * @brief Alert immune system of calcium instability
 *
 * WHAT: Notify immune system of calcium homeostatic threat
 * WHY:  Dysregulated calcium is threat to neural health
 * HOW:  Create antigen from instability signature
 *
 * @param bridge Calcium-immune bridge
 * @param antigen_id Output: created antigen ID
 * @return 0 on success
 */
int calcium_immune_alert_instability(
    calcium_immune_bridge_t* bridge,
    uint32_t* antigen_id
);

/**
 * @brief Trigger anti-inflammatory signaling from healthy calcium
 *
 * WHAT: Signal healthy calcium homeostasis to immune system
 * WHY:  Normal calcium dynamics indicate neural health
 * HOW:  Request IL-10 release when calcium is stable
 *
 * @param bridge Calcium-immune bridge
 * @return 0 on success
 */
int calcium_immune_signal_healthy_dynamics(calcium_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update calcium-immune bridge (both directions)
 *
 * WHAT: Process all calcium-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine/inflammation effects, detect instabilities
 *
 * @param bridge Calcium-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int calcium_immune_bridge_update(
    calcium_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine effects on calcium
 */
int calcium_immune_get_cytokine_effects(
    const calcium_immune_bridge_t* bridge,
    cytokine_calcium_effects_t* effects
);

/**
 * @brief Get current inflammation state
 */
int calcium_immune_get_inflammation_state(
    const calcium_immune_bridge_t* bridge,
    inflammation_calcium_state_t* state
);

/**
 * @brief Get current instability state
 */
int calcium_immune_get_instability_state(
    const calcium_immune_bridge_t* bridge,
    calcium_instability_state_t* state
);

/**
 * @brief Check if calcium dynamics is impaired by inflammation
 */
bool calcium_immune_is_dynamics_impaired(const calcium_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 */
int calcium_immune_connect_bio_async(calcium_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int calcium_immune_disconnect_bio_async(calcium_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 */
bool calcium_immune_is_bio_async_connected(const calcium_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CALCIUM_IMMUNE_BRIDGE_H */
