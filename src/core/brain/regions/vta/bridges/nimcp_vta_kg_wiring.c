//=============================================================================
// nimcp_vta_kg_wiring.c - VTA Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_vta_kg_wiring.c
 * @brief Implementation of VTA Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for VTA module
 * WHY:  Enables semantic queries about reward, motivation, and reinforcement
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/vta/bridges/nimcp_vta_kg_wiring.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(vta_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Helper Functions
//=============================================================================

static brain_kg_node_id_t create_vta_node(
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
        NIMCP_LOG_DEBUG(VTA_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

static brain_kg_edge_id_t create_vta_edge(
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

int vta_kg_default_config(vta_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_kg_default_config: config is NULL");
        return -1;
    }

    config->register_dopaminergic_neurons = true;
    config->register_reward_prediction = true;
    config->register_motivation_drive = true;
    config->register_reinforcement_learning = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int vta_kg_register_all(
    brain_kg_t* kg,
    const vta_kg_config_t* config,
    vta_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_kg_register_all: kg is NULL");
        return -1;
    }

    vta_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        vta_kg_default_config(&local_config);
    }

    vta_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create root node */
    local_state.root_id = create_vta_node(
        kg, VTA_KG_ROOT_NAME,
        BRAIN_KG_NODE_SUBCORTICAL,
        "Ventral Tegmental Area - dopamine release, reward prediction, motivation",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(VTA_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vta_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Dopaminergic neurons */
    if (local_config.register_dopaminergic_neurons) {
        local_state.dopaminergic_neurons_id = create_vta_node(
            kg, "dopaminergic_neurons",
            (brain_kg_node_type_t)VTA_KG_NODE_DOPAMINERGIC,
            "Dopaminergic neurons - DA release to NAc and PFC, reward signaling",
            admin_token
        );
        if (local_state.dopaminergic_neurons_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_vta_edge(kg, local_state.root_id, local_state.dopaminergic_neurons_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains dopaminergic neurons", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Reward prediction */
    if (local_config.register_reward_prediction) {
        local_state.reward_prediction_id = create_vta_node(
            kg, "reward_prediction",
            (brain_kg_node_type_t)VTA_KG_NODE_REWARD_PREDICTION,
            "Reward prediction - reward prediction error, expected value computation",
            admin_token
        );
        if (local_state.reward_prediction_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_vta_edge(kg, local_state.root_id, local_state.reward_prediction_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains reward prediction", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Motivation drive */
    if (local_config.register_motivation_drive) {
        local_state.motivation_drive_id = create_vta_node(
            kg, "motivation_drive",
            (brain_kg_node_type_t)VTA_KG_NODE_MOTIVATION,
            "Motivation drive - incentive salience, wanting, approach behavior",
            admin_token
        );
        if (local_state.motivation_drive_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_vta_edge(kg, local_state.root_id, local_state.motivation_drive_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains motivation drive", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Reinforcement learning */
    if (local_config.register_reinforcement_learning) {
        local_state.reinforcement_learning_id = create_vta_node(
            kg, "reinforcement_learning",
            (brain_kg_node_type_t)VTA_KG_NODE_REINFORCEMENT,
            "Reinforcement learning - TD learning, policy update, action-value mapping",
            admin_token
        );
        if (local_state.reinforcement_learning_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_vta_edge(kg, local_state.root_id, local_state.reinforcement_learning_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains reinforcement learning", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Cross edges */
    if (local_config.register_cross_edges) {
        /* Dopaminergic neurons release DA for reward prediction */
        if (local_state.dopaminergic_neurons_id != BRAIN_KG_INVALID_NODE &&
            local_state.reward_prediction_id != BRAIN_KG_INVALID_NODE) {
            create_vta_edge(kg, local_state.dopaminergic_neurons_id,
                local_state.reward_prediction_id,
                (brain_kg_edge_type_t)VTA_KG_EDGE_RELEASES_DA,
                "DA neurons encode reward prediction error", 0.95f, admin_token);
            local_state.edge_count++;
        }
        /* Reward prediction drives motivation */
        if (local_state.reward_prediction_id != BRAIN_KG_INVALID_NODE &&
            local_state.motivation_drive_id != BRAIN_KG_INVALID_NODE) {
            create_vta_edge(kg, local_state.reward_prediction_id,
                local_state.motivation_drive_id,
                (brain_kg_edge_type_t)VTA_KG_EDGE_DRIVES_MOTIVATION,
                "reward prediction drives motivational salience", 0.9f, admin_token);
            local_state.edge_count++;
        }
        /* Reward prediction error reinforces learning */
        if (local_state.reward_prediction_id != BRAIN_KG_INVALID_NODE &&
            local_state.reinforcement_learning_id != BRAIN_KG_INVALID_NODE) {
            create_vta_edge(kg, local_state.reward_prediction_id,
                local_state.reinforcement_learning_id,
                (brain_kg_edge_type_t)VTA_KG_EDGE_REINFORCES,
                "RPE signal reinforces action-value learning", 0.9f, admin_token);
            local_state.edge_count++;
        }
        /* Motivation modulates reinforcement learning */
        if (local_state.motivation_drive_id != BRAIN_KG_INVALID_NODE &&
            local_state.reinforcement_learning_id != BRAIN_KG_INVALID_NODE) {
            create_vta_edge(kg, local_state.motivation_drive_id,
                local_state.reinforcement_learning_id,
                BRAIN_KG_EDGE_MODULATES,
                "motivation level modulates learning rate", 0.8f, admin_token);
            local_state.edge_count++;
        }
    }

    local_state.registered = true;
    if (state) *state = local_state;

    NIMCP_LOG_INFO(VTA_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t vta_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, VTA_KG_ROOT_NAME);
}

brain_kg_node_id_t vta_kg_find_subsystem(brain_kg_t* kg, const char* name) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

int vta_kg_unregister_all(
    brain_kg_t* kg,
    vta_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vta_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(VTA_KG_MODULE_NAME, "Unregistered VTA KG nodes");
    return 0;
}

//=============================================================================
// Runtime Event Emission (Wave W2)
//=============================================================================

/*
 * HOT-PATH INTEGRATION CANDIDATE: VTA has dedicated tick-style files
 * (nimcp_reward_prediction_error.c, nimcp_dopamine_release.c,
 * nimcp_incentive_salience.c). Callers computing RPE should invoke
 * vta_kg_emit_event(brain, "dopamine_rpe", rpe, ts_us). Not wired in W2.
 */

void vta_kg_emit_event(
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
             "vta_event_%s_%" PRIu64, kind, ts_us);
    snprintf(desc, sizeof(desc),
             "vta runtime event: kind=%s intensity=%.3f ts_us=%" PRIu64,
             kind, (double)intensity, ts_us);

    brain_kg_node_id_t evt_id = brain_kg_add_node(
        kg, name, BRAIN_KG_NODE_CUSTOM, desc
    );

    if (evt_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.4f", (double)intensity);
        brain_kg_add_metadata(kg, evt_id, "intensity", val_str);
        brain_kg_add_metadata(kg, evt_id, "kind", kind);

        brain_kg_node_id_t root = brain_kg_find_node(kg, VTA_KG_ROOT_NAME);
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
