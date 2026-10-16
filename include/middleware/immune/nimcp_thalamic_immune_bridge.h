/**
 * @file nimcp_thalamic_immune_bridge.h
 * @brief Thalamic Router-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and thalamic routing
 * WHY:  Biological evidence shows inflammation affects thalamic filtering (hypervigilance
 *       during sickness), IL-6 modulates relay neurons, immune signals get priority routing
 * HOW:  Cytokines modulate routing priorities and gating thresholds, routing anomalies
 *       trigger immune alerts, threat signals get priority during inflammation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → ROUTING PATHWAYS:
 * -------------------------
 * 1. Cytokine Effects on Thalamic Gating:
 *    - IL-6 affects thalamic relay neurons (increases excitability)
 *    - Pro-inflammatory cytokines reduce sensory gating
 *    - Result: Hypervigilance, reduced filtering during inflammation
 *    - Reference: Capuron & Miller (2011) "Immune system to brain signaling"
 *
 * 2. Inflammation-Induced Hypervigilance:
 *    - Systemic inflammation → increased threat sensitivity
 *    - Thalamic reticular nucleus (TRN) gating threshold lowered
 *    - Enhanced detection of threat-related signals
 *    - Reference: Harrison et al. (2009) "Inflammation and neural response"
 *
 * 3. Priority Routing During Sickness:
 *    - Immune-related signals bypass normal filtering
 *    - Threat detection circuits get enhanced throughput
 *    - Social signals deprioritized (sickness behavior)
 *    - Reference: Dantzer et al. (2008) "Sickness behavior mechanisms"
 *
 * 4. IL-10 Restores Normal Gating:
 *    - Anti-inflammatory cytokines normalize sensory gating
 *    - Threshold restoration, reduced hypervigilance
 *    - Return to balanced attention allocation
 *    - Reference: Maes et al. (1999) "Anti-inflammatory effects"
 *
 * ROUTING → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Routing Anomalies as Threats:
 *    - Aberrant routing patterns suggest neural dysfunction
 *    - Queue overflow → resource exhaustion threat
 *    - Excessive signal dropping → system compromise
 *    - Trigger immune investigation (antigen presentation)
 *
 * 2. Priority Violations:
 *    - High-priority signal drops indicate severe threat
 *    - Repeated violations trigger inflammation
 *    - Pattern recognition for Byzantine behavior
 *
 * 3. Attention Allocation Failures:
 *    - Routing table corruption suggests malicious activity
 *    - Callback failures indicate module compromise
 *    - Anomaly detection feeds immune system
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    THALAMIC-IMMUNE BRIDGE                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → ROUTING PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-6  → +20% │  ───────┐                                       │  ║
 * ║   │   │ IL-1β → +15% │         │  Increase Routing Priority            │  ║
 * ║   │   │ TNF-α → +25% │         ├─→ (Threat signals, immune-related)   │  ║
 * ║   │   │              │         │                                       │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     THALAMIC ROUTER             │                             │  ║
 * ║   │   │  - Attention threshold lowered  │                             │  ║
 * ║   │   │  - Threat priority elevated     │                             │  ║
 * ║   │   │  - Social signals deprioritized │                             │  ║
 * ║   │   │  - Sensory gating reduced       │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   Normalize  │     Restore Normal Gating                       │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  ROUTING → IMMUNE PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ ANOMALIES    │ ──→ Antigen Presentation                        │  ║
 * ║   │   │ Queue Full   │ ──→ Resource Exhaustion Threat                  │  ║
 * ║   │   │ Signal Drops │ ──→ System Compromise Alert                     │  ║
 * ║   │   │ High Priority│ ──→ Critical Failure → Inflammation             │  ║
 * ║   │   │ Drop         │                                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ ROUTING OK   │ ──→ IL-10 Release (System Healthy)              │  ║
 * ║   │   │ Low Latency  │ ──→ Reduce Inflammation                         │  ║
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

#ifndef NIMCP_THALAMIC_IMMUNE_BRIDGE_H
#define NIMCP_THALAMIC_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/routing/nimcp_thalamic_router.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine routing modulation factors */
#define CYTOKINE_IL6_PRIORITY_BOOST      0.20f   /**< IL-6 → +20% priority */
#define CYTOKINE_IL1_PRIORITY_BOOST      0.15f   /**< IL-1β → +15% priority */
#define CYTOKINE_TNF_PRIORITY_BOOST      0.25f   /**< TNF-α → +25% priority */
#define CYTOKINE_IL10_GATING_RESTORE     0.30f   /**< IL-10 → restore gating */

/* Inflammation-based threshold modulation */
#define INFLAMMATION_GATING_REDUCTION    0.30f   /**< Lower threshold during inflammation */
#define INFLAMMATION_THREAT_PRIORITY     1.50f   /**< Boost threat signal priority */
#define INFLAMMATION_SOCIAL_DEGRADE      0.50f   /**< Reduce social signal priority */

/* Anomaly detection thresholds */
#define ROUTING_ANOMALY_QUEUE_THRESHOLD  0.85f   /**< Queue >85% full = anomaly */
#define ROUTING_ANOMALY_DROP_THRESHOLD   0.10f   /**< >10% drop rate = anomaly */
#define ROUTING_ANOMALY_LATENCY_MS       100.0f  /**< >100ms latency = anomaly */

/* Immune trigger thresholds */
#define ROUTING_IMMUNE_TRIGGER_DROPS     10      /**< High-priority drops to trigger */
#define ROUTING_IMMUNE_SEVERE_DROPS      50      /**< Total drops for severe response */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine routing effects
 *
 * How cytokine levels modulate routing behavior
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il6_priority_boost;        /**< IL-6 induced priority increase */
    float il1_priority_boost;        /**< IL-1β induced priority increase */
    float tnf_priority_boost;        /**< TNF-α induced priority increase */

    /* Anti-inflammatory effects */
    float il10_gating_restoration;   /**< IL-10 restores normal gating */

    /* Aggregate effects */
    float total_priority_modifier;   /**< Combined priority modulation */
    float gating_threshold_modifier; /**< Sensory gating adjustment */
    float threat_focus_level;        /**< Threat signal amplification [0-1] */
    float social_suppression_level;  /**< Social signal attenuation [0-1] */
} cytokine_routing_effects_t;

/**
 * @brief Inflammation routing state
 *
 * How inflammation affects routing priorities
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_intensity;     /**< Normalized intensity [0-1] */

    /* Routing modulation */
    float hypervigilance_level;       /**< Enhanced threat detection [0-1] */
    float gating_reduction;           /**< Reduced filtering [0-1] */
    float threat_priority_boost;      /**< Threat signal priority multiplier */
    float social_priority_penalty;    /**< Social signal priority multiplier */

    /* Behavioral state */
    bool sickness_behavior_active;    /**< Sickness behavior mode */
    float attention_bias;             /**< Bias toward threat attention */
} inflammation_routing_state_t;

/**
 * @brief Routing anomaly detection
 *
 * Routing metrics that trigger immune response
 */
typedef struct {
    /* Queue metrics */
    float queue_utilization;          /**< Queue depth / capacity [0-1] */
    uint64_t signals_queued;          /**< Total signals queued */
    uint64_t signals_dropped;         /**< Total signals dropped */
    uint64_t priority_drops;          /**< High-priority drops (critical) */

    /* Performance metrics */
    float avg_latency_ms;             /**< Average routing latency */
    float throughput_hz;              /**< Signals per second */
    float drop_rate;                  /**< Drop rate [0-1] */

    /* Anomaly flags */
    bool queue_critical;              /**< Queue near full */
    bool excessive_drops;             /**< Drop rate too high */
    bool high_latency;                /**< Latency exceeds threshold */
    bool priority_violations;         /**< High-priority signals dropped */

    /* Immune triggers */
    uint32_t anomaly_count;           /**< Cumulative anomalies */
    float threat_severity;            /**< Anomaly severity [0-1] */
} routing_anomaly_state_t;

/**
 * @brief Routing health immune feedback
 *
 * Healthy routing boosts anti-inflammatory response
 */
typedef struct {
    /* Health indicators */
    float routing_efficiency;         /**< Overall efficiency [0-1] */
    float queue_headroom;             /**< Available queue space [0-1] */
    float success_rate;               /**< 1 - drop_rate [0-1] */

    /* Immune benefits */
    float il10_boost;                 /**< IL-10 release boost from health */
    float inflammation_reduction;     /**< Reduced inflammation [0-1] */
} routing_health_feedback_t;

/**
 * @brief Complete thalamic-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    thalamic_router_t* thalamic_router;

    /* Current state */
    cytokine_routing_effects_t cytokine_effects;
    inflammation_routing_state_t inflammation_state;
    routing_anomaly_state_t anomaly_state;
    routing_health_feedback_t health_feedback;

    /* Integration flags */
    bool enable_cytokine_routing_modulation;
    bool enable_inflammation_hypervigilance;
    bool enable_routing_anomaly_detection;
    bool enable_health_feedback;
    bool enable_priority_escalation;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t immune_triggered_anomalies;
    uint32_t health_boosts;
    uint64_t priority_escalations;
    uint64_t gating_adjustments;
    } thalamic_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_routing_modulation;
    bool enable_inflammation_hypervigilance;
    bool enable_routing_anomaly_detection;
    bool enable_health_feedback;
    bool enable_priority_escalation;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float anomaly_sensitivity;         /**< Anomaly trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float queue_anomaly_threshold;     /**< Queue utilization for anomaly [0.7-0.9] */
    float drop_anomaly_threshold;      /**< Drop rate for anomaly [0.05-0.2] */
    float latency_anomaly_ms;          /**< Latency threshold [50-200] */
} thalamic_immune_config_t;

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
int thalamic_immune_default_config(thalamic_immune_config_t* config);

/**
 * @brief Create thalamic-immune bridge
 *
 * WHAT: Initialize bidirectional thalamic-immune integration
 * WHY:  Enable realistic immune-routing coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param thalamic_router Thalamic router
 * @return New bridge or NULL on failure
 */
thalamic_immune_bridge_t* thalamic_immune_bridge_create(
    const thalamic_immune_config_t* config,
    brain_immune_system_t* immune_system,
    thalamic_router_t* thalamic_router
);

/**
 * @brief Destroy thalamic-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void thalamic_immune_bridge_destroy(thalamic_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Routing API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to routing priorities
 *
 * WHAT: Modulate routing based on cytokine levels
 * WHY:  Pro-inflammatory cytokines increase threat priority
 * HOW:  Query immune cytokines, adjust router attention weights
 *
 * @param bridge Thalamic-immune bridge
 * @return 0 on success
 */
int thalamic_immune_apply_cytokine_effects(thalamic_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation to routing behavior
 *
 * WHAT: Induce hypervigilance and reduced gating from inflammation
 * WHY:  Systemic inflammation causes threat focus
 * HOW:  Lower gating threshold, boost threat signal priority
 *
 * @param bridge Thalamic-immune bridge
 * @return 0 on success
 */
int thalamic_immune_apply_inflammation_effects(thalamic_immune_bridge_t* bridge);

/**
 * @brief Escalate priority for immune-related signals
 *
 * WHAT: Boost priority of threat/immune signals during inflammation
 * WHY:  Ensure critical signals bypass normal filtering
 * HOH:  Set HIGH priority for immune-tagged signals
 *
 * @param bridge Thalamic-immune bridge
 * @param source_id Source module ID
 * @param dest_id Destination module ID
 * @param is_threat_signal True if threat-related
 * @return 0 on success
 */
int thalamic_immune_escalate_priority(
    thalamic_immune_bridge_t* bridge,
    uint32_t source_id,
    uint32_t dest_id,
    bool is_threat_signal
);

/**
 * @brief Restore normal gating from IL-10
 *
 * WHAT: Normalize routing thresholds from anti-inflammatory signals
 * WHY:  IL-10 restores balanced attention allocation
 * HOW:  Increase gating threshold, reduce priority bias
 *
 * @param bridge Thalamic-immune bridge
 * @return 0 on success
 */
int thalamic_immune_restore_gating(thalamic_immune_bridge_t* bridge);

/* ============================================================================
 * Routing → Immune API
 * ============================================================================ */

/**
 * @brief Detect routing anomalies
 *
 * WHAT: Monitor routing metrics for dysfunction
 * WHY:  Routing failures indicate potential threats
 * HOW:  Check queue depth, drop rate, latency against thresholds
 *
 * @param bridge Thalamic-immune bridge
 * @return 0 on success
 */
int thalamic_immune_detect_anomalies(thalamic_immune_bridge_t* bridge);

/**
 * @brief Trigger immune response from routing anomalies
 *
 * WHAT: Present routing failures as antigens
 * WHY:  Routing dysfunction suggests system compromise
 * HOW:  Create antigen from anomaly signature, severity based on metrics
 *
 * @param bridge Thalamic-immune bridge
 * @return 0 on success
 */
int thalamic_immune_trigger_from_anomaly(thalamic_immune_bridge_t* bridge);

/**
 * @brief Boost immunity from healthy routing
 *
 * WHAT: Release IL-10 when routing performs well
 * WHY:  Healthy routing indicates stable system
 * HOW:  Check efficiency metrics, trigger anti-inflammatory response
 *
 * @param bridge Thalamic-immune bridge
 * @return 0 on success
 */
int thalamic_immune_boost_from_health(thalamic_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update thalamic-immune bridge (both directions)
 *
 * WHAT: Process all immune-routing interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, detect anomalies, boost from health
 *
 * @param bridge Thalamic-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int thalamic_immune_bridge_update(
    thalamic_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine routing effects
 *
 * @param bridge Thalamic-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int thalamic_immune_get_cytokine_effects(
    const thalamic_immune_bridge_t* bridge,
    cytokine_routing_effects_t* effects
);

/**
 * @brief Get current inflammation routing state
 *
 * @param bridge Thalamic-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int thalamic_immune_get_inflammation_state(
    const thalamic_immune_bridge_t* bridge,
    inflammation_routing_state_t* state
);

/**
 * @brief Get routing anomaly state
 *
 * @param bridge Thalamic-immune bridge
 * @param state Output anomaly state
 * @return 0 on success
 */
int thalamic_immune_get_anomaly_state(
    const thalamic_immune_bridge_t* bridge,
    routing_anomaly_state_t* state
);

/**
 * @brief Check if in hypervigilance mode
 *
 * WHAT: Determine if inflammation inducing hypervigilance
 * WHY:  Hypervigilance is distinct routing state
 * HOW:  Check inflammation level and threat focus
 *
 * @param bridge Thalamic-immune bridge
 * @return true if hypervigilant
 */
bool thalamic_immune_is_hypervigilant(const thalamic_immune_bridge_t* bridge);

/**
 * @brief Get current gating threshold
 *
 * WHAT: Get effective sensory gating threshold
 * WHY:  Threshold varies with inflammation
 * HOW:  Base threshold + cytokine/inflammation modulation
 *
 * @param bridge Thalamic-immune bridge
 * @return Effective gating threshold [0-1]
 */
float thalamic_immune_get_gating_threshold(const thalamic_immune_bridge_t* bridge);

/**
 * @brief Get threat priority multiplier
 *
 * WHAT: Get current threat signal priority boost
 * WHY:  Threat priority increases with inflammation
 * HOW:  Base + inflammation-induced escalation
 *
 * @param bridge Thalamic-immune bridge
 * @return Priority multiplier [1.0-2.0]
 */
float thalamic_immune_get_threat_priority_multiplier(
    const thalamic_immune_bridge_t* bridge
);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_THALAMIC
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int thalamic_immune_connect_bio_async(thalamic_immune_bridge_t* bridge);

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
int thalamic_immune_disconnect_bio_async(thalamic_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool thalamic_immune_is_bio_async_connected(const thalamic_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THALAMIC_IMMUNE_BRIDGE_H */
