/**
 * @file nimcp_habenula_plasticity_bridge.c
 * @brief Implementation of Habenula-Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/habenula/nimcp_habenula_plasticity_bridge.h"
#include "core/brain/regions/habenula/nimcp_habenula_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(habenula_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HABENULA_PLASTICITY_BRIDGE"


struct nimcp_habenula_plasticity_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_habenula_plasticity_config_t config;
    nimcp_habenula_adapter_t habenula_adapter;
    void* plasticity_coordinator;
    nimcp_habenula_plasticity_synapse_t* synapses;
    uint32_t synapse_count;
    uint32_t synapse_capacity;
    nimcp_habenula_plasticity_bridge_state_t state;
    nimcp_habenula_plasticity_modulation_t current_modulation;
    nimcp_habenula_weight_change_cb weight_callback;
    void* callback_user_data;
    nimcp_habenula_plasticity_stats_t stats;
};

static nimcp_habenula_plasticity_synapse_t* find_synapse(nimcp_habenula_plasticity_bridge_t* b, uint32_t id) {
    for (uint32_t i = 0; i < b->synapse_count; i++) if (b->synapses[i].synapse_id == id) return &b->synapses[i];
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_synapse: operation failed");
    return NULL;
}

nimcp_habenula_plasticity_config_t nimcp_habenula_plasticity_config_default(void) {
    return (nimcp_habenula_plasticity_config_t){
        .enable_habenula_gating = true, .habenula_lr_multiplier = 1.5f, .gating_threshold = 0.3f,
        .enable_punishment_learning = true, .punishment_learning_rate = 0.2f, .negative_rpe_scale = 1.5f, .punishment_ltd_boost = 2.0f,
        .stdp_ltp_window_ms = HABENULA_PLASTICITY_STDP_WINDOW, .stdp_ltd_window_ms = HABENULA_PLASTICITY_STDP_WINDOW, .aversive_stdp_modulation = 0.5f,
        .enable_avoidance_learning = true, .avoidance_learning_rate = 0.15f, .successful_avoidance_boost = 2.0f, .failed_avoidance_penalty = 1.5f,
        .enable_disappointment = true, .disappointment_scale = 1.5f, .omission_penalty = 1.2f,
        .enable_relief_learning = true, .relief_learning_rate = NIMCP_LEARNING_RATE_COARSE, .relief_ltp_boost = 1.5f,
        .enable_inhibition_strengthening = true, .inhibition_learning_rate = NIMCP_LEARNING_RATE_COARSE,
        .weight_min = 0.0f, .weight_max = 1.0f, .initial_weight = NIMCP_SYNAPSE_WEIGHT_INIT, .enable_bio_async = false
    };
}

nimcp_habenula_plasticity_bridge_t* nimcp_habenula_plasticity_create(const nimcp_habenula_plasticity_config_t* config) {
    nimcp_habenula_plasticity_bridge_t* b = nimcp_calloc(1, sizeof(*b));
    if (!b) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "b is NULL");

        return NULL;

    }
    b->config = config ? *config : nimcp_habenula_plasticity_config_default();
    b->synapse_capacity = HABENULA_PLASTICITY_MAX_SYNAPSES;
    b->synapses = nimcp_calloc(b->synapse_capacity, sizeof(nimcp_habenula_plasticity_synapse_t));
    if (!b->synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_habenula_plasticity_create: synapses allocation failed");
        nimcp_free(b);
        return NULL;
    }
    b->state.state = HABENULA_PLASTICITY_STATE_IDLE;
    b->current_modulation.lr_multiplier = 1.0f;
    return b;
}

void nimcp_habenula_plasticity_destroy(nimcp_habenula_plasticity_bridge_t* b) {
    if (!b) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "habenula_plasticity");
    nimcp_free(b->synapses);
    nimcp_free(b);
}

int nimcp_habenula_plasticity_reset(nimcp_habenula_plasticity_bridge_t* b) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_reset: b is NULL");
        return -1;
    }
    for (uint32_t i = 0; i < b->synapse_count; i++) b->synapses[i].weight = b->synapses[i].initial_weight;
    memset(&b->state, 0, sizeof(b->state));
    b->state.state = HABENULA_PLASTICITY_STATE_IDLE;
    memset(&b->stats, 0, sizeof(b->stats));
    return 0;
}

int nimcp_habenula_plasticity_connect_habenula(nimcp_habenula_plasticity_bridge_t* b, nimcp_habenula_adapter_t a) { if (!b||!a) return -1; b->habenula_adapter = a; return 0; }
int nimcp_habenula_plasticity_connect_coordinator(nimcp_habenula_plasticity_bridge_t* b, struct nimcp_plasticity_coordinator* c) { if (!b||!c) return -1; b->plasticity_coordinator = c; return 0; }

int nimcp_habenula_plasticity_register_synapse(nimcp_habenula_plasticity_bridge_t* b, uint32_t id, nimcp_habenula_synapse_type_t t, float w) {
    if (!b || b->synapse_count >= b->synapse_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_habenula_plasticity_register_synapse: b is NULL");
        return -1;
    }
    nimcp_habenula_plasticity_synapse_t* s = &b->synapses[b->synapse_count++];
    s->synapse_id = id; s->type = t; s->weight = s->initial_weight = w;
    b->state.registered_synapses = b->synapse_count;
    return 0;
}

int nimcp_habenula_plasticity_unregister_synapse(nimcp_habenula_plasticity_bridge_t* b, uint32_t id) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_unregister_synapse: b is NULL");
        return -1;
    }
    for (uint32_t i = 0; i < b->synapse_count; i++) {
        if (b->synapses[i].synapse_id == id) {
            if (i < b->synapse_count - 1) b->synapses[i] = b->synapses[b->synapse_count - 1];
            b->synapse_count--;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_habenula_plasticity_unregister_synapse: validation failed");
    return -1;
}

int nimcp_habenula_plasticity_get_synapse(nimcp_habenula_plasticity_bridge_t* b, uint32_t id, nimcp_habenula_plasticity_synapse_t* o) {
    if (!b || !o) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_get_synapse: required parameter is NULL (b, o)");
        return -1;
    }
    nimcp_habenula_plasticity_synapse_t* s = find_synapse(b, id);
    if (!s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_get_synapse: s is NULL");
        return -1;
    }
    *o = *s;
    return 0;
}

int nimcp_habenula_plasticity_pre_spike(nimcp_habenula_plasticity_bridge_t* b, uint32_t id, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_pre_spike: b is NULL");
        return -1;
    }
    nimcp_habenula_plasticity_synapse_t* s = find_synapse(b, id);
    if (!s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_pre_spike: s is NULL");
        return -1;
    }
    s->last_pre_spike_us = ts;
    s->eligibility_trace = 1.0f;
    b->stats.total_pre_spikes++;
    return 0;
}

int nimcp_habenula_plasticity_post_spike(nimcp_habenula_plasticity_bridge_t* b, uint32_t id, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_post_spike: b is NULL");
        return -1;
    }
    nimcp_habenula_plasticity_synapse_t* s = find_synapse(b, id);
    if (!s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_post_spike: s is NULL");
        return -1;
    }
    s->last_post_spike_us = ts;
    b->stats.total_post_spikes++;
    return 0;
}

int nimcp_habenula_plasticity_punishment(nimcp_habenula_plasticity_bridge_t* b, float p, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_punishment: b is NULL");
        return -1;
    }
    b->state.habenula.punishment_active = true;
    b->state.habenula.last_punishment_us = ts;
    b->stats.punishment_events++;
    b->stats.total_punishment += p;
    b->current_modulation.punishment_signal = p * b->config.punishment_learning_rate;
    return 0;
}

int nimcp_habenula_plasticity_negative_rpe(nimcp_habenula_plasticity_bridge_t* b, float rpe, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_negative_rpe: b is NULL");
        return -1;
    }
    b->state.habenula.negative_rpe = nimcp_clampf(rpe, 0.0f, 1.0f);
    b->current_modulation.ltd_boost = 1.0f + rpe * b->config.negative_rpe_scale;
    return 0;
}

int nimcp_habenula_plasticity_disappointment(nimcp_habenula_plasticity_bridge_t* b, float expected, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_disappointment: b is NULL");
        return -1;
    }
    b->state.habenula.disappointment = expected * b->config.disappointment_scale;
    return 0;
}

int nimcp_habenula_plasticity_avoidance_success(nimcp_habenula_plasticity_bridge_t* b, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_avoidance_success: b is NULL");
        return -1;
    }
    b->stats.avoidance_successes++;
    for (uint32_t i = 0; i < b->synapse_count; i++) {
        if (b->synapses[i].type == HABENULA_SYNAPSE_AVOIDANCE && b->synapses[i].eligibility_trace > 0.1f) {
            float boost = b->synapses[i].eligibility_trace * b->config.successful_avoidance_boost * 0.01f;
            b->synapses[i].weight = nimcp_clampf(b->synapses[i].weight + boost, b->config.weight_min, b->config.weight_max);
            b->synapses[i].avoidance_strength += 0.1f;
            b->synapses[i].avoidance_successes++;
        }
    }
    return 0;
}

int nimcp_habenula_plasticity_avoidance_failure(nimcp_habenula_plasticity_bridge_t* b, float p, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_avoidance_failure: b is NULL");
        return -1;
    }
    b->stats.avoidance_failures++;
    nimcp_habenula_plasticity_punishment(b, p, ts);
    return 0;
}

int nimcp_habenula_plasticity_relief(nimcp_habenula_plasticity_bridge_t* b, float r, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_relief: b is NULL");
        return -1;
    }
    b->stats.relief_events++;
    for (uint32_t i = 0; i < b->synapse_count; i++) {
        if (b->synapses[i].eligibility_trace > 0.1f) {
            float boost = b->synapses[i].eligibility_trace * r * b->config.relief_ltp_boost * 0.01f;
            b->synapses[i].weight = nimcp_clampf(b->synapses[i].weight + boost, b->config.weight_min, b->config.weight_max);
        }
    }
    return 0;
}

int nimcp_habenula_plasticity_set_state(nimcp_habenula_plasticity_bridge_t* b, float av, float rpe) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_set_state: b is NULL");
        return -1;
    }
    b->state.habenula.aversive_level = nimcp_clampf(av, 0.0f, 1.0f);
    b->state.habenula.negative_rpe = nimcp_clampf(rpe, 0.0f, 1.0f);
    b->current_modulation.lr_multiplier = b->config.habenula_lr_multiplier * (1.0f + av);
    b->state.global_lr_modulation = b->current_modulation.lr_multiplier;
    return 0;
}

int nimcp_habenula_plasticity_update(nimcp_habenula_plasticity_bridge_t* b, float dt_ms) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_update: b is NULL");
        return -1;
    }
    b->stats.total_updates++;
    float decay = expf(-dt_ms / 100.0f);
    for (uint32_t i = 0; i < b->synapse_count; i++) {
        b->synapses[i].eligibility_trace *= decay;
        b->synapses[i].punishment_trace *= decay;
    }
    return 0;
}

int nimcp_habenula_plasticity_convert_traces(nimcp_habenula_plasticity_bridge_t* b, float aversive) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_plasticity_convert_traces: b is NULL");
        return -1;
    }
    for (uint32_t i = 0; i < b->synapse_count; i++) {
        if (b->synapses[i].punishment_trace > 0.1f) {
            float dw = -b->synapses[i].punishment_trace * aversive * b->config.punishment_ltd_boost * 0.01f;
            b->synapses[i].weight = nimcp_clampf(b->synapses[i].weight + dw, b->config.weight_min, b->config.weight_max);
            b->stats.ltd_events++;
        }
    }
    return 0;
}

int nimcp_habenula_plasticity_get_modulation(nimcp_habenula_plasticity_bridge_t* b, nimcp_habenula_plasticity_modulation_t* m) { if (!b||!m) return -1; *m = b->current_modulation; return 0; }
float nimcp_habenula_plasticity_get_avoidance_progress(nimcp_habenula_plasticity_bridge_t* b) {
    if (!b || b->synapse_count == 0) return 0.0f;
    float total = 0.0f;
    for (uint32_t i = 0; i < b->synapse_count; i++) total += b->synapses[i].avoidance_strength;
    return total / b->synapse_count;
}
float nimcp_habenula_plasticity_get_prediction_accuracy(nimcp_habenula_plasticity_bridge_t* b) { return b ? 1.0f - nimcp_clampf(b->stats.avg_negative_rpe, 0.0f, 1.0f) : 0.0f; }
float nimcp_habenula_plasticity_get_inhibition_strength(nimcp_habenula_plasticity_bridge_t* b) { return b ? b->state.avg_avoidance_strength : 0.0f; }
int nimcp_habenula_plasticity_get_state(const nimcp_habenula_plasticity_bridge_t* b, nimcp_habenula_plasticity_bridge_state_t* s) { if (!b||!s) return -1; *s = b->state; return 0; }
int nimcp_habenula_plasticity_get_stats(const nimcp_habenula_plasticity_bridge_t* b, nimcp_habenula_plasticity_stats_t* s) { if (!b||!s) return -1; *s = b->stats; return 0; }
void nimcp_habenula_plasticity_reset_stats(nimcp_habenula_plasticity_bridge_t* b) { if (b) memset(&b->stats, 0, sizeof(b->stats)); }
int nimcp_habenula_plasticity_set_weight_callback(nimcp_habenula_plasticity_bridge_t* b, nimcp_habenula_weight_change_cb cb, void* ud) { if (!b) return -1; b->weight_callback = cb; b->callback_user_data = ud; return 0; }
int nimcp_habenula_plasticity_connect_bio_async(nimcp_habenula_plasticity_bridge_t* b) { if (!b) return -1; b->state.bio_async_connected = true; return 0; }
int nimcp_habenula_plasticity_disconnect_bio_async(nimcp_habenula_plasticity_bridge_t* b) { if (!b) return -1; b->state.bio_async_connected = false; return 0; }
bool nimcp_habenula_plasticity_is_bio_async_connected(const nimcp_habenula_plasticity_bridge_t* b) { return b ? b->state.bio_async_connected : false; }
