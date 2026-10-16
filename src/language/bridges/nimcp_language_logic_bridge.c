#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_logic_bridge.c - Language-Logic Bridge Implementation
//=============================================================================
/**
 * @file nimcp_language_logic_bridge.c
 * @brief Implementation of Language-Logic bridge for symbolic reasoning
 *
 * WHAT: Bridge connecting language layer with symbolic logic module
 * WHY:  Enable logical inference, entailment, and consistency checking
 * HOW:  Translate language structures to logical forms for reasoning
 *
 * @version 1.0.0 - Phase L8: Additional Integration Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#include "language/bridges/nimcp_language_logic_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>

#define LOG_MODULE "LANG_LOGIC_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_logic_bridge)

//=============================================================================
// Configuration API Implementation
//=============================================================================

static void logic_default_config_internal(language_logic_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(language_logic_config_t));

    config->enable_entailment_checking = true;
    config->enable_consistency_checking = true;
    config->enable_presupposition_check = true;
    config->enable_reference_resolution = true;
    config->enable_implicature_reasoning = false;

    config->max_inference_depth = 5;
    config->default_timeout_ms = 100;
    config->min_confidence_threshold = 0.7f;

    config->enable_caching = true;
    config->cache_size = 256;

    config->enable_bio_async = false;
    config->update_interval_ms = 50;
}

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* language_logic_operation_to_string(language_logic_operation_t op) {
    static const char* names[] = {
        "ENTAILMENT",
        "CONTRADICTION",
        "CONSISTENCY",
        "IMPLICATION",
        "PRESUPPOSITION",
        "REFERENCE",
        "INFERENCE"
    };
    if (op >= LANG_LOGIC_COUNT) return "UNKNOWN";
    return names[op];
}

const char* language_logic_result_to_string(language_logic_result_t result) {
    static const char* names[] = {
        "TRUE",
        "FALSE",
        "UNKNOWN",
        "INCONSISTENT",
        "TIMEOUT",
        "ERROR"
    };
    if (result > LANG_LOGIC_RESULT_ERROR) return "UNKNOWN";
    return names[result];
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

language_logic_bridge_t* language_logic_bridge_create(
    language_orchestrator_t* orchestrator,
    const language_logic_config_t* config)
{
    language_logic_bridge_t* bridge = (language_logic_bridge_t*)
        nimcp_calloc(1, sizeof(language_logic_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(language_logic_config_t));
    } else {
        logic_default_config_internal(&bridge->config);
    }

    bridge->orchestrator = orchestrator;
    bridge->logic_engine = NULL;

    /* Initialize discourse state */
    memset(&bridge->discourse_state, 0, sizeof(language_discourse_state_t));
    bridge->discourse_state.is_consistent = true;
    bridge->discourse_state.coherence_score = 1.0f;

    memset(&bridge->stats, 0, sizeof(language_logic_stats_t));
    bridge->last_update_us = 0;
    bridge->initialized = true;
    bridge->active = false;

    LOG_INFO(LOG_MODULE, "Logic bridge created");
    return bridge;
}

void language_logic_bridge_destroy(language_logic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_logic");

    nimcp_free(bridge);
    LOG_INFO(LOG_MODULE, "Logic bridge destroyed");
}

int language_logic_bridge_connect_logic_engine(
    language_logic_bridge_t* bridge,
    symbolic_logic_t* logic_engine)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->logic_engine = logic_engine;
    return 0;
}

//=============================================================================
// Inference API Implementation
//=============================================================================

int language_logic_bridge_check_entailment(
    language_logic_bridge_t* bridge,
    const char* premise,
    const char* hypothesis,
    language_logic_inference_t* result)
{
    if (!bridge || !premise || !hypothesis || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_check_entailment: required parameter is NULL (bridge, premise, hypothesis, result)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_check_entailment: bridge->active is NULL");
        return -1;
    }
    if (!bridge->config.enable_entailment_checking) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_check_entailment: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(language_logic_inference_t));

    /* Simple lexical overlap heuristic (would use real logic engine) */
    float similarity = 0.0f;
    if (strstr(premise, hypothesis) != NULL ||
        strstr(hypothesis, premise) != NULL) {
        similarity = 0.8f;
        result->result = LANG_LOGIC_RESULT_TRUE;
    } else {
        similarity = 0.3f;
        result->result = LANG_LOGIC_RESULT_UNKNOWN;
    }

    result->confidence = similarity;
    result->inference_steps = 1;
    result->time_ms = 1;
    result->has_proof = false;
    strncpy(result->explanation, "Lexical overlap heuristic",
            sizeof(result->explanation) - 1);

    bridge->stats.entailment_checks++;

    /* Update average confidence */
    float n = (float)(bridge->stats.entailment_checks + bridge->stats.consistency_checks);
    bridge->stats.avg_confidence =
        ((n - 1) * bridge->stats.avg_confidence + result->confidence) / n;

    return 0;
}

int language_logic_bridge_check_consistency(
    language_logic_bridge_t* bridge,
    const char** statements,
    uint32_t count,
    language_logic_inference_t* result)
{
    if (!bridge || !statements || count == 0 || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_check_consistency: required parameter is NULL (bridge, statements, result)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_check_consistency: bridge->active is NULL");
        return -1;
    }
    if (!bridge->config.enable_consistency_checking) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_check_consistency: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(language_logic_inference_t));

    /* Simple negation check (would use real logic engine) */
    bool found_contradiction = false;
    for (uint32_t i = 0; i < count && !found_contradiction; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            /* Check for simple negation pattern */
            if ((strstr(statements[i], "not") != NULL) !=
                (strstr(statements[j], "not") != NULL)) {
                /* One has negation, one doesn't - might be contradiction */
                found_contradiction = true;
                break;
            }
        }
    }

    if (found_contradiction) {
        result->result = LANG_LOGIC_RESULT_INCONSISTENT;
        result->confidence = 0.7f;
        bridge->stats.contradictions_found++;
        strncpy(result->explanation, "Potential contradiction detected",
                sizeof(result->explanation) - 1);
    } else {
        result->result = LANG_LOGIC_RESULT_TRUE;
        result->confidence = 0.9f;
        strncpy(result->explanation, "No contradictions found",
                sizeof(result->explanation) - 1);
    }

    result->inference_steps = count * (count - 1) / 2;
    result->time_ms = result->inference_steps;
    result->has_proof = false;

    bridge->stats.consistency_checks++;
    return 0;
}

int language_logic_bridge_resolve_reference(
    language_logic_bridge_t* bridge,
    const char* text,
    const char* reference,
    char* resolved,
    uint32_t resolved_size)
{
    if (!bridge || !text || !reference || !resolved || resolved_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_resolve_reference: required parameter is NULL (bridge, text, reference, resolved)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_resolve_reference: bridge->active is NULL");
        return -1;
    }
    if (!bridge->config.enable_reference_resolution) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_resolve_reference: bridge->config is NULL");
        return -1;
    }

    /* Simple recency-based resolution (would use real coreference) */
    /* For now, just return the reference as-is */
    strncpy(resolved, reference, resolved_size - 1);
    resolved[resolved_size - 1] = '\0';

    return 0;
}

int language_logic_bridge_query(
    language_logic_bridge_t* bridge,
    const language_logic_query_t* query,
    language_logic_inference_t* result)
{
    if (!bridge || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_query: required parameter is NULL (bridge, query, result)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_query: bridge->active is NULL");
        return -1;
    }

    memset(result, 0, sizeof(language_logic_inference_t));

    /* Route based on operation type */
    switch (query->operation) {
        case LANG_LOGIC_ENTAILMENT:
            return language_logic_bridge_check_entailment(
                bridge, query->premise_a, query->premise_b, result);

        case LANG_LOGIC_CONSISTENCY:
            /* Would need to convert to array for consistency check */
            result->result = LANG_LOGIC_RESULT_UNKNOWN;
            result->confidence = 0.5f;
            strncpy(result->explanation, "Direct consistency check not supported via query",
                    sizeof(result->explanation) - 1);
            break;

        case LANG_LOGIC_INFERENCE:
            /* General inference */
            result->result = LANG_LOGIC_RESULT_UNKNOWN;
            result->confidence = 0.5f;
            result->inference_steps = 1;
            strncpy(result->explanation, "General inference not implemented",
                    sizeof(result->explanation) - 1);
            bridge->stats.inferences_made++;
            break;

        default:
            result->result = LANG_LOGIC_RESULT_ERROR;
            result->confidence = 0.0f;
            strncpy(result->explanation, "Unknown operation type",
                    sizeof(result->explanation) - 1);
            break;
    }

    return 0;
}

//=============================================================================
// Discourse API Implementation
//=============================================================================

int language_logic_bridge_add_to_discourse(
    language_logic_bridge_t* bridge,
    const char* statement)
{
    if (!bridge || !statement) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_add_to_discourse: required parameter is NULL (bridge, statement)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_add_to_discourse: bridge->active is NULL");
        return -1;
    }

    /* Would add to actual discourse representation */
    /* For now, just track that something was added */

    return 0;
}

int language_logic_bridge_get_discourse_state(
    const language_logic_bridge_t* bridge,
    language_discourse_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_get_discourse_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    memcpy(state, &bridge->discourse_state, sizeof(language_discourse_state_t));
    return 0;
}

int language_logic_bridge_clear_discourse(language_logic_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    memset(&bridge->discourse_state, 0, sizeof(language_discourse_state_t));
    bridge->discourse_state.is_consistent = true;
    bridge->discourse_state.coherence_score = 1.0f;
    return 0;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

int language_logic_bridge_get_stats(
    const language_logic_bridge_t* bridge,
    language_logic_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_logic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(language_logic_stats_t));
    return 0;
}

void language_logic_bridge_reset_stats(language_logic_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(language_logic_stats_t));
}
