//=============================================================================
// nimcp_cerebellum_kg_wiring.h - Cerebellum Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_cerebellum_kg_wiring.h
 * @brief Knowledge Graph registration for Cerebellum module
 *
 * WHAT: Registers Cerebellum concepts (motor learning, coordination, timing
 *       control) as nodes in the brain's internal KG.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about motor learning state
 *       - Cross-module reasoning about coordination and timing
 *       - Introspection of cerebellar circuit dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - Cerebellum root node
 *         +-- Purkinje cells
 *         +-- Granule cells
 *         +-- Deep nuclei
 *         +-- Motor learning
 *         +-- Timing control
 *
 * @author NIMCP Development Team
 * @date 2026-02-04
 * @version 1.0.0
 */

#ifndef NIMCP_CEREBELLUM_KG_WIRING_H
#define NIMCP_CEREBELLUM_KG_WIRING_H

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

#define CEREBELLUM_KG_MODULE_NAME    "cerebellum_kg_wiring"
#define CEREBELLUM_KG_ROOT_NAME      "cerebellum"

//=============================================================================
// Node Type Extensions (Cerebellum-specific, starting at 0x2800)
//=============================================================================

typedef enum {
    CEREBELLUM_KG_NODE_PURKINJE = 0x2800,
    CEREBELLUM_KG_NODE_GRANULE,
    CEREBELLUM_KG_NODE_DEEP_NUCLEI,
    CEREBELLUM_KG_NODE_MOTOR_LEARNING,
    CEREBELLUM_KG_NODE_TIMING
} cerebellum_kg_node_type_t;

typedef enum {
    CEREBELLUM_KG_EDGE_INHIBITS_DEEP = 0x2800,
    CEREBELLUM_KG_EDGE_EXCITES_PURKINJE,
    CEREBELLUM_KG_EDGE_COORDINATES_TIMING,
    CEREBELLUM_KG_EDGE_REFINES_MOTOR
} cerebellum_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    bool register_purkinje_cells;
    bool register_granule_cells;
    bool register_deep_nuclei;
    bool register_motor_learning;
    bool register_timing_control;
    bool register_cross_edges;
    bool include_state_metadata;
} cerebellum_kg_config_t;

typedef struct {
    brain_kg_node_id_t root_id;
    brain_kg_node_id_t purkinje_cells_id;
    brain_kg_node_id_t granule_cells_id;
    brain_kg_node_id_t deep_nuclei_id;
    brain_kg_node_id_t motor_learning_id;
    brain_kg_node_id_t timing_control_id;
    uint32_t node_count;
    uint32_t edge_count;
    bool registered;
} cerebellum_kg_state_t;

//=============================================================================
// API
//=============================================================================

NIMCP_EXPORT int cerebellum_kg_default_config(cerebellum_kg_config_t* config);

NIMCP_EXPORT int cerebellum_kg_register_all(
    brain_kg_t* kg,
    const cerebellum_kg_config_t* config,
    cerebellum_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT int cerebellum_kg_unregister_all(
    brain_kg_t* kg,
    cerebellum_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT brain_kg_node_id_t cerebellum_kg_get_root(brain_kg_t* kg);

NIMCP_EXPORT brain_kg_node_id_t cerebellum_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CEREBELLUM_KG_WIRING_H */
