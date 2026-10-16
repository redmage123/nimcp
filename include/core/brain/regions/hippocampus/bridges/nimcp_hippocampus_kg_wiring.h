//=============================================================================
// nimcp_hippocampus_kg_wiring.h - Hippocampus Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_hippocampus_kg_wiring.h
 * @brief Knowledge Graph registration for Hippocampus module
 *
 * WHAT: Registers Hippocampus concepts (memory formation, spatial navigation,
 *       pattern separation/completion) as nodes in the brain's internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about memory state ("what is current encoding strength?")
 *       - Cross-module reasoning about memory consolidation
 *       - Introspection of spatial and episodic memory systems
 *       - Graph-based analysis of memory dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - Hippocampus root node
 *         ├── CA1 (pattern completion, output)
 *         ├── CA3 (pattern completion, recurrent)
 *         ├── Dentate Gyrus (pattern separation)
 *         ├── Subiculum (output gateway)
 *         ├── Entorhinal Cortex interface
 *         ├── Memory nodes
 *         │   ├── Episodic memory
 *         │   ├── Spatial memory
 *         │   ├── Working memory buffer
 *         │   ├── Encoding system
 *         │   ├── Retrieval system
 *         │   └── Consolidation system
 *         └── Navigation nodes
 *             ├── Place cells
 *             ├── Grid cells
 *             └── Head direction
 *
 * EDGES: Represent causal/functional relationships:
 *       - Encoding -> Consolidation (TRIGGERS)
 *       - Pattern separation -> Pattern completion (ENABLES)
 *       - Spatial -> Episodic (INTEGRATES_WITH)
 *       - Retrieval -> Working memory (SENDS_TO)
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_HIPPOCAMPUS_KG_WIRING_H
#define NIMCP_HIPPOCAMPUS_KG_WIRING_H

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

/** Module name for logging */
#define HIPPOCAMPUS_KG_MODULE_NAME    "hippocampus_kg_wiring"

/** Hippocampus root node name */
#define HIPPOCAMPUS_KG_ROOT_NAME      "hippocampus"

/** CA1 subfield node name */
#define HIPPOCAMPUS_KG_CA1_NAME       "ca1"

/** CA3 subfield node name */
#define HIPPOCAMPUS_KG_CA3_NAME       "ca3"

/** Dentate Gyrus node name */
#define HIPPOCAMPUS_KG_DG_NAME        "dentate_gyrus"

/** Subiculum node name */
#define HIPPOCAMPUS_KG_SUBICULUM_NAME "subiculum"

/** Entorhinal cortex interface node name */
#define HIPPOCAMPUS_KG_EC_NAME        "entorhinal_interface"

/** Memory system node name */
#define HIPPOCAMPUS_KG_MEMORY_NAME    "memory_system"

/** Navigation system node name */
#define HIPPOCAMPUS_KG_NAV_NAME       "navigation_system"

//=============================================================================
// Node Type Extensions (for Hippocampus-specific concepts)
//=============================================================================

/**
 * @brief Hippocampus-specific KG node types
 *
 * These extend the base brain_kg_node_type_t with Hippocampus-specific types.
 * Values start at 0x2100 to avoid conflicts with core and other region types.
 */
typedef enum {
    /** Hippocampal subfield (CA1, CA3, DG, etc.) */
    HIPPOCAMPUS_KG_NODE_SUBFIELD = 0x2100,

    /** Memory type (episodic, spatial, semantic) */
    HIPPOCAMPUS_KG_NODE_MEMORY_TYPE,

    /** Memory process (encoding, retrieval, consolidation) */
    HIPPOCAMPUS_KG_NODE_MEMORY_PROCESS,

    /** Navigation component (place cells, grid cells) */
    HIPPOCAMPUS_KG_NODE_NAV_COMPONENT,

    /** Pattern processing (separation, completion) */
    HIPPOCAMPUS_KG_NODE_PATTERN_PROCESS,

    /** Replay/reactivation */
    HIPPOCAMPUS_KG_NODE_REPLAY,

    /** Theta rhythm component */
    HIPPOCAMPUS_KG_NODE_THETA_RHYTHM
} hippocampus_kg_node_type_t;

/**
 * @brief Hippocampus-specific edge types
 */
typedef enum {
    /** Memory encoding triggers consolidation */
    HIPPOCAMPUS_KG_EDGE_TRIGGERS = 0x2100,

    /** Pattern separation enables discrimination */
    HIPPOCAMPUS_KG_EDGE_ENABLES,

    /** Spatial context enhances episodic */
    HIPPOCAMPUS_KG_EDGE_ENHANCES,

    /** Replay strengthens trace */
    HIPPOCAMPUS_KG_EDGE_STRENGTHENS,

    /** Theta rhythm coordinates */
    HIPPOCAMPUS_KG_EDGE_COORDINATES,

    /** Sharp-wave ripple consolidates */
    HIPPOCAMPUS_KG_EDGE_CONSOLIDATES,

    /** Pattern completion recalls */
    HIPPOCAMPUS_KG_EDGE_RECALLS
} hippocampus_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Hippocampus KG wiring configuration
 */
typedef struct {
    /** Register CA1 subfield nodes */
    bool register_ca1;

    /** Register CA3 subfield nodes */
    bool register_ca3;

    /** Register dentate gyrus nodes */
    bool register_dg;

    /** Register subiculum nodes */
    bool register_subiculum;

    /** Register memory system nodes */
    bool register_memory_nodes;

    /** Register navigation nodes */
    bool register_nav_nodes;

    /** Register inter-subsystem edges */
    bool register_cross_edges;

    /** Include state metadata */
    bool include_state_metadata;

    /** Register theta rhythm edges */
    bool register_theta_edges;
} hippocampus_kg_config_t;

/**
 * @brief Hippocampus KG wiring state (node IDs for reference)
 */
typedef struct {
    /** Hippocampus root node ID */
    brain_kg_node_id_t root_id;

    /* Subfield node IDs */
    /** CA1 subfield node ID */
    brain_kg_node_id_t ca1_id;

    /** CA3 subfield node ID */
    brain_kg_node_id_t ca3_id;

    /** Dentate gyrus node ID */
    brain_kg_node_id_t dg_id;

    /** Subiculum node ID */
    brain_kg_node_id_t subiculum_id;

    /** Entorhinal cortex interface node ID */
    brain_kg_node_id_t ec_id;

    /* Memory system node IDs */
    /** Memory system node ID */
    brain_kg_node_id_t memory_system_id;

    /** Episodic memory node ID */
    brain_kg_node_id_t episodic_id;

    /** Spatial memory node ID */
    brain_kg_node_id_t spatial_id;

    /** Working memory buffer node ID */
    brain_kg_node_id_t working_buffer_id;

    /** Encoding system node ID */
    brain_kg_node_id_t encoding_id;

    /** Retrieval system node ID */
    brain_kg_node_id_t retrieval_id;

    /** Consolidation system node ID */
    brain_kg_node_id_t consolidation_id;

    /** Pattern separation node ID */
    brain_kg_node_id_t pattern_sep_id;

    /** Pattern completion node ID */
    brain_kg_node_id_t pattern_comp_id;

    /* Navigation node IDs */
    /** Navigation system node ID */
    brain_kg_node_id_t nav_system_id;

    /** Place cells node ID */
    brain_kg_node_id_t place_cells_id;

    /** Grid cells node ID */
    brain_kg_node_id_t grid_cells_id;

    /** Head direction node ID */
    brain_kg_node_id_t head_dir_id;

    /* Counters */
    /** Number of nodes registered */
    uint32_t node_count;

    /** Number of edges registered */
    uint32_t edge_count;

    /** Registration successful */
    bool registered;
} hippocampus_kg_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default Hippocampus KG wiring configuration
 *
 * WHAT: Initializes configuration with sensible defaults
 * WHY:  Provides consistent starting point for KG registration
 * HOW:  Sets all registration flags to true
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hippocampus_kg_default_config(hippocampus_kg_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register all Hippocampus nodes in KG
 *
 * WHAT: Creates nodes for Hippocampus concepts in the brain's KG
 * WHY:  Enables semantic queries and reasoning about memory
 * HOW:  Creates hierarchical node structure with typed edges
 *
 * @param kg Knowledge graph to register in
 * @param config Registration configuration (NULL for defaults)
 * @param state Output registration state (optional, may be NULL)
 * @param admin_token Admin token for write access
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hippocampus_kg_register_all(
    brain_kg_t* kg,
    const hippocampus_kg_config_t* config,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register Hippocampus subfield nodes
 *
 * WHAT: Creates nodes for CA1, CA3, DG, subiculum
 * WHY:  Represents anatomical structure of hippocampus
 * HOW:  Creates subfield nodes linked to hippocampus root
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (hippocampus root)
 * @param config Configuration options
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hippocampus_kg_register_subfields(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const hippocampus_kg_config_t* config,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register memory system nodes
 *
 * WHAT: Creates nodes for memory types and processes
 * WHY:  Represents hippocampus's core memory functions
 * HOW:  Creates memory nodes with causal relationships
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (hippocampus root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hippocampus_kg_register_memory_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register navigation-related nodes
 *
 * WHAT: Creates nodes for spatial navigation (place cells, grid cells)
 * WHY:  Represents hippocampus's spatial mapping functions
 * HOW:  Creates navigation nodes linked to memory system
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (hippocampus root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hippocampus_kg_register_nav_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cross-subsystem edges
 *
 * WHAT: Creates edges between hippocampus subsystems
 * WHY:  Represents causal relationships across modules
 * HOW:  Encoding -> Consolidation, Spatial -> Episodic, etc.
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hippocampus_kg_register_cross_edges(
    brain_kg_t* kg,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// State Synchronization API
//=============================================================================

/**
 * @brief Update hippocampus node metadata with current state
 *
 * WHAT: Synchronizes KG node metadata with hippocampus state
 * WHY:  Enables queries about current memory state
 * HOW:  Updates node metadata fields
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param encoding_strength Current encoding strength
 * @param retrieval_accuracy Current retrieval accuracy
 * @param consolidation_progress Current consolidation progress
 * @param spatial_precision Current spatial precision
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hippocampus_kg_update_state(
    brain_kg_t* kg,
    const hippocampus_kg_state_t* state,
    float encoding_strength,
    float retrieval_accuracy,
    float consolidation_progress,
    float spatial_precision,
    uint64_t admin_token
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get hippocampus root node ID
 *
 * @param kg Knowledge graph
 * @return Root node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t hippocampus_kg_get_root(brain_kg_t* kg);

/**
 * @brief Find hippocampus subsystem by name
 *
 * @param kg Knowledge graph
 * @param name Subsystem name ("ca1", "ca3", etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t hippocampus_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/**
 * @brief Get all memory-related nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* hippocampus_kg_get_memory_nodes(brain_kg_t* kg);

/**
 * @brief Get all navigation-related nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* hippocampus_kg_get_nav_nodes(brain_kg_t* kg);

/**
 * @brief Get hippocampus subfield nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* hippocampus_kg_get_subfields(brain_kg_t* kg);

/**
 * @brief Unregister all hippocampus nodes (cleanup)
 *
 * WHAT: Removes all hippocampus nodes from KG
 * WHY:  Clean shutdown and resource release
 * HOW:  Removes nodes in reverse creation order
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hippocampus_kg_unregister_all(
    brain_kg_t* kg,
    hippocampus_kg_state_t* state,
    uint64_t admin_token
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HIPPOCAMPUS_KG_WIRING_H */
