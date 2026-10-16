/**
 * @file nimcp_glial_integration_fep_bridge.c
 * @brief Implementation of glial_integration-FEP bridge
 */

#include "glial/integration/nimcp_glial_integration_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(glial_integration_fep_bridge)

int glial_integration_fep_default_config(glial_integration_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->metabolic_gain = GLIAL_INTEGRATION_FEP_DEFAULT_METABOLIC_GAIN;
    config->homeostasis_gain = GLIAL_INTEGRATION_FEP_DEFAULT_HOMEOSTASIS_GAIN;
    config->network_precision_factor = 0.75f;
    config->enable_metabolic_prediction = true;
    config->enable_homeostatic_precision = true;
    config->enable_coordinated_response = true;
    return 0;
}

glial_integration_fep_bridge_t* glial_integration_fep_create(
    const glial_integration_fep_config_t* config,
    glial_integration_t* glial_integration,
    fep_system_t* fep_system)
{
    if (!config || !glial_integration || !fep_system) {
        NIMCP_LOGGING_ERROR("glial_integration_fep_create: NULL parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "glial_integration_fep_create: required parameter is NULL (config, glial_integration, fep_system)");
        return NULL;
    }

    glial_integration_fep_bridge_t* bridge = (glial_integration_fep_bridge_t*)
        nimcp_malloc(sizeof(glial_integration_fep_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "glial_integration_fep_create: allocation failed");

    memset(bridge, 0, sizeof(glial_integration_fep_bridge_t));
    memcpy(&bridge->config, config, sizeof(glial_integration_fep_config_t));
    bridge->glial_integration = glial_integration;
    bridge->fep_system = fep_system;

    if (bridge_base_init(&bridge->base, 0, "glial_integration_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("glial_integration_fep_create: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "glial_integration_fep_create: bridge->base is NULL");
        return NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Glial integration-FEP bridge created");
    return bridge;
}

void glial_integration_fep_destroy(glial_integration_fep_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) {
        glial_integration_fep_disconnect_bio_async(bridge);
    }
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Glial integration-FEP bridge destroyed");
}

int glial_integration_fep_update_fep_to_glial(glial_integration_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");

    float free_energy = fep_get_free_energy(bridge->fep_system);

    // Metabolic demand prediction based on free energy
    bridge->fep_effects.metabolic_demand_prediction =
        bridge->config.metabolic_gain * free_energy / 10.0f;

    // Network stability requirement
    float precision_estimate = 1.0f / (1.0f + free_energy);
    bridge->fep_effects.network_stability_requirement =
        bridge->config.homeostasis_gain * (1.0f - precision_estimate);

    // Precision distribution pattern
    bridge->fep_effects.precision_distribution_pattern = precision_estimate;

    // Coordinated support level
    bridge->fep_effects.coordinated_support_level =
        (free_energy > 5.0f) ? 1.0f : (free_energy / 5.0f);

    bridge->stats.total_updates++;
    return 0;
}

int glial_integration_fep_update_glial_to_fep(glial_integration_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->glial_integration, NIMCP_ERROR_NULL_POINTER, "bridge or glial_integration is NULL");

    glial_integration_stats_t stats;
    glial_integration_get_stats(bridge->glial_integration, &stats);

    // Metabolic capacity
    bridge->glial_effects.metabolic_capacity =
        (float)stats.num_astrocytes / 100.0f; // Normalized

    // Network homeostasis level
    bridge->glial_effects.network_homeostasis_level =
        stats.avg_synaptic_modulation;

    // Overall precision support
    bridge->glial_effects.overall_precision_support =
        bridge->config.network_precision_factor *
        (stats.avg_synaptic_modulation + stats.avg_myelination_factor) / 2.0f;

    // Glial prediction capacity
    bridge->glial_effects.glial_prediction_capacity =
        (float)(stats.num_astrocytes + stats.num_oligodendrocytes) / 200.0f;

    return 0;
}

int glial_integration_fep_update(glial_integration_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    int ret = glial_integration_fep_update_fep_to_glial(bridge);
    if (ret != 0) return ret;
    return glial_integration_fep_update_glial_to_fep(bridge);
}

int glial_integration_fep_apply_modulation(glial_integration_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    bridge->stats.homeostatic_adjustments++;
    return 0;
}

float glial_integration_fep_get_metabolic_prediction(const glial_integration_fep_bridge_t* bridge) {
    return bridge ? bridge->fep_effects.metabolic_demand_prediction : 0.0f;
}

float glial_integration_fep_get_precision_support(const glial_integration_fep_bridge_t* bridge) {
    return bridge ? bridge->glial_effects.overall_precision_support : 0.0f;
}

int glial_integration_fep_get_stats(const glial_integration_fep_bridge_t* bridge,
                                    glial_integration_fep_stats_t* stats)
{
    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    memcpy(stats, &bridge->stats, sizeof(glial_integration_fep_stats_t));
    return 0;
}

int glial_integration_fep_connect_bio_async(glial_integration_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_GLIAL_INTEGRATION,
        .module_name = "glial_integration_fep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Glial integration-FEP bridge connected to bio-async");
        return 0;
    }
    NIMCP_LOGGING_WARN("Bio-async router not available for glial integration-FEP bridge");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "glial_integration_fep_connect_bio_async: validation failed");
    return -1;
}

int glial_integration_fep_disconnect_bio_async(glial_integration_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Glial integration-FEP bridge disconnected from bio-async");
    return 0;
}

bool glial_integration_fep_is_bio_async_connected(const glial_integration_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
