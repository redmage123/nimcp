//=============================================================================
// nimcp_pfc_kg_wiring.c - Prefrontal Cortex Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_pfc_kg_wiring.c
 * @brief Implementation of Prefrontal Cortex Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for PFC module
 * WHY:  Enables semantic queries about executive function and control
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/prefrontal/bridges/nimcp_pfc_kg_wiring.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(pfc_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create a PFC node with description
 *
 * WHAT: Helper to create a single KG node
 * WHY:  Reduces boilerplate in registration functions
 * HOW:  Wraps brain_kg_add_node with logging
 */
static brain_kg_node_id_t create_pfc_node(
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
        NIMCP_LOG_DEBUG(PFC_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

/**
 * @brief Create an edge between PFC nodes
 *
 * WHAT: Helper to create a single KG edge
 * WHY:  Reduces boilerplate in registration functions
 * HOW:  Wraps brain_kg_add_edge with validation
 */
static brain_kg_edge_id_t create_pfc_edge(
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

int pfc_kg_default_config(pfc_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pfc_kg_default_config: config is NULL");
        return -1;
    }

    config->register_dlpfc = true;
    config->register_vmpfc = true;
    config->register_acc = true;
    config->register_lpfc = true;
    config->register_executive_nodes = true;
    config->register_decision_nodes = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;
    config->register_control_edges = true;

    return 0;
}

//=============================================================================
// Registration API - Main Entry Point
//=============================================================================

int pfc_kg_register_all(
    brain_kg_t* kg,
    const pfc_kg_config_t* config,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pfc_kg_register_all: kg is NULL");
        return -1;
    }

    /* Use provided config or defaults */
    pfc_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        pfc_kg_default_config(&local_config);
    }

    /* Initialize local state */
    pfc_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create PFC root node */
    local_state.root_id = create_pfc_node(
        kg, PFC_KG_ROOT_NAME,
        BRAIN_KG_NODE_CORTICAL,
        "Prefrontal Cortex - executive control, working memory, planning, decision-making",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(PFC_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pfc_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Register subregions */
    if (pfc_kg_register_subregions(kg, local_state.root_id, &local_config,
                                    &local_state, admin_token) < 0) {
        NIMCP_LOG_WARN(PFC_KG_MODULE_NAME, "Failed to register subregions");
    }

    /* Register executive nodes */
    if (local_config.register_executive_nodes) {
        if (pfc_kg_register_executive_nodes(kg, local_state.root_id,
                                             &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(PFC_KG_MODULE_NAME, "Failed to register executive nodes");
        }
    }

    /* Register monitoring nodes */
    if (pfc_kg_register_monitoring_nodes(kg, local_state.root_id,
                                          &local_state, admin_token) < 0) {
        NIMCP_LOG_WARN(PFC_KG_MODULE_NAME, "Failed to register monitoring nodes");
    }

    /* Register decision nodes */
    if (local_config.register_decision_nodes) {
        if (pfc_kg_register_decision_nodes(kg, local_state.root_id,
                                            &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(PFC_KG_MODULE_NAME, "Failed to register decision nodes");
        }
    }

    /* Register cross-subsystem edges */
    if (local_config.register_cross_edges) {
        pfc_kg_register_cross_edges(kg, &local_state, admin_token);
    }

    local_state.registered = true;

    /* Copy to output if requested */
    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(PFC_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Registration API - Subregions
//=============================================================================

int pfc_kg_register_subregions(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const pfc_kg_config_t* config,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pfc_kg_register_subregions: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Register Dorsolateral PFC */
    if (!config || config->register_dlpfc) {
        state->dlpfc_id = create_pfc_node(
            kg, PFC_KG_DLPFC_NAME,
            (brain_kg_node_type_t)PFC_KG_NODE_SUBREGION,
            "Dorsolateral PFC - working memory, cognitive control, planning",
            admin_token
        );
        if (state->dlpfc_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_pfc_edge(kg, parent_id, state->dlpfc_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains dlPFC", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Register Ventromedial PFC */
    if (!config || config->register_vmpfc) {
        state->vmpfc_id = create_pfc_node(
            kg, PFC_KG_VMPFC_NAME,
            (brain_kg_node_type_t)PFC_KG_NODE_SUBREGION,
            "Ventromedial PFC - value-based decisions, emotion regulation, social cognition",
            admin_token
        );
        if (state->vmpfc_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_pfc_edge(kg, parent_id, state->vmpfc_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains vmPFC", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Register Anterior Cingulate Cortex */
    if (!config || config->register_acc) {
        state->acc_id = create_pfc_node(
            kg, PFC_KG_ACC_NAME,
            (brain_kg_node_type_t)PFC_KG_NODE_SUBREGION,
            "Anterior Cingulate - conflict monitoring, error detection, motivation",
            admin_token
        );
        if (state->acc_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_pfc_edge(kg, parent_id, state->acc_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains ACC", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Register Lateral PFC */
    if (!config || config->register_lpfc) {
        state->lpfc_id = create_pfc_node(
            kg, PFC_KG_LPFC_NAME,
            (brain_kg_node_type_t)PFC_KG_NODE_SUBREGION,
            "Lateral PFC - rule representation, task set, abstract reasoning",
            admin_token
        );
        if (state->lpfc_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_pfc_edge(kg, parent_id, state->lpfc_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains lPFC", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Create inter-subregion connections */
    if (state->dlpfc_id != BRAIN_KG_INVALID_NODE &&
        state->vmpfc_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->dlpfc_id, state->vmpfc_id,
            BRAIN_KG_EDGE_COORDINATES_WITH, "dlPFC-vmPFC integration", 0.8f, admin_token);
        state->edge_count++;
    }

    if (state->acc_id != BRAIN_KG_INVALID_NODE &&
        state->dlpfc_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->acc_id, state->dlpfc_id,
            BRAIN_KG_EDGE_SENDS_TO, "ACC signals to dlPFC", 0.85f, admin_token);
        state->edge_count++;
    }

    if (state->lpfc_id != BRAIN_KG_INVALID_NODE &&
        state->dlpfc_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->lpfc_id, state->dlpfc_id,
            BRAIN_KG_EDGE_SENDS_TO, "lPFC rule to dlPFC", 0.8f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Executive Nodes
//=============================================================================

/**
 * @brief Register working memory nodes
 */
static int register_wm_nodes(
    brain_kg_t* kg,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    /* Working memory node */
    state->working_memory_id = create_pfc_node(
        kg, "working_memory",
        (brain_kg_node_type_t)PFC_KG_NODE_EXECUTIVE_FUNCTION,
        "Working memory - active maintenance and manipulation of information",
        admin_token
    );
    if (state->working_memory_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->executive_system_id, state->working_memory_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains working memory", 1.0f, admin_token);
        state->edge_count++;
    }

    /* WM maintenance node */
    state->wm_maintenance_id = create_pfc_node(
        kg, "wm_maintenance",
        (brain_kg_node_type_t)PFC_KG_NODE_WM_COMPONENT,
        "WM maintenance - sustained active representation",
        admin_token
    );
    if (state->wm_maintenance_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->working_memory_id, state->wm_maintenance_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "performs maintenance", 1.0f, admin_token);
        state->edge_count++;
    }

    /* WM manipulation node */
    state->wm_manipulation_id = create_pfc_node(
        kg, "wm_manipulation",
        (brain_kg_node_type_t)PFC_KG_NODE_WM_COMPONENT,
        "WM manipulation - updating, reordering, transformation",
        admin_token
    );
    if (state->wm_manipulation_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->working_memory_id, state->wm_manipulation_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "performs manipulation", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

/**
 * @brief Register attention nodes
 */
static int register_attention_nodes(
    brain_kg_t* kg,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    /* Attention control node */
    state->attention_id = create_pfc_node(
        kg, "attention_control",
        (brain_kg_node_type_t)PFC_KG_NODE_EXECUTIVE_FUNCTION,
        "Attention control - top-down selection, biasing, filtering",
        admin_token
    );
    if (state->attention_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->executive_system_id, state->attention_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains attention control", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Selective attention node */
    state->selective_attn_id = create_pfc_node(
        kg, "selective_attention",
        (brain_kg_node_type_t)PFC_KG_NODE_ATTENTION_COMPONENT,
        "Selective attention - focus on relevant, filter irrelevant",
        admin_token
    );
    if (state->selective_attn_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->attention_id, state->selective_attn_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "performs selection", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Sustained attention node */
    state->sustained_attn_id = create_pfc_node(
        kg, "sustained_attention",
        (brain_kg_node_type_t)PFC_KG_NODE_ATTENTION_COMPONENT,
        "Sustained attention - vigilance, alertness maintenance",
        admin_token
    );
    if (state->sustained_attn_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->attention_id, state->sustained_attn_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "maintains vigilance", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

/**
 * @brief Register control nodes
 */
static int register_control_nodes(
    brain_kg_t* kg,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    /* Inhibition node */
    state->inhibition_id = create_pfc_node(
        kg, "inhibition",
        (brain_kg_node_type_t)PFC_KG_NODE_EXECUTIVE_FUNCTION,
        "Inhibition - response suppression, impulse control",
        admin_token
    );
    if (state->inhibition_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->executive_system_id, state->inhibition_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains inhibition", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Cognitive flexibility node */
    state->flexibility_id = create_pfc_node(
        kg, "cognitive_flexibility",
        (brain_kg_node_type_t)PFC_KG_NODE_EXECUTIVE_FUNCTION,
        "Cognitive flexibility - task switching, set shifting, adaptation",
        admin_token
    );
    if (state->flexibility_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->executive_system_id, state->flexibility_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains flexibility", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Planning node */
    state->planning_id = create_pfc_node(
        kg, "planning",
        (brain_kg_node_type_t)PFC_KG_NODE_EXECUTIVE_FUNCTION,
        "Planning - goal decomposition, sequencing, temporal organization",
        admin_token
    );
    if (state->planning_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->executive_system_id, state->planning_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains planning", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int pfc_kg_register_executive_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pfc_kg_register_executive_nodes: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create executive system node */
    state->executive_system_id = create_pfc_node(
        kg, PFC_KG_EXECUTIVE_NAME,
        BRAIN_KG_NODE_COGNITIVE,
        "Executive system - core executive functions, cognitive control",
        admin_token
    );
    if (state->executive_system_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pfc_kg_register_executive_nodes: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, parent_id, state->executive_system_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains executive system", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Register working memory nodes */
    register_wm_nodes(kg, state, admin_token);

    /* Register attention nodes */
    register_attention_nodes(kg, state, admin_token);

    /* Register control nodes */
    register_control_nodes(kg, state, admin_token);

    /* Create executive function relationships */
    if (state->working_memory_id != BRAIN_KG_INVALID_NODE &&
        state->attention_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->attention_id, state->working_memory_id,
            BRAIN_KG_EDGE_MODULATES,
            "attention gates WM input", 0.85f, admin_token);
        state->edge_count++;
    }

    if (state->inhibition_id != BRAIN_KG_INVALID_NODE &&
        state->flexibility_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->inhibition_id, state->flexibility_id,
            BRAIN_KG_EDGE_MODULATES,
            "inhibition enables flexibility", 0.8f, admin_token);
        state->edge_count++;
    }

    if (state->planning_id != BRAIN_KG_INVALID_NODE &&
        state->working_memory_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->working_memory_id, state->planning_id,
            (brain_kg_edge_type_t)PFC_KG_EDGE_INFORMS,
            "WM supports planning", 0.9f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Monitoring Nodes
//=============================================================================

int pfc_kg_register_monitoring_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pfc_kg_register_monitoring_nodes: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Conflict monitoring node */
    state->conflict_id = create_pfc_node(
        kg, "conflict_monitoring",
        (brain_kg_node_type_t)PFC_KG_NODE_MONITORING,
        "Conflict monitoring - response competition, uncertainty detection",
        admin_token
    );
    if (state->conflict_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, parent_id, state->conflict_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "performs conflict monitoring", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Error detection node */
    state->error_id = create_pfc_node(
        kg, "error_detection",
        (brain_kg_node_type_t)PFC_KG_NODE_MONITORING,
        "Error detection - performance error, prediction error",
        admin_token
    );
    if (state->error_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, parent_id, state->error_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "performs error detection", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Performance monitoring node */
    state->performance_id = create_pfc_node(
        kg, "performance_monitoring",
        (brain_kg_node_type_t)PFC_KG_NODE_MONITORING,
        "Performance monitoring - outcome evaluation, feedback processing",
        admin_token
    );
    if (state->performance_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, parent_id, state->performance_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "performs performance monitoring", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Link monitoring to ACC */
    if (state->acc_id != BRAIN_KG_INVALID_NODE) {
        if (state->conflict_id != BRAIN_KG_INVALID_NODE) {
            create_pfc_edge(kg, state->acc_id, state->conflict_id,
                BRAIN_KG_EDGE_MODULATES,
                "ACC performs conflict monitoring", 0.9f, admin_token);
            state->edge_count++;
        }

        if (state->error_id != BRAIN_KG_INVALID_NODE) {
            create_pfc_edge(kg, state->acc_id, state->error_id,
                BRAIN_KG_EDGE_MODULATES,
                "ACC performs error detection", 0.9f, admin_token);
            state->edge_count++;
        }
    }

    /* Conflict triggers control adjustment */
    if (state->conflict_id != BRAIN_KG_INVALID_NODE &&
        state->executive_system_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->conflict_id, state->executive_system_id,
            (brain_kg_edge_type_t)PFC_KG_EDGE_TRIGGERS_CONTROL,
            "conflict triggers control adjustment", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Error triggers learning */
    if (state->error_id != BRAIN_KG_INVALID_NODE &&
        state->flexibility_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->error_id, state->flexibility_id,
            (brain_kg_edge_type_t)PFC_KG_EDGE_ADJUSTS,
            "error triggers strategy adjustment", 0.8f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Decision Nodes
//=============================================================================

int pfc_kg_register_decision_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pfc_kg_register_decision_nodes: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create decision system node */
    state->decision_system_id = create_pfc_node(
        kg, PFC_KG_DECISION_NAME,
        BRAIN_KG_NODE_COGNITIVE,
        "Decision system - goal/action/strategy selection",
        admin_token
    );
    if (state->decision_system_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pfc_kg_register_decision_nodes: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, parent_id, state->decision_system_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains decision system", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Goal selection node */
    state->goal_selection_id = create_pfc_node(
        kg, "goal_selection",
        (brain_kg_node_type_t)PFC_KG_NODE_DECISION_TYPE,
        "Goal selection - objective prioritization, goal maintenance",
        admin_token
    );
    if (state->goal_selection_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->decision_system_id, state->goal_selection_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "supports goal selection", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Action selection node */
    state->action_selection_id = create_pfc_node(
        kg, "action_selection",
        (brain_kg_node_type_t)PFC_KG_NODE_DECISION_TYPE,
        "Action selection - motor plan selection, response mapping",
        admin_token
    );
    if (state->action_selection_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->decision_system_id, state->action_selection_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "supports action selection", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Strategy selection node */
    state->strategy_selection_id = create_pfc_node(
        kg, "strategy_selection",
        (brain_kg_node_type_t)PFC_KG_NODE_DECISION_TYPE,
        "Strategy selection - approach selection, problem-solving strategy",
        admin_token
    );
    if (state->strategy_selection_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->decision_system_id, state->strategy_selection_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "supports strategy selection", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Rule representation node */
    state->rule_id = create_pfc_node(
        kg, "rule_representation",
        (brain_kg_node_type_t)PFC_KG_NODE_CONTROL_PROCESS,
        "Rule representation - task rules, conditional logic, if-then mappings",
        admin_token
    );
    if (state->rule_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pfc_edge(kg, state->decision_system_id, state->rule_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "represents rules", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Goals guide actions */
    if (state->goal_selection_id != BRAIN_KG_INVALID_NODE &&
        state->action_selection_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->goal_selection_id, state->action_selection_id,
            (brain_kg_edge_type_t)PFC_KG_EDGE_GUIDES,
            "goals guide action selection", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Rules guide decisions */
    if (state->rule_id != BRAIN_KG_INVALID_NODE &&
        state->action_selection_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->rule_id, state->action_selection_id,
            (brain_kg_edge_type_t)PFC_KG_EDGE_GUIDES,
            "rules guide action selection", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Planning sequences actions */
    if (state->planning_id != BRAIN_KG_INVALID_NODE &&
        state->action_selection_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->planning_id, state->action_selection_id,
            (brain_kg_edge_type_t)PFC_KG_EDGE_SEQUENCES,
            "planning sequences actions", 0.85f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Cross Edges
//=============================================================================

/**
 * @brief Register subregion-to-function edges
 */
static void register_subregion_function_edges(
    brain_kg_t* kg,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    /* dlPFC handles working memory */
    if (state->dlpfc_id != BRAIN_KG_INVALID_NODE &&
        state->working_memory_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->dlpfc_id, state->working_memory_id,
            BRAIN_KG_EDGE_MODULATES,
            "dlPFC supports working memory", 0.9f, admin_token);
        state->edge_count++;
    }

    /* dlPFC handles attention control */
    if (state->dlpfc_id != BRAIN_KG_INVALID_NODE &&
        state->attention_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->dlpfc_id, state->attention_id,
            BRAIN_KG_EDGE_MODULATES,
            "dlPFC controls attention", 0.85f, admin_token);
        state->edge_count++;
    }

    /* vmPFC handles value-based decisions */
    if (state->vmpfc_id != BRAIN_KG_INVALID_NODE &&
        state->goal_selection_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->vmpfc_id, state->goal_selection_id,
            BRAIN_KG_EDGE_MODULATES,
            "vmPFC values inform goals", 0.85f, admin_token);
        state->edge_count++;
    }

    /* lPFC handles rules */
    if (state->lpfc_id != BRAIN_KG_INVALID_NODE &&
        state->rule_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->lpfc_id, state->rule_id,
            BRAIN_KG_EDGE_MODULATES,
            "lPFC represents rules", 0.9f, admin_token);
        state->edge_count++;
    }
}

/**
 * @brief Register executive-decision integration edges
 */
static void register_executive_decision_edges(
    brain_kg_t* kg,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    /* Executive system drives decisions */
    if (state->executive_system_id != BRAIN_KG_INVALID_NODE &&
        state->decision_system_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->executive_system_id, state->decision_system_id,
            BRAIN_KG_EDGE_INTEGRATES_WITH,
            "executive functions support decisions", 0.9f, admin_token);
        state->edge_count++;
    }

    /* WM informs decisions */
    if (state->working_memory_id != BRAIN_KG_INVALID_NODE &&
        state->decision_system_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->working_memory_id, state->decision_system_id,
            (brain_kg_edge_type_t)PFC_KG_EDGE_INFORMS,
            "WM informs decisions", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Inhibition suppresses inappropriate actions */
    if (state->inhibition_id != BRAIN_KG_INVALID_NODE &&
        state->action_selection_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->inhibition_id, state->action_selection_id,
            (brain_kg_edge_type_t)PFC_KG_EDGE_SUPPRESSES,
            "inhibition suppresses actions", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Flexibility enables strategy switching */
    if (state->flexibility_id != BRAIN_KG_INVALID_NODE &&
        state->strategy_selection_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->flexibility_id, state->strategy_selection_id,
            (brain_kg_edge_type_t)PFC_KG_EDGE_SWITCHES,
            "flexibility enables strategy switching", 0.85f, admin_token);
        state->edge_count++;
    }
}

/**
 * @brief Register goal maintenance edges
 */
static void register_goal_maintenance_edges(
    brain_kg_t* kg,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    /* WM maintenance supports goal maintenance */
    if (state->wm_maintenance_id != BRAIN_KG_INVALID_NODE &&
        state->goal_selection_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->wm_maintenance_id, state->goal_selection_id,
            (brain_kg_edge_type_t)PFC_KG_EDGE_MAINTAINS,
            "WM maintains goals", 0.9f, admin_token);
        state->edge_count++;
    }

    /* dlPFC maintains goals */
    if (state->dlpfc_id != BRAIN_KG_INVALID_NODE &&
        state->goal_selection_id != BRAIN_KG_INVALID_NODE) {
        create_pfc_edge(kg, state->dlpfc_id, state->goal_selection_id,
            (brain_kg_edge_type_t)PFC_KG_EDGE_MAINTAINS,
            "dlPFC maintains goal representation", 0.85f, admin_token);
        state->edge_count++;
    }
}

int pfc_kg_register_cross_edges(
    brain_kg_t* kg,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pfc_kg_register_cross_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Register subregion-to-function edges */
    register_subregion_function_edges(kg, state, admin_token);

    /* Register executive-decision integration edges */
    register_executive_decision_edges(kg, state, admin_token);

    /* Register goal maintenance edges */
    register_goal_maintenance_edges(kg, state, admin_token);

    return 0;
}

//=============================================================================
// State Synchronization API
//=============================================================================

int pfc_kg_update_state(
    brain_kg_t* kg,
    const pfc_kg_state_t* state,
    float wm_load,
    float control_demand,
    float conflict_level,
    float attention_focus,
    uint64_t admin_token
) {
    (void)admin_token;  /* Reserved for future access control */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pfc_kg_update_state: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Update WM load metadata */
    if (state->working_memory_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.1f%%", wm_load * 100.0f);
        brain_kg_add_metadata(kg, state->working_memory_id, "load", val_str);
    }

    /* Update control demand metadata */
    if (state->executive_system_id != BRAIN_KG_INVALID_NODE) {
        char ctrl_str[32];
        snprintf(ctrl_str, sizeof(ctrl_str), "%.3f", control_demand);
        brain_kg_add_metadata(kg, state->executive_system_id, "control_demand", ctrl_str);
    }

    /* Update conflict level metadata */
    if (state->conflict_id != BRAIN_KG_INVALID_NODE) {
        char conf_str[32];
        snprintf(conf_str, sizeof(conf_str), "%.3f", conflict_level);
        brain_kg_add_metadata(kg, state->conflict_id, "conflict_level", conf_str);
    }

    /* Update attention focus metadata */
    if (state->attention_id != BRAIN_KG_INVALID_NODE) {
        char attn_str[32];
        snprintf(attn_str, sizeof(attn_str), "%.3f", attention_focus);
        brain_kg_add_metadata(kg, state->attention_id, "focus_strength", attn_str);
    }

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t pfc_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, PFC_KG_ROOT_NAME);
}

brain_kg_node_id_t pfc_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

brain_kg_node_list_t* pfc_kg_get_executive_nodes(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)PFC_KG_NODE_EXECUTIVE_FUNCTION
    );
}

brain_kg_node_list_t* pfc_kg_get_decision_nodes(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)PFC_KG_NODE_DECISION_TYPE
    );
}

brain_kg_node_list_t* pfc_kg_get_subregions(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)PFC_KG_NODE_SUBREGION
    );
}

int pfc_kg_unregister_all(
    brain_kg_t* kg,
    pfc_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;  /* Would be used for actual deletion */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pfc_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    /*
     * Note: Full implementation would remove nodes in reverse order
     * For now, mark as unregistered
     */
    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(PFC_KG_MODULE_NAME, "Unregistered PFC KG nodes");

    return 0;
}

//=============================================================================
// Runtime Event Emission (Wave W2)
//=============================================================================

/*
 * HOT-PATH GAP: Prefrontal cortex has an adapter + quantum bridge but no
 * dedicated tick .c file. Executive callers issuing goals/plans should
 * invoke pfc_kg_emit_event(brain, "goal_update", priority, ts_us).
 *
 * Node name uses canonical `prefrontal_cortex` per naming registry §1.
 */

void pfc_kg_emit_event(
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

    char name[160];
    char desc[192];
    snprintf(name, sizeof(name),
             "prefrontal_cortex_event_%s_%" PRIu64, kind, ts_us);
    snprintf(desc, sizeof(desc),
             "PFC runtime event: kind=%s intensity=%.3f ts_us=%" PRIu64,
             kind, (double)intensity, ts_us);

    brain_kg_node_id_t evt_id = brain_kg_add_node(
        kg, name, BRAIN_KG_NODE_CUSTOM, desc
    );

    if (evt_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.4f", (double)intensity);
        brain_kg_add_metadata(kg, evt_id, "intensity", val_str);
        brain_kg_add_metadata(kg, evt_id, "kind", kind);

        brain_kg_node_id_t root = brain_kg_find_node(kg, PFC_KG_ROOT_NAME);
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
