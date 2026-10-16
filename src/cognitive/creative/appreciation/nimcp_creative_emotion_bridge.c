//=============================================================================
// nimcp_creative_emotion_bridge.c - Creative-Emotion Bridge Implementation
//=============================================================================
/**
 * @file nimcp_creative_emotion_bridge.c
 * @brief Bridge connecting creative system to emotion system
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/appreciation/nimcp_creative_emotion_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>

#define LOG_MODULE "CREATIVE_EMO_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(creative_emotion_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_creative_emotion_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_creative_emotion_bridge_mesh_registry = NULL;

nimcp_error_t creative_emotion_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_creative_emotion_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "creative_emotion_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "creative_emotion_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_creative_emotion_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_creative_emotion_bridge_mesh_registry = registry;
    return err;
}

void creative_emotion_bridge_mesh_unregister(void) {
    if (g_creative_emotion_bridge_mesh_registry && g_creative_emotion_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_creative_emotion_bridge_mesh_registry, g_creative_emotion_bridge_mesh_id);
        g_creative_emotion_bridge_mesh_id = 0;
        g_creative_emotion_bridge_mesh_registry = NULL;
    }
}


//=============================================================================
// Configuration Defaults
//=============================================================================

void creative_emotion_bridge_config_defaults(creative_emotion_bridge_config_t* config) {
    if (!config) return;

    config->emotion_sensitivity = 1.0f;
    config->awe_threshold = 0.8f;
    config->chills_threshold = 0.9f;
    config->propagate_to_emotion_system = false;
    config->receive_from_emotion_system = false;
    config->emotion_decay_rate = 0.1f;
    config->store_emotional_memories = false;
    config->memory_threshold = 0.7f;
    config->max_emotional_memories = 100;
}

//=============================================================================
// Lifecycle API
//=============================================================================

creative_emotion_bridge_t* creative_emotion_bridge_create(
    const creative_emotion_bridge_config_t* config) {

    creative_emotion_bridge_t* bridge = nimcp_calloc(1, sizeof(creative_emotion_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate emotion bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_emotion_bridge_config_defaults: bridge is NULL");
        return NULL;
    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(creative_emotion_bridge_config_t));
    } else {
        creative_emotion_bridge_config_defaults(&bridge->config);
    }

    bridge->events_processed = 0;
    bridge->avg_engagement = 0.0f;
    bridge->last_update_us = 0;

    LOG_INFO(LOG_MODULE, "Creative emotion bridge created");
    return bridge;
}

void creative_emotion_bridge_destroy(creative_emotion_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->event_history) {
        nimcp_free(bridge->event_history);
    }

    nimcp_free(bridge);
    bridge = NULL;
    LOG_INFO(LOG_MODULE, "Creative emotion bridge destroyed");
}

//=============================================================================
// Processing API - Stubs
//=============================================================================

int creative_emotion_bridge_process(creative_emotion_bridge_t* bridge,
                                     const aesthetic_evaluation_t* eval,
                                     aesthetic_emotional_state_t* out) {
    if (!bridge || !eval || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_emotion_bridge_destroy: required parameter is NULL (bridge, eval, out)");
        return -1;
    }

    memset(out, 0, sizeof(aesthetic_emotional_state_t));
    bridge->events_processed++;

    return 0;
}

int creative_emotion_bridge_extract(creative_emotion_bridge_t* bridge,
                                     const void* content,
                                     art_modality_t modality,
                                     aesthetic_emotional_response_t* out) {
    if (!bridge || !content || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_emotion_bridge_destroy: required parameter is NULL (bridge, content, out)");
        return -1;
    }

    memset(out, 0, sizeof(aesthetic_emotional_response_t));

    return 0;
}

void creative_emotion_bridge_set_emotion_system(creative_emotion_bridge_t* bridge,
                                                  void* emotion_system) {
    if (!bridge) return;
    bridge->emotion_system = emotion_system;
}

/* Alias to match header declaration */
void creative_emotion_set_emotion_system(creative_emotion_bridge_t* bridge,
                                          void* emotion_system) {
    creative_emotion_bridge_set_emotion_system(bridge, emotion_system);
}

void creative_emotion_bridge_set_hippocampus(creative_emotion_bridge_t* bridge,
                                               void* hippocampus) {
    if (!bridge) return;
    bridge->hippocampus = hippocampus;
}

const aesthetic_emotional_state_t* creative_emotion_bridge_get_state(
    const creative_emotion_bridge_t* bridge) {
    return bridge ? &bridge->current_state : NULL;
}
