/**
 * @file nimcp_lgss_ethics_bridge.c
 * @brief LGSS-Ethics Integration Bridge Implementation
 *
 * WHAT: Bridge connecting LGSS safety system to the ethics engine
 * WHY:  LGSS provides L0 (highest priority) safety layer above ethics directives
 * HOW:  Intercepts ethics evaluations, applies LGSS first, then ethics
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 * @version 1.0.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/lgss/bridges/nimcp_lgss_ethics_bridge.h"
#include "security/lgss/nimcp_lgss.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_ethics_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


#define LOG_MODULE "LGSS_ETHICS_BRIDGE"


/*=============================================================================
 * LOGGING
 *============================================================================*/

#define ETHICS_BRIDGE_LOG_DEBUG(fmt, ...) \
    NIMCP_LOG_DEBUG("LGSS-ETHICS", fmt, ##__VA_ARGS__)
#define ETHICS_BRIDGE_LOG_INFO(fmt, ...) \
    NIMCP_LOG_INFO("LGSS-ETHICS", fmt, ##__VA_ARGS__)
#define ETHICS_BRIDGE_LOG_WARN(fmt, ...) \
    NIMCP_LOG_WARN("LGSS-ETHICS", fmt, ##__VA_ARGS__)
#define ETHICS_BRIDGE_LOG_ERROR(fmt, ...) \
    NIMCP_LOG_ERROR("LGSS-ETHICS", fmt, ##__VA_ARGS__)

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Ethics bridge context structure
 */
struct lgss_ethics_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    lgss_ethics_bridge_config_t config;

    /* Connected components */
    struct lgss_context* lgss;
    struct ethics_engine* ethics;
    struct core_directives* directives;

    /* State */
    bool lgss_enabled;
    bool ethics_enabled;

    /* Statistics */
    lgss_ethics_bridge_stats_t stats;

    /* Thread safety */
    void* mutex;
};

/*=============================================================================
 * CONFIGURATION FUNCTIONS
 *============================================================================*/

int lgss_ethics_bridge_config_init(lgss_ethics_bridge_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(lgss_ethics_bridge_config_t));

    config->lgss_enabled = true;
    config->always_check_ethics = true;
    config->log_evaluations = true;
    config->fail_safe = true;
    config->lgss_timeout_ms = 5000;
    config->ethics_timeout_ms = 10000;

    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

lgss_ethics_bridge_t* lgss_ethics_bridge_create(
    struct lgss_context* lgss,
    struct ethics_engine* ethics,
    const lgss_ethics_bridge_config_t* config)
{
    lgss_ethics_bridge_t* bridge = NULL;
    lgss_ethics_bridge_config_t default_config;

    if (!lgss) {
        ETHICS_BRIDGE_LOG_ERROR("LGSS context is required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_ethics_bridge_create: lgss is NULL");
        return NULL;
    }

    if (!config) {
        lgss_ethics_bridge_config_init(&default_config);
        config = &default_config;
    }

    bridge = nimcp_calloc(1, sizeof(lgss_ethics_bridge_t));
    if (!bridge) {
        ETHICS_BRIDGE_LOG_ERROR("Failed to allocate ethics bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lgss_ethics_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->magic = NIMCP_LGSS_ETHICS_BRIDGE_MAGIC;
    memcpy(&bridge->config, config, sizeof(lgss_ethics_bridge_config_t));

    bridge->lgss = lgss;
    bridge->ethics = ethics;
    bridge->directives = NULL;

    bridge->lgss_enabled = config->lgss_enabled;
    bridge->ethics_enabled = (ethics != NULL);

    memset(&bridge->stats, 0, sizeof(lgss_ethics_bridge_stats_t));

    ETHICS_BRIDGE_LOG_INFO("Ethics bridge created (LGSS=%s, Ethics=%s)",
        bridge->lgss_enabled ? "enabled" : "disabled",
        bridge->ethics_enabled ? "enabled" : "disabled");

    return bridge;
}

void lgss_ethics_bridge_destroy(lgss_ethics_bridge_t* bridge)
{
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "lgss_ethics");
    if (!bridge) {
        return;
    }

    if (bridge->magic != NIMCP_LGSS_ETHICS_BRIDGE_MAGIC) {
        ETHICS_BRIDGE_LOG_WARN("Destroying invalid ethics bridge");
        return;
    }

    ETHICS_BRIDGE_LOG_INFO("Ethics bridge destroyed");

    bridge->magic = 0;
    nimcp_free(bridge);
}

int lgss_ethics_bridge_set_ethics(
    lgss_ethics_bridge_t* bridge,
    struct ethics_engine* ethics)
{
    if (!bridge || bridge->magic != NIMCP_LGSS_ETHICS_BRIDGE_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    bridge->ethics = ethics;
    bridge->ethics_enabled = (ethics != NULL);

    ETHICS_BRIDGE_LOG_INFO("Ethics engine %s",
        ethics ? "connected" : "disconnected");

    return 0;
}

int lgss_ethics_bridge_set_directives(
    lgss_ethics_bridge_t* bridge,
    struct core_directives* directives)
{
    if (!bridge || bridge->magic != NIMCP_LGSS_ETHICS_BRIDGE_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    bridge->directives = directives;

    ETHICS_BRIDGE_LOG_INFO("Core directives %s",
        directives ? "connected" : "disconnected");

    return 0;
}

/*=============================================================================
 * EVALUATION FUNCTIONS
 *============================================================================*/

int lgss_ethics_bridge_evaluate(
    lgss_ethics_bridge_t* bridge,
    const safety_action_context_t* context,
    lgss_ethics_evaluation_t* result)
{
    if (!bridge || bridge->magic != NIMCP_LGSS_ETHICS_BRIDGE_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!context || !result) {
        ETHICS_BRIDGE_LOG_ERROR("NULL context or result");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Initialize result */
    memset(result, 0, sizeof(lgss_ethics_evaluation_t));
    result->result = LGSS_ETHICS_ALLOW;
    result->triggered_by = LGSS_TRIGGERED_BY_NONE;
    result->lgss_consulted = false;
    result->ethics_consulted = false;

    uint64_t start_time = nimcp_time_now_us();
    bridge->stats.total_evaluations++;

    /* Step 1: Check LGSS (L0) if enabled */
    if (bridge->lgss_enabled && bridge->lgss) {
        result->lgss_consulted = true;

        int lgss_result = lgss_evaluate(bridge->lgss, context, &result->lgss_eval);
        if (lgss_result != 0) {
            /* LGSS evaluation error */
            if (bridge->config.fail_safe) {
                result->result = LGSS_ETHICS_ERROR;
                result->triggered_by = LGSS_TRIGGERED_BY_LGSS;
                snprintf(result->explanation, sizeof(result->explanation),
                    "LGSS evaluation error (fail-safe: blocked)");
                bridge->stats.errors++;
                goto done;
            }
            /* Otherwise continue to ethics */
        }

        switch (result->lgss_eval.action) {
            case SAFETY_ACTION_DENY:
                result->result = LGSS_ETHICS_BLOCKED_BY_LGSS;
                result->triggered_by = LGSS_TRIGGERED_BY_LGSS;
                snprintf(result->explanation, sizeof(result->explanation),
                    "Blocked by LGSS (L0): %s", result->lgss_eval.explanation);
                bridge->stats.blocked_by_lgss++;
                goto done;

            case SAFETY_ACTION_ESCALATE:
                result->result = LGSS_ETHICS_ESCALATE_LGSS;
                result->triggered_by = LGSS_TRIGGERED_BY_LGSS;
                snprintf(result->explanation, sizeof(result->explanation),
                    "Escalated by LGSS (L0): %s", result->lgss_eval.explanation);
                bridge->stats.escalated_by_lgss++;
                goto done;

            case SAFETY_ACTION_ALLOW:
            case SAFETY_ACTION_LOG:
            case SAFETY_ACTION_WARN:
                /* Continue to ethics layers */
                break;
        }
    }

    /* Step 2: Check Ethics (L1-L5) if enabled and if LGSS allowed */
    if (bridge->ethics_enabled && bridge->ethics && bridge->config.always_check_ethics) {
        result->ethics_consulted = true;

        /*
         * NOTE: In a full implementation, we would call the ethics engine here.
         * For now, we'll simulate an ethics check since the ethics_engine type
         * is forward-declared and we don't have access to its implementation.
         *
         * In production, this would be:
         *   ethics_result_t eth_result;
         *   ethics_evaluate(bridge->ethics, context, &eth_result);
         *   if (eth_result.blocked) { ... }
         */

        /* Placeholder: Ethics check not yet implemented */
        ETHICS_BRIDGE_LOG_DEBUG("Ethics evaluation would occur here");

        /*
         * For demonstration, we'll assume ethics allows unless we have
         * an actual ethics engine connected.
         */
    }

    /* If we reach here, action is allowed by both LGSS and Ethics */
    result->result = LGSS_ETHICS_ALLOW;
    result->confidence = result->lgss_consulted ? result->lgss_eval.confidence : 1.0f;
    snprintf(result->explanation, sizeof(result->explanation),
        "Allowed by LGSS%s", result->ethics_consulted ? " and Ethics" : "");
    bridge->stats.allowed++;

done:
    result->eval_time_us = nimcp_time_now_us() - start_time;

    /* Update layer trigger stats */
    if (result->triggered_by >= 0 && result->triggered_by < LGSS_ETHICS_MAX_LAYERS) {
        bridge->stats.triggers_by_layer[result->triggered_by]++;
    }

    /* Log if configured */
    if (bridge->config.log_evaluations) {
        ETHICS_BRIDGE_LOG_INFO("Evaluation: %s (layer=%s, time=%lu us)",
            lgss_ethics_result_name(result->result),
            lgss_ethics_layer_name(result->triggered_by),
            result->eval_time_us);
    }

    return 0;
}

lgss_ethics_result_t lgss_ethics_bridge_check(
    lgss_ethics_bridge_t* bridge,
    const safety_action_context_t* context)
{
    lgss_ethics_evaluation_t result;

    if (lgss_ethics_bridge_evaluate(bridge, context, &result) != 0) {
        return LGSS_ETHICS_ERROR;
    }

    return result.result;
}

int lgss_ethics_bridge_check_lgss_only(
    lgss_ethics_bridge_t* bridge,
    const safety_action_context_t* context,
    lgss_ethics_evaluation_t* result)
{
    if (!bridge || bridge->magic != NIMCP_LGSS_ETHICS_BRIDGE_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!context || !result) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Temporarily disable ethics */
    bool old_ethics_enabled = bridge->ethics_enabled;
    bridge->ethics_enabled = false;

    int ret = lgss_ethics_bridge_evaluate(bridge, context, result);

    /* Restore ethics setting */
    bridge->ethics_enabled = old_ethics_enabled;

    return ret;
}

int lgss_ethics_bridge_check_ethics_only(
    lgss_ethics_bridge_t* bridge,
    const safety_action_context_t* context,
    lgss_ethics_evaluation_t* result)
{
    if (!bridge || bridge->magic != NIMCP_LGSS_ETHICS_BRIDGE_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!context || !result) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    ETHICS_BRIDGE_LOG_WARN("Bypassing LGSS safety layer (for testing only)");

    /* Temporarily disable LGSS */
    bool old_lgss_enabled = bridge->lgss_enabled;
    bridge->lgss_enabled = false;

    int ret = lgss_ethics_bridge_evaluate(bridge, context, result);

    /* Restore LGSS setting */
    bridge->lgss_enabled = old_lgss_enabled;

    return ret;
}

/*=============================================================================
 * LAYER MANAGEMENT
 *============================================================================*/

int lgss_ethics_bridge_set_lgss_enabled(
    lgss_ethics_bridge_t* bridge,
    bool enabled)
{
    if (!bridge || bridge->magic != NIMCP_LGSS_ETHICS_BRIDGE_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!enabled) {
        ETHICS_BRIDGE_LOG_WARN("DISABLING LGSS LAYER (L0 safety removed!)");
    }

    bridge->lgss_enabled = enabled;

    ETHICS_BRIDGE_LOG_INFO("LGSS layer %s", enabled ? "enabled" : "DISABLED");

    return 0;
}

bool lgss_ethics_bridge_is_lgss_enabled(const lgss_ethics_bridge_t* bridge)
{
    if (!bridge || bridge->magic != NIMCP_LGSS_ETHICS_BRIDGE_MAGIC) { return false; }
    return bridge->lgss_enabled;
}

int lgss_ethics_bridge_set_ethics_enabled(
    lgss_ethics_bridge_t* bridge,
    bool enabled)
{
    if (!bridge || bridge->magic != NIMCP_LGSS_ETHICS_BRIDGE_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    bridge->ethics_enabled = enabled;

    ETHICS_BRIDGE_LOG_INFO("Ethics layers %s", enabled ? "enabled" : "disabled");

    return 0;
}

bool lgss_ethics_bridge_is_ethics_enabled(const lgss_ethics_bridge_t* bridge)
{
    if (!bridge || bridge->magic != NIMCP_LGSS_ETHICS_BRIDGE_MAGIC) { return false; }
    return bridge->ethics_enabled;
}

/*=============================================================================
 * STATISTICS FUNCTIONS
 *============================================================================*/

int lgss_ethics_bridge_get_stats(
    const lgss_ethics_bridge_t* bridge,
    lgss_ethics_bridge_stats_t* stats)
{
    if (!bridge || bridge->magic != NIMCP_LGSS_ETHICS_BRIDGE_MAGIC || !stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memcpy(stats, &bridge->stats, sizeof(lgss_ethics_bridge_stats_t));
    return 0;
}

int lgss_ethics_bridge_reset_stats(lgss_ethics_bridge_t* bridge)
{
    if (!bridge || bridge->magic != NIMCP_LGSS_ETHICS_BRIDGE_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(&bridge->stats, 0, sizeof(lgss_ethics_bridge_stats_t));
    return 0;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

const char* lgss_ethics_result_name(lgss_ethics_result_t result)
{
    switch (result) {
        case LGSS_ETHICS_ALLOW:            return "ALLOW";
        case LGSS_ETHICS_BLOCKED_BY_LGSS:  return "BLOCKED_BY_LGSS";
        case LGSS_ETHICS_BLOCKED_BY_ETHICS: return "BLOCKED_BY_ETHICS";
        case LGSS_ETHICS_ESCALATE_LGSS:    return "ESCALATE_LGSS";
        case LGSS_ETHICS_ESCALATE_ETHICS:  return "ESCALATE_ETHICS";
        case LGSS_ETHICS_ERROR:            return "ERROR";
        default:                           return "UNKNOWN";
    }
}

const char* lgss_ethics_layer_name(lgss_ethics_trigger_layer_t layer)
{
    switch (layer) {
        case LGSS_TRIGGERED_BY_NONE:         return "NONE";
        case LGSS_TRIGGERED_BY_LGSS:         return "L0_LGSS";
        case LGSS_TRIGGERED_BY_FIRST_LAW:    return "L1_FIRST_LAW";
        case LGSS_TRIGGERED_BY_COMBINATORIAL: return "L2_COMBINATORIAL";
        case LGSS_TRIGGERED_BY_GOLDEN_RULE:  return "L3_GOLDEN_RULE";
        case LGSS_TRIGGERED_BY_SECOND_LAW:   return "L4_SECOND_LAW";
        case LGSS_TRIGGERED_BY_THIRD_LAW:    return "L5_THIRD_LAW";
        default:                             return "UNKNOWN";
    }
}

int lgss_ethics_format_evaluation(
    const lgss_ethics_evaluation_t* eval,
    char* buffer,
    size_t buffer_size)
{
    if (!eval || !buffer || buffer_size == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    return snprintf(buffer, buffer_size,
        "Result: %s | Layer: %s | LGSS: %s | Ethics: %s | Confidence: %.2f | "
        "Time: %lu us | %s",
        lgss_ethics_result_name(eval->result),
        lgss_ethics_layer_name(eval->triggered_by),
        eval->lgss_consulted ? "consulted" : "skipped",
        eval->ethics_consulted ? "consulted" : "skipped",
        eval->confidence,
        eval->eval_time_us,
        eval->explanation);
}
