/**
 * @file nimcp_hypergraph.h
 * @brief Hypergraph Data Structure for N-ary Relations
 *
 * Implements a hypergraph structure where edges can connect arbitrary numbers
 * of vertices, enabling representation of n-ary relations in knowledge
 * representation and mathematical reasoning.
 *
 * Key Concepts:
 * - Hyperedges connect 1 to N vertices (not just 2 like regular graphs)
 * - Supports typed edges (relations, constraints, rules, theorems)
 * - Incidence structure for efficient queries
 * - Dual hypergraph computation
 * - Transversal computation for constraint satisfaction
 *
 * Biological Basis:
 * - Semantic networks in temporal lobe
 * - Multi-modal association cortex binding
 * - Hippocampal relational memory encoding
 *
 * Integration:
 * - Extends nimcp_graph_ternary.h for higher-arity relations
 * - Integrates with knowledge graph systems
 * - Supports neuro-symbolic reasoning
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_HYPERGRAPH_H
#define NIMCP_HYPERGRAPH_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/containers/nimcp_graph_ternary.h"
#include "async/nimcp_bio_async.h"
#include "utils/exception/nimcp_exception_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Module Constants
 * ============================================================================ */

/** Bio-async module identifier for hypergraph */
#define BIO_MODULE_HYPERGRAPH           0x0394

/** Maximum vertices in a single hyperedge */
#define HYPERGRAPH_MAX_EDGE_VERTICES    32

/** Maximum vertices in hypergraph */
#define HYPERGRAPH_MAX_VERTICES         100000

/** Maximum edges in hypergraph */
#define HYPERGRAPH_MAX_EDGES            500000

/** Maximum label length */
#define HYPERGRAPH_MAX_LABEL_LEN        64

/** Default initial capacity */
#define HYPERGRAPH_DEFAULT_CAPACITY     1024

/* ============================================================================
 * Hyperedge Types
 * ============================================================================ */

/**
 * @brief Types of hyperedges in the knowledge representation
 */
typedef enum {
    HYPEREDGE_RELATION = 0,          /**< N-ary relation (e.g., Between(A,B,C)) */
    HYPEREDGE_CONSTRAINT,            /**< Multi-variable constraint */
    HYPEREDGE_RULE,                  /**< Inference rule */
    HYPEREDGE_THEOREM,               /**< Mathematical theorem */
    HYPEREDGE_DEFINITION,            /**< Mathematical definition */
    HYPEREDGE_AXIOM,                 /**< Foundational axiom */
    HYPEREDGE_LEMMA,                 /**< Supporting lemma */
    HYPEREDGE_COROLLARY,             /**< Derived corollary */
    HYPEREDGE_FUNCTION,              /**< Functional mapping */
    HYPEREDGE_EQUIVALENCE,           /**< Equivalence relation */
    HYPEREDGE_ORDERING,              /**< Ordering relation */
    HYPEREDGE_TYPE_COUNT             /**< Total edge types */
} hyperedge_type_t;

/**
 * @brief Vertex types in the hypergraph
 */
typedef enum {
    HYPERVERTEX_CONSTANT = 0,        /**< Mathematical constant */
    HYPERVERTEX_VARIABLE,            /**< Bound or free variable */
    HYPERVERTEX_FUNCTION,            /**< Function symbol */
    HYPERVERTEX_PREDICATE,           /**< Predicate symbol */
    HYPERVERTEX_SET,                 /**< Set or collection */
    HYPERVERTEX_TYPE,                /**< Type definition */
    HYPERVERTEX_PROOF_STEP,          /**< Step in a proof */
    HYPERVERTEX_EXPRESSION,          /**< Mathematical expression */
    HYPERVERTEX_TYPE_COUNT           /**< Total vertex types */
} hypervertex_type_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Vertex in a hypergraph
 */
typedef struct nimcp_hypervertex {
    uint32_t id;                     /**< Unique vertex identifier */
    hypervertex_type_t type;         /**< Vertex type */
    char label[HYPERGRAPH_MAX_LABEL_LEN]; /**< Human-readable label */
    float value;                     /**< Numeric value if applicable */
    void* data;                      /**< Optional associated data */
    uint32_t data_size;              /**< Size of associated data */

    /* Incidence tracking */
    uint32_t* incident_edges;        /**< IDs of incident hyperedges */
    uint32_t incident_count;         /**< Number of incident edges */
    uint32_t incident_capacity;      /**< Capacity of incident array */

    /* Metadata */
    float confidence;                /**< Confidence in this vertex */
    uint64_t created_time_us;        /**< Creation timestamp */
    uint64_t modified_time_us;       /**< Last modification timestamp */
} nimcp_hypervertex_t;

/**
 * @brief Hyperedge connecting multiple vertices
 */
typedef struct nimcp_hyperedge {
    uint32_t id;                     /**< Unique edge identifier */
    hyperedge_type_t type;           /**< Edge type */
    char label[HYPERGRAPH_MAX_LABEL_LEN]; /**< Human-readable label */

    /* Connected vertices */
    uint32_t* vertices;              /**< Array of vertex IDs */
    uint32_t vertex_count;           /**< Number of vertices */
    uint32_t vertex_capacity;        /**< Capacity of vertices array */

    /* Edge properties */
    trit_t weight;                   /**< Ternary weight (STRONG/WEAK/ABSENT) */
    float confidence;                /**< Confidence in this edge */
    bool is_directed;                /**< Whether edge is directed */
    bool is_symmetric;               /**< Whether relation is symmetric */

    /* Logical properties */
    bool is_reflexive;               /**< Reflexive relation */
    bool is_transitive;              /**< Transitive relation */
    bool is_antisymmetric;           /**< Antisymmetric relation */

    /* Timestamps */
    uint64_t created_time_us;        /**< Creation timestamp */
    uint64_t modified_time_us;       /**< Last modification timestamp */
} nimcp_hyperedge_t;

/**
 * @brief Incidence matrix entry for efficient queries
 */
typedef struct incidence_entry {
    uint32_t vertex_id;              /**< Vertex ID */
    uint32_t edge_id;                /**< Edge ID */
    uint32_t position;               /**< Position in edge's vertex list */
    struct incidence_entry* next;    /**< Hash chain */
} incidence_entry_t;

/**
 * @brief Query result for hypergraph traversal
 */
typedef struct hypergraph_query_result {
    uint32_t* vertex_ids;            /**< Matching vertex IDs */
    uint32_t vertex_count;           /**< Number of vertices */
    uint32_t* edge_ids;              /**< Matching edge IDs */
    uint32_t edge_count;             /**< Number of edges */
    float total_weight;              /**< Sum of edge weights */
    float avg_confidence;            /**< Average confidence */
} hypergraph_query_result_t;

/**
 * @brief Dual hypergraph (vertices and edges swapped)
 */
typedef struct nimcp_hypergraph_dual {
    struct nimcp_hypergraph* dual;   /**< The dual hypergraph */
    uint32_t* vertex_to_edge_map;    /**< Original vertices -> dual edges */
    uint32_t* edge_to_vertex_map;    /**< Original edges -> dual vertices */
} nimcp_hypergraph_dual_t;

/**
 * @brief Configuration for hypergraph
 */
typedef struct hypergraph_config {
    uint32_t initial_vertex_capacity;  /**< Initial vertex array size */
    uint32_t initial_edge_capacity;    /**< Initial edge array size */
    uint32_t incidence_hash_size;      /**< Hash table size for incidence */
    bool enable_incidence_index;       /**< Maintain incidence index */
    bool enable_thread_safety;         /**< Enable mutex protection */
    bool enable_bio_async;             /**< Enable async messaging */
    float default_confidence;          /**< Default confidence for new entries */
} hypergraph_config_t;

/**
 * @brief Statistics for hypergraph
 */
typedef struct hypergraph_stats {
    uint32_t vertex_count;           /**< Current vertex count */
    uint32_t edge_count;             /**< Current edge count */
    uint32_t max_edge_arity;         /**< Maximum edge arity seen */
    float avg_edge_arity;            /**< Average edge arity */
    uint32_t total_incidences;       /**< Total vertex-edge incidences */

    /* Edge type distribution */
    uint32_t edge_type_counts[HYPEREDGE_TYPE_COUNT];

    /* Vertex type distribution */
    uint32_t vertex_type_counts[HYPERVERTEX_TYPE_COUNT];

    /* Performance */
    uint64_t total_queries;          /**< Total queries performed */
    uint64_t total_query_time_us;    /**< Total query time */
    float avg_query_time_us;         /**< Average query time */
} hypergraph_stats_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Opaque handle to hypergraph
 */
typedef struct nimcp_hypergraph nimcp_hypergraph_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create a new hypergraph
 *
 * @return Hypergraph handle or NULL on failure
 */
NIMCP_API nimcp_hypergraph_t* nimcp_hypergraph_create(void);

/**
 * @brief W7: Register hypergraph with brain's internal KG.
 *
 * Creates 'cog_neuro_symbolic_hypergraph' structural root (if absent) and
 * emits one sync event with current vertex/edge counts.  Idempotent.
 *
 * Forward-declared as void* brain to avoid circular include.
 *
 * @param hg    Hypergraph (required).
 * @param brain Owning brain (required).
 * @return 0 on success, -1 on NULL arg.
 */
struct brain_struct;
NIMCP_API int nimcp_hypergraph_kg_register(
    nimcp_hypergraph_t* hg,
    struct brain_struct* brain);

/**
 * @brief Create hypergraph with configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Hypergraph handle or NULL on failure
 */
NIMCP_API nimcp_hypergraph_t* nimcp_hypergraph_create_with_config(
    const hypergraph_config_t* config);

/**
 * @brief Destroy hypergraph
 *
 * @param hg Hypergraph to destroy
 */
NIMCP_API void nimcp_hypergraph_destroy(nimcp_hypergraph_t* hg);

/**
 * @brief Clear all vertices and edges
 *
 * @param hg The hypergraph
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_clear(nimcp_hypergraph_t* hg);

/**
 * @brief Get default configuration
 *
 * @param config Configuration to fill
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_get_default_config(
    hypergraph_config_t* config);

/* ============================================================================
 * Vertex Operations
 * ============================================================================ */

/**
 * @brief Add a vertex to the hypergraph
 *
 * @param hg The hypergraph
 * @param type Vertex type
 * @param label Vertex label
 * @param confidence Initial confidence
 * @return Vertex ID or UINT32_MAX on failure
 */
NIMCP_API uint32_t nimcp_hypergraph_add_vertex(
    nimcp_hypergraph_t* hg,
    hypervertex_type_t type,
    const char* label,
    float confidence);

/**
 * @brief Add vertex with associated data
 *
 * @param hg The hypergraph
 * @param type Vertex type
 * @param label Vertex label
 * @param data Associated data (copied)
 * @param data_size Size of data
 * @param confidence Initial confidence
 * @return Vertex ID or UINT32_MAX on failure
 */
NIMCP_API uint32_t nimcp_hypergraph_add_vertex_with_data(
    nimcp_hypergraph_t* hg,
    hypervertex_type_t type,
    const char* label,
    const void* data,
    uint32_t data_size,
    float confidence);

/**
 * @brief Remove a vertex from the hypergraph
 *
 * Also removes all incident edges.
 *
 * @param hg The hypergraph
 * @param vertex_id Vertex to remove
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_remove_vertex(
    nimcp_hypergraph_t* hg,
    uint32_t vertex_id);

/**
 * @brief Get vertex by ID
 *
 * @param hg The hypergraph
 * @param vertex_id Vertex ID
 * @return Pointer to vertex or NULL
 */
NIMCP_API const nimcp_hypervertex_t* nimcp_hypergraph_get_vertex(
    const nimcp_hypergraph_t* hg,
    uint32_t vertex_id);

/**
 * @brief Find vertex by label
 *
 * @param hg The hypergraph
 * @param label Label to find
 * @return Vertex ID or UINT32_MAX if not found
 */
NIMCP_API uint32_t nimcp_hypergraph_find_vertex(
    const nimcp_hypergraph_t* hg,
    const char* label);

/**
 * @brief Update vertex confidence
 *
 * @param hg The hypergraph
 * @param vertex_id Vertex ID
 * @param confidence New confidence
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_update_vertex_confidence(
    nimcp_hypergraph_t* hg,
    uint32_t vertex_id,
    float confidence);

/* ============================================================================
 * Edge Operations
 * ============================================================================ */

/**
 * @brief Add a hyperedge connecting multiple vertices
 *
 * @param hg The hypergraph
 * @param type Edge type
 * @param vertices Array of vertex IDs
 * @param count Number of vertices
 * @param weight Ternary weight
 * @param label Edge label
 * @return Edge ID or UINT32_MAX on failure
 */
NIMCP_API uint32_t nimcp_hypergraph_add_edge(
    nimcp_hypergraph_t* hg,
    hyperedge_type_t type,
    const uint32_t* vertices,
    uint32_t count,
    trit_t weight,
    const char* label);

/**
 * @brief Add edge with full configuration
 *
 * @param hg The hypergraph
 * @param type Edge type
 * @param vertices Array of vertex IDs
 * @param count Number of vertices
 * @param weight Ternary weight
 * @param confidence Edge confidence
 * @param is_directed Whether directed
 * @param label Edge label
 * @return Edge ID or UINT32_MAX on failure
 */
NIMCP_API uint32_t nimcp_hypergraph_add_edge_full(
    nimcp_hypergraph_t* hg,
    hyperedge_type_t type,
    const uint32_t* vertices,
    uint32_t count,
    trit_t weight,
    float confidence,
    bool is_directed,
    const char* label);

/**
 * @brief Remove a hyperedge
 *
 * @param hg The hypergraph
 * @param edge_id Edge to remove
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_remove_edge(
    nimcp_hypergraph_t* hg,
    uint32_t edge_id);

/**
 * @brief Get edge by ID
 *
 * @param hg The hypergraph
 * @param edge_id Edge ID
 * @return Pointer to edge or NULL
 */
NIMCP_API const nimcp_hyperedge_t* nimcp_hypergraph_get_edge(
    const nimcp_hypergraph_t* hg,
    uint32_t edge_id);

/**
 * @brief Get all edges incident to a vertex
 *
 * @param hg The hypergraph
 * @param vertex_id Vertex ID
 * @param edges Output array of edge IDs
 * @param max_edges Maximum edges to return
 * @return Number of edges found
 */
NIMCP_API uint32_t nimcp_hypergraph_get_incident_edges(
    const nimcp_hypergraph_t* hg,
    uint32_t vertex_id,
    uint32_t* edges,
    uint32_t max_edges);

/**
 * @brief Get edges of a specific type
 *
 * @param hg The hypergraph
 * @param type Edge type to find
 * @param edges Output array of edge IDs
 * @param max_edges Maximum edges to return
 * @return Number of edges found
 */
NIMCP_API uint32_t nimcp_hypergraph_get_edges_by_type(
    const nimcp_hypergraph_t* hg,
    hyperedge_type_t type,
    uint32_t* edges,
    uint32_t max_edges);

/**
 * @brief Update edge weight
 *
 * @param hg The hypergraph
 * @param edge_id Edge ID
 * @param weight New weight
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_update_edge_weight(
    nimcp_hypergraph_t* hg,
    uint32_t edge_id,
    trit_t weight);

/* ============================================================================
 * Edge Contraction and Transformation
 * ============================================================================ */

/**
 * @brief Contract a hyperedge, merging its vertices
 *
 * All vertices in the edge are merged into a single vertex.
 *
 * @param hg The hypergraph
 * @param edge_id Edge to contract
 * @return ID of merged vertex, or UINT32_MAX on failure
 */
NIMCP_API uint32_t nimcp_hypergraph_contract_edge(
    nimcp_hypergraph_t* hg,
    uint32_t edge_id);

/**
 * @brief Add vertex to existing edge
 *
 * @param hg The hypergraph
 * @param edge_id Edge ID
 * @param vertex_id Vertex to add
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_extend_edge(
    nimcp_hypergraph_t* hg,
    uint32_t edge_id,
    uint32_t vertex_id);

/**
 * @brief Remove vertex from edge (without deleting vertex)
 *
 * @param hg The hypergraph
 * @param edge_id Edge ID
 * @param vertex_id Vertex to remove from edge
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_shrink_edge(
    nimcp_hypergraph_t* hg,
    uint32_t edge_id,
    uint32_t vertex_id);

/* ============================================================================
 * Dual Hypergraph
 * ============================================================================ */

/**
 * @brief Compute the dual hypergraph
 *
 * In the dual, vertices become edges and edges become vertices.
 * Two dual vertices are connected iff corresponding original edges
 * share a vertex.
 *
 * @param hg The hypergraph
 * @return Dual hypergraph or NULL on failure
 */
NIMCP_API nimcp_hypergraph_dual_t* nimcp_hypergraph_compute_dual(
    const nimcp_hypergraph_t* hg);

/**
 * @brief Destroy dual hypergraph
 *
 * @param dual Dual to destroy
 */
NIMCP_API void nimcp_hypergraph_dual_destroy(nimcp_hypergraph_dual_t* dual);

/* ============================================================================
 * Transversal Computation
 * ============================================================================ */

/**
 * @brief Compute a minimal transversal
 *
 * A transversal is a set of vertices that intersects every edge.
 * Useful for constraint satisfaction and covering problems.
 *
 * @param hg The hypergraph
 * @param vertices Output array of vertex IDs
 * @param max_vertices Maximum vertices to return
 * @return Size of transversal
 */
NIMCP_API uint32_t nimcp_hypergraph_transversal(
    const nimcp_hypergraph_t* hg,
    uint32_t* vertices,
    uint32_t max_vertices);

/**
 * @brief Compute all minimal transversals
 *
 * @param hg The hypergraph
 * @param transversals Output array of arrays
 * @param sizes Output array of transversal sizes
 * @param max_transversals Maximum transversals to return
 * @return Number of transversals found
 */
NIMCP_API uint32_t nimcp_hypergraph_all_transversals(
    const nimcp_hypergraph_t* hg,
    uint32_t** transversals,
    uint32_t* sizes,
    uint32_t max_transversals);

/* ============================================================================
 * Query Operations
 * ============================================================================ */

/**
 * @brief Find edges containing all specified vertices
 *
 * @param hg The hypergraph
 * @param vertices Vertices that must be in edge
 * @param vertex_count Number of vertices
 * @param edges Output array of edge IDs
 * @param max_edges Maximum edges to return
 * @return Number of edges found
 */
NIMCP_API uint32_t nimcp_hypergraph_find_edges_containing(
    const nimcp_hypergraph_t* hg,
    const uint32_t* vertices,
    uint32_t vertex_count,
    uint32_t* edges,
    uint32_t max_edges);

/**
 * @brief Get neighbors of a vertex
 *
 * Returns all vertices that share at least one edge.
 *
 * @param hg The hypergraph
 * @param vertex_id Central vertex
 * @param neighbors Output array of vertex IDs
 * @param max_neighbors Maximum neighbors to return
 * @return Number of neighbors found
 */
NIMCP_API uint32_t nimcp_hypergraph_get_neighbors(
    const nimcp_hypergraph_t* hg,
    uint32_t vertex_id,
    uint32_t* neighbors,
    uint32_t max_neighbors);

/**
 * @brief Check if vertices are connected
 *
 * Returns true if there exists an edge containing both vertices.
 *
 * @param hg The hypergraph
 * @param vertex_a First vertex
 * @param vertex_b Second vertex
 * @return true if connected
 */
NIMCP_API bool nimcp_hypergraph_are_connected(
    const nimcp_hypergraph_t* hg,
    uint32_t vertex_a,
    uint32_t vertex_b);

/**
 * @brief Execute a pattern query
 *
 * Finds subgraphs matching the specified pattern.
 *
 * @param hg The hypergraph
 * @param pattern Pattern hypergraph to match
 * @param result Query results
 * @param max_matches Maximum matches to find
 * @return Number of matches found
 */
NIMCP_API uint32_t nimcp_hypergraph_pattern_query(
    const nimcp_hypergraph_t* hg,
    const nimcp_hypergraph_t* pattern,
    hypergraph_query_result_t* result,
    uint32_t max_matches);

/* ============================================================================
 * Conversion Functions
 * ============================================================================ */

/**
 * @brief Create hypergraph from knowledge base
 *
 * @param logic Knowledge base / logic system
 * @return Hypergraph representation or NULL on failure
 */
NIMCP_API nimcp_hypergraph_t* nimcp_hypergraph_from_knowledge_base(
    const void* logic);

/**
 * @brief Create hypergraph from ternary graph
 *
 * Converts binary edges to 2-vertex hyperedges.
 *
 * @param ternary Ternary graph
 * @return Hypergraph or NULL on failure
 */
NIMCP_API nimcp_hypergraph_t* nimcp_hypergraph_from_ternary(
    const NimcpTernaryGraph* ternary);

/**
 * @brief Convert to ternary graph (2-section)
 *
 * Creates a binary graph where vertices are connected if
 * they share a hyperedge.
 *
 * @param hg The hypergraph
 * @return Ternary graph or NULL on failure
 */
NIMCP_API NimcpTernaryGraph* nimcp_hypergraph_to_ternary(
    const nimcp_hypergraph_t* hg);

/**
 * @brief Export to adjacency-like tensor representation
 *
 * @param hg The hypergraph
 * @param tensor Output tensor (caller allocates)
 * @param tensor_size Size of tensor buffer
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_to_tensor(
    const nimcp_hypergraph_t* hg,
    float* tensor,
    uint32_t tensor_size);

/* ============================================================================
 * Connected Components
 * ============================================================================ */

/**
 * @brief Find connected components
 *
 * @param hg The hypergraph
 * @param vertex_components Output array mapping vertex to component
 * @param num_components Output number of components
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_connected_components(
    const nimcp_hypergraph_t* hg,
    uint32_t* vertex_components,
    uint32_t* num_components);

/**
 * @brief Check if hypergraph is connected
 *
 * @param hg The hypergraph
 * @return true if connected
 */
NIMCP_API bool nimcp_hypergraph_is_connected(const nimcp_hypergraph_t* hg);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Register with bio-async router
 *
 * Registers this module with the global bio-async router if available.
 *
 * @param hg The hypergraph
 * @return NIMCP_SUCCESS on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_register_bio_async(
    nimcp_hypergraph_t* hg);

/**
 * @brief Unregister from bio-async router
 *
 * @param hg The hypergraph
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_unregister_bio_async(
    nimcp_hypergraph_t* hg);

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

/**
 * @brief Get hypergraph statistics
 *
 * @param hg The hypergraph
 * @param stats Output statistics
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_get_stats(
    const nimcp_hypergraph_t* hg,
    hypergraph_stats_t* stats);

/**
 * @brief Get vertex count
 *
 * @param hg The hypergraph
 * @return Number of vertices
 */
NIMCP_API uint32_t nimcp_hypergraph_vertex_count(const nimcp_hypergraph_t* hg);

/**
 * @brief Get edge count
 *
 * @param hg The hypergraph
 * @return Number of edges
 */
NIMCP_API uint32_t nimcp_hypergraph_edge_count(const nimcp_hypergraph_t* hg);

/**
 * @brief Print diagnostic information
 *
 * @param hg The hypergraph
 */
NIMCP_API void nimcp_hypergraph_print_diagnostics(const nimcp_hypergraph_t* hg);

/* ============================================================================
 * Query Result Management
 * ============================================================================ */

/**
 * @brief Initialize query result
 *
 * @param result Result to initialize
 * @param max_vertices Maximum vertices
 * @param max_edges Maximum edges
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t nimcp_hypergraph_query_result_init(
    hypergraph_query_result_t* result,
    uint32_t max_vertices,
    uint32_t max_edges);

/**
 * @brief Clean up query result
 *
 * @param result Result to clean up
 */
NIMCP_API void nimcp_hypergraph_query_result_cleanup(
    hypergraph_query_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPERGRAPH_H */
