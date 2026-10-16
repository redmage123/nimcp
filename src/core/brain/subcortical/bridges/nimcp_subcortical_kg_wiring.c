//=============================================================================
// nimcp_subcortical_kg_wiring.c - Subcortical Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_subcortical_kg_wiring.c
 * @brief Implementation of Subcortical Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for Subcortical modules
 * WHY:  Enables semantic queries about action selection, motor control, and reward
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/subcortical/bridges/nimcp_subcortical_kg_wiring.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(subcortical_kg_wiring, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create a subcortical node with description
 *
 * WHAT: Helper to create a single KG node
 * WHY:  Reduces boilerplate in registration functions
 * HOW:  Wraps brain_kg_add_node with logging
 */
static brain_kg_node_id_t create_subcortical_node(
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
        NIMCP_LOG_DEBUG(SUBCORTICAL_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

/**
 * @brief Create an edge between subcortical nodes
 *
 * WHAT: Helper to create a single KG edge
 * WHY:  Reduces boilerplate in registration functions
 * HOW:  Wraps brain_kg_add_edge with validation
 */
static brain_kg_edge_id_t create_subcortical_edge(
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

int subcortical_kg_default_config(subcortical_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subcortical_kg_default_config: config is NULL");
        return -1;
    }

    config->register_basal_ganglia = true;
    config->register_thalamus = true;
    config->register_nucleus_accumbens = true;
    config->register_pathways = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;
    config->register_dopamine_edges = true;
    config->register_sensory_edges = true;
    config->register_striatal_subdivisions = true;
    config->register_vta = true;
    config->register_extended_thalamus = true;

    return 0;
}

//=============================================================================
// Registration API - Basal Ganglia Components
//=============================================================================

/**
 * @brief Register striatum and its pathways
 */
static int register_striatum(
    brain_kg_t* kg,
    brain_kg_node_id_t bg_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    /* Striatum main node */
    state->striatum_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_STRIATUM_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_BG_COMPONENT,
        "Striatum - input nucleus of basal ganglia, D1/D2 MSN pathways for action selection",
        admin_token
    );
    if (state->striatum_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, bg_id, state->striatum_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains striatum", 1.0f, admin_token);
        state->edge_count++;
    }

    /* D1 Direct pathway (GO) */
    state->d1_pathway_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_D1_PATHWAY_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_BG_COMPONENT,
        "D1 MSN pathway - direct pathway, facilitates action via disinhibition",
        admin_token
    );
    if (state->d1_pathway_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, state->striatum_id, state->d1_pathway_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains D1 pathway", 1.0f, admin_token);
        state->edge_count++;
    }

    /* D2 Indirect pathway (NO-GO) */
    state->d2_pathway_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_D2_PATHWAY_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_BG_COMPONENT,
        "D2 MSN pathway - indirect pathway, suppresses action via GPe",
        admin_token
    );
    if (state->d2_pathway_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, state->striatum_id, state->d2_pathway_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains D2 pathway", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

/**
 * @brief Register globus pallidus (GPe and GPi)
 */
static int register_globus_pallidus(
    brain_kg_t* kg,
    brain_kg_node_id_t bg_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    /* Globus Pallidus Externa (GPe) */
    state->gpe_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_GPE_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_BG_COMPONENT,
        "GPe - external segment, relay in indirect pathway, tonically active inhibition",
        admin_token
    );
    if (state->gpe_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, bg_id, state->gpe_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains GPe", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Globus Pallidus Interna (GPi) */
    state->gpi_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_GPI_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_BG_COMPONENT,
        "GPi - internal segment, output nucleus, tonic inhibition of thalamus",
        admin_token
    );
    if (state->gpi_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, bg_id, state->gpi_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains GPi", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

/**
 * @brief Register subthalamic nucleus
 */
static int register_subthalamic(
    brain_kg_t* kg,
    brain_kg_node_id_t bg_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    state->stn_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_STN_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_BG_COMPONENT,
        "STN - subthalamic nucleus, hyperdirect pathway, global action suppression",
        admin_token
    );
    if (state->stn_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, bg_id, state->stn_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains STN", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

/**
 * @brief Register substantia nigra (SNc and SNr)
 */
static int register_substantia_nigra(
    brain_kg_t* kg,
    brain_kg_node_id_t bg_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    /* Substantia Nigra pars Compacta (dopamine) */
    state->snc_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_SNC_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_DOPAMINERGIC,
        "SNc - dopamine neurons, reward prediction error signal to striatum",
        admin_token
    );
    if (state->snc_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, bg_id, state->snc_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains SNc", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Substantia Nigra pars Reticulata (output) */
    state->snr_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_SNR_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_BG_COMPONENT,
        "SNr - output nucleus, parallel to GPi, inhibits superior colliculus/thalamus",
        admin_token
    );
    if (state->snr_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, bg_id, state->snr_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains SNr", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

/**
 * @brief Register internal basal ganglia connections
 */
static int register_bg_internal_edges(
    brain_kg_t* kg,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    /* Direct pathway: D1 -> GPi (inhibitory) */
    if (state->d1_pathway_id != BRAIN_KG_INVALID_NODE &&
        state->gpi_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->d1_pathway_id, state->gpi_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_INHIBITS_VIA_GABA,
            "D1 MSNs inhibit GPi (direct pathway)", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Indirect pathway: D2 -> GPe (inhibitory) */
    if (state->d2_pathway_id != BRAIN_KG_INVALID_NODE &&
        state->gpe_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->d2_pathway_id, state->gpe_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_INHIBITS_VIA_GABA,
            "D2 MSNs inhibit GPe (indirect pathway)", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Indirect pathway: GPe -> STN (inhibitory) */
    if (state->gpe_id != BRAIN_KG_INVALID_NODE &&
        state->stn_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->gpe_id, state->stn_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_INHIBITS_VIA_GABA,
            "GPe inhibits STN", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Indirect pathway: STN -> GPi (excitatory) */
    if (state->stn_id != BRAIN_KG_INVALID_NODE &&
        state->gpi_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->stn_id, state->gpi_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_EXCITES_VIA_GLUT,
            "STN excites GPi (indirect/hyperdirect)", 0.9f, admin_token);
        state->edge_count++;
    }

    /* STN -> SNr (excitatory) */
    if (state->stn_id != BRAIN_KG_INVALID_NODE &&
        state->snr_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->stn_id, state->snr_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_EXCITES_VIA_GLUT,
            "STN excites SNr", 0.85f, admin_token);
        state->edge_count++;
    }

    /* SNc -> Striatum (dopaminergic modulation) */
    if (state->snc_id != BRAIN_KG_INVALID_NODE &&
        state->striatum_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->snc_id, state->striatum_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_MODULATES_VIA_DA,
            "SNc provides dopamine to striatum (RPE signal)", 0.95f, admin_token);
        state->edge_count++;
    }

    /* GPe -> GPi (inhibitory, arkypallidal projection) */
    if (state->gpe_id != BRAIN_KG_INVALID_NODE &&
        state->gpi_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->gpe_id, state->gpi_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_INHIBITS_VIA_GABA,
            "GPe inhibits GPi (arkypallidal)", 0.7f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int subcortical_kg_register_basal_ganglia(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const subcortical_kg_config_t* config,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    (void)config;  /* All BG components registered by default */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subcortical_kg_register_basal_ganglia: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create basal ganglia root node */
    state->bg_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_BG_NAME,
        BRAIN_KG_NODE_SUBCORTICAL,
        "Basal Ganglia - action selection, motor control, habit formation via direct/indirect pathways",
        admin_token
    );
    if (state->bg_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(SUBCORTICAL_KG_MODULE_NAME, "Failed to create BG root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "subcortical_kg_register_basal_ganglia: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, parent_id, state->bg_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains basal ganglia", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Register components */
    register_striatum(kg, state->bg_id, state, admin_token);
    register_globus_pallidus(kg, state->bg_id, state, admin_token);
    register_subthalamic(kg, state->bg_id, state, admin_token);
    register_substantia_nigra(kg, state->bg_id, state, admin_token);

    /* Register internal BG edges */
    register_bg_internal_edges(kg, state, admin_token);

    return 0;
}

//=============================================================================
// Registration API - Thalamus
//=============================================================================

/**
 * @brief Register sensory relay nuclei (LGN, MGN, VPL, VPM)
 */
static int register_sensory_nuclei(
    brain_kg_t* kg,
    brain_kg_node_id_t thal_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    /* Lateral Geniculate Nucleus (visual) */
    state->lgn_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_LGN_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_SENSORY_RELAY,
        "LGN - visual relay from retina to V1, magno/parvo/konio pathways",
        admin_token
    );
    if (state->lgn_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, thal_id, state->lgn_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains LGN", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Medial Geniculate Nucleus (auditory) */
    state->mgn_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_MGN_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_SENSORY_RELAY,
        "MGN - auditory relay from inferior colliculus to A1",
        admin_token
    );
    if (state->mgn_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, thal_id, state->mgn_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains MGN", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Ventral Posterolateral (body somatosensory) */
    state->vpl_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_VPL_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_SENSORY_RELAY,
        "VPL - body somatosensory relay to S1 (touch, proprioception)",
        admin_token
    );
    if (state->vpl_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, thal_id, state->vpl_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains VPL", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Ventral Posteromedial (face somatosensory) */
    state->vpm_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_VPM_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_SENSORY_RELAY,
        "VPM - face somatosensory relay to S1 (trigeminal)",
        admin_token
    );
    if (state->vpm_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, thal_id, state->vpm_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains VPM", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

/**
 * @brief Register motor nuclei (VA, VL)
 */
static int register_motor_nuclei(
    brain_kg_t* kg,
    brain_kg_node_id_t thal_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    /* Ventral Anterior (from BG) */
    state->va_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_VA_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_MOTOR_CONTROL,
        "VA - motor relay from BG to premotor/SMA, action selection output",
        admin_token
    );
    if (state->va_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, thal_id, state->va_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains VA", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Ventral Lateral (from cerebellum) */
    state->vl_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_VL_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_MOTOR_CONTROL,
        "VL - motor relay from cerebellum to M1, motor coordination",
        admin_token
    );
    if (state->vl_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, thal_id, state->vl_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains VL", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

/**
 * @brief Register associative nuclei (Pulvinar, MD)
 */
static int register_associative_nuclei(
    brain_kg_t* kg,
    brain_kg_node_id_t thal_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    /* Pulvinar (attention, visual association) */
    state->pulvinar_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_PULVINAR_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_THALAMIC_NUCLEUS,
        "Pulvinar - attention modulation, visual association, cortico-cortical relay",
        admin_token
    );
    if (state->pulvinar_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, thal_id, state->pulvinar_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains Pulvinar", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Mediodorsal (prefrontal, executive) */
    state->md_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_MD_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_THALAMIC_NUCLEUS,
        "MD - prefrontal relay, working memory, executive function support",
        admin_token
    );
    if (state->md_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, thal_id, state->md_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains MD", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

/**
 * @brief Register thalamic reticular nucleus (gating)
 */
static int register_trn(
    brain_kg_t* kg,
    brain_kg_node_id_t thal_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    state->trn_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_TRN_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_GATING,
        "TRN - thalamic reticular nucleus, attention-modulated gating of all thalamic nuclei",
        admin_token
    );
    if (state->trn_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, thal_id, state->trn_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains TRN", 1.0f, admin_token);
        state->edge_count++;

        /* TRN gates all relay nuclei */
        if (state->lgn_id != BRAIN_KG_INVALID_NODE) {
            create_subcortical_edge(kg, state->trn_id, state->lgn_id,
                (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_GATES,
                "TRN gates LGN", 0.8f, admin_token);
            state->edge_count++;
        }
        if (state->mgn_id != BRAIN_KG_INVALID_NODE) {
            create_subcortical_edge(kg, state->trn_id, state->mgn_id,
                (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_GATES,
                "TRN gates MGN", 0.8f, admin_token);
            state->edge_count++;
        }
        if (state->va_id != BRAIN_KG_INVALID_NODE) {
            create_subcortical_edge(kg, state->trn_id, state->va_id,
                (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_GATES,
                "TRN gates VA", 0.8f, admin_token);
            state->edge_count++;
        }
        if (state->pulvinar_id != BRAIN_KG_INVALID_NODE) {
            create_subcortical_edge(kg, state->trn_id, state->pulvinar_id,
                (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_GATES,
                "TRN gates Pulvinar", 0.85f, admin_token);
            state->edge_count++;
        }
    }

    return 0;
}

int subcortical_kg_register_thalamus(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const subcortical_kg_config_t* config,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    (void)config;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subcortical_kg_register_thalamus: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create thalamus root node */
    state->thalamus_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_THALAMUS_NAME,
        BRAIN_KG_NODE_SUBCORTICAL,
        "Thalamus - sensory gateway to cortex, attention-modulated relay and gating",
        admin_token
    );
    if (state->thalamus_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(SUBCORTICAL_KG_MODULE_NAME, "Failed to create thalamus root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "subcortical_kg_register_thalamus: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, parent_id, state->thalamus_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains thalamus", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Register nuclei */
    register_sensory_nuclei(kg, state->thalamus_id, state, admin_token);
    register_motor_nuclei(kg, state->thalamus_id, state, admin_token);
    register_associative_nuclei(kg, state->thalamus_id, state, admin_token);
    register_trn(kg, state->thalamus_id, state, admin_token);

    return 0;
}

//=============================================================================
// Registration API - Nucleus Accumbens
//=============================================================================

int subcortical_kg_register_nucleus_accumbens(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subcortical_kg_register_nucleus_accumbens: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create NAc root node */
    state->nac_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_NAC_NAME,
        BRAIN_KG_NODE_SUBCORTICAL,
        "Nucleus Accumbens - reward processing, motivation, wanting vs liking",
        admin_token
    );
    if (state->nac_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(SUBCORTICAL_KG_MODULE_NAME, "Failed to create NAc root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "subcortical_kg_register_nucleus_accumbens: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, parent_id, state->nac_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains nucleus accumbens", 1.0f, admin_token);
        state->edge_count++;
    }

    /* NAc Core (goal-directed) */
    state->nac_core_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_NAC_CORE_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_NAC_COMPONENT,
        "NAc Core - goal-directed behavior, instrumental learning, incentive salience",
        admin_token
    );
    if (state->nac_core_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, state->nac_id, state->nac_core_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains NAc core", 1.0f, admin_token);
        state->edge_count++;
    }

    /* NAc Shell (hedonic/motivation) */
    state->nac_shell_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_NAC_SHELL_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_NAC_COMPONENT,
        "NAc Shell - hedonic hotspots, motivation state, liking responses",
        admin_token
    );
    if (state->nac_shell_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, state->nac_id, state->nac_shell_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains NAc shell", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Core-Shell interaction */
    if (state->nac_core_id != BRAIN_KG_INVALID_NODE &&
        state->nac_shell_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->nac_shell_id, state->nac_core_id,
            BRAIN_KG_EDGE_MODULATES,
            "Shell motivational state modulates core goal-directed behavior", 0.8f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Pathways
//=============================================================================

int subcortical_kg_register_pathways(
    brain_kg_t* kg,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subcortical_kg_register_pathways: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Direct Pathway node */
    state->direct_pathway_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_DIRECT_PATHWAY_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_PATHWAY,
        "Direct Pathway - Striatum D1 -> GPi -> Thalamus, facilitates action (GO)",
        admin_token
    );
    if (state->direct_pathway_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;

        /* Link pathway components */
        if (state->d1_pathway_id != BRAIN_KG_INVALID_NODE) {
            create_subcortical_edge(kg, state->direct_pathway_id, state->d1_pathway_id,
                (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_PATHWAY_COMPONENT,
                "D1 MSNs are part of direct pathway", 1.0f, admin_token);
            state->edge_count++;
        }
        if (state->gpi_id != BRAIN_KG_INVALID_NODE) {
            create_subcortical_edge(kg, state->direct_pathway_id, state->gpi_id,
                (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_PATHWAY_COMPONENT,
                "GPi is direct pathway target", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Indirect Pathway node */
    state->indirect_pathway_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_INDIRECT_PATHWAY_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_PATHWAY,
        "Indirect Pathway - Striatum D2 -> GPe -> STN -> GPi, suppresses action (NO-GO)",
        admin_token
    );
    if (state->indirect_pathway_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;

        /* Link pathway components */
        if (state->d2_pathway_id != BRAIN_KG_INVALID_NODE) {
            create_subcortical_edge(kg, state->indirect_pathway_id, state->d2_pathway_id,
                (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_PATHWAY_COMPONENT,
                "D2 MSNs are part of indirect pathway", 1.0f, admin_token);
            state->edge_count++;
        }
        if (state->gpe_id != BRAIN_KG_INVALID_NODE) {
            create_subcortical_edge(kg, state->indirect_pathway_id, state->gpe_id,
                (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_PATHWAY_COMPONENT,
                "GPe is indirect pathway relay", 1.0f, admin_token);
            state->edge_count++;
        }
        if (state->stn_id != BRAIN_KG_INVALID_NODE) {
            create_subcortical_edge(kg, state->indirect_pathway_id, state->stn_id,
                (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_PATHWAY_COMPONENT,
                "STN is indirect pathway relay", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    /* Hyperdirect Pathway node */
    state->hyperdirect_pathway_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_HYPERDIRECT_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_PATHWAY,
        "Hyperdirect Pathway - Cortex -> STN -> GPi, fast global action suppression",
        admin_token
    );
    if (state->hyperdirect_pathway_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;

        if (state->stn_id != BRAIN_KG_INVALID_NODE) {
            create_subcortical_edge(kg, state->hyperdirect_pathway_id, state->stn_id,
                (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_PATHWAY_COMPONENT,
                "STN receives hyperdirect cortical input", 1.0f, admin_token);
            state->edge_count++;
        }
    }

    return 0;
}

//=============================================================================
// Registration API - Cross Edges
//=============================================================================

/**
 * @brief Register BG to thalamus output edges
 */
static void register_bg_thalamus_edges(
    brain_kg_t* kg,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    /* GPi -> VA (inhibitory output, disinhibits when D1 active) */
    if (state->gpi_id != BRAIN_KG_INVALID_NODE &&
        state->va_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->gpi_id, state->va_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_INHIBITS_VIA_GABA,
            "GPi inhibits VA thalamus (motor output)", 0.9f, admin_token);
        state->edge_count++;
    }

    /* SNr -> VA (parallel output to GPi) */
    if (state->snr_id != BRAIN_KG_INVALID_NODE &&
        state->va_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->snr_id, state->va_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_INHIBITS_VIA_GABA,
            "SNr inhibits VA thalamus", 0.85f, admin_token);
        state->edge_count++;
    }

    /* GPi -> MD (cognitive/executive loop) */
    if (state->gpi_id != BRAIN_KG_INVALID_NODE &&
        state->md_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->gpi_id, state->md_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_INHIBITS_VIA_GABA,
            "GPi inhibits MD (cognitive loop)", 0.8f, admin_token);
        state->edge_count++;
    }
}

/**
 * @brief Register dopamine modulation edges
 */
static void register_dopamine_edges(
    brain_kg_t* kg,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    /* SNc -> NAc (mesolimbic pathway) */
    if (state->snc_id != BRAIN_KG_INVALID_NODE &&
        state->nac_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->snc_id, state->nac_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_MODULATES_VIA_DA,
            "SNc provides dopamine to NAc (reward signal)", 0.9f, admin_token);
        state->edge_count++;
    }

    /* SNc -> D1 pathway (facilitates GO) */
    if (state->snc_id != BRAIN_KG_INVALID_NODE &&
        state->d1_pathway_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->snc_id, state->d1_pathway_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_MODULATES_VIA_DA,
            "Dopamine excites D1 MSNs (facilitates GO)", 0.9f, admin_token);
        state->edge_count++;
    }

    /* SNc -> D2 pathway (inhibits NO-GO) */
    if (state->snc_id != BRAIN_KG_INVALID_NODE &&
        state->d2_pathway_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->snc_id, state->d2_pathway_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_MODULATES_VIA_DA,
            "Dopamine inhibits D2 MSNs (suppresses NO-GO)", 0.9f, admin_token);
        state->edge_count++;
    }
}

/**
 * @brief Register NAc integration edges
 */
static void register_nac_integration_edges(
    brain_kg_t* kg,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    /* NAc Core -> BG (ventral striatum is part of BG) */
    if (state->nac_core_id != BRAIN_KG_INVALID_NODE &&
        state->bg_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->nac_core_id, state->bg_id,
            BRAIN_KG_EDGE_INTEGRATES_WITH,
            "NAc core integrates with BG for goal-directed actions", 0.85f, admin_token);
        state->edge_count++;
    }

    /* NAc -> GPi (ventral pallidum) */
    if (state->nac_id != BRAIN_KG_INVALID_NODE &&
        state->gpi_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->nac_id, state->gpi_id,
            BRAIN_KG_EDGE_SENDS_TO,
            "NAc projects to ventral pallidum (GPi homolog)", 0.75f, admin_token);
        state->edge_count++;
    }
}

int subcortical_kg_register_cross_edges(
    brain_kg_t* kg,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subcortical_kg_register_cross_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Register BG -> Thalamus output edges */
    register_bg_thalamus_edges(kg, state, admin_token);

    /* Register dopamine modulation edges */
    register_dopamine_edges(kg, state, admin_token);

    /* Register NAc integration edges */
    register_nac_integration_edges(kg, state, admin_token);

    /* Thalamus -> Striatum feedback */
    if (state->va_id != BRAIN_KG_INVALID_NODE &&
        state->striatum_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->va_id, state->striatum_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_EXCITES_VIA_GLUT,
            "Thalamic feedback to striatum", 0.7f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Striatal Subdivisions
//=============================================================================

int subcortical_kg_register_striatal_subdivisions(
    brain_kg_t* kg,
    brain_kg_node_id_t striatum_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subcortical_kg_register_striatal_subdivisions: required parameter is NULL (kg, state)");
        return -1;
    }
    if (striatum_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "subcortical_kg_register_striatal_subdivisions: validation failed");
        return -1;
    }

    /* Dorsal Striatum (associative/sensorimotor) */
    state->dorsal_striatum_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_DORSAL_STRIATUM_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_STRIATAL_REGION,
        "Dorsal Striatum - associative and sensorimotor processing, habit formation",
        admin_token
    );
    if (state->dorsal_striatum_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, striatum_id, state->dorsal_striatum_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains dorsal striatum", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Ventral Striatum (limbic/reward) */
    state->ventral_striatum_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_VENTRAL_STRIATUM_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_STRIATAL_REGION,
        "Ventral Striatum - limbic processing, reward, motivation (includes NAc)",
        admin_token
    );
    if (state->ventral_striatum_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, striatum_id, state->ventral_striatum_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains ventral striatum", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Caudate Nucleus (cognitive control, eye movements) */
    state->caudate_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_CAUDATE_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_STRIATAL_REGION,
        "Caudate Nucleus - cognitive control, goal-directed behavior, eye movements",
        admin_token
    );
    if (state->caudate_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, state->dorsal_striatum_id, state->caudate_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains caudate", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Putamen (motor learning, habit formation) */
    state->putamen_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_PUTAMEN_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_STRIATAL_REGION,
        "Putamen - motor learning, habit formation, sensorimotor integration",
        admin_token
    );
    if (state->putamen_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, state->dorsal_striatum_id, state->putamen_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains putamen", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Link ventral striatum to NAc */
    if (state->ventral_striatum_id != BRAIN_KG_INVALID_NODE &&
        state->nac_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->ventral_striatum_id, state->nac_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "ventral striatum includes NAc", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - VTA Dopamine System
//=============================================================================

int subcortical_kg_register_vta(
    brain_kg_t* kg,
    brain_kg_node_id_t bg_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subcortical_kg_register_vta: required parameter is NULL (kg, state)");
        return -1;
    }

    state->vta_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_VTA_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_DOPAMINERGIC,
        "VTA - mesolimbic dopamine, reward prediction, motivation signaling",
        admin_token
    );
    if (state->vta_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(SUBCORTICAL_KG_MODULE_NAME, "Failed to create VTA node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "subcortical_kg_register_vta: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to BG parent */
    if (bg_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, bg_id, state->vta_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains VTA", 1.0f, admin_token);
        state->edge_count++;
    }

    /* VTA -> NAc (mesolimbic pathway) */
    if (state->nac_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->vta_id, state->nac_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_MESOLIMBIC,
            "VTA provides dopamine to NAc (mesolimbic)", 0.95f, admin_token);
        state->edge_count++;
    }

    /* VTA -> NAc Shell */
    if (state->nac_shell_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->vta_id, state->nac_shell_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_MODULATES_VIA_DA,
            "VTA modulates NAc shell hedonic response", 0.9f, admin_token);
        state->edge_count++;
    }

    /* VTA -> Ventral Striatum */
    if (state->ventral_striatum_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->vta_id, state->ventral_striatum_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_MODULATES_VIA_DA,
            "VTA provides dopamine to ventral striatum", 0.9f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Extended Thalamic Nuclei
//=============================================================================

int subcortical_kg_register_extended_thalamus(
    brain_kg_t* kg,
    brain_kg_node_id_t thalamus_id,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subcortical_kg_register_extended_thalamus: required parameter is NULL (kg, state)");
        return -1;
    }
    if (thalamus_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "subcortical_kg_register_extended_thalamus: validation failed");
        return -1;
    }

    /* Anterior Nucleus (limbic, Papez circuit) */
    state->an_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_AN_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_LIMBIC,
        "Anterior Nucleus - limbic thalamus, Papez circuit, memory/emotion integration",
        admin_token
    );
    if (state->an_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, thalamus_id, state->an_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains AN", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Intralaminar Nuclei (arousal, attention) */
    state->il_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_IL_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_AROUSAL,
        "Intralaminar Nuclei - arousal, attention, consciousness, cortical activation",
        admin_token
    );
    if (state->il_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, thalamus_id, state->il_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains IL", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Centromedian-Parafascicular (CM-PF, BG integration) */
    state->cm_pf_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_CM_PF_NAME,
        (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_THALAMIC_NUCLEUS,
        "CM-PF - basal ganglia integration, attention, motor preparation",
        admin_token
    );
    if (state->cm_pf_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_subcortical_edge(kg, thalamus_id, state->cm_pf_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains CM-PF", 1.0f, admin_token);
        state->edge_count++;

        /* CM-PF -> Striatum (thalamostriatal pathway) */
        if (state->striatum_id != BRAIN_KG_INVALID_NODE) {
            create_subcortical_edge(kg, state->cm_pf_id, state->striatum_id,
                (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_EXCITES_VIA_GLUT,
                "CM-PF provides glutamatergic input to striatum", 0.8f, admin_token);
            state->edge_count++;
        }

        /* CM-PF receives from GPi (feedback loop) */
        if (state->gpi_id != BRAIN_KG_INVALID_NODE) {
            create_subcortical_edge(kg, state->gpi_id, state->cm_pf_id,
                (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_INHIBITS_VIA_GABA,
                "GPi inhibits CM-PF (BG output)", 0.75f, admin_token);
            state->edge_count++;
        }
    }

    /* TRN gates IL */
    if (state->trn_id != BRAIN_KG_INVALID_NODE &&
        state->il_id != BRAIN_KG_INVALID_NODE) {
        create_subcortical_edge(kg, state->trn_id, state->il_id,
            (brain_kg_edge_type_t)SUBCORTICAL_KG_EDGE_GATES,
            "TRN gates IL nuclei", 0.8f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// Registration API - Main Entry Point
//=============================================================================

int subcortical_kg_register_all(
    brain_kg_t* kg,
    const subcortical_kg_config_t* config,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subcortical_kg_register_all: kg is NULL");
        return -1;
    }

    /* Use provided config or defaults */
    subcortical_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        subcortical_kg_default_config(&local_config);
    }

    /* Initialize local state */
    subcortical_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Initialize all node IDs to invalid */
    local_state.root_id = BRAIN_KG_INVALID_NODE;
    local_state.bg_id = BRAIN_KG_INVALID_NODE;
    local_state.striatum_id = BRAIN_KG_INVALID_NODE;
    local_state.thalamus_id = BRAIN_KG_INVALID_NODE;
    local_state.nac_id = BRAIN_KG_INVALID_NODE;

    /* Create subcortical root node */
    local_state.root_id = create_subcortical_node(
        kg, SUBCORTICAL_KG_ROOT_NAME,
        BRAIN_KG_NODE_SUBCORTICAL,
        "Subcortical structures - basal ganglia, thalamus, nucleus accumbens",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(SUBCORTICAL_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "subcortical_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    subcortical_kg_wiring_heartbeat("registering_subcortical", 0.1f);

    /* Register basal ganglia */
    if (local_config.register_basal_ganglia) {
        if (subcortical_kg_register_basal_ganglia(kg, local_state.root_id, &local_config,
                                                   &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(SUBCORTICAL_KG_MODULE_NAME, "Failed to register basal ganglia");
        }
    }
    subcortical_kg_wiring_heartbeat("registering_subcortical", 0.4f);

    /* Register thalamus */
    if (local_config.register_thalamus) {
        if (subcortical_kg_register_thalamus(kg, local_state.root_id, &local_config,
                                              &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(SUBCORTICAL_KG_MODULE_NAME, "Failed to register thalamus");
        }
    }
    subcortical_kg_wiring_heartbeat("registering_subcortical", 0.6f);

    /* Register nucleus accumbens */
    if (local_config.register_nucleus_accumbens) {
        if (subcortical_kg_register_nucleus_accumbens(kg, local_state.root_id,
                                                       &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(SUBCORTICAL_KG_MODULE_NAME, "Failed to register nucleus accumbens");
        }
    }
    subcortical_kg_wiring_heartbeat("registering_subcortical", 0.65f);

    /* Register striatal subdivisions */
    if (local_config.register_striatal_subdivisions) {
        if (subcortical_kg_register_striatal_subdivisions(kg, local_state.striatum_id,
                                                           &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(SUBCORTICAL_KG_MODULE_NAME, "Failed to register striatal subdivisions");
        }
    }
    subcortical_kg_wiring_heartbeat("registering_subcortical", 0.7f);

    /* Register VTA */
    if (local_config.register_vta) {
        if (subcortical_kg_register_vta(kg, local_state.bg_id,
                                         &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(SUBCORTICAL_KG_MODULE_NAME, "Failed to register VTA");
        }
    }
    subcortical_kg_wiring_heartbeat("registering_subcortical", 0.75f);

    /* Register extended thalamus */
    if (local_config.register_extended_thalamus) {
        if (subcortical_kg_register_extended_thalamus(kg, local_state.thalamus_id,
                                                       &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(SUBCORTICAL_KG_MODULE_NAME, "Failed to register extended thalamus");
        }
    }
    subcortical_kg_wiring_heartbeat("registering_subcortical", 0.8f);

    /* Register pathways */
    if (local_config.register_pathways) {
        if (subcortical_kg_register_pathways(kg, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(SUBCORTICAL_KG_MODULE_NAME, "Failed to register pathways");
        }
    }
    subcortical_kg_wiring_heartbeat("registering_subcortical", 0.9f);

    /* Register cross-structure edges */
    if (local_config.register_cross_edges) {
        subcortical_kg_register_cross_edges(kg, &local_state, admin_token);
    }

    local_state.registered = true;
    subcortical_kg_wiring_heartbeat("registering_subcortical", 1.0f);

    /* Copy to output if requested */
    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(SUBCORTICAL_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// State Synchronization API
//=============================================================================

int subcortical_kg_update_state(
    brain_kg_t* kg,
    const subcortical_kg_state_t* state,
    float dopamine_level,
    float action_selection_entropy,
    float thalamic_gating,
    float motivation_level,
    uint64_t admin_token
) {
    (void)admin_token;  /* Reserved for future access control */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subcortical_kg_update_state: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Update SNc dopamine level */
    if (state->snc_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.3f", dopamine_level);
        brain_kg_add_metadata(kg, state->snc_id, "dopamine_level", val_str);
    }

    /* Update striatum action selection entropy */
    if (state->striatum_id != BRAIN_KG_INVALID_NODE) {
        char ent_str[32];
        snprintf(ent_str, sizeof(ent_str), "%.3f", action_selection_entropy);
        brain_kg_add_metadata(kg, state->striatum_id, "selection_entropy", ent_str);
    }

    /* Update thalamus gating level */
    if (state->thalamus_id != BRAIN_KG_INVALID_NODE) {
        char gate_str[32];
        snprintf(gate_str, sizeof(gate_str), "%.3f", thalamic_gating);
        brain_kg_add_metadata(kg, state->thalamus_id, "gating_level", gate_str);
    }

    /* Update NAc motivation level */
    if (state->nac_id != BRAIN_KG_INVALID_NODE) {
        char mot_str[32];
        snprintf(mot_str, sizeof(mot_str), "%.3f", motivation_level);
        brain_kg_add_metadata(kg, state->nac_id, "motivation_level", mot_str);
    }

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t subcortical_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, SUBCORTICAL_KG_ROOT_NAME);
}

brain_kg_node_id_t subcortical_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

brain_kg_node_list_t* subcortical_kg_get_basal_ganglia_nodes(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_BG_COMPONENT
    );
}

brain_kg_node_list_t* subcortical_kg_get_thalamus_nodes(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_THALAMIC_NUCLEUS
    );
}

brain_kg_node_list_t* subcortical_kg_get_pathway_nodes(brain_kg_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(
        kg, (brain_kg_node_type_t)SUBCORTICAL_KG_NODE_PATHWAY
    );
}

int subcortical_kg_unregister_all(
    brain_kg_t* kg,
    subcortical_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;  /* Would be used for actual deletion */
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "subcortical_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    /*
     * Note: Full implementation would remove nodes in reverse order
     * For now, mark as unregistered
     */
    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(SUBCORTICAL_KG_MODULE_NAME, "Unregistered Subcortical KG nodes");

    return 0;
}

//=============================================================================
// Runtime Event Emission (Wave W2)
//=============================================================================

/*
 * HOT-PATH INTEGRATION CANDIDATE: Subcortical has many tick-style files
 * (nimcp_striatum.c, nimcp_thalamus.c, nimcp_nucleus_accumbens.c,
 * nimcp_bg_vigor.c, etc.). Action-selection callers should invoke
 * subcortical_kg_emit_event(brain, "action_selected", margin, ts_us) when
 * direct/indirect pathway wins the winner-take-all. Not wired in W2.
 */

void subcortical_kg_emit_event(
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

    char name[128];
    char desc[192];
    snprintf(name, sizeof(name),
             "subcortical_event_%s_%" PRIu64, kind, ts_us);
    snprintf(desc, sizeof(desc),
             "subcortical runtime event: kind=%s intensity=%.3f ts_us=%" PRIu64,
             kind, (double)intensity, ts_us);

    brain_kg_node_id_t evt_id = brain_kg_add_node(
        kg, name, BRAIN_KG_NODE_CUSTOM, desc
    );

    if (evt_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.4f", (double)intensity);
        brain_kg_add_metadata(kg, evt_id, "intensity", val_str);
        brain_kg_add_metadata(kg, evt_id, "kind", kind);

        brain_kg_node_id_t root = brain_kg_find_node(kg, SUBCORTICAL_KG_ROOT_NAME);
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

/* Suppress unused function warnings for health agent helpers */
__attribute__((unused))
static void suppress_unused_warnings(void) {
    (void)subcortical_kg_wiring_set_health_agent;
    (void)subcortical_kg_wiring_heartbeat;
}
