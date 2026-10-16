/**
 * @file nimcp_reciprocity_eval.c
 * @brief Golden Rule Reciprocity Evaluation Implementation
 * @version 1.0.0
 * @date 2025-12-16
 */

#include "core/directives/nimcp_reciprocity_eval.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(reciprocity_eval)

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Reciprocity evaluator implementation
 */
struct reciprocity_evaluator_struct {
    /* Configuration */
    reciprocity_config_t config;

    /* Statistics */
    reciprocity_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Compute string similarity (Levenshtein-based)
 *
 * WHAT: Calculate how similar two strings are
 * WHY:  Detect symmetric actions with minor variations
 * HOW:  Simple character-level comparison (simplified Levenshtein)
 */
static float compute_string_similarity(const char* s1, const char* s2) {
    if (!s1 || !s2) return 0.0f;

    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);

    if (len1 == 0 && len2 == 0) return 1.0f;
    if (len1 == 0 || len2 == 0) return 0.0f;

    /* Simple similarity: count matching characters */
    size_t matches = 0;
    size_t min_len = (len1 < len2) ? len1 : len2;

    for (size_t i = 0; i < min_len; i++) {
        if (tolower(s1[i]) == tolower(s2[i])) {
            matches++;
        }
    }

    /* Normalize by average length */
    float avg_len = (len1 + len2) / 2.0f;
    return matches / avg_len;
}

/**
 * @brief Detect harmful keywords in action
 *
 * WHAT: Check if action contains harmful terms
 * WHY:  Quick detection of obviously unacceptable actions
 * HOW:  Keyword matching against harm patterns
 */
static bool contains_harmful_keywords(const char* action) {
    if (!action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contains_harmful_keywords: action is NULL");
        return false;
    }

    /* Common harmful action patterns */
    const char* harmful_keywords[] = {
        "kill", "harm", "hurt", "steal", "fraud", "deceive",
        "manipulate", "exploit", "abuse", "violate", "attack",
        "destroy", "damage", "sabotage", "betray", NULL
    };

    /* Convert to lowercase for case-insensitive matching */
    char lower_action[RECIPROCITY_MAX_DESCRIPTION_LEN];
    strncpy(lower_action, action, RECIPROCITY_MAX_DESCRIPTION_LEN - 1);
    lower_action[RECIPROCITY_MAX_DESCRIPTION_LEN - 1] = '\0';

    for (size_t i = 0; i < strlen(lower_action); i++) {
        lower_action[i] = tolower(lower_action[i]);
    }

    /* Check for harmful keywords */
    for (int i = 0; harmful_keywords[i] != NULL; i++) {
        if (strstr(lower_action, harmful_keywords[i]) != NULL) {
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "contains_harmful_keywords: validation failed");
    return false;
}

/**
 * @brief Detect beneficial keywords in action
 *
 * WHAT: Check if action contains beneficial terms
 * WHY:  Quick detection of positive actions
 * HOW:  Keyword matching against benefit patterns
 */
static bool contains_beneficial_keywords(const char* action) {
    if (!action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contains_beneficial_keywords: action is NULL");
        return false;
    }

    /* Common beneficial action patterns */
    const char* beneficial_keywords[] = {
        "help", "assist", "support", "protect", "care",
        "heal", "improve", "benefit", "enhance", "share",
        "educate", "teach", "guide", "rescue", NULL
    };

    /* Convert to lowercase */
    char lower_action[RECIPROCITY_MAX_DESCRIPTION_LEN];
    strncpy(lower_action, action, RECIPROCITY_MAX_DESCRIPTION_LEN - 1);
    lower_action[RECIPROCITY_MAX_DESCRIPTION_LEN - 1] = '\0';

    for (size_t i = 0; i < strlen(lower_action); i++) {
        lower_action[i] = tolower(lower_action[i]);
    }

    /* Check for beneficial keywords */
    for (int i = 0; beneficial_keywords[i] != NULL; i++) {
        if (strstr(lower_action, beneficial_keywords[i]) != NULL) {
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "contains_beneficial_keywords: validation failed");
    return false;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int reciprocity_eval_default_config(reciprocity_config_t* config) {
    /* Guard: validate input */
    if (!config) {
        NIMCP_LOGGING_ERROR("reciprocity_eval_default_config: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reciprocity_eval_default_config: config is NULL");
        return -1;
    }

    /* Set defaults */
    config->symmetry_threshold = RECIPROCITY_DEFAULT_SYMMETRY_THRESHOLD;
    config->strict_mode = false;
    config->enable_perspective_taking = true;

    return 0;
}

reciprocity_evaluator_t reciprocity_eval_create(const reciprocity_config_t* config) {
    /* Allocate evaluator */
    reciprocity_evaluator_t eval = nimcp_malloc(sizeof(struct reciprocity_evaluator_struct));
    if (!eval) {
        NIMCP_LOGGING_ERROR("reciprocity_eval_create: malloc failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "eval is NULL");

        return NULL;
    }

    /* Initialize configuration */
    if (config) {
        memcpy(&eval->config, config, sizeof(reciprocity_config_t));
    } else {
        reciprocity_eval_default_config(&eval->config);
    }

    /* Initialize statistics */
    memset(&eval->stats, 0, sizeof(reciprocity_stats_t));

    /* Initialize bio-async */
    eval->bio_ctx = NULL;
    eval->bio_async_enabled = false;

    /* Initialize mutex */
    eval->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!eval->mutex) {
        NIMCP_LOGGING_ERROR("reciprocity_eval_create: mutex malloc failed");
        nimcp_free(eval);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reciprocity_eval_create: eval->mutex is NULL");
        return NULL;
    }

    if (nimcp_mutex_init(eval->mutex, NULL) != 0) {
        NIMCP_LOGGING_ERROR("reciprocity_eval_create: mutex init failed");
        nimcp_free(eval);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reciprocity_eval_create: validation failed");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Reciprocity evaluator created");
    return eval;
}

void reciprocity_eval_destroy(reciprocity_evaluator_t evaluator) {
    /* Guard: validate input */
    if (!evaluator) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (evaluator->bio_async_enabled) {
        reciprocity_eval_disconnect_bio_async(evaluator);
    }

    /* Destroy and free mutex */
    if (evaluator->mutex) {
        nimcp_mutex_destroy(evaluator->mutex);
        nimcp_free(evaluator->mutex);
    }

    /* Free evaluator */
    nimcp_free(evaluator);

    NIMCP_LOGGING_INFO("Reciprocity evaluator destroyed");
}

/* ============================================================================
 * Evaluation API
 * ============================================================================ */

int reciprocity_eval_check(
    reciprocity_evaluator_t evaluator,
    const char* action,
    const char* target,
    reciprocity_evaluation_t* evaluation
) {
    /* Guard: validate inputs */
    if (!evaluator || !action || !target || !evaluation) {
        NIMCP_LOGGING_ERROR("reciprocity_eval_check: NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reciprocity_eval_check: required parameter is NULL (evaluator, action, target, evaluation)");
        return -1;
    }

    /* Lock mutex */
    nimcp_mutex_lock(evaluator->mutex);

    /* Compute symmetry score */
    float symmetry = reciprocity_eval_get_symmetry_score(evaluator, action, target);
    evaluation->symmetry_score = symmetry;

    /* Generate reversed action */
    char reversed[RECIPROCITY_MAX_DESCRIPTION_LEN];
    reciprocity_eval_reverse_perspective(evaluator, action, reversed);

    /* Check acceptability */
    bool would_accept = reciprocity_eval_would_accept(evaluator, reversed);
    evaluation->would_accept_reversed = would_accept;

    /* Determine verdict */
    if (evaluator->config.strict_mode) {
        /* Strict mode: require perfect symmetry */
        if (symmetry >= 0.99f && would_accept) {
            evaluation->result = RECIPROCITY_PASS;
            snprintf(evaluation->explanation, RECIPROCITY_MAX_EXPLANATION_LEN,
                    "Perfect symmetry (%.2f) and acceptable when reversed", symmetry);
        } else {
            evaluation->result = RECIPROCITY_FAIL;
            snprintf(evaluation->explanation, RECIPROCITY_MAX_EXPLANATION_LEN,
                    "Strict mode: symmetry %.2f or not acceptable", symmetry);
        }
    } else {
        /* Normal mode: use threshold */
        if (symmetry >= evaluator->config.symmetry_threshold && would_accept) {
            evaluation->result = RECIPROCITY_PASS;
            snprintf(evaluation->explanation, RECIPROCITY_MAX_EXPLANATION_LEN,
                    "Good symmetry (%.2f) and acceptable when reversed", symmetry);
        } else if (symmetry < evaluator->config.symmetry_threshold * 0.5f || !would_accept) {
            evaluation->result = RECIPROCITY_FAIL;
            snprintf(evaluation->explanation, RECIPROCITY_MAX_EXPLANATION_LEN,
                    "Low symmetry (%.2f) or not acceptable when reversed", symmetry);
        } else {
            evaluation->result = RECIPROCITY_WARN;
            snprintf(evaluation->explanation, RECIPROCITY_MAX_EXPLANATION_LEN,
                    "Moderate symmetry (%.2f), uncertain acceptability", symmetry);
        }
    }

    /* Update statistics */
    evaluator->stats.total_evaluations++;
    evaluator->stats.average_symmetry =
        (evaluator->stats.average_symmetry * (evaluator->stats.total_evaluations - 1) + symmetry) /
        evaluator->stats.total_evaluations;

    switch (evaluation->result) {
        case RECIPROCITY_PASS:
            evaluator->stats.passes++;
            break;
        case RECIPROCITY_FAIL:
            evaluator->stats.failures++;
            break;
        case RECIPROCITY_WARN:
            evaluator->stats.warnings++;
            break;
        default:
            break;
    }

    /* Unlock mutex */
    nimcp_mutex_unlock(evaluator->mutex);

    return 0;
}

int reciprocity_eval_reverse_perspective(
    reciprocity_evaluator_t evaluator,
    const char* action,
    char* reversed_action
) {
    /* Guard: validate inputs */
    if (!evaluator || !action || !reversed_action) {
        NIMCP_LOGGING_ERROR("reciprocity_eval_reverse_perspective: NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reciprocity_eval_reverse_perspective: required parameter is NULL (evaluator, action, reversed_action)");
        return -1;
    }

    /* Simple perspective reversal: prepend "If someone did to me: " */
    snprintf(reversed_action, RECIPROCITY_MAX_DESCRIPTION_LEN,
            "If someone did to me: %s", action);

    return 0;
}

bool reciprocity_eval_would_accept(
    reciprocity_evaluator_t evaluator,
    const char* action_if_received
) {
    /* Guard: validate inputs */
    if (!evaluator || !action_if_received) {
        NIMCP_LOGGING_ERROR("reciprocity_eval_would_accept: NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reciprocity_eval_would_accept: required parameter is NULL (evaluator, action_if_received)");
        return false;
    }

    /* Check for harmful keywords - reject immediately */
    if (contains_harmful_keywords(action_if_received)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reciprocity_eval_would_accept: validation failed");
        return false;
    }

    /* Check for beneficial keywords - likely accept */
    if (contains_beneficial_keywords(action_if_received)) {
        return true;
    }

    /* Neutral actions - default to cautious rejection unless explicitly beneficial */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reciprocity_eval_would_accept: validation failed");
    return false;
}

float reciprocity_eval_get_symmetry_score(
    reciprocity_evaluator_t evaluator,
    const char* action,
    const char* target
) {
    /* Guard: validate inputs */
    if (!evaluator || !action || !target) {
        NIMCP_LOGGING_ERROR("reciprocity_eval_get_symmetry_score: NULL parameter");
        return 0.0f;
    }

    /* Generate reversed action */
    char reversed[RECIPROCITY_MAX_DESCRIPTION_LEN];
    snprintf(reversed, RECIPROCITY_MAX_DESCRIPTION_LEN,
            "Reverse of: %s (targeting: %s)", action, target);

    /* Compute string similarity between original and reversed */
    float similarity = compute_string_similarity(action, reversed);

    /* Check for power asymmetries (keywords indicating one-sided control) */
    bool has_power_asymmetry =
        (strstr(action, "access") != NULL) ||
        (strstr(action, "control") != NULL) ||
        (strstr(action, "monitor") != NULL) ||
        (strstr(action, "audit") != NULL);

    /* Reduce symmetry if power asymmetry detected */
    if (has_power_asymmetry) {
        similarity *= 0.6f;  /* 40% penalty for power asymmetry */
    }

    /* Check if action involves privacy */
    bool involves_privacy =
        (strstr(action, "private") != NULL) ||
        (strstr(action, "location") != NULL) ||
        (strstr(action, "data") != NULL) ||
        (strstr(action, "information") != NULL);

    /* Boost symmetry for privacy-neutral actions */
    if (!involves_privacy && contains_beneficial_keywords(action)) {
        similarity = fminf(1.0f, similarity * 1.2f);
    }

    return nimcp_clamp01(similarity);
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int reciprocity_eval_get_stats(
    reciprocity_evaluator_t evaluator,
    reciprocity_stats_t* stats
) {
    /* Guard: validate inputs */
    if (!evaluator || !stats) {
        NIMCP_LOGGING_ERROR("reciprocity_eval_get_stats: NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reciprocity_eval_get_stats: required parameter is NULL (evaluator, stats)");
        return -1;
    }

    /* Lock mutex */
    nimcp_mutex_lock(evaluator->mutex);

    /* Copy statistics */
    memcpy(stats, &evaluator->stats, sizeof(reciprocity_stats_t));

    /* Unlock mutex */
    nimcp_mutex_unlock(evaluator->mutex);

    return 0;
}

int reciprocity_eval_reset_stats(reciprocity_evaluator_t evaluator) {
    /* Guard: validate input */
    if (!evaluator) {
        NIMCP_LOGGING_ERROR("reciprocity_eval_reset_stats: NULL evaluator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reciprocity_eval_reset_stats: evaluator is NULL");
        return -1;
    }

    /* Lock mutex */
    nimcp_mutex_lock(evaluator->mutex);

    /* Reset statistics */
    memset(&evaluator->stats, 0, sizeof(reciprocity_stats_t));

    /* Unlock mutex */
    nimcp_mutex_unlock(evaluator->mutex);

    NIMCP_LOGGING_INFO("Reciprocity evaluator statistics reset");
    return 0;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int reciprocity_eval_connect_bio_async(reciprocity_evaluator_t evaluator) {
    /* Guard: validate input */
    if (!evaluator) {
        NIMCP_LOGGING_ERROR("reciprocity_eval_connect_bio_async: NULL evaluator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reciprocity_eval_connect_bio_async: evaluator is NULL");
        return -1;
    }

    /* Guard: check if already connected */
    if (evaluator->bio_async_enabled) {
        NIMCP_LOGGING_WARN("reciprocity_eval_connect_bio_async: already connected");
        return 0;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_RECIPROCITY_EVAL,
        .module_name = "reciprocity_evaluator",
        .inbox_capacity = 32,
        .user_data = evaluator
    };

    evaluator->bio_ctx = bio_router_register_module(&info);
    if (evaluator->bio_ctx) {
        evaluator->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Reciprocity evaluator connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reciprocity_eval_connect_bio_async: validation failed");
        return -1;
    }
}

int reciprocity_eval_disconnect_bio_async(reciprocity_evaluator_t evaluator) {
    /* Guard: validate input */
    if (!evaluator) {
        NIMCP_LOGGING_ERROR("reciprocity_eval_disconnect_bio_async: NULL evaluator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reciprocity_eval_disconnect_bio_async: evaluator is NULL");
        return -1;
    }

    /* Guard: check if connected */
    if (!evaluator->bio_async_enabled) {
        return 0;
    }

    /* Unregister from bio-async router */
    if (evaluator->bio_ctx) {
        bio_router_unregister_module(evaluator->bio_ctx);
        evaluator->bio_ctx = NULL;
    }

    evaluator->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Reciprocity evaluator disconnected from bio-async router");

    return 0;
}

bool reciprocity_eval_is_bio_async_connected(reciprocity_evaluator_t evaluator) {
    /* Guard: validate input */
    if (!evaluator) {
        return false;
    }

    return evaluator->bio_async_enabled;
}
