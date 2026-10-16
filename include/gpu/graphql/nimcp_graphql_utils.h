/**
 * @file nimcp_graphql_utils.h
 * @brief Lightweight GraphQL-Inspired Query Interface for GPU Graph Operations
 *
 * WHAT: GraphQL-style query interface for GPU graph operations
 * WHY:  Provides a declarative query language for graph traversal and analysis
 * HOW:  Query parser, executor pattern, filter expression evaluation
 *
 * ARCHITECTURE:
 *
 *   +---------------------------------------------------------+
 *   |              GRAPHQL-STYLE QUERY INTERFACE               |
 *   |                                                         |
 *   |  +-------------+  +-------------+  +----------------+   |
 *   |  |   Parser    |  |  Executor   |  |   Filter Eval  |   |
 *   |  | (Query AST) |  | (Strategy)  |  | (Expressions)  |   |
 *   |  +-------------+  +-------------+  +----------------+   |
 *   |                          |                              |
 *   |              +-----------------------+                  |
 *   |              |   GPU Graph Backend   |                  |
 *   |              | (CSR, BFS, Centrality)|                  |
 *   |              +-----------------------+                  |
 *   +---------------------------------------------------------+
 *
 * QUERY TYPES:
 * - "vertices"   : Query vertex properties/features
 * - "edges"      : Query edge properties/weights
 * - "neighbors"  : Get neighbors of specified vertices
 * - "path"       : Find paths between vertices
 * - "subgraph"   : Extract induced subgraph
 *
 * FILTER SYNTAX:
 * - "degree > 5"          : Degree threshold
 * - "weight >= 0.5"       : Edge weight filter
 * - "centrality > 0.1"    : Centrality threshold
 * - "distance < 3"        : Distance-based filter
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_GRAPHQL_UTILS_H
#define NIMCP_GRAPHQL_UTILS_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//=============================================================================
// Constants
//=============================================================================

/** Maximum length for query type strings */
#define NIMCP_GRAPHQL_MAX_QUERY_TYPE_LEN 32

/** Maximum length for filter expressions */
#define NIMCP_GRAPHQL_MAX_FILTER_LEN 256

/** Maximum number of target vertices in a query */
#define NIMCP_GRAPHQL_MAX_VERTICES 1024

/** Maximum traversal depth */
#define NIMCP_GRAPHQL_MAX_DEPTH 100

//=============================================================================
// Query Types
//=============================================================================

/**
 * @brief Query type enumeration
 */
typedef enum nimcp_graphql_query_type_e {
    NIMCP_GRAPHQL_QUERY_VERTICES = 0,    /**< Query vertices by properties */
    NIMCP_GRAPHQL_QUERY_EDGES,           /**< Query edges by properties */
    NIMCP_GRAPHQL_QUERY_NEIGHBORS,       /**< Get neighbors of vertices */
    NIMCP_GRAPHQL_QUERY_PATH,            /**< Find paths between vertices */
    NIMCP_GRAPHQL_QUERY_SUBGRAPH,        /**< Extract induced subgraph */
    NIMCP_GRAPHQL_QUERY_CENTRALITY,      /**< Compute centrality measures */
    NIMCP_GRAPHQL_QUERY_CLUSTERING,      /**< Compute clustering coefficients */
    NIMCP_GRAPHQL_QUERY_COUNT            /**< Number of query types */
} nimcp_graphql_query_type_t;

/**
 * @brief Filter operator enumeration
 */
typedef enum nimcp_graphql_filter_op_e {
    NIMCP_GRAPHQL_OP_EQ = 0,     /**< Equal (==) */
    NIMCP_GRAPHQL_OP_NE,         /**< Not equal (!=) */
    NIMCP_GRAPHQL_OP_LT,         /**< Less than (<) */
    NIMCP_GRAPHQL_OP_LE,         /**< Less than or equal (<=) */
    NIMCP_GRAPHQL_OP_GT,         /**< Greater than (>) */
    NIMCP_GRAPHQL_OP_GE,         /**< Greater than or equal (>=) */
    NIMCP_GRAPHQL_OP_AND,        /**< Logical AND */
    NIMCP_GRAPHQL_OP_OR,         /**< Logical OR */
    NIMCP_GRAPHQL_OP_NOT,        /**< Logical NOT */
    NIMCP_GRAPHQL_OP_COUNT       /**< Number of operators */
} nimcp_graphql_filter_op_t;

/**
 * @brief Filter field enumeration
 */
typedef enum nimcp_graphql_filter_field_e {
    NIMCP_GRAPHQL_FIELD_DEGREE = 0,      /**< Vertex degree */
    NIMCP_GRAPHQL_FIELD_WEIGHT,          /**< Edge weight */
    NIMCP_GRAPHQL_FIELD_CENTRALITY,      /**< Centrality score */
    NIMCP_GRAPHQL_FIELD_CLUSTERING,      /**< Clustering coefficient */
    NIMCP_GRAPHQL_FIELD_DISTANCE,        /**< Distance from source */
    NIMCP_GRAPHQL_FIELD_ID,              /**< Vertex/edge ID */
    NIMCP_GRAPHQL_FIELD_FEATURE,         /**< Feature vector element */
    NIMCP_GRAPHQL_FIELD_COUNT            /**< Number of fields */
} nimcp_graphql_filter_field_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Parsed filter expression node
 */
typedef struct nimcp_graphql_filter_node_s {
    nimcp_graphql_filter_op_t op;        /**< Operator type */
    nimcp_graphql_filter_field_t field;  /**< Field to filter on */
    float value;                          /**< Comparison value */
    int feature_index;                    /**< Feature index (for FIELD_FEATURE) */
    struct nimcp_graphql_filter_node_s* left;  /**< Left child (for binary ops) */
    struct nimcp_graphql_filter_node_s* right; /**< Right child (for binary ops) */
} nimcp_graphql_filter_node_t;

/**
 * @brief Query result structure
 */
typedef struct nimcp_graphql_result_s {
    void* data;                  /**< Result data (type depends on query) */
    size_t num_elements;         /**< Number of result elements */
    size_t element_size;         /**< Size of each element in bytes */
    nimcp_graphql_query_type_t query_type; /**< Type of query that produced result */
    bool on_device;              /**< Whether data is on GPU device */
} nimcp_graphql_result_t;

/**
 * @brief Graph query structure
 *
 * WHAT: Encapsulates a graph query request
 * WHY:  Provides a declarative interface for graph operations
 * HOW:  Combines query type, filters, and target vertices
 */
typedef struct nimcp_graph_query_s {
    char query_type[NIMCP_GRAPHQL_MAX_QUERY_TYPE_LEN];  /**< Query type string */
    char filter_expression[NIMCP_GRAPHQL_MAX_FILTER_LEN]; /**< Filter expression */
    int* vertex_ids;             /**< Target vertices (host memory) */
    size_t num_vertices;         /**< Number of target vertices */
    int depth;                   /**< Traversal depth for path/neighbor queries */
    nimcp_graphql_result_t* result; /**< Query result (set by executor) */
} nimcp_graph_query_t;

/**
 * @brief GraphQL executor structure (Strategy pattern)
 *
 * WHAT: Executes graph queries using a strategy pattern
 * WHY:  Enables different backends (GPU, CPU) and extensibility
 * HOW:  Function pointers for execute and parse operations
 */
typedef struct nimcp_graphql_executor_s {
    /**
     * @brief Execute a parsed query
     * @param self Executor instance
     * @param query Query to execute
     * @return Query result or NULL on failure
     */
    void* (*execute)(struct nimcp_graphql_executor_s* self, nimcp_graph_query_t* query);

    /**
     * @brief Parse a filter expression
     * @param self Executor instance
     * @param filter Filter expression string
     * @return Parsed filter tree or NULL on parse error
     */
    nimcp_graphql_filter_node_t* (*parse_filter)(struct nimcp_graphql_executor_s* self,
                                                   const char* filter);

    /**
     * @brief Evaluate filter on a vertex
     * @param self Executor instance
     * @param filter Parsed filter tree
     * @param vertex_id Vertex to evaluate
     * @return true if vertex passes filter
     */
    bool (*evaluate_filter)(struct nimcp_graphql_executor_s* self,
                           const nimcp_graphql_filter_node_t* filter,
                           int vertex_id);

    nimcp_gpu_context_t* gpu_ctx;    /**< GPU context for device operations */
    void* graph;                      /**< Reference to GPU graph */
    nimcp_graphql_filter_node_t* current_filter; /**< Currently parsed filter */
} nimcp_graphql_executor_t;

//=============================================================================
// Executor Lifecycle
//=============================================================================

/**
 * @brief Create a GraphQL executor with GPU context
 *
 * @param gpu_context GPU context for CUDA operations
 * @return Executor instance or NULL on failure
 */
NIMCP_EXPORT nimcp_graphql_executor_t* nimcp_graphql_executor_create(
    nimcp_gpu_context_t* gpu_context
);

/**
 * @brief Destroy a GraphQL executor
 *
 * @param exec Executor to destroy (can be NULL)
 */
NIMCP_EXPORT void nimcp_graphql_executor_destroy(nimcp_graphql_executor_t* exec);

/**
 * @brief Set the graph to query
 *
 * @param exec Executor instance
 * @param graph GPU graph (nimcp_gpu_graph_t*)
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_graphql_executor_set_graph(
    nimcp_graphql_executor_t* exec,
    void* graph
);

//=============================================================================
// Query Execution
//=============================================================================

/**
 * @brief Execute a GraphQL-style query string
 *
 * WHAT: Parses and executes a graph query
 * WHY:  Provides a high-level declarative interface
 * HOW:  Parse query string, build query struct, execute, return result
 *
 * Query string format:
 *   "{ <query_type>(<params>) [where <filter>] }"
 *
 * Examples:
 *   "{ vertices(ids: [1,2,3]) where degree > 5 }"
 *   "{ neighbors(id: 0, depth: 2) }"
 *   "{ path(from: 0, to: 10) }"
 *   "{ subgraph(ids: [1,2,3,4]) }"
 *
 * @param exec Executor instance
 * @param query_string Query string in GraphQL-like format
 * @param result Output: query result (caller must free with result_destroy)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
NIMCP_EXPORT nimcp_error_t nimcp_graphql_execute(
    nimcp_graphql_executor_t* exec,
    const char* query_string,
    void** result
);

/**
 * @brief Execute a structured query
 *
 * @param exec Executor instance
 * @param query Query structure (must be initialized)
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_graphql_execute_query(
    nimcp_graphql_executor_t* exec,
    nimcp_graph_query_t* query
);

//=============================================================================
// Query Builder API
//=============================================================================

/**
 * @brief Create a new query structure
 *
 * @return New query or NULL on allocation failure
 */
NIMCP_EXPORT nimcp_graph_query_t* nimcp_graph_query_create(void);

/**
 * @brief Destroy a query structure
 *
 * @param query Query to destroy
 */
NIMCP_EXPORT void nimcp_graph_query_destroy(nimcp_graph_query_t* query);

/**
 * @brief Set query type
 *
 * @param query Query structure
 * @param type Query type string ("vertices", "edges", etc.)
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_graph_query_set_type(
    nimcp_graph_query_t* query,
    const char* type
);

/**
 * @brief Set filter expression
 *
 * @param query Query structure
 * @param filter Filter expression string
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_graph_query_set_filter(
    nimcp_graph_query_t* query,
    const char* filter
);

/**
 * @brief Set target vertices
 *
 * @param query Query structure
 * @param vertex_ids Array of vertex IDs
 * @param num_vertices Number of vertices
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_graph_query_set_vertices(
    nimcp_graph_query_t* query,
    const int* vertex_ids,
    size_t num_vertices
);

/**
 * @brief Set traversal depth
 *
 * @param query Query structure
 * @param depth Traversal depth
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_graph_query_set_depth(
    nimcp_graph_query_t* query,
    int depth
);

//=============================================================================
// Filter Expression API
//=============================================================================

/**
 * @brief Parse a filter expression into an AST
 *
 * @param filter_str Filter expression string
 * @return Parsed filter tree or NULL on error
 */
NIMCP_EXPORT nimcp_graphql_filter_node_t* nimcp_graphql_parse_filter(
    const char* filter_str
);

/**
 * @brief Destroy a filter expression tree
 *
 * @param filter Filter tree to destroy
 */
NIMCP_EXPORT void nimcp_graphql_filter_destroy(nimcp_graphql_filter_node_t* filter);

/**
 * @brief Evaluate a filter expression on vertex data
 *
 * @param filter Parsed filter tree
 * @param degree Vertex degree
 * @param weight Edge weight (if applicable)
 * @param centrality Centrality score
 * @param clustering Clustering coefficient
 * @param distance Distance from source
 * @param features Feature vector (can be NULL)
 * @param num_features Number of features
 * @return true if filter passes
 */
NIMCP_EXPORT bool nimcp_graphql_filter_evaluate(
    const nimcp_graphql_filter_node_t* filter,
    int degree,
    float weight,
    float centrality,
    float clustering,
    float distance,
    const float* features,
    size_t num_features
);

/**
 * @brief Create a simple comparison filter node
 *
 * @param field Field to compare
 * @param op Comparison operator
 * @param value Comparison value
 * @return New filter node or NULL
 */
NIMCP_EXPORT nimcp_graphql_filter_node_t* nimcp_graphql_filter_create_comparison(
    nimcp_graphql_filter_field_t field,
    nimcp_graphql_filter_op_t op,
    float value
);

/**
 * @brief Create a binary logical filter node (AND, OR)
 *
 * @param op Logical operator (AND or OR)
 * @param left Left operand filter
 * @param right Right operand filter
 * @return New filter node or NULL
 */
NIMCP_EXPORT nimcp_graphql_filter_node_t* nimcp_graphql_filter_create_logical(
    nimcp_graphql_filter_op_t op,
    nimcp_graphql_filter_node_t* left,
    nimcp_graphql_filter_node_t* right
);

//=============================================================================
// Result Handling
//=============================================================================

/**
 * @brief Destroy a query result
 *
 * @param result Result to destroy
 */
NIMCP_EXPORT void nimcp_graphql_result_destroy(nimcp_graphql_result_t* result);

/**
 * @brief Copy result data to host memory
 *
 * @param result Query result (may be on device)
 * @param host_buffer Pre-allocated host buffer
 * @param buffer_size Size of host buffer in bytes
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_graphql_result_to_host(
    const nimcp_graphql_result_t* result,
    void* host_buffer,
    size_t buffer_size
);

/**
 * @brief Get result as int array (for vertex IDs)
 *
 * @param result Query result
 * @param out_array Output array (caller allocated)
 * @param max_elements Maximum elements to copy
 * @return Number of elements copied
 */
NIMCP_EXPORT size_t nimcp_graphql_result_get_ints(
    const nimcp_graphql_result_t* result,
    int* out_array,
    size_t max_elements
);

/**
 * @brief Get result as float array (for weights, scores)
 *
 * @param result Query result
 * @param out_array Output array (caller allocated)
 * @param max_elements Maximum elements to copy
 * @return Number of elements copied
 */
NIMCP_EXPORT size_t nimcp_graphql_result_get_floats(
    const nimcp_graphql_result_t* result,
    float* out_array,
    size_t max_elements
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert query type string to enum
 *
 * @param type_str Query type string
 * @return Query type enum value
 */
NIMCP_EXPORT nimcp_graphql_query_type_t nimcp_graphql_query_type_from_string(
    const char* type_str
);

/**
 * @brief Convert query type enum to string
 *
 * @param type Query type enum
 * @return Query type string
 */
NIMCP_EXPORT const char* nimcp_graphql_query_type_to_string(
    nimcp_graphql_query_type_t type
);

/**
 * @brief Validate a query string syntax
 *
 * @param query_string Query string to validate
 * @param error_msg Output: error message buffer (can be NULL)
 * @param error_msg_size Size of error message buffer
 * @return true if query is valid
 */
NIMCP_EXPORT bool nimcp_graphql_validate_query(
    const char* query_string,
    char* error_msg,
    size_t error_msg_size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRAPHQL_UTILS_H */
