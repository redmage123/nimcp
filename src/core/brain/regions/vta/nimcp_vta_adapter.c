/**
 * @file nimcp_vta_adapter.c
 * @brief VTA Adapter implementation for brain integration
 * @date 2026-01-11
 */

#include "core/brain/regions/vta/nimcp_vta_adapter.h"
#include "core/brain/regions/vta/nimcp_vta.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(vta_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * Internal Structure
 *===========================================================================*/

typedef struct {
    nimcp_vta_msg_type_t type;
    nimcp_vta_callback_fn callback;
    void* user_data;
} vta_callback_entry_t;

struct nimcp_vta_adapter {
    nimcp_vta_system_t vta;
    nimcp_vta_adapter_config_t config;

    /* Connections */
    struct nimcp_brain* brain;
    struct nimcp_bio_router* router;
    struct nimcp_immune_system* immune;

    /* Message queue */
    nimcp_vta_message_t message_queue[VTA_ADAPTER_MSG_QUEUE_SIZE];
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;

    /* Callbacks */
    vta_callback_entry_t callbacks[VTA_ADAPTER_MAX_CALLBACKS];
    uint32_t num_callbacks;

    /* Training integration */
    struct nimcp_training_hub* training_hub;
    bool training_connected;
    nimcp_vta_training_callback_fn training_callback;
    void* training_callback_user_data;
    nimcp_vta_training_modulation_t current_modulation;
    float accumulated_reward;
    float goal_progress;
    uint32_t training_events_received;
    uint32_t training_modulations_sent;

    /* State tracking */
    bool is_active;
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t updates_processed;
    float total_active_time;
};

static int enqueue_message(nimcp_vta_adapter_t adapter, const nimcp_vta_message_t* msg) {
    if (adapter->queue_count >= VTA_ADAPTER_MSG_QUEUE_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "enqueue_message: capacity exceeded");
        return -1;  /* Queue full */
    }

    adapter->message_queue[adapter->queue_tail] = *msg;
    adapter->queue_tail = (adapter->queue_tail + 1) % VTA_ADAPTER_MSG_QUEUE_SIZE;
    adapter->queue_count++;
    adapter->messages_sent++;

    return 0;
}

static int dequeue_message(nimcp_vta_adapter_t adapter, nimcp_vta_message_t* msg) {
    if (adapter->queue_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dequeue_message: adapter->queue_count is zero");
        return -1;  /* Queue empty */
    }

    *msg = adapter->message_queue[adapter->queue_head];
    adapter->queue_head = (adapter->queue_head + 1) % VTA_ADAPTER_MSG_QUEUE_SIZE;
    adapter->queue_count--;
    adapter->messages_received++;

    return 0;
}

static void invoke_callbacks(nimcp_vta_adapter_t adapter, const nimcp_vta_message_t* msg) {
    for (uint32_t i = 0; i < adapter->num_callbacks; i++) {
        if (adapter->callbacks[i].type == msg->type && adapter->callbacks[i].callback) {
            adapter->callbacks[i].callback(adapter, msg, adapter->callbacks[i].user_data);
        }
    }
}

static void create_standard_projections(nimcp_vta_adapter_t adapter) {
    uint32_t id;

    /* NAc projection */
    nimcp_vta_add_projection(&adapter->vta, VTA_TARGET_NAC, "NAc", 0.9f, &id);

    /* PFC projection */
    nimcp_vta_add_projection(&adapter->vta, VTA_TARGET_PFC, "PFC", 0.7f, &id);

    /* Hippocampus projection */
    nimcp_vta_add_projection(&adapter->vta, VTA_TARGET_HIPPOCAMPUS, "Hippocampus", 0.5f, &id);

    /* Amygdala projection */
    nimcp_vta_add_projection(&adapter->vta, VTA_TARGET_AMYGDALA, "Amygdala", 0.6f, &id);
}

/*=============================================================================
 * Configuration API
 *===========================================================================*/

nimcp_vta_adapter_config_t nimcp_vta_adapter_default_config(void) {
    nimcp_vta_adapter_config_t config;
    memset(&config, 0, sizeof(config));

    config.enable_bio_async = true;
    config.auto_create_projections = true;
    config.connect_to_nac = true;
    config.connect_to_pfc = true;
    config.message_rate_limit = 100.0f;

    /* Training integration defaults */
    config.enable_training_integration = true;
    config.reward_lr_boost = 1.5f;
    config.motivation_persistence_scale = 0.8f;
    config.rpe_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return config;
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_vta_adapter_t nimcp_vta_adapter_create(const nimcp_vta_adapter_config_t* config) {
    nimcp_vta_adapter_t adapter = (nimcp_vta_adapter_t)nimcp_calloc(1, sizeof(struct nimcp_vta_adapter));
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;
    }

    if (config) {
        adapter->config = *config;
    } else {
        adapter->config = nimcp_vta_adapter_default_config();
    }

    /* Initialize VTA system */
    if (nimcp_vta_init(&adapter->vta, NULL) != VTA_OK) {
        nimcp_free(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_vta_adapter_create: validation failed");
        return NULL;
    }

    /* Create standard projections if configured */
    if (adapter->config.auto_create_projections) {
        create_standard_projections(adapter);
    }

    /* Initialize training modulation */
    adapter->current_modulation.lr_multiplier = 1.0f;
    adapter->current_modulation.reward_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    adapter->current_modulation.motivation_signal = 0.5f;
    adapter->current_modulation.persistence_factor = 0.5f;
    adapter->current_modulation.rpe_signal = 0.0f;
    adapter->current_modulation.suggest_exploration = false;
    adapter->current_modulation.suggest_checkpoint = false;
    adapter->accumulated_reward = 0.0f;
    adapter->goal_progress = 0.0f;

    adapter->is_active = true;

    /* Register with bio-async if available */
    if (adapter->config.enable_bio_async) {
        /* Bio-async router not available, skipping registration */
    }

    return adapter;
}

void nimcp_vta_adapter_destroy(nimcp_vta_adapter_t adapter) {
    if (!adapter) {
        return;
    }

    nimcp_vta_shutdown(&adapter->vta);
    nimcp_free(adapter);
}

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_vta_adapter_connect_brain(
    nimcp_vta_adapter_t adapter,
    struct nimcp_brain* brain
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    adapter->brain = brain;
    return 0;
}

int nimcp_vta_adapter_disconnect(nimcp_vta_adapter_t adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    adapter->brain = NULL;
    adapter->router = NULL;
    adapter->immune = NULL;
    return 0;
}

int nimcp_vta_adapter_set_router(
    nimcp_vta_adapter_t adapter,
    struct nimcp_bio_router* router
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    adapter->router = router;
    return 0;
}

int nimcp_vta_adapter_connect_immune(
    nimcp_vta_adapter_t adapter,
    struct nimcp_immune_system* immune
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    adapter->immune = immune;
    return 0;
}

/*=============================================================================
 * VTA Access API
 *===========================================================================*/

nimcp_vta_system_t* nimcp_vta_adapter_get_vta(nimcp_vta_adapter_t adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;
    }
    return &adapter->vta;
}

/*=============================================================================
 * Messaging API
 *===========================================================================*/

int nimcp_vta_adapter_send_message(
    nimcp_vta_adapter_t adapter,
    const nimcp_vta_message_t* msg
) {
    if (!adapter || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_adapter_send_message: required parameter is NULL (adapter, msg)");
        return -1;
    }

    return enqueue_message(adapter, msg);
}

int nimcp_vta_adapter_process_messages(
    nimcp_vta_adapter_t adapter,
    int max_messages
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    int processed = 0;
    nimcp_vta_message_t msg;

    while (processed < max_messages && dequeue_message(adapter, &msg) == 0) {
        /* Process message based on type */
        switch (msg.type) {
            case VTA_MSG_REWARD:
                nimcp_vta_signal_reward(&adapter->vta, msg.data.reward.reward_magnitude);
                break;

            case VTA_MSG_BURST:
                nimcp_vta_trigger_burst(&adapter->vta,
                                        msg.data.burst.intensity,
                                        msg.data.burst.duration);
                break;

            default:
                break;
        }

        /* Invoke callbacks */
        invoke_callbacks(adapter, &msg);
        processed++;
    }

    return processed;
}

int nimcp_vta_adapter_register_callback(
    nimcp_vta_adapter_t adapter,
    nimcp_vta_msg_type_t type,
    nimcp_vta_callback_fn callback,
    void* user_data
) {
    if (!adapter || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_adapter_register_callback: required parameter is NULL (adapter, callback)");
        return -1;
    }

    if (adapter->num_callbacks >= VTA_ADAPTER_MAX_CALLBACKS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_vta_adapter_register_callback: capacity exceeded");
        return -1;
    }

    vta_callback_entry_t* entry = &adapter->callbacks[adapter->num_callbacks];
    entry->type = type;
    entry->callback = callback;
    entry->user_data = user_data;
    adapter->num_callbacks++;

    return 0;
}

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_vta_adapter_update(nimcp_vta_adapter_t adapter, float dt) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    /* Update VTA system */
    nimcp_vta_error_t err = nimcp_vta_update(&adapter->vta, dt);
    if (err != VTA_OK) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_vta_adapter_update: validation failed");
        return -1;
    }

    /* Process any pending messages */
    nimcp_vta_adapter_process_messages(adapter, 10);

    /* Generate messages for state changes */
    float da, rpe;
    nimcp_vta_get_da(&adapter->vta, &da);
    nimcp_vta_get_rpe(&adapter->vta, &rpe);

    /* Send DA level update */
    nimcp_vta_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = VTA_MSG_DA_LEVEL;
    msg.data.da.da_level = da;
    msg.data.da.baseline_ratio = da / VTA_DEFAULT_DA_BASELINE;

    /* Only send if significant change (rate limit) */
    static float last_da = 0.0f;
    if (fabsf(da - last_da) > 5.0f) {
        enqueue_message(adapter, &msg);
        last_da = da;
    }

    /* Update stats */
    adapter->updates_processed++;
    adapter->total_active_time += dt;

    return 0;
}

/*=============================================================================
 * Reward Processing API
 *===========================================================================*/

int nimcp_vta_adapter_process_reward(
    nimcp_vta_adapter_t adapter,
    float reward_magnitude
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    /* Signal reward to VTA */
    nimcp_vta_signal_reward(&adapter->vta, reward_magnitude);

    /* Generate RPE message */
    nimcp_vta_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = VTA_MSG_RPE_UPDATE;

    float rpe;
    nimcp_vta_get_rpe(&adapter->vta, &rpe);
    msg.data.rpe.rpe = rpe;
    msg.data.rpe.actual = reward_magnitude;
    msg.data.rpe.expected = adapter->vta.reward.expected_reward;
    msg.data.rpe.is_positive = rpe > 0;

    enqueue_message(adapter, &msg);

    return 0;
}

int nimcp_vta_adapter_process_cue(
    nimcp_vta_adapter_t adapter,
    uint32_t cue_id,
    float predictive_value
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    /* Set expectation based on cue */
    nimcp_vta_set_expectation(&adapter->vta, predictive_value);

    /* Generate cue detected message */
    nimcp_vta_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = VTA_MSG_CUE_DETECTED;
    msg.data.generic_value = predictive_value;

    enqueue_message(adapter, &msg);

    return 0;
}

int nimcp_vta_adapter_process_goal(
    nimcp_vta_adapter_t adapter,
    uint32_t goal_id,
    float value,
    float effort,
    float distance
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    /* Update motivation based on goal */
    float motivation;
    nimcp_vta_modulate_motivation(&adapter->vta, value * (1.0f - effort), &motivation);

    /* Generate motivation message */
    nimcp_vta_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = VTA_MSG_MOTIVATION_UPDATE;
    msg.data.motivation.wanting = adapter->vta.motivation.wanting;
    msg.data.motivation.vigor = adapter->vta.motivation.effort_willingness;
    msg.data.motivation.goal_id = goal_id;

    enqueue_message(adapter, &msg);

    return 0;
}

/*=============================================================================
 * Integration API
 *===========================================================================*/

int nimcp_vta_adapter_process_immune(
    nimcp_vta_adapter_t adapter,
    float inflammation,
    const float* cytokines,
    uint32_t num_cytokines
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    /* Inflammation affects DA system - sickness behavior */
    float inhibition = inflammation * 0.5f;
    nimcp_vta_apply_inhibition(&adapter->vta, inhibition);

    /* Pro-inflammatory cytokines reduce DA */
    if (cytokines && num_cytokines > 0) {
        float cytokine_effect = 0.0f;
        for (uint32_t i = 0; i < num_cytokines; i++) {
            cytokine_effect += cytokines[i];
        }
        cytokine_effect /= num_cytokines;
        nimcp_vta_apply_inhibition(&adapter->vta, cytokine_effect * 0.3f);
    }

    return 0;
}

int nimcp_vta_adapter_apply_pfc_modulation(
    nimcp_vta_adapter_t adapter,
    float inhibition
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    /* PFC provides top-down control over VTA */
    nimcp_vta_apply_inhibition(&adapter->vta, nimcp_clampf(inhibition, 0.0f, 1.0f));
    return 0;
}

int nimcp_vta_adapter_apply_habenula_inhibition(
    nimcp_vta_adapter_t adapter,
    float inhibition
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    /* Habenula strongly inhibits VTA during negative outcomes */
    nimcp_vta_apply_inhibition(&adapter->vta, nimcp_clampf(inhibition, 0.0f, 1.0f) * 1.5f);

    /* May trigger pause */
    if (inhibition > 0.5f) {
        nimcp_vta_trigger_pause(&adapter->vta, inhibition, 200.0f);
    }

    return 0;
}

/*=============================================================================
 * State API
 *===========================================================================*/

int nimcp_vta_adapter_get_state(
    nimcp_vta_adapter_t adapter,
    nimcp_vta_adapter_state_t* state
) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_adapter_get_state: required parameter is NULL (adapter, state)");
        return -1;
    }

    state->is_active = adapter->is_active;
    nimcp_vta_get_da(&adapter->vta, &state->da_level);
    nimcp_vta_get_rpe(&adapter->vta, &state->current_rpe);
    state->motivation = adapter->vta.motivation.wanting;
    state->messages_sent = adapter->messages_sent;
    state->messages_received = adapter->messages_received;
    state->updates_processed = adapter->updates_processed;
    state->total_active_time = adapter->total_active_time;
    state->training_events_received = adapter->training_events_received;
    state->training_modulations_sent = adapter->training_modulations_sent;

    return 0;
}

int nimcp_vta_adapter_reset_stats(nimcp_vta_adapter_t adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    adapter->messages_sent = 0;
    adapter->messages_received = 0;
    adapter->updates_processed = 0;
    adapter->total_active_time = 0.0f;
    adapter->training_events_received = 0;
    adapter->training_modulations_sent = 0;

    return 0;
}

/*=============================================================================
 * Training Integration Implementation
 *===========================================================================*/

static void update_vta_training_modulation(nimcp_vta_adapter_t adapter) {
    if (!adapter) return;

    /* Get VTA state */
    float da_level, rpe;
    nimcp_vta_get_da(&adapter->vta, &da_level);
    nimcp_vta_get_rpe(&adapter->vta, &rpe);
    float motivation = adapter->vta.motivation.wanting;
    float effort_willingness = adapter->vta.motivation.effort_willingness;

    /* Compute LR multiplier based on DA and reward history */
    /* High DA + positive RPE -> increased LR (reinforce successful strategies) */
    float lr_base = 1.0f;
    float da_ratio = da_level / VTA_DEFAULT_DA_BASELINE;
    if (da_ratio > 1.0f) {
        lr_base += (da_ratio - 1.0f) * adapter->config.reward_lr_boost * 0.5f;
    }
    if (adapter->accumulated_reward > 0.2f) {
        lr_base += adapter->accumulated_reward * 0.2f;
    }
    adapter->current_modulation.lr_multiplier = nimcp_clampf(lr_base, 0.5f, 2.0f);

    /* Compute reward sensitivity based on DA level */
    /* Low DA -> higher sensitivity to rewards (seeking) */
    /* High DA -> lower sensitivity (satiation) */
    float sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    if (da_ratio < 0.8f) {
        sensitivity = 1.0f + (0.8f - da_ratio) * 0.5f;
    } else if (da_ratio > 1.2f) {
        sensitivity = 1.0f - (da_ratio - 1.2f) * 0.3f;
    }
    adapter->current_modulation.reward_sensitivity = nimcp_clampf(sensitivity, 0.5f, 2.0f);

    /* Compute motivation signal based on wanting */
    adapter->current_modulation.motivation_signal = nimcp_clampf(motivation, 0.0f, 1.0f);

    /* Compute persistence factor based on effort willingness and goal progress */
    float persistence = effort_willingness * adapter->config.motivation_persistence_scale;
    if (adapter->goal_progress > 0.5f) {
        persistence += (adapter->goal_progress - 0.5f) * 0.3f;  /* Boost near goal */
    }
    adapter->current_modulation.persistence_factor = nimcp_clampf(persistence, 0.0f, 1.0f);

    /* Store RPE for training layer */
    adapter->current_modulation.rpe_signal = rpe * adapter->config.rpe_sensitivity;

    /* Suggest exploration when DA is low (seeking new rewards) */
    adapter->current_modulation.suggest_exploration = (da_ratio < 0.7f);

    /* Suggest checkpoint on significant positive RPE (milestone) */
    adapter->current_modulation.suggest_checkpoint = (rpe > 0.5f && adapter->goal_progress > 0.3f);

    /* Decay accumulated reward */
    adapter->accumulated_reward *= 0.95f;
}

int nimcp_vta_adapter_connect_training(
    nimcp_vta_adapter_t adapter,
    struct nimcp_training_hub* training_hub
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    adapter->training_hub = training_hub;
    adapter->training_connected = (training_hub != NULL);

    return 0;
}

int nimcp_vta_adapter_on_training_event(
    nimcp_vta_adapter_t adapter,
    nimcp_vta_train_event_t event,
    const nimcp_vta_training_state_t* state
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    adapter->training_events_received++;

    switch (event) {
        case VTA_TRAIN_EVENT_LOSS:
            /* Loss improvement is rewarding */
            if (state && state->loss_improvement < -0.05f) {
                /* Negative loss_improvement means improvement */
                float reward_signal = -state->loss_improvement * 0.5f;
                adapter->accumulated_reward += reward_signal;
                nimcp_vta_signal_reward(&adapter->vta, reward_signal);
            }
            break;

        case VTA_TRAIN_EVENT_REWARD:
            /* Direct reward signal */
            if (state) {
                float rpe = state->current_reward - state->expected_reward;
                adapter->accumulated_reward += state->current_reward * 0.3f;
                nimcp_vta_set_expectation(&adapter->vta, state->expected_reward);
                nimcp_vta_signal_reward(&adapter->vta, state->current_reward);
            }
            break;

        case VTA_TRAIN_EVENT_IMPROVEMENT:
            /* Performance improvement triggers DA burst */
            if (state && state->loss_improvement < -0.1f) {
                nimcp_vta_trigger_burst(&adapter->vta, -state->loss_improvement, 100.0f);
            }
            break;

        case VTA_TRAIN_EVENT_GOAL_PROGRESS:
            /* Goal progress affects motivation */
            if (state) {
                adapter->goal_progress = state->goal_progress;
                float motivation_boost = state->goal_progress * 0.3f;
                nimcp_vta_modulate_motivation(&adapter->vta, motivation_boost, NULL);
            }
            break;

        case VTA_TRAIN_EVENT_MILESTONE:
            /* Milestone reached - significant reward */
            adapter->accumulated_reward += 0.5f;
            nimcp_vta_trigger_burst(&adapter->vta, 0.8f, 200.0f);
            break;

        case VTA_TRAIN_EVENT_EPOCH_START:
            /* New epoch - reset expectation */
            nimcp_vta_set_expectation(&adapter->vta, 0.0f);
            break;

        case VTA_TRAIN_EVENT_EPOCH_END:
            /* Epoch complete - mild reward */
            adapter->accumulated_reward += 0.1f;
            nimcp_vta_signal_reward(&adapter->vta, 0.2f);
            break;

        default:
            break;
    }

    /* Update modulation values */
    update_vta_training_modulation(adapter);

    /* Invoke callback if registered */
    if (adapter->training_callback) {
        adapter->training_callback(&adapter->current_modulation,
                                   adapter->training_callback_user_data);
        adapter->training_modulations_sent++;
    }

    return 0;
}

int nimcp_vta_adapter_get_training_modulation(
    nimcp_vta_adapter_t adapter,
    nimcp_vta_training_modulation_t* modulation
) {
    if (!adapter || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_adapter_get_training_modulation: required parameter is NULL (adapter, modulation)");
        return -1;
    }

    /* Update modulation before returning */
    update_vta_training_modulation(adapter);

    *modulation = adapter->current_modulation;
    adapter->training_modulations_sent++;

    return 0;
}

int nimcp_vta_adapter_register_training_callback(
    nimcp_vta_adapter_t adapter,
    nimcp_vta_training_callback_fn callback,
    void* user_data
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    adapter->training_callback = callback;
    adapter->training_callback_user_data = user_data;

    return 0;
}

int nimcp_vta_adapter_compute_training_rpe(
    nimcp_vta_adapter_t adapter,
    float expected,
    float received,
    float* rpe
) {
    if (!adapter || !rpe) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_adapter_compute_training_rpe: required parameter is NULL (adapter, rpe)");
        return -1;
    }

    /* Set expectation and compute RPE through VTA */
    nimcp_vta_set_expectation(&adapter->vta, expected);
    nimcp_vta_signal_reward(&adapter->vta, received);
    nimcp_vta_get_rpe(&adapter->vta, rpe);

    return 0;
}

int nimcp_vta_adapter_process_training_reward(
    nimcp_vta_adapter_t adapter,
    float reward
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    /* Accumulate reward */
    adapter->accumulated_reward += reward * 0.3f;

    /* Signal reward to VTA */
    nimcp_vta_signal_reward(&adapter->vta, reward);

    /* Update modulation */
    update_vta_training_modulation(adapter);

    return 0;
}

int nimcp_vta_adapter_process_goal_progress(
    nimcp_vta_adapter_t adapter,
    float progress
) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;
    }

    /* Update goal progress */
    adapter->goal_progress = nimcp_clampf(progress, 0.0f, 1.0f);

    /* Progress toward goal modulates motivation */
    float motivation_boost = progress * 0.3f;
    nimcp_vta_modulate_motivation(&adapter->vta, motivation_boost, NULL);

    /* Update modulation */
    update_vta_training_modulation(adapter);

    return 0;
}
