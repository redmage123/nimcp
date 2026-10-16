//=============================================================================
// nimcp_temporal_kg_wiring.c - Temporal Lobe Knowledge Graph Registration Implementation
//=============================================================================
/**
 * @file nimcp_temporal_kg_wiring.c
 * @brief Implementation of Temporal Lobe Knowledge Graph registration
 *
 * WHAT: Implements KG node/edge creation for Temporal Lobe module
 * WHY:  Enables semantic queries about auditory and recognition processing
 * HOW:  Creates hierarchical node structure with typed relationships
 */

#include "core/brain/regions/temporal/bridges/nimcp_temporal_kg_wiring.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(temporal_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Helper Functions
//=============================================================================

static brain_kg_node_id_t create_temporal_node(
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
        NIMCP_LOG_DEBUG(TEMPORAL_KG_MODULE_NAME,
            "Created node '%s' (id=%u)", name, id);
    }
    return id;
}

static brain_kg_edge_id_t create_temporal_edge(
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

int temporal_kg_default_config(temporal_kg_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_kg_default_config: config is NULL");
        return -1;
    }

    config->register_auditory_processing = true;
    config->register_object_recognition = true;
    config->register_face_processing = true;
    config->register_memory_encoding = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int temporal_kg_register_all(
    brain_kg_t* kg,
    const temporal_kg_config_t* config,
    temporal_kg_state_t* state,
    uint64_t admin_token
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_kg_register_all: kg is NULL");
        return -1;
    }

    temporal_kg_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        temporal_kg_default_config(&local_config);
    }

    temporal_kg_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create root node */
    local_state.root_id = create_temporal_node(
        kg, TEMPORAL_KG_ROOT_NAME,
        BRAIN_KG_NODE_CORTICAL,
        "Temporal lobe - auditory processing, object recognition, memory encoding",
        admin_token
    );
    if (local_state.root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOG_ERROR(TEMPORAL_KG_MODULE_NAME, "Failed to create root node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_kg_register_all: validation failed");
        return -1;
    }
    local_state.node_count++;

    /* Auditory processing */
    if (local_config.register_auditory_processing) {
        local_state.auditory_processing_id = create_temporal_node(
            kg, "auditory_processing",
            (brain_kg_node_type_t)TEMPORAL_KG_NODE_AUDITORY,
            "Auditory processing - sound analysis, frequency decomposition, temporal patterns",
            admin_token
        );
        if (local_state.auditory_processing_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_temporal_edge(kg, local_state.root_id, local_state.auditory_processing_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains auditory processing", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Object recognition */
    if (local_config.register_object_recognition) {
        local_state.object_recognition_id = create_temporal_node(
            kg, "object_recognition",
            (brain_kg_node_type_t)TEMPORAL_KG_NODE_RECOGNITION,
            "Object recognition - visual object identification, categorization",
            admin_token
        );
        if (local_state.object_recognition_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_temporal_edge(kg, local_state.root_id, local_state.object_recognition_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains object recognition", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Face processing */
    if (local_config.register_face_processing) {
        local_state.face_processing_id = create_temporal_node(
            kg, "face_processing",
            (brain_kg_node_type_t)TEMPORAL_KG_NODE_FACE_AREA,
            "Face processing - fusiform face area, identity recognition, expression analysis",
            admin_token
        );
        if (local_state.face_processing_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_temporal_edge(kg, local_state.root_id, local_state.face_processing_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains face processing", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Memory encoding */
    if (local_config.register_memory_encoding) {
        local_state.memory_encoding_id = create_temporal_node(
            kg, "memory_encoding",
            (brain_kg_node_type_t)TEMPORAL_KG_NODE_MEMORY_ENCODER,
            "Memory encoding - medial temporal lobe encoding, declarative memory formation",
            admin_token
        );
        if (local_state.memory_encoding_id != BRAIN_KG_INVALID_NODE) {
            local_state.node_count++;
            create_temporal_edge(kg, local_state.root_id, local_state.memory_encoding_id,
                BRAIN_KG_EDGE_CONNECTS_TO, "contains memory encoding", 1.0f, admin_token);
            local_state.edge_count++;
        }
    }

    /* Cross edges */
    if (local_config.register_cross_edges) {
        if (local_state.auditory_processing_id != BRAIN_KG_INVALID_NODE &&
            local_state.object_recognition_id != BRAIN_KG_INVALID_NODE) {
            create_temporal_edge(kg, local_state.auditory_processing_id,
                local_state.object_recognition_id,
                BRAIN_KG_EDGE_INTEGRATES_WITH,
                "auditory cues support object recognition", 0.7f, admin_token);
            local_state.edge_count++;
        }
        if (local_state.object_recognition_id != BRAIN_KG_INVALID_NODE &&
            local_state.face_processing_id != BRAIN_KG_INVALID_NODE) {
            create_temporal_edge(kg, local_state.object_recognition_id,
                local_state.face_processing_id,
                (brain_kg_edge_type_t)TEMPORAL_KG_EDGE_RECOGNIZES,
                "object recognition specializes into face processing", 0.85f, admin_token);
            local_state.edge_count++;
        }
        if (local_state.object_recognition_id != BRAIN_KG_INVALID_NODE &&
            local_state.memory_encoding_id != BRAIN_KG_INVALID_NODE) {
            create_temporal_edge(kg, local_state.object_recognition_id,
                local_state.memory_encoding_id,
                (brain_kg_edge_type_t)TEMPORAL_KG_EDGE_ENCODES,
                "recognized objects encoded into memory", 0.8f, admin_token);
            local_state.edge_count++;
        }
        if (local_state.face_processing_id != BRAIN_KG_INVALID_NODE &&
            local_state.memory_encoding_id != BRAIN_KG_INVALID_NODE) {
            create_temporal_edge(kg, local_state.face_processing_id,
                local_state.memory_encoding_id,
                (brain_kg_edge_type_t)TEMPORAL_KG_EDGE_ENCODES,
                "face identities encoded into memory", 0.85f, admin_token);
            local_state.edge_count++;
        }
    }

    local_state.registered = true;
    if (state) *state = local_state;

    NIMCP_LOG_INFO(TEMPORAL_KG_MODULE_NAME,
        "Registered %u nodes, %u edges",
        local_state.node_count, local_state.edge_count);

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t temporal_kg_get_root(brain_kg_t* kg) {
    if (!kg) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, TEMPORAL_KG_ROOT_NAME);
}

brain_kg_node_id_t temporal_kg_find_subsystem(brain_kg_t* kg, const char* name) {
    if (!kg || !name) return BRAIN_KG_INVALID_NODE;
    return brain_kg_find_node(kg, name);
}

int temporal_kg_unregister_all(
    brain_kg_t* kg,
    temporal_kg_state_t* state,
    uint64_t admin_token
) {
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }

    state->registered = false;
    state->node_count = 0;
    state->edge_count = 0;

    NIMCP_LOG_INFO(TEMPORAL_KG_MODULE_NAME, "Unregistered Temporal KG nodes");
    return 0;
}

//=============================================================================
// Runtime Event Emission (Wave W2)
//=============================================================================

/*
 * HOT-PATH GAP: Temporal lobe has an adapter + substrate/thalamic/quantum
 * bridges but no dedicated tick .c file. Semantic-retrieval callers should
 * invoke temporal_kg_emit_event(brain, "semantic_retrieve", match_score, ts).
 */

void temporal_kg_emit_event(
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
             "temporal_event_%s_%" PRIu64, kind, ts_us);
    snprintf(desc, sizeof(desc),
             "temporal runtime event: kind=%s intensity=%.3f ts_us=%" PRIu64,
             kind, (double)intensity, ts_us);

    brain_kg_node_id_t evt_id = brain_kg_add_node(
        kg, name, BRAIN_KG_NODE_CUSTOM, desc
    );

    if (evt_id != BRAIN_KG_INVALID_NODE) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%.4f", (double)intensity);
        brain_kg_add_metadata(kg, evt_id, "intensity", val_str);
        brain_kg_add_metadata(kg, evt_id, "kind", kind);

        brain_kg_node_id_t root = brain_kg_find_node(kg, TEMPORAL_KG_ROOT_NAME);
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
