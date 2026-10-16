//=============================================================================
// nimcp_amygdala_kg_wiring.c - Amygdala Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_amygdala_kg_wiring.c
 * @brief Implementation of Amygdala Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for Amygdala module
 * WHY:  Enables semantic queries about fear and emotional processing
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/amygdala/bridges/nimcp_amygdala_kg_wiring.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(amygdala_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create an amygdala node with description
 *
 * WHAT: Helper to create a single KG node
 * WHY:  Reduces boilerplate in registration functions
 * HOW:  Wraps brain_kg_add_node with logging
 */
static brain_kg_node_id_t create_amygdala_node(
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
        NIMCP_LOG_DEBUG(AMYGDALA_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

/**
 * @brief Create an edge between amygdala nodes
 *
 * WHAT: Helper to create a single KG edge
 * WHY:  Reduces boilerplate in registration functions
 * HOW:  Wraps brain_kg_add_edge with validation
 */
static brain_kg_edge_id_t create_amygdala_edge(
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

int amygdala_kg_default_config(amygdala_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amygdala_kg_default_config: config is NULL");
        return -1;
    }

    config->register_bla = true;
    config->register_cea = true;
    config->register_itc = true;
    config->register_emotion_nodes = true;
    config->register_learning_nodes = true;
    config->register_output_nodes = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;
    config->register_extinction_edges = true;

    return 0;
}

//=============================================================================
// Registration API - Main Entry Point
//=============================================================================

int amygdala_kg_register_all(
    brain_kg_t* kg,
    const amygdala_kg_config_t* config,
    amygdala_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amygdala_kg_register_all: kg is NULL");
        return -1;
    }

    /* Use provided config or defaults */
    amygdala_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        amygdala_kg_default_config(&local_config);
    }

    /* Initialize local state */
    amygdala_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create amygdala root node */
    local_state.root_id = create_amygdala_node(
        kg, AMYGDALA_KG_ROOT_NAME,
        BRAIN_KG_NODE_SUBCORTICAL,
        "Amygdala - fear processing, emotional learning, threat detection",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(AMYGDALA_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "amygdala_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Register nuclei */
    if (amygdala_kg_register_nuclei(kg, local_state.root_id, &local_config,
                                     &local_state, admin_token) < 0) {
        NIMCP_LOG_WARN(AMYGDALA_KG_MODULE_NAME, "Failed to register nuclei");
    }

    /* Register emotion nodes */
    if (local_config.register_emotion_nodes) {
        if (amygdala_kg_register_emotion_nodes(kg, local_state.root_id,
                                                &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(AMYGDALA_KG_MODULE_NAME, "Failed to register emotion nodes");
        }
    }

    /* Register learning nodes */
    if (local_config.register_learning_nodes) {
        if (amygdala_kg_register_learning_nodes(kg, local_state.root_id,
                                                 &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(AMYGDALA_KG_MODULE_NAME, "Failed to register learning nodes");
        }
    }

    /* Register output nodes */
    if (local_config.register_output_nodes) {
        if (amygdala_kg_register_output_nodes(kg, local_state.root_id,
                                               &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(AMYGDALA_KG_MODULE_NAME, "Failed to register output nodes");
        }
    }

    /* Register cross-subsystem edges */
    if (local_config.register_cross_edges) {
        amygdala_kg_register_cross_edges(kg, &local_state, admin_token);
    }

    local_state.registered = true;

    /* Copy to output if requested */
    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(AMYGDALA_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Registration API - Nuclei
//=============================================================================

int amygdala_kg_register_nuclei(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const amygdala_kg_config_t* config,
    amygdala_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amygdala_kg_register_nuclei: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Register Basolateral Complex */
    if (!config || config->register_bla) {
        state->bla_id = create_amygdala_node(
            kg, AMYGDALA_KG_BLA_NAME,
            (brain_kg_node_type_t)AMYGDALA_KG_NODE_NUCLEUS,
            "Basolateral Complex - sensory input integration, CS-US associations",
            admin_token
        );
        if (state->bla_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_amygdala_edge(kg, parent_id, state->bla_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains BLA", 1.0f, admin_token);
            state->edge_count++;
        }

        /* Register Lateral Nucleus (part of BLA) */
        state->la_id = create_amygdala_node(
            kg, AMYGDALA_KG_LA_NAME,
            (brain_kg_node_type_t)AMYGDALA_KG_NODE_NUCLEUS,
            "Lateral nucleus - sensory input gateway, fear conditioning",
            admin_token
        );
        if (state->la_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_amygdala_edge(kg, state->bla_id, state->la_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains LA", 1.0f, admin_token);
            state->edge_count++;
        }

        /* Register Basal Nucleus (part of BLA) */
        state->ba_id = create_amygdala_node(
            kg, AMYGDALA_KG_BA_NAME,
            (brain_kg_node_type_t)AMYGDALA_KG_NODE_NUCLEUS,
            "Basal nucleus - hippocampal input, contextual fear",
            admin_token
        );
        if (state->ba_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_amygdala_edge(kg, state->bla_id, state->ba_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains BA", 1.0f, admin_token);
            state->edge_count++;
        }

        /* LA -> BA pathway */
        if (state->la_id != BRAIN_KG_INVALID_NODE &&
            state->ba_id != BRAIN_KG_INVALID_NODE) {
            create_amygdala_edge(kg, state->la_id, state->ba_id,
                BRAIN_KG_EDGE_SENDS_TO, "LA projects to BA", 0.8f, admin_token);
            state->edge_count++;
        }
    }

    /* Register Central Nucleus */
    if (!config || config->register_cea) {
        state->cea_id = create_amygdala_node(
            kg, AMYGDALA_KG_CEA_NAME,
            (brain_kg_node_type_t)AMYGDALA_KG_NODE_NUCLEUS,
            "Central nucleus - fear response output, autonomic control",
            admin_token
        );
        if (state->cea_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_amygdala_edge(kg, parent_id, state->cea_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains CeA", 1.0f, admin_token);
            state->edge_count++;
        }

        /* CeA medial division */
        state->cea_m_id = create_amygdala_node(
            kg, "cea_medial",
            (brain_kg_node_type_t)AMYGDALA_KG_NODE_NUCLEUS,
            "CeA medial - conditioned fear responses, brainstem projections",
            admin_token
        );
        if (state->cea_m_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_amygdala_edge(kg, state->cea_id, state->cea_m_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains CeM", 1.0f, admin_token);
            state->edge_count++;
        }

        /* CeA lateral division */
        state->cea_l_id = create_amygdala_node(
            kg, "cea_lateral",
            (brain_kg_node_type_t)AMYGDALA_KG_NODE_NUCLEUS,
            "CeA lateral - attention, arousal modulation",
            admin_token
        );
        if (state->cea_l_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_amygdala_edge(kg, state->cea_id, state->cea_l_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains CeL", 1.0f, admin_token);
            state->edge_count++;
        }

        /* BLA -> CeA pathway */
        if (state->bla_id != BRAIN_KG_INVALID_NODE &&
            state->cea_id != BRAIN_KG_INVALID_NODE) {
            create_amygdala_edge(kg, state->bla_id, state->cea_id,
                BRAIN_KG_EDGE_SENDS_TO, "BLA projects to CeA", 0.9f, admin_token);
            state->edge_count++;
        }
    }

    /* Register Intercalated Cells */
    if (!config || config->register_itc) {
        state->itc_id = create_amygdala_node(
            kg, AMYGDALA_KG_ITC_NAME,
            (brain_kg_node_type_t)AMYGDALA_KG_NODE_NUCLEUS,
            "Intercalated cells - GABAergic inhibition, fear extinction",
            admin_token
        );
        if (state->itc_id != BRAIN_KG_INVALID_NODE) {
            state->node_count++;
            create_amygdala_edge(kg, parent_id, state->itc_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains ITC", 1.0f, admin_token);
            state->edge_count++;
        }

        /* ITC inhibits CeA (extinction pathway) */
        if (state->itc_id != BRAIN_KG_INVALID_NODE &&
            state->cea_id != BRAIN_KG_INVALID_NODE) {
            create_amygdala_edge(kg, state->itc_id, state->cea_id,
                BRAIN_KG_EDGE_INHIBITS, "ITC inhibits CeA (extinction)", 0.85f, admin_token);
            state->edge_count++;
        }

        /* BLA -> ITC pathway */
        if (state->bla_id != BRAIN_KG_INVALID_NODE &&
            state->itc_id != BRAIN_KG_INVALID_NODE) {
            create_amygdala_edge(kg, state->bla_id, state->itc_id,
                BRAIN_KG_EDGE_SENDS_TO, "BLA activates ITC", 0.7f, admin_token);
            state->edge_count++;
        }
    }

    return 0;
}

//=============================================================================
// Registration API - Emotion Nodes
//=============================================================================

int amygdala_kg_register_emotion_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    amygdala_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amygdala_kg_register_emotion_nodes: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create emotional processing system node */
    state->emotion_system_id = create_amygdala_node(
        kg, AMYGDALA_KG_EMOTION_NAME,
        BRAIN_KG_NODE_COGNITIVE,
        "Emotional processing system - fear, threat, valence assessment",
        admin_token
    );
    if (state->emotion_system_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "amygdala_kg_register_emotion_nodes: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, parent_id, state->emotion_system_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains emotional processing", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Fear detection node */
    state->fear_detect_id = create_amygdala_node(
        kg, "fear_detection",
        (brain_kg_node_type_t)AMYGDALA_KG_NODE_EMOTION_PROCESS,
        "Fear detection - rapid threat identification, fear response initiation",
        admin_token
    );
    if (state->fear_detect_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_amygdala_edge(kg, state->emotion_system_id, state->fear_detect_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "performs fear detection", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Threat assessment node */
    state->threat_id = create_amygdala_node(
        kg, "threat_assessment",
        (brain_kg_node_type_t)AMYGDALA_KG_NODE_EMOTION_PROCESS,
        "Threat assessment - danger level evaluation, risk analysis",
        admin_token
    );
    if (state->threat_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_amygdala_edge(kg, state->emotion_system_id, state->threat_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "performs threat assessment", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Emotional valence node */
    state->valence_id = create_amygdala_node(
        kg, "emotional_valence",
        (brain_kg_node_type_t)AMYGDALA_KG_NODE_EMOTION_TYPE,
        "Emotional valence - positive/negative value assignment",
        admin_token
    );
    if (state->valence_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_amygdala_edge(kg, state->emotion_system_id, state->valence_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "computes valence", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Arousal modulation node */
    state->arousal_id = create_amygdala_node(
        kg, "arousal_modulation",
        (brain_kg_node_type_t)AMYGDALA_KG_NODE_AROUSAL_STATE,
        "Arousal modulation - alertness level, vigilance control",
        admin_token
    );
    if (state->arousal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_amygdala_edge(kg, state->emotion_system_id, state->arousal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "modulates arousal", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Social emotion node */
    state->social_id = create_amygdala_node(
        kg, "social_emotion",
        (brain_kg_node_type_t)AMYGDALA_KG_NODE_EMOTION_TYPE,
        "Social emotion - face processing, social threat, empathy",
        admin_token
    );
    if (state->social_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_amygdala_edge(kg, state->emotion_system_id, state->social_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "processes social emotion", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Threat triggers fear detection */
    if (state->threat_id != BRAIN_KG_INVALID_NODE &&
        state->fear_detect_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->threat_id, state->fear_detect_id,
            (brain_kg_edge_type_t)AMYGDALA_KG_EDGE_TRIGGERS_FEAR,
            "threat triggers fear", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Fear detection modulates arousal */
    if (state->fear_detect_id != BRAIN_KG_INVALID_NODE &&
        state->arousal_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->fear_detect_id, state->arousal_id,
            (brain_kg_edge_type_t)AMYGDALA_KG_EDGE_MODULATES_AROUSAL,
            "fear increases arousal", 0.85f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Learning Nodes
//=============================================================================

int amygdala_kg_register_learning_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    amygdala_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amygdala_kg_register_learning_nodes: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create learning system node */
    state->learning_system_id = create_amygdala_node(
        kg, AMYGDALA_KG_LEARNING_NAME,
        BRAIN_KG_NODE_PLASTICITY,
        "Emotional learning system - fear conditioning, extinction, memory",
        admin_token
    );
    if (state->learning_system_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "amygdala_kg_register_learning_nodes: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, parent_id, state->learning_system_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains learning system", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Fear conditioning node */
    state->conditioning_id = create_amygdala_node(
        kg, "fear_conditioning",
        (brain_kg_node_type_t)AMYGDALA_KG_NODE_LEARNING_PROCESS,
        "Fear conditioning - CS-US association, Hebbian plasticity",
        admin_token
    );
    if (state->conditioning_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_amygdala_edge(kg, state->learning_system_id, state->conditioning_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "supports conditioning", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Fear extinction node */
    state->extinction_id = create_amygdala_node(
        kg, "fear_extinction",
        (brain_kg_node_type_t)AMYGDALA_KG_NODE_LEARNING_PROCESS,
        "Fear extinction - inhibitory learning, safety memory formation",
        admin_token
    );
    if (state->extinction_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_amygdala_edge(kg, state->learning_system_id, state->extinction_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "supports extinction", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Emotional memory node */
    state->emotional_memory_id = create_amygdala_node(
        kg, "emotional_memory",
        (brain_kg_node_type_t)AMYGDALA_KG_NODE_LEARNING_PROCESS,
        "Emotional memory - fear memory storage, emotional enhancement",
        admin_token
    );
    if (state->emotional_memory_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_amygdala_edge(kg, state->learning_system_id, state->emotional_memory_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "stores emotional memories", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Conditioning strengthens fear */
    if (state->conditioning_id != BRAIN_KG_INVALID_NODE &&
        state->fear_detect_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->conditioning_id, state->fear_detect_id,
            (brain_kg_edge_type_t)AMYGDALA_KG_EDGE_STRENGTHENS,
            "conditioning strengthens fear response", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Extinction inhibits fear */
    if (state->extinction_id != BRAIN_KG_INVALID_NODE &&
        state->fear_detect_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->extinction_id, state->fear_detect_id,
            (brain_kg_edge_type_t)AMYGDALA_KG_EDGE_INHIBITS_FEAR,
            "extinction inhibits fear response", 0.8f, admin_token);
        state->edge_count++;
    }

    /* Conditioning creates emotional memory */
    if (state->conditioning_id != BRAIN_KG_INVALID_NODE &&
        state->emotional_memory_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->conditioning_id, state->emotional_memory_id,
            BRAIN_KG_EDGE_SENDS_TO,
            "conditioning creates emotional memory", 0.9f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Output Nodes
//=============================================================================

int amygdala_kg_register_output_nodes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    amygdala_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amygdala_kg_register_output_nodes: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Autonomic output node */
    state->autonomic_id = create_amygdala_node(
        kg, "autonomic_output",
        (brain_kg_node_type_t)AMYGDALA_KG_NODE_OUTPUT_PATHWAY,
        "Autonomic output - heart rate, blood pressure, sweating",
        admin_token
    );
    if (state->autonomic_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_amygdala_edge(kg, parent_id, state->autonomic_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "generates autonomic output", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Behavioral output node */
    state->behavioral_id = create_amygdala_node(
        kg, "behavioral_output",
        (brain_kg_node_type_t)AMYGDALA_KG_NODE_OUTPUT_PATHWAY,
        "Behavioral output - freezing, fight-flight, defensive behavior",
        admin_token
    );
    if (state->behavioral_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_amygdala_edge(kg, parent_id, state->behavioral_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "generates behavioral output", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Hormonal output node */
    state->hormonal_id = create_amygdala_node(
        kg, "hormonal_output",
        (brain_kg_node_type_t)AMYGDALA_KG_NODE_OUTPUT_PATHWAY,
        "Hormonal output - HPA axis, cortisol, stress response",
        admin_token
    );
    if (state->hormonal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_amygdala_edge(kg, parent_id, state->hormonal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "generates hormonal output", 1.0f, admin_token);
        state->edge_count++;
    }

    /* CeA drives all outputs */
    if (state->cea_id != BRAIN_KG_INVALID_NODE) {
        if (state->autonomic_id != BRAIN_KG_INVALID_NODE) {
            create_amygdala_edge(kg, state->cea_id, state->autonomic_id,
                (brain_kg_edge_type_t)AMYGDALA_KG_EDGE_GENERATES_RESPONSE,
                "CeA triggers autonomic response", 0.9f, admin_token);
            state->edge_count++;
        }

        if (state->behavioral_id != BRAIN_KG_INVALID_NODE) {
            create_amygdala_edge(kg, state->cea_id, state->behavioral_id,
                (brain_kg_edge_type_t)AMYGDALA_KG_EDGE_GENERATES_RESPONSE,
                "CeA triggers behavioral response", 0.9f, admin_token);
            state->edge_count++;
        }

        if (state->hormonal_id != BRAIN_KG_INVALID_NODE) {
            create_amygdala_edge(kg, state->cea_id, state->hormonal_id,
                (brain_kg_edge_type_t)AMYGDALA_KG_EDGE_GENERATES_RESPONSE,
                "CeA triggers hormonal response", 0.85f, admin_token);
            state->edge_count++;
        }
    }

    return 0;
}

//=============================================================================
// Registration API - Cross Edges
//=============================================================================

/**
 * @brief Register nucleus-to-function edges
 */
static void register_nucleus_function_edges(
    brain_kg_t* kg,
    amygdala_kg_state_t* state,
    uint64_t admin_token
) {
    /* LA handles threat assessment */
    if (state->la_id != BRAIN_KG_INVALID_NODE &&
        state->threat_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->la_id, state->threat_id,
            BRAIN_KG_EDGE_MODULATES,
            "LA performs threat assessment", 0.9f, admin_token);
        state->edge_count++;
    }

    /* BA handles contextual modulation */
    if (state->ba_id != BRAIN_KG_INVALID_NODE &&
        state->emotional_memory_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->ba_id, state->emotional_memory_id,
            BRAIN_KG_EDGE_MODULATES,
            "BA provides contextual modulation", 0.8f, admin_token);
        state->edge_count++;
    }

    /* CeA lateral handles arousal */
    if (state->cea_l_id != BRAIN_KG_INVALID_NODE &&
        state->arousal_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->cea_l_id, state->arousal_id,
            BRAIN_KG_EDGE_MODULATES,
            "CeL modulates arousal", 0.85f, admin_token);
        state->edge_count++;
    }

    /* CeA medial handles fear responses */
    if (state->cea_m_id != BRAIN_KG_INVALID_NODE &&
        state->fear_detect_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->cea_m_id, state->fear_detect_id,
            BRAIN_KG_EDGE_COORDINATES_WITH,
            "CeM coordinates fear response", 0.9f, admin_token);
        state->edge_count++;
    }
}

/**
 * @brief Register extinction-related edges
 */
static void register_extinction_edges(
    brain_kg_t* kg,
    amygdala_kg_state_t* state,
    uint64_t admin_token
) {
    /* ITC drives extinction */
    if (state->itc_id != BRAIN_KG_INVALID_NODE &&
        state->extinction_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->itc_id, state->extinction_id,
            BRAIN_KG_EDGE_MODULATES,
            "ITC mediates extinction", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Extinction forms safety memory */
    if (state->extinction_id != BRAIN_KG_INVALID_NODE &&
        state->emotional_memory_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->extinction_id, state->emotional_memory_id,
            BRAIN_KG_EDGE_SENDS_TO,
            "extinction creates safety memory", 0.8f, admin_token);
        state->edge_count++;
    }
}

/**
 * @brief Register emotion-learning integration edges
 */
static void register_emotion_learning_edges(
    brain_kg_t* kg,
    amygdala_kg_state_t* state,
    uint64_t admin_token
) {
    /* Emotional processing integrates with learning */
    if (state->emotion_system_id != BRAIN_KG_INVALID_NODE &&
        state->learning_system_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->emotion_system_id, state->learning_system_id,
            BRAIN_KG_EDGE_INTEGRATES_WITH,
            "emotion drives learning", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Fear detection triggers conditioning */
    if (state->fear_detect_id != BRAIN_KG_INVALID_NODE &&
        state->conditioning_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->fear_detect_id, state->conditioning_id,
            BRAIN_KG_EDGE_SENDS_TO,
            "fear enables conditioning", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Valence influences emotional memory */
    if (state->valence_id != BRAIN_KG_INVALID_NODE &&
        state->emotional_memory_id != BRAIN_KG_INVALID_NODE) {
        create_amygdala_edge(kg, state->valence_id, state->emotional_memory_id,
            BRAIN_KG_EDGE_MODULATES,
            "valence modulates memory strength", 0.8f, admin_token);
        state->edge_count++;
    }
}

int amygdala_kg_register_cross_edges(
    brain_kg_t* kg,
    amygdala_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amygdala_kg_register_cross_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Register nucleus-to-function edges */
    register_nucleus_function_edges(kg, state, admin_token);

    /* Register extinction-related edges */
    register_extinction_edges(kg, state, admin_token);

    /* Register emotion-learning integration edges */
    register_emotion_learning_edges(kg, state, admin_token);

    return 0;
}

//=============================================================================
// State Synchronization API
//=============================================================================

int amygdala_kg_update_state(
    brain_kg_t* kg,
    const amygdala_kg_state_t* state,
    float threat_level,
    float fear_strength,
    float arousal_level,
    float extinction_progress,
    uint64_t admin_token
) {
    (void)admin_token;  /* Reserved for future access control */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amygdala_kg_update_state: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Update threat metadata */
    if (state->threat_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.3f", threat_level);
        brain_kg_add_metadata(kg, state->threat_id, "threat_level", val_str);
    }

    /* Update fear metadata */
    if (state->fear_detect_id != BRAIN_KG_INVALID_NODE) {
        char fear_str[32];
        snprintf(fear_str, sizeof(fear_str), "%.3f", fear_strength);
        brain_kg_add_metadata(kg, state->fear_detect_id, "fear_strength", fear_str);
    }

    /* Update arousal metadata */
    if (state->arousal_id != BRAIN_KG_INVALID_NODE) {
        char arousal_str[32];
        snprintf(arousal_str, sizeof(arousal_str), "%.3f", arousal_level);
        brain_kg_add_metadata(kg, state->arousal_id, "arousal_level", arousal_str);
    }

    /* Update extinction metadata */
    if (state->extinction_id != BRAIN_KG_INVALID_NODE) {
        char ext_str[32];
        snprintf(ext_str, sizeof(ext_str), "%.1f%%", extinction_progress * 100.0f);
        brain_kg_add_metadata(kg, state->extinction_id, "progress", ext_str);
    }

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t amygdala_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, AMYGDALA_KG_ROOT_NAME);
}

brain_kg_node_id_t amygdala_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

brain_kg_node_list_t* amygdala_kg_get_emotion_nodes(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)AMYGDALA_KG_NODE_EMOTION_PROCESS
    );
}

brain_kg_node_list_t* amygdala_kg_get_learning_nodes(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)AMYGDALA_KG_NODE_LEARNING_PROCESS
    );
}

brain_kg_node_list_t* amygdala_kg_get_nuclei(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)AMYGDALA_KG_NODE_NUCLEUS
    );
}

int amygdala_kg_unregister_all(
    brain_kg_t* kg,
    amygdala_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;  /* Would be used for actual deletion */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amygdala_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    /*
     * Note: Full implementation would remove nodes in reverse order
     * For now, mark as unregistered
     */
    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(AMYGDALA_KG_MODULE_NAME, "Unregistered Amygdala KG nodes");

    return 0;
}

//=============================================================================
// Runtime Event Emission (Wave W2)
//=============================================================================

/*
 * HOT-PATH GAP: The amygdala region has no dedicated tick/update .c file in
 * this tree (only bridges + this KG-wiring file). The owning tick path lives
 * in the subcortical amygdala bridges (see subcortical/nimcp_amygdala_*.c).
 * Callers that observe a fear spike or reward event should invoke
 * amygdala_kg_emit_event() at the point the event occurs; no autonomous
 * wiring is added in W2.
 */

void amygdala_kg_emit_event(
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

    /* Elevate to ADMIN for the duration of this write. See
     * docs/claude/kg-node-naming-registry.md §7. */
    if (brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, tok) != 0) {
        return;
    }

    char name[128];
    char desc[192];
    snprintf(name, sizeof(name),
             "amygdala_event_%s_%" PRIu64, kind, ts_us);
    snprintf(desc, sizeof(desc),
             "amygdala runtime event: kind=%s intensity=%.3f ts_us=%" PRIu64,
             kind, (double)intensity, ts_us);

    brain_kg_node_id_t evt_id = brain_kg_add_node(
        kg, name, BRAIN_KG_NODE_CUSTOM, desc
    );

    if (evt_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.4f", (double)intensity);
        brain_kg_add_metadata(kg, evt_id, "intensity", val_str);
        brain_kg_add_metadata(kg, evt_id, "kind", kind);

        brain_kg_node_id_t root = brain_kg_find_node(kg, AMYGDALA_KG_ROOT_NAME);
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
