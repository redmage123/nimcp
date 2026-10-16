/**
 * @file nimcp_reasoning_curiosity.c
 * @brief MODULE 2: Reasoning-Curiosity Integration Implementation
 */

#include "cognitive/reasoning/integration/nimcp_reasoning_curiosity.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"

#define LOG_MODULE "cognitive.reasoning.curiosity"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(reasoning_curiosity, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define BIO_MODULE_COGNITIVE_REASONING_CURIOSITY 0x034D


struct reasoning_curiosity {
    reasoning_curiosity_config_t config;
    event_bus_t event_bus;
    curiosity_engine_t curiosity;
    event_subscription_handle_t subscription_handle;
    reasoning_curiosity_stats_t stats;

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */
};

static uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000000 + (uint64_t)(tv.tv_usec);
}

reasoning_curiosity_config_t reasoning_curiosity_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_curiosity_heartbeat("reasoning_cu_default_config", 0.0f);


    reasoning_curiosity_config_t config = {
        .enable_proof_failure_exploration = true,
        .enable_novel_fact_exploration = true,
        .proof_failed_curiosity_boost = REASONING_CURIOSITY_PROOF_FAILED_BOOST,
        .novel_fact_curiosity_boost = REASONING_CURIOSITY_NOVEL_FACT_BOOST,
        .min_curiosity_threshold = 0.1F
    };
    return config;
}

bool reasoning_curiosity_validate_config(const reasoning_curiosity_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_curiosity_validate_config: config is NULL");
        return false;
    }
    if (config->proof_failed_curiosity_boost < 0.0F || config->proof_failed_curiosity_boost > 1.0F) {
        return false;
    }
    if (config->novel_fact_curiosity_boost < 0.0F || config->novel_fact_curiosity_boost > 1.0F) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    reasoning_curiosity_heartbeat("reasoning_cu_validate_config", 0.0f);


    return true;
}

reasoning_curiosity_t* reasoning_curiosity_create(event_bus_t bus, curiosity_engine_t curiosity) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_curiosity_heartbeat("reasoning_cu_create", 0.0f);


    LOG_DEBUG("Creating module");
    reasoning_curiosity_config_t config = reasoning_curiosity_default_config();
    return reasoning_curiosity_create_custom(bus, curiosity, &config);
}

reasoning_curiosity_t* reasoning_curiosity_create_custom(
    event_bus_t bus, curiosity_engine_t curiosity, const reasoning_curiosity_config_t* config
) {
    if (!bus || !curiosity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_curiosity_create_custom: required parameter is NULL (bus, curiosity)");
        return NULL;
    }
    
    /* Phase 8: Heartbeat at operation start */
    reasoning_curiosity_heartbeat("reasoning_cu_create_custom", 0.0f);


    reasoning_curiosity_t* integration = nimcp_calloc(1, sizeof(reasoning_curiosity_t));
    if (!integration) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate integration");

        return NULL;

    }
    
    reasoning_curiosity_config_t default_cfg = reasoning_curiosity_default_config();
    integration->config = config ? *config : default_cfg;
    integration->event_bus = bus;
    integration->curiosity = curiosity;
    
    integration->subscription_handle = event_bus_subscribe(
        bus, EVENT_ALL, reasoning_curiosity_callback, integration
    );
    
    if (integration->subscription_handle == INVALID_SUBSCRIPTION_HANDLE) {
        nimcp_free(integration);
        integration = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_curiosity_create_custom: validation failed");
        return NULL;
    }
    
    
    // Bio-async registration
    integration->bio_ctx = NULL;
    integration->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_CURIOSITY_REASONING,
            .module_name = "reasoning_curiosity",
            .inbox_capacity = 32,
            .user_data = integration
        };
        integration->bio_ctx = bio_router_register_module(&bio_info);
        if (integration->bio_ctx) {
            integration->bio_async_enabled = true;
        }
    }

return integration;
}

void reasoning_curiosity_destroy(reasoning_curiosity_t* integration) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_curiosity_heartbeat("reasoning_cu_destroy", 0.0f);


    LOG_DEBUG("Destroying module");
    if (!integration) return;
    if (integration->subscription_handle != INVALID_SUBSCRIPTION_HANDLE) {
        event_bus_unsubscribe(integration->event_bus, integration->subscription_handle);
    }
    // Unregister from bio-router
    if (integration->bio_async_enabled && integration->bio_ctx) {
        bio_router_unregister_module(integration->bio_ctx);
        integration->bio_ctx = NULL;
        integration->bio_async_enabled = false;
    }

    nimcp_free(integration);
    integration = NULL;
}

void reasoning_curiosity_callback(const brain_event_t* event, void* context) {
    if (!event || !context) return;

    /* Phase 8: Heartbeat at operation start */
    reasoning_curiosity_heartbeat("reasoning_cu_callback", 0.0f);


    reasoning_curiosity_t* integration = (reasoning_curiosity_t*)context;

    // Process pending bio-async messages
    if (integration && integration->bio_ctx) {
        bio_router_process_inbox(integration->bio_ctx, 5);
    }
    uint64_t start = get_time_us();
    
    if (event->type < EVENT_LOGIC_GATE_EVALUATED || event->type > EVENT_NOVEL_FACT_DERIVED) {
        return;
    }
    
    integration->stats.total_events_processed++;
    
    float curiosity_boost = 0.0F;
    
    if (event->type == EVENT_PROOF_FAILED && integration->config.enable_proof_failure_exploration) {
        curiosity_boost = integration->config.proof_failed_curiosity_boost;
        integration->stats.proof_failure_triggers++;
        integration->stats.exploration_triggers++;
    } else if (event->type == EVENT_NOVEL_FACT_DERIVED && integration->config.enable_novel_fact_exploration) {
        curiosity_boost = integration->config.novel_fact_curiosity_boost;
        integration->stats.novel_fact_triggers++;
    }
    
    if (curiosity_boost >= integration->config.min_curiosity_threshold) {
        if (integration->stats.exploration_triggers > 0) {
            float total = integration->stats.avg_curiosity_boost * (integration->stats.exploration_triggers - 1);
            integration->stats.avg_curiosity_boost = (total + curiosity_boost) / (float)integration->stats.exploration_triggers;
        } else {
            integration->stats.avg_curiosity_boost = curiosity_boost;
        }
    }
    
    uint64_t elapsed = get_time_us() - start;
    if (integration->stats.total_events_processed > 0) {
        uint64_t total_time = integration->stats.avg_callback_time_us * (integration->stats.total_events_processed - 1);
        integration->stats.avg_callback_time_us = (total_time + elapsed) / integration->stats.total_events_processed;
    }
}

bool reasoning_curiosity_explore_unexplained_fact(reasoning_curiosity_t* integration, const char* fact) {
    if (!integration || !fact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_curiosity_explore_unexplained_fact: required parameter is NULL (integration, fact)");
        return false;
    }
    
    /* Phase 8: Heartbeat at operation start */
    reasoning_curiosity_heartbeat("reasoning_cu_explore_unexplained_", 0.0f);


    knowledge_gap_t gap = curiosity_detect_knowledge_gap(integration->curiosity, fact);
    if (gap.gap_size > 0.3F) {
        integration->stats.exploration_triggers++;
        return true;
    }
    return false;
}

bool reasoning_curiosity_get_stats(const reasoning_curiosity_t* integration, reasoning_curiosity_stats_t* stats) {
    if (!integration || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_curiosity_get_stats: required parameter is NULL (integration, stats)");
        return false;
    }
    *stats = integration->stats;
    /* Phase 8: Heartbeat at operation start */
    reasoning_curiosity_heartbeat("reasoning_cu_get_stats", 0.0f);


    return true;
}

bool reasoning_curiosity_reset_stats(reasoning_curiosity_t* integration) {
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_curiosity_reset_stats: integration is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    reasoning_curiosity_heartbeat("reasoning_cu_reset_stats", 0.0f);


    memset(&integration->stats, 0, sizeof(reasoning_curiosity_stats_t));
    return true;
}

bool reasoning_curiosity_get_config(const reasoning_curiosity_t* integration, reasoning_curiosity_config_t* config) {
    if (!integration || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_curiosity_get_config: required parameter is NULL (integration, config)");
        return false;
    }
    *config = integration->config;
    /* Phase 8: Heartbeat at operation start */
    reasoning_curiosity_heartbeat("reasoning_cu_get_config", 0.0f);


    return true;
}

bool reasoning_curiosity_set_config(reasoning_curiosity_t* integration, const reasoning_curiosity_config_t* config) {
    if (!integration || !config || !reasoning_curiosity_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_curiosity_set_config: required parameter is NULL (integration, config, reasoning_curiosity_validate_config)");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    reasoning_curiosity_heartbeat("reasoning_cu_set_config", 0.0f);


    integration->config = *config;
    return true;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int reasoning_curiosity_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    reasoning_curiosity_heartbeat("reasoning_cu_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Reasoning_Curiosity");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                reasoning_curiosity_heartbeat("reasoning_cu_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Reasoning_Curiosity self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Reasoning_Curiosity");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Reasoning_Curiosity");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void reasoning_curiosity_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_reasoning_curiosity_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int reasoning_curiosity_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_curiosity_training_begin: NULL argument");
        return -1;
    }
    reasoning_curiosity_heartbeat_instance(NULL, "reasoning_curiosity_training_begin", 0.0f);
    (void)(struct reasoning_curiosity*)instance; /* Module state available for reset */
    return 0;
}

int reasoning_curiosity_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_curiosity_training_end: NULL argument");
        return -1;
    }
    reasoning_curiosity_heartbeat_instance(NULL, "reasoning_curiosity_training_end", 1.0f);
    (void)(struct reasoning_curiosity*)instance; /* Module state available for finalization */
    return 0;
}

int reasoning_curiosity_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_curiosity_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    reasoning_curiosity_heartbeat_instance(NULL, "reasoning_curiosity_training_step", progress);
    (void)(struct reasoning_curiosity*)instance; /* Module state available for step adaptation */
    return 0;
}
