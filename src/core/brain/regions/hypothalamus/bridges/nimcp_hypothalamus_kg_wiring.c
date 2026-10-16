//=============================================================================
// nimcp_hypothalamus_kg_wiring.c - Hypothalamus Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_hypothalamus_kg_wiring.c
 * @brief Implementation of Hypothalamus Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for Hypothalamus module
 * WHY:  Enables semantic queries about homeostasis, circadian, and stress
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/hypothalamus/bridges/nimcp_hypothalamus_kg_wiring.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Helper Functions
//=============================================================================

static brain_kg_node_id_t create_hypothalamus_node(
    brain_kg_t* kg,
    const char* name,
    brain_kg_node_type_t type,
    const char* description,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;

    brain_kg_node_id_t id = brain_kg_add_node(kg, name, type, description);
    if (id != BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_DEBUG(HYPOTHALAMUS_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

static brain_kg_edge_id_t create_hypothalamus_edge(
    brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    brain_kg_edge_type_t type,
    const char* description,
    float weight,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg) return BRAIN_KG_INVALID_NODE;
    if (from == BRAIN_KG_INVALID_NODE || to == BRAIN_KG_INVALID_NODE) {
        return BRAIN_KG_INVALID_NODE;
    }
    return brain_kg_add_edge(kg, from, to, type, description, weight);
}

//=============================================================================
// Configuration API
//=============================================================================

int hypothalamus_kg_default_config(hypothalamus_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_kg_default_config: config is NULL");
        return -1;
    }

    config->register_scn = true;
    config->register_pvn = true;
    config->register_vmh = true;
    config->register_lh = true;
    config->register_homeostasis = true;
    config->register_circadian_rhythm = true;
    config->register_stress_response = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int hypothalamus_kg_register_all(
    brain_kg_t* kg,
    const hypothalamus_kg_config_t* config,
    hypothalamus_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_kg_register_all: kg is NULL");
        return -1;
    }

    hypothalamus_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        hypothalamus_kg_default_config(&local_config);
    }

    hypothalamus_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create root node */
    local_state.root_id = create_hypothalamus_node(
        kg, HYPOTHALAMUS_KG_ROOT_NAME,
        BRAIN_KG_NODE_SUBCORTICAL,
        "Hypothalamus - homeostasis, circadian rhythm, HPA axis, neuroendocrine control",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(HYPOTHALAMUS_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypothalamus_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* SCN */
    if (local_config.register_scn) {
        local_state.scn_id = create_hypothalamus_node(
            kg, "SCN",
            (brain_kg_node_type_t)HYPOTHALAMUS_KG_NODE_SCN,
            "Suprachiasmatic nucleus - master circadian clock, light entrainment",
            admin_token
        );
        if (local_state.scn_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_hypothalamus_edge(kg, local_state.root_id, local_state.scn_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains SCN", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* PVN */
    if (local_config.register_pvn) {
        local_state.pvn_id = create_hypothalamus_node(
            kg, "PVN",
            (brain_kg_node_type_t)HYPOTHALAMUS_KG_NODE_PVN,
            "Paraventricular nucleus - CRH release, autonomic control, HPA axis",
            admin_token
        );
        if (local_state.pvn_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_hypothalamus_edge(kg, local_state.root_id, local_state.pvn_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains PVN", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* VMH */
    if (local_config.register_vmh) {
        local_state.vmh_id = create_hypothalamus_node(
            kg, "VMH",
            (brain_kg_node_type_t)HYPOTHALAMUS_KG_NODE_VMH,
            "Ventromedial hypothalamus - satiety center, defensive behavior",
            admin_token
        );
        if (local_state.vmh_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_hypothalamus_edge(kg, local_state.root_id, local_state.vmh_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains VMH", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* LH */
    if (local_config.register_lh) {
        local_state.lh_id = create_hypothalamus_node(
            kg, "LH",
            (brain_kg_node_type_t)HYPOTHALAMUS_KG_NODE_LH,
            "Lateral hypothalamus - hunger center, arousal, reward seeking",
            admin_token
        );
        if (local_state.lh_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_hypothalamus_edge(kg, local_state.root_id, local_state.lh_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains LH", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Homeostasis */
    if (local_config.register_homeostasis) {
        local_state.homeostasis_id = create_hypothalamus_node(
            kg, "homeostasis",
            (brain_kg_node_type_t)HYPOTHALAMUS_KG_NODE_HOMEOSTASIS,
            "Homeostasis - temperature, osmolality, energy balance regulation",
            admin_token
        );
        if (local_state.homeostasis_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_hypothalamus_edge(kg, local_state.root_id, local_state.homeostasis_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains homeostasis", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Circadian rhythm */
    if (local_config.register_circadian_rhythm) {
        local_state.circadian_rhythm_id = create_hypothalamus_node(
            kg, "circadian_rhythm",
            (brain_kg_node_type_t)HYPOTHALAMUS_KG_NODE_CIRCADIAN,
            "Circadian rhythm - sleep-wake cycle, hormonal oscillation, photoperiod",
            admin_token
        );
        if (local_state.circadian_rhythm_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_hypothalamus_edge(kg, local_state.root_id, local_state.circadian_rhythm_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains circadian rhythm", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Stress response */
    if (local_config.register_stress_response) {
        local_state.stress_response_id = create_hypothalamus_node(
            kg, "stress_response",
            (brain_kg_node_type_t)HYPOTHALAMUS_KG_NODE_STRESS,
            "Stress response - HPA axis activation, cortisol regulation, fight-or-flight",
            admin_token
        );
        if (local_state.stress_response_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_hypothalamus_edge(kg, local_state.root_id, local_state.stress_response_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains stress response", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Cross edges */
    if (local_config.register_cross_edges) {
        /* SCN entrains circadian rhythm */
        if (local_state.scn_id != BRAIN_KG_INVALID_NODE &&
            local_state.circadian_rhythm_id != BRAIN_KG_INVALID_NODE) {
            create_hypothalamus_edge(kg, local_state.scn_id,
                local_state.circadian_rhythm_id,
                (brain_kg_edge_type_t)HYPOTHALAMUS_KG_EDGE_ENTRAINS,
                "SCN entrains circadian rhythm", 0.95f, admin_token);
            local_state.edge_count++;
        }
        /* PVN activates HPA stress response */
        if (local_state.pvn_id != BRAIN_KG_INVALID_NODE &&
            local_state.stress_response_id != BRAIN_KG_INVALID_NODE) {
            create_hypothalamus_edge(kg, local_state.pvn_id,
                local_state.stress_response_id,
                (brain_kg_edge_type_t)HYPOTHALAMUS_KG_EDGE_ACTIVATES_HPA,
                "PVN activates HPA axis stress response", 0.9f, admin_token);
            local_state.edge_count++;
        }
        /* VMH regulates homeostasis */
        if (local_state.vmh_id != BRAIN_KG_INVALID_NODE &&
            local_state.homeostasis_id != BRAIN_KG_INVALID_NODE) {
            create_hypothalamus_edge(kg, local_state.vmh_id,
                local_state.homeostasis_id,
                (brain_kg_edge_type_t)HYPOTHALAMUS_KG_EDGE_REGULATES,
                "VMH regulates satiety and energy balance", 0.85f, admin_token);
            local_state.edge_count++;
        }
        /* LH drives feeding and arousal */
        if (local_state.lh_id != BRAIN_KG_INVALID_NODE &&
            local_state.homeostasis_id != BRAIN_KG_INVALID_NODE) {
            create_hypothalamus_edge(kg, local_state.lh_id,
                local_state.homeostasis_id,
                (brain_kg_edge_type_t)HYPOTHALAMUS_KG_EDGE_DRIVES,
                "LH drives hunger and arousal signaling", 0.85f, admin_token);
            local_state.edge_count++;
        }
        /* Circadian modulates homeostasis */
        if (local_state.circadian_rhythm_id != BRAIN_KG_INVALID_NODE &&
            local_state.homeostasis_id != BRAIN_KG_INVALID_NODE) {
            create_hypothalamus_edge(kg, local_state.circadian_rhythm_id,
                local_state.homeostasis_id,
                BRAIN_KG_EDGE_MODULATES,
                "circadian rhythm modulates homeostatic setpoints", 0.8f, admin_token);
            local_state.edge_count++;
        }
    }

    local_state.registered = true;
    if (state) *state = local_state;

    NIMCP_LOG_INFO(HYPOTHALAMUS_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t hypothalamus_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, HYPOTHALAMUS_KG_ROOT_NAME);
}

brain_kg_node_id_t hypothalamus_kg_find_subsystem(brain_kg_t* kg, const char* name) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

int hypothalamus_kg_unregister_all(
    brain_kg_t* kg,
    hypothalamus_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(HYPOTHALAMUS_KG_MODULE_NAME, "Unregistered Hypothalamus KG nodes");
    return 0;
}

//=============================================================================
// Runtime Event Emission (Wave W2)
//=============================================================================

/*
 * HOT-PATH INTEGRATION CANDIDATE: The hypothalamus has tick-style files
 * (nimcp_hypothalamus_drives.c, nimcp_hypothalamus_homeostasis.c,
 * nimcp_hypothalamus_orchestrator.c). Those call sites should invoke
 * hypothalamus_kg_emit_event(brain, "drive_change", delta, ts_us) when a
 * drive state crosses a threshold. Not wired in W2 (scope: bridge files only).
 */

void hypothalamus_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg || !kind) {
        return;
    }

    brain_kg_t* kg = brain->internal_kg;
    uint64_t tok = brain->internal_kg_admin_token;
    if (brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, tok) != 0) {
        return;
    }

    char name[128];
    char desc[192];
    snprintf(name, sizeof(name),
             "hypothalamus_event_%s_%" PRIu64, kind, ts_us);
    snprintf(desc, sizeof(desc),
             "hypothalamus runtime event: kind=%s intensity=%.3f ts_us=%" PRIu64,
             kind, (double)intensity, ts_us);

    brain_kg_node_id_t evt_id = brain_kg_add_node(
        kg, name, BRAIN_KG_NODE_CUSTOM, desc
    );

    if (evt_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.4f", (double)intensity);
        brain_kg_add_metadata(kg, evt_id, "intensity", val_str);
        brain_kg_add_metadata(kg, evt_id, "kind", kind);

        brain_kg_node_id_t root = brain_kg_find_node(kg, HYPOTHALAMUS_KG_ROOT_NAME);
        if (root != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(
                kg, evt_id, root,
                BRAIN_KG_EDGE_SENDS_TO,
                "produced_by",
                (intensity < 0.0f ? -intensity : intensity)
            );
        }
    }

    (void)brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}
