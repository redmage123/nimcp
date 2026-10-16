//=============================================================================
// nimcp_ofc_kg_wiring.c - OFC Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_ofc_kg_wiring.c
 * @brief Implementation of OFC Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for OFC module
 * WHY:  Enables semantic queries about value computation and decisions
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/ofc/bridges/nimcp_ofc_kg_wiring.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(ofc_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create an OFC node with description
 *
 * WHAT: Helper to create a single KG node
 * WHY:  Reduces boilerplate in registration functions
 * HOW:  Wraps brain_kg_add_node with logging
 */
static brain_kg_node_id_t create_ofc_node(
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
        NIMCP_LOG_DEBUG(OFC_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

/**
 * @brief Create an edge between OFC nodes
 *
 * WHAT: Helper to create a single KG edge
 * WHY:  Reduces boilerplate in registration functions
 * HOW:  Wraps brain_kg_add_edge with validation
 */
static brain_kg_edge_id_t create_ofc_edge(
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

int ofc_kg_default_config(ofc_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_kg_default_config: config is NULL");
        return -1;
    }

    config->register_lateral = true;
    config->register_medial = true;
    config->register_anterior = true;
    config->register_posterior = true;
    config->register_value_nodes = true;
    config->register_decision_nodes = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;
    config->register_emotion_edges = true;

    return 0;
}

//=============================================================================
// Registration API - Main Entry Point
//=============================================================================

int ofc_kg_register_all(
    brain_kg_t* kg,
    const ofc_kg_config_t* config,
    ofc_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_kg_register_all: kg is NULL");
        return -1;
    }

    /* Use provided config or defaults */
    ofc_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        ofc_kg_default_config(&local_config);
    }

    /* Initialize local state */
    ofc_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create OFC root node */
    local_state.root_id = create_ofc_node(
        kg, OFC_KG_ROOT_NAME,
        BRAIN_KG_NODE_CORTICAL,
        "Orbitofrontal Cortex - value-based decision making",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(OFC_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ofc_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Register subdivisions */
    if (ofc_kg_register_subdivisions(kg, local_state.root_id, &local_config,
                                      &local_state, admin_token) < 0) {
        NIMCP_LOG_WARN(OFC_KG_MODULE_NAME, "Failed to register subdivisions");
    }

    /* Register value computation nodes */
    if (local_config.register_value_nodes) {
        if (ofc_kg_register_value_nodes(kg, local_state.root_id,
                                         &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(OFC_KG_MODULE_NAME, "Failed to register value nodes");
        }
    }

    /* Register decision nodes */
    if (local_config.register_decision_nodes) {
        if (ofc_kg_register_decision_nodes(kg, local_state.root_id,
                                            &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(OFC_KG_MODULE_NAME, "Failed to register decision nodes");
        }
    }

    /* Register cross-subsystem edges */
    if (local_config.register_cross_edges) {
        ofc_kg_register_cross_edges(kg, &local_state, admin_token);
    }

    local_state.registered = true;

    /* Copy to output if requested */
    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(OFC_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Registration API - Subdivisions
//=============================================================================

int ofc_kg_register_subdivisions(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const ofc_kg_config_t* config,
    ofc_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_kg_register_subdivisions: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Register lateral OFC */
    if (!config || config->register_lateral) {
        state->lateral_id = create_ofc_node(
            kg, OFC_KG_LATERAL_NAME,
            (brain_kg_node_type_t)OFC_KG_NODE_SUBDIVISION,
            "Lateral OFC - stimulus-reward associations, reversal learning",
            admin_token
        );
        if (state->lateral_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_ofc_edge(kg, parent_id, state->lateral_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains lateral OFC", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Register medial OFC */
    if (!config || config->register_medial) {
        state->medial_id = create_ofc_node(
            kg, OFC_KG_MEDIAL_NAME,
            (brain_kg_node_type_t)OFC_KG_NODE_SUBDIVISION,
            "Medial OFC - value comparison, choice selection",
            admin_token
        );
        if (state->medial_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_ofc_edge(kg, parent_id, state->medial_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains medial OFC", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Register anterior OFC */
    if (!config || config->register_anterior) {
        state->anterior_id = create_ofc_node(
            kg, OFC_KG_ANTERIOR_NAME,
            (brain_kg_node_type_t)OFC_KG_NODE_SUBDIVISION,
            "Anterior OFC - abstract/social reward processing",
            admin_token
        );
        if (state->anterior_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_ofc_edge(kg, parent_id, state->anterior_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains anterior OFC", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Register posterior OFC */
    if (!config || config->register_posterior) {
        state->posterior_id = create_ofc_node(
            kg, OFC_KG_POSTERIOR_NAME,
            (brain_kg_node_type_t)OFC_KG_NODE_SUBDIVISION,
            "Posterior OFC - primary reward, sensory integration",
            admin_token
        );
        if (state->posterior_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_ofc_edge(kg, parent_id, state->posterior_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains posterior OFC", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    return 0;
}

//=============================================================================
// Registration API - Value Computation Nodes
//=============================================================================

/**
 * @brief Register core value signal nodes
 */
static int register_core_value_nodes(
    brain_kg_t* kg,
    ofc_kg_state_t* state,
    uint64_t admin_token
) {
    /* Expected value node */
    state->expected_value_id = create_ofc_node(
        kg, "expected_value",
        (brain_kg_node_type_t)OFC_KG_NODE_VALUE_SIGNAL,
        "Expected value - predicted outcome utility",
        admin_token
    );
    if (state->expected_value_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->value_system_id, state->expected_value_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "computes expected value", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Received value node */
    state->received_value_id = create_ofc_node(
        kg, "received_value",
        (brain_kg_node_type_t)OFC_KG_NODE_VALUE_SIGNAL,
        "Received value - actual outcome utility",
        admin_token
    );
    if (state->received_value_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->value_system_id, state->received_value_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "tracks received value", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Prediction error node */
    state->prediction_error_id = create_ofc_node(
        kg, "prediction_error",
        (brain_kg_node_type_t)OFC_KG_NODE_LEARNING_SIGNAL,
        "Prediction error - received minus expected value",
        admin_token
    );
    if (state->prediction_error_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->value_system_id, state->prediction_error_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "computes prediction error", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

/**
 * @brief Register extended value signal nodes
 */
static int register_extended_value_nodes(
    brain_kg_t* kg,
    ofc_kg_state_t* state,
    uint64_t admin_token
) {
    /* Probability node */
    state->probability_id = create_ofc_node(
        kg, "reward_probability",
        (brain_kg_node_type_t)OFC_KG_NODE_VALUE_SIGNAL,
        "Reward probability - likelihood of positive outcome",
        admin_token
    );
    if (state->probability_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->value_system_id, state->probability_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "estimates probability", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Magnitude node */
    state->magnitude_id = create_ofc_node(
        kg, "reward_magnitude",
        (brain_kg_node_type_t)OFC_KG_NODE_VALUE_SIGNAL,
        "Reward magnitude - size/intensity of outcome",
        admin_token
    );
    if (state->magnitude_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->value_system_id, state->magnitude_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "estimates magnitude", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Delay (temporal discount) node */
    state->delay_id = create_ofc_node(
        kg, "temporal_delay",
        (brain_kg_node_type_t)OFC_KG_NODE_VALUE_SIGNAL,
        "Temporal delay - hyperbolic discounting factor",
        admin_token
    );
    if (state->delay_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->value_system_id, state->delay_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "applies temporal discount", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Risk node */
    state->risk_id = create_ofc_node(
        kg, "risk_assessment",
        (brain_kg_node_type_t)OFC_KG_NODE_RISK_ASSESSMENT,
        "Risk assessment - outcome variance/uncertainty",
        admin_token
    );
    if (state->risk_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->value_system_id, state->risk_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "assesses risk", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Social value node */
    state->social_id = create_ofc_node(
        kg, "social_value",
        (brain_kg_node_type_t)OFC_KG_NODE_SOCIAL_VALUE,
        "Social value - interpersonal/social reward component",
        admin_token
    );
    if (state->social_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->value_system_id, state->social_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "computes social value", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int ofc_kg_register_value_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    ofc_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_kg_register_value_nodes: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create value computation subsystem node */
    state->value_system_id = create_ofc_node(
        kg, OFC_KG_VALUE_NAME,
        BRAIN_KG_NODE_COGNITIVE,
        "Value computation system - economic utility estimation",
        admin_token
    );
    if (state->value_system_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ofc_kg_register_value_nodes: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, parent_id, state->value_system_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains value system", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Register core value nodes */
    register_core_value_nodes(kg, state, admin_token);

    /* Register extended value nodes */
    register_extended_value_nodes(kg, state, admin_token);

    /* Create edges between value nodes */
    if (state->expected_value_id != BRAIN_KG_INVALID_NODE &&
        state->received_value_id != BRAIN_KG_INVALID_NODE &&
        state->prediction_error_id != BRAIN_KG_INVALID_NODE) {

        /* Expected and received combine to form prediction error */
        create_ofc_edge(kg, state->expected_value_id, state->prediction_error_id,
            (brain_kg_edge_type_t)OFC_KG_EDGE_DRIVES,
            "expected value contributes to RPE", 0.8f, admin_token);
        state->edge_count++;

        create_ofc_edge(kg, state->received_value_id, state->prediction_error_id,
            (brain_kg_edge_type_t)OFC_KG_EDGE_DRIVES,
            "received value contributes to RPE", 0.8f, admin_token);
        state->edge_count++;
    }

    /* Delay discounts expected value */
    if (state->delay_id != BRAIN_KG_INVALID_NODE &&
        state->expected_value_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, state->delay_id, state->expected_value_id,
            (brain_kg_edge_type_t)OFC_KG_EDGE_DISCOUNTS,
            "temporal delay discounts expected value", 0.7f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Decision Nodes
//=============================================================================

int ofc_kg_register_decision_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    ofc_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_kg_register_decision_nodes: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create decision system subsystem node */
    state->decision_system_id = create_ofc_node(
        kg, OFC_KG_DECISION_NAME,
        BRAIN_KG_NODE_COGNITIVE,
        "Decision system - choice selection and execution",
        admin_token
    );
    if (state->decision_system_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ofc_kg_register_decision_nodes: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, parent_id, state->decision_system_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains decision system", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Binary choice node */
    state->binary_choice_id = create_ofc_node(
        kg, "binary_choice",
        (brain_kg_node_type_t)OFC_KG_NODE_DECISION_TYPE,
        "Binary choice - two-alternative forced choice",
        admin_token
    );
    if (state->binary_choice_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->decision_system_id, state->binary_choice_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "supports binary choice", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Multi-alternative choice node */
    state->multi_choice_id = create_ofc_node(
        kg, "multi_choice",
        (brain_kg_node_type_t)OFC_KG_NODE_DECISION_TYPE,
        "Multi-alternative choice - multiple option selection",
        admin_token
    );
    if (state->multi_choice_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->decision_system_id, state->multi_choice_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "supports multi-choice", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Sequential decision node */
    state->sequential_id = create_ofc_node(
        kg, "sequential_decision",
        (brain_kg_node_type_t)OFC_KG_NODE_DECISION_TYPE,
        "Sequential decision - evidence accumulation over time",
        admin_token
    );
    if (state->sequential_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->decision_system_id, state->sequential_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "supports sequential decision", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Social decision node */
    state->social_decision_id = create_ofc_node(
        kg, "social_decision",
        (brain_kg_node_type_t)OFC_KG_NODE_DECISION_TYPE,
        "Social decision - interpersonal choice",
        admin_token
    );
    if (state->social_decision_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->decision_system_id, state->social_decision_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "supports social decision", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Moral judgment node */
    state->moral_id = create_ofc_node(
        kg, "moral_judgment",
        (brain_kg_node_type_t)OFC_KG_NODE_DECISION_TYPE,
        "Moral judgment - ethical decision making",
        admin_token
    );
    if (state->moral_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_ofc_edge(kg, state->decision_system_id, state->moral_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "supports moral judgment", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Cross Edges
//=============================================================================

/**
 * @brief Register value-to-decision edges
 */
static void register_value_decision_edges(
    brain_kg_t* kg,
    ofc_kg_state_t* state,
    uint64_t admin_token
) {
    /* Value system drives decision system */
    if (state->value_system_id != BRAIN_KG_INVALID_NODE &&
        state->decision_system_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, state->value_system_id, state->decision_system_id,
            (brain_kg_edge_type_t)OFC_KG_EDGE_DRIVES,
            "value computation drives decision", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Expected value influences decisions */
    if (state->expected_value_id != BRAIN_KG_INVALID_NODE &&
        state->decision_system_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, state->expected_value_id, state->decision_system_id,
            (brain_kg_edge_type_t)OFC_KG_EDGE_DRIVES,
            "expected value guides choice", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Risk influences decision threshold */
    if (state->risk_id != BRAIN_KG_INVALID_NODE &&
        state->decision_system_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, state->risk_id, state->decision_system_id,
            (brain_kg_edge_type_t)OFC_KG_EDGE_INFLUENCES_THRESHOLD,
            "risk assessment influences decision threshold", 0.6f, admin_token);
        state->edge_count++;
    }

    /* Social value drives social decision */
    if (state->social_id != BRAIN_KG_INVALID_NODE &&
        state->social_decision_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, state->social_id, state->social_decision_id,
            (brain_kg_edge_type_t)OFC_KG_EDGE_DRIVES,
            "social value drives social decision", 0.8f, admin_token);
        state->edge_count++;
    }
}

/**
 * @brief Register subdivision-to-function edges
 */
static void register_subdivision_edges(
    brain_kg_t* kg,
    ofc_kg_state_t* state,
    uint64_t admin_token
) {
    /* Lateral OFC handles reversal learning */
    if (state->lateral_id != BRAIN_KG_INVALID_NODE &&
        state->prediction_error_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, state->lateral_id, state->prediction_error_id,
            BRAIN_KG_EDGE_MODULATES,
            "lateral OFC processes reversal signals", 0.7f, admin_token);
        state->edge_count++;
    }

    /* Medial OFC handles value comparison */
    if (state->medial_id != BRAIN_KG_INVALID_NODE &&
        state->expected_value_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, state->medial_id, state->expected_value_id,
            BRAIN_KG_EDGE_MODULATES,
            "medial OFC compares values", 0.8f, admin_token);
        state->edge_count++;
    }

    /* Anterior OFC handles abstract rewards */
    if (state->anterior_id != BRAIN_KG_INVALID_NODE &&
        state->social_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, state->anterior_id, state->social_id,
            BRAIN_KG_EDGE_MODULATES,
            "anterior OFC processes social rewards", 0.75f, admin_token);
        state->edge_count++;
    }

    /* Posterior OFC handles primary rewards */
    if (state->posterior_id != BRAIN_KG_INVALID_NODE &&
        state->received_value_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, state->posterior_id, state->received_value_id,
            BRAIN_KG_EDGE_MODULATES,
            "posterior OFC processes primary rewards", 0.85f, admin_token);
        state->edge_count++;
    }
}

/**
 * @brief Register learning-related edges
 */
static void register_learning_edges(
    brain_kg_t* kg,
    ofc_kg_state_t* state,
    uint64_t admin_token
) {
    /* Prediction error triggers learning (updates expected value) */
    if (state->prediction_error_id != BRAIN_KG_INVALID_NODE &&
        state->expected_value_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, state->prediction_error_id, state->expected_value_id,
            (brain_kg_edge_type_t)OFC_KG_EDGE_UPDATES_EXPECTATION,
            "RPE updates expected value (learning)", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Prediction error triggers probability update */
    if (state->prediction_error_id != BRAIN_KG_INVALID_NODE &&
        state->probability_id != BRAIN_KG_INVALID_NODE) {
        create_ofc_edge(kg, state->prediction_error_id, state->probability_id,
            (brain_kg_edge_type_t)OFC_KG_EDGE_UPDATES_EXPECTATION,
            "RPE updates probability estimates", 0.7f, admin_token);
        state->edge_count++;
    }
}

int ofc_kg_register_cross_edges(
    brain_kg_t* kg,
    ofc_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_kg_register_cross_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Register value-to-decision edges */
    register_value_decision_edges(kg, state, admin_token);

    /* Register subdivision-to-function edges */
    register_subdivision_edges(kg, state, admin_token);

    /* Register learning-related edges */
    register_learning_edges(kg, state, admin_token);

    return 0;
}

//=============================================================================
// State Synchronization API
//=============================================================================

int ofc_kg_update_state(
    brain_kg_t* kg,
    const ofc_kg_state_t* state,
    float expected_value,
    float prediction_error,
    float decision_confidence,
    float risk_level,
    uint64_t admin_token
) {
    (void)admin_token;  /* Reserved for future access control */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_kg_update_state: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Update expected value metadata */
    if (state->expected_value_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.3f", expected_value);
        brain_kg_add_metadata(kg, state->expected_value_id, "current_value", val_str);
    }

    /* Update prediction error metadata */
    if (state->prediction_error_id != BRAIN_KG_INVALID_NODE) {
        char rpe_str[32];
        snprintf(rpe_str, sizeof(rpe_str), "%.3f", prediction_error);
        brain_kg_add_metadata(kg, state->prediction_error_id, "current_rpe", rpe_str);
    }

    /* Update decision system metadata */
    if (state->decision_system_id != BRAIN_KG_INVALID_NODE) {
        char conf_str[32];
        snprintf(conf_str, sizeof(conf_str), "%.1f%%", decision_confidence * 100.0f);
        brain_kg_add_metadata(kg, state->decision_system_id, "confidence", conf_str);
    }

    /* Update risk metadata */
    if (state->risk_id != BRAIN_KG_INVALID_NODE) {
        char risk_str[32];
        snprintf(risk_str, sizeof(risk_str), "%.3f", risk_level);
        brain_kg_add_metadata(kg, state->risk_id, "current_risk", risk_str);
    }

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t ofc_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, OFC_KG_ROOT_NAME);
}

brain_kg_node_id_t ofc_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

brain_kg_node_list_t* ofc_kg_get_value_nodes(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)OFC_KG_NODE_VALUE_SIGNAL
    );
}

brain_kg_node_list_t* ofc_kg_get_decision_nodes(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)OFC_KG_NODE_DECISION_TYPE
    );
}

brain_kg_node_list_t* ofc_kg_get_subdivisions(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)OFC_KG_NODE_SUBDIVISION
    );
}

int ofc_kg_unregister_all(
    brain_kg_t* kg,
    ofc_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;  /* Would be used for actual deletion */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    /*
     * Note: Full implementation would remove nodes in reverse order
     * For now, mark as unregistered
     */
    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(OFC_KG_MODULE_NAME, "Unregistered OFC KG nodes");

    return 0;
}

//=============================================================================
// Runtime Event Emission (Wave W2)
//=============================================================================

/*
 * HOT-PATH GAP: OFC has no dedicated tick .c file in this tree; only
 * adapter + quantum bridge. Decision-outcome events should call
 * ofc_kg_emit_event(brain, "decision_outcome", received_minus_expected, ts).
 *
 * Node name uses canonical `orbitofrontal_cortex` (not abbreviated `ofc`)
 * per kg-node-naming-registry.md §1.
 */

void ofc_kg_emit_event(
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
             "orbitofrontal_cortex_event_%s_%" PRIu64, kind, ts_us);
    snprintf(desc, sizeof(desc),
             "OFC runtime event: kind=%s intensity=%.3f ts_us=%" PRIu64,
             kind, (double)intensity, ts_us);

    brain_kg_node_id_t evt_id = brain_kg_add_node(
        kg, name, BRAIN_KG_NODE_CUSTOM, desc
    );

    if (evt_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.4f", (double)intensity);
        brain_kg_add_metadata(kg, evt_id, "intensity", val_str);
        brain_kg_add_metadata(kg, evt_id, "kind", kind);

        brain_kg_node_id_t root = brain_kg_find_node(kg, OFC_KG_ROOT_NAME);
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
