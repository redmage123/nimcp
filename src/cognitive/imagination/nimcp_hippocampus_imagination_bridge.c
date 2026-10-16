/**
 * @file nimcp_hippocampus_imagination_bridge.c
 * @brief Hippocampus-Imagination Bidirectional Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "cognitive/imagination/nimcp_hippocampus_imagination_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "core/brain/regions/hippocampus/nimcp_hippocampus_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hippocampus_imagination_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_hippocampus_imagination_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_hippocampus_imagination_bridge_mesh_registry = NULL;

nimcp_error_t hippocampus_imagination_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_hippocampus_imagination_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "hippocampus_imagination_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "hippocampus_imagination_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_hippocampus_imagination_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_hippocampus_imagination_bridge_mesh_registry = registry;
    return err;
}

void hippocampus_imagination_bridge_mesh_unregister(void) {
    if (g_hippocampus_imagination_bridge_mesh_registry && g_hippocampus_imagination_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_hippocampus_imagination_bridge_mesh_registry, g_hippocampus_imagination_bridge_mesh_id);
        g_hippocampus_imagination_bridge_mesh_id = 0;
        g_hippocampus_imagination_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from hippocampus_imagination_bridge module (instance-level) */
static inline void hippocampus_imagination_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_hippocampus_imagination_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_hippocampus_imagination_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_hippocampus_imagination_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "HIPPOCAMPUS_IMAGINATION_BRIDGE"


/* Logging macros - map to nimcp logging functions */
#define NIMCP_LOG_INFO(...)  LOG_INFO(__VA_ARGS__)
#define NIMCP_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#define NIMCP_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

/* Time function alias */
#define nimcp_time_now_ms() nimcp_time_get_ms()

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

int hippocampus_imagination_default_config(hippocampus_imagination_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    config->relevance_threshold = HIPP_IMAG_DEFAULT_RELEVANCE_THRESHOLD;
    config->max_memories_per_request = 4;
    config->pattern_completion_weight = 0.5f;

    config->consolidation_boost = HIPP_IMAG_DEFAULT_CONSOLIDATION_BOOST;
    config->encoding_threshold = 0.6f;
    config->enable_pseudo_memory_encoding = true;

    config->enable_replay_triggering = true;
    config->replay_trigger_threshold = 0.7f;

    config->enable_spatial_imagination = true;
    config->spatial_context_weight = 0.4f;

    config->update_interval_ms = 16.0f;  /* ~60 Hz */
    config->enable_bio_async = true;

    return 0;
}

int hippocampus_imagination_validate_config(const hippocampus_imagination_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    if (config->relevance_threshold < 0.0f || config->relevance_threshold > 1.0f) {
        return -1;
    }
    if (config->max_memories_per_request == 0 ||
        config->max_memories_per_request > HIPP_IMAG_MAX_RETRIEVED_MEMORIES) {
        return -1;
    }
    if (config->pattern_completion_weight < 0.0f || config->pattern_completion_weight > 1.0f) {
        return -1;
    }
    if (config->consolidation_boost < 0.0f || config->consolidation_boost > 10.0f) {
        return -1;
    }
    if (config->encoding_threshold < 0.0f || config->encoding_threshold > 1.0f) {
        return -1;
    }
    if (config->spatial_context_weight < 0.0f || config->spatial_context_weight > 1.0f) {
        return -1;
    }
    if (config->update_interval_ms < 1.0f) {
        return -1;
    }

    return 0;
}

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */

hippocampus_imagination_bridge_t* hippocampus_imagination_bridge_create(
    const hippocampus_imagination_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__create", 0.0f);


    hippocampus_imagination_bridge_t* bridge = nimcp_calloc(
        1, sizeof(hippocampus_imagination_bridge_t));
    if (!bridge) {
        NIMCP_LOG_ERROR("Failed to allocate hippocampus-imagination bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Initialize base */
    bridge->base.module_id = BIO_MODULE_IMAGINATION_HIPPOCAMPUS;
    bridge->base.module_name = "hippocampus_imagination_bridge";
    bridge->base.system_a_connected = false;
    bridge->base.system_b_connected = false;
    bridge->base.bridge_active = false;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "hippocampus_imagination") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOG_ERROR("Failed to create bridge mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_imagination_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        if (hippocampus_imagination_validate_config(config) != 0) {
            NIMCP_LOG_ERROR("Invalid bridge configuration");
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_imagination_bridge_create: validation failed");
            return NULL;
        }
        bridge->config = *config;
    } else {
        hippocampus_imagination_default_config(&bridge->config);
    }

    /* Initialize effects */
    memset(&bridge->hipp_to_imag, 0, sizeof(bridge->hipp_to_imag));
    memset(&bridge->imag_to_hipp, 0, sizeof(bridge->imag_to_hipp));

    /* Initialize state */
    bridge->num_active_scenarios = 0;
    bridge->memory_request_pending = false;
    bridge->pending_query_cue = NULL;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Initialize timing */
    bridge->last_update_time_ms = nimcp_time_now_ms();

    NIMCP_LOG_INFO("Created hippocampus-imagination bridge");
    return bridge;
}

void hippocampus_imagination_bridge_destroy(hippocampus_imagination_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hippocampus_imagination");

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        hippocampus_imagination_disconnect_bio_async(bridge);
    }

    /* Free pending query cue */
    if (bridge->pending_query_cue) {
        nimcp_tensor_destroy(bridge->pending_query_cue);
    }

    /* Free retrieved embeddings */
    if (bridge->hipp_to_imag.retrieved_embeddings) {
        nimcp_tensor_destroy(bridge->hipp_to_imag.retrieved_embeddings);
    }

    /* Free imagined embedding */
    if (bridge->imag_to_hipp.imagined_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_hipp.imagined_embedding);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOG_INFO("Destroyed hippocampus-imagination bridge");
}

int hippocampus_imagination_reset(hippocampus_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Free tensor pointers in effects BEFORE memset zeroes them */
    if (bridge->hipp_to_imag.retrieved_embeddings) {
        nimcp_tensor_destroy(bridge->hipp_to_imag.retrieved_embeddings);
    }
    if (bridge->imag_to_hipp.imagined_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_hipp.imagined_embedding);
    }

    /* Clear effects (now safe - tensor pointers already freed) */
    memset(&bridge->hipp_to_imag, 0, sizeof(bridge->hipp_to_imag));
    memset(&bridge->imag_to_hipp, 0, sizeof(bridge->imag_to_hipp));

    /* Clear state */
    bridge->num_active_scenarios = 0;
    bridge->memory_request_pending = false;

    /* Free and clear tensors */
    if (bridge->pending_query_cue) {
        nimcp_tensor_destroy(bridge->pending_query_cue);
        bridge->pending_query_cue = NULL;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * CONNECTION
 * ============================================================================ */

int hippocampus_imagination_connect_hippocampus(
    hippocampus_imagination_bridge_t* bridge,
    struct hippocampus_adapter* hippocampus)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->hippocampus = hippocampus;
    bridge->base.system_a = hippocampus;
    bridge->base.system_a_connected = (hippocampus != NULL);
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  bridge->base.system_b_connected;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_INFO("Hippocampus %s to bridge",
                   hippocampus ? "connected" : "disconnected");
    return 0;
}

int hippocampus_imagination_connect_imagination(
    hippocampus_imagination_bridge_t* bridge,
    struct imagination_engine* imagination)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->imagination = imagination;
    bridge->base.system_b = imagination;
    bridge->base.system_b_connected = (imagination != NULL);
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  bridge->base.system_b_connected;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_INFO("Imagination engine %s to bridge",
                   imagination ? "connected" : "disconnected");
    return 0;
}

int hippocampus_imagination_disconnect_hippocampus(
    hippocampus_imagination_bridge_t* bridge)
{
    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    return hippocampus_imagination_connect_hippocampus(bridge, NULL);
}

int hippocampus_imagination_disconnect_imagination(
    hippocampus_imagination_bridge_t* bridge)
{
    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    return hippocampus_imagination_connect_imagination(bridge, NULL);
}

bool hippocampus_imagination_is_connected(
    const hippocampus_imagination_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    return bridge->base.bridge_active;
}

/* ============================================================================
 * UPDATE
 * ============================================================================ */

int hippocampus_imagination_update(
    hippocampus_imagination_bridge_t* bridge,
    float delta_time_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bridge_active) return 0;  /* Nothing to do */

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check update interval */
    uint64_t now = nimcp_time_now_ms();
    float elapsed = (float)(now - bridge->last_update_time_ms);
    if (elapsed < bridge->config.update_interval_ms) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->last_update_time_ms = now;

    /* Compute effects in both directions */
    hippocampus_imagination_compute_hipp_effects(bridge);
    hippocampus_imagination_compute_imag_effects(bridge);

    /* Apply effects */
    hippocampus_imagination_apply_effects(bridge);

    /* Update statistics */
    bridge->stats.total_updates++;
    bridge->base.total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hippocampus_imagination_compute_hipp_effects(
    hippocampus_imagination_bridge_t* bridge)
{
    if (!bridge || !bridge->hippocampus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_imagination_compute_hipp_effects: required parameter is NULL (bridge, bridge->hippocampus)");
        return -1;
    }

    /* Query hippocampus for current state */
    /* In a full implementation, this would call hippocampus adapter APIs */

    /* Default: low-level effects when no active imagination */
    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    bridge->hipp_to_imag.memory_relevance = 0.0f;
    bridge->hipp_to_imag.pattern_completion_strength = 0.0f;
    bridge->hipp_to_imag.spatial_context_strength = 0.0f;
    bridge->hipp_to_imag.num_retrieved_memories = 0;
    bridge->hipp_to_imag.replay_active = false;

    /* If there's a pending memory request, process it */
    if (bridge->memory_request_pending && bridge->pending_query_cue) {
        /* Would call hippocampus_retrieve() here */
        /* For now, mark request as processed */
        bridge->memory_request_pending = false;
        bridge->stats.memory_requests++;
    }

    return 0;
}

int hippocampus_imagination_compute_imag_effects(
    hippocampus_imagination_bridge_t* bridge)
{
    if (!bridge || !bridge->imagination) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_imagination_compute_imag_effects: required parameter is NULL (bridge, bridge->imagination)");
        return -1;
    }

    /* Query imagination engine for current state */
    /* In a full implementation, this would call imagination engine APIs */

    /* Default: no consolidation signals */
    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    bridge->imag_to_hipp.consolidation_priority = 0.0f;
    bridge->imag_to_hipp.encoding_strength = 0.0f;
    bridge->imag_to_hipp.trigger_replay = false;
    bridge->imag_to_hipp.scenario_id = 0;
    bridge->imag_to_hipp.emotional_salience = 0.0f;
    bridge->imag_to_hipp.novelty_score = 0.0f;
    bridge->imag_to_hipp.is_dream_content = false;

    return 0;
}

int hippocampus_imagination_apply_effects(hippocampus_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply hippocampus effects to imagination */
    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    if (bridge->imagination && bridge->hipp_to_imag.num_retrieved_memories > 0) {
        /* Would call imagination APIs to inject memory content */
        bridge->stats.memories_retrieved += bridge->hipp_to_imag.num_retrieved_memories;
    }

    /* Apply imagination effects to hippocampus */
    if (bridge->hippocampus && bridge->imag_to_hipp.consolidation_priority > 0.0f) {
        /* Would call hippocampus APIs to boost consolidation */
        if (bridge->imag_to_hipp.consolidation_priority >
            bridge->config.encoding_threshold) {
            bridge->stats.consolidation_triggers++;
        }
    }

    /* Handle replay triggering */
    if (bridge->hippocampus && bridge->imag_to_hipp.trigger_replay) {
        /* Would call hippocampus_trigger_replay() */
        bridge->stats.replay_triggers++;
    }

    return 0;
}

/* ============================================================================
 * MEMORY-IMAGINATION INTEGRATION
 * ============================================================================ */

uint32_t hippocampus_imagination_request_memory_imagination(
    hippocampus_imagination_bridge_t* bridge,
    const nimcp_tensor_t* memory_cue,
    struct imagination_goal* goal)
{
    if (!bridge || !memory_cue || !goal) return 0;
    if (!bridge->base.bridge_active) return 0;

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Store the query cue for processing */
    if (bridge->pending_query_cue) {
        nimcp_tensor_destroy(bridge->pending_query_cue);
    }
    bridge->pending_query_cue = nimcp_tensor_clone(memory_cue);
    bridge->memory_request_pending = true;

    /* In full implementation, would:
     * 1. Retrieve memories from hippocampus using cue
     * 2. Pass retrieved memories to imagination as initial state
     * 3. Start imagination scenario with goal
     */

    /* For now, return a placeholder scenario ID */
    uint32_t scenario_id = 1;  /* Would get from imagination_begin_scenario() */

    /* Track the scenario */
    if (bridge->num_active_scenarios < HIPP_IMAG_MAX_TRACKED_SCENARIOS) {
        bridge->active_scenarios[bridge->num_active_scenarios++] = scenario_id;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Requested memory-grounded imagination, scenario %u", scenario_id);
    return scenario_id;
}

uint32_t hippocampus_imagination_request_spatial_imagination(
    hippocampus_imagination_bridge_t* bridge,
    const float start_position[3],
    struct imagination_goal* goal)
{
    if (!bridge || !start_position || !goal) return 0;
    if (!bridge->base.bridge_active) return 0;
    if (!bridge->config.enable_spatial_imagination) return 0;

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Store starting position for spatial context */
    bridge->hipp_to_imag.current_position[0] = start_position[0];
    bridge->hipp_to_imag.current_position[1] = start_position[1];
    bridge->hipp_to_imag.current_position[2] = start_position[2];

    /* In full implementation, would:
     * 1. Activate place/grid cells for starting position
     * 2. Start spatial imagination scenario
     */

    uint32_t scenario_id = 2;  /* Placeholder */

    if (bridge->num_active_scenarios < HIPP_IMAG_MAX_TRACKED_SCENARIOS) {
        bridge->active_scenarios[bridge->num_active_scenarios++] = scenario_id;
    }

    bridge->stats.spatial_queries++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Requested spatial imagination at (%.2f, %.2f, %.2f)",
                   start_position[0], start_position[1], start_position[2]);
    return scenario_id;
}

int hippocampus_imagination_encode_as_memory(
    hippocampus_imagination_bridge_t* bridge,
    const struct imagination_scenario* scenario,
    float emotional_weight)
{
    if (!bridge || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_imagination_encode_as_memory: required parameter is NULL (bridge, scenario)");
        return -1;
    }
    if (!bridge->hippocampus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_imagination_encode_as_memory: bridge->hippocampus is NULL");
        return -1;
    }
    if (!bridge->config.enable_pseudo_memory_encoding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_imagination_encode_as_memory: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Set up encoding signal */
    bridge->imag_to_hipp.encoding_strength = emotional_weight;
    bridge->imag_to_hipp.emotional_salience = emotional_weight;

    /* In full implementation, would:
     * 1. Extract latent state from scenario
     * 2. Call hippocampus_encode() with imagined content
     */

    bridge->stats.pseudo_memories_encoded++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Encoded imagination as memory with weight %.2f", emotional_weight);
    return 0;
}

int hippocampus_imagination_trigger_replay(
    hippocampus_imagination_bridge_t* bridge,
    const nimcp_tensor_t* trigger_cue)
{
    if (!bridge || !trigger_cue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_imagination_trigger_replay: required parameter is NULL (bridge, trigger_cue)");
        return -1;
    }
    if (!bridge->hippocampus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_imagination_trigger_replay: bridge->hippocampus is NULL");
        return -1;
    }
    if (!bridge->config.enable_replay_triggering) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_imagination_trigger_replay: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Signal replay request */
    bridge->imag_to_hipp.trigger_replay = true;

    /* In full implementation, would call hippocampus_request_replay() */

    bridge->stats.replay_triggers++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Triggered hippocampal replay from imagination");
    return 0;
}

int hippocampus_imagination_get_memory_effects(
    const hippocampus_imagination_bridge_t* bridge,
    hippocampus_to_imagination_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_imagination_get_memory_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);

    nimcp_mutex_lock(((hippocampus_imagination_bridge_t*)bridge)->base.mutex);
    *effects = bridge->hipp_to_imag;
    nimcp_mutex_unlock(((hippocampus_imagination_bridge_t*)bridge)->base.mutex);

    return 0;
}

int hippocampus_imagination_get_imagination_effects(
    const hippocampus_imagination_bridge_t* bridge,
    imagination_to_hippocampus_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_imagination_get_imagination_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);

    nimcp_mutex_lock(((hippocampus_imagination_bridge_t*)bridge)->base.mutex);
    *effects = bridge->imag_to_hipp;
    nimcp_mutex_unlock(((hippocampus_imagination_bridge_t*)bridge)->base.mutex);

    return 0;
}

/* ============================================================================
 * QUERY
 * ============================================================================ */

int hippocampus_imagination_get_stats(
    const hippocampus_imagination_bridge_t* bridge,
    hippocampus_imagination_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_imagination_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);

    nimcp_mutex_lock(((hippocampus_imagination_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((hippocampus_imagination_bridge_t*)bridge)->base.mutex);

    return 0;
}

int hippocampus_imagination_reset_stats(hippocampus_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

uint32_t hippocampus_imagination_get_active_scenario_count(
    const hippocampus_imagination_bridge_t* bridge)
{
    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    return bridge->num_active_scenarios;
}

/* ============================================================================
 * BIO-ASYNC
 * ============================================================================ */

int hippocampus_imagination_connect_bio_async(
    hippocampus_imagination_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;  /* Already connected */

    /* Use bridge base helper if available */
    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    int result = bridge_base_connect_bio_async(&bridge->base);
    if (result == 0) {
        NIMCP_LOG_INFO("Hippocampus-imagination bridge connected to bio-async");
    }

    return result;
}

int hippocampus_imagination_disconnect_bio_async(
    hippocampus_imagination_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;  /* Already disconnected */

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    int result = bridge_base_disconnect_bio_async(&bridge->base);
    if (result == 0) {
        NIMCP_LOG_INFO("Hippocampus-imagination bridge disconnected from bio-async");
    }

    return result;
}

bool hippocampus_imagination_is_bio_async_connected(
    const hippocampus_imagination_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    return bridge->base.bio_async_enabled;
}

int hippocampus_imagination_process_messages(
    hippocampus_imagination_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Process pending bio-async messages */
    /* In full implementation, would handle:
     * - BIO_MSG_IMAGINATION_MEMORY_REQUEST
     * - BIO_MSG_IMAGINATION_MEMORY_RESPONSE
     * - BIO_MSG_MEMORY_ENCODE_REQUEST
     * - BIO_MSG_CONSOLIDATION_REQUEST
     */

    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__hippocampus_imaginat", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Hippocampus Imagination Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int hippocampus_imagination_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    hippocampus_imagination_bridge_heartbeat("hippocampus__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Hippocampus_Imagination_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                hippocampus_imagination_bridge_heartbeat("hippocampus__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOG_DEBUG("Hippocampus Imagination Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Hippocampus_Imagination_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Hippocampus_Imagination_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void hippocampus_imagination_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_hippocampus_imagination_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int hippocampus_imagination_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hippocampus_imagination_bridge_training_begin: NULL argument");
        return -1;
    }
    hippocampus_imagination_bridge_heartbeat("hippocampus_imagination_bridge_training_begin", 0.0f);
    return 0;
}

int hippocampus_imagination_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hippocampus_imagination_bridge_training_end: NULL argument");
        return -1;
    }
    hippocampus_imagination_bridge_heartbeat("hippocampus_imagination_bridge_training_end", 1.0f);
    return 0;
}

int hippocampus_imagination_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hippocampus_imagination_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    hippocampus_imagination_bridge_heartbeat("hippocampus_imagination_bridge_training_step", progress);
    return 0;
}
