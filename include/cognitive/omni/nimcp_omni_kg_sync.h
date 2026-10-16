/**
 * @file nimcp_omni_kg_sync.h
 * @brief Omnidirectional Inference Knowledge Graph Synchronization
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: KG sync utilities for omnidirectional inference modules
 * WHY:  Enable runtime topology queries and self-awareness for omni inference
 * HOW:  Create KG nodes for omni modules, edges for prediction directions
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * SELF-AWARE INFERENCE:
 * ---------------------
 * Omnidirectional inference benefits from KG-based self-awareness:
 *
 *   1. DIRECTION-AWARE ROUTING:
 *      - KG tracks which modules support which inference directions
 *      - FORWARD, BACKWARD, LATERAL, HIERARCHICAL capabilities per module
 *      - Runtime queries for capability discovery
 *
 *   2. MODALITY MAPPING:
 *      - KG links sensory modalities to omni inference modules
 *      - Cross-modal prediction paths discoverable at runtime
 *      - Precision weighting stored as edge attributes
 *
 *   3. HIERARCHICAL STRUCTURE:
 *      - KG represents predictive hierarchy levels
 *      - Top-down and bottom-up prediction paths as edges
 *      - Free energy routing through graph
 *
 * KG NODE TYPES FOR OMNI:
 * -----------------------
 *   Node Type              Description
 *   ─────────────────────────────────────────────
 *   OMNI_PREDICTOR         Bidirectional JEPA predictor
 *   OMNI_MEMORY            Hopfield associative memory
 *   OMNI_HIERARCHY         Predictive coding level
 *   OMNI_REPLAY            Temporal replay buffer
 *   OMNI_BRIDGE            Integration bridge module
 *   OMNI_SENSORY           Sensory modality processor
 *
 * KG EDGE TYPES FOR OMNI:
 * -----------------------
 *   Edge Type              Description
 *   ─────────────────────────────────────────────
 *   PREDICTS_FORWARD       Forward prediction (A→B)
 *   PREDICTS_BACKWARD      Backward inference (B→A)
 *   PREDICTS_LATERAL       Cross-modal prediction
 *   PREDICTS_UP            Bottom-up in hierarchy
 *   PREDICTS_DOWN          Top-down in hierarchy
 *   MODULATES_PRECISION    Precision weighting influence
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_KG_SYNC_H
#define NIMCP_OMNI_KG_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/brain/nimcp_brain_kg.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct omni_kg_sync omni_kg_sync_t;
typedef struct jepa_bidirectional jepa_bidirectional_t;
typedef struct hopfield_memory hopfield_memory_t;
typedef struct predictive_hierarchy predictive_hierarchy_t;
typedef struct temporal_replay temporal_replay_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum omni modules in KG */
#define OMNI_KG_MAX_MODULES                64

/** @brief Maximum prediction edges per module */
#define OMNI_KG_MAX_PRED_EDGES             32

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Omnidirectional module types for KG
 */
typedef enum {
    OMNI_KG_TYPE_PREDICTOR = 0,      /**< JEPA bidirectional predictor */
    OMNI_KG_TYPE_MEMORY,             /**< Hopfield associative memory */
    OMNI_KG_TYPE_HIERARCHY_LEVEL,    /**< Predictive hierarchy level */
    OMNI_KG_TYPE_REPLAY,             /**< Temporal replay buffer */
    OMNI_KG_TYPE_BRIDGE,             /**< Integration bridge */
    OMNI_KG_TYPE_SENSORY,            /**< Sensory modality processor */
    OMNI_KG_TYPE_CORTICAL,           /**< Cortical column integration */
    OMNI_KG_TYPE_LANGUAGE,           /**< Language processing module */
    OMNI_KG_TYPE_COUNT
} omni_kg_module_type_t;

/**
 * @brief Prediction direction edge types for KG
 */
typedef enum {
    OMNI_KG_EDGE_PREDICTS_FORWARD = 0, /**< Forward prediction */
    OMNI_KG_EDGE_PREDICTS_BACKWARD,    /**< Backward inference */
    OMNI_KG_EDGE_PREDICTS_LATERAL,     /**< Cross-modal/lateral */
    OMNI_KG_EDGE_PREDICTS_UP,          /**< Bottom-up in hierarchy */
    OMNI_KG_EDGE_PREDICTS_DOWN,        /**< Top-down in hierarchy */
    OMNI_KG_EDGE_MODULATES_PRECISION,  /**< Precision modulation */
    OMNI_KG_EDGE_BINDS_WITH,           /**< Cross-modal binding */
    OMNI_KG_EDGE_REPLAYS_TO,           /**< Replay sequence target */
    OMNI_KG_EDGE_COUNT
} omni_kg_edge_type_t;

/**
 * @brief Inference capability flags
 */
typedef enum {
    OMNI_KG_CAP_NONE = 0,
    OMNI_KG_CAP_FORWARD     = (1 << 0),  /**< Supports forward prediction */
    OMNI_KG_CAP_BACKWARD    = (1 << 1),  /**< Supports backward inference */
    OMNI_KG_CAP_LATERAL     = (1 << 2),  /**< Supports lateral inference */
    OMNI_KG_CAP_HIERARCHICAL = (1 << 3), /**< Supports hierarchical prediction */
    OMNI_KG_CAP_ASSOCIATIVE = (1 << 4),  /**< Supports associative retrieval */
    OMNI_KG_CAP_MASKED      = (1 << 5),  /**< Supports masked prediction */
    OMNI_KG_CAP_ALL         = 0x3F       /**< All capabilities */
} omni_kg_capability_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Module registration info for KG
 */
typedef struct {
    char name[64];                       /**< Module name */
    omni_kg_module_type_t type;          /**< Module type */
    uint16_t bio_module_id;              /**< Bio-async module ID */
    omni_kg_capability_t capabilities;   /**< Supported inference directions */
    float default_precision;             /**< Default precision weight */
    void* module_ptr;                    /**< Pointer to module instance */
} omni_kg_module_info_t;

/**
 * @brief Prediction edge info for KG
 */
typedef struct {
    brain_kg_node_id_t from_node;        /**< Source node ID */
    brain_kg_node_id_t to_node;          /**< Target node ID */
    omni_kg_edge_type_t edge_type;       /**< Edge type */
    float precision;                     /**< Edge precision weight */
    float strength;                      /**< Connection strength */
} omni_kg_edge_info_t;

/**
 * @brief Sync configuration
 */
typedef struct {
    bool create_nodes;                   /**< Create module nodes */
    bool create_edges;                   /**< Create prediction edges */
    bool update_precision;               /**< Update precision weights */
    bool sync_capabilities;              /**< Sync capability flags */
    bool enable_logging;                 /**< Enable sync logging */
} omni_kg_sync_config_t;

/**
 * @brief Sync statistics
 */
typedef struct {
    uint32_t nodes_created;              /**< Nodes created */
    uint32_t nodes_updated;              /**< Nodes updated */
    uint32_t edges_created;              /**< Edges created */
    uint32_t edges_updated;              /**< Edges updated */
    uint32_t total_syncs;                /**< Total sync operations */
    uint64_t last_sync_time_ms;          /**< Last sync timestamp */
} omni_kg_sync_stats_t;

/**
 * @brief Omni KG sync manager
 */
struct omni_kg_sync {
    brain_kg_t* kg;                      /**< Brain KG reference */
    omni_kg_sync_config_t config;        /**< Configuration */
    omni_kg_sync_stats_t stats;          /**< Statistics */

    /* Registered modules */
    omni_kg_module_info_t* modules;      /**< Module registry */
    brain_kg_node_id_t* node_ids;        /**< KG node IDs for modules */
    uint32_t module_count;               /**< Number of registered modules */
    uint32_t module_capacity;            /**< Registry capacity */

    /* Thread safety */
    void* mutex;
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default sync configuration
 */
int omni_kg_sync_default_config(omni_kg_sync_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create omni KG sync manager
 *
 * @param kg Brain knowledge graph
 * @param config Configuration (NULL for defaults)
 * @return New sync manager or NULL on failure
 */
omni_kg_sync_t* omni_kg_sync_create(brain_kg_t* kg,
                                     const omni_kg_sync_config_t* config);

/**
 * @brief Destroy sync manager
 */
void omni_kg_sync_destroy(omni_kg_sync_t* sync);

/* ============================================================================
 * Registration API
 * ============================================================================ */

/**
 * @brief Register omnidirectional module with KG
 *
 * @param sync Sync manager
 * @param info Module info
 * @return KG node ID or BRAIN_KG_INVALID_NODE on failure
 */
brain_kg_node_id_t omni_kg_register_module(omni_kg_sync_t* sync,
                                            const omni_kg_module_info_t* info);

/**
 * @brief Register JEPA bidirectional predictor
 */
brain_kg_node_id_t omni_kg_register_jepa(omni_kg_sync_t* sync,
                                          const char* name,
                                          jepa_bidirectional_t* jepa);

/**
 * @brief Register Hopfield memory
 */
brain_kg_node_id_t omni_kg_register_hopfield(omni_kg_sync_t* sync,
                                              const char* name,
                                              hopfield_memory_t* hopfield);

/**
 * @brief Register predictive hierarchy level
 */
brain_kg_node_id_t omni_kg_register_pred_level(omni_kg_sync_t* sync,
                                                const char* name,
                                                uint32_t level_index,
                                                predictive_hierarchy_t* hierarchy);

/**
 * @brief Register temporal replay buffer
 */
brain_kg_node_id_t omni_kg_register_replay(omni_kg_sync_t* sync,
                                            const char* name,
                                            temporal_replay_t* replay);

/**
 * @brief Register integration bridge
 */
brain_kg_node_id_t omni_kg_register_bridge(omni_kg_sync_t* sync,
                                            const char* name,
                                            omni_kg_capability_t capabilities,
                                            void* bridge);

/* ============================================================================
 * Edge API
 * ============================================================================ */

/**
 * @brief Add prediction edge between modules
 *
 * @param sync Sync manager
 * @param from_node Source node
 * @param to_node Target node
 * @param edge_type Edge type
 * @param precision Precision weight
 * @return 0 on success
 */
int omni_kg_add_prediction_edge(omni_kg_sync_t* sync,
                                 brain_kg_node_id_t from_node,
                                 brain_kg_node_id_t to_node,
                                 omni_kg_edge_type_t edge_type,
                                 float precision);

/**
 * @brief Add bidirectional prediction edges
 */
int omni_kg_add_bidirectional_edge(omni_kg_sync_t* sync,
                                    brain_kg_node_id_t node_a,
                                    brain_kg_node_id_t node_b,
                                    float precision);

/**
 * @brief Add hierarchical prediction edges (up and down)
 */
int omni_kg_add_hierarchical_edges(omni_kg_sync_t* sync,
                                    brain_kg_node_id_t lower_node,
                                    brain_kg_node_id_t upper_node,
                                    float precision);

/**
 * @brief Update edge precision
 */
int omni_kg_update_edge_precision(omni_kg_sync_t* sync,
                                   brain_kg_node_id_t from_node,
                                   brain_kg_node_id_t to_node,
                                   omni_kg_edge_type_t edge_type,
                                   float new_precision);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get modules with specific capability
 */
int omni_kg_get_modules_with_capability(const omni_kg_sync_t* sync,
                                         omni_kg_capability_t capability,
                                         brain_kg_node_id_t* nodes_out,
                                         uint32_t max_nodes);

/**
 * @brief Get prediction path between modules
 */
int omni_kg_get_prediction_path(const omni_kg_sync_t* sync,
                                 brain_kg_node_id_t from_node,
                                 brain_kg_node_id_t to_node,
                                 omni_kg_edge_type_t direction,
                                 brain_kg_node_id_t* path_out,
                                 uint32_t max_path_len);

/**
 * @brief Get modules that can predict to a target
 */
int omni_kg_get_predictors_for(const omni_kg_sync_t* sync,
                                brain_kg_node_id_t target_node,
                                omni_kg_edge_type_t direction,
                                brain_kg_node_id_t* predictors_out,
                                uint32_t max_predictors);

/**
 * @brief Get module node ID by name
 */
brain_kg_node_id_t omni_kg_get_node_by_name(omni_kg_sync_t* sync,
                                             const char* name);

/**
 * @brief Get module capabilities
 */
omni_kg_capability_t omni_kg_get_capabilities(omni_kg_sync_t* sync,
                                               brain_kg_node_id_t node);

/* ============================================================================
 * Sync API
 * ============================================================================ */

/**
 * @brief Sync all registered modules to brain KG
 */
int omni_kg_sync_all(omni_kg_sync_t* sync);

/**
 * @brief Sync precision weights from modules to KG edges
 */
int omni_kg_sync_precision(omni_kg_sync_t* sync);

/**
 * @brief Sync capabilities from modules
 */
int omni_kg_sync_capabilities(omni_kg_sync_t* sync);

/**
 * @brief Get sync statistics
 */
int omni_kg_get_sync_stats(omni_kg_sync_t* sync,
                            omni_kg_sync_stats_t* stats);

/**
 * @brief Reset sync statistics
 */
int omni_kg_reset_sync_stats(omni_kg_sync_t* sync);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_kg_module_type_to_string(omni_kg_module_type_t type);
const char* omni_kg_edge_type_to_string(omni_kg_edge_type_t edge);
const char* omni_kg_capability_to_string(omni_kg_capability_t cap);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_KG_SYNC_H */
