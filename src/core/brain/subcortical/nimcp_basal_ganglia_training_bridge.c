/**
 * @file nimcp_basal_ganglia_training_bridge.c
 * @brief Basal ganglia-training module plasticity bridge implementation
 */

#include "core/brain/subcortical/nimcp_basal_ganglia_training_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/math/nimcp_math_helpers.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(basal_ganglia_training_bridge, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


#define LOG_MODULE "BASAL_GANGLIA_TRAINING_BRIDGE"


/* ============================================================================
 * Static Helpers
 * ============================================================================ */

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

void bgtr_bridge_default_config(bgtr_bridge_config_t* config) {
    if (!config) return;

    config->learning_type = BGTR_LEARN_ACTOR_CRITIC;
    config->learning_rate = BGTR_DEFAULT_LEARNING_RATE;
    config->d1_learning_rate = BGTR_DEFAULT_LEARNING_RATE;
    config->d2_learning_rate = BGTR_DEFAULT_LEARNING_RATE;
    config->trace_decay = BGTR_DEFAULT_TRACE_DECAY;
    config->da_scaling = BGTR_DEFAULT_DA_SCALE;
    config->habit_rate_scale = BGTR_HABIT_LEARNING_SCALE;
    config->max_traces = BGTR_MAX_ELIGIBILITY_TRACES;
    config->enable_eligibility = true;
    config->enable_habit_learning = true;
    config->enable_d1_d2_asymmetry = true;
}

bgtr_bridge_t* bgtr_bridge_create(
    const bgtr_bridge_config_t* config,
    uint32_t num_actions
) {
    bgtr_bridge_t* bridge = nimcp_calloc(1, sizeof(bgtr_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgtr_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply config */
    if (config) {
        bridge->config = *config;
    } else {
        bgtr_bridge_default_config(&bridge->config);
    }

    bridge->num_actions = num_actions > 0 ? num_actions : 16;

    /* Allocate eligibility traces */
    bridge->max_traces = bridge->config.max_traces;
    bridge->traces = nimcp_calloc(bridge->max_traces, sizeof(bgtr_eligibility_trace_t));
    if (!bridge->traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgtr_bridge_create: failed to allocate traces");
        nimcp_free(bridge);
        return NULL;
    }
    bridge->num_traces = 0;

    /* Initialize weight handles (will be allocated when training context connected) */
    memset(&bridge->d1_weights, 0, sizeof(nimcp_training_weights_t));
    memset(&bridge->d2_weights, 0, sizeof(nimcp_training_weights_t));
    memset(&bridge->habit_weights, 0, sizeof(nimcp_training_weights_t));

    /* Initialize learning state */
    bridge->last_rpe = 0.0f;
    bridge->last_reward = 0.0f;
    bridge->last_action = 0;
    bridge->last_action_time = 0;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "basal_ganglia_training") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "bgtr_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge->traces);
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void bgtr_bridge_destroy(bgtr_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "basal_ganglia_training");

    /* Free weights if allocated */
    if (bridge->training && bridge->d1_weights.handle) {
        nimcp_training_free_weights(bridge->training, &bridge->d1_weights);
    }
    if (bridge->training && bridge->d2_weights.handle) {
        nimcp_training_free_weights(bridge->training, &bridge->d2_weights);
    }
    if (bridge->training && bridge->habit_weights.handle) {
        nimcp_training_free_weights(bridge->training, &bridge->habit_weights);
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    if (bridge->traces) nimcp_free(bridge->traces);

    nimcp_free(bridge);
}

int bgtr_bridge_reset(bgtr_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtr_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Clear traces */
    bridge->num_traces = 0;
    memset(bridge->traces, 0, bridge->max_traces * sizeof(bgtr_eligibility_trace_t));

    /* Reset learning state */
    bridge->last_rpe = 0.0f;
    bridge->last_reward = 0.0f;
    bridge->last_action = 0;
    bridge->last_action_time = 0;

    memset(&bridge->stats, 0, sizeof(bgtr_bridge_stats_t));

    return 0;
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int bgtr_bridge_connect_bg(bgtr_bridge_t* bridge, basal_ganglia_t* bg) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtr_bridge_connect_bg: bridge is NULL");
        return -1;
    }
    bridge->bg = bg;

    /* Update num_actions if BG has different count */
    if (bg && bg->num_actions != bridge->num_actions) {
        bridge->num_actions = bg->num_actions;
    }

    return 0;
}

int bgtr_bridge_connect_training(
    bgtr_bridge_t* bridge,
    nimcp_training_context_t* training
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtr_bridge_connect_training: bridge is NULL");
        return -1;
    }
    bridge->training = training;

    /* Allocate weights using training module */
    if (training) {
        nimcp_result_t result;

        result = nimcp_training_alloc_weights(
            training, bridge->num_actions, NULL, &bridge->d1_weights);
        if (result != NIMCP_SUCCESS) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgtr_bridge_connect_training: validation failed");
            return -1;
        }

        result = nimcp_training_alloc_weights(
            training, bridge->num_actions, NULL, &bridge->d2_weights);
        if (result != NIMCP_SUCCESS) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgtr_bridge_connect_training: validation failed");
            return -1;
        }

        if (bridge->config.enable_habit_learning) {
            result = nimcp_training_alloc_weights(
                training, bridge->num_actions, NULL, &bridge->habit_weights);
            if (result != NIMCP_SUCCESS) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgtr_bridge_connect_training: validation failed");
                return -1;
            }
        }

        /* Initialize weights to baseline */
        float* d1 = nimcp_training_write_weights(training, &bridge->d1_weights);
        float* d2 = nimcp_training_write_weights(training, &bridge->d2_weights);
        if (d1 && d2) {
            for (uint32_t i = 0; i < bridge->num_actions; i++) {
                d1[i] = 0.5f;  /* Baseline direct pathway */
                d2[i] = 0.5f;  /* Baseline indirect pathway */
            }
        }
    }

    return 0;
}

bool bgtr_bridge_is_connected(const bgtr_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bg != NULL;
}

/* ============================================================================
 * Learning Functions
 * ============================================================================ */

int bgtr_bridge_record_action(
    bgtr_bridge_t* bridge,
    uint32_t action_id,
    uint32_t context_id,
    uint64_t current_time_ms
) {
    if (!bridge || action_id >= bridge->num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgtr_bridge_record_action: bridge is NULL");
        return -1;
    }

    bridge->last_action = action_id;
    bridge->last_action_time = current_time_ms;

    if (!bridge->config.enable_eligibility) return 0;

    /* Find existing trace or create new */
    bgtr_eligibility_trace_t* trace = NULL;
    for (uint32_t i = 0; i < bridge->num_traces; i++) {
        if (bridge->traces[i].action_id == action_id &&
            bridge->traces[i].context_id == context_id) {
            trace = &bridge->traces[i];
            break;
        }
    }

    if (!trace && bridge->num_traces < bridge->max_traces) {
        trace = &bridge->traces[bridge->num_traces++];
        trace->action_id = action_id;
        trace->context_id = context_id;
        trace->creation_time_ms = current_time_ms;
        bridge->stats.traces_created++;
    }

    if (trace) {
        trace->trace_value = 1.0f;  /* Reset trace to max */
        trace->is_active = true;
    }

    return 0;
}

int bgtr_bridge_process_reward(
    bgtr_bridge_t* bridge,
    float reward,
    float expected_reward
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtr_bridge_process_reward: bridge is NULL");
        return -1;
    }

    /* Compute RPE */
    float rpe = (reward - expected_reward) * bridge->config.da_scaling;
    bridge->last_rpe = rpe;
    bridge->last_reward = reward;

    /* Update statistics */
    if (reward > 0) {
        bridge->stats.reward_events++;
    } else if (reward < 0) {
        bridge->stats.punishment_events++;
    }
    bridge->stats.total_reward += reward;
    bridge->stats.avg_rpe = (bridge->stats.avg_rpe * bridge->stats.total_updates +
                             fabsf(rpe)) / (bridge->stats.total_updates + 1);

    /* Update weights */
    return bgtr_bridge_update_weights(bridge, rpe);
}

int bgtr_bridge_update_weights(bgtr_bridge_t* bridge, float rpe) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtr_bridge_update_weights: bridge is NULL");
        return -1;
    }

    int updates = 0;

    /* Get writable weight pointers */
    float* d1 = NULL;
    float* d2 = NULL;

    if (bridge->training) {
        d1 = nimcp_training_write_weights(bridge->training, &bridge->d1_weights);
        d2 = nimcp_training_write_weights(bridge->training, &bridge->d2_weights);
    }

    if (!d1 || !d2) {
        /* No training context, use simple internal update */
        return 0;
    }

    /* Update weights based on eligibility traces */
    for (uint32_t i = 0; i < bridge->num_traces; i++) {
        bgtr_eligibility_trace_t* trace = &bridge->traces[i];
        if (!trace->is_active || trace->trace_value < 0.01f) continue;

        uint32_t action = trace->action_id;
        if (action >= bridge->num_actions) continue;

        float delta;

        /* D1 pathway: positive RPE → LTP */
        delta = bridge->config.d1_learning_rate * trace->trace_value * rpe;
        d1[action] = nimcp_clampf(d1[action] + delta, 0.0f, 1.0f);
        bridge->stats.d1_updates++;

        /* D2 pathway: negative RPE → LTP (asymmetric learning) */
        if (bridge->config.enable_d1_d2_asymmetry) {
            delta = bridge->config.d2_learning_rate * trace->trace_value * (-rpe);
        } else {
            delta = bridge->config.d2_learning_rate * trace->trace_value * rpe;
        }
        d2[action] = nimcp_clampf(d2[action] + delta, 0.0f, 1.0f);
        bridge->stats.d2_updates++;

        updates++;
        bridge->stats.traces_triggered++;
    }

    bridge->stats.total_updates += updates;
    bridge->stats.avg_weight_delta =
        (bridge->stats.avg_weight_delta * (bridge->stats.total_updates - updates) +
         fabsf(rpe) * bridge->config.learning_rate) / bridge->stats.total_updates;

    return updates;
}

int bgtr_bridge_decay_traces(bgtr_bridge_t* bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtr_bridge_decay_traces: bridge is NULL");
        return -1;
    }

    float decay = powf(bridge->config.trace_decay, dt_ms / 1000.0f);

    for (uint32_t i = 0; i < bridge->num_traces; i++) {
        bridge->traces[i].trace_value *= decay;
        if (bridge->traces[i].trace_value < 0.001f) {
            bridge->traces[i].is_active = false;
        }
    }

    return 0;
}

int bgtr_bridge_strengthen_habit(
    bgtr_bridge_t* bridge,
    uint32_t action_id,
    float amount
) {
    if (!bridge || action_id >= bridge->num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgtr_bridge_strengthen_habit: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_habit_learning) return 0;

    if (bridge->training && bridge->habit_weights.handle) {
        float* habits = nimcp_training_write_weights(
            bridge->training, &bridge->habit_weights);
        if (habits) {
            habits[action_id] = nimcp_clampf(
                habits[action_id] + amount * bridge->config.habit_rate_scale,
                0.0f, 1.0f
            );
        }
    }

    return 0;
}

/* ============================================================================
 * Weight Access Functions
 * ============================================================================ */

float bgtr_bridge_get_d1_weight(
    const bgtr_bridge_t* bridge,
    uint32_t action_id
) {
    if (!bridge || action_id >= bridge->num_actions) return -1.0f;

    if (bridge->training) {
        const float* d1 = nimcp_training_read_weights(
            (nimcp_training_context_t*)bridge->training,
            &bridge->d1_weights
        );
        if (d1) return d1[action_id];
    }

    return 0.5f;  /* Default */
}

float bgtr_bridge_get_d2_weight(
    const bgtr_bridge_t* bridge,
    uint32_t action_id
) {
    if (!bridge || action_id >= bridge->num_actions) return -1.0f;

    if (bridge->training) {
        const float* d2 = nimcp_training_read_weights(
            (nimcp_training_context_t*)bridge->training,
            &bridge->d2_weights
        );
        if (d2) return d2[action_id];
    }

    return 0.5f;  /* Default */
}

float bgtr_bridge_get_action_value(
    const bgtr_bridge_t* bridge,
    uint32_t action_id
) {
    if (!bridge || action_id >= bridge->num_actions) return 0.0f;

    float d1 = bgtr_bridge_get_d1_weight(bridge, action_id);
    float d2 = bgtr_bridge_get_d2_weight(bridge, action_id);

    if (d1 < 0 || d2 < 0) return 0.0f;

    return d1 - d2;  /* Net activation */
}

int bgtr_bridge_set_weight(
    bgtr_bridge_t* bridge,
    uint32_t action_id,
    bgtr_pathway_target_t target,
    float weight
) {
    if (!bridge || action_id >= bridge->num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgtr_bridge_set_weight: bridge is NULL");
        return -1;
    }
    if (!bridge->training) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtr_bridge_set_weight: bridge->training is NULL");
        return -1;
    }

    weight = nimcp_clampf(weight, 0.0f, 1.0f);

    switch (target) {
        case BGTR_TARGET_DIRECT:
        case BGTR_TARGET_DMS: {
            float* d1 = nimcp_training_write_weights(
                bridge->training, &bridge->d1_weights);
            if (d1) d1[action_id] = weight;
            break;
        }
        case BGTR_TARGET_INDIRECT: {
            float* d2 = nimcp_training_write_weights(
                bridge->training, &bridge->d2_weights);
            if (d2) d2[action_id] = weight;
            break;
        }
        case BGTR_TARGET_BOTH: {
            float* d1 = nimcp_training_write_weights(
                bridge->training, &bridge->d1_weights);
            float* d2 = nimcp_training_write_weights(
                bridge->training, &bridge->d2_weights);
            if (d1) d1[action_id] = weight;
            if (d2) d2[action_id] = weight;
            break;
        }
        case BGTR_TARGET_DLS: {
            if (bridge->habit_weights.handle) {
                float* h = nimcp_training_write_weights(
                    bridge->training, &bridge->habit_weights);
                if (h) h[action_id] = weight;
            }
            break;
        }
    }

    return 0;
}

/* ============================================================================
 * Eligibility Trace Functions
 * ============================================================================ */

uint32_t bgtr_bridge_get_trace_count(const bgtr_bridge_t* bridge) {
    if (!bridge) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->num_traces; i++) {
        if (bridge->traces[i].is_active) count++;
    }
    return count;
}

float bgtr_bridge_get_trace(
    const bgtr_bridge_t* bridge,
    uint32_t action_id
) {
    if (!bridge) return 0.0f;

    for (uint32_t i = 0; i < bridge->num_traces; i++) {
        if (bridge->traces[i].action_id == action_id &&
            bridge->traces[i].is_active) {
            return bridge->traces[i].trace_value;
        }
    }

    return 0.0f;
}

void bgtr_bridge_clear_traces(bgtr_bridge_t* bridge) {
    if (!bridge) return;
    bridge->num_traces = 0;
    memset(bridge->traces, 0, bridge->max_traces * sizeof(bgtr_eligibility_trace_t));
}

/* ============================================================================
 * Checkpoint Functions
 * ============================================================================ */

int bgtr_bridge_checkpoint(
    bgtr_bridge_t* bridge,
    nimcp_training_checkpoint_t* checkpoint
) {
    if (!bridge || !checkpoint || !bridge->training) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtr_bridge_checkpoint: required parameter is NULL (bridge, checkpoint, bridge->training)");
        return -1;
    }

    nimcp_training_weights_t weights[3] = {
        bridge->d1_weights,
        bridge->d2_weights,
        bridge->habit_weights
    };

    return nimcp_training_checkpoint_create(
        bridge->training,
        weights,
        bridge->config.enable_habit_learning ? 3 : 2,
        checkpoint
    ) == NIMCP_SUCCESS ? 0 : -1;
}

int bgtr_bridge_restore(
    bgtr_bridge_t* bridge,
    const nimcp_training_checkpoint_t* checkpoint
) {
    if (!bridge || !checkpoint || !bridge->training) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtr_bridge_restore: required parameter is NULL (bridge, checkpoint, bridge->training)");
        return -1;
    }

    nimcp_training_weights_t weights[3] = {
        bridge->d1_weights,
        bridge->d2_weights,
        bridge->habit_weights
    };

    return nimcp_training_checkpoint_restore(
        bridge->training,
        weights,
        bridge->config.enable_habit_learning ? 3 : 2,
        checkpoint
    ) == NIMCP_SUCCESS ? 0 : -1;
}

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

int bgtr_bridge_get_stats(
    const bgtr_bridge_t* bridge,
    bgtr_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtr_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void bgtr_bridge_reset_stats(bgtr_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bgtr_bridge_stats_t));
}

const char* bgtr_learning_type_name(bgtr_learning_type_t type) {
    switch (type) {
        case BGTR_LEARN_ACTOR_CRITIC: return "actor_critic";
        case BGTR_LEARN_THREE_FACTOR: return "three_factor";
        case BGTR_LEARN_REWARD_MODULATED: return "reward_modulated";
        case BGTR_LEARN_HABIT_FORMATION: return "habit_formation";
        default: return "unknown";
    }
}

const char* bgtr_pathway_target_name(bgtr_pathway_target_t target) {
    switch (target) {
        case BGTR_TARGET_DIRECT: return "direct";
        case BGTR_TARGET_INDIRECT: return "indirect";
        case BGTR_TARGET_BOTH: return "both";
        case BGTR_TARGET_DMS: return "dms";
        case BGTR_TARGET_DLS: return "dls";
        default: return "unknown";
    }
}
