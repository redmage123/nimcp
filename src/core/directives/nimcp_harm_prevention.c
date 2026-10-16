/**
 * @file nimcp_harm_prevention.c
 * @brief Asimov's First Law - Harm Prevention System Implementation
 * @version 1.0.0
 * @date 2025-12-16
 *
 * WHAT: Implementation of First Law harm prevention system
 * WHY:  Prevent harm through action evaluation and inaction detection
 * HOW:  Thread-safe harm checking with pluggable classifier
 */

#include "core/directives/nimcp_harm_prevention.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(harm_prevention)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Harm prevention system internal structure
 *
 * WHAT: Complete state for harm prevention system
 * WHY:  Encapsulate all data needed for thread-safe operation
 * HOW:  Mutex-protected access to classifier and statistics
 */
struct harm_prevention_system {
    /* Configuration */
    harm_prevention_config_t config;

    /* Harm classifier */
    harm_classifier_fn classifier;
    void* classifier_user_data;

    /* Statistics */
    harm_prevention_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Thread safety */
    void* mutex;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute harm decision from score
 *
 * WHAT: Convert harm score to decision type
 * WHY:  Centralize decision logic
 * HOW:  Compare score to thresholds
 */
static harm_decision_t compute_decision(
    float harm_score,
    float block_threshold,
    float warn_threshold
)
{
    /* Guard clauses */
    if (harm_score >= block_threshold) {
        return HARM_DECISION_BLOCK;
    }
    if (harm_score >= warn_threshold) {
        return HARM_DECISION_WARN;
    }
    return HARM_DECISION_ALLOW;
}

/**
 * @brief Check if decision violates First Law
 *
 * WHAT: Determine if allowing action would violate First Law
 * WHY:  Track First Law violations for auditing
 * HOW:  Block/escalate decisions indicate violations
 */
static bool is_first_law_violation(harm_decision_t decision)
{
    return (decision == HARM_DECISION_BLOCK ||
            decision == HARM_DECISION_ESCALATE);
}

/**
 * @brief Update running statistics
 *
 * WHAT: Update stats with new evaluation result
 * WHY:  Maintain accurate statistics for monitoring
 * HOW:  Increment counters, update averages
 */
static void update_stats(
    harm_prevention_stats_t* stats,
    const harm_prevention_result_t* result
)
{
    /* Guard clause */
    if (!stats || !result) {
        return;
    }

    /* Update counts */
    stats->total_evaluations++;

    switch (result->decision) {
        case HARM_DECISION_ALLOW:
            stats->allowed_count++;
            break;
        case HARM_DECISION_WARN:
            stats->warned_count++;
            break;
        case HARM_DECISION_BLOCK:
            stats->blocked_count++;
            break;
        case HARM_DECISION_ESCALATE:
            stats->escalated_count++;
            break;
    }

    /* Update First Law violations */
    if (result->first_law_violated) {
        stats->first_law_violations++;
    }

    /* Update harm score statistics */
    if (result->harm_score > stats->max_harm_score) {
        stats->max_harm_score = result->harm_score;
    }

    /* Update running average (incremental formula) */
    float delta = result->harm_score - stats->avg_harm_score;
    stats->avg_harm_score += delta / (float)stats->total_evaluations;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Provide default configuration
 * WHY:  Easy initialization with safe defaults
 * HOW:  Set evidence-based thresholds and enable all features
 */
int harm_prevention_default_config(harm_prevention_config_t* config)
{
    /* Guard clause */
    if (!config) {
        NIMCP_LOGGING_ERROR("Null config pointer");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Set defaults */
    config->block_threshold = HARM_PREVENTION_DEFAULT_BLOCK_THRESHOLD;
    config->warn_threshold = HARM_PREVENTION_DEFAULT_WARN_THRESHOLD;
    config->enable_human_escalation = true;
    config->enable_inaction_detection = true;
    config->escalation_callback = NULL;
    config->callback_user_data = NULL;

    return NIMCP_SUCCESS;
}

/**
 * WHAT: Create harm prevention system
 * WHY:  Initialize First Law enforcement
 * HOW:  Allocate, configure, validate classifier
 */
harm_prevention_system_t* harm_prevention_create(
    const harm_prevention_config_t* config,
    harm_classifier_fn classifier,
    void* classifier_user_data
)
{
    /* Guard clause - classifier is required */
    if (!classifier) {
        NIMCP_LOGGING_ERROR("Null classifier function");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "classifier is NULL");

        return NULL;
    }

    /* Allocate system */
    harm_prevention_system_t* system = nimcp_calloc(1, sizeof(*system));
    if (!system) {
        NIMCP_LOGGING_ERROR("Failed to allocate harm prevention system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "system is NULL");

        return NULL;
    }

    /* Set configuration */
    if (config) {
        system->config = *config;
    } else {
        harm_prevention_default_config(&system->config);
    }

    /* Validate thresholds */
    if (system->config.warn_threshold > system->config.block_threshold) {
        NIMCP_LOGGING_WARN("Warn threshold > block threshold, swapping");
        float temp = system->config.warn_threshold;
        system->config.warn_threshold = system->config.block_threshold;
        system->config.block_threshold = temp;
    }

    /* Set classifier */
    system->classifier = classifier;
    system->classifier_user_data = classifier_user_data;

    /* Initialize statistics */
    memset(&system->stats, 0, sizeof(system->stats));

    /* Initialize bio-async */
    system->bio_async_enabled = false;

    /* Note: mutex intentionally left NULL (void*) for portability */
    /* Actual mutex implementation would be platform-specific */

    NIMCP_LOGGING_INFO("Harm prevention system created");

    return system;
}

/**
 * WHAT: Destroy harm prevention system
 * WHY:  Proper cleanup on shutdown
 * HOW:  Disconnect bio-async, free memory
 */
void harm_prevention_destroy(harm_prevention_system_t* system)
{
    /* Guard clause */
    if (!system) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (system->bio_async_enabled) {
        harm_prevention_disconnect_bio_async(system);
    }

    NIMCP_LOGGING_INFO("Destroying harm prevention system (blocked=%lu)",
                       system->stats.blocked_count);

    /* Free system */
    nimcp_free(system);
}

/* ============================================================================
 * Action Evaluation Implementation
 * ============================================================================ */

/**
 * WHAT: Evaluate action for harm
 * WHY:  Prevent harmful actions
 * HOW:  Classify harm, decide, update stats
 */
int harm_prevention_evaluate_action(
    harm_prevention_system_t* system,
    const char* action_desc,
    const void* action_data,
    size_t action_data_len,
    harm_prevention_result_t* result
)
{
    /* Guard clauses */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!action_desc || !result) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->timestamp_ms = nimcp_time_get_ms();

    /* Invoke harm classifier */
    float harm_score = system->classifier(
        action_desc,
        action_data,
        action_data_len,
        system->classifier_user_data
    );

    /* Check for classifier error */
    if (harm_score < 0.0f) {
        NIMCP_LOGGING_ERROR("Harm classifier failed");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "invalid state");
    }

    /* Clamp harm score to [0.0, 1.0] */
    if (harm_score > 1.0f) {
        harm_score = 1.0f;
    }

    result->harm_score = harm_score;

    /* Compute decision */
    result->decision = compute_decision(
        harm_score,
        system->config.block_threshold,
        system->config.warn_threshold
    );

    /* Check for First Law violation */
    result->first_law_violated = is_first_law_violation(result->decision);

    /* Determine if human review required */
    result->requires_human_review = (
        (result->decision == HARM_DECISION_BLOCK ||
         result->decision == HARM_DECISION_ESCALATE) &&
        system->config.enable_human_escalation
    );

    /* Generate reason string */
    snprintf(result->reason, HARM_REASON_MAX_LEN,
             "Harm score: %.3f, Decision: %s",
             harm_score,
             result->decision == HARM_DECISION_ALLOW ? "ALLOW" :
             result->decision == HARM_DECISION_WARN ? "WARN" :
             result->decision == HARM_DECISION_BLOCK ? "BLOCK" : "ESCALATE");

    /* Update statistics */
    update_stats(&system->stats, result);

    /* Invoke escalation callback if needed */
    if (result->requires_human_review && system->config.escalation_callback) {
        system->config.escalation_callback(
            result,
            system->config.callback_user_data
        );
    }

    /* Log decision */
    if (result->decision == HARM_DECISION_BLOCK) {
        NIMCP_LOGGING_WARN("BLOCKED: %s (harm=%.3f)",
                           action_desc, harm_score);
    } else if (result->decision == HARM_DECISION_WARN) {
        NIMCP_LOGGING_INFO("WARNING: %s (harm=%.3f)",
                           action_desc, harm_score);
    }

    return NIMCP_SUCCESS;
}

/**
 * WHAT: Evaluate inaction for harm
 * WHY:  Detect harm through inaction (First Law clause 2)
 * HOW:  Classify context as "inaction scenario", decide
 */
int harm_prevention_evaluate_inaction(
    harm_prevention_system_t* system,
    const char* context_desc,
    const char* required_action,
    harm_prevention_result_t* result
)
{
    /* Guard clauses */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!context_desc || !required_action || !result) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    /* Check if inaction detection enabled */
    if (!system->config.enable_inaction_detection) {
        result->decision = HARM_DECISION_ALLOW;
        result->harm_score = 0.0f;
        return NIMCP_SUCCESS;
    }

    /* Build inaction description for classifier */
    char inaction_desc[NIMCP_ERROR_BUFFER_LARGE];
    snprintf(inaction_desc, sizeof(inaction_desc),
             "INACTION: %s. Required action: %s",
             context_desc, required_action);

    /* Evaluate using standard action evaluation */
    int ret = harm_prevention_evaluate_action(
        system,
        inaction_desc,
        NULL,
        0,
        result
    );

    if (ret == NIMCP_SUCCESS) {
        system->stats.inaction_detections++;
    }

    return ret;
}

/**
 * WHAT: Block action with reason
 * WHY:  Manual policy-based blocking
 * HOW:  Create synthetic block result, update stats
 */
int harm_prevention_block_action(
    harm_prevention_system_t* system,
    uint32_t action_id,
    const char* reason
)
{
    /* Guard clauses */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!reason) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    /* Create synthetic result */
    harm_prevention_result_t result;
    memset(&result, 0, sizeof(result));
    result.decision = HARM_DECISION_BLOCK;
    result.harm_score = 1.0f;  /* Maximum harm for manual block */
    result.timestamp_ms = nimcp_time_get_ms();
    result.first_law_violated = true;
    result.requires_human_review = false;

    snprintf(result.reason, HARM_REASON_MAX_LEN,
             "Manual block: %s", reason);

    /* Update statistics */
    update_stats(&system->stats, &result);

    NIMCP_LOGGING_WARN("Manually blocked action %u: %s", action_id, reason);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

/**
 * WHAT: Register escalation callback
 * WHY:  Enable human notification
 * HOW:  Store callback in config
 */
int harm_prevention_register_escalation_callback(
    harm_prevention_system_t* system,
    harm_escalation_callback_fn callback,
    void* user_data
)
{
    /* Guard clause */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    system->config.escalation_callback = callback;
    system->config.callback_user_data = user_data;

    NIMCP_LOGGING_INFO("Escalation callback registered");

    return NIMCP_SUCCESS;
}

/**
 * WHAT: Update harm thresholds
 * WHY:  Runtime tuning of sensitivity
 * HOW:  Validate and update thresholds
 */
int harm_prevention_update_thresholds(
    harm_prevention_system_t* system,
    float block_threshold,
    float warn_threshold
)
{
    /* Guard clauses */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (block_threshold < 0.0f || block_threshold > 1.0f ||
        warn_threshold < 0.0f || warn_threshold > 1.0f) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "out of range");
    }
    if (warn_threshold > block_threshold) {
        NIMCP_LOGGING_ERROR("Warn threshold must be <= block threshold");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    system->config.block_threshold = block_threshold;
    system->config.warn_threshold = warn_threshold;

    NIMCP_LOGGING_INFO("Updated thresholds: block=%.3f, warn=%.3f",
                       block_threshold, warn_threshold);

    return NIMCP_SUCCESS;
}

/**
 * WHAT: Enable/disable inaction detection
 * WHY:  Toggle inaction monitoring
 * HOW:  Set config flag
 */
int harm_prevention_enable_inaction_detection(
    harm_prevention_system_t* system,
    bool enable
)
{
    /* Guard clause */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    system->config.enable_inaction_detection = enable;

    NIMCP_LOGGING_INFO("Inaction detection %s",
                       enable ? "enabled" : "disabled");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

/**
 * WHAT: Get blocked action count
 * WHY:  Monitor effectiveness
 * HOW:  Return counter
 */
uint64_t harm_prevention_get_blocked_count(
    const harm_prevention_system_t* system
)
{
    /* Guard clause */
    if (!system) {
        return 0;
    }

    return system->stats.blocked_count;
}

/**
 * WHAT: Get comprehensive statistics
 * WHY:  Audit trail and monitoring
 * HOW:  Copy stats struct
 */
int harm_prevention_get_stats(
    const harm_prevention_system_t* system,
    harm_prevention_stats_t* stats
)
{
    /* Guard clauses */
    if (!system || !stats) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    *stats = system->stats;

    return NIMCP_SUCCESS;
}

/**
 * WHAT: Check if action would be blocked (no side effects)
 * WHY:  Pre-check without updating stats
 * HOW:  Classify and compare, don't update
 */
bool harm_prevention_would_block(
    const harm_prevention_system_t* system,
    const char* action_desc,
    const void* action_data,
    size_t action_data_len
)
{
    /* Guard clauses */
    if (!system || !action_desc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "harm_prevention_would_block: required parameter is NULL (system, action_desc)");
        return false;
    }

    /* Invoke classifier */
    float harm_score = system->classifier(
        action_desc,
        action_data,
        action_data_len,
        system->classifier_user_data
    );

    /* Check for error */
    if (harm_score < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "harm_prevention_would_block: validation failed");
        return false;
    }

    /* Compare to block threshold */
    return (harm_score >= system->config.block_threshold);
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable inter-module harm signaling
 * HOW:  Register with BIO_MODULE_HARM_PREVENTION
 */
int harm_prevention_connect_bio_async(
    harm_prevention_system_t* system
)
{
    /* Guard clause */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Check if already connected */
    if (system->bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_HARM_PREVENTION,
        .module_name = "harm_prevention",
        .inbox_capacity = 32,
        .user_data = system
    };

    system->bio_ctx = bio_router_register_module(&info);
    if (system->bio_ctx) {
        system->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available");
    }

    return NIMCP_SUCCESS;
}

/**
 * WHAT: Disconnect from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister from router
 */
int harm_prevention_disconnect_bio_async(
    harm_prevention_system_t* system
)
{
    /* Guard clause */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Check if connected */
    if (!system->bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    /* Unregister from router */
    if (system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
    }

    system->bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

/**
 * WHAT: Check bio-async connection status
 * WHY:  Verify messaging capability
 * HOW:  Return enabled flag
 */
bool harm_prevention_is_bio_async_connected(
    const harm_prevention_system_t* system
)
{
    /* Guard clause */
    if (!system) {
        return false;
    }

    return system->bio_async_enabled;
}
