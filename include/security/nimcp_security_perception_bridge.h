/**
 * @file nimcp_security_perception_bridge.h
 * @brief Security-Perception Bridge - Sensory Threat Analysis and Defense
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Direct bridge between security layer and perception modules for analyzing sensory threats
 * WHY:  Adversarial inputs can exploit perception systems (adversarial images, audio attacks).
 *       This bridge provides real-time threat detection and defense at the sensory level.
 * HOW:  Integrate BBB, anomaly detector, and immune system with visual/audio/speech cortices
 *       to detect and quarantine malicious perceptual data before it corrupts cognition.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SENSORY GATING & THREAT DETECTION:
 * -----------------------------------
 * The biological nervous system filters sensory input for potential threats:
 * - Thalamus: Acts as sensory relay/gatekeeper before cortical processing
 * - Amygdala: Rapid threat detection in visual/auditory streams
 * - Prefrontal Cortex: Top-down filtering of suspicious inputs
 * - Immune System: Responds to "danger signals" from sensory pathways
 *
 * This bridge implements computational analogues:
 * - Thalamic Filtering: Pre-cortical anomaly screening
 * - Threat Salience: Dangerous inputs boost security attention
 * - Cross-Modal Detection: Coordinated threat detection across senses
 * - Immune Escalation: Sensory threats trigger immune response
 *
 * ADVERSARIAL INPUT DEFENSE:
 * ---------------------------
 * Modern adversarial examples exploit perception systems:
 * - Adversarial images: Carefully crafted pixels fool classifiers
 * - Audio attacks: Inaudible perturbations trigger misclassification
 * - Universal perturbations: Attack patterns that generalize
 *
 * Defense strategies:
 * - Statistical anomaly detection on raw sensory features
 * - Cross-modal consistency checks (visual + audio alignment)
 * - Immune memory of known attack patterns
 * - Quarantine and controlled analysis of suspicious inputs
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           SECURITY-PERCEPTION BRIDGE (Sensory Threat Defense)              ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   PERCEPTION MODULES (Inputs)                       │  ║
 * ║   │   ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐    │  ║
 * ║   │   │   Visual     │  │    Audio     │  │      Speech          │    │  ║
 * ║   │   │   Cortex     │  │   Cortex     │  │      Cortex          │    │  ║
 * ║   │   └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘    │  ║
 * ║   │          │                 │                      │                │  ║
 * ║   └──────────┼─────────────────┼──────────────────────┼────────────────┘  ║
 * ║              │                 │                      │                   ║
 * ║              ▼                 ▼                      ▼                   ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   THREAT ANALYSIS LAYER                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐ │  ║
 * ║   │   │ Visual Anomaly  │   │ Audio Anomaly   │   │ Cross-Modal     │ │  ║
 * ║   │   │   Detection     │   │   Detection     │   │   Consistency   │ │  ║
 * ║   │   │                 │   │                 │   │   Checker       │ │  ║
 * ║   │   │ • Feature stats │   │ • Spectral      │   │ • AV alignment  │ │  ║
 * ║   │   │ • Adversarial   │   │   anomalies     │   │ • Temporal sync │ │  ║
 * ║   │   │   patterns      │   │ • Inaudible     │   │ • Semantic match│ │  ║
 * ║   │   │ • Universal     │   │   frequencies   │   │                 │ │  ║
 * ║   │   │   perturbations │   │ • Attack sigs   │   │                 │ │  ║
 * ║   │   └─────────────────┘   └─────────────────┘   └─────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   SECURITY INTEGRATION                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────────────────────────────────────────────────────┐  │  ║
 * ║   │   │  Blood-Brain Barrier         Anomaly Detector               │  │  ║
 * ║   │   │  • Input validation          • Bayesian inference            │  │  ║
 * ║   │   │  • Quarantine regions        • Online learning               │  │  ║
 * ║   │   │  • Threat reporting          • Adaptive thresholds           │  │  ║
 * ║   │   └─────────────────────────────────────────────────────────────┘  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   IMMUNE ESCALATION PIPELINE                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   Perception Threat → BBB Validation → Anomaly Detection →         │  ║
 * ║   │   → Security Alert → Immune Antigen Presentation →                 │  ║
 * ║   │   → Adaptive Response (Quarantine/Memory/Antibodies)               │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   RESPONSE ACTIONS                                  │  ║
 * ║   │                                                                     │  ║
 * ║   │   • Sensory Quarantine: Isolate malicious features                 │  ║
 * ║   │   • Security Salience Boost: Increase attention to threats         │  ║
 * ║   │   • Cross-Modal Validation: Require multi-sense confirmation       │  ║
 * ║   │   • Memory Storage: Learn attack signatures                        │  ║
 * ║   │   • Bio-Async Alerts: Notify other modules                         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Bridge: Connects security and perception layers
 * - Chain of Responsibility: Threat detection escalation
 * - Strategy: Pluggable detection algorithms
 * - Observer: Threat notification callbacks
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * REFERENCES:
 * - Goodfellow et al. (2014) "Explaining and Harnessing Adversarial Examples"
 * - Carlini & Wagner (2017) "Towards Evaluating the Robustness of Neural Networks"
 * - Kurakin et al. (2018) "Adversarial Examples in the Physical World"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_PERCEPTION_BRIDGE_H
#define NIMCP_SECURITY_PERCEPTION_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Security modules */
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"

/* Perception modules */
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"

/* Immune system */
#include "cognitive/immune/nimcp_brain_immune.h"

/* Bio-async communication */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Utilities */
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SEC_PERCEPT_MAX_QUARANTINED       128    /**< Max quarantined inputs */
#define SEC_PERCEPT_MAX_ATTACK_SIGS       256    /**< Max known attack signatures */
#define SEC_PERCEPT_FEATURE_DIM           128    /**< Max feature dimension */
#define SEC_PERCEPT_MODULE_NAME           "security_perception_bridge"
#define SEC_PERCEPT_SIGNATURE_SIZE        64     /**< Attack signature size */

/* Threat score thresholds */
#define SEC_PERCEPT_THRESHOLD_LOW         0.3f   /**< Low threat threshold */
#define SEC_PERCEPT_THRESHOLD_MEDIUM      0.6f   /**< Medium threat threshold */
#define SEC_PERCEPT_THRESHOLD_HIGH        0.8f   /**< High threat threshold */
#define SEC_PERCEPT_THRESHOLD_CRITICAL    0.95f  /**< Critical threat threshold */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct security_perception_bridge security_perception_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Sensory modality types
 *
 * WHAT: Which sensory stream is being analyzed
 * WHY:  Different modalities have different threat patterns
 */
typedef enum {
    SENSORY_MODALITY_VISUAL = 0,       /**< Visual input (images) */
    SENSORY_MODALITY_AUDIO,            /**< Audio input (sound) */
    SENSORY_MODALITY_SPEECH,           /**< Speech input (language) */
    SENSORY_MODALITY_MULTIMODAL,       /**< Multiple modalities */
    SENSORY_MODALITY_COUNT
} sensory_modality_t;

/**
 * @brief Sensory threat types
 *
 * WHAT: Categories of adversarial attacks on perception
 * WHY:  Different threats require different defenses
 */
typedef enum {
    SENSORY_THREAT_NONE = 0,           /**< No threat detected */
    SENSORY_THREAT_ADVERSARIAL_EXAMPLE,/**< Adversarial perturbation */
    SENSORY_THREAT_UNIVERSAL_PERTURBATION, /**< Universal attack pattern */
    SENSORY_THREAT_BACKDOOR_TRIGGER,   /**< Backdoor activation pattern */
    SENSORY_THREAT_STATISTICAL_ANOMALY,/**< Statistical outlier */
    SENSORY_THREAT_CROSS_MODAL_MISMATCH,/**< Audio-visual inconsistency */
    SENSORY_THREAT_TEMPORAL_ANOMALY,   /**< Temporal pattern attack */
    SENSORY_THREAT_FREQUENCY_ATTACK,   /**< Inaudible frequency attack */
    SENSORY_THREAT_UNKNOWN             /**< Unknown threat pattern */
} sensory_threat_type_t;

/**
 * @brief Threat response actions
 *
 * WHAT: Actions to take when threat detected
 * WHY:  Graduated response based on severity
 */
typedef enum {
    THREAT_RESPONSE_ALLOW = 0,         /**< Allow input through */
    THREAT_RESPONSE_LOG,               /**< Log but allow */
    THREAT_RESPONSE_QUARANTINE,        /**< Isolate for analysis */
    THREAT_RESPONSE_SANITIZE,          /**< Strip suspicious features */
    THREAT_RESPONSE_REJECT,            /**< Block completely */
    THREAT_RESPONSE_IMMUNE_ESCALATE    /**< Escalate to immune system */
} threat_response_action_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SEC_PERCEPT_STATE_STOPPED = 0,     /**< Not operational */
    SEC_PERCEPT_STATE_STARTING,        /**< Initialization */
    SEC_PERCEPT_STATE_RUNNING,         /**< Active monitoring */
    SEC_PERCEPT_STATE_DEGRADED,        /**< Partial functionality */
    SEC_PERCEPT_STATE_ERROR            /**< Error state */
} sec_percept_state_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Sensory threat detection result
 *
 * WHAT: Result of analyzing sensory input for threats
 * WHY:  Provide actionable threat information
 */
typedef struct {
    sensory_modality_t modality;       /**< Input modality */
    sensory_threat_type_t threat_type; /**< Detected threat type */
    float threat_score;                /**< Threat probability (0-1) */
    float confidence;                  /**< Detection confidence (0-1) */

    /* Component scores */
    float statistical_anomaly_score;   /**< Statistical outlier score */
    float adversarial_score;           /**< Adversarial pattern score */
    float cross_modal_score;           /**< Cross-modal consistency score */

    /* Metadata */
    uint64_t timestamp_us;             /**< Detection time */
    uint32_t input_id;                 /**< Input identifier */
    threat_response_action_t recommended_action; /**< Recommended response */

    /* Explanation */
    char explanation[256];             /**< Human-readable explanation */
    uint8_t signature[SEC_PERCEPT_SIGNATURE_SIZE]; /**< Threat signature */
    size_t signature_len;              /**< Signature length */
} sensory_threat_result_t;

/**
 * @brief Quarantined sensory input
 *
 * WHAT: Isolated suspicious input for analysis
 * WHY:  Prevent contamination while studying attack
 */
typedef struct {
    uint32_t id;                       /**< Quarantine ID */
    sensory_modality_t modality;       /**< Input modality */
    float* features;                   /**< Extracted features */
    uint32_t feature_dim;              /**< Feature dimension */
    sensory_threat_result_t threat;    /**< Threat analysis */
    uint64_t quarantine_time;          /**< When quarantined */
    bool analyzed;                     /**< Analysis complete */
    bool neutralized;                  /**< Threat neutralized */
} quarantined_input_t;

/**
 * @brief Attack signature database entry
 *
 * WHAT: Known attack pattern for recognition
 * WHY:  Immune memory of previous attacks
 */
typedef struct {
    uint32_t id;                       /**< Signature ID */
    sensory_modality_t modality;       /**< Attack modality */
    sensory_threat_type_t type;        /**< Attack type */
    uint8_t signature[SEC_PERCEPT_SIGNATURE_SIZE]; /**< Attack signature */
    size_t signature_len;              /**< Signature length */
    float detection_threshold;         /**< Match threshold */
    uint32_t detection_count;          /**< Times detected */
    uint64_t last_seen;                /**< Last detection time */
} attack_signature_t;

/**
 * @brief Cross-modal consistency check result
 *
 * WHAT: Result of audio-visual alignment check
 * WHY:  Detect attacks exploiting single modality
 */
typedef struct {
    bool consistent;                   /**< Inputs are consistent */
    float visual_audio_sync;           /**< Temporal synchronization (0-1) */
    float semantic_alignment;          /**< Semantic match (0-1) */
    float overall_consistency;         /**< Overall consistency (0-1) */
    char mismatch_reason[128];         /**< Reason for mismatch */
} cross_modal_check_t;

/**
 * @brief Security-perception bridge configuration
 */
typedef struct {
    /* Detection thresholds */
    float visual_anomaly_threshold;    /**< Visual threat threshold */
    float audio_anomaly_threshold;     /**< Audio threat threshold */
    float cross_modal_threshold;       /**< Cross-modal mismatch threshold */
    float immune_escalation_threshold; /**< Escalate to immune threshold */

    /* Feature analysis */
    bool enable_statistical_checks;    /**< Enable statistical anomaly detection */
    bool enable_adversarial_detection; /**< Enable adversarial pattern detection */
    bool enable_cross_modal_validation;/**< Enable cross-modal consistency checks */
    bool enable_temporal_analysis;     /**< Enable temporal pattern analysis */

    /* Integration enables */
    bool enable_bbb;                   /**< Enable BBB integration */
    bool enable_anomaly_detector;      /**< Enable anomaly detector */
    bool enable_immune_system;         /**< Enable immune escalation */
    bool enable_bio_async;             /**< Enable bio-async messaging */

    /* Performance tuning */
    uint32_t max_quarantine_size;      /**< Max quarantined inputs */
    uint32_t max_attack_signatures;    /**< Max stored signatures */
    bool enable_online_learning;       /**< Learn from new threats */
    float learning_rate;               /**< Signature learning rate */

    /* Response policy */
    threat_response_action_t default_action; /**< Default response */
    bool auto_quarantine;              /**< Automatically quarantine threats */
    bool auto_immune_escalation;       /**< Auto-escalate to immune */
} sec_percept_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Input counts */
    uint64_t total_visual_inputs;      /**< Total visual inputs analyzed */
    uint64_t total_audio_inputs;       /**< Total audio inputs analyzed */
    uint64_t total_multimodal_inputs;  /**< Total multimodal inputs */

    /* Threat counts */
    uint64_t threats_detected;         /**< Total threats detected */
    uint64_t adversarial_detected;     /**< Adversarial examples detected */
    uint64_t statistical_anomalies;    /**< Statistical anomalies detected */
    uint64_t cross_modal_mismatches;   /**< Cross-modal mismatches */

    /* Response counts */
    uint64_t inputs_quarantined;       /**< Inputs quarantined */
    uint64_t inputs_rejected;          /**< Inputs rejected */
    uint64_t immune_escalations;       /**< Escalations to immune */

    /* Signature learning */
    uint32_t signatures_learned;       /**< Attack signatures learned */
    uint32_t signature_matches;        /**< Signature matches */

    /* Performance */
    float avg_analysis_time_us;        /**< Average analysis time */
    float max_analysis_time_us;        /**< Max analysis time */

    /* Current state */
    uint32_t current_quarantine_count; /**< Currently quarantined */
    sec_percept_state_t state;         /**< Current state */
} sec_percept_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default security-perception bridge configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with balanced security
 * HOW:  Return pre-configured structure with moderate thresholds
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sec_percept_default_config(sec_percept_config_t* config);

/**
 * @brief Create security-perception bridge
 *
 * WHAT: Initialize bridge infrastructure
 * WHY:  Connect security and perception layers
 * HOW:  Allocate structures, initialize detectors
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
security_perception_bridge_t* sec_percept_create(const sec_percept_config_t* config);

/**
 * @brief Destroy security-perception bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free quarantine, signatures, disconnect integrations
 *
 * @param bridge Bridge handle (NULL safe)
 */
void sec_percept_destroy(security_perception_bridge_t* bridge);

/**
 * @brief Start bridge monitoring
 *
 * WHAT: Begin active threat monitoring
 * WHY:  Activate security processing
 * HOW:  Set state to RUNNING, register with bio-async
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_percept_start(security_perception_bridge_t* bridge);

/**
 * @brief Stop bridge monitoring
 *
 * WHAT: Halt threat monitoring
 * WHY:  Graceful shutdown
 * HOW:  Set state to STOPPED, unregister
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_percept_stop(security_perception_bridge_t* bridge);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect Blood-Brain Barrier
 *
 * WHAT: Link BBB for input validation and quarantine
 * WHY:  Leverage BBB's input validation capabilities
 * HOW:  Store BBB handle for validation calls
 *
 * @param bridge Bridge handle
 * @param bbb BBB system handle
 * @return 0 on success, -1 on error
 */
int sec_percept_connect_bbb(
    security_perception_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Connect anomaly detector
 *
 * WHAT: Link anomaly detector for statistical analysis
 * WHY:  Use Bayesian inference for threat detection
 * HOW:  Store detector handle
 *
 * @param bridge Bridge handle
 * @param detector Anomaly detector handle
 * @return 0 on success, -1 on error
 */
int sec_percept_connect_anomaly_detector(
    security_perception_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
);

/**
 * @brief Connect brain immune system
 *
 * WHAT: Link immune system for escalation
 * WHY:  Enable adaptive immune response to threats
 * HOW:  Store immune handle for antigen presentation
 *
 * @param bridge Bridge handle
 * @param immune Brain immune system handle
 * @return 0 on success, -1 on error
 */
int sec_percept_connect_immune(
    security_perception_bridge_t* bridge,
    brain_immune_system_t* immune
);

/**
 * @brief Connect visual cortex
 *
 * WHAT: Link visual cortex for feature extraction
 * WHY:  Analyze visual threats in context
 * HOW:  Store cortex handle
 *
 * @param bridge Bridge handle
 * @param cortex Visual cortex handle
 * @return 0 on success, -1 on error
 */
int sec_percept_connect_visual_cortex(
    security_perception_bridge_t* bridge,
    visual_cortex_t* cortex
);

/**
 * @brief Connect audio cortex
 *
 * WHAT: Link audio cortex for feature extraction
 * WHY:  Analyze audio threats in context
 * HOW:  Store cortex handle
 *
 * @param bridge Bridge handle
 * @param cortex Audio cortex handle
 * @return 0 on success, -1 on error
 */
int sec_percept_connect_audio_cortex(
    security_perception_bridge_t* bridge,
    audio_cortex_t* cortex
);

/* ============================================================================
 * Threat Detection API
 * ============================================================================ */

/**
 * @brief Analyze visual input for threats
 *
 * WHAT: Scan visual features for adversarial patterns
 * WHY:  Detect adversarial images before cortical processing
 * HOW:  Statistical checks + adversarial pattern matching
 *
 * @param bridge Bridge handle
 * @param features Visual features (from cortex)
 * @param feature_dim Feature dimension
 * @param result Output: threat analysis result
 * @return 0 on success, -1 on error
 */
int sec_percept_analyze_visual(
    security_perception_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    sensory_threat_result_t* result
);

/**
 * @brief Analyze audio input for threats
 *
 * WHAT: Scan audio features for attacks
 * WHY:  Detect inaudible frequency attacks, audio perturbations
 * HOW:  Spectral analysis + anomaly detection
 *
 * @param bridge Bridge handle
 * @param features Audio features (from cortex)
 * @param feature_dim Feature dimension
 * @param result Output: threat analysis result
 * @return 0 on success, -1 on error
 */
int sec_percept_analyze_audio(
    security_perception_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    sensory_threat_result_t* result
);

/**
 * @brief Analyze multimodal input for threats
 *
 * WHAT: Perform cross-modal consistency check
 * WHY:  Detect attacks exploiting single modality
 * HOW:  Compare visual + audio alignment
 *
 * @param bridge Bridge handle
 * @param visual_features Visual features
 * @param visual_dim Visual feature dimension
 * @param audio_features Audio features
 * @param audio_dim Audio feature dimension
 * @param result Output: threat analysis result
 * @return 0 on success, -1 on error
 */
int sec_percept_analyze_multimodal(
    security_perception_bridge_t* bridge,
    const float* visual_features,
    uint32_t visual_dim,
    const float* audio_features,
    uint32_t audio_dim,
    sensory_threat_result_t* result
);

/**
 * @brief Check cross-modal consistency
 *
 * WHAT: Verify audio-visual alignment
 * WHY:  Detect single-modality attacks
 * HOW:  Temporal sync + semantic alignment
 *
 * @param bridge Bridge handle
 * @param visual_features Visual features
 * @param visual_dim Visual feature dimension
 * @param audio_features Audio features
 * @param audio_dim Audio feature dimension
 * @param check Output: consistency check result
 * @return 0 on success, -1 on error
 */
int sec_percept_check_cross_modal(
    security_perception_bridge_t* bridge,
    const float* visual_features,
    uint32_t visual_dim,
    const float* audio_features,
    uint32_t audio_dim,
    cross_modal_check_t* check
);

/* ============================================================================
 * Quarantine Management API
 * ============================================================================ */

/**
 * @brief Quarantine suspicious input
 *
 * WHAT: Isolate input for further analysis
 * WHY:  Prevent contamination while studying threat
 * HOW:  Store in quarantine queue, mark for analysis
 *
 * @param bridge Bridge handle
 * @param threat Threat detection result
 * @param features Input features
 * @param feature_dim Feature dimension
 * @param quarantine_id Output: assigned quarantine ID
 * @return 0 on success, -1 on error
 */
int sec_percept_quarantine_input(
    security_perception_bridge_t* bridge,
    const sensory_threat_result_t* threat,
    const float* features,
    uint32_t feature_dim,
    uint32_t* quarantine_id
);

/**
 * @brief Get quarantined input
 *
 * WHAT: Retrieve quarantined input by ID
 * WHY:  Analyze or release quarantined input
 * HOW:  Lookup in quarantine queue
 *
 * @param bridge Bridge handle
 * @param quarantine_id Quarantine ID
 * @param input Output: quarantined input (pointer to internal storage)
 * @return 0 on success, -1 if not found
 */
int sec_percept_get_quarantined(
    const security_perception_bridge_t* bridge,
    uint32_t quarantine_id,
    const quarantined_input_t** input
);

/**
 * @brief Release quarantined input
 *
 * WHAT: Remove input from quarantine
 * WHY:  False positive or neutralized threat
 * HOW:  Remove from queue, free resources
 *
 * @param bridge Bridge handle
 * @param quarantine_id Quarantine ID
 * @return 0 on success, -1 if not found
 */
int sec_percept_release_quarantine(
    security_perception_bridge_t* bridge,
    uint32_t quarantine_id
);

/**
 * @brief Clear all quarantined inputs
 *
 * WHAT: Empty quarantine queue
 * WHY:  System reset or maintenance
 * HOW:  Free all quarantine entries
 *
 * @param bridge Bridge handle
 */
void sec_percept_clear_quarantine(security_perception_bridge_t* bridge);

/* ============================================================================
 * Attack Signature Management API
 * ============================================================================ */

/**
 * @brief Learn attack signature
 *
 * WHAT: Store attack pattern for future recognition
 * WHY:  Build immune memory of threats
 * HOW:  Extract signature, add to database
 *
 * @param bridge Bridge handle
 * @param threat Confirmed threat
 * @param features Threat features
 * @param feature_dim Feature dimension
 * @return 0 on success, -1 on error
 */
int sec_percept_learn_signature(
    security_perception_bridge_t* bridge,
    const sensory_threat_result_t* threat,
    const float* features,
    uint32_t feature_dim
);

/**
 * @brief Match attack signature
 *
 * WHAT: Check if features match known attack
 * WHY:  Fast recognition of known threats
 * HOW:  Compare against signature database
 *
 * @param bridge Bridge handle
 * @param features Input features
 * @param feature_dim Feature dimension
 * @param modality Input modality
 * @param signature_id Output: matched signature ID (if found)
 * @param match_score Output: match confidence (0-1)
 * @return 0 if match found, -1 if no match
 */
int sec_percept_match_signature(
    const security_perception_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    sensory_modality_t modality,
    uint32_t* signature_id,
    float* match_score
);

/**
 * @brief Get attack signature
 *
 * @param bridge Bridge handle
 * @param signature_id Signature ID
 * @param signature Output: signature (pointer to internal storage)
 * @return 0 on success, -1 if not found
 */
int sec_percept_get_signature(
    const security_perception_bridge_t* bridge,
    uint32_t signature_id,
    const attack_signature_t** signature
);

/**
 * @brief Clear attack signatures
 *
 * @param bridge Bridge handle
 */
void sec_percept_clear_signatures(security_perception_bridge_t* bridge);

/* ============================================================================
 * Immune Escalation API
 * ============================================================================ */

/**
 * @brief Escalate threat to immune system
 *
 * WHAT: Present sensory threat as antigen to immune
 * WHY:  Trigger adaptive immune response
 * HOW:  Convert threat to antigen, present to immune
 *
 * @param bridge Bridge handle
 * @param threat Threat to escalate
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success, -1 on error
 */
int sec_percept_escalate_to_immune(
    security_perception_bridge_t* bridge,
    const sensory_threat_result_t* threat,
    uint32_t* antigen_id
);

/**
 * @brief Boost security salience
 *
 * WHAT: Increase attention allocation to security threats
 * WHY:  Prioritize threat processing over normal inputs
 * HOW:  Modulate cortical attention based on threat level
 *
 * @param bridge Bridge handle
 * @param threat Detected threat
 * @param salience_boost Attention boost factor (1.0-2.0)
 * @return 0 on success, -1 on error
 */
int sec_percept_boost_security_salience(
    security_perception_bridge_t* bridge,
    const sensory_threat_result_t* threat,
    float salience_boost
);

/* ============================================================================
 * Statistics and Monitoring API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int sec_percept_get_stats(
    const security_perception_bridge_t* bridge,
    sec_percept_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void sec_percept_reset_stats(security_perception_bridge_t* bridge);

/**
 * @brief Get current state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
sec_percept_state_t sec_percept_get_state(
    const security_perception_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Communication API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async
 * WHY:  Enable threat notification messaging
 * HOW:  Register module, set up handlers
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_percept_connect_bio_async(security_perception_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int sec_percept_disconnect_bio_async(security_perception_bridge_t* bridge);

/**
 * @brief Send threat alert via bio-async
 *
 * WHAT: Broadcast threat detection to other modules
 * WHY:  Coordinate system-wide defense
 * HOW:  Send BIO_MSG_SECURITY_THREAT_DETECTED
 *
 * @param bridge Bridge handle
 * @param threat Threat to broadcast
 * @return 0 on success, -1 on error
 */
int sec_percept_send_threat_alert(
    security_perception_bridge_t* bridge,
    const sensory_threat_result_t* threat
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get modality name
 *
 * @param modality Sensory modality
 * @return Human-readable name
 */
const char* sec_percept_modality_name(sensory_modality_t modality);

/**
 * @brief Get threat type name
 *
 * @param type Threat type
 * @return Human-readable name
 */
const char* sec_percept_threat_type_name(sensory_threat_type_t type);

/**
 * @brief Get response action name
 *
 * @param action Response action
 * @return Human-readable name
 */
const char* sec_percept_response_action_name(threat_response_action_t action);

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return Human-readable name
 */
const char* sec_percept_state_name(sec_percept_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_PERCEPTION_BRIDGE_H */
