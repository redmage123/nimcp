/**
 * @file nimcp_lgss_input_validator.h
 * @brief LGSS Component A10: Perception Safety - Input Validation Layer
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Central input validation for all perceptual inputs entering the NIMCP system
 * WHY:  All external inputs are potential attack vectors; validation at the
 *       perception boundary prevents adversarial inputs from corrupting internal state
 * HOW:  Multi-stage validation: structure check, range check, anomaly detection,
 *       adversarial detection, and statistical validation per modality
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PERIPHERAL GATING:
 * ------------------
 * The biological nervous system implements input validation at multiple levels:
 * - Retina: Gain control adapts to light levels, preventing saturation
 * - Cochlea: Acoustic reflex protects against damaging sound levels
 * - Somatosensory: Habituation reduces response to constant stimuli
 * - Nociceptors: Pain response gates attention to protect the organism
 *
 * This module implements computational analogues:
 * - Structure Validation: Ensure inputs have expected format
 * - Range Validation: Check values are within safe bounds
 * - Anomaly Detection: Statistical outliers may indicate attacks
 * - Adversarial Detection: ML-based detection of adversarial perturbations
 *
 * THREAT MODEL:
 * -------------
 * - Visual: Adversarial patches, pixel perturbations, optical illusions
 * - Audio: Hidden commands, ultrasonic injection, adversarial audio
 * - Text: Prompt injection, jailbreaks, manipulation attempts
 * - Proprioceptive: False position/velocity signals, sensor spoofing
 * - Tactile: Phantom touch, sensor injection
 *
 * ARCHITECTURE:
 * ```
 * +========================================================================+
 * |                    INPUT VALIDATOR (Perception Safety)                  |
 * +========================================================================+
 * |                                                                         |
 * |   +-------------+  +-------------+  +-------------+  +-------------+   |
 * |   |   VISUAL    |  |   AUDIO     |  |    TEXT     |  | PROPRIO/    |   |
 * |   |   INPUT     |  |   INPUT     |  |    INPUT    |  | TACTILE     |   |
 * |   +------+------+  +------+------+  +------+------+  +------+------+   |
 * |          |                |                |                |          |
 * |          v                v                v                v          |
 * |   +------+------+  +------+------+  +------+------+  +------+------+   |
 * |   | Structure   |  | Structure   |  | Structure   |  | Structure   |   |
 * |   | Validation  |  | Validation  |  | Validation  |  | Validation  |   |
 * |   +------+------+  +------+------+  +------+------+  +------+------+   |
 * |          |                |                |                |          |
 * |          +-------+--------+-------+--------+                |          |
 * |                  |                                          |          |
 * |                  v                                          v          |
 * |   +==============+===========================================+         |
 * |   |              ANOMALY / ADVERSARIAL DETECTION             |         |
 * |   +=====================+====================================+         |
 * |                         |                                              |
 * |                         v                                              |
 * |   +=====[ VALIDATION RESULT: VALID / MALFORMED / ADVERSARIAL ]======+  |
 * |                                                                         |
 * +=========================================================================+
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

#ifndef NIMCP_LGSS_INPUT_VALIDATOR_H
#define NIMCP_LGSS_INPUT_VALIDATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Common utilities */
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

/** Magic number for input validator */
#define LGSS_INPUT_VALIDATOR_MAGIC 0x4C475356  /* "LGSV" */

/** Maximum input sizes per modality */
#define LGSS_MAX_VISUAL_WIDTH       4096
#define LGSS_MAX_VISUAL_HEIGHT      4096
#define LGSS_MAX_AUDIO_SAMPLES      65536
#define LGSS_MAX_TEXT_LENGTH        1048576   /* 1MB */
#define LGSS_MAX_PROPRIO_JOINTS     256
#define LGSS_MAX_TACTILE_POINTS     4096

/** Default thresholds */
#define LGSS_DEFAULT_ANOMALY_THRESHOLD      0.7f
#define LGSS_DEFAULT_ADVERSARIAL_THRESHOLD  0.8f
#define LGSS_DEFAULT_INJECTION_THRESHOLD    0.6f
#define LGSS_DEFAULT_OVERFLOW_THRESHOLD     0.9f

/*=============================================================================
 * ENUMERATIONS
 *============================================================================*/

/**
 * @brief Input validation status codes
 *
 * WHAT: Outcome of input validation
 * WHY:  Distinguish between different types of validation failures
 *       to enable appropriate responses
 */
typedef enum {
    LGSS_INPUT_VALID = 0,              /**< Input passed all validation */
    LGSS_INPUT_MALFORMED,              /**< Input structure is invalid */
    LGSS_INPUT_ADVERSARIAL,            /**< Adversarial perturbation detected */
    LGSS_INPUT_INJECTION,              /**< Injection attack detected (text) */
    LGSS_INPUT_OVERFLOW,               /**< Buffer overflow attempt detected */
    LGSS_INPUT_SUSPICIOUS              /**< Statistical anomaly detected */
} input_validation_status_t;

/**
 * @brief Input modality types
 *
 * WHAT: Supported input modality types
 * WHY:  Different modalities have different validation requirements
 */
typedef enum {
    LGSS_MODALITY_VISUAL = 0,          /**< Visual input (images, video frames) */
    LGSS_MODALITY_AUDIO,               /**< Audio input (audio samples) */
    LGSS_MODALITY_TEXT,                /**< Text input (strings, prompts) */
    LGSS_MODALITY_PROPRIOCEPTIVE,      /**< Proprioceptive input (joint states) */
    LGSS_MODALITY_TACTILE,             /**< Tactile input (touch sensors) */
    LGSS_MODALITY_COUNT                /**< Number of modalities */
} input_modality_t;

/**
 * @brief Validation check flags (bitmask)
 *
 * WHAT: Flags indicating which validation checks to perform
 * WHY:  Allow selective validation for performance tuning
 */
typedef enum {
    LGSS_CHECK_STRUCTURE    = (1 << 0), /**< Check input structure/format */
    LGSS_CHECK_RANGE        = (1 << 1), /**< Check value ranges */
    LGSS_CHECK_ANOMALY      = (1 << 2), /**< Run anomaly detection */
    LGSS_CHECK_ADVERSARIAL  = (1 << 3), /**< Run adversarial detection */
    LGSS_CHECK_INJECTION    = (1 << 4), /**< Check for injection (text) */
    LGSS_CHECK_OVERFLOW     = (1 << 5), /**< Check for overflow attempts */
    LGSS_CHECK_ALL          = 0x3F      /**< Run all checks */
} lgss_validation_flags_t;

/*=============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

typedef struct lgss_input_validator lgss_input_validator_t;

/*=============================================================================
 * CONFIGURATION STRUCTURE
 *============================================================================*/

/**
 * @brief Input validator configuration
 *
 * WHAT: Configuration for input validation behavior
 * WHY:  Allow tuning of validation strictness and enabled checks
 * HOW:  Set flags and thresholds for each validation component
 */
typedef struct {
    /* Per-modality enable flags */
    bool enable_visual_validation;     /**< Enable visual input validation */
    bool enable_audio_validation;      /**< Enable audio input validation */
    bool enable_text_validation;       /**< Enable text input validation */
    bool enable_proprio_validation;    /**< Enable proprioceptive validation */
    bool enable_tactile_validation;    /**< Enable tactile input validation */

    /* Detection thresholds */
    float anomaly_threshold;           /**< Anomaly detection threshold [0-1] */
    float adversarial_threshold;       /**< Adversarial detection threshold [0-1] */
    float injection_threshold;         /**< Injection detection threshold [0-1] */
    float overflow_threshold;          /**< Overflow detection threshold [0-1] */

    /* Per-modality thresholds */
    float visual_anomaly_threshold;    /**< Visual-specific anomaly threshold */
    float audio_anomaly_threshold;     /**< Audio-specific anomaly threshold */
    float text_anomaly_threshold;      /**< Text-specific anomaly threshold */

    /* Validation check flags (bitmask of lgss_validation_flags_t) */
    uint32_t validation_flags;         /**< Which checks to perform */

    /* Performance tuning */
    bool enable_caching;               /**< Cache validation results */
    bool enable_fast_mode;             /**< Skip expensive checks if early fail */
    uint32_t max_validation_time_ms;   /**< Maximum time for validation */

    /* Integration */
    bool enable_logging;               /**< Enable detailed logging */
    bool enable_bio_async;             /**< Enable bio-async notifications */
    uint32_t alert_module_id;          /**< Module to notify on threats */
} lgss_input_validator_config_t;

/*=============================================================================
 * VALIDATION RESULT STRUCTURES
 *============================================================================*/

/**
 * @brief Input validation result
 *
 * WHAT: Detailed result of input validation
 * WHY:  Provide actionable information about validation outcome
 * HOW:  Status code, confidence, triggered checks, and explanation
 */
typedef struct {
    input_validation_status_t status;  /**< Validation status */
    input_modality_t modality;         /**< Input modality */

    /* Confidence scores */
    float confidence;                  /**< Overall confidence [0-1] */
    float anomaly_score;               /**< Anomaly detection score [0-1] */
    float adversarial_score;           /**< Adversarial detection score [0-1] */
    float injection_score;             /**< Injection detection score [0-1] */

    /* Check results (bitmask) */
    uint32_t failed_checks;            /**< Bitmask of failed checks */

    /* Explanation */
    char explanation[256];             /**< Human-readable explanation */

    /* Metadata */
    uint64_t timestamp_us;             /**< Validation timestamp */
    uint32_t validation_time_us;       /**< Time taken for validation */
    size_t input_size;                 /**< Size of input validated */
} lgss_validation_result_t;

/**
 * @brief Input validator statistics
 *
 * WHAT: Cumulative statistics for validator operation
 * WHY:  Monitor validation effectiveness and performance
 */
typedef struct {
    /* Per-status counts */
    uint64_t total_validations;        /**< Total inputs validated */
    uint64_t valid_count;              /**< Inputs that passed validation */
    uint64_t malformed_count;          /**< Malformed inputs rejected */
    uint64_t adversarial_count;        /**< Adversarial inputs detected */
    uint64_t injection_count;          /**< Injection attempts detected */
    uint64_t overflow_count;           /**< Overflow attempts detected */
    uint64_t suspicious_count;         /**< Suspicious inputs flagged */

    /* Per-modality counts */
    uint64_t visual_validations;       /**< Visual inputs validated */
    uint64_t audio_validations;        /**< Audio inputs validated */
    uint64_t text_validations;         /**< Text inputs validated */
    uint64_t proprio_validations;      /**< Proprioceptive inputs validated */
    uint64_t tactile_validations;      /**< Tactile inputs validated */

    /* Performance metrics */
    float avg_validation_time_us;      /**< Average validation time */
    float max_validation_time_us;      /**< Maximum validation time */

    /* False positive tracking */
    uint64_t false_positives;          /**< Reported false positives */
    float estimated_precision;         /**< Estimated precision */
} lgss_validator_stats_t;

/*=============================================================================
 * INPUT DATA STRUCTURES
 *============================================================================*/

/**
 * @brief Visual input data
 */
typedef struct {
    const uint8_t* pixels;             /**< Pixel data (RGB/RGBA/grayscale) */
    uint32_t width;                    /**< Image width */
    uint32_t height;                   /**< Image height */
    uint32_t channels;                 /**< Number of channels (1/3/4) */
    uint8_t bits_per_channel;          /**< Bits per channel (8/16) */
} lgss_visual_input_t;

/**
 * @brief Audio input data
 */
typedef struct {
    const float* samples;              /**< Audio samples (float, normalized) */
    size_t num_samples;                /**< Number of samples */
    uint32_t sample_rate;              /**< Sample rate in Hz */
    uint32_t num_channels;             /**< Number of audio channels */
} lgss_audio_input_t;

/**
 * @brief Text input data
 */
typedef struct {
    const char* text;                  /**< Text content (UTF-8) */
    size_t length;                     /**< Text length in bytes */
    const char* source;                /**< Source identifier (optional) */
    bool is_user_input;                /**< Whether this is direct user input */
} lgss_text_input_t;

/**
 * @brief Proprioceptive input data
 */
typedef struct {
    const float* joint_positions;      /**< Joint position values */
    const float* joint_velocities;     /**< Joint velocity values (optional) */
    uint32_t num_joints;               /**< Number of joints */
    uint64_t timestamp_us;             /**< Sensor timestamp */
} lgss_proprio_input_t;

/**
 * @brief Tactile input data
 */
typedef struct {
    const float* pressure_values;      /**< Pressure sensor values */
    const float* positions;            /**< 3D positions (x,y,z per point) */
    uint32_t num_points;               /**< Number of touch points */
    uint64_t timestamp_us;             /**< Sensor timestamp */
} lgss_tactile_input_t;

/**
 * @brief Generic input wrapper
 */
typedef struct {
    input_modality_t modality;         /**< Input modality type */
    union {
        lgss_visual_input_t visual;
        lgss_audio_input_t audio;
        lgss_text_input_t text;
        lgss_proprio_input_t proprio;
        lgss_tactile_input_t tactile;
    } data;
} lgss_input_t;

/*=============================================================================
 * CONFIGURATION API
 *============================================================================*/

/**
 * @brief Get default input validator configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with balanced security/performance
 * HOW:  Return pre-configured structure with moderate thresholds
 *
 * @param config Output configuration structure
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
NIMCP_EXPORT nimcp_error_t lgss_input_validator_default_config(
    lgss_input_validator_config_t* config
);

/*=============================================================================
 * LIFECYCLE API
 *============================================================================*/

/**
 * @brief Create input validator
 *
 * WHAT: Allocate and initialize input validator instance
 * WHY:  Enable input validation for perceptual inputs
 * HOW:  Allocate structure, initialize components, apply configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Validator handle or NULL on failure
 */
NIMCP_EXPORT lgss_input_validator_t* lgss_input_validator_create(
    const lgss_input_validator_config_t* config
);

/**
 * @brief Destroy input validator
 *
 * WHAT: Clean up and free validator resources
 * WHY:  Proper resource deallocation
 * HOW:  Free internal structures, zero memory
 *
 * @param validator Validator handle (NULL safe)
 */
NIMCP_EXPORT void lgss_input_validator_destroy(
    lgss_input_validator_t* validator
);

/*=============================================================================
 * VALIDATION API
 *============================================================================*/

/**
 * @brief Validate input (ALL perceptual inputs pass through this)
 *
 * WHAT: Central validation function for all perceptual inputs
 * WHY:  Single entry point ensures all inputs are validated
 * HOW:  Route to modality-specific validation, aggregate results
 *
 * CRITICAL: This is the primary entry point for perception safety.
 * All external inputs MUST pass through this function before processing.
 *
 * @param validator Validator handle
 * @param input Input data wrapper
 * @param result Output: validation result
 * @return NIMCP_SUCCESS on success (check result for validation status)
 */
NIMCP_EXPORT nimcp_error_t lgss_input_validator_check(
    lgss_input_validator_t* validator,
    const lgss_input_t* input,
    lgss_validation_result_t* result
);

/**
 * @brief Validate visual input
 *
 * WHAT: Validate image/video frame input
 * WHY:  Detect adversarial images, pixel anomalies
 * HOW:  Structure check, range check, pattern analysis
 *
 * @param validator Validator handle
 * @param input Visual input data
 * @param result Output: validation result
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_input_validator_check_visual(
    lgss_input_validator_t* validator,
    const lgss_visual_input_t* input,
    lgss_validation_result_t* result
);

/**
 * @brief Validate audio input
 *
 * WHAT: Validate audio sample input
 * WHY:  Detect adversarial audio, ultrasonic attacks
 * HOW:  Range check, spectral analysis, anomaly detection
 *
 * @param validator Validator handle
 * @param input Audio input data
 * @param result Output: validation result
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_input_validator_check_audio(
    lgss_input_validator_t* validator,
    const lgss_audio_input_t* input,
    lgss_validation_result_t* result
);

/**
 * @brief Validate text input
 *
 * WHAT: Validate text/prompt input
 * WHY:  Detect prompt injection, jailbreak attempts
 * HOW:  Pattern matching, statistical analysis, content filtering
 *
 * @param validator Validator handle
 * @param input Text input data
 * @param result Output: validation result
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_input_validator_check_text(
    lgss_input_validator_t* validator,
    const lgss_text_input_t* input,
    lgss_validation_result_t* result
);

/**
 * @brief Validate proprioceptive input
 *
 * WHAT: Validate joint state input
 * WHY:  Detect sensor spoofing, false position signals
 * HOW:  Range check, temporal consistency, anomaly detection
 *
 * @param validator Validator handle
 * @param input Proprioceptive input data
 * @param result Output: validation result
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_input_validator_check_proprio(
    lgss_input_validator_t* validator,
    const lgss_proprio_input_t* input,
    lgss_validation_result_t* result
);

/**
 * @brief Validate tactile input
 *
 * WHAT: Validate tactile sensor input
 * WHY:  Detect phantom touch, sensor injection
 * HOW:  Range check, spatial consistency, anomaly detection
 *
 * @param validator Validator handle
 * @param input Tactile input data
 * @param result Output: validation result
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_input_validator_check_tactile(
    lgss_input_validator_t* validator,
    const lgss_tactile_input_t* input,
    lgss_validation_result_t* result
);

/*=============================================================================
 * STATISTICS API
 *============================================================================*/

/**
 * @brief Get validator statistics
 *
 * WHAT: Retrieve cumulative validation statistics
 * WHY:  Monitor validation effectiveness
 * HOW:  Copy current statistics to output structure
 *
 * @param validator Validator handle
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_input_validator_get_stats(
    const lgss_input_validator_t* validator,
    lgss_validator_stats_t* stats
);

/**
 * @brief Reset validator statistics
 *
 * WHAT: Reset all statistics counters to zero
 * WHY:  Start fresh measurement period
 * HOW:  Zero out statistics structure
 *
 * @param validator Validator handle
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_input_validator_reset_stats(
    lgss_input_validator_t* validator
);

/**
 * @brief Report false positive
 *
 * WHAT: Report that a detection was a false positive
 * WHY:  Enable precision tracking
 * HOW:  Increment false positive counter
 *
 * @param validator Validator handle
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_input_validator_report_false_positive(
    lgss_input_validator_t* validator
);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get validation status name
 *
 * WHAT: Get human-readable name for validation status
 * WHY:  Logging and debugging
 * HOW:  Lookup in static string table
 *
 * @param status Validation status code
 * @return Human-readable name string
 */
NIMCP_EXPORT const char* lgss_validation_status_name(
    input_validation_status_t status
);

/**
 * @brief Get modality name
 *
 * WHAT: Get human-readable name for input modality
 * WHY:  Logging and debugging
 * HOW:  Lookup in static string table
 *
 * @param modality Input modality code
 * @return Human-readable name string
 */
NIMCP_EXPORT const char* lgss_modality_name(
    input_modality_t modality
);

/**
 * @brief Get validation flag name
 *
 * WHAT: Get human-readable name for validation flag
 * WHY:  Logging and debugging
 * HOW:  Lookup based on flag bit
 *
 * @param flag Validation flag (single bit)
 * @return Human-readable name string
 */
NIMCP_EXPORT const char* lgss_validation_flag_name(
    lgss_validation_flags_t flag
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_INPUT_VALIDATOR_H */
