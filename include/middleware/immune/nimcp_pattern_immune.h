/**
 * @file nimcp_pattern_immune.h
 * @brief Pattern Detection-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and pattern detection modules
 * WHY:  Neuroinflammation impairs pattern recognition; abnormal patterns indicate infection/damage
 * HOW:  Inflammation disrupts oscillation/synchrony/sequence detection accuracy;
 *       anomalous patterns trigger immune surveillance and antigen presentation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → PATTERN PATHWAYS:
 * -------------------------
 * 1. Neuroinflammation Effects on Neural Oscillations:
 *    - Pro-inflammatory cytokines (IL-1β, IL-6, TNF-α) disrupt oscillatory coherence
 *    - Gamma oscillation degradation (attention/binding impairment)
 *    - Theta-gamma coupling disruption (memory encoding failure)
 *    - Reference: Kucewicz et al. (2011) "Dysfunctional prefrontal cortical network activity"
 *
 * 2. Inflammation-Induced Synchrony Disruption:
 *    - Cytokines reduce neuronal excitability → desynchronization
 *    - Critical event detection impairment
 *    - Cross-correlation reduction between regions
 *    - Reference: Beattie et al. (2002) "Control of synaptic strength by glia-neuron interactions"
 *
 * 3. Sequence Detection Impairment:
 *    - Hippocampal inflammation disrupts replay sequences
 *    - Memory consolidation failure during neuroinflammation
 *    - Temporal pattern matching degradation
 *    - Reference: Hein et al. (2010) "IL-6 impairs long-term potentiation"
 *
 * PATTERN → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Pathological Oscillation Patterns:
 *    - Seizure-like hypersynchrony → immune alert
 *    - Abnormal slow-wave activity → surveillance trigger
 *    - Delta power increase during waking → threat detection
 *    - Reference: Vezzani et al. (2011) "The role of inflammation in epilepsy"
 *
 * 2. Anomalous Synchrony Detection:
 *    - Excessive synchrony (>0.9) → possible pathology
 *    - Loss of synchrony (<0.2) → network damage
 *    - Critical event storms → immune activation
 *    - Reference: de Curtis & Avanzini (2001) "Interictal spikes in focal epileptogenesis"
 *
 * 3. Abnormal Sequence Patterns:
 *    - Repetitive pathological sequences → immune response
 *    - Sequence replay failure → damage detection
 *    - Temporal pattern degradation → surveillance
 *    - Reference: Buzsáki (2015) "Hippocampal sharp wave-ripple: A cognitive biomarker"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    PATTERN-IMMUNE BRIDGE                                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → PATTERN PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────┐                                         │  ║
 * ║   │   │   INFLAMMATION       │                                         │  ║
 * ║   │   │ ───────────────────  │                                         │  ║
 * ║   │   │ LOCAL    → -10% acc  │  ─────┐                                 │  ║
 * ║   │   │ REGIONAL → -30% acc  │       │                                 │  ║
 * ║   │   │ SYSTEMIC → -60% acc  │       ├──→ Pattern Accuracy Degradation │  ║
 * ║   │   │ STORM    → -90% acc  │       │                                 │  ║
 * ║   │   └──────────────────────┘       │                                 │  ║
 * ║   │                                  ▼                                 │  ║
 * ║   │   ┌─────────────────────────────────────────┐                     │  ║
 * ║   │   │     PATTERN DETECTORS                   │                     │  ║
 * ║   │   │  - Oscillation coherence reduction      │                     │  ║
 * ║   │   │  - Synchrony index degradation          │                     │  ║
 * ║   │   │  - Sequence match strength loss         │                     │  ║
 * ║   │   │  - Pattern library mismatch increase    │                     │  ║
 * ║   │   └─────────────────────────────────────────┘                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  PATTERN → IMMUNE PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────┐                                         │  ║
 * ║   │   │  PATHOLOGICAL        │                                         │  ║
 * ║   │   │  PATTERNS            │                                         │  ║
 * ║   │   │ ───────────────────  │                                         │  ║
 * ║   │   │ Seizure oscillation  │ ──→ Immune Alert (Severity 8/10)        │  ║
 * ║   │   │ Hypersynchrony       │ ──→ Immune Alert (Severity 7/10)        │  ║
 * ║   │   │ Desynchronization    │ ──→ Immune Alert (Severity 6/10)        │  ║
 * ║   │   │ Sequence failure     │ ──→ Immune Alert (Severity 5/10)        │  ║
 * ║   │   │ Repetitive patterns  │ ──→ Immune Alert (Severity 6/10)        │  ║
 * ║   │   └──────────────────────┘                                         │  ║
 * ║   │                                  ▼                                 │  ║
 * ║   │   ┌─────────────────────────────────────────┐                     │  ║
 * ║   │   │     ANTIGEN PRESENTATION                │                     │  ║
 * ║   │   │  Anomalous patterns converted to        │                     │  ║
 * ║   │   │  immune signatures (epitopes)           │                     │  ║
 * ║   │   └─────────────────────────────────────────┘                     │  ║
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

#ifndef NIMCP_PATTERN_IMMUNE_H
#define NIMCP_PATTERN_IMMUNE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/patterns/nimcp_oscillation_detector.h"
#include "middleware/patterns/nimcp_synchrony_detector.h"
#include "middleware/patterns/nimcp_sequence_detector.h"
#include "middleware/patterns/nimcp_pattern_library.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Inflammation accuracy degradation factors */
#define INFLAMMATION_NONE_ACCURACY_FACTOR       1.00f   /**< No degradation */
#define INFLAMMATION_LOCAL_ACCURACY_FACTOR      0.90f   /**< 10% degradation */
#define INFLAMMATION_REGIONAL_ACCURACY_FACTOR   0.70f   /**< 30% degradation */
#define INFLAMMATION_SYSTEMIC_ACCURACY_FACTOR   0.40f   /**< 60% degradation */
#define INFLAMMATION_STORM_ACCURACY_FACTOR      0.10f   /**< 90% degradation */

/* Pathological pattern detection thresholds */
#define PATHOLOGICAL_GAMMA_MIN_HZ               100.0f  /**< Seizure-like gamma */
#define PATHOLOGICAL_DELTA_WAKING_POWER         0.4f    /**< Abnormal delta during waking */
#define PATHOLOGICAL_HYPERSYNCHRONY_THRESHOLD   0.90f   /**< Excessive synchrony */
#define PATHOLOGICAL_DESYNCHRONY_THRESHOLD      0.20f   /**< Loss of synchrony */
#define PATHOLOGICAL_REPETITION_COUNT           5       /**< Repetitive sequence threshold */
#define PATHOLOGICAL_SEQUENCE_FAILURE_RATE      0.7f    /**< High failure rate */

/* Immune alert severities for pattern anomalies */
#define PATTERN_ANOMALY_SEIZURE_SEVERITY        8       /**< Seizure-like activity (1-10) */
#define PATTERN_ANOMALY_HYPERSYNC_SEVERITY      7       /**< Hypersynchrony */
#define PATTERN_ANOMALY_DESYNC_SEVERITY         6       /**< Desynchronization */
#define PATTERN_ANOMALY_SEQUENCE_FAIL_SEVERITY  5       /**< Sequence failure */
#define PATTERN_ANOMALY_REPETITIVE_SEVERITY     6       /**< Repetitive patterns */

/* Maximum pattern anomalies tracked */
#define PATTERN_IMMUNE_MAX_ANOMALIES            64

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Pattern anomaly types that trigger immune surveillance
 *
 * BIOLOGICAL BASIS:
 * Different pattern disruptions indicate different types of neural pathology
 */
typedef enum {
    PATTERN_ANOMALY_NONE = 0,
    PATTERN_ANOMALY_SEIZURE_OSCILLATION,    /**< Epileptiform activity */
    PATTERN_ANOMALY_HYPERSYNCHRONY,         /**< Excessive synchronization */
    PATTERN_ANOMALY_DESYNCHRONIZATION,      /**< Loss of coordination */
    PATTERN_ANOMALY_SEQUENCE_FAILURE,       /**< Memory replay disruption */
    PATTERN_ANOMALY_REPETITIVE_SEQUENCE,    /**< Pathological repetition */
    PATTERN_ANOMALY_DELTA_INTRUSION,        /**< Delta waves during waking */
    PATTERN_ANOMALY_GAMMA_DISRUPTION,       /**< Gamma coherence loss */
    PATTERN_ANOMALY_THETA_GAMMA_UNCOUPLING  /**< Memory encoding failure */
} pattern_anomaly_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Inflammation effects on pattern detection
 *
 * How neuroinflammation degrades pattern recognition accuracy
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t inflammation_level;
    float inflammation_duration_sec;       /**< How long inflamed */

    /* Accuracy degradation factors */
    float oscillation_accuracy_factor;     /**< Oscillation detection accuracy [0-1] */
    float synchrony_accuracy_factor;       /**< Synchrony detection accuracy [0-1] */
    float sequence_accuracy_factor;        /**< Sequence detection accuracy [0-1] */
    float pattern_match_accuracy_factor;   /**< Pattern library match accuracy [0-1] */

    /* Specific disruptions */
    float gamma_coherence_loss;            /**< Gamma band degradation [0-1] */
    float theta_gamma_uncoupling;          /**< Cross-freq coupling loss [0-1] */
    float synchrony_reduction;             /**< Overall synchrony decrease [0-1] */
    float sequence_replay_failure_rate;    /**< Failed replay rate [0-1] */

    /* Temporal effects */
    float temporal_precision_loss;         /**< Timing accuracy degradation [0-1] */
    float pattern_completion_impairment;   /**< Partial pattern recognition loss [0-1] */
} inflammation_pattern_effects_t;

/**
 * @brief Detected pattern anomaly
 *
 * Represents a pathological pattern that triggers immune response
 */
typedef struct {
    uint32_t anomaly_id;                   /**< Unique anomaly identifier */
    pattern_anomaly_type_t type;           /**< Anomaly type */

    /* Detection details */
    double detection_time_ms;              /**< When detected */
    float severity;                        /**< Anomaly severity [0-1] */
    float confidence;                      /**< Detection confidence [0-1] */

    /* Pattern signature (converted to immune epitope) */
    uint8_t pattern_signature[BRAIN_IMMUNE_EPITOPE_SIZE];
    size_t signature_len;

    /* Associated immune response */
    uint32_t antigen_id;                   /**< Presented antigen ID */
    bool immune_alerted;                   /**< Immune system notified */

    /* Source detector */
    enum {
        ANOMALY_SOURCE_OSCILLATION,
        ANOMALY_SOURCE_SYNCHRONY,
        ANOMALY_SOURCE_SEQUENCE,
        ANOMALY_SOURCE_PATTERN_LIBRARY
    } source;
} pattern_anomaly_t;

/**
 * @brief Pathological oscillation state
 */
typedef struct {
    /* Seizure-like activity */
    bool has_seizure_oscillation;          /**< Epileptiform detected */
    float seizure_gamma_power;             /**< High gamma power (>100Hz) */

    /* Delta intrusion */
    bool has_delta_intrusion;              /**< Delta during waking */
    float delta_power_waking;              /**< Delta power during waking state */

    /* Gamma disruption */
    bool has_gamma_disruption;             /**< Loss of gamma coherence */
    float gamma_coherence;                 /**< Gamma coherence [0-1] */

    /* Theta-gamma coupling */
    bool has_theta_gamma_uncoupling;       /**< Coupling failure */
    float theta_gamma_coupling_strength;   /**< Coupling strength [0-1] */
} pathological_oscillation_state_t;

/**
 * @brief Pathological synchrony state
 */
typedef struct {
    /* Hypersynchrony */
    bool has_hypersynchrony;               /**< Excessive synchrony */
    float synchrony_index;                 /**< Current synchrony [0-1] */

    /* Desynchronization */
    bool has_desynchronization;            /**< Loss of synchrony */
    float mean_correlation;                /**< Mean pairwise correlation */

    /* Critical event storm */
    bool has_critical_event_storm;         /**< Too many critical events */
    uint32_t critical_events_per_sec;      /**< Event rate */
} pathological_synchrony_state_t;

/**
 * @brief Pathological sequence state
 */
typedef struct {
    /* Sequence failure */
    bool has_sequence_failure;             /**< High failure rate */
    float sequence_match_rate;             /**< Successful matches [0-1] */

    /* Repetitive sequences */
    bool has_repetitive_sequences;         /**< Pathological repetition */
    uint32_t max_repetition_count;         /**< Max consecutive repeats */

    /* Replay disruption */
    bool has_replay_disruption;            /**< Memory consolidation failure */
    float replay_accuracy;                 /**< Replay match strength [0-1] */
} pathological_sequence_state_t;

/**
 * @brief Complete pattern-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    oscillation_detector_t* oscillation_detector;
    synchrony_detector_t* synchrony_detector;
    sequence_detector_t* sequence_detector;
    pattern_library_t* pattern_library;

    /* Current state */
    inflammation_pattern_effects_t inflammation_effects;
    pathological_oscillation_state_t pathological_oscillation;
    pathological_synchrony_state_t pathological_synchrony;
    pathological_sequence_state_t pathological_sequence;

    /* Detected anomalies */
    pattern_anomaly_t* anomalies;
    size_t anomaly_count;
    size_t anomaly_capacity;
    uint32_t next_anomaly_id;

    /* Integration flags */
    bool enable_inflammation_degradation;
    bool enable_oscillation_monitoring;
    bool enable_synchrony_monitoring;
    bool enable_sequence_monitoring;
    bool enable_pattern_library_monitoring;

    /* Statistics */
    uint64_t total_updates;
    uint32_t total_anomalies_detected;
    uint32_t immune_alerts_triggered;
    uint32_t pattern_degradation_events;

    } pattern_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_inflammation_degradation;      /**< Inflammation degrades patterns */
    bool enable_oscillation_monitoring;        /**< Monitor oscillation anomalies */
    bool enable_synchrony_monitoring;          /**< Monitor synchrony anomalies */
    bool enable_sequence_monitoring;           /**< Monitor sequence anomalies */
    bool enable_pattern_library_monitoring;    /**< Monitor pattern library anomalies */

    /* Sensitivity tuning */
    float inflammation_sensitivity;            /**< Inflammation effect multiplier [0.5-2.0] */
    float anomaly_detection_sensitivity;       /**< Anomaly threshold multiplier [0.5-2.0] */

    /* Thresholds (override defaults if non-zero) */
    float pathological_hypersync_threshold;    /**< Custom hypersync threshold */
    float pathological_desync_threshold;       /**< Custom desync threshold */
    float pathological_sequence_fail_rate;     /**< Custom sequence failure rate */

    /* Capacity */
    size_t max_anomalies;                      /**< Maximum anomalies to track */
} pattern_immune_config_t;

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
int pattern_immune_default_config(pattern_immune_config_t* config);

/**
 * @brief Create pattern-immune bridge
 *
 * WHAT: Initialize bidirectional pattern-immune integration
 * WHY:  Enable realistic inflammation-pattern coupling and anomaly detection
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system (required)
 * @param oscillation_detector Oscillation detector (optional, can be NULL)
 * @param synchrony_detector Synchrony detector (optional, can be NULL)
 * @param sequence_detector Sequence detector (optional, can be NULL)
 * @param pattern_library Pattern library (optional, can be NULL)
 * @return New bridge or NULL on failure
 */
pattern_immune_bridge_t* pattern_immune_bridge_create(
    const pattern_immune_config_t* config,
    brain_immune_system_t* immune_system,
    oscillation_detector_t* oscillation_detector,
    synchrony_detector_t* synchrony_detector,
    sequence_detector_t* sequence_detector,
    pattern_library_t* pattern_library
);

/**
 * @brief Destroy pattern-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void pattern_immune_bridge_destroy(pattern_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Pattern API
 * ============================================================================ */

/**
 * @brief Apply inflammation effects to pattern detection
 *
 * WHAT: Degrade pattern detection accuracy based on inflammation level
 * WHY:  Neuroinflammation disrupts neural oscillations and synchrony
 * HOW:  Query immune inflammation level, scale detector accuracy factors
 *
 * @param bridge Pattern-immune bridge
 * @return 0 on success
 */
int pattern_immune_apply_inflammation_effects(pattern_immune_bridge_t* bridge);

/**
 * @brief Compute pattern accuracy degradation factor
 *
 * WHAT: Calculate accuracy multiplier from inflammation level
 * WHY:  Different inflammation levels cause different degradation
 * HOW:  Map inflammation level to accuracy factor [0-1]
 *
 * @param bridge Pattern-immune bridge
 * @param inflammation_level Current inflammation level
 * @return Accuracy factor [0-1]
 */
float pattern_immune_compute_accuracy_factor(
    const pattern_immune_bridge_t* bridge,
    brain_inflammation_level_t inflammation_level
);

/**
 * @brief Degrade oscillation detection
 *
 * WHAT: Apply inflammation effects to oscillation detector
 * WHY:  Cytokines disrupt oscillatory coherence
 * HOW:  Reduce band power sensitivity, increase noise
 *
 * @param bridge Pattern-immune bridge
 * @return 0 on success
 */
int pattern_immune_degrade_oscillation(pattern_immune_bridge_t* bridge);

/**
 * @brief Degrade synchrony detection
 *
 * WHAT: Apply inflammation effects to synchrony detector
 * WHY:  Cytokines reduce neuronal excitability and synchronization
 * HOW:  Reduce correlation coefficients, increase detection thresholds
 *
 * @param bridge Pattern-immune bridge
 * @return 0 on success
 */
int pattern_immune_degrade_synchrony(pattern_immune_bridge_t* bridge);

/**
 * @brief Degrade sequence detection
 *
 * WHAT: Apply inflammation effects to sequence detector
 * WHY:  Hippocampal inflammation disrupts replay and consolidation
 * HOW:  Reduce match strength, increase temporal tolerance
 *
 * @param bridge Pattern-immune bridge
 * @return 0 on success
 */
int pattern_immune_degrade_sequence(pattern_immune_bridge_t* bridge);

/**
 * @brief Degrade pattern library matching
 *
 * WHAT: Apply inflammation effects to pattern library
 * WHY:  Pattern completion and recognition impaired during inflammation
 * HOW:  Reduce similarity thresholds, increase mismatch tolerance
 *
 * @param bridge Pattern-immune bridge
 * @return 0 on success
 */
int pattern_immune_degrade_pattern_library(pattern_immune_bridge_t* bridge);

/* ============================================================================
 * Pattern → Immune API
 * ============================================================================ */

/**
 * @brief Detect pathological oscillation patterns
 *
 * WHAT: Identify seizure-like or abnormal oscillatory activity
 * WHY:  Pathological oscillations may indicate neural damage or infection
 * HOW:  Check for high gamma, delta intrusion, gamma disruption
 *
 * @param bridge Pattern-immune bridge
 * @param oscillation_result Latest oscillation detection result
 * @return 0 on success
 */
int pattern_immune_detect_pathological_oscillation(
    pattern_immune_bridge_t* bridge,
    const oscillation_result_t* oscillation_result
);

/**
 * @brief Detect pathological synchrony patterns
 *
 * WHAT: Identify hypersynchrony or desynchronization
 * WHY:  Abnormal synchrony indicates network pathology
 * HOW:  Check synchrony index extremes, critical event storms
 *
 * @param bridge Pattern-immune bridge
 * @param synchrony_result Latest synchrony detection result
 * @return 0 on success
 */
int pattern_immune_detect_pathological_synchrony(
    pattern_immune_bridge_t* bridge,
    const synchrony_result_t* synchrony_result
);

/**
 * @brief Detect pathological sequence patterns
 *
 * WHAT: Identify sequence failure or repetitive patterns
 * WHY:  Sequence disruption indicates memory system damage
 * HOW:  Check match rates, repetition counts, replay accuracy
 *
 * @param bridge Pattern-immune bridge
 * @param detections Latest sequence detections
 * @param num_detections Number of detections
 * @return 0 on success
 */
int pattern_immune_detect_pathological_sequence(
    pattern_immune_bridge_t* bridge,
    const sequence_detection_t* detections,
    uint32_t num_detections
);

/**
 * @brief Present pattern anomaly as immune antigen
 *
 * WHAT: Convert detected pattern anomaly to immune threat
 * WHY:  Enable immune response to neural pathology
 * HOW:  Create epitope from pattern signature, present to immune system
 *
 * @param bridge Pattern-immune bridge
 * @param anomaly Pattern anomaly to present
 * @return 0 on success
 */
int pattern_immune_present_anomaly(
    pattern_immune_bridge_t* bridge,
    pattern_anomaly_t* anomaly
);

/**
 * @brief Create pattern signature for anomaly
 *
 * WHAT: Generate unique pattern signature as immune epitope
 * WHY:  Pattern features → immune signature for threat tracking
 * HOW:  Hash pattern characteristics into fixed-size signature
 *
 * @param anomaly_type Anomaly type
 * @param pattern_features Feature vector (e.g., band powers, sync index)
 * @param num_features Number of features
 * @param signature Output signature buffer
 * @param signature_len Output signature length
 * @return 0 on success
 */
int pattern_immune_create_signature(
    pattern_anomaly_type_t anomaly_type,
    const float* pattern_features,
    uint32_t num_features,
    uint8_t* signature,
    size_t* signature_len
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update pattern-immune bridge (both directions)
 *
 * WHAT: Process all pattern-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply inflammation effects, detect anomalies, present antigens
 *
 * @param bridge Pattern-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int pattern_immune_bridge_update(
    pattern_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current inflammation pattern effects
 *
 * @param bridge Pattern-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int pattern_immune_get_inflammation_effects(
    const pattern_immune_bridge_t* bridge,
    inflammation_pattern_effects_t* effects
);

/**
 * @brief Get pathological oscillation state
 *
 * @param bridge Pattern-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int pattern_immune_get_pathological_oscillation_state(
    const pattern_immune_bridge_t* bridge,
    pathological_oscillation_state_t* state
);

/**
 * @brief Get pathological synchrony state
 *
 * @param bridge Pattern-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int pattern_immune_get_pathological_synchrony_state(
    const pattern_immune_bridge_t* bridge,
    pathological_synchrony_state_t* state
);

/**
 * @brief Get pathological sequence state
 *
 * @param bridge Pattern-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int pattern_immune_get_pathological_sequence_state(
    const pattern_immune_bridge_t* bridge,
    pathological_sequence_state_t* state
);

/**
 * @brief Get detected anomalies
 *
 * @param bridge Pattern-immune bridge
 * @param anomalies Output array for anomalies
 * @param max_anomalies Maximum anomalies to return
 * @param num_anomalies Output: number of anomalies returned
 * @return 0 on success
 */
int pattern_immune_get_anomalies(
    const pattern_immune_bridge_t* bridge,
    pattern_anomaly_t* anomalies,
    uint32_t max_anomalies,
    uint32_t* num_anomalies
);

/**
 * @brief Check if experiencing pattern degradation
 *
 * @param bridge Pattern-immune bridge
 * @return true if any pattern detection is degraded
 */
bool pattern_immune_is_degraded(const pattern_immune_bridge_t* bridge);

/**
 * @brief Get anomaly type as string
 */
const char* pattern_anomaly_type_to_string(pattern_anomaly_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PATTERN_IMMUNE_H */
