//=============================================================================
// nimcp_pag_kg_wiring.c - PAG Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_pag_kg_wiring.c
 * @brief Implementation of PAG KG registration
 */

#include "core/brain/regions/pag/bridges/nimcp_pag_kg_wiring.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pag_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create a PAG node with description
 */
static brain_kg_node_id_t create_pag_node(
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
        NIMCP_LOG_DEBUG(PAG_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

/**
 * @brief Create an edge between PAG nodes
 */
static brain_kg_edge_id_t create_pag_edge(
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

int pag_kg_default_config(pag_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_default_config: config is NULL");
        return -1;
    }

    config->register_columns = true;
    config->register_defense = true;
    config->register_pain = true;
    config->register_emotions = true;
    config->register_vocalizations = true;
    config->register_autonomic = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int pag_kg_register_all(
    brain_kg_t* kg,
    const pag_kg_config_t* config,
    pag_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_register_all: kg is NULL");
        return -1;
    }

    pag_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        pag_kg_default_config(&local_config);
    }

    pag_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create PAG root node */
    local_state.root_id = create_pag_node(
        kg, PAG_KG_ROOT_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "Periaqueductal Gray - survival behavior control center, "
        "defensive responses, pain modulation, emotional expression",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(PAG_KG_MODULE_NAME, "Failed to create PAG root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Register subsystems */
    if (local_config.register_columns) {
        if (pag_kg_register_columns(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(PAG_KG_MODULE_NAME, "Failed to register column subsystem");
        }
    }

    if (local_config.register_defense) {
        if (pag_kg_register_defense(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(PAG_KG_MODULE_NAME, "Failed to register defense subsystem");
        }
    }

    if (local_config.register_pain) {
        if (pag_kg_register_pain(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(PAG_KG_MODULE_NAME, "Failed to register pain subsystem");
        }
    }

    if (local_config.register_emotions) {
        if (pag_kg_register_emotions(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(PAG_KG_MODULE_NAME, "Failed to register emotion subsystem");
        }
    }

    if (local_config.register_vocalizations) {
        if (pag_kg_register_vocalizations(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(PAG_KG_MODULE_NAME, "Failed to register vocalization subsystem");
        }
    }

    if (local_config.register_autonomic) {
        if (pag_kg_register_autonomic(kg, local_state.root_id, &local_state, admin_token) < 0) {
            NIMCP_LOG_WARN(PAG_KG_MODULE_NAME, "Failed to register autonomic subsystem");
        }
    }

    /* Register cross-subsystem edges */
    if (local_config.register_cross_edges) {
        pag_kg_register_cross_edges(kg, &local_state, admin_token);
    }

    local_state.registered = true;

    /* Copy to output if requested */
    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(PAG_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

int pag_kg_register_columns(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pag_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_register_columns: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create columns subsystem node */
    state->columns_subsystem_id = create_pag_node(
        kg, PAG_KG_COLUMNS_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "PAG columnar organization - functional subdivisions controlling behavior",
        admin_token
    );
    if (state->columns_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_kg_register_columns: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, parent_id, state->columns_subsystem_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains column subsystem", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create dorsolateral column */
    state->dorsolateral_id = create_pag_node(
        kg, "pag_dorsolateral",
        (brain_kg_node_type_t)PAG_KG_NODE_COLUMN,
        "Dorsolateral PAG (dlPAG) - active coping, fight/flight, "
        "tachycardia, hypertension, non-opioid analgesia",
        admin_token
    );
    if (state->dorsolateral_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->columns_subsystem_id, state->dorsolateral_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains dorsolateral column", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create lateral column */
    state->lateral_id = create_pag_node(
        kg, "pag_lateral",
        (brain_kg_node_type_t)PAG_KG_NODE_COLUMN,
        "Lateral PAG (lPAG) - vocalization, active defense, "
        "affective responses, threat displays",
        admin_token
    );
    if (state->lateral_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->columns_subsystem_id, state->lateral_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains lateral column", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create dorsomedial column */
    state->dorsomedial_id = create_pag_node(
        kg, "pag_dorsomedial",
        (brain_kg_node_type_t)PAG_KG_NODE_COLUMN,
        "Dorsomedial PAG (dmPAG) - defensive attention, scanning, "
        "threat assessment, vigilance",
        admin_token
    );
    if (state->dorsomedial_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->columns_subsystem_id, state->dorsomedial_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains dorsomedial column", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create ventrolateral column */
    state->ventrolateral_id = create_pag_node(
        kg, "pag_ventrolateral",
        (brain_kg_node_type_t)PAG_KG_NODE_COLUMN,
        "Ventrolateral PAG (vlPAG) - passive coping, freeze/fawn, "
        "bradycardia, hypotension, opioid analgesia",
        admin_token
    );
    if (state->ventrolateral_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->columns_subsystem_id, state->ventrolateral_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains ventrolateral column", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create inter-column competition edges */
    if (state->dorsolateral_id != BRAIN_KG_INVALID_NODE &&
        state->ventrolateral_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->dorsolateral_id, state->ventrolateral_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_INHIBITS,
            "active coping inhibits passive coping", 0.7f, admin_token);
        state->edge_count++;
        create_pag_edge(kg, state->ventrolateral_id, state->dorsolateral_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_INHIBITS,
            "passive coping inhibits active coping", 0.7f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int pag_kg_register_defense(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pag_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_register_defense: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create defense subsystem node */
    state->defense_subsystem_id = create_pag_node(
        kg, PAG_KG_DEFENSE_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "PAG defensive behavior system - 4F response patterns",
        admin_token
    );
    if (state->defense_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_kg_register_defense: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, parent_id, state->defense_subsystem_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains defense subsystem", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create fight response */
    state->fight_id = create_pag_node(
        kg, "pag_fight",
        (brain_kg_node_type_t)PAG_KG_NODE_DEFENSE_STATE,
        "Fight response - active confrontation, aggression, "
        "sympathetic activation, increased muscle tone",
        admin_token
    );
    if (state->fight_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->defense_subsystem_id, state->fight_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains fight response", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create flight response */
    state->flight_id = create_pag_node(
        kg, "pag_flight",
        (brain_kg_node_type_t)PAG_KG_NODE_DEFENSE_STATE,
        "Flight response - active escape, running, "
        "cardiovascular mobilization, enhanced motor output",
        admin_token
    );
    if (state->flight_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->defense_subsystem_id, state->flight_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains flight response", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create freeze response */
    state->freeze_id = create_pag_node(
        kg, "pag_freeze",
        (brain_kg_node_type_t)PAG_KG_NODE_DEFENSE_STATE,
        "Freeze response - passive immobility, tonic immobility, "
        "parasympathetic activation, bradycardia",
        admin_token
    );
    if (state->freeze_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->defense_subsystem_id, state->freeze_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains freeze response", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create fawn response */
    state->fawn_id = create_pag_node(
        kg, "pag_fawn",
        (brain_kg_node_type_t)PAG_KG_NODE_DEFENSE_STATE,
        "Fawn response - passive submission, appeasement, "
        "social deference, conflict avoidance",
        admin_token
    );
    if (state->fawn_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->defense_subsystem_id, state->fawn_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains fawn response", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create coping strategy nodes */
    state->active_coping_id = create_pag_node(
        kg, "pag_active_coping",
        (brain_kg_node_type_t)PAG_KG_NODE_COPING_STRATEGY,
        "Active coping strategy - fight or flight responses, "
        "dlPAG/lPAG mediated, sympathetic dominance",
        admin_token
    );
    if (state->active_coping_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
    }

    state->passive_coping_id = create_pag_node(
        kg, "pag_passive_coping",
        (brain_kg_node_type_t)PAG_KG_NODE_COPING_STRATEGY,
        "Passive coping strategy - freeze or fawn responses, "
        "vlPAG mediated, parasympathetic dominance",
        admin_token
    );
    if (state->passive_coping_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
    }

    return 0;
}

int pag_kg_register_pain(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pag_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_register_pain: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create pain modulation subsystem node */
    state->pain_subsystem_id = create_pag_node(
        kg, PAG_KG_PAIN_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "PAG pain modulation system - descending inhibition, "
        "endogenous analgesia pathways",
        admin_token
    );
    if (state->pain_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_kg_register_pain: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, parent_id, state->pain_subsystem_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains pain modulation subsystem", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create opioid pathway */
    state->opioid_pathway_id = create_pag_node(
        kg, "pag_opioid_pathway",
        (brain_kg_node_type_t)PAG_KG_NODE_PAIN_PATHWAY,
        "Opioid pathway - endogenous endorphins/enkephalins, "
        "mu-opioid receptors, vlPAG-RVM projection",
        admin_token
    );
    if (state->opioid_pathway_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->pain_subsystem_id, state->opioid_pathway_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains opioid pathway", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create non-opioid pathway */
    state->non_opioid_pathway_id = create_pag_node(
        kg, "pag_non_opioid_pathway",
        (brain_kg_node_type_t)PAG_KG_NODE_PAIN_PATHWAY,
        "Non-opioid pathway - stress-induced analgesia, "
        "dlPAG mediated, naloxone-insensitive",
        admin_token
    );
    if (state->non_opioid_pathway_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->pain_subsystem_id, state->non_opioid_pathway_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains non-opioid pathway", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create cannabinoid pathway */
    state->cannabinoid_pathway_id = create_pag_node(
        kg, "pag_cannabinoid_pathway",
        (brain_kg_node_type_t)PAG_KG_NODE_PAIN_PATHWAY,
        "Cannabinoid pathway - endocannabinoid system, "
        "CB1 receptors, stress-induced release",
        admin_token
    );
    if (state->cannabinoid_pathway_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->pain_subsystem_id, state->cannabinoid_pathway_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains cannabinoid pathway", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create serotonergic pathway */
    state->serotonergic_pathway_id = create_pag_node(
        kg, "pag_serotonergic_pathway",
        (brain_kg_node_type_t)PAG_KG_NODE_PAIN_PATHWAY,
        "Serotonergic pathway - 5-HT descending inhibition, "
        "RVM to spinal dorsal horn projection",
        admin_token
    );
    if (state->serotonergic_pathway_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->pain_subsystem_id, state->serotonergic_pathway_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains serotonergic pathway", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create noradrenergic pathway */
    state->noradrenergic_pathway_id = create_pag_node(
        kg, "pag_noradrenergic_pathway",
        (brain_kg_node_type_t)PAG_KG_NODE_PAIN_PATHWAY,
        "Noradrenergic pathway - norepinephrine-mediated, "
        "alpha-2 adrenergic receptors, locus coeruleus input",
        admin_token
    );
    if (state->noradrenergic_pathway_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->pain_subsystem_id, state->noradrenergic_pathway_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains noradrenergic pathway", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create analgesia state node */
    state->analgesia_id = create_pag_node(
        kg, "pag_analgesia",
        (brain_kg_node_type_t)PAG_KG_NODE_ANALGESIA,
        "Analgesia state - pain inhibition output, "
        "descending modulation to spinal cord",
        admin_token
    );
    if (state->analgesia_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->pain_subsystem_id, state->analgesia_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "produces analgesia output", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create pathway -> analgesia edges */
    if (state->opioid_pathway_id != BRAIN_KG_INVALID_NODE &&
        state->analgesia_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->opioid_pathway_id, state->analgesia_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_PRODUCES,
            "opioid pathway produces analgesia", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->cannabinoid_pathway_id != BRAIN_KG_INVALID_NODE &&
        state->analgesia_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->cannabinoid_pathway_id, state->analgesia_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_PRODUCES,
            "cannabinoid pathway produces analgesia", 0.7f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int pag_kg_register_emotions(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pag_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_register_emotions: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create emotion subsystem node */
    state->emotion_subsystem_id = create_pag_node(
        kg, PAG_KG_EMOTION_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "PAG emotional expression system - affective states, "
        "survival-related emotions",
        admin_token
    );
    if (state->emotion_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_kg_register_emotions: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, parent_id, state->emotion_subsystem_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains emotion subsystem", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create fear emotion */
    state->fear_id = create_pag_node(
        kg, "pag_fear",
        (brain_kg_node_type_t)PAG_KG_NODE_EMOTION_STATE,
        "Fear emotion - terror, dread, amygdala input, "
        "activates dlPAG for flight or vlPAG for freeze",
        admin_token
    );
    if (state->fear_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->emotion_subsystem_id, state->fear_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains fear emotion", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create rage emotion */
    state->rage_id = create_pag_node(
        kg, "pag_rage",
        (brain_kg_node_type_t)PAG_KG_NODE_EMOTION_STATE,
        "Rage emotion - anger, fury, hypothalamic input, "
        "activates dlPAG for fight response",
        admin_token
    );
    if (state->rage_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->emotion_subsystem_id, state->rage_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains rage emotion", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create pain emotion */
    state->pain_emotion_id = create_pag_node(
        kg, "pag_pain_affect",
        (brain_kg_node_type_t)PAG_KG_NODE_EMOTION_STATE,
        "Pain affect - suffering, distress, spinothalamic input, "
        "activates pain modulation pathways",
        admin_token
    );
    if (state->pain_emotion_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->emotion_subsystem_id, state->pain_emotion_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains pain affect", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create panic emotion */
    state->panic_id = create_pag_node(
        kg, "pag_panic",
        (brain_kg_node_type_t)PAG_KG_NODE_EMOTION_STATE,
        "Panic emotion - separation distress, suffocation alarm, "
        "activates dlPAG for escape, distress vocalizations",
        admin_token
    );
    if (state->panic_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->emotion_subsystem_id, state->panic_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains panic emotion", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create maternal emotion */
    state->maternal_id = create_pag_node(
        kg, "pag_maternal",
        (brain_kg_node_type_t)PAG_KG_NODE_EMOTION_STATE,
        "Maternal emotion - nurturing, care, oxytocin modulation, "
        "maternal vocalizations, protective behaviors",
        admin_token
    );
    if (state->maternal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->emotion_subsystem_id, state->maternal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains maternal emotion", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create reproductive emotion */
    state->reproductive_id = create_pag_node(
        kg, "pag_reproductive",
        (brain_kg_node_type_t)PAG_KG_NODE_EMOTION_STATE,
        "Reproductive emotion - sexual, mating behaviors, "
        "hypothalamic input, lordosis/mounting coordination",
        admin_token
    );
    if (state->reproductive_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->emotion_subsystem_id, state->reproductive_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains reproductive emotion", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int pag_kg_register_vocalizations(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pag_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_register_vocalizations: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create vocalization subsystem node */
    state->vocal_subsystem_id = create_pag_node(
        kg, PAG_KG_VOCAL_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "PAG vocalization control system - emotional calls, "
        "lPAG mediated voice production",
        admin_token
    );
    if (state->vocal_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_kg_register_vocalizations: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, parent_id, state->vocal_subsystem_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains vocalization subsystem", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create alarm vocalization */
    state->alarm_vocal_id = create_pag_node(
        kg, "pag_vocal_alarm",
        (brain_kg_node_type_t)PAG_KG_NODE_VOCALIZATION,
        "Alarm vocalization - warning calls, threat detection, "
        "conspecific alert, lPAG activation",
        admin_token
    );
    if (state->alarm_vocal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->vocal_subsystem_id, state->alarm_vocal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains alarm vocalization", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create aggression vocalization */
    state->aggression_vocal_id = create_pag_node(
        kg, "pag_vocal_aggression",
        (brain_kg_node_type_t)PAG_KG_NODE_VOCALIZATION,
        "Aggressive vocalization - threat displays, growling, "
        "territorial defense, dlPAG/lPAG activation",
        admin_token
    );
    if (state->aggression_vocal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->vocal_subsystem_id, state->aggression_vocal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains aggression vocalization", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create submission vocalization */
    state->submission_vocal_id = create_pag_node(
        kg, "pag_vocal_submission",
        (brain_kg_node_type_t)PAG_KG_NODE_VOCALIZATION,
        "Submissive vocalization - appeasement calls, "
        "conflict de-escalation, vlPAG mediated",
        admin_token
    );
    if (state->submission_vocal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->vocal_subsystem_id, state->submission_vocal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains submission vocalization", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create distress vocalization */
    state->distress_vocal_id = create_pag_node(
        kg, "pag_vocal_distress",
        (brain_kg_node_type_t)PAG_KG_NODE_VOCALIZATION,
        "Distress vocalization - pain cries, separation calls, "
        "help-seeking, panic-induced",
        admin_token
    );
    if (state->distress_vocal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->vocal_subsystem_id, state->distress_vocal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains distress vocalization", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create pleasure vocalization */
    state->pleasure_vocal_id = create_pag_node(
        kg, "pag_vocal_pleasure",
        (brain_kg_node_type_t)PAG_KG_NODE_VOCALIZATION,
        "Pleasure vocalization - affiliative calls, purring, "
        "social bonding, positive affect",
        admin_token
    );
    if (state->pleasure_vocal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->vocal_subsystem_id, state->pleasure_vocal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains pleasure vocalization", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create startle vocalization */
    state->startle_vocal_id = create_pag_node(
        kg, "pag_vocal_startle",
        (brain_kg_node_type_t)PAG_KG_NODE_VOCALIZATION,
        "Startle vocalization - sudden surprise, reflexive cry, "
        "acoustic startle reflex",
        admin_token
    );
    if (state->startle_vocal_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->vocal_subsystem_id, state->startle_vocal_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains startle vocalization", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int pag_kg_register_autonomic(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    pag_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_register_autonomic: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Create autonomic subsystem node */
    state->autonomic_subsystem_id = create_pag_node(
        kg, PAG_KG_AUTONOMIC_NAME,
        BRAIN_KG_NODE_BRAINSTEM,
        "PAG autonomic output system - cardiovascular and respiratory "
        "control for survival behaviors",
        admin_token
    );
    if (state->autonomic_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_kg_register_autonomic: validation failed");
        return -1;
    }
    state->node_count++;

    /* Link to parent */
    if (parent_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, parent_id, state->autonomic_subsystem_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains autonomic subsystem", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create cardiovascular output */
    state->cardiovascular_id = create_pag_node(
        kg, "pag_cardiovascular",
        (brain_kg_node_type_t)PAG_KG_NODE_AUTONOMIC_OUTPUT,
        "Cardiovascular output - heart rate, blood pressure, "
        "vasoconstriction control, sympathetic/parasympathetic balance",
        admin_token
    );
    if (state->cardiovascular_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->autonomic_subsystem_id, state->cardiovascular_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains cardiovascular output", 1.0f, admin_token);
        state->edge_count++;
    }

    /* Create respiratory output */
    state->respiratory_id = create_pag_node(
        kg, "pag_respiratory",
        (brain_kg_node_type_t)PAG_KG_NODE_AUTONOMIC_OUTPUT,
        "Respiratory output - breathing rate, depth, apnea control, "
        "coordination with vocalization",
        admin_token
    );
    if (state->respiratory_id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        create_pag_edge(kg, state->autonomic_subsystem_id, state->respiratory_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "contains respiratory output", 1.0f, admin_token);
        state->edge_count++;
    }

    return 0;
}

int pag_kg_register_cross_edges(
    brain_kg_t* kg,
    pag_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_register_cross_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* ===== Column -> Defense edges ===== */

    /* dlPAG activates fight/flight */
    if (state->dorsolateral_id != BRAIN_KG_INVALID_NODE &&
        state->fight_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->dorsolateral_id, state->fight_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_ACTIVATES,
            "dlPAG activates fight response", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->dorsolateral_id != BRAIN_KG_INVALID_NODE &&
        state->flight_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->dorsolateral_id, state->flight_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_ACTIVATES,
            "dlPAG activates flight response", 0.9f, admin_token);
        state->edge_count++;
    }

    /* vlPAG activates freeze/fawn */
    if (state->ventrolateral_id != BRAIN_KG_INVALID_NODE &&
        state->freeze_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->ventrolateral_id, state->freeze_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_ACTIVATES,
            "vlPAG activates freeze response", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->ventrolateral_id != BRAIN_KG_INVALID_NODE &&
        state->fawn_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->ventrolateral_id, state->fawn_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_ACTIVATES,
            "vlPAG activates fawn response", 0.8f, admin_token);
        state->edge_count++;
    }

    /* ===== Column -> Coping strategy edges ===== */

    if (state->dorsolateral_id != BRAIN_KG_INVALID_NODE &&
        state->active_coping_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->dorsolateral_id, state->active_coping_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_CONTROLS,
            "dlPAG controls active coping", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->ventrolateral_id != BRAIN_KG_INVALID_NODE &&
        state->passive_coping_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->ventrolateral_id, state->passive_coping_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_CONTROLS,
            "vlPAG controls passive coping", 0.9f, admin_token);
        state->edge_count++;
    }

    /* ===== Column -> Pain pathway edges ===== */

    if (state->ventrolateral_id != BRAIN_KG_INVALID_NODE &&
        state->opioid_pathway_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->ventrolateral_id, state->opioid_pathway_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_ACTIVATES,
            "vlPAG activates opioid analgesia", 0.85f, admin_token);
        state->edge_count++;
    }

    if (state->dorsolateral_id != BRAIN_KG_INVALID_NODE &&
        state->non_opioid_pathway_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->dorsolateral_id, state->non_opioid_pathway_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_ACTIVATES,
            "dlPAG activates non-opioid analgesia", 0.8f, admin_token);
        state->edge_count++;
    }

    /* ===== Emotion -> Column edges ===== */

    if (state->fear_id != BRAIN_KG_INVALID_NODE &&
        state->dorsolateral_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->fear_id, state->dorsolateral_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_MODULATES,
            "fear modulates dlPAG for flight", 0.8f, admin_token);
        state->edge_count++;
    }

    if (state->fear_id != BRAIN_KG_INVALID_NODE &&
        state->ventrolateral_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->fear_id, state->ventrolateral_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_MODULATES,
            "fear modulates vlPAG for freeze", 0.7f, admin_token);
        state->edge_count++;
    }

    if (state->rage_id != BRAIN_KG_INVALID_NODE &&
        state->dorsolateral_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->rage_id, state->dorsolateral_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_MODULATES,
            "rage modulates dlPAG for fight", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->panic_id != BRAIN_KG_INVALID_NODE &&
        state->dorsolateral_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->panic_id, state->dorsolateral_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_MODULATES,
            "panic modulates dlPAG for escape", 0.85f, admin_token);
        state->edge_count++;
    }

    /* ===== Emotion -> Vocalization edges ===== */

    if (state->rage_id != BRAIN_KG_INVALID_NODE &&
        state->aggression_vocal_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->rage_id, state->aggression_vocal_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_ELICITS,
            "rage elicits aggressive vocalization", 0.85f, admin_token);
        state->edge_count++;
    }

    if (state->fear_id != BRAIN_KG_INVALID_NODE &&
        state->alarm_vocal_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->fear_id, state->alarm_vocal_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_ELICITS,
            "fear elicits alarm vocalization", 0.8f, admin_token);
        state->edge_count++;
    }

    if (state->panic_id != BRAIN_KG_INVALID_NODE &&
        state->distress_vocal_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->panic_id, state->distress_vocal_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_ELICITS,
            "panic elicits distress vocalization", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->pain_emotion_id != BRAIN_KG_INVALID_NODE &&
        state->distress_vocal_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->pain_emotion_id, state->distress_vocal_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_ELICITS,
            "pain affect elicits distress vocalization", 0.85f, admin_token);
        state->edge_count++;
    }

    /* ===== Defense -> Autonomic edges ===== */

    if (state->fight_id != BRAIN_KG_INVALID_NODE &&
        state->cardiovascular_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->fight_id, state->cardiovascular_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_TRIGGERS,
            "fight triggers tachycardia, hypertension", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->flight_id != BRAIN_KG_INVALID_NODE &&
        state->cardiovascular_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->flight_id, state->cardiovascular_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_TRIGGERS,
            "flight triggers tachycardia, vasoconstriction", 0.9f, admin_token);
        state->edge_count++;
    }

    if (state->freeze_id != BRAIN_KG_INVALID_NODE &&
        state->cardiovascular_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->freeze_id, state->cardiovascular_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_TRIGGERS,
            "freeze triggers bradycardia, hypotension", 0.85f, admin_token);
        state->edge_count++;
    }

    if (state->freeze_id != BRAIN_KG_INVALID_NODE &&
        state->respiratory_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->freeze_id, state->respiratory_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_TRIGGERS,
            "freeze triggers breath-holding, apnea", 0.8f, admin_token);
        state->edge_count++;
    }

    /* ===== Pain -> Analgesia edges ===== */

    if (state->pain_emotion_id != BRAIN_KG_INVALID_NODE &&
        state->analgesia_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->pain_emotion_id, state->analgesia_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_INDUCES,
            "pain stress induces analgesia", 0.7f, admin_token);
        state->edge_count++;
    }

    /* ===== Vocalization -> Respiratory edges ===== */

    if (state->vocal_subsystem_id != BRAIN_KG_INVALID_NODE &&
        state->respiratory_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->vocal_subsystem_id, state->respiratory_id,
            BRAIN_KG_EDGE_COORDINATES_WITH,
            "vocalization coordinates with respiration", 0.9f, admin_token);
        state->edge_count++;
    }

    /* ===== lPAG -> Vocalization edges ===== */

    if (state->lateral_id != BRAIN_KG_INVALID_NODE &&
        state->vocal_subsystem_id != BRAIN_KG_INVALID_NODE) {
        create_pag_edge(kg, state->lateral_id, state->vocal_subsystem_id,
            (brain_kg_edge_type_t)PAG_KG_EDGE_ACTIVATES,
            "lPAG activates vocalization system", 0.9f, admin_token);
        state->edge_count++;
    }

    return 0;
}

//=============================================================================
// State Synchronization API
//=============================================================================

int pag_kg_update_state(
    brain_kg_t* kg,
    const pag_kg_state_t* state,
    const float* column_activities,
    int defense_active,
    float analgesia_level,
    int dominant_emotion,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_update_state: required parameter is NULL (kg, state)");
        return -1;
    }

    char value_str[NIMCP_ID_BUFFER_SIZE];

    /* Update column activity metadata */
    if (column_activities) {
        if (state->dorsolateral_id != BRAIN_KG_INVALID_NODE) {
            snprintf(value_str, sizeof(value_str), "%.2f", column_activities[0]);
            brain_kg_add_metadata(kg, state->dorsolateral_id, "activity", value_str);
        }
        if (state->lateral_id != BRAIN_KG_INVALID_NODE) {
            snprintf(value_str, sizeof(value_str), "%.2f", column_activities[1]);
            brain_kg_add_metadata(kg, state->lateral_id, "activity", value_str);
        }
        if (state->dorsomedial_id != BRAIN_KG_INVALID_NODE) {
            snprintf(value_str, sizeof(value_str), "%.2f", column_activities[2]);
            brain_kg_add_metadata(kg, state->dorsomedial_id, "activity", value_str);
        }
        if (state->ventrolateral_id != BRAIN_KG_INVALID_NODE) {
            snprintf(value_str, sizeof(value_str), "%.2f", column_activities[3]);
            brain_kg_add_metadata(kg, state->ventrolateral_id, "activity", value_str);
        }
    }

    /* Update defense state metadata */
    if (state->defense_subsystem_id != BRAIN_KG_INVALID_NODE) {
        const char* defense_names[] = {"fight", "flight", "freeze", "fawn", "none"};
        int idx = (defense_active >= 0 && defense_active < 4) ? defense_active : 4;
        brain_kg_add_metadata(kg, state->defense_subsystem_id, "active_defense", defense_names[idx]);
    }

    /* Update analgesia metadata */
    if (state->analgesia_id != BRAIN_KG_INVALID_NODE) {
        snprintf(value_str, sizeof(value_str), "%.1f%%", analgesia_level * 100.0f);
        brain_kg_add_metadata(kg, state->analgesia_id, "level", value_str);
    }

    /* Update dominant emotion metadata */
    if (state->emotion_subsystem_id != BRAIN_KG_INVALID_NODE) {
        const char* emotion_names[] = {"fear", "rage", "pain", "panic", "maternal", "reproductive", "none"};
        int idx = (dominant_emotion >= 0 && dominant_emotion < 6) ? dominant_emotion : 6;
        brain_kg_add_metadata(kg, state->emotion_subsystem_id, "dominant", emotion_names[idx]);
    }

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t pag_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, PAG_KG_ROOT_NAME);
}

brain_kg_node_id_t pag_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

brain_kg_node_list_t* pag_kg_get_columns(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)PAG_KG_NODE_COLUMN);
}

brain_kg_node_list_t* pag_kg_get_pain_pathways(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)PAG_KG_NODE_PAIN_PATHWAY);
}

brain_kg_node_list_t* pag_kg_get_defense_states(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)PAG_KG_NODE_DEFENSE_STATE);
}

brain_kg_node_list_t* pag_kg_get_emotions(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)PAG_KG_NODE_EMOTION_STATE);
}

brain_kg_node_list_t* pag_kg_get_vocalizations(brain_kg_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return brain_kg_get_nodes_by_type(kg, (brain_kg_node_type_t)PAG_KG_NODE_VOCALIZATION);
}

brain_kg_node_id_t pag_kg_get_column_for_defense(
    brain_kg_t* kg,
    brain_kg_node_id_t defense_id
) {
    if (!kg || defense_id == BRAIN_KG_INVALID_NODE) return BRAIN_KG_INVALID_NODE;

    /* Get incoming edges to the defense node */
    brain_kg_edge_list_t* edges = brain_kg_get_incoming(kg, defense_id);
    if (!edges) return BRAIN_KG_INVALID_NODE;

    brain_kg_node_id_t column_id = BRAIN_KG_INVALID_NODE;

    /* Find edge with ACTIVATES type from a column node */
    for (uint32_t i = 0; i < edges->count; i++) {
        const brain_kg_edge_t* edge = edges->edges[i];
        if (edge && edge->type == (brain_kg_edge_type_t)PAG_KG_EDGE_ACTIVATES) {
            /* Check if source is a column node */
            const brain_kg_node_t* source = brain_kg_get_node(kg, edge->from);
            if (source && source->type == (brain_kg_node_type_t)PAG_KG_NODE_COLUMN) {
                column_id = edge->from;
                break;
            }
        }
    }

    brain_kg_edge_list_destroy(edges);
    return column_id;
}

int pag_kg_unregister_all(
    brain_kg_t* kg,
    pag_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }
    (void)admin_token;

    /* Mark as unregistered */
    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(PAG_KG_MODULE_NAME, "Unregistered PAG KG nodes");

    return 0;
}
