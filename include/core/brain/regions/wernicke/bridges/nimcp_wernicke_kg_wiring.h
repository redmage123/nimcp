//=============================================================================
// nimcp_wernicke_kg_wiring.h - Wernicke Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_wernicke_kg_wiring.h
 * @brief Knowledge Graph registration for Wernicke's area module
 *
 * WHAT: Registers Wernicke's area concepts (language comprehension,
 *       phonological processing, semantic analysis) as nodes in the brain's
 *       internal Knowledge Graph.
 *
 * WHY:  KG integration enables:
 *       - Semantic queries about language comprehension state
 *       - Cross-module reasoning about language processing pipelines
 *       - Introspection of phonological and semantic systems
 *       - Graph-based analysis of language dynamics
 *
 * HOW:  Creates hierarchical node structure:
 *       - Wernicke root node
 *         +-- Auditory cortex
 *         +-- Phonological processing
 *         +-- Semantic processing
 *         +-- Syntax comprehension
 *
 * EDGES: Represent causal/functional relationships:
 *       - Auditory -> Phonological (SENDS_TO)
 *       - Phonological -> Semantic (PROCESSES)
 *       - Semantic -> Syntax (INTEGRATES_WITH)
 *
 * @author NIMCP Development Team
 * @date 2026-02-04
 * @version 1.0.0
 */

#ifndef NIMCP_WERNICKE_KG_WIRING_H
#define NIMCP_WERNICKE_KG_WIRING_H

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

#define WERNICKE_KG_MODULE_NAME    "wernicke_kg_wiring"
#define WERNICKE_KG_ROOT_NAME      "wernicke"

//=============================================================================
// Node Type Extensions (Wernicke-specific, starting at 0x2400)
//=============================================================================

typedef enum {
    WERNICKE_KG_NODE_AUDITORY_AREA = 0x2400,
    WERNICKE_KG_NODE_PHONOLOGICAL,
    WERNICKE_KG_NODE_SEMANTIC,
    WERNICKE_KG_NODE_SYNTAX,
    WERNICKE_KG_NODE_COMPREHENSION_PROCESS
} wernicke_kg_node_type_t;

typedef enum {
    WERNICKE_KG_EDGE_PROCESSES = 0x2400,
    WERNICKE_KG_EDGE_DECODES,
    WERNICKE_KG_EDGE_MAPS_TO,
    WERNICKE_KG_EDGE_PARSES
} wernicke_kg_edge_type_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    bool register_auditory_cortex;
    bool register_phonological;
    bool register_semantic;
    bool register_syntax;
    bool register_cross_edges;
    bool include_state_metadata;
} wernicke_kg_config_t;

typedef struct {
    brain_kg_node_id_t root_id;
    brain_kg_node_id_t auditory_cortex_id;
    brain_kg_node_id_t phonological_processing_id;
    brain_kg_node_id_t semantic_processing_id;
    brain_kg_node_id_t syntax_comprehension_id;
    uint32_t node_count;
    uint32_t edge_count;
    bool registered;
} wernicke_kg_state_t;

//=============================================================================
// API
//=============================================================================

NIMCP_EXPORT int wernicke_kg_default_config(wernicke_kg_config_t* config);

NIMCP_EXPORT int wernicke_kg_register_all(
    brain_kg_t* kg,
    const wernicke_kg_config_t* config,
    wernicke_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT int wernicke_kg_unregister_all(
    brain_kg_t* kg,
    wernicke_kg_state_t* state,
    uint64_t admin_token
);

NIMCP_EXPORT brain_kg_node_id_t wernicke_kg_get_root(brain_kg_t* kg);

NIMCP_EXPORT brain_kg_node_id_t wernicke_kg_find_subsystem(
    brain_kg_t* kg,
    const char* name
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WERNICKE_KG_WIRING_H */
