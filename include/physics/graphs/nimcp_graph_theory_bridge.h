//=============================================================================
// nimcp_graph_theory_bridge.h - Graph Theory NIMCP Bridge
//=============================================================================
/**
 * @file nimcp_graph_theory_bridge.h
 * @brief Graph-theoretic analysis bridge with full NIMCP integration
 *
 * WHAT: Bridge connecting graph algorithms to all NIMCP subsystems:
 *       - KG module wiring for graph concept registration
 *       - Exception handling with immune presentation
 *       - Bio-async messaging for asynchronous computations
 *       - Logging integration for analysis tracing
 *
 * WHY:  Provides unified access to graph analysis with:
 *       - Centrality metrics (degree, betweenness, PageRank)
 *       - Community detection (Louvain, spectral)
 *       - Small-world and scale-free properties
 *       - Network topology validation
 *       - Quantum walk algorithms
 *
 * HOW:  Wraps nimcp_kg_algorithms and nimcp_graph_metrics with
 *       NIMCP bridge pattern for cross-module integration.
 *
 * BIOLOGY:
 *   - Brain networks exhibit small-world properties (high clustering, short paths)
 *   - Modularity Q ~ 0.3-0.5 indicates hierarchical organization
 *   - Hub nodes correspond to integrative brain regions
 *   - Community structure reflects functional segregation
 *
 * REFERENCES:
 *   - Bullmore & Sporns (2009): Complex brain networks
 *   - Newman (2006): Modularity and community structure
 *   - Watts & Strogatz (1998): Small-world networks
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 * @version 1.0.0
 */

#ifndef NIMCP_GRAPH_THEORY_BRIDGE_H
#define NIMCP_GRAPH_THEORY_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "utils/containers/nimcp_graph.h"
#include "utils/algorithms/nimcp_graph_metrics.h"
#include "core/brain/nimcp_kg_algorithms.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "core/brain/nimcp_brain_kg.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Module Constants
//=============================================================================

/** Module name for logging and identification */
#define GRAPH_THEORY_BRIDGE_MODULE_NAME    "graph_theory_bridge"

/** KG module ID for graph theory (0x0470 range) */
#define GRAPH_THEORY_KG_MODULE_ID          0x0470

/** Exception category for graph theory errors */
#define GRAPH_THEORY_EXCEPTION_CATEGORY    0x0047

/** Maximum dimension for spectral analysis */
#define GRAPH_THEORY_MAX_SPECTRAL_DIM      1024

/** Maximum nodes for full analysis */
#define GRAPH_THEORY_MAX_NODES             100000

/** Default convergence tolerance */
#define GRAPH_THEORY_DEFAULT_TOLERANCE     1e-6f

//=============================================================================
// Message Types (0x4700 - 0x476F range)
//=============================================================================

/**
 * @brief Bio-async message types for graph theory operations
 */
typedef enum {
    /* Centrality Analysis (0x4700-0x470F) */
    GRAPH_MSG_CENTRALITY_REQUEST        = 0x4700,  /**< Request centrality computation */
    GRAPH_MSG_CENTRALITY_RESULT         = 0x4701,  /**< Centrality computation result */
    GRAPH_MSG_DEGREE_DIST_REQUEST       = 0x4702,  /**< Request degree distribution */
    GRAPH_MSG_DEGREE_DIST_RESULT        = 0x4703,  /**< Degree distribution result */
    GRAPH_MSG_PAGERANK_REQUEST          = 0x4704,  /**< Request PageRank computation */
    GRAPH_MSG_PAGERANK_RESULT           = 0x4705,  /**< PageRank result */
    GRAPH_MSG_BETWEENNESS_REQUEST       = 0x4706,  /**< Request betweenness centrality */
    GRAPH_MSG_BETWEENNESS_RESULT        = 0x4707,  /**< Betweenness centrality result */

    /* Community Detection (0x4710-0x471F) */
    GRAPH_MSG_COMMUNITY_REQUEST         = 0x4710,  /**< Request community detection */
    GRAPH_MSG_COMMUNITY_RESULT          = 0x4711,  /**< Community detection result */
    GRAPH_MSG_LOUVAIN_REQUEST           = 0x4712,  /**< Request Louvain algorithm */
    GRAPH_MSG_LOUVAIN_RESULT            = 0x4713,  /**< Louvain result */
    GRAPH_MSG_SPECTRAL_CLUSTER_REQUEST  = 0x4714,  /**< Request spectral clustering */
    GRAPH_MSG_SPECTRAL_CLUSTER_RESULT   = 0x4715,  /**< Spectral clustering result */
    GRAPH_MSG_MODULARITY_REQUEST        = 0x4716,  /**< Request modularity Q */
    GRAPH_MSG_MODULARITY_RESULT         = 0x4717,  /**< Modularity Q result */

    /* Topology Metrics (0x4720-0x472F) */
    GRAPH_MSG_METRICS_REQUEST           = 0x4720,  /**< Request full metrics */
    GRAPH_MSG_METRICS_RESULT            = 0x4721,  /**< Full metrics result */
    GRAPH_MSG_CLUSTERING_REQUEST        = 0x4722,  /**< Request clustering coeff */
    GRAPH_MSG_CLUSTERING_RESULT         = 0x4723,  /**< Clustering coeff result */
    GRAPH_MSG_PATH_LENGTH_REQUEST       = 0x4724,  /**< Request path length */
    GRAPH_MSG_PATH_LENGTH_RESULT        = 0x4725,  /**< Path length result */
    GRAPH_MSG_SMALL_WORLD_REQUEST       = 0x4726,  /**< Request small-world coeff */
    GRAPH_MSG_SMALL_WORLD_RESULT        = 0x4727,  /**< Small-world coeff result */
    GRAPH_MSG_ASSORTATIVITY_REQUEST     = 0x4728,  /**< Request assortativity */
    GRAPH_MSG_ASSORTATIVITY_RESULT      = 0x4729,  /**< Assortativity result */

    /* Spectral Analysis (0x4730-0x473F) */
    GRAPH_MSG_SPECTRAL_REQUEST          = 0x4730,  /**< Request spectral analysis */
    GRAPH_MSG_SPECTRAL_RESULT           = 0x4731,  /**< Spectral analysis result */
    GRAPH_MSG_LAPLACIAN_REQUEST         = 0x4732,  /**< Request Laplacian spectrum */
    GRAPH_MSG_LAPLACIAN_RESULT          = 0x4733,  /**< Laplacian spectrum result */
    GRAPH_MSG_FIEDLER_REQUEST           = 0x4734,  /**< Request Fiedler vector */
    GRAPH_MSG_FIEDLER_RESULT            = 0x4735,  /**< Fiedler vector result */

    /* Quantum Walks (0x4740-0x474F) */
    GRAPH_MSG_QWALK_REQUEST             = 0x4740,  /**< Request quantum walk */
    GRAPH_MSG_QWALK_RESULT              = 0x4741,  /**< Quantum walk result */
    GRAPH_MSG_QWALK_SEARCH_REQUEST      = 0x4742,  /**< Request quantum walk search */
    GRAPH_MSG_QWALK_SEARCH_RESULT       = 0x4743,  /**< Quantum walk search result */

    /* Hyperbolic Embeddings (0x4750-0x475F) */
    GRAPH_MSG_HYPERBOLIC_REQUEST        = 0x4750,  /**< Request hyperbolic embedding */
    GRAPH_MSG_HYPERBOLIC_RESULT         = 0x4751,  /**< Hyperbolic embedding result */
    GRAPH_MSG_CURVATURE_REQUEST         = 0x4752,  /**< Request graph curvature */
    GRAPH_MSG_CURVATURE_RESULT          = 0x4753,  /**< Graph curvature result */

    /* Phase Coherence (0x4760-0x476F) */
    GRAPH_MSG_PHASE_REQUEST             = 0x4760,  /**< Request phase coherence */
    GRAPH_MSG_PHASE_RESULT              = 0x4761,  /**< Phase coherence result */
    GRAPH_MSG_SYNC_REQUEST              = 0x4762,  /**< Request synchronization */
    GRAPH_MSG_SYNC_RESULT               = 0x4763   /**< Synchronization result */
} graph_theory_msg_type_t;

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Graph theory bridge error codes
 */
typedef enum {
    GRAPH_THEORY_OK                     = 0,       /**< Success */
    GRAPH_THEORY_ERROR_INVALID_PARAM    = -1,      /**< Invalid parameter */
    GRAPH_THEORY_ERROR_ALLOC            = -2,      /**< Memory allocation failed */
    GRAPH_THEORY_ERROR_NOT_INIT         = -3,      /**< Bridge not initialized */
    GRAPH_THEORY_ERROR_ALREADY_INIT     = -4,      /**< Bridge already initialized */
    GRAPH_THEORY_ERROR_KG_WIRING        = -5,      /**< KG wiring error */
    GRAPH_THEORY_ERROR_EXCEPTION        = -6,      /**< Exception handler error */
    GRAPH_THEORY_ERROR_BIO_ASYNC        = -7,      /**< Bio-async error */
    GRAPH_THEORY_ERROR_GRAPH_INVALID    = -8,      /**< Invalid graph structure */
    GRAPH_THEORY_ERROR_COMPUTATION      = -9,      /**< Computation error */
    GRAPH_THEORY_ERROR_CONVERGENCE      = -10,     /**< Algorithm did not converge */
    GRAPH_THEORY_ERROR_DIMENSION        = -11,     /**< Dimension mismatch or exceeded */
    GRAPH_THEORY_ERROR_DISCONNECTED     = -12,     /**< Graph is disconnected */
    GRAPH_THEORY_ERROR_TIMEOUT          = -13      /**< Operation timed out */
} graph_theory_error_t;

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief Centrality algorithm selection
 */
typedef enum {
    CENTRALITY_DEGREE                   = 0,       /**< Degree centrality */
    CENTRALITY_BETWEENNESS              = 1,       /**< Betweenness centrality */
    CENTRALITY_CLOSENESS                = 2,       /**< Closeness centrality */
    CENTRALITY_EIGENVECTOR              = 3,       /**< Eigenvector centrality */
    CENTRALITY_PAGERANK                 = 4,       /**< PageRank */
    CENTRALITY_KATZ                     = 5        /**< Katz centrality */
} centrality_type_t;

/**
 * @brief Community detection algorithm selection
 */
typedef enum {
    COMMUNITY_LOUVAIN                   = 0,       /**< Louvain algorithm */
    COMMUNITY_SPECTRAL                  = 1,       /**< Spectral clustering */
    COMMUNITY_LABEL_PROP                = 2,       /**< Label propagation */
    COMMUNITY_GIRVAN_NEWMAN             = 3        /**< Girvan-Newman */
} community_algo_t;

/**
 * @brief Quantum walk type
 */
typedef enum {
    QWALK_CONTINUOUS                    = 0,       /**< Continuous-time quantum walk */
    QWALK_DISCRETE                      = 1,       /**< Discrete-time quantum walk */
    QWALK_GROVER                        = 2        /**< Grover walk */
} qwalk_type_t;

//=============================================================================
// Bridge Configuration
//=============================================================================

/**
 * @brief Graph theory bridge configuration
 */
typedef struct {
    /** Enable KG module wiring */
    bool enable_kg_wiring;

    /** Enable exception handling */
    bool enable_exception_handling;

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** Enable immune system presentation for exceptions */
    bool enable_immune_presentation;

    /** Enable logging */
    bool enable_logging;

    /** Default centrality algorithm */
    centrality_type_t default_centrality;

    /** Default community detection algorithm */
    community_algo_t default_community_algo;

    /** Maximum iterations for iterative algorithms */
    uint32_t max_iterations;

    /** Convergence tolerance */
    float tolerance;

    /** PageRank damping factor (0.85 typical) */
    float pagerank_damping;

    /** Number of spectral dimensions for embedding */
    uint32_t spectral_dim;

    /** Enable parallel computation where available */
    bool enable_parallel;

    /** Worker thread count (0 = auto) */
    uint32_t num_workers;
} graph_theory_bridge_config_t;

//=============================================================================
// Analysis Result Types
//=============================================================================

/**
 * @brief Centrality analysis result
 */
typedef struct {
    /** Node indices */
    uint32_t* node_ids;

    /** Centrality values */
    float* values;

    /** Number of nodes */
    uint32_t num_nodes;

    /** Centrality type computed */
    centrality_type_t type;

    /** Maximum centrality value */
    float max_value;

    /** Node with maximum centrality */
    uint32_t max_node;

    /** Mean centrality */
    float mean_value;

    /** Centrality variance */
    float variance;

    /** Power-law exponent (if applicable) */
    float power_law_exponent;

    /** Is scale-free based on distribution */
    bool is_scale_free;
} graph_centrality_result_t;

/**
 * @brief Community detection result
 */
typedef struct {
    /** Community assignment per node */
    uint32_t* assignments;

    /** Number of nodes */
    uint32_t num_nodes;

    /** Number of communities found */
    uint32_t num_communities;

    /** Modularity Q value */
    float modularity;

    /** Community sizes */
    uint32_t* community_sizes;

    /** Intra-community edges fraction */
    float intra_edge_fraction;

    /** Algorithm used */
    community_algo_t algorithm;

    /** Hierarchy levels (for hierarchical methods) */
    uint32_t hierarchy_levels;
} graph_community_result_t;

/**
 * @brief Network topology metrics
 */
typedef struct {
    /** Newman's modularity Q */
    float modularity;

    /** Average clustering coefficient */
    float clustering_coefficient;

    /** Characteristic path length */
    float characteristic_path_length;

    /** Small-world coefficient sigma */
    float small_world_sigma;

    /** Graph diameter */
    uint32_t diameter;

    /** Degree assortativity */
    float assortativity;

    /** Network density */
    float density;

    /** Global efficiency */
    float global_efficiency;

    /** Local efficiency */
    float local_efficiency;

    /** Rich-club coefficient at max degree */
    float rich_club;

    /** Is small-world (sigma > 1) */
    bool is_small_world;

    /** Is scale-free (power law degree) */
    bool is_scale_free;
} graph_topology_metrics_t;

/**
 * @brief Spectral analysis result
 */
typedef struct {
    /** Eigenvalues */
    float* eigenvalues;

    /** Number of eigenvalues */
    uint32_t num_eigenvalues;

    /** Spectral gap (lambda_2 - lambda_1) */
    float spectral_gap;

    /** Algebraic connectivity (Fiedler value) */
    float algebraic_connectivity;

    /** Fiedler vector */
    float* fiedler_vector;

    /** Number of connected components */
    uint32_t num_components;

    /** Spectral radius */
    float spectral_radius;
} graph_spectral_result_t;

/**
 * @brief Quantum walk result
 */
typedef struct {
    /** Final probability distribution */
    float* probabilities;

    /** Number of nodes */
    uint32_t num_nodes;

    /** Evolution time */
    float evolution_time;

    /** Walk type */
    qwalk_type_t type;

    /** Hitting time (for search) */
    float hitting_time;

    /** Target node found (for search) */
    bool target_found;

    /** Found node index */
    uint32_t found_node;

    /** Quantum speedup factor */
    float speedup_factor;
} graph_qwalk_result_t;

/**
 * @brief Hyperbolic embedding result
 */
typedef struct {
    /** Poincare disk coordinates (2D per node) */
    float* coordinates;

    /** Number of nodes */
    uint32_t num_nodes;

    /** Embedding dimension */
    uint32_t dimension;

    /** Curvature parameter K */
    float curvature;

    /** Mean distortion */
    float mean_distortion;

    /** Max distortion */
    float max_distortion;

    /** Hierarchy detected */
    bool is_hierarchical;
} graph_hyperbolic_result_t;

//=============================================================================
// KG Node Types for Graph Theory
//=============================================================================

/**
 * @brief Graph theory specific KG node types
 */
typedef enum {
    /** Graph analysis root */
    GRAPH_KG_NODE_ROOT                  = 0x1700,
    /** Centrality metric */
    GRAPH_KG_NODE_CENTRALITY,
    /** Community */
    GRAPH_KG_NODE_COMMUNITY,
    /** Topology metric */
    GRAPH_KG_NODE_TOPOLOGY,
    /** Spectral property */
    GRAPH_KG_NODE_SPECTRAL,
    /** Quantum walk process */
    GRAPH_KG_NODE_QUANTUM_WALK,
    /** Embedding */
    GRAPH_KG_NODE_EMBEDDING,
    /** Hub node */
    GRAPH_KG_NODE_HUB
} graph_kg_node_type_t;

/**
 * @brief Graph theory specific KG edge types
 */
typedef enum {
    /** Member of community */
    GRAPH_KG_EDGE_MEMBER_OF             = 0x1700,
    /** Connects to hub */
    GRAPH_KG_EDGE_CONNECTS_HUB,
    /** Has centrality */
    GRAPH_KG_EDGE_HAS_CENTRALITY,
    /** Has metric */
    GRAPH_KG_EDGE_HAS_METRIC,
    /** Spectral property of */
    GRAPH_KG_EDGE_SPECTRAL_OF
} graph_kg_edge_type_t;

//=============================================================================
// Bridge State
//=============================================================================

/**
 * @brief KG registration state for graph theory
 */
typedef struct {
    /** Root node ID */
    brain_kg_node_id_t root_id;

    /** Centrality subsystem node */
    brain_kg_node_id_t centrality_id;

    /** Community subsystem node */
    brain_kg_node_id_t community_id;

    /** Topology subsystem node */
    brain_kg_node_id_t topology_id;

    /** Spectral subsystem node */
    brain_kg_node_id_t spectral_id;

    /** Quantum walk subsystem node */
    brain_kg_node_id_t qwalk_id;

    /** Embedding subsystem node */
    brain_kg_node_id_t embedding_id;

    /** Total nodes registered */
    uint32_t node_count;

    /** Total edges registered */
    uint32_t edge_count;

    /** Registration complete */
    bool registered;
} graph_theory_kg_state_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct graph_theory_bridge* graph_theory_bridge_t;

//=============================================================================
// Bridge Lifecycle API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_bridge_default_config(
    graph_theory_bridge_config_t* config
);

/**
 * @brief Create graph theory bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT graph_theory_bridge_t graph_theory_bridge_create(
    const graph_theory_bridge_config_t* config
);

/**
 * @brief Destroy graph theory bridge
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void graph_theory_bridge_destroy(graph_theory_bridge_t bridge);

/**
 * @brief Register bridge with knowledge graph
 *
 * @param bridge Bridge handle
 * @param kg Knowledge graph
 * @param admin_token Admin token for KG access
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_bridge_register_kg(
    graph_theory_bridge_t bridge,
    brain_kg_t* kg,
    uint64_t admin_token
);

/**
 * @brief Get KG registration state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_bridge_get_kg_state(
    graph_theory_bridge_t bridge,
    graph_theory_kg_state_t* state
);

/**
 * @brief Register exception handlers
 *
 * @param bridge Bridge handle
 * @param handler Exception handler
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_bridge_register_exception(
    graph_theory_bridge_t bridge,
    void* handler
);

/**
 * @brief Register bio-async channel
 *
 * @param bridge Bridge handle
 * @param channel Bio-async channel
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_bridge_register_bio_async(
    graph_theory_bridge_t bridge,
    void* channel
);

//=============================================================================
// Wave W15: Runtime event emission + read-path
//=============================================================================

/* Forward-declared brain handle for admin-token self-elevation. */
#ifndef NIMCP_BRAIN_T_DEFINED
#define NIMCP_BRAIN_T_DEFINED
typedef struct brain_struct* brain_t;
#endif

/**
 * @brief Register brain handle with the graph-theory bridge for runtime emit.
 *
 * Once registered, `graph_theory_kg_trigger_metric_event` can self-elevate
 * admin. Pass NULL to clear.
 */
NIMCP_EXPORT void graph_theory_kg_register_brain(
    graph_theory_bridge_t bridge,
    brain_t brain
);

/**
 * @brief Emit an aggregated graph-metric event (modularity / clustering /
 *        small-world transition / hub change).
 *
 * WHAT:  Creates event node `graph_theory_event_<kind>_<ts_us>` with
 *        metric value + description, and edge back to the `graph_theory`
 *        root node.
 * WHY:   Lets downstream reasoners query for recent topology shifts.
 * WHEN:  Call from analysis-complete callbacks or when a metric crosses
 *        a configured threshold — never in an inner graph-walk loop.
 *
 * @param bridge   Graph theory bridge (must have been registered to KG)
 * @param kind     Event kind ("modularity_shift", "clustering_drop", ...)
 * @param metric   Metric value (e.g. the new modularity)
 * @param ts_us    Timestamp in microseconds
 */
NIMCP_EXPORT void graph_theory_kg_trigger_metric_event(
    graph_theory_bridge_t bridge,
    const char* kind,
    float metric,
    uint64_t ts_us
);

/**
 * @brief Read-path: count graph-theory event nodes matching a kind substring.
 */
NIMCP_EXPORT uint32_t graph_theory_kg_count_metric_events(
    graph_theory_bridge_t bridge,
    const char* kind_substr
);

//=============================================================================
// Centrality Analysis API
//=============================================================================

/**
 * @brief Compute centrality for all nodes
 *
 * @param bridge Bridge handle
 * @param graph Graph to analyze
 * @param type Centrality type
 * @param result Output result (caller must free with graph_centrality_result_destroy)
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_compute_centrality(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    centrality_type_t type,
    graph_centrality_result_t** result
);

/**
 * @brief Find hub nodes (high centrality)
 *
 * @param bridge Bridge handle
 * @param graph Graph to analyze
 * @param type Centrality type for hub detection
 * @param top_k Number of top hubs to return
 * @param hub_ids Output array of hub node IDs
 * @param hub_scores Output array of hub scores
 * @return Number of hubs found
 */
NIMCP_EXPORT int32_t graph_theory_find_hubs(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    centrality_type_t type,
    uint32_t top_k,
    uint32_t* hub_ids,
    float* hub_scores
);

/**
 * @brief Free centrality result
 *
 * @param result Result to free
 */
NIMCP_EXPORT void graph_centrality_result_destroy(graph_centrality_result_t* result);

//=============================================================================
// Community Detection API
//=============================================================================

/**
 * @brief Detect communities in graph
 *
 * @param bridge Bridge handle
 * @param graph Graph to analyze
 * @param algo Algorithm to use
 * @param num_communities Hint for number of communities (0 = auto)
 * @param result Output result (caller must free)
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_detect_communities(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    community_algo_t algo,
    uint32_t num_communities,
    graph_community_result_t** result
);

/**
 * @brief Compute modularity for given partition
 *
 * @param bridge Bridge handle
 * @param graph Graph to analyze
 * @param assignments Community assignment per node
 * @param num_nodes Number of nodes
 * @return Modularity Q value, or -2.0 on error
 */
NIMCP_EXPORT float graph_theory_compute_modularity(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    const uint32_t* assignments,
    uint32_t num_nodes
);

/**
 * @brief Free community result
 *
 * @param result Result to free
 */
NIMCP_EXPORT void graph_community_result_destroy(graph_community_result_t* result);

//=============================================================================
// Topology Metrics API
//=============================================================================

/**
 * @brief Compute comprehensive topology metrics
 *
 * @param bridge Bridge handle
 * @param graph Graph to analyze
 * @param communities Optional community assignments for modularity
 * @param metrics Output metrics
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_compute_metrics(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    const uint32_t* communities,
    graph_topology_metrics_t* metrics
);

/**
 * @brief Validate graph has brain-like topology
 *
 * @param bridge Bridge handle
 * @param graph Graph to validate
 * @param tolerance Tolerance for biological thresholds
 * @param is_valid Output: true if brain-like
 * @param report Output: validation report (optional, caller must free)
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_validate_brain_topology(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    float tolerance,
    bool* is_valid,
    char** report
);

//=============================================================================
// Spectral Analysis API
//=============================================================================

/**
 * @brief Compute spectral properties
 *
 * @param bridge Bridge handle
 * @param graph Graph to analyze
 * @param num_eigenvalues Number of eigenvalues to compute
 * @param result Output result (caller must free)
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_spectral_analysis(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    uint32_t num_eigenvalues,
    graph_spectral_result_t** result
);

/**
 * @brief Compute Fiedler vector for graph bipartitioning
 *
 * @param bridge Bridge handle
 * @param graph Graph to analyze
 * @param fiedler_vector Output vector (caller must allocate num_nodes floats)
 * @param algebraic_connectivity Output algebraic connectivity
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_compute_fiedler(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    float* fiedler_vector,
    float* algebraic_connectivity
);

/**
 * @brief Free spectral result
 *
 * @param result Result to free
 */
NIMCP_EXPORT void graph_spectral_result_destroy(graph_spectral_result_t* result);

//=============================================================================
// Quantum Walk API
//=============================================================================

/**
 * @brief Perform quantum walk on graph
 *
 * @param bridge Bridge handle
 * @param graph Graph for walk
 * @param type Quantum walk type
 * @param start_node Starting node
 * @param evolution_time Evolution time (for continuous) or steps (for discrete)
 * @param result Output result (caller must free)
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_quantum_walk(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    qwalk_type_t type,
    uint32_t start_node,
    float evolution_time,
    graph_qwalk_result_t** result
);

/**
 * @brief Quantum walk search for marked vertex
 *
 * @param bridge Bridge handle
 * @param graph Graph to search
 * @param marked_nodes Array of marked node indices
 * @param num_marked Number of marked nodes
 * @param result Output result (caller must free)
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_quantum_search(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    const uint32_t* marked_nodes,
    uint32_t num_marked,
    graph_qwalk_result_t** result
);

/**
 * @brief Free quantum walk result
 *
 * @param result Result to free
 */
NIMCP_EXPORT void graph_qwalk_result_destroy(graph_qwalk_result_t* result);

//=============================================================================
// Hyperbolic Embedding API
//=============================================================================

/**
 * @brief Compute hyperbolic embedding
 *
 * @param bridge Bridge handle
 * @param graph Graph to embed
 * @param dimension Embedding dimension (typically 2)
 * @param result Output result (caller must free)
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_hyperbolic_embed(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    uint32_t dimension,
    graph_hyperbolic_result_t** result
);

/**
 * @brief Compute Ollivier-Ricci curvature
 *
 * @param bridge Bridge handle
 * @param graph Graph to analyze
 * @param edge_curvatures Output curvature per edge (caller must allocate)
 * @param mean_curvature Output mean curvature
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_compute_curvature(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    float* edge_curvatures,
    float* mean_curvature
);

/**
 * @brief Free hyperbolic result
 *
 * @param result Result to free
 */
NIMCP_EXPORT void graph_hyperbolic_result_destroy(graph_hyperbolic_result_t* result);

//=============================================================================
// Phase Coherence API
//=============================================================================

/**
 * @brief Compute phase coherence on graph
 *
 * @param bridge Bridge handle
 * @param graph Graph structure
 * @param phases Phase values per node
 * @param num_nodes Number of nodes
 * @param global_coherence Output global phase coherence
 * @param local_coherences Output local coherence per node (optional)
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_compute_phase_coherence(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    const float* phases,
    uint32_t num_nodes,
    float* global_coherence,
    float* local_coherences
);

/**
 * @brief Compute synchronization between node groups
 *
 * @param bridge Bridge handle
 * @param graph Graph structure
 * @param phases Phase values per node
 * @param communities Community assignments
 * @param num_nodes Number of nodes
 * @param num_communities Number of communities
 * @param sync_matrix Output sync matrix (num_communities x num_communities)
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_compute_sync_matrix(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    const float* phases,
    const uint32_t* communities,
    uint32_t num_nodes,
    uint32_t num_communities,
    float* sync_matrix
);

//=============================================================================
// Async Operations
//=============================================================================

/**
 * @brief Submit async centrality computation
 *
 * @param bridge Bridge handle
 * @param graph Graph to analyze
 * @param type Centrality type
 * @param callback Callback when complete
 * @param user_data User context
 * @return Request ID, or 0 on error
 */
NIMCP_EXPORT uint64_t graph_theory_async_centrality(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    centrality_type_t type,
    void (*callback)(graph_centrality_result_t* result, void* user_data),
    void* user_data
);

/**
 * @brief Submit async community detection
 *
 * @param bridge Bridge handle
 * @param graph Graph to analyze
 * @param algo Algorithm to use
 * @param callback Callback when complete
 * @param user_data User context
 * @return Request ID, or 0 on error
 */
NIMCP_EXPORT uint64_t graph_theory_async_communities(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    community_algo_t algo,
    void (*callback)(graph_community_result_t* result, void* user_data),
    void* user_data
);

/**
 * @brief Cancel async request
 *
 * @param bridge Bridge handle
 * @param request_id Request to cancel
 * @return GRAPH_THEORY_OK on success
 */
NIMCP_EXPORT graph_theory_error_t graph_theory_cancel_request(
    graph_theory_bridge_t bridge,
    uint64_t request_id
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Static error string
 */
NIMCP_EXPORT const char* graph_theory_error_string(graph_theory_error_t error);

/**
 * @brief Get centrality type name
 *
 * @param type Centrality type
 * @return Static type name
 */
NIMCP_EXPORT const char* graph_theory_centrality_name(centrality_type_t type);

/**
 * @brief Get community algorithm name
 *
 * @param algo Algorithm
 * @return Static algorithm name
 */
NIMCP_EXPORT const char* graph_theory_community_algo_name(community_algo_t algo);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRAPH_THEORY_BRIDGE_H */
