//=============================================================================
// nimcp_vta_kg_wiring.h - VTA Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_vta_kg_wiring.h
 * @brief Knowledge Graph registration for Ventral Tegmental Area module
 *
 * WHAT: Registers VTA concepts (reward prediction, motivation,
 *       reinforcement learning) as nodes in the brain's internal KG.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about reward prediction state
 *       - Cross-module reasoning about motivation and reinforcement
 *       - Introspection of dopaminergic drive systems
 *
 * HOW:  Creates hierarchical node structure:
 *       - VTA root node
 *         +-- Dopaminergic neurons
 *         +-- Reward prediction
 *         +-- Motivation drive
 *         +-- Reinforcement learning
 *
 * @author NIMCP Development Team
 * @date 2026-02-04
 * @version 1.0.0
 */

#ifndef NIMCP_VTA_KG_WIRING_H
#define NIMCP_VTA_KG_WIRING_H

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

#define VTA_KG_MODULE_NAME    "vta_kg_wiring"
#define VTA_KG_ROOT_NAME      "vta"

//=============================================================================
// Node Type Extensions (VTA-specific, starting at 0x2A00)
//=============================================================================

typedef enum {
    VTA_KG_NODE_DOPAMINERGIC = 0x2A00,
    VTA_KG_NODE_REWARD_PREDICTION,
    VTA_KG_NODE_MOTIVATION,
    VTA_KG_NODE_REINFORCEMENT
} vta_kg_node_type_t;

typedef enum {
    VTA_KG_EDGE_RELEASES_DA = 0x2A00,
    VTA_KG_EDGE_PREDICTS_REWARD,
    VTA_KG_EDGE_DRIVES_MOTIVATION,
    VTA_KG_EDGE_REINFORCES
} vta_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    bool register_dopaminergic_neurons;
    bool register_reward_prediction;
    bool register_motivation_drive;
    bool register_reinforcement_learning;
    bool register_cross_edges;
    bool include_state_metadata;
} vta_kg_config_t;

typedef struct {
    brain_kg_node_id_t root_id;
    brain_kg_node_id_t dopaminergic_neurons_id;
    brain_kg_node_id_t reward_prediction_id;
    brain_kg_node_id_t motivation_drive_id;
    brain_kg_node_id_t reinforcement_learning_id;
    uint32_t node_count;
    uint32_t edge_count;
    bool registered;
} vta_kg_state_t;

//=============================================================================
// API
//=============================================================================

NIMCP_EXPORT int vta_kg_default_config(vta_kg_config_t* config);

NIMCP_EXPORT int vta_kg_register_all(
    brain_kg_t* kg,
    const vta_kg_config_t* config,
    vta_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT int vta_kg_unregister_all(
    brain_kg_t* kg,
    vta_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT brain_kg_node_id_t vta_kg_get_root(brain_kg_t* kg);

NIMCP_EXPORT brain_kg_node_id_t vta_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/* Forward decl for runtime event emit API (W2). */
struct brain_struct;

/**
 * @brief Emit a runtime VTA event into the brain's internal KG
 *
 * Supported kinds: "dopamine_rpe". Silent no-op if brain/KG unavailable.
 * Creates `vta_event_<kind>_<ts_us>` node + edge to `vta`.
 */
NIMCP_EXPORT void vta_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VTA_KG_WIRING_H */
