//=============================================================================
// nimcp_lc_kg_wiring.c - Locus Coeruleus Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_lc_kg_wiring.c
 * @brief Implementation of Locus Coeruleus Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for Locus Coeruleus module
 * WHY:  Enables semantic queries about arousal and attention modulation
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/locus_coeruleus/bridges/nimcp_lc_kg_wiring.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(lc_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Helper Functions
//=============================================================================

static brain_kg_node_id_t create_lc_node(
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
        NIMCP_LOG_DEBUG(LC_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

static brain_kg_edge_id_t create_lc_edge(
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

int lc_kg_default_config(lc_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_kg_default_config: config is NULL");
        return -1;
    }

    config->register_noradrenergic_neurons = true;
    config->register_arousal_regulation = true;
    config->register_attention_modulation = true;
    config->register_stress_response = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int lc_kg_register_all(
    brain_kg_t* kg,
    const lc_kg_config_t* config,
    lc_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_kg_register_all: kg is NULL");
        return -1;
    }

    lc_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        lc_kg_default_config(&local_config);
    }

    lc_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create root node */
    local_state.root_id = create_lc_node(
        kg, LC_KG_ROOT_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Locus Coeruleus - norepinephrine release, arousal regulation, attention modulation",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(LC_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lc_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Noradrenergic neurons */
    if (local_config.register_noradrenergic_neurons) {
        local_state.noradrenergic_neurons_id = create_lc_node(
            kg, "noradrenergic_neurons",
            (brain_kg_node_type_t)LC_KG_NODE_NORADRENERGIC,
            "Noradrenergic neurons - NE release to cortex, thalamus, hippocampus",
            admin_token
        );
        if (local_state.noradrenergic_neurons_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_lc_edge(kg, local_state.root_id, local_state.noradrenergic_neurons_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains noradrenergic neurons", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Arousal regulation */
    if (local_config.register_arousal_regulation) {
        local_state.arousal_regulation_id = create_lc_node(
            kg, "arousal_regulation",
            (brain_kg_node_type_t)LC_KG_NODE_AROUSAL,
            "Arousal regulation - wakefulness, alertness, tonic/phasic modes",
            admin_token
        );
        if (local_state.arousal_regulation_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_lc_edge(kg, local_state.root_id, local_state.arousal_regulation_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains arousal regulation", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Attention modulation */
    if (local_config.register_attention_modulation) {
        local_state.attention_modulation_id = create_lc_node(
            kg, "attention_modulation",
            (brain_kg_node_type_t)LC_KG_NODE_ATTENTION_MOD,
            "Attention modulation - gain control, signal-to-noise ratio, task engagement",
            admin_token
        );
        if (local_state.attention_modulation_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_lc_edge(kg, local_state.root_id, local_state.attention_modulation_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains attention modulation", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Stress response */
    if (local_config.register_stress_response) {
        local_state.stress_response_id = create_lc_node(
            kg, "lc_stress_response",
            (brain_kg_node_type_t)LC_KG_NODE_STRESS,
            "Stress response - CRF-driven activation, fight-or-flight, vigilance",
            admin_token
        );
        if (local_state.stress_response_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_lc_edge(kg, local_state.root_id, local_state.stress_response_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains stress response", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Cross edges */
    if (local_config.register_cross_edges) {
        /* NE neurons drive arousal */
        if (local_state.noradrenergic_neurons_id != BRAIN_KG_INVALID_NODE &&
            local_state.arousal_regulation_id != BRAIN_KG_INVALID_NODE) {
            create_lc_edge(kg, local_state.noradrenergic_neurons_id,
                local_state.arousal_regulation_id,
                (brain_kg_edge_type_t)LC_KG_EDGE_AROUSES,
                "NE release drives cortical arousal", 0.95f, admin_token);
            local_state.edge_count++;
        }
        /* NE neurons modulate attention */
        if (local_state.noradrenergic_neurons_id != BRAIN_KG_INVALID_NODE &&
            local_state.attention_modulation_id != BRAIN_KG_INVALID_NODE) {
            create_lc_edge(kg, local_state.noradrenergic_neurons_id,
                local_state.attention_modulation_id,
                (brain_kg_edge_type_t)LC_KG_EDGE_MODULATES_ATTENTION,
                "NE modulates attentional gain control", 0.9f, admin_token);
            local_state.edge_count++;
        }
        /* Arousal modulates attention */
        if (local_state.arousal_regulation_id != BRAIN_KG_INVALID_NODE &&
            local_state.attention_modulation_id != BRAIN_KG_INVALID_NODE) {
            create_lc_edge(kg, local_state.arousal_regulation_id,
                local_state.attention_modulation_id,
                BRAIN_KG_EDGE_MODULATES,
                "arousal level shapes attention mode (tonic vs phasic)", 0.85f, admin_token);
            local_state.edge_count++;
        }
        /* Stress drives arousal */
        if (local_state.stress_response_id != BRAIN_KG_INVALID_NODE &&
            local_state.arousal_regulation_id != BRAIN_KG_INVALID_NODE) {
            create_lc_edge(kg, local_state.stress_response_id,
                local_state.arousal_regulation_id,
                (brain_kg_edge_type_t)LC_KG_EDGE_RESPONDS_STRESS,
                "stress CRF activation increases arousal", 0.9f, admin_token);
            local_state.edge_count++;
        }
    }

    local_state.registered = true;
    if (state) *state = local_state;

    NIMCP_LOG_INFO(LC_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t lc_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, LC_KG_ROOT_NAME);
}

brain_kg_node_id_t lc_kg_find_subsystem(brain_kg_t* kg, const char* name) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

int lc_kg_unregister_all(
    brain_kg_t* kg,
    lc_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lc_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(LC_KG_MODULE_NAME, "Unregistered LC KG nodes");
    return 0;
}

//=============================================================================
// Runtime Event Emission (Wave W2)
//=============================================================================

/*
 * HOT-PATH INTEGRATION CANDIDATE: Locus coeruleus has several tick-style
 * files (nimcp_arousal_modulation.c, nimcp_norepinephrine_release.c,
 * nimcp_novelty_detection.c). Those should call lc_kg_emit_event when
 * arousal crosses a phasic-burst threshold. Not wired in W2 scope.
 */

void lc_kg_emit_event(
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
             "locus_coeruleus_event_%s_%" PRIu64, kind, ts_us);
    snprintf(desc, sizeof(desc),
             "locus_coeruleus runtime event: kind=%s intensity=%.3f ts_us=%" PRIu64,
             kind, (double)intensity, ts_us);

    brain_kg_node_id_t evt_id = brain_kg_add_node(
        kg, name, BRAIN_KG_NODE_CUSTOM, desc
    );

    if (evt_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.4f", (double)intensity);
        brain_kg_add_metadata(kg, evt_id, "intensity", val_str);
        brain_kg_add_metadata(kg, evt_id, "kind", kind);

        brain_kg_node_id_t root = brain_kg_find_node(kg, LC_KG_ROOT_NAME);
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
