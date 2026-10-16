/**
 * @file nimcp_astrocytes_fep_bridge.c
 * @brief Implementation of astrocytes-FEP bridge
 */

#include "glial/astrocytes/nimcp_astrocytes_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(astrocytes_fep_bridge)

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int astrocytes_fep_default_config(astrocytes_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->precision_sensitivity = ASTROCYTES_FEP_DEFAULT_PRECISION_SENSITIVITY;
    config->glutamate_gain = ASTROCYTES_FEP_DEFAULT_GLUTAMATE_GAIN;
    config->calcium_pe_threshold = ASTROCYTES_FEP_CALCIUM_PE_THRESHOLD;
    config->d_serine_precision_factor = 0.4f;
    config->enable_calcium_prediction = true;
    config->enable_glutamate_precision = true;
    config->enable_prediction_errors = true;

    return 0;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

astrocytes_fep_bridge_t* astrocytes_fep_create(
    const astrocytes_fep_config_t* config,
    astrocyte_network_t* astrocyte_network,
    fep_system_t* fep_system)
{
    if (!config || !astrocyte_network || !fep_system) {
        NIMCP_LOGGING_ERROR("astrocytes_fep_create: NULL parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocytes_fep_create: required parameter is NULL (config, astrocyte_network, fep_system)");
        return NULL;
    }

    astrocytes_fep_bridge_t* bridge = (astrocytes_fep_bridge_t*)
        nimcp_malloc(sizeof(astrocytes_fep_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "astrocytes_fep_create: allocation failed");

    memset(bridge, 0, sizeof(astrocytes_fep_bridge_t));
    memcpy(&bridge->config, config, sizeof(astrocytes_fep_config_t));
    bridge->astrocyte_network = astrocyte_network;
    bridge->fep_system = fep_system;

    if (bridge_base_init(&bridge->base, 0, "astrocytes_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("astrocytes_fep_create: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "astrocytes_fep_create: bridge->base is NULL");
        return NULL;
    }

    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Astrocytes-FEP bridge created");
    return bridge;
}

void astrocytes_fep_destroy(astrocytes_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        astrocytes_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Astrocytes-FEP bridge destroyed");
}

/* ============================================================================
 * Update Functions
 * ============================================================================ */

int astrocytes_fep_update_fep_to_astrocyte(astrocytes_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");

    // Get current FEP free energy and precision
    float free_energy = fep_get_free_energy(bridge->fep_system);
    float prediction_error = fep_get_prediction_error(bridge->fep_system, 0);

    // Compute calcium threshold shift based on precision
    // High free energy (low precision) → lower threshold (easier calcium waves)
    bridge->fep_effects.calcium_threshold_shift =
        -bridge->config.precision_sensitivity * free_energy * 0.1f;

    // Prediction error modulates glutamate release
    bridge->fep_effects.glutamate_release_factor =
        1.0f + bridge->config.glutamate_gain * prediction_error;

    // Clamp to valid range
    if (bridge->fep_effects.glutamate_release_factor < 0.5f) {
        bridge->fep_effects.glutamate_release_factor = 0.5f;
    }
    if (bridge->fep_effects.glutamate_release_factor > 2.0f) {
        bridge->fep_effects.glutamate_release_factor = 2.0f;
    }

    // D-serine release (precision-gated NMDA modulation)
    bridge->fep_effects.d_serine_release_factor =
        1.0f + bridge->config.d_serine_precision_factor * (1.0f / (1.0f + free_energy));

    // ATP demand prediction (free energy as proxy for computational cost)
    bridge->fep_effects.atp_demand_prediction = free_energy / 10.0f;
    if (bridge->fep_effects.atp_demand_prediction > 1.0f) {
        bridge->fep_effects.atp_demand_prediction = 1.0f;
    }

    bridge->stats.total_updates++;
    return 0;
}

int astrocytes_fep_update_astrocyte_to_fep(astrocytes_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->astrocyte_network, NIMCP_ERROR_NULL_POINTER, "bridge or astrocyte_network is NULL");

    // Get astrocyte network statistics
    float avg_calcium = 0.0f;
    float max_calcium = 0.0f;
    float avg_glutamate = 0.0f;

    astrocyte_network_get_stats(bridge->astrocyte_network,
                                 &avg_calcium, &max_calcium, &avg_glutamate);

    // Glutamate modulates synaptic precision
    // High glutamate → increased precision (better signal quality)
    bridge->astro_effects.synaptic_precision_modulation =
        1.0f + bridge->config.glutamate_gain * avg_glutamate;

    // Calcium generates prediction error signal
    // High calcium indicates unexpected synaptic events
    if (avg_calcium > bridge->config.calcium_pe_threshold) {
        bridge->astro_effects.prediction_error_signal =
            (avg_calcium - bridge->config.calcium_pe_threshold) / 2.0f;
    } else {
        bridge->astro_effects.prediction_error_signal = 0.0f;
    }

    // Metabolic uncertainty (low ATP → high uncertainty)
    // Simplified: assume ATP correlated with low calcium
    bridge->astro_effects.metabolic_uncertainty =
        (max_calcium > 5.0f) ? 0.5f : 0.0f;

    bridge->stats.precision_modulations++;
    return 0;
}

int astrocytes_fep_update(astrocytes_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    int ret = astrocytes_fep_update_fep_to_astrocyte(bridge);
    if (ret != 0) {
        return ret;
    }

    ret = astrocytes_fep_update_astrocyte_to_fep(bridge);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

int astrocytes_fep_apply_modulation(astrocytes_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    // Note: Actual application would modify astrocyte network state
    // This is a simplified version showing the pattern
    // In full implementation, would iterate astrocytes and apply effects

    bridge->stats.calcium_predictions++;
    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

float astrocytes_fep_get_calcium_shift(const astrocytes_fep_bridge_t* bridge) {
    if (!bridge) {
        return 0.0f;
    }
    return bridge->fep_effects.calcium_threshold_shift;
}

float astrocytes_fep_get_precision_modulation(const astrocytes_fep_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;
    }
    return bridge->astro_effects.synaptic_precision_modulation;
}

float astrocytes_fep_get_prediction_error(const astrocytes_fep_bridge_t* bridge) {
    if (!bridge) {
        return 0.0f;
    }
    return bridge->astro_effects.prediction_error_signal;
}

int astrocytes_fep_get_stats(
    const astrocytes_fep_bridge_t* bridge,
    astrocytes_fep_stats_t* stats)
{
    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    memcpy(stats, &bridge->stats, sizeof(astrocytes_fep_stats_t));
    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int astrocytes_fep_connect_bio_async(astrocytes_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0; // Already connected
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_ASTROCYTE,
        .module_name = "astrocytes_fep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Astrocytes-FEP bridge connected to bio-async");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available for astrocytes-FEP bridge");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astrocytes_fep_connect_bio_async: validation failed");
    return -1;
}

int astrocytes_fep_disconnect_bio_async(astrocytes_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Astrocytes-FEP bridge disconnected from bio-async");
    return 0;
}

bool astrocytes_fep_is_bio_async_connected(const astrocytes_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
