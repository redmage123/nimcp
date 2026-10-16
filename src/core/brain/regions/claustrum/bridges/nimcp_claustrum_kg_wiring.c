//=============================================================================
// nimcp_claustrum_kg_wiring.c - Claustrum Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_claustrum_kg_wiring.c
 * @brief Implementation of claustrum layer KG registration
 */

#include "core/brain/regions/claustrum/bridges/nimcp_claustrum_kg_wiring.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(claustrum_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create a claustrum node with description
 *
 * WHAT: Helper to create a node with logging
 * WHY:  Consistent node creation across registration functions
 * HOW:  Call brain_kg_add_node with logging
 */
static brain_kg_node_id_t create_claustrum_node(
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
        NIMCP_LOG_DEBUG(CLAUSTRUM_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

/**
 * @brief Create an edge between claustrum nodes
 *
 * WHAT: Helper to create an edge with validation
 * WHY:  Consistent edge creation with null checks
 * HOW:  Validate nodes, call brain_kg_add_edge
 */
static brain_kg_edge_id_t create_claustrum_edge(
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

    brain_kg_edge_id_t id = brain_kg_add_edge(
        kg, from, to, type, description, weight
    );
    return id;
}

//=============================================================================
// Configuration API
//=============================================================================

int claustrum_kg_default_config(claustrum_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_kg_default_config: config is NULL");
        return -1;
    }

    config->register_modalities = true;
    config->register_states = true;
    config->register_consciousness = true;
    config->register_oscillations = true;
    config->register_binding_edges = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int claustrum_kg_register_all(
    brain_kg_t* kg,
    const claustrum_kg_config_t* config,
    claustrum_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_kg_register_all: kg is NULL");
        return -1;
    }

    /* Use provided config or defaults */
    claustrum_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        claustrum_kg_default_config(&local_config);
    }

    /* Initialize local state */
    claustrum_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create claustrum root node */
    local_state.root_id = create_claustrum_node(
        kg, CLAUSTRUM_KG_ROOT_NAME,
        BRAIN_KG_NODE_SUBCORTICAL,
        "Claustrum - consciousness integration and cross-modal binding",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(CLAUSTRUM_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Register subsystems based on configuration */
    if (local_config.register_modalities) {
        int ret = claustrum_kg_register_modalities(
            kg, local_state.root_id, &local_state, admin_token
        );
        if (ret < 0) {
            NIMCP_LOG_WARN(CLAUSTRUM_KG_MODULE_NAME,
                "Failed to register modalities subsystem");
        }
    }

    if (local_config.register_states) {
        int ret = claustrum_kg_register_states(
            kg, local_state.root_id, &local_state, admin_token
        );
        if (ret < 0) {
            NIMCP_LOG_WARN(CLAUSTRUM_KG_MODULE_NAME,
                "Failed to register states subsystem");
        }
    }

    if (local_config.register_consciousness) {
        int ret = claustrum_kg_register_consciousness(
            kg, local_state.root_id, &local_state, admin_token
        );
        if (ret < 0) {
            NIMCP_LOG_WARN(CLAUSTRUM_KG_MODULE_NAME,
                "Failed to register consciousness subsystem");
        }
    }

    if (local_config.register_oscillations) {
        int ret = claustrum_kg_register_oscillations(
            kg, local_state.root_id, &local_state, admin_token
        );
        if (ret < 0) {
            NIMCP_LOG_WARN(CLAUSTRUM_KG_MODULE_NAME,
                "Failed to register oscillations subsystem");
        }
    }

    /* Register edges */
    if (local_config.register_binding_edges) {
        claustrum_kg_register_binding_edges(kg, &local_state, admin_token);
    }

    if (local_config.register_cross_edges) {
        claustrum_kg_register_cross_edges(kg, &local_state, admin_token);
    }

    local_state.registered = true;

    /* Copy to output if requested */
    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(CLAUSTRUM_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

int claustrum_kg_register_modalities(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    claustrum_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_kg_register_modalities: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create modalities subsystem node */
    state->modalities_id = create_claustrum_node(
        kg, CLAUSTRUM_KG_MODALITIES_NAME,
        BRAIN_KG_NODE_PERCEPTION,
        "Sensory modality inputs for cross-modal binding",
        admin_token
    );
    if (state->modalities_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_kg_register_modalities: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, parent_id, state->modalities_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains modalities", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create individual modality nodes */
    state->visual_id = create_claustrum_node(
        kg, "claustrum_visual",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_MODALITY,
        "Visual modality - V1-V4 cortical input stream",
        admin_token
    );
    if (state->visual_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->modalities_id, state->visual_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "receives visual input", 1.0f, admin_token);
        state->edge_count++;
    }

    state->auditory_id = create_claustrum_node(
        kg, "claustrum_auditory",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_MODALITY,
        "Auditory modality - A1 and belt region input stream",
        admin_token
    );
    if (state->auditory_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->modalities_id, state->auditory_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "receives auditory input", 1.0f, admin_token);
        state->edge_count++;
    }

    state->somatosensory_id = create_claustrum_node(
        kg, "claustrum_somatosensory",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_MODALITY,
        "Somatosensory modality - tactile and proprioceptive input",
        admin_token
    );
    if (state->somatosensory_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->modalities_id, state->somatosensory_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "receives somatosensory input", 1.0f, admin_token);
        state->edge_count++;
    }

    state->olfactory_id = create_claustrum_node(
        kg, "claustrum_olfactory",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_MODALITY,
        "Olfactory modality - smell input stream",
        admin_token
    );
    if (state->olfactory_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->modalities_id, state->olfactory_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "receives olfactory input", 1.0f, admin_token);
        state->edge_count++;
    }

    state->gustatory_id = create_claustrum_node(
        kg, "claustrum_gustatory",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_MODALITY,
        "Gustatory modality - taste input stream",
        admin_token
    );
    if (state->gustatory_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->modalities_id, state->gustatory_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "receives gustatory input", 1.0f, admin_token);
        state->edge_count++;
    }

    state->interoceptive_id = create_claustrum_node(
        kg, "claustrum_interoceptive",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_MODALITY,
        "Interoceptive modality - internal body state signals",
        admin_token
    );
    if (state->interoceptive_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->modalities_id, state->interoceptive_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "receives interoceptive input", 1.0f, admin_token);
        state->edge_count++;
    }

    state->vestibular_id = create_claustrum_node(
        kg, "claustrum_vestibular",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_MODALITY,
        "Vestibular modality - balance and spatial orientation",
        admin_token
    );
    if (state->vestibular_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->modalities_id, state->vestibular_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "receives vestibular input", 1.0f, admin_token);
        state->edge_count++;
    }

    state->motor_efference_id = create_claustrum_node(
        kg, "claustrum_motor_efference",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_MODALITY,
        "Motor efference copy - predicted sensory consequences",
        admin_token
    );
    if (state->motor_efference_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->modalities_id, state->motor_efference_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "receives motor efference copy", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int claustrum_kg_register_states(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    claustrum_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_kg_register_states: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create states subsystem node */
    state->states_id = create_claustrum_node(
        kg, CLAUSTRUM_KG_STATES_NAME,
        BRAIN_KG_NODE_COGNITIVE,
        "Claustrum operational states",
        admin_token
    );
    if (state->states_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_kg_register_states: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, parent_id, state->states_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains states", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create individual state nodes */
    state->binding_state_id = create_claustrum_node(
        kg, "claustrum_state_binding",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_STATE,
        "Binding state - cross-modal integration active",
        admin_token
    );
    if (state->binding_state_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->states_id, state->binding_state_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "binding state", 1.0f, admin_token);
        state->edge_count++;
    }

    state->synchronizing_state_id = create_claustrum_node(
        kg, "claustrum_state_synchronizing",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_STATE,
        "Synchronizing state - temporal alignment active",
        admin_token
    );
    if (state->synchronizing_state_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->states_id, state->synchronizing_state_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "synchronizing state", 1.0f, admin_token);
        state->edge_count++;
    }

    state->switching_state_id = create_claustrum_node(
        kg, "claustrum_state_switching",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_STATE,
        "Switching state - task/brain state transition",
        admin_token
    );
    if (state->switching_state_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->states_id, state->switching_state_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "switching state", 1.0f, admin_token);
        state->edge_count++;
    }

    state->broadcasting_state_id = create_claustrum_node(
        kg, "claustrum_state_broadcasting",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_STATE,
        "Broadcasting state - global workspace dissemination",
        admin_token
    );
    if (state->broadcasting_state_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->states_id, state->broadcasting_state_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "broadcasting state", 1.0f, admin_token);
        state->edge_count++;
    }

    state->gating_state_id = create_claustrum_node(
        kg, "claustrum_state_gating",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_STATE,
        "Gating state - workspace access control",
        admin_token
    );
    if (state->gating_state_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->states_id, state->gating_state_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "gating state", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int claustrum_kg_register_consciousness(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    claustrum_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_kg_register_consciousness: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create consciousness subsystem node */
    state->consciousness_id = create_claustrum_node(
        kg, CLAUSTRUM_KG_CONSCIOUSNESS_NAME,
        BRAIN_KG_NODE_COGNITIVE,
        "Consciousness levels - GWT-based awareness hierarchy",
        admin_token
    );
    if (state->consciousness_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_kg_register_consciousness: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, parent_id, state->consciousness_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "manages consciousness", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create individual consciousness level nodes */
    state->unconscious_id = create_claustrum_node(
        kg, "claustrum_level_unconscious",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_CONSCIOUSNESS_LEVEL,
        "Unconscious level - below awareness threshold",
        admin_token
    );
    if (state->unconscious_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->consciousness_id, state->unconscious_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "unconscious level", 1.0f, admin_token);
        state->edge_count++;
    }

    state->preconscious_id = create_claustrum_node(
        kg, "claustrum_level_preconscious",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_CONSCIOUSNESS_LEVEL,
        "Preconscious level - available but not accessed",
        admin_token
    );
    if (state->preconscious_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->consciousness_id, state->preconscious_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "preconscious level", 1.0f, admin_token);
        state->edge_count++;
    }

    state->conscious_id = create_claustrum_node(
        kg, "claustrum_level_conscious",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_CONSCIOUSNESS_LEVEL,
        "Conscious level - in global workspace",
        admin_token
    );
    if (state->conscious_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->consciousness_id, state->conscious_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "conscious level", 1.0f, admin_token);
        state->edge_count++;
    }

    state->focal_id = create_claustrum_node(
        kg, "claustrum_level_focal",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_CONSCIOUSNESS_LEVEL,
        "Focal level - center of attention",
        admin_token
    );
    if (state->focal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->consciousness_id, state->focal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "focal level", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create hierarchy edges between consciousness levels */
    if (state->unconscious_id != BRAIN_KG_INVALID_NODE &&
        state->preconscious_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->unconscious_id, state->preconscious_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_ENABLES,
            "can transition to preconscious", 0.7f, admin_token);
        state->edge_count++;
    }

    if (state->preconscious_id != BRAIN_KG_INVALID_NODE &&
        state->conscious_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->preconscious_id, state->conscious_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_ENABLES,
            "can transition to conscious", 0.7f, admin_token);
        state->edge_count++;
    }

    if (state->conscious_id != BRAIN_KG_INVALID_NODE &&
        state->focal_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->conscious_id, state->focal_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_ENABLES,
            "can transition to focal attention", 0.7f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int claustrum_kg_register_oscillations(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    claustrum_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_kg_register_oscillations: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create oscillations subsystem node */
    state->oscillations_id = create_claustrum_node(
        kg, CLAUSTRUM_KG_OSCILLATIONS_NAME,
        BRAIN_KG_NODE_INTEGRATION,
        "Neural oscillations for synchronization and binding",
        admin_token
    );
    if (state->oscillations_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_kg_register_oscillations: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, parent_id, state->oscillations_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains oscillations", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create gamma oscillation node */
    state->gamma_id = create_claustrum_node(
        kg, "claustrum_gamma_oscillation",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_OSCILLATION,
        "Gamma oscillation (30-100 Hz) - feature binding",
        admin_token
    );
    if (state->gamma_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->oscillations_id, state->gamma_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "gamma band oscillator", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create alpha oscillation node */
    state->alpha_id = create_claustrum_node(
        kg, "claustrum_alpha_oscillation",
        (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_OSCILLATION,
        "Alpha oscillation (8-12 Hz) - attention gating",
        admin_token
    );
    if (state->alpha_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_claustrum_edge(kg, state->oscillations_id, state->alpha_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "alpha band oscillator", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create cross-frequency coupling edge */
    if (state->gamma_id != BRAIN_KG_INVALID_NODE &&
        state->alpha_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->alpha_id, state->gamma_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_COUPLES_OSCILLATION,
            "alpha modulates gamma amplitude (PAC)", 0.8f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int claustrum_kg_register_binding_edges(
    brain_kg_t* kg,
    claustrum_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_kg_register_binding_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Visual-auditory binding (audiovisual integration) */
    if (state->visual_id != BRAIN_KG_INVALID_NODE &&
        state->auditory_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->visual_id, state->auditory_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_CROSS_BINDS_WITH,
            "audiovisual binding pathway", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Visual-somatosensory binding */
    if (state->visual_id != BRAIN_KG_INVALID_NODE &&
        state->somatosensory_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->visual_id, state->somatosensory_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_CROSS_BINDS_WITH,
            "visuotactile binding pathway", 0.8f, admin_token);
        state->edge_count++;
    }

    /* Auditory-somatosensory binding */
    if (state->auditory_id != BRAIN_KG_INVALID_NODE &&
        state->somatosensory_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->auditory_id, state->somatosensory_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_CROSS_BINDS_WITH,
            "audiotactile binding pathway", 0.7f, admin_token);
        state->edge_count++;
    }

    /* Olfactory-gustatory binding (flavor perception) */
    if (state->olfactory_id != BRAIN_KG_INVALID_NODE &&
        state->gustatory_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->olfactory_id, state->gustatory_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_CROSS_BINDS_WITH,
            "flavor binding (olfactory-gustatory)", 0.95f, admin_token);
        state->edge_count++;
    }

    /* Vestibular-visual binding (spatial orientation) */
    if (state->vestibular_id != BRAIN_KG_INVALID_NODE &&
        state->visual_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->vestibular_id, state->visual_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_CROSS_BINDS_WITH,
            "spatial orientation binding", 0.8f, admin_token);
        state->edge_count++;
    }

    /* Motor efference-somatosensory binding (sensorimotor) */
    if (state->motor_efference_id != BRAIN_KG_INVALID_NODE &&
        state->somatosensory_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->motor_efference_id, state->somatosensory_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_CROSS_BINDS_WITH,
            "sensorimotor prediction binding", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Interoceptive-olfactory binding (embodiment) */
    if (state->interoceptive_id != BRAIN_KG_INVALID_NODE &&
        state->olfactory_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->interoceptive_id, state->olfactory_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_CROSS_BINDS_WITH,
            "interoceptive-olfactory binding", 0.6f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int claustrum_kg_register_cross_edges(
    brain_kg_t* kg,
    claustrum_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_kg_register_cross_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Gamma oscillation synchronizes modalities */
    if (state->gamma_id != BRAIN_KG_INVALID_NODE &&
        state->modalities_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->gamma_id, state->modalities_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_SYNCHRONIZES,
            "gamma synchronizes modalities for binding", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Alpha oscillation gates workspace access */
    if (state->alpha_id != BRAIN_KG_INVALID_NODE &&
        state->gating_state_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->alpha_id, state->gating_state_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_GATES,
            "alpha oscillation modulates workspace gating", 0.8f, admin_token);
        state->edge_count++;
    }

    /* Binding state enables conscious access */
    if (state->binding_state_id != BRAIN_KG_INVALID_NODE &&
        state->conscious_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->binding_state_id, state->conscious_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_ENABLES,
            "successful binding enables conscious access", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Broadcasting state enables focal attention */
    if (state->broadcasting_state_id != BRAIN_KG_INVALID_NODE &&
        state->focal_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->broadcasting_state_id, state->focal_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_BROADCASTS_TO,
            "workspace broadcast reaches focal attention", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Synchronizing state connects to gamma */
    if (state->synchronizing_state_id != BRAIN_KG_INVALID_NODE &&
        state->gamma_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->synchronizing_state_id, state->gamma_id,
            BRAIN_KG_EDGE_COORDINATES_WITH,
            "synchronization driven by gamma oscillation", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Switching state attends to salience */
    if (state->switching_state_id != BRAIN_KG_INVALID_NODE &&
        state->states_id != BRAIN_KG_INVALID_NODE) {
        create_claustrum_edge(kg, state->switching_state_id, state->states_id,
            (brain_kg_edge_type_t)CLAUSTRUM_KG_EDGE_SWITCHES_TO,
            "switching reconfigures state network", 0.7f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// State Synchronization API
//=============================================================================

int claustrum_kg_update_state(
    brain_kg_t* kg,
    const claustrum_kg_state_t* state,
    float binding_strength,
    float gamma_coherence,
    uint32_t consciousness_level,
    uint64_t admin_token
) {
    (void)admin_token;  /* Reserved for future access control */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_kg_update_state: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Update binding state metadata */
    if (state->binding_state_id != BRAIN_KG_INVALID_NODE) {
        char strength_str[32];
        snprintf(strength_str, sizeof(strength_str), "%.2f", binding_strength);
        brain_kg_add_metadata(kg, state->binding_state_id, "strength", strength_str);
    }

    /* Update gamma oscillation metadata */
    if (state->gamma_id != BRAIN_KG_INVALID_NODE) {
        char coherence_str[32];
        snprintf(coherence_str, sizeof(coherence_str), "%.2f", gamma_coherence);
        brain_kg_add_metadata(kg, state->gamma_id, "coherence", coherence_str);
    }

    /* Update consciousness level active flag */
    brain_kg_node_id_t level_ids[] = {
        state->unconscious_id,
        state->preconscious_id,
        state->conscious_id,
        state->focal_id
    };

    for (uint32_t i = 0; i < 4; i++) {
        if (level_ids[i] != BRAIN_KG_INVALID_NODE) {
            const char* active = (i == consciousness_level) ? "true" : "false";
            brain_kg_add_metadata(kg, level_ids[i], "active", active);
        }
    }

    return 0;
}

int claustrum_kg_update_modality(
    brain_kg_t* kg,
    const claustrum_kg_state_t* state,
    uint32_t modality,
    float activity,
    bool bound,
    uint64_t admin_token
) {
    (void)admin_token;  /* Reserved for future access control */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_kg_update_modality: required parameter is NULL (kg, state)");
        return -1;
    }
    if (modality > 7) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_kg_update_modality: validation failed");
        return -1;
    }

    /* Map modality index to node ID */
    brain_kg_node_id_t modality_ids[] = {
        state->visual_id,
        state->auditory_id,
        state->somatosensory_id,
        state->olfactory_id,
        state->gustatory_id,
        state->interoceptive_id,
        state->vestibular_id,
        state->motor_efference_id
    };

    brain_kg_node_id_t node_id = modality_ids[modality];
    if (node_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_kg_update_modality: validation failed");
        return -1;
    }

    /* Update activity metadata */
    char activity_str[32];
    snprintf(activity_str, sizeof(activity_str), "%.2f", activity);
    brain_kg_add_metadata(kg, node_id, "activity", activity_str);

    /* Update bound status */
    brain_kg_add_metadata(kg, node_id, "bound", bound ? "true" : "false");

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t claustrum_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, CLAUSTRUM_KG_ROOT_NAME);
}

brain_kg_node_id_t claustrum_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;

    /* Prepend claustrum_ if not already prefixed */
    char full_name[NIMCP_LABEL_BUFFER_SIZE];
    if (strncmp(name, "claustrum_", 10) == 0) {
        snprintf(full_name, sizeof(full_name), "%s", name);
    } else {
        snprintf(full_name, sizeof(full_name), "claustrum_%s", name);
    }

    return brain_kg_find_node(kg, full_name);
}

brain_kg_node_list_t* claustrum_kg_get_modalities(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)CLAUSTRUM_KG_NODE_MODALITY
    );
}

brain_kg_node_list_t* claustrum_kg_get_active_modalities(
    brain_kg_t* kg,
    float min_activity
) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }

    /* Get all modalities then filter by activity */
    brain_kg_node_list_t* all = claustrum_kg_get_modalities(kg);
    if (!all) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "all is NULL");

        return NULL;

    }

    /* For now, return all - full implementation would filter by metadata */
    (void)min_activity;  /* Would be used to filter by activity metadata */
    return all;
}

brain_kg_node_list_t* claustrum_kg_get_bound_modalities(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }

    /* Get all modalities then filter by bound status */
    brain_kg_node_list_t* all = claustrum_kg_get_modalities(kg);
    if (!all) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "all is NULL");

        return NULL;

    }

    /* For now, return all - full implementation would filter by bound metadata */
    return all;
}

brain_kg_node_id_t claustrum_kg_get_current_consciousness(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;

    /* Search consciousness level nodes for active=true */
    const char* level_names[] = {
        "claustrum_level_unconscious",
        "claustrum_level_preconscious",
        "claustrum_level_conscious",
        "claustrum_level_focal"
    };

    for (int i = 3; i >= 0; i--) {  /* Check from highest to lowest */
        brain_kg_node_id_t id = brain_kg_find_node(kg, level_names[i]);
        if (id != BRAIN_KG_INVALID_NODE) {
            /* Full implementation would check active metadata */
            /* For now, return focal if it exists (most commonly queried) */
            if (i == 3) return id;
        }
    }

    return BRAIN_KG_INVALID_NODE;
}

int claustrum_kg_unregister_all(
    brain_kg_t* kg,
    claustrum_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }
    (void)admin_token;  /* Would be used for actual deletion */

    /* Mark as unregistered - full implementation would remove nodes */
    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(CLAUSTRUM_KG_MODULE_NAME, "Unregistered claustrum KG nodes");

    return 0;
}
