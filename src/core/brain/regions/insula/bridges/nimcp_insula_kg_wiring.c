//=============================================================================
// nimcp_insula_kg_wiring.c - Insula Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_insula_kg_wiring.c
 * @brief Implementation of Insula Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for Insula module
 * WHY:  Enables semantic queries about interoception and emotion-cognition
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/insula/bridges/nimcp_insula_kg_wiring.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(insula_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Helper Functions
//=============================================================================

static brain_kg_node_id_t create_insula_node(
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
        NIMCP_LOG_DEBUG(INSULA_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

static brain_kg_edge_id_t create_insula_edge(
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

int insula_kg_default_config(insula_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_kg_default_config: config is NULL");
        return -1;
    }

    config->register_anterior_insula = true;
    config->register_posterior_insula = true;
    config->register_interoception = true;
    config->register_pain_processing = true;
    config->register_emotion_awareness = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int insula_kg_register_all(
    brain_kg_t* kg,
    const insula_kg_config_t* config,
    insula_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_kg_register_all: kg is NULL");
        return -1;
    }

    insula_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        insula_kg_default_config(&local_config);
    }

    insula_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create root node */
    local_state.root_id = create_insula_node(
        kg, INSULA_KG_ROOT_NAME,
        BRAIN_KG_NODE_CORTICAL,
        "Insula - interoception, emotion awareness, pain processing, self-awareness",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(INSULA_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insula_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Anterior insula */
    if (local_config.register_anterior_insula) {
        local_state.anterior_insula_id = create_insula_node(
            kg, "anterior_insula",
            (brain_kg_node_type_t)INSULA_KG_NODE_ANTERIOR,
            "Anterior insula - subjective feeling, awareness, salience detection",
            admin_token
        );
        if (local_state.anterior_insula_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_insula_edge(kg, local_state.root_id, local_state.anterior_insula_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains anterior insula", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Posterior insula */
    if (local_config.register_posterior_insula) {
        local_state.posterior_insula_id = create_insula_node(
            kg, "posterior_insula",
            (brain_kg_node_type_t)INSULA_KG_NODE_POSTERIOR,
            "Posterior insula - primary interoceptive cortex, body-state representation",
            admin_token
        );
        if (local_state.posterior_insula_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_insula_edge(kg, local_state.root_id, local_state.posterior_insula_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains posterior insula", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Interoception */
    if (local_config.register_interoception) {
        local_state.interoception_id = create_insula_node(
            kg, "interoception",
            (brain_kg_node_type_t)INSULA_KG_NODE_INTEROCEPTIVE,
            "Interoception - internal body state sensing, visceral awareness",
            admin_token
        );
        if (local_state.interoception_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_insula_edge(kg, local_state.root_id, local_state.interoception_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains interoception", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Pain processing */
    if (local_config.register_pain_processing) {
        local_state.pain_processing_id = create_insula_node(
            kg, "pain_processing",
            (brain_kg_node_type_t)INSULA_KG_NODE_PAIN,
            "Pain processing - nociceptive integration, pain awareness",
            admin_token
        );
        if (local_state.pain_processing_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_insula_edge(kg, local_state.root_id, local_state.pain_processing_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains pain processing", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Emotion awareness */
    if (local_config.register_emotion_awareness) {
        local_state.emotion_awareness_id = create_insula_node(
            kg, "emotion_awareness",
            (brain_kg_node_type_t)INSULA_KG_NODE_EMOTION_AWARENESS,
            "Emotion awareness - conscious feeling states, emotional self-awareness",
            admin_token
        );
        if (local_state.emotion_awareness_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_insula_edge(kg, local_state.root_id, local_state.emotion_awareness_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains emotion awareness", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Cross edges */
    if (local_config.register_cross_edges) {
        /* Posterior insula senses body state -> interoception */
        if (local_state.posterior_insula_id != BRAIN_KG_INVALID_NODE &&
            local_state.interoception_id != BRAIN_KG_INVALID_NODE) {
            create_insula_edge(kg, local_state.posterior_insula_id,
                local_state.interoception_id,
                (brain_kg_edge_type_t)INSULA_KG_EDGE_SENSES,
                "posterior insula provides interoceptive signals", 0.9f, admin_token);
            local_state.edge_count++;
        }
        /* Interoception -> anterior insula for awareness */
        if (local_state.interoception_id != BRAIN_KG_INVALID_NODE &&
            local_state.anterior_insula_id != BRAIN_KG_INVALID_NODE) {
            create_insula_edge(kg, local_state.interoception_id,
                local_state.anterior_insula_id,
                BRAIN_KG_EDGE_SENDS_TO,
                "interoceptive signals reach conscious awareness", 0.85f, admin_token);
            local_state.edge_count++;
        }
        /* Anterior insula integrates emotion awareness */
        if (local_state.anterior_insula_id != BRAIN_KG_INVALID_NODE &&
            local_state.emotion_awareness_id != BRAIN_KG_INVALID_NODE) {
            create_insula_edge(kg, local_state.anterior_insula_id,
                local_state.emotion_awareness_id,
                (brain_kg_edge_type_t)INSULA_KG_EDGE_INTEGRATES,
                "anterior insula integrates emotional awareness", 0.85f, admin_token);
            local_state.edge_count++;
        }
        /* Pain processing -> posterior insula */
        if (local_state.pain_processing_id != BRAIN_KG_INVALID_NODE &&
            local_state.posterior_insula_id != BRAIN_KG_INVALID_NODE) {
            create_insula_edge(kg, local_state.pain_processing_id,
                local_state.posterior_insula_id,
                (brain_kg_edge_type_t)INSULA_KG_EDGE_SIGNALS,
                "pain signals processed by posterior insula", 0.9f, admin_token);
            local_state.edge_count++;
        }
    }

    local_state.registered = true;
    if (state) *state = local_state;

    NIMCP_LOG_INFO(INSULA_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t insula_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, INSULA_KG_ROOT_NAME);
}

brain_kg_node_id_t insula_kg_find_subsystem(brain_kg_t* kg, const char* name) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

int insula_kg_unregister_all(
    brain_kg_t* kg,
    insula_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(INSULA_KG_MODULE_NAME, "Unregistered Insula KG nodes");
    return 0;
}

//=============================================================================
// Runtime Event Emission (Wave W2)
//=============================================================================

/*
 * HOT-PATH GAP: Insula has no dedicated tick .c file in this tree.
 * Salience-network detections should invoke insula_kg_emit_event.
 */

void insula_kg_emit_event(
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
             "insula_event_%s_%" PRIu64, kind, ts_us);
    snprintf(desc, sizeof(desc),
             "insula runtime event: kind=%s intensity=%.3f ts_us=%" PRIu64,
             kind, (double)intensity, ts_us);

    brain_kg_node_id_t evt_id = brain_kg_add_node(
        kg, name, BRAIN_KG_NODE_CUSTOM, desc
    );

    if (evt_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.4f", (double)intensity);
        brain_kg_add_metadata(kg, evt_id, "intensity", val_str);
        brain_kg_add_metadata(kg, evt_id, "kind", kind);

        brain_kg_node_id_t root = brain_kg_find_node(kg, INSULA_KG_ROOT_NAME);
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
