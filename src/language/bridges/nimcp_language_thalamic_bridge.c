#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_thalamic_bridge.c - Language-Thalamic Bridge Implementation
//=============================================================================
/**
 * @file nimcp_language_thalamic_bridge.c
 * @brief Implementation of Language-Thalamic bridge for signal routing
 *
 * WHAT: Bridge connecting language layer with thalamic router
 * WHY:  Enable signal gating and relay through thalamic nuclei
 * HOW:  Routes language signals through Pulvinar, VA, VL, MD, MGN nuclei
 *
 * @version 1.0.0 - Phase L8: Additional Integration Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#include "language/bridges/nimcp_language_thalamic_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>

#define LOG_MODULE "LANG_THALAMIC_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_thalamic_bridge)

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* language_thalamic_nucleus_to_string(language_thalamic_nucleus_t nucleus) {
    static const char* names[] = {
        "PULVINAR",
        "VA",
        "VL",
        "MD",
        "LGN",
        "MGN",
        "TRN"
    };
    if (nucleus >= LANG_THAL_NUCLEUS_COUNT) return "UNKNOWN";
    return names[nucleus];
}

//=============================================================================
// Configuration API Implementation
//=============================================================================

static void thalamic_default_config_internal(language_thalamic_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(language_thalamic_config_t));

    config->enable_attention_gating = true;
    config->enable_motor_priority = true;
    config->enable_semantic_routing = true;
    config->enable_multimodal_routing = true;

    config->attention_threshold = 0.3f;
    config->motor_priority_boost = 0.2f;
    config->gating_decay_rate = 0.1f;

    config->relay_latency_us = 1000;
    config->gating_window_ms = 50;

    config->enable_bio_async = false;
    config->update_interval_ms = 10;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

language_thalamic_bridge_t* language_thalamic_bridge_create(
    language_orchestrator_t* orchestrator,
    const language_thalamic_config_t* config)
{
    language_thalamic_bridge_t* bridge = (language_thalamic_bridge_t*)
        nimcp_calloc(1, sizeof(language_thalamic_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(language_thalamic_config_t));
    } else {
        thalamic_default_config_internal(&bridge->config);
    }

    bridge->orchestrator = orchestrator;
    bridge->router = NULL;
    bridge->current_attention = 0.5f;

    /* Initialize gating states to default (0.5 = half-open) */
    for (int i = 0; i < LANG_THAL_NUCLEUS_COUNT; i++) {
        bridge->gating_state[i] = 0.5f;
    }

    memset(&bridge->stats, 0, sizeof(language_thalamic_stats_t));
    bridge->initialized = true;
    bridge->active = false;

    LOG_INFO(LOG_MODULE, "Thalamic bridge created");
    return bridge;
}

void language_thalamic_bridge_destroy(language_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_thalamic");

    nimcp_free(bridge);
    LOG_INFO(LOG_MODULE, "Thalamic bridge destroyed");
}

int language_thalamic_bridge_connect_router(
    language_thalamic_bridge_t* bridge,
    thalamic_router_t* router)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->router = router;
    return 0;
}

//=============================================================================
// Signal Routing API Implementation
//=============================================================================

int language_thalamic_bridge_route_phoneme(
    language_thalamic_bridge_t* bridge,
    const void* phoneme_data,
    uint32_t size)
{
    if (!bridge || !phoneme_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_thalamic_bridge_route_phoneme: required parameter is NULL (bridge, phoneme_data)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_thalamic_bridge_route_phoneme: bridge->active is NULL");
        return -1;
    }

    (void)size;  /* Would use in actual routing */

    /* Route through MGN (auditory relay) */
    float gating = bridge->gating_state[LANG_THAL_NUCLEUS_MGN];
    if (gating < 0.1f) {
        /* Signal gated - don't route */
        bridge->stats.signals_blocked++;
        return 0;
    }

    bridge->stats.phonemes_relayed++;
    return 0;
}

int language_thalamic_bridge_route_word(
    language_thalamic_bridge_t* bridge,
    const void* word_data,
    uint32_t size)
{
    if (!bridge || !word_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_thalamic_bridge_route_word: required parameter is NULL (bridge, word_data)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_thalamic_bridge_route_word: bridge->active is NULL");
        return -1;
    }

    (void)size;

    /* Route through Pulvinar (attention/integration) */
    float gating = bridge->gating_state[LANG_THAL_NUCLEUS_PULVINAR];
    if (gating < 0.1f) {
        bridge->stats.signals_blocked++;
        return 0;
    }

    bridge->stats.words_relayed++;
    return 0;
}

int language_thalamic_bridge_route_motor_speech(
    language_thalamic_bridge_t* bridge,
    const void* motor_data,
    uint32_t size)
{
    if (!bridge || !motor_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_thalamic_bridge_route_motor_speech: required parameter is NULL (bridge, motor_data)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_thalamic_bridge_route_motor_speech: bridge->active is NULL");
        return -1;
    }

    (void)size;

    /* Route through VA/VL (motor relay) */
    float gating = bridge->gating_state[LANG_THAL_NUCLEUS_VA];
    if (gating < 0.1f) {
        bridge->stats.signals_blocked++;
        return 0;
    }

    bridge->stats.motor_commands_sent++;
    return 0;
}

int language_thalamic_bridge_route_semantic(
    language_thalamic_bridge_t* bridge,
    const void* semantic_data,
    uint32_t size)
{
    if (!bridge || !semantic_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_thalamic_bridge_route_semantic: required parameter is NULL (bridge, semantic_data)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_thalamic_bridge_route_semantic: bridge->active is NULL");
        return -1;
    }

    (void)size;

    /* Route through MD (prefrontal) */
    float gating = bridge->gating_state[LANG_THAL_NUCLEUS_MD];
    if (gating < 0.1f) {
        bridge->stats.signals_blocked++;
        return 0;
    }

    bridge->stats.semantic_relays++;
    return 0;
}

int language_thalamic_bridge_send_signal(
    language_thalamic_bridge_t* bridge,
    const language_thalamic_signal_t* signal)
{
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_thalamic_bridge_send_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_thalamic_bridge_send_signal: bridge->active is NULL");
        return -1;
    }

    /* Bounds check for target nucleus */
    if (signal->target < 0 || signal->target >= LANG_THAL_NUCLEUS_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_thalamic_bridge_send_signal: signal->target out of bounds");
        return -1;
    }

    /* Check gating for target nucleus */
    float gating = bridge->gating_state[signal->target];
    if (gating < signal->gating_threshold) {
        bridge->stats.signals_blocked++;
        return 0;
    }

    /* Route based on signal type */
    switch (signal->signal_type) {
        case LANG_THAL_SIGNAL_PHONEME_RELAY:
            bridge->stats.phonemes_relayed++;
            break;
        case LANG_THAL_SIGNAL_WORD_RELAY:
            bridge->stats.words_relayed++;
            break;
        case LANG_THAL_SIGNAL_MOTOR_SPEECH:
            bridge->stats.motor_commands_sent++;
            break;
        case LANG_THAL_SIGNAL_SEMANTIC_RELAY:
            bridge->stats.semantic_relays++;
            break;
        default:
            break;
    }

    return 0;
}

//=============================================================================
// Attention Gating API Implementation
//=============================================================================

int language_thalamic_bridge_set_attention(
    language_thalamic_bridge_t* bridge,
    float attention_level)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Clamp attention to [0, 1] */
    if (attention_level < 0.0f) attention_level = 0.0f;
    if (attention_level > 1.0f) attention_level = 1.0f;

    bridge->current_attention = attention_level;

    /* Modulate pulvinar gating based on attention */
    bridge->gating_state[LANG_THAL_NUCLEUS_PULVINAR] =
        0.3f + 0.7f * attention_level;

    /* Modulate TRN inversely (higher attention = less inhibition) */
    bridge->gating_state[LANG_THAL_NUCLEUS_TRN] =
        0.3f + 0.5f * (1.0f - attention_level);

    bridge->stats.attention_gates++;
    bridge->stats.avg_attention_level =
        (bridge->stats.avg_attention_level * (bridge->stats.attention_gates - 1) +
         attention_level) / bridge->stats.attention_gates;

    return 0;
}

int language_thalamic_bridge_gate_nucleus(
    language_thalamic_bridge_t* bridge,
    language_thalamic_nucleus_t nucleus,
    float gate_level)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (nucleus >= LANG_THAL_NUCLEUS_COUNT) return -1;

    /* Clamp gating to [0, 1] */
    if (gate_level < 0.0f) gate_level = 0.0f;
    if (gate_level > 1.0f) gate_level = 1.0f;

    bridge->gating_state[nucleus] = gate_level;
    return 0;
}

float language_thalamic_bridge_get_gate_state(
    const language_thalamic_bridge_t* bridge,
    language_thalamic_nucleus_t nucleus)
{
    if (!bridge || nucleus >= LANG_THAL_NUCLEUS_COUNT) return 0.0f;
    return bridge->gating_state[nucleus];
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

int language_thalamic_bridge_get_stats(
    const language_thalamic_bridge_t* bridge,
    language_thalamic_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(language_thalamic_stats_t));
    return 0;
}

void language_thalamic_bridge_reset_stats(language_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(language_thalamic_stats_t));
}
