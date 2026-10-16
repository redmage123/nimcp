/**
 * @file nimcp_rcog_imagination_bridge.c
 * @brief Imagination Engine Integration Bridge Implementation for Recursive Cognition
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "cognitive/recursive/nimcp_rcog_imagination_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rcog_imagination_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from rcog_imagination_bridge module (instance-level) */
static inline void rcog_imagination_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_rcog_imagination_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_rcog_imagination_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_rcog_imagination_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

static nimcp_health_agent_t* g_rcog_imagination_bridge_instance_health_agent = NULL;

void rcog_imagination_bridge_set_instance_health_agent(
    rcog_imagination_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    (void)bridge;
    g_rcog_imagination_bridge_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int rcog_imagination_bridge_training_begin(rcog_imagination_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_imagination_bridge_training_begin: NULL argument");
        return -1;
    }
    rcog_imagination_bridge_heartbeat_instance(
        g_rcog_imagination_bridge_instance_health_agent, "rcog_imag_training_begin", 0.0f);
    return 0;
}

int rcog_imagination_bridge_training_step(rcog_imagination_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_imagination_bridge_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    rcog_imagination_bridge_heartbeat_instance(
        g_rcog_imagination_bridge_instance_health_agent, "rcog_imag_training_step", clamped);
    return 0;
}

int rcog_imagination_bridge_training_end(rcog_imagination_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_imagination_bridge_training_end: NULL argument");
        return -1;
    }
    rcog_imagination_bridge_heartbeat_instance(
        g_rcog_imagination_bridge_instance_health_agent, "rcog_imag_training_end", 1.0f);
    return 0;
}

#define LOG_MODULE "RCOG_IMAGINATION_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Imagination bridge internal structure
 */
struct rcog_imagination_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    rcog_imagination_bridge_config_t config;

    /* Connections */
    struct imagination_engine* imagination;
    struct rcog_engine* engine;
    bool connected;

    /* Effects state */
    rcog_to_imagination_effects_t outgoing_effects;
    imagination_to_rcog_effects_t incoming_effects;

    /* Simulation results storage */
    rcog_simulation_result_t simulation_results[RCOG_IMAG_MAX_SIMULATED_DECOMPOSITIONS];
    size_t num_simulation_results;

    /* Rehearsal results storage */
    rcog_rehearsal_result_t rehearsal_results[RCOG_IMAG_MAX_REHEARSED_SUBTASKS];
    size_t num_rehearsal_results;

    /* Statistics */
    rcog_imagination_bridge_stats_t stats;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

rcog_imagination_bridge_config_t rcog_imagination_bridge_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_default_config", 0.0f);


    rcog_imagination_bridge_config_t config = {0};

    config.simulation_threshold = RCOG_IMAG_DEFAULT_SIMULATION_THRESHOLD;
    config.max_simulation_depth = RCOG_IMAG_DEFAULT_IMAGINATION_DEPTH;
    config.simulation_timeout_ms = 1000.0f;

    config.enable_automatic_rehearsal = true;
    config.rehearsal_threshold = 0.5f;
    config.max_rehearsals_per_batch = RCOG_IMAG_MAX_REHEARSED_SUBTASKS;

    config.default_mode = RCOG_IMAG_MODE_DIRECTED;
    config.enable_creative_mode = true;

    config.max_imagination_cpu_fraction = 0.3f;
    config.max_concurrent_scenarios = 4;

    return config;
}

rcog_imagination_bridge_t* rcog_imagination_bridge_create(
    const rcog_imagination_bridge_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_create", 0.0f);


    rcog_imagination_bridge_t* bridge = nimcp_calloc(1, sizeof(rcog_imagination_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = rcog_imagination_bridge_default_config();
    }

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "rcog_imagination") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "rcog_imagination_bridge_create: validation failed");
        return NULL;
    }

    return bridge;
}

rcog_imagination_bridge_t* rcog_imagination_bridge_create_default(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_create_default", 0.0f);


    return rcog_imagination_bridge_create(NULL);
}

void rcog_imagination_bridge_destroy(rcog_imagination_bridge_t* bridge) {
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "rcog_imagination");
    }

    /* Cleanup base bridge infrastructure */
    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_destroy", 0.0f);


    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

int rcog_imagination_bridge_connect(
    rcog_imagination_bridge_t* bridge,
    struct imagination_engine* imagination
) {
    if (!bridge || !imagination) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_connect", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->imagination = imagination;
    bridge->connected = (bridge->imagination != NULL && bridge->engine != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

int rcog_imagination_bridge_connect_engine(
    rcog_imagination_bridge_t* bridge,
    struct rcog_engine* engine
) {
    if (!bridge || !engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_connect_engine", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->engine = engine;
    bridge->connected = (bridge->imagination != NULL && bridge->engine != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

bool rcog_imagination_bridge_is_connected(const rcog_imagination_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_is_connected", 0.0f);


    return bridge && bridge->connected;
}

/*=============================================================================
 * UPDATE
 *===========================================================================*/

int rcog_imagination_bridge_update(
    rcog_imagination_bridge_t* bridge,
    float delta_time_ms
) {
    if (!bridge) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_update", 0.0f);


    (void)delta_time_ms;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset request flags */
    bridge->outgoing_effects.request_decomposition_simulation = false;
    bridge->outgoing_effects.request_subtask_rehearsal = false;
    bridge->outgoing_effects.request_counterfactual = false;
    bridge->outgoing_effects.request_answer_prediction = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

/*=============================================================================
 * SIMULATION
 *===========================================================================*/

int rcog_imagination_bridge_simulate_decompositions(
    rcog_imagination_bridge_t* bridge,
    const rcog_goal_t* goal,
    struct rcog_decomposition** decompositions,
    size_t num_decompositions,
    rcog_simulation_result_t* results
) {
    if (!bridge || !goal || !decompositions || !results) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_simulate_decompositi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return RCOG_ERROR_IMAGINATION_FAILED;
    }

    /* In full implementation, this would run imagination simulation */
    /* For now, generate plausible placeholder results */
    for (size_t i = 0; i < num_decompositions && i < RCOG_IMAG_MAX_SIMULATED_DECOMPOSITIONS; i++) {
        results[i].decomposition_id = (uint32_t)i;
        results[i].predicted_success_rate = 0.7f + (float)i * 0.05f;
        results[i].predicted_completion_time_ms = 100.0f * (float)(num_decompositions - i);
        results[i].predicted_confidence = 0.8f;
        results[i].resource_efficiency = 0.75f;
        results[i].predicted_depth = 3;
        results[i].has_deadlock_risk = false;
        results[i].risk_description = NULL;
    }

    bridge->stats.simulations_requested++;
    bridge->stats.simulations_completed++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

int rcog_imagination_bridge_rehearse_subtasks(
    rcog_imagination_bridge_t* bridge,
    struct rcog_subtask** subtasks,
    size_t num_subtasks,
    rcog_rehearsal_result_t* results
) {
    if (!bridge || !subtasks || !results) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_rehearse_subtasks", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return RCOG_ERROR_IMAGINATION_FAILED;
    }

    /* Generate placeholder rehearsal results */
    for (size_t i = 0; i < num_subtasks && i < RCOG_IMAG_MAX_REHEARSED_SUBTASKS; i++) {
        results[i].subtask_id = i;
        results[i].predicted_success = 0.85f;
        results[i].predicted_duration_ms = 50.0f;
        results[i].predicted_confidence = 0.8f;
        results[i].should_execute = true;
        results[i].imagined_result_quality = 0.75f;
    }

    bridge->stats.rehearsals_requested++;
    bridge->stats.rehearsals_completed++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

int rcog_imagination_bridge_counterfactual_analysis(
    rcog_imagination_bridge_t* bridge,
    const struct rcog_decomposition* actual,
    struct rcog_decomposition** alternatives,
    size_t num_alternatives,
    float* benefit_scores
) {
    if (!bridge || !actual || !alternatives || !benefit_scores) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_counterfactual_analy", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return RCOG_ERROR_IMAGINATION_FAILED;
    }

    /* Generate placeholder benefit scores */
    for (size_t i = 0; i < num_alternatives; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_alternatives > 256) {
            rcog_imagination_bridge_heartbeat("rcog_imagina_loop",
                             (float)(i + 1) / (float)num_alternatives);
        }

        benefit_scores[i] = 0.1f * (float)i; /* First alternatives have lower benefit */
    }

    bridge->stats.counterfactuals_analyzed++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

int rcog_imagination_bridge_predict_answer(
    rcog_imagination_bridge_t* bridge,
    const rcog_goal_t* goal,
    float* predicted_confidence,
    uint32_t* predicted_steps
) {
    if (!bridge || !goal || !predicted_confidence || !predicted_steps) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_predict_answer", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return RCOG_ERROR_IMAGINATION_FAILED;
    }

    /* Generate placeholder predictions */
    *predicted_confidence = 0.85f;
    *predicted_steps = 5;

    bridge->incoming_effects.predicted_final_confidence = *predicted_confidence;
    bridge->incoming_effects.predicted_steps_remaining = *predicted_steps;

    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

/*=============================================================================
 * CREATIVE GENERATION
 *===========================================================================*/

int rcog_imagination_bridge_generate_creative_strategy(
    rcog_imagination_bridge_t* bridge,
    const rcog_goal_t* goal,
    const char* constraint,
    char** strategy,
    float* novelty_score
) {
    if (!bridge || !goal || !strategy || !novelty_score) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_generate_creative_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return RCOG_ERROR_IMAGINATION_FAILED;
    }

    if (!bridge->config.enable_creative_mode) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return RCOG_ERROR_TOOL_ACCESS_DENIED;
    }

    /* Placeholder creative strategy */
    (void)constraint;
    *strategy = NULL; /* Would be allocated in full implementation */
    *novelty_score = 0.6f;

    bridge->stats.creative_strategies_generated++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return RCOG_OK;
}

/*=============================================================================
 * EFFECTS ACCESS
 *===========================================================================*/

int rcog_imagination_bridge_get_outgoing_effects(
    const rcog_imagination_bridge_t* bridge,
    rcog_to_imagination_effects_t* effects
) {
    if (!bridge || !effects) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_get_outgoing_effects", 0.0f);


    nimcp_mutex_lock(((rcog_imagination_bridge_t*)bridge)->base.mutex);
    *effects = bridge->outgoing_effects;
    nimcp_mutex_unlock(((rcog_imagination_bridge_t*)bridge)->base.mutex);

    return RCOG_OK;
}

int rcog_imagination_bridge_get_incoming_effects(
    const rcog_imagination_bridge_t* bridge,
    imagination_to_rcog_effects_t* effects
) {
    if (!bridge || !effects) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_get_incoming_effects", 0.0f);


    nimcp_mutex_lock(((rcog_imagination_bridge_t*)bridge)->base.mutex);
    *effects = bridge->incoming_effects;
    nimcp_mutex_unlock(((rcog_imagination_bridge_t*)bridge)->base.mutex);

    return RCOG_OK;
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

int rcog_imagination_bridge_get_stats(
    const rcog_imagination_bridge_t* bridge,
    rcog_imagination_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_get_stats", 0.0f);


    nimcp_mutex_lock(((rcog_imagination_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((rcog_imagination_bridge_t*)bridge)->base.mutex);

    return RCOG_OK;
}

void rcog_imagination_bridge_reset_stats(rcog_imagination_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(rcog_imagination_bridge_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int rcog_imagination_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_imagination_bridge_heartbeat("rcog_imagina_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Recursive_Cognition_Imagination_Bridge_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                rcog_imagination_bridge_heartbeat("rcog_imagina_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Log self-knowledge observations */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Recursive_Cognition_Imagination_Bridge_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Recursive_Cognition_Imagination_Bridge_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}
