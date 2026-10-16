/**
 * @file nimcp_reasoning_causal.h
 * @brief Causal Reasoning Engine — DAG-based causal inference with do-calculus
 *
 * WHAT: Directed acyclic graph (DAG) for causal reasoning with interventions
 *       and counterfactual queries
 * WHY:  The reasoning system can do forward/backward chaining and convergent
 *       evidence accumulation, but cannot distinguish "A correlates with B"
 *       from "A causes B". Causal reasoning via do-calculus is essential for
 *       correct scientific and decision-making inference.
 * HOW:  DAG structure with nodes (variables) and edges (causal links).
 *       Three query types:
 *       - Association P(Y|X): observational conditioning
 *       - Intervention P(Y|do(X)): causal effect via do-operator
 *       - Counterfactual P(Y_x|X',Y'): hypothetical reasoning
 *
 * BIOLOGICAL BASIS:
 * Models the prefrontal cortex's ability to distinguish correlation from
 * causation. The do-operator models mental simulation of interventions,
 * analogous to how the PFC can mentally "cut" confounding influences to
 * reason about direct causal effects (counterfactual thinking).
 *
 * INTEGRATION:
 * - Reasoning chain: REASONING_STEP_CAUSAL step type
 * - Convergent architecture: causal module as evidence producer
 * - Config: enable_causal_reasoning (opt-in, default false)
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#ifndef NIMCP_REASONING_CAUSAL_H
#define NIMCP_REASONING_CAUSAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum nodes in the causal DAG */
#define CAUSAL_MAX_NODES 256

/** Maximum edges in the causal DAG */
#define CAUSAL_MAX_EDGES 1024

/** Maximum parents per node (fan-in limit) */
#define CAUSAL_MAX_PARENTS 16

/** Maximum name length for nodes and edges */
#define CAUSAL_MAX_NAME_LEN 64

/** Default prior probability for unobserved nodes */
#define CAUSAL_DEFAULT_PRIOR 0.5f

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief A variable node in the causal DAG
 *
 * WHAT: Represents a variable (random or deterministic) in the causal model
 * WHY:  Nodes hold observation/intervention state for query resolution
 */
typedef struct {
    uint32_t id;                          /**< Unique node identifier */
    char name[CAUSAL_MAX_NAME_LEN];       /**< Human-readable name */
    float prior_probability;               /**< Prior P(node) [0,1] */
    float observed_value;                  /**< Observed value (NAN if unobserved) */
    bool is_observed;                      /**< True if observed_value is set */
    bool is_intervened;                    /**< True if do-operator applied */
    float intervened_value;                /**< Intervention value */
} causal_node_t;

/**
 * @brief A directed causal edge in the DAG
 *
 * WHAT: Represents a causal relationship from_id -> to_id
 * WHY:  Edges encode the direction and strength of causal influence
 */
typedef struct {
    uint32_t from_id;                      /**< Source node ID */
    uint32_t to_id;                        /**< Target node ID */
    float strength;                        /**< Causal strength [0,1] */
    char description[CAUSAL_MAX_NAME_LEN]; /**< Description of relationship */
} causal_edge_t;

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Type of causal query
 *
 * WHAT: Categorizes queries by the causal inference level
 * WHY:  Each level requires different computation (Pearl's ladder of causation)
 */
typedef enum {
    CAUSAL_QUERY_ASSOCIATION,     /**< Level 1: P(Y|X) — seeing */
    CAUSAL_QUERY_INTERVENTION,    /**< Level 2: P(Y|do(X)) — doing */
    CAUSAL_QUERY_COUNTERFACTUAL   /**< Level 3: P(Y_x|X',Y') — imagining */
} causal_query_type_t;

/*=============================================================================
 * QUERY AND RESULT STRUCTURES
 *===========================================================================*/

/**
 * @brief Causal query specification
 *
 * WHAT: Describes what to compute in the causal DAG
 * WHY:  Unified query interface for all three levels of causal inference
 */
typedef struct {
    causal_query_type_t type;              /**< Query type */
    uint32_t target_id;                    /**< Node to query about */
    uint32_t condition_ids[8];             /**< Conditioning node IDs */
    float condition_values[8];             /**< Conditioning values */
    uint32_t num_conditions;               /**< Number of conditions */
    uint32_t intervention_id;              /**< Node to intervene on (for do/counterfactual) */
    float intervention_value;              /**< Intervention value */
} causal_query_t;

/**
 * @brief Result of a causal query
 *
 * WHAT: Contains the computed probability, confidence, and explanation
 * WHY:  Rich result structure for traceability and integration with reasoning chain
 */
typedef struct {
    float probability;                     /**< Computed probability [0,1] */
    float confidence;                      /**< Confidence in result [0,1] */
    bool is_causal;                        /**< True if causal link found, false if only correlation */
    float causal_strength;                 /**< Aggregate causal strength along path */
    char explanation[256];                 /**< Human-readable explanation */
    uint32_t path_length;                  /**< Length of causal path (0 if no path) */
} causal_result_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Configuration for the causal DAG
 *
 * WHAT: Tuneable parameters for causal reasoning
 * WHY:  Allow domain-specific tuning of causal inference behavior
 */
typedef struct {
    uint32_t max_nodes;                    /**< Maximum nodes (default CAUSAL_MAX_NODES) */
    uint32_t max_edges;                    /**< Maximum edges (default CAUSAL_MAX_EDGES) */
    float default_prior;                   /**< Default prior probability (default 0.5) */
    float propagation_damping;             /**< Decay per edge in propagation (default 0.9) */
} causal_dag_config_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Statistics for the causal DAG
 *
 * WHAT: Aggregate metrics across causal reasoning operations
 * WHY:  Monitor causal inference performance and usage patterns
 */
typedef struct {
    uint32_t num_nodes;                    /**< Current number of nodes */
    uint32_t num_edges;                    /**< Current number of edges */
    uint32_t num_queries;                  /**< Total queries executed */
    float avg_path_length;                 /**< Running average path length */
    uint32_t num_interventions;            /**< Total interventions performed */
    uint32_t num_counterfactuals;          /**< Total counterfactual queries */
} causal_dag_stats_t;

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

/** Opaque causal DAG handle */
typedef struct causal_dag causal_dag_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create a causal DAG
 *
 * WHAT: Allocate and initialize a causal DAG instance
 * WHY:  Required before any causal reasoning operations
 * HOW:  Allocate struct, copy config, zero statistics
 *
 * @param config DAG configuration (NULL for defaults)
 * @return DAG instance or NULL on allocation failure
 *
 * COMPLEXITY: O(1)
 */
causal_dag_t* causal_dag_create(const causal_dag_config_t* config);

/**
 * @brief Destroy a causal DAG
 *
 * WHAT: Free all DAG resources
 * WHY:  Prevent memory leaks
 *
 * @param dag DAG to destroy (NULL safe)
 *
 * COMPLEXITY: O(1)
 */
void causal_dag_destroy(causal_dag_t* dag);

/**
 * @brief Get default causal DAG configuration
 *
 * @return Default configuration struct
 */
causal_dag_config_t causal_dag_default_config(void);

/*=============================================================================
 * NODE AND EDGE MANAGEMENT
 *===========================================================================*/

/**
 * @brief Add a node to the causal DAG
 *
 * @param dag Causal DAG
 * @param name Human-readable node name
 * @param prior Prior probability [0,1]
 * @return Node ID (>= 0) on success, -1 on error
 */
int causal_dag_add_node(causal_dag_t* dag, const char* name, float prior);

/**
 * @brief Add a directed causal edge
 *
 * WHAT: Add edge from_id -> to_id with given causal strength
 * WHY:  Build the causal structure of the model
 * HOW:  Validate no cycle would be created, then add edge
 *
 * @param dag Causal DAG
 * @param from_id Source node ID
 * @param to_id Target node ID
 * @param strength Causal strength [0,1]
 * @return 0 on success, -1 on error (cycle, invalid IDs, etc.)
 */
int causal_dag_add_edge(causal_dag_t* dag, uint32_t from_id, uint32_t to_id,
                         float strength);

/**
 * @brief Remove a directed causal edge
 *
 * @param dag Causal DAG
 * @param from_id Source node ID
 * @param to_id Target node ID
 * @return 0 on success, -1 on error (edge not found)
 */
int causal_dag_remove_edge(causal_dag_t* dag, uint32_t from_id, uint32_t to_id);

/*=============================================================================
 * OBSERVATION AND INTERVENTION
 *===========================================================================*/

/**
 * @brief Set an observation on a node
 *
 * @param dag Causal DAG
 * @param node_id Node to observe
 * @param value Observed value
 * @return 0 on success, -1 on error
 */
int causal_dag_observe(causal_dag_t* dag, uint32_t node_id, float value);

/**
 * @brief Apply the do-operator (intervention) on a node
 *
 * WHAT: Set node to a fixed value, cutting all incoming causal edges
 * WHY:  The do-operator is the foundation of causal inference
 * HOW:  Mark node as intervened, store intervention value
 *
 * @param dag Causal DAG
 * @param node_id Node to intervene on
 * @param value Intervention value
 * @return 0 on success, -1 on error
 */
int causal_dag_intervene(causal_dag_t* dag, uint32_t node_id, float value);

/**
 * @brief Clear an intervention on a node
 *
 * @param dag Causal DAG
 * @param node_id Node to clear intervention
 * @return 0 on success, -1 on error
 */
int causal_dag_clear_intervention(causal_dag_t* dag, uint32_t node_id);

/*=============================================================================
 * QUERIES
 *===========================================================================*/

/**
 * @brief Execute a causal query
 *
 * WHAT: Compute probability for a causal query
 * WHY:  Core function for causal reasoning
 * HOW:  Dispatch based on query type (association/intervention/counterfactual)
 *
 * @param dag Causal DAG
 * @param query Query specification
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int causal_dag_query(causal_dag_t* dag, const causal_query_t* query,
                      causal_result_t* result);

/*=============================================================================
 * GRAPH OPERATIONS
 *===========================================================================*/

/**
 * @brief Find a directed path from source to target
 *
 * @param dag Causal DAG
 * @param from_id Source node ID
 * @param to_id Target node ID
 * @param path_out Output array for path (caller allocates, size >= CAUSAL_MAX_NODES)
 * @param path_len Output path length
 * @return 0 on success (path found), -1 on error (no path or invalid IDs)
 */
int causal_dag_find_path(const causal_dag_t* dag, uint32_t from_id,
                          uint32_t to_id, uint32_t* path_out, uint32_t* path_len);

/**
 * @brief Check if ancestor_id is an ancestor of descendant_id
 *
 * @param dag Causal DAG
 * @param ancestor_id Potential ancestor
 * @param descendant_id Potential descendant
 * @return true if ancestor_id is an ancestor
 */
bool causal_dag_is_ancestor(const causal_dag_t* dag, uint32_t ancestor_id,
                             uint32_t descendant_id);

/**
 * @brief Get parent node IDs for a given node
 *
 * @param dag Causal DAG
 * @param node_id Node to query
 * @param parents_out Output array (caller allocates, size >= CAUSAL_MAX_PARENTS)
 * @param count Output number of parents
 * @return 0 on success, -1 on error
 */
int causal_dag_get_parents(const causal_dag_t* dag, uint32_t node_id,
                            uint32_t* parents_out, uint32_t* count);

/**
 * @brief Get child node IDs for a given node
 *
 * @param dag Causal DAG
 * @param node_id Node to query
 * @param children_out Output array (caller allocates, size >= CAUSAL_MAX_NODES)
 * @param count Output number of children
 * @return 0 on success, -1 on error
 */
int causal_dag_get_children(const causal_dag_t* dag, uint32_t node_id,
                             uint32_t* children_out, uint32_t* count);

/*=============================================================================
 * STATISTICS AND VALIDATION
 *===========================================================================*/

/**
 * @brief Get DAG statistics
 *
 * @param dag Causal DAG
 * @param stats Output statistics struct
 * @return 0 on success, -1 on error
 */
int causal_dag_get_stats(const causal_dag_t* dag, causal_dag_stats_t* stats);

/**
 * @brief Validate DAG acyclicity
 *
 * WHAT: Check that the DAG contains no cycles
 * WHY:  A DAG by definition must be acyclic; cycles invalidate causal reasoning
 * HOW:  Kahn's algorithm for topological sort
 *
 * @param dag Causal DAG
 * @return 0 if valid (acyclic), -1 if invalid (has cycles or NULL)
 */
int causal_dag_validate(const causal_dag_t* dag);

/**
 * @brief Get node data by ID
 *
 * @param dag Causal DAG
 * @param node_id Node ID
 * @param out Output node struct
 * @return 0 on success, -1 on error (invalid ID)
 */
int causal_dag_get_node(const causal_dag_t* dag, uint32_t node_id,
                         causal_node_t* out);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_CAUSAL_H */
