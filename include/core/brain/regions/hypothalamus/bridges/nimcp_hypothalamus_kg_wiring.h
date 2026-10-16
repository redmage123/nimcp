//=============================================================================
// nimcp_hypothalamus_kg_wiring.h - Hypothalamus Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_hypothalamus_kg_wiring.h
 * @brief Knowledge Graph registration for Hypothalamus module
 *
 * WHAT: Registers Hypothalamus concepts (homeostasis, circadian rhythm,
 *       HPA stress response) as nodes in the brain's internal KG.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about homeostatic state
 *       - Cross-module reasoning about circadian and stress regulation
 *       - Introspection of neuroendocrine systems
 *
 * HOW:  Creates hierarchical node structure:
 *       - Hypothalamus root node
 *         +-- SCN (suprachiasmatic nucleus)
 *         +-- PVN (paraventricular nucleus)
 *         +-- VMH (ventromedial hypothalamus)
 *         +-- LH (lateral hypothalamus)
 *         +-- Homeostasis
 *         +-- Circadian rhythm
 *         +-- Stress response
 *
 * @author NIMCP Development Team
 * @date 2026-02-04
 * @version 1.0.0
 */

#ifndef NIMCP_HYPOTHALAMUS_KG_WIRING_H
#define NIMCP_HYPOTHALAMUS_KG_WIRING_H

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

#define HYPOTHALAMUS_KG_MODULE_NAME    "hypothalamus_kg_wiring"
#define HYPOTHALAMUS_KG_ROOT_NAME      "hypothalamus"

//=============================================================================
// Node Type Extensions (Hypothalamus-specific, starting at 0x2900)
//=============================================================================

typedef enum {
    HYPOTHALAMUS_KG_NODE_SCN = 0x2900,
    HYPOTHALAMUS_KG_NODE_PVN,
    HYPOTHALAMUS_KG_NODE_VMH,
    HYPOTHALAMUS_KG_NODE_LH,
    HYPOTHALAMUS_KG_NODE_HOMEOSTASIS,
    HYPOTHALAMUS_KG_NODE_CIRCADIAN,
    HYPOTHALAMUS_KG_NODE_STRESS
} hypothalamus_kg_node_type_t;

typedef enum {
    HYPOTHALAMUS_KG_EDGE_REGULATES = 0x2900,
    HYPOTHALAMUS_KG_EDGE_ENTRAINS,
    HYPOTHALAMUS_KG_EDGE_ACTIVATES_HPA,
    HYPOTHALAMUS_KG_EDGE_DRIVES
} hypothalamus_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    bool register_scn;
    bool register_pvn;
    bool register_vmh;
    bool register_lh;
    bool register_homeostasis;
    bool register_circadian_rhythm;
    bool register_stress_response;
    bool register_cross_edges;
    bool include_state_metadata;
} hypothalamus_kg_config_t;

typedef struct {
    brain_kg_node_id_t root_id;
    brain_kg_node_id_t scn_id;
    brain_kg_node_id_t pvn_id;
    brain_kg_node_id_t vmh_id;
    brain_kg_node_id_t lh_id;
    brain_kg_node_id_t homeostasis_id;
    brain_kg_node_id_t circadian_rhythm_id;
    brain_kg_node_id_t stress_response_id;
    uint32_t node_count;
    uint32_t edge_count;
    bool registered;
} hypothalamus_kg_state_t;

//=============================================================================
// API
//=============================================================================

NIMCP_EXPORT int hypothalamus_kg_default_config(hypothalamus_kg_config_t* config);

NIMCP_EXPORT int hypothalamus_kg_register_all(
    brain_kg_t* kg,
    const hypothalamus_kg_config_t* config,
    hypothalamus_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT int hypothalamus_kg_unregister_all(
    brain_kg_t* kg,
    hypothalamus_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT brain_kg_node_id_t hypothalamus_kg_get_root(brain_kg_t* kg);

NIMCP_EXPORT brain_kg_node_id_t hypothalamus_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/* Forward decl for runtime event emit API (W2). */
struct brain_struct;

/**
 * @brief Emit a runtime hypothalamus event into the brain's internal KG
 *
 * Supported kinds: "drive_change". Silent no-op if brain/KG unavailable.
 * Creates `hypothalamus_event_<kind>_<ts_us>` node + edge to `hypothalamus`.
 */
NIMCP_EXPORT void hypothalamus_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_KG_WIRING_H */
