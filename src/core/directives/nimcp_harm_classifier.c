/**
 * @file nimcp_harm_classifier.c
 * @brief Implementation of harm classification system
 * @version 1.0.0
 * @date 2025-12-16
 *
 * WHAT: Action outcome harm classifier for directive safety
 * WHY:  Enable safe directive-following with quantitative harm assessment
 * HOW:  Pattern matching, severity weighting, threshold-based classification
 */

#include "core/directives/nimcp_harm_classifier.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(harm_classifier)

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** @brief Default severity values for each harm type */
static const float DEFAULT_SEVERITIES[HARM_TYPE_COUNT] = {
    0.7f,  /* PHYSICAL_INJURY */
    1.0f,  /* DEATH */
    0.6f,  /* PSYCHOLOGICAL */
    0.3f,  /* PROPERTY_DAMAGE */
    0.5f,  /* PRIVACY_VIOLATION */
    0.4f,  /* FINANCIAL */
    0.6f,  /* ENVIRONMENTAL */
    0.5f   /* SECURITY_BREACH */
};

/** @brief Harm type names for logging */
static const char* HARM_TYPE_NAMES[HARM_TYPE_COUNT] = {
    "PHYSICAL_INJURY",
    "DEATH",
    "PSYCHOLOGICAL",
    "PROPERTY_DAMAGE",
    "PRIVACY_VIOLATION",
    "FINANCIAL",
    "ENVIRONMENTAL",
    "SECURITY_BREACH"
};

/** @brief Keyword patterns for each harm type */
static const char* HARM_KEYWORDS[HARM_TYPE_COUNT][HARM_CLASSIFIER_MAX_KEYWORDS] = {
    /* PHYSICAL_INJURY */
    {"injure", "hurt", "wound", "damage", "break", "fracture", "cut", "burn",
     "stab", "shoot", "strike", "hit", "punch", "kick", "beat", "assault"},

    /* DEATH */
    {"kill", "murder", "execute", "assassinate", "terminate", "destroy life",
     "end life", "lethal", "fatal", "deadly", "death", "die", "poison", "strangle",
     "suffocate", "drown"},

    /* PSYCHOLOGICAL */
    {"traumatize", "torture", "abuse", "harass", "bully", "threaten", "intimidate",
     "manipulate", "gaslight", "distress", "torment", "terrorize", "frighten",
     "scare", "humiliate", "degrade"},

    /* PROPERTY_DAMAGE */
    {"delete", "remove", "destroy", "demolish", "break", "smash", "vandalize",
     "wreck", "ruin", "damage property", "burn down", "tear down", "trash",
     "sabotage", "deface", "corrupt"},

    /* PRIVACY_VIOLATION */
    {"spy", "surveil", "track", "monitor", "eavesdrop", "intercept", "expose",
     "leak", "reveal", "disclose", "publish private", "dox", "invade privacy",
     "steal data", "access private", "breach confidentiality"},

    /* FINANCIAL */
    {"steal", "rob", "embezzle", "defraud", "scam", "swindle", "extort",
     "bankrupt", "financial loss", "drain funds", "misappropriate", "launder",
     "forge", "counterfeit", "ransomware", "crypto theft"},

    /* ENVIRONMENTAL */
    {"pollute", "contaminate", "dump waste", "toxic release", "oil spill",
     "deforest", "clear cut", "habitat destruction", "species extinction",
     "overfishing", "poison ecosystem", "carbon emission", "climate damage",
     "nuclear waste", "chemical leak", "radiation"},

    /* SECURITY_BREACH */
    {"hack", "breach", "exploit", "penetrate", "backdoor", "rootkit", "malware",
     "virus", "ransomware", "ddos", "injection", "overflow", "unauthorized access",
     "crack password", "bypass security", "zero-day"}
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Convert string to lowercase in-place
 *
 * WHAT: Lowercase conversion for case-insensitive matching
 * WHY:  Pattern matching should ignore case
 * HOW:  tolower() on each character
 */
static void lowercase_string(char* str) {
    if (!str) return;

    for (size_t i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

/**
 * @brief Check if keyword present in text
 *
 * WHAT: Case-insensitive substring search
 * WHY:  Detect harm indicators in action descriptions
 * HOW:  strstr() after lowercasing
 */
static bool contains_keyword(const char* text, const char* keyword) {
    if (!text || !keyword) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contains_keyword: required parameter is NULL (text, keyword)");
        return false;
    }

    /* Create lowercase copies */
    char text_lower[HARM_CLASSIFIER_MAX_ACTION_DESC];
    char keyword_lower[NIMCP_LABEL_BUFFER_SIZE];

    strncpy(text_lower, text, sizeof(text_lower) - 1);
    text_lower[sizeof(text_lower) - 1] = '\0';
    lowercase_string(text_lower);

    strncpy(keyword_lower, keyword, sizeof(keyword_lower) - 1);
    keyword_lower[sizeof(keyword_lower) - 1] = '\0';
    lowercase_string(keyword_lower);

    return strstr(text_lower, keyword_lower) != NULL;
}

/**
 * @brief Detect harm type in action description
 *
 * WHAT: Pattern match keywords for specific harm type
 * WHY:  Identify potential harms from text
 * HOW:  Check all keywords, return highest probability match
 */
static float detect_harm_type(
    const char* action_description,
    harm_type_t type,
    char* description_out
) {
    if (!action_description) return 0.0f;

    uint32_t matches = 0;
    const char* matched_keyword = NULL;

    /* Check all keywords for this harm type */
    for (int i = 0; i < HARM_CLASSIFIER_MAX_KEYWORDS; i++) {
        const char* keyword = HARM_KEYWORDS[type][i];
        if (!keyword) break;

        if (contains_keyword(action_description, keyword)) {
            matches++;
            if (!matched_keyword) {
                matched_keyword = keyword;
            }
        }
    }

    /* No matches */
    if (matches == 0) {
        return 0.0f;
    }

    /* Populate description */
    if (description_out && matched_keyword) {
        snprintf(description_out, HARM_CLASSIFIER_MAX_HARM_DESC,
                 "Detected '%s' pattern (%u matches)",
                 matched_keyword, matches);
    }

    /* Probability scales with match count (cap at 1.0) */
    float probability = fminf(1.0f, matches * 0.3f);
    return probability;
}

/**
 * @brief Compute total harm score
 *
 * WHAT: Weighted sum of all harm assessments
 * WHY:  Aggregate score for classification decision
 * HOW:  Σ (probability × severity × weight)
 */
static float compute_total_harm_score(
    const harm_classification_t* result,
    const harm_classifier_config_t* config
) {
    if (!result || !config) return 0.0f;

    float total = 0.0f;

    for (uint32_t i = 0; i < HARM_TYPE_COUNT; i++) {
        const harm_assessment_t* harm = &result->harms[i];
        float weight = config->severity_weights[i];

        /* Contribution = P(harm) × severity × weight */
        total += harm->probability * harm->severity * weight;
    }

    return total;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int harm_classifier_default_config(harm_classifier_config_t* config) {
    /* Guard clause */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "harm_classifier_default_config: config is NULL");
        return -1;
    }

    /* Set defaults */
    config->harm_threshold = HARM_CLASSIFIER_DEFAULT_THRESHOLD;
    config->strict_mode = false;
    config->enable_pattern_matching = true;
    config->enable_context_analysis = true;

    /* Initialize severity weights to 1.0 (use default severities) */
    for (int i = 0; i < HARM_TYPE_COUNT; i++) {
        config->severity_weights[i] = 1.0f;
    }

    return 0;
}

harm_classifier_t* harm_classifier_create(const harm_classifier_config_t* config) {
    /* Allocate classifier */
    harm_classifier_t* classifier = (harm_classifier_t*)nimcp_calloc(1, sizeof(harm_classifier_t));
    if (!classifier) {
        NIMCP_LOGGING_ERROR("Failed to allocate harm classifier");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "classifier is NULL");

        return NULL;
    }

    /* Apply config or defaults */
    if (config) {
        classifier->config = *config;
    } else {
        harm_classifier_default_config(&classifier->config);
    }

    /* Create mutex */
    classifier->mutex = nimcp_platform_mutex_create();
    if (!classifier->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(classifier);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "harm_classifier_create: classifier->mutex is NULL");
        return NULL;
    }

    /* Initialize stats */
    memset(&classifier->stats, 0, sizeof(harm_classifier_stats_t));

    /* Bio-async disabled by default */
    classifier->bio_async_enabled = false;
    classifier->bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Created harm classifier (threshold=%.3f, strict=%d)",
                       classifier->config.harm_threshold,
                       classifier->config.strict_mode);

    return classifier;
}

void harm_classifier_destroy(harm_classifier_t* classifier) {
    /* Guard clause */
    if (!classifier) return;

    /* Disconnect bio-async if connected */
    if (classifier->bio_async_enabled) {
        harm_classifier_disconnect_bio_async(classifier);
    }

    /* Destroy mutex */
    if (classifier->mutex) {
        nimcp_platform_mutex_destroy(classifier->mutex);
        nimcp_free(classifier->mutex);
        classifier->mutex = NULL;
    }

    /* Free classifier */
    nimcp_free(classifier);

    NIMCP_LOGGING_INFO("Destroyed harm classifier");
}

/* ============================================================================
 * Classification API Implementation
 * ============================================================================ */

int harm_classifier_classify(
    harm_classifier_t* classifier,
    const char* action_description,
    const void* action_data,
    size_t data_len,
    harm_classification_t* result
) {
    /* Guard clauses */
    if (!classifier) {
        NIMCP_LOGGING_ERROR("NULL classifier");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "harm_classifier_classify: classifier is NULL");
        return -1;
    }
    if (!action_description) {
        NIMCP_LOGGING_ERROR("NULL action_description");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "harm_classifier_classify: action_description is NULL");
        return -1;
    }
    if (!result) {
        NIMCP_LOGGING_ERROR("NULL result");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "harm_classifier_classify: result is NULL");
        return -1;
    }

    /* Lock for thread safety */
    nimcp_platform_mutex_lock(classifier->mutex);

    /* Initialize result */
    memset(result, 0, sizeof(harm_classification_t));

    /* Pattern matching enabled? */
    if (!classifier->config.enable_pattern_matching) {
        result->is_harmful = false;
        result->confidence = 0.0f;
        nimcp_platform_mutex_unlock(classifier->mutex);
        return 0;
    }

    /* Detect each harm type */
    for (uint32_t i = 0; i < HARM_TYPE_COUNT; i++) {
        harm_assessment_t* harm = &result->harms[i];
        harm->type = (harm_type_t)i;
        harm->severity = DEFAULT_SEVERITIES[i];

        /* Detect probability via pattern matching */
        harm->probability = detect_harm_type(
            action_description,
            (harm_type_t)i,
            harm->description
        );

        if (harm->probability > 0.0f) {
            result->harm_count++;
            classifier->stats.pattern_matches++;
        }
    }

    /* Compute total harm score */
    result->total_harm_score = compute_total_harm_score(result, &classifier->config);

    /* Classification decision */
    if (classifier->config.strict_mode) {
        result->is_harmful = (result->harm_count > 0);
    } else {
        result->is_harmful = (result->total_harm_score > classifier->config.harm_threshold);
    }

    /* Confidence = normalized harm score */
    result->confidence = fminf(1.0f, result->total_harm_score);

    /* Update stats */
    classifier->stats.total_classifications++;
    if (result->is_harmful) {
        classifier->stats.harmful_detected++;
    } else {
        classifier->stats.safe_detected++;
    }

    /* Update average harm score (running average) */
    float n = (float)classifier->stats.total_classifications;
    classifier->stats.avg_harm_score =
        (classifier->stats.avg_harm_score * (n - 1.0f) + result->total_harm_score) / n;

    /* Update max harm score */
    if (result->total_harm_score > classifier->stats.max_harm_score) {
        classifier->stats.max_harm_score = result->total_harm_score;
    }

    /* Unlock */
    nimcp_platform_mutex_unlock(classifier->mutex);

    NIMCP_LOGGING_DEBUG("Classified action: harmful=%d, score=%.3f, harms=%u",
                        result->is_harmful, result->total_harm_score, result->harm_count);

    return 0;
}

int harm_classifier_classify_with_context(
    harm_classifier_t* classifier,
    const char* action_description,
    const char* context_description,
    harm_classification_t* result
) {
    /* Guard clauses */
    if (!classifier) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "classifier is NULL");

        return -1;

    }
    if (!action_description) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "action_description is NULL");

        return -1;

    }
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return -1;

    }

    /* If no context, fall back to basic classification */
    if (!context_description || !classifier->config.enable_context_analysis) {
        return harm_classifier_classify(classifier, action_description, NULL, 0, result);
    }

    /* Combine action and context for enhanced analysis */
    char combined[HARM_CLASSIFIER_MAX_ACTION_DESC * 2];
    snprintf(combined, sizeof(combined), "%s [Context: %s]",
             action_description, context_description);

    /* Lock for stats */
    nimcp_platform_mutex_lock(classifier->mutex);

    /* Classify combined text */
    int ret = harm_classifier_classify(classifier, combined, NULL, 0, result);

    /* Track context usage */
    if (ret == 0 && result->harm_count > 0) {
        classifier->stats.context_matches++;
    }

    nimcp_platform_mutex_unlock(classifier->mutex);

    return ret;
}

/* ============================================================================
 * Configuration API Implementation
 * ============================================================================ */

int harm_classifier_set_severity_weight(
    harm_classifier_t* classifier,
    harm_type_t harm_type,
    float weight
) {
    /* Guard clauses */
    if (!classifier) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "classifier is NULL");

        return -1;

    }
    if (harm_type >= HARM_TYPE_COUNT) return -1;
    if (weight < 0.0f || weight > 2.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "harm_classifier_set_severity_weight: validation failed");
        return -1;
    }

    nimcp_platform_mutex_lock(classifier->mutex);
    classifier->config.severity_weights[harm_type] = weight;
    nimcp_platform_mutex_unlock(classifier->mutex);

    NIMCP_LOGGING_INFO("Set %s severity weight to %.3f",
                       HARM_TYPE_NAMES[harm_type], weight);

    return 0;
}

float harm_classifier_get_severity_weight(
    const harm_classifier_t* classifier,
    harm_type_t harm_type
) {
    /* Guard clauses */
    if (!classifier) return -1.0f;
    if (harm_type >= HARM_TYPE_COUNT) return -1.0f;

    return classifier->config.severity_weights[harm_type];
}

int harm_classifier_set_threshold(
    harm_classifier_t* classifier,
    float threshold
) {
    /* Guard clauses */
    if (!classifier) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "classifier is NULL");

        return -1;

    }
    if (threshold < 0.0f || threshold > 1.0f) return -1;

    nimcp_platform_mutex_lock(classifier->mutex);
    classifier->config.harm_threshold = threshold;
    nimcp_platform_mutex_unlock(classifier->mutex);

    NIMCP_LOGGING_INFO("Set harm threshold to %.3f", threshold);

    return 0;
}

float harm_classifier_get_threshold(const harm_classifier_t* classifier) {
    /* Guard clause */
    if (!classifier) return -1.0f;

    return classifier->config.harm_threshold;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int harm_classifier_get_stats(
    const harm_classifier_t* classifier,
    harm_classifier_stats_t* stats
) {
    /* Guard clauses */
    if (!classifier) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "classifier is NULL");

        return -1;

    }
    if (!stats) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");

        return -1;

    }

    /* Copy stats (thread-safe read) */
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)classifier->mutex);
    *stats = classifier->stats;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)classifier->mutex);

    return 0;
}

int harm_classifier_reset_stats(harm_classifier_t* classifier) {
    /* Guard clause */
    if (!classifier) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "classifier is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(classifier->mutex);
    memset(&classifier->stats, 0, sizeof(harm_classifier_stats_t));
    nimcp_platform_mutex_unlock(classifier->mutex);

    NIMCP_LOGGING_INFO("Reset harm classifier statistics");

    return 0;
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

int harm_classifier_connect_bio_async(harm_classifier_t* classifier) {
    /* Guard clause */
    if (!classifier) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "classifier is NULL");

        return -1;

    }

    /* Already connected? */
    if (classifier->bio_async_enabled) {
        return 0;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_HARM_CLASSIFIER,
        .module_name = "harm_classifier",
        .inbox_capacity = 32,
        .user_data = classifier
    };

    classifier->bio_ctx = bio_router_register_module(&info);
    if (classifier->bio_ctx) {
        classifier->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "harm_classifier_connect_bio_async: validation failed");
    return -1;
}

int harm_classifier_disconnect_bio_async(harm_classifier_t* classifier) {
    /* Guard clause */
    if (!classifier) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "classifier is NULL");

        return -1;

    }

    /* Not connected? */
    if (!classifier->bio_async_enabled) {
        return 0;
    }

    /* Unregister */
    if (classifier->bio_ctx) {
        bio_router_unregister_module(classifier->bio_ctx);
        classifier->bio_ctx = NULL;
    }

    classifier->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool harm_classifier_is_bio_async_connected(const harm_classifier_t* classifier) {
    /* Guard clause */
    if (!classifier) {
        return false;
    }

    return classifier->bio_async_enabled;
}

/* ============================================================================
 * Utility API Implementation
 * ============================================================================ */

const char* harm_classifier_get_type_name(harm_type_t type) {
    /* Guard clause */
    if (type >= HARM_TYPE_COUNT) {
        return "UNKNOWN";
    }

    return HARM_TYPE_NAMES[type];
}

float harm_classifier_get_default_severity(harm_type_t type) {
    /* Guard clause */
    if (type >= HARM_TYPE_COUNT) {
        return -1.0f;
    }

    return DEFAULT_SEVERITIES[type];
}
