/**
 * @file nimcp_lc_adapter.c
 * @brief Locus Coeruleus Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "core/brain/regions/locus_coeruleus/nimcp_lc_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lc_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Structure
//=============================================================================

#define LC_ADAPTER_MAX_CALLBACKS 16
#define LC_ADAPTER_MSG_QUEUE_SIZE 64

typedef struct {
    nimcp_lc_msg_callback_t callback;
    void* user_data;
} lc_callback_entry_t;

struct nimcp_lc_adapter_struct {
    /* Core LC system */
    nimcp_lc_system_t lc;

    /* Configuration */
    nimcp_lc_adapter_config_t config;

    /* Brain connection */
    nimcp_brain_t* brain;
    bool connected_to_brain;

    /* Bio-async */
    nimcp_bio_router_t* router;
    nimcp_lc_message_t message_queue[LC_ADAPTER_MSG_QUEUE_SIZE];
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;

    /* Immune integration */
    nimcp_immune_sensor_t* immune_sensor;
    bool immune_connected;

    /* Thalamic gate */
    float thalamic_gate;

    /* Callbacks */
    lc_callback_entry_t callbacks[LC_MSG_COUNT][LC_ADAPTER_MAX_CALLBACKS];
    uint32_t callback_counts[LC_MSG_COUNT];

    /* Training integration */
    struct nimcp_training_hub* training_hub;
    bool training_connected;
    nimcp_lc_training_callback_fn training_callback;
    void* training_callback_user_data;
    nimcp_lc_training_modulation_t current_modulation;
    float accumulated_novelty;
    float accumulated_stress;

    /* State */
    nimcp_lc_adapter_state_t state;
    bool initialized;
};

//=============================================================================
// Internal Helpers
//=============================================================================

static int enqueue_message(nimcp_lc_adapter_t adapter, const nimcp_lc_message_t* msg) {
    if (!adapter || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enqueue_message: required parameter is NULL (adapter, msg)");
        return -1;
    }

    if (adapter->queue_count >= LC_ADAPTER_MSG_QUEUE_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "enqueue_message: capacity exceeded");
        return -1;  /* Queue full */
    }

    adapter->message_queue[adapter->queue_tail] = *msg;
    adapter->queue_tail = (adapter->queue_tail + 1) % LC_ADAPTER_MSG_QUEUE_SIZE;
    adapter->queue_count++;

    return 0;
}

static int dequeue_message(nimcp_lc_adapter_t adapter, nimcp_lc_message_t* msg) {
    if (!adapter || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dequeue_message: required parameter is NULL (adapter, msg)");
        return -1;
    }

    if (adapter->queue_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dequeue_message: adapter->queue_count is zero");
        return -1;  /* Queue empty */
    }

    *msg = adapter->message_queue[adapter->queue_head];
    adapter->queue_head = (adapter->queue_head + 1) % LC_ADAPTER_MSG_QUEUE_SIZE;
    adapter->queue_count--;

    return 0;
}

static void invoke_callbacks(nimcp_lc_adapter_t adapter, const nimcp_lc_message_t* msg) {
    if (!adapter || !msg) return;

    if (msg->type >= LC_MSG_COUNT) return;

    for (uint32_t i = 0; i < adapter->callback_counts[msg->type]; i++) {
        lc_callback_entry_t* entry = &adapter->callbacks[msg->type][i];
        if (entry->callback) {
            entry->callback(adapter, msg, entry->user_data);
        }
    }
}

//=============================================================================
// Lifecycle Implementation
//=============================================================================

nimcp_lc_adapter_config_t nimcp_lc_adapter_default_config(void) {
    nimcp_lc_adapter_config_t config;
    memset(&config, 0, sizeof(config));

    config.lc_config = nimcp_lc_default_config();

    config.enable_bio_async = true;
    config.enable_immune_sensing = true;
    config.enable_thalamic_relay = true;
    config.enable_cortical_modulation = true;

    config.message_queue_size = LC_ADAPTER_MSG_QUEUE_SIZE;
    config.message_timeout_ms = 100.0f;

    config.auto_create_projections = true;

    config.enable_adapter_logging = false;
    config.log_prefix = "LC";

    /* Training integration defaults */
    config.enable_training_integration = true;
    config.novelty_lr_boost = 1.3f;
    config.stress_attention_boost = 1.2f;
    config.arousal_exploration_scale = 0.5f;

    return config;
}

nimcp_lc_adapter_t nimcp_lc_adapter_create(const nimcp_lc_adapter_config_t* config) {
    nimcp_lc_adapter_t adapter = (nimcp_lc_adapter_t)nimcp_calloc(1, sizeof(struct nimcp_lc_adapter_struct));
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        adapter->config = *config;
    } else {
        adapter->config = nimcp_lc_adapter_default_config();
    }

    /* Initialize LC system */
    nimcp_lc_error_t err = nimcp_lc_init(&adapter->lc, &adapter->config.lc_config);
    if (err != LC_OK) {
        nimcp_free(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_create: validation failed");
        return NULL;
    }

    /* Auto-create standard projections */
    if (adapter->config.auto_create_projections) {
        uint32_t proj_id;
        nimcp_lc_add_projection(&adapter->lc, LC_TARGET_CORTEX, "PFC", 0.8f, &proj_id);
        nimcp_lc_add_projection(&adapter->lc, LC_TARGET_HIPPOCAMPUS, "HPC", 0.7f, &proj_id);
        nimcp_lc_add_projection(&adapter->lc, LC_TARGET_AMYGDALA, "AMY", 0.9f, &proj_id);
        nimcp_lc_add_projection(&adapter->lc, LC_TARGET_THALAMUS, "THL", 0.6f, &proj_id);
    }

    /* Initialize message queue */
    adapter->queue_head = 0;
    adapter->queue_tail = 0;
    adapter->queue_count = 0;

    /* Initialize thalamic gate */
    adapter->thalamic_gate = 1.0f;

    /* Initialize training modulation */
    adapter->current_modulation.lr_multiplier = 1.0f;
    adapter->current_modulation.attention_gain = 1.0f;
    adapter->current_modulation.exploration_factor = 0.5f;
    adapter->current_modulation.vigilance_level = 0.5f;
    adapter->current_modulation.suggest_attention_reset = false;
    adapter->current_modulation.suggest_save_state = false;
    adapter->accumulated_novelty = 0.0f;
    adapter->accumulated_stress = 0.0f;

    /* Initialize state */
    adapter->state.is_active = true;
    adapter->state.is_connected = false;
    adapter->state.is_processing = false;

    adapter->initialized = true;

    return adapter;
}

void nimcp_lc_adapter_destroy(nimcp_lc_adapter_t adapter) {
    if (!adapter) return;

    /* Disconnect from brain */
    if (adapter->connected_to_brain) {
        nimcp_lc_adapter_disconnect(adapter);
    }

    /* Shutdown LC system */
    nimcp_lc_shutdown(&adapter->lc);

    nimcp_free(adapter);
}

int nimcp_lc_adapter_connect_brain(nimcp_lc_adapter_t adapter, nimcp_brain_t* brain) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_connect_brain: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    adapter->brain = brain;
    adapter->connected_to_brain = (brain != NULL);
    adapter->state.is_connected = adapter->connected_to_brain;

    return 0;
}

int nimcp_lc_adapter_disconnect(nimcp_lc_adapter_t adapter) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_disconnect: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    adapter->brain = NULL;
    adapter->connected_to_brain = false;
    adapter->state.is_connected = false;

    return 0;
}

//=============================================================================
// Module Interface Implementation
//=============================================================================

nimcp_module_interface_t* nimcp_lc_adapter_get_interface(nimcp_lc_adapter_t adapter) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_get_interface: required parameter is NULL (adapter, adapter->initialized)");
        return NULL;
    }

    /* Interface would be implemented based on brain module system */
    /* For now, return NULL as placeholder */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_get_interface: required parameter is NULL (adapter, adapter->initialized)");
    return NULL;
}

nimcp_lc_system_t* nimcp_lc_adapter_get_lc(nimcp_lc_adapter_t adapter) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_get_lc: required parameter is NULL (adapter, adapter->initialized)");
        return NULL;
    }

    return &adapter->lc;
}

//=============================================================================
// Bio-Async Implementation
//=============================================================================

int nimcp_lc_adapter_set_router(nimcp_lc_adapter_t adapter, nimcp_bio_router_t* router) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_set_router: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    adapter->router = router;
    adapter->state.bio_async_connected = (router != NULL);

    return 0;
}

int nimcp_lc_adapter_send_message(nimcp_lc_adapter_t adapter, const nimcp_lc_message_t* message) {
    if (!adapter || !adapter->initialized || !message) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_send_message: required parameter is NULL (adapter, adapter->initialized, message)");
        return -1;
    }

    /* Enqueue for local processing */
    int result = enqueue_message(adapter, message);
    if (result != 0) {
        return result;
    }

    adapter->state.messages_sent++;

    /* If router is available, also send via bio-async */
    /* This would involve converting to bio-async message format */
    /* For now, just enqueue locally */

    return 0;
}

int nimcp_lc_adapter_process_messages(nimcp_lc_adapter_t adapter, uint32_t max_messages) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_process_messages: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    adapter->state.is_processing = true;

    uint32_t processed = 0;
    nimcp_lc_message_t msg;

    while (processed < max_messages && dequeue_message(adapter, &msg) == 0) {
        /* Invoke callbacks */
        invoke_callbacks(adapter, &msg);

        /* Handle message internally */
        switch (msg.type) {
            case LC_MSG_REQUEST_INPUT:
                /* Handle input request */
                break;

            case LC_MSG_STRESS_RESPONSE:
                nimcp_lc_signal_stress(&adapter->lc, msg.data.stress.stress_level);
                break;

            default:
                break;
        }

        adapter->state.messages_received++;
        processed++;
    }

    adapter->state.is_processing = false;

    return (int)processed;
}

int nimcp_lc_adapter_register_callback(
    nimcp_lc_adapter_t adapter,
    nimcp_lc_msg_type_t msg_type,
    nimcp_lc_msg_callback_t callback,
    void* user_data
) {
    if (!adapter || !adapter->initialized || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_register_callback: required parameter is NULL (adapter, adapter->initialized, callback)");
        return -1;
    }

    if (msg_type >= LC_MSG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_lc_adapter_register_callback: capacity exceeded");
        return -1;
    }

    if (adapter->callback_counts[msg_type] >= LC_ADAPTER_MAX_CALLBACKS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_lc_adapter_register_callback: capacity exceeded");
        return -1;  /* Too many callbacks */
    }

    uint32_t idx = adapter->callback_counts[msg_type];
    adapter->callbacks[msg_type][idx].callback = callback;
    adapter->callbacks[msg_type][idx].user_data = user_data;
    adapter->callback_counts[msg_type]++;

    return 0;
}

//=============================================================================
// Integration Implementation
//=============================================================================

int nimcp_lc_adapter_connect_immune(nimcp_lc_adapter_t adapter, nimcp_immune_sensor_t* sensor) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_connect_immune: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    adapter->immune_sensor = sensor;
    adapter->immune_connected = (sensor != NULL);
    adapter->state.immune_connected = adapter->immune_connected;

    return 0;
}

int nimcp_lc_adapter_process_immune(
    nimcp_lc_adapter_t adapter,
    float inflammation_level,
    const float* cytokine_levels,
    uint32_t num_cytokines
) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_process_immune: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    /* Inflammation increases LC activity (sickness behavior) */
    if (inflammation_level > 0.3f) {
        /* High inflammation causes stress-like LC activation */
        float stress_equiv = inflammation_level * 0.5f;
        nimcp_lc_signal_stress(&adapter->lc, stress_equiv);
    }

    /* Specific cytokines could have different effects */
    /* IL-1, IL-6, TNF-alpha typically increase LC activity */
    if (cytokine_levels && num_cytokines > 0) {
        float cytokine_sum = 0.0f;
        for (uint32_t i = 0; i < num_cytokines; i++) {
            cytokine_sum += cytokine_levels[i];
        }
        float avg_cytokine = (num_cytokines > 0) ? cytokine_sum / num_cytokines : 0.0f;

        if (avg_cytokine > 0.5f) {
            nimcp_lc_apply_excitation(&adapter->lc, avg_cytokine * 0.3f);
        }
    }

    return 0;
}

int nimcp_lc_adapter_apply_thalamic_gate(nimcp_lc_adapter_t adapter, float gate_level) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_apply_thalamic_gate: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    adapter->thalamic_gate = (gate_level < 0.0f) ? 0.0f : ((gate_level > 1.0f) ? 1.0f : gate_level);
    adapter->state.thalamic_connected = true;

    return 0;
}

//=============================================================================
// Update Implementation
//=============================================================================

int nimcp_lc_adapter_update(nimcp_lc_adapter_t adapter, float dt) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_update: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    /* Process pending messages */
    nimcp_lc_adapter_process_messages(adapter, 10);

    /* Apply thalamic gating to inputs */
    if (adapter->thalamic_gate < 1.0f) {
        /* Reduced gate means reduced excitation */
        float reduction = 1.0f - adapter->thalamic_gate;
        nimcp_lc_apply_inhibition(&adapter->lc, reduction * 0.3f);
    }

    /* Update LC system */
    nimcp_lc_error_t err = nimcp_lc_update(&adapter->lc, dt);
    if (err != LC_OK) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_lc_adapter_update: validation failed");
        return -1;
    }

    /* Generate messages for significant events */

    /* Check for mode changes */
    static __thread nimcp_lc_mode_t last_mode = LC_MODE_TONIC;
    nimcp_lc_mode_t current_mode = nimcp_lc_get_mode(&adapter->lc);
    if (current_mode != last_mode) {
        nimcp_lc_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = LC_MSG_MODE_CHANGED;
        msg.timestamp = adapter->lc.current_time;
        msg.data.mode_change.old_mode = last_mode;
        msg.data.mode_change.new_mode = current_mode;
        nimcp_lc_adapter_send_message(adapter, &msg);
        last_mode = current_mode;
    }

    /* Check for high novelty */
    if (adapter->lc.novelty_signal > 0.5f) {
        nimcp_lc_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = LC_MSG_NOVELTY_DETECTED;
        msg.timestamp = adapter->lc.current_time;
        msg.data.novelty.novelty_score = adapter->lc.novelty_signal;
        msg.data.novelty.surprise_magnitude = adapter->lc.surprise_magnitude;
        msg.data.novelty.triggered_burst = (current_mode == LC_MODE_PHASIC);
        nimcp_lc_adapter_send_message(adapter, &msg);
    }

    /* Update state */
    adapter->state.updates_processed++;
    adapter->state.total_active_time += dt;

    return 0;
}

int nimcp_lc_adapter_process_input(
    nimcp_lc_adapter_t adapter,
    const float* input,
    uint32_t input_size
) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_process_input: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    /* Detect novelty from input */
    float novelty;
    nimcp_lc_detect_novelty(&adapter->lc, input, input_size, &novelty);

    /* Generate novelty message if significant */
    if (novelty > 0.3f) {
        nimcp_lc_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = LC_MSG_NOVELTY_DETECTED;
        msg.timestamp = adapter->lc.current_time;
        msg.data.novelty.novelty_score = novelty;
        msg.data.novelty.surprise_magnitude = adapter->lc.surprise_magnitude;
        msg.data.novelty.triggered_burst = (novelty > 0.6f);
        nimcp_lc_adapter_send_message(adapter, &msg);
    }

    return 0;
}

//=============================================================================
// State/Stats Implementation
//=============================================================================

int nimcp_lc_adapter_get_state(nimcp_lc_adapter_t adapter, nimcp_lc_adapter_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_get_state: required parameter is NULL (adapter, state)");
        return -1;
    }

    if (!adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_get_state: adapter->initialized is NULL");
        return -1;
    }

    *state = adapter->state;
    return 0;
}

int nimcp_lc_adapter_reset_stats(nimcp_lc_adapter_t adapter) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_reset_stats: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    adapter->state.messages_received = 0;
    adapter->state.messages_sent = 0;
    adapter->state.updates_processed = 0;
    adapter->state.total_active_time = 0.0f;
    adapter->state.training_events_received = 0;
    adapter->state.training_modulations_sent = 0;

    nimcp_lc_reset_metrics(&adapter->lc);

    return 0;
}

//=============================================================================
// Training Integration Implementation
//=============================================================================

static void update_training_modulation(nimcp_lc_adapter_t adapter) {
    if (!adapter) return;

    /* Get LC state (direct struct access) */
    float arousal = adapter->lc.arousal_level;
    float ne_level = adapter->lc.ne_concentration / 100.0f;  /* Normalize to 0-1 range */
    nimcp_lc_mode_t mode = nimcp_lc_get_mode(&adapter->lc);

    /* Compute LR multiplier based on arousal and novelty */
    /* High arousal + novelty -> increased LR (adaptive gain) */
    float lr_base = 1.0f;
    if (arousal > 0.5f) {
        lr_base += (arousal - 0.5f) * adapter->config.novelty_lr_boost;
    }
    if (adapter->accumulated_novelty > 0.3f) {
        lr_base += adapter->accumulated_novelty * 0.3f;
    }
    adapter->current_modulation.lr_multiplier = lr_base;

    /* Compute attention gain based on NE level */
    /* NE increases signal-to-noise ratio (gain modulation) */
    float gain_base = 1.0f;
    if (ne_level > 0.3f) {
        gain_base += (ne_level - 0.3f) * adapter->config.stress_attention_boost;
    }
    adapter->current_modulation.attention_gain = gain_base;

    /* Compute exploration factor based on arousal and mode */
    /* Phasic mode -> more exploration (attention reset) */
    /* Tonic mode -> more exploitation (stable attention) */
    float exploration = 0.5f;
    if (mode == LC_MODE_PHASIC) {
        exploration = 0.7f + arousal * 0.2f;
    } else if (mode == LC_MODE_TONIC) {
        exploration = 0.3f + arousal * adapter->config.arousal_exploration_scale * 0.2f;
    }
    adapter->current_modulation.exploration_factor = exploration;

    /* Compute vigilance level */
    /* High NE -> stricter pattern matching */
    adapter->current_modulation.vigilance_level = 0.3f + ne_level * 0.5f;

    /* Suggest attention reset during phasic bursts */
    adapter->current_modulation.suggest_attention_reset = (mode == LC_MODE_PHASIC && arousal > 0.7f);

    /* Suggest save state during high stress (checkpoint before potential issues) */
    adapter->current_modulation.suggest_save_state = (adapter->accumulated_stress > 0.7f);

    /* Decay accumulated values */
    adapter->accumulated_novelty *= 0.95f;
    adapter->accumulated_stress *= 0.98f;
}

int nimcp_lc_adapter_connect_training(
    nimcp_lc_adapter_t adapter,
    struct nimcp_training_hub* training_hub
) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_connect_training: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    adapter->training_hub = training_hub;
    adapter->training_connected = (training_hub != NULL);

    return 0;
}

int nimcp_lc_adapter_on_training_event(
    nimcp_lc_adapter_t adapter,
    nimcp_lc_train_event_t event,
    const nimcp_lc_training_state_t* state
) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_on_training_event: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    adapter->state.training_events_received++;

    switch (event) {
        case LC_TRAIN_EVENT_LOSS:
            /* High loss or worsening trend increases stress */
            if (state && state->loss_trend > 0.1f) {
                adapter->accumulated_stress += state->loss_trend * 0.3f;
                nimcp_lc_apply_excitation(&adapter->lc, state->loss_trend * 0.2f);
            }
            break;

        case LC_TRAIN_EVENT_GRADIENT:
            /* Gradient explosions trigger stress response */
            if (state && state->gradient_norm > 10.0f) {
                adapter->accumulated_stress += 0.2f;
                nimcp_lc_signal_stress(&adapter->lc, 0.5f);
            }
            break;

        case LC_TRAIN_EVENT_NOVELTY:
            /* Novel patterns trigger phasic burst */
            if (state && state->novelty_score > 0.3f) {
                adapter->accumulated_novelty += state->novelty_score;
                /* Set novelty signal directly and apply excitation */
                adapter->lc.novelty_signal = state->novelty_score;
                adapter->lc.surprise_magnitude = state->novelty_score * 0.8f;
                nimcp_lc_apply_excitation(&adapter->lc, state->novelty_score * 0.5f);
            }
            break;

        case LC_TRAIN_EVENT_DIFFICULTY:
            /* Task difficulty affects arousal */
            if (state) {
                float arousal_input = state->difficulty * 0.5f;
                nimcp_lc_apply_excitation(&adapter->lc, arousal_input);
            }
            break;

        case LC_TRAIN_EVENT_EPOCH_START:
            /* New epoch - reset attention */
            adapter->accumulated_novelty = 0.0f;
            break;

        case LC_TRAIN_EVENT_EPOCH_END:
            /* Epoch complete - mild arousal increase */
            nimcp_lc_apply_excitation(&adapter->lc, 0.1f);
            break;

        case LC_TRAIN_EVENT_LR_CHANGE:
            /* LR change is mildly novel */
            adapter->accumulated_novelty += 0.1f;
            break;

        default:
            break;
    }

    /* Update modulation values */
    update_training_modulation(adapter);

    /* Invoke callback if registered */
    if (adapter->training_callback) {
        adapter->training_callback(&adapter->current_modulation,
                                   adapter->training_callback_user_data);
        adapter->state.training_modulations_sent++;
    }

    return 0;
}

int nimcp_lc_adapter_get_training_modulation(
    nimcp_lc_adapter_t adapter,
    nimcp_lc_training_modulation_t* modulation
) {
    if (!adapter || !adapter->initialized || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_get_training_modulation: required parameter is NULL (adapter, adapter->initialized, modulation)");
        return -1;
    }

    /* Update modulation before returning */
    update_training_modulation(adapter);

    *modulation = adapter->current_modulation;
    adapter->state.training_modulations_sent++;

    return 0;
}

int nimcp_lc_adapter_register_training_callback(
    nimcp_lc_adapter_t adapter,
    nimcp_lc_training_callback_fn callback,
    void* user_data
) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_register_training_callback: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    adapter->training_callback = callback;
    adapter->training_callback_user_data = user_data;

    return 0;
}

int nimcp_lc_adapter_compute_attention_gain(
    nimcp_lc_adapter_t adapter,
    float feature_salience,
    float* gain
) {
    if (!adapter || !adapter->initialized || !gain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_compute_attention_gain: required parameter is NULL (adapter, adapter->initialized, gain)");
        return -1;
    }

    /* Get current NE level and arousal (direct struct access) */
    float ne_level = adapter->lc.ne_concentration / 100.0f;  /* Normalize to 0-1 */
    float arousal = adapter->lc.arousal_level;

    /* Compute gain based on NE and feature salience */
    /* High NE + high salience -> high gain */
    /* Low NE or low salience -> lower gain */
    float base_gain = 1.0f + ne_level * 0.5f;
    float salience_mod = 0.5f + feature_salience * 0.5f;
    float arousal_mod = 0.8f + arousal * 0.4f;

    *gain = base_gain * salience_mod * arousal_mod;

    return 0;
}

int nimcp_lc_adapter_process_novelty(
    nimcp_lc_adapter_t adapter,
    float novelty_score
) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_process_novelty: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    /* Accumulate novelty */
    adapter->accumulated_novelty += novelty_score;

    /* High novelty triggers phasic burst */
    if (novelty_score > 0.5f) {
        /* Set novelty signal directly and apply strong excitation */
        adapter->lc.novelty_signal = novelty_score;
        adapter->lc.surprise_magnitude = novelty_score * 0.8f;
        nimcp_lc_apply_excitation(&adapter->lc, novelty_score * 0.5f);
    } else if (novelty_score > 0.2f) {
        adapter->lc.novelty_signal = novelty_score;
        nimcp_lc_apply_excitation(&adapter->lc, novelty_score * 0.3f);
    }

    /* Update modulation */
    update_training_modulation(adapter);

    return 0;
}

int nimcp_lc_adapter_process_training_stress(
    nimcp_lc_adapter_t adapter,
    float stress_level
) {
    if (!adapter || !adapter->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_adapter_process_training_stress: required parameter is NULL (adapter, adapter->initialized)");
        return -1;
    }

    /* Accumulate stress */
    adapter->accumulated_stress += stress_level;

    /* Signal stress to LC system */
    nimcp_lc_signal_stress(&adapter->lc, stress_level);

    /* Update modulation */
    update_training_modulation(adapter);

    return 0;
}
