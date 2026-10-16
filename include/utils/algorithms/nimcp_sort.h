/**
 * @file nimcp_sort.h
 * @brief Consolidated Sorting and Graph Algorithms for NIMCP
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Central repository for sorting and graph traversal algorithms
 * WHY:  Eliminate code duplication of common algorithms across modules
 * HOW:  Generic callback-based APIs that work with any data structure
 *
 * ALGORITHMS PROVIDED:
 * - Topological Sort (Kahn's algorithm) - O(V + E)
 * - Comparison Sort (qsort wrapper) - O(n log n)
 * - Insertion Sort (for small arrays) - O(n^2) but fast for n < 16
 * - BFS Graph Traversal - O(V + E)
 * - DFS Graph Traversal - O(V + E)
 *
 * USAGE PATTERN:
 * All graph algorithms use callbacks to access graph structure,
 * allowing them to work with any underlying data representation.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SORT_H
#define NIMCP_SORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Threshold below which insertion sort is used instead of qsort */
#define NIMCP_SORT_INSERTION_THRESHOLD 16

/** Maximum reasonable graph size for stack-allocated work buffers */
#define NIMCP_SORT_MAX_STACK_NODES 256

/* ============================================================================
 * Return Codes
 * ============================================================================ */

typedef enum nimcp_sort_result {
    NIMCP_SORT_OK              =  0,   /**< Success */
    NIMCP_SORT_ERROR_NULL      = -1,   /**< NULL parameter */
    NIMCP_SORT_ERROR_CYCLE     = -2,   /**< Cycle detected in graph */
    NIMCP_SORT_ERROR_MEMORY    = -3,   /**< Memory allocation failed */
    NIMCP_SORT_ERROR_OVERFLOW  = -4,   /**< Output buffer too small */
    NIMCP_SORT_ERROR_CALLBACK  = -5,   /**< Callback returned error */
    NIMCP_SORT_ERROR_INVALID   = -6    /**< Invalid parameter */
} nimcp_sort_result_t;

/* ============================================================================
 * Callback Types for Graph Algorithms
 * ============================================================================ */

/**
 * @brief Callback to get number of dependencies (in-edges) for a node
 *
 * @param node_index  Index of the node (0 to node_count-1)
 * @param user_data   User-provided context pointer
 * @return Number of dependencies, or UINT32_MAX on error
 */
typedef uint32_t (*nimcp_get_dependency_count_fn)(
    uint32_t node_index,
    void* user_data
);

/**
 * @brief Callback to get a specific dependency of a node
 *
 * @param node_index  Index of the node
 * @param dep_index   Index of the dependency (0 to dep_count-1)
 * @param user_data   User-provided context pointer
 * @return Index of the dependency node, or UINT32_MAX on error
 */
typedef uint32_t (*nimcp_get_dependency_fn)(
    uint32_t node_index,
    uint32_t dep_index,
    void* user_data
);

/**
 * @brief Callback to get number of dependents (out-edges) for a node
 *
 * For some algorithms, it's more efficient to iterate dependents than
 * to search all nodes for dependencies.
 *
 * @param node_index  Index of the node (0 to node_count-1)
 * @param user_data   User-provided context pointer
 * @return Number of dependents, or UINT32_MAX on error
 */
typedef uint32_t (*nimcp_get_dependent_count_fn)(
    uint32_t node_index,
    void* user_data
);

/**
 * @brief Callback to get a specific dependent of a node
 *
 * @param node_index  Index of the node
 * @param dep_index   Index of the dependent (0 to dependent_count-1)
 * @param user_data   User-provided context pointer
 * @return Index of the dependent node, or UINT32_MAX on error
 */
typedef uint32_t (*nimcp_get_dependent_fn)(
    uint32_t node_index,
    uint32_t dep_index,
    void* user_data
);

/**
 * @brief Callback to get number of neighbors (for undirected graphs)
 *
 * @param node_index  Index of the node
 * @param user_data   User-provided context pointer
 * @return Number of neighbors, or UINT32_MAX on error
 */
typedef uint32_t (*nimcp_get_neighbor_count_fn)(
    uint32_t node_index,
    void* user_data
);

/**
 * @brief Callback to get a specific neighbor
 *
 * @param node_index     Index of the node
 * @param neighbor_index Index of the neighbor
 * @param user_data      User-provided context pointer
 * @return Index of the neighbor node, or UINT32_MAX on error
 */
typedef uint32_t (*nimcp_get_neighbor_fn)(
    uint32_t node_index,
    uint32_t neighbor_index,
    void* user_data
);

/**
 * @brief Visitor callback for graph traversal
 *
 * @param node_index  Index of the visited node
 * @param depth       Depth from starting node (0 for start)
 * @param user_data   User-provided context pointer
 * @return true to continue traversal, false to stop
 */
typedef bool (*nimcp_visit_fn)(
    uint32_t node_index,
    uint32_t depth,
    void* user_data
);

/* ============================================================================
 * Graph Access Structures
 * ============================================================================ */

/**
 * @brief Configuration for topological sort
 *
 * Provides callbacks to access dependency graph structure.
 * Use either dependency callbacks OR dependent callbacks, not both.
 */
typedef struct nimcp_topo_config {
    uint32_t node_count;                      /**< Total number of nodes */
    void* user_data;                          /**< User context for callbacks */

    /* Dependency access (A depends on B = edge B -> A) */
    nimcp_get_dependency_count_fn get_dep_count;  /**< Get incoming edge count */
    nimcp_get_dependency_fn get_dep;              /**< Get incoming edge */

    /* Optional: Dependent access for efficiency (edge A -> B) */
    nimcp_get_dependent_count_fn get_dependent_count;  /**< Get outgoing edge count */
    nimcp_get_dependent_fn get_dependent;              /**< Get outgoing edge */
} nimcp_topo_config_t;

/**
 * @brief Configuration for graph traversal (BFS/DFS)
 */
typedef struct nimcp_traversal_config {
    uint32_t node_count;                      /**< Total number of nodes */
    uint32_t start_node;                      /**< Starting node index */
    void* user_data;                          /**< User context for callbacks */

    /* Neighbor access */
    nimcp_get_neighbor_count_fn get_neighbor_count;
    nimcp_get_neighbor_fn get_neighbor;

    /* Visitor callback */
    nimcp_visit_fn visit;
    void* visit_user_data;                    /**< Separate context for visitor */
} nimcp_traversal_config_t;

/* ============================================================================
 * Topological Sort API
 * ============================================================================ */

/**
 * @brief Perform topological sort on a directed acyclic graph
 *
 * WHAT: Sort nodes so dependencies come before dependents
 * WHY:  Required for ordered startup, build systems, inference propagation
 * HOW:  Kahn's algorithm - O(V + E) time, O(V) space
 *
 * The output array will contain node indices in topological order,
 * meaning for any edge A -> B, A will appear before B in the output.
 *
 * @param config      Graph configuration with callbacks
 * @param order_out   Output array for sorted indices
 * @param max_order   Size of output array
 * @param sorted_count Output: number of nodes successfully sorted
 * @return NIMCP_SORT_OK on success, NIMCP_SORT_ERROR_CYCLE if cycle detected
 *
 * @note If a cycle exists, the function returns NIMCP_SORT_ERROR_CYCLE
 *       and sorted_count indicates how many nodes were processed before
 *       the cycle was detected.
 */
nimcp_sort_result_t nimcp_topological_sort(
    const nimcp_topo_config_t* config,
    uint32_t* order_out,
    uint32_t max_order,
    uint32_t* sorted_count
);

/**
 * @brief Check if a directed graph has cycles
 *
 * WHAT: Detect cycles without producing sorted output
 * WHY:  Validation before attempting topological sort
 * HOW:  Uses same algorithm but discards output
 *
 * @param config  Graph configuration with callbacks
 * @return true if graph has cycles, false if acyclic
 */
bool nimcp_has_cycle(const nimcp_topo_config_t* config);

/**
 * @brief Get nodes that are part of a cycle
 *
 * WHAT: Identify which nodes participate in cycles
 * WHY:  Error reporting and debugging
 * HOW:  Nodes not included in topological sort are in cycles
 *
 * @param config       Graph configuration with callbacks
 * @param cycle_nodes  Output array for cycle node indices
 * @param max_nodes    Size of output array
 * @param cycle_count  Output: number of nodes in cycles
 * @return NIMCP_SORT_OK on success
 */
nimcp_sort_result_t nimcp_find_cycle_nodes(
    const nimcp_topo_config_t* config,
    uint32_t* cycle_nodes,
    uint32_t max_nodes,
    uint32_t* cycle_count
);

/* ============================================================================
 * Comparison Sort API
 * ============================================================================ */

/**
 * @brief Sort an array using optimal algorithm for size
 *
 * WHAT: Generic comparison sort with automatic algorithm selection
 * WHY:  Insertion sort is faster for small arrays, qsort for large
 * HOW:  Uses insertion sort for n < NIMCP_SORT_INSERTION_THRESHOLD
 *
 * @param base      Pointer to array to sort
 * @param nmemb     Number of elements
 * @param size      Size of each element in bytes
 * @param compare   Comparison function (same as qsort)
 */
void nimcp_sort(
    void* base,
    size_t nmemb,
    size_t size,
    int (*compare)(const void*, const void*)
);

/**
 * @brief Sort an array with user context for comparison
 *
 * WHAT: Sort with stateful comparison function
 * WHY:  Sometimes comparison needs external state
 * HOW:  Wrapper around qsort_r or manual implementation
 *
 * @param base      Pointer to array to sort
 * @param nmemb     Number of elements
 * @param size      Size of each element in bytes
 * @param compare   Comparison function with context
 * @param context   User-provided context passed to compare
 */
void nimcp_sort_r(
    void* base,
    size_t nmemb,
    size_t size,
    int (*compare)(const void*, const void*, void*),
    void* context
);

/**
 * @brief Insertion sort for small arrays
 *
 * WHAT: Simple O(n^2) sort with low overhead
 * WHY:  Faster than qsort for small n due to less overhead
 * HOW:  Standard insertion sort with byte-level swapping
 *
 * @param base    Pointer to array
 * @param nmemb   Number of elements
 * @param size    Size of each element
 * @param compare Comparison function
 */
void nimcp_insertion_sort(
    void* base,
    size_t nmemb,
    size_t size,
    int (*compare)(const void*, const void*)
);

/* ============================================================================
 * Graph Traversal API
 * ============================================================================ */

/**
 * @brief Breadth-first search traversal
 *
 * WHAT: Visit nodes in BFS order from starting node
 * WHY:  Shortest path finding, level-order processing
 * HOW:  Queue-based iteration - O(V + E)
 *
 * @param config  Traversal configuration
 * @return NIMCP_SORT_OK on success, or error code
 */
nimcp_sort_result_t nimcp_bfs(const nimcp_traversal_config_t* config);

/**
 * @brief Depth-first search traversal
 *
 * WHAT: Visit nodes in DFS order from starting node
 * WHY:  Connectivity, topological sorting, cycle detection
 * HOW:  Stack-based iteration (iterative, not recursive) - O(V + E)
 *
 * @param config  Traversal configuration
 * @return NIMCP_SORT_OK on success, or error code
 */
nimcp_sort_result_t nimcp_dfs(const nimcp_traversal_config_t* config);

/**
 * @brief Find all connected components
 *
 * WHAT: Group nodes into connected components
 * WHY:  Identify disconnected subgraphs
 * HOW:  BFS from each unvisited node
 *
 * @param config          Traversal configuration (start_node ignored)
 * @param component_ids   Output: component ID for each node (0-indexed)
 * @param num_components  Output: total number of components
 * @return NIMCP_SORT_OK on success
 */
nimcp_sort_result_t nimcp_find_components(
    const nimcp_traversal_config_t* config,
    uint32_t* component_ids,
    uint32_t* num_components
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Reverse an array of uint32_t in place
 *
 * @param array  Array to reverse
 * @param count  Number of elements
 */
void nimcp_reverse_u32(uint32_t* array, uint32_t count);

/**
 * @brief Binary search in sorted uint32_t array
 *
 * @param array   Sorted array to search
 * @param count   Number of elements
 * @param value   Value to find
 * @return Index of value if found, UINT32_MAX otherwise
 */
uint32_t nimcp_binary_search_u32(
    const uint32_t* array,
    uint32_t count,
    uint32_t value
);

/**
 * @brief Check if uint32_t array is sorted
 *
 * @param array  Array to check
 * @param count  Number of elements
 * @return true if sorted in ascending order
 */
bool nimcp_is_sorted_u32(const uint32_t* array, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SORT_H */
