//=============================================================================
// nimcp_cerebellum_kg_wiring.c - Cerebellum Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_cerebellum_kg_wiring.c
 * @brief Implementation of Cerebellum Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for Cerebellum module
 * WHY:  Enables semantic queries about motor learning and timing
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/cerebellum/bridges/nimcp_cerebellum_kg_wiring.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(cerebellum_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Helper Functions
//=============================================================================

static brain_kg_node_id_t create_cerebellum_node(
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
        NIMCP_LOG_DEBUG(CEREBELLUM_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

static brain_kg_edge_id_t create_cerebellum_edge(
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

int cerebellum_kg_default_config(cerebellum_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_kg_default_config: config is NULL");
        return -1;
    }

    config->register_purkinje_cells = true;
    config->register_granule_cells = true;
    config->register_deep_nuclei = true;
    config->register_motor_learning = true;
    config->register_timing_control = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int cerebellum_kg_register_all(
    brain_kg_t* kg,
    const cerebellum_kg_config_t* config,
    cerebellum_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_kg_register_all: kg is NULL");
        return -1;
    }

    cerebellum_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        cerebellum_kg_default_config(&local_config);
    }

    cerebellum_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create root node */
    local_state.root_id = create_cerebellum_node(
        kg, CEREBELLUM_KG_ROOT_NAME,
        BRAIN_KG_NODE_SUBCORTICAL,
        "Cerebellum - motor learning, coordination, timing, error correction",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(CEREBELLUM_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cerebellum_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Purkinje cells */
    if (local_config.register_purkinje_cells) {
        local_state.purkinje_cells_id = create_cerebellum_node(
            kg, "purkinje_cells",
            (brain_kg_node_type_t)CEREBELLUM_KG_NODE_PURKINJE,
            "Purkinje cells - primary output neurons, inhibitory, error signals",
            admin_token
        );
        if (local_state.purkinje_cells_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_cerebellum_edge(kg, local_state.root_id, local_state.purkinje_cells_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains Purkinje cells", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Granule cells */
    if (local_config.register_granule_cells) {
        local_state.granule_cells_id = create_cerebellum_node(
            kg, "granule_cells",
            (brain_kg_node_type_t)CEREBELLUM_KG_NODE_GRANULE,
            "Granule cells - most numerous neurons, parallel fiber input expansion",
            admin_token
        );
        if (local_state.granule_cells_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_cerebellum_edge(kg, local_state.root_id, local_state.granule_cells_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains granule cells", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Deep nuclei */
    if (local_config.register_deep_nuclei) {
        local_state.deep_nuclei_id = create_cerebellum_node(
            kg, "deep_nuclei",
            (brain_kg_node_type_t)CEREBELLUM_KG_NODE_DEEP_NUCLEI,
            "Deep cerebellar nuclei - primary output, motor command modulation",
            admin_token
        );
        if (local_state.deep_nuclei_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_cerebellum_edge(kg, local_state.root_id, local_state.deep_nuclei_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains deep nuclei", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Motor learning */
    if (local_config.register_motor_learning) {
        local_state.motor_learning_id = create_cerebellum_node(
            kg, "motor_learning",
            (brain_kg_node_type_t)CEREBELLUM_KG_NODE_MOTOR_LEARNING,
            "Motor learning - adaptive control, error-driven plasticity",
            admin_token
        );
        if (local_state.motor_learning_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_cerebellum_edge(kg, local_state.root_id, local_state.motor_learning_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains motor learning", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Timing control */
    if (local_config.register_timing_control) {
        local_state.timing_control_id = create_cerebellum_node(
            kg, "timing_control",
            (brain_kg_node_type_t)CEREBELLUM_KG_NODE_TIMING,
            "Timing control - temporal precision, interval timing, rhythmic coordination",
            admin_token
        );
        if (local_state.timing_control_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_cerebellum_edge(kg, local_state.root_id, local_state.timing_control_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains timing control", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Cross edges */
    if (local_config.register_cross_edges) {
        /* Granule cells excite Purkinje cells via parallel fibers */
        if (local_state.granule_cells_id != BRAIN_KG_INVALID_NODE &&
            local_state.purkinje_cells_id != BRAIN_KG_INVALID_NODE) {
            create_cerebellum_edge(kg, local_state.granule_cells_id,
                local_state.purkinje_cells_id,
                (brain_kg_edge_type_t)CEREBELLUM_KG_EDGE_EXCITES_PURKINJE,
                "granule cells excite Purkinje via parallel fibers", 0.9f, admin_token);
            local_state.edge_count++;
        }
        /* Purkinje cells inhibit deep nuclei */
        if (local_state.purkinje_cells_id != BRAIN_KG_INVALID_NODE &&
            local_state.deep_nuclei_id != BRAIN_KG_INVALID_NODE) {
            create_cerebellum_edge(kg, local_state.purkinje_cells_id,
                local_state.deep_nuclei_id,
                (brain_kg_edge_type_t)CEREBELLUM_KG_EDGE_INHIBITS_DEEP,
                "Purkinje cells inhibit deep nuclei output", 0.9f, admin_token);
            local_state.edge_count++;
        }
        /* Motor learning refines motor output */
        if (local_state.motor_learning_id != BRAIN_KG_INVALID_NODE &&
            local_state.purkinje_cells_id != BRAIN_KG_INVALID_NODE) {
            create_cerebellum_edge(kg, local_state.motor_learning_id,
                local_state.purkinje_cells_id,
                (brain_kg_edge_type_t)CEREBELLUM_KG_EDGE_REFINES_MOTOR,
                "motor learning modifies Purkinje cell responses", 0.85f, admin_token);
            local_state.edge_count++;
        }
        /* Timing control coordinates via deep nuclei */
        if (local_state.timing_control_id != BRAIN_KG_INVALID_NODE &&
            local_state.deep_nuclei_id != BRAIN_KG_INVALID_NODE) {
            create_cerebellum_edge(kg, local_state.timing_control_id,
                local_state.deep_nuclei_id,
                (brain_kg_edge_type_t)CEREBELLUM_KG_EDGE_COORDINATES_TIMING,
                "timing control coordinates deep nuclei output", 0.8f, admin_token);
            local_state.edge_count++;
        }
    }

    local_state.registered = true;
    if (state) *state = local_state;

    NIMCP_LOG_INFO(CEREBELLUM_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t cerebellum_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, CEREBELLUM_KG_ROOT_NAME);
}

brain_kg_node_id_t cerebellum_kg_find_subsystem(brain_kg_t* kg, const char* name) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

int cerebellum_kg_unregister_all(
    brain_kg_t* kg,
    cerebellum_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(CEREBELLUM_KG_MODULE_NAME, "Unregistered Cerebellum KG nodes");
    return 0;
}
