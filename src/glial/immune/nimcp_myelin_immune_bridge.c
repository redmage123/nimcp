/**
 * @file nimcp_myelin_immune_bridge.c
 * @brief Myelin-Immune Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 */

#include "glial/immune/nimcp_myelin_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(myelin_immune_bridge)

/* ============================================================================
 * KG-Driven Wiring Infrastructure
 * ============================================================================ */

/**
 * Handler map for myelin-immune bridge module.
 * Currently empty - handlers to be added as module evolves.
 */
DEFINE_HANDLER_MAP_BEGIN(myelin_immune)
    /* Future handlers will be added here as needed */
DEFINE_HANDLER_MAP_END()

/**
 * Wiring callback for KG-driven handler registration.
 */
DEFINE_HANDLER_CALLBACK(myelin_immune, myelin_immune_bridge_t, bridge)

int myelin_immune_default_config(myelin_immune_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "myelin_immune_default_config: config is NULL");
        return -1;
    }
    memset(config, 0, sizeof(*config));
    config->il1_damage_rate = 0.2f;
    config->tnf_damage_rate = 0.3f;
    config->ifn_gamma_damage_rate = 0.4f;
    config->il10_repair_rate = 0.1f;
    config->integrity_repair_rate = 0.01f;
    config->enable_bio_async = true;
    config->inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL;
    return 0;
}

myelin_immune_bridge_t* myelin_immune_create(
    const myelin_immune_config_t* config,
    nimcp_myelin_sheath_t* myelin,
    brain_immune_system_t* immune_system)
{
    myelin_immune_bridge_t* bridge = nimcp_malloc(sizeof(myelin_immune_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }
    memset(bridge, 0, sizeof(*bridge));

    if (config) bridge->config = *config;
    else myelin_immune_default_config(&bridge->config);

    bridge->myelin = myelin;
    bridge->immune_system = immune_system;
    bridge->sheath_integrity = 1.0f;
    bridge->conduction_efficiency = 1.0f;

    if (bridge_base_init(&bridge->base, 0, "myelin_immune") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->initialized = true;
    NIMCP_LOGGING_INFO("Created myelin-immune bridge");
    return bridge;
}

void myelin_immune_destroy(myelin_immune_bridge_t* bridge)
{
    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int myelin_immune_connect_bio_async(myelin_immune_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_MYELIN,
        .module_name = MYELIN_IMMUNE_MODULE_NAME,
        .inbox_capacity = bridge->config.inbox_capacity,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int myelin_immune_disconnect_bio_async(myelin_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) { bio_router_unregister_module(bridge->base.bio_ctx); bridge->base.bio_ctx = NULL; }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool myelin_immune_is_bio_async_connected(const myelin_immune_bridge_t* bridge)
{
    return bridge && bridge->base.bio_async_enabled;
}

int myelin_immune_update_cytokine_effects(myelin_immune_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);

    float il1 = 0, tnf = 0, ifn = 0, il10 = 0;
    if (bridge->immune_system) {
        il1 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL1);
        tnf = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_TNF);
        ifn = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);
        il10 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL10);
    }

    bridge->cytokine_effects.il1_damage = il1 * bridge->config.il1_damage_rate;
    bridge->cytokine_effects.tnf_damage = tnf * bridge->config.tnf_damage_rate;
    bridge->cytokine_effects.ifn_gamma_damage = ifn * bridge->config.ifn_gamma_damage_rate;
    bridge->cytokine_effects.il10_repair = il10 * bridge->config.il10_repair_rate;
    bridge->cytokine_effects.net_damage =
        bridge->cytokine_effects.il1_damage +
        bridge->cytokine_effects.tnf_damage +
        bridge->cytokine_effects.ifn_gamma_damage -
        bridge->cytokine_effects.il10_repair;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int myelin_immune_apply_damage(myelin_immune_bridge_t* bridge, float dt_ms)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);

    float damage = bridge->cytokine_effects.net_damage * (dt_ms / 1000.0f);
    if (damage > 0) {
        bridge->sheath_integrity -= damage;
        if (bridge->sheath_integrity < 0) bridge->sheath_integrity = 0;
        bridge->stats.total_integrity_lost += damage;
        bridge->stats.damage_events++;
    }
    bridge->conduction_efficiency = bridge->sheath_integrity;
    bridge->stats.current_integrity = bridge->sheath_integrity;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int myelin_immune_apply_repair(myelin_immune_bridge_t* bridge, float dt_ms)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->cytokine_effects.net_damage < 0.1f && bridge->sheath_integrity < 1.0f) {
        float repair = bridge->config.integrity_repair_rate * (dt_ms / 1000.0f);
        bridge->sheath_integrity += repair;
        if (bridge->sheath_integrity > 1.0f) bridge->sheath_integrity = 1.0f;
        bridge->stats.total_integrity_restored += repair;
        bridge->stats.repair_events++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int myelin_immune_update(myelin_immune_bridge_t* bridge, float dt_ms)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    myelin_immune_update_cytokine_effects(bridge);
    myelin_immune_apply_damage(bridge, dt_ms);
    myelin_immune_apply_repair(bridge, dt_ms);
    return 0;
}

float myelin_immune_get_integrity(const myelin_immune_bridge_t* bridge)
{
    return bridge ? bridge->sheath_integrity : 1.0f;
}

float myelin_immune_get_conduction_efficiency(const myelin_immune_bridge_t* bridge)
{
    return bridge ? bridge->conduction_efficiency : 1.0f;
}

int myelin_immune_get_stats(const myelin_immune_bridge_t* bridge, myelin_immune_stats_t* stats)
{
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    *stats = bridge->stats;
    return 0;
}

void myelin_immune_reset_stats(myelin_immune_bridge_t* bridge)
{
    if (!bridge) return;
    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}
