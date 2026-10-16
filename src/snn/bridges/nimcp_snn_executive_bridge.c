/**
 * @file nimcp_snn_executive_bridge.c
 * @brief SNN-Executive Control integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_executive_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_executive_bridge)

#define BIO_MODULE_SNN_EXECUTIVE_BRIDGE 0x0613

void snn_executive_config_default(snn_executive_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_config_default: null config pointer");
        return;
    }
    config->inhibition_rate_threshold = 30.0f;
    config->task_switch_rate_change = 15.0f;
    config->cognitive_load_max_rate = 80.0f;
    config->enable_interneuron_control = true;
    config->interneuron_efficacy = 0.5f;
    config->executive_population_id = 0;
    config->interneuron_population_id = 0;
    config->update_interval_ms = 100.0f;
    config->enable_bio_async = false;
}

snn_executive_bridge_t* snn_executive_bridge_create(
    const snn_executive_config_t* config,
    snn_network_t* snn,
    executive_controller_t* executive
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_bridge_create: config is NULL");
        return NULL;
    }
    if (!snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_bridge_create: snn is NULL");
        return NULL;
    }
    if (!executive) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_bridge_create: executive is NULL");
        return NULL;
    }

    snn_executive_bridge_t* bridge = nimcp_malloc(sizeof(snn_executive_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-executive bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_executive_bridge_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_executive_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_executive") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->executive = executive;
    bridge->config = *config;

    if (config->executive_population_id > 0) {
        bridge->executive_pop = snn_network_get_population(snn, config->executive_population_id);
    }

    if (config->interneuron_population_id > 0) {
        bridge->interneuron_pop = snn_network_get_population(snn, config->interneuron_population_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-executive bridge");
    return bridge;
}

void snn_executive_bridge_destroy(snn_executive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_bridge_destroy: null bridge pointer");
        return;
    }
    if (bridge->base.bio_async_enabled) {
        snn_executive_bridge_disconnect_bio_async(bridge);
    }
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-executive bridge");
}

int snn_executive_bridge_connect_bio_async(snn_executive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_EXECUTIVE_BRIDGE,
        .module_name = "snn_executive_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_executive_bridge_connect_bio_async: validation failed");
    return -1;
}

int snn_executive_bridge_disconnect_bio_async(snn_executive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_executive_bridge_is_bio_async_connected(const snn_executive_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_executive_bridge_process(
    snn_executive_bridge_t* bridge,
    const float* input,
    float* output
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_bridge_process: null bridge pointer");
        return -1;
    }

    int ret = snn_executive_bridge_update(bridge, bridge->config.update_interval_ms);
    if (ret != 0) return ret;

    if (output) {
        output[0] = bridge->state.inhibition_strength;
        output[1] = bridge->state.cognitive_load;
    }

    return 0;
}

int snn_executive_bridge_update(snn_executive_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_bridge_update: null bridge pointer");
        return -1;
    }

    if (bridge->last_update_time > 0 && (dt < bridge->config.update_interval_ms)) {
        return 0;
    }

    /* Update executive population rate */
    if (bridge->executive_pop) {
        bridge->state.executive_rate = snn_network_get_population_rate(
            bridge->snn,
            bridge->config.executive_population_id,
            bridge->config.update_interval_ms
        );

        /* Compute cognitive load */
        bridge->state.cognitive_load = snn_executive_compute_cognitive_load(
            bridge, bridge->state.executive_rate
        );

        /* Detect task switch */
        bridge->state.task_switching = snn_executive_detect_task_switch(
            bridge, bridge->state.executive_rate
        );

        if (bridge->state.task_switching) {
            bridge->state.switch_count++;
        }

        bridge->last_executive_rate = bridge->state.executive_rate;
    }

    /* Update interneuron rate and compute inhibition */
    if (bridge->interneuron_pop) {
        bridge->state.interneuron_rate = snn_network_get_population_rate(
            bridge->snn,
            bridge->config.interneuron_population_id,
            bridge->config.update_interval_ms
        );

        bridge->state.inhibition_strength = snn_executive_compute_inhibition(
            bridge, bridge->state.interneuron_rate
        );

        /* Apply inhibition */
        snn_executive_apply_inhibition(bridge, bridge->state.inhibition_strength);
    }

    bridge->last_update_time += dt;
    return 0;
}

float snn_executive_compute_inhibition(
    const snn_executive_bridge_t* bridge,
    float interneuron_rate
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_compute_inhibition: null bridge pointer");
        return 0.0f;
    }

    /* Linear scaling above threshold */
    float threshold = bridge->config.inhibition_rate_threshold;
    if (interneuron_rate < threshold) {
        return 0.0f;
    }

    float excess = interneuron_rate - threshold;
    float inhibition = excess / 50.0f;  /* Scale factor */

    /* Apply efficacy */
    inhibition *= bridge->config.interneuron_efficacy;

    /* Clamp to [0, 1] */
    if (inhibition > 1.0f) inhibition = 1.0f;

    return inhibition;
}

bool snn_executive_detect_task_switch(
    snn_executive_bridge_t* bridge,
    float current_rate
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_detect_task_switch: null bridge pointer");
        return false;
    }

    /* Detect significant rate change */
    float rate_change = fabsf(current_rate - bridge->last_executive_rate);
    return (rate_change >= bridge->config.task_switch_rate_change);
}

float snn_executive_compute_cognitive_load(
    const snn_executive_bridge_t* bridge,
    float population_rate
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_compute_cognitive_load: null bridge pointer");
        return 0.0f;
    }

    float max_rate = bridge->config.cognitive_load_max_rate;
    float load = population_rate / max_rate;

    /* Clamp to [0, 1] */
    if (load > 1.0f) load = 1.0f;

    return load;
}

int snn_executive_apply_inhibition(
    snn_executive_bridge_t* bridge,
    float inhibition_strength
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_apply_inhibition: null bridge pointer");
        return -1;
    }

    if (!bridge->config.enable_interneuron_control) {
        return 0;  /* Disabled */
    }

    /* Note: Actual implementation would modulate excitatory populations */
    /* This requires access to population/neuron input currents */
    /* Placeholder for demonstration */

    return 0;
}

int snn_executive_bridge_get_state(
    const snn_executive_bridge_t* bridge,
    snn_executive_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

float snn_executive_get_inhibition(const snn_executive_bridge_t* bridge) {
    return bridge ? bridge->state.inhibition_strength : 0.0f;
}

float snn_executive_get_cognitive_load(const snn_executive_bridge_t* bridge) {
    return bridge ? bridge->state.cognitive_load : 0.0f;
}

bool snn_executive_is_task_switching(const snn_executive_bridge_t* bridge) {
    return bridge ? bridge->state.task_switching : false;
}

int snn_executive_get_stats(
    const snn_executive_bridge_t* bridge,
    uint32_t* switch_count,
    float* avg_inhibition,
    float* avg_load
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_executive_get_stats: bridge is NULL");
        return -1;
    }
    if (switch_count) *switch_count = bridge->state.switch_count;
    if (avg_inhibition) *avg_inhibition = bridge->state.inhibition_strength;
    if (avg_load) *avg_load = bridge->state.cognitive_load;
    return 0;
}

void snn_executive_reset_stats(snn_executive_bridge_t* bridge) {
    if (!bridge) return;
    bridge->state.switch_count = 0;
    bridge->state.inhibition_strength = 0.0f;
    bridge->state.cognitive_load = 0.0f;
}
