//=============================================================================
// nimcp_temporal_kg_wiring.h - Temporal Lobe Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_temporal_kg_wiring.h
 * @brief Knowledge Graph registration for Temporal Lobe module
 *
 * WHAT: Registers Temporal Lobe concepts (auditory processing, object/face
 *       recognition, memory encoding) as nodes in the brain's internal KG.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about temporal processing state
 *       - Cross-module reasoning about perception pipelines
 *       - Introspection of recognition and memory encoding systems
 *
 * HOW:  Creates hierarchical node structure:
 *       - Temporal root node
 *         +-- Auditory processing
 *         +-- Object recognition
 *         +-- Face processing
 *         +-- Memory encoding
 *
 * @author NIMCP Development Team
 * @date 2026-02-04
 * @version 1.0.0
 */

#ifndef NIMCP_TEMPORAL_KG_WIRING_H
#define NIMCP_TEMPORAL_KG_WIRING_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_kg.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define TEMPORAL_KG_MODULE_NAME    "temporal_kg_wiring"
#define TEMPORAL_KG_ROOT_NAME      "temporal_lobe"

//=============================================================================
// Node Type Extensions (Temporal-specific, starting at 0x2500)
//=============================================================================

typedef enum {
    TEMPORAL_KG_NODE_AUDITORY = 0x2500,
    TEMPORAL_KG_NODE_RECOGNITION,
    TEMPORAL_KG_NODE_FACE_AREA,
    TEMPORAL_KG_NODE_MEMORY_ENCODER,
    TEMPORAL_KG_NODE_ASSOCIATION_AREA
} temporal_kg_node_type_t;

typedef enum {
    TEMPORAL_KG_EDGE_PROCESSES = 0x2500,
    TEMPORAL_KG_EDGE_RECOGNIZES,
    TEMPORAL_KG_EDGE_ENCODES,
    TEMPORAL_KG_EDGE_ASSOCIATES
} temporal_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    bool register_auditory_processing;
    bool register_object_recognition;
    bool register_face_processing;
    bool register_memory_encoding;
    bool register_cross_edges;
    bool include_state_metadata;
} temporal_kg_config_t;

typedef struct {
    brain_kg_node_id_t root_id;
    brain_kg_node_id_t auditory_processing_id;
    brain_kg_node_id_t object_recognition_id;
    brain_kg_node_id_t face_processing_id;
    brain_kg_node_id_t memory_encoding_id;
    uint32_t node_count;
    uint32_t edge_count;
    bool registered;
} temporal_kg_state_t;

//=============================================================================
// API
//=============================================================================

NIMCP_EXPORT int temporal_kg_default_config(temporal_kg_config_t* config);

NIMCP_EXPORT int temporal_kg_register_all(
    brain_kg_t* kg,
    const temporal_kg_config_t* config,
    temporal_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT int temporal_kg_unregister_all(
    brain_kg_t* kg,
    temporal_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT brain_kg_node_id_t temporal_kg_get_root(brain_kg_t* kg);

NIMCP_EXPORT brain_kg_node_id_t temporal_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/* Forward decl for runtime event emit API (W2). */
struct brain_struct;

/**
 * @brief Emit a runtime temporal-lobe event into the brain's internal KG
 *
 * Supported kinds: "semantic_retrieve". Silent no-op if brain/KG unavailable.
 * Creates `temporal_event_<kind>_<ts_us>` node + edge to `temporal_lobe`.
 */
NIMCP_EXPORT void temporal_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TEMPORAL_KG_WIRING_H */
