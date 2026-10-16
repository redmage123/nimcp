/**
 * @file nimcp_raphe_plasticity_bridge.c
 * @brief Implementation of Raphe-Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/raphe/nimcp_raphe_plasticity_bridge.h"
#include "core/brain/regions/raphe/nimcp_raphe_adapter.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(raphe_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "RAPHE_PLASTICITY_BRIDGE"


struct nimcp_raphe_plasticity_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_raphe_plasticity_config_t config;
    nimcp_raphe_adapter_t raphe_adapter;
    void* plasticity_coordinator;
    nimcp_raphe_plasticity_synapse_t* synapses;
    uint32_t synapse_count;
    uint32_t synapse_capacity;
    nimcp_raphe_plasticity_bridge_state_t state;
    nimcp_raphe_plasticity_modulation_t current_modulation;
    nimcp_raphe_weight_change_cb weight_callback;
    void* callback_user_data;
    nimcp_raphe_plasticity_stats_t stats;
};

static nimcp_raphe_plasticity_synapse_t* find_synapse(nimcp_raphe_plasticity_bridge_t* b, uint32_t id) {
    for (uint32_t i = 0; i < b->synapse_count; i++) if (b->synapses[i].synapse_id == id) return &b->synapses[i];
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_synapse: operation failed");
    return NULL;
}

nimcp_raphe_plasticity_config_t nimcp_raphe_plasticity_config_default(void) {
    return (nimcp_raphe_plasticity_config_t){
        .enable_ht_gating = true, .ht_lr_multiplier_min = 0.5f, .ht_lr_multiplier_max = 2.0f, .ht_gating_threshold = 0.3f,
        .enable_mood_modulation = true, .positive_mood_ltp_boost = 1.5f, .negative_mood_ltd_boost = 1.5f, .mood_bias_strength = 0.5f,
        .stdp_ltp_window_ms = RAPHE_PLASTICITY_STDP_WINDOW, .stdp_ltd_window_ms = RAPHE_PLASTICITY_STDP_WINDOW, .ht_stdp_modulation = 0.5f,
        .enable_inhibition_learning = true, .inhibition_learning_rate = NIMCP_LEARNING_RATE_COARSE, .successful_inhibition_boost = 1.5f,
        .enable_extinction = true, .extinction_rate = 0.05f, .safety_learning_rate = NIMCP_LEARNING_RATE_COARSE, .extinction_ht_requirement = 0.5f,
        .enable_patience_learning = true, .patience_learning_rate = NIMCP_LEARNING_RATE_COARSE, .delayed_reward_boost = 2.0f,
        .weight_min = 0.0f, .weight_max = 1.0f, .initial_weight = NIMCP_SYNAPSE_WEIGHT_INIT, .enable_bio_async = false
    };
}

nimcp_raphe_plasticity_bridge_t* nimcp_raphe_plasticity_create(const nimcp_raphe_plasticity_config_t* config) {
    nimcp_raphe_plasticity_bridge_t* b = nimcp_calloc(1, sizeof(*b));
    if (!b) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "b is NULL");

        return NULL;

    }
    b->config = config ? *config : nimcp_raphe_plasticity_config_default();
    b->synapse_capacity = RAPHE_PLASTICITY_MAX_SYNAPSES;
    b->synapses = nimcp_calloc(b->synapse_capacity, sizeof(nimcp_raphe_plasticity_synapse_t));
    if (!b->synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_raphe_plasticity_create: synapses allocation failed");
        nimcp_free(b);
        return NULL;
    }
    b->state.state = RAPHE_PLASTICITY_STATE_IDLE;
    b->current_modulation.lr_multiplier = 1.0f;
    return b;
}

void nimcp_raphe_plasticity_destroy(nimcp_raphe_plasticity_bridge_t* b) {
    if (!b) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "raphe_plasticity");
    nimcp_free(b->synapses);
    nimcp_free(b);
}

int nimcp_raphe_plasticity_reset(nimcp_raphe_plasticity_bridge_t* b) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_reset: b is NULL");
        return -1;
    }
    for (uint32_t i = 0; i < b->synapse_count; i++) b->synapses[i].weight = b->synapses[i].initial_weight;
    memset(&b->state, 0, sizeof(b->state));
    b->state.state = RAPHE_PLASTICITY_STATE_IDLE;
    memset(&b->stats, 0, sizeof(b->stats));
    return 0;
}

int nimcp_raphe_plasticity_connect_raphe(nimcp_raphe_plasticity_bridge_t* b, nimcp_raphe_adapter_t a) { if (!b||!a) return -1; b->raphe_adapter = a; return 0; }
int nimcp_raphe_plasticity_connect_coordinator(nimcp_raphe_plasticity_bridge_t* b, struct nimcp_plasticity_coordinator* c) { if (!b||!c) return -1; b->plasticity_coordinator = c; return 0; }

int nimcp_raphe_plasticity_register_synapse(nimcp_raphe_plasticity_bridge_t* b, uint32_t id, nimcp_raphe_synapse_type_t t, float w) {
    if (!b || b->synapse_count >= b->synapse_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_raphe_plasticity_register_synapse: b is NULL");
        return -1;
    }
    nimcp_raphe_plasticity_synapse_t* s = &b->synapses[b->synapse_count++];
    s->synapse_id = id; s->type = t; s->weight = s->initial_weight = w;
    b->state.registered_synapses = b->synapse_count;
    return 0;
}

int nimcp_raphe_plasticity_unregister_synapse(nimcp_raphe_plasticity_bridge_t* b, uint32_t id) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_unregister_synapse: b is NULL");
        return -1;
    }
    for (uint32_t i = 0; i < b->synapse_count; i++) {
        if (b->synapses[i].synapse_id == id) {
            if (i < b->synapse_count - 1) b->synapses[i] = b->synapses[b->synapse_count - 1];
            b->synapse_count--;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_raphe_plasticity_unregister_synapse: validation failed");
    return -1;
}

int nimcp_raphe_plasticity_get_synapse(nimcp_raphe_plasticity_bridge_t* b, uint32_t id, nimcp_raphe_plasticity_synapse_t* o) {
    if (!b || !o) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_get_synapse: required parameter is NULL (b, o)");
        return -1;
    }
    nimcp_raphe_plasticity_synapse_t* s = find_synapse(b, id);
    if (!s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_get_synapse: s is NULL");
        return -1;
    }
    *o = *s;
    return 0;
}

int nimcp_raphe_plasticity_pre_spike(nimcp_raphe_plasticity_bridge_t* b, uint32_t id, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_pre_spike: b is NULL");
        return -1;
    }
    nimcp_raphe_plasticity_synapse_t* s = find_synapse(b, id);
    if (!s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_pre_spike: s is NULL");
        return -1;
    }
    s->last_pre_spike_us = ts;
    b->stats.total_pre_spikes++;
    return 0;
}

int nimcp_raphe_plasticity_post_spike(nimcp_raphe_plasticity_bridge_t* b, uint32_t id, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_post_spike: b is NULL");
        return -1;
    }
    nimcp_raphe_plasticity_synapse_t* s = find_synapse(b, id);
    if (!s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_post_spike: s is NULL");
        return -1;
    }
    s->last_post_spike_us = ts;
    b->stats.total_post_spikes++;
    return 0;
}

int nimcp_raphe_plasticity_inhibition_success(nimcp_raphe_plasticity_bridge_t* b, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_inhibition_success: b is NULL");
        return -1;
    }
    b->stats.inhibition_successes++;
    return 0;
}

int nimcp_raphe_plasticity_inhibition_failure(nimcp_raphe_plasticity_bridge_t* b, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_inhibition_failure: b is NULL");
        return -1;
    }
    b->stats.inhibition_failures++;
    return 0;
}

int nimcp_raphe_plasticity_extinction_trial(nimcp_raphe_plasticity_bridge_t* b, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_extinction_trial: b is NULL");
        return -1;
    }
    b->stats.extinction_trials++;
    return 0;
}

int nimcp_raphe_plasticity_set_ht_state(nimcp_raphe_plasticity_bridge_t* b, float ht, float mood) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_set_ht_state: b is NULL");
        return -1;
    }
    b->state.ht.serotonin_level = nimcp_clampf(ht, 0.0f, 100.0f);
    b->state.ht.mood_valence = nimcp_clampf(mood, -1.0f, 1.0f);
    float n = ht / 100.0f;
    b->current_modulation.lr_multiplier = b->config.ht_lr_multiplier_min +
        (b->config.ht_lr_multiplier_max - b->config.ht_lr_multiplier_min) * n;
    b->current_modulation.mood_bias = mood * b->config.mood_bias_strength;
    return 0;
}

int nimcp_raphe_plasticity_update(nimcp_raphe_plasticity_bridge_t* b, float dt_ms) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_update: b is NULL");
        return -1;
    }
    b->stats.total_updates++;
    return 0;
}

int nimcp_raphe_plasticity_start_patience(nimcp_raphe_plasticity_bridge_t* b, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_start_patience: b is NULL");
        return -1;
    }
    b->state.ht.patience_active = true;
    b->state.ht.patience_start_us = ts;
    return 0;
}

int nimcp_raphe_plasticity_complete_patience(nimcp_raphe_plasticity_bridge_t* b, float r, uint64_t ts) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_complete_patience: b is NULL");
        return -1;
    }
    b->state.ht.patience_active = false;
    return 0;
}

int nimcp_raphe_plasticity_get_modulation(nimcp_raphe_plasticity_bridge_t* b, nimcp_raphe_plasticity_modulation_t* m) {
    if (!b || !m) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_plasticity_get_modulation: required parameter is NULL (b, m)");
        return -1;
    }
    *m = b->current_modulation;
    return 0;
}

float nimcp_raphe_plasticity_get_inhibition_strength(nimcp_raphe_plasticity_bridge_t* b) {
    return b ? b->state.avg_inhibition_strength : 0.0f;
}

float nimcp_raphe_plasticity_get_extinction_progress(nimcp_raphe_plasticity_bridge_t* b) {
    if (!b || b->synapse_count == 0) return 0.0f;
    float total = 0.0f;
    for (uint32_t i = 0; i < b->synapse_count; i++) total += b->synapses[i].extinction_level;
    return total / b->synapse_count;
}

float nimcp_raphe_plasticity_get_mood_feedback(nimcp_raphe_plasticity_bridge_t* b) {
    return b ? b->state.ht.mood_valence : 0.0f;
}

int nimcp_raphe_plasticity_get_state(const nimcp_raphe_plasticity_bridge_t* b, nimcp_raphe_plasticity_bridge_state_t* s) { if (!b||!s) return -1; *s = b->state; return 0; }
int nimcp_raphe_plasticity_get_stats(const nimcp_raphe_plasticity_bridge_t* b, nimcp_raphe_plasticity_stats_t* s) { if (!b||!s) return -1; *s = b->stats; return 0; }
void nimcp_raphe_plasticity_reset_stats(nimcp_raphe_plasticity_bridge_t* b) { if (b) memset(&b->stats, 0, sizeof(b->stats)); }
int nimcp_raphe_plasticity_set_weight_callback(nimcp_raphe_plasticity_bridge_t* b, nimcp_raphe_weight_change_cb cb, void* ud) { if (!b) return -1; b->weight_callback = cb; b->callback_user_data = ud; return 0; }
int nimcp_raphe_plasticity_connect_bio_async(nimcp_raphe_plasticity_bridge_t* b) { if (!b) return -1; b->state.bio_async_connected = true; return 0; }
int nimcp_raphe_plasticity_disconnect_bio_async(nimcp_raphe_plasticity_bridge_t* b) { if (!b) return -1; b->state.bio_async_connected = false; return 0; }
bool nimcp_raphe_plasticity_is_bio_async_connected(const nimcp_raphe_plasticity_bridge_t* b) { return b ? b->state.bio_async_connected : false; }
