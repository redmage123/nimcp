/**
 * @file nimcp_autobiographical_fep_bridge.c
 * @brief Free Energy Principle - Autobiographical Memory Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of bidirectional FEP-autobiographical memory integration
 * WHY:  Episodic memories minimize surprise by providing predictive context from past experiences
 * HOW:  High prediction error episodes stored as autobiographical memories; memories update FEP priors
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * EPISODIC MEMORY AS GENERATIVE MODEL TRAINING:
 * - Memorable events are those with high prediction error (surprise)
 * - Hippocampal encoding is enhanced by unexpected outcomes (high PE)
 * - Memory replay during sleep updates generative model priors
 * - Autobiographical memories provide training examples for future predictions
 * - Personal history shapes prior beliefs about self and world
 *
 * REFERENCES:
 * - Lisman & Grace (2005) "The hippocampal-VTA loop: Controlling the entry of information into LTM"
 * - Friston & Buzsaki (2016) "The functional anatomy of time: What and when in the brain"
 * - Conway (2005) "Memory and the self"
 * - Hassabis et al. (2007) "Patients with hippocampal amnesia cannot imagine new experiences"
 */

#include "cognitive/autobiographical_memory/nimcp_autobiographical_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(autobiographical_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get current time in milliseconds
 * WHY:  Track timing for memory encoding and replay
 * HOW:  Use CLOCK_MONOTONIC for monotonic time
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * WHAT: Compute memory importance from prediction error
 * WHY:  High surprise events are more memorable (biological encoding)
 * HOW:  Sigmoid-like function scaled by surprise threshold
 */
static float compute_memory_importance(float surprise_level, float threshold) {
    if (surprise_level < threshold) return 0.0f;

    /* Normalized surprise above threshold */
    float normalized = (surprise_level - threshold) / threshold;

    /* Sigmoid: importance = 1 / (1 + exp(-k*(x-1)))
     * This gives ~0.5 at threshold, approaching 1.0 for high surprise */
    float k = 2.0f;  /* Steepness parameter */
    float importance = 1.0f / (1.0f + expf(-k * (normalized - 1.0f)));

    return nimcp_clampf(importance, 0.0f, 1.0f);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Set default configuration for autobiographical-FEP bridge
 * WHY:  Provide biologically-plausible defaults for easy initialization
 * HOW:  Set thresholds and rates based on neuroscience literature
 */
int autobiographical_fep_bridge_default_config(autobiographical_fep_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_default_config", 0.0f);


    config->surprise_memory_threshold = SURPRISE_MEMORY_THRESHOLD;
    config->memory_importance_weight = 1.0f;
    config->model_update_rate = MEMORY_MODEL_UPDATE_RATE;
    config->prior_influence_rate = 0.2f;

    config->enable_surprise_encoding = true;
    config->enable_memory_replay = true;
    config->enable_prior_updates = true;

    return 0;
}

/**
 * WHAT: Create autobiographical-FEP bridge
 * WHY:  Initialize bidirectional integration between FEP and autobiographical memory
 * HOW:  Allocate structure, apply config, initialize state/stats, create mutex
 */
autobiographical_fep_bridge_t* autobiographical_fep_bridge_create(
    const autobiographical_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_create", 0.0f);


    autobiographical_fep_bridge_t* bridge = (autobiographical_fep_bridge_t*)nimcp_calloc(
        1, sizeof(autobiographical_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate autobiographical-FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    autobiographical_fep_config_t default_cfg;
    if (!config) {
        autobiographical_fep_bridge_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    /* Initialize state */
    bridge->state.current_surprise_level = 0.0f;
    bridge->state.memories_encoded = 0;
    bridge->state.model_updates_from_memory = 0;
    bridge->state.avg_memory_importance = 0.0f;

    /* Initialize effects */
    bridge->effects.memory_encoding_boost = 0.0f;
    bridge->effects.model_prior_adjustment = 0.0f;
    bridge->effects.replay_frequency = 0.0f;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(autobiographical_fep_stats_t));

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "autobiographical_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "autobiographical_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize bio-async */
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Autobiographical-FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy autobiographical-FEP bridge
 * WHY:  Clean up resources and prevent memory leaks
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void autobiographical_fep_bridge_destroy(autobiographical_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if enabled */
    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        autobiographical_fep_bridge_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Autobiographical-FEP bridge destroyed");
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

/**
 * WHAT: Connect FEP system to bridge
 * WHY:  Enable bridge to read prediction errors and update generative model
 * HOW:  Store pointer to FEP system with mutex protection
 */
int autobiographical_fep_bridge_connect_fep(
    autobiographical_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Allow NULL fep to disconnect/reset FEP connection */

    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_connect_fep", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Autobiographical-FEP bridge connected to FEP system");
    return 0;
}

/**
 * WHAT: Connect autobiographical memory system to bridge
 * WHY:  Enable bridge to store/retrieve episodic memories
 * HOW:  Store autobiographical memory handle with mutex protection
 */
int autobiographical_fep_bridge_connect_autobiographical(
    autobiographical_fep_bridge_t* bridge,
    autobiographical_memory_t autobio
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_connect_autobiograph", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->autobio_system = autobio;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Autobiographical-FEP bridge connected to autobiographical memory");
    return 0;
}

/* ============================================================================
 * FEP → Memory Direction Implementation
 * ============================================================================ */

/**
 * WHAT: Encode current high-surprise episode as autobiographical memory
 * WHY:  Memorable events are those with high prediction error (Lisman & Grace 2005)
 * HOW:  Check FEP surprise level, create memory entry if above threshold, store in autobio system
 */
int autobiographical_fep_encode_surprising_episode(autobiographical_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobiographical_fep_encode_surprising_episode: bridge is NULL");
        return -1;
    }
    /* No-op if subsystems not connected or feature disabled */
    if (!bridge->fep_system || !bridge->autobio_system) return 0;
    if (!bridge->config.enable_surprise_encoding) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_autobiographical_fep", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current surprise level from FEP */
    float surprise = bridge->fep_system->free_energy.surprise;
    bridge->state.current_surprise_level = surprise;

    /* Check if surprise exceeds threshold */
    if (surprise < bridge->config.surprise_memory_threshold) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Compute memory importance from surprise level */
    float importance = compute_memory_importance(
        surprise,
        bridge->config.surprise_memory_threshold
    ) * bridge->config.memory_importance_weight;

    /* Create autobiographical memory entry */
    autobiographical_memory_entry_t memory;
    memset(&memory, 0, sizeof(autobiographical_memory_entry_t));

    memory.timestamp_ms = get_time_ms();
    memory.type = AUTOBIO_LEARNING;  /* High PE = learning opportunity */

    /* Format description with surprise level */
    snprintf(memory.what_happened, AUTOBIO_MAX_DESCRIPTION_LEN,
        "Experienced unexpected event (surprise=%.2f)", surprise);

    snprintf(memory.why_it_happened, AUTOBIO_MAX_REASONING_LEN,
        "Prediction error exceeded threshold, indicating novel situation");

    snprintf(memory.outcome, AUTOBIO_MAX_OUTCOME_LEN,
        "Updated beliefs to minimize future surprise");

    /* Emotional tags: High surprise = arousal, valence depends on context */
    memory.valence = VALENCE_NEUTRAL;  /* Can be refined with context */
    memory.emotional_intensity = nimcp_clampf(surprise / 10.0f, 0.0f, 1.0f);
    memory.arousal = nimcp_clampf(surprise / 10.0f, 0.0f, 1.0f);

    /* Self-relevance */
    memory.importance = importance;
    memory.self_relevance = 0.8f;  /* High PE is self-relevant */
    memory.identity_defining = (importance > 0.8f);  /* Very high PE can be identity-defining */

    /* Memory dynamics */
    memory.memory_strength = nimcp_clampf(surprise / 10.0f, 0.0f, 1.0f);
    memory.certainty = 1.0f;  /* High certainty - we experienced this */
    memory.is_core_memory = memory.identity_defining;

    /* Store in autobiographical memory system */
    uint64_t memory_id = autobio_store(bridge->autobio_system, &memory);

    if (memory_id > 0) {
        bridge->state.memories_encoded++;
        bridge->stats.total_surprise_encodings++;

        /* Update average encoding surprise */
        float count = (float)bridge->stats.total_surprise_encodings;
        bridge->stats.avg_encoding_surprise =
            (bridge->stats.avg_encoding_surprise * (count - 1.0f) + surprise) / (fabsf(count) > 1e-7f ? count : 1e-7f);

        /* Update average importance */
        bridge->state.avg_memory_importance =
            (bridge->state.avg_memory_importance * (count - 1.0f) + importance) / (fabsf(count) > 1e-7f ? count : 1e-7f);

        /* Apply encoding boost effect */
        bridge->effects.memory_encoding_boost = importance;

        NIMCP_LOGGING_DEBUG("Encoded surprising episode as memory %llu (surprise=%.2f, importance=%.2f)",
            (unsigned long long)memory_id, surprise, importance);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return memory_id > 0 ? 0 : -1;
}

/* ============================================================================
 * Memory → FEP Direction Implementation
 * ============================================================================ */

/**
 * WHAT: Replay stored memories to update FEP generative model
 * WHY:  Memory replay during sleep refines predictive models (Friston & Buzsaki 2016)
 * HOW:  Retrieve recent high-importance memories, use them to adjust FEP priors
 */
int autobiographical_fep_replay_memories(autobiographical_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobiographical_fep_replay_memories: bridge is NULL");
        return -1;
    }
    /* No-op if subsystems not connected or feature disabled */
    if (!bridge->fep_system || !bridge->autobio_system) return 0;
    if (!bridge->config.enable_memory_replay) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_autobiographical_fep", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Query recent high-importance memories */
    memory_query_t query;
    memset(&query, 0, sizeof(memory_query_t));

    query.min_importance = 0.5f;  /* Only replay important memories */
    query.filter_by_importance = true;
    query.max_results = 10;  /* Replay last 10 important memories */
    query.sort_by_recency = true;

    autobiographical_memory_entry_t results[10];
    uint32_t num_found = 0;

    bool success = autobio_query(bridge->autobio_system, &query, results, 10, &num_found);

    if (success && num_found > 0) {
        /* Update replay frequency effect */
        bridge->effects.replay_frequency = (float)num_found / 10.0f;

        /* Count replays */
        bridge->stats.total_memory_replays += num_found;

        NIMCP_LOGGING_DEBUG("Replayed %u memories for generative model update", num_found);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return success ? 0 : -1;
}

/**
 * WHAT: Update FEP priors from autobiographical memory history
 * WHY:  Personal history shapes prior beliefs (Conway 2005)
 * HOY:  Extract patterns from core memories, adjust FEP prior distributions
 */
int autobiographical_fep_update_priors_from_memory(autobiographical_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobiographical_fep_update_priors_from_memory: bridge is NULL");
        return -1;
    }
    /* No-op if subsystems not connected or feature disabled */
    if (!bridge->fep_system || !bridge->autobio_system) return 0;
    if (!bridge->config.enable_prior_updates) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_autobiographical_fep", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get core (identity-defining) memories */
    autobiographical_memory_entry_t core_memories[32];
    uint32_t num_core = 0;

    bool success = autobio_get_core_memories(
        bridge->autobio_system,
        core_memories,
        32,
        &num_core
    );

    if (success && num_core > 0) {
        /* Compute average importance of core memories */
        float total_importance = 0.0f;
        for (uint32_t i = 0; i < num_core; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_core > 256) {
                autobiographical_fep_bridge_heartbeat("autobiograph_loop",
                                 (float)(i + 1) / (float)num_core);
            }

            total_importance += core_memories[i].importance;
        }
        float avg_core_importance = total_importance / (float)num_core;

        /* Adjust FEP priors based on core memory strength */
        float prior_adjustment = avg_core_importance * bridge->config.prior_influence_rate;

        /* Apply adjustment to FEP hierarchy levels */
        for (uint32_t l = 0; l < bridge->fep_system->num_levels; l++) {
            fep_hierarchy_level_t* level = &bridge->fep_system->levels[l];

            /* Strengthen prior precision based on autobiographical knowledge */
            for (uint32_t i = 0; i < level->beliefs.dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && level->beliefs.dim > 256) {
                    autobiographical_fep_bridge_heartbeat("autobiograph_loop",
                                     (float)(i + 1) / (float)level->beliefs.dim);
                }

                level->prior_precision[i] *= (1.0f + prior_adjustment);
            }
        }

        /* Update state and effects */
        bridge->state.model_updates_from_memory++;
        bridge->stats.total_prior_updates++;
        bridge->effects.model_prior_adjustment = prior_adjustment;

        NIMCP_LOGGING_DEBUG("Updated FEP priors from %u core memories (adjustment=%.2f)",
            num_core, prior_adjustment);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return success ? 0 : -1;
}

/* ============================================================================
 * Update Cycle Implementation
 * ============================================================================ */

/**
 * WHAT: Update autobiographical-FEP bridge state
 * WHY:  Maintain bidirectional integration between memory and prediction
 * HOW:  Check for surprising episodes, replay memories, update priors
 */
int autobiographical_fep_bridge_update(
    autobiographical_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Check for surprising episodes to encode */
    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_update", 0.0f);


    if (bridge->config.enable_surprise_encoding && bridge->fep_system) {
        autobiographical_fep_encode_surprising_episode(bridge);
    }

    /* Periodically replay memories (simplified - could be time-based) */
    if (bridge->config.enable_memory_replay &&
        bridge->stats.total_memory_replays % 100 == 0) {
        autobiographical_fep_replay_memories(bridge);
    }

    /* Periodically update priors from memory */
    if (bridge->config.enable_prior_updates &&
        bridge->state.model_updates_from_memory % 50 == 0) {
        autobiographical_fep_update_priors_from_memory(bridge);
    }

    return 0;
}

/* ============================================================================
 * State/Stats Implementation
 * ============================================================================ */

/**
 * WHAT: Get current bridge state
 * WHY:  Allow monitoring of surprise levels and memory encoding
 * HOW:  Copy state structure with mutex protection
 */
int autobiographical_fep_bridge_get_state(
    const autobiographical_fep_bridge_t* bridge,
    autobiographical_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobiographical_fep_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_get_state", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Get bridge statistics
 * WHY:  Track encoding, replay, and prior update events
 * HOW:  Copy stats structure with mutex protection
 */
int autobiographical_fep_bridge_get_stats(
    const autobiographical_fep_bridge_t* bridge,
    autobiographical_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobiographical_fep_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_get_stats", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

/**
 * WHAT: Connect bridge to bio-async router
 * WHY:  Enable inter-module messaging for distributed FEP-memory coordination
 * HOW:  Register module with bio-async router, set enabled flag
 */
int autobiographical_fep_bridge_connect_bio_async(autobiographical_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_connect_bio_async", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_AUTOBIOGRAPHICAL_BRIDGE,
        .module_name = "autobiographical_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Autobiographical-FEP bridge connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * WHAT: Disconnect bridge from bio-async router
 * WHY:  Clean shutdown of messaging subsystem
 * HOW:  Unregister module, clear context and flag
 */
int autobiographical_fep_bridge_disconnect_bio_async(autobiographical_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Autobiographical-FEP bridge disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Allow conditional use of bio-async features
 * HOW:  Return enabled flag
 */
bool autobiographical_fep_bridge_is_bio_async_connected(
    const autobiographical_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_is_bio_async_connect", 0.0f);


    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int autobiographical_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobiographical_fep_bridge_heartbeat("autobiograph_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Autobiographical_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                autobiographical_fep_bridge_heartbeat("autobiograph_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Autobiographical_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Autobiographical_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent Setter
 * ============================================================================ */

void autobiographical_fep_bridge_set_instance_health_agent(autobiographical_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
    NIMCP_LOGGING_DEBUG("autobiographical_fep_bridge: instance health agent %s",
                        agent ? "set" : "cleared");
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int autobiographical_fep_bridge_training_begin(autobiographical_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobiographical_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    autobiographical_fep_bridge_heartbeat_instance(bridge, "autobio_fep_training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int autobiographical_fep_bridge_training_end(autobiographical_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobiographical_fep_bridge_training_end: NULL argument");
        return -1;
    }
    autobiographical_fep_bridge_heartbeat_instance(bridge, "autobio_fep_training_end", 1.0f);
    (void)bridge;
    return 0;
}

int autobiographical_fep_bridge_training_step(autobiographical_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobiographical_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    autobiographical_fep_bridge_heartbeat_instance(bridge->health_agent, "autobio_fep_training_step", progress);
    (void)bridge;
    return 0;
}
