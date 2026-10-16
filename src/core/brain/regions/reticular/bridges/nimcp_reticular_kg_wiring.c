//=============================================================================
// nimcp_reticular_kg_wiring.c - Reticular Formation Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_reticular_kg_wiring.c
 * @brief Implementation of reticular formation KG registration
 */

#include "core/brain/regions/reticular/bridges/nimcp_reticular_kg_wiring.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//=============================================================================
#include <stddef.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(reticular_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create a reticular node with description
 */
static brain_kg_node_id_t create_reticular_node(
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
        NIMCP_LOG_DEBUG(RETICULAR_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

/**
 * @brief Create an edge between reticular nodes
 */
static brain_kg_edge_id_t create_reticular_edge(
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

/**
 * @brief Get node ID for nucleus type from state
 */
static brain_kg_node_id_t get_nucleus_node_id(
    const reticular_kg_state_t* state,
    int nucleus_type
) {
    if (!state) return BRAIN_KG_INVALID_NODE;

    switch (nucleus_type) {
        case 0:  return state->dorsal_raphe_id;      /* RAPHE_DORSAL */
        case 1:  return state->median_raphe_id;      /* RAPHE_MEDIAN */
        case 2:  return state->raphe_magnus_id;      /* RAPHE_MAGNUS */
        case 3:  return state->raphe_obscurus_id;    /* RAPHE_OBSCURUS */
        case 4:  return state->locus_coeruleus_id;   /* LOCUS_COERULEUS */
        case 5:  return state->lateral_tegmental_id; /* LATERAL_TEGMENTAL */
        case 6:  return state->ppn_id;               /* PEDUNCULOPONTINE */
        case 7:  return state->ldt_id;               /* LATERODORSAL_TEGMENTAL */
        case 8:  return state->pontine_oral_id;      /* PONTINE_ORAL */
        case 9:  return state->pontine_caudal_id;    /* PONTINE_CAUDAL */
        case 10: return state->gigantocellular_id;   /* GIGANTOCELLULAR */
        case 11: return state->parvocellular_id;     /* PARVOCELLULAR */
        case 12: return state->paramedian_id;        /* PARAMEDIAN */
        case 13: return state->ventral_medullary_id; /* VENTRAL_MEDULLARY */
        case 14: return state->vta_id;               /* VTA */
        default: return BRAIN_KG_INVALID_NODE;
    }
}

/**
 * @brief Get node ID for autonomic function type from state
 */
static brain_kg_node_id_t get_autonomic_node_id(
    const reticular_kg_state_t* state,
    int autonomic_type
) {
    if (!state) return BRAIN_KG_INVALID_NODE;

    switch (autonomic_type) {
        case 0: return state->cardiovascular_id;
        case 1: return state->respiratory_id;
        case 2: return state->vasomotor_id;
        case 3: return state->digestive_id;
        default: return BRAIN_KG_INVALID_NODE;
    }
}

//=============================================================================
// Configuration API
//=============================================================================

int reticular_kg_default_config(reticular_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_default_config: config is NULL");
        return -1;
    }

    config->register_nuclei = true;
    config->register_arousal = true;
    config->register_modulators = true;
    config->register_autonomic = true;
    config->register_reflexes = true;
    config->register_motor = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;
    config->register_nucleus_details = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int reticular_kg_register_all(
    brain_kg_t* kg,
    const reticular_kg_config_t* config,
    reticular_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_register_all: kg is NULL");
        return -1;
    }

    reticular_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        reticular_kg_default_config(&local_config);
    }

    reticular_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Initialize all IDs to invalid */
    local_state.root_id = BRAIN_KG_INVALID_NODE;
    local_state.nuclei_root_id = BRAIN_KG_INVALID_NODE;
    local_state.arousal_root_id = BRAIN_KG_INVALID_NODE;
    local_state.modulators_root_id = BRAIN_KG_INVALID_NODE;
    local_state.autonomic_root_id = BRAIN_KG_INVALID_NODE;
    local_state.reflexes_root_id = BRAIN_KG_INVALID_NODE;
    local_state.motor_root_id = BRAIN_KG_INVALID_NODE;

    /* Create reticular formation root node */
    local_state.root_id = create_reticular_node(
        kg, RETICULAR_KG_ROOT_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Reticular Formation - Arousal, consciousness, vital functions, and motor control",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(RETICULAR_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Register subsystems */
    if (local_config.register_nuclei) {
        if (reticular_kg_register_nuclei(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(RETICULAR_KG_MODULE_NAME, "Failed to register nuclei subsystem");
        }
    }

    if (local_config.register_arousal) {
        if (reticular_kg_register_arousal(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(RETICULAR_KG_MODULE_NAME, "Failed to register arousal subsystem");
        }
    }

    if (local_config.register_modulators) {
        if (reticular_kg_register_modulators(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(RETICULAR_KG_MODULE_NAME, "Failed to register modulators subsystem");
        }
    }

    if (local_config.register_autonomic) {
        if (reticular_kg_register_autonomic(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(RETICULAR_KG_MODULE_NAME, "Failed to register autonomic subsystem");
        }
    }

    if (local_config.register_reflexes) {
        if (reticular_kg_register_reflexes(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(RETICULAR_KG_MODULE_NAME, "Failed to register reflexes subsystem");
        }
    }

    if (local_config.register_motor) {
        if (reticular_kg_register_motor(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(RETICULAR_KG_MODULE_NAME, "Failed to register motor subsystem");
        }
    }

    /* Register cross-subsystem edges */
    if (local_config.register_cross_edges) {
        reticular_kg_register_cross_edges(kg, &local_state, admin_token);
    }

    local_state.registered = true;

    /* Copy to output if requested */
    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(RETICULAR_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

int reticular_kg_register_nuclei(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    reticular_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_register_nuclei: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create nuclei subsystem root node */
    state->nuclei_root_id = create_reticular_node(
        kg, RETICULAR_KG_NUCLEI_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Reticular nuclei - sources of neuromodulatory projections",
        admin_token
    );
    if (state->nuclei_root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_kg_register_nuclei: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, parent_id, state->nuclei_root_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains nuclei subsystem", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Serotonergic nuclei (Raphe) */
    state->dorsal_raphe_id = create_reticular_node(
        kg, "dorsal_raphe",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Dorsal raphe nucleus - 5-HT source for mood, sleep, cognition",
        admin_token
    );
    if (state->dorsal_raphe_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->dorsal_raphe_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains dorsal raphe", 1.0f, admin_token);
        state->edge_count++;
    }

    state->median_raphe_id = create_reticular_node(
        kg, "median_raphe",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Median raphe nucleus - 5-HT source for anxiety, fear, memory",
        admin_token
    );
    if (state->median_raphe_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->median_raphe_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains median raphe", 1.0f, admin_token);
        state->edge_count++;
    }

    state->raphe_magnus_id = create_reticular_node(
        kg, "raphe_magnus",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Raphe magnus - descending pain modulation",
        admin_token
    );
    if (state->raphe_magnus_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->raphe_magnus_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains raphe magnus", 1.0f, admin_token);
        state->edge_count++;
    }

    state->raphe_obscurus_id = create_reticular_node(
        kg, "raphe_obscurus",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Raphe obscurus - autonomic regulation",
        admin_token
    );
    if (state->raphe_obscurus_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->raphe_obscurus_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains raphe obscurus", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Noradrenergic nuclei */
    state->locus_coeruleus_id = create_reticular_node(
        kg, "locus_coeruleus",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Locus coeruleus - NE source for vigilance, attention, arousal",
        admin_token
    );
    if (state->locus_coeruleus_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->locus_coeruleus_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains locus coeruleus", 1.0f, admin_token);
        state->edge_count++;
    }

    state->lateral_tegmental_id = create_reticular_node(
        kg, "lateral_tegmental",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Lateral tegmental nuclei (A5/A7) - NE projections to spinal cord",
        admin_token
    );
    if (state->lateral_tegmental_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->lateral_tegmental_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains lateral tegmental", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Cholinergic nuclei */
    state->ppn_id = create_reticular_node(
        kg, "pedunculopontine_nucleus",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Pedunculopontine nucleus (PPN) - ACh for arousal, locomotion, REM",
        admin_token
    );
    if (state->ppn_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->ppn_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains PPN", 1.0f, admin_token);
        state->edge_count++;
    }

    state->ldt_id = create_reticular_node(
        kg, "laterodorsal_tegmental",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Laterodorsal tegmental nucleus (LDT) - ACh for cortical activation, REM",
        admin_token
    );
    if (state->ldt_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->ldt_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains LDT", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Pontine reticular nuclei */
    state->pontine_oral_id = create_reticular_node(
        kg, "pontine_oral",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Pontis oralis - REM sleep atonia generation",
        admin_token
    );
    if (state->pontine_oral_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->pontine_oral_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains pontis oralis", 1.0f, admin_token);
        state->edge_count++;
    }

    state->pontine_caudal_id = create_reticular_node(
        kg, "pontine_caudal",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Pontis caudalis - startle response, motor patterns",
        admin_token
    );
    if (state->pontine_caudal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->pontine_caudal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains pontis caudalis", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Medullary reticular nuclei */
    state->gigantocellular_id = create_reticular_node(
        kg, "gigantocellular",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Gigantocellular nucleus - motor tone, postural control",
        admin_token
    );
    if (state->gigantocellular_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->gigantocellular_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains gigantocellular", 1.0f, admin_token);
        state->edge_count++;
    }

    state->parvocellular_id = create_reticular_node(
        kg, "parvocellular",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Parvocellular nucleus - respiratory rhythm generation",
        admin_token
    );
    if (state->parvocellular_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->parvocellular_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains parvocellular", 1.0f, admin_token);
        state->edge_count++;
    }

    state->paramedian_id = create_reticular_node(
        kg, "paramedian",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Paramedian nucleus - eye movements, gaze control",
        admin_token
    );
    if (state->paramedian_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->paramedian_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains paramedian", 1.0f, admin_token);
        state->edge_count++;
    }

    state->ventral_medullary_id = create_reticular_node(
        kg, "ventral_medullary",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Ventral medullary nucleus - cardiovascular control center",
        admin_token
    );
    if (state->ventral_medullary_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->ventral_medullary_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains ventral medullary", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Dopaminergic nucleus */
    state->vta_id = create_reticular_node(
        kg, "ventral_tegmental_area",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS,
        "Ventral tegmental area (VTA) - DA for reward, motivation, arousal",
        admin_token
    );
    if (state->vta_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->nuclei_root_id, state->vta_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains VTA", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int reticular_kg_register_arousal(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    reticular_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_register_arousal: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create arousal subsystem root node */
    state->arousal_root_id = create_reticular_node(
        kg, RETICULAR_KG_AROUSAL_NAME,
        BRAIN_KG_NODE_COGNITIVE,
        "Arousal states - sleep-wake continuum and consciousness levels",
        admin_token
    );
    if (state->arousal_root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_kg_register_arousal: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, parent_id, state->arousal_root_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains arousal states", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Sleep states */
    state->deep_sleep_id = create_reticular_node(
        kg, "deep_sleep",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_AROUSAL_STATE,
        "Deep sleep (Stage 3-4 NREM) - delta waves, restorative, low consciousness",
        admin_token
    );
    if (state->deep_sleep_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->arousal_root_id, state->deep_sleep_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains deep sleep state", 1.0f, admin_token);
        state->edge_count++;
    }

    state->light_sleep_id = create_reticular_node(
        kg, "light_sleep",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_AROUSAL_STATE,
        "Light sleep (Stage 1-2 NREM) - theta, K-complexes, sleep spindles",
        admin_token
    );
    if (state->light_sleep_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->arousal_root_id, state->light_sleep_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains light sleep state", 1.0f, admin_token);
        state->edge_count++;
    }

    state->rem_sleep_id = create_reticular_node(
        kg, "rem_sleep",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_AROUSAL_STATE,
        "REM sleep - desynchronized EEG, dreaming, muscle atonia",
        admin_token
    );
    if (state->rem_sleep_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->arousal_root_id, state->rem_sleep_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains REM sleep state", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Wake states */
    state->drowsy_id = create_reticular_node(
        kg, "drowsy",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_AROUSAL_STATE,
        "Drowsy - pre-sleep, alpha dropout, decreased vigilance",
        admin_token
    );
    if (state->drowsy_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->arousal_root_id, state->drowsy_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains drowsy state", 1.0f, admin_token);
        state->edge_count++;
    }

    state->relaxed_id = create_reticular_node(
        kg, "relaxed",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_AROUSAL_STATE,
        "Relaxed wakefulness - alpha rhythm, quiet attention",
        admin_token
    );
    if (state->relaxed_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->arousal_root_id, state->relaxed_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains relaxed state", 1.0f, admin_token);
        state->edge_count++;
    }

    state->alert_id = create_reticular_node(
        kg, "alert",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_AROUSAL_STATE,
        "Alert - active attention, beta rhythm, engaged cognition",
        admin_token
    );
    if (state->alert_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->arousal_root_id, state->alert_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains alert state", 1.0f, admin_token);
        state->edge_count++;
    }

    state->hypervigilant_id = create_reticular_node(
        kg, "hypervigilant",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_AROUSAL_STATE,
        "Hypervigilant - high arousal, stress response, gamma activity",
        admin_token
    );
    if (state->hypervigilant_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->arousal_root_id, state->hypervigilant_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains hypervigilant state", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create state transition edges */
    if (state->deep_sleep_id != BRAIN_KG_INVALID_NODE &&
        state->light_sleep_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->deep_sleep_id, state->light_sleep_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_TRANSITIONS_TO,
            "transitions to light sleep", 0.7f, admin_token);
        state->edge_count++;
    }

    if (state->light_sleep_id != BRAIN_KG_INVALID_NODE &&
        state->rem_sleep_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->light_sleep_id, state->rem_sleep_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_TRANSITIONS_TO,
            "transitions to REM", 0.6f, admin_token);
        state->edge_count++;
    }

    if (state->light_sleep_id != BRAIN_KG_INVALID_NODE &&
        state->drowsy_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->light_sleep_id, state->drowsy_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_TRANSITIONS_TO,
            "transitions to drowsy (awakening)", 0.5f, admin_token);
        state->edge_count++;
    }

    if (state->drowsy_id != BRAIN_KG_INVALID_NODE &&
        state->relaxed_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->drowsy_id, state->relaxed_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_TRANSITIONS_TO,
            "transitions to relaxed", 0.8f, admin_token);
        state->edge_count++;
    }

    if (state->relaxed_id != BRAIN_KG_INVALID_NODE &&
        state->alert_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->relaxed_id, state->alert_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_TRANSITIONS_TO,
            "transitions to alert", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->alert_id != BRAIN_KG_INVALID_NODE &&
        state->hypervigilant_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->alert_id, state->hypervigilant_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_TRANSITIONS_TO,
            "transitions to hypervigilant (stress)", 0.4f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int reticular_kg_register_modulators(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    reticular_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_register_modulators: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create modulators subsystem root node */
    state->modulators_root_id = create_reticular_node(
        kg, RETICULAR_KG_MODULATORS_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Neuromodulators - chemical messengers for state regulation",
        admin_token
    );
    if (state->modulators_root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_kg_register_modulators: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, parent_id, state->modulators_root_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains neuromodulators", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Monoamines */
    state->serotonin_id = create_reticular_node(
        kg, "serotonin",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NEUROMODULATOR,
        "Serotonin (5-HT) - mood, sleep, pain, appetite regulation",
        admin_token
    );
    if (state->serotonin_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->modulators_root_id, state->serotonin_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains serotonin", 1.0f, admin_token);
        state->edge_count++;
    }

    state->norepinephrine_id = create_reticular_node(
        kg, "norepinephrine",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NEUROMODULATOR,
        "Norepinephrine (NE) - vigilance, attention, stress response",
        admin_token
    );
    if (state->norepinephrine_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->modulators_root_id, state->norepinephrine_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains norepinephrine", 1.0f, admin_token);
        state->edge_count++;
    }

    state->dopamine_id = create_reticular_node(
        kg, "dopamine",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NEUROMODULATOR,
        "Dopamine (DA) - reward, motivation, motor control",
        admin_token
    );
    if (state->dopamine_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->modulators_root_id, state->dopamine_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains dopamine", 1.0f, admin_token);
        state->edge_count++;
    }

    state->histamine_id = create_reticular_node(
        kg, "histamine",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NEUROMODULATOR,
        "Histamine (HA) - wakefulness, arousal from TMN",
        admin_token
    );
    if (state->histamine_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->modulators_root_id, state->histamine_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains histamine", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Acetylcholine */
    state->acetylcholine_id = create_reticular_node(
        kg, "acetylcholine",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NEUROMODULATOR,
        "Acetylcholine (ACh) - attention, memory, REM sleep",
        admin_token
    );
    if (state->acetylcholine_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->modulators_root_id, state->acetylcholine_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains acetylcholine", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Neuropeptide */
    state->orexin_id = create_reticular_node(
        kg, "orexin",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NEUROMODULATOR,
        "Orexin/Hypocretin - wakefulness stability, narcolepsy prevention",
        admin_token
    );
    if (state->orexin_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->modulators_root_id, state->orexin_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains orexin", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Amino acid transmitters */
    state->gaba_id = create_reticular_node(
        kg, "gaba",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NEUROMODULATOR,
        "GABA - inhibitory neurotransmitter, sleep promotion",
        admin_token
    );
    if (state->gaba_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->modulators_root_id, state->gaba_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains GABA", 1.0f, admin_token);
        state->edge_count++;
    }

    state->glutamate_id = create_reticular_node(
        kg, "glutamate",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_NEUROMODULATOR,
        "Glutamate - excitatory neurotransmitter, arousal",
        admin_token
    );
    if (state->glutamate_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->modulators_root_id, state->glutamate_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains glutamate", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int reticular_kg_register_autonomic(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    reticular_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_register_autonomic: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create autonomic subsystem root node */
    state->autonomic_root_id = create_reticular_node(
        kg, RETICULAR_KG_AUTONOMIC_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Autonomic functions - vital homeostatic control",
        admin_token
    );
    if (state->autonomic_root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_kg_register_autonomic: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, parent_id, state->autonomic_root_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains autonomic functions", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Cardiovascular */
    state->cardiovascular_id = create_reticular_node(
        kg, "cardiovascular_center",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_AUTONOMIC,
        "Cardiovascular center - heart rate, blood pressure control",
        admin_token
    );
    if (state->cardiovascular_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->autonomic_root_id, state->cardiovascular_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains cardiovascular center", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Respiratory */
    state->respiratory_id = create_reticular_node(
        kg, "respiratory_center",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_AUTONOMIC,
        "Respiratory center - breathing rate and depth control",
        admin_token
    );
    if (state->respiratory_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->autonomic_root_id, state->respiratory_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains respiratory center", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Vasomotor */
    state->vasomotor_id = create_reticular_node(
        kg, "vasomotor_center",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_AUTONOMIC,
        "Vasomotor center - vascular tone regulation",
        admin_token
    );
    if (state->vasomotor_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->autonomic_root_id, state->vasomotor_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains vasomotor center", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Digestive */
    state->digestive_id = create_reticular_node(
        kg, "digestive_center",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_AUTONOMIC,
        "Digestive center - GI motility and secretion",
        admin_token
    );
    if (state->digestive_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->autonomic_root_id, state->digestive_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains digestive center", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int reticular_kg_register_reflexes(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    reticular_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_register_reflexes: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create reflexes subsystem root node */
    state->reflexes_root_id = create_reticular_node(
        kg, RETICULAR_KG_REFLEXES_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Reflexes - protective and automatic motor patterns",
        admin_token
    );
    if (state->reflexes_root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_kg_register_reflexes: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, parent_id, state->reflexes_root_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains reflexes", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Airway protection */
    state->swallowing_id = create_reticular_node(
        kg, "swallowing_reflex",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_REFLEX,
        "Swallowing (deglutition) - coordinated pharyngeal motor pattern",
        admin_token
    );
    if (state->swallowing_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->reflexes_root_id, state->swallowing_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains swallowing reflex", 1.0f, admin_token);
        state->edge_count++;
    }

    state->coughing_id = create_reticular_node(
        kg, "coughing_reflex",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_REFLEX,
        "Coughing - airway clearance and protection",
        admin_token
    );
    if (state->coughing_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->reflexes_root_id, state->coughing_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains coughing reflex", 1.0f, admin_token);
        state->edge_count++;
    }

    state->vomiting_id = create_reticular_node(
        kg, "vomiting_reflex",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_REFLEX,
        "Vomiting (emesis) - protective gastric expulsion",
        admin_token
    );
    if (state->vomiting_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->reflexes_root_id, state->vomiting_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains vomiting reflex", 1.0f, admin_token);
        state->edge_count++;
    }

    state->sneezing_id = create_reticular_node(
        kg, "sneezing_reflex",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_REFLEX,
        "Sneezing - nasal irritant expulsion",
        admin_token
    );
    if (state->sneezing_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->reflexes_root_id, state->sneezing_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains sneezing reflex", 1.0f, admin_token);
        state->edge_count++;
    }

    state->gagging_id = create_reticular_node(
        kg, "gagging_reflex",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_REFLEX,
        "Gagging - pharyngeal protection from foreign objects",
        admin_token
    );
    if (state->gagging_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->reflexes_root_id, state->gagging_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains gagging reflex", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Arousal-related */
    state->yawning_id = create_reticular_node(
        kg, "yawning_reflex",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_REFLEX,
        "Yawning - arousal modulation, social contagion",
        admin_token
    );
    if (state->yawning_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->reflexes_root_id, state->yawning_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains yawning reflex", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Defensive */
    state->startle_id = create_reticular_node(
        kg, "startle_reflex",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_REFLEX,
        "Startle (acoustic startle) - rapid defensive response",
        admin_token
    );
    if (state->startle_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->reflexes_root_id, state->startle_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains startle reflex", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Postural */
    state->righting_id = create_reticular_node(
        kg, "righting_reflex",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_REFLEX,
        "Righting reflex - postural correction and balance",
        admin_token
    );
    if (state->righting_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->reflexes_root_id, state->righting_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains righting reflex", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int reticular_kg_register_motor(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    reticular_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_register_motor: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create motor control subsystem root node */
    state->motor_root_id = create_reticular_node(
        kg, RETICULAR_KG_MOTOR_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Motor control - muscle tone and locomotion regulation",
        admin_token
    );
    if (state->motor_root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_kg_register_motor: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, parent_id, state->motor_root_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains motor control", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Postural tone */
    state->postural_tone_id = create_reticular_node(
        kg, "postural_tone",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_MOTOR_FUNCTION,
        "Postural tone - antigravity muscle tone via reticulospinal tract",
        admin_token
    );
    if (state->postural_tone_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->motor_root_id, state->postural_tone_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains postural tone control", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Locomotor drive */
    state->locomotor_drive_id = create_reticular_node(
        kg, "locomotor_drive",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_MOTOR_FUNCTION,
        "Locomotor drive - mesencephalic locomotor region activation",
        admin_token
    );
    if (state->locomotor_drive_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->motor_root_id, state->locomotor_drive_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains locomotor drive", 1.0f, admin_token);
        state->edge_count++;
    }

    /* REM atonia */
    state->rem_atonia_id = create_reticular_node(
        kg, "rem_atonia",
        (brain_kg_node_type_t)RETICULAR_KG_NODE_MOTOR_FUNCTION,
        "REM atonia - muscle paralysis during REM sleep",
        admin_token
    );
    if (state->rem_atonia_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_reticular_edge(kg, state->motor_root_id, state->rem_atonia_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains REM atonia control", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int reticular_kg_register_cross_edges(
    brain_kg_t* kg,
    reticular_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_register_cross_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* ==========================================
     * Nucleus -> Neuromodulator relationships
     * ========================================== */

    /* Raphe nuclei release serotonin */
    if (state->dorsal_raphe_id != BRAIN_KG_INVALID_NODE &&
        state->serotonin_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->dorsal_raphe_id, state->serotonin_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_RELEASES,
            "dorsal raphe releases serotonin", 0.95f, admin_token);
        state->edge_count++;
    }

    if (state->median_raphe_id != BRAIN_KG_INVALID_NODE &&
        state->serotonin_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->median_raphe_id, state->serotonin_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_RELEASES,
            "median raphe releases serotonin", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Locus coeruleus releases norepinephrine */
    if (state->locus_coeruleus_id != BRAIN_KG_INVALID_NODE &&
        state->norepinephrine_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->locus_coeruleus_id, state->norepinephrine_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_RELEASES,
            "locus coeruleus releases norepinephrine", 0.95f, admin_token);
        state->edge_count++;
    }

    /* PPN and LDT release acetylcholine */
    if (state->ppn_id != BRAIN_KG_INVALID_NODE &&
        state->acetylcholine_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->ppn_id, state->acetylcholine_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_RELEASES,
            "PPN releases acetylcholine", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->ldt_id != BRAIN_KG_INVALID_NODE &&
        state->acetylcholine_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->ldt_id, state->acetylcholine_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_RELEASES,
            "LDT releases acetylcholine", 0.9f, admin_token);
        state->edge_count++;
    }

    /* VTA releases dopamine */
    if (state->vta_id != BRAIN_KG_INVALID_NODE &&
        state->dopamine_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->vta_id, state->dopamine_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_RELEASES,
            "VTA releases dopamine", 0.95f, admin_token);
        state->edge_count++;
    }

    /* ==========================================
     * Neuromodulator -> Arousal state relationships
     * ========================================== */

    /* Serotonin promotes wakefulness, inhibits REM */
    if (state->serotonin_id != BRAIN_KG_INVALID_NODE &&
        state->relaxed_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->serotonin_id, state->relaxed_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_MODULATES_AROUSAL,
            "serotonin promotes relaxed wakefulness", 0.7f, admin_token);
        state->edge_count++;
    }

    if (state->serotonin_id != BRAIN_KG_INVALID_NODE &&
        state->rem_sleep_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->serotonin_id, state->rem_sleep_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_INHIBITS_FUNCTION,
            "serotonin inhibits REM sleep", 0.8f, admin_token);
        state->edge_count++;
    }

    /* Norepinephrine promotes alertness */
    if (state->norepinephrine_id != BRAIN_KG_INVALID_NODE &&
        state->alert_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->norepinephrine_id, state->alert_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_MODULATES_AROUSAL,
            "norepinephrine promotes alertness", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->norepinephrine_id != BRAIN_KG_INVALID_NODE &&
        state->hypervigilant_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->norepinephrine_id, state->hypervigilant_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_MODULATES_AROUSAL,
            "norepinephrine drives hypervigilance", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Acetylcholine promotes REM and alertness */
    if (state->acetylcholine_id != BRAIN_KG_INVALID_NODE &&
        state->rem_sleep_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->acetylcholine_id, state->rem_sleep_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_MODULATES_AROUSAL,
            "acetylcholine promotes REM sleep", 0.85f, admin_token);
        state->edge_count++;
    }

    if (state->acetylcholine_id != BRAIN_KG_INVALID_NODE &&
        state->alert_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->acetylcholine_id, state->alert_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_MODULATES_AROUSAL,
            "acetylcholine enhances alertness", 0.7f, admin_token);
        state->edge_count++;
    }

    /* Orexin stabilizes wakefulness */
    if (state->orexin_id != BRAIN_KG_INVALID_NODE &&
        state->alert_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->orexin_id, state->alert_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_MODULATES_AROUSAL,
            "orexin stabilizes wakefulness", 0.9f, admin_token);
        state->edge_count++;
    }

    /* GABA promotes sleep */
    if (state->gaba_id != BRAIN_KG_INVALID_NODE &&
        state->deep_sleep_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->gaba_id, state->deep_sleep_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_MODULATES_AROUSAL,
            "GABA promotes deep sleep", 0.85f, admin_token);
        state->edge_count++;
    }

    /* ==========================================
     * Arousal state -> Nucleus relationships
     * ========================================== */

    /* Alert state activates LC */
    if (state->alert_id != BRAIN_KG_INVALID_NODE &&
        state->locus_coeruleus_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->alert_id, state->locus_coeruleus_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_ACTIVATES,
            "alertness activates locus coeruleus", 0.8f, admin_token);
        state->edge_count++;
    }

    /* REM activates cholinergic nuclei */
    if (state->rem_sleep_id != BRAIN_KG_INVALID_NODE &&
        state->ppn_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->rem_sleep_id, state->ppn_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_ACTIVATES,
            "REM activates PPN", 0.85f, admin_token);
        state->edge_count++;
    }

    if (state->rem_sleep_id != BRAIN_KG_INVALID_NODE &&
        state->ldt_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->rem_sleep_id, state->ldt_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_ACTIVATES,
            "REM activates LDT", 0.85f, admin_token);
        state->edge_count++;
    }

    /* ==========================================
     * Autonomic -> Nucleus relationships
     * ========================================== */

    /* Cardiovascular center regulated by ventral medullary */
    if (state->cardiovascular_id != BRAIN_KG_INVALID_NODE &&
        state->ventral_medullary_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->ventral_medullary_id, state->cardiovascular_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_REGULATES,
            "ventral medullary controls cardiovascular function", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Respiratory center regulated by parvocellular */
    if (state->respiratory_id != BRAIN_KG_INVALID_NODE &&
        state->parvocellular_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->parvocellular_id, state->respiratory_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_REGULATES,
            "parvocellular generates respiratory rhythm", 0.9f, admin_token);
        state->edge_count++;
    }

    /* ==========================================
     * Reflex -> Motor control relationships
     * ========================================== */

    /* Startle triggers motor response */
    if (state->startle_id != BRAIN_KG_INVALID_NODE &&
        state->postural_tone_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->startle_id, state->postural_tone_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_TRIGGERS,
            "startle triggers postural response", 0.9f, admin_token);
        state->edge_count++;
    }

    /* Righting reflex uses postural tone */
    if (state->righting_id != BRAIN_KG_INVALID_NODE &&
        state->postural_tone_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->righting_id, state->postural_tone_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_TRIGGERS,
            "righting reflex adjusts postural tone", 0.85f, admin_token);
        state->edge_count++;
    }

    /* ==========================================
     * Motor control relationships
     * ========================================== */

    /* Gigantocellular controls postural tone */
    if (state->gigantocellular_id != BRAIN_KG_INVALID_NODE &&
        state->postural_tone_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->gigantocellular_id, state->postural_tone_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_REGULATES,
            "gigantocellular regulates postural tone", 0.9f, admin_token);
        state->edge_count++;
    }

    /* PPN drives locomotion */
    if (state->ppn_id != BRAIN_KG_INVALID_NODE &&
        state->locomotor_drive_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->ppn_id, state->locomotor_drive_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_REGULATES,
            "PPN modulates locomotor drive", 0.85f, admin_token);
        state->edge_count++;
    }

    /* Pontine oral generates REM atonia */
    if (state->pontine_oral_id != BRAIN_KG_INVALID_NODE &&
        state->rem_atonia_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->pontine_oral_id, state->rem_atonia_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_REGULATES,
            "pontis oralis generates REM atonia", 0.9f, admin_token);
        state->edge_count++;
    }

    /* REM state activates atonia */
    if (state->rem_sleep_id != BRAIN_KG_INVALID_NODE &&
        state->rem_atonia_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->rem_sleep_id, state->rem_atonia_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_ACTIVATES,
            "REM sleep activates muscle atonia", 0.95f, admin_token);
        state->edge_count++;
    }

    /* Pontine caudal mediates startle */
    if (state->pontine_caudal_id != BRAIN_KG_INVALID_NODE &&
        state->startle_id != BRAIN_KG_INVALID_NODE) {
        create_reticular_edge(kg, state->pontine_caudal_id, state->startle_id,
            (brain_kg_edge_type_t)RETICULAR_KG_EDGE_REGULATES,
            "pontis caudalis mediates startle response", 0.9f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// State Synchronization API
//=============================================================================

int reticular_kg_update_state(
    brain_kg_t* kg,
    const reticular_kg_state_t* state,
    float arousal_level,
    int arousal_state,
    float serotonin_level,
    float norepinephrine_level,
    float acetylcholine_level,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_update_state: required parameter is NULL (kg, state)");
        return -1;
    }

    char value_str[NIMCP_ID_BUFFER_SIZE];

    /* Update root node with arousal level */
    if (state->root_id != BRAIN_KG_INVALID_NODE) {
        snprintf(value_str, sizeof(value_str), "%.2f", arousal_level);
        brain_kg_add_metadata(kg, state->root_id, "arousal_level", value_str);

        const char* state_names[] = {
            "deep_sleep", "light_sleep", "rem_sleep", "drowsy",
            "relaxed", "alert", "hypervigilant"
        };
        if (arousal_state >= 0 && arousal_state < 7) {
            brain_kg_add_metadata(kg, state->root_id, "arousal_state", state_names[arousal_state]);
        }
    }

    /* Update neuromodulator levels */
    if (state->serotonin_id != BRAIN_KG_INVALID_NODE) {
        snprintf(value_str, sizeof(value_str), "%.1f%%", serotonin_level * 100.0f);
        brain_kg_add_metadata(kg, state->serotonin_id, "concentration", value_str);
    }

    if (state->norepinephrine_id != BRAIN_KG_INVALID_NODE) {
        snprintf(value_str, sizeof(value_str), "%.1f%%", norepinephrine_level * 100.0f);
        brain_kg_add_metadata(kg, state->norepinephrine_id, "concentration", value_str);
    }

    if (state->acetylcholine_id != BRAIN_KG_INVALID_NODE) {
        snprintf(value_str, sizeof(value_str), "%.1f%%", acetylcholine_level * 100.0f);
        brain_kg_add_metadata(kg, state->acetylcholine_id, "concentration", value_str);
    }

    return 0;
}

int reticular_kg_update_nucleus(
    brain_kg_t* kg,
    const reticular_kg_state_t* state,
    int nucleus_type,
    float activity,
    float firing_rate,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_update_nucleus: required parameter is NULL (kg, state)");
        return -1;
    }

    brain_kg_node_id_t node_id = get_nucleus_node_id(state, nucleus_type);
    if (node_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_kg_update_nucleus: validation failed");
        return -1;
    }

    char value_str[NIMCP_ID_BUFFER_SIZE];

    snprintf(value_str, sizeof(value_str), "%.2f", activity);
    brain_kg_add_metadata(kg, node_id, "activity", value_str);

    snprintf(value_str, sizeof(value_str), "%.1f Hz", firing_rate);
    brain_kg_add_metadata(kg, node_id, "firing_rate", value_str);

    return 0;
}

int reticular_kg_update_autonomic(
    brain_kg_t* kg,
    const reticular_kg_state_t* state,
    int autonomic_type,
    float sympathetic_tone,
    float parasympathetic_tone,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_update_autonomic: required parameter is NULL (kg, state)");
        return -1;
    }

    brain_kg_node_id_t node_id = get_autonomic_node_id(state, autonomic_type);
    if (node_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_kg_update_autonomic: validation failed");
        return -1;
    }

    char value_str[NIMCP_ID_BUFFER_SIZE];

    snprintf(value_str, sizeof(value_str), "%.2f", sympathetic_tone);
    brain_kg_add_metadata(kg, node_id, "sympathetic_tone", value_str);

    snprintf(value_str, sizeof(value_str), "%.2f", parasympathetic_tone);
    brain_kg_add_metadata(kg, node_id, "parasympathetic_tone", value_str);

    float balance = sympathetic_tone - parasympathetic_tone;
    snprintf(value_str, sizeof(value_str), "%.2f", balance);
    brain_kg_add_metadata(kg, node_id, "balance", value_str);

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t reticular_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, RETICULAR_KG_ROOT_NAME);
}

brain_kg_node_id_t reticular_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

brain_kg_node_list_t* reticular_kg_get_nuclei(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS);
}

brain_kg_node_list_t* reticular_kg_get_arousal_states(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)RETICULAR_KG_NODE_AROUSAL_STATE);
}

brain_kg_node_list_t* reticular_kg_get_modulators(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)RETICULAR_KG_NODE_NEUROMODULATOR);
}

brain_kg_node_list_t* reticular_kg_get_nuclei_for_modulator(
    brain_kg_t* kg,
    brain_kg_node_id_t modulator_id
) {
    if (!kg || modulator_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_get_nuclei_for_modulator: kg is NULL");
        return NULL;
    }

    /* Get incoming edges to modulator (RELEASES edges from nuclei) */
    brain_kg_edge_list_t* edges = brain_kg_get_incoming(kg, modulator_id);
    if (!edges) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "edges is NULL");

        return NULL;

    }

    /* Allocate result list */
    brain_kg_node_list_t* result = (brain_kg_node_list_t*)nimcp_malloc(sizeof(brain_kg_node_list_t));
    if (!result) {
        brain_kg_edge_list_destroy(edges);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reticular_kg_get_nuclei_for_modulator: result is NULL");
        return NULL;
    }

    result->nodes = (brain_kg_node_t**)nimcp_malloc(edges->count * sizeof(brain_kg_node_t*));
    if (!result->nodes) {
        nimcp_free(result);
        brain_kg_edge_list_destroy(edges);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reticular_kg_get_nuclei_for_modulator: result->nodes is NULL");
        return NULL;
    }
    result->count = 0;
    result->capacity = edges->count;

    /* Filter for RELEASES edges and get source nuclei */
    for (uint32_t i = 0; i < edges->count; i++) {
        if (edges->edges[i]->type == (brain_kg_edge_type_t)RETICULAR_KG_EDGE_RELEASES) {
            const brain_kg_node_t* node = brain_kg_get_node(kg, edges->edges[i]->from);
            if (node && node->type == (brain_kg_node_type_t)RETICULAR_KG_NODE_NUCLEUS) {
                result->nodes[result->count++] = (brain_kg_node_t*)node;
            }
        }
    }

    brain_kg_edge_list_destroy(edges);
    return result;
}

brain_kg_node_list_t* reticular_kg_get_modulators_from_nucleus(
    brain_kg_t* kg,
    brain_kg_node_id_t nucleus_id
) {
    if (!kg || nucleus_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_get_modulators_from_nucleus: kg is NULL");
        return NULL;
    }

    /* Get outgoing edges from nucleus (RELEASES edges to modulators) */
    brain_kg_edge_list_t* edges = brain_kg_get_outgoing(kg, nucleus_id);
    if (!edges) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "edges is NULL");

        return NULL;

    }

    /* Allocate result list */
    brain_kg_node_list_t* result = (brain_kg_node_list_t*)nimcp_malloc(sizeof(brain_kg_node_list_t));
    if (!result) {
        brain_kg_edge_list_destroy(edges);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reticular_kg_get_modulators_from_nucleus: result is NULL");
        return NULL;
    }

    result->nodes = (brain_kg_node_t**)nimcp_malloc(edges->count * sizeof(brain_kg_node_t*));
    if (!result->nodes) {
        nimcp_free(result);
        brain_kg_edge_list_destroy(edges);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reticular_kg_get_modulators_from_nucleus: result->nodes is NULL");
        return NULL;
    }
    result->count = 0;
    result->capacity = edges->count;

    /* Filter for RELEASES edges and get target modulators */
    for (uint32_t i = 0; i < edges->count; i++) {
        if (edges->edges[i]->type == (brain_kg_edge_type_t)RETICULAR_KG_EDGE_RELEASES) {
            const brain_kg_node_t* node = brain_kg_get_node(kg, edges->edges[i]->to);
            if (node && node->type == (brain_kg_node_type_t)RETICULAR_KG_NODE_NEUROMODULATOR) {
                result->nodes[result->count++] = (brain_kg_node_t*)node;
            }
        }
    }

    brain_kg_edge_list_destroy(edges);
    return result;
}

int reticular_kg_unregister_all(
    brain_kg_t* kg,
    reticular_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    /* In a full implementation, would iterate and remove all nodes */
    /* For now, just mark as unregistered */
    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(RETICULAR_KG_MODULE_NAME, "Unregistered reticular KG nodes");
    (void)admin_token;  /* Would be used for actual deletion */

    return 0;
}
