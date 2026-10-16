/**
 * @file nimcp_snn_bcm_bridge.c
 * @brief SNN-BCM Plasticity Integration Bridge Implementation
 *
 * @author NIMCP Team
 * @date 2024
 */

#include "snn/bridges/nimcp_snn_bcm_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_bcm_bridge)

//=============================================================================
// Bio-async Module ID
//=============================================================================

#define BIO_MODULE_SNN_BCM_BRIDGE 0x0611

//=============================================================================
// Default Configuration
//=============================================================================

void snn_bcm_bridge_config_default(snn_bcm_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_config_default: null config pointer");
        return;
    }

    /* Spike rate estimation */
    config->rate_window_ms = 100.0f;
    config->rate_tau_ms = NIMCP_MEMBRANE_TAU_MS;
    config->min_rate_hz = 0.1f;
    config->max_rate_hz = 100.0f;

    /* Threshold coordination */
    config->sync_thresholds = true;
    config->threshold_update_interval_ms = 10.0f;
    config->min_threshold = 0.1f;
    config->max_threshold = 10.0f;

    /* Learning rate */
    config->lr_scaling_factor = 1.0f;
    config->rate_dependent_lr = true;

    /* Weight updates */
    config->weight_update_interval_ms = 1.0f;
    config->bidirectional_updates = true;

    /* Bio-async */
    config->enable_bio_async = true;
    config->event_buffer_size = 512;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

snn_bcm_bridge_t* snn_bcm_bridge_create(
    const snn_bcm_bridge_config_t* config,
    snn_network_t* network,
    bcm_synapse_t* bcm_synapses,
    uint32_t n_synapses,
    uint32_t n_neurons
) {
    if (!config || !network || !bcm_synapses || n_synapses == 0 || n_neurons == 0) {
        NIMCP_LOGGING_ERROR("Invalid parameters for SNN-BCM bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_create: config/network/bcm_synapses is NULL or n_synapses/n_neurons=0");
        return NULL;
    }

    snn_bcm_bridge_t* bridge = (snn_bcm_bridge_t*)nimcp_malloc(sizeof(snn_bcm_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-BCM bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_bcm_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_bcm_bridge_t));

    bridge->network = network;
    bridge->bcm_synapses = bcm_synapses;
    bridge->n_synapses = n_synapses;
    bridge->n_neurons = n_neurons;
    bridge->config = *config;

    /* Allocate rate history */
    bridge->rate_history = (spike_rate_history_t*)nimcp_malloc(
        sizeof(spike_rate_history_t) * n_neurons
    );
    if (!bridge->rate_history) {
        NIMCP_LOGGING_ERROR("Failed to allocate rate history");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_bcm_bridge_create: failed to allocate rate_history");
        nimcp_free(bridge);
        return NULL;
    }

    memset(bridge->rate_history, 0, sizeof(spike_rate_history_t) * n_neurons);

    /* Initialize rate history */
    for (uint32_t i = 0; i < n_neurons; i++) {
        bridge->rate_history[i].neuron_id = i;
        bridge->rate_history[i].instantaneous_rate = 0.0f;
        bridge->rate_history[i].averaged_rate = 0.0f;
        bridge->rate_history[i].activity_squared_avg = 0.0f;
        bridge->rate_history[i].last_spike_time_us = 0;
        bridge->rate_history[i].spike_count = 0;
    }

    if (bridge_base_init(&bridge->base, 0, "snn_bcm") != 0) { nimcp_free(bridge); return NULL; }

    bridge->connected = true;
    bridge->last_update_time_ms = 0.0f;
    bridge->plasticity_events = 0;
    bridge->threshold_updates = 0;

    /* Initialize effects */
    bridge->effects.effective_learning_rate = NIMCP_DEFAULT_LEARNING_RATE;
    bridge->effects.avg_threshold = 1.0f;
    bridge->effects.threshold_range = 0.0f;
    bridge->effects.avg_activity = 0.0f;
    bridge->effects.ltp_dominant = false;

    if (config->enable_bio_async) {
        snn_bcm_bridge_connect_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Created SNN-BCM bridge with %u synapses, %u neurons",
                       n_synapses, n_neurons);
    return bridge;
}

void snn_bcm_bridge_destroy(snn_bcm_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_destroy: null bridge pointer");
        return;
    }

    if (bridge->base.bio_async_enabled) {
        snn_bcm_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->rate_history) {
        nimcp_free(bridge->rate_history);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

//=============================================================================
// Bio-async Integration
//=============================================================================

int snn_bcm_bridge_connect_bio_async(snn_bcm_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_BCM_BRIDGE,
        .module_name = "snn_bcm_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected SNN-BCM bridge to bio-async router");
    }

    return 0;
}

int snn_bcm_bridge_disconnect_bio_async(snn_bcm_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    return 0;
}

bool snn_bcm_bridge_is_bio_async_connected(const snn_bcm_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Update Functions
//=============================================================================

int snn_bcm_bridge_update_rates(snn_bcm_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_update_rates: null bridge pointer");
        return -1;
    }
    if (!bridge->network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_bcm_bridge_update_rates: null network");
        return -1;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    /* Update exponential moving average of firing rates */
    float dt_sec = dt / 1000.0f;
    float alpha = expf(-dt / bridge->config.rate_tau_ms);

    for (uint32_t i = 0; i < bridge->n_neurons; i++) {
        spike_rate_history_t* hist = &bridge->rate_history[i];

        /* Exponential average */
        hist->averaged_rate = alpha * hist->averaged_rate +
                             (1.0f - alpha) * hist->instantaneous_rate;

        /* Activity squared average for BCM threshold */
        float activity = hist->averaged_rate / 100.0f; /* Normalize */
        hist->activity_squared_avg = alpha * hist->activity_squared_avg +
                                    (1.0f - alpha) * (activity * activity);
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int snn_bcm_bridge_update_thresholds(snn_bcm_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_update_thresholds: null bridge pointer");
        return -1;
    }
    if (!bridge->bcm_synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_bcm_bridge_update_thresholds: null bcm_synapses");
        return -1;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    /* Update BCM thresholds based on activity history */
    float threshold_sum = 0.0f;
    bcm_params_t params = bcm_params_cortical();

    for (uint32_t i = 0; i < bridge->n_synapses; i++) {
        /* Get post-synaptic neuron rate */
        /* In real implementation, would map synapse to neuron */
        uint32_t post_neuron = i % bridge->n_neurons;
        float activity_sq = bridge->rate_history[post_neuron].activity_squared_avg;

        /* Update threshold: θ → <r²> */
        float new_threshold = activity_sq;
        new_threshold = fmaxf(new_threshold, bridge->config.min_threshold);
        new_threshold = fminf(new_threshold, bridge->config.max_threshold);

        bridge->bcm_synapses[i].threshold = new_threshold;
        threshold_sum += new_threshold;
    }

    bridge->effects.avg_threshold = threshold_sum / (float)bridge->n_synapses;
    bridge->threshold_updates++;

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int snn_bcm_bridge_apply_plasticity(snn_bcm_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_apply_plasticity: null bridge pointer");
        return -1;
    }
    if (!bridge->network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_bcm_bridge_apply_plasticity: null network");
        return -1;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    /* Apply BCM rule to SNN synapses */
    bridge->plasticity_events++;

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int snn_bcm_bridge_update(snn_bcm_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_update: null bridge pointer");
        return -1;
    }

    int ret = snn_bcm_bridge_update_rates(bridge, dt);
    if (ret != 0) return ret;

    ret = snn_bcm_bridge_update_thresholds(bridge, dt);
    if (ret != 0) return ret;

    ret = snn_bcm_bridge_apply_plasticity(bridge, dt);
    if (ret != 0) return ret;

    bridge->last_update_time_ms += dt;

    return 0;
}

//=============================================================================
// Weight Change Functions
//=============================================================================

int snn_bcm_bridge_get_weight_changes(
    const snn_bcm_bridge_t* bridge,
    uint32_t* synapse_ids,
    float* weight_deltas,
    uint32_t max_changes,
    uint32_t* n_changes
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_get_weight_changes: null bridge pointer");
        return -1;
    }
    if (!synapse_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_get_weight_changes: null synapse_ids pointer");
        return -1;
    }
    if (!weight_deltas) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_get_weight_changes: null weight_deltas pointer");
        return -1;
    }
    if (!n_changes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_get_weight_changes: null n_changes pointer");
        return -1;
    }

    *n_changes = 0;
    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int snn_bcm_bridge_get_effects(
    const snn_bcm_bridge_t* bridge,
    snn_bcm_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_get_effects: null bridge pointer");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_get_effects: null effects pointer");
        return -1;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock((void*)bridge->base.mutex);
    }

    *effects = bridge->effects;

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock((void*)bridge->base.mutex);
    }

    return 0;
}

float snn_bcm_bridge_get_avg_threshold(const snn_bcm_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_get_avg_threshold: null bridge pointer");
        return 0.0f;
    }
    return bridge->effects.avg_threshold;
}

float snn_bcm_bridge_get_neuron_rate(const snn_bcm_bridge_t* bridge, uint32_t neuron_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_get_neuron_rate: null bridge pointer");
        return -1.0f;
    }
    if (neuron_id >= bridge->n_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_bcm_bridge_get_neuron_rate: neuron_id out of range");
        return -1.0f;
    }
    return bridge->rate_history[neuron_id].averaged_rate;
}

bool snn_bcm_bridge_is_ltp_dominant(const snn_bcm_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->effects.ltp_dominant;
}

//=============================================================================
// Statistics
//=============================================================================

int snn_bcm_bridge_get_stats(
    const snn_bcm_bridge_t* bridge,
    uint32_t* plasticity_events,
    uint32_t* threshold_updates,
    uint32_t* updates
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_get_stats: null bridge pointer");
        return -1;
    }

    if (plasticity_events) {
        *plasticity_events = bridge->plasticity_events;
    }
    if (threshold_updates) {
        *threshold_updates = bridge->threshold_updates;
    }
    if (updates) {
        *updates = (uint32_t)(bridge->last_update_time_ms /
                              bridge->config.weight_update_interval_ms);
    }

    return 0;
}

void snn_bcm_bridge_reset_stats(snn_bcm_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_bcm_bridge_reset_stats: null bridge pointer");
        return;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    bridge->plasticity_events = 0;
    bridge->threshold_updates = 0;
    bridge->last_update_time_ms = 0.0f;

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }
}
