//=============================================================================
// nimcp_wernicke_kg_wiring.c - Wernicke Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_wernicke_kg_wiring.c
 * @brief Implementation of Wernicke Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for Wernicke's area module
 * WHY:  Enables semantic queries about language comprehension
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/wernicke/bridges/nimcp_wernicke_kg_wiring.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(wernicke_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Helper Functions
//=============================================================================

static brain_kg_node_id_t create_wernicke_node(
    brain_kg_t* kg,
    const char* name,
    brain_kg_node_type_t type,
    const char* description,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;

    brain_kg_node_id_t id = brain_kg_add_node(kg, name, type, description);
    if (id != BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_DEBUG(WERNICKE_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

static brain_kg_edge_id_t create_wernicke_edge(
    brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    brain_kg_edge_type_t type,
    const char* description,
    float weight,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg) return BRAIN_KG_INVALID_NODE;
    if (from == BRAIN_KG_INVALID_NODE || to == BRAIN_KG_INVALID_NODE) {
        return BRAIN_KG_INVALID_NODE;
    }
    return brain_kg_add_edge(kg, from, to, type, description, weight);
}

//=============================================================================
// Configuration API
//=============================================================================

int wernicke_kg_default_config(wernicke_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_kg_default_config: config is NULL");
        return -1;
    }

    config->register_auditory_cortex = true;
    config->register_phonological = true;
    config->register_semantic = true;
    config->register_syntax = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int wernicke_kg_register_all(
    brain_kg_t* kg,
    const wernicke_kg_config_t* config,
    wernicke_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_kg_register_all: kg is NULL");
        return -1;
    }

    wernicke_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        wernicke_kg_default_config(&local_config);
    }

    wernicke_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create root node */
    local_state.root_id = create_wernicke_node(
        kg, WERNICKE_KG_ROOT_NAME,
        BRAIN_KG_NODE_CORTICAL,
        "Wernicke's area - language comprehension, phonological and semantic processing",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(WERNICKE_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Auditory cortex */
    if (local_config.register_auditory_cortex) {
        local_state.auditory_cortex_id = create_wernicke_node(
            kg, "auditory_cortex",
            (brain_kg_node_type_t)WERNICKE_KG_NODE_AUDITORY_AREA,
            "Auditory cortex - primary sound processing, tonotopic mapping",
            admin_token
        );
        if (local_state.auditory_cortex_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_wernicke_edge(kg, local_state.root_id, local_state.auditory_cortex_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains auditory cortex", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Phonological processing */
    if (local_config.register_phonological) {
        local_state.phonological_processing_id = create_wernicke_node(
            kg, "phonological_processing",
            (brain_kg_node_type_t)WERNICKE_KG_NODE_PHONOLOGICAL,
            "Phonological processing - phoneme recognition, sound-to-meaning mapping",
            admin_token
        );
        if (local_state.phonological_processing_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_wernicke_edge(kg, local_state.root_id, local_state.phonological_processing_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains phonological processing", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Semantic processing */
    if (local_config.register_semantic) {
        local_state.semantic_processing_id = create_wernicke_node(
            kg, "semantic_processing",
            (brain_kg_node_type_t)WERNICKE_KG_NODE_SEMANTIC,
            "Semantic processing - word meaning extraction, conceptual retrieval",
            admin_token
        );
        if (local_state.semantic_processing_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_wernicke_edge(kg, local_state.root_id, local_state.semantic_processing_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains semantic processing", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Syntax comprehension */
    if (local_config.register_syntax) {
        local_state.syntax_comprehension_id = create_wernicke_node(
            kg, "syntax_comprehension",
            (brain_kg_node_type_t)WERNICKE_KG_NODE_SYNTAX,
            "Syntax comprehension - grammatical structure parsing, sentence-level understanding",
            admin_token
        );
        if (local_state.syntax_comprehension_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_wernicke_edge(kg, local_state.root_id, local_state.syntax_comprehension_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains syntax comprehension", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Cross edges: auditory -> phonological -> semantic -> syntax */
    if (local_config.register_cross_edges) {
        if (local_state.auditory_cortex_id != BRAIN_KG_INVALID_NODE &&
            local_state.phonological_processing_id != BRAIN_KG_INVALID_NODE) {
            create_wernicke_edge(kg, local_state.auditory_cortex_id,
                local_state.phonological_processing_id,
                BRAIN_KG_EDGE_SENDS_TO,
                "auditory input feeds phonological analysis", 0.9f, admin_token);
            local_state.edge_count++;
        }
        if (local_state.phonological_processing_id != BRAIN_KG_INVALID_NODE &&
            local_state.semantic_processing_id != BRAIN_KG_INVALID_NODE) {
            create_wernicke_edge(kg, local_state.phonological_processing_id,
                local_state.semantic_processing_id,
                (brain_kg_edge_type_t)WERNICKE_KG_EDGE_PROCESSES,
                "phonological output drives semantic retrieval", 0.85f, admin_token);
            local_state.edge_count++;
        }
        if (local_state.semantic_processing_id != BRAIN_KG_INVALID_NODE &&
            local_state.syntax_comprehension_id != BRAIN_KG_INVALID_NODE) {
            create_wernicke_edge(kg, local_state.semantic_processing_id,
                local_state.syntax_comprehension_id,
                BRAIN_KG_EDGE_INTEGRATES_WITH,
                "semantic meaning integrates with syntactic structure", 0.8f, admin_token);
            local_state.edge_count++;
        }
    }

    local_state.registered = true;
    if (state) *state = local_state;

    NIMCP_LOG_INFO(WERNICKE_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t wernicke_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, WERNICKE_KG_ROOT_NAME);
}

brain_kg_node_id_t wernicke_kg_find_subsystem(brain_kg_t* kg, const char* name) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

int wernicke_kg_unregister_all(
    brain_kg_t* kg,
    wernicke_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(WERNICKE_KG_MODULE_NAME, "Unregistered Wernicke KG nodes");
    return 0;
}
