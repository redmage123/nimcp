/**
 * @file nimcp_swarm_consciousness_fep_bridge.c
 * @brief FEP Bridge Implementation for Swarm Collective Consciousness
 */

#include "swarm/nimcp_swarm_consciousness_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_consciousness_fep_bridge)

void swarm_consciousness_fep_default_config(swarm_consciousness_fep_config_t* config) {
    if (!config) return;
    config->phi_fe_coupling = 0.8f;
    config->integration_precision_gain = 1.3f;
    config->consciousness_lr_boost = 1.1f;
    config->enable_phi_tracking = true;
    config->enable_emergence_detection = true;
}

swarm_consciousness_fep_bridge_t* swarm_consciousness_fep_create(
    const swarm_consciousness_fep_config_t* config,
    swarm_consciousness_ctx_t* consciousness_ctx,
    fep_system_t* fep_system)
{
    if (!consciousness_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_fep_create: consciousness_ctx is NULL");
        return NULL;
    }
    if (!fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_fep_create: fep_system is NULL");
        return NULL;
    }

    swarm_consciousness_fep_bridge_t* bridge = (swarm_consciousness_fep_bridge_t*)nimcp_malloc(sizeof(swarm_consciousness_fep_bridge_t));
    if (!bridge) return NULL;

    memset(bridge, 0, sizeof(swarm_consciousness_fep_bridge_t));

    if (config) bridge->config = *config;
    else swarm_consciousness_fep_default_config(&bridge->config);

    bridge->fep_system = fep_system;
    bridge->consciousness_ctx = consciousness_ctx;
    bridge->base.bio_async_enabled = false;

    if (bridge_base_init(&bridge->base, 0, "swarm_consciousness_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_fep_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Swarm consciousness FEP bridge created");
    return bridge;
}

void swarm_consciousness_fep_destroy(swarm_consciousness_fep_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) swarm_consciousness_fep_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int swarm_consciousness_fep_update(swarm_consciousness_fep_bridge_t* bridge) {
    if (!bridge) return -1;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Get FEP state
    float free_energy = fep_get_free_energy(bridge->fep_system);

    // Derive consciousness metrics from FEP state (inverse relationship)
    // High FE = low phi (fragmented), Low FE = high phi (integrated)
    float phi = fmaxf(0.0f, 1.0f - free_energy * 0.2f);

    // Classify consciousness state based on derived phi
    swarm_consciousness_state_t cons_state;
    if (phi < 0.2f) {
        cons_state = SWARM_CONSCIOUSNESS_DORMANT;
    } else if (phi < 0.5f) {
        cons_state = SWARM_CONSCIOUSNESS_EMERGING;
    } else if (phi < 0.8f) {
        cons_state = SWARM_CONSCIOUSNESS_UNIFIED;
    } else {
        cons_state = SWARM_CONSCIOUSNESS_TRANSCENDENT;
    }

    // FEP effects on consciousness: high FE → reduce phi
    bridge->fep_effects.phi_modulation = -tanhf(free_energy * 0.3f);
    bridge->fep_effects.integration_boost = fmaxf(0.0f, 1.0f - free_energy * 0.2f);
    bridge->fep_effects.coherence_adjustment = -free_energy * 0.1f;
    bridge->fep_effects.consciousness_bias = expf(-free_energy);

    // Consciousness effects on FEP: high phi → high precision
    bridge->consciousness_effects.precision_from_phi = 0.5f + phi * 1.5f;
    bridge->consciousness_effects.learning_rate_from_consciousness = 0.8f + (cons_state == SWARM_CONSCIOUSNESS_UNIFIED ? 0.4f : 0.0f);
    bridge->consciousness_effects.integration_weight = phi;
    bridge->consciousness_effects.consciousness_prior = cons_state;

    bridge->state.last_collective_phi = phi;
    bridge->state.last_free_energy = free_energy;
    if (bridge->state.last_consciousness_state != cons_state) {
        bridge->state.consciousness_transitions++;
        bridge->state.last_consciousness_state = cons_state;
        bridge->stats.emergence_events++;
    }

    bridge->stats.total_updates++;
    bridge->stats.avg_phi = (bridge->stats.avg_phi * (bridge->stats.total_updates - 1) + phi) / bridge->stats.total_updates;
    bridge->stats.avg_free_energy = (bridge->stats.avg_free_energy * (bridge->stats.total_updates - 1) + free_energy) / bridge->stats.total_updates;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_consciousness_fep_apply_modulation(swarm_consciousness_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    return 0;
}

int swarm_consciousness_fep_get_effects(const swarm_consciousness_fep_bridge_t* bridge, swarm_consciousness_fep_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->fep_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_consciousness_fep_get_consciousness_effects(const swarm_consciousness_fep_bridge_t* bridge, fep_swarm_consciousness_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->consciousness_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_consciousness_fep_get_stats(const swarm_consciousness_fep_bridge_t* bridge, swarm_consciousness_fep_stats_t* stats) {
    if (!bridge || !stats) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_consciousness_fep_connect_bio_async(swarm_consciousness_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_SWARM_CONSCIOUSNESS,
        .module_name = "swarm_consciousness_fep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int swarm_consciousness_fep_disconnect_bio_async(swarm_consciousness_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool swarm_consciousness_fep_is_bio_async_connected(const swarm_consciousness_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
