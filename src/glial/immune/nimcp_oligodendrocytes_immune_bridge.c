/**
 * @file nimcp_oligodendrocytes_immune_bridge.c
 * @brief Oligodendrocytes-Immune Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Connects oligodendrocytes to brain immune system
 * WHY:  Model inflammation-induced demyelination (MS pathophysiology)
 * HOW:  Routes cytokine signals to modulate myelination and survival
 *
 * @author NIMCP Development Team
 */

#include "glial/immune/nimcp_oligodendrocytes_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(oligodendrocytes_immune_bridge)

/* ============================================================================
 * KG-Driven Wiring Infrastructure
 * ============================================================================ */

/**
 * Handler map for oligodendrocyte-immune bridge module.
 * Currently empty - handlers to be added as module evolves.
 */
DEFINE_HANDLER_MAP_BEGIN(oligo_immune)
    /* Future handlers will be added here as needed */
DEFINE_HANDLER_MAP_END()

/**
 * Wiring callback for KG-driven handler registration.
 */
DEFINE_HANDLER_CALLBACK(oligo_immune, oligo_immune_bridge_t, bridge)

/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static oligo_damage_state_t classify_damage(float damage_level)
{
    if (damage_level >= OLIGO_IMMUNE_DEATH_THRESHOLD) return OLIGO_DAMAGE_DEAD;
    if (damage_level >= 0.7f) return OLIGO_DAMAGE_SEVERE;
    if (damage_level >= 0.4f) return OLIGO_DAMAGE_MODERATE;
    if (damage_level >= 0.1f) return OLIGO_DAMAGE_MILD;
    return OLIGO_DAMAGE_NONE;
}

static demyelination_state_t classify_demyelination(
    oligo_damage_state_t damage_state,
    float myelination_modifier)
{
    if (myelination_modifier > 0.0f) return DEMYELINATION_REMYELINATING;
    if (damage_state == OLIGO_DAMAGE_DEAD) return DEMYELINATION_CHRONIC;
    if (damage_state >= OLIGO_DAMAGE_MODERATE) return DEMYELINATION_ACTIVE;
    if (damage_state >= OLIGO_DAMAGE_MILD) return DEMYELINATION_EARLY;
    return DEMYELINATION_NONE;
}

/* NOTE: Message handlers are registered separately via bio_router_register_handler() */

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int oligo_immune_default_config(oligo_immune_config_t* config)
{
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(*config));

    /* Cytokine sensitivity - based on neuroimmunology literature */
    config->il1_myelination_reduction = 0.3f;   /* IL-1 reduces myelination 30% */
    config->il6_progenitor_inhibition = 0.4f;   /* IL-6 inhibits OPCs 40% */
    config->tnf_oligodendrocyte_death = 0.5f;   /* TNF-alpha high death rate */
    config->il10_protection_factor = 0.6f;      /* IL-10 60% protective */
    config->ifn_gamma_demyelination = 0.5f;     /* IFN-gamma demyelination */

    /* Damage dynamics */
    config->damage_accumulation_rate = 0.01f;
    config->damage_repair_rate = 0.005f;
    config->death_threshold = OLIGO_IMMUNE_DEATH_THRESHOLD;

    /* Remyelination */
    config->remyelination_rate = 0.01f;
    config->opc_recruitment_rate = 0.005f;
    config->enable_remyelination = true;

    /* Bio-async */
    config->enable_bio_async = true;
    config->inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL;

    return 0;
}

oligo_immune_bridge_t* oligo_immune_create(
    const oligo_immune_config_t* config,
    oligodendrocyte_t* oligo,
    brain_immune_system_t* immune_system)
{
    oligo_immune_bridge_t* bridge = nimcp_malloc(sizeof(oligo_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate oligo-immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(*bridge));

    if (config) {
        bridge->config = *config;
    } else {
        oligo_immune_default_config(&bridge->config);
    }

    bridge->oligo = oligo;
    bridge->immune_system = immune_system;
    bridge->myelination_rate_modifier = 1.0f;

    /* Create mutex using platform API (allocates + initializes internally) */
    if (bridge_base_init(&bridge->base, 0, "oligodendrocytes_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "oligo_immune_create: bridge->base is NULL");
        return NULL;
    }

    bridge->initialized = true;
    bridge->last_update_time = nimcp_time_get_us();
    bridge->damage_state = OLIGO_DAMAGE_NONE;
    bridge->demyelination_state = DEMYELINATION_NONE;

    NIMCP_LOGGING_INFO("Created oligo-immune bridge");
    return bridge;
}

void oligo_immune_destroy(oligo_immune_bridge_t* bridge)
{
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        oligo_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex (created with nimcp_platform_mutex_create) */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed oligo-immune bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int oligo_immune_connect_network(
    oligo_immune_bridge_t* bridge,
    oligodendrocyte_network_t* network)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->oligo_network = network;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int oligo_immune_connect_bio_async(oligo_immune_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_OLIGODENDROCYTE,
        .module_name = OLIGO_IMMUNE_MODULE_NAME,
        .inbox_capacity = bridge->config.inbox_capacity,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected oligo-immune to bio-async");
    }
    return 0;
}

int oligo_immune_disconnect_bio_async(oligo_immune_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool oligo_immune_is_bio_async_connected(const oligo_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Cytokine Effects API
 * ============================================================================ */

int oligo_immune_update_cytokine_effects(oligo_immune_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get cytokine levels from immune system */
    float il1 = 0.0f, il6 = 0.0f, il10 = 0.0f, tnf = 0.0f, ifn_gamma = 0.0f;

    if (bridge->immune_system) {
        il1 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL1);
        il6 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL6);
        il10 = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL10);
        tnf = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_TNF);
        ifn_gamma = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);
    }

    /* Compute effects */
    bridge->cytokine_effects.il1_effect = il1 * bridge->config.il1_myelination_reduction;
    bridge->cytokine_effects.il6_effect = il6 * bridge->config.il6_progenitor_inhibition;
    bridge->cytokine_effects.tnf_effect = tnf * bridge->config.tnf_oligodendrocyte_death;
    bridge->cytokine_effects.il10_effect = il10 * bridge->config.il10_protection_factor;
    bridge->cytokine_effects.ifn_gamma_effect = ifn_gamma * bridge->config.ifn_gamma_demyelination;

    /* Net signals */
    bridge->cytokine_effects.net_damage_signal =
        bridge->cytokine_effects.il1_effect +
        bridge->cytokine_effects.il6_effect +
        bridge->cytokine_effects.tnf_effect +
        bridge->cytokine_effects.ifn_gamma_effect;

    bridge->cytokine_effects.net_protection_signal =
        bridge->cytokine_effects.il10_effect;

    bridge->stats.cytokine_events++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int oligo_immune_get_cytokine_effects(
    const oligo_immune_bridge_t* bridge,
    oligo_cytokine_effects_t* effects)
{
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");
    *effects = bridge->cytokine_effects;
    return 0;
}

int oligo_immune_apply_modulation(oligo_immune_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute myelination modifier */
    float net_effect = bridge->cytokine_effects.net_protection_signal -
                       bridge->cytokine_effects.net_damage_signal;

    bridge->myelination_rate_modifier = 1.0f + net_effect;
    if (bridge->myelination_rate_modifier < 0.0f) {
        bridge->myelination_rate_modifier = 0.0f;
    }
    if (bridge->myelination_rate_modifier > 2.0f) {
        bridge->myelination_rate_modifier = 2.0f;
    }

    /* OPC recruitment inhibition from IL-6 */
    bridge->progenitor_recruitment = 1.0f - bridge->cytokine_effects.il6_effect;
    if (bridge->progenitor_recruitment < 0.0f) {
        bridge->progenitor_recruitment = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Damage and Death API
 * ============================================================================ */

int oligo_immune_accumulate_damage(
    oligo_immune_bridge_t* bridge,
    float dt_ms)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Accumulate damage from net damage signal */
    float damage_delta = bridge->cytokine_effects.net_damage_signal *
                         bridge->config.damage_accumulation_rate * (dt_ms / 1000.0f);

    /* Subtract protection */
    damage_delta -= bridge->cytokine_effects.net_protection_signal *
                    bridge->config.damage_repair_rate * (dt_ms / 1000.0f);

    bridge->damage_level += damage_delta;

    /* Clamp */
    if (bridge->damage_level < 0.0f) bridge->damage_level = 0.0f;
    if (bridge->damage_level > OLIGO_IMMUNE_MAX_DAMAGE) {
        bridge->damage_level = OLIGO_IMMUNE_MAX_DAMAGE;
    }

    /* Update state */
    bridge->damage_state = classify_damage(bridge->damage_level);

    if (damage_delta > 0) {
        bridge->stats.total_damage_accumulated += damage_delta;
        bridge->stats.damage_events++;
    }

    bridge->stats.current_damage_level = bridge->damage_level;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool oligo_immune_check_death(oligo_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bool died = false;
    if (bridge->damage_level >= bridge->config.death_threshold) {
        bridge->damage_state = OLIGO_DAMAGE_DEAD;
        bridge->stats.death_events++;
        died = true;
        NIMCP_LOGGING_WARN("Oligodendrocyte death from inflammatory damage");
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return died;
}

oligo_damage_state_t oligo_immune_get_damage_state(
    const oligo_immune_bridge_t* bridge)
{
    if (!bridge) return OLIGO_DAMAGE_NONE;
    return bridge->damage_state;
}

/* ============================================================================
 * Demyelination API
 * ============================================================================ */

int oligo_immune_process_demyelination(
    oligo_immune_bridge_t* bridge,
    float dt_ms)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Demyelination rate based on damage and IFN-gamma */
    float demyelin_rate = bridge->damage_level * 0.1f +
                          bridge->cytokine_effects.ifn_gamma_effect * 0.2f;

    if (bridge->oligo && demyelin_rate > 0.0f) {
        /* Would reduce myelination levels on oligodendrocyte */
        bridge->stats.total_myelin_lost += demyelin_rate * (dt_ms / 1000.0f);
    }

    /* Update demyelination state */
    bridge->demyelination_state = classify_demyelination(
        bridge->damage_state, bridge->myelination_rate_modifier - 1.0f);

    bridge->stats.demyelination_state = bridge->demyelination_state;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

demyelination_state_t oligo_immune_get_demyelination_state(
    const oligo_immune_bridge_t* bridge)
{
    if (!bridge) return DEMYELINATION_NONE;
    return bridge->demyelination_state;
}

/* ============================================================================
 * Remyelination API
 * ============================================================================ */

int oligo_immune_process_remyelination(
    oligo_immune_bridge_t* bridge,
    float dt_ms)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_remyelination) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Remyelination only if inflammation resolved */
    bool can_remyelinate = (bridge->cytokine_effects.net_damage_signal < 0.2f) &&
                           (bridge->damage_state != OLIGO_DAMAGE_DEAD);

    if (can_remyelinate) {
        float remyelin_rate = bridge->config.remyelination_rate *
                              bridge->progenitor_recruitment;

        if (bridge->oligo && remyelin_rate > 0.0f) {
            bridge->stats.total_myelin_restored += remyelin_rate * (dt_ms / 1000.0f);
            bridge->stats.remyelination_events++;
        }

        /* Also repair damage slowly */
        bridge->damage_level -= bridge->config.damage_repair_rate * (dt_ms / 1000.0f);
        if (bridge->damage_level < 0.0f) bridge->damage_level = 0.0f;
        bridge->damage_state = classify_damage(bridge->damage_level);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float oligo_immune_get_remyelination_capacity(
    const oligo_immune_bridge_t* bridge)
{
    if (!bridge) return 0.0f;
    if (bridge->damage_state == OLIGO_DAMAGE_DEAD) return 0.0f;

    /* Capacity based on inflammation level and OPC recruitment */
    float capacity = bridge->progenitor_recruitment *
                     (1.0f - bridge->cytokine_effects.net_damage_signal);
    if (capacity < 0.0f) capacity = 0.0f;
    if (capacity > 1.0f) capacity = 1.0f;

    return capacity;
}

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

int oligo_immune_update(oligo_immune_bridge_t* bridge, float dt_ms)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Update cytokine effects */
    oligo_immune_update_cytokine_effects(bridge);

    /* Apply modulation */
    oligo_immune_apply_modulation(bridge);

    /* Accumulate damage */
    oligo_immune_accumulate_damage(bridge, dt_ms);

    /* Check for death */
    oligo_immune_check_death(bridge);

    /* Process demyelination */
    oligo_immune_process_demyelination(bridge, dt_ms);

    /* Process remyelination */
    oligo_immune_process_remyelination(bridge, dt_ms);

    bridge->last_update_time = nimcp_time_get_us();

    return 0;
}

int oligo_immune_get_stats(
    const oligo_immune_bridge_t* bridge,
    oligo_immune_stats_t* stats)
{
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    *stats = bridge->stats;
    return 0;
}

void oligo_immune_reset_stats(oligo_immune_bridge_t* bridge)
{
    if (!bridge) return;
    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

/* ============================================================================
 * String Conversion
 * ============================================================================ */

const char* oligo_damage_state_to_string(oligo_damage_state_t state)
{
    switch (state) {
        case OLIGO_DAMAGE_NONE:     return "NONE";
        case OLIGO_DAMAGE_MILD:     return "MILD";
        case OLIGO_DAMAGE_MODERATE: return "MODERATE";
        case OLIGO_DAMAGE_SEVERE:   return "SEVERE";
        case OLIGO_DAMAGE_DEAD:     return "DEAD";
        default: return "UNKNOWN";
    }
}

const char* demyelination_state_to_string(demyelination_state_t state)
{
    switch (state) {
        case DEMYELINATION_NONE:          return "NONE";
        case DEMYELINATION_EARLY:         return "EARLY";
        case DEMYELINATION_ACTIVE:        return "ACTIVE";
        case DEMYELINATION_CHRONIC:       return "CHRONIC";
        case DEMYELINATION_REMYELINATING: return "REMYELINATING";
        default: return "UNKNOWN";
    }
}
