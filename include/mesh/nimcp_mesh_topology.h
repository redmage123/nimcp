/**
 * @file nimcp_mesh_topology.h
 * @brief Mesh Network Topology Management with Fractal Integration
 *
 * WHAT: Topology analysis and coordinator placement for mesh network
 * WHY:  Optimize coordinator placement using fractal network properties
 * HOW:  Integrate with fractal topology for hub identification and betweenness centrality
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                      MESH TOPOLOGY MANAGEMENT                           │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐         │
 * │  │ Hub Detection   │  │ Centrality      │  │ Coordinator     │         │
 * │  │ (Degree-based)  │  │ Analysis        │  │ Placement       │         │
 * │  └─────────────────┘  └─────────────────┘  └─────────────────┘         │
 * │                              │                       │                  │
 * │                              ▼                       ▼                  │
 * │                    ┌─────────────────────────────────────┐              │
 * │                    │       Fractal Topology Engine       │              │
 * │                    │    (Scale-free, Power-law, Hubs)    │              │
 * │                    └─────────────────────────────────────┘              │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIOLOGICAL MOTIVATION:
 * - Brain networks exhibit scale-free connectivity (Sporns et al., 2004)
 * - Hub neurons are critical for information integration
 * - Coordinator placement should match network topology
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_TOPOLOGY_H
#define NIMCP_MESH_TOPOLOGY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "mesh/nimcp_mesh_types.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Default hub detection threshold (percentile) */
#define MESH_TOPOLOGY_HUB_PERCENTILE        0.90f

/** @brief Default high centrality threshold */
#define MESH_TOPOLOGY_HIGH_CENTRALITY       0.75f

/** @brief Maximum nodes in topology analysis */
#define MESH_TOPOLOGY_MAX_NODES             1024

/** @brief Default scale-free gamma exponent */
#define MESH_TOPOLOGY_DEFAULT_GAMMA         -2.1f

/** @brief Optimal coordinator per log(participants) ratio */
#define MESH_TOPOLOGY_COORD_LOG_RATIO       2.0f

/** @brief Minimum coordinators per pool */
#define MESH_TOPOLOGY_MIN_COORDINATORS      3

/** @brief Maximum coordinators per pool */
#define MESH_TOPOLOGY_MAX_COORDINATORS      8

/* ============================================================================
 * Topology Metrics
 * ============================================================================ */

/**
 * @brief Topology metrics for a channel
 *
 * WHAT: Graph-theoretic metrics describing network topology
 * WHY:  Guide coordinator placement and load balancing
 */
typedef struct mesh_topology_metrics {
    uint32_t num_participants;          /**< Total participants */
    uint32_t num_connections;           /**< Total connections */
    float avg_degree;                   /**< Average connections per node */
    float degree_std;                   /**< Degree standard deviation */
    float clustering_coefficient;       /**< Local clustering */
    float characteristic_path_length;   /**< Average shortest path */
    float power_law_gamma;              /**< Power-law exponent (if scale-free) */
    float power_law_fit_r2;             /**< Goodness of fit */
    float small_world_sigma;            /**< Small-world coefficient */
    uint32_t num_hubs;                  /**< Identified hub nodes */
    float hub_connectivity_fraction;    /**< Fraction of edges through hubs */
} mesh_topology_metrics_t;

/**
 * @brief Node-level topology information
 *
 * WHAT: Per-participant topology metrics
 * WHY:  Identify important nodes for coordinator assignment
 */
typedef struct mesh_node_info {
    mesh_participant_id_t participant_id;   /**< Participant */
    uint32_t degree;                        /**< Number of connections */
    float betweenness_centrality;           /**< Betweenness centrality [0, 1] */
    float closeness_centrality;             /**< Closeness centrality [0, 1] */
    float eigenvector_centrality;           /**< Eigenvector centrality [0, 1] */
    bool is_hub;                            /**< Whether node is a hub */
    uint32_t cluster_id;                    /**< Cluster assignment */
    uint32_t coordinator_affinity;          /**< Preferred coordinator index */
} mesh_node_info_t;

/**
 * @brief Coordinator placement recommendation
 *
 * WHAT: Recommended coordinator configuration based on topology
 * WHY:  Optimal load balancing and fault tolerance
 */
typedef struct mesh_coord_placement {
    uint32_t recommended_pool_size;         /**< Recommended coordinators */
    mesh_participant_id_t* hub_assignments; /**< Hubs to assign to leader */
    size_t hub_count;                       /**< Number of hubs */
    float* load_distribution;               /**< Expected load per coordinator */
    size_t distribution_size;               /**< Size of load array */
} mesh_coord_placement_t;

/* ============================================================================
 * Topology Configuration
 * ============================================================================ */

/**
 * @brief Topology analysis configuration
 */
typedef struct mesh_topology_config {
    float hub_percentile;               /**< Percentile for hub detection */
    float high_centrality_threshold;    /**< Threshold for high centrality */
    bool compute_clustering;            /**< Compute clustering coefficient */
    bool compute_path_length;           /**< Compute characteristic path */
    bool compute_small_world;           /**< Compute small-world sigma */
    bool fit_power_law;                 /**< Fit power-law distribution */
    uint32_t max_path_samples;          /**< Max samples for path estimation */
} mesh_topology_config_t;

/* ============================================================================
 * Mesh Topology Context
 * ============================================================================ */

/**
 * @brief Opaque mesh topology context
 */
typedef struct mesh_topology_ctx_internal* mesh_topology_ctx_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create default topology configuration
 *
 * WHAT: Returns sensible defaults for topology analysis
 * WHY:  Easy starting point
 * HOW:  Biologically-motivated default values
 *
 * @return Default configuration
 */
mesh_topology_config_t mesh_topology_default_config(void);

/**
 * @brief Create mesh topology context
 *
 * WHAT: Initialize topology analysis context
 * WHY:  Analyze channel topology for coordinator placement
 * HOW:  Allocate structures, prepare for analysis
 *
 * @param config Configuration (NULL for defaults)
 * @return Context handle or NULL on failure
 */
mesh_topology_ctx_t mesh_topology_create(const mesh_topology_config_t* config);

/**
 * @brief Destroy mesh topology context
 *
 * WHAT: Free all resources
 * WHY:  Prevent memory leaks
 *
 * @param ctx Context to destroy
 */
void mesh_topology_destroy(mesh_topology_ctx_t ctx);

/* ============================================================================
 * Node Registration
 * ============================================================================ */

/**
 * @brief Add participant to topology
 *
 * WHAT: Register a participant for topology analysis
 * WHY:  Build graph of participants before analysis
 *
 * @param ctx Topology context
 * @param participant_id Participant to add
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_topology_add_participant(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t participant_id
);

/**
 * @brief Add connection between participants
 *
 * WHAT: Register a connection (edge) in the topology
 * WHY:  Build graph structure
 *
 * @param ctx Topology context
 * @param from Source participant
 * @param to Target participant
 * @param weight Connection weight (1.0 for unweighted)
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_topology_add_connection(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t from,
    mesh_participant_id_t to,
    float weight
);

/**
 * @brief Remove participant from topology
 *
 * WHAT: Unregister participant and its connections
 * WHY:  Handle participant departure
 *
 * @param ctx Topology context
 * @param participant_id Participant to remove
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_topology_remove_participant(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t participant_id
);

/**
 * @brief Clear all topology data
 *
 * WHAT: Remove all participants and connections
 * WHY:  Reset for new analysis
 *
 * @param ctx Topology context
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_topology_clear(mesh_topology_ctx_t ctx);

/* ============================================================================
 * Topology Analysis
 * ============================================================================ */

/**
 * @brief Compute topology metrics
 *
 * WHAT: Analyze graph and compute metrics
 * WHY:  Understand network structure for optimization
 * HOW:  Graph algorithms (BFS, clustering, centrality)
 *
 * @param ctx Topology context
 * @param metrics Output metrics
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_topology_compute_metrics(
    mesh_topology_ctx_t ctx,
    mesh_topology_metrics_t* metrics
);

/**
 * @brief Get node information for a participant
 *
 * WHAT: Retrieve computed metrics for single node
 * WHY:  Access per-node analysis results
 *
 * @param ctx Topology context
 * @param participant_id Target participant
 * @param info Output node information
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_topology_get_node_info(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t participant_id,
    mesh_node_info_t* info
);

/**
 * @brief Identify hub participants
 *
 * WHAT: Find high-degree nodes in the network
 * WHY:  Hubs are critical for information flow
 * HOW:  Rank by degree, return top percentile
 *
 * @param ctx Topology context
 * @param hub_ids Output array of hub participant IDs (caller allocates)
 * @param hub_capacity Capacity of hub_ids array
 * @param num_hubs Output: actual number of hubs found
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_topology_identify_hubs(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t* hub_ids,
    size_t hub_capacity,
    size_t* num_hubs
);

/**
 * @brief Compute betweenness centrality for all nodes
 *
 * WHAT: Calculate betweenness centrality scores
 * WHY:  Identify nodes critical for information routing
 * HOW:  Brandes' algorithm O(VE)
 *
 * @param ctx Topology context
 * @return NIMCP_OK on success (access via get_node_info)
 */
nimcp_error_t mesh_topology_compute_betweenness(mesh_topology_ctx_t ctx);

/**
 * @brief Check if network is scale-free
 *
 * WHAT: Test for power-law degree distribution
 * WHY:  Scale-free networks have different optimization strategies
 *
 * @param ctx Topology context
 * @param gamma Output: power-law exponent
 * @param r_squared Output: goodness of fit
 * @return true if scale-free (r² > 0.8), false otherwise
 */
bool mesh_topology_is_scale_free(
    mesh_topology_ctx_t ctx,
    float* gamma,
    float* r_squared
);

/**
 * @brief Check if network is small-world
 *
 * WHAT: Test for small-world properties
 * WHY:  Small-world networks balance local and global processing
 *
 * @param ctx Topology context
 * @param sigma Output: small-world coefficient
 * @return true if small-world (sigma > 1), false otherwise
 */
bool mesh_topology_is_small_world(
    mesh_topology_ctx_t ctx,
    float* sigma
);

/* ============================================================================
 * Coordinator Placement
 * ============================================================================ */

/**
 * @brief Compute recommended coordinator placement
 *
 * WHAT: Determine optimal coordinator configuration
 * WHY:  Balance load and minimize communication overhead
 * HOW:  Use topology metrics to guide placement
 *
 * @param ctx Topology context
 * @param placement Output placement recommendation
 * @return NIMCP_OK on success
 *
 * @note Caller must free placement->hub_assignments and placement->load_distribution
 */
nimcp_error_t mesh_topology_recommend_placement(
    mesh_topology_ctx_t ctx,
    mesh_coord_placement_t* placement
);

/**
 * @brief Compute optimal pool size for participant count
 *
 * WHAT: Calculate coordinator pool size
 * WHY:  Scale coordinator count with log(participants)
 * HOW:  pool_size = max(3, min(8, 2*log2(N)))
 *
 * @param num_participants Number of participants
 * @return Recommended pool size
 */
uint32_t mesh_topology_optimal_pool_size(uint32_t num_participants);

/**
 * @brief Assign participant to coordinator
 *
 * WHAT: Determine which coordinator should handle a participant
 * WHY:  Load balancing based on topology
 * HOW:  Hash-based with hub priority to leader
 *
 * @param ctx Topology context
 * @param participant_id Participant to assign
 * @param pool_size Number of coordinators in pool
 * @return Coordinator index [0, pool_size)
 */
uint32_t mesh_topology_assign_coordinator(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t participant_id,
    uint32_t pool_size
);

/**
 * @brief Rebalance participant assignments after topology change
 *
 * WHAT: Recalculate assignments for all participants
 * WHY:  Maintain balance after join/leave events
 *
 * @param ctx Topology context
 * @param pool_size Number of coordinators
 * @param assignments Output array (indexed by participant order)
 * @param num_assignments Output: number of assignments
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_topology_rebalance_assignments(
    mesh_topology_ctx_t ctx,
    uint32_t pool_size,
    uint32_t* assignments,
    size_t* num_assignments
);

/* ============================================================================
 * Cluster Detection
 * ============================================================================ */

/**
 * @brief Detect clusters in topology
 *
 * WHAT: Find densely connected subgroups
 * WHY:  Co-locate cluster members to same coordinator
 * HOW:  Louvain community detection algorithm
 *
 * @param ctx Topology context
 * @param max_clusters Maximum clusters to detect
 * @param num_clusters Output: actual clusters found
 * @return NIMCP_OK on success (access cluster_id via get_node_info)
 */
nimcp_error_t mesh_topology_detect_clusters(
    mesh_topology_ctx_t ctx,
    uint32_t max_clusters,
    uint32_t* num_clusters
);

/**
 * @brief Get participants in a cluster
 *
 * WHAT: Retrieve all participants in given cluster
 * WHY:  Access cluster membership
 *
 * @param ctx Topology context
 * @param cluster_id Cluster ID
 * @param participants Output array (caller allocates)
 * @param capacity Array capacity
 * @param count Output: actual count
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_topology_get_cluster_members(
    mesh_topology_ctx_t ctx,
    uint32_t cluster_id,
    mesh_participant_id_t* participants,
    size_t capacity,
    size_t* count
);

/* ============================================================================
 * Statistics and Debugging
 * ============================================================================ */

/**
 * @brief Get topology context statistics
 *
 * @param ctx Topology context
 * @param num_participants Output: participant count
 * @param num_connections Output: connection count
 * @param num_hubs Output: identified hubs
 * @param num_clusters Output: detected clusters
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_topology_get_stats(
    mesh_topology_ctx_t ctx,
    uint32_t* num_participants,
    uint32_t* num_connections,
    uint32_t* num_hubs,
    uint32_t* num_clusters
);

/**
 * @brief Print topology debug information
 *
 * @param ctx Topology context
 */
void mesh_topology_print_debug(mesh_topology_ctx_t ctx);

/* ============================================================================
 * Free Functions
 * ============================================================================ */

/**
 * @brief Free coordinator placement resources
 *
 * @param placement Placement to free (fields only, not struct itself)
 */
void mesh_coord_placement_free(mesh_coord_placement_t* placement);

/* ============================================================================
 * BBB Integration API
 * ============================================================================ */

/**
 * Forward declaration for BBB system
 */
#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif

struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/**
 * @brief Set BBB system for mesh topology validation
 *
 * WHAT: Configure BBB for topology change validation
 * WHY:  Prevent unauthorized topology modifications
 *
 * @param bbb BBB system (can be NULL to disable)
 */
void mesh_topology_set_bbb(bbb_system_t bbb);

/**
 * @brief Get current BBB system for mesh topology
 *
 * @return BBB system or NULL
 */
bbb_system_t mesh_topology_get_bbb(void);

/**
 * @brief Set health agent for mesh topology heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void mesh_topology_set_health_agent(nimcp_health_agent_t* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_TOPOLOGY_H */
