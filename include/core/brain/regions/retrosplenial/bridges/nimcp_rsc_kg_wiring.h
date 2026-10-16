//=============================================================================
// nimcp_rsc_kg_wiring.h - Retrosplenial Cortex Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_rsc_kg_wiring.h
 * @brief Knowledge Graph registration for Retrosplenial Cortex (RSC) module
 *
 * WHAT: Registers RSC concepts (reference frames, context types, navigation,
 *       imagination) as nodes in the brain's internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about spatial processing state ("which frame is active?")
 *       - Cross-module reasoning about spatial-contextual relationships
 *       - Introspection of reference frame transformations
 *       - Graph-based analysis of spatial navigation dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - RSC root node
 *         +-- Reference Frames subsystem
 *         |   +-- Egocentric frame
 *         |   +-- Allocentric frame
 *         |   +-- Object-centered frame
 *         |   +-- Route-centered frame
 *         +-- Context Types subsystem
 *         |   +-- Spatial context
 *         |   +-- Temporal context
 *         |   +-- Environmental context
 *         |   +-- Social context
 *         |   +-- Emotional context
 *         |   +-- Task context
 *         +-- Navigation subsystem
 *         |   +-- Head direction integration
 *         |   +-- Landmark anchoring
 *         |   +-- Scene recognition
 *         +-- Imagination subsystem
 *             +-- Prospective (future)
 *             +-- Retrospective (past)
 *             +-- Counterfactual (what-if)
 *
 * EDGES: Represent functional relationships:
 *       - Egocentric -> Allocentric (TRANSFORMS_TO)
 *       - Context -> Memory (BINDS_TO)
 *       - Navigation -> Frames (USES)
 *       - Head direction -> Frame transform (MODULATES)
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_RSC_KG_WIRING_H
#define NIMCP_RSC_KG_WIRING_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/nimcp_brain_kg_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define RSC_KG_MODULE_NAME        "rsc_kg_wiring"

/** RSC root node name */
#define RSC_KG_ROOT_NAME          "retrosplenial_cortex"

/** Reference frames subsystem node name */
#define RSC_KG_FRAMES_NAME        "reference_frames"

/** Context types subsystem node name */
#define RSC_KG_CONTEXT_NAME       "context_types"

/** Navigation subsystem node name */
#define RSC_KG_NAVIGATION_NAME    "navigation"

/** Imagination subsystem node name */
#define RSC_KG_IMAGINATION_NAME   "imagination"

//=============================================================================
// Node Type Extensions (RSC-specific concepts)
//=============================================================================

/**
 * @brief RSC-specific KG node types
 *
 * These extend the base brain_kg_node_type_t with RSC-specific types.
 * Values start at 0x2000 to avoid conflicts with core and physics types.
 */
typedef enum {
    /** Reference frame node (egocentric, allocentric, etc.) */
    RSC_KG_NODE_REFERENCE_FRAME = 0x2000,

    /** Context type node (spatial, temporal, etc.) */
    RSC_KG_NODE_CONTEXT_TYPE,

    /** Navigation component node */
    RSC_KG_NODE_NAVIGATION_COMPONENT,

    /** Imagination mode node */
    RSC_KG_NODE_IMAGINATION_MODE,

    /** Landmark node */
    RSC_KG_NODE_LANDMARK,

    /** Scene representation node */
    RSC_KG_NODE_SCENE,

    /** Transformation process node */
    RSC_KG_NODE_TRANSFORM_PROCESS,

    /** Context encoding process node */
    RSC_KG_NODE_ENCODING_PROCESS,

    /** Memory binding node */
    RSC_KG_NODE_MEMORY_BINDING
} rsc_kg_node_type_t;

/**
 * @brief RSC-specific edge types
 */
typedef enum {
    /** Frame transformation relationship */
    RSC_KG_EDGE_TRANSFORMS_TO = 0x2000,

    /** Context-memory binding relationship */
    RSC_KG_EDGE_BINDS_TO,

    /** Functional dependency (A uses B) */
    RSC_KG_EDGE_USES,

    /** Calibration relationship */
    RSC_KG_EDGE_CALIBRATES,

    /** Anchoring relationship (landmark anchoring) */
    RSC_KG_EDGE_ANCHORS,

    /** Scene-context association */
    RSC_KG_EDGE_ASSOCIATES_WITH,

    /** Temporal projection relationship */
    RSC_KG_EDGE_PROJECTS_TO,

    /** Spatial encoding relationship */
    RSC_KG_EDGE_ENCODES,

    /** Integration relationship (combining signals) */
    RSC_KG_EDGE_INTEGRATES
} rsc_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief RSC KG wiring configuration
 */
typedef struct {
    /** Register reference frame nodes */
    bool register_frames;

    /** Register context type nodes */
    bool register_contexts;

    /** Register navigation component nodes */
    bool register_navigation;

    /** Register imagination mode nodes */
    bool register_imagination;

    /** Register transformation edges between frames */
    bool register_transform_edges;

    /** Register context-memory binding edges */
    bool register_context_edges;

    /** Register cross-subsystem edges */
    bool register_cross_edges;

    /** Include state metadata */
    bool include_state_metadata;
} rsc_kg_config_t;

/**
 * @brief RSC KG wiring state (node IDs for reference)
 */
typedef struct {
    /** RSC root node ID */
    brain_kg_node_id_t root_id;

    /* Subsystem node IDs */
    brain_kg_node_id_t frames_subsystem_id;
    brain_kg_node_id_t context_subsystem_id;
    brain_kg_node_id_t navigation_subsystem_id;
    brain_kg_node_id_t imagination_subsystem_id;

    /* Reference frame node IDs */
    brain_kg_node_id_t egocentric_id;
    brain_kg_node_id_t allocentric_id;
    brain_kg_node_id_t object_centered_id;
    brain_kg_node_id_t route_centered_id;

    /* Context type node IDs */
    brain_kg_node_id_t spatial_context_id;
    brain_kg_node_id_t temporal_context_id;
    brain_kg_node_id_t environmental_context_id;
    brain_kg_node_id_t social_context_id;
    brain_kg_node_id_t emotional_context_id;
    brain_kg_node_id_t task_context_id;

    /* Navigation component node IDs */
    brain_kg_node_id_t head_direction_id;
    brain_kg_node_id_t landmarks_id;
    brain_kg_node_id_t scene_recognition_id;

    /* Imagination mode node IDs */
    brain_kg_node_id_t prospective_id;
    brain_kg_node_id_t retrospective_id;
    brain_kg_node_id_t counterfactual_id;

    /** Number of nodes registered */
    uint32_t node_count;

    /** Number of edges registered */
    uint32_t edge_count;

    /** Registration successful */
    bool registered;
} rsc_kg_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default RSC KG wiring configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Provide standard setup for typical RSC KG registration
 * HOW:  Set all registration flags to true
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_default_config(rsc_kg_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register all RSC nodes in KG
 *
 * WHAT: Creates nodes for RSC concepts in the brain's KG
 * WHY:  Enables semantic queries and reasoning about spatial processing
 * HOW:  Creates hierarchical node structure with typed edges
 *
 * @param kg Knowledge graph to register in
 * @param config Registration configuration (NULL for defaults)
 * @param state Output registration state (optional, may be NULL)
 * @param admin_token Admin token for write access
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_register_all(
    brain_kg_t* kg,
    const rsc_kg_config_t* config,
    rsc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register reference frame subsystem nodes
 *
 * WHAT: Creates nodes for spatial reference frames
 * WHY:  Enable queries about frame transformation state
 * HOW:  Create egocentric, allocentric, object-centered, route-centered nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (RSC root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_register_frames(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rsc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register context type subsystem nodes
 *
 * WHAT: Creates nodes for context encoding types
 * WHY:  Enable queries about contextual processing
 * HOW:  Create spatial, temporal, environmental, social, emotional, task nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (RSC root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_register_contexts(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rsc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register navigation subsystem nodes
 *
 * WHAT: Creates nodes for navigation components
 * WHY:  Enable queries about spatial navigation state
 * HOW:  Create head direction, landmarks, scene recognition nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (RSC root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_register_navigation(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rsc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register imagination subsystem nodes
 *
 * WHAT: Creates nodes for imagination/mental simulation modes
 * WHY:  Enable queries about prospective/retrospective processing
 * HOW:  Create prospective, retrospective, counterfactual nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (RSC root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_register_imagination(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rsc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register transformation edges between frames
 *
 * WHAT: Creates edges representing frame transformation relationships
 * WHY:  Represent causal relationships between reference frames
 * HOW:  Egocentric <-> Allocentric, etc.
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_register_transform_edges(
    brain_kg_t* kg,
    rsc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register context-memory binding edges
 *
 * WHAT: Creates edges for context-memory relationships
 * WHY:  Represent how contexts bind to memories
 * HOW:  Context types -> memory binding nodes
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_register_context_edges(
    brain_kg_t* kg,
    rsc_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cross-subsystem edges
 *
 * WHAT: Creates edges between RSC subsystems
 * WHY:  Represents causal relationships across modules
 * HOW:  Navigation -> Frames, Head direction -> Transform, etc.
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_register_cross_edges(
    brain_kg_t* kg,
    rsc_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// State Synchronization API
//=============================================================================

/**
 * @brief Update RSC node metadata with current state
 *
 * WHAT: Synchronizes KG node metadata with RSC state
 * WHY:  Enables queries about current RSC processing values
 * HOW:  Updates node metadata fields
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param transform_accuracy Current frame transform accuracy [0-1]
 * @param context_strength Current context encoding strength [0-1]
 * @param scene_familiarity Current scene familiarity score [0-1]
 * @param head_direction Current head direction (radians)
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_update_state(
    brain_kg_t* kg,
    const rsc_kg_state_t* state,
    float transform_accuracy,
    float context_strength,
    float scene_familiarity,
    float head_direction,
    uint64_t admin_token
);

/**
 * @brief Update active frame in KG
 *
 * WHAT: Mark which reference frame is currently dominant
 * WHY:  Enable queries about current spatial processing mode
 * HOW:  Update frame node metadata
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param active_frame Currently active frame type (0=ego, 1=allo, etc.)
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_update_active_frame(
    brain_kg_t* kg,
    const rsc_kg_state_t* state,
    uint32_t active_frame,
    uint64_t admin_token
);

/**
 * @brief Update imagination state in KG
 *
 * WHAT: Mark current imagination mode and activity
 * WHY:  Enable queries about mental simulation state
 * HOW:  Update imagination node metadata
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param active Whether imagination is active
 * @param mode Current imagination mode (0=prospective, 1=retrospective, etc.)
 * @param vividness Imagination vividness [0-1]
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_update_imagination_state(
    brain_kg_t* kg,
    const rsc_kg_state_t* state,
    bool active,
    uint32_t mode,
    float vividness,
    uint64_t admin_token
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get RSC root node ID
 *
 * @param kg Knowledge graph
 * @return Root node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t rsc_kg_get_root(brain_kg_t* kg);

/**
 * @brief Find RSC subsystem by name
 *
 * @param kg Knowledge graph
 * @param name Subsystem name ("reference_frames", "context_types", etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t rsc_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/**
 * @brief Get all reference frame nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* rsc_kg_get_reference_frames(brain_kg_t* kg);

/**
 * @brief Get all context type nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* rsc_kg_get_context_types(brain_kg_t* kg);

/**
 * @brief Get all navigation component nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* rsc_kg_get_navigation_components(brain_kg_t* kg);

/**
 * @brief Get all imagination mode nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* rsc_kg_get_imagination_modes(brain_kg_t* kg);

/**
 * @brief Find reference frame node by type
 *
 * @param kg Knowledge graph
 * @param frame_type Frame type (0=egocentric, 1=allocentric, etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t rsc_kg_find_frame_by_type(
    brain_kg_t* kg,
    uint32_t frame_type
);

/**
 * @brief Find context type node by type
 *
 * @param kg Knowledge graph
 * @param context_type Context type (0=spatial, 1=temporal, etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t rsc_kg_find_context_by_type(
    brain_kg_t* kg,
    uint32_t context_type
);

//=============================================================================
// Cleanup API
//=============================================================================

/**
 * @brief Unregister all RSC nodes (cleanup)
 *
 * WHAT: Remove all RSC nodes and edges from KG
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Remove nodes in reverse registration order
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_kg_unregister_all(
    brain_kg_t* kg,
    rsc_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// String Conversion API
//=============================================================================

/**
 * @brief Get string name for RSC node type
 *
 * @param type RSC node type
 * @return String representation
 */
NIMCP_EXPORT const char* rsc_kg_node_type_to_string(rsc_kg_node_type_t type);

/**
 * @brief Get string name for RSC edge type
 *
 * @param type RSC edge type
 * @return String representation
 */
NIMCP_EXPORT const char* rsc_kg_edge_type_to_string(rsc_kg_edge_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RSC_KG_WIRING_H */
