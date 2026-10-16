//=============================================================================
// nimcp_physics_kg_wiring.h - Physics Layer Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_physics_kg_wiring.h
 * @brief Knowledge Graph registration for Phase 1 Physics modules
 *
 * WHAT: Registers physics layer concepts (ion channels, thermodynamic processes,
 *       ephaptic fields) as nodes in the brain's internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about physics state ("which channels are active?")
 *       - Cross-module reasoning about physical constraints
 *       - Introspection of biophysical relationships
 *       - Graph-based analysis of neural dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - Physics layer root node
 *         ├── Hodgkin-Huxley subsystem
 *         │   ├── Ion channel types (Na, K, Leak)
 *         │   ├── Gating variables (m, h, n)
 *         │   └── Population dynamics
 *         ├── Thermodynamics subsystem
 *         │   ├── Heat flow processes
 *         │   ├── ATP metabolism
 *         │   └── Entropy production
 *         └── Ephaptic coupling subsystem
 *             ├── LFP generation
 *             ├── Phase synchronization
 *             └── Field propagation
 *
 * EDGES: Represent causal/functional relationships:
 *       - Ion channels → membrane potential (CAUSES)
 *       - Temperature → channel kinetics (MODULATES)
 *       - Ephaptic field → membrane potential (INFLUENCES)
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PHYSICS_KG_WIRING_H
#define NIMCP_PHYSICS_KG_WIRING_H

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
#define PHYSICS_KG_MODULE_NAME    "physics_kg_wiring"

/** Physics layer root node name */
#define PHYSICS_KG_ROOT_NAME      "physics_layer"

/** Hodgkin-Huxley subsystem node name */
#define PHYSICS_KG_HH_NAME        "hodgkin_huxley"

/** Thermodynamics subsystem node name */
#define PHYSICS_KG_THERMO_NAME    "thermodynamics"

/** Ephaptic coupling subsystem node name */
#define PHYSICS_KG_EPHAPTIC_NAME  "ephaptic_coupling"

//=============================================================================
// Node Type Extensions (for physics-specific concepts)
//=============================================================================

/**
 * @brief Physics-specific KG node types
 *
 * These extend the base brain_kg_node_type_t with physics-layer specific types.
 * Values start at 0x1000 to avoid conflicts with core types.
 */
typedef enum {
    /** Ion channel type (Na, K, Leak, Ca, etc.) */
    PHYSICS_KG_NODE_ION_CHANNEL = 0x1000,

    /** Gating variable (m, h, n, etc.) */
    PHYSICS_KG_NODE_GATING_VAR,

    /** Thermodynamic process (heat flow, ATP synthesis, etc.) */
    PHYSICS_KG_NODE_THERMO_PROCESS,

    /** Energy pool (ATP, heat reservoir, etc.) */
    PHYSICS_KG_NODE_ENERGY_POOL,

    /** Field type (LFP, ephaptic, etc.) */
    PHYSICS_KG_NODE_FIELD,

    /** Oscillator/phase variable */
    PHYSICS_KG_NODE_OSCILLATOR,

    /** Physical constraint */
    PHYSICS_KG_NODE_CONSTRAINT,

    /** Measurable quantity */
    PHYSICS_KG_NODE_MEASUREMENT
} physics_kg_node_type_t;

/**
 * @brief Physics-specific edge types
 */
typedef enum {
    /** Direct causal relationship */
    PHYSICS_KG_EDGE_CAUSES = 0x1000,

    /** Modulatory relationship */
    PHYSICS_KG_EDGE_MODULATES,

    /** Influence without direct causation */
    PHYSICS_KG_EDGE_INFLUENCES,

    /** Constraint relationship */
    PHYSICS_KG_EDGE_CONSTRAINS,

    /** Coupling relationship */
    PHYSICS_KG_EDGE_COUPLES_TO,

    /** Energy transfer relationship */
    PHYSICS_KG_EDGE_TRANSFERS_ENERGY,

    /** Temporal precedence */
    PHYSICS_KG_EDGE_PRECEDES
} physics_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief KG wiring configuration
 */
typedef struct {
    /** Register Hodgkin-Huxley nodes */
    bool register_hh;

    /** Register thermodynamics nodes */
    bool register_thermo;

    /** Register ephaptic nodes */
    bool register_ephaptic;

    /** Register individual ion channel types */
    bool register_channel_details;

    /** Register inter-subsystem edges */
    bool register_cross_edges;

    /** Include state metadata */
    bool include_state_metadata;
} physics_kg_config_t;

/**
 * @brief KG wiring state (node IDs for reference)
 */
typedef struct {
    /** Physics layer root node ID */
    brain_kg_node_id_t root_id;

    /** Hodgkin-Huxley subsystem node ID */
    brain_kg_node_id_t hh_id;

    /** Na channel node ID */
    brain_kg_node_id_t na_channel_id;

    /** K channel node ID */
    brain_kg_node_id_t k_channel_id;

    /** Leak channel node ID */
    brain_kg_node_id_t leak_channel_id;

    /** Thermodynamics subsystem node ID */
    brain_kg_node_id_t thermo_id;

    /** ATP pool node ID */
    brain_kg_node_id_t atp_id;

    /** Heat pool node ID */
    brain_kg_node_id_t heat_id;

    /** Ephaptic coupling subsystem node ID */
    brain_kg_node_id_t ephaptic_id;

    /** LFP node ID */
    brain_kg_node_id_t lfp_id;

    /** Phase sync node ID */
    brain_kg_node_id_t phase_sync_id;

    /** Number of nodes registered */
    uint32_t node_count;

    /** Number of edges registered */
    uint32_t edge_count;

    /** Registration successful */
    bool registered;
} physics_kg_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default KG wiring configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_kg_default_config(physics_kg_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register all physics layer nodes in KG
 *
 * WHAT: Creates nodes for physics concepts in the brain's KG
 * WHY:  Enables semantic queries and reasoning about physics
 * HOW:  Creates hierarchical node structure with typed edges
 *
 * @param kg Knowledge graph to register in
 * @param config Registration configuration (NULL for defaults)
 * @param state Output registration state (optional, may be NULL)
 * @param admin_token Admin token for write access
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_kg_register_all(
    brain_kg_t* kg,
    const physics_kg_config_t* config,
    physics_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register Hodgkin-Huxley subsystem nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (physics root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_kg_register_hh(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    physics_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register thermodynamics subsystem nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (physics root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_kg_register_thermo(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    physics_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register ephaptic coupling subsystem nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (physics root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_kg_register_ephaptic(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    physics_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cross-subsystem edges
 *
 * WHAT: Creates edges between physics subsystems
 * WHY:  Represents causal relationships across modules
 * HOW:  Temperature → HH kinetics, ATP → channels, etc.
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_kg_register_cross_edges(
    brain_kg_t* kg,
    physics_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// State Synchronization API
//=============================================================================

/**
 * @brief Update physics node metadata with current state
 *
 * WHAT: Synchronizes KG node metadata with physics state
 * WHY:  Enables queries about current physics values
 * HOW:  Updates node metadata fields
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param temperature Current temperature (K)
 * @param atp_level Current ATP level (0-1)
 * @param lfp_amplitude Current LFP amplitude (mV)
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_kg_update_state(
    brain_kg_t* kg,
    const physics_kg_state_t* state,
    float temperature,
    float atp_level,
    float lfp_amplitude,
    uint64_t admin_token
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get physics layer root node ID
 *
 * @param kg Knowledge graph
 * @return Root node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t physics_kg_get_root(brain_kg_t* kg);

/**
 * @brief Find physics subsystem by name
 *
 * @param kg Knowledge graph
 * @param name Subsystem name ("hodgkin_huxley", "thermodynamics", etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t physics_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/**
 * @brief Get all ion channel nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* physics_kg_get_ion_channels(brain_kg_t* kg);

/**
 * @brief Unregister all physics nodes (cleanup)
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_kg_unregister_all(
    brain_kg_t* kg,
    physics_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// Wave W15: Runtime event emission + query path
//
// These public triggers forward to file-scope static emit functions in the
// physics subsystem .c files. Emit is rate-limited or mode-change gated —
// never per-neuron, per-tick, or per-spike (see audit risk 4).
//=============================================================================

/* Forward-declared brain handle (full struct lives in brain_internal.h). */
#ifndef NIMCP_BRAIN_T_DEFINED
#define NIMCP_BRAIN_T_DEFINED
typedef struct brain_struct* brain_t;
#endif

/**
 * @brief Register brain handle for physics KG wiring runtime emit.
 *
 * Installs a file-scope backpointer used by `physics_kg_emit_state_summary`
 * so the summary function can self-elevate the admin token when called from
 * a subsystem that only has a `brain_kg_t*` and not a full `brain_t`.
 */
NIMCP_EXPORT void physics_kg_wiring_register_brain(brain_t brain);

/**
 * @brief Emit an aggregated physics-wide state summary event.
 *
 * WHAT:  Creates a single event node `physics_event_state_summary_<ts_us>`
 *        with metadata (temperature, atp_level, lfp_amplitude, dominant
 *        subsystem) and edges back to the physics_layer root + each active
 *        subsystem node.
 * WHY:   Cross-subsystem snapshot at epoch boundaries (e.g. every N ticks
 *        or on mode change); aggregated to avoid KG firehose.
 * WHEN:  Call from a training-epoch boundary, sleep stage transition, or
 *        when any subsystem reports a mode change.
 *
 * @param brain       Brain (handle used to reach internal_kg + admin token)
 * @param temperature Current temperature (K), or NaN to skip
 * @param atp_level   Current ATP level [0,1], or NaN to skip
 * @param lfp_amp     Current LFP amplitude (mV), or NaN to skip
 * @param trigger     Free-text description of why this summary was emitted
 * @param ts_us       Timestamp in microseconds (unique per call)
 */
NIMCP_EXPORT void physics_kg_trigger_state_summary(
    brain_t brain,
    float temperature,
    float atp_level,
    float lfp_amp,
    const char* trigger,
    uint64_t ts_us
);

/**
 * @brief Read-path: count physics event nodes of a given kind.
 *
 * Walks outgoing edges of the `physics_layer` root to find event nodes
 * whose description contains the given kind substring. Returns 0 when the
 * KG is missing/disabled.
 */
NIMCP_EXPORT uint32_t physics_kg_count_events(brain_kg_t* kg,
                                              const char* kind_substr);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_KG_WIRING_H */
