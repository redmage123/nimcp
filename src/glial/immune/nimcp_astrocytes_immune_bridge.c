/**
 * @file nimcp_astrocytes_immune_bridge.c
 * @brief Single-Cell Astrocyte-Immune Bridge Implementation
 * @version 2.0.0
 * @date 2025-12-27
 *
 * WHAT: Bridge for individual astrocyte phenotype switching (A1/A2)
 * WHY:  Single-cell level immune-astrocyte interaction
 * HOW:  Works with nimcp_astrocyte_t (individual cells)
 *
 * NOTE: This module uses "astro_cell_*" prefix to avoid conflicts
 * with the polymorphic base class (astro_immune_*)
 */

#include "glial/immune/nimcp_astrocytes_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(astrocytes_immune_bridge)

/* ============================================================================
 * KG-Driven Wiring Infrastructure
 * ============================================================================ */

/**
 * Handler map for astrocyte-immune bridge module.
 * Currently empty - handlers to be added as module evolves.
 * This infrastructure enables future KG-driven wiring.
 */
DEFINE_HANDLER_MAP_BEGIN(astro_cell_immune)
    /* Future handlers will be added here as needed */
DEFINE_HANDLER_MAP_END()

/**
 * Wiring callback for KG-driven handler registration.
 */
DEFINE_HANDLER_CALLBACK(astro_cell_immune, astro_immune_bridge_t, bridge)

int astro_cell_default_config(astro_immune_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astro_cell_default_config: config is NULL");
        return -1;
    }
    memset(config, 0, sizeof(*config));
    config->il1_a1_induction = 0.5f;
    config->tnf_a1_induction = 0.4f;
    config->il6_reactivity_gain = 0.3f;
    config->il10_a2_promotion = 0.6f;
    config->scar_formation_threshold = 0.8f;
    config->glutamate_clearance_base = 1.0f;
    config->enable_bio_async = true;
    config->inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL;
    return 0;
}

astro_immune_bridge_t* astro_cell_create(
    const astro_immune_config_t* config,
    nimcp_astrocyte_t* astrocyte,
    brain_immune_system_t* immune_system)
{
    astro_immune_bridge_t* bridge = nimcp_malloc(sizeof(astro_immune_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }
    memset(bridge, 0, sizeof(*bridge));

    if (config) bridge->config = *config;
    else astro_cell_default_config(&bridge->config);

    bridge->astrocyte = astrocyte;
    bridge->immune_system = immune_system;
    bridge->reactivity_state = ASTRO_QUIESCENT;
    bridge->glutamate_clearance_rate = bridge->config.glutamate_clearance_base;

    if (bridge_base_init(&bridge->base, 0, "astrocytes_immune") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->initialized = true;
    NIMCP_LOGGING_INFO("Created single-cell astro-immune bridge");
    return bridge;
}

void astro_cell_destroy(astro_immune_bridge_t* bridge)
{
    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int astro_cell_connect_bio_async(astro_immune_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_ASTROCYTE,
        .module_name = ASTRO_IMMUNE_MODULE_NAME,
        .inbox_capacity = bridge->config.inbox_capacity,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int astro_cell_disconnect_bio_async(astro_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) { bio_router_unregister_module(bridge->base.bio_ctx); bridge->base.bio_ctx = NULL; }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool astro_cell_is_bio_async_connected(const astro_immune_bridge_t* bridge)
{
    return bridge && bridge->base.bio_async_enabled;
}

int astro_cell_update_cytokine_effects(astro_immune_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->base.mutex, NIMCP_ERROR_NULL_POINTER, "bridge mutex is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    float il1 = 0, tnf = 0, il6 = 0, il10 = 0;
    if (bridge->immune_system) {
        il1 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL1);
        tnf = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_TNF);
        il6 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL6);
        il10 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL10);
    }

    bridge->cytokine_effects.il1_effect = il1 * bridge->config.il1_a1_induction;
    bridge->cytokine_effects.tnf_effect = tnf * bridge->config.tnf_a1_induction;
    bridge->cytokine_effects.il6_effect = il6 * bridge->config.il6_reactivity_gain;
    bridge->cytokine_effects.il10_effect = il10 * bridge->config.il10_a2_promotion;

    bridge->cytokine_effects.a1_drive =
        bridge->cytokine_effects.il1_effect +
        bridge->cytokine_effects.tnf_effect +
        bridge->cytokine_effects.il6_effect * 0.5f;

    bridge->cytokine_effects.a2_drive = bridge->cytokine_effects.il10_effect;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int astro_cell_update_reactivity(astro_immune_bridge_t* bridge, float dt_ms)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(dt_ms > 0.0f && isfinite(dt_ms), NIMCP_ERROR_INVALID_PARAM, "dt_ms must be positive and finite");
    NIMCP_CHECK_THROW(bridge->base.mutex, NIMCP_ERROR_NULL_POINTER, "bridge mutex is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    astrocyte_reactivity_t old_state = bridge->reactivity_state;

    float a1 = bridge->cytokine_effects.a1_drive;
    float a2 = bridge->cytokine_effects.a2_drive;

    if (a1 > 0.5f && a1 > a2) {
        bridge->reactivity_state = ASTRO_A1_REACTIVE;
        bridge->reactivity_level = a1;
        bridge->glutamate_clearance_rate = bridge->config.glutamate_clearance_base * (1.0f - a1 * 0.5f);
    } else if (a2 > 0.3f) {
        bridge->reactivity_state = ASTRO_A2_REACTIVE;
        bridge->reactivity_level = a2;
        bridge->gliotransmitter_release = a2 * 0.5f;
        bridge->glutamate_clearance_rate = bridge->config.glutamate_clearance_base;
    } else {
        bridge->reactivity_state = ASTRO_QUIESCENT;
        bridge->reactivity_level = 0.0f;
        bridge->glutamate_clearance_rate = bridge->config.glutamate_clearance_base;
    }

    if (bridge->reactivity_state == ASTRO_A1_REACTIVE) {
        bridge->scar_formation_progress += 0.001f * (dt_ms / 1000.0f);
        if (bridge->scar_formation_progress >= bridge->config.scar_formation_threshold) {
            bridge->reactivity_state = ASTRO_SCAR_FORMING;
            bridge->stats.scar_formations++;
        }
    }

    if (old_state != bridge->reactivity_state) {
        bridge->stats.reactivity_changes++;
        if (bridge->reactivity_state == ASTRO_A1_REACTIVE) bridge->stats.a1_activations++;
        if (bridge->reactivity_state == ASTRO_A2_REACTIVE) bridge->stats.a2_activations++;
    }

    bridge->stats.glutamate_clearance_rate = bridge->glutamate_clearance_rate;
    bridge->stats.gliotransmitter_release = bridge->gliotransmitter_release;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int astro_cell_update(astro_immune_bridge_t* bridge, float dt_ms)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->initialized, NIMCP_ERROR_INVALID_PARAM, "bridge not initialized");
    NIMCP_CHECK_THROW(dt_ms > 0.0f && isfinite(dt_ms), NIMCP_ERROR_INVALID_PARAM, "dt_ms must be positive and finite");

    astro_cell_update_cytokine_effects(bridge);
    astro_cell_update_reactivity(bridge, dt_ms);
    return 0;
}

astrocyte_reactivity_t astro_cell_get_reactivity(const astro_immune_bridge_t* bridge)
{
    if (!bridge) return ASTRO_QUIESCENT;
    if (!bridge->base.mutex) return bridge->reactivity_state;

    nimcp_mutex_lock(((astro_immune_bridge_t*)bridge)->base.mutex);
    astrocyte_reactivity_t state = bridge->reactivity_state;
    nimcp_mutex_unlock(((astro_immune_bridge_t*)bridge)->base.mutex);
    return state;
}

float astro_cell_get_glutamate_clearance(const astro_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;
    if (!bridge->base.mutex) return bridge->glutamate_clearance_rate;

    nimcp_mutex_lock(((astro_immune_bridge_t*)bridge)->base.mutex);
    float rate = bridge->glutamate_clearance_rate;
    nimcp_mutex_unlock(((astro_immune_bridge_t*)bridge)->base.mutex);
    return rate;
}

int astro_cell_get_stats(const astro_immune_bridge_t* bridge, astro_immune_stats_t* stats)
{
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    NIMCP_CHECK_THROW(bridge->base.mutex, NIMCP_ERROR_INVALID_STATE, "bridge mutex is NULL");

    nimcp_mutex_lock(((astro_immune_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((astro_immune_bridge_t*)bridge)->base.mutex);

    return 0;
}

void astro_cell_reset_stats(astro_immune_bridge_t* bridge)
{
    if (!bridge) return;
    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

const char* astrocyte_reactivity_to_string(astrocyte_reactivity_t state)
{
    switch (state) {
        case ASTRO_QUIESCENT:    return "QUIESCENT";
        case ASTRO_A1_REACTIVE:  return "A1_REACTIVE";
        case ASTRO_A2_REACTIVE:  return "A2_REACTIVE";
        case ASTRO_SCAR_FORMING: return "SCAR_FORMING";
        default: return "UNKNOWN";
    }
}
