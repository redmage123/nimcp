//=============================================================================
// nimcp_insula_kg_wiring.h - Insula Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_insula_kg_wiring.h
 * @brief Knowledge Graph registration for Insula module
 *
 * WHAT: Registers Insula concepts (interoception, emotion-cognition integration,
 *       pain processing) as nodes in the brain's internal KG.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about interoceptive awareness state
 *       - Cross-module reasoning about emotion-body-cognition loops
 *       - Introspection of pain and emotion awareness systems
 *
 * HOW:  Creates hierarchical node structure:
 *       - Insula root node
 *         +-- Anterior insula
 *         +-- Posterior insula
 *         +-- Interoception
 *         +-- Pain processing
 *         +-- Emotion awareness
 *
 * @author NIMCP Development Team
 * @date 2026-02-04
 * @version 1.0.0
 */

#ifndef NIMCP_INSULA_KG_WIRING_H
#define NIMCP_INSULA_KG_WIRING_H

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

#define INSULA_KG_MODULE_NAME    "insula_kg_wiring"
#define INSULA_KG_ROOT_NAME      "insula"

//=============================================================================
// Node Type Extensions (Insula-specific, starting at 0x2600)
//=============================================================================

typedef enum {
    INSULA_KG_NODE_ANTERIOR = 0x2600,
    INSULA_KG_NODE_POSTERIOR,
    INSULA_KG_NODE_INTEROCEPTIVE,
    INSULA_KG_NODE_PAIN,
    INSULA_KG_NODE_EMOTION_AWARENESS
} insula_kg_node_type_t;

typedef enum {
    INSULA_KG_EDGE_SENSES = 0x2600,
    INSULA_KG_EDGE_INTEGRATES,
    INSULA_KG_EDGE_SIGNALS,
    INSULA_KG_EDGE_REGULATES
} insula_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    bool register_anterior_insula;
    bool register_posterior_insula;
    bool register_interoception;
    bool register_pain_processing;
    bool register_emotion_awareness;
    bool register_cross_edges;
    bool include_state_metadata;
} insula_kg_config_t;

typedef struct {
    brain_kg_node_id_t root_id;
    brain_kg_node_id_t anterior_insula_id;
    brain_kg_node_id_t posterior_insula_id;
    brain_kg_node_id_t interoception_id;
    brain_kg_node_id_t pain_processing_id;
    brain_kg_node_id_t emotion_awareness_id;
    uint32_t node_count;
    uint32_t edge_count;
    bool registered;
} insula_kg_state_t;

//=============================================================================
// API
//=============================================================================

NIMCP_EXPORT int insula_kg_default_config(insula_kg_config_t* config);

NIMCP_EXPORT int insula_kg_register_all(
    brain_kg_t* kg,
    const insula_kg_config_t* config,
    insula_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT int insula_kg_unregister_all(
    brain_kg_t* kg,
    insula_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT brain_kg_node_id_t insula_kg_get_root(brain_kg_t* kg);

NIMCP_EXPORT brain_kg_node_id_t insula_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/* Forward decl for runtime event emit API (W2). */
struct brain_struct;

/**
 * @brief Emit a runtime insula event into the brain's internal KG
 *
 * Supported kinds: "salience". Silent no-op if brain/KG unavailable.
 * Creates `insula_event_<kind>_<ts_us>` node + edge to `insula`.
 */
NIMCP_EXPORT void insula_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INSULA_KG_WIRING_H */
