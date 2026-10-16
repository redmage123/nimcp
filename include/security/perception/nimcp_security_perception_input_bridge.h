/**
 * @file nimcp_security_perception_input_bridge.h
 * @brief Security-Perception Input Bridge - Input Validation for Audio/Visual Streams
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bridge for validating raw perceptual inputs (audio samples, image frames)
 *       before they enter the perception processing pipeline.
 * WHY:  Adversarial inputs can compromise perception systems at the earliest stage.
 *       Validating raw input data (before feature extraction) provides defense-in-depth.
 * HOW:  Integrate security validation with cochlea (audio) and visual cortex (visual),
 *       using BBB for input gating and anomaly detector for ML-based threat detection.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PERIPHERAL SENSORY GATING:
 * --------------------------
 * The biological nervous system filters sensory input at multiple stages:
 * - Retina: Contrast normalization, light adaptation
 * - Cochlea: Hair cell protection, acoustic reflex
 * - Thalamus: Sensory gating before cortical processing
 * - Peripheral nerves: Signal attenuation under damage
 *
 * This bridge implements computational analogues at the input stage:
 * - Range Validation: Ensure inputs within safe dynamic range
 * - Statistical Validation: Detect abnormal input distributions
 * - Anomaly Detection: ML-based detection of adversarial patterns
 * - Input Gating: Security-controlled attenuation of suspicious inputs
 *
 * ADVERSARIAL INPUT THREATS:
 * --------------------------
 * Raw input-level attacks include:
 * - Audio: Ultrasonic/infrasonic injection, hidden commands, audio adversarial
 * - Visual: Pixel-level perturbations, physical adversarial patches, spoofing
 * - Both: Out-of-range values, NaN/Inf injection, buffer overflow attempts
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |          SECURITY-PERCEPTION INPUT BRIDGE (Raw Input Validation)          |
 * +===========================================================================+
 * |                                                                           |
 * |   +-------------------+                      +-------------------+        |
 * |   |   Audio Input     |                      |   Visual Input    |        |
 * |   | (raw samples)     |                      |  (raw pixels)     |        |
 * |   +--------+----------+                      +--------+----------+        |
 * |            |                                          |                   |
 * |            v                                          v                   |
 * |   +--------+----------+                      +--------+----------+        |
 * |   | Audio Validation  |                      | Visual Validation |        |
 * |   | - Range check     |                      | - Range check     |        |
 * |   | - Stats check     |                      | - Stats check     |        |
 * |   | - Anomaly detect  |                      | - Anomaly detect  |        |
 * |   +--------+----------+                      +--------+----------+        |
 * |            |                                          |                   |
 * |            +----------------+     +-------------------+                   |
 * |                             |     |                                       |
 * |                             v     v                                       |
 * |                    +--------+-----+--------+                              |
 * |                    |     Input Gating      |                              |
 * |                    |  (Security Control)   |                              |
 * |                    +-----------+-----------+                              |
 * |                                |                                          |
 * |            +-------------------+-------------------+                      |
 * |            |                   |                   |                      |
 * |            v                   v                   v                      |
 * |   +--------+------+   +-------+-------+   +-------+-------+               |
 * |   |    BBB        |   | Anomaly       |   | Effects       |               |
 * |   | (validation)  |   | Detector      |   | (bidirectional|               |
 * |   +---------------+   | (ML-based)    |   |  flow)        |               |
 * |                       +---------------+   +---------------+               |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * DESIGN PATTERNS:
 * - Bridge: Connects security layer to perception inputs
 * - Strategy: Pluggable validation algorithms per modality
 * - Observer: Notification of validation failures
 * - Filter: Input gating based on security assessment
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

#ifndef NIMCP_SECURITY_PERCEPTION_INPUT_BRIDGE_H
#define NIMCP_SECURITY_PERCEPTION_INPUT_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bridge base */
#include "utils/bridge/nimcp_bridge_base.h"

/* Security modules */
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"

/* Perception modules */
#include "perception/nimcp_cochlea.h"
#include "perception/nimcp_visual_cortex.h"

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

/** Maximum audio samples per validation batch */
#define SEC_PERCEPT_INPUT_MAX_AUDIO_SAMPLES     8192

/** Maximum image dimensions for validation */
#define SEC_PERCEPT_INPUT_MAX_IMAGE_WIDTH       4096
#define SEC_PERCEPT_INPUT_MAX_IMAGE_HEIGHT      4096

/** Module identification */
#define SEC_PERCEPT_INPUT_MODULE_NAME           "security_perception_input_bridge"
#define BIO_MODULE_SEC_PERCEPT_INPUT            0x4010

/** Default thresholds */
#define SEC_PERCEPT_INPUT_DEFAULT_AUDIO_THRESHOLD     0.7f
#define SEC_PERCEPT_INPUT_DEFAULT_VISUAL_THRESHOLD    0.7f
#define SEC_PERCEPT_INPUT_DEFAULT_GATE_THRESHOLD      0.8f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct security_perception_input_bridge security_perception_input_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Input validation result codes
 *
 * WHAT: Outcome of input validation
 * WHY:  Distinguish between pass, soft failures, and hard failures
 */
typedef enum {
    SEC_INPUT_VALID = 0,               /**< Input passed all validation */
    SEC_INPUT_RANGE_WARNING,           /**< Input has range anomalies (soft) */
    SEC_INPUT_STATS_WARNING,           /**< Input has statistical anomalies (soft) */
    SEC_INPUT_ANOMALY_DETECTED,        /**< ML anomaly detected (hard) */
    SEC_INPUT_ADVERSARIAL_DETECTED,    /**< Adversarial pattern detected (hard) */
    SEC_INPUT_SPOOFING_DETECTED,       /**< Spoofing/replay detected (hard) */
    SEC_INPUT_MALFORMED,               /**< Input structurally invalid (hard) */
    SEC_INPUT_REJECTED                 /**< Input rejected by policy (hard) */
} sec_input_validation_result_t;

/**
 * @brief Input gating action
 *
 * WHAT: What to do with input based on security assessment
 * WHY:  Graduated response from pass-through to complete blocking
 */
typedef enum {
    SEC_INPUT_GATE_PASS = 0,           /**< Pass input unchanged */
    SEC_INPUT_GATE_ATTENUATE,          /**< Reduce input amplitude/intensity */
    SEC_INPUT_GATE_SANITIZE,           /**< Remove suspicious components */
    SEC_INPUT_GATE_HOLD,               /**< Hold for further analysis */
    SEC_INPUT_GATE_BLOCK               /**< Block input completely */
} sec_input_gate_action_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SEC_INPUT_STATE_UNINITIALIZED = 0, /**< Not yet initialized */
    SEC_INPUT_STATE_READY,             /**< Ready for validation */
    SEC_INPUT_STATE_PROCESSING,        /**< Currently processing */
    SEC_INPUT_STATE_DEGRADED,          /**< Partial functionality */
    SEC_INPUT_STATE_ERROR              /**< Error state */
} sec_input_state_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security-Perception Input Bridge Configuration
 *
 * WHAT: Configuration for input validation bridge
 * WHY:  Control validation strictness, features, and thresholds
 * HOW:  Set flags and thresholds for each validation component
 */
typedef struct {
    /* Audio validation settings */
    bool enable_audio_validation;      /**< Enable audio input validation */
    float audio_anomaly_threshold;     /**< Audio anomaly score threshold [0-1] */
    float audio_min_value;             /**< Minimum valid audio sample value */
    float audio_max_value;             /**< Maximum valid audio sample value */
    bool detect_ultrasonic;            /**< Detect ultrasonic content */
    bool detect_infrasonic;            /**< Detect infrasonic content */

    /* Visual validation settings */
    bool enable_visual_validation;     /**< Enable visual input validation */
    float visual_anomaly_threshold;    /**< Visual anomaly score threshold [0-1] */
    uint8_t visual_min_value;          /**< Minimum valid pixel value */
    uint8_t visual_max_value;          /**< Maximum valid pixel value */
    bool detect_adversarial_patches;   /**< Detect adversarial patch patterns */
    bool detect_spoofing;              /**< Detect spoofing/replay attacks */

    /* Anomaly detection settings */
    float statistical_threshold;       /**< Statistical anomaly threshold */
    float ml_confidence_threshold;     /**< ML detection confidence threshold */
    bool enable_online_learning;       /**< Enable online model updates */

    /* Gating settings */
    float gate_threshold;              /**< Threshold for input gating [0-1] */
    float attenuation_factor;          /**< Attenuation factor when gating [0-1] */
    bool enable_auto_gating;           /**< Enable automatic gating */

    /* Integration settings */
    bool enable_bbb;                   /**< Enable BBB integration */
    bool enable_anomaly_detector;      /**< Enable anomaly detector integration */
    bool enable_bio_async;             /**< Enable bio-async messaging */
    bool enable_logging;               /**< Enable detailed logging */
} sec_percept_input_config_t;

/* ============================================================================
 * Effects Structures (Bidirectional Flow)
 * ============================================================================ */

/**
 * @brief Security to Perception Effects
 *
 * WHAT: Effects flowing from security assessment to perception processing
 * WHY:  Allow security to modulate how perception handles inputs
 * HOW:  Threat levels gate or attenuate perception processing
 */
typedef struct {
    /* Threat assessment */
    float audio_threat_level;          /**< Current audio threat level [0-1] */
    float visual_threat_level;         /**< Current visual threat level [0-1] */
    float combined_threat_level;       /**< Combined threat assessment [0-1] */

    /* Gating controls */
    sec_input_gate_action_t audio_gate_action;  /**< Current audio gating action */
    sec_input_gate_action_t visual_gate_action; /**< Current visual gating action */
    float audio_attenuation;           /**< Audio attenuation factor [0-1] */
    float visual_attenuation;          /**< Visual attenuation factor [0-1] */

    /* Processing guidance */
    bool require_enhanced_validation;  /**< Request enhanced validation */
    bool require_cross_modal_check;    /**< Request cross-modal consistency */
    uint32_t suspicious_regions_mask;  /**< Bitmask of suspicious input regions */
} sec_to_percept_input_effects_t;

/**
 * @brief Perception to Security Effects
 *
 * WHAT: Information flowing from perception to security for analysis
 * WHY:  Provide security with raw input characteristics for anomaly detection
 * HOW:  Perception reports input statistics and potential anomalies
 */
typedef struct {
    /* Audio input statistics */
    float audio_mean;                  /**< Mean audio sample value */
    float audio_variance;              /**< Audio sample variance */
    float audio_peak_amplitude;        /**< Peak amplitude detected */
    float audio_dc_offset;             /**< DC offset in audio */
    uint32_t audio_sample_count;       /**< Number of samples processed */
    float audio_clipping_ratio;        /**< Ratio of clipped samples */

    /* Visual input statistics */
    float visual_mean_intensity;       /**< Mean pixel intensity */
    float visual_variance;             /**< Pixel value variance */
    float visual_contrast;             /**< Image contrast measure */
    float visual_edge_density;         /**< Edge density measure */
    uint32_t visual_pixel_count;       /**< Total pixels processed */
    float visual_saturation_ratio;     /**< Ratio of saturated pixels */

    /* Anomaly indicators */
    float audio_anomaly_score;         /**< Computed audio anomaly score [0-1] */
    float visual_anomaly_score;        /**< Computed visual anomaly score [0-1] */
    bool anomaly_flag_raised;          /**< Whether anomaly threshold exceeded */
    uint64_t anomaly_timestamp_us;     /**< When anomaly was detected */
} percept_to_sec_input_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Bridge State
 *
 * WHAT: Current operational state of the bridge
 * WHY:  Track validation state and connection status
 */
typedef struct {
    sec_input_state_t operational_state;    /**< Current operational state */

    /* Connection status */
    bool cochlea_connected;            /**< Cochlea connection status */
    bool visual_cortex_connected;      /**< Visual cortex connection status */
    bool bbb_connected;                /**< BBB connection status */
    bool anomaly_detector_connected;   /**< Anomaly detector connection status */

    /* Current validation state */
    sec_input_validation_result_t last_audio_result;  /**< Last audio validation result */
    sec_input_validation_result_t last_visual_result; /**< Last visual validation result */
    uint64_t last_validation_time_us;  /**< Timestamp of last validation */

    /* Active threat tracking */
    bool audio_threat_active;          /**< Audio threat currently active */
    bool visual_threat_active;         /**< Visual threat currently active */
    float current_audio_threat;        /**< Current audio threat score */
    float current_visual_threat;       /**< Current visual threat score */
} sec_percept_input_state_t;

/**
 * @brief Bridge Statistics
 *
 * WHAT: Cumulative statistics for bridge operation
 * WHY:  Monitor validation effectiveness and performance
 */
typedef struct {
    /* Validation counts */
    uint64_t audio_validations_total;  /**< Total audio validations performed */
    uint64_t audio_validations_passed; /**< Audio validations that passed */
    uint64_t audio_validations_failed; /**< Audio validations that failed */

    uint64_t visual_validations_total; /**< Total visual validations performed */
    uint64_t visual_validations_passed;/**< Visual validations that passed */
    uint64_t visual_validations_failed;/**< Visual validations that failed */

    /* Anomaly detection counts */
    uint64_t audio_anomalies_detected; /**< Audio anomalies detected */
    uint64_t visual_anomalies_detected;/**< Visual anomalies detected */
    uint64_t adversarial_detected;     /**< Adversarial patterns detected */
    uint64_t spoofing_detected;        /**< Spoofing attempts detected */

    /* Gating counts */
    uint64_t inputs_attenuated;        /**< Inputs attenuated */
    uint64_t inputs_sanitized;         /**< Inputs sanitized */
    uint64_t inputs_blocked;           /**< Inputs blocked */

    /* Performance metrics */
    float avg_audio_validation_time_us;/**< Average audio validation time */
    float avg_visual_validation_time_us;/**< Average visual validation time */
    float max_validation_time_us;      /**< Maximum validation time */

    /* False positive tracking */
    uint64_t false_positives_reported; /**< False positives reported */
    float estimated_precision;         /**< Estimated precision score */
} sec_percept_input_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Security-Perception Input Bridge
 *
 * WHAT: Main bridge structure for input validation
 * WHY:  Coordinate security validation of perceptual inputs
 * HOW:  Connect to cochlea, visual cortex, BBB, anomaly detector
 */
struct security_perception_input_bridge {
    /* Base bridge - MUST be first member */
    bridge_base_t base;

    /* Connected systems */
    cochlea_t* cochlea;                /**< Connected cochlea (audio) */
    visual_cortex_t* visual_cortex;    /**< Connected visual cortex */
    bbb_system_t bbb;                  /**< Connected BBB system */
    nimcp_anomaly_detector_t anomaly_detector; /**< Connected anomaly detector */

    /* Configuration */
    sec_percept_input_config_t config; /**< Bridge configuration */

    /* Bidirectional effects */
    sec_to_percept_input_effects_t sec_to_percept;   /**< Security->Perception effects */
    percept_to_sec_input_effects_t percept_to_sec;   /**< Perception->Security effects */

    /* State and statistics */
    sec_percept_input_state_t state;   /**< Current state */
    sec_percept_input_stats_t stats;   /**< Cumulative statistics */
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with balanced security/performance
 * HOW:  Return pre-configured structure with moderate thresholds
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error (null pointer)
 */
int security_perception_input_default_config(sec_percept_input_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create security-perception input bridge
 *
 * WHAT: Allocate and initialize bridge instance
 * WHY:  Enable security validation of perceptual inputs
 * HOW:  Allocate structure, initialize base, apply configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
security_perception_input_bridge_t* security_perception_input_bridge_create(
    const sec_percept_input_config_t* config
);

/**
 * @brief Destroy security-perception input bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, cleanup base, free memory
 *
 * @param bridge Bridge handle (NULL safe)
 */
void security_perception_input_bridge_destroy(
    security_perception_input_bridge_t* bridge
);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect cochlea for audio input validation
 *
 * WHAT: Link cochlea module to bridge
 * WHY:  Enable audio input validation before cochlear processing
 * HOW:  Store cochlea handle, update connection state
 *
 * @param bridge Bridge handle
 * @param cochlea Cochlea module handle
 * @return 0 on success, -1 on error
 */
int security_perception_input_connect_cochlea(
    security_perception_input_bridge_t* bridge,
    cochlea_t* cochlea
);

/**
 * @brief Connect visual cortex for visual input validation
 *
 * WHAT: Link visual cortex module to bridge
 * WHY:  Enable visual input validation before cortical processing
 * HOW:  Store visual cortex handle, update connection state
 *
 * @param bridge Bridge handle
 * @param visual_cortex Visual cortex module handle
 * @return 0 on success, -1 on error
 */
int security_perception_input_connect_visual_cortex(
    security_perception_input_bridge_t* bridge,
    visual_cortex_t* visual_cortex
);

/**
 * @brief Connect BBB for input validation
 *
 * WHAT: Link BBB system to bridge
 * WHY:  Use BBB's validation capabilities for raw inputs
 * HOW:  Store BBB handle for validation calls
 *
 * @param bridge Bridge handle
 * @param bbb BBB system handle
 * @return 0 on success, -1 on error
 */
int security_perception_input_connect_bbb(
    security_perception_input_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Connect anomaly detector for ML-based detection
 *
 * WHAT: Link anomaly detector to bridge
 * WHY:  Enable ML-based anomaly detection on raw inputs
 * HOW:  Store detector handle for analysis calls
 *
 * @param bridge Bridge handle
 * @param detector Anomaly detector handle
 * @return 0 on success, -1 on error
 */
int security_perception_input_connect_anomaly_detector(
    security_perception_input_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
);

/* ============================================================================
 * Audio Validation API
 * ============================================================================ */

/**
 * @brief Validate audio input samples
 *
 * WHAT: Validate raw audio samples before cochlear processing
 * WHY:  Detect adversarial audio, out-of-range values, anomalies
 * HOW:  Range check, statistical analysis, anomaly detection
 *
 * @param bridge Bridge handle
 * @param samples Audio sample buffer (float)
 * @param num_samples Number of samples
 * @param sample_rate Sample rate in Hz
 * @param result Output: validation result code
 * @return 0 on success, -1 on error
 */
int security_perception_validate_audio_input(
    security_perception_input_bridge_t* bridge,
    const float* samples,
    size_t num_samples,
    uint32_t sample_rate,
    sec_input_validation_result_t* result
);

/**
 * @brief Detect audio anomalies using ML
 *
 * WHAT: ML-based anomaly detection on audio input
 * WHY:  Detect sophisticated audio attacks beyond simple validation
 * HOW:  Feature extraction, anomaly scoring via connected detector
 *
 * @param bridge Bridge handle
 * @param samples Audio sample buffer
 * @param num_samples Number of samples
 * @param anomaly_score Output: anomaly score [0-1]
 * @param confidence Output: detection confidence [0-1]
 * @return 0 on success, -1 on error
 */
int security_perception_detect_audio_anomaly(
    security_perception_input_bridge_t* bridge,
    const float* samples,
    size_t num_samples,
    float* anomaly_score,
    float* confidence
);

/* ============================================================================
 * Visual Validation API
 * ============================================================================ */

/**
 * @brief Validate visual input frames
 *
 * WHAT: Validate raw image frames before visual cortex processing
 * WHY:  Detect adversarial images, pixel anomalies, spoofing
 * HOW:  Range check, statistical analysis, pattern detection
 *
 * @param bridge Bridge handle
 * @param pixels Image pixel buffer (uint8_t RGB or grayscale)
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels (1=grayscale, 3=RGB, 4=RGBA)
 * @param result Output: validation result code
 * @return 0 on success, -1 on error
 */
int security_perception_validate_visual_input(
    security_perception_input_bridge_t* bridge,
    const uint8_t* pixels,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    sec_input_validation_result_t* result
);

/**
 * @brief Detect visual anomalies (adversarial/spoofing)
 *
 * WHAT: ML-based detection of adversarial images and spoofing
 * WHY:  Detect sophisticated visual attacks beyond simple validation
 * HOW:  Feature extraction, pattern matching, anomaly scoring
 *
 * @param bridge Bridge handle
 * @param pixels Image pixel buffer
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels
 * @param anomaly_score Output: anomaly score [0-1]
 * @param confidence Output: detection confidence [0-1]
 * @return 0 on success, -1 on error
 */
int security_perception_detect_visual_anomaly(
    security_perception_input_bridge_t* bridge,
    const uint8_t* pixels,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    float* anomaly_score,
    float* confidence
);

/* ============================================================================
 * Input Gating API
 * ============================================================================ */

/**
 * @brief Apply security-based input gating
 *
 * WHAT: Gate (attenuate/block) input based on security assessment
 * WHY:  Protect perception systems from suspicious inputs
 * HOW:  Evaluate threat level, determine action, apply gating
 *
 * @param bridge Bridge handle
 * @param threat_score Current threat assessment [0-1]
 * @param action Output: gating action to take
 * @param attenuation Output: attenuation factor if applicable [0-1]
 * @return 0 on success, -1 on error
 */
int security_perception_gate_input(
    security_perception_input_bridge_t* bridge,
    float threat_score,
    sec_input_gate_action_t* action,
    float* attenuation
);

/**
 * @brief Apply gating to audio samples
 *
 * WHAT: Apply security gating to audio sample buffer
 * WHY:  Modify audio based on security assessment
 * HOW:  Scale samples by attenuation factor, zero if blocked
 *
 * @param bridge Bridge handle
 * @param samples Input/output audio sample buffer (modified in place)
 * @param num_samples Number of samples
 * @return 0 on success, -1 on error
 */
int security_perception_apply_audio_gating(
    security_perception_input_bridge_t* bridge,
    float* samples,
    size_t num_samples
);

/**
 * @brief Apply gating to visual pixels
 *
 * WHAT: Apply security gating to image pixel buffer
 * WHY:  Modify image based on security assessment
 * HOW:  Scale pixels by attenuation factor, zero if blocked
 *
 * @param bridge Bridge handle
 * @param pixels Input/output pixel buffer (modified in place)
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels
 * @return 0 on success, -1 on error
 */
int security_perception_apply_visual_gating(
    security_perception_input_bridge_t* bridge,
    uint8_t* pixels,
    uint32_t width,
    uint32_t height,
    uint32_t channels
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update security to perception effects
 *
 * WHAT: Update effects flowing from security to perception
 * WHY:  Propagate current threat assessment to perception modules
 * HOW:  Compute effects from current state, update effects structure
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_perception_input_update_sec_to_percept(
    security_perception_input_bridge_t* bridge
);

/**
 * @brief Update perception to security effects
 *
 * WHAT: Update effects flowing from perception to security
 * WHY:  Provide security with latest input statistics
 * HOW:  Collect statistics from connected perception modules
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_perception_input_update_percept_to_sec(
    security_perception_input_bridge_t* bridge
);

/**
 * @brief Perform full bidirectional update cycle
 *
 * WHAT: Update both directions in single call
 * WHY:  Convenience for regular update cycles
 * HOW:  Call both update functions in sequence
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_perception_input_update(
    security_perception_input_bridge_t* bridge
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get security to perception effects
 *
 * WHAT: Retrieve current security->perception effects
 * WHY:  Allow perception modules to query current security state
 * HOW:  Copy current effects to output structure
 *
 * @param bridge Bridge handle
 * @param effects Output: security->perception effects
 * @return 0 on success, -1 on error
 */
int security_perception_input_get_sec_to_percept_effects(
    const security_perception_input_bridge_t* bridge,
    sec_to_percept_input_effects_t* effects
);

/**
 * @brief Get perception to security effects
 *
 * WHAT: Retrieve current perception->security effects
 * WHY:  Allow security to query perception input statistics
 * HOW:  Copy current effects to output structure
 *
 * @param bridge Bridge handle
 * @param effects Output: perception->security effects
 * @return 0 on success, -1 on error
 */
int security_perception_input_get_percept_to_sec_effects(
    const security_perception_input_bridge_t* bridge,
    percept_to_sec_input_effects_t* effects
);

/**
 * @brief Get bridge state
 *
 * WHAT: Retrieve current bridge operational state
 * WHY:  Monitor bridge health and connections
 * HOW:  Copy current state to output structure
 *
 * @param bridge Bridge handle
 * @param state Output: bridge state
 * @return 0 on success, -1 on error
 */
int security_perception_input_get_state(
    const security_perception_input_bridge_t* bridge,
    sec_percept_input_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve cumulative bridge statistics
 * WHY:  Monitor validation effectiveness
 * HOW:  Copy current statistics to output structure
 *
 * @param bridge Bridge handle
 * @param stats Output: bridge statistics
 * @return 0 on success, -1 on error
 */
int security_perception_input_get_stats(
    const security_perception_input_bridge_t* bridge,
    sec_percept_input_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Reset all cumulative statistics to zero
 * WHY:  Start fresh measurement period
 * HOW:  Zero out statistics structure
 *
 * @param bridge Bridge handle
 */
void security_perception_input_reset_stats(
    security_perception_input_bridge_t* bridge
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get validation result name
 *
 * WHAT: Get human-readable name for validation result
 * WHY:  Logging and debugging
 * HOW:  Lookup in static string table
 *
 * @param result Validation result code
 * @return Human-readable name string
 */
const char* security_perception_input_result_name(
    sec_input_validation_result_t result
);

/**
 * @brief Get gate action name
 *
 * WHAT: Get human-readable name for gate action
 * WHY:  Logging and debugging
 * HOW:  Lookup in static string table
 *
 * @param action Gate action code
 * @return Human-readable name string
 */
const char* security_perception_input_gate_action_name(
    sec_input_gate_action_t action
);

/**
 * @brief Get state name
 *
 * WHAT: Get human-readable name for bridge state
 * WHY:  Logging and debugging
 * HOW:  Lookup in static string table
 *
 * @param state Bridge state code
 * @return Human-readable name string
 */
const char* security_perception_input_state_name(
    sec_input_state_t state
);

/**
 * @brief Report false positive
 *
 * WHAT: Report that a detection was a false positive
 * WHY:  Enable precision tracking and model improvement
 * HOW:  Increment false positive counter, optionally update model
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_perception_input_report_false_positive(
    security_perception_input_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_PERCEPTION_INPUT_BRIDGE_H */
