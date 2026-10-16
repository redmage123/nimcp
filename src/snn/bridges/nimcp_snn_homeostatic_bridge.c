/**
 * @file nimcp_snn_homeostatic_bridge.c
 * @brief SNN-Homeostatic Plasticity Integration Bridge Implementation
 */

#include "snn/bridges/nimcp_snn_homeostatic_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_homeostatic_bridge)

#define BIO_MODULE_SNN_HOMEOSTATIC_BRIDGE 0x0612

void snn_homeostatic_bridge_config_default(snn_homeostatic_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_config_default: null config pointer");
        return;
    }

    config->target_rate_hz = 5.0f;
    config->rate_tolerance = 0.5f;
    config->rate_window_ms = 1000.0f;

    config->enable_threshold_adaptation = true;
    config->threshold_tau_ms = 10000.0f; /* 10 seconds */
    config->threshold_step_size = 0.5f; /* mV */
    config->min_threshold_mv = -70.0f;
    config->max_threshold_mv = -40.0f;

    config->update_interval_ms = 100.0f;
    config->bidirectional_updates = true;

    config->enable_bio_async = true;
}

snn_homeostatic_bridge_t* snn_homeostatic_bridge_create(
    const snn_homeostatic_bridge_config_t* config,
    snn_network_t* network,
    homeostatic_controller_t controller,
    uint32_t n_neurons
) {
    if (!config || !network || !controller || n_neurons == 0) {
        NIMCP_LOGGING_ERROR("Invalid parameters for SNN-Homeostatic bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_create: config/network/controller is NULL or n_neurons=0");
        return NULL;
    }

    snn_homeostatic_bridge_t* bridge = (snn_homeostatic_bridge_t*)
        nimcp_malloc(sizeof(snn_homeostatic_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-Homeostatic bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_homeostatic_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_homeostatic_bridge_t));

    bridge->network = network;
    bridge->controller = controller;
    bridge->n_neurons = n_neurons;
    bridge->config = *config;

    bridge->neuron_states = (neuron_homeostatic_state_t*)
        nimcp_malloc(sizeof(neuron_homeostatic_state_t) * n_neurons);
    if (!bridge->neuron_states) {
        NIMCP_LOGGING_ERROR("Failed to allocate neuron states");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_homeostatic_bridge_create: failed to allocate neuron_states");
        nimcp_free(bridge);
        return NULL;
    }

    memset(bridge->neuron_states, 0, sizeof(neuron_homeostatic_state_t) * n_neurons);

    for (uint32_t i = 0; i < n_neurons; i++) {
        bridge->neuron_states[i].neuron_id = i;
        bridge->neuron_states[i].current_rate = 0.0f;
        bridge->neuron_states[i].rate_deviation = 0.0f;
        bridge->neuron_states[i].threshold_adjustment = 0.0f;
        bridge->neuron_states[i].ip_state = intrinsic_plasticity_state_init(-55.0f, 1.0f);
        bridge->neuron_states[i].is_stable = true;
    }

    if (bridge_base_init(&bridge->base, 0, "snn_homeostatic") != 0) { nimcp_free(bridge); return NULL; }
    bridge->connected = true;

    if (config->enable_bio_async) {
        snn_homeostatic_bridge_connect_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Created SNN-Homeostatic bridge with %u neurons", n_neurons);
    return bridge;
}

void snn_homeostatic_bridge_destroy(snn_homeostatic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_destroy: null bridge pointer");
        return;
    }

    if (bridge->base.bio_async_enabled) {
        snn_homeostatic_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->neuron_states) {
        nimcp_free(bridge->neuron_states);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int snn_homeostatic_bridge_connect_bio_async(snn_homeostatic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_HOMEOSTATIC_BRIDGE,
        .module_name = "snn_homeostatic_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected SNN-Homeostatic bridge to bio-async");
    }

    return 0;
}

int snn_homeostatic_bridge_disconnect_bio_async(snn_homeostatic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    return 0;
}

bool snn_homeostatic_bridge_is_bio_async_connected(const snn_homeostatic_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_homeostatic_bridge_update_rates(snn_homeostatic_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_update_rates: null bridge pointer");
        return -1;
    }

    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);

    float rate_sum = 0.0f;
    uint32_t above = 0, below = 0;

    for (uint32_t i = 0; i < bridge->n_neurons; i++) {
        neuron_homeostatic_state_t* state = &bridge->neuron_states[i];

        /* Compute rate deviation */
        state->rate_deviation = state->current_rate - bridge->config.target_rate_hz;
        state->is_stable = fabsf(state->rate_deviation) < bridge->config.rate_tolerance;

        if (state->rate_deviation > 0) above++;
        else if (state->rate_deviation < 0) below++;

        rate_sum += state->current_rate;
    }

    bridge->effects.avg_firing_rate = rate_sum / (float)bridge->n_neurons;
    bridge->effects.rate_deviation = bridge->effects.avg_firing_rate -
                                    bridge->config.target_rate_hz;
    bridge->effects.neurons_above_target = above;
    bridge->effects.neurons_below_target = below;
    bridge->effects.network_stable = (above == 0 && below == 0);

    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int snn_homeostatic_bridge_apply_plasticity(snn_homeostatic_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_apply_plasticity: null bridge pointer");
        return -1;
    }
    if (!bridge->config.enable_threshold_adaptation) {
        return -1;
    }

    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);

    float threshold_sum = 0.0f;

    for (uint32_t i = 0; i < bridge->n_neurons; i++) {
        neuron_homeostatic_state_t* state = &bridge->neuron_states[i];

        if (!state->is_stable) {
            /* Adjust threshold: firing too much → increase threshold */
            float adjustment = state->rate_deviation > 0 ?
                bridge->config.threshold_step_size :
                -bridge->config.threshold_step_size;

            state->threshold_adjustment += adjustment;
            state->threshold_adjustment = fmaxf(state->threshold_adjustment,
                                               bridge->config.min_threshold_mv);
            state->threshold_adjustment = fminf(state->threshold_adjustment,
                                               bridge->config.max_threshold_mv);

            bridge->threshold_adjustments++;
        }

        threshold_sum += state->threshold_adjustment;
    }

    bridge->effects.avg_threshold_shift = threshold_sum / (float)bridge->n_neurons;

    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int snn_homeostatic_bridge_update(snn_homeostatic_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_update: null bridge pointer");
        return -1;
    }

    int ret = snn_homeostatic_bridge_update_rates(bridge, dt);
    if (ret != 0) return ret;

    ret = snn_homeostatic_bridge_apply_plasticity(bridge, dt);
    if (ret != 0) return ret;

    bridge->last_update_time_ms += dt;
    bridge->stability_checks++;

    return 0;
}

int snn_homeostatic_bridge_get_weight_changes(
    const snn_homeostatic_bridge_t* bridge,
    uint32_t* neuron_ids,
    float* threshold_adjustments,
    uint32_t max_changes,
    uint32_t* n_changes
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_get_weight_changes: null bridge pointer");
        return -1;
    }
    if (!neuron_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_get_weight_changes: null neuron_ids pointer");
        return -1;
    }
    if (!threshold_adjustments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_get_weight_changes: null threshold_adjustments pointer");
        return -1;
    }
    if (!n_changes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_get_weight_changes: null n_changes pointer");
        return -1;
    }

    *n_changes = 0;
    return 0;
}

int snn_homeostatic_bridge_get_effects(
    const snn_homeostatic_bridge_t* bridge,
    snn_homeostatic_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_get_effects: null bridge pointer");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_get_effects: null effects pointer");
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

float snn_homeostatic_bridge_get_avg_rate(const snn_homeostatic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_get_avg_rate: null bridge pointer");
        return 0.0f;
    }
    return bridge->effects.avg_firing_rate;
}

bool snn_homeostatic_bridge_is_stable(const snn_homeostatic_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->effects.network_stable;
}

int snn_homeostatic_bridge_get_stats(
    const snn_homeostatic_bridge_t* bridge,
    uint32_t* threshold_adjustments,
    uint32_t* stability_checks,
    uint32_t* updates
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_get_stats: null bridge pointer");
        return -1;
    }

    if (threshold_adjustments) *threshold_adjustments = bridge->threshold_adjustments;
    if (stability_checks) *stability_checks = bridge->stability_checks;
    if (updates) *updates = (uint32_t)(bridge->last_update_time_ms /
                                       bridge->config.update_interval_ms);

    return 0;
}

void snn_homeostatic_bridge_reset_stats(snn_homeostatic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_bridge_reset_stats: null bridge pointer");
        return;
    }

    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->threshold_adjustments = 0;
    bridge->stability_checks = 0;
    bridge->last_update_time_ms = 0.0f;

    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);
}
