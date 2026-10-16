//=============================================================================
// nimcp_cingulate_kg_wiring.h - Cingulate Cortex Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_cingulate_kg_wiring.h
 * @brief Knowledge Graph registration for Cingulate Cortex module
 *
 * WHAT: Registers Cingulate concepts (conflict monitoring, error detection,
 *       reward processing) as nodes in the brain's internal KG.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about conflict detection state
 *       - Cross-module reasoning about error monitoring pipelines
 *       - Introspection of reward evaluation systems
 *
 * HOW:  Creates hierarchical node structure:
 *       - Cingulate root node
 *         +-- dACC (dorsal anterior cingulate)
 *         +-- vACC (ventral anterior cingulate)
 *         +-- PCC (posterior cingulate)
 *         +-- Conflict detection
 *         +-- Error monitoring
 *         +-- Reward processing
 *
 * @author NIMCP Development Team
 * @date 2026-02-04
 * @version 1.0.0
 */

#ifndef NIMCP_CINGULATE_KG_WIRING_H
#define NIMCP_CINGULATE_KG_WIRING_H

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

#define CINGULATE_KG_MODULE_NAME    "cingulate_kg_wiring"
#define CINGULATE_KG_ROOT_NAME      "cingulate_cortex"

//=============================================================================
// Node Type Extensions (Cingulate-specific, starting at 0x2700)
//=============================================================================

typedef enum {
    CINGULATE_KG_NODE_DORSAL_ACC = 0x2700,
    CINGULATE_KG_NODE_VENTRAL_ACC,
    CINGULATE_KG_NODE_PCC,
    CINGULATE_KG_NODE_CONFLICT,
    CINGULATE_KG_NODE_ERROR_MONITOR,
    CINGULATE_KG_NODE_REWARD
} cingulate_kg_node_type_t;

typedef enum {
    CINGULATE_KG_EDGE_DETECTS = 0x2700,
    CINGULATE_KG_EDGE_MONITORS,
    CINGULATE_KG_EDGE_EVALUATES,
    CINGULATE_KG_EDGE_SIGNALS_ERROR
} cingulate_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    bool register_dacc;
    bool register_vacc;
    bool register_pcc;
    bool register_conflict_detection;
    bool register_error_monitoring;
    bool register_reward_processing;
    bool register_cross_edges;
    bool include_state_metadata;
} cingulate_kg_config_t;

typedef struct {
    brain_kg_node_id_t root_id;
    brain_kg_node_id_t dacc_id;
    brain_kg_node_id_t vacc_id;
    brain_kg_node_id_t pcc_id;
    brain_kg_node_id_t conflict_detection_id;
    brain_kg_node_id_t error_monitoring_id;
    brain_kg_node_id_t reward_processing_id;
    uint32_t node_count;
    uint32_t edge_count;
    bool registered;
} cingulate_kg_state_t;

//=============================================================================
// API
//=============================================================================

NIMCP_EXPORT int cingulate_kg_default_config(cingulate_kg_config_t* config);

NIMCP_EXPORT int cingulate_kg_register_all(
    brain_kg_t* kg,
    const cingulate_kg_config_t* config,
    cingulate_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT int cingulate_kg_unregister_all(
    brain_kg_t* kg,
    cingulate_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT brain_kg_node_id_t cingulate_kg_get_root(brain_kg_t* kg);

NIMCP_EXPORT brain_kg_node_id_t cingulate_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/* Forward decl for runtime event emit API (W2). */
struct brain_struct;

/**
 * @brief Emit a runtime cingulate event into the brain's internal KG
 *
 * Supported kinds: "conflict". Silent no-op if brain/KG unavailable.
 * Creates `cingulate_event_<kind>_<ts_us>` node + edge to
 * `cingulate_cortex`. Self-elevates admin token.
 */
NIMCP_EXPORT void cingulate_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CINGULATE_KG_WIRING_H */
