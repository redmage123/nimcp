//=============================================================================
// nimcp_habenula_kg_wiring.h - Habenula Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_habenula_kg_wiring.h
 * @brief Knowledge Graph registration for Habenula module
 *
 * WHAT: Registers Habenula concepts (reward evaluation, punishment signaling,
 *       mood regulation) as nodes in the brain's internal KG.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about reward evaluation state
 *       - Cross-module reasoning about punishment and avoidance
 *       - Introspection of mood regulation dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - Habenula root node
 *         +-- Lateral habenula
 *         +-- Medial habenula
 *         +-- Reward evaluation
 *         +-- Punishment signal
 *         +-- Mood regulation
 *
 * @author NIMCP Development Team
 * @date 2026-02-04
 * @version 1.0.0
 */

#ifndef NIMCP_HABENULA_KG_WIRING_H
#define NIMCP_HABENULA_KG_WIRING_H

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

#define HABENULA_KG_MODULE_NAME    "habenula_kg_wiring"
#define HABENULA_KG_ROOT_NAME      "habenula"

//=============================================================================
// Node Type Extensions (Habenula-specific, starting at 0x2B00)
//=============================================================================

typedef enum {
    HABENULA_KG_NODE_LATERAL = 0x2B00,
    HABENULA_KG_NODE_MEDIAL,
    HABENULA_KG_NODE_REWARD_EVAL,
    HABENULA_KG_NODE_PUNISHMENT,
    HABENULA_KG_NODE_MOOD
} habenula_kg_node_type_t;

typedef enum {
    HABENULA_KG_EDGE_EVALUATES = 0x2B00,
    HABENULA_KG_EDGE_SIGNALS_PUNISHMENT,
    HABENULA_KG_EDGE_SUPPRESSES,
    HABENULA_KG_EDGE_REGULATES_MOOD
} habenula_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    bool register_lateral_habenula;
    bool register_medial_habenula;
    bool register_reward_evaluation;
    bool register_punishment_signal;
    bool register_mood_regulation;
    bool register_cross_edges;
    bool include_state_metadata;
} habenula_kg_config_t;

typedef struct {
    brain_kg_node_id_t root_id;
    brain_kg_node_id_t lateral_habenula_id;
    brain_kg_node_id_t medial_habenula_id;
    brain_kg_node_id_t reward_evaluation_id;
    brain_kg_node_id_t punishment_signal_id;
    brain_kg_node_id_t mood_regulation_id;
    uint32_t node_count;
    uint32_t edge_count;
    bool registered;
} habenula_kg_state_t;

//=============================================================================
// API
//=============================================================================

NIMCP_EXPORT int habenula_kg_default_config(habenula_kg_config_t* config);

NIMCP_EXPORT int habenula_kg_register_all(
    brain_kg_t* kg,
    const habenula_kg_config_t* config,
    habenula_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT int habenula_kg_unregister_all(
    brain_kg_t* kg,
    habenula_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT brain_kg_node_id_t habenula_kg_get_root(brain_kg_t* kg);

NIMCP_EXPORT brain_kg_node_id_t habenula_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

/* Forward decl for runtime event emit API (W2). */
struct brain_struct;

/**
 * @brief Emit a runtime habenula event into the brain's internal KG
 *
 * Supported kinds: "negative_rpe". Silent no-op if brain/KG unavailable.
 * Creates `habenula_event_<kind>_<ts_us>` node + edge to `habenula`.
 */
NIMCP_EXPORT void habenula_kg_emit_event(
    struct brain_struct* brain,
    const char* kind,
    float intensity,
    uint64_t ts_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HABENULA_KG_WIRING_H */
