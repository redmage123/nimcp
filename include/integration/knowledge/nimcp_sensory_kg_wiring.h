/**
 * @file nimcp_sensory_kg_wiring.h
 * @brief Sensory Module Knowledge Graph Wiring
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Knowledge graph integration for Phase 6 sensory modules (somatosensory,
 *       olfactory, gustatory) that registers sensory entities, relationships,
 *       and processing nodes in the NIMCP knowledge graph.
 *
 * WHY: The knowledge graph needs to understand sensory processing:
 *      - Register sensory cortex nodes for reasoning about perception
 *      - Create edges between sensory modalities and downstream processors
 *      - Enable cross-modal queries (e.g., "what tastes go with this smell?")
 *      - Support security validation of sensory data pathways
 *      - Enable introspection of sensory processing state
 *
 * HOW: Provides registration APIs for each sensory module that:
 *      1. Create node entries for cortical regions
 *      2. Define edge types for sensory relationships
 *      3. Register with the BBB for security validation
 *      4. Support querying sensory state through the KG
 *
 * KNOWLEDGE GRAPH STRUCTURE:
 * ==========================
 *
 * NODE TYPES:
 * -----------
 * - SENSORY_CORTEX: Root category for sensory cortices
 * - SOMATOSENSORY_REGION: Body map regions (fingers, face, etc.)
 * - OLFACTORY_RECEPTOR: Odor receptor types
 * - GUSTATORY_RECEPTOR: Taste receptor types
 * - SENSORY_PATHWAY: Processing pathway nodes
 *
 * EDGE TYPES:
 * -----------
 * - PROCESSES_INPUT: Cortex processes sensory input
 * - PROJECTS_TO: Sends output to target
 * - MODULATES: Modulates activity of target
 * - INTEGRATES_WITH: Cross-modal integration
 * - ASSOCIATED_MEMORY: Links to memory systems
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SENSORY_KG_WIRING_H
#define NIMCP_SENSORY_KG_WIRING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"
#include "core/brain/regions/olfactory/nimcp_olfactory.h"
#include "core/brain/regions/gustatory/nimcp_gustatory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of KG nodes per sensory module */
#define SENSORY_KG_MAX_NODES_PER_MODULE     256

/** Maximum number of edges */
#define SENSORY_KG_MAX_EDGES                1024

/** Node ID ranges for sensory modules */
#define SENSORY_KG_SOMATOSENSORY_BASE       0x4000
#define SENSORY_KG_OLFACTORY_BASE           0x4100
#define SENSORY_KG_GUSTATORY_BASE           0x4200
#define SENSORY_KG_CROSS_MODAL_BASE         0x4300

/** Version */
#define SENSORY_KG_VERSION_MAJOR            1
#define SENSORY_KG_VERSION_MINOR            0
#define SENSORY_KG_VERSION_PATCH            0

/* ============================================================================
 * Node Types
 * ============================================================================ */

/**
 * @brief KG node types for sensory modules
 */
typedef enum {
    /* Meta types */
    SENSORY_KG_NODE_ROOT = 0,           /**< Root sensory node */
    SENSORY_KG_NODE_CORTEX,             /**< Sensory cortex */
    SENSORY_KG_NODE_PATHWAY,            /**< Processing pathway */
    SENSORY_KG_NODE_RECEPTOR,           /**< Receptor type */

    /* Somatosensory specific */
    SENSORY_KG_NODE_BODY_REGION,        /**< Body map region */
    SENSORY_KG_NODE_MECHANORECEPTOR,    /**< Mechanoreceptor type */
    SENSORY_KG_NODE_THERMORECEPTOR,     /**< Thermoreceptor */
    SENSORY_KG_NODE_NOCICEPTOR,         /**< Pain receptor */
    SENSORY_KG_NODE_PROPRIOCEPTOR,      /**< Proprioceptor */

    /* Olfactory specific */
    SENSORY_KG_NODE_GLOMERULUS,         /**< Olfactory glomerulus */
    SENSORY_KG_NODE_ODOR_CATEGORY,      /**< Odor category */
    SENSORY_KG_NODE_ODOR_MEMORY,        /**< Odor-memory association */

    /* Gustatory specific */
    SENSORY_KG_NODE_TASTE_BUD,          /**< Taste bud region */
    SENSORY_KG_NODE_TASTE_QUALITY,      /**< Taste quality (sweet, etc.) */
    SENSORY_KG_NODE_FOOD_CATEGORY,      /**< Food category */

    /* Cross-modal */
    SENSORY_KG_NODE_FLAVOR,             /**< Flavor (taste+smell) */
    SENSORY_KG_NODE_TEXTURE,            /**< Texture perception */
    SENSORY_KG_NODE_CHEMOSENSORY,       /**< Combined chemical senses */

    SENSORY_KG_NODE_TYPE_COUNT
} sensory_kg_node_type_t;

/**
 * @brief KG edge types for sensory relationships
 */
typedef enum {
    /* Processing edges */
    SENSORY_KG_EDGE_PROCESSES = 0,      /**< Processes input from */
    SENSORY_KG_EDGE_PROJECTS_TO,        /**< Projects output to */
    SENSORY_KG_EDGE_MODULATES,          /**< Modulates activity */
    SENSORY_KG_EDGE_INHIBITS,           /**< Inhibits target */

    /* Integration edges */
    SENSORY_KG_EDGE_INTEGRATES_WITH,    /**< Cross-modal integration */
    SENSORY_KG_EDGE_BINDS_WITH,         /**< Perceptual binding */
    SENSORY_KG_EDGE_ENHANCES,           /**< Enhances perception */

    /* Memory edges */
    SENSORY_KG_EDGE_ASSOCIATED_MEMORY,  /**< Links to memory */
    SENSORY_KG_EDGE_TRIGGERS_MEMORY,    /**< Triggers memory recall */
    SENSORY_KG_EDGE_LEARNED_FROM,       /**< Learned association */

    /* Behavioral edges */
    SENSORY_KG_EDGE_TRIGGERS_RESPONSE,  /**< Triggers behavioral response */
    SENSORY_KG_EDGE_INDICATES_DANGER,   /**< Indicates danger */
    SENSORY_KG_EDGE_INDICATES_REWARD,   /**< Indicates reward */

    /* Anatomical edges */
    SENSORY_KG_EDGE_LOCATED_IN,         /**< Anatomical location */
    SENSORY_KG_EDGE_PART_OF,            /**< Part of larger structure */
    SENSORY_KG_EDGE_INNERVATES,         /**< Neural innervation */

    SENSORY_KG_EDGE_TYPE_COUNT
} sensory_kg_edge_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief KG node for sensory entity
 */
typedef struct {
    uint32_t node_id;                   /**< Unique node ID */
    sensory_kg_node_type_t type;        /**< Node type */
    char name[64];                      /**< Human-readable name */
    char description[256];              /**< Description */

    /* Module association */
    uint32_t module_id;                 /**< Source module (bio-async ID) */
    uint32_t local_id;                  /**< Module-local ID */

    /* Properties */
    float importance;                   /**< Importance weight [0, 1] */
    float activation;                   /**< Current activation [0, 1] */
    bool is_active;                     /**< Currently active */

    /* Security */
    uint32_t security_level;            /**< BBB security clearance */
    bool validated;                     /**< BBB validated */

    /* Timestamps */
    uint64_t created_time;              /**< Creation timestamp */
    uint64_t last_updated;              /**< Last update timestamp */
} sensory_kg_node_t;

/**
 * @brief KG edge for sensory relationship
 */
typedef struct {
    uint32_t edge_id;                   /**< Unique edge ID */
    sensory_kg_edge_type_t type;        /**< Edge type */

    /* Connection */
    uint32_t source_node_id;            /**< Source node ID */
    uint32_t target_node_id;            /**< Target node ID */

    /* Properties */
    float weight;                       /**< Connection weight [0, 1] */
    float confidence;                   /**< Confidence in relationship */
    bool bidirectional;                 /**< Bidirectional edge */
    bool active;                        /**< Currently active */

    /* Timestamps */
    uint64_t created_time;              /**< Creation timestamp */
    uint64_t last_used;                 /**< Last traversal timestamp */
} sensory_kg_edge_t;

/**
 * @brief Query result for KG traversal
 */
typedef struct {
    sensory_kg_node_t** nodes;          /**< Matched nodes */
    uint32_t num_nodes;                 /**< Number of nodes */
    sensory_kg_edge_t** edges;          /**< Traversed edges */
    uint32_t num_edges;                 /**< Number of edges */
    float relevance_score;              /**< Query relevance [0, 1] */
} sensory_kg_query_result_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief KG wiring configuration
 */
typedef struct {
    /* Capacity */
    uint32_t max_nodes;                 /**< Maximum nodes */
    uint32_t max_edges;                 /**< Maximum edges */

    /* Features */
    bool enable_somatosensory;          /**< Register somatosensory */
    bool enable_olfactory;              /**< Register olfactory */
    bool enable_gustatory;              /**< Register gustatory */
    bool enable_cross_modal;            /**< Enable cross-modal nodes */

    /* Security */
    bool enable_bbb_validation;         /**< Validate through BBB */
    uint32_t default_security_level;    /**< Default security level */

    /* Performance */
    bool enable_caching;                /**< Enable query caching */
    uint32_t cache_ttl_ms;              /**< Cache TTL */

    /* Logging */
    bool enable_logging;                /**< Enable logging */
} sensory_kg_config_t;

/**
 * @brief KG wiring statistics
 */
typedef struct {
    /* Node counts */
    uint32_t total_nodes;
    uint32_t somatosensory_nodes;
    uint32_t olfactory_nodes;
    uint32_t gustatory_nodes;
    uint32_t cross_modal_nodes;

    /* Edge counts */
    uint32_t total_edges;
    uint32_t processing_edges;
    uint32_t integration_edges;
    uint32_t memory_edges;

    /* Query stats */
    uint64_t queries_processed;
    uint64_t cache_hits;
    uint64_t cache_misses;

    /* Errors */
    uint64_t registration_errors;
    uint64_t validation_errors;
} sensory_kg_stats_t;

/* ============================================================================
 * Handle
 * ============================================================================ */

typedef struct sensory_kg_wiring_struct sensory_kg_wiring_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int sensory_kg_default_config(sensory_kg_config_t* config);
sensory_kg_wiring_t* sensory_kg_wiring_create(const sensory_kg_config_t* config);
void sensory_kg_wiring_destroy(sensory_kg_wiring_t* wiring);

/* ============================================================================
 * Registration API - Somatosensory
 * ============================================================================ */

int sensory_kg_register_somatosensory(sensory_kg_wiring_t* wiring, nimcp_somatosensory_t* soma);
int sensory_kg_register_body_region(sensory_kg_wiring_t* wiring, const char* name, body_segment_t region, uint32_t* node_id);
int sensory_kg_register_mechanoreceptor(sensory_kg_wiring_t* wiring, const char* name, soma_receptor_type_t type, uint32_t* node_id);
int sensory_kg_register_touch_pathway(sensory_kg_wiring_t* wiring, uint32_t source_id, uint32_t target_id);

/* ============================================================================
 * Registration API - Olfactory
 * ============================================================================ */

int sensory_kg_register_olfactory(sensory_kg_wiring_t* wiring, nimcp_olfactory_t* olfact);
int sensory_kg_register_glomerulus(sensory_kg_wiring_t* wiring, uint32_t glom_id, uint32_t* node_id);
int sensory_kg_register_odor_category(sensory_kg_wiring_t* wiring, const char* name, odor_category_t category, uint32_t* node_id);
int sensory_kg_register_odor_memory(sensory_kg_wiring_t* wiring, uint32_t odor_node, uint32_t memory_id);

/* ============================================================================
 * Registration API - Gustatory
 * ============================================================================ */

int sensory_kg_register_gustatory(sensory_kg_wiring_t* wiring, nimcp_gustatory_t* gust);
int sensory_kg_register_taste_quality(sensory_kg_wiring_t* wiring, basic_taste_t taste, uint32_t* node_id);
int sensory_kg_register_tongue_region(sensory_kg_wiring_t* wiring, tongue_region_t region, uint32_t* node_id);
int sensory_kg_register_food_category(sensory_kg_wiring_t* wiring, const char* name, food_category_t category, uint32_t* node_id);

/* ============================================================================
 * Cross-Modal Registration
 * ============================================================================ */

int sensory_kg_register_flavor(sensory_kg_wiring_t* wiring, uint32_t taste_node, uint32_t odor_node, uint32_t* flavor_node_id);
int sensory_kg_register_chemosensory(sensory_kg_wiring_t* wiring, uint32_t olfact_node, uint32_t gust_node, uint32_t* node_id);
int sensory_kg_create_integration_edge(sensory_kg_wiring_t* wiring, uint32_t node_a, uint32_t node_b, float weight);

/* ============================================================================
 * Edge Management
 * ============================================================================ */

int sensory_kg_add_edge(sensory_kg_wiring_t* wiring, sensory_kg_edge_type_t type, uint32_t source, uint32_t target, float weight);
int sensory_kg_remove_edge(sensory_kg_wiring_t* wiring, uint32_t edge_id);
int sensory_kg_update_edge_weight(sensory_kg_wiring_t* wiring, uint32_t edge_id, float new_weight);

/* ============================================================================
 * Query API
 * ============================================================================ */

int sensory_kg_query_node(sensory_kg_wiring_t* wiring, uint32_t node_id, sensory_kg_node_t* node);
int sensory_kg_query_by_type(sensory_kg_wiring_t* wiring, sensory_kg_node_type_t type, sensory_kg_query_result_t* result);
int sensory_kg_query_connected(sensory_kg_wiring_t* wiring, uint32_t node_id, sensory_kg_edge_type_t edge_type, sensory_kg_query_result_t* result);
int sensory_kg_query_path(sensory_kg_wiring_t* wiring, uint32_t source_id, uint32_t target_id, sensory_kg_query_result_t* result);
void sensory_kg_free_query_result(sensory_kg_query_result_t* result);

/* ============================================================================
 * Activation API
 * ============================================================================ */

int sensory_kg_activate_node(sensory_kg_wiring_t* wiring, uint32_t node_id, float activation);
int sensory_kg_propagate_activation(sensory_kg_wiring_t* wiring, uint32_t source_id, float decay_rate);
int sensory_kg_decay_activations(sensory_kg_wiring_t* wiring, float decay_factor);

/* ============================================================================
 * Security API
 * ============================================================================ */

int sensory_kg_validate_node(sensory_kg_wiring_t* wiring, uint32_t node_id, void* bbb_context);
int sensory_kg_set_security_level(sensory_kg_wiring_t* wiring, uint32_t node_id, uint32_t level);
bool sensory_kg_check_access(sensory_kg_wiring_t* wiring, uint32_t node_id, uint32_t requester_level);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int sensory_kg_get_stats(const sensory_kg_wiring_t* wiring, sensory_kg_stats_t* stats);
int sensory_kg_reset_stats(sensory_kg_wiring_t* wiring);
const char* sensory_kg_node_type_name(sensory_kg_node_type_t type);
const char* sensory_kg_edge_type_name(sensory_kg_edge_type_t type);
void sensory_kg_print_summary(const sensory_kg_wiring_t* wiring);

/* ============================================================================
 * Serialization
 * ============================================================================ */

size_t sensory_kg_get_serialization_size(const sensory_kg_wiring_t* wiring);
int sensory_kg_serialize(const sensory_kg_wiring_t* wiring, uint8_t* buffer, size_t size, size_t* written);
sensory_kg_wiring_t* sensory_kg_deserialize(const uint8_t* buffer, size_t size, size_t* bytes_read);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SENSORY_KG_WIRING_H */
