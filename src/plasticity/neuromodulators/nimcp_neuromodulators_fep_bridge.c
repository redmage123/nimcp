/**
 * @file nimcp_neuromodulators_fep_bridge.c
 * @brief Neuromodulators FEP Bridge Implementation
 */

#include "plasticity/neuromodulators/nimcp_neuromodulators_fep_bridge.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromodulators_fep_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(neuromod_fep_bridge)

#define LOG_MODULE "NEUROMODULATORS_FEP_BRIDGE"


int neuromod_fep_bridge_default_config(neuromod_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_fep_bridge_default_config: config is NULL");
        return -1;
    }
    config->da_pe_sensitivity = NEUROMOD_FEP_DA_PE_GAIN;
    config->ach_precision_sensitivity = NEUROMOD_FEP_ACH_PRECISION_GAIN;
    config->ne_uncertainty_sensitivity = NEUROMOD_FEP_NE_UNCERTAINTY_GAIN;
    config->sht_complexity_sensitivity = NEUROMOD_FEP_5HT_COMPLEXITY_GAIN;
    config->enable_da_pe_coupling = true;
    config->enable_ach_precision_coupling = true;
    config->enable_ne_uncertainty_coupling = true;
    config->enable_sht_complexity_coupling = true;
    return 0;
}

neuromod_fep_bridge_t* neuromod_fep_bridge_create(const neuromod_fep_config_t* config) {
    neuromod_fep_bridge_t* bridge = (neuromod_fep_bridge_t*)nimcp_malloc(sizeof(neuromod_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromod_fep_bridge_create: failed to allocate bridge");
        return NULL;
    }

    if (config) memcpy(&bridge->config, config, sizeof(neuromod_fep_config_t));
    else neuromod_fep_bridge_default_config(&bridge->config);

    memset(&bridge->fep_effects, 0, sizeof(neuromod_fep_effects_t));
    memset(&bridge->neuromod_effects, 0, sizeof(neuromod_fep_feedback_t));
    memset(&bridge->stats, 0, sizeof(neuromod_fep_stats_t));

    if (bridge_base_init(&bridge->base, 0, "neuromodulators_fep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromod_fep_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromod_fep_bridge_create: mutex allocation failed");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->fep_system = NULL;
    bridge->neuromod_system = NULL;
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Created %s bridge", "neuromodulators_fep");
    return bridge;
}

void neuromod_fep_bridge_destroy(neuromod_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromodulators_fep");
    if (bridge->base.bio_async_enabled) neuromod_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int neuromod_fep_bridge_connect_fep(neuromod_fep_bridge_t* bridge, fep_system_t* fep) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_fep_bridge_connect_fep: bridge is NULL");
        return -1;
    }
    if (!fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_fep_bridge_connect_fep: fep is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int neuromod_fep_bridge_connect_neuromod(neuromod_fep_bridge_t* bridge, neuromodulator_system_t neuromod) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_fep_bridge_connect_neuromod: bridge is NULL");
        return -1;
    }
    if (!neuromod) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_fep_bridge_connect_neuromod: neuromod is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->neuromod_system = neuromod;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int neuromod_fep_bridge_disconnect(neuromod_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_fep_bridge_disconnect: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->neuromod_system = NULL;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float neuromod_fep_compute_da_from_pe(neuromod_fep_bridge_t* bridge, float pe) {
    if (!bridge || !bridge->config.enable_da_pe_coupling) return 0.0f;
    return pe * bridge->config.da_pe_sensitivity;
}

float neuromod_fep_compute_ach_from_precision(neuromod_fep_bridge_t* bridge, float precision) {
    if (!bridge || !bridge->config.enable_ach_precision_coupling) return 0.0f;
    return precision * bridge->config.ach_precision_sensitivity;
}

float neuromod_fep_compute_ne_from_uncertainty(neuromod_fep_bridge_t* bridge, float uncertainty) {
    if (!bridge || !bridge->config.enable_ne_uncertainty_coupling) return 0.0f;
    return uncertainty * bridge->config.ne_uncertainty_sensitivity;
}

float neuromod_fep_get_learning_rate_modulation(const neuromod_fep_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->neuromod_effects.learning_rate_modulation;
}

int neuromod_fep_bridge_update(neuromod_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_fep_bridge_update: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->fep_system && bridge->neuromod_system) {
        float pe = fep_get_prediction_error(bridge->fep_system, 0);
        bridge->fep_effects.pe_magnitude = fabsf(pe);
        bridge->fep_effects.da_release = neuromod_fep_compute_da_from_pe(bridge, pe);

        if (bridge->config.enable_da_pe_coupling && bridge->fep_effects.da_release != 0.0f) {
            neuromodulator_release_dopamine(bridge->neuromod_system, bridge->fep_effects.da_release, 0.0f);
            bridge->stats.da_releases++;
        }

        neuromodulator_pool_t pool = neuromodulator_pool_create();
        if (neuromodulator_get_levels(bridge->neuromod_system, &pool)) {
            float da = neuromodulator_pool_get_dopamine(&pool);
            float ach = neuromodulator_pool_get_acetylcholine(&pool);
            bridge->neuromod_effects.da_level = da;
            bridge->neuromod_effects.ach_level = ach;
            bridge->neuromod_effects.learning_rate_modulation = 1.0f + 0.5f * da;
        }
        neuromodulator_pool_destroy(&pool);

        bridge->stats.total_updates++;
        bridge->stats.avg_da_level = (bridge->stats.avg_da_level * (bridge->stats.total_updates - 1) + bridge->neuromod_effects.da_level) / bridge->stats.total_updates;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int neuromod_fep_bridge_get_stats(const neuromod_fep_bridge_t* bridge, neuromod_fep_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_fep_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_fep_bridge_get_stats: stats is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(neuromod_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int neuromod_fep_bridge_connect_bio_async(neuromod_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_fep_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_NEUROMODULATORS_BRIDGE,
        .module_name = "neuromod_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
    }
    return 0;
}

int neuromod_fep_bridge_disconnect_bio_async(neuromod_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_fep_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool neuromod_fep_bridge_is_bio_async_connected(const neuromod_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
