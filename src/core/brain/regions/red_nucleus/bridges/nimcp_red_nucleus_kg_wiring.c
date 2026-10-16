//=============================================================================
// nimcp_red_nucleus_kg_wiring.c - Red Nucleus Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_red_nucleus_kg_wiring.c
 * @brief Implementation of Red Nucleus KG registration
 */

#include "core/brain/regions/red_nucleus/bridges/nimcp_red_nucleus_kg_wiring.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(red_nucleus_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create a Red Nucleus node with description
 */
static brain_kg_node_id_t create_rn_node(
    brain_kg_t* kg,
    const char* name,
    brain_kg_node_type_t type,
    const char* description,
    uint64_t admin_token
) {
    (void)admin_token;  /* Not used in current API */
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;

    brain_kg_node_id_t id = brain_kg_add_node(kg, name, type, description);
    if (id != BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_DEBUG(RN_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

/**
 * @brief Create an edge between Red Nucleus nodes
 */
static brain_kg_edge_id_t create_rn_edge(
    brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    brain_kg_edge_type_t type,
    const char* description,
    float weight,
    uint64_t admin_token
) {
    (void)admin_token;  /* Not used in current API */
    if (!kg) return BRAIN_KG_INVALID_NODE;
    if (from == BRAIN_KG_INVALID_NODE || to == BRAIN_KG_INVALID_NODE) {
        return BRAIN_KG_INVALID_NODE;
    }

    brain_kg_edge_id_t id = brain_kg_add_edge(
        kg, from, to, type, description, weight
    );
    return id;
}

//=============================================================================
// Configuration API
//=============================================================================

int rn_kg_default_config(rn_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_default_config: config is NULL");
        return -1;
    }

    config->register_subdivisions = true;
    config->register_motor_cmds = true;
    config->register_effectors = true;
    config->register_errors = true;
    config->register_cerebellar = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int rn_kg_register_all(
    brain_kg_t* kg,
    const rn_kg_config_t* config,
    rn_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_register_all: kg is NULL");
        return -1;
    }

    rn_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        rn_kg_default_config(&local_config);
    }

    rn_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create Red Nucleus root node */
    local_state.root_id = create_rn_node(
        kg, RN_KG_ROOT_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Red Nucleus - Motor coordination and motor learning center",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(RN_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rn_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Register subsystems */
    if (local_config.register_subdivisions) {
        if (rn_kg_register_subdivisions(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(RN_KG_MODULE_NAME, "Failed to register subdivisions subsystem");
        }
    }

    if (local_config.register_motor_cmds) {
        if (rn_kg_register_motor(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(RN_KG_MODULE_NAME, "Failed to register motor commands subsystem");
        }
    }

    if (local_config.register_effectors) {
        if (rn_kg_register_effectors(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(RN_KG_MODULE_NAME, "Failed to register effectors subsystem");
        }
    }

    if (local_config.register_errors) {
        if (rn_kg_register_errors(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(RN_KG_MODULE_NAME, "Failed to register error types subsystem");
        }
    }

    if (local_config.register_cerebellar) {
        if (rn_kg_register_cerebellar(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(RN_KG_MODULE_NAME, "Failed to register cerebellar connections subsystem");
        }
    }

    /* Register cross-subsystem edges */
    if (local_config.register_cross_edges) {
        rn_kg_register_cross_edges(kg, &local_state, admin_token);
    }

    local_state.registered = true;

    /* Copy to output if requested */
    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(RN_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

int rn_kg_register_subdivisions(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rn_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_register_subdivisions: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create subdivisions subsystem node */
    state->subdiv_subsystem_id = create_rn_node(
        kg, RN_KG_SUBDIV_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Red Nucleus subdivisions - Magnocellular and Parvocellular regions",
        admin_token
    );
    if (state->subdiv_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rn_kg_register_subdivisions: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, parent_id, state->subdiv_subsystem_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains subdivisions", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create magnocellular subdivision node */
    state->magnocellular_id = create_rn_node(
        kg, "rn_magnocellular",
        (brain_kg_node_type_t)RN_KG_NODE_SUBDIVISION,
        "Magnocellular (RNm) - Large neurons, rubrospinal tract origin, forelimb control",
        admin_token
    );
    if (state->magnocellular_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->subdiv_subsystem_id, state->magnocellular_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains magnocellular", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create parvocellular subdivision node */
    state->parvocellular_id = create_rn_node(
        kg, "rn_parvocellular",
        (brain_kg_node_type_t)RN_KG_NODE_SUBDIVISION,
        "Parvocellular (RNp) - Small neurons, rubro-olivary projection, motor learning",
        admin_token
    );
    if (state->parvocellular_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->subdiv_subsystem_id, state->parvocellular_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains parvocellular", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create inter-subdivision edge */
    if (state->magnocellular_id != BRAIN_KG_INVALID_NODE &&
        state->parvocellular_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->magnocellular_id, state->parvocellular_id,
            BRAIN_KG_EDGE_COORDINATES_WITH,
            "magnocellular coordinates with parvocellular for motor learning", 0.7f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int rn_kg_register_motor(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rn_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_register_motor: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create motor commands subsystem node */
    state->motor_subsystem_id = create_rn_node(
        kg, RN_KG_MOTOR_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Red Nucleus motor commands - Velocity, force, position, trajectory control",
        admin_token
    );
    if (state->motor_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rn_kg_register_motor: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, parent_id, state->motor_subsystem_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains motor commands", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create velocity command node */
    state->cmd_velocity_id = create_rn_node(
        kg, "rn_cmd_velocity",
        (brain_kg_node_type_t)RN_KG_NODE_MOTOR_CMD,
        "Velocity command - Movement speed modulation",
        admin_token
    );
    if (state->cmd_velocity_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->motor_subsystem_id, state->cmd_velocity_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains velocity command", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create force command node */
    state->cmd_force_id = create_rn_node(
        kg, "rn_cmd_force",
        (brain_kg_node_type_t)RN_KG_NODE_MOTOR_CMD,
        "Force command - Muscle force generation",
        admin_token
    );
    if (state->cmd_force_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->motor_subsystem_id, state->cmd_force_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains force command", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create position command node */
    state->cmd_position_id = create_rn_node(
        kg, "rn_cmd_position",
        (brain_kg_node_type_t)RN_KG_NODE_MOTOR_CMD,
        "Position command - Limb positioning control",
        admin_token
    );
    if (state->cmd_position_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->motor_subsystem_id, state->cmd_position_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains position command", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create trajectory command node */
    state->cmd_trajectory_id = create_rn_node(
        kg, "rn_cmd_trajectory",
        (brain_kg_node_type_t)RN_KG_NODE_MOTOR_CMD,
        "Trajectory command - Movement path computation",
        admin_token
    );
    if (state->cmd_trajectory_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->motor_subsystem_id, state->cmd_trajectory_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains trajectory command", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create posture command node */
    state->cmd_posture_id = create_rn_node(
        kg, "rn_cmd_posture",
        (brain_kg_node_type_t)RN_KG_NODE_MOTOR_CMD,
        "Posture command - Postural adjustment control",
        admin_token
    );
    if (state->cmd_posture_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->motor_subsystem_id, state->cmd_posture_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains posture command", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create balance command node */
    state->cmd_balance_id = create_rn_node(
        kg, "rn_cmd_balance",
        (brain_kg_node_type_t)RN_KG_NODE_MOTOR_CMD,
        "Balance command - Balance correction control",
        admin_token
    );
    if (state->cmd_balance_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->motor_subsystem_id, state->cmd_balance_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains balance command", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create skilled movement command node */
    state->cmd_skilled_id = create_rn_node(
        kg, "rn_cmd_skilled",
        (brain_kg_node_type_t)RN_KG_NODE_MOTOR_CMD,
        "Skilled command - Learned skilled movement sequences",
        admin_token
    );
    if (state->cmd_skilled_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->motor_subsystem_id, state->cmd_skilled_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains skilled command", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int rn_kg_register_effectors(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rn_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_register_effectors: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create effectors subsystem node */
    state->effector_subsystem_id = create_rn_node(
        kg, RN_KG_EFFECTOR_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Red Nucleus effectors - Forelimb, hindlimb, and axial musculature targets",
        admin_token
    );
    if (state->effector_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rn_kg_register_effectors: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, parent_id, state->effector_subsystem_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains effectors", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create forelimb proximal effector node */
    state->forelimb_proximal_id = create_rn_node(
        kg, "rn_eff_forelimb_proximal",
        (brain_kg_node_type_t)RN_KG_NODE_EFFECTOR,
        "Forelimb proximal effector - Shoulder and upper arm control",
        admin_token
    );
    if (state->forelimb_proximal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->effector_subsystem_id, state->forelimb_proximal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains forelimb proximal", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create forelimb distal effector node */
    state->forelimb_distal_id = create_rn_node(
        kg, "rn_eff_forelimb_distal",
        (brain_kg_node_type_t)RN_KG_NODE_EFFECTOR,
        "Forelimb distal effector - Hand and finger control",
        admin_token
    );
    if (state->forelimb_distal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->effector_subsystem_id, state->forelimb_distal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains forelimb distal", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create hindlimb proximal effector node */
    state->hindlimb_proximal_id = create_rn_node(
        kg, "rn_eff_hindlimb_proximal",
        (brain_kg_node_type_t)RN_KG_NODE_EFFECTOR,
        "Hindlimb proximal effector - Hip and thigh control",
        admin_token
    );
    if (state->hindlimb_proximal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->effector_subsystem_id, state->hindlimb_proximal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains hindlimb proximal", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create hindlimb distal effector node */
    state->hindlimb_distal_id = create_rn_node(
        kg, "rn_eff_hindlimb_distal",
        (brain_kg_node_type_t)RN_KG_NODE_EFFECTOR,
        "Hindlimb distal effector - Foot and toe control",
        admin_token
    );
    if (state->hindlimb_distal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->effector_subsystem_id, state->hindlimb_distal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains hindlimb distal", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create axial effector node */
    state->axial_id = create_rn_node(
        kg, "rn_eff_axial",
        (brain_kg_node_type_t)RN_KG_NODE_EFFECTOR,
        "Axial effector - Trunk and core musculature control",
        admin_token
    );
    if (state->axial_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->effector_subsystem_id, state->axial_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains axial", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int rn_kg_register_errors(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rn_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_register_errors: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create error types subsystem node */
    state->error_subsystem_id = create_rn_node(
        kg, RN_KG_ERROR_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Red Nucleus error types - Position, velocity, force, timing, trajectory errors",
        admin_token
    );
    if (state->error_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rn_kg_register_errors: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, parent_id, state->error_subsystem_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains error types", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create position error node */
    state->error_position_id = create_rn_node(
        kg, "rn_error_position",
        (brain_kg_node_type_t)RN_KG_NODE_ERROR_TYPE,
        "Position error - Deviation from target position",
        admin_token
    );
    if (state->error_position_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->error_subsystem_id, state->error_position_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains position error", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create velocity error node */
    state->error_velocity_id = create_rn_node(
        kg, "rn_error_velocity",
        (brain_kg_node_type_t)RN_KG_NODE_ERROR_TYPE,
        "Velocity error - Deviation from target velocity",
        admin_token
    );
    if (state->error_velocity_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->error_subsystem_id, state->error_velocity_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains velocity error", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create force error node */
    state->error_force_id = create_rn_node(
        kg, "rn_error_force",
        (brain_kg_node_type_t)RN_KG_NODE_ERROR_TYPE,
        "Force error - Deviation from target force",
        admin_token
    );
    if (state->error_force_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->error_subsystem_id, state->error_force_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains force error", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create timing error node */
    state->error_timing_id = create_rn_node(
        kg, "rn_error_timing",
        (brain_kg_node_type_t)RN_KG_NODE_ERROR_TYPE,
        "Timing error - Temporal deviation in movement execution",
        admin_token
    );
    if (state->error_timing_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->error_subsystem_id, state->error_timing_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains timing error", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create trajectory error node */
    state->error_trajectory_id = create_rn_node(
        kg, "rn_error_trajectory",
        (brain_kg_node_type_t)RN_KG_NODE_ERROR_TYPE,
        "Trajectory error - Deviation from planned movement path",
        admin_token
    );
    if (state->error_trajectory_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->error_subsystem_id, state->error_trajectory_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains trajectory error", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int rn_kg_register_cerebellar(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rn_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_register_cerebellar: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create cerebellar connections subsystem node */
    state->cerebellar_subsystem_id = create_rn_node(
        kg, RN_KG_CEREBELLAR_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Red Nucleus cerebellar connections - Dentate input, olivary/thalamic outputs",
        admin_token
    );
    if (state->cerebellar_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rn_kg_register_cerebellar: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, parent_id, state->cerebellar_subsystem_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains cerebellar connections", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create dentate input node */
    state->dentate_input_id = create_rn_node(
        kg, "rn_cerebellar_dentate_input",
        (brain_kg_node_type_t)RN_KG_NODE_CEREBELLAR_CONN,
        "Dentate nucleus input - Cerebellar output to Red Nucleus (dentato-rubral)",
        admin_token
    );
    if (state->dentate_input_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->cerebellar_subsystem_id, state->dentate_input_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains dentate input", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create olivary output node */
    state->olivary_output_id = create_rn_node(
        kg, "rn_cerebellar_olivary_output",
        (brain_kg_node_type_t)RN_KG_NODE_CEREBELLAR_CONN,
        "Olivary output - Red Nucleus to inferior olive (rubro-olivary, error signal)",
        admin_token
    );
    if (state->olivary_output_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->cerebellar_subsystem_id, state->olivary_output_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains olivary output", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create thalamic output node */
    state->thalamic_output_id = create_rn_node(
        kg, "rn_cerebellar_thalamic_output",
        (brain_kg_node_type_t)RN_KG_NODE_CEREBELLAR_CONN,
        "Thalamic output - Dentato-rubro-thalamic pathway to motor cortex",
        admin_token
    );
    if (state->thalamic_output_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_rn_edge(kg, state->cerebellar_subsystem_id, state->thalamic_output_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains thalamic output", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int rn_kg_register_cross_edges(
    brain_kg_t* kg,
    rn_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_register_cross_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* ==========================================
     * Subdivision -> Motor Command relationships
     * ==========================================*/

    /* Magnocellular controls rubrospinal motor commands */
    if (state->magnocellular_id != BRAIN_KG_INVALID_NODE &&
        state->cmd_velocity_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->magnocellular_id, state->cmd_velocity_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_CONTROLS,
            "magnocellular controls velocity commands", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->magnocellular_id != BRAIN_KG_INVALID_NODE &&
        state->cmd_force_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->magnocellular_id, state->cmd_force_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_CONTROLS,
            "magnocellular controls force commands", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->magnocellular_id != BRAIN_KG_INVALID_NODE &&
        state->cmd_position_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->magnocellular_id, state->cmd_position_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_CONTROLS,
            "magnocellular controls position commands", 0.8f, admin_token);
        state->edge_count++;
    }

    /* Parvocellular involved in skilled/learned movements */
    if (state->parvocellular_id != BRAIN_KG_INVALID_NODE &&
        state->cmd_skilled_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->parvocellular_id, state->cmd_skilled_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_CONTROLS,
            "parvocellular modulates skilled movements via learning", 0.8f, admin_token);
        state->edge_count++;
    }

    /* ==========================================
     * Motor Command -> Effector relationships
     * ==========================================*/

    /* Velocity commands target forelimb distal (primary rubrospinal target) */
    if (state->cmd_velocity_id != BRAIN_KG_INVALID_NODE &&
        state->forelimb_distal_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->cmd_velocity_id, state->forelimb_distal_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_TARGETS,
            "velocity commands target forelimb distal", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Force commands target forelimb proximal */
    if (state->cmd_force_id != BRAIN_KG_INVALID_NODE &&
        state->forelimb_proximal_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->cmd_force_id, state->forelimb_proximal_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_TARGETS,
            "force commands target forelimb proximal", 0.8f, admin_token);
        state->edge_count++;
    }

    /* Posture commands target axial musculature */
    if (state->cmd_posture_id != BRAIN_KG_INVALID_NODE &&
        state->axial_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->cmd_posture_id, state->axial_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_TARGETS,
            "posture commands target axial musculature", 0.7f, admin_token);
        state->edge_count++;
    }

    /* Balance commands target hindlimb */
    if (state->cmd_balance_id != BRAIN_KG_INVALID_NODE &&
        state->hindlimb_proximal_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->cmd_balance_id, state->hindlimb_proximal_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_TARGETS,
            "balance commands target hindlimb proximal", 0.6f, admin_token);
        state->edge_count++;
    }

    /* ==========================================
     * Error -> Learning relationships
     * ==========================================*/

    /* Position error triggers learning via parvocellular */
    if (state->error_position_id != BRAIN_KG_INVALID_NODE &&
        state->parvocellular_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->error_position_id, state->parvocellular_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_TRIGGERS_LEARNING,
            "position error triggers motor learning", 0.8f, admin_token);
        state->edge_count++;
    }

    /* Velocity error triggers learning */
    if (state->error_velocity_id != BRAIN_KG_INVALID_NODE &&
        state->parvocellular_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->error_velocity_id, state->parvocellular_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_TRIGGERS_LEARNING,
            "velocity error triggers motor learning", 0.7f, admin_token);
        state->edge_count++;
    }

    /* Trajectory error triggers learning */
    if (state->error_trajectory_id != BRAIN_KG_INVALID_NODE &&
        state->parvocellular_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->error_trajectory_id, state->parvocellular_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_TRIGGERS_LEARNING,
            "trajectory error triggers motor learning", 0.8f, admin_token);
        state->edge_count++;
    }

    /* ==========================================
     * Cerebellar -> Motor relationships
     * ==========================================*/

    /* Dentate input activates parvocellular */
    if (state->dentate_input_id != BRAIN_KG_INVALID_NODE &&
        state->parvocellular_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->dentate_input_id, state->parvocellular_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_PROJECTS_TO,
            "dentate nucleus projects to parvocellular RN", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Parvocellular generates olivary output */
    if (state->parvocellular_id != BRAIN_KG_INVALID_NODE &&
        state->olivary_output_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->parvocellular_id, state->olivary_output_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_GENERATES,
            "parvocellular generates rubro-olivary error signal", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Error signals route through olivary output */
    if (state->error_subsystem_id != BRAIN_KG_INVALID_NODE &&
        state->olivary_output_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->error_subsystem_id, state->olivary_output_id,
            BRAIN_KG_EDGE_SENDS_TO,
            "error signals sent to inferior olive via rubro-olivary", 0.8f, admin_token);
        state->edge_count++;
    }

    /* Dentate modulates magnocellular for motor correction */
    if (state->dentate_input_id != BRAIN_KG_INVALID_NODE &&
        state->magnocellular_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->dentate_input_id, state->magnocellular_id,
            BRAIN_KG_EDGE_MODULATES,
            "dentate input modulates magnocellular for motor correction", 0.7f, admin_token);
        state->edge_count++;
    }

    /* Thalamic output receives from subdivisions */
    if (state->parvocellular_id != BRAIN_KG_INVALID_NODE &&
        state->thalamic_output_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->parvocellular_id, state->thalamic_output_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_GENERATES,
            "parvocellular contributes to thalamic projection", 0.6f, admin_token);
        state->edge_count++;
    }

    /* ==========================================
     * Error -> Effector relationships (feedback)
     * ==========================================*/

    /* Errors affect specific effectors */
    if (state->error_position_id != BRAIN_KG_INVALID_NODE &&
        state->forelimb_distal_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->error_position_id, state->forelimb_distal_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_AFFECTS,
            "position error affects forelimb distal correction", 0.7f, admin_token);
        state->edge_count++;
    }

    if (state->error_timing_id != BRAIN_KG_INVALID_NODE &&
        state->cmd_trajectory_id != BRAIN_KG_INVALID_NODE) {
        create_rn_edge(kg, state->error_timing_id, state->cmd_trajectory_id,
            (brain_kg_edge_type_t)RN_KG_EDGE_MODIFIES,
            "timing error modifies trajectory commands", 0.6f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// State Synchronization API
//=============================================================================

int rn_kg_update_state(
    brain_kg_t* kg,
    const rn_kg_state_t* state,
    float magno_activity,
    float parvo_activity,
    float avg_error,
    float skill_level,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_update_state: required parameter is NULL (kg, state)");
        return -1;
    }
    (void)admin_token;  /* Used for future access control */

    /* Update magnocellular activity metadata */
    if (state->magnocellular_id != BRAIN_KG_INVALID_NODE) {
        char activity_str[32];
        snprintf(activity_str, sizeof(activity_str), "%.1f%%", magno_activity * 100.0f);
        brain_kg_add_metadata(kg, state->magnocellular_id, "activity", activity_str);
    }

    /* Update parvocellular activity metadata */
    if (state->parvocellular_id != BRAIN_KG_INVALID_NODE) {
        char activity_str[32];
        snprintf(activity_str, sizeof(activity_str), "%.1f%%", parvo_activity * 100.0f);
        brain_kg_add_metadata(kg, state->parvocellular_id, "activity", activity_str);
    }

    /* Update error subsystem metadata */
    if (state->error_subsystem_id != BRAIN_KG_INVALID_NODE) {
        char error_str[32];
        snprintf(error_str, sizeof(error_str), "%.3f", avg_error);
        brain_kg_add_metadata(kg, state->error_subsystem_id, "avg_error", error_str);
    }

    /* Update skill level in skilled command node */
    if (state->cmd_skilled_id != BRAIN_KG_INVALID_NODE) {
        char skill_str[32];
        snprintf(skill_str, sizeof(skill_str), "%.1f%%", skill_level * 100.0f);
        brain_kg_add_metadata(kg, state->cmd_skilled_id, "skill_level", skill_str);
    }

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t rn_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, RN_KG_ROOT_NAME);
}

brain_kg_node_id_t rn_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

brain_kg_node_list_t* rn_kg_get_subdivisions(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)RN_KG_NODE_SUBDIVISION);
}

brain_kg_node_list_t* rn_kg_get_motor_cmds(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)RN_KG_NODE_MOTOR_CMD);
}

brain_kg_node_list_t* rn_kg_get_effectors(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)RN_KG_NODE_EFFECTOR);
}

brain_kg_node_list_t* rn_kg_get_error_types(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)RN_KG_NODE_ERROR_TYPE);
}

brain_kg_node_list_t* rn_kg_get_cerebellar_conns(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)RN_KG_NODE_CEREBELLAR_CONN);
}

int rn_kg_unregister_all(
    brain_kg_t* kg,
    rn_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }
    (void)admin_token;  /* Would be used for actual deletion */

    /* Note: In a full implementation, would iterate and remove all nodes */
    /* For now, just mark as unregistered */
    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(RN_KG_MODULE_NAME, "Unregistered Red Nucleus KG nodes");

    return 0;
}
