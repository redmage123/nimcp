/**
 * @file nimcp_lgss_speech_gate.h
 * @brief LGSS Speech Output Gate - Final safety barrier for speech/text output
 *
 * WHAT: Provides gated speech and text output with content classification
 *       and harm prevention. ALL speech/text output must pass through this gate.
 *
 * WHY:  Speech output can potentially cause harm through deception, manipulation,
 *       private information disclosure, or unsafe instruction delivery. This gate
 *       ensures all output is classified and filtered appropriately.
 *
 * HOW:  Implements content classification (placeholder for ML models), harm
 *       detection, and configurable filtering. Suspicious content is blocked
 *       or flagged based on confidence thresholds.
 *
 * BIOLOGICAL BASIS: Analogous to Broca's area speech production pathway with
 *       social cognition filtering and prefrontal cortex ethical oversight.
 */

#ifndef NIMCP_LGSS_SPEECH_GATE_H
#define NIMCP_LGSS_SPEECH_GATE_H

#include "utils/validation/nimcp_common.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * ============================================================================= */

/** Magic number for speech gate validation ('SPGT') */
#define NIMCP_SPEECH_GATE_MAGIC 0x53504754

/** Maximum speech content length for single proposal */
#define NIMCP_SPEECH_MAX_LENGTH 65536

/** Maximum number of filter patterns */
#define NIMCP_SPEECH_MAX_PATTERNS 256

/** Default harm threshold (0.0-1.0) */
#define NIMCP_SPEECH_DEFAULT_HARM_THRESHOLD 0.7f

/** Number of content class categories */
#define NIMCP_SPEECH_CONTENT_CLASS_COUNT 6

/* =============================================================================
 * Enumerations
 * ============================================================================= */

/**
 * @brief Content classification categories
 *
 * WHAT: Categories for classifying speech/text content
 * WHY:  Different content types require different handling
 * HOW:  Classification model assigns category and confidence
 */
typedef enum {
    SPEECH_CONTENT_SAFE = 0,        /**< Content is safe for output */
    SPEECH_CONTENT_HARMFUL,         /**< Content could cause direct harm */
    SPEECH_CONTENT_DECEPTIVE,       /**< Content is misleading or false */
    SPEECH_CONTENT_MANIPULATION,    /**< Content attempts to manipulate */
    SPEECH_CONTENT_PRIVATE_INFO,    /**< Content contains private information */
    SPEECH_CONTENT_UNSAFE_INSTRUCTION /**< Content provides dangerous instructions */
} speech_content_class_t;

/**
 * @brief Speech gate emission result
 */
typedef enum {
    SPEECH_EMIT_SUCCESS = 0,        /**< Content emitted successfully */
    SPEECH_EMIT_BLOCKED_HARMFUL,    /**< Blocked: harmful content detected */
    SPEECH_EMIT_BLOCKED_DECEPTIVE,  /**< Blocked: deceptive content detected */
    SPEECH_EMIT_BLOCKED_MANIPULATION, /**< Blocked: manipulation detected */
    SPEECH_EMIT_BLOCKED_PRIVATE,    /**< Blocked: private info detected */
    SPEECH_EMIT_BLOCKED_UNSAFE,     /**< Blocked: unsafe instruction detected */
    SPEECH_EMIT_BLOCKED_FILTER,     /**< Blocked: matched filter pattern */
    SPEECH_EMIT_BLOCKED_LENGTH,     /**< Blocked: exceeded length limit */
    SPEECH_EMIT_GATE_DISABLED,      /**< Speech gate is disabled */
    SPEECH_EMIT_INVALID_PROPOSAL,   /**< Invalid speech proposal */
    SPEECH_EMIT_ERROR               /**< General emission error */
} speech_emit_result_t;

/**
 * @brief Target audience categories
 *
 * WHAT: Categories for the intended audience of speech
 * WHY:  Audience affects what content is appropriate
 * HOW:  Used to adjust filtering thresholds
 */
typedef enum {
    SPEECH_AUDIENCE_GENERAL = 0,    /**< General/unknown audience */
    SPEECH_AUDIENCE_ADULT,          /**< Adult audience */
    SPEECH_AUDIENCE_CHILD,          /**< Child/minor audience - strictest filtering */
    SPEECH_AUDIENCE_PROFESSIONAL,   /**< Professional/technical context */
    SPEECH_AUDIENCE_MEDICAL,        /**< Medical/healthcare context */
    SPEECH_AUDIENCE_INTERNAL        /**< Internal system communication only */
} speech_audience_t;

/* =============================================================================
 * Structures
 * ============================================================================= */

/**
 * @brief Speech/text proposal for gate evaluation
 *
 * WHAT: A proposed speech/text output awaiting gate approval
 * WHY:  All speech output must be validated before emission
 * HOW:  Contains text content and classification metadata
 */
typedef struct {
    const char* text;               /**< Text content to emit */
    size_t length;                  /**< Length of text in bytes */
    speech_content_class_t detected_class; /**< Detected content class */
    float harm_confidence;          /**< Confidence in harm detection (0.0-1.0) */
    speech_audience_t target_audience; /**< Target audience for content */
    uint64_t timestamp;             /**< Proposal timestamp (ns since epoch) */
    uint32_t sequence_id;           /**< Unique sequence identifier */
    const char* context;            /**< Optional context string */
    void* metadata;                 /**< Optional additional metadata */
    size_t metadata_size;           /**< Size of metadata in bytes */
} speech_proposal_t;

/**
 * @brief Classification result from content analysis
 *
 * WHAT: Detailed classification results for speech content
 * WHY:  Provides confidence scores for each content class
 * HOW:  Populated by speech_gate_classify()
 */
typedef struct {
    speech_content_class_t primary_class; /**< Most likely content class */
    float class_confidences[NIMCP_SPEECH_CONTENT_CLASS_COUNT]; /**< Per-class confidence */
    float overall_harm_score;        /**< Overall harmfulness score (0.0-1.0) */
    bool contains_profanity;         /**< Profanity detected */
    bool contains_personal_data;     /**< Personal identifiable info detected */
    bool contains_urls;              /**< URLs detected */
    bool contains_code;              /**< Code/commands detected */
    uint32_t flagged_patterns;       /**< Number of flagged patterns matched */
    char primary_concern[256];       /**< Human-readable primary concern */
} speech_classification_result_t;

/**
 * @brief Speech gate emission details
 *
 * WHAT: Detailed information about why content was blocked/allowed
 * WHY:  Enables debugging and audit logging
 * HOW:  Populated when speech_gate_emit() is called
 */
typedef struct {
    speech_emit_result_t result;     /**< Emission result */
    speech_content_class_t blocked_class; /**< Class that triggered block */
    float confidence;                /**< Confidence of blocking decision */
    char description[256];           /**< Human-readable description */
    uint64_t timestamp;              /**< When decision was made */
    const char* matched_pattern;     /**< Pattern that matched (if applicable) */
} speech_gate_emit_details_t;

/**
 * @brief Speech gate statistics
 *
 * WHAT: Operational statistics for the speech gate
 * WHY:  Enables monitoring and tuning of filtering
 * HOW:  Updated atomically during gate operations
 */
typedef struct {
    uint64_t proposals_submitted;    /**< Total proposals submitted */
    uint64_t proposals_approved;     /**< Proposals that passed the gate */
    uint64_t proposals_blocked;      /**< Proposals blocked */
    uint64_t harmful_blocked;        /**< Harmful content blocked */
    uint64_t deceptive_blocked;      /**< Deceptive content blocked */
    uint64_t manipulation_blocked;   /**< Manipulation content blocked */
    uint64_t private_info_blocked;   /**< Private info blocked */
    uint64_t unsafe_blocked;         /**< Unsafe instructions blocked */
    uint64_t pattern_blocked;        /**< Filter pattern matches */
    uint64_t total_chars_approved;   /**< Total characters approved */
    uint64_t total_chars_blocked;    /**< Total characters blocked */
    float avg_classification_time_us; /**< Average classification time (us) */
} speech_gate_stats_t;

/**
 * @brief Custom content classifier callback type
 *
 * WHAT: Function pointer for external content classification
 * WHY:  Allows integration with ML classification models
 * HOW:  Called during content classification
 *
 * @param text Text content to classify
 * @param length Text length in bytes
 * @param result Output: classification result
 * @param user_data User-provided context data
 * @return NIMCP_SUCCESS or error code
 */
typedef nimcp_result_t (*speech_classifier_callback_t)(
    const char* text,
    size_t length,
    speech_classification_result_t* result,
    void* user_data
);

/** Forward declaration */
typedef struct speech_gate speech_gate_t;

/**
 * @brief Speech gate configuration
 */
typedef struct {
    float harm_threshold;            /**< Threshold for blocking harmful (0.0-1.0) */
    bool filter_harmful;             /**< Enable harmful content filtering */
    bool filter_deceptive;           /**< Enable deceptive content filtering */
    bool filter_manipulation;        /**< Enable manipulation filtering */
    bool filter_private_info;        /**< Enable private info filtering */
    bool filter_unsafe_instructions; /**< Enable unsafe instruction filtering */
    bool filter_profanity;           /**< Enable profanity filtering */
    bool allow_internal_bypass;      /**< Allow bypass for internal audience */
    speech_classifier_callback_t custom_classifier; /**< External classifier */
    void* classifier_user_data;      /**< User data for classifier */
    bool log_all_emissions;          /**< Log all emissions (debug mode) */
    bool strict_mode;                /**< Block any borderline content */
} speech_gate_config_t;

/* =============================================================================
 * Function Declarations
 * ============================================================================= */

/**
 * @brief Create a speech gate instance
 *
 * WHAT: Allocates and initializes a new speech gate
 * WHY:  Speech gate is required for safe text/speech output
 * HOW:  Allocates memory, initializes default filters
 *
 * @param config Optional configuration (NULL for defaults)
 * @return New speech gate instance, or NULL on failure
 */
speech_gate_t* speech_gate_create(const speech_gate_config_t* config);

/**
 * @brief Destroy a speech gate instance
 *
 * WHAT: Deallocates a speech gate and all associated resources
 * WHY:  Proper cleanup prevents memory leaks
 * HOW:  Frees all internal structures and the gate itself
 *
 * @param gate Speech gate to destroy
 */
void speech_gate_destroy(speech_gate_t* gate);

/**
 * @brief Emit speech/text through the gate
 *
 * WHAT: Validates and emits speech/text content
 * WHY:  This is the ONLY path through which speech should be emitted
 * HOW:  Classifies content, checks against filters, emits if approved
 *
 * @param gate Speech gate instance
 * @param proposal Speech proposal to emit
 * @param details Output: emission details (can be NULL)
 * @return Emission result code
 */
speech_emit_result_t speech_gate_emit(
    speech_gate_t* gate,
    const speech_proposal_t* proposal,
    speech_gate_emit_details_t* details
);

/**
 * @brief Classify content without emitting
 *
 * WHAT: Performs content classification only
 * WHY:  Allows pre-checking content before submission
 * HOW:  Runs classification pipeline, returns results
 *
 * @param gate Speech gate instance
 * @param text Text content to classify
 * @param length Text length in bytes
 * @param result Output: classification result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t speech_gate_classify(
    speech_gate_t* gate,
    const char* text,
    size_t length,
    speech_classification_result_t* result
);

/**
 * @brief Check if content would be blocked without emitting
 *
 * WHAT: Dry-run validation of speech content
 * WHY:  Allows pre-checking content before submission
 * HOW:  Performs all checks but skips actual emission
 *
 * @param gate Speech gate instance
 * @param proposal Speech proposal to check
 * @param details Output: emission details if would block (can be NULL)
 * @return true if content would be blocked, false if would pass
 */
bool speech_gate_would_block(
    const speech_gate_t* gate,
    const speech_proposal_t* proposal,
    speech_gate_emit_details_t* details
);

/**
 * @brief Add a filter pattern to block
 *
 * WHAT: Adds a text pattern to the block list
 * WHY:  Allows custom filtering beyond ML classification
 * HOW:  Pattern is matched during emission checks
 *
 * @param gate Speech gate instance
 * @param pattern Pattern string to block (can include wildcards)
 * @param case_sensitive Whether match is case-sensitive
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t speech_gate_add_filter_pattern(
    speech_gate_t* gate,
    const char* pattern,
    bool case_sensitive
);

/**
 * @brief Remove a filter pattern
 *
 * WHAT: Removes a text pattern from the block list
 * WHY:  Allows adjustment of custom filters
 * HOW:  Removes pattern from active filter list
 *
 * @param gate Speech gate instance
 * @param pattern Pattern string to remove
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t speech_gate_remove_filter_pattern(
    speech_gate_t* gate,
    const char* pattern
);

/**
 * @brief Clear all filter patterns
 *
 * WHAT: Removes all custom filter patterns
 * WHY:  Allows reset of filter configuration
 * HOW:  Clears entire pattern list
 *
 * @param gate Speech gate instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t speech_gate_clear_filter_patterns(speech_gate_t* gate);

/**
 * @brief Set harm detection threshold
 *
 * WHAT: Configures the threshold for blocking harmful content
 * WHY:  Allows tuning of sensitivity
 * HOW:  Content with harm score above threshold is blocked
 *
 * @param gate Speech gate instance
 * @param threshold New threshold (0.0-1.0)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t speech_gate_set_harm_threshold(
    speech_gate_t* gate,
    float threshold
);

/**
 * @brief Enable or disable the speech gate
 *
 * WHAT: Enables or disables all speech filtering
 * WHY:  Emergency bypass or testing
 * HOW:  When disabled, all content passes through
 *
 * WARNING: Disabling the gate removes all safety protections!
 *
 * @param gate Speech gate instance
 * @param enabled Whether gate should be enabled
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t speech_gate_set_enabled(speech_gate_t* gate, bool enabled);

/**
 * @brief Get speech gate statistics
 *
 * WHAT: Retrieves operational statistics
 * WHY:  Enables monitoring and performance analysis
 * HOW:  Copies current statistics to output structure
 *
 * @param gate Speech gate instance
 * @param stats Output buffer for statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t speech_gate_get_stats(
    const speech_gate_t* gate,
    speech_gate_stats_t* stats
);

/**
 * @brief Reset speech gate statistics
 *
 * WHAT: Clears all accumulated statistics
 * WHY:  Allows fresh statistics collection
 * HOW:  Zeros all statistic counters atomically
 *
 * @param gate Speech gate instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t speech_gate_reset_stats(speech_gate_t* gate);

/**
 * @brief Get human-readable name for a content class
 *
 * @param content_class Content class enum value
 * @return Static string name for the content class
 */
const char* speech_content_class_name(speech_content_class_t content_class);

/**
 * @brief Get human-readable name for an emission result
 *
 * @param result Emission result enum value
 * @return Static string name for the result
 */
const char* speech_emit_result_name(speech_emit_result_t result);

/**
 * @brief Get human-readable name for an audience type
 *
 * @param audience Audience enum value
 * @return Static string name for the audience type
 */
const char* speech_audience_name(speech_audience_t audience);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_SPEECH_GATE_H */
