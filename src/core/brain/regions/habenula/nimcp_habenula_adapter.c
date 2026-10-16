/**
 * @file nimcp_habenula_adapter.c
 * @brief Habenula adapter with bidirectional training layer integration
 */

#include "core/brain/regions/habenula/nimcp_habenula_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(habenula_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * Constants
 *===========================================================================*/

#define MAX_CALLBACKS 16
#define MSG_QUEUE_SIZE 64

/*=============================================================================
 * Internal Structures
 *===========================================================================*/

typedef struct {
    char topic[NIMCP_ID_BUFFER_SIZE];
    nimcp_habenula_msg_callback_t callback;
    void* user_data;
} callback_entry_t;

typedef struct {
    char topic[NIMCP_ID_BUFFER_SIZE];
    uint8_t data[256];
    size_t size;
} message_t;

struct nimcp_habenula_adapter_impl {
    nimcp_habenula_system_t habenula;
    nimcp_habenula_adapter_config_t config;

    /* Messaging */
    callback_entry_t callbacks[MAX_CALLBACKS];
    uint32_t callback_count;
    message_t message_queue[MSG_QUEUE_SIZE];
    uint32_t queue_head;
    uint32_t queue_tail;

    /* Training integration */
    void* training_handle;
    nimcp_habenula_training_callback_t training_callback;
    void* training_callback_data;
    nimcp_habenula_training_modulation_t current_modulation;
    float training_update_timer;
    float training_stress;

    /* State tracking */
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t training_events;

    bool connected;
};

static void update_training_modulation(nimcp_habenula_adapter_t adapter) {
    if (!adapter) return;

    nimcp_habenula_system_t* hab = &adapter->habenula;
    nimcp_habenula_training_modulation_t* mod = &adapter->current_modulation;

    /* Get current habenula state */
    float disappointment = hab->lhb.disappointment;
    float aversion = hab->mhb.aversion_level;
    float helplessness = hab->depression.helplessness_index;
    float firing_ratio = hab->neurons.combined_firing_rate /
                        hab->config.max_firing_rate;

    /* LR reduction during disappointment
     * High disappointment -> reduce learning rate (avoid reinforcing failures) */
    mod->lr_reduction_factor = 1.0f - (disappointment * adapter->config.disappointment_lr_scale);
    mod->lr_reduction_factor = nimcp_clampf(mod->lr_reduction_factor, 0.3f, 1.0f);

    /* Exploration penalty during high aversion
     * If currently aversive, penalize exploration */
    mod->exploration_penalty = aversion * 0.5f;
    mod->exploration_penalty = nimcp_clampf(mod->exploration_penalty, 0.0f, 1.0f);

    /* Negative example weight factor
     * Higher habenula activity -> weight negative examples more */
    mod->negative_weight_factor = 1.0f + firing_ratio * 0.5f;
    mod->negative_weight_factor = nimcp_clampf(mod->negative_weight_factor, 1.0f, 2.0f);

    /* Patience reduction with helplessness */
    mod->patience_reduction = helplessness * 0.3f;
    mod->patience_reduction = nimcp_clampf(mod->patience_reduction, 0.0f, 0.5f);

    /* Suggest early stop if very helpless */
    mod->suggest_early_stop = (helplessness > 0.7f);

    /* Suggest checkpoint during high disappointment (save before things get worse) */
    mod->suggest_checkpoint = (disappointment > 0.6f && adapter->training_stress > 0.5f);

    /* Avoidance signal for current approach */
    float avoidance;
    nimcp_habenula_get_avoidance_signal(hab, &avoidance);
    mod->avoidance_signal = avoidance;
}

/*=============================================================================
 * Configuration API
 *===========================================================================*/

void nimcp_habenula_adapter_default_config(nimcp_habenula_adapter_config_t* config) {
    if (!config) return;

    nimcp_habenula_default_config(&config->habenula_config);
    config->enable_training_integration = true;
    config->enable_vta_coordination = true;
    config->enable_raphe_coordination = true;
    config->training_update_interval = 100.0f; /* ms */
    config->disappointment_lr_scale = 0.5f;
    config->failure_penalty_scale = 0.2f;
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_habenula_adapter_t nimcp_habenula_adapter_create(
    const nimcp_habenula_adapter_config_t* config) {

    nimcp_habenula_adapter_t adapter = nimcp_calloc(1, sizeof(*adapter));
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adapter is NULL");

        return NULL;

    }

    if (config) {
        adapter->config = *config;
    } else {
        nimcp_habenula_adapter_default_config(&adapter->config);
    }

    if (nimcp_habenula_init(&adapter->habenula, &adapter->config.habenula_config) != HABENULA_OK) {
        nimcp_free(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_habenula_adapter_default_config: validation failed");
        return NULL;
    }

    /* Initialize modulation defaults */
    adapter->current_modulation.lr_reduction_factor = 1.0f;
    adapter->current_modulation.exploration_penalty = 0.0f;
    adapter->current_modulation.negative_weight_factor = 1.0f;
    adapter->current_modulation.patience_reduction = 0.0f;
    adapter->current_modulation.suggest_early_stop = false;
    adapter->current_modulation.suggest_checkpoint = false;
    adapter->current_modulation.avoidance_signal = 0.0f;

    adapter->connected = true;

    return adapter;
}

void nimcp_habenula_adapter_destroy(nimcp_habenula_adapter_t adapter) {
    if (!adapter) return;

    nimcp_habenula_shutdown(&adapter->habenula);
    nimcp_free(adapter);
}

int nimcp_habenula_adapter_disconnect(nimcp_habenula_adapter_t adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_disconnect: adapter is NULL");
        return -1;
    }

    adapter->connected = false;
    adapter->training_handle = NULL;
    adapter->training_callback = NULL;

    return 0;
}

/*=============================================================================
 * Access API
 *===========================================================================*/

nimcp_habenula_system_t* nimcp_habenula_adapter_get_habenula(
    nimcp_habenula_adapter_t adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return &adapter->habenula;
}

/*=============================================================================
 * Messaging API
 *===========================================================================*/

int nimcp_habenula_adapter_send_message(nimcp_habenula_adapter_t adapter,
                                         const char* topic,
                                         const void* data,
                                         size_t size) {
    if (!adapter || !topic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_disconnect: required parameter is NULL (adapter, topic)");
        return -1;
    }
    if (size > 256) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_habenula_adapter_disconnect: validation failed");
        return -1;
    }

    uint32_t next_tail = (adapter->queue_tail + 1) % MSG_QUEUE_SIZE;
    if (next_tail == adapter->queue_head) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_habenula_adapter_disconnect: validation failed");
        return -1; /* Queue full */
    }

    message_t* msg = &adapter->message_queue[adapter->queue_tail];
    strncpy(msg->topic, topic, sizeof(msg->topic) - 1);
    if (data && size > 0) {
        memcpy(msg->data, data, size);
    }
    msg->size = size;

    adapter->queue_tail = next_tail;
    adapter->messages_sent++;

    return 0;
}

int nimcp_habenula_adapter_process_messages(nimcp_habenula_adapter_t adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_process_messages: adapter is NULL");
        return -1;
    }

    while (adapter->queue_head != adapter->queue_tail) {
        message_t* msg = &adapter->message_queue[adapter->queue_head];

        /* Find matching callbacks */
        for (uint32_t i = 0; i < adapter->callback_count; i++) {
            if (strcmp(adapter->callbacks[i].topic, msg->topic) == 0 ||
                strcmp(adapter->callbacks[i].topic, "*") == 0) {
                adapter->callbacks[i].callback(
                    adapter->callbacks[i].user_data,
                    msg->topic,
                    msg->data,
                    msg->size);
            }
        }

        adapter->queue_head = (adapter->queue_head + 1) % MSG_QUEUE_SIZE;
        adapter->messages_received++;
    }

    return 0;
}

int nimcp_habenula_adapter_register_callback(nimcp_habenula_adapter_t adapter,
                                              const char* topic,
                                              nimcp_habenula_msg_callback_t callback,
                                              void* user_data) {
    if (!adapter || !topic || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_process_messages: required parameter is NULL (adapter, topic, callback)");
        return -1;
    }
    if (adapter->callback_count >= MAX_CALLBACKS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_habenula_adapter_process_messages: capacity exceeded");
        return -1;
    }

    callback_entry_t* entry = &adapter->callbacks[adapter->callback_count];
    strncpy(entry->topic, topic, sizeof(entry->topic) - 1);
    entry->callback = callback;
    entry->user_data = user_data;

    adapter->callback_count++;
    return 0;
}

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_habenula_adapter_update(nimcp_habenula_adapter_t adapter, float dt_ms) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_update: adapter is NULL");
        return -1;
    }

    /* Update habenula system */
    nimcp_habenula_update(&adapter->habenula, dt_ms);

    /* Process messages */
    nimcp_habenula_adapter_process_messages(adapter);

    /* Update training modulation periodically */
    if (adapter->config.enable_training_integration) {
        adapter->training_update_timer += dt_ms;
        if (adapter->training_update_timer >= adapter->config.training_update_interval) {
            adapter->training_update_timer = 0.0f;
            update_training_modulation(adapter);

            /* Invoke training callback if registered */
            if (adapter->training_callback) {
                adapter->training_callback(
                    adapter->training_callback_data,
                    &adapter->current_modulation);
            }
        }
    }

    /* Decay training stress */
    adapter->training_stress *= 0.99f;

    return 0;
}

/*=============================================================================
 * Training Layer Integration API
 *===========================================================================*/

int nimcp_habenula_adapter_connect_training(nimcp_habenula_adapter_t adapter,
                                             void* training_handle) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_update: adapter is NULL");
        return -1;
    }

    adapter->training_handle = training_handle;
    return 0;
}

int nimcp_habenula_adapter_on_training_event(
    nimcp_habenula_adapter_t adapter,
    const nimcp_habenula_train_event_data_t* event) {
    if (!adapter || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_update: required parameter is NULL (adapter, event)");
        return -1;
    }

    adapter->training_events++;

    switch (event->type) {
        case HABENULA_TRAIN_EVENT_LOSS:
            /* High loss -> disappointment if higher than expected */
            if (event->value > event->expected) {
                nimcp_habenula_process_outcome(&adapter->habenula,
                                               event->expected,
                                               -event->value); /* Negative = bad */
            }
            adapter->training_stress += event->value * 0.1f;
            adapter->training_stress = nimcp_clampf(adapter->training_stress, 0.0f, 1.0f);
            break;

        case HABENULA_TRAIN_EVENT_FAILURE:
            nimcp_habenula_apply_aversive(&adapter->habenula, event->value);
            nimcp_habenula_record_coping_failure(&adapter->habenula);
            adapter->training_stress += 0.2f;
            break;

        case HABENULA_TRAIN_EVENT_TIMEOUT:
            nimcp_habenula_apply_aversive(&adapter->habenula, 0.3f);
            adapter->training_stress += 0.15f;
            break;

        case HABENULA_TRAIN_EVENT_REWARD:
            /* Positive reward reduces habenula activity */
            nimcp_habenula_record_coping_success(&adapter->habenula);
            nimcp_habenula_apply_inhibition(&adapter->habenula, event->value * 0.5f);
            adapter->training_stress *= 0.8f;
            break;

        case HABENULA_TRAIN_EVENT_PUNISHMENT:
            nimcp_habenula_apply_aversive(&adapter->habenula, event->value);
            nimcp_habenula_process_outcome(&adapter->habenula, 0.0f, -event->value);
            adapter->training_stress += event->value * 0.3f;
            break;

        case HABENULA_TRAIN_EVENT_EPOCH_END:
            /* Epoch end can reduce stress slightly */
            adapter->training_stress *= 0.9f;
            break;

        case HABENULA_TRAIN_EVENT_PLATEAU:
            /* Learning plateau -> frustration -> disappointment */
            nimcp_habenula_apply_aversive(&adapter->habenula, 0.4f);
            adapter->training_stress += 0.1f;
            break;

        case HABENULA_TRAIN_EVENT_GRADIENT:
            /* Large gradients might indicate instability */
            if (event->value > 1.0f) {
                adapter->training_stress += (event->value - 1.0f) * 0.05f;
            }
            break;
    }

    adapter->training_stress = nimcp_clampf(adapter->training_stress, 0.0f, 1.0f);

    return 0;
}

int nimcp_habenula_adapter_get_training_modulation(
    nimcp_habenula_adapter_t adapter,
    nimcp_habenula_training_modulation_t* modulation) {
    if (!adapter || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_update: required parameter is NULL (adapter, modulation)");
        return -1;
    }

    *modulation = adapter->current_modulation;
    return 0;
}

int nimcp_habenula_adapter_register_training_callback(
    nimcp_habenula_adapter_t adapter,
    nimcp_habenula_training_callback_t callback,
    void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_update: adapter is NULL");
        return -1;
    }

    adapter->training_callback = callback;
    adapter->training_callback_data = user_data;
    return 0;
}

/*=============================================================================
 * Reward Processing API
 *===========================================================================*/

int nimcp_habenula_adapter_process_reward_outcome(
    nimcp_habenula_adapter_t adapter,
    float expected,
    float received) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_update: adapter is NULL");
        return -1;
    }

    return nimcp_habenula_process_outcome(&adapter->habenula, expected, received);
}

int nimcp_habenula_adapter_process_punishment(
    nimcp_habenula_adapter_t adapter,
    float intensity) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_update: adapter is NULL");
        return -1;
    }

    return nimcp_habenula_apply_aversive(&adapter->habenula, intensity);
}

int nimcp_habenula_adapter_compute_negative_reinforcement(
    nimcp_habenula_adapter_t adapter,
    float* negative_signal) {
    if (!adapter || !negative_signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_update: required parameter is NULL (adapter, negative_signal)");
        return -1;
    }

    /* Combine disappointment, aversion, and helplessness */
    float disappointment = adapter->habenula.lhb.disappointment;
    float aversion = adapter->habenula.mhb.aversion_level;
    float helplessness = adapter->habenula.depression.helplessness_index;

    *negative_signal = (disappointment * 0.5f + aversion * 0.3f + helplessness * 0.2f);
    *negative_signal = nimcp_clampf(*negative_signal, 0.0f, 1.0f);

    return 0;
}

/*=============================================================================
 * VTA Coordination API
 *===========================================================================*/

int nimcp_habenula_adapter_get_vta_output(
    nimcp_habenula_adapter_t adapter,
    float* inhibition) {
    if (!adapter || !inhibition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_update: required parameter is NULL (adapter, inhibition)");
        return -1;
    }

    return nimcp_habenula_get_vta_inhibition(&adapter->habenula, inhibition);
}

int nimcp_habenula_adapter_apply_vta_input(
    nimcp_habenula_adapter_t adapter,
    float da_level) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_update: adapter is NULL");
        return -1;
    }

    if (!adapter->config.enable_vta_coordination) return 0;

    return nimcp_habenula_apply_vta_feedback(&adapter->habenula, da_level);
}

/*=============================================================================
 * Raphe Coordination API
 *===========================================================================*/

int nimcp_habenula_adapter_get_raphe_output(
    nimcp_habenula_adapter_t adapter,
    float* modulation) {
    if (!adapter || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_update: required parameter is NULL (adapter, modulation)");
        return -1;
    }

    return nimcp_habenula_get_raphe_modulation(&adapter->habenula, modulation);
}

int nimcp_habenula_adapter_apply_raphe_input(
    nimcp_habenula_adapter_t adapter,
    float ht_level) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter is NULL");
        return -1;
    }

    if (!adapter->config.enable_raphe_coordination) return 0;

    /* High serotonin inhibits habenula (anxiolytic effect) */
    float normalized = nimcp_clampf(ht_level / 100.0f, 0.0f, 1.0f);
    if (normalized > 0.5f) {
        nimcp_habenula_apply_inhibition(&adapter->habenula, (normalized - 0.5f) * 0.3f);
    }

    return 0;
}

/*=============================================================================
 * Depression/Helplessness API
 *===========================================================================*/

int nimcp_habenula_adapter_should_stop(
    nimcp_habenula_adapter_t adapter,
    bool* should_stop) {
    if (!adapter || !should_stop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, should_stop)");
        return -1;
    }

    *should_stop = adapter->current_modulation.suggest_early_stop;
    return 0;
}

int nimcp_habenula_adapter_get_depression_state(
    nimcp_habenula_adapter_t adapter,
    float* helplessness,
    bool* is_depressed) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter is NULL");
        return -1;
    }

    if (helplessness) {
        nimcp_habenula_get_helplessness(&adapter->habenula, helplessness);
    }
    if (is_depressed) {
        nimcp_habenula_is_depressed(&adapter->habenula, is_depressed);
    }

    return 0;
}

int nimcp_habenula_adapter_record_failure(nimcp_habenula_adapter_t adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_record_failure: adapter is NULL");
        return -1;
    }
    return nimcp_habenula_record_coping_failure(&adapter->habenula);
}

int nimcp_habenula_adapter_record_success(nimcp_habenula_adapter_t adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_record_success: adapter is NULL");
        return -1;
    }
    return nimcp_habenula_record_coping_success(&adapter->habenula);
}

/*=============================================================================
 * State API
 *===========================================================================*/

int nimcp_habenula_adapter_get_state(nimcp_habenula_adapter_t adapter,
                                      nimcp_habenula_adapter_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_record_success: required parameter is NULL (adapter, state)");
        return -1;
    }

    state->firing_rate = adapter->habenula.neurons.combined_firing_rate;
    state->disappointment = adapter->habenula.lhb.disappointment;
    state->aversion = adapter->habenula.mhb.aversion_level;
    state->vta_inhibition = adapter->habenula.lhb.vta_inhibition_output;

    float raphe_mod;
    nimcp_habenula_get_raphe_modulation(&adapter->habenula, &raphe_mod);
    state->raphe_modulation = raphe_mod;

    state->helplessness = adapter->habenula.depression.helplessness_index;
    state->is_depressed = adapter->habenula.depression.is_depressed;
    state->mode = adapter->habenula.mode;
    state->messages_sent = adapter->messages_sent;
    state->messages_received = adapter->messages_received;
    state->training_events = adapter->training_events;

    return 0;
}

int nimcp_habenula_adapter_reset_stats(nimcp_habenula_adapter_t adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_adapter_reset_stats: adapter is NULL");
        return -1;
    }

    adapter->messages_sent = 0;
    adapter->messages_received = 0;
    adapter->training_events = 0;
    nimcp_habenula_reset_metrics(&adapter->habenula);

    return 0;
}
