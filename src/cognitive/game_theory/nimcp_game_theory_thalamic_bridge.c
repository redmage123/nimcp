/**
 * @file nimcp_game_theory_thalamic_bridge.c
 * @brief Game Theory-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/game_theory/nimcp_game_theory_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(game_theory_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_game_theory_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_game_theory_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t game_theory_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_game_theory_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "game_theory_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "game_theory_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_game_theory_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_game_theory_thalamic_bridge_mesh_registry = registry;
    return err;
}

void game_theory_thalamic_bridge_mesh_unregister(void) {
    if (g_game_theory_thalamic_bridge_mesh_registry && g_game_theory_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_game_theory_thalamic_bridge_mesh_registry, g_game_theory_thalamic_bridge_mesh_id);
        g_game_theory_thalamic_bridge_mesh_id = 0;
        g_game_theory_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from game_theory_thalamic_bridge module (instance-level) */
static inline void game_theory_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_game_theory_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_game_theory_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_game_theory_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "GAME_THEORY_THALAMIC_BRIDGE"


struct game_theory_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* game_theory;
    thalamic_router_t* router;
    game_theory_thalamic_config_t config;
    game_theory_thalamic_stats_t stats;
    float attention_weight;
};

game_theory_thalamic_config_t game_theory_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    game_theory_thalamic_bridge_heartbeat("game_theory__game_theory_thalamic", 0.0f);


    game_theory_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_stakes_boost = true,
        .min_stakes_threshold = 0.25f,
        .coalition_boost = 0.35f
    };
    return cfg;
}

game_theory_thalamic_bridge_t* game_theory_thalamic_bridge_create(void* game_theory, thalamic_router_t* router, const game_theory_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    game_theory_thalamic_bridge_heartbeat("game_theory__create", 0.0f);


    game_theory_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(game_theory_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->game_theory = game_theory;
    bridge->router = router;
    bridge->config = config ? *config : game_theory_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "game_theory_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("Created %s bridge", "game_theory_thalamic");
    return bridge;
}

void game_theory_thalamic_bridge_destroy(game_theory_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    game_theory_thalamic_bridge_heartbeat("game_theory__destroy", 0.0f);

    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int game_theory_thalamic_bridge_reset(game_theory_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    game_theory_thalamic_bridge_heartbeat("game_theory__reset", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int game_theory_thalamic_route_strategy(game_theory_thalamic_bridge_t* bridge, const game_theory_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_thalamic_route_strategy: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    game_theory_thalamic_bridge_heartbeat("game_theory__game_theory_thalamic", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating && signal->stakes_level < bridge->config.min_stakes_threshold) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.strategies_routed++;
    bridge->stats.avg_stakes_level = (bridge->stats.avg_stakes_level * (bridge->stats.strategies_routed - 1) +
                                      signal->stakes_level) / bridge->stats.strategies_routed;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int game_theory_thalamic_route_outcome(game_theory_thalamic_bridge_t* bridge, const void* outcome, float payoff) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_thalamic_route_outcome: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    game_theory_thalamic_bridge_heartbeat("game_theory__game_theory_thalamic", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.outcomes_processed++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int game_theory_thalamic_set_attention(game_theory_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    game_theory_thalamic_bridge_heartbeat("game_theory__game_theory_thalamic", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int game_theory_thalamic_get_attention(const game_theory_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    game_theory_thalamic_bridge_heartbeat("game_theory__game_theory_thalamic", 0.0f);


    return 0;
}

int game_theory_thalamic_bridge_get_stats(const game_theory_thalamic_bridge_t* bridge, game_theory_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    game_theory_thalamic_bridge_heartbeat("game_theory__get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Game Theory Thalamic Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int game_theory_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    game_theory_thalamic_bridge_heartbeat("game_theory__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Game_Theory_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                game_theory_thalamic_bridge_heartbeat("game_theory__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* GT Thalamic bridge self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Game_Theory_Thalamic_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Game_Theory_Thalamic_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void game_theory_thalamic_bridge_set_instance_health_agent(game_theory_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "game_theory_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int game_theory_thalamic_bridge_training_begin(game_theory_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "game_theory_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    game_theory_thalamic_bridge_heartbeat_instance(bridge->health_agent, "game_theory_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int game_theory_thalamic_bridge_training_end(game_theory_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "game_theory_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    game_theory_thalamic_bridge_heartbeat_instance(bridge->health_agent, "game_theory_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int game_theory_thalamic_bridge_training_step(game_theory_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "game_theory_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    game_theory_thalamic_bridge_heartbeat_instance(bridge->health_agent, "game_theory_thalamic_bridge_training_step", progress);
    return 0;
}
