/**
 * @file nimcp_lgss_speech_gate.c
 * @brief LGSS Speech Output Gate implementation
 *
 * WHAT: Implements gated speech/text output with content classification and filtering.
 * WHY:  Ensures all speech output is validated for safety before emission.
 * HOW:  Content classification, pattern matching, harm detection, and threshold filtering.
 */

#include "security/lgss/gates/nimcp_lgss_speech_gate.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_speech_gate, MESH_ADAPTER_CATEGORY_SECURITY)


#endif

/* =============================================================================
 * Internal Constants
 * ============================================================================= */

/** Maximum pattern length */
#define MAX_PATTERN_LENGTH 256

/* =============================================================================
 * Internal Structures
 * ============================================================================= */

/**
 * @brief Filter pattern entry
 */
typedef struct filter_pattern {
    char pattern[MAX_PATTERN_LENGTH];
    bool case_sensitive;
    struct filter_pattern* next;
} filter_pattern_t;

/**
 * @brief Speech gate internal structure
 */
struct speech_gate {
    uint32_t magic;                     /**< Magic number for validation */
    bool enabled;                       /**< Whether gate is enabled */
    speech_gate_config_t config;        /**< Gate configuration */
    speech_gate_stats_t stats;          /**< Operational statistics */
    filter_pattern_t* patterns;         /**< Linked list of filter patterns */
    uint32_t pattern_count;             /**< Number of patterns */
    uint32_t next_sequence_id;          /**< Next sequence ID for validation */
};

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

/**
 * @brief Get current timestamp in nanoseconds
 */
static uint64_t get_timestamp_ns(void) {
#ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#else
    return 0;
#endif
}

/**
 * @brief Validate gate structure
 */
static bool validate_gate(const speech_gate_t* gate) {
    return gate != NULL && gate->magic == NIMCP_SPEECH_GATE_MAGIC;
}

/**
 * @brief Case-insensitive string search
 */
static const char* strcasestr_local(const char* haystack, const char* needle) {
    if (haystack == NULL || needle == NULL) {
        return NULL;
    }

    size_t needle_len = strlen(needle);
    if (needle_len == 0) {
        return haystack;
    }

    size_t haystack_len = strlen(haystack);
    if (haystack_len < needle_len) {
        return NULL;
    }

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        bool match = true;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower((unsigned char)haystack[i + j]) !=
                tolower((unsigned char)needle[j])) {
                match = false;
                break;
            }
        }
        if (match) {
            return &haystack[i];
        }
    }

    return NULL;
}

/**
 * @brief Check if text matches a pattern
 */
static bool matches_pattern(const char* text, const filter_pattern_t* pattern) {
    if (text == NULL || pattern == NULL) {
        return false;
    }

    if (pattern->case_sensitive) {
        return strstr(text, pattern->pattern) != NULL;
    } else {
        return strcasestr_local(text, pattern->pattern) != NULL;
    }
}

/**
 * @brief Basic harmful content detection (placeholder for ML model)
 *
 * WHAT: Simple keyword-based harm detection
 * WHY:  Provides baseline protection until ML model is integrated
 * HOW:  Checks for known harmful phrases and patterns
 */
static float detect_harmful_content(const char* text, size_t length) {
    if (text == NULL || length == 0) {
        return 0.0f;
    }

    /* Placeholder: In production, this would call an ML classifier */
    /* For now, check for some basic harmful patterns */

    const char* harmful_patterns[] = {
        "kill", "harm", "hurt", "destroy", "attack",
        "weapon", "bomb", "explosive", "poison",
        NULL
    };

    int matches = 0;
    for (int i = 0; harmful_patterns[i] != NULL; i++) {
        if (strcasestr_local(text, harmful_patterns[i]) != NULL) {
            matches++;
        }
    }

    /* Very simple scoring - in production use ML model */
    float score = (float)matches * 0.2f;
    if (score > 1.0f) {
        score = 1.0f;
    }

    return score;
}

/**
 * @brief Basic deception detection (placeholder for ML model)
 */
static float detect_deceptive_content(const char* text, size_t length) {
    if (text == NULL || length == 0) {
        return 0.0f;
    }

    /* Placeholder: Check for basic deceptive patterns */
    const char* deceptive_patterns[] = {
        "trust me", "believe me", "honestly",
        "would never lie", "100% true", "guaranteed",
        NULL
    };

    int matches = 0;
    for (int i = 0; deceptive_patterns[i] != NULL; i++) {
        if (strcasestr_local(text, deceptive_patterns[i]) != NULL) {
            matches++;
        }
    }

    float score = (float)matches * 0.15f;
    if (score > 1.0f) {
        score = 1.0f;
    }

    return score;
}

/**
 * @brief Basic manipulation detection (placeholder for ML model)
 */
static float detect_manipulation_content(const char* text, size_t length) {
    if (text == NULL || length == 0) {
        return 0.0f;
    }

    /* Placeholder: Check for manipulative patterns */
    const char* manipulation_patterns[] = {
        "you must", "you have to", "you need to",
        "only way", "no choice", "everyone knows",
        "stupid if", "crazy not to",
        NULL
    };

    int matches = 0;
    for (int i = 0; manipulation_patterns[i] != NULL; i++) {
        if (strcasestr_local(text, manipulation_patterns[i]) != NULL) {
            matches++;
        }
    }

    float score = (float)matches * 0.2f;
    if (score > 1.0f) {
        score = 1.0f;
    }

    return score;
}

/**
 * @brief Basic private information detection (placeholder)
 */
static float detect_private_info(const char* text, size_t length) {
    if (text == NULL || length == 0) {
        return 0.0f;
    }

    /* Placeholder: Check for patterns that might indicate private info */
    /* In production, use NER and pattern matching for SSN, credit cards, etc. */

    float score = 0.0f;

    /* Check for potential SSN pattern (XXX-XX-XXXX) */
    for (size_t i = 0; i + 10 < length; i++) {
        if (isdigit((unsigned char)text[i]) &&
            isdigit((unsigned char)text[i + 1]) &&
            isdigit((unsigned char)text[i + 2]) &&
            text[i + 3] == '-' &&
            isdigit((unsigned char)text[i + 4]) &&
            isdigit((unsigned char)text[i + 5]) &&
            text[i + 6] == '-') {
            score += 0.5f;
        }
    }

    /* Check for email patterns */
    if (strcasestr_local(text, "@") != NULL &&
        (strcasestr_local(text, ".com") != NULL ||
         strcasestr_local(text, ".org") != NULL ||
         strcasestr_local(text, ".net") != NULL)) {
        score += 0.3f;
    }

    if (score > 1.0f) {
        score = 1.0f;
    }

    return score;
}

/**
 * @brief Basic unsafe instruction detection (placeholder)
 */
static float detect_unsafe_instructions(const char* text, size_t length) {
    if (text == NULL || length == 0) {
        return 0.0f;
    }

    /* Placeholder: Check for potentially dangerous instructions */
    const char* unsafe_patterns[] = {
        "how to make", "how to build", "instructions for",
        "step by step", "recipe for",
        "hack", "bypass", "exploit",
        NULL
    };

    int matches = 0;
    for (int i = 0; unsafe_patterns[i] != NULL; i++) {
        if (strcasestr_local(text, unsafe_patterns[i]) != NULL) {
            matches++;
        }
    }

    /* Context matters - these alone aren't harmful */
    float score = (float)matches * 0.1f;
    if (score > 1.0f) {
        score = 1.0f;
    }

    return score;
}

/**
 * @brief Check for profanity (placeholder)
 */
static bool contains_profanity(const char* text, size_t length) {
    if (text == NULL || length == 0) {
        return false;
    }

    /* Placeholder: In production, use a comprehensive profanity filter */
    /* For now, just return false */
    (void)text;
    (void)length;

    return false;
}

/**
 * @brief Check for URLs in text
 */
static bool contains_urls(const char* text, size_t length) {
    if (text == NULL || length == 0) {
        return false;
    }

    return strcasestr_local(text, "http://") != NULL ||
           strcasestr_local(text, "https://") != NULL ||
           strcasestr_local(text, "www.") != NULL;
}

/**
 * @brief Check for code/commands in text
 */
static bool contains_code(const char* text, size_t length) {
    if (text == NULL || length == 0) {
        return false;
    }

    /* Check for common code indicators */
    return strstr(text, "```") != NULL ||
           strstr(text, "function") != NULL ||
           strstr(text, "sudo") != NULL ||
           strstr(text, "rm -rf") != NULL ||
           strstr(text, "exec(") != NULL;
}

/**
 * @brief Fill emission details
 */
static void fill_emit_details(
    speech_gate_emit_details_t* details,
    speech_emit_result_t result,
    speech_content_class_t blocked_class,
    float confidence,
    const char* description,
    const char* matched_pattern
) {
    if (details == NULL) {
        return;
    }
    details->result = result;
    details->blocked_class = blocked_class;
    details->confidence = confidence;
    details->timestamp = get_timestamp_ns();
    details->matched_pattern = matched_pattern;
    if (description != NULL) {
        snprintf(details->description, sizeof(details->description), "%s", description);
    } else {
        details->description[0] = '\0';
    }
}

/* =============================================================================
 * Public Functions
 * ============================================================================= */

speech_gate_t* speech_gate_create(const speech_gate_config_t* config) {
    speech_gate_t* gate = (speech_gate_t*)nimcp_calloc(1, sizeof(speech_gate_t));
    if (gate == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gate is NULL");

        return NULL;
    }

    gate->magic = NIMCP_SPEECH_GATE_MAGIC;
    gate->enabled = true;
    gate->patterns = NULL;
    gate->pattern_count = 0;
    gate->next_sequence_id = 1;

    /* Apply configuration if provided */
    if (config != NULL) {
        memcpy(&gate->config, config, sizeof(speech_gate_config_t));
    } else {
        /* Default configuration - enable all safety filters */
        gate->config.harm_threshold = NIMCP_SPEECH_DEFAULT_HARM_THRESHOLD;
        gate->config.filter_harmful = true;
        gate->config.filter_deceptive = true;
        gate->config.filter_manipulation = true;
        gate->config.filter_private_info = true;
        gate->config.filter_unsafe_instructions = true;
        gate->config.filter_profanity = true;
        gate->config.allow_internal_bypass = false;
        gate->config.custom_classifier = NULL;
        gate->config.classifier_user_data = NULL;
        gate->config.log_all_emissions = false;
        gate->config.strict_mode = false;
    }

    /* Initialize statistics */
    memset(&gate->stats, 0, sizeof(speech_gate_stats_t));

    return gate;
}

void speech_gate_destroy(speech_gate_t* gate) {
    if (gate == NULL) {
        return;
    }
    if (gate->magic != NIMCP_SPEECH_GATE_MAGIC) {
        return;  /* Invalid gate */
    }

    /* Free all patterns */
    filter_pattern_t* pattern = gate->patterns;
    while (pattern != NULL) {
        filter_pattern_t* next = pattern->next;
        nimcp_free(pattern);
        pattern = next;
    }

    gate->magic = 0;  /* Invalidate before free */
    nimcp_free(gate);
}

speech_emit_result_t speech_gate_emit(
    speech_gate_t* gate,
    const speech_proposal_t* proposal,
    speech_gate_emit_details_t* details
) {
    uint64_t start_time = get_timestamp_ns();

    /* Validate inputs */
    if (!validate_gate(gate)) {
        fill_emit_details(details, SPEECH_EMIT_ERROR, SPEECH_CONTENT_SAFE, 0,
                         "Invalid gate", NULL);
        return SPEECH_EMIT_ERROR;
    }

    if (proposal == NULL || proposal->text == NULL) {
        fill_emit_details(details, SPEECH_EMIT_INVALID_PROPOSAL, SPEECH_CONTENT_SAFE, 0,
                         "NULL proposal or text", NULL);
        gate->stats.proposals_submitted++;
        gate->stats.proposals_blocked++;
        return SPEECH_EMIT_INVALID_PROPOSAL;
    }

    gate->stats.proposals_submitted++;

    /* Check if gate is enabled */
    if (!gate->enabled) {
        fill_emit_details(details, SPEECH_EMIT_GATE_DISABLED, SPEECH_CONTENT_SAFE, 0,
                         "Speech gate is disabled", NULL);
        gate->stats.proposals_blocked++;
        return SPEECH_EMIT_GATE_DISABLED;
    }

    /* Check length limit */
    if (proposal->length > NIMCP_SPEECH_MAX_LENGTH) {
        fill_emit_details(details, SPEECH_EMIT_BLOCKED_LENGTH, SPEECH_CONTENT_SAFE, 0,
                         "Content exceeds maximum length", NULL);
        gate->stats.proposals_blocked++;
        return SPEECH_EMIT_BLOCKED_LENGTH;
    }

    /* Allow internal bypass if configured */
    if (gate->config.allow_internal_bypass &&
        proposal->target_audience == SPEECH_AUDIENCE_INTERNAL) {
        /* Bypass all checks for internal communication */
        gate->stats.proposals_approved++;
        gate->stats.total_chars_approved += proposal->length;
        fill_emit_details(details, SPEECH_EMIT_SUCCESS, SPEECH_CONTENT_SAFE, 0,
                         "Internal bypass", NULL);
        return SPEECH_EMIT_SUCCESS;
    }

    /* Check against filter patterns */
    const filter_pattern_t* pattern = gate->patterns;
    while (pattern != NULL) {
        if (matches_pattern(proposal->text, pattern)) {
            fill_emit_details(details, SPEECH_EMIT_BLOCKED_FILTER, SPEECH_CONTENT_SAFE, 1.0f,
                             "Matched filter pattern", pattern->pattern);
            gate->stats.proposals_blocked++;
            gate->stats.pattern_blocked++;
            gate->stats.total_chars_blocked += proposal->length;
            return SPEECH_EMIT_BLOCKED_FILTER;
        }
        pattern = pattern->next;
    }

    /* Classify content */
    speech_classification_result_t classification;
    nimcp_result_t class_result = speech_gate_classify(gate, proposal->text,
                                                        proposal->length, &classification);
    if (class_result != NIMCP_SUCCESS) {
        fill_emit_details(details, SPEECH_EMIT_ERROR, SPEECH_CONTENT_SAFE, 0,
                         "Classification failed", NULL);
        gate->stats.proposals_blocked++;
        return SPEECH_EMIT_ERROR;
    }

    /* Check harmful content */
    if (gate->config.filter_harmful &&
        classification.class_confidences[SPEECH_CONTENT_HARMFUL] >= gate->config.harm_threshold) {
        fill_emit_details(details, SPEECH_EMIT_BLOCKED_HARMFUL, SPEECH_CONTENT_HARMFUL,
                         classification.class_confidences[SPEECH_CONTENT_HARMFUL],
                         classification.primary_concern, NULL);
        gate->stats.proposals_blocked++;
        gate->stats.harmful_blocked++;
        gate->stats.total_chars_blocked += proposal->length;
        return SPEECH_EMIT_BLOCKED_HARMFUL;
    }

    /* Check deceptive content */
    if (gate->config.filter_deceptive &&
        classification.class_confidences[SPEECH_CONTENT_DECEPTIVE] >= gate->config.harm_threshold) {
        fill_emit_details(details, SPEECH_EMIT_BLOCKED_DECEPTIVE, SPEECH_CONTENT_DECEPTIVE,
                         classification.class_confidences[SPEECH_CONTENT_DECEPTIVE],
                         "Deceptive content detected", NULL);
        gate->stats.proposals_blocked++;
        gate->stats.deceptive_blocked++;
        gate->stats.total_chars_blocked += proposal->length;
        return SPEECH_EMIT_BLOCKED_DECEPTIVE;
    }

    /* Check manipulation content */
    if (gate->config.filter_manipulation &&
        classification.class_confidences[SPEECH_CONTENT_MANIPULATION] >= gate->config.harm_threshold) {
        fill_emit_details(details, SPEECH_EMIT_BLOCKED_MANIPULATION, SPEECH_CONTENT_MANIPULATION,
                         classification.class_confidences[SPEECH_CONTENT_MANIPULATION],
                         "Manipulative content detected", NULL);
        gate->stats.proposals_blocked++;
        gate->stats.manipulation_blocked++;
        gate->stats.total_chars_blocked += proposal->length;
        return SPEECH_EMIT_BLOCKED_MANIPULATION;
    }

    /* Check private info */
    if (gate->config.filter_private_info &&
        classification.class_confidences[SPEECH_CONTENT_PRIVATE_INFO] >= gate->config.harm_threshold) {
        fill_emit_details(details, SPEECH_EMIT_BLOCKED_PRIVATE, SPEECH_CONTENT_PRIVATE_INFO,
                         classification.class_confidences[SPEECH_CONTENT_PRIVATE_INFO],
                         "Private information detected", NULL);
        gate->stats.proposals_blocked++;
        gate->stats.private_info_blocked++;
        gate->stats.total_chars_blocked += proposal->length;
        return SPEECH_EMIT_BLOCKED_PRIVATE;
    }

    /* Check unsafe instructions */
    if (gate->config.filter_unsafe_instructions &&
        classification.class_confidences[SPEECH_CONTENT_UNSAFE_INSTRUCTION] >= gate->config.harm_threshold) {
        fill_emit_details(details, SPEECH_EMIT_BLOCKED_UNSAFE, SPEECH_CONTENT_UNSAFE_INSTRUCTION,
                         classification.class_confidences[SPEECH_CONTENT_UNSAFE_INSTRUCTION],
                         "Unsafe instructions detected", NULL);
        gate->stats.proposals_blocked++;
        gate->stats.unsafe_blocked++;
        gate->stats.total_chars_blocked += proposal->length;
        return SPEECH_EMIT_BLOCKED_UNSAFE;
    }

    /* Strict mode: block any borderline content */
    if (gate->config.strict_mode && classification.overall_harm_score > 0.3f) {
        fill_emit_details(details, SPEECH_EMIT_BLOCKED_HARMFUL, classification.primary_class,
                         classification.overall_harm_score,
                         "Blocked by strict mode", NULL);
        gate->stats.proposals_blocked++;
        gate->stats.total_chars_blocked += proposal->length;
        return SPEECH_EMIT_BLOCKED_HARMFUL;
    }

    /* All checks passed - approve */
    gate->stats.proposals_approved++;
    gate->stats.total_chars_approved += proposal->length;

    /* Update classification time statistics */
    uint64_t elapsed_us = (get_timestamp_ns() - start_time) / 1000;
    float elapsed_f = (float)elapsed_us;

    if (gate->stats.proposals_approved == 1) {
        gate->stats.avg_classification_time_us = elapsed_f;
    } else {
        gate->stats.avg_classification_time_us =
            gate->stats.avg_classification_time_us * 0.95f + elapsed_f * 0.05f;
    }

    fill_emit_details(details, SPEECH_EMIT_SUCCESS, SPEECH_CONTENT_SAFE,
                     classification.overall_harm_score, "Content approved", NULL);
    return SPEECH_EMIT_SUCCESS;
}

nimcp_result_t speech_gate_classify(
    speech_gate_t* gate,
    const char* text,
    size_t length,
    speech_classification_result_t* result
) {
    if (!validate_gate(gate)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (text == NULL || result == NULL) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (length == 0) {
        /* Empty text is safe */
        memset(result, 0, sizeof(speech_classification_result_t));
        result->primary_class = SPEECH_CONTENT_SAFE;
        return NIMCP_SUCCESS;
    }

    /* Initialize result */
    memset(result, 0, sizeof(speech_classification_result_t));

    /* Use custom classifier if available */
    if (gate->config.custom_classifier != NULL) {
        return gate->config.custom_classifier(text, length, result,
                                               gate->config.classifier_user_data);
    }

    /* Built-in classification (placeholder for ML model) */
    result->class_confidences[SPEECH_CONTENT_SAFE] = 1.0f;  /* Start with safe assumption */
    result->class_confidences[SPEECH_CONTENT_HARMFUL] = detect_harmful_content(text, length);
    result->class_confidences[SPEECH_CONTENT_DECEPTIVE] = detect_deceptive_content(text, length);
    result->class_confidences[SPEECH_CONTENT_MANIPULATION] = detect_manipulation_content(text, length);
    result->class_confidences[SPEECH_CONTENT_PRIVATE_INFO] = detect_private_info(text, length);
    result->class_confidences[SPEECH_CONTENT_UNSAFE_INSTRUCTION] = detect_unsafe_instructions(text, length);

    /* Find primary class (highest non-safe score) */
    result->primary_class = SPEECH_CONTENT_SAFE;
    float max_score = 0.0f;
    for (int i = 1; i < NIMCP_SPEECH_CONTENT_CLASS_COUNT; i++) {
        if (result->class_confidences[i] > max_score) {
            max_score = result->class_confidences[i];
            result->primary_class = (speech_content_class_t)i;
        }
    }

    /* If nothing detected, it's safe */
    if (max_score < 0.1f) {
        result->primary_class = SPEECH_CONTENT_SAFE;
    }

    /* Adjust safe confidence based on detected concerns */
    result->class_confidences[SPEECH_CONTENT_SAFE] = 1.0f - max_score;

    /* Calculate overall harm score */
    result->overall_harm_score = max_score;

    /* Check additional properties */
    result->contains_profanity = contains_profanity(text, length);
    result->contains_personal_data = result->class_confidences[SPEECH_CONTENT_PRIVATE_INFO] > 0.3f;
    result->contains_urls = contains_urls(text, length);
    result->contains_code = contains_code(text, length);

    /* Generate primary concern description */
    if (result->primary_class != SPEECH_CONTENT_SAFE) {
        snprintf(result->primary_concern, sizeof(result->primary_concern),
                 "Detected %s content (confidence: %.2f)",
                 speech_content_class_name(result->primary_class),
                 result->class_confidences[result->primary_class]);
    } else {
        snprintf(result->primary_concern, sizeof(result->primary_concern),
                 "No significant concerns detected");
    }

    return NIMCP_SUCCESS;
}

bool speech_gate_would_block(
    const speech_gate_t* gate,
    const speech_proposal_t* proposal,
    speech_gate_emit_details_t* details
) {
    /* Create a mutable copy of gate for emit call */
    speech_gate_t* mutable_gate = (speech_gate_t*)gate;

    /* Save current stats */
    speech_gate_stats_t saved_stats;
    if (validate_gate(gate)) {
        memcpy(&saved_stats, &gate->stats, sizeof(speech_gate_stats_t));
    }

    /* Execute to check for blocking */
    speech_emit_result_t result = speech_gate_emit(mutable_gate, proposal, details);

    /* Restore stats */
    if (validate_gate(gate)) {
        memcpy(&mutable_gate->stats, &saved_stats, sizeof(speech_gate_stats_t));
    }

    return result != SPEECH_EMIT_SUCCESS;
}

nimcp_result_t speech_gate_add_filter_pattern(
    speech_gate_t* gate,
    const char* pattern,
    bool case_sensitive
) {
    if (!validate_gate(gate)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (pattern == NULL || strlen(pattern) == 0) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (strlen(pattern) >= MAX_PATTERN_LENGTH) {
        return NIMCP_ERROR_BUFFER_TOO_SMALL;
    }
    if (gate->pattern_count >= NIMCP_SPEECH_MAX_PATTERNS) {
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Check for duplicate */
    filter_pattern_t* existing = gate->patterns;
    while (existing != NULL) {
        if (strcmp(existing->pattern, pattern) == 0) {
            return NIMCP_ALREADY_EXISTS;
        }
        existing = existing->next;
    }

    /* Create new pattern */
    filter_pattern_t* new_pattern = (filter_pattern_t*)nimcp_calloc(1, sizeof(filter_pattern_t));
    if (new_pattern == NULL) {
        return NIMCP_NO_MEMORY;
    }

    strncpy(new_pattern->pattern, pattern, MAX_PATTERN_LENGTH - 1);
    new_pattern->case_sensitive = case_sensitive;
    new_pattern->next = gate->patterns;
    gate->patterns = new_pattern;
    gate->pattern_count++;

    return NIMCP_SUCCESS;
}

nimcp_result_t speech_gate_remove_filter_pattern(
    speech_gate_t* gate,
    const char* pattern
) {
    if (!validate_gate(gate)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (pattern == NULL) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    filter_pattern_t** prev = &gate->patterns;
    filter_pattern_t* current = gate->patterns;

    while (current != NULL) {
        if (strcmp(current->pattern, pattern) == 0) {
            *prev = current->next;
            nimcp_free(current);
            gate->pattern_count--;
            return NIMCP_SUCCESS;
        }
        prev = &current->next;
        current = current->next;
    }

    return NIMCP_NOT_FOUND;
}

nimcp_result_t speech_gate_clear_filter_patterns(speech_gate_t* gate) {
    if (!validate_gate(gate)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    filter_pattern_t* pattern = gate->patterns;
    while (pattern != NULL) {
        filter_pattern_t* next = pattern->next;
        nimcp_free(pattern);
        pattern = next;
    }

    gate->patterns = NULL;
    gate->pattern_count = 0;

    return NIMCP_SUCCESS;
}

nimcp_result_t speech_gate_set_harm_threshold(
    speech_gate_t* gate,
    float threshold
) {
    if (!validate_gate(gate)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (threshold < 0.0f || threshold > 1.0f) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    gate->config.harm_threshold = threshold;
    return NIMCP_SUCCESS;
}

nimcp_result_t speech_gate_set_enabled(speech_gate_t* gate, bool enabled) {
    if (!validate_gate(gate)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    gate->enabled = enabled;
    return NIMCP_SUCCESS;
}

nimcp_result_t speech_gate_get_stats(
    const speech_gate_t* gate,
    speech_gate_stats_t* stats
) {
    if (!validate_gate(gate)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (stats == NULL) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memcpy(stats, &gate->stats, sizeof(speech_gate_stats_t));
    return NIMCP_SUCCESS;
}

nimcp_result_t speech_gate_reset_stats(speech_gate_t* gate) {
    if (!validate_gate(gate)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(&gate->stats, 0, sizeof(speech_gate_stats_t));
    return NIMCP_SUCCESS;
}

const char* speech_content_class_name(speech_content_class_t content_class) {
    switch (content_class) {
        case SPEECH_CONTENT_SAFE:              return "SAFE";
        case SPEECH_CONTENT_HARMFUL:           return "HARMFUL";
        case SPEECH_CONTENT_DECEPTIVE:         return "DECEPTIVE";
        case SPEECH_CONTENT_MANIPULATION:      return "MANIPULATION";
        case SPEECH_CONTENT_PRIVATE_INFO:      return "PRIVATE_INFO";
        case SPEECH_CONTENT_UNSAFE_INSTRUCTION: return "UNSAFE_INSTRUCTION";
        default:                               return "UNKNOWN";
    }
}

const char* speech_emit_result_name(speech_emit_result_t result) {
    switch (result) {
        case SPEECH_EMIT_SUCCESS:              return "SUCCESS";
        case SPEECH_EMIT_BLOCKED_HARMFUL:      return "BLOCKED_HARMFUL";
        case SPEECH_EMIT_BLOCKED_DECEPTIVE:    return "BLOCKED_DECEPTIVE";
        case SPEECH_EMIT_BLOCKED_MANIPULATION: return "BLOCKED_MANIPULATION";
        case SPEECH_EMIT_BLOCKED_PRIVATE:      return "BLOCKED_PRIVATE";
        case SPEECH_EMIT_BLOCKED_UNSAFE:       return "BLOCKED_UNSAFE";
        case SPEECH_EMIT_BLOCKED_FILTER:       return "BLOCKED_FILTER";
        case SPEECH_EMIT_BLOCKED_LENGTH:       return "BLOCKED_LENGTH";
        case SPEECH_EMIT_GATE_DISABLED:        return "GATE_DISABLED";
        case SPEECH_EMIT_INVALID_PROPOSAL:     return "INVALID_PROPOSAL";
        case SPEECH_EMIT_ERROR:                return "ERROR";
        default:                               return "UNKNOWN";
    }
}

const char* speech_audience_name(speech_audience_t audience) {
    switch (audience) {
        case SPEECH_AUDIENCE_GENERAL:      return "GENERAL";
        case SPEECH_AUDIENCE_ADULT:        return "ADULT";
        case SPEECH_AUDIENCE_CHILD:        return "CHILD";
        case SPEECH_AUDIENCE_PROFESSIONAL: return "PROFESSIONAL";
        case SPEECH_AUDIENCE_MEDICAL:      return "MEDICAL";
        case SPEECH_AUDIENCE_INTERNAL:     return "INTERNAL";
        default:                           return "UNKNOWN";
    }
}
