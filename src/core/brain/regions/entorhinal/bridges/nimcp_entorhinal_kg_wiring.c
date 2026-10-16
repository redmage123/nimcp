//=============================================================================
// nimcp_entorhinal_kg_wiring.c - Entorhinal Cortex Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_entorhinal_kg_wiring.c
 * @brief Implementation of Entorhinal Cortex Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for Entorhinal Cortex module
 * WHY:  Enables semantic queries about spatial mapping and memory gateway
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/entorhinal/bridges/nimcp_entorhinal_kg_wiring.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(entorhinal_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Helper Functions
//=============================================================================

static brain_kg_node_id_t create_entorhinal_node(
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
        NIMCP_LOG_DEBUG(ENTORHINAL_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

static brain_kg_edge_id_t create_entorhinal_edge(
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

int entorhinal_kg_default_config(entorhinal_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_kg_default_config: config is NULL");
        return -1;
    }

    config->register_grid_cells = true;
    config->register_border_cells = true;
    config->register_spatial_mapping = true;
    config->register_memory_gateway = true;
    config->register_cortical_input = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int entorhinal_kg_register_all(
    brain_kg_t* kg,
    const entorhinal_kg_config_t* config,
    entorhinal_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_kg_register_all: kg is NULL");
        return -1;
    }

    entorhinal_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        entorhinal_kg_default_config(&local_config);
    }

    entorhinal_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create root node */
    local_state.root_id = create_entorhinal_node(
        kg, ENTORHINAL_KG_ROOT_NAME,
        BRAIN_KG_NODE_CORTICAL,
        "Entorhinal cortex - memory gateway, grid cells, spatial mapping, cortical-hippocampal relay",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(ENTORHINAL_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Grid cells */
    if (local_config.register_grid_cells) {
        local_state.grid_cells_id = create_entorhinal_node(
            kg, "ec_grid_cells",
            (brain_kg_node_type_t)ENTORHINAL_KG_NODE_GRID_CELLS,
            "Grid cells - hexagonal spatial firing, metric representation, path integration",
            admin_token
        );
        if (local_state.grid_cells_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_entorhinal_edge(kg, local_state.root_id, local_state.grid_cells_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains grid cells", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Border cells */
    if (local_config.register_border_cells) {
        local_state.border_cells_id = create_entorhinal_node(
            kg, "ec_border_cells",
            (brain_kg_node_type_t)ENTORHINAL_KG_NODE_BORDER_CELLS,
            "Border cells - environmental boundary detection, geometric representation",
            admin_token
        );
        if (local_state.border_cells_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_entorhinal_edge(kg, local_state.root_id, local_state.border_cells_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains border cells", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Spatial mapping */
    if (local_config.register_spatial_mapping) {
        local_state.spatial_mapping_id = create_entorhinal_node(
            kg, "ec_spatial_mapping",
            (brain_kg_node_type_t)ENTORHINAL_KG_NODE_SPATIAL_MAP,
            "Spatial mapping - cognitive map construction, allocentric coordinate system",
            admin_token
        );
        if (local_state.spatial_mapping_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_entorhinal_edge(kg, local_state.root_id, local_state.spatial_mapping_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains spatial mapping", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Memory gateway */
    if (local_config.register_memory_gateway) {
        local_state.memory_gateway_id = create_entorhinal_node(
            kg, "ec_memory_gateway",
            (brain_kg_node_type_t)ENTORHINAL_KG_NODE_MEMORY_GATE,
            "Memory gateway - cortex-hippocampus relay, input filtering, layer II/III projections",
            admin_token
        );
        if (local_state.memory_gateway_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_entorhinal_edge(kg, local_state.root_id, local_state.memory_gateway_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains memory gateway", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Cortical input */
    if (local_config.register_cortical_input) {
        local_state.cortical_input_id = create_entorhinal_node(
            kg, "ec_cortical_input",
            (brain_kg_node_type_t)ENTORHINAL_KG_NODE_CORTICAL_INPUT,
            "Cortical input - multimodal sensory convergence, perirhinal/parahippocampal input",
            admin_token
        );
        if (local_state.cortical_input_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_entorhinal_edge(kg, local_state.root_id, local_state.cortical_input_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains cortical input", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Cross edges */
    if (local_config.register_cross_edges) {
        /* Grid cells contribute to spatial mapping */
        if (local_state.grid_cells_id != BRAIN_KG_INVALID_NODE &&
            local_state.spatial_mapping_id != BRAIN_KG_INVALID_NODE) {
            create_entorhinal_edge(kg, local_state.grid_cells_id,
                local_state.spatial_mapping_id,
                (brain_kg_edge_type_t)ENTORHINAL_KG_EDGE_MAPS,
                "grid cells provide metric spatial map", 0.95f, admin_token);
            local_state.edge_count++;
        }
        /* Border cells bound spatial mapping */
        if (local_state.border_cells_id != BRAIN_KG_INVALID_NODE &&
            local_state.spatial_mapping_id != BRAIN_KG_INVALID_NODE) {
            create_entorhinal_edge(kg, local_state.border_cells_id,
                local_state.spatial_mapping_id,
                (brain_kg_edge_type_t)ENTORHINAL_KG_EDGE_BOUNDS,
                "border cells define spatial boundaries", 0.85f, admin_token);
            local_state.edge_count++;
        }
        /* Cortical input relays to memory gateway */
        if (local_state.cortical_input_id != BRAIN_KG_INVALID_NODE &&
            local_state.memory_gateway_id != BRAIN_KG_INVALID_NODE) {
            create_entorhinal_edge(kg, local_state.cortical_input_id,
                local_state.memory_gateway_id,
                (brain_kg_edge_type_t)ENTORHINAL_KG_EDGE_RELAYS,
                "cortical input relays to hippocampal memory gateway", 0.9f, admin_token);
            local_state.edge_count++;
        }
        /* Memory gateway gates spatial information */
        if (local_state.memory_gateway_id != BRAIN_KG_INVALID_NODE &&
            local_state.spatial_mapping_id != BRAIN_KG_INVALID_NODE) {
            create_entorhinal_edge(kg, local_state.memory_gateway_id,
                local_state.spatial_mapping_id,
                (brain_kg_edge_type_t)ENTORHINAL_KG_EDGE_GATES,
                "memory gateway gates spatial context encoding", 0.85f, admin_token);
            local_state.edge_count++;
        }
    }

    local_state.registered = true;
    if (state) *state = local_state;

    NIMCP_LOG_INFO(ENTORHINAL_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t entorhinal_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, ENTORHINAL_KG_ROOT_NAME);
}

brain_kg_node_id_t entorhinal_kg_find_subsystem(brain_kg_t* kg, const char* name) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

int entorhinal_kg_unregister_all(
    brain_kg_t* kg,
    entorhinal_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(ENTORHINAL_KG_MODULE_NAME, "Unregistered Entorhinal KG nodes");
    return 0;
}

//=============================================================================
// Runtime Event Emission (Wave W2)
//=============================================================================

/*
 * HOT-PATH GAP: Entorhinal cortex has no dedicated tick .c file in this
 * tree. Spatial / grid-cell fire events should call entorhinal_kg_emit_event
 * at the point a grid-cell population spikes.
 */

void entorhinal_kg_emit_event(
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
             "entorhinal_event_%s_%" PRIu64, kind, ts_us);
    snprintf(desc, sizeof(desc),
             "entorhinal runtime event: kind=%s intensity=%.3f ts_us=%" PRIu64,
             kind, (double)intensity, ts_us);

    brain_kg_node_id_t evt_id = brain_kg_add_node(
        kg, name, BRAIN_KG_NODE_CUSTOM, desc
    );

    if (evt_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.4f", (double)intensity);
        brain_kg_add_metadata(kg, evt_id, "intensity", val_str);
        brain_kg_add_metadata(kg, evt_id, "kind", kind);

        brain_kg_node_id_t root = brain_kg_find_node(kg, ENTORHINAL_KG_ROOT_NAME);
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
