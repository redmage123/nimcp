/**
 * @file nimcp_myelin_sheath_fep_bridge.c
 * @brief Implementation of myelin_sheath-FEP bridge
 */

#include "glial/myelin_sheath/nimcp_myelin_sheath_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(myelin_sheath_fep_bridge)

int myelin_sheath_fep_default_config(myelin_sheath_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->integrity_sensitivity = MYELIN_SHEATH_FEP_DEFAULT_INTEGRITY_SENSITIVITY;
    config->g_ratio_gain = MYELIN_SHEATH_FEP_DEFAULT_G_RATIO_GAIN;
    config->velocity_precision_factor = 0.85f;
    config->enable_integrity_precision = true;
    config->enable_g_ratio_optimization = true;
    config->enable_conduction_block_modeling = true;
    return 0;
}

myelin_sheath_fep_bridge_t* myelin_sheath_fep_create(
    const myelin_sheath_fep_config_t* config,
    myelin_sheath_network_t* myelin_network,
    fep_system_t* fep_system)
{
    if (!config || !myelin_network || !fep_system) {
        NIMCP_LOGGING_ERROR("myelin_sheath_fep_create: NULL parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "myelin_sheath_fep_create: required parameter is NULL (config, myelin_network, fep_system)");
        return NULL;
    }

    myelin_sheath_fep_bridge_t* bridge = (myelin_sheath_fep_bridge_t*)
        nimcp_malloc(sizeof(myelin_sheath_fep_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "myelin_sheath_fep_create: allocation failed");

    memset(bridge, 0, sizeof(myelin_sheath_fep_bridge_t));
    memcpy(&bridge->config, config, sizeof(myelin_sheath_fep_config_t));
    bridge->myelin_network = myelin_network;
    bridge->fep_system = fep_system;

    if (bridge_base_init(&bridge->base, 0, "myelin_sheath_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("myelin_sheath_fep_create: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "myelin_sheath_fep_create: bridge->base is NULL");
        return NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Myelin sheath-FEP bridge created");
    return bridge;
}

void myelin_sheath_fep_destroy(myelin_sheath_fep_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) {
        myelin_sheath_fep_disconnect_bio_async(bridge);
    }
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Myelin sheath-FEP bridge destroyed");
}

int myelin_sheath_fep_update_fep_to_myelin(myelin_sheath_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");

    float free_energy = fep_get_free_energy(bridge->fep_system);
    float precision_estimate = 1.0f / (1.0f + free_energy);

    // High precision → optimize g-ratio
    bridge->fep_effects.target_g_ratio_shift =
        bridge->config.g_ratio_gain * (precision_estimate - 0.7f);

    // Lamellae count modulation
    bridge->fep_effects.lamellae_count_modulation =
        1.0f + bridge->config.integrity_sensitivity * precision_estimate;

    // Repair rate based on prediction error
    float prediction_error = fep_get_prediction_error(bridge->fep_system, 0);
    bridge->fep_effects.repair_rate_modulation =
        1.0f + 0.5f * prediction_error;

    // Compaction target
    bridge->fep_effects.compaction_target = precision_estimate;

    bridge->stats.total_updates++;
    return 0;
}

int myelin_sheath_fep_update_myelin_to_fep(myelin_sheath_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->myelin_network, NIMCP_ERROR_NULL_POINTER, "bridge or myelin_network is NULL");

    myelin_network_stats_t stats;
    myelin_network_get_stats(bridge->myelin_network, &stats);

    // Velocity precision contribution
    bridge->myelin_effects.velocity_precision_contribution =
        stats.mean_velocity_ratio * bridge->config.velocity_precision_factor;

    // Integrity affects uncertainty
    bridge->myelin_effects.integrity_uncertainty =
        1.0f - stats.mean_integrity;

    // Conduction reliability
    bridge->myelin_effects.conduction_reliability =
        (float)stats.healthy_sheaths / (float)(stats.total_sheaths + 1);

    // Metabolic efficiency
    bridge->myelin_effects.metabolic_efficiency = stats.mean_g_ratio;

    return 0;
}

int myelin_sheath_fep_update(myelin_sheath_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    int ret = myelin_sheath_fep_update_fep_to_myelin(bridge);
    if (ret != 0) return ret;
    return myelin_sheath_fep_update_myelin_to_fep(bridge);
}

int myelin_sheath_fep_apply_modulation(myelin_sheath_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    bridge->stats.g_ratio_optimizations++;
    return 0;
}

float myelin_sheath_fep_get_g_ratio_shift(const myelin_sheath_fep_bridge_t* bridge) {
    return bridge ? bridge->fep_effects.target_g_ratio_shift : 0.0f;
}

float myelin_sheath_fep_get_velocity_precision(const myelin_sheath_fep_bridge_t* bridge) {
    return bridge ? bridge->myelin_effects.velocity_precision_contribution : 0.0f;
}

int myelin_sheath_fep_get_stats(const myelin_sheath_fep_bridge_t* bridge,
                                myelin_sheath_fep_stats_t* stats)
{
    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    memcpy(stats, &bridge->stats, sizeof(myelin_sheath_fep_stats_t));
    return 0;
}

int myelin_sheath_fep_connect_bio_async(myelin_sheath_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_MYELIN,
        .module_name = "myelin_sheath_fep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Myelin sheath-FEP bridge connected to bio-async");
        return 0;
    }
    NIMCP_LOGGING_WARN("Bio-async router not available for myelin sheath-FEP bridge");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "myelin_sheath_fep_connect_bio_async: validation failed");
    return -1;
}

int myelin_sheath_fep_disconnect_bio_async(myelin_sheath_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Myelin sheath-FEP bridge disconnected from bio-async");
    return 0;
}

bool myelin_sheath_fep_is_bio_async_connected(const myelin_sheath_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
