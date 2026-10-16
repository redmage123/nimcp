/**
 * @file nimcp_sensory_swarm_bridge.c
 * @brief Unified Sensory-Swarm Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 *
 * @author NIMCP Development Team
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/sensory_integration/nimcp_sensory_swarm_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE_MESH_ONLY(sensory_swarm_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "SENSORY_SWARM_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct sensory_swarm_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    sensory_swarm_config_t config;

    /* Registered modules */
    nimcp_somatosensory_t* soma;
    nimcp_olfactory_t* olfact;
    nimcp_gustatory_t* gust;

    /* Swarm nodes */
    sensory_swarm_node_t* nodes;
    uint32_t num_nodes;
    uint32_t next_node_id;

    /* Tasks */
    sensory_swarm_task_t* tasks;
    uint32_t num_tasks;
    uint32_t next_task_id;

    /* Statistics */
    sensory_swarm_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp(void) {
    static _Atomic uint64_t counter = 0;
    return counter++;
}

static float randf(void) {
    return (float)nimcp_tl_rand() / (float)RAND_MAX;
}

static sensory_swarm_node_t* find_node(sensory_swarm_bridge_t* bridge, uint32_t node_id) {
    for (uint32_t i = 0; i < bridge->num_nodes; i++) {
        if (bridge->nodes[i].node_id == node_id && bridge->nodes[i].active) {
            return &bridge->nodes[i];
        }
    }
    return NULL;  /* Not found is a normal condition */
}

static sensory_swarm_task_t* find_task(sensory_swarm_bridge_t* bridge, uint32_t task_id) {
    for (uint32_t i = 0; i < bridge->num_tasks; i++) {
        if (bridge->tasks[i].task_id == task_id) {
            return &bridge->tasks[i];
        }
    }
    return NULL;  /* Not found is a normal condition */
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int sensory_swarm_default_config(sensory_swarm_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(sensory_swarm_config_t));

    config->max_nodes = SENSORY_SWARM_MAX_NODES;
    config->max_tasks = SENSORY_SWARM_MAX_TASKS;
    config->consensus_quorum = SENSORY_SWARM_CONSENSUS_QUORUM;
    config->node_timeout_ms = 5000.0f;

    config->enable_touch_swarm = true;
    config->enable_smell_swarm = true;
    config->enable_taste_swarm = true;

    config->exploration_step_size = 0.1f;
    config->tracking_gain = 0.5f;
    config->consensus_decay = 0.95f;

    config->enable_logging = false;

    return 0;
}

sensory_swarm_bridge_t* sensory_swarm_bridge_create(const sensory_swarm_config_t* config) {
    sensory_swarm_bridge_t* bridge = (sensory_swarm_bridge_t*)nimcp_calloc(1, sizeof(sensory_swarm_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sensory_swarm_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "sensory_swarm");

    if (config) {
        memcpy(&bridge->config, config, sizeof(sensory_swarm_config_t));
    } else {
        sensory_swarm_default_config(&bridge->config);
    }

    bridge->nodes = (sensory_swarm_node_t*)nimcp_calloc(bridge->config.max_nodes, sizeof(sensory_swarm_node_t));
    bridge->tasks = (sensory_swarm_task_t*)nimcp_calloc(bridge->config.max_tasks, sizeof(sensory_swarm_task_t));

    if (!bridge->nodes || !bridge->tasks) {
        nimcp_free(bridge->nodes);
        nimcp_free(bridge->tasks);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sensory_swarm_bridge_create: sub-allocation failed");
        return NULL;
    }

    bridge->next_node_id = 1;
    bridge->next_task_id = 1;

    NIMCP_LOGGING_INFO("Created %s bridge", "sensory_swarm");
    return bridge;
}

void sensory_swarm_bridge_destroy(sensory_swarm_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "sensory_swarm");

    /* Free task resources */
    for (uint32_t i = 0; i < bridge->num_tasks; i++) {
        nimcp_free(bridge->tasks[i].input_data);
        nimcp_free(bridge->tasks[i].target_position);
        nimcp_free(bridge->tasks[i].assigned_nodes);
        nimcp_free(bridge->tasks[i].node_results);
        nimcp_free(bridge->tasks[i].node_confidences);
    }

    nimcp_free(bridge->nodes);
    nimcp_free(bridge->tasks);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Module Registration Implementation
 * ============================================================================ */

int sensory_swarm_register_somatosensory(sensory_swarm_bridge_t* bridge, nimcp_somatosensory_t* soma) {
    if (!bridge || !soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_register_somatosensory: required parameter is NULL (bridge, soma)");
        return -1;
    }
    bridge->soma = soma;
    return 0;
}

int sensory_swarm_register_olfactory(sensory_swarm_bridge_t* bridge, nimcp_olfactory_t* olfact) {
    if (!bridge || !olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_register_olfactory: required parameter is NULL (bridge, olfact)");
        return -1;
    }
    bridge->olfact = olfact;
    return 0;
}

int sensory_swarm_register_gustatory(sensory_swarm_bridge_t* bridge, nimcp_gustatory_t* gust) {
    if (!bridge || !gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_register_gustatory: required parameter is NULL (bridge, gust)");
        return -1;
    }
    bridge->gust = gust;
    return 0;
}

/* ============================================================================
 * Node Management Implementation
 * ============================================================================ */

int sensory_swarm_add_node(sensory_swarm_bridge_t* bridge, sensory_swarm_modality_t modality,
                           const float* position, uint32_t* node_id) {
    if (!bridge || !node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_register_gustatory: required parameter is NULL (bridge, node_id)");
        return -1;
    }
    if (bridge->num_nodes >= bridge->config.max_nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "sensory_swarm_register_gustatory: capacity exceeded");
        return -1;
    }

    sensory_swarm_node_t* node = &bridge->nodes[bridge->num_nodes++];

    node->node_id = bridge->next_node_id++;
    node->modality = modality;
    if (position) {
        node->position[0] = position[0];
        node->position[1] = position[1];
        node->position[2] = position[2];
    }
    node->sensory_value = 0.0f;
    node->confidence = 0.0f;
    node->active = true;
    node->last_update = get_timestamp();

    *node_id = node->node_id;
    bridge->stats.active_nodes++;

    return 0;
}

int sensory_swarm_remove_node(sensory_swarm_bridge_t* bridge, uint32_t node_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_remove_node: bridge is NULL");
        return -1;
    }

    sensory_swarm_node_t* node = find_node(bridge, node_id);
    if (!node) {
        return -1;  /* Node not found is not an immune-worthy error */
    }

    node->active = false;
    bridge->stats.active_nodes--;

    return 0;
}

int sensory_swarm_update_node(sensory_swarm_bridge_t* bridge, uint32_t node_id,
                              float sensory_value, float confidence) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_remove_node: bridge is NULL");
        return -1;
    }

    sensory_swarm_node_t* node = find_node(bridge, node_id);
    if (!node) {
        return -1;  /* Node not found is not an immune-worthy error */
    }

    node->sensory_value = sensory_value;
    node->confidence = confidence;
    node->last_update = get_timestamp();

    return 0;
}

int sensory_swarm_get_node_count(const sensory_swarm_bridge_t* bridge, sensory_swarm_modality_t modality) {
    if (!bridge) return 0;

    int count = 0;
    for (uint32_t i = 0; i < bridge->num_nodes; i++) {
        if (bridge->nodes[i].active && bridge->nodes[i].modality == modality) {
            count++;
        }
    }
    return count;
}

/* ============================================================================
 * Task Management Implementation
 * ============================================================================ */

int sensory_swarm_submit_task(sensory_swarm_bridge_t* bridge, sensory_swarm_task_type_t type,
                              sensory_swarm_modality_t modality, const float* input,
                              uint32_t input_dim, uint32_t* task_id) {
    if (!bridge || !task_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_get_node_count: required parameter is NULL (bridge, task_id)");
        return -1;
    }
    if (bridge->num_tasks >= bridge->config.max_tasks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "sensory_swarm_get_node_count: capacity exceeded");
        return -1;
    }

    sensory_swarm_task_t* task = &bridge->tasks[bridge->num_tasks++];
    memset(task, 0, sizeof(sensory_swarm_task_t));

    task->task_id = bridge->next_task_id++;
    task->type = type;
    task->modality = modality;
    task->status = SENSORY_SWARM_STATUS_PENDING;

    if (input && input_dim > 0) {
        task->input_data = (float*)nimcp_calloc(input_dim, sizeof(float));
        if (!task->input_data) {
            bridge->num_tasks--;  /* Roll back task slot */
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sensory_swarm_submit_task: input_data allocation failed");
            return -1;
        }
        memcpy(task->input_data, input, input_dim * sizeof(float));
        task->input_dim = input_dim;
    }

    /* Assign nodes of matching modality */
    uint32_t modality_count = sensory_swarm_get_node_count(bridge, modality);
    if (modality_count > 0) {
        task->assigned_nodes = (uint32_t*)nimcp_calloc(modality_count, sizeof(uint32_t));
        task->node_results = (float*)nimcp_calloc(modality_count, sizeof(float));
        task->node_confidences = (float*)nimcp_calloc(modality_count, sizeof(float));

        if (task->assigned_nodes && task->node_results && task->node_confidences) {
            uint32_t idx = 0;
            for (uint32_t i = 0; i < bridge->num_nodes && idx < modality_count; i++) {
                if (bridge->nodes[i].active && bridge->nodes[i].modality == modality) {
                    task->assigned_nodes[idx++] = bridge->nodes[i].node_id;
                }
            }
            task->num_assigned = idx;
        }
    }

    task->start_time = get_timestamp();
    task->status = SENSORY_SWARM_STATUS_DISTRIBUTED;

    *task_id = task->task_id;
    bridge->stats.active_tasks++;

    /* Track by modality */
    switch (modality) {
        case SENSORY_SWARM_MODALITY_TOUCH:
            bridge->stats.touch_tasks++;
            break;
        case SENSORY_SWARM_MODALITY_SMELL:
            bridge->stats.smell_tasks++;
            break;
        case SENSORY_SWARM_MODALITY_TASTE:
            bridge->stats.taste_tasks++;
            break;
        default:
            break;
    }

    return 0;
}

int sensory_swarm_get_task_status(sensory_swarm_bridge_t* bridge, uint32_t task_id,
                                  sensory_swarm_task_status_t* status) {
    if (!bridge || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_get_node_count: required parameter is NULL (bridge, status)");
        return -1;
    }

    sensory_swarm_task_t* task = find_task(bridge, task_id);
    if (!task) {
        return -1;  /* Task not found is not an immune-worthy error */
    }

    *status = task->status;
    return 0;
}

int sensory_swarm_get_task_result(sensory_swarm_bridge_t* bridge, uint32_t task_id,
                                  float* result, float* confidence) {
    if (!bridge || !result || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_get_node_count: required parameter is NULL (bridge, result, confidence)");
        return -1;
    }

    sensory_swarm_task_t* task = find_task(bridge, task_id);
    if (!task) {
        return -1;  /* Task not found is not an immune-worthy error */
    }
    if (task->status != SENSORY_SWARM_STATUS_COMPLETE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sensory_swarm_get_node_count: validation failed");
        return -1;
    }

    *result = task->aggregated_result;
    *confidence = task->consensus_confidence;

    return 0;
}

int sensory_swarm_cancel_task(sensory_swarm_bridge_t* bridge, uint32_t task_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_cancel_task: bridge is NULL");
        return -1;
    }

    sensory_swarm_task_t* task = find_task(bridge, task_id);
    if (!task) {
        return -1;  /* Task not found is not an immune-worthy error */
    }

    task->status = SENSORY_SWARM_STATUS_FAILED;
    bridge->stats.active_tasks--;
    bridge->stats.tasks_failed++;

    return 0;
}

/* ============================================================================
 * Exploration Implementation
 * ============================================================================ */

int sensory_swarm_explore_tactile(sensory_swarm_bridge_t* bridge, const float* bounds,
                                  sensory_swarm_exploration_t* result) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_cancel_task: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enable_touch_swarm) {
        return -1;  /* Feature disabled, not an error worth reporting to immune */
    }
    (void)bounds;

    /* Allocate result */
    result->map_dim[0] = 10;
    result->map_dim[1] = 10;
    result->map_dim[2] = 1;
    uint32_t map_size = result->map_dim[0] * result->map_dim[1] * result->map_dim[2];

    result->explored_map = (float*)nimcp_calloc(map_size, sizeof(float));
    if (!result->explored_map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sensory_swarm_cancel_task: result->explored_map is NULL");
        return -1;
    }

    /* Simulate distributed exploration */
    uint32_t touch_nodes = sensory_swarm_get_node_count(bridge, SENSORY_SWARM_MODALITY_TOUCH);
    float coverage_per_node = 1.0f / (touch_nodes > 0 ? touch_nodes : 1);

    for (uint32_t i = 0; i < map_size; i++) {
        result->explored_map[i] = randf() * (touch_nodes > 0 ? 1.0f : 0.2f);
    }

    result->coverage = touch_nodes * coverage_per_node;
    if (result->coverage > 1.0f) result->coverage = 1.0f;

    result->interesting_points = (uint32_t)(map_size * 0.1f);
    result->num_hotspots = result->interesting_points / 2;

    if (result->num_hotspots > 0) {
        result->hotspot_positions = (float*)nimcp_calloc(result->num_hotspots * 3, sizeof(float));
        if (result->hotspot_positions) {
            for (uint32_t i = 0; i < result->num_hotspots; i++) {
                result->hotspot_positions[i * 3] = randf() * 10.0f;
                result->hotspot_positions[i * 3 + 1] = randf() * 10.0f;
                result->hotspot_positions[i * 3 + 2] = 0.0f;
            }
        }
    }

    return 0;
}

int sensory_swarm_explore_olfactory(sensory_swarm_bridge_t* bridge, const float* bounds,
                                    sensory_swarm_exploration_t* result) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_cancel_task: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enable_smell_swarm) {
        return -1;  /* Feature disabled, not an error worth reporting to immune */
    }
    (void)bounds;

    /* Similar to tactile but for olfactory space */
    result->map_dim[0] = 8;
    result->map_dim[1] = 8;
    result->map_dim[2] = 8;
    uint32_t map_size = result->map_dim[0] * result->map_dim[1] * result->map_dim[2];

    result->explored_map = (float*)nimcp_calloc(map_size, sizeof(float));
    if (!result->explored_map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sensory_swarm_cancel_task: result->explored_map is NULL");
        return -1;
    }

    uint32_t smell_nodes = sensory_swarm_get_node_count(bridge, SENSORY_SWARM_MODALITY_SMELL);

    for (uint32_t i = 0; i < map_size; i++) {
        result->explored_map[i] = randf() * (smell_nodes > 0 ? 1.0f : 0.2f);
    }

    result->coverage = (bridge->config.max_nodes > 0) ? ((float)smell_nodes / bridge->config.max_nodes) : 0.0f;
    result->num_hotspots = 2;
    result->interesting_points = 5;

    return 0;
}

/* ============================================================================
 * Tracking Implementation
 * ============================================================================ */

int sensory_swarm_track_odor(sensory_swarm_bridge_t* bridge, const float* initial_position,
                             sensory_swarm_tracking_t* result) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_cancel_task: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enable_smell_swarm) {
        return -1;  /* Feature disabled, not an error worth reporting to immune */
    }

    memset(result, 0, sizeof(sensory_swarm_tracking_t));

    if (initial_position) {
        result->source_position[0] = initial_position[0] + randf() * 2.0f - 1.0f;
        result->source_position[1] = initial_position[1] + randf() * 2.0f - 1.0f;
        result->source_position[2] = initial_position[2] + randf() * 0.5f;
    }

    uint32_t smell_nodes = sensory_swarm_get_node_count(bridge, SENSORY_SWARM_MODALITY_SMELL);
    result->source_confidence = smell_nodes > 5 ? 0.8f : (float)smell_nodes / 10.0f;

    result->gradient_direction[0] = randf() * 2.0f - 1.0f;
    result->gradient_direction[1] = randf() * 2.0f - 1.0f;
    result->gradient_direction[2] = randf() * 0.5f;
    result->gradient_magnitude = randf() * 0.5f + 0.2f;

    result->source_found = (result->source_confidence > 0.7f);

    return 0;
}

int sensory_swarm_track_texture(sensory_swarm_bridge_t* bridge, float target_texture,
                                sensory_swarm_tracking_t* result) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_cancel_task: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enable_touch_swarm) {
        return -1;  /* Feature disabled, not an error worth reporting to immune */
    }
    (void)target_texture;

    memset(result, 0, sizeof(sensory_swarm_tracking_t));

    result->source_position[0] = randf() * 10.0f;
    result->source_position[1] = randf() * 10.0f;
    result->source_confidence = randf() * 0.6f + 0.2f;
    result->source_found = (result->source_confidence > 0.6f);

    return 0;
}

/* ============================================================================
 * Consensus Implementation
 * ============================================================================ */

int sensory_swarm_build_consensus(sensory_swarm_bridge_t* bridge, sensory_swarm_modality_t modality,
                                  float* consensus_value, float* confidence) {
    if (!bridge || !consensus_value || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_cancel_task: required parameter is NULL (bridge, consensus_value, confidence)");
        return -1;
    }

    float sum = 0.0f;
    float weight_sum = 0.0f;
    uint32_t contributors = 0;

    for (uint32_t i = 0; i < bridge->num_nodes; i++) {
        sensory_swarm_node_t* node = &bridge->nodes[i];
        if (node->active && node->modality == modality) {
            sum += node->sensory_value * node->confidence;
            weight_sum += node->confidence;
            contributors++;
        }
    }

    if (weight_sum > 0.0f) {
        *consensus_value = sum / weight_sum;
        *confidence = weight_sum / contributors;

        /* Check quorum */
        uint32_t total_nodes = sensory_swarm_get_node_count(bridge, modality);
        float quorum = (total_nodes > 0) ? ((float)contributors / total_nodes) : 0.0f;
        if (quorum < bridge->config.consensus_quorum && fabsf(bridge->config.consensus_quorum) > 1e-10f) {
            *confidence *= quorum / bridge->config.consensus_quorum;
        }
    } else {
        *consensus_value = 0.0f;
        *confidence = 0.0f;
    }

    {
        float new_conf = bridge->stats.avg_consensus_confidence * 0.9f + *confidence * 0.1f;
        if (isfinite(new_conf)) bridge->stats.avg_consensus_confidence = new_conf;
    }

    return 0;
}

int sensory_swarm_evaluate_food(sensory_swarm_bridge_t* bridge, const taste_stimulus_t* stimulus,
                                float* quality, float* confidence) {
    if (!bridge || !stimulus || !quality || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_cancel_task: required parameter is NULL (bridge, stimulus, quality, confidence)");
        return -1;
    }
    if (!bridge->config.enable_taste_swarm) {
        return -1;  /* Feature disabled, not an error worth reporting to immune */
    }

    /* Compute quality from taste profile */
    float reward = stimulus->sweet * 0.3f + stimulus->umami * 0.25f +
                   stimulus->salty * 0.15f - stimulus->bitter * 0.2f -
                   stimulus->sour * 0.1f;

    *quality = (reward + 0.5f);  /* Shift to [0, 1] */
    if (*quality < 0.0f) *quality = 0.0f;
    if (*quality > 1.0f) *quality = 1.0f;

    uint32_t taste_nodes = sensory_swarm_get_node_count(bridge, SENSORY_SWARM_MODALITY_TASTE);
    *confidence = taste_nodes > 3 ? 0.8f : (float)taste_nodes / 5.0f;

    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int sensory_swarm_update(sensory_swarm_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_update: bridge is NULL");
        return -1;
    }
    (void)dt;

    /* Process pending tasks */
    for (uint32_t i = 0; i < bridge->num_tasks; i++) {
        sensory_swarm_task_t* task = &bridge->tasks[i];

        if (task->status == SENSORY_SWARM_STATUS_DISTRIBUTED) {
            task->status = SENSORY_SWARM_STATUS_PROCESSING;
        } else if (task->status == SENSORY_SWARM_STATUS_PROCESSING) {
            /* Simulate node results */
            for (uint32_t j = 0; j < task->num_assigned; j++) {
                task->node_results[j] = randf();
                task->node_confidences[j] = randf() * 0.5f + 0.5f;
            }
            task->status = SENSORY_SWARM_STATUS_AGGREGATING;
        } else if (task->status == SENSORY_SWARM_STATUS_AGGREGATING) {
            /* Aggregate results */
            float sum = 0.0f;
            float conf_sum = 0.0f;
            for (uint32_t j = 0; j < task->num_assigned; j++) {
                sum += task->node_results[j] * task->node_confidences[j];
                conf_sum += task->node_confidences[j];
            }

            if (conf_sum > 0.0f) {
                task->aggregated_result = sum / conf_sum;
                task->consensus_confidence = conf_sum / task->num_assigned;
            }

            task->completion_time = get_timestamp();
            task->status = SENSORY_SWARM_STATUS_COMPLETE;

            bridge->stats.active_tasks--;
            bridge->stats.tasks_completed++;
        }
    }

    return 0;
}

int sensory_swarm_synchronize(sensory_swarm_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_synchronize: bridge is NULL");
        return -1;
    }

    /* Update all node timestamps */
    uint64_t now = get_timestamp();
    for (uint32_t i = 0; i < bridge->num_nodes; i++) {
        if (bridge->nodes[i].active) {
            bridge->nodes[i].last_update = now;
        }
    }

    return 0;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int sensory_swarm_get_stats(const sensory_swarm_bridge_t* bridge, sensory_swarm_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(sensory_swarm_stats_t));
    return 0;
}

int sensory_swarm_reset_stats(sensory_swarm_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_swarm_reset_stats: bridge is NULL");
        return -1;
    }

    uint32_t active = bridge->stats.active_nodes;
    uint32_t active_tasks = bridge->stats.active_tasks;

    memset(&bridge->stats, 0, sizeof(sensory_swarm_stats_t));

    bridge->stats.active_nodes = active;
    bridge->stats.active_tasks = active_tasks;

    return 0;
}

void sensory_swarm_print_summary(const sensory_swarm_bridge_t* bridge) {
    if (!bridge) return;

    printf("=== Sensory Swarm Bridge Summary ===\n");
    printf("Active Nodes: %u\n", bridge->stats.active_nodes);
    printf("Active Tasks: %u\n", bridge->stats.active_tasks);
    printf("Completed: %lu, Failed: %lu\n",
           (unsigned long)bridge->stats.tasks_completed,
           (unsigned long)bridge->stats.tasks_failed);
    printf("Touch Tasks: %lu\n", (unsigned long)bridge->stats.touch_tasks);
    printf("Smell Tasks: %lu\n", (unsigned long)bridge->stats.smell_tasks);
    printf("Taste Tasks: %lu\n", (unsigned long)bridge->stats.taste_tasks);
    printf("Avg Consensus Confidence: %.2f\n", bridge->stats.avg_consensus_confidence);
    printf("====================================\n");
}

/* ============================================================================
 * Result Cleanup
 * ============================================================================ */

void sensory_swarm_exploration_free(sensory_swarm_exploration_t* result) {
    if (!result) return;
    nimcp_free(result->explored_map);
    nimcp_free(result->hotspot_positions);
    result->explored_map = NULL;
    result->hotspot_positions = NULL;
}
