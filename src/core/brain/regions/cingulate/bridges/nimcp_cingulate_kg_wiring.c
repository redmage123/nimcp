//=============================================================================
// nimcp_cingulate_kg_wiring.c - Cingulate Cortex Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_cingulate_kg_wiring.c
 * @brief Implementation of Cingulate Cortex Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for Cingulate Cortex module
 * WHY:  Enables semantic queries about conflict monitoring and error detection
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/cingulate/bridges/nimcp_cingulate_kg_wiring.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(cingulate_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Helper Functions
//=============================================================================

static brain_kg_node_id_t create_cingulate_node(
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
        NIMCP_LOG_DEBUG(CINGULATE_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

static brain_kg_edge_id_t create_cingulate_edge(
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

int cingulate_kg_default_config(cingulate_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_kg_default_config: config is NULL");
        return -1;
    }

    config->register_dacc = true;
    config->register_vacc = true;
    config->register_pcc = true;
    config->register_conflict_detection = true;
    config->register_error_monitoring = true;
    config->register_reward_processing = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int cingulate_kg_register_all(
    brain_kg_t* kg,
    const cingulate_kg_config_t* config,
    cingulate_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_kg_register_all: kg is NULL");
        return -1;
    }

    cingulate_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        cingulate_kg_default_config(&local_config);
    }

    cingulate_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create root node */
    local_state.root_id = create_cingulate_node(
        kg, CINGULATE_KG_ROOT_NAME,
        BRAIN_KG_NODE_CORTICAL,
        "Cingulate cortex - conflict monitoring, error detection, reward evaluation",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(CINGULATE_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cingulate_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* dACC */
    if (local_config.register_dacc) {
        local_state.dacc_id = create_cingulate_node(
            kg, "dACC",
            (brain_kg_node_type_t)CINGULATE_KG_NODE_DORSAL_ACC,
            "Dorsal ACC - cognitive conflict monitoring, response selection",
            admin_token
        );
        if (local_state.dacc_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_cingulate_edge(kg, local_state.root_id, local_state.dacc_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains dACC", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* vACC */
    if (local_config.register_vacc) {
        local_state.vacc_id = create_cingulate_node(
            kg, "vACC",
            (brain_kg_node_type_t)CINGULATE_KG_NODE_VENTRAL_ACC,
            "Ventral ACC - emotional regulation, autonomic control",
            admin_token
        );
        if (local_state.vacc_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_cingulate_edge(kg, local_state.root_id, local_state.vacc_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains vACC", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* PCC */
    if (local_config.register_pcc) {
        local_state.pcc_id = create_cingulate_node(
            kg, "PCC",
            (brain_kg_node_type_t)CINGULATE_KG_NODE_PCC,
            "Posterior cingulate - self-referential processing, default mode network",
            admin_token
        );
        if (local_state.pcc_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_cingulate_edge(kg, local_state.root_id, local_state.pcc_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains PCC", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Conflict detection */
    if (local_config.register_conflict_detection) {
        local_state.conflict_detection_id = create_cingulate_node(
            kg, "conflict_detection",
            (brain_kg_node_type_t)CINGULATE_KG_NODE_CONFLICT,
            "Conflict detection - response conflict monitoring, cognitive control signaling",
            admin_token
        );
        if (local_state.conflict_detection_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_cingulate_edge(kg, local_state.root_id, local_state.conflict_detection_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains conflict detection", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Error monitoring */
    if (local_config.register_error_monitoring) {
        local_state.error_monitoring_id = create_cingulate_node(
            kg, "error_monitoring",
            (brain_kg_node_type_t)CINGULATE_KG_NODE_ERROR_MONITOR,
            "Error monitoring - error-related negativity, performance adjustment",
            admin_token
        );
        if (local_state.error_monitoring_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_cingulate_edge(kg, local_state.root_id, local_state.error_monitoring_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains error monitoring", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Reward processing */
    if (local_config.register_reward_processing) {
        local_state.reward_processing_id = create_cingulate_node(
            kg, "reward_processing",
            (brain_kg_node_type_t)CINGULATE_KG_NODE_REWARD,
            "Reward processing - outcome evaluation, reward-based decision making",
            admin_token
        );
        if (local_state.reward_processing_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_cingulate_edge(kg, local_state.root_id, local_state.reward_processing_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains reward processing", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Cross edges */
    if (local_config.register_cross_edges) {
        /* dACC drives conflict detection */
        if (local_state.dacc_id != BRAIN_KG_INVALID_NODE &&
            local_state.conflict_detection_id != BRAIN_KG_INVALID_NODE) {
            create_cingulate_edge(kg, local_state.dacc_id,
                local_state.conflict_detection_id,
                (brain_kg_edge_type_t)CINGULATE_KG_EDGE_DETECTS,
                "dACC detects response conflict", 0.9f, admin_token);
            local_state.edge_count++;
        }
        /* dACC monitors errors */
        if (local_state.dacc_id != BRAIN_KG_INVALID_NODE &&
            local_state.error_monitoring_id != BRAIN_KG_INVALID_NODE) {
            create_cingulate_edge(kg, local_state.dacc_id,
                local_state.error_monitoring_id,
                (brain_kg_edge_type_t)CINGULATE_KG_EDGE_MONITORS,
                "dACC monitors performance errors", 0.85f, admin_token);
            local_state.edge_count++;
        }
        /* vACC evaluates reward */
        if (local_state.vacc_id != BRAIN_KG_INVALID_NODE &&
            local_state.reward_processing_id != BRAIN_KG_INVALID_NODE) {
            create_cingulate_edge(kg, local_state.vacc_id,
                local_state.reward_processing_id,
                (brain_kg_edge_type_t)CINGULATE_KG_EDGE_EVALUATES,
                "vACC evaluates reward outcomes", 0.85f, admin_token);
            local_state.edge_count++;
        }
        /* Error monitoring signals conflict detection */
        if (local_state.error_monitoring_id != BRAIN_KG_INVALID_NODE &&
            local_state.conflict_detection_id != BRAIN_KG_INVALID_NODE) {
            create_cingulate_edge(kg, local_state.error_monitoring_id,
                local_state.conflict_detection_id,
                (brain_kg_edge_type_t)CINGULATE_KG_EDGE_SIGNALS_ERROR,
                "errors increase conflict sensitivity", 0.8f, admin_token);
            local_state.edge_count++;
        }
    }

    local_state.registered = true;
    if (state) *state = local_state;

    NIMCP_LOG_INFO(CINGULATE_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t cingulate_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, CINGULATE_KG_ROOT_NAME);
}

brain_kg_node_id_t cingulate_kg_find_subsystem(brain_kg_t* kg, const char* name) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

int cingulate_kg_unregister_all(
    brain_kg_t* kg,
    cingulate_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(CINGULATE_KG_MODULE_NAME, "Unregistered Cingulate KG nodes");
    return 0;
}

//=============================================================================
// Runtime Event Emission (Wave W2)
//=============================================================================

/*
 * HOT-PATH GAP: Cingulate currently has only an adapter + bridges — no
 * standalone tick .c file emitting conflict/error signals. Callers detecting
 * response conflict should invoke cingulate_kg_emit_event(.., "conflict", ..).
 */

void cingulate_kg_emit_event(
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
             "cingulate_event_%s_%" PRIu64, kind, ts_us);
    snprintf(desc, sizeof(desc),
             "cingulate runtime event: kind=%s intensity=%.3f ts_us=%" PRIu64,
             kind, (double)intensity, ts_us);

    brain_kg_node_id_t evt_id = brain_kg_add_node(
        kg, name, BRAIN_KG_NODE_CUSTOM, desc
    );

    if (evt_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.4f", (double)intensity);
        brain_kg_add_metadata(kg, evt_id, "intensity", val_str);
        brain_kg_add_metadata(kg, evt_id, "kind", kind);

        brain_kg_node_id_t root = brain_kg_find_node(kg, CINGULATE_KG_ROOT_NAME);
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
