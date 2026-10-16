/**
 * @file nimcp_tom_social_bridge.c
 * @brief Theory of Mind - Social Cognition Bridge Implementation
 *
 * WHAT: Bidirectional integration enabling ToM to inform social responses and
 *       social cues to trigger ToM inference processes.
 *
 * WHY: Theory of Mind and social cognition are deeply interdependent. Understanding
 *      others' mental states (ToM) is essential for appropriate social responses,
 *      while social cues provide input for mental state inference.
 *
 * HOW: Maintains shared agent models that both systems can query and update.
 *      ToM inferences guide social response selection; social cues trigger
 *      ToM updates about observed agents.
 *
 * @author NIMCP Development Team
 * @date 2025-01
 */

#include "cognitive/integration/nimcp_tom_social_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(tom_social_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_tom_social_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_tom_social_bridge_mesh_registry = NULL;

nimcp_error_t tom_social_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_tom_social_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "tom_social_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "tom_social_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_tom_social_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_tom_social_bridge_mesh_registry = registry;
    return err;
}

void tom_social_bridge_mesh_unregister(void) {
    if (g_tom_social_bridge_mesh_registry && g_tom_social_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_tom_social_bridge_mesh_registry, g_tom_social_bridge_mesh_id);
        g_tom_social_bridge_mesh_id = 0;
        g_tom_social_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from tom_social_bridge module (instance-level) */
static inline void tom_social_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_tom_social_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_tom_social_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_tom_social_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "TOM_SOCIAL_BRIDGE"


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal agent model representation
 *
 * WHAT: Stores mental state model for a tracked agent
 * WHY: Shared representation for ToM and social systems
 * HOW: Contains belief/desire/intention states with confidence and timestamps
 */
typedef struct agent_model {
    uint32_t agent_id;
    float belief_state[8];      /**< Simplified belief representation */
    float desire_state[8];      /**< Simplified desire representation */
    float intention_state[8];   /**< Simplified intention representation */
    float confidence;
    uint64_t last_update;
    bool valid;
} agent_model_t;

/**
 * @brief ToM-Social bridge internal structure
 *
 * WHAT: Full bridge structure with agent tracking
 * WHY: Maintains shared agent models for ToM/social integration
 * HOW: Array of agent models with thread-safe access
 */
struct tom_social_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    tom_social_config_t config;
    agent_model_t* agents;
    size_t agent_capacity;
    size_t agent_count;
    tom_social_stats_t stats;
    bool initialized;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Find agent model by ID (unlocked)
 *
 * @param bridge Bridge instance
 * @param agent_id Agent ID to find
 * @return Pointer to agent model, NULL if not found
 */
static agent_model_t* find_agent_unlocked(tom_social_bridge_t* bridge, uint32_t agent_id) {
    for (size_t i = 0; i < bridge->agent_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->agent_count > 256) {
            tom_social_bridge_heartbeat("tom_social_b_loop",
                             (float)(i + 1) / (float)bridge->agent_count);
        }

        if (bridge->agents[i].valid && bridge->agents[i].agent_id == agent_id) {
            return &bridge->agents[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_agent_unlocked: validation failed");
    return NULL;
}

/**
 * @brief Find or create agent model (unlocked)
 *
 * @param bridge Bridge instance
 * @param agent_id Agent ID to find or create
 * @return Pointer to agent model, NULL if capacity exceeded
 */
static agent_model_t* find_or_create_agent_unlocked(tom_social_bridge_t* bridge, uint32_t agent_id) {
    /* Try to find existing */
    agent_model_t* agent = find_agent_unlocked(bridge, agent_id);
    if (agent) return agent;

    /* Check capacity */
    if (bridge->agent_count >= bridge->agent_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "find_or_create_agent_unlocked: capacity exceeded");
        return NULL;
    }

    /* Find first invalid slot or use next slot */
    for (size_t i = 0; i < bridge->agent_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->agent_capacity > 256) {
            tom_social_bridge_heartbeat("tom_social_b_loop",
                             (float)(i + 1) / (float)bridge->agent_capacity);
        }

        if (!bridge->agents[i].valid) {
            agent = &bridge->agents[i];
            break;
        }
    }

    if (!agent && bridge->agent_count < bridge->agent_capacity) {
        agent = &bridge->agents[bridge->agent_count];
    }

    if (!agent) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");


        return NULL;


    }

    /* Initialize new agent */
    memset(agent, 0, sizeof(agent_model_t));
    agent->agent_id = agent_id;
    agent->confidence = 0.5f;  /* Start with moderate confidence */
    agent->valid = true;
    bridge->agent_count++;
    bridge->stats.active_agents = (uint32_t)bridge->agent_count;

    return agent;
}

/**
 * @brief Get current timestamp in milliseconds
 *
 * @return Current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int tom_social_default_config(tom_social_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_default_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_social_bridge_heartbeat("tom_social_b_tom_social_default_c", 0.0f);


    config->inference_depth = 2;
    config->social_weight = 0.7f;
    config->agent_capacity = 32;
    config->enable_auto_inference = true;
    config->enable_response_suggestions = true;
    config->inference_confidence_threshold = 0.5f;

    return 0;
}

/* ============================================================================
 * Lifecycle Management
 * ============================================================================ */

tom_social_bridge_t* tom_social_bridge_create(const tom_social_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    tom_social_bridge_heartbeat("tom_social_b_create", 0.0f);


    tom_social_bridge_t* bridge = nimcp_malloc(sizeof(tom_social_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    memset(bridge, 0, sizeof(tom_social_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        tom_social_default_config(&bridge->config);
    }

    /* Allocate agents array */
    bridge->agent_capacity = bridge->config.agent_capacity;
    if (bridge->agent_capacity == 0) {
        bridge->agent_capacity = 32;  /* Default capacity */
    }
    if (bridge->agent_capacity > TOM_SOCIAL_MAX_AGENTS) {
        bridge->agent_capacity = TOM_SOCIAL_MAX_AGENTS;
    }

    bridge->agents = nimcp_calloc(bridge->agent_capacity, sizeof(agent_model_t));
    if (!bridge->agents) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tom_social_bridge_create: bridge->agents is NULL");
        return NULL;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "tom_social") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge->agents);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tom_social_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "tom_social");
    return bridge;
}

void tom_social_bridge_destroy(tom_social_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "tom_social");

    /* Phase 8: Heartbeat at operation start */
    tom_social_bridge_heartbeat("tom_social_b_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    if (bridge->agents) {
        nimcp_free(bridge->agents);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Core Functions
 * ============================================================================ */

int tom_social_infer_for_response(
    tom_social_bridge_t* bridge,
    uint32_t agent_id,
    tom_social_mental_state_t* mental_state_out
) {
    if (!bridge || !mental_state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_infer_for_response: required parameter is NULL (bridge, mental_state_out)");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_infer_for_response: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_social_bridge_heartbeat("tom_social_b_tom_social_infer_for", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    agent_model_t* agent = find_agent_unlocked(bridge, agent_id);
    if (!agent) {
        bridge->stats.inference_failures++;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_infer_for_response: agent is NULL");
        return -1;
    }

    /* Initialize output */
    memset(mental_state_out, 0, sizeof(tom_social_mental_state_t));
    mental_state_out->agent_id = agent_id;
    mental_state_out->confidence = agent->confidence;
    mental_state_out->last_update_ms = agent->last_update;

    /* Derive mental state from belief/desire/intention states */
    /* Compute emotional valence from belief state (average of first 4 beliefs) */
    float valence_sum = 0.0f;
    for (int i = 0; i < 4; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 4 > 256) {
            tom_social_bridge_heartbeat("tom_social_b_loop",
                             (float)(i + 1) / (float)4);
        }

        valence_sum += agent->belief_state[i];
    }
    mental_state_out->emotional_valence = (valence_sum / 4.0f) * bridge->config.social_weight;

    /* Compute arousal from intention state (average of first 4 intentions) */
    float arousal_sum = 0.0f;
    for (int i = 0; i < 4; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 4 > 256) {
            tom_social_bridge_heartbeat("tom_social_b_loop",
                             (float)(i + 1) / (float)4);
        }

        arousal_sum += agent->intention_state[i];
    }
    mental_state_out->emotional_arousal = (arousal_sum / 4.0f) * bridge->config.social_weight;

    /* Intent toward self from desire state */
    mental_state_out->intent_toward_self = agent->desire_state[0] * bridge->config.social_weight;

    /* Attention to self from intention state */
    mental_state_out->attention_to_self = agent->intention_state[0] * bridge->config.social_weight;

    /* Cognitive load from belief state complexity */
    float load_sum = 0.0f;
    for (int i = 4; i < 8; i++) {
        load_sum += agent->belief_state[i] * agent->belief_state[i];
    }
    mental_state_out->cognitive_load = (load_sum / 4.0f) * bridge->config.social_weight;

    /* Belief count based on non-zero beliefs */
    mental_state_out->belief_count = 0;
    for (int i = 0; i < 8; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 8 > 256) {
            tom_social_bridge_heartbeat("tom_social_b_loop",
                             (float)(i + 1) / (float)8);
        }

        if (agent->belief_state[i] != 0.0f) {
            mental_state_out->belief_count++;
        }
    }

    /* Update statistics */
    bridge->stats.inferences_made++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_social_on_social_cue(
    tom_social_bridge_t* bridge,
    tom_social_cue_type_t cue_type,
    const void* cue_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_on_social_cue: bridge is NULL");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_on_social_cue: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_social_bridge_heartbeat("tom_social_b_tom_social_on_social", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Process social cue to update relevant agent models */
    /* The cue_data interpretation depends on cue_type */
    /* For now, we update a generic model based on cue type */

    /* Default agent (agent 0) if no specific agent provided */
    agent_model_t* agent = find_or_create_agent_unlocked(bridge, 0);
    if (!agent) {
        bridge->stats.cue_failures++;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_on_social_cue: agent is NULL");
        return -1;
    }

    /* Update agent state based on cue type */
    switch (cue_type) {
        case TOM_SOCIAL_CUE_FACIAL_EXPRESSION:
            /* Update emotional belief state */
            agent->belief_state[0] += 0.1f;  /* Positive emotion indicator */
            if (agent->belief_state[0] > 1.0f) agent->belief_state[0] = 1.0f;
            break;

        case TOM_SOCIAL_CUE_BODY_LANGUAGE:
            /* Update intention state */
            agent->intention_state[0] += 0.1f;
            if (agent->intention_state[0] > 1.0f) agent->intention_state[0] = 1.0f;
            break;

        case TOM_SOCIAL_CUE_VOCAL_TONE:
            /* Update emotional arousal belief */
            agent->belief_state[1] += 0.1f;
            if (agent->belief_state[1] > 1.0f) agent->belief_state[1] = 1.0f;
            break;

        case TOM_SOCIAL_CUE_GAZE_DIRECTION:
            /* Update attention-related intention */
            agent->intention_state[1] += 0.1f;
            if (agent->intention_state[1] > 1.0f) agent->intention_state[1] = 1.0f;
            break;

        case TOM_SOCIAL_CUE_PROXIMITY:
            /* Update social desire state */
            agent->desire_state[0] += 0.1f;
            if (agent->desire_state[0] > 1.0f) agent->desire_state[0] = 1.0f;
            break;

        case TOM_SOCIAL_CUE_GESTURE:
            /* Update communication intention */
            agent->intention_state[2] += 0.1f;
            if (agent->intention_state[2] > 1.0f) agent->intention_state[2] = 1.0f;
            break;

        case TOM_SOCIAL_CUE_VERBAL_CONTENT:
            /* Update belief about agent's knowledge */
            agent->belief_state[4] += 0.1f;
            if (agent->belief_state[4] > 1.0f) agent->belief_state[4] = 1.0f;
            break;

        case TOM_SOCIAL_CUE_CONTEXT:
            /* Update contextual beliefs */
            agent->belief_state[5] += 0.1f;
            if (agent->belief_state[5] > 1.0f) agent->belief_state[5] = 1.0f;
            break;

        default:
            break;
    }

    agent->last_update = get_current_time_ms();
    bridge->stats.social_cues_processed++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_social_update_agent_model(
    tom_social_bridge_t* bridge,
    uint32_t agent_id,
    const tom_social_belief_update_t* belief_update
) {
    if (!bridge || !belief_update) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_update_agent_model: required parameter is NULL (bridge, belief_update)");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_update_agent_model: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_social_bridge_heartbeat("tom_social_b_tom_social_update_ag", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    agent_model_t* agent = find_or_create_agent_unlocked(bridge, agent_id);
    if (!agent) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_update_agent_model: agent is NULL");
        return -1;
    }

    /* Compute learning rate based on inference depth */
    float learning_rate = 0.1f / (float)(bridge->config.inference_depth + 1);

    /* Apply belief update with learning rate and confidence weighting */
    uint32_t belief_index = belief_update->belief_type % 8;
    float update_magnitude = belief_update->belief_value * belief_update->confidence * learning_rate;

    /* Update appropriate state based on source */
    switch (belief_update->source) {
        case 0:  /* ToM source -> update belief state */
            agent->belief_state[belief_index] += update_magnitude;
            if (agent->belief_state[belief_index] > 1.0f)
                agent->belief_state[belief_index] = 1.0f;
            if (agent->belief_state[belief_index] < -1.0f)
                agent->belief_state[belief_index] = -1.0f;
            break;

        case 1:  /* Social source -> update desire state */
            agent->desire_state[belief_index] += update_magnitude;
            if (agent->desire_state[belief_index] > 1.0f)
                agent->desire_state[belief_index] = 1.0f;
            if (agent->desire_state[belief_index] < -1.0f)
                agent->desire_state[belief_index] = -1.0f;
            break;

        case 2:  /* External source -> update intention state */
        default:
            agent->intention_state[belief_index] += update_magnitude;
            if (agent->intention_state[belief_index] > 1.0f)
                agent->intention_state[belief_index] = 1.0f;
            if (agent->intention_state[belief_index] < -1.0f)
                agent->intention_state[belief_index] = -1.0f;
            break;
    }

    /* Update agent confidence (weighted average with new update confidence) */
    agent->confidence = agent->confidence * 0.9f + belief_update->confidence * 0.1f;

    agent->last_update = get_current_time_ms();
    bridge->stats.agent_models_updated++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_social_get_agent_state(
    tom_social_bridge_t* bridge,
    uint32_t agent_id,
    tom_social_agent_state_t* state_out
) {
    if (!bridge || !state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_get_agent_state: required parameter is NULL (bridge, state_out)");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_get_agent_state: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_social_bridge_heartbeat("tom_social_b_tom_social_get_agent", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    agent_model_t* agent = find_agent_unlocked(bridge, agent_id);
    if (!agent) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_get_agent_state: agent is NULL");
        return -1;
    }

    /* Initialize output */
    memset(state_out, 0, sizeof(tom_social_agent_state_t));

    /* Copy mental state */
    state_out->mental_state.agent_id = agent->agent_id;
    state_out->mental_state.confidence = agent->confidence;
    state_out->mental_state.last_update_ms = agent->last_update;

    /* Derive mental state values from internal representation */
    float valence_sum = 0.0f;
    for (int i = 0; i < 4; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 4 > 256) {
            tom_social_bridge_heartbeat("tom_social_b_loop",
                             (float)(i + 1) / (float)4);
        }

        valence_sum += agent->belief_state[i];
    }
    state_out->mental_state.emotional_valence = valence_sum / 4.0f;

    float arousal_sum = 0.0f;
    for (int i = 0; i < 4; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 4 > 256) {
            tom_social_bridge_heartbeat("tom_social_b_loop",
                             (float)(i + 1) / (float)4);
        }

        arousal_sum += agent->intention_state[i];
    }
    state_out->mental_state.emotional_arousal = arousal_sum / 4.0f;

    state_out->mental_state.intent_toward_self = agent->desire_state[0];
    state_out->mental_state.attention_to_self = agent->intention_state[0];

    float load_sum = 0.0f;
    for (int i = 4; i < 8; i++) {
        load_sum += agent->belief_state[i] * agent->belief_state[i];
    }
    state_out->mental_state.cognitive_load = load_sum / 4.0f;

    /* Count non-zero beliefs */
    state_out->mental_state.belief_count = 0;
    for (int i = 0; i < 8; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 8 > 256) {
            tom_social_bridge_heartbeat("tom_social_b_loop",
                             (float)(i + 1) / (float)8);
        }

        if (agent->belief_state[i] != 0.0f) {
            state_out->mental_state.belief_count++;
        }
    }

    /* Compute time since observation */
    uint64_t current_time = get_current_time_ms();
    state_out->time_since_observation = current_time - agent->last_update;

    /* Set observation metadata */
    state_out->observation_count = 1;  /* Simplified - would track in real impl */
    state_out->model_stability = agent->confidence;
    state_out->is_observed = (state_out->time_since_observation < 5000);  /* Observed if updated within 5s */
    state_out->is_valid = agent->valid;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int tom_social_get_stats(
    const tom_social_bridge_t* bridge,
    tom_social_stats_t* stats_out
) {
    if (!bridge || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_get_stats: required parameter is NULL (bridge, stats_out)");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_get_stats: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_social_bridge_heartbeat("tom_social_b_tom_social_get_stats", 0.0f);


    nimcp_mutex_lock(((tom_social_bridge_t*)bridge)->base.mutex);
    *stats_out = bridge->stats;
    nimcp_mutex_unlock(((tom_social_bridge_t*)bridge)->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void tom_social_bridge_set_instance_health_agent(tom_social_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "tom_social_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int tom_social_bridge_training_begin(tom_social_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_social_bridge_training_begin: NULL argument");
        return -1;
    }
    tom_social_bridge_heartbeat_instance(bridge->health_agent, "tom_social_bridge_training_begin", 0.0f);
    return 0;
}

int tom_social_bridge_training_end(tom_social_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_social_bridge_training_end: NULL argument");
        return -1;
    }
    tom_social_bridge_heartbeat_instance(bridge->health_agent, "tom_social_bridge_training_end", 1.0f);
    return 0;
}

int tom_social_bridge_training_step(tom_social_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_social_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    tom_social_bridge_heartbeat_instance(bridge->health_agent, "tom_social_bridge_training_step", progress);
    return 0;
}
