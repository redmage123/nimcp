/**
 * @file nimcp_oligodendrocytes_fep_bridge.c
 * @brief Implementation of oligodendrocytes-FEP bridge
 */

#include "glial/oligodendrocytes/nimcp_oligodendrocytes_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(oligodendrocytes_fep_bridge)

int oligodendrocytes_fep_default_config(oligodendrocytes_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->temporal_sensitivity = OLIGODENDROCYTES_FEP_DEFAULT_TEMPORAL_SENSITIVITY;
    config->myelination_gain = OLIGODENDROCYTES_FEP_DEFAULT_MYELINATION_GAIN;
    config->g_ratio_precision_factor = 0.8f;
    config->enable_temporal_precision = true;
    config->enable_activity_myelination = true;
    config->enable_metabolic_efficiency = true;
    return 0;
}

oligodendrocytes_fep_bridge_t* oligodendrocytes_fep_create(
    const oligodendrocytes_fep_config_t* config,
    oligodendrocyte_network_t* oligodendrocyte_network,
    fep_system_t* fep_system)
{
    if (!config || !oligodendrocyte_network || !fep_system) {
        NIMCP_LOGGING_ERROR("oligodendrocytes_fep_create: NULL parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oligodendrocytes_fep_create: required parameter is NULL (config, oligodendrocyte_network, fep_system)");
        return NULL;
    }

    oligodendrocytes_fep_bridge_t* bridge = (oligodendrocytes_fep_bridge_t*)
        nimcp_malloc(sizeof(oligodendrocytes_fep_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "oligodendrocytes_fep_create: allocation failed");

    memset(bridge, 0, sizeof(oligodendrocytes_fep_bridge_t));
    memcpy(&bridge->config, config, sizeof(oligodendrocytes_fep_config_t));
    bridge->oligodendrocyte_network = oligodendrocyte_network;
    bridge->fep_system = fep_system;

    if (bridge_base_init(&bridge->base, 0, "oligodendrocytes_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("oligodendrocytes_fep_create: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "oligodendrocytes_fep_create: bridge->base is NULL");
        return NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Oligodendrocytes-FEP bridge created");
    return bridge;
}

void oligodendrocytes_fep_destroy(oligodendrocytes_fep_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) {
        oligodendrocytes_fep_disconnect_bio_async(bridge);
    }
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Oligodendrocytes-FEP bridge destroyed");
}

int oligodendrocytes_fep_update_fep_to_oligodendrocyte(oligodendrocytes_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");

    float free_energy = fep_get_free_energy(bridge->fep_system);

    // High precision (low FE) → increase myelination
    float precision_estimate = 1.0f / (1.0f + free_energy);
    bridge->fep_effects.myelination_rate_modulation =
        1.0f + bridge->config.myelination_gain * precision_estimate;

    // Temporal precision requirements
    bridge->fep_effects.temporal_precision_requirement =
        bridge->config.temporal_sensitivity * precision_estimate;

    // G-ratio optimization target
    bridge->fep_effects.g_ratio_target_shift =
        (precision_estimate - 0.5f) * 0.1f; // Shift toward optimal

    // Metabolic cost prediction
    bridge->fep_effects.metabolic_cost_prediction = free_energy / 5.0f;

    bridge->stats.total_updates++;
    return 0;
}

int oligodendrocytes_fep_update_oligodendrocyte_to_fep(oligodendrocytes_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->oligodendrocyte_network, NIMCP_ERROR_NULL_POINTER, "bridge or oligodendrocyte_network is NULL");

    oligodendrocyte_network_stats_t stats;
    oligodendrocyte_network_get_stats(bridge->oligodendrocyte_network, &stats);

    // Myelination improves temporal precision
    bridge->oligo_effects.myelination_level = stats.avg_myelination_level;
    bridge->oligo_effects.temporal_precision_contribution =
        stats.avg_conduction_velocity / 100.0f; // Normalize velocity

    // Conduction delay affects FEP timing
    bridge->oligo_effects.conduction_delay_factor =
        1.0f / (1.0f + stats.avg_conduction_velocity / 10.0f);

    // Energy efficiency
    bridge->oligo_effects.energy_efficiency_ratio =
        stats.network_myelination_efficiency;

    return 0;
}

int oligodendrocytes_fep_update(oligodendrocytes_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    int ret = oligodendrocytes_fep_update_fep_to_oligodendrocyte(bridge);
    if (ret != 0) return ret;
    return oligodendrocytes_fep_update_oligodendrocyte_to_fep(bridge);
}

int oligodendrocytes_fep_apply_modulation(oligodendrocytes_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    bridge->stats.temporal_precision_adjustments++;
    return 0;
}

float oligodendrocytes_fep_get_myelination_modulation(const oligodendrocytes_fep_bridge_t* bridge) {
    return bridge ? bridge->fep_effects.myelination_rate_modulation : 1.0f;
}

float oligodendrocytes_fep_get_temporal_precision(const oligodendrocytes_fep_bridge_t* bridge) {
    return bridge ? bridge->oligo_effects.temporal_precision_contribution : 0.0f;
}

int oligodendrocytes_fep_get_stats(const oligodendrocytes_fep_bridge_t* bridge,
                                   oligodendrocytes_fep_stats_t* stats)
{
    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    memcpy(stats, &bridge->stats, sizeof(oligodendrocytes_fep_stats_t));
    return 0;
}

int oligodendrocytes_fep_connect_bio_async(oligodendrocytes_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_OLIGODENDROCYTE,
        .module_name = "oligodendrocytes_fep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Oligodendrocytes-FEP bridge connected to bio-async");
        return 0;
    }
    NIMCP_LOGGING_WARN("Bio-async router not available for oligodendrocytes-FEP bridge");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "oligodendrocytes_fep_connect_bio_async: validation failed");
    return -1;
}

int oligodendrocytes_fep_disconnect_bio_async(oligodendrocytes_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Oligodendrocytes-FEP bridge disconnected from bio-async");
    return 0;
}

bool oligodendrocytes_fep_is_bio_async_connected(const oligodendrocytes_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
