//=============================================================================
// nimcp_hippocampus_kg_wiring.c - Hippocampus Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_hippocampus_kg_wiring.c
 * @brief Implementation of Hippocampus Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for Hippocampus module
 * WHY:  Enables semantic queries about memory and spatial navigation
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/hippocampus/bridges/nimcp_hippocampus_kg_wiring.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hippocampus_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create a hippocampus node with description
 *
 * WHAT: Helper to create a single KG node
 * WHY:  Reduces boilerplate in registration functions
 * HOW:  Wraps brain_kg_add_node with logging
 */
static brain_kg_node_id_t create_hippocampus_node(
    brain_kg_t* kg,
    const char* name,
    brain_kg_node_type_t type,
    const char* description,
    uint64_t admin_token
) {
    (void)admin_token;  /* Reserved for future access control */
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;

    brain_kg_node_id_t id = brain_kg_add_node(kg, name, type, description);
    if (id != BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_DEBUG(HIPPOCAMPUS_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

/**
 * @brief Create an edge between hippocampus nodes
 *
 * WHAT: Helper to create a single KG edge
 * WHY:  Reduces boilerplate in registration functions
 * HOW:  Wraps brain_kg_add_edge with validation
 */
static brain_kg_edge_id_t create_hippocampus_edge(
    brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    brain_kg_edge_type_t type,
    const char* description,
    float weight,
    uint64_t admin_token
) {
    (void)admin_token;  /* Reserved for future access control */
    if (!kg) return BRAIN_KG_INVALID_NODE;
    if (from == BRAIN_KG_INVALID_NODE || to == BRAIN_KG_INVALID_NODE) {
        return BRAIN_KG_INVALID_NODE;
    }

    return brain_kg_add_edge(kg, from, to, type, description, weight);
}

//=============================================================================
// Configuration API
//=============================================================================

int hippocampus_kg_default_config(hippocampus_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_kg_default_config: config is NULL");
        return -1;
    }

    config->register_ca1 = true;
    config->register_ca3 = true;
    config->register_dg = true;
    config->register_subiculum = true;
    config->register_memory_nodes = true;
    config->register_nav_nodes = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;
    config->register_theta_edges = true;

    return 0;
}

//=============================================================================
// Registration API - Main Entry Point
//=============================================================================

int hippocampus_kg_register_all(
    brain_kg_t* kg,
    const hippocampus_kg_config_t* config,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_kg_register_all: kg is NULL");
        return -1;
    }

    /* Use provided config or defaults */
    hippocampus_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        hippocampus_kg_default_config(&local_config);
    }

    /* Initialize local state */
    hippocampus_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create hippocampus root node */
    local_state.root_id = create_hippocampus_node(
        kg, HIPPOCAMPUS_KG_ROOT_NAME,
        BRAIN_KG_NODE_SUBCORTICAL,
        "Hippocampus - memory formation, spatial navigation, pattern processing",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(HIPPOCAMPUS_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippocampus_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Register subfields */
    if (hippocampus_kg_register_subfields(kg, local_state.root_id, &local_config,
                                          &local_state, admin_token) < 0) {
        NIMCP_LOG_WARN(HIPPOCAMPUS_KG_MODULE_NAME, "Failed to register subfields");
    }

    /* Register memory nodes */
    if (local_config.register_memory_nodes) {
        if (hippocampus_kg_register_memory_nodes(kg, local_state.root_id,
                                                  &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(HIPPOCAMPUS_KG_MODULE_NAME, "Failed to register memory nodes");
        }
    }

    /* Register navigation nodes */
    if (local_config.register_nav_nodes) {
        if (hippocampus_kg_register_nav_nodes(kg, local_state.root_id,
                                               &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(HIPPOCAMPUS_KG_MODULE_NAME, "Failed to register nav nodes");
        }
    }

    /* Register cross-subsystem edges */
    if (local_config.register_cross_edges) {
        hippocampus_kg_register_cross_edges(kg, &local_state, admin_token);
    }

    local_state.registered = true;

    /* Copy to output if requested */
    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(HIPPOCAMPUS_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Registration API - Subfields
//=============================================================================

int hippocampus_kg_register_subfields(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const hippocampus_kg_config_t* config,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_kg_register_subfields: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Register CA1 */
    if (!config || config->register_ca1) {
        state->ca1_id = create_hippocampus_node(
            kg, HIPPOCAMPUS_KG_CA1_NAME,
            (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_SUBFIELD,
            "CA1 - pattern completion, temporal coding, output to cortex",
            admin_token
        );
        if (state->ca1_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_hippocampus_edge(kg, parent_id, state->ca1_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains CA1", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Register CA3 */
    if (!config || config->register_ca3) {
        state->ca3_id = create_hippocampus_node(
            kg, HIPPOCAMPUS_KG_CA3_NAME,
            (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_SUBFIELD,
            "CA3 - autoassociative memory, pattern completion, recurrent",
            admin_token
        );
        if (state->ca3_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_hippocampus_edge(kg, parent_id, state->ca3_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains CA3", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Register dentate gyrus */
    if (!config || config->register_dg) {
        state->dg_id = create_hippocampus_node(
            kg, HIPPOCAMPUS_KG_DG_NAME,
            (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_SUBFIELD,
            "Dentate Gyrus - pattern separation, sparse coding, neurogenesis",
            admin_token
        );
        if (state->dg_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_hippocampus_edge(kg, parent_id, state->dg_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains DG", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Register subiculum */
    if (!config || config->register_subiculum) {
        state->subiculum_id = create_hippocampus_node(
            kg, HIPPOCAMPUS_KG_SUBICULUM_NAME,
            (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_SUBFIELD,
            "Subiculum - output gateway, boundary coding, spatial processing",
            admin_token
        );
        if (state->subiculum_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_hippocampus_edge(kg, parent_id, state->subiculum_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains subiculum", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Register entorhinal cortex interface */
    state->ec_id = create_hippocampus_node(
        kg, HIPPOCAMPUS_KG_EC_NAME,
        (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_SUBFIELD,
        "Entorhinal cortex interface - input/output hub, grid cells",
        admin_token
    );
    if (state->ec_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_hippocampus_edge(kg, parent_id, state->ec_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "interfaces with EC", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create trisynaptic pathway edges */
    if (state->ec_id != BRAIN_KG_INVALID_NODE &&
        state->dg_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->ec_id, state->dg_id,
            BRAIN_KG_EDGE_SENDS_TO, "perforant path to DG", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->dg_id != BRAIN_KG_INVALID_NODE &&
        state->ca3_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->dg_id, state->ca3_id,
            BRAIN_KG_EDGE_SENDS_TO, "mossy fiber pathway", 0.85f, admin_token);
        state->edge_count++;
    }

    if (state->ca3_id != BRAIN_KG_INVALID_NODE &&
        state->ca1_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->ca3_id, state->ca1_id,
            BRAIN_KG_EDGE_SENDS_TO, "Schaffer collaterals", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->ca1_id != BRAIN_KG_INVALID_NODE &&
        state->subiculum_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->ca1_id, state->subiculum_id,
            BRAIN_KG_EDGE_SENDS_TO, "CA1 output to subiculum", 0.85f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Memory Nodes
//=============================================================================

/**
 * @brief Register core memory nodes
 */
static int register_core_memory_nodes(
    brain_kg_t* kg,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
) {
    /* Episodic memory node */
    state->episodic_id = create_hippocampus_node(
        kg, "episodic_memory",
        (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_MEMORY_TYPE,
        "Episodic memory - autobiographical events, what-where-when",
        admin_token
    );
    if (state->episodic_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_hippocampus_edge(kg, state->memory_system_id, state->episodic_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "supports episodic memory", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Spatial memory node */
    state->spatial_id = create_hippocampus_node(
        kg, "spatial_memory",
        (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_MEMORY_TYPE,
        "Spatial memory - cognitive maps, allocentric representation",
        admin_token
    );
    if (state->spatial_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_hippocampus_edge(kg, state->memory_system_id, state->spatial_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "supports spatial memory", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Working memory buffer node */
    state->working_buffer_id = create_hippocampus_node(
        kg, "working_memory_buffer",
        (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_MEMORY_TYPE,
        "Working memory buffer - temporary maintenance, binding",
        admin_token
    );
    if (state->working_buffer_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_hippocampus_edge(kg, state->memory_system_id, state->working_buffer_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "provides WM buffer", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

/**
 * @brief Register memory process nodes
 */
static int register_memory_process_nodes(
    brain_kg_t* kg,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
) {
    /* Encoding system node */
    state->encoding_id = create_hippocampus_node(
        kg, "encoding_system",
        (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_MEMORY_PROCESS,
        "Encoding system - new memory trace formation",
        admin_token
    );
    if (state->encoding_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_hippocampus_edge(kg, state->memory_system_id, state->encoding_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains encoding", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Retrieval system node */
    state->retrieval_id = create_hippocampus_node(
        kg, "retrieval_system",
        (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_MEMORY_PROCESS,
        "Retrieval system - memory recall, pattern completion",
        admin_token
    );
    if (state->retrieval_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_hippocampus_edge(kg, state->memory_system_id, state->retrieval_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains retrieval", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Consolidation system node */
    state->consolidation_id = create_hippocampus_node(
        kg, "consolidation_system",
        (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_MEMORY_PROCESS,
        "Consolidation system - memory stabilization, replay, transfer",
        admin_token
    );
    if (state->consolidation_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_hippocampus_edge(kg, state->memory_system_id, state->consolidation_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains consolidation", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Pattern separation node */
    state->pattern_sep_id = create_hippocampus_node(
        kg, "pattern_separation",
        (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_PATTERN_PROCESS,
        "Pattern separation - orthogonalization, discrimination",
        admin_token
    );
    if (state->pattern_sep_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_hippocampus_edge(kg, state->memory_system_id, state->pattern_sep_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "performs pattern separation", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Pattern completion node */
    state->pattern_comp_id = create_hippocampus_node(
        kg, "pattern_completion",
        (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_PATTERN_PROCESS,
        "Pattern completion - cue-driven recall, associative retrieval",
        admin_token
    );
    if (state->pattern_comp_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_hippocampus_edge(kg, state->memory_system_id, state->pattern_comp_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "performs pattern completion", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int hippocampus_kg_register_memory_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_kg_register_memory_nodes: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create memory system subsystem node */
    state->memory_system_id = create_hippocampus_node(
        kg, HIPPOCAMPUS_KG_MEMORY_NAME,
        BRAIN_KG_NODE_COGNITIVE,
        "Memory system - episodic encoding, consolidation, retrieval",
        admin_token
    );
    if (state->memory_system_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_kg_register_memory_nodes: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, parent_id, state->memory_system_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains memory system", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Register core memory nodes */
    register_core_memory_nodes(kg, state, admin_token);

    /* Register memory process nodes */
    register_memory_process_nodes(kg, state, admin_token);

    /* Create edges between memory nodes */
    if (state->encoding_id != BRAIN_KG_INVALID_NODE &&
        state->consolidation_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->encoding_id, state->consolidation_id,
            (brain_kg_edge_type_t)HIPPOCAMPUS_KG_EDGE_TRIGGERS,
            "encoding triggers consolidation", 0.85f, admin_token);
        state->edge_count++;
    }

    if (state->pattern_sep_id != BRAIN_KG_INVALID_NODE &&
        state->encoding_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->pattern_sep_id, state->encoding_id,
            (brain_kg_edge_type_t)HIPPOCAMPUS_KG_EDGE_ENABLES,
            "pattern separation enables encoding", 0.8f, admin_token);
        state->edge_count++;
    }

    if (state->pattern_comp_id != BRAIN_KG_INVALID_NODE &&
        state->retrieval_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->pattern_comp_id, state->retrieval_id,
            (brain_kg_edge_type_t)HIPPOCAMPUS_KG_EDGE_ENABLES,
            "pattern completion enables retrieval", 0.85f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Navigation Nodes
//=============================================================================

int hippocampus_kg_register_nav_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_kg_register_nav_nodes: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create navigation system node */
    state->nav_system_id = create_hippocampus_node(
        kg, HIPPOCAMPUS_KG_NAV_NAME,
        BRAIN_KG_NODE_COGNITIVE,
        "Navigation system - spatial coding, path integration, mapping",
        admin_token
    );
    if (state->nav_system_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippocampus_kg_register_nav_nodes: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, parent_id, state->nav_system_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains navigation system", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Place cells node */
    state->place_cells_id = create_hippocampus_node(
        kg, "place_cells",
        (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_NAV_COMPONENT,
        "Place cells - location-specific firing, cognitive map",
        admin_token
    );
    if (state->place_cells_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_hippocampus_edge(kg, state->nav_system_id, state->place_cells_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains place cells", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Grid cells node */
    state->grid_cells_id = create_hippocampus_node(
        kg, "grid_cells",
        (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_NAV_COMPONENT,
        "Grid cells - metric representation, path integration",
        admin_token
    );
    if (state->grid_cells_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_hippocampus_edge(kg, state->nav_system_id, state->grid_cells_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains grid cells", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Head direction cells node */
    state->head_dir_id = create_hippocampus_node(
        kg, "head_direction_cells",
        (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_NAV_COMPONENT,
        "Head direction cells - orientation, compass signal",
        admin_token
    );
    if (state->head_dir_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_hippocampus_edge(kg, state->nav_system_id, state->head_dir_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains head direction cells", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create navigation-related edges */
    if (state->grid_cells_id != BRAIN_KG_INVALID_NODE &&
        state->place_cells_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->grid_cells_id, state->place_cells_id,
            BRAIN_KG_EDGE_SENDS_TO, "grid cells inform place cells", 0.8f, admin_token);
        state->edge_count++;
    }

    if (state->head_dir_id != BRAIN_KG_INVALID_NODE &&
        state->grid_cells_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->head_dir_id, state->grid_cells_id,
            BRAIN_KG_EDGE_SENDS_TO, "head direction modulates grid", 0.75f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Cross Edges
//=============================================================================

/**
 * @brief Register subfield-to-function edges
 */
static void register_subfield_function_edges(
    brain_kg_t* kg,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
) {
    /* DG handles pattern separation */
    if (state->dg_id != BRAIN_KG_INVALID_NODE &&
        state->pattern_sep_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->dg_id, state->pattern_sep_id,
            BRAIN_KG_EDGE_MODULATES,
            "DG performs pattern separation", 0.9f, admin_token);
        state->edge_count++;
    }

    /* CA3 handles pattern completion */
    if (state->ca3_id != BRAIN_KG_INVALID_NODE &&
        state->pattern_comp_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->ca3_id, state->pattern_comp_id,
            BRAIN_KG_EDGE_MODULATES,
            "CA3 performs pattern completion", 0.9f, admin_token);
        state->edge_count++;
    }

    /* CA1 handles place cells */
    if (state->ca1_id != BRAIN_KG_INVALID_NODE &&
        state->place_cells_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->ca1_id, state->place_cells_id,
            BRAIN_KG_EDGE_CONNECTS_TO,
            "CA1 contains place cells", 0.85f, admin_token);
        state->edge_count++;
    }

    /* EC handles grid cells */
    if (state->ec_id != BRAIN_KG_INVALID_NODE &&
        state->grid_cells_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->ec_id, state->grid_cells_id,
            BRAIN_KG_EDGE_CONNECTS_TO,
            "EC contains grid cells", 0.85f, admin_token);
        state->edge_count++;
    }
}

/**
 * @brief Register memory-navigation integration edges
 */
static void register_memory_nav_edges(
    brain_kg_t* kg,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
) {
    /* Spatial memory integrates with navigation */
    if (state->spatial_id != BRAIN_KG_INVALID_NODE &&
        state->nav_system_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->nav_system_id, state->spatial_id,
            BRAIN_KG_EDGE_INTEGRATES_WITH,
            "navigation supports spatial memory", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Place cells contribute to episodic memory */
    if (state->place_cells_id != BRAIN_KG_INVALID_NODE &&
        state->episodic_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->place_cells_id, state->episodic_id,
            (brain_kg_edge_type_t)HIPPOCAMPUS_KG_EDGE_ENHANCES,
            "place cells provide context for episodes", 0.8f, admin_token);
        state->edge_count++;
    }

    /* Consolidation uses replay */
    if (state->consolidation_id != BRAIN_KG_INVALID_NODE &&
        state->place_cells_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->consolidation_id, state->place_cells_id,
            (brain_kg_edge_type_t)HIPPOCAMPUS_KG_EDGE_COORDINATES,
            "consolidation triggers place cell replay", 0.75f, admin_token);
        state->edge_count++;
    }
}

/**
 * @brief Register retrieval-related edges
 */
static void register_retrieval_edges(
    brain_kg_t* kg,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
) {
    /* Retrieval sends to working buffer */
    if (state->retrieval_id != BRAIN_KG_INVALID_NODE &&
        state->working_buffer_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->retrieval_id, state->working_buffer_id,
            BRAIN_KG_EDGE_SENDS_TO,
            "retrieved memories enter WM buffer", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Retrieval uses pattern completion */
    if (state->retrieval_id != BRAIN_KG_INVALID_NODE &&
        state->pattern_comp_id != BRAIN_KG_INVALID_NODE) {
        create_hippocampus_edge(kg, state->pattern_comp_id, state->retrieval_id,
            (brain_kg_edge_type_t)HIPPOCAMPUS_KG_EDGE_RECALLS,
            "pattern completion enables recall", 0.9f, admin_token);
        state->edge_count++;
    }
}

int hippocampus_kg_register_cross_edges(
    brain_kg_t* kg,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_kg_register_cross_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Register subfield-to-function edges */
    register_subfield_function_edges(kg, state, admin_token);

    /* Register memory-navigation integration edges */
    register_memory_nav_edges(kg, state, admin_token);

    /* Register retrieval-related edges */
    register_retrieval_edges(kg, state, admin_token);

    return 0;
}

//=============================================================================
// State Synchronization API
//=============================================================================

int hippocampus_kg_update_state(
    brain_kg_t* kg,
    const hippocampus_kg_state_t* state,
    float encoding_strength,
    float retrieval_accuracy,
    float consolidation_progress,
    float spatial_precision,
    uint64_t admin_token
) {
    (void)admin_token;  /* Reserved for future access control */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_kg_update_state: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Update encoding metadata */
    if (state->encoding_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.3f", encoding_strength);
        brain_kg_add_metadata(kg, state->encoding_id, "encoding_strength", val_str);
    }

    /* Update retrieval metadata */
    if (state->retrieval_id != BRAIN_KG_INVALID_NODE) {
        char acc_str[32];
        snprintf(acc_str, sizeof(acc_str), "%.1f%%", retrieval_accuracy * 100.0f);
        brain_kg_add_metadata(kg, state->retrieval_id, "accuracy", acc_str);
    }

    /* Update consolidation metadata */
    if (state->consolidation_id != BRAIN_KG_INVALID_NODE) {
        char prog_str[32];
        snprintf(prog_str, sizeof(prog_str), "%.1f%%", consolidation_progress * 100.0f);
        brain_kg_add_metadata(kg, state->consolidation_id, "progress", prog_str);
    }

    /* Update spatial precision metadata */
    if (state->nav_system_id != BRAIN_KG_INVALID_NODE) {
        char prec_str[32];
        snprintf(prec_str, sizeof(prec_str), "%.3f", spatial_precision);
        brain_kg_add_metadata(kg, state->nav_system_id, "spatial_precision", prec_str);
    }

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t hippocampus_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, HIPPOCAMPUS_KG_ROOT_NAME);
}

brain_kg_node_id_t hippocampus_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

brain_kg_node_list_t* hippocampus_kg_get_memory_nodes(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_MEMORY_TYPE
    );
}

brain_kg_node_list_t* hippocampus_kg_get_nav_nodes(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_NAV_COMPONENT
    );
}

brain_kg_node_list_t* hippocampus_kg_get_subfields(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)HIPPOCAMPUS_KG_NODE_SUBFIELD
    );
}

int hippocampus_kg_unregister_all(
    brain_kg_t* kg,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;  /* Would be used for actual deletion */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    /*
     * Note: Full implementation would remove nodes in reverse order
     * For now, mark as unregistered
     */
    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(HIPPOCAMPUS_KG_MODULE_NAME, "Unregistered Hippocampus KG nodes");

    return 0;
}
