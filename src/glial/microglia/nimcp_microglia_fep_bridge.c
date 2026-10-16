/**
 * @file nimcp_microglia_fep_bridge.c
 * @brief Implementation of microglia-FEP bridge
 */

#include "glial/microglia/nimcp_microglia_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(microglia_fep_bridge)

int microglia_fep_default_config(microglia_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->precision_threshold = MICROGLIA_FEP_DEFAULT_PRECISION_THRESHOLD;
    config->pe_pruning_gain = MICROGLIA_FEP_DEFAULT_PE_PRUNING_GAIN;
    config->cytokine_precision_impact = MICROGLIA_FEP_CYTOKINE_PRECISION_IMPACT;
    config->complement_precision_factor = 0.6f;
    config->enable_precision_pruning = true;
    config->enable_pe_modulation = true;
    config->enable_cytokine_feedback = true;
    return 0;
}

microglia_fep_bridge_t* microglia_fep_create(
    const microglia_fep_config_t* config,
    microglia_network_t* microglia_network,
    fep_system_t* fep_system)
{
    if (!config || !microglia_network || !fep_system) {
        NIMCP_LOGGING_ERROR("microglia_fep_create: NULL parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "microglia_fep_create: required parameter is NULL (config, microglia_network, fep_system)");
        return NULL;
    }

    microglia_fep_bridge_t* bridge = (microglia_fep_bridge_t*)
        nimcp_malloc(sizeof(microglia_fep_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "microglia_fep_create: allocation failed");

    memset(bridge, 0, sizeof(microglia_fep_bridge_t));
    memcpy(&bridge->config, config, sizeof(microglia_fep_config_t));
    bridge->microglia_network = microglia_network;
    bridge->fep_system = fep_system;

    if (bridge_base_init(&bridge->base, 0, "microglia_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("microglia_fep_create: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "microglia_fep_create: bridge->base is NULL");
        return NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Microglia-FEP bridge created");
    return bridge;
}

void microglia_fep_destroy(microglia_fep_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) {
        microglia_fep_disconnect_bio_async(bridge);
    }
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Microglia-FEP bridge destroyed");
}

/* Internal helper: update FEP to microglia effects (caller must hold lock) */
static int microglia_fep_update_fep_to_microglia_locked(microglia_fep_bridge_t* bridge) {
    float free_energy = fep_get_free_energy(bridge->fep_system);
    float prediction_error = fep_get_prediction_error(bridge->fep_system, 0);

    // Low precision (high FE) → increase pruning threshold
    bridge->fep_effects.pruning_threshold_shift =
        bridge->config.precision_threshold * (free_energy / 10.0f);

    // Prediction error modulates surveillance
    bridge->fep_effects.surveillance_intensity =
        fminf(1.0f, bridge->config.pe_pruning_gain * prediction_error);

    // Complement tagging strength inversely related to precision
    float precision_estimate = 1.0f / (1.0f + free_energy);
    bridge->fep_effects.complement_tag_strength =
        bridge->config.complement_precision_factor * (1.0f - precision_estimate);

    // Cytokine release when prediction errors high
    bridge->fep_effects.cytokine_release_factor =
        (prediction_error > 1.0f) ? (1.0f + prediction_error * 0.3f) : 1.0f;

    bridge->stats.total_updates++;
    return 0;
}

/* Internal helper: update microglia to FEP effects (caller must hold lock) */
static int microglia_fep_update_microglia_to_fep_locked(microglia_fep_bridge_t* bridge) {
    microglia_network_stats_t stats;
    microglia_network_get_stats(bridge->microglia_network, &stats);

    // Pruning increases structural uncertainty
    // Guard against division by zero with max(1, monitored_synapses)
    bridge->micro_effects.synapses_pruned = stats.total_pruned;
    uint32_t denominator = (stats.total_monitored_synapses > 0) ? stats.total_monitored_synapses : 1;
    bridge->micro_effects.structural_uncertainty =
        (float)stats.total_pruned / (float)denominator;

    // Cytokines modulate network precision
    float inflammation = stats.total_pro_inflammatory - stats.total_anti_inflammatory;
    bridge->micro_effects.cytokine_precision_modulation =
        -bridge->config.cytokine_precision_impact * inflammation;

    // Overall precision shift
    bridge->micro_effects.network_precision_shift =
        bridge->micro_effects.cytokine_precision_modulation -
        bridge->micro_effects.structural_uncertainty * 0.5f;

    return 0;
}

int microglia_fep_update_fep_to_microglia(microglia_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    int ret = microglia_fep_update_fep_to_microglia_locked(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);
    return ret;
}

int microglia_fep_update_microglia_to_fep(microglia_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->microglia_network, NIMCP_ERROR_NULL_POINTER, "bridge or microglia_network is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    int ret = microglia_fep_update_microglia_to_fep_locked(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);
    return ret;
}

int microglia_fep_update(microglia_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system && bridge->microglia_network, NIMCP_ERROR_NULL_POINTER, "fep_system or microglia_network is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    int ret = microglia_fep_update_fep_to_microglia_locked(bridge);
    if (ret == 0) {
        ret = microglia_fep_update_microglia_to_fep_locked(bridge);
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return ret;
}

int microglia_fep_apply_modulation(microglia_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    bridge->stats.precision_guided_prunings++;
    return 0;
}

float microglia_fep_get_pruning_threshold(const microglia_fep_bridge_t* bridge) {
    return bridge ? (bridge->config.precision_threshold +
                     bridge->fep_effects.pruning_threshold_shift) : 0.0f;
}

float microglia_fep_get_precision_modulation(const microglia_fep_bridge_t* bridge) {
    return bridge ? bridge->micro_effects.network_precision_shift : 0.0f;
}

uint32_t microglia_fep_get_synapses_pruned(const microglia_fep_bridge_t* bridge) {
    return bridge ? bridge->micro_effects.synapses_pruned : 0;
}

int microglia_fep_get_stats(const microglia_fep_bridge_t* bridge,
                             microglia_fep_stats_t* stats)
{
    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    memcpy(stats, &bridge->stats, sizeof(microglia_fep_stats_t));
    return 0;
}

int microglia_fep_connect_bio_async(microglia_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_MICROGLIA,
        .module_name = "microglia_fep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Microglia-FEP bridge connected to bio-async");
        return 0;
    }
    NIMCP_LOGGING_WARN("Bio-async router not available for microglia-FEP bridge");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "microglia_fep_connect_bio_async: validation failed");
    return -1;
}

int microglia_fep_disconnect_bio_async(microglia_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Microglia-FEP bridge disconnected from bio-async");
    return 0;
}

bool microglia_fep_is_bio_async_connected(const microglia_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
