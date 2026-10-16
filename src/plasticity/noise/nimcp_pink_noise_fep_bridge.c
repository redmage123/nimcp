/**
 * @file nimcp_pink_noise_fep_bridge.c
 * @brief Pink Noise FEP Bridge Implementation
 */

#include "plasticity/noise/nimcp_pink_noise_fep_bridge.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/math/nimcp_math_helpers.h"
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pink_noise_fep_bridge, MESH_ADAPTER_CATEGORY_PLASTICITY)

#define LOG_MODULE "PINK_NOISE_FEP_BRIDGE"


int pink_noise_fep_bridge_default_config(pink_noise_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_fep_bridge_default_config: config is NULL");
        return -1;
    }
    config->precision_amplitude_gain = 0.5f;
    config->free_energy_alpha_gain = 0.2f;
    config->base_amplitude = 0.05f;
    config->base_alpha = 1.0f;
    config->enable_precision_amplitude = true;
    config->enable_fe_spectral_modulation = true;
    config->enable_noise_uncertainty_feedback = true;
    return 0;
}

pink_noise_fep_bridge_t* pink_noise_fep_bridge_create(const pink_noise_fep_config_t* config) {
    pink_noise_fep_bridge_t* bridge = (pink_noise_fep_bridge_t*)nimcp_malloc(sizeof(pink_noise_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    if (config) memcpy(&bridge->config, config, sizeof(pink_noise_fep_config_t));
    else pink_noise_fep_bridge_default_config(&bridge->config);

    memset(&bridge->fep_effects, 0, sizeof(pink_noise_fep_effects_t));
    memset(&bridge->noise_effects, 0, sizeof(pink_noise_fep_feedback_t));
    memset(&bridge->stats, 0, sizeof(pink_noise_fep_stats_t));

    if (bridge_base_init(&bridge->base, 0, "pink_noise_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->fep_system = NULL;
    bridge->noise_generator = NULL;
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Created %s bridge", "pink_noise_fep");
    return bridge;
}

void pink_noise_fep_bridge_destroy(pink_noise_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pink_noise_fep");
    if (bridge->base.bio_async_enabled) pink_noise_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int pink_noise_fep_bridge_connect_fep(pink_noise_fep_bridge_t* bridge, fep_system_t* fep) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_fep_bridge_connect_fep: bridge is NULL");
        return -1;
    }
    if (!fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_fep_bridge_connect_fep: fep is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pink_noise_fep_bridge_connect_noise(pink_noise_fep_bridge_t* bridge, pink_noise_generator_t generator) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_fep_bridge_connect_noise: bridge is NULL");
        return -1;
    }
    if (!generator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_fep_bridge_connect_noise: generator is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->noise_generator = generator;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pink_noise_fep_bridge_disconnect(pink_noise_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_fep_bridge_disconnect: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->noise_generator = NULL;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float pink_noise_fep_compute_amplitude_from_precision(pink_noise_fep_bridge_t* bridge, float precision) {
    if (!bridge || !bridge->config.enable_precision_amplitude) return bridge->config.base_amplitude;

    /* Lower precision → higher noise amplitude (explore) */
    float scaling = 1.0f / (precision + 0.1f);
    float amplitude = bridge->config.base_amplitude * scaling * bridge->config.precision_amplitude_gain;

    return nimcp_clampf(amplitude, PINK_NOISE_FEP_AMPLITUDE_MIN, PINK_NOISE_FEP_AMPLITUDE_MAX);
}

float pink_noise_fep_compute_alpha_from_free_energy(pink_noise_fep_bridge_t* bridge, float free_energy) {
    if (!bridge || !bridge->config.enable_fe_spectral_modulation) return bridge->config.base_alpha;

    /* Higher free energy → steeper spectral slope (more low frequency) */
    float alpha = bridge->config.base_alpha + free_energy * bridge->config.free_energy_alpha_gain;

    return nimcp_clampf(alpha, PINK_NOISE_FEP_ALPHA_MIN, PINK_NOISE_FEP_ALPHA_MAX);
}

float pink_noise_fep_compute_uncertainty_from_noise(const pink_noise_fep_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_noise_uncertainty_feedback) return 0.0f;

    /* Noise amplitude reflects environmental uncertainty */
    return bridge->noise_effects.noise_amplitude / PINK_NOISE_FEP_AMPLITUDE_MAX;
}

int pink_noise_fep_bridge_update(pink_noise_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_fep_bridge_update: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->fep_system) {
        /* Get FEP state */
        fep_belief_t beliefs;
        fep_get_beliefs(bridge->fep_system, 0, &beliefs);
        bridge->fep_effects.precision_value = beliefs.precision ? beliefs.precision[0] : 1.0f;

        fep_free_energy_t fe;
        fep_compute_free_energy(bridge->fep_system, &fe);
        bridge->fep_effects.free_energy_value = fe.total;

        /* Compute noise modulation */
        bridge->fep_effects.effective_amplitude = pink_noise_fep_compute_amplitude_from_precision(bridge, bridge->fep_effects.precision_value);
        bridge->fep_effects.effective_alpha = pink_noise_fep_compute_alpha_from_free_energy(bridge, bridge->fep_effects.free_energy_value);

        bridge->noise_effects.noise_amplitude = bridge->fep_effects.effective_amplitude;
        bridge->noise_effects.noise_alpha = bridge->fep_effects.effective_alpha;
        bridge->noise_effects.uncertainty_estimate = pink_noise_fep_compute_uncertainty_from_noise(bridge);

        /* Update statistics */
        bridge->stats.total_updates++;
        bridge->stats.avg_amplitude = (bridge->stats.avg_amplitude * (bridge->stats.total_updates - 1) + bridge->fep_effects.effective_amplitude) / bridge->stats.total_updates;
        bridge->stats.avg_alpha = (bridge->stats.avg_alpha * (bridge->stats.total_updates - 1) + bridge->fep_effects.effective_alpha) / bridge->stats.total_updates;
        bridge->stats.avg_precision_scaling = (bridge->stats.avg_precision_scaling * (bridge->stats.total_updates - 1) + bridge->fep_effects.precision_value) / bridge->stats.total_updates;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int pink_noise_fep_bridge_get_stats(const pink_noise_fep_bridge_t* bridge, pink_noise_fep_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_fep_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_fep_bridge_get_stats: stats is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(pink_noise_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int pink_noise_fep_bridge_connect_bio_async(pink_noise_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_fep_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_NOISE_BRIDGE,
        .module_name = "pink_noise_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
    }
    return 0;
}

int pink_noise_fep_bridge_disconnect_bio_async(pink_noise_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_fep_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool pink_noise_fep_bridge_is_bio_async_connected(const pink_noise_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
