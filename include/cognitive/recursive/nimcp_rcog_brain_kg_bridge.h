/**
 * @file nimcp_rcog_brain_kg_bridge.h
 * @brief Brain Knowledge Graph Integration Bridge for Recursive Cognition
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Bidirectional bridge connecting recursive cognition with brain knowledge graph
 * WHY:  KG provides self-awareness, introspection, and semantic knowledge
 * HOW:  Full bridge pattern with node registration and capability queries
 *
 * BIOLOGICAL BASIS:
 * The brain maintains models of its own structure and capabilities:
 * - Meta-cognition: Awareness of own cognitive processes
 * - Self-model: Understanding of own capabilities and limitations
 * - Introspection: Ability to examine internal state
 * - Semantic memory: Knowledge about concepts and relationships
 *
 * ARCHITECTURE:
 * ```
 * +----------------------+                    +----------------------+
 * | RECURSIVE COGNITION  |                    |   BRAIN KNOWLEDGE    |
 * |                      |                    |       GRAPH          |
 * | - Engine             |<-- self-model ---->| - Module Nodes       |
 * |   (registered node)  |    (capabilities)  | - Capability Edges   |
 * | - Context Store      |                    | - State Properties   |
 * |   (variable node)    |<-- introspection ->| - Semantic Nodes     |
 * | - Processing State   |    (current state) | - Relationship Edges |
 * |   (property updates) |                    |                      |
 * +----------------------+                    +----------------------+
 *           |                                           |
 *           +---------------- BRIDGE -------------------+
 *                      (self-awareness & semantics)
 * ```
 *
 * KG NODE TYPES:
 * - BRAIN_KG_NODE_COGNITIVE: Recursive cognition engine
 * - BRAIN_KG_NODE_UTILITY: Context store
 * - BRAIN_KG_NODE_COORDINATOR: Orchestrator
 * - BRAIN_KG_NODE_INTEGRATION: Delegation pool
 *
 * KG EDGE RELATIONSHIPS:
 * - PROVIDES_TO: Engine provides context variables
 * - COORDINATES_WITH: Orchestrator coordinates with delegation
 * - INTEGRATES_WITH: Engine integrates with other systems
 */

#ifndef NIMCP_RCOG_BRAIN_KG_BRIDGE_H
#define NIMCP_RCOG_BRAIN_KG_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/recursive/nimcp_rcog_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

struct rcog_engine;
struct rcog_context_store;
struct rcog_orchestrator;
struct brain_kg;
struct kg_reader;
struct brain_kg_node;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum capabilities to register */
#define RCOG_KG_MAX_CAPABILITIES        32

/** Maximum semantic queries per update */
#define RCOG_KG_MAX_SEMANTIC_QUERIES    8

/** Maximum properties to update per cycle */
#define RCOG_KG_MAX_PROPERTY_UPDATES    16

/** Default introspection update interval (ms) */
#define RCOG_KG_DEFAULT_UPDATE_INTERVAL 100

/*=============================================================================
 * NODE TYPES
 *===========================================================================*/

/**
 * @brief KG node types for recursive cognition components
 */
typedef enum {
    RCOG_KG_NODE_ENGINE = 0,         /**< Main engine node */
    RCOG_KG_NODE_CONTEXT_STORE,      /**< Context store node */
    RCOG_KG_NODE_ORCHESTRATOR,       /**< Orchestrator node */
    RCOG_KG_NODE_DELEGATION,         /**< Delegation pool node */
    RCOG_KG_NODE_ANSWER_REFINER,     /**< Answer refiner node */
    RCOG_KG_NODE_TOOL_ROUTER,        /**< Tool router node */
    RCOG_KG_NODE_COUNT
} rcog_kg_node_type_t;

/**
 * @brief KG edge relationship types
 */
typedef enum {
    RCOG_KG_EDGE_PROVIDES_TO = 0,    /**< A provides service to B */
    RCOG_KG_EDGE_COORDINATES_WITH,   /**< A coordinates with B */
    RCOG_KG_EDGE_INTEGRATES_WITH,    /**< A integrates with B */
    RCOG_KG_EDGE_DELEGATES_TO,       /**< A delegates work to B */
    RCOG_KG_EDGE_REFINES_FOR,        /**< A refines answers for B */
    RCOG_KG_EDGE_ROUTES_TO           /**< A routes tools to B */
} rcog_kg_edge_type_t;

/*=============================================================================
 * EFFECTS STRUCTURES
 *===========================================================================*/

/**
 * @brief Capability description for KG registration
 */
typedef struct {
    const char* name;                /**< Capability name */
    const char* description;         /**< Human-readable description */
    rcog_capability_tier_t tier;     /**< Required capability tier */
    float performance_factor;        /**< Performance rating [0.0-1.0] */
    bool currently_available;        /**< Whether currently available */
} rcog_capability_info_t;

/**
 * @brief Processing state for KG updates
 */
typedef struct {
    bool is_processing;              /**< Currently processing */
    uint32_t active_subtasks;        /**< Number of active subtasks */
    uint32_t current_depth;          /**< Current recursion depth */
    float current_confidence;        /**< Current answer confidence */
    rcog_answer_status_t answer_status; /**< Answer refinement status */
    size_t context_variable_count;   /**< Number of context variables */
    size_t total_context_size;       /**< Total context size in bytes */
    float processing_load;           /**< Current processing load [0.0-1.0] */
} rcog_processing_state_t;

/**
 * @brief Semantic query result
 */
typedef struct {
    const char* query;               /**< Original query */
    bool success;                    /**< Query succeeded */
    uint32_t num_results;            /**< Number of results */
    void* result_data;               /**< Result data (format depends on query) */
    size_t result_size;              /**< Size of result data */
} rcog_semantic_query_result_t;

/**
 * @brief Effects flowing from recursive cognition to brain KG
 *
 * WHAT: State updates and capability registration
 * WHY:  Keep KG aware of current processing state
 */
typedef struct {
    /* State updates */
    bool update_processing_state;    /**< Update processing state */
    rcog_processing_state_t state;   /**< Current processing state */

    /* Capability registration */
    bool register_capabilities;      /**< Register capabilities */
    uint32_t num_capabilities;       /**< Number of capabilities */
    rcog_capability_info_t capabilities[RCOG_KG_MAX_CAPABILITIES];

    /* Context variable updates */
    bool update_context_info;        /**< Update context info */
    const char** variable_names;     /**< Variable names */
    uint32_t num_variables;          /**< Number of variables */

    /* Performance metrics */
    float avg_decomposition_time_ms; /**< Avg decomposition time */
    float avg_subtask_time_ms;       /**< Avg subtask completion time */
    float avg_refinement_steps;      /**< Avg refinement steps */
} rcog_to_kg_effects_t;

/**
 * @brief Effects flowing from brain KG to recursive cognition
 *
 * WHAT: Self-model information and semantic knowledge
 * WHY:  Enable meta-cognition and informed processing
 */
typedef struct {
    /* Self-model information */
    bool self_model_available;       /**< Self-model is available */
    uint32_t registered_capabilities; /**< Number of registered capabilities */
    float overall_health;            /**< Overall system health [0.0-1.0] */

    /* Capability information */
    bool capability_query_complete;  /**< Capability query done */
    uint32_t available_tools;        /**< Number of available tools */
    float capacity_estimate;         /**< Estimated available capacity */

    /* Semantic knowledge */
    bool semantic_results_ready;     /**< Semantic query results ready */
    uint32_t num_semantic_results;   /**< Number of results */
    rcog_semantic_query_result_t* semantic_results; /**< Results array */

    /* Integration status */
    uint32_t connected_modules;      /**< Number of connected modules */
    bool full_graph_available;       /**< Full KG is available */

    /* Introspection data */
    bool introspection_available;    /**< Introspection data available */
    const char* current_module_focus; /**< Current processing focus */
    float attention_level;           /**< Current attention level */
} kg_to_rcog_effects_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Brain KG bridge configuration
 */
typedef struct {
    /* Registration settings */
    bool auto_register_on_connect;   /**< Auto-register on connect */
    bool register_all_components;    /**< Register all sub-components */

    /* Update settings */
    uint32_t state_update_interval_ms; /**< State update interval */
    bool enable_continuous_updates;  /**< Continuous state updates */

    /* Introspection settings */
    bool enable_introspection;       /**< Enable introspection queries */
    bool enable_semantic_queries;    /**< Enable semantic knowledge queries */
    uint32_t max_semantic_query_depth; /**< Max depth for semantic search */

    /* Performance settings */
    bool lazy_capability_query;      /**< Query capabilities lazily */
    uint32_t capability_cache_ttl_ms; /**< Cache TTL for capabilities */
} rcog_brain_kg_bridge_config_t;

/*=============================================================================
 * BRIDGE HANDLE
 *===========================================================================*/

/**
 * @brief Brain KG bridge opaque handle
 */
typedef struct rcog_brain_kg_bridge rcog_brain_kg_bridge_t;

/**
 * @brief KG node handle for registered components
 */
typedef uint64_t rcog_kg_node_id_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create brain KG bridge with configuration
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
rcog_brain_kg_bridge_t* rcog_brain_kg_bridge_create(
    const rcog_brain_kg_bridge_config_t* config
);

/**
 * @brief Create bridge with default configuration
 * @return Bridge handle or NULL on error
 */
rcog_brain_kg_bridge_t* rcog_brain_kg_bridge_create_default(void);

/**
 * @brief Destroy brain KG bridge
 * @param bridge Bridge handle (NULL safe)
 */
void rcog_brain_kg_bridge_destroy(rcog_brain_kg_bridge_t* bridge);

/**
 * @brief Get default configuration
 * @return Default configuration
 */
rcog_brain_kg_bridge_config_t rcog_brain_kg_bridge_default_config(void);

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

/**
 * @brief Connect bridge to brain knowledge graph
 * @param bridge Bridge handle
 * @param kg Brain KG handle
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_connect(
    rcog_brain_kg_bridge_t* bridge,
    struct brain_kg* kg
);

/**
 * @brief Connect bridge to KG reader for semantic queries
 * @param bridge Bridge handle
 * @param reader KG reader handle
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_connect_reader(
    rcog_brain_kg_bridge_t* bridge,
    struct kg_reader* reader
);

/**
 * @brief Connect bridge to recursive cognition engine
 * @param bridge Bridge handle
 * @param engine Recursive cognition engine handle
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_connect_engine(
    rcog_brain_kg_bridge_t* bridge,
    struct rcog_engine* engine
);

/**
 * @brief Check if bridge is connected
 * @param bridge Bridge handle
 * @return true if connected
 */
bool rcog_brain_kg_bridge_is_connected(const rcog_brain_kg_bridge_t* bridge);

/*=============================================================================
 * UPDATE
 *===========================================================================*/

/**
 * @brief Update bridge state
 * @param bridge Bridge handle
 * @param delta_time_ms Time since last update
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_update(
    rcog_brain_kg_bridge_t* bridge,
    float delta_time_ms
);

/*=============================================================================
 * REGISTRATION
 *===========================================================================*/

/**
 * @brief Register recursive cognition engine in brain KG
 * @param bridge Bridge handle
 * @param node_id Output registered node ID
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_register_engine(
    rcog_brain_kg_bridge_t* bridge,
    rcog_kg_node_id_t* node_id
);

/**
 * @brief Register a component as a sub-node
 * @param bridge Bridge handle
 * @param component_type Type of component
 * @param node_id Output registered node ID
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_register_component(
    rcog_brain_kg_bridge_t* bridge,
    rcog_kg_node_type_t component_type,
    rcog_kg_node_id_t* node_id
);

/**
 * @brief Register a capability
 * @param bridge Bridge handle
 * @param capability Capability information
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_register_capability(
    rcog_brain_kg_bridge_t* bridge,
    const rcog_capability_info_t* capability
);

/**
 * @brief Create an edge between registered nodes
 * @param bridge Bridge handle
 * @param from_node Source node ID
 * @param to_node Target node ID
 * @param edge_type Relationship type
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_create_edge(
    rcog_brain_kg_bridge_t* bridge,
    rcog_kg_node_id_t from_node,
    rcog_kg_node_id_t to_node,
    rcog_kg_edge_type_t edge_type
);

/*=============================================================================
 * STATE UPDATES
 *===========================================================================*/

/**
 * @brief Update processing state in KG
 * @param bridge Bridge handle
 * @param state Current processing state
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_update_state(
    rcog_brain_kg_bridge_t* bridge,
    const rcog_processing_state_t* state
);

/**
 * @brief Update a node property
 * @param bridge Bridge handle
 * @param node_id Node to update
 * @param property_name Property name
 * @param value Property value (as string)
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_set_property(
    rcog_brain_kg_bridge_t* bridge,
    rcog_kg_node_id_t node_id,
    const char* property_name,
    const char* value
);

/*=============================================================================
 * CAPABILITY QUERIES
 *===========================================================================*/

/**
 * @brief Query available capabilities
 * @param bridge Bridge handle
 * @param capabilities Output array of capabilities
 * @param max_capabilities Maximum to return
 * @param num_capabilities Output number of capabilities
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_query_capabilities(
    rcog_brain_kg_bridge_t* bridge,
    rcog_capability_info_t* capabilities,
    size_t max_capabilities,
    size_t* num_capabilities
);

/**
 * @brief Check if a specific capability is available
 * @param bridge Bridge handle
 * @param capability_name Capability to check
 * @return true if available
 */
bool rcog_brain_kg_bridge_has_capability(
    const rcog_brain_kg_bridge_t* bridge,
    const char* capability_name
);

/*=============================================================================
 * SEMANTIC QUERIES
 *===========================================================================*/

/**
 * @brief Load semantic knowledge as context variable
 * @param bridge Bridge handle
 * @param context_store Context store to load into
 * @param variable_name Name for the variable
 * @param query Semantic query
 * @param max_entities Maximum entities to return
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_load_semantic_knowledge(
    rcog_brain_kg_bridge_t* bridge,
    struct rcog_context_store* context_store,
    const char* variable_name,
    const char* query,
    size_t max_entities
);

/**
 * @brief Execute a semantic query
 * @param bridge Bridge handle
 * @param query Query string
 * @param result Output result
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_semantic_query(
    rcog_brain_kg_bridge_t* bridge,
    const char* query,
    rcog_semantic_query_result_t* result
);

/**
 * @brief Free semantic query result
 * @param result Result to free
 */
void rcog_brain_kg_bridge_free_semantic_result(
    rcog_semantic_query_result_t* result
);

/*=============================================================================
 * INTROSPECTION
 *===========================================================================*/

/**
 * @brief Get current processing focus from introspection
 * @param bridge Bridge handle
 * @param focus Output focus string (caller-allocated)
 * @param max_len Maximum string length
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_get_focus(
    const rcog_brain_kg_bridge_t* bridge,
    char* focus,
    size_t max_len
);

/**
 * @brief Get overall system health from KG
 * @param bridge Bridge handle
 * @return System health [0.0-1.0]
 */
float rcog_brain_kg_bridge_get_system_health(
    const rcog_brain_kg_bridge_t* bridge
);

/**
 * @brief Get list of connected modules from KG
 * @param bridge Bridge handle
 * @param modules Output array of module names
 * @param max_modules Maximum to return
 * @param num_modules Output number of modules
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_get_connected_modules(
    const rcog_brain_kg_bridge_t* bridge,
    char** modules,
    size_t max_modules,
    size_t* num_modules
);

/*=============================================================================
 * EFFECTS ACCESS
 *===========================================================================*/

/**
 * @brief Get current effects from rcog to KG
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_get_outgoing_effects(
    const rcog_brain_kg_bridge_t* bridge,
    rcog_to_kg_effects_t* effects
);

/**
 * @brief Get current effects from KG to rcog
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_get_incoming_effects(
    const rcog_brain_kg_bridge_t* bridge,
    kg_to_rcog_effects_t* effects
);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t registrations_performed;
    uint64_t state_updates;
    uint64_t capability_queries;
    uint64_t semantic_queries;
    uint64_t properties_updated;
    uint64_t edges_created;
    float avg_query_time_ms;
    float avg_update_time_ms;
} rcog_brain_kg_bridge_stats_t;

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int rcog_brain_kg_bridge_get_stats(
    const rcog_brain_kg_bridge_t* bridge,
    rcog_brain_kg_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 * @param bridge Bridge handle
 */
void rcog_brain_kg_bridge_reset_stats(rcog_brain_kg_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_BRAIN_KG_BRIDGE_H */
