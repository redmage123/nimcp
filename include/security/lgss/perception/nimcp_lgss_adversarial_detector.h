/**
 * @file nimcp_lgss_adversarial_detector.h
 * @brief LGSS Component A10: Perception Safety - Adversarial Detection Layer
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: ML-based detection of adversarial perturbations in perceptual inputs
 * WHY:  Adversarial attacks exploit ML model vulnerabilities through carefully
 *       crafted perturbations that are imperceptible to humans but cause
 *       misclassification in neural networks
 * HOW:  Combine multiple detection strategies: statistical analysis, input
 *       transformation defense, feature squeezing, and ML-based classifiers
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SENSORY ROBUSTNESS:
 * -------------------
 * Biological sensory systems are remarkably robust to perturbations:
 * - Visual cortex: Invariance to small translations, rotations, lighting changes
 * - Auditory cortex: Noise robustness, cocktail party effect
 * - Olfactory system: Concentration invariance
 *
 * Adversarial attacks exploit the gap between biological and artificial
 * robustness. This module implements defenses inspired by biological principles:
 * - Feature Squeezing: Reduce input precision (like retinal sampling)
 * - Input Transformation: Random transforms (like saccades, head movement)
 * - Ensemble Detection: Multiple detectors (like redundant neural pathways)
 *
 * ADVERSARIAL ATTACK TAXONOMY:
 * ----------------------------
 * - White-box: Attacker has full model access (FGSM, PGD, C&W)
 * - Black-box: Attacker only has input/output access (transfer, query-based)
 * - Physical: Real-world perturbations (adversarial patches, 3D prints)
 *
 * DETECTION METHODS:
 * ------------------
 * 1. Statistical Analysis: Detect unusual input distributions
 * 2. Feature Squeezing: Compare outputs before/after compression
 * 3. Input Transformation: Check consistency under random transforms
 * 4. ML Classifier: Trained adversarial detector model (placeholder)
 * 5. Gradient Analysis: Check gradient magnitude patterns
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |                    ADVERSARIAL DETECTOR                                  |
 * +=========================================================================+
 * |                                                                          |
 * |   +----------------+                                                     |
 * |   |  Raw Input     |                                                     |
 * |   +-------+--------+                                                     |
 * |           |                                                              |
 * |           v                                                              |
 * |   +-------+--------+     +------------------+     +------------------+   |
 * |   | Feature        |---->| Transformation   |---->| Ensemble         |   |
 * |   | Squeezing      |     | Defense          |     | Classifier       |   |
 * |   +-------+--------+     +--------+---------+     +--------+---------+   |
 * |           |                       |                        |             |
 * |           v                       v                        v             |
 * |   +-------+--------+     +--------+---------+     +--------+---------+   |
 * |   | Statistical    |     | Consistency      |     | ML Detection     |   |
 * |   | Analysis       |     | Check            |     | Score            |   |
 * |   +-------+--------+     +--------+---------+     +--------+---------+   |
 * |           |                       |                        |             |
 * |           +----------+------------+------------------------+             |
 * |                      |                                                   |
 * |                      v                                                   |
 * |           +----------+------------+                                      |
 * |           | Aggregated Detection  |                                      |
 * |           | Result + Confidence   |                                      |
 * |           +-----------------------+                                      |
 * |                                                                          |
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

#ifndef NIMCP_LGSS_ADVERSARIAL_DETECTOR_H
#define NIMCP_LGSS_ADVERSARIAL_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Common utilities */
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"

/* Input validator types */
#include "security/lgss/perception/nimcp_lgss_input_validator.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

/** Magic number for adversarial detector */
#define LGSS_ADVERSARIAL_DETECTOR_MAGIC 0x4C475341  /* "LGSA" */

/** Default thresholds */
#define LGSS_ADV_DEFAULT_THRESHOLD          0.7f
#define LGSS_ADV_SQUEEZING_THRESHOLD        0.15f
#define LGSS_ADV_TRANSFORM_THRESHOLD        0.1f
#define LGSS_ADV_STATISTICAL_THRESHOLD      3.0f  /* Standard deviations */

/** Maximum transforms for consistency check */
#define LGSS_ADV_MAX_TRANSFORMS             8

/*=============================================================================
 * ENUMERATIONS
 *============================================================================*/

/**
 * @brief Detection method flags (bitmask)
 *
 * WHAT: Flags indicating which detection methods to use
 * WHY:  Different methods have different accuracy/performance tradeoffs
 */
typedef enum {
    LGSS_ADV_METHOD_STATISTICAL     = (1 << 0), /**< Statistical anomaly detection */
    LGSS_ADV_METHOD_SQUEEZING       = (1 << 1), /**< Feature squeezing defense */
    LGSS_ADV_METHOD_TRANSFORM       = (1 << 2), /**< Input transformation defense */
    LGSS_ADV_METHOD_ML_CLASSIFIER   = (1 << 3), /**< ML-based adversarial classifier */
    LGSS_ADV_METHOD_GRADIENT        = (1 << 4), /**< Gradient-based analysis */
    LGSS_ADV_METHOD_ALL             = 0x1F      /**< All methods */
} lgss_adv_method_flags_t;

/**
 * @brief Adversarial handling flags (bitmask)
 *
 * WHAT: Flags indicating how to handle detected adversarial inputs
 * WHY:  Different scenarios require different responses
 */
typedef enum {
    LGSS_ADV_HANDLE_LOG             = (1 << 0), /**< Log detection */
    LGSS_ADV_HANDLE_ALERT           = (1 << 1), /**< Send alert via bio-async */
    LGSS_ADV_HANDLE_REJECT          = (1 << 2), /**< Reject input entirely */
    LGSS_ADV_HANDLE_SANITIZE        = (1 << 3), /**< Attempt to sanitize input */
    LGSS_ADV_HANDLE_QUARANTINE      = (1 << 4), /**< Store for later analysis */
    LGSS_ADV_HANDLE_DEFAULT         = 0x07      /**< Log + Alert + Reject */
} lgss_adv_handling_flags_t;

/**
 * @brief Detected attack type
 *
 * WHAT: Classification of detected adversarial attack
 * WHY:  Different attack types may require different responses
 */
typedef enum {
    LGSS_ADV_ATTACK_NONE = 0,          /**< No attack detected */
    LGSS_ADV_ATTACK_PERTURBATION,      /**< Small perturbation attack (FGSM, PGD) */
    LGSS_ADV_ATTACK_PATCH,             /**< Adversarial patch attack */
    LGSS_ADV_ATTACK_NOISE,             /**< Adversarial noise injection */
    LGSS_ADV_ATTACK_UNIVERSAL,         /**< Universal adversarial perturbation */
    LGSS_ADV_ATTACK_PHYSICAL,          /**< Physical-world adversarial object */
    LGSS_ADV_ATTACK_UNKNOWN            /**< Unknown attack type */
} lgss_adv_attack_type_t;

/*=============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

typedef struct lgss_adversarial_detector lgss_adversarial_detector_t;

/*=============================================================================
 * RESULT STRUCTURES
 *============================================================================*/

/**
 * @brief Adversarial detection result
 *
 * WHAT: Detailed result of adversarial detection
 * WHY:  Provide actionable information about detection
 * HOW:  Attack type, confidence, method that triggered, explanation
 */
typedef struct {
    /* Input reference */
    const void* input_data;            /**< Pointer to input data */
    input_modality_t modality;         /**< Input modality */
    input_validation_status_t status;  /**< Overall validation status */

    /* Detection results */
    bool is_adversarial;               /**< Whether adversarial input detected */
    lgss_adv_attack_type_t attack_type;/**< Detected attack type */
    float confidence;                  /**< Detection confidence [0-1] */

    /* Per-method scores */
    float statistical_score;           /**< Statistical analysis score [0-1] */
    float squeezing_score;             /**< Feature squeezing score [0-1] */
    float transform_score;             /**< Transform consistency score [0-1] */
    float ml_classifier_score;         /**< ML classifier score [0-1] */
    float gradient_score;              /**< Gradient analysis score [0-1] */

    /* Method that triggered detection */
    uint32_t triggered_methods;        /**< Bitmask of methods that detected */
    const char* primary_method;        /**< Name of primary detection method */

    /* Explanation */
    char explanation[256];             /**< Human-readable explanation */

    /* Metadata */
    uint64_t timestamp_us;             /**< Detection timestamp */
    uint32_t detection_time_us;        /**< Time taken for detection */
} adversarial_detection_result_t;

/*=============================================================================
 * CONFIGURATION STRUCTURE
 *============================================================================*/

/**
 * @brief Adversarial detector configuration
 *
 * WHAT: Configuration for adversarial detection behavior
 * WHY:  Allow tuning of detection sensitivity and methods
 * HOW:  Set flags and thresholds for each detection component
 */
typedef struct {
    /* Detection method flags */
    uint32_t detection_methods;        /**< Bitmask of lgss_adv_method_flags_t */
    uint32_t handling_flags;           /**< Bitmask of lgss_adv_handling_flags_t */

    /* Overall threshold */
    float detection_threshold;         /**< Overall detection threshold [0-1] */

    /* Per-method thresholds */
    float statistical_threshold;       /**< Statistical detection threshold */
    float squeezing_threshold;         /**< Feature squeezing threshold */
    float transform_threshold;         /**< Transform consistency threshold */
    float ml_threshold;                /**< ML classifier threshold */
    float gradient_threshold;          /**< Gradient analysis threshold */

    /* Feature squeezing parameters */
    uint8_t squeezing_bit_depth;       /**< Bit depth for color squeezing (1-8) */
    uint32_t squeezing_filter_size;    /**< Filter size for spatial squeezing */

    /* Transform defense parameters */
    uint32_t num_transforms;           /**< Number of random transforms */
    float max_rotation_deg;            /**< Max rotation for transforms */
    float max_translation_pct;         /**< Max translation for transforms */

    /* ML model configuration */
    const char* model_path;            /**< Path to adversarial detector model */
    bool enable_online_learning;       /**< Enable online model updates */

    /* Performance tuning */
    bool enable_fast_mode;             /**< Skip expensive methods on early detect */
    uint32_t max_detection_time_ms;    /**< Maximum time for detection */

    /* Integration */
    bool enable_logging;               /**< Enable detailed logging */
    bool enable_bio_async;             /**< Enable bio-async notifications */
} lgss_adversarial_config_t;

/*=============================================================================
 * STATISTICS STRUCTURE
 *============================================================================*/

/**
 * @brief Adversarial detector statistics
 *
 * WHAT: Cumulative statistics for detector operation
 * WHY:  Monitor detection effectiveness and performance
 */
typedef struct {
    /* Detection counts */
    uint64_t total_checks;             /**< Total inputs checked */
    uint64_t adversarial_detected;     /**< Adversarial inputs detected */
    uint64_t clean_inputs;             /**< Clean inputs passed */

    /* Per-attack-type counts */
    uint64_t perturbation_attacks;     /**< Perturbation attacks detected */
    uint64_t patch_attacks;            /**< Patch attacks detected */
    uint64_t noise_attacks;            /**< Noise attacks detected */
    uint64_t universal_attacks;        /**< Universal perturbation detected */
    uint64_t physical_attacks;         /**< Physical attacks detected */
    uint64_t unknown_attacks;          /**< Unknown attacks detected */

    /* Per-method detection counts */
    uint64_t statistical_triggers;     /**< Statistical method triggers */
    uint64_t squeezing_triggers;       /**< Squeezing method triggers */
    uint64_t transform_triggers;       /**< Transform method triggers */
    uint64_t ml_triggers;              /**< ML classifier triggers */
    uint64_t gradient_triggers;        /**< Gradient method triggers */

    /* Performance metrics */
    float avg_detection_time_us;       /**< Average detection time */
    float max_detection_time_us;       /**< Maximum detection time */

    /* Accuracy tracking */
    uint64_t false_positives;          /**< Reported false positives */
    uint64_t false_negatives;          /**< Reported false negatives */
    float estimated_precision;         /**< Estimated precision */
    float estimated_recall;            /**< Estimated recall */
} lgss_adversarial_stats_t;

/*=============================================================================
 * CONFIGURATION API
 *============================================================================*/

/**
 * @brief Get default adversarial detector configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with balanced security/performance
 * HOW:  Return pre-configured structure
 *
 * @param config Output configuration structure
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
NIMCP_EXPORT nimcp_error_t lgss_adversarial_detector_default_config(
    lgss_adversarial_config_t* config
);

/*=============================================================================
 * LIFECYCLE API
 *============================================================================*/

/**
 * @brief Create adversarial detector
 *
 * WHAT: Allocate and initialize adversarial detector instance
 * WHY:  Enable adversarial detection for perceptual inputs
 * HOW:  Allocate structure, initialize methods, apply configuration
 *
 * @param validator Input validator to connect (required)
 * @param config Configuration (NULL for defaults)
 * @return Detector handle or NULL on failure
 */
NIMCP_EXPORT lgss_adversarial_detector_t* lgss_adversarial_detector_create(
    lgss_input_validator_t* validator,
    const lgss_adversarial_config_t* config
);

/**
 * @brief Destroy adversarial detector
 *
 * WHAT: Clean up and free detector resources
 * WHY:  Proper resource deallocation
 * HOW:  Free internal structures, unload models, zero memory
 *
 * @param detector Detector handle (NULL safe)
 */
NIMCP_EXPORT void lgss_adversarial_detector_destroy(
    lgss_adversarial_detector_t* detector
);

/*=============================================================================
 * DETECTION API
 *============================================================================*/

/**
 * @brief Check if input is adversarial
 *
 * WHAT: Primary adversarial detection function
 * WHY:  Detect adversarial perturbations before they affect processing
 * HOW:  Run enabled detection methods, aggregate results
 *
 * @param detector Detector handle
 * @param input Input data
 * @param modality Input modality
 * @param input_size Size of input data
 * @param result Output: detection result
 * @return NIMCP_SUCCESS on success (check result for detection)
 */
NIMCP_EXPORT nimcp_error_t lgss_adversarial_detector_is_adversarial(
    lgss_adversarial_detector_t* detector,
    const void* input,
    input_modality_t modality,
    size_t input_size,
    adversarial_detection_result_t* result
);

/**
 * @brief Check visual input for adversarial perturbations
 *
 * WHAT: Specialized adversarial detection for visual inputs
 * WHY:  Visual inputs have specific adversarial vulnerabilities
 * HOW:  Visual-specific squeezing, transform, and statistical checks
 *
 * @param detector Detector handle
 * @param input Visual input data
 * @param result Output: detection result
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_adversarial_detector_check_visual(
    lgss_adversarial_detector_t* detector,
    const lgss_visual_input_t* input,
    adversarial_detection_result_t* result
);

/**
 * @brief Check audio input for adversarial perturbations
 *
 * WHAT: Specialized adversarial detection for audio inputs
 * WHY:  Audio inputs have specific adversarial vulnerabilities
 * HOW:  Audio-specific spectral analysis, compression checks
 *
 * @param detector Detector handle
 * @param input Audio input data
 * @param result Output: detection result
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_adversarial_detector_check_audio(
    lgss_adversarial_detector_t* detector,
    const lgss_audio_input_t* input,
    adversarial_detection_result_t* result
);

/**
 * @brief Get detection confidence
 *
 * WHAT: Get confidence score for adversarial detection
 * WHY:  Allow threshold-based decisions external to detector
 * HOW:  Return aggregated confidence from enabled methods
 *
 * @param detector Detector handle
 * @param result Detection result to query
 * @return Confidence score [0-1], or -1.0 on error
 */
NIMCP_EXPORT float lgss_adversarial_detector_get_confidence(
    const lgss_adversarial_detector_t* detector,
    const adversarial_detection_result_t* result
);

/*=============================================================================
 * STATISTICS API
 *============================================================================*/

/**
 * @brief Get detector statistics
 *
 * WHAT: Retrieve cumulative detection statistics
 * WHY:  Monitor detection effectiveness
 * HOW:  Copy current statistics to output structure
 *
 * @param detector Detector handle
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_adversarial_detector_get_stats(
    const lgss_adversarial_detector_t* detector,
    lgss_adversarial_stats_t* stats
);

/**
 * @brief Reset detector statistics
 *
 * WHAT: Reset all statistics counters to zero
 * WHY:  Start fresh measurement period
 * HOW:  Zero out statistics structure
 *
 * @param detector Detector handle
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_adversarial_detector_reset_stats(
    lgss_adversarial_detector_t* detector
);

/**
 * @brief Report false positive
 *
 * WHAT: Report that a detection was a false positive
 * WHY:  Enable precision tracking and model improvement
 * HOW:  Increment false positive counter, optionally update model
 *
 * @param detector Detector handle
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_adversarial_detector_report_false_positive(
    lgss_adversarial_detector_t* detector
);

/**
 * @brief Report false negative
 *
 * WHAT: Report that an adversarial input was missed
 * WHY:  Enable recall tracking and model improvement
 * HOW:  Increment false negative counter, optionally update model
 *
 * @param detector Detector handle
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_adversarial_detector_report_false_negative(
    lgss_adversarial_detector_t* detector
);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get attack type name
 *
 * WHAT: Get human-readable name for attack type
 * WHY:  Logging and debugging
 * HOW:  Lookup in static string table
 *
 * @param attack_type Attack type code
 * @return Human-readable name string
 */
NIMCP_EXPORT const char* lgss_adv_attack_type_name(
    lgss_adv_attack_type_t attack_type
);

/**
 * @brief Get detection method name
 *
 * WHAT: Get human-readable name for detection method
 * WHY:  Logging and debugging
 * HOW:  Lookup based on flag bit
 *
 * @param method Detection method flag (single bit)
 * @return Human-readable name string
 */
NIMCP_EXPORT const char* lgss_adv_method_name(
    lgss_adv_method_flags_t method
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_ADVERSARIAL_DETECTOR_H */
