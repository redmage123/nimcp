/**
 * @file nimcp_basal_ganglia_adapter.c
 * @brief Basal Ganglia Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/adapters/executive/nimcp_basal_ganglia_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_neural_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(basal_ganglia_adapter)

#define BG_MAX_ACTIONS 32

struct nimcp_basal_ganglia_adapter_struct {
    nimcp_basal_ganglia_config_t config;
    nimcp_module_interface_t interface;
    bg_action_t actions[BG_MAX_ACTIONS];
    nimcp_basal_ganglia_state_t state;
    nimcp_basal_ganglia_stats_t stats;
    float gpi_inhibition[BG_MAX_ACTIONS];
    bool is_initialized;
};

static float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

static nimcp_layer_error_t bg_init(void* module, void* config) {
    nimcp_basal_ganglia_adapter_t adapter = (nimcp_basal_ganglia_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (config) adapter->config = *(nimcp_basal_ganglia_config_t*)config;

    /* Initialize action channels */
    for (uint32_t i = 0; i < adapter->config.num_actions && i < BG_MAX_ACTIONS; i++) {
        adapter->actions[i].action_id = i;
        adapter->actions[i].salience = 0.0f;
        adapter->actions[i].direct_activity = 0.0f;
        adapter->actions[i].indirect_activity = 0.0f;
        adapter->actions[i].selection_probability = 0.0f;
        adapter->gpi_inhibition[i] = 1.0f; /* Tonic inhibition */
    }

    adapter->state.dopamine_level = adapter->config.dopamine_baseline;
    adapter->is_initialized = true;
    adapter->state.is_active = true;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t bg_shutdown(void* module) {
    nimcp_basal_ganglia_adapter_t adapter = (nimcp_basal_ganglia_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    adapter->is_initialized = false;
    adapter->state.is_active = false;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t bg_update(void* module, float dt) {
    nimcp_basal_ganglia_adapter_t adapter = (nimcp_basal_ganglia_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    float d1_gain = adapter->config.d1_d2_balance * (1.0f + adapter->state.dopamine_level);
    float d2_gain = (1.0f - adapter->config.d1_d2_balance) * (1.0f - adapter->state.dopamine_level * 0.5f);

    /* Compute pathway activities and GPi output */
    float total_prob = 0.0f;
    float max_activity = 0.0f;
    float second_max = 0.0f;

    for (uint32_t i = 0; i < adapter->config.num_actions && i < BG_MAX_ACTIONS; i++) {
        /* Direct pathway: Striatum -> GPi (inhibitory) -> disinhibition */
        adapter->actions[i].direct_activity = adapter->actions[i].salience * d1_gain;

        /* Indirect pathway: Striatum -> GPe -> STN -> GPi (net excitatory) -> inhibition */
        adapter->actions[i].indirect_activity = adapter->actions[i].salience * d2_gain;

        /* GPi output (lower = more disinhibition = more likely selection) */
        float gpi = 1.0f - adapter->actions[i].direct_activity + adapter->actions[i].indirect_activity;
        if (adapter->config.enable_stn) {
            gpi += adapter->state.stn_activity * 0.5f; /* Global inhibition */
        }
        adapter->gpi_inhibition[i] = fmaxf(0.0f, fminf(1.0f, gpi));

        /* Selection probability (inverse of GPi) */
        float activity = 1.0f - adapter->gpi_inhibition[i];
        adapter->actions[i].selection_probability = activity;
        total_prob += activity;

        if (activity > max_activity) {
            second_max = max_activity;
            max_activity = activity;
        } else if (activity > second_max) {
            second_max = activity;
        }
    }

    /* Normalize probabilities */
    if (total_prob > 0.0001f) {
        for (uint32_t i = 0; i < adapter->config.num_actions && i < BG_MAX_ACTIONS; i++) {
            adapter->actions[i].selection_probability /= total_prob;
        }
    }

    /* Compute conflict (ACC-like) */
    adapter->state.conflict_level = (second_max > 0.0001f) ? (second_max / (max_activity + 0.0001f)) : 0.0f;

    /* Update GPi output (mean) */
    float mean_gpi = 0.0f;
    for (uint32_t i = 0; i < adapter->config.num_actions && i < BG_MAX_ACTIONS; i++) {
        mean_gpi += adapter->gpi_inhibition[i];
    }
    adapter->state.gpi_output = mean_gpi / adapter->config.num_actions;

    /* Dopamine decay toward baseline */
    adapter->state.dopamine_level += (adapter->config.dopamine_baseline - adapter->state.dopamine_level) * 0.1f * dt;

    adapter->stats.updates_processed++;
    (void)dt;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t bg_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_basal_ganglia_adapter_t adapter = (nimcp_basal_ganglia_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->stats.messages_handled++;

    if (msg->header.msg_type == NIMCP_LAYER_MSG_MODULATE && msg->payload) {
        /* Dopamine modulation */
        float da_delta = *(float*)msg->payload;
        nimcp_basal_ganglia_adapter_apply_dopamine(adapter, da_delta);
    } else if (msg->header.msg_type == NIMCP_LAYER_MSG_INHIBIT) {
        /* Response inhibition */
        float strength = msg->payload ? *(float*)msg->payload : 1.0f;
        nimcp_basal_ganglia_adapter_inhibit_response(adapter, strength);
    }

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t bg_get_state(void* module, void* state_out, size_t* size) {
    nimcp_basal_ganglia_adapter_t adapter = (nimcp_basal_ganglia_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;
    if (*size < sizeof(nimcp_basal_ganglia_state_t)) {
        *size = sizeof(nimcp_basal_ganglia_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }
    *(nimcp_basal_ganglia_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_basal_ganglia_state_t);
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t bg_set_state(void* module, const void* state, size_t size) {
    nimcp_basal_ganglia_adapter_t adapter = (nimcp_basal_ganglia_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;
    if (size < sizeof(nimcp_basal_ganglia_state_t)) return NIMCP_LAYER_ERR_INVALID_MSG;
    adapter->state = *(const nimcp_basal_ganglia_state_t*)state;
    return NIMCP_LAYER_OK;
}

static const char* bg_get_name(void* module) {
    (void)module;
    return "Basal_Ganglia_Adapter";
}

nimcp_basal_ganglia_config_t nimcp_basal_ganglia_adapter_default_config(void) {
    nimcp_basal_ganglia_config_t config = {
        .num_actions = 8,
        .striatum_size = 1000,
        .d1_d2_balance = 0.5f,
        .selection_threshold = 0.6f,
        .lateral_inhibition = 0.8f,
        .dopamine_baseline = NIMCP_DOPAMINE_BASELINE,
        .learning_rate = NIMCP_LEARNING_RATE_DEFAULT,
        .enable_stn = true,
        .enable_logging = false
    };
    return config;
}

nimcp_basal_ganglia_adapter_t nimcp_basal_ganglia_adapter_create(const nimcp_basal_ganglia_config_t* config) {
    nimcp_basal_ganglia_adapter_t adapter = (nimcp_basal_ganglia_adapter_t)nimcp_calloc(1, sizeof(struct nimcp_basal_ganglia_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate basal ganglia adapter");

    adapter->config = config ? *config : nimcp_basal_ganglia_adapter_default_config();
    if (adapter->config.num_actions > BG_MAX_ACTIONS) {
        adapter->config.num_actions = BG_MAX_ACTIONS;
    }

    adapter->interface.init = bg_init;
    adapter->interface.shutdown = bg_shutdown;
    adapter->interface.update = bg_update;
    adapter->interface.handle_message = bg_handle_message;
    adapter->interface.get_state = bg_get_state;
    adapter->interface.set_state = bg_set_state;
    adapter->interface.get_name = bg_get_name;

    return adapter;
}

void nimcp_basal_ganglia_adapter_destroy(nimcp_basal_ganglia_adapter_t adapter) {
    if (!adapter) return;
    if (adapter->is_initialized) bg_shutdown(adapter);
    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_basal_ganglia_adapter_get_interface(nimcp_basal_ganglia_adapter_t adapter) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_basal_ganglia_adapter_set_action_salience(
    nimcp_basal_ganglia_adapter_t adapter,
    uint32_t action_id,
    float salience)
{
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if (action_id >= adapter->config.num_actions || action_id >= BG_MAX_ACTIONS) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    adapter->actions[action_id].salience = fmaxf(0.0f, fminf(1.0f, salience));
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_basal_ganglia_adapter_select_action(
    nimcp_basal_ganglia_adapter_t adapter,
    uint32_t* selected_action_out,
    float* confidence_out)
{
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* Find action with highest selection probability */
    uint32_t best_action = 0;
    float best_prob = 0.0f;

    for (uint32_t i = 0; i < adapter->config.num_actions && i < BG_MAX_ACTIONS; i++) {
        if (adapter->actions[i].selection_probability > best_prob) {
            best_prob = adapter->actions[i].selection_probability;
            best_action = i;
        }
    }

    /* Check threshold */
    if (best_prob < adapter->config.selection_threshold) {
        /* No clear winner - conflict */
        adapter->stats.conflicts_resolved++;
    }

    adapter->state.selected_action = best_action;
    adapter->state.selection_confidence = best_prob;
    adapter->stats.actions_selected++;

    if (selected_action_out) *selected_action_out = best_action;
    if (confidence_out) *confidence_out = best_prob;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_basal_ganglia_adapter_apply_dopamine(
    nimcp_basal_ganglia_adapter_t adapter,
    float dopamine_delta)
{
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    adapter->state.dopamine_level += dopamine_delta;
    adapter->state.dopamine_level = fmaxf(0.0f, fminf(1.0f, adapter->state.dopamine_level));

    if (dopamine_delta > 0.1f) {
        adapter->stats.dopamine_bursts++;
    }

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_basal_ganglia_adapter_inhibit_response(
    nimcp_basal_ganglia_adapter_t adapter,
    float inhibition_strength)
{
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if (!adapter->config.enable_stn) return NIMCP_LAYER_ERR_INVALID_MSG;

    adapter->state.stn_activity = fmaxf(0.0f, fminf(1.0f, inhibition_strength));
    adapter->stats.inhibitions_triggered++;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_basal_ganglia_adapter_get_state(nimcp_basal_ganglia_adapter_t adapter, nimcp_basal_ganglia_state_t* state_out) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_basal_ganglia_adapter_get_stats(nimcp_basal_ganglia_adapter_t adapter, nimcp_basal_ganglia_stats_t* stats_out) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_basal_ganglia_adapter_reset_stats(nimcp_basal_ganglia_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
