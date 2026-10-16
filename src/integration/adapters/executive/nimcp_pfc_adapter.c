/**
 * @file nimcp_pfc_adapter.c
 * @brief Prefrontal Cortex Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/adapters/executive/nimcp_pfc_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pfc_adapter)

#define PFC_MAX_WM_SLOTS 16
#define PFC_MAX_RULES 64
#define PFC_WM_CONTENT_SIZE 64

struct nimcp_pfc_adapter_struct {
    nimcp_pfc_config_t config;
    nimcp_module_interface_t interface;
    pfc_wm_slot_t wm_slots[PFC_MAX_WM_SLOTS];
    pfc_rule_t rules[PFC_MAX_RULES];
    nimcp_pfc_state_t state;
    nimcp_pfc_stats_t stats;
    float gate_threshold_modulated;
    float current_inhibition;
    bool is_initialized;
};

static nimcp_layer_error_t pfc_init(void* module, void* config) {
    nimcp_pfc_adapter_t adapter = (nimcp_pfc_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (config) adapter->config = *(nimcp_pfc_config_t*)config;

    /* Initialize working memory slots */
    for (uint32_t i = 0; i < adapter->config.wm_slots && i < PFC_MAX_WM_SLOTS; i++) {
        memset(&adapter->wm_slots[i], 0, sizeof(pfc_wm_slot_t));
    }

    /* Initialize rules */
    for (uint32_t i = 0; i < adapter->config.max_rules && i < PFC_MAX_RULES; i++) {
        memset(&adapter->rules[i], 0, sizeof(pfc_rule_t));
    }

    adapter->gate_threshold_modulated = adapter->config.gate_threshold;
    adapter->current_inhibition = 0.0f;
    adapter->is_initialized = true;
    adapter->state.is_active = true;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t pfc_shutdown(void* module) {
    nimcp_pfc_adapter_t adapter = (nimcp_pfc_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    adapter->is_initialized = false;
    adapter->state.is_active = false;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t pfc_update(void* module, float dt) {
    nimcp_pfc_adapter_t adapter = (nimcp_pfc_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t occupied = 0;
    float total_activation = 0.0f;

    /* Update working memory slots (decay) */
    for (uint32_t i = 0; i < adapter->config.wm_slots && i < PFC_MAX_WM_SLOTS; i++) {
        if (adapter->wm_slots[i].is_occupied) {
            adapter->wm_slots[i].decay_timer += dt;

            /* Activation decay */
            float decay = adapter->config.decay_rate * dt;
            adapter->wm_slots[i].activation -= decay;

            if (adapter->wm_slots[i].activation <= 0.0f) {
                /* Slot decayed - clear it */
                adapter->wm_slots[i].is_occupied = false;
                adapter->wm_slots[i].activation = 0.0f;
            } else {
                occupied++;
                total_activation += adapter->wm_slots[i].activation;
            }
        }
    }

    /* Update state */
    adapter->state.occupied_slots = occupied;
    adapter->state.mean_wm_activation = (occupied > 0) ? (total_activation / occupied) : 0.0f;
    adapter->state.cognitive_load = (float)occupied / adapter->config.wm_slots;

    /* Compute conflict (simplified - based on cognitive load and inhibitory demands) */
    if (adapter->config.enable_acc) {
        float load_conflict = adapter->state.cognitive_load * 0.5f;
        float inhibition_conflict = adapter->current_inhibition * 0.5f;
        adapter->state.conflict_level = fmaxf(load_conflict, inhibition_conflict);

        if (adapter->state.conflict_level > adapter->config.conflict_threshold) {
            adapter->stats.conflicts_detected++;
        }
    }

    /* Decay inhibition */
    adapter->current_inhibition *= (1.0f - 0.1f * dt);
    adapter->state.inhibitory_control = adapter->current_inhibition;

    /* Count active rules */
    uint32_t active_rules = 0;
    for (uint32_t i = 0; i < adapter->config.max_rules && i < PFC_MAX_RULES; i++) {
        if (adapter->rules[i].is_active) active_rules++;
    }
    adapter->state.active_rules = active_rules;

    adapter->stats.updates_processed++;
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t pfc_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_pfc_adapter_t adapter = (nimcp_pfc_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->stats.messages_handled++;

    if (msg->header.msg_type == NIMCP_LAYER_MSG_DATA_PUSH && msg->payload) {
        /* Store in working memory */
        uint32_t slot;
        nimcp_pfc_adapter_wm_store(adapter, (float*)msg->payload,
            msg->header.payload_size / sizeof(float), 0.5f, &slot);
    } else if (msg->header.msg_type == NIMCP_LAYER_MSG_MODULATE && msg->payload) {
        /* Dopamine modulation of gating */
        float da_level = *(float*)msg->payload;
        nimcp_pfc_adapter_modulate_gating(adapter, da_level);
    } else if (msg->header.msg_type == NIMCP_LAYER_MSG_INHIBIT && msg->payload) {
        float strength = *(float*)msg->payload;
        nimcp_pfc_adapter_apply_inhibition(adapter, strength);
    }

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t pfc_get_state(void* module, void* state_out, size_t* size) {
    nimcp_pfc_adapter_t adapter = (nimcp_pfc_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;
    if (*size < sizeof(nimcp_pfc_state_t)) {
        *size = sizeof(nimcp_pfc_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }
    *(nimcp_pfc_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_pfc_state_t);
    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t pfc_set_state(void* module, const void* state, size_t size) {
    nimcp_pfc_adapter_t adapter = (nimcp_pfc_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;
    if (size < sizeof(nimcp_pfc_state_t)) return NIMCP_LAYER_ERR_INVALID_MSG;
    adapter->state = *(const nimcp_pfc_state_t*)state;
    return NIMCP_LAYER_OK;
}

static const char* pfc_get_name(void* module) {
    (void)module;
    return "PFC_Adapter";
}

nimcp_pfc_config_t nimcp_pfc_adapter_default_config(void) {
    nimcp_pfc_config_t config = {
        .wm_slots = NIMCP_WM_SLOTS,  /* Miller's magic number +/- 2 */
        .max_rules = 32,
        .gate_threshold = 0.5f,
        .decay_rate = 0.1f,
        .conflict_threshold = 0.6f,
        .inhibition_strength = 0.8f,
        .dopamine_sensitivity = 0.5f,
        .enable_acc = true,
        .enable_logging = false
    };
    return config;
}

nimcp_pfc_adapter_t nimcp_pfc_adapter_create(const nimcp_pfc_config_t* config) {
    nimcp_pfc_adapter_t adapter = (nimcp_pfc_adapter_t)nimcp_calloc(1, sizeof(struct nimcp_pfc_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate PFC adapter");

    adapter->config = config ? *config : nimcp_pfc_adapter_default_config();
    if (adapter->config.wm_slots > PFC_MAX_WM_SLOTS) {
        adapter->config.wm_slots = PFC_MAX_WM_SLOTS;
    }
    if (adapter->config.max_rules > PFC_MAX_RULES) {
        adapter->config.max_rules = PFC_MAX_RULES;
    }

    adapter->interface.init = pfc_init;
    adapter->interface.shutdown = pfc_shutdown;
    adapter->interface.update = pfc_update;
    adapter->interface.handle_message = pfc_handle_message;
    adapter->interface.get_state = pfc_get_state;
    adapter->interface.set_state = pfc_set_state;
    adapter->interface.get_name = pfc_get_name;

    return adapter;
}

void nimcp_pfc_adapter_destroy(nimcp_pfc_adapter_t adapter) {
    if (!adapter) return;
    if (adapter->is_initialized) pfc_shutdown(adapter);
    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_pfc_adapter_adapter_get_interface(nimcp_pfc_adapter_t adapter) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_pfc_adapter_wm_store(
    nimcp_pfc_adapter_t adapter,
    const float* content,
    uint32_t content_size,
    float priority,
    uint32_t* slot_out)
{
    if (!adapter || !content) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* Check gating threshold (modulated by dopamine) */
    if (priority < adapter->gate_threshold_modulated) {
        adapter->stats.gate_closes++;
        return NIMCP_LAYER_ERR_INVALID_MSG; /* Gate closed */
    }

    /* Find empty slot or lowest activation slot */
    uint32_t target_slot = 0;
    float lowest_activation = 2.0f;
    bool found_empty = false;

    for (uint32_t i = 0; i < adapter->config.wm_slots && i < PFC_MAX_WM_SLOTS; i++) {
        if (!adapter->wm_slots[i].is_occupied) {
            target_slot = i;
            found_empty = true;
            break;
        }
        if (adapter->wm_slots[i].activation < lowest_activation) {
            lowest_activation = adapter->wm_slots[i].activation;
            target_slot = i;
        }
    }

    if (!found_empty && priority < lowest_activation) {
        adapter->stats.gate_closes++;
        return NIMCP_LAYER_ERR_CAPACITY; /* Can't displace higher priority item */
    }

    /* Store content */
    pfc_wm_slot_t* slot = &adapter->wm_slots[target_slot];
    uint32_t copy_size = content_size < PFC_WM_CONTENT_SIZE ? content_size : PFC_WM_CONTENT_SIZE;
    memcpy(slot->content, content, copy_size * sizeof(float));
    slot->content_size = copy_size;
    slot->activation = priority;
    slot->decay_timer = 0.0f;
    slot->is_occupied = true;

    /* Update occupied_slots count immediately */
    uint32_t occupied = 0;
    for (uint32_t i = 0; i < adapter->config.wm_slots && i < PFC_MAX_WM_SLOTS; i++) {
        if (adapter->wm_slots[i].is_occupied) occupied++;
    }
    adapter->state.occupied_slots = occupied;

    adapter->stats.wm_stores++;
    adapter->stats.gate_opens++;

    if (slot_out) *slot_out = target_slot;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pfc_adapter_wm_retrieve(
    nimcp_pfc_adapter_t adapter,
    uint32_t slot,
    float* content_out,
    uint32_t max_size)
{
    if (!adapter || !content_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if (slot >= adapter->config.wm_slots || slot >= PFC_MAX_WM_SLOTS) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    pfc_wm_slot_t* wm_slot = &adapter->wm_slots[slot];
    if (!wm_slot->is_occupied) return NIMCP_LAYER_ERR_NOT_REGISTERED;

    uint32_t copy_size = wm_slot->content_size < max_size ? wm_slot->content_size : max_size;
    memcpy(content_out, wm_slot->content, copy_size * sizeof(float));

    /* Refresh activation on retrieval */
    wm_slot->activation = fminf(1.0f, wm_slot->activation + 0.1f);

    adapter->stats.wm_retrievals++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pfc_adapter_wm_refresh(nimcp_pfc_adapter_t adapter, uint32_t slot) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if (slot >= adapter->config.wm_slots || slot >= PFC_MAX_WM_SLOTS) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    pfc_wm_slot_t* wm_slot = &adapter->wm_slots[slot];
    if (!wm_slot->is_occupied) return NIMCP_LAYER_ERR_NOT_REGISTERED;

    wm_slot->activation = 1.0f;
    wm_slot->decay_timer = 0.0f;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pfc_adapter_wm_clear(nimcp_pfc_adapter_t adapter, uint32_t slot) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if (slot >= adapter->config.wm_slots || slot >= PFC_MAX_WM_SLOTS) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    memset(&adapter->wm_slots[slot], 0, sizeof(pfc_wm_slot_t));
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pfc_adapter_modulate_gating(nimcp_pfc_adapter_t adapter, float dopamine_level) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* Higher dopamine lowers gating threshold (more permissive) */
    float da_effect = (dopamine_level - 0.5f) * adapter->config.dopamine_sensitivity;
    adapter->gate_threshold_modulated = adapter->config.gate_threshold - da_effect;
    adapter->gate_threshold_modulated = fmaxf(0.1f, fminf(0.9f, adapter->gate_threshold_modulated));

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pfc_adapter_get_conflict(nimcp_pfc_adapter_t adapter, float* conflict_out) {
    if (!adapter || !conflict_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *conflict_out = adapter->state.conflict_level;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pfc_adapter_apply_inhibition(nimcp_pfc_adapter_t adapter, float inhibition_strength) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    adapter->current_inhibition = fmaxf(adapter->current_inhibition,
        inhibition_strength * adapter->config.inhibition_strength);
    adapter->current_inhibition = fminf(1.0f, adapter->current_inhibition);

    adapter->stats.inhibitions_applied++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pfc_adapter_get_state(nimcp_pfc_adapter_t adapter, nimcp_pfc_state_t* state_out) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pfc_adapter_get_stats(nimcp_pfc_adapter_t adapter, nimcp_pfc_stats_t* stats_out) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_pfc_adapter_reset_stats(nimcp_pfc_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
