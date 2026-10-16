#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_physics_kg_wiring.c - Physics Layer Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_physics_kg_wiring.c
 * @brief Implementation of physics layer KG registration
 */

#include "physics/bridges/nimcp_physics_kg_wiring.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_kg_wiring)

/*=============================================================================
 * W15: Physics-wide runtime event emission + read-path
 *
 * Rate-limit: emit aggregated state summary only when trigger() is called.
 * Callers choose when (epoch boundary, mode change, sleep stage flip, etc).
 * Never call from a per-neuron / per-tick / per-spike loop.
 *===========================================================================*/

static brain_t s_physics_kg_brain = NULL;

void physics_kg_wiring_register_brain(brain_t brain) {
    s_physics_kg_brain = brain;
}

void physics_kg_trigger_state_summary(brain_t brain,
                                      float temperature,
                                      float atp_level,
                                      float lfp_amp,
                                      const char* trigger,
                                      uint64_t ts_us) {
    if (!brain) brain = s_physics_kg_brain;
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    char ev_name[160];
    snprintf(ev_name, sizeof(ev_name),
             "physics_event_state_summary_%llu",
             (unsigned long long)ts_us);
    char desc[256];
    snprintf(desc, sizeof(desc),
             "Physics aggregated state summary: trigger=%s T=%.2fK atp=%.3f lfp=%.4fmV",
             trigger ? trigger : "unspecified",
             isnan(temperature) ? 0.0f : temperature,
             isnan(atp_level)   ? 0.0f : atp_level,
             isnan(lfp_amp)     ? 0.0f : lfp_amp);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, ev_name,
        BRAIN_KG_NODE_CORE, desc);
    if (ev != BRAIN_KG_INVALID_NODE) {
        /* Link back to the physics_layer root and each active subsystem. */
        brain_kg_node_id_t root = brain_kg_find_node(kg, PHYSICS_KG_ROOT_NAME);
        if (root != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, root, ev, BRAIN_KG_EDGE_SENDS_TO,
                "physics_state_summary", 1.0f);
        }
        /* Metadata */
        char buf[64];
        if (!isnan(temperature)) {
            snprintf(buf, sizeof(buf), "%.2f K", temperature);
            brain_kg_add_metadata(kg, ev, "temperature", buf);
        }
        if (!isnan(atp_level)) {
            snprintf(buf, sizeof(buf), "%.3f", atp_level);
            brain_kg_add_metadata(kg, ev, "atp_level", buf);
        }
        if (!isnan(lfp_amp)) {
            snprintf(buf, sizeof(buf), "%.4f mV", lfp_amp);
            brain_kg_add_metadata(kg, ev, "lfp_amplitude", buf);
        }
        if (trigger) {
            brain_kg_add_metadata(kg, ev, "trigger", trigger);
        }
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", buf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

uint32_t physics_kg_count_events(brain_kg_t* kg, const char* kind_substr) {
    if (!kg) return 0;
    brain_kg_node_id_t root = brain_kg_find_node(kg, PHYSICS_KG_ROOT_NAME);
    if (root == BRAIN_KG_INVALID_NODE) return 0;
    brain_kg_edge_list_t* out = brain_kg_get_outgoing(kg, root);
    if (!out) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < out->count; ++i) {
        const brain_kg_edge_t* e = out->edges[i];
        if (!e) continue;
        const brain_kg_node_t* to = brain_kg_get_node(kg, e->to);
        if (!to) continue;
        if (!kind_substr || !*kind_substr) {
            count++;
        } else if (to->description && strstr(to->description, kind_substr)) {
            count++;
        } else if (to->name && strstr(to->name, kind_substr)) {
            count++;
        }
    }
    brain_kg_edge_list_destroy(out);
    return count;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create a physics node with description
 */
static brain_kg_node_id_t create_physics_node(
    brain_kg_t* kg,
    const char* name,
    brain_kg_node_type_t type,
    const char* description,
    uint64_t admin_token
) {
    (void)admin_token;  /* Not used in current API */
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;

    brain_kg_node_id_t id = brain_kg_add_node(kg, name, type, description);
    if (id != BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_DEBUG(PHYSICS_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

/**
 * @brief Create an edge between physics nodes
 */
static brain_kg_edge_id_t create_physics_edge(
    brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    brain_kg_edge_type_t type,
    const char* description,
    float weight,
    uint64_t admin_token
) {
    (void)admin_token;  /* Not used in current API */
    if (!kg) return BRAIN_KG_INVALID_NODE;
    if (from == BRAIN_KG_INVALID_NODE || to == BRAIN_KG_INVALID_NODE) {
        return BRAIN_KG_INVALID_NODE;
    }

    brain_kg_edge_id_t id = brain_kg_add_edge(
        kg, from, to, type, description, weight
    );
    return id;
}

//=============================================================================
// Configuration API
//=============================================================================

int physics_kg_default_config(physics_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_kg_default_config: config is NULL");
        return -1;
    }

    config->register_hh = true;
    config->register_thermo = true;
    config->register_ephaptic = true;
    config->register_channel_details = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int physics_kg_register_all(
    brain_kg_t* kg,
    const physics_kg_config_t* config,
    physics_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_kg_register_all: kg is NULL");
        return -1;
    }

    physics_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        physics_kg_default_config(&local_config);
    }

    physics_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create physics layer root node */
    local_state.root_id = create_physics_node(
        kg, PHYSICS_KG_ROOT_NAME,
        BRAIN_KG_NODE_CORE,
        "Phase 1 Physics layer - biophysical simulation",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(PHYSICS_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Register subsystems */
    if (local_config.register_hh) {
        if (physics_kg_register_hh(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(PHYSICS_KG_MODULE_NAME, "Failed to register HH subsystem");
        }
    }

    if (local_config.register_thermo) {
        if (physics_kg_register_thermo(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(PHYSICS_KG_MODULE_NAME, "Failed to register thermo subsystem");
        }
    }

    if (local_config.register_ephaptic) {
        if (physics_kg_register_ephaptic(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(PHYSICS_KG_MODULE_NAME, "Failed to register ephaptic subsystem");
        }
    }

    /* Register cross-subsystem edges */
    if (local_config.register_cross_edges) {
        physics_kg_register_cross_edges(kg, &local_state, admin_token);
    }

    local_state.registered = true;

    /* Copy to output if requested */
    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(PHYSICS_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

int physics_kg_register_hh(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    physics_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_kg_register_hh: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create HH subsystem node */
    state->hh_id = create_physics_node(
        kg, PHYSICS_KG_HH_NAME,
        BRAIN_KG_NODE_CORE,
        "Hodgkin-Huxley biophysics - ion channels and action potentials",
        admin_token
    );
    if (state->hh_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_kg_register_hh: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_physics_edge(kg, parent_id, state->hh_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains HH subsystem", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create Na channel node */
    state->na_channel_id = create_physics_node(
        kg, "na_channel",
        (brain_kg_node_type_t)PHYSICS_KG_NODE_ION_CHANNEL,
        "Sodium ion channel - fast activation, rapid inactivation",
        admin_token
    );
    if (state->na_channel_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_physics_edge(kg, state->hh_id, state->na_channel_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains Na channel", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create K channel node */
    state->k_channel_id = create_physics_node(
        kg, "k_channel",
        (brain_kg_node_type_t)PHYSICS_KG_NODE_ION_CHANNEL,
        "Potassium ion channel - delayed rectifier",
        admin_token
    );
    if (state->k_channel_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_physics_edge(kg, state->hh_id, state->k_channel_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains K channel", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create Leak channel node */
    state->leak_channel_id = create_physics_node(
        kg, "leak_channel",
        (brain_kg_node_type_t)PHYSICS_KG_NODE_ION_CHANNEL,
        "Leak conductance - passive membrane property",
        admin_token
    );
    if (state->leak_channel_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_physics_edge(kg, state->hh_id, state->leak_channel_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains leak channel", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create edges between channels */
    if (state->na_channel_id != BRAIN_KG_INVALID_NODE &&
        state->k_channel_id != BRAIN_KG_INVALID_NODE) {
        create_physics_edge(kg, state->na_channel_id, state->k_channel_id,
            (brain_kg_edge_type_t)PHYSICS_KG_EDGE_PRECEDES,
            "Na activation precedes K activation", 0.8f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int physics_kg_register_thermo(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    physics_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_kg_register_thermo: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create thermodynamics subsystem node */
    state->thermo_id = create_physics_node(
        kg, PHYSICS_KG_THERMO_NAME,
        BRAIN_KG_NODE_CORE,
        "Thermodynamics - energy, entropy, Landauer computation",
        admin_token
    );
    if (state->thermo_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_kg_register_thermo: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_physics_edge(kg, parent_id, state->thermo_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains thermodynamics", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create ATP pool node */
    state->atp_id = create_physics_node(
        kg, "atp_pool",
        (brain_kg_node_type_t)PHYSICS_KG_NODE_ENERGY_POOL,
        "ATP energy pool - cellular energy currency",
        admin_token
    );
    if (state->atp_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_physics_edge(kg, state->thermo_id, state->atp_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "manages ATP pool", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create heat pool node */
    state->heat_id = create_physics_node(
        kg, "heat_pool",
        (brain_kg_node_type_t)PHYSICS_KG_NODE_ENERGY_POOL,
        "Heat reservoir - thermal energy buffer",
        admin_token
    );
    if (state->heat_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_physics_edge(kg, state->thermo_id, state->heat_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "manages heat pool", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int physics_kg_register_ephaptic(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    physics_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_kg_register_ephaptic: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create ephaptic coupling subsystem node */
    state->ephaptic_id = create_physics_node(
        kg, PHYSICS_KG_EPHAPTIC_NAME,
        BRAIN_KG_NODE_CORE,
        "Ephaptic coupling - LFP and field effects",
        admin_token
    );
    if (state->ephaptic_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_kg_register_ephaptic: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_physics_edge(kg, parent_id, state->ephaptic_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains ephaptic coupling", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create LFP node */
    state->lfp_id = create_physics_node(
        kg, "local_field_potential",
        (brain_kg_node_type_t)PHYSICS_KG_NODE_FIELD,
        "Local field potential - summed extracellular activity",
        admin_token
    );
    if (state->lfp_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_physics_edge(kg, state->ephaptic_id, state->lfp_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "computes LFP", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create phase sync node */
    state->phase_sync_id = create_physics_node(
        kg, "phase_synchronization",
        (brain_kg_node_type_t)PHYSICS_KG_NODE_OSCILLATOR,
        "Phase synchronization - Kuramoto oscillator dynamics",
        admin_token
    );
    if (state->phase_sync_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_physics_edge(kg, state->ephaptic_id, state->phase_sync_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "manages phase sync", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int physics_kg_register_cross_edges(
    brain_kg_t* kg,
    physics_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_kg_register_cross_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Temperature modulates HH kinetics */
    if (state->heat_id != BRAIN_KG_INVALID_NODE &&
        state->hh_id != BRAIN_KG_INVALID_NODE) {
        create_physics_edge(kg, state->heat_id, state->hh_id,
            (brain_kg_edge_type_t)PHYSICS_KG_EDGE_MODULATES,
            "temperature modulates channel kinetics (Q10)", 0.7f, admin_token);
        state->edge_count++;
    }

    /* ATP powers ion pumps (implicit in HH) */
    if (state->atp_id != BRAIN_KG_INVALID_NODE &&
        state->hh_id != BRAIN_KG_INVALID_NODE) {
        create_physics_edge(kg, state->atp_id, state->hh_id,
            (brain_kg_edge_type_t)PHYSICS_KG_EDGE_TRANSFERS_ENERGY,
            "ATP powers Na/K ATPase", 0.9f, admin_token);
        state->edge_count++;
    }

    /* HH activity generates LFP */
    if (state->hh_id != BRAIN_KG_INVALID_NODE &&
        state->lfp_id != BRAIN_KG_INVALID_NODE) {
        create_physics_edge(kg, state->hh_id, state->lfp_id,
            (brain_kg_edge_type_t)PHYSICS_KG_EDGE_CAUSES,
            "membrane currents generate LFP", 0.8f, admin_token);
        state->edge_count++;
    }

    /* LFP influences HH via ephaptic coupling */
    if (state->lfp_id != BRAIN_KG_INVALID_NODE &&
        state->hh_id != BRAIN_KG_INVALID_NODE) {
        create_physics_edge(kg, state->lfp_id, state->hh_id,
            (brain_kg_edge_type_t)PHYSICS_KG_EDGE_INFLUENCES,
            "extracellular field influences membrane potential", 0.3f, admin_token);
        state->edge_count++;
    }

    /* Phase sync couples multiple neurons */
    if (state->phase_sync_id != BRAIN_KG_INVALID_NODE &&
        state->hh_id != BRAIN_KG_INVALID_NODE) {
        create_physics_edge(kg, state->phase_sync_id, state->hh_id,
            (brain_kg_edge_type_t)PHYSICS_KG_EDGE_COUPLES_TO,
            "synchronization couples neuron populations", 0.5f, admin_token);
        state->edge_count++;
    }

    /* HH activity consumes ATP */
    if (state->hh_id != BRAIN_KG_INVALID_NODE &&
        state->atp_id != BRAIN_KG_INVALID_NODE) {
        create_physics_edge(kg, state->hh_id, state->atp_id,
            (brain_kg_edge_type_t)PHYSICS_KG_EDGE_TRANSFERS_ENERGY,
            "neural activity consumes ATP", 0.6f, admin_token);
        state->edge_count++;
    }

    /* HH activity generates heat */
    if (state->hh_id != BRAIN_KG_INVALID_NODE &&
        state->heat_id != BRAIN_KG_INVALID_NODE) {
        create_physics_edge(kg, state->hh_id, state->heat_id,
            (brain_kg_edge_type_t)PHYSICS_KG_EDGE_TRANSFERS_ENERGY,
            "neural activity generates heat (entropy)", 0.4f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// State Synchronization API
//=============================================================================

int physics_kg_update_state(
    brain_kg_t* kg,
    const physics_kg_state_t* state,
    float temperature,
    float atp_level,
    float lfp_amplitude,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_kg_update_state: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Update heat pool metadata */
    if (state->heat_id != BRAIN_KG_INVALID_NODE) {
        char temp_str[32];
        snprintf(temp_str, sizeof(temp_str), "%.2f K", temperature);
        brain_kg_add_metadata(kg, state->heat_id, "temperature", temp_str);
    }

    /* Update ATP pool metadata */
    if (state->atp_id != BRAIN_KG_INVALID_NODE) {
        char atp_str[32];
        snprintf(atp_str, sizeof(atp_str), "%.1f%%", atp_level * 100.0f);
        brain_kg_add_metadata(kg, state->atp_id, "level", atp_str);
    }

    /* Update LFP metadata */
    if (state->lfp_id != BRAIN_KG_INVALID_NODE) {
        char lfp_str[32];
        snprintf(lfp_str, sizeof(lfp_str), "%.3f mV", lfp_amplitude);
        brain_kg_add_metadata(kg, state->lfp_id, "amplitude", lfp_str);
    }

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t physics_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, PHYSICS_KG_ROOT_NAME);
}

brain_kg_node_id_t physics_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

brain_kg_node_list_t* physics_kg_get_ion_channels(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)PHYSICS_KG_NODE_ION_CHANNEL);
}

int physics_kg_unregister_all(
    brain_kg_t* kg,
    physics_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Remove nodes in reverse order of creation */
    /* Child nodes first, then parents */

    /* Note: In a full implementation, would iterate and remove all */
    /* For now, just mark as unregistered */
    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(PHYSICS_KG_MODULE_NAME, "Unregistered physics KG nodes");
    (void)admin_token;  /* Would be used for actual deletion */

    return 0;
}
