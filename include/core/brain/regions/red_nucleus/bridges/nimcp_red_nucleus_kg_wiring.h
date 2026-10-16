//=============================================================================
// nimcp_red_nucleus_kg_wiring.h - Red Nucleus Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_red_nucleus_kg_wiring.h
 * @brief Knowledge Graph registration for Red Nucleus motor coordination module
 *
 * WHAT: Registers Red Nucleus concepts (subdivisions, motor commands, errors,
 *       cerebellar connections) as nodes in the brain's internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about motor state ("which effectors are active?")
 *       - Cross-module reasoning about motor coordination
 *       - Introspection of cerebellar-rubral relationships
 *       - Graph-based analysis of motor learning dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - Red Nucleus root node
 *         +-- Subdivisions subsystem
 *         |   +-- Magnocellular (rubrospinal tract)
 *         |   +-- Parvocellular (rubro-olivary)
 *         +-- Motor commands subsystem
 *         |   +-- Velocity, Force, Position commands
 *         |   +-- Trajectory, Posture, Balance commands
 *         |   +-- Skilled movement commands
 *         +-- Effectors subsystem
 *         |   +-- Forelimb proximal/distal
 *         |   +-- Hindlimb proximal/distal
 *         |   +-- Axial musculature
 *         +-- Error types subsystem
 *         |   +-- Position, Velocity, Force errors
 *         |   +-- Timing, Trajectory errors
 *         +-- Cerebellar connections subsystem
 *             +-- Dentate input
 *             +-- Olivary output
 *             +-- Thalamic output
 *
 * EDGES: Represent causal/functional relationships:
 *       - Subdivision -> Motor command (CONTROLS)
 *       - Error -> Learning (TRIGGERS)
 *       - Cerebellar -> Motor (MODULATES)
 *       - Dentate -> Parvocellular (PROJECTS_TO)
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_RED_NUCLEUS_KG_WIRING_H
#define NIMCP_RED_NUCLEUS_KG_WIRING_H

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
#define RN_KG_MODULE_NAME       "red_nucleus_kg_wiring"

/** Red Nucleus root node name */
#define RN_KG_ROOT_NAME         "red_nucleus"

/** Subdivisions subsystem node name */
#define RN_KG_SUBDIV_NAME       "rn_subdivisions"

/** Motor commands subsystem node name */
#define RN_KG_MOTOR_NAME        "rn_motor_commands"

/** Effectors subsystem node name */
#define RN_KG_EFFECTOR_NAME     "rn_effectors"

/** Error types subsystem node name */
#define RN_KG_ERROR_NAME        "rn_error_types"

/** Cerebellar connections subsystem node name */
#define RN_KG_CEREBELLAR_NAME   "rn_cerebellar_connections"

//=============================================================================
// Node Type Extensions (for Red Nucleus-specific concepts)
//=============================================================================

/**
 * @brief Red Nucleus-specific KG node types
 *
 * These extend the base brain_kg_node_type_t with Red Nucleus-layer specific types.
 * Values start at 0x3100 to avoid conflicts with core types and physics types.
 */
typedef enum {
    /** Red Nucleus subdivision (magnocellular, parvocellular) */
    RN_KG_NODE_SUBDIVISION = 0x3100,

    /** Motor command type (velocity, force, position, etc.) */
    RN_KG_NODE_MOTOR_CMD,

    /** Motor effector (forelimb, hindlimb, axial) */
    RN_KG_NODE_EFFECTOR,

    /** Motor error type (position, velocity, force, timing, trajectory) */
    RN_KG_NODE_ERROR_TYPE,

    /** Cerebellar connection type (dentate, olivary, thalamic) */
    RN_KG_NODE_CEREBELLAR_CONN,

    /** Motor learning state */
    RN_KG_NODE_LEARNING_STATE,

    /** Motor trajectory */
    RN_KG_NODE_TRAJECTORY,

    /** Rubrospinal output */
    RN_KG_NODE_RUBROSPINAL_OUTPUT
} rn_kg_node_type_t;

/**
 * @brief Red Nucleus-specific edge types
 */
typedef enum {
    /** Subdivision controls motor command */
    RN_KG_EDGE_CONTROLS = 0x3100,

    /** Error triggers learning */
    RN_KG_EDGE_TRIGGERS_LEARNING,

    /** Cerebellar structure projects to target */
    RN_KG_EDGE_PROJECTS_TO,

    /** Motor command targets effector */
    RN_KG_EDGE_TARGETS,

    /** Error affects effector */
    RN_KG_EDGE_AFFECTS,

    /** Learning modifies command */
    RN_KG_EDGE_MODIFIES,

    /** Dentate input activates subdivision */
    RN_KG_EDGE_ACTIVATES,

    /** Subdivision generates output */
    RN_KG_EDGE_GENERATES,

    /** Error feedback from cerebellar loop */
    RN_KG_EDGE_PROVIDES_FEEDBACK
} rn_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief KG wiring configuration for Red Nucleus
 */
typedef struct {
    /** Register subdivision nodes */
    bool register_subdivisions;

    /** Register motor command nodes */
    bool register_motor_cmds;

    /** Register effector nodes */
    bool register_effectors;

    /** Register error type nodes */
    bool register_errors;

    /** Register cerebellar connection nodes */
    bool register_cerebellar;

    /** Register inter-subsystem edges */
    bool register_cross_edges;

    /** Include state metadata */
    bool include_state_metadata;
} rn_kg_config_t;

/**
 * @brief KG wiring state (node IDs for reference)
 */
typedef struct {
    /** Red Nucleus root node ID */
    brain_kg_node_id_t root_id;

    /** Subdivisions subsystem node ID */
    brain_kg_node_id_t subdiv_subsystem_id;

    /** Magnocellular subdivision node ID */
    brain_kg_node_id_t magnocellular_id;

    /** Parvocellular subdivision node ID */
    brain_kg_node_id_t parvocellular_id;

    /** Motor commands subsystem node ID */
    brain_kg_node_id_t motor_subsystem_id;

    /** Motor command node IDs */
    brain_kg_node_id_t cmd_velocity_id;
    brain_kg_node_id_t cmd_force_id;
    brain_kg_node_id_t cmd_position_id;
    brain_kg_node_id_t cmd_trajectory_id;
    brain_kg_node_id_t cmd_posture_id;
    brain_kg_node_id_t cmd_balance_id;
    brain_kg_node_id_t cmd_skilled_id;

    /** Effectors subsystem node ID */
    brain_kg_node_id_t effector_subsystem_id;

    /** Effector node IDs */
    brain_kg_node_id_t forelimb_proximal_id;
    brain_kg_node_id_t forelimb_distal_id;
    brain_kg_node_id_t hindlimb_proximal_id;
    brain_kg_node_id_t hindlimb_distal_id;
    brain_kg_node_id_t axial_id;

    /** Error types subsystem node ID */
    brain_kg_node_id_t error_subsystem_id;

    /** Error type node IDs */
    brain_kg_node_id_t error_position_id;
    brain_kg_node_id_t error_velocity_id;
    brain_kg_node_id_t error_force_id;
    brain_kg_node_id_t error_timing_id;
    brain_kg_node_id_t error_trajectory_id;

    /** Cerebellar connections subsystem node ID */
    brain_kg_node_id_t cerebellar_subsystem_id;

    /** Cerebellar connection node IDs */
    brain_kg_node_id_t dentate_input_id;
    brain_kg_node_id_t olivary_output_id;
    brain_kg_node_id_t thalamic_output_id;

    /** Number of nodes registered */
    uint32_t node_count;

    /** Number of edges registered */
    uint32_t edge_count;

    /** Registration successful */
    bool registered;
} rn_kg_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default KG wiring configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_default_config(rn_kg_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register all Red Nucleus nodes in KG
 *
 * WHAT: Creates nodes for Red Nucleus concepts in the brain's KG
 * WHY:  Enables semantic queries and reasoning about motor coordination
 * HOW:  Creates hierarchical node structure with typed edges
 *
 * @param kg Knowledge graph to register in
 * @param config Registration configuration (NULL for defaults)
 * @param state Output registration state (optional, may be NULL)
 * @param admin_token Admin token for write access
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_register_all(
    brain_kg_t* kg,
    const rn_kg_config_t* config,
    rn_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register subdivisions subsystem nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (Red Nucleus root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_register_subdivisions(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rn_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register motor commands subsystem nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (Red Nucleus root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_register_motor(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rn_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register effectors subsystem nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (Red Nucleus root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_register_effectors(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rn_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register error types subsystem nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (Red Nucleus root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_register_errors(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rn_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cerebellar connections subsystem nodes
 *
 * @param kg Knowledge graph
 * @param parent_id Parent node (Red Nucleus root)
 * @param state Output state
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_register_cerebellar(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rn_kg_state_t* state,
    uint64_t admin_token
);

/**
 * @brief Register cross-subsystem edges
 *
 * WHAT: Creates edges between Red Nucleus subsystems
 * WHY:  Represents causal relationships across motor control components
 * HOW:  Subdivision->command, error->learning, cerebellar->motor
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_register_cross_edges(
    brain_kg_t* kg,
    rn_kg_state_t* state,
    uint64_t admin_token
);

//=============================================================================
// State Synchronization API
//=============================================================================

/**
 * @brief Update Red Nucleus node metadata with current state
 *
 * WHAT: Synchronizes KG node metadata with Red Nucleus state
 * WHY:  Enables queries about current motor activity
 * HOW:  Updates node metadata fields
 *
 * @param kg Knowledge graph
 * @param state KG wiring state
 * @param magno_activity Magnocellular activity level [0, 1]
 * @param parvo_activity Parvocellular activity level [0, 1]
 * @param avg_error Average motor error magnitude
 * @param skill_level Current skill level [0, 1]
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_update_state(
    brain_kg_t* kg,
    const rn_kg_state_t* state,
    float magno_activity,
    float parvo_activity,
    float avg_error,
    float skill_level,
    uint64_t admin_token
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get Red Nucleus root node ID
 *
 * @param kg Knowledge graph
 * @return Root node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t rn_kg_get_root(brain_kg_t* kg);

/**
 * @brief Find Red Nucleus subsystem by name
 *
 * @param kg Knowledge graph
 * @param name Subsystem name ("rn_subdivisions", "rn_motor_commands", etc.)
 * @return Node ID or BRAIN_KG_INVALID_NODE
 */
NIMCP_EXPORT brain_kg_node_id_t rn_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/**
 * @brief Get all subdivision nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* rn_kg_get_subdivisions(brain_kg_t* kg);

/**
 * @brief Get all motor command nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* rn_kg_get_motor_cmds(brain_kg_t* kg);

/**
 * @brief Get all effector nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* rn_kg_get_effectors(brain_kg_t* kg);

/**
 * @brief Get all error type nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* rn_kg_get_error_types(brain_kg_t* kg);

/**
 * @brief Get all cerebellar connection nodes
 *
 * @param kg Knowledge graph
 * @return Node list (caller must free) or NULL
 */
NIMCP_EXPORT brain_kg_node_list_t* rn_kg_get_cerebellar_conns(brain_kg_t* kg);

/**
 * @brief Unregister all Red Nucleus nodes (cleanup)
 *
 * @param kg Knowledge graph
 * @param state State with node IDs
 * @param admin_token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_unregister_all(
    brain_kg_t* kg,
    rn_kg_state_t* state,
    uint64_t admin_token
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RED_NUCLEUS_KG_WIRING_H */
