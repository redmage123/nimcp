//=============================================================================
// nimcp_claustrum_kg_wiring.h - Claustrum Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_claustrum_kg_wiring.h
 * @brief Knowledge Graph registration for Claustrum module
 *
 * WHAT: Registers claustrum concepts (modalities, binding states, oscillations,
 *       consciousness levels) as nodes in the brain's internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about claustrum state ("which modalities are bound?")
 *       - Cross-module reasoning about consciousness integration
 *       - Introspection of cross-modal binding relationships
 *       - Graph-based analysis of attention and awareness dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - Claustrum root node
 *         +-- Modalities subsystem
 *         |   +-- Visual, Auditory, Somatosensory modality nodes
 *         |   +-- Olfactory, Gustatory, Interoceptive modality nodes
 *         |   +-- Vestibular, Motor_efference modality nodes
 *         +-- States subsystem
 *         |   +-- Binding, Synchronizing, Switching states
 *         |   +-- Broadcasting, Gating states
 *         +-- Consciousness subsystem
 *         |   +-- Unconscious, Preconscious, Conscious, Focal levels
 *         +-- Oscillations subsystem
 *             +-- Gamma oscillation (binding)
 *             +-- Alpha oscillation (gating)
 *
 * EDGES: Represent functional relationships:
 *       - Modality -> Binding state (BINDS_TO)
 *       - Oscillation -> Modality (SYNCHRONIZES)
 *       - State -> Consciousness level (ENABLES)
 *       - Modality <-> Modality (CROSS_BINDS_WITH)
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_CLAUSTRUM_KG_WIRING_H
#define NIMCP_CLAUSTRUM_KG_WIRING_H

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
#define CLAUSTRUM_KG_MODULE_NAME    "claustrum_kg_wiring"

/** Claustrum root node name */
#define CLAUSTRUM_KG_ROOT_NAME      "claustrum"

/** Modalities subsystem node name */
#define CLAUSTRUM_KG_MODALITIES_NAME "claustrum_modalities"

/** States subsystem node name */
#define CLAUSTRUM_KG_STATES_NAME    "claustrum_states"

/** Consciousness subsystem node name */
#define CLAUSTRUM_KG_CONSCIOUSNESS_NAME "claustrum_consciousness"

/** Oscillations subsystem node name */
#define CLAUSTRUM_KG_OSCILLATIONS_NAME "claustrum_oscillations"

//=============================================================================
// Node Type Extensions (for claustrum-specific concepts)
//=============================================================================

/**
 * @brief Claustrum-specific KG node types
 *
 * WHAT: Node type enumeration for claustrum-specific concepts
 * WHY:  Enable typed queries and semantic filtering
 * HOW:  Values start at 0x2000 to avoid conflicts with core/physics types
 */
typedef enum {
    /** Sensory modality input stream */
    CLAUSTRUM_KG_NODE_MODALITY = 0x2000,

    /** Operational state (binding, synchronizing, etc.) */
    CLAUSTRUM_KG_NODE_STATE,

    /** Consciousness level (unconscious to focal) */
    CLAUSTRUM_KG_NODE_CONSCIOUSNESS_LEVEL,

    /** Neural oscillation band */
    CLAUSTRUM_KG_NODE_OSCILLATION,

    /** Bound percept (unified cross-modal representation) */
    CLAUSTRUM_KG_NODE_PERCEPT,

    /** Brain state (default mode, task-positive, etc.) */
    CLAUSTRUM_KG_NODE_BRAIN_STATE,

    /** Cortical region link */
    CLAUSTRUM_KG_NODE_CORTICAL_LINK,

    /** Attention bias target */
    CLAUSTRUM_KG_NODE_ATTENTION_TARGET
} claustrum_kg_node_type_t;

/**
 * @brief Claustrum-specific edge types
 *
 * WHAT: Edge type enumeration for claustrum relationships
 * WHY:  Represent binding, synchronization, and consciousness relationships
 * HOW:  Values start at 0x2000 to avoid conflicts
 */
typedef enum {
    /** Cross-modal binding relationship */
    CLAUSTRUM_KG_EDGE_BINDS_TO = 0x2000,

    /** Temporal synchronization */
    CLAUSTRUM_KG_EDGE_SYNCHRONIZES,

    /** Enables state transition */
    CLAUSTRUM_KG_EDGE_ENABLES,

    /** Cross-modal binding between modalities */
    CLAUSTRUM_KG_EDGE_CROSS_BINDS_WITH,

    /** Gates access to workspace */
    CLAUSTRUM_KG_EDGE_GATES,

    /** Broadcasts to workspace */
    CLAUSTRUM_KG_EDGE_BROADCASTS_TO,

    /** Attention modulation */
    CLAUSTRUM_KG_EDGE_ATTENDS_TO,

    /** State switching relationship */
    CLAUSTRUM_KG_EDGE_SWITCHES_TO,

    /** Oscillation coupling */
    CLAUSTRUM_KG_EDGE_COUPLES_OSCILLATION
} claustrum_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief KG wiring configuration for claustrum
 *
 * WHAT: Configuration options for claustrum KG registration
 * WHY:  Allow selective registration based on system capabilities
 * HOW:  Boolean flags for each subsystem
 */
typedef struct {
    /** Register all modality nodes */
    bool register_modalities;

    /** Register state nodes */
    bool register_states;

    /** Register consciousness level nodes */
    bool register_consciousness;

    /** Register oscillation nodes */
    bool register_oscillations;

    /** Register cross-modal binding edges */
    bool register_binding_edges;

    /** Register inter-subsystem edges */
    bool register_cross_edges;

    /** Include state metadata for queries */
    bool include_state_metadata;
} claustrum_kg_config_t;

/**
 * @brief KG wiring state (node IDs for reference)
 *
 * WHAT: Cached node IDs for efficient state updates
 * WHY:  Avoid repeated lookups during runtime
 * HOW:  Store IDs after registration
 */
typedef struct {
    /** Claustrum root node ID */
    brain_kg_node_id_t root_id;

    /** Modalities subsystem node ID */
    brain_kg_node_id_t modalities_id;

    /** Individual modality node IDs */
    brain_kg_node_id_t visual_id;
    brain_kg_node_id_t auditory_id;
    brain_kg_node_id_t somatosensory_id;
    brain_kg_node_id_t olfactory_id;
    brain_kg_node_id_t gustatory_id;
    brain_kg_node_id_t interoceptive_id;
    brain_kg_node_id_t vestibular_id;
    brain_kg_node_id_t motor_efference_id;

    /** States subsystem node ID */
    brain_kg_node_id_t states_id;

    /** Individual state node IDs */
    brain_kg_node_id_t binding_state_id;
    brain_kg_node_id_t synchronizing_state_id;
    brain_kg_node_id_t switching_state_id;
    brain_kg_node_id_t broadcasting_state_id;
    brain_kg_node_id_t gating_state_id;

    /** Consciousness subsystem node ID */
    brain_kg_node_id_t consciousness_id;

    /** Individual consciousness level node IDs */
    brain_kg_node_id_t unconscious_id;
    brain_kg_node_id_t preconscious_id;
    brain_kg_node_id_t conscious_id;
    brain_kg_node_id_t focal_id;

    /** Oscillations subsystem node ID */
    brain_kg_node_id_t oscillations_id;

    /** Individual oscillation node IDs */
    brain_kg_node_id_t gamma_id;
    brain_kg_node_id_t alpha_id;

    /** Number of nodes registered */
    uint32_t node_count;

    /** Number of edges registered */
    uint32_t edge_count;

    /** Registration successful */
    bool registered;
} claustrum_kg_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default KG wiring configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Easy initialization with all subsystems enabled
 * HOW:  Set all registration flags to true
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_kg_default_config(claustrum_kg_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register all claustrum nodes in KG
 *
 * WHAT: Creates nodes for claustrum concepts in the brain's KG
 * WHY:  Enables semantic queries and reasoning about claustrum
 * HOW:  Creates hierarchical node structure with typed edges
 *
 * @param kg Knowledge graph to register in
 * @param config Registration configuration (NULL for defaults)
 * @param state Output registration state (optional, may be NULL)
 * @param admin_token Admin token for write access
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_kg_register_all(
    brain_kg_t* kg,
    const claustrum_kg_config_t* config,
    claustrum_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register modality subsystem nodes
 *
 * WHAT: Creates nodes for each sensory modality
 * WHY:  Enable queries about modality state and binding
 * HOW:  Creates modality container and child nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (claustrum root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_kg_register_modalities(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    claustrum_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register state subsystem nodes
 *
 * WHAT: Creates nodes for operational states
 * WHY:  Enable queries about current processing state
 * HOW:  Creates state container and child nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (claustrum root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_kg_register_states(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    claustrum_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register consciousness level nodes
 *
 * WHAT: Creates nodes for consciousness levels
 * WHY:  Enable queries about awareness state
 * HOW:  Creates consciousness container and level nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (claustrum root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_kg_register_consciousness(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    claustrum_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register oscillation nodes
 *
 * WHAT: Creates nodes for neural oscillation bands
 * WHY:  Enable queries about synchronization state
 * HOW:  Creates oscillation container with gamma/alpha nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (claustrum root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_kg_register_oscillations(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    claustrum_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cross-modal binding edges
 *
 * WHAT: Creates edges between modalities for binding relationships
 * WHY:  Represent cross-modal integration pathways
 * HOW:  Connect modalities with CROSS_BINDS_WITH edges
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_kg_register_binding_edges(
    brain_kg_t* kg,
    claustrum_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cross-subsystem edges
 *
 * WHAT: Creates edges between claustrum subsystems
 * WHY:  Represents functional relationships across components
 * HOW:  Oscillations -> Modalities, States -> Consciousness, etc.
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_kg_register_cross_edges(
    brain_kg_t* kg,
    claustrum_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// State Synchronization API
//=============================================================================

/**
 * @brief Update claustrum node metadata with current state
 *
 * WHAT: Synchronizes KG node metadata with claustrum state
 * WHY:  Enables queries about current claustrum values
 * HOW:  Updates node metadata fields
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param binding_strength Current binding strength [0-1]
 * @param gamma_coherence Current gamma coherence [0-1]
 * @param consciousness_level Current consciousness level
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_kg_update_state(
    brain_kg_t* kg,
    const claustrum_kg_state_t* state,
    float binding_strength,
    float gamma_coherence,
    uint32_t consciousness_level,
    uint64_t admin_token
);

/**
 * @brief Update modality activity in KG
 *
 * WHAT: Updates activity level metadata for a modality
 * WHY:  Enable queries about which modalities are active
 * HOW:  Updates metadata on modality node
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param modality Modality index (0-7)
 * @param activity Activity level [0-1]
 * @param bound Whether modality is currently bound
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_kg_update_modality(
    brain_kg_t* kg,
    const claustrum_kg_state_t* state,
    uint32_t modality,
    float activity,
    bool bound,
    uint64_t admin_token
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get claustrum root node ID
 *
 * WHAT: Find the claustrum root node
 * WHY:  Entry point for claustrum queries
 * HOW:  Lookup by name
 *
 * @param kg Knowledge graph
 * @return Root node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t claustrum_kg_get_root(brain_kg_t* kg);

/**
 * @brief Find claustrum subsystem by name
 *
 * WHAT: Find a claustrum subsystem node
 * WHY:  Enable targeted subsystem queries
 * HOW:  Lookup by name
 *
 * @param kg Knowledge graph
 * @param name Subsystem name ("modalities", "states", etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t claustrum_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/**
 * @brief Get all modality nodes
 *
 * WHAT: Retrieve all modality nodes
 * WHY:  Enable queries across all modalities
 * HOW:  Filter by claustrum modality node type
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* claustrum_kg_get_modalities(brain_kg_t* kg);

/**
 * @brief Get active modality nodes
 *
 * WHAT: Retrieve modalities with activity above threshold
 * WHY:  Find currently active sensory inputs
 * HOW:  Filter by activity metadata
 *
 * @param kg Knowledge graph
 * @param min_activity Minimum activity threshold [0-1]
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* claustrum_kg_get_active_modalities(
    brain_kg_t* kg,
    float min_activity
);

/**
 * @brief Get bound modality nodes
 *
 * WHAT: Retrieve modalities currently in binding
 * WHY:  Query cross-modal binding state
 * HOW:  Filter by bound metadata flag
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* claustrum_kg_get_bound_modalities(
    brain_kg_t* kg
);

/**
 * @brief Get current consciousness level node
 *
 * WHAT: Find the currently active consciousness level
 * WHY:  Query awareness state
 * HOW:  Check active metadata on consciousness nodes
 *
 * @param kg Knowledge graph
 * @return Node ID of active level or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t claustrum_kg_get_current_consciousness(
    brain_kg_t* kg
);

/**
 * @brief Unregister all claustrum nodes (cleanup)
 *
 * WHAT: Remove all claustrum nodes from KG
 * WHY:  Clean shutdown and resource release
 * HOW:  Remove nodes in reverse creation order
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_kg_unregister_all(
    brain_kg_t* kg,
    claustrum_kg_state_t* state,
    uint64_t admin_token
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CLAUSTRUM_KG_WIRING_H */
