/**
 * @file nimcp_harm_classifier.h
 * @brief Action outcome harm classification for directive safety
 * @version 1.0.0
 * @date 2025-12-16
 *
 * WHAT: Classifier for determining if action outcomes are harmful or safe
 * WHY:  Essential for directive-following systems to prevent harmful actions while
 *       maintaining agency; provides quantitative harm assessment for decision-making
 * HOW:  Pattern matching on action descriptions, severity-weighted harm scoring,
 *       configurable thresholds for harm detection, multi-type harm taxonomy
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HARM AVOIDANCE SYSTEMS:
 * -----------------------
 * 1. Amygdala Threat Detection:
 *    - Rapid categorization of potential harms
 *    - Severity assessment (intensity coding)
 *    - Multi-modal threat categories (physical, social, resource)
 *    - Reference: LeDoux (2000) "Emotion circuits in the brain"
 *
 * 2. Prefrontal Harm Prediction:
 *    - Ventromedial PFC: Outcome valuation
 *    - Dorsolateral PFC: Consequence simulation
 *    - Probability estimation of harm occurrence
 *    - Reference: Bechara et al. (2000) "Emotion, decision making and the OFC"
 *
 * 3. Insular Cortex Risk Assessment:
 *    - Interoceptive signals of potential harm
 *    - Confidence estimation in harm prediction
 *    - Integration of bodily threat signals
 *    - Reference: Craig (2009) "How do you feel - now?"
 *
 * 4. Harm Taxonomies in Human Cognition:
 *    - Physical harm (injury, death)
 *    - Psychological harm (trauma, distress)
 *    - Social harm (reputation, relationships)
 *    - Resource harm (property, financial)
 *    - Reference: Gray et al. (2014) "The harm made by comparisons"
 *
 * COMPUTATIONAL MODEL:
 * --------------------
 * This module implements a multi-dimensional harm classifier that mirrors the brain's
 * threat evaluation system:
 *
 * 1. Harm Type Taxonomy (analogous to amygdala categorization):
 *    - 8 harm categories with biological grounding
 *    - Severity weights calibrated to human moral intuitions
 *
 * 2. Probabilistic Assessment (analogous to PFC prediction):
 *    - Probability of harm occurrence [0-1]
 *    - Severity of harm if it occurs [0-1]
 *    - Total harm score = Σ P(harm_i) × severity_i
 *
 * 3. Threshold-Based Classification (analogous to action inhibition):
 *    - Configurable harm threshold (default 0.1)
 *    - Strict mode: Any harm > 0 triggers blocking
 *    - Confidence estimation for classification
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                         HARM CLASSIFIER                                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    INPUT: ACTION DESCRIPTION                        │  ║
 * ║   │   "Delete file /home/user/important_data.txt"                       │  ║
 * ║   └──────────────────────────┬─────────────────────────────────────────┘  ║
 * ║                              │                                            ║
 * ║   ┌──────────────────────────▼─────────────────────────────────────────┐  ║
 * ║   │              PATTERN MATCHING & KEYWORD DETECTION                   │  ║
 * ║   │  - "delete" → PROPERTY_DAMAGE (probability=0.8)                     │  ║
 * ║   │  - "file" → PROPERTY_DAMAGE (severity=0.4)                          │  ║
 * ║   │  - "kill", "terminate" → PHYSICAL_INJURY or DEATH                  │  ║
 * ║   │  - "breach", "hack" → SECURITY_BREACH                               │  ║
 * ║   └──────────────────────────┬─────────────────────────────────────────┘  ║
 * ║                              │                                            ║
 * ║   ┌──────────────────────────▼─────────────────────────────────────────┐  ║
 * ║   │                    HARM ASSESSMENT (per type)                       │  ║
 * ║   │                                                                     │  ║
 * ║   │   harm_type_t type;           // e.g., PROPERTY_DAMAGE             │  ║
 * ║   │   float severity;              // 0.4 (file deletion)               │  ║
 * ║   │   float probability;           // 0.8 (likely to occur)             │  ║
 * ║   │   char description[256];       // "File deletion"                   │  ║
 * ║   └──────────────────────────┬─────────────────────────────────────────┘  ║
 * ║                              │                                            ║
 * ║   ┌──────────────────────────▼─────────────────────────────────────────┐  ║
 * ║   │              SEVERITY-WEIGHTED HARM SCORING                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   total_harm_score = Σ (P(harm_i) × severity_i × weight_i)         │  ║
 * ║   │                    = 0.8 × 0.4 × 1.0                                │  ║
 * ║   │                    = 0.32                                           │  ║
 * ║   └──────────────────────────┬─────────────────────────────────────────┘  ║
 * ║                              │                                            ║
 * ║   ┌──────────────────────────▼─────────────────────────────────────────┐  ║
 * ║   │                 CLASSIFICATION DECISION                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   is_harmful = (total_harm_score > threshold)                       │  ║
 * ║   │              = (0.32 > 0.1) = TRUE                                  │  ║
 * ║   │                                                                     │  ║
 * ║   │   confidence = min(1.0, total_harm_score / max_severity)            │  ║
 * ║   │              = min(1.0, 0.32 / 1.0) = 0.32                          │  ║
 * ║   └──────────────────────────┬─────────────────────────────────────────┘  ║
 * ║                              │                                            ║
 * ║   ┌──────────────────────────▼─────────────────────────────────────────┐  ║
 * ║   │                  OUTPUT: CLASSIFICATION RESULT                      │  ║
 * ║   │                                                                     │  ║
 * ║   │   total_harm_score: 0.32                                            │  ║
 * ║   │   harm_count: 1                                                     │  ║
 * ║   │   is_harmful: true                                                  │  ║
 * ║   │   confidence: 0.32                                                  │  ║
 * ║   │   harms[PROPERTY_DAMAGE]: severity=0.4, prob=0.8                    │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * HARM TYPE SEVERITY CALIBRATION:
 * --------------------------------
 * Default severity weights calibrated to human moral intuitions:
 * - DEATH: 1.0 (maximum harm)
 * - PHYSICAL_INJURY: 0.7 (severe but recoverable)
 * - PSYCHOLOGICAL: 0.6 (long-lasting impact)
 * - SECURITY_BREACH: 0.5 (potential for cascading harms)
 * - FINANCIAL: 0.4 (recoverable with time)
 * - PROPERTY_DAMAGE: 0.3 (replaceable resources)
 * - PRIVACY_VIOLATION: 0.5 (dignity and autonomy)
 * - ENVIRONMENTAL: 0.6 (long-term ecosystem impact)
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

#ifndef NIMCP_HARM_CLASSIFIER_H
#define NIMCP_HARM_CLASSIFIER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/platform/nimcp_platform_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum length of action description for classification */
#define HARM_CLASSIFIER_MAX_ACTION_DESC     1024

/** @brief Maximum length of harm description */
#define HARM_CLASSIFIER_MAX_HARM_DESC       256

/** @brief Default harm threshold (actions above this are blocked) */
#define HARM_CLASSIFIER_DEFAULT_THRESHOLD   0.1f

/** @brief Number of pattern keywords for each harm type */
#define HARM_CLASSIFIER_MAX_KEYWORDS        16

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Taxonomy of harm types
 *
 * WHAT: Categories of potential harms from actions
 * WHY:  Different harms have different severities and require different responses
 * HOW:  Modeled on human moral psychology and risk assessment
 */
typedef enum {
    HARM_TYPE_PHYSICAL_INJURY = 0,  /**< Bodily injury (severity: 0.7) */
    HARM_TYPE_DEATH,                 /**< Loss of life (severity: 1.0) */
    HARM_TYPE_PSYCHOLOGICAL,         /**< Mental distress, trauma (severity: 0.6) */
    HARM_TYPE_PROPERTY_DAMAGE,       /**< Damage to property/resources (severity: 0.3) */
    HARM_TYPE_PRIVACY_VIOLATION,     /**< Breach of privacy/dignity (severity: 0.5) */
    HARM_TYPE_FINANCIAL,             /**< Financial loss (severity: 0.4) */
    HARM_TYPE_ENVIRONMENTAL,         /**< Environmental damage (severity: 0.6) */
    HARM_TYPE_SECURITY_BREACH,       /**< Security compromise (severity: 0.5) */
    HARM_TYPE_COUNT                  /**< Number of harm types */
} harm_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Assessment of a specific harm type
 *
 * WHAT: Quantitative evaluation of one type of potential harm
 * WHY:  Multi-dimensional harm representation for nuanced decision-making
 * HOW:  Probability × severity = expected harm contribution
 */
typedef struct {
    harm_type_t type;               /**< Type of harm */
    float severity;                 /**< Severity if occurs [0-1] (1.0 = death) */
    float probability;              /**< Probability of occurrence [0-1] */
    char description[HARM_CLASSIFIER_MAX_HARM_DESC]; /**< Human-readable description */
} harm_assessment_t;

/**
 * @brief Complete harm classification result
 *
 * WHAT: Full output of harm classification process
 * WHY:  Provides both binary decision (is_harmful) and detailed breakdown
 * HOW:  Aggregates all harm assessments into weighted score
 */
typedef struct {
    float total_harm_score;         /**< Weighted sum of P(harm) × severity */
    uint32_t harm_count;            /**< Number of harms detected */
    harm_assessment_t harms[HARM_TYPE_COUNT]; /**< Per-type assessments */
    bool is_harmful;                /**< total_harm_score > threshold */
    float confidence;               /**< Classifier confidence [0-1] */
} harm_classification_t;

/**
 * @brief Harm classifier configuration
 *
 * WHAT: Tunable parameters for harm classification
 * WHY:  Different use cases require different safety thresholds
 * HOW:  Weights and thresholds calibrated to application needs
 */
typedef struct {
    float harm_threshold;           /**< Classification threshold (default 0.1) */
    float severity_weights[HARM_TYPE_COUNT]; /**< Per-type severity multipliers */
    bool strict_mode;               /**< If true, any harm > 0 is blocked */
    bool enable_pattern_matching;   /**< Enable keyword pattern detection */
    bool enable_context_analysis;   /**< Enable contextual harm assessment */
} harm_classifier_config_t;

/**
 * @brief Harm classifier statistics
 *
 * WHAT: Operational metrics for harm classification
 * WHY:  Monitor classifier performance and calibration
 * HOW:  Track classifications, decisions, and performance
 */
typedef struct {
    uint64_t total_classifications; /**< Total actions classified */
    uint64_t harmful_detected;      /**< Actions classified as harmful */
    uint64_t safe_detected;         /**< Actions classified as safe */
    float avg_harm_score;           /**< Average harm score across all */
    float max_harm_score;           /**< Maximum harm score seen */
    uint64_t pattern_matches;       /**< Keyword pattern hits */
    uint64_t context_matches;       /**< Context-based detections */
} harm_classifier_stats_t;

/**
 * @brief Harm classifier state
 *
 * WHAT: Main classifier object
 * WHY:  Encapsulates all state for harm classification
 * HOW:  Thread-safe singleton with configuration and stats
 */
typedef struct {
    /* Configuration */
    harm_classifier_config_t config;

    /* Statistics */
    harm_classifier_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Whether bio-async is active */

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;  /**< Mutex for thread-safe access */
} harm_classifier_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with human-calibrated defaults
 * HOW:  Return struct with research-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int harm_classifier_default_config(harm_classifier_config_t* config);

/**
 * @brief Create harm classifier
 *
 * WHAT: Initialize harm classification system
 * WHY:  Enable harm detection for directive safety
 * HOW:  Allocate structure, initialize mutex, load config
 *
 * @param config Configuration (NULL for defaults)
 * @return New classifier or NULL on failure
 */
harm_classifier_t* harm_classifier_create(const harm_classifier_config_t* config);

/**
 * @brief Destroy harm classifier
 *
 * WHAT: Clean up classifier resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure, destroy mutex
 *
 * @param classifier Classifier to destroy
 */
void harm_classifier_destroy(harm_classifier_t* classifier);

/* ============================================================================
 * Classification API
 * ============================================================================ */

/**
 * @brief Classify action for harm
 *
 * WHAT: Determine if action description indicates harmful outcome
 * WHY:  Primary classification interface for directive system
 * HOW:  Pattern match keywords, compute severity-weighted score, threshold
 *
 * @param classifier Harm classifier
 * @param action_description Natural language action description
 * @param action_data Optional structured action data (can be NULL)
 * @param data_len Length of action_data
 * @param result Output classification result
 * @return 0 on success, -1 on error
 */
int harm_classifier_classify(
    harm_classifier_t* classifier,
    const char* action_description,
    const void* action_data,
    size_t data_len,
    harm_classification_t* result
);

/**
 * @brief Classify action with additional context
 *
 * WHAT: Enhanced classification with contextual information
 * WHY:  Context improves accuracy (e.g., "delete temp file" vs "delete user data")
 * HOW:  Combine action and context for more nuanced assessment
 *
 * @param classifier Harm classifier
 * @param action_description Action to classify
 * @param context_description Contextual information (can be NULL)
 * @param result Output classification result
 * @return 0 on success, -1 on error
 */
int harm_classifier_classify_with_context(
    harm_classifier_t* classifier,
    const char* action_description,
    const char* context_description,
    harm_classification_t* result
);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Set severity weight for harm type
 *
 * WHAT: Adjust relative importance of harm type
 * WHY:  Calibrate classifier to specific use case (e.g., medical vs financial)
 * HOW:  Update weight multiplier for harm type
 *
 * @param classifier Harm classifier
 * @param harm_type Type to adjust
 * @param weight New weight [0-2.0] (1.0 = default)
 * @return 0 on success, -1 on error
 */
int harm_classifier_set_severity_weight(
    harm_classifier_t* classifier,
    harm_type_t harm_type,
    float weight
);

/**
 * @brief Get severity weight for harm type
 *
 * WHAT: Query current weight for harm type
 * WHY:  Inspect current calibration
 * HOW:  Return weight multiplier
 *
 * @param classifier Harm classifier
 * @param harm_type Type to query
 * @return Weight [0-2.0] or -1.0 on error
 */
float harm_classifier_get_severity_weight(
    const harm_classifier_t* classifier,
    harm_type_t harm_type
);

/**
 * @brief Set harm threshold
 *
 * WHAT: Adjust classification threshold
 * WHY:  Control sensitivity (lower = more sensitive)
 * HOW:  Update threshold value
 *
 * @param classifier Harm classifier
 * @param threshold New threshold [0-1.0]
 * @return 0 on success, -1 on error
 */
int harm_classifier_set_threshold(
    harm_classifier_t* classifier,
    float threshold
);

/**
 * @brief Get current harm threshold
 *
 * WHAT: Query classification threshold
 * WHY:  Inspect current sensitivity setting
 * HOW:  Return threshold value
 *
 * @param classifier Harm classifier
 * @return Threshold [0-1.0] or -1.0 on error
 */
float harm_classifier_get_threshold(const harm_classifier_t* classifier);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get classifier statistics
 *
 * WHAT: Retrieve operational metrics
 * WHY:  Monitor classifier performance and calibration
 * HOW:  Copy stats structure
 *
 * @param classifier Harm classifier
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int harm_classifier_get_stats(
    const harm_classifier_t* classifier,
    harm_classifier_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Start fresh measurement period
 * HOW:  Zero all stat fields
 *
 * @param classifier Harm classifier
 * @return 0 on success, -1 on error
 */
int harm_classifier_reset_stats(harm_classifier_t* classifier);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect classifier to bio-async router
 *
 * WHAT: Register classifier as bio-async module
 * WHY:  Enable inter-module messaging for distributed harm assessment
 * HOW:  Register with bio_router using BIO_MODULE_HARM_CLASSIFIER
 *
 * @param classifier Harm classifier
 * @return 0 on success, -1 on error
 */
int harm_classifier_connect_bio_async(harm_classifier_t* classifier);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister classifier from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param classifier Harm classifier
 * @return 0 on success, -1 on error
 */
int harm_classifier_disconnect_bio_async(harm_classifier_t* classifier);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Verify messaging capability
 * HOW:  Return bio_async_enabled flag
 *
 * @param classifier Harm classifier
 * @return true if connected, false otherwise
 */
bool harm_classifier_is_bio_async_connected(const harm_classifier_t* classifier);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get harm type name
 *
 * WHAT: Convert harm type enum to string
 * WHY:  Human-readable logging and debugging
 * HOW:  Static string lookup
 *
 * @param type Harm type
 * @return String name or "UNKNOWN"
 */
const char* harm_classifier_get_type_name(harm_type_t type);

/**
 * @brief Get default severity for harm type
 *
 * WHAT: Query default severity calibration
 * WHY:  Understand baseline harm weights
 * HOW:  Return default severity value
 *
 * @param type Harm type
 * @return Default severity [0-1.0] or -1.0 on error
 */
float harm_classifier_get_default_severity(harm_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HARM_CLASSIFIER_H */
