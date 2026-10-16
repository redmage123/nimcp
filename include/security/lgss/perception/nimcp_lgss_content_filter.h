/**
 * @file nimcp_lgss_content_filter.h
 * @brief LGSS Component A10: Perception Safety - Content Filtering Layer
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Content filtering for text inputs to detect prompt injection, jailbreaks,
 *       and manipulation attempts targeting the AI system
 * WHY:  Text inputs (prompts, instructions) are a primary attack vector for
 *       manipulating AI systems through social engineering and injection attacks
 * HOW:  Multi-layer filtering: pattern matching, statistical analysis, semantic
 *       classification, and heuristic rules
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE MEMORY AND PATTERN RECOGNITION:
 * --------------------------------------
 * The biological immune system maintains memory of past pathogens:
 * - T cells: Remember specific threats, rapid response on re-exposure
 * - Antibodies: Lock onto known pathogen signatures
 * - Innate immunity: Pattern recognition receptors detect common threat features
 *
 * This module implements analogous defenses for text content:
 * - Pattern Database: Known jailbreak patterns (like antibodies)
 * - Statistical Analysis: Anomalous text features (like innate immunity)
 * - Semantic Analysis: Understanding intent (like T cell recognition)
 * - Adaptive Learning: Update patterns from new threats
 *
 * ATTACK TAXONOMY:
 * ----------------
 * 1. Direct Prompt Injection:
 *    - "Ignore previous instructions and..."
 *    - Role-play attacks ("Pretend you are DAN...")
 *    - System prompt extraction attempts
 *
 * 2. Indirect Prompt Injection:
 *    - Malicious content in retrieved documents
 *    - Encoded instructions in data
 *    - Hidden text in formatting
 *
 * 3. Jailbreak Attempts:
 *    - Hypothetical scenarios
 *    - Character role-play
 *    - Multi-turn manipulation
 *    - Encoded/obfuscated requests
 *
 * 4. Manipulation Attempts:
 *    - Emotional manipulation
 *    - Authority claims
 *    - Urgency/scarcity pressure
 *    - Social engineering
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |                       CONTENT FILTER                                     |
 * +=========================================================================+
 * |                                                                          |
 * |   +----------------+                                                     |
 * |   |  Text Input    |                                                     |
 * |   +-------+--------+                                                     |
 * |           |                                                              |
 * |           v                                                              |
 * |   +-------+--------+                                                     |
 * |   | Preprocessing  |  (normalization, encoding detection)                |
 * |   +-------+--------+                                                     |
 * |           |                                                              |
 * |           +------------------+------------------+                        |
 * |           |                  |                  |                        |
 * |           v                  v                  v                        |
 * |   +-------+--------+ +-------+--------+ +-------+--------+              |
 * |   | Pattern        | | Statistical    | | Semantic       |              |
 * |   | Matching       | | Analysis       | | Classification |              |
 * |   +-------+--------+ +-------+--------+ +-------+--------+              |
 * |           |                  |                  |                        |
 * |           +-------+----------+------------------+                        |
 * |                   |                                                      |
 * |                   v                                                      |
 * |   +---------------+-----------------+                                    |
 * |   |     FILTER DECISION             |                                    |
 * |   | (SAFE / JAILBREAK / INJECTION / |                                    |
 * |   |  MANIPULATION / SUSPICIOUS)     |                                    |
 * |   +---------------------------------+                                    |
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

#ifndef NIMCP_LGSS_CONTENT_FILTER_H
#define NIMCP_LGSS_CONTENT_FILTER_H

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

/** Magic number for content filter */
#define LGSS_CONTENT_FILTER_MAGIC 0x4C475346  /* "LGSF" */

/** Maximum pattern length */
#define LGSS_MAX_PATTERN_LENGTH     1024

/** Maximum number of custom patterns */
#define LGSS_MAX_CUSTOM_PATTERNS    256

/** Default thresholds */
#define LGSS_FILTER_DEFAULT_THRESHOLD       0.6f
#define LGSS_FILTER_JAILBREAK_THRESHOLD     0.5f
#define LGSS_FILTER_INJECTION_THRESHOLD     0.5f
#define LGSS_FILTER_MANIPULATION_THRESHOLD  0.7f

/*=============================================================================
 * ENUMERATIONS
 *============================================================================*/

/**
 * @brief Content filter result status
 *
 * WHAT: Outcome of content filtering
 * WHY:  Distinguish between different types of threats
 */
typedef enum {
    LGSS_CONTENT_SAFE = 0,             /**< Content is safe */
    LGSS_CONTENT_JAILBREAK,            /**< Jailbreak attempt detected */
    LGSS_CONTENT_INJECTION,            /**< Prompt injection detected */
    LGSS_CONTENT_MANIPULATION,         /**< Manipulation attempt detected */
    LGSS_CONTENT_SUSPICIOUS,           /**< Suspicious but not confirmed */
    LGSS_CONTENT_MALFORMED             /**< Malformed/invalid content */
} lgss_content_status_t;

/**
 * @brief Filter category flags (bitmask)
 *
 * WHAT: Categories of filtering to apply
 * WHY:  Allow selective filtering for different use cases
 */
typedef enum {
    LGSS_FILTER_CAT_INSTRUCTION     = (1 << 0), /**< Instruction override patterns */
    LGSS_FILTER_CAT_ROLEPLAY        = (1 << 1), /**< Role-play jailbreak patterns */
    LGSS_FILTER_CAT_ENCODING        = (1 << 2), /**< Encoded/obfuscated content */
    LGSS_FILTER_CAT_EXTRACTION      = (1 << 3), /**< System prompt extraction */
    LGSS_FILTER_CAT_MANIPULATION    = (1 << 4), /**< Social manipulation */
    LGSS_FILTER_CAT_DELIMITER       = (1 << 5), /**< Delimiter injection */
    LGSS_FILTER_CAT_INDIRECT        = (1 << 6), /**< Indirect injection markers */
    LGSS_FILTER_CAT_ALL             = 0x7F      /**< All categories */
} lgss_filter_category_flags_t;

/**
 * @brief Jailbreak pattern type
 *
 * WHAT: Classification of detected jailbreak pattern
 * WHY:  Different patterns may require different responses
 */
typedef enum {
    LGSS_JAILBREAK_NONE = 0,           /**< No jailbreak detected */
    LGSS_JAILBREAK_INSTRUCTION_OVERRIDE, /**< "Ignore previous instructions" */
    LGSS_JAILBREAK_ROLEPLAY,           /**< DAN, jailbroken character, etc. */
    LGSS_JAILBREAK_HYPOTHETICAL,       /**< "Hypothetically...", "In fiction..." */
    LGSS_JAILBREAK_ENCODED,            /**< Base64, ROT13, other encodings */
    LGSS_JAILBREAK_DELIMITER,          /**< Fake system/assistant tags */
    LGSS_JAILBREAK_MULTI_TURN,         /**< Multi-turn manipulation setup */
    LGSS_JAILBREAK_OTHER               /**< Other/unknown pattern */
} lgss_jailbreak_type_t;

/**
 * @brief Manipulation technique type
 *
 * WHAT: Classification of detected manipulation technique
 * WHY:  Track different manipulation strategies
 */
typedef enum {
    LGSS_MANIP_NONE = 0,               /**< No manipulation detected */
    LGSS_MANIP_EMOTIONAL,              /**< Emotional appeals */
    LGSS_MANIP_AUTHORITY,              /**< False authority claims */
    LGSS_MANIP_URGENCY,                /**< Urgency/scarcity pressure */
    LGSS_MANIP_SOCIAL_PROOF,           /**< "Everyone does it" appeals */
    LGSS_MANIP_RECIPROCITY,            /**< Implied debt/obligation */
    LGSS_MANIP_FLATTERY,               /**< Excessive flattery */
    LGSS_MANIP_THREAT,                 /**< Veiled threats */
    LGSS_MANIP_OTHER                   /**< Other/unknown technique */
} lgss_manipulation_type_t;

/*=============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

typedef struct lgss_content_filter lgss_content_filter_t;

/*=============================================================================
 * RESULT STRUCTURES
 *============================================================================*/

/**
 * @brief Content filter result
 *
 * WHAT: Detailed result of content filtering
 * WHY:  Provide actionable information about detected threats
 * HOW:  Status, type classification, confidence, matched patterns
 */
typedef struct {
    /* Overall result */
    lgss_content_status_t status;      /**< Filter result status */
    float confidence;                  /**< Overall confidence [0-1] */

    /* Type classification */
    lgss_jailbreak_type_t jailbreak_type;     /**< If jailbreak, what type */
    lgss_manipulation_type_t manipulation_type; /**< If manipulation, what type */

    /* Per-category scores */
    float instruction_score;           /**< Instruction override score */
    float roleplay_score;              /**< Role-play jailbreak score */
    float encoding_score;              /**< Encoded content score */
    float extraction_score;            /**< Prompt extraction score */
    float manipulation_score;          /**< Manipulation score */
    float delimiter_score;             /**< Delimiter injection score */
    float indirect_score;              /**< Indirect injection score */

    /* Triggered categories (bitmask) */
    uint32_t triggered_categories;     /**< Bitmask of triggered categories */

    /* Matched pattern info */
    bool pattern_matched;              /**< Whether a known pattern matched */
    char matched_pattern[128];         /**< Description of matched pattern */
    size_t match_offset;               /**< Offset where match was found */
    size_t match_length;               /**< Length of matched content */

    /* Explanation */
    char explanation[256];             /**< Human-readable explanation */

    /* Metadata */
    uint64_t timestamp_us;             /**< Filter timestamp */
    uint32_t filter_time_us;           /**< Time taken for filtering */
    size_t content_length;             /**< Length of content filtered */
} lgss_content_filter_result_t;

/*=============================================================================
 * CONFIGURATION STRUCTURE
 *============================================================================*/

/**
 * @brief Content filter configuration
 *
 * WHAT: Configuration for content filtering behavior
 * WHY:  Allow tuning of filter sensitivity and enabled categories
 * HOW:  Set flags and thresholds for each filtering component
 */
typedef struct {
    /* Category flags */
    uint32_t filter_categories;        /**< Bitmask of lgss_filter_category_flags_t */

    /* Overall threshold */
    float detection_threshold;         /**< Overall detection threshold [0-1] */

    /* Per-category thresholds */
    float instruction_threshold;       /**< Instruction override threshold */
    float roleplay_threshold;          /**< Role-play jailbreak threshold */
    float encoding_threshold;          /**< Encoded content threshold */
    float extraction_threshold;        /**< Prompt extraction threshold */
    float manipulation_threshold;      /**< Manipulation threshold */
    float delimiter_threshold;         /**< Delimiter injection threshold */
    float indirect_threshold;          /**< Indirect injection threshold */

    /* Pattern matching options */
    bool case_sensitive;               /**< Case-sensitive pattern matching */
    bool enable_fuzzy_matching;        /**< Enable fuzzy pattern matching */
    float fuzzy_threshold;             /**< Fuzzy matching similarity threshold */

    /* Encoding detection */
    bool detect_base64;                /**< Detect Base64 encoded content */
    bool detect_rot13;                 /**< Detect ROT13 encoded content */
    bool detect_unicode_tricks;        /**< Detect Unicode confusables */
    bool detect_whitespace_hiding;     /**< Detect hidden text in whitespace */

    /* Statistical analysis */
    bool enable_statistical;           /**< Enable statistical analysis */
    float entropy_threshold;           /**< Entropy threshold for anomaly */

    /* Custom patterns */
    const char** custom_patterns;      /**< Array of custom patterns */
    size_t num_custom_patterns;        /**< Number of custom patterns */

    /* Performance tuning */
    bool enable_fast_mode;             /**< Skip expensive checks on early detect */
    uint32_t max_filter_time_ms;       /**< Maximum time for filtering */

    /* Integration */
    bool enable_logging;               /**< Enable detailed logging */
    bool enable_bio_async;             /**< Enable bio-async notifications */
} lgss_content_filter_config_t;

/*=============================================================================
 * STATISTICS STRUCTURE
 *============================================================================*/

/**
 * @brief Content filter statistics
 *
 * WHAT: Cumulative statistics for filter operation
 * WHY:  Monitor filter effectiveness and performance
 */
typedef struct {
    /* Overall counts */
    uint64_t total_filtered;           /**< Total content filtered */
    uint64_t safe_count;               /**< Safe content passed */
    uint64_t threat_count;             /**< Threats detected */

    /* Per-status counts */
    uint64_t jailbreak_count;          /**< Jailbreak attempts */
    uint64_t injection_count;          /**< Injection attempts */
    uint64_t manipulation_count;       /**< Manipulation attempts */
    uint64_t suspicious_count;         /**< Suspicious content */
    uint64_t malformed_count;          /**< Malformed content */

    /* Per-jailbreak-type counts */
    uint64_t instruction_override_count;
    uint64_t roleplay_count;
    uint64_t hypothetical_count;
    uint64_t encoded_count;
    uint64_t delimiter_count;
    uint64_t multi_turn_count;

    /* Per-manipulation-type counts */
    uint64_t emotional_manip_count;
    uint64_t authority_manip_count;
    uint64_t urgency_manip_count;
    uint64_t other_manip_count;

    /* Performance metrics */
    float avg_filter_time_us;          /**< Average filter time */
    float max_filter_time_us;          /**< Maximum filter time */

    /* Accuracy tracking */
    uint64_t false_positives;          /**< Reported false positives */
    uint64_t false_negatives;          /**< Reported false negatives */
    float estimated_precision;         /**< Estimated precision */
    float estimated_recall;            /**< Estimated recall */
} lgss_content_filter_stats_t;

/*=============================================================================
 * CONFIGURATION API
 *============================================================================*/

/**
 * @brief Get default content filter configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with balanced security/usability
 * HOW:  Return pre-configured structure
 *
 * @param config Output configuration structure
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
NIMCP_EXPORT nimcp_error_t lgss_content_filter_default_config(
    lgss_content_filter_config_t* config
);

/*=============================================================================
 * LIFECYCLE API
 *============================================================================*/

/**
 * @brief Create content filter
 *
 * WHAT: Allocate and initialize content filter instance
 * WHY:  Enable content filtering for text inputs
 * HOW:  Allocate structure, load patterns, apply configuration
 *
 * @param validator Input validator to connect (required)
 * @param config Configuration (NULL for defaults)
 * @return Filter handle or NULL on failure
 */
NIMCP_EXPORT lgss_content_filter_t* lgss_content_filter_create(
    lgss_input_validator_t* validator,
    const lgss_content_filter_config_t* config
);

/**
 * @brief Destroy content filter
 *
 * WHAT: Clean up and free filter resources
 * WHY:  Proper resource deallocation
 * HOW:  Free internal structures, patterns, zero memory
 *
 * @param filter Filter handle (NULL safe)
 */
NIMCP_EXPORT void lgss_content_filter_destroy(
    lgss_content_filter_t* filter
);

/*=============================================================================
 * FILTERING API
 *============================================================================*/

/**
 * @brief Check if text content is safe
 *
 * WHAT: Primary content filtering function
 * WHY:  Determine if text content should be allowed
 * HOW:  Run all enabled filtering methods, aggregate results
 *
 * @param filter Filter handle
 * @param text Text content to filter
 * @param length Length of text in bytes
 * @param result Output: filter result
 * @return NIMCP_SUCCESS on success (check result for filter decision)
 */
NIMCP_EXPORT nimcp_error_t lgss_content_filter_is_safe(
    lgss_content_filter_t* filter,
    const char* text,
    size_t length,
    lgss_content_filter_result_t* result
);

/**
 * @brief Detect jailbreak attempts
 *
 * WHAT: Specialized jailbreak detection
 * WHY:  Focus on jailbreak patterns specifically
 * HOW:  Pattern matching + heuristics for jailbreak attempts
 *
 * @param filter Filter handle
 * @param text Text content to check
 * @param length Length of text in bytes
 * @param result Output: filter result
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_content_filter_detect_jailbreak(
    lgss_content_filter_t* filter,
    const char* text,
    size_t length,
    lgss_content_filter_result_t* result
);

/**
 * @brief Detect manipulation attempts
 *
 * WHAT: Specialized manipulation detection
 * WHY:  Focus on social engineering/manipulation patterns
 * HOW:  Pattern matching + sentiment/intent analysis
 *
 * @param filter Filter handle
 * @param text Text content to check
 * @param length Length of text in bytes
 * @param result Output: filter result
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_content_filter_detect_manipulation(
    lgss_content_filter_t* filter,
    const char* text,
    size_t length,
    lgss_content_filter_result_t* result
);

/**
 * @brief Detect prompt injection in context
 *
 * WHAT: Detect indirect prompt injection in retrieved content
 * WHY:  Retrieved documents may contain hidden instructions
 * HOW:  Look for instruction markers, delimiter abuse, etc.
 *
 * @param filter Filter handle
 * @param text Retrieved/context text to check
 * @param length Length of text in bytes
 * @param source Source identifier (e.g., "web", "database")
 * @param result Output: filter result
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_content_filter_detect_indirect_injection(
    lgss_content_filter_t* filter,
    const char* text,
    size_t length,
    const char* source,
    lgss_content_filter_result_t* result
);

/*=============================================================================
 * PATTERN MANAGEMENT API
 *============================================================================*/

/**
 * @brief Add custom pattern
 *
 * WHAT: Add a custom pattern to the filter
 * WHY:  Allow domain-specific pattern additions
 * HOW:  Compile and add pattern to pattern database
 *
 * @param filter Filter handle
 * @param pattern Pattern string (regex or literal)
 * @param category Category for the pattern
 * @param is_regex Whether pattern is regex (false = literal)
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_content_filter_add_pattern(
    lgss_content_filter_t* filter,
    const char* pattern,
    lgss_filter_category_flags_t category,
    bool is_regex
);

/**
 * @brief Clear custom patterns
 *
 * WHAT: Remove all custom patterns
 * WHY:  Reset to default patterns only
 * HOW:  Clear custom pattern list
 *
 * @param filter Filter handle
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_content_filter_clear_custom_patterns(
    lgss_content_filter_t* filter
);

/*=============================================================================
 * STATISTICS API
 *============================================================================*/

/**
 * @brief Get filter statistics
 *
 * WHAT: Retrieve cumulative filter statistics
 * WHY:  Monitor filter effectiveness
 * HOW:  Copy current statistics to output structure
 *
 * @param filter Filter handle
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_content_filter_get_stats(
    const lgss_content_filter_t* filter,
    lgss_content_filter_stats_t* stats
);

/**
 * @brief Reset filter statistics
 *
 * WHAT: Reset all statistics counters to zero
 * WHY:  Start fresh measurement period
 * HOW:  Zero out statistics structure
 *
 * @param filter Filter handle
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_content_filter_reset_stats(
    lgss_content_filter_t* filter
);

/**
 * @brief Report false positive
 *
 * WHAT: Report that a detection was a false positive
 * WHY:  Enable precision tracking
 * HOW:  Increment false positive counter
 *
 * @param filter Filter handle
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_content_filter_report_false_positive(
    lgss_content_filter_t* filter
);

/**
 * @brief Report false negative
 *
 * WHAT: Report that a threat was missed
 * WHY:  Enable recall tracking
 * HOW:  Increment false negative counter
 *
 * @param filter Filter handle
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t lgss_content_filter_report_false_negative(
    lgss_content_filter_t* filter
);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get content status name
 *
 * WHAT: Get human-readable name for content status
 * WHY:  Logging and debugging
 * HOW:  Lookup in static string table
 *
 * @param status Content status code
 * @return Human-readable name string
 */
NIMCP_EXPORT const char* lgss_content_status_name(
    lgss_content_status_t status
);

/**
 * @brief Get jailbreak type name
 *
 * WHAT: Get human-readable name for jailbreak type
 * WHY:  Logging and debugging
 * HOW:  Lookup in static string table
 *
 * @param type Jailbreak type code
 * @return Human-readable name string
 */
NIMCP_EXPORT const char* lgss_jailbreak_type_name(
    lgss_jailbreak_type_t type
);

/**
 * @brief Get manipulation type name
 *
 * WHAT: Get human-readable name for manipulation type
 * WHY:  Logging and debugging
 * HOW:  Lookup in static string table
 *
 * @param type Manipulation type code
 * @return Human-readable name string
 */
NIMCP_EXPORT const char* lgss_manipulation_type_name(
    lgss_manipulation_type_t type
);

/**
 * @brief Get filter category name
 *
 * WHAT: Get human-readable name for filter category
 * WHY:  Logging and debugging
 * HOW:  Lookup based on flag bit
 *
 * @param category Filter category flag (single bit)
 * @return Human-readable name string
 */
NIMCP_EXPORT const char* lgss_filter_category_name(
    lgss_filter_category_flags_t category
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_CONTENT_FILTER_H */
