//=============================================================================
// nimcp_lc_kg_wiring.h - Locus Coeruleus Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_lc_kg_wiring.h
 * @brief Knowledge Graph registration for Locus Coeruleus module
 *
 * WHAT: Registers Locus Coeruleus concepts (noradrenergic arousal,
 *       attention modulation, stress response) as nodes in the brain's
 *       internal KG.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about arousal regulation state
 *       - Cross-module reasoning about attention and alertness
 *       - Introspection of norepinephrine-driven modulation
 *
 * HOW:  Creates hierarchical node structure:
 *       - Locus Coeruleus root node
 *         +-- Noradrenergic neurons
 *         +-- Arousal regulation
 *         +-- Attention modulation
 *         +-- Stress response
 *
 * @author NIMCP Development Team
 * @date 2026-02-04
 * @version 1.0.0
 */

#ifndef NIMCP_LC_KG_WIRING_H
#define NIMCP_LC_KG_WIRING_H

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

#define LC_KG_MODULE_NAME    "lc_kg_wiring"
#define LC_KG_ROOT_NAME      "locus_coeruleus"

//=============================================================================
// Node Type Extensions (LC-specific, starting at 0x2C00)
//=============================================================================

typedef enum {
    LC_KG_NODE_NORADRENERGIC = 0x2C00,
    LC_KG_NODE_AROUSAL,
    LC_KG_NODE_ATTENTION_MOD,
    LC_KG_NODE_STRESS
} lc_kg_node_type_t;

typedef enum {
    LC_KG_EDGE_RELEASES_NE = 0x2C00,
    LC_KG_EDGE_AROUSES,
    LC_KG_EDGE_MODULATES_ATTENTION,
    LC_KG_EDGE_RESPONDS_STRESS
} lc_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    bool register_noradrenergic_neurons;
    bool register_arousal_regulation;
    bool register_attention_modulation;
    bool register_stress_response;
    bool register_cross_edges;
    bool include_state_metadata;
} lc_kg_config_t;

typedef struct {
    brain_kg_node_id_t root_id;
    brain_kg_node_id_t noradrenergic_neurons_id;
    brain_kg_node_id_t arousal_regulation_id;
    brain_kg_node_id_t attention_modulation_id;
    brain_kg_node_id_t stress_response_id;
    uint32_t node_count;
    uint32_t edge_count;
    bool registered;
} lc_kg_state_t;

//=============================================================================
// API
//=============================================================================

NIMCP_EXPORT int lc_kg_default_config(lc_kg_config_t* config);

NIMCP_EXPORT int lc_kg_register_all(
    brain_kg_t* kg,
    const lc_kg_config_t* config,
    lc_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT int lc_kg_unregister_all(
    brain_kg_t* kg,
    lc_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT brain_kg_node_id_t lc_kg_get_root(brain_kg_t* kg);

NIMCP_EXPORT brain_kg_node_id_t lc_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/* Forward decl for runtime event emit API (W2). */
struct brain_struct;

/**
 * @brief Emit a runtime locus coeruleus event into the brain's internal KG
 *
 * Supported kinds: "arousal_shift". Silent no-op if brain/KG unavailable.
 * Creates `locus_coeruleus_event_<kind>_<ts_us>` node + edge to
 * `locus_coeruleus`.
 */
NIMCP_EXPORT void lc_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LC_KG_WIRING_H */
