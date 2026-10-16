/**
 * @file nimcp_sequence_immune_bridge.h
 * @brief Sequence Detector-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and sequence detection
 * WHY:  Biological evidence shows inflammation impairs procedural learning and sequence timing,
 *       while anomalous sequences can indicate neural dysfunction requiring immune response.
 * HOW:  Cytokines modulate detection accuracy and timing tolerance, anomalous sequences
 *       trigger immune alerts, inflammation affects pattern matching thresholds.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → SEQUENCE PATHWAYS:
 * ---------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6) Affect Basal Ganglia:
 *    - Basal ganglia critical for sequence learning and motor programs
 *    - IL-6 disrupts striatal sequence encoding
 *    - Reduces timing precision in learned sequences
 *    - Impairs procedural memory consolidation
 *    - Reference: Harrison et al. (2009) "Inflammation impairs procedural learning"
 *
 * 2. Inflammation Reduces Sequence Detection Accuracy:
 *    - Sickness behavior → reduced attention to temporal patterns
 *    - Cytokines increase neural noise, reducing signal-to-noise ratio
 *    - Temporal tolerance increases (less precise matching)
 *    - Pattern matching threshold elevated (miss subtle sequences)
 *    - Reference: Yirmiya & Goshen (2011) "Immune modulation of learning and memory"
 *
 * 3. IL-10 (Anti-inflammatory) Restores Sequence Learning:
 *    - Resolution of inflammation → restored timing precision
 *    - Enhanced consolidation of procedural memories
 *    - Normalized basal ganglia function
 *    - Reference: Rizzo et al. (2018) "Anti-inflammatory treatment and cognition"
 *
 * SEQUENCE → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Anomalous Sequences Indicate Neural Dysfunction:
 *    - Corrupted temporal patterns may signal neural pathology
 *    - Unexpected sequence replay (e.g., during waking) → alert
 *    - Sequence timing violations suggest neural instability
 *    - Trigger immune surveillance of affected regions
 *    - Reference: Wilson & McNaughton (1994) "Reactivation of hippocampal ensembles"
 *
 * 2. Sequence Learning Failure Under Inflammation:
 *    - Inability to learn new sequences → feedback to immune system
 *    - Persistent sequence detection failures may indicate chronic inflammation
 *    - Poor template matching → potential inflammatory marker
 *    - Reference: Monje et al. (2003) "Inflammatory blockade restores learning"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  SEQUENCE DETECTOR-IMMUNE BRIDGE                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE → SEQUENCE PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → +50ms│  ───────┐                                       │  ║
 * ║   │   │ IL-6  → +30ms│         │                                       │  ║
 * ║   │   │ TNF-α → +70ms│         ├──→ Increased Timing Tolerance         │  ║
 * ║   │   │              │         │    (Reduced Precision)                │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │   SEQUENCE DETECTOR             │                             │  ║
 * ║   │   │  - Detection accuracy ↓         │                             │  ║
 * ║   │   │  - Timing tolerance ↑           │                             │  ║
 * ║   │   │  - Match threshold ↑            │                             │  ║
 * ║   │   │  - Learning rate ↓              │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   Restore    │     Recovery, Precision Restored                │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              SEQUENCE → IMMUNE PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │  ANOMALOUS SEQUENCES     │ ──→ Immune Alert                    │  ║
 * ║   │   │  TIMING VIOLATIONS       │ ──→ Surveillance Trigger            │  ║
 * ║   │   │  CORRUPTED PATTERNS      │ ──→ Regional Inflammation           │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │  SUCCESSFUL LEARNING     │ ──→ IL-10 Positive Feedback         │  ║
 * ║   │   │  NORMAL REPLAY           │ ──→ No Immune Activation            │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
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

#ifndef NIMCP_SEQUENCE_IMMUNE_BRIDGE_H
#define NIMCP_SEQUENCE_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/patterns/nimcp_sequence_detector.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine timing impact (milliseconds added to tolerance) */
#define CYTOKINE_IL1_TIMING_IMPACT      50.0f   /**< IL-1β → +50ms tolerance */
#define CYTOKINE_IL6_TIMING_IMPACT      30.0f   /**< IL-6 → +30ms tolerance */
#define CYTOKINE_TNF_TIMING_IMPACT      70.0f   /**< TNF-α → +70ms tolerance */
#define CYTOKINE_IFN_GAMMA_TIMING_IMPACT 20.0f  /**< IFN-γ → +20ms tolerance */

/* Inflammation detection accuracy impact */
#define INFLAMMATION_ACCURACY_MULTIPLIER_LOCAL     0.95f  /**< 5% reduction */
#define INFLAMMATION_ACCURACY_MULTIPLIER_REGIONAL  0.85f  /**< 15% reduction */
#define INFLAMMATION_ACCURACY_MULTIPLIER_SYSTEMIC  0.70f  /**< 30% reduction */
#define INFLAMMATION_ACCURACY_MULTIPLIER_STORM     0.50f  /**< 50% reduction */

/* Anomaly detection thresholds */
#define SEQUENCE_ANOMALY_THRESHOLD          0.3f   /**< Match strength < 0.3 = anomaly */
#define SEQUENCE_TIMING_VIOLATION_THRESHOLD 3.0f   /**< >3x expected timing = violation */
#define SEQUENCE_LEARNING_FAILURE_COUNT     5      /**< 5 failed template learns = alert */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on sequence detection
 *
 * Represents how cytokine levels modulate sequence detector parameters
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_timing_penalty;        /**< IL-1β added timing tolerance (ms) */
    float il6_timing_penalty;        /**< IL-6 added timing tolerance (ms) */
    float tnf_timing_penalty;        /**< TNF-α added timing tolerance (ms) */
    float ifn_gamma_timing_penalty;  /**< IFN-γ added timing tolerance (ms) */

    /* Anti-inflammatory effects */
    float il10_precision_boost;      /**< IL-10 improved precision */

    /* Aggregate effects */
    float total_timing_tolerance_ms; /**< Combined timing tolerance increase */
    float detection_accuracy_factor; /**< Detection accuracy multiplier [0-1] */
    float learning_impairment;       /**< Template learning difficulty [0-1] */
} cytokine_sequence_effects_t;

/**
 * @brief Inflammation impact on sequence detection
 *
 * How chronic inflammation affects temporal pattern processing
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* Sequence detection impacts */
    float accuracy_reduction;          /**< Reduced detection accuracy [0-1] */
    float timing_precision_loss;       /**< Increased timing jitter [0-1] */
    float pattern_threshold_increase;  /**< Higher match threshold needed */
    float learning_rate_reduction;     /**< Slower template learning [0-1] */

    /* Basal ganglia function */
    float procedural_memory_impairment; /**< Procedural learning deficit [0-1] */
    float sequence_consolidation_rate;  /**< Memory consolidation speed [0-1] */
} inflammation_sequence_state_t;

/**
 * @brief Sequence anomaly immune trigger
 *
 * How sequence anomalies trigger immune response
 */
typedef struct {
    /* Anomaly indicators */
    float anomalous_match_strength;    /**< Low match strength detected */
    float timing_violation_factor;     /**< Excessive timing deviation */
    uint32_t learning_failure_count;   /**< Consecutive learning failures */
    uint32_t corrupted_sequence_count; /**< Corrupted pattern detections */

    /* Immune triggers */
    bool alert_triggered;              /**< Immune alert sent */
    float immune_activation_level;     /**< Immune response strength [0-1] */
    uint32_t target_region_id;         /**< Brain region with anomaly */

    /* Tracking */
    uint32_t consecutive_failures;     /**< Consecutive detection failures */
    float anomaly_severity;            /**< Overall anomaly severity [0-1] */
} sequence_immune_trigger_t;

/**
 * @brief Successful sequence learning immune feedback
 *
 * How successful sequence detection benefits immune function
 */
typedef struct {
    /* Learning success indicators */
    float template_learning_success_rate; /**< Template learn success [0-1] */
    float detection_accuracy;             /**< Average match strength */
    uint32_t successful_detections;       /**< Count of good detections */

    /* Immune benefits */
    float il10_release_boost;             /**< Anti-inflammatory boost */
    float inflammation_reduction;         /**< Reduced inflammation [0-1] */
    bool positive_feedback_active;        /**< Positive loop engaged */
} sequence_immune_feedback_t;

/**
 * @brief Complete sequence-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    sequence_detector_t* sequence_detector;

    /* Current state */
    cytokine_sequence_effects_t cytokine_effects;
    inflammation_sequence_state_t inflammation_state;
    sequence_immune_trigger_t immune_trigger;
    sequence_immune_feedback_t positive_feedback;

    /* Baseline parameters (pre-modulation) */
    float baseline_temporal_tolerance_ms;
    float baseline_min_strength_threshold;
    uint32_t baseline_max_templates;

    /* Integration flags */
    bool enable_cytokine_modulation;
    bool enable_inflammation_impairment;
    bool enable_anomaly_immune_trigger;
    bool enable_positive_feedback;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t anomaly_alerts_sent;
    uint32_t positive_feedback_events;
    uint32_t detection_failures;
    uint32_t learning_failures;
    } sequence_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_modulation;
    bool enable_inflammation_impairment;
    bool enable_anomaly_immune_trigger;
    bool enable_positive_feedback;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float anomaly_sensitivity;         /**< Anomaly detection sensitivity [0.5-2.0] */

    /* Thresholds */
    float anomaly_match_threshold;     /**< Match strength for anomaly [0.1-0.5] */
    float timing_violation_threshold;  /**< Timing factor for violation [2.0-5.0] */
    uint32_t learning_failure_threshold; /**< Failures before alert [3-10] */
} sequence_immune_config_t;

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
int sequence_immune_default_config(sequence_immune_config_t* config);

/**
 * @brief Create sequence-immune bridge
 *
 * WHAT: Initialize bidirectional sequence-immune integration
 * WHY:  Enable realistic immune-sequence coupling
 * HOW:  Allocate structure, link subsystems, capture baseline params
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param sequence_detector Sequence detector system
 * @return New bridge or NULL on failure
 */
sequence_immune_bridge_t* sequence_immune_bridge_create(
    const sequence_immune_config_t* config,
    brain_immune_system_t* immune_system,
    sequence_detector_t* sequence_detector
);

/**
 * @brief Destroy sequence-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void sequence_immune_bridge_destroy(sequence_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Sequence API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to sequence detector
 *
 * WHAT: Modulate sequence detection based on cytokine levels
 * WHY:  Pro-inflammatory cytokines reduce timing precision
 * HOW:  Query immune system cytokines, adjust timing tolerance and thresholds
 *
 * @param bridge Sequence-immune bridge
 * @return 0 on success
 */
int sequence_immune_apply_cytokine_effects(sequence_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to sequence detector
 *
 * WHAT: Reduce detection accuracy and learning from prolonged inflammation
 * WHY:  Chronic inflammation impairs procedural memory
 * HOW:  Check inflammation duration/level, reduce detector performance
 *
 * @param bridge Sequence-immune bridge
 * @return 0 on success
 */
int sequence_immune_apply_inflammation_effects(sequence_immune_bridge_t* bridge);

/**
 * @brief Compute detection accuracy factor from inflammation
 *
 * WHAT: Calculate detection accuracy reduction from immune state
 * WHY:  Inflammation reduces signal-to-noise ratio in pattern matching
 * HOW:  Map inflammation level to accuracy multiplier
 *
 * @param bridge Sequence-immune bridge
 * @return Accuracy factor [0.5-1.0]
 */
float sequence_immune_compute_accuracy_factor(const sequence_immune_bridge_t* bridge);

/**
 * @brief Compute timing tolerance increase from cytokines
 *
 * WHAT: Calculate additional timing tolerance from cytokine levels
 * WHY:  Cytokines increase neural noise, reducing temporal precision
 * HOW:  Sum cytokine-specific timing penalties
 *
 * @param bridge Sequence-immune bridge
 * @return Added timing tolerance (ms)
 */
float sequence_immune_compute_timing_tolerance(const sequence_immune_bridge_t* bridge);

/* ============================================================================
 * Sequence → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from sequence anomaly
 *
 * WHAT: Activate immune system from anomalous sequence detection
 * WHY:  Corrupted sequences may indicate neural dysfunction
 * HOW:  Check match strength, timing violations, trigger immune alert
 *
 * @param bridge Sequence-immune bridge
 * @param detection Sequence detection result
 * @return 0 on success
 */
int sequence_immune_trigger_from_anomaly(
    sequence_immune_bridge_t* bridge,
    const sequence_detection_t* detection
);

/**
 * @brief Report sequence learning failure
 *
 * WHAT: Notify immune system of template learning failure
 * WHY:  Persistent learning failures may indicate chronic inflammation
 * HOW:  Increment failure count, trigger alert if threshold exceeded
 *
 * @param bridge Sequence-immune bridge
 * @return 0 on success
 */
int sequence_immune_report_learning_failure(sequence_immune_bridge_t* bridge);

/**
 * @brief Report sequence detection failure
 *
 * WHAT: Notify immune system of poor sequence matching
 * WHY:  Detection failures may indicate inflammatory impairment
 * HOW:  Track consecutive failures, escalate if persistent
 *
 * @param bridge Sequence-immune bridge
 * @param match_strength Actual match strength achieved
 * @return 0 on success
 */
int sequence_immune_report_detection_failure(
    sequence_immune_bridge_t* bridge,
    float match_strength
);

/**
 * @brief Boost immune function from successful sequence learning
 *
 * WHAT: Release IL-10 when sequence learning succeeds
 * WHY:  Successful learning indicates healthy basal ganglia function
 * HOW:  Track learning success, trigger anti-inflammatory response
 *
 * @param bridge Sequence-immune bridge
 * @return 0 on success
 */
int sequence_immune_boost_from_learning_success(sequence_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update sequence-immune bridge (both directions)
 *
 * WHAT: Process all immune-sequence interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, check for anomalies, process feedback
 *
 * @param bridge Sequence-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int sequence_immune_bridge_update(
    sequence_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine sequence effects
 *
 * @param bridge Sequence-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int sequence_immune_get_cytokine_effects(
    const sequence_immune_bridge_t* bridge,
    cytokine_sequence_effects_t* effects
);

/**
 * @brief Get current inflammation sequence state
 *
 * @param bridge Sequence-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int sequence_immune_get_inflammation_state(
    const sequence_immune_bridge_t* bridge,
    inflammation_sequence_state_t* state
);

/**
 * @brief Check if sequence detection is impaired
 *
 * WHAT: Determine if inflammation is significantly affecting detection
 * WHY:  Detection impairment is clinically significant state
 * HOW:  Check accuracy factor and timing tolerance thresholds
 *
 * @param bridge Sequence-immune bridge
 * @return true if detection is impaired
 */
bool sequence_immune_is_detection_impaired(const sequence_immune_bridge_t* bridge);

/**
 * @brief Get current detection accuracy factor
 *
 * @param bridge Sequence-immune bridge
 * @return Accuracy factor [0.5-1.0]
 */
float sequence_immune_get_accuracy_factor(const sequence_immune_bridge_t* bridge);

/**
 * @brief Get current timing tolerance
 *
 * @param bridge Sequence-immune bridge
 * @return Total timing tolerance (ms)
 */
float sequence_immune_get_timing_tolerance(const sequence_immune_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Sequence-immune bridge
 * @param total_updates Output: total update cycles
 * @param anomaly_alerts Output: anomaly alerts sent
 * @param learning_failures Output: learning failure count
 * @return 0 on success
 */
int sequence_immune_get_stats(
    const sequence_immune_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* anomaly_alerts,
    uint32_t* learning_failures
);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_SEQUENCE
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int sequence_immune_connect_bio_async(sequence_immune_bridge_t* bridge);

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
int sequence_immune_disconnect_bio_async(sequence_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool sequence_immune_is_bio_async_connected(const sequence_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SEQUENCE_IMMUNE_BRIDGE_H */
