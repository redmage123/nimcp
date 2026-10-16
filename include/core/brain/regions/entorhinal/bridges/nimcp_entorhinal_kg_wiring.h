//=============================================================================
// nimcp_entorhinal_kg_wiring.h - Entorhinal Cortex Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_entorhinal_kg_wiring.h
 * @brief Knowledge Graph registration for Entorhinal Cortex module
 *
 * WHAT: Registers Entorhinal Cortex concepts (grid cells, spatial mapping,
 *       memory gateway) as nodes in the brain's internal KG.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about spatial mapping state
 *       - Cross-module reasoning about memory gateway function
 *       - Introspection of grid cell and border cell systems
 *
 * HOW:  Creates hierarchical node structure:
 *       - Entorhinal root node
 *         +-- Grid cells
 *         +-- Border cells
 *         +-- Spatial mapping
 *         +-- Memory gateway
 *         +-- Cortical input
 *
 * @author NIMCP Development Team
 * @date 2026-02-04
 * @version 1.0.0
 */

#ifndef NIMCP_ENTORHINAL_KG_WIRING_H
#define NIMCP_ENTORHINAL_KG_WIRING_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_kg.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define ENTORHINAL_KG_MODULE_NAME    "entorhinal_kg_wiring"
#define ENTORHINAL_KG_ROOT_NAME      "entorhinal_cortex"

//=============================================================================
// Node Type Extensions (Entorhinal-specific, starting at 0x2D00)
//=============================================================================

typedef enum {
    ENTORHINAL_KG_NODE_GRID_CELLS = 0x2D00,
    ENTORHINAL_KG_NODE_BORDER_CELLS,
    ENTORHINAL_KG_NODE_SPATIAL_MAP,
    ENTORHINAL_KG_NODE_MEMORY_GATE,
    ENTORHINAL_KG_NODE_CORTICAL_INPUT
} entorhinal_kg_node_type_t;

typedef enum {
    ENTORHINAL_KG_EDGE_MAPS = 0x2D00,
    ENTORHINAL_KG_EDGE_GATES,
    ENTORHINAL_KG_EDGE_RELAYS,
    ENTORHINAL_KG_EDGE_BOUNDS
} entorhinal_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    bool register_grid_cells;
    bool register_border_cells;
    bool register_spatial_mapping;
    bool register_memory_gateway;
    bool register_cortical_input;
    bool register_cross_edges;
    bool include_state_metadata;
} entorhinal_kg_config_t;

typedef struct {
    brain_kg_node_id_t root_id;
    brain_kg_node_id_t grid_cells_id;
    brain_kg_node_id_t border_cells_id;
    brain_kg_node_id_t spatial_mapping_id;
    brain_kg_node_id_t memory_gateway_id;
    brain_kg_node_id_t cortical_input_id;
    uint32_t node_count;
    uint32_t edge_count;
    bool registered;
} entorhinal_kg_state_t;

//=============================================================================
// API
//=============================================================================

NIMCP_EXPORT int entorhinal_kg_default_config(entorhinal_kg_config_t* config);

NIMCP_EXPORT int entorhinal_kg_register_all(
    brain_kg_t* kg,
    const entorhinal_kg_config_t* config,
    entorhinal_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT int entorhinal_kg_unregister_all(
    brain_kg_t* kg,
    entorhinal_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT brain_kg_node_id_t entorhinal_kg_get_root(brain_kg_t* kg);

NIMCP_EXPORT brain_kg_node_id_t entorhinal_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/* Forward decl for runtime event emit API (W2). */
struct brain_struct;

/**
 * @brief Emit a runtime entorhinal event into the brain's internal KG
 *
 * Supported kinds: "grid_cell_fire". Silent no-op if brain/KG unavailable.
 * Creates `entorhinal_event_<kind>_<ts_us>` node + edge to
 * `entorhinal_cortex`. Self-elevates admin token.
 */
NIMCP_EXPORT void entorhinal_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENTORHINAL_KG_WIRING_H */
