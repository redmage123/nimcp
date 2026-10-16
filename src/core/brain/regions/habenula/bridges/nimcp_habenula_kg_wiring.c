//=============================================================================
// nimcp_habenula_kg_wiring.c - Habenula Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_habenula_kg_wiring.c
 * @brief Implementation of Habenula Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for Habenula module
 * WHY:  Enables semantic queries about reward evaluation and punishment
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/habenula/bridges/nimcp_habenula_kg_wiring.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(habenula_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Helper Functions
//=============================================================================

static brain_kg_node_id_t create_habenula_node(
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
        NIMCP_LOG_DEBUG(HABENULA_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

static brain_kg_edge_id_t create_habenula_edge(
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

int habenula_kg_default_config(habenula_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "habenula_kg_default_config: config is NULL");
        return -1;
    }

    config->register_lateral_habenula = true;
    config->register_medial_habenula = true;
    config->register_reward_evaluation = true;
    config->register_punishment_signal = true;
    config->register_mood_regulation = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int habenula_kg_register_all(
    brain_kg_t* kg,
    const habenula_kg_config_t* config,
    habenula_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "habenula_kg_register_all: kg is NULL");
        return -1;
    }

    habenula_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        habenula_kg_default_config(&local_config);
    }

    habenula_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create root node */
    local_state.root_id = create_habenula_node(
        kg, HABENULA_KG_ROOT_NAME,
        BRAIN_KG_NODE_SUBCORTICAL,
        "Habenula - reward evaluation, punishment signaling, mood regulation",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(HABENULA_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habenula_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Lateral habenula */
    if (local_config.register_lateral_habenula) {
        local_state.lateral_habenula_id = create_habenula_node(
            kg, "lateral_habenula",
            (brain_kg_node_type_t)HABENULA_KG_NODE_LATERAL,
            "Lateral habenula - negative reward signal, DA/5-HT suppression, aversion",
            admin_token
        );
        if (local_state.lateral_habenula_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_habenula_edge(kg, local_state.root_id, local_state.lateral_habenula_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains lateral habenula", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Medial habenula */
    if (local_config.register_medial_habenula) {
        local_state.medial_habenula_id = create_habenula_node(
            kg, "medial_habenula",
            (brain_kg_node_type_t)HABENULA_KG_NODE_MEDIAL,
            "Medial habenula - cholinergic signaling, interpeduncular nucleus relay",
            admin_token
        );
        if (local_state.medial_habenula_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_habenula_edge(kg, local_state.root_id, local_state.medial_habenula_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains medial habenula", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Reward evaluation */
    if (local_config.register_reward_evaluation) {
        local_state.reward_evaluation_id = create_habenula_node(
            kg, "reward_evaluation",
            (brain_kg_node_type_t)HABENULA_KG_NODE_REWARD_EVAL,
            "Reward evaluation - outcome comparison, disappointment encoding",
            admin_token
        );
        if (local_state.reward_evaluation_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_habenula_edge(kg, local_state.root_id, local_state.reward_evaluation_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains reward evaluation", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Punishment signal */
    if (local_config.register_punishment_signal) {
        local_state.punishment_signal_id = create_habenula_node(
            kg, "punishment_signal",
            (brain_kg_node_type_t)HABENULA_KG_NODE_PUNISHMENT,
            "Punishment signal - aversive outcome encoding, avoidance learning",
            admin_token
        );
        if (local_state.punishment_signal_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_habenula_edge(kg, local_state.root_id, local_state.punishment_signal_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains punishment signal", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Mood regulation */
    if (local_config.register_mood_regulation) {
        local_state.mood_regulation_id = create_habenula_node(
            kg, "mood_regulation",
            (brain_kg_node_type_t)HABENULA_KG_NODE_MOOD,
            "Mood regulation - serotonin modulation, depressive circuit control",
            admin_token
        );
        if (local_state.mood_regulation_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_habenula_edge(kg, local_state.root_id, local_state.mood_regulation_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains mood regulation", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Cross edges */
    if (local_config.register_cross_edges) {
        /* Lateral habenula evaluates reward */
        if (local_state.lateral_habenula_id != BRAIN_KG_INVALID_NODE &&
            local_state.reward_evaluation_id != BRAIN_KG_INVALID_NODE) {
            create_habenula_edge(kg, local_state.lateral_habenula_id,
                local_state.reward_evaluation_id,
                (brain_kg_edge_type_t)HABENULA_KG_EDGE_EVALUATES,
                "lateral habenula evaluates negative reward prediction errors", 0.9f, admin_token);
            local_state.edge_count++;
        }
        /* Reward evaluation signals punishment */
        if (local_state.reward_evaluation_id != BRAIN_KG_INVALID_NODE &&
            local_state.punishment_signal_id != BRAIN_KG_INVALID_NODE) {
            create_habenula_edge(kg, local_state.reward_evaluation_id,
                local_state.punishment_signal_id,
                (brain_kg_edge_type_t)HABENULA_KG_EDGE_SIGNALS_PUNISHMENT,
                "negative evaluation triggers punishment signal", 0.85f, admin_token);
            local_state.edge_count++;
        }
        /* Lateral habenula suppresses DA (regulates mood) */
        if (local_state.lateral_habenula_id != BRAIN_KG_INVALID_NODE &&
            local_state.mood_regulation_id != BRAIN_KG_INVALID_NODE) {
            create_habenula_edge(kg, local_state.lateral_habenula_id,
                local_state.mood_regulation_id,
                (brain_kg_edge_type_t)HABENULA_KG_EDGE_REGULATES_MOOD,
                "LHb hyperactivity linked to mood dysregulation", 0.85f, admin_token);
            local_state.edge_count++;
        }
        /* Medial habenula contributes to mood */
        if (local_state.medial_habenula_id != BRAIN_KG_INVALID_NODE &&
            local_state.mood_regulation_id != BRAIN_KG_INVALID_NODE) {
            create_habenula_edge(kg, local_state.medial_habenula_id,
                local_state.mood_regulation_id,
                BRAIN_KG_EDGE_MODULATES,
                "medial habenula modulates mood via cholinergic pathway", 0.75f, admin_token);
            local_state.edge_count++;
        }
    }

    local_state.registered = true;
    if (state) *state = local_state;

    NIMCP_LOG_INFO(HABENULA_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t habenula_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, HABENULA_KG_ROOT_NAME);
}

brain_kg_node_id_t habenula_kg_find_subsystem(brain_kg_t* kg, const char* name) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

int habenula_kg_unregister_all(
    brain_kg_t* kg,
    habenula_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "habenula_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(HABENULA_KG_MODULE_NAME, "Unregistered Habenula KG nodes");
    return 0;
}

//=============================================================================
// Runtime Event Emission (Wave W2)
//=============================================================================

/*
 * HOT-PATH GAP: Habenula has no dedicated tick .c file. Negative RPE
 * signals (lateral habenula bursts) should invoke habenula_kg_emit_event.
 */

void habenula_kg_emit_event(
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
             "habenula_event_%s_%" PRIu64, kind, ts_us);
    snprintf(desc, sizeof(desc),
             "habenula runtime event: kind=%s intensity=%.3f ts_us=%" PRIu64,
             kind, (double)intensity, ts_us);

    brain_kg_node_id_t evt_id = brain_kg_add_node(
        kg, name, BRAIN_KG_NODE_CUSTOM, desc
    );

    if (evt_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.4f", (double)intensity);
        brain_kg_add_metadata(kg, evt_id, "intensity", val_str);
        brain_kg_add_metadata(kg, evt_id, "kind", kind);

        brain_kg_node_id_t root = brain_kg_find_node(kg, HABENULA_KG_ROOT_NAME);
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
