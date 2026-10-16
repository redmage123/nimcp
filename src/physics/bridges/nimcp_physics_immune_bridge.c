#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_physics_immune_bridge.c - Physics Layer to Immune System Integration
//=============================================================================
/**
 * @file nimcp_physics_immune_bridge.c
 * @brief Implementation of physics-immune bidirectional bridge
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/bridges/nimcp_physics_immune_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_immune_bridge)

#define LOG_MODULE "PHYSICS_IMMUNE_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct physics_immune_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /** Configuration */
    physics_immune_config_t config;

    /** Connected physics modules */
    nimcp_thermodynamic_state_t* thermo;
    nimcp_hh_population_t* hh_pop;
    nimcp_ephaptic_system_t* ephaptic;

    /** Connected immune system */
    brain_immune_system_t* immune;

    /** Current physics state */
    physics_immune_state_t current_state;

    /** Current modulation being applied */
    physics_immune_modulation_t current_modulation;

    /** Statistics */
    physics_immune_stats_t stats;

    /** Time since last temperature sample */
    float temp_sample_timer;

    /** Time since last ATP sample */
    float atp_sample_timer;

    /** Initialized flag */
    bool initialized;
};

//=============================================================================
// Configuration API
//=============================================================================

int physics_immune_default_config(physics_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->monitor_temperature = true;
    config->monitor_atp = true;
    config->enable_cytokine_mod = true;
    config->enable_inflammation = true;
    config->temp_sample_interval_ms = 100.0f;
    config->atp_sample_interval_ms = 50.0f;
    config->fever_response_scale = 1.5f;
    config->hypo_response_scale = 0.7f;
    config->cytokine_mod_strength = 0.3f;

    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

physics_immune_bridge_t* physics_immune_bridge_create(
    const physics_immune_config_t* config
) {
    physics_immune_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate physics-immune bridge");
    if (bridge_base_init(&bridge->base, 0, "physics_immune") != 0) { nimcp_free(bridge); return NULL; }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        physics_immune_default_config(&bridge->config);
    }

    /* Initialize state to normal values */
    bridge->current_state.temperature = PHYSICS_IMMUNE_NORMAL_TEMP;
    bridge->current_state.atp_level = 1.0f;

    /* Initialize modulation to neutral values */
    bridge->current_modulation.g_na_modifier = 1.0f;
    bridge->current_modulation.g_k_modifier = 1.0f;
    bridge->current_modulation.g_leak_modifier = 1.0f;
    bridge->current_modulation.cm_modifier = 1.0f;
    bridge->current_modulation.temp_offset = 0.0f;
    bridge->current_modulation.q10_modifier = 1.0f;
    bridge->current_modulation.inflammation_level = 0.0f;

    bridge->initialized = true;

    NIMCP_LOG_INFO(PHYSICS_IMMUNE_MODULE_NAME, "Physics-immune bridge created");
    return bridge;
}

void physics_immune_bridge_destroy(physics_immune_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "physics_immune");

    NIMCP_LOG_INFO(PHYSICS_IMMUNE_MODULE_NAME, "Bridge destroyed - "
        "physics_to_immune: %lu, immune_to_physics: %lu",
        (unsigned long)bridge->stats.physics_to_immune_count,
        (unsigned long)bridge->stats.immune_to_physics_count);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int physics_immune_connect_physics(
    physics_immune_bridge_t* bridge,
    nimcp_thermodynamic_state_t* thermo,
    nimcp_hh_population_t* hh_pop,
    nimcp_ephaptic_system_t* ephaptic
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->thermo = thermo;
    bridge->hh_pop = hh_pop;
    bridge->ephaptic = ephaptic;

    NIMCP_LOG_DEBUG(PHYSICS_IMMUNE_MODULE_NAME,
        "Connected physics: thermo=%p, hh=%p, ephaptic=%p",
        (void*)thermo, (void*)hh_pop, (void*)ephaptic);

    return 0;
}

int physics_immune_connect_immune(
    physics_immune_bridge_t* bridge,
    brain_immune_system_t* immune
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->immune = immune;

    NIMCP_LOG_DEBUG(PHYSICS_IMMUNE_MODULE_NAME,
        "Connected immune system: %p", (void*)immune);

    return 0;
}

//=============================================================================
// Physics → Immune API
//=============================================================================

int physics_immune_report_state(
    physics_immune_bridge_t* bridge,
    physics_immune_state_t* state
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Sample thermodynamics state */
    if (bridge->thermo) {
        /* Use available thermodynamics getters */
        double atp_ratio = nimcp_thermo_get_atp_ratio(bridge->thermo);
        bridge->current_state.atp_level = (float)atp_ratio;

        /* Temperature would be read from state directly if accessible */
        /* For now, use default normal temperature if not available */
        if (bridge->current_state.temperature < 20.0f) {
            bridge->current_state.temperature = PHYSICS_IMMUNE_NORMAL_TEMP;
        }
    }

    /* Sample HH population state */
    if (bridge->hh_pop) {
        /* Get population firing rate as proxy for activity level */
        float rate;
        if (nimcp_hh_population_get_rate(bridge->hh_pop, &rate) == NIMCP_SUCCESS) {
            /* Higher rate = more depolarized on average */
            bridge->current_state.avg_membrane_potential = -70.0f + (rate * 0.5f);
        }
    }

    /* Sample ephaptic state */
    if (bridge->ephaptic) {
        /* Use available ephaptic getters */
        float coherence;
        if (nimcp_ephaptic_get_phase_coherence(bridge->ephaptic, &coherence) == NIMCP_SUCCESS) {
            bridge->current_state.phase_coherence = coherence;
        }
        /* LFP amplitude would need to be tracked separately or via bridge */
    }

    /* Update statistics */
    bridge->stats.physics_to_immune_count++;

    /* Copy to output if requested */
    if (state) {
        *state = bridge->current_state;
    }

    return 0;
}

physics_immune_interaction_t physics_immune_check_temperature(
    physics_immune_bridge_t* bridge
) {
    if (!bridge || !bridge->config.monitor_temperature) {
        return PHYSICS_IMMUNE_INTERACTION_NONE;
    }

    float temp = bridge->current_state.temperature;

    if (temp >= PHYSICS_IMMUNE_FEVER_THRESH) {
        bridge->stats.temp_alerts++;
        NIMCP_LOG_DEBUG(PHYSICS_IMMUNE_MODULE_NAME,
            "Fever detected: %.1f°C", temp);
        return PHYSICS_IMMUNE_INTERACTION_TEMP_FEVER;
    }

    if (temp <= PHYSICS_IMMUNE_HYPO_THRESH) {
        bridge->stats.temp_alerts++;
        NIMCP_LOG_DEBUG(PHYSICS_IMMUNE_MODULE_NAME,
            "Hypothermia detected: %.1f°C", temp);
        return PHYSICS_IMMUNE_INTERACTION_TEMP_HYPO;
    }

    return PHYSICS_IMMUNE_INTERACTION_NONE;
}

physics_immune_interaction_t physics_immune_check_atp(
    physics_immune_bridge_t* bridge
) {
    if (!bridge || !bridge->config.monitor_atp) {
        return PHYSICS_IMMUNE_INTERACTION_NONE;
    }

    float atp = bridge->current_state.atp_level;

    if (atp <= PHYSICS_IMMUNE_ATP_CRITICAL) {
        bridge->stats.atp_alerts++;
        NIMCP_LOG_WARN(PHYSICS_IMMUNE_MODULE_NAME,
            "Critical ATP level: %.1f%%", atp * 100.0f);
        return PHYSICS_IMMUNE_INTERACTION_ATP_CRITICAL;
    }

    if (atp <= PHYSICS_IMMUNE_ATP_LOW) {
        bridge->stats.atp_alerts++;
        NIMCP_LOG_DEBUG(PHYSICS_IMMUNE_MODULE_NAME,
            "Low ATP level: %.1f%%", atp * 100.0f);
        return PHYSICS_IMMUNE_INTERACTION_ATP_LOW;
    }

    return PHYSICS_IMMUNE_INTERACTION_NONE;
}

//=============================================================================
// Immune → Physics API
//=============================================================================

int physics_immune_apply_modulation(
    physics_immune_bridge_t* bridge,
    const physics_immune_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_immune_apply_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    bridge->current_modulation = *modulation;
    bridge->stats.immune_to_physics_count++;

    /* Apply to HH population if connected */
    if (bridge->hh_pop && bridge->config.enable_cytokine_mod) {
        /* In a full implementation, would modify each neuron's conductances */
        /* For now, we store the modulation for query */
        NIMCP_LOG_DEBUG(PHYSICS_IMMUNE_MODULE_NAME,
            "Applied modulation: g_Na=%.2f, g_K=%.2f",
            modulation->g_na_modifier, modulation->g_k_modifier);
    }

    return 0;
}

int physics_immune_receive_cytokine(
    physics_immune_bridge_t* bridge,
    uint32_t cytokine_type,
    float concentration
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_cytokine_mod) return 0;

    /* Map cytokine type to conductance modulation */
    physics_immune_modulation_t mod = bridge->current_modulation;
    float strength = bridge->config.cytokine_mod_strength;

    /* Different cytokines affect different channels */
    /* Simplified model based on known inflammatory effects */
    switch (cytokine_type) {
        case 0:  /* IL-1 analog - reduces Na conductance */
            mod.g_na_modifier = 1.0f - (strength * concentration);
            break;
        case 1:  /* TNF-alpha analog - increases leak conductance */
            mod.g_leak_modifier = 1.0f + (strength * concentration);
            break;
        case 2:  /* IL-6 analog - reduces K conductance */
            mod.g_k_modifier = 1.0f - (strength * concentration * 0.5f);
            break;
        default:
            break;
    }

    /* Clamp modifiers to valid range */
    mod.g_na_modifier = fmaxf(PHYSICS_IMMUNE_MIN_CYTOKINE_MOD,
                              fminf(PHYSICS_IMMUNE_MAX_CYTOKINE_MOD, mod.g_na_modifier));
    mod.g_k_modifier = fmaxf(PHYSICS_IMMUNE_MIN_CYTOKINE_MOD,
                             fminf(PHYSICS_IMMUNE_MAX_CYTOKINE_MOD, mod.g_k_modifier));
    mod.g_leak_modifier = fmaxf(PHYSICS_IMMUNE_MIN_CYTOKINE_MOD,
                                fminf(PHYSICS_IMMUNE_MAX_CYTOKINE_MOD, mod.g_leak_modifier));

    mod.cytokine_source = cytokine_type;
    bridge->stats.cytokine_mods_applied++;

    return physics_immune_apply_modulation(bridge, &mod);
}

int physics_immune_receive_inflammation(
    physics_immune_bridge_t* bridge,
    float inflammation_level,
    float region_x,
    float region_y,
    float region_z
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_inflammation) return 0;

    (void)region_x;  /* Spatial specificity for future use */
    (void)region_y;
    (void)region_z;

    /* Inflammation increases membrane permeability */
    physics_immune_modulation_t mod = bridge->current_modulation;
    mod.inflammation_level = inflammation_level;

    /* Scale effects by inflammation level */
    float scale = 1.0f + (inflammation_level * 0.5f);
    mod.g_leak_modifier *= scale;
    mod.cm_modifier = 1.0f - (inflammation_level * 0.1f);  /* Slight capacitance reduction */

    bridge->stats.inflammation_events++;

    NIMCP_LOG_DEBUG(PHYSICS_IMMUNE_MODULE_NAME,
        "Inflammation received: level=%.2f at (%.1f, %.1f, %.1f)",
        inflammation_level, region_x, region_y, region_z);

    return physics_immune_apply_modulation(bridge, &mod);
}

//=============================================================================
// Update API
//=============================================================================

int physics_immune_update(
    physics_immune_bridge_t* bridge,
    float dt
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Update timers */
    bridge->temp_sample_timer += dt;
    bridge->atp_sample_timer += dt;

    /* Sample temperature if interval elapsed */
    if (bridge->config.monitor_temperature &&
        bridge->temp_sample_timer >= bridge->config.temp_sample_interval_ms) {
        bridge->temp_sample_timer = 0.0f;
        physics_immune_report_state(bridge, NULL);
        physics_immune_check_temperature(bridge);
    }

    /* Sample ATP if interval elapsed */
    if (bridge->config.monitor_atp &&
        bridge->atp_sample_timer >= bridge->config.atp_sample_interval_ms) {
        bridge->atp_sample_timer = 0.0f;
        physics_immune_check_atp(bridge);
    }

    bridge->stats.last_update_ms += dt;

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

int physics_immune_get_state(
    const physics_immune_bridge_t* bridge,
    physics_immune_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_immune_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->current_state;
    return 0;
}

int physics_immune_get_modulation(
    const physics_immune_bridge_t* bridge,
    physics_immune_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_immune_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }
    *modulation = bridge->current_modulation;
    return 0;
}

int physics_immune_get_stats(
    const physics_immune_bridge_t* bridge,
    physics_immune_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_immune_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

bool physics_immune_is_connected(const physics_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* At least one physics module and immune must be connected */
    bool has_physics = bridge->thermo || bridge->hh_pop || bridge->ephaptic;
    return has_physics && bridge->immune;
}
