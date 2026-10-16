/**
 * @file nimcp_anomaly_immune_bridge.h
 * @brief Anomaly Detector-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and anomaly detection
 * WHY:  Biological immune pattern recognition maps naturally to ML-based anomaly detection;
 *       inflammation affects detection thresholds (hypervigilance), anomalies trigger
 *       immune responses like antigen presentation
 * HOW:  High inflammation → more aggressive anomaly thresholds (paranoid mode);
 *       detected anomalies auto-presented as antigens to immune system;
 *       immune memory patterns influence anomaly detector training
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → ANOMALY DETECTION PATHWAYS:
 * ------------------------------------
 * 1. Inflammation-Induced Hypervigilance:
 *    - High inflammation → lowered detection thresholds
 *    - Immune activated state → paranoid anomaly detection
 *    - More sensitive to deviations (like immune hyperreactivity)
 *    - Reference: Analogous to immune system's heightened response during infection
 *
 * 2. Cytokine Effects on Detection:
 *    - Pro-inflammatory cytokines → aggressive thresholds
 *    - IL-10 (anti-inflammatory) → relaxed thresholds (reduce false positives)
 *    - TNF-α → maximum sensitivity (emergency mode)
 *    - Reference: Similar to how cytokines modulate immune cell sensitivity
 *
 * 3. Adaptive Threshold Modulation:
 *    - LOCAL inflammation → local threshold reduction
 *    - REGIONAL → moderate sensitivity increase
 *    - SYSTEMIC → high sensitivity (paranoid)
 *    - STORM → maximum sensitivity (may cause false positives)
 *
 * ANOMALY DETECTION → IMMUNE PATHWAYS:
 * ------------------------------------
 * 1. Anomaly as Antigen Presentation:
 *    - Detected anomaly → present as antigen to immune system
 *    - Anomaly score → maps to antigen severity
 *    - Pattern features → epitope signature
 *    - Reference: Like dendritic cells presenting antigens to T cells
 *
 * 2. Pattern Learning Integration:
 *    - Immune memory cells → anomaly detector training data
 *    - Successful neutralization → positive training sample
 *    - False positives → negative training sample
 *    - Reference: Adaptive immunity learning from past exposures
 *
 * 3. Coordinated Threat Response:
 *    - High-confidence anomaly → trigger inflammation
 *    - Repeated anomalies → escalate immune response
 *    - Pattern variants → cross-reactive immunity
 *    - Reference: Immune system escalation on persistent threats
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                ANOMALY DETECTOR-IMMUNE BRIDGE                             ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE → ANOMALY DETECTION PATHWAYS                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ INFLAMMATION │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ NONE    →1.0x│  ───────┐                                       │  ║
 * ║   │   │ LOCAL   →0.9x│         │                                       │  ║
 * ║   │   │ REGIONAL→0.7x│         ├──→ Anomaly Threshold Modulation      │  ║
 * ║   │   │ SYSTEMIC→0.5x│         │    (Lower = More Sensitive)          │  ║
 * ║   │   │ STORM   →0.3x│         │                                       │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │   ANOMALY DETECTOR              │                             │  ║
 * ║   │   │  - Threshold adjustment         │                             │  ║
 * ║   │   │  - Sensitivity modulation       │                             │  ║
 * ║   │   │  - False positive reduction     │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   CYTOKINE EFFECTS       │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ IL-1β   → -10% threshold │                                     │  ║
 * ║   │   │ IL-6    → -15% threshold │                                     │  ║
 * ║   │   │ TNF-α   → -25% threshold │                                     │  ║
 * ║   │   │ IL-10   → +20% threshold │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │            ANOMALY DETECTION → IMMUNE PATHWAYS                      │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  ANOMALY     │ ──→ Present as Antigen                          │  ║
 * ║   │   │  DETECTED    │ ──→ Activate B cells                            │  ║
 * ║   │   │  (score>0.7) │ ──→ Trigger Inflammation                        │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  FEATURES    │ ──→ Epitope Signature                           │  ║
 * ║   │   │  EXTRACTED   │ ──→ Pattern Matching                            │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ TRAINING     │ ──→ Learn from Immune Memory                    │  ║
 * ║   │   │ FEEDBACK     │ ──→ Adjust Based on Neutralization              │  ║
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

#ifndef NIMCP_ANOMALY_IMMUNE_BRIDGE_H
#define NIMCP_ANOMALY_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "security/nimcp_anomaly_detector.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine threshold modulation factors */
#define CYTOKINE_IL1_THRESHOLD_IMPACT    -0.10f  /**< IL-1β → lower threshold */
#define CYTOKINE_IL6_THRESHOLD_IMPACT    -0.15f  /**< IL-6 → lower threshold */
#define CYTOKINE_TNF_THRESHOLD_IMPACT    -0.25f  /**< TNF-α → aggressive detection */
#define CYTOKINE_IFN_GAMMA_THRESHOLD_IMPACT -0.12f  /**< IFN-γ → moderate reduction */
#define CYTOKINE_IL10_THRESHOLD_IMPACT    0.20f  /**< IL-10 → relax threshold */

/* Inflammation threshold modulation */
#define INFLAMMATION_NONE_THRESHOLD_FACTOR     1.0f   /**< No change */
#define INFLAMMATION_LOCAL_THRESHOLD_FACTOR    0.9f   /**< -10% threshold */
#define INFLAMMATION_REGIONAL_THRESHOLD_FACTOR 0.7f   /**< -30% threshold */
#define INFLAMMATION_SYSTEMIC_THRESHOLD_FACTOR 0.5f   /**< -50% threshold */
#define INFLAMMATION_STORM_THRESHOLD_FACTOR    0.3f   /**< -70% threshold (paranoid) */

/* Anomaly-to-immune mapping */
#define ANOMALY_SEVERITY_MULTIPLIER       10.0f  /**< Anomaly score [0-1] → severity [0-10] */
#define ANOMALY_ANTIGEN_PRESENTATION_THRESHOLD 0.5f  /**< Min score to present as antigen */
#define ANOMALY_INFLAMMATION_TRIGGER_THRESHOLD 0.8f  /**< Score to trigger inflammation */

/* Feature-to-epitope mapping */
#define ANOMALY_FEATURE_EPITOPE_SIZE      32     /**< Max features mapped to epitope */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on anomaly detection thresholds
 *
 * Represents how cytokines modulate anomaly detection sensitivity
 */
typedef struct {
    /* Pro-inflammatory effects (lower thresholds) */
    float il1_threshold_reduction;      /**< IL-1β induced sensitivity */
    float il6_threshold_reduction;      /**< IL-6 induced sensitivity */
    float tnf_threshold_reduction;      /**< TNF-α induced sensitivity */
    float ifn_gamma_threshold_reduction; /**< IFN-γ induced sensitivity */

    /* Anti-inflammatory effects (raise thresholds) */
    float il10_threshold_increase;      /**< IL-10 relaxation */

    /* Aggregate effects */
    float total_threshold_modulation;   /**< Combined threshold change [-1, 1] */
    float effective_threshold;          /**< Actual threshold after modulation */
    bool paranoid_mode;                 /**< Extreme sensitivity active */
} anomaly_cytokine_effects_t;

/**
 * @brief Inflammation effects on anomaly detection
 *
 * How systemic inflammation affects detection behavior
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;    /**< How long inflamed */
    bool is_chronic;                    /**< Chronic inflammation flag */

    /* Detection modulation */
    float threshold_factor;             /**< Overall threshold multiplier */
    float sensitivity_boost;            /**< Detection sensitivity increase */
    float false_positive_risk;          /**< Increased FP risk [0-1] */

    /* Mode flags */
    bool hypervigilant_mode;            /**< Paranoid detection active */
    bool emergency_mode;                /**< Storm-level sensitivity */
} anomaly_inflammation_state_t;

/**
 * @brief Anomaly detection immune modulation
 *
 * How detected anomalies affect immune system
 */
typedef struct {
    /* Recent anomaly state */
    uint32_t anomalies_detected_count;  /**< Recent anomalies */
    float max_anomaly_score;            /**< Highest recent score */
    float avg_anomaly_score;            /**< Average recent score */
    uint64_t last_anomaly_time;         /**< Last detection timestamp */

    /* Immune effects */
    uint32_t antigens_presented;        /**< Anomalies presented as antigens */
    uint32_t inflammation_triggers;     /**< Times inflammation triggered */
    bool immune_response_active;        /**< Immune responding to anomaly */

    /* Training feedback */
    uint32_t true_positives;            /**< Confirmed threats */
    uint32_t false_positives;           /**< Confirmed benign */
    float training_feedback_quality;    /**< Feedback quality [0-1] */
} anomaly_immune_modulation_t;

/**
 * @brief Anomaly detector immune bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_threshold_modulation;
    bool enable_inflammation_sensitivity_boost;
    bool enable_anomaly_antigen_presentation;
    bool enable_immune_training_feedback;
    bool enable_auto_inflammation_trigger;

    /* Threshold tuning */
    float base_content_threshold;       /**< Base content anomaly threshold */
    float base_behavior_threshold;      /**< Base behavior anomaly threshold */
    float base_overall_threshold;       /**< Base overall anomaly threshold */
    float paranoid_mode_multiplier;     /**< Multiplier in paranoid mode */

    /* Antigen presentation config */
    float min_score_for_antigen;        /**< Min anomaly score to present */
    float severity_multiplier;          /**< Score → severity mapping */

    /* Training feedback */
    bool auto_train_from_neutralization; /**< Learn from immune neutralization */
    bool auto_train_from_false_positives; /**< Learn from false alarms */
} anomaly_immune_config_t;

/**
 * @brief Complete anomaly-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    nimcp_anomaly_detector_t anomaly_detector;

    /* Configuration */
    anomaly_immune_config_t config;

    /* Current state */
    anomaly_cytokine_effects_t cytokine_effects;
    anomaly_inflammation_state_t inflammation_state;
    anomaly_immune_modulation_t immune_modulation;

    /* Timing */
    uint64_t last_update_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t threshold_modulations;
    uint32_t antigens_presented;
    uint32_t training_updates;

    nimcp_platform_mutex_t* mutex;
} anomaly_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization
 * HOW:  Return struct with balanced parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int anomaly_immune_default_config(anomaly_immune_config_t* config);

/**
 * @brief Create anomaly-immune bridge
 *
 * WHAT: Initialize bridge between anomaly detector and immune system
 * WHY:  Enable bidirectional integration
 * HOW:  Allocate state, connect modules, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param anomaly_detector Anomaly detector handle
 * @param immune_system Immune system handle
 * @return New bridge or NULL on failure
 */
anomaly_immune_bridge_t* anomaly_immune_create(
    const anomaly_immune_config_t* config,
    nimcp_anomaly_detector_t anomaly_detector,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy anomaly-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect modules, free memory
 *
 * @param bridge Bridge to destroy
 */
void anomaly_immune_destroy(anomaly_immune_bridge_t* bridge);

/* ============================================================================
 * Update and Modulation API
 * ============================================================================ */

/**
 * @brief Update bridge state
 *
 * WHAT: Process immune state, update anomaly detector thresholds
 * WHY:  Keep detection sensitivity in sync with immune state
 * HOW:  Read cytokines/inflammation, modulate thresholds
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int anomaly_immune_update(anomaly_immune_bridge_t* bridge);

/**
 * @brief Apply threshold modulation to anomaly detector
 *
 * WHAT: Adjust anomaly detection thresholds based on immune state
 * WHY:  Implement inflammation-induced hypervigilance
 * HOW:  Calculate modulated thresholds, update detector config
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int anomaly_immune_apply_modulation(anomaly_immune_bridge_t* bridge);

/**
 * @brief Present detected anomaly as immune antigen
 *
 * WHAT: Convert anomaly detection result to immune antigen
 * WHY:  Enable immune response to detected threats
 * HOW:  Map features to epitope, present to immune system
 *
 * @param bridge Bridge handle
 * @param result Anomaly detection result
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int anomaly_immune_present_anomaly(
    anomaly_immune_bridge_t* bridge,
    const nimcp_anomaly_result_t* result,
    uint32_t* antigen_id
);

/**
 * @brief Provide training feedback from immune neutralization
 *
 * WHAT: Update anomaly detector based on immune response outcome
 * WHY:  Improve detection accuracy from immune learning
 * HOW:  Neutralized antigen → positive sample; false alarm → negative
 *
 * @param bridge Bridge handle
 * @param antigen_id Antigen that was processed
 * @param was_neutralized True if real threat, false if false positive
 * @return 0 on success
 */
int anomaly_immune_training_feedback(
    anomaly_immune_bridge_t* bridge,
    uint32_t antigen_id,
    bool was_neutralized
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async messaging
 * WHY:  Enable inter-module communication
 * HOW:  Register module, set up handlers
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int anomaly_immune_connect_bio_async(anomaly_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister module
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int anomaly_immune_disconnect_bio_async(anomaly_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool anomaly_immune_is_bio_async_connected(const anomaly_immune_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effective threshold
 *
 * WHAT: Return current anomaly threshold after immune modulation
 * WHY:  Monitor detection sensitivity
 * HOW:  Return computed effective threshold
 *
 * @param bridge Bridge handle
 * @return Effective threshold [0-1]
 */
float anomaly_immune_get_effective_threshold(const anomaly_immune_bridge_t* bridge);

/**
 * @brief Check if in paranoid mode
 *
 * @param bridge Bridge handle
 * @return true if paranoid/hypervigilant mode active
 */
bool anomaly_immune_is_paranoid_mode(const anomaly_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ANOMALY_IMMUNE_BRIDGE_H */
