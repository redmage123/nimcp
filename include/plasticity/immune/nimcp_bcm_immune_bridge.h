/**
 * @file nimcp_bcm_immune_bridge.h
 * @brief BCM Learning-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and BCM learning
 * WHY:  Biological evidence shows cytokines modulate synaptic plasticity thresholds
 *       and aberrant plasticity dynamics trigger immune responses
 * HOW:  Cytokines shift BCM theta_m threshold, inflammation affects sliding rate,
 *       abnormal threshold dynamics trigger immune responses, recovery restores normal BCM
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → BCM PATHWAYS:
 * -------------------------
 * 1. IL-1β Effects on BCM Threshold:
 *    - Shifts theta_m (modification threshold) upward
 *    - Reduces LTP induction, favors LTD
 *    - Models fever-induced learning impairment
 *    - Reference: Schneider et al. (1998) "IL-1β inhibits LTP in hippocampus"
 *
 * 2. TNF-α Modulation of Sliding Rate:
 *    - Accelerates threshold adaptation (tau reduced)
 *    - Increases metaplasticity dynamics
 *    - Affects homeostatic set-point stability
 *    - Reference: Stellwagen & Malenka (2006) "TNF-α and synaptic plasticity"
 *
 * 3. IL-6 Impact on Learning Rate:
 *    - Reduces BCM learning rate (η decreased)
 *    - Slows weight change dynamics
 *    - Models sickness-induced cognitive slowing
 *    - Reference: Balschun et al. (2004) "IL-6 impairs LTP and memory"
 *
 * 4. Chronic Inflammation → Aberrant BCM:
 *    - Sustained elevation disrupts theta_m homeostasis
 *    - Threshold becomes unstable (oscillations)
 *    - Impaired metaplasticity (set-point dysregulation)
 *    - Reference: Pickering & O'Connor (2007) "Pro-inflammatory cytokines and synaptic function"
 *
 * 5. IL-10 Recovery Effects:
 *    - Restores normal theta_m dynamics
 *    - Re-establishes homeostatic set-points
 *    - Normalizes sliding rate
 *    - Reference: Maes et al. (1999) "Anti-inflammatory cytokines restore plasticity"
 *
 * BCM → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Threshold Instability → Immune Alert:
 *    - Excessive theta_m oscillations detected as pathological
 *    - Runaway thresholds (too high/low) trigger inflammation
 *    - Models synaptic dysfunction as "neural damage"
 *    - Severity: Threshold variance > 3x baseline
 *
 * 2. Learning Rate Collapse → Immune Response:
 *    - Near-zero weight changes indicate plasticity failure
 *    - Triggers moderate immune response
 *    - Models loss of cortical adaptability
 *    - Severity: LTP+LTD events < 10% normal
 *
 * 3. Metaplasticity Failure → Inflammation:
 *    - Theta_m stops sliding appropriately
 *    - Homeostatic mechanisms broken
 *    - Triggers regional inflammation
 *    - Severity: Threshold stuck (< 1% change over time)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    BCM-IMMUNE BRIDGE                                       ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → BCM PATHWAYS                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β →+θ_m  │  ───────┐                                       │  ║
 * ║   │   │ TNF-α →-τ    │         │                                       │  ║
 * ║   │   │ IL-6  →-η    │         ├──→ BCM Parameter Modulation           │  ║
 * ║   │   │              │         │    (Threshold, Rate, Learning)        │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     BCM LEARNING SYSTEM         │                             │  ║
 * ║   │   │  - Modified theta_m             │                             │  ║
 * ║   │   │  - Altered sliding rate         │                             │  ║
 * ║   │   │  - Reduced learning rate        │                             │  ║
 * ║   │   │  - Threshold instability        │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │ Recovery     │     Restore Normal BCM                          │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  BCM → IMMUNE PATHWAYS                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ θ_m UNSTABLE │ ──→ Immune Alert (Threshold Oscillations)      │  ║
 * ║   │   │ LTP/LTD LOW  │ ──→ Immune Response (Plasticity Collapse)      │  ║
 * ║   │   │ STUCK θ_m    │ ──→ Inflammation (Metaplasticity Failure)      │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
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

#ifndef NIMCP_BCM_IMMUNE_BRIDGE_H
#define NIMCP_BCM_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/bcm/nimcp_bcm.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine BCM parameter modulation factors */
#define CYTOKINE_IL1_THRESHOLD_ELEVATION  1.5f   /**< IL-1β increases theta_m by 50% */
#define CYTOKINE_IL6_LEARNING_REDUCTION   0.7f   /**< IL-6 reduces learning rate to 70% */
#define CYTOKINE_TNF_SLIDING_ACCELERATION 0.5f   /**< TNF-α halves tau (faster sliding) */
#define CYTOKINE_IL10_RECOVERY_BOOST      1.2f   /**< IL-10 boosts recovery toward baseline */

/* BCM abnormality detection thresholds */
#define BCM_THRESHOLD_INSTABILITY_FACTOR  3.0f   /**< Threshold variance > 3x baseline = unstable */
#define BCM_LEARNING_COLLAPSE_FACTOR      0.1f   /**< LTP+LTD < 10% normal = collapsed */
#define BCM_METAPLASTICITY_STUCK_FACTOR   0.01f  /**< Threshold change < 1% = stuck */

/* Inflammation modulation ranges */
#define INFLAMMATION_THETA_MIN_FACTOR     0.5f   /**< Min theta scaling (storm) */
#define INFLAMMATION_THETA_MAX_FACTOR     2.5f   /**< Max theta scaling (storm) */
#define INFLAMMATION_LR_MIN_FACTOR        0.2f   /**< Min learning rate (storm) */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on BCM parameters
 *
 * Represents how cytokine levels modulate BCM learning dynamics
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_threshold_elevation;    /**< IL-1β theta_m increase */
    float il6_learning_reduction;     /**< IL-6 learning rate reduction */
    float tnf_sliding_acceleration;   /**< TNF-α tau reduction (faster sliding) */

    /* Anti-inflammatory effects */
    float il10_recovery_factor;       /**< IL-10 recovery toward baseline */

    /* Aggregate effects */
    float theta_m_multiplier;         /**< Combined threshold modulation [0.5-2.5] */
    float learning_rate_multiplier;   /**< Combined learning rate modulation [0.2-1.0] */
    float tau_multiplier;             /**< Combined sliding rate modulation [0.5-2.0] */
} cytokine_bcm_effects_t;

/**
 * @brief Inflammation impact on BCM dynamics
 *
 * How chronic inflammation disrupts BCM homeostasis
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* BCM disruptions */
    float threshold_instability;       /**< Theta_m oscillation severity [0-1] */
    float metaplasticity_impairment;   /**< Sliding dysfunction [0-1] */
    float learning_suppression;        /**< Overall plasticity reduction [0-1] */

    /* Set-point dysregulation */
    float homeostatic_error;           /**< Distance from healthy set-point [0-1] */
    float recovery_time_estimate_ms;   /**< Estimated recovery duration */
} inflammation_bcm_state_t;

/**
 * @brief BCM abnormality detection for immune triggering
 *
 * Monitors BCM dynamics for pathological patterns
 */
typedef struct {
    /* Threshold dynamics */
    float threshold_mean;              /**< Average theta_m */
    float threshold_variance;          /**< Theta_m variance */
    float threshold_instability_score; /**< How unstable (0-1) */

    /* Learning activity */
    uint64_t ltp_events;               /**< Potentiation count */
    uint64_t ltd_events;               /**< Depression count */
    float learning_activity_score;     /**< Overall activity (0-1) */

    /* Metaplasticity health */
    float sliding_rate;                /**< Actual theta_m change rate */
    float metaplasticity_health;       /**< Sliding function health (0-1) */

    /* Immune triggers */
    bool threshold_unstable;           /**< Exceeds instability threshold */
    bool learning_collapsed;           /**< Below collapse threshold */
    bool metaplasticity_stuck;         /**< Below stuck threshold */
    uint32_t immune_trigger_severity;  /**< Combined severity (1-10) */
} bcm_abnormality_state_t;

/**
 * @brief BCM baseline metrics for comparison
 *
 * Healthy baseline to detect deviations
 */
typedef struct {
    float baseline_threshold_mean;     /**< Healthy theta_m average */
    float baseline_threshold_variance; /**< Healthy theta_m variance */
    float baseline_ltp_rate;           /**< Healthy LTP event rate */
    float baseline_ltd_rate;           /**< Healthy LTD event rate */
    float baseline_sliding_rate;       /**< Healthy theta_m adaptation rate */

    uint64_t samples_collected;        /**< Number of samples in baseline */
    bool baseline_established;         /**< Enough data collected */
} bcm_baseline_metrics_t;

/**
 * @brief Complete BCM-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    bcm_params_t* bcm_params;          /**< BCM parameters being modulated */

    /* Current state */
    cytokine_bcm_effects_t cytokine_effects;
    inflammation_bcm_state_t inflammation_state;
    bcm_abnormality_state_t abnormality_state;
    bcm_baseline_metrics_t baseline_metrics;

    /* Integration flags */
    bool enable_cytokine_modulation;
    bool enable_inflammation_disruption;
    bool enable_bcm_immune_trigger;
    bool enable_baseline_tracking;
    bool enable_recovery_assistance;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t immune_triggered_responses;
    uint32_t recovery_assists;
    uint32_t threshold_instabilities_detected;
    uint32_t learning_collapses_detected;
    uint32_t metaplasticity_failures_detected;
    } bcm_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_modulation;
    bool enable_inflammation_disruption;
    bool enable_bcm_immune_trigger;
    bool enable_baseline_tracking;
    bool enable_recovery_assistance;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float abnormality_sensitivity;     /**< Detection threshold multiplier [0.5-2.0] */

    /* Thresholds */
    float threshold_instability_factor; /**< Variance factor for instability [2.0-5.0] */
    float learning_collapse_factor;     /**< Activity factor for collapse [0.05-0.2] */
    float metaplasticity_stuck_factor;  /**< Change factor for stuck [0.005-0.02] */

    /* Baseline collection */
    uint64_t baseline_samples_required; /**< Samples needed to establish baseline */
} bcm_immune_config_t;

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
int bcm_immune_default_config(bcm_immune_config_t* config);

/**
 * @brief Create BCM-immune bridge
 *
 * WHAT: Initialize bidirectional BCM-immune integration
 * WHY:  Enable realistic immune-plasticity coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param bcm_params BCM parameters to modulate
 * @return New bridge or NULL on failure
 */
bcm_immune_bridge_t* bcm_immune_bridge_create(
    const bcm_immune_config_t* config,
    brain_immune_system_t* immune_system,
    bcm_params_t* bcm_params
);

/**
 * @brief Destroy BCM-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void bcm_immune_bridge_destroy(bcm_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → BCM API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to BCM parameters
 *
 * WHAT: Modulate BCM parameters based on cytokine levels
 * WHY:  Pro-inflammatory cytokines shift learning dynamics
 * HOW:  Query immune cytokines, adjust theta_m, eta, tau
 *
 * @param bridge BCM-immune bridge
 * @return 0 on success
 */
int bcm_immune_apply_cytokine_effects(bcm_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to BCM dynamics
 *
 * WHAT: Disrupt BCM homeostasis from prolonged inflammation
 * WHY:  Chronic inflammation causes plasticity dysregulation
 * HOW:  Check inflammation duration/level, destabilize threshold
 *
 * @param bridge BCM-immune bridge
 * @return 0 on success
 */
int bcm_immune_apply_inflammation_effects(bcm_immune_bridge_t* bridge);

/**
 * @brief Compute theta_m modulation from inflammation
 *
 * WHAT: Calculate threshold shift from immune state
 * WHY:  Inflammation alters LTP/LTD balance
 * HOW:  Map inflammation level/duration to theta multiplier
 *
 * @param bridge BCM-immune bridge
 * @return Theta multiplier [0.5-2.5]
 */
float bcm_immune_compute_theta_modulation(const bcm_immune_bridge_t* bridge);

/**
 * @brief Restore normal BCM parameters from IL-10
 *
 * WHAT: Accelerate recovery toward baseline BCM dynamics
 * WHY:  IL-10 promotes homeostatic restoration
 * HOW:  Query IL-10 level, move parameters toward baseline
 *
 * @param bridge BCM-immune bridge
 * @return 0 on success
 */
int bcm_immune_assist_recovery(bcm_immune_bridge_t* bridge);

/* ============================================================================
 * BCM → Immune API
 * ============================================================================ */

/**
 * @brief Update BCM baseline metrics
 *
 * WHAT: Collect healthy BCM statistics for deviation detection
 * WHY:  Need baseline to identify abnormalities
 * HOW:  Track theta_m, LTP/LTD rates, sliding rate
 *
 * @param bridge BCM-immune bridge
 * @param synapses Array of BCM synapses
 * @param num_synapses Number of synapses
 * @param stats Current BCM statistics
 * @return 0 on success
 */
int bcm_immune_update_baseline(
    bcm_immune_bridge_t* bridge,
    const bcm_synapse_t* synapses,
    uint32_t num_synapses,
    const bcm_stats_t* stats
);

/**
 * @brief Detect BCM abnormalities
 *
 * WHAT: Check for pathological BCM dynamics
 * WHY:  Abnormal plasticity indicates neural dysfunction
 * HOW:  Compare current metrics to baseline, detect outliers
 *
 * @param bridge BCM-immune bridge
 * @param synapses Array of BCM synapses
 * @param num_synapses Number of synapses
 * @param stats Current BCM statistics
 * @return 0 on success
 */
int bcm_immune_detect_abnormalities(
    bcm_immune_bridge_t* bridge,
    const bcm_synapse_t* synapses,
    uint32_t num_synapses,
    const bcm_stats_t* stats
);

/**
 * @brief Trigger immune response from BCM abnormality
 *
 * WHAT: Activate immune system from plasticity dysfunction
 * WHY:  Plasticity failure is neural damage requiring immune response
 * HOW:  Create antigen from abnormality, present to immune system
 *
 * @param bridge BCM-immune bridge
 * @return 0 on success
 */
int bcm_immune_trigger_from_abnormality(bcm_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update BCM-immune bridge (both directions)
 *
 * WHAT: Process all BCM-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, detect abnormalities, trigger immune
 *
 * @param bridge BCM-immune bridge
 * @param synapses Array of BCM synapses for monitoring
 * @param num_synapses Number of synapses
 * @param stats Current BCM statistics
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int bcm_immune_bridge_update(
    bcm_immune_bridge_t* bridge,
    const bcm_synapse_t* synapses,
    uint32_t num_synapses,
    const bcm_stats_t* stats,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine BCM effects
 *
 * @param bridge BCM-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int bcm_immune_get_cytokine_effects(
    const bcm_immune_bridge_t* bridge,
    cytokine_bcm_effects_t* effects
);

/**
 * @brief Get current inflammation BCM state
 *
 * @param bridge BCM-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int bcm_immune_get_inflammation_state(
    const bcm_immune_bridge_t* bridge,
    inflammation_bcm_state_t* state
);

/**
 * @brief Get BCM abnormality state
 *
 * @param bridge BCM-immune bridge
 * @param state Output abnormality state
 * @return 0 on success
 */
int bcm_immune_get_abnormality_state(
    const bcm_immune_bridge_t* bridge,
    bcm_abnormality_state_t* state
);

/**
 * @brief Check if BCM dynamics are healthy
 *
 * WHAT: Determine if BCM learning is within normal range
 * WHY:  Quick health check for monitoring
 * HOW:  Check if any abnormality flags are set
 *
 * @param bridge BCM-immune bridge
 * @return true if healthy (no abnormalities)
 */
bool bcm_immune_is_healthy(const bcm_immune_bridge_t* bridge);

/**
 * @brief Get threshold instability severity
 *
 * @param bridge BCM-immune bridge
 * @return Instability score [0-1]
 */
float bcm_immune_get_threshold_instability(const bcm_immune_bridge_t* bridge);

/**
 * @brief Get learning activity level
 *
 * @param bridge BCM-immune bridge
 * @return Activity score [0-1]
 */
float bcm_immune_get_learning_activity(const bcm_immune_bridge_t* bridge);

/**
 * @brief Get metaplasticity health
 *
 * @param bridge BCM-immune bridge
 * @return Health score [0-1]
 */
float bcm_immune_get_metaplasticity_health(const bcm_immune_bridge_t* bridge);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_BCM
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int bcm_immune_connect_bio_async(bcm_immune_bridge_t* bridge);

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
int bcm_immune_disconnect_bio_async(bcm_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool bcm_immune_is_bio_async_connected(const bcm_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BCM_IMMUNE_BRIDGE_H */
