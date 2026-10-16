/**
 * @file nimcp_kg_analytics.h
 * @brief Graph Analytics and Insights for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Analytics module for KG access patterns, topology health, capacity planning, and optimization
 * WHY:  Enable performance monitoring, predictive planning, and self-optimization of the KG
 * HOW:  Statistical analysis of access patterns, graph topology metrics, and ML-based forecasting
 *
 * ANALYTICS CAPABILITIES:
 * ```
 * +===========================================================================+
 * |                    KG ANALYTICS SYSTEM                                    |
 * +===========================================================================+
 * |                                                                            |
 * |   Access Pattern Analysis                                                  |
 * |   -----------------------------------------------------------------        |
 * |   | Track read/write/query counts per node                          |      |
 * |   | Identify hot nodes (frequently accessed)                        |      |
 * |   | Identify cold nodes (rarely accessed, archive candidates)       |      |
 * |   | Compute access frequency and temporal patterns                  |      |
 * |   -----------------------------------------------------------------        |
 * |                                                                            |
 * |   Topology Health                                                          |
 * |   -----------------------------------------------------------------        |
 * |   | Connectivity score - how well connected is the graph?           |      |
 * |   | Balance score - is load evenly distributed?                     |      |
 * |   | Redundancy score - are there backup paths?                      |      |
 * |   | Identify isolated nodes, bottlenecks, dead ends                 |      |
 * |   -----------------------------------------------------------------        |
 * |                                                                            |
 * |   Capacity Planning                                                        |
 * |   -----------------------------------------------------------------        |
 * |   | Project node/storage growth over 30/90 days                     |      |
 * |   | Estimate days until capacity limits reached                     |      |
 * |   | Monitor growth rate trends                                      |      |
 * |   -----------------------------------------------------------------        |
 * |                                                                            |
 * |   Optimization Recommendations                                             |
 * |   -----------------------------------------------------------------        |
 * |   | Suggest index creation/removal                                  |      |
 * |   | Recommend denormalization or partitioning                       |      |
 * |   | Identify caching opportunities                                  |      |
 * |   | Estimate improvement and implementation cost                    |      |
 * |   -----------------------------------------------------------------        |
 * |                                                                            |
 * |   Query Analysis                                                           |
 * |   -----------------------------------------------------------------        |
 * |   | Track slow query execution times                                |      |
 * |   | Explain query execution plans                                   |      |
 * |   | Identify optimization opportunities                             |      |
 * |   -----------------------------------------------------------------        |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * BIOLOGICAL BASIS:
 * - Mirrors brain's metabolic regulation (tracking resource usage)
 * - Analogous to neural plasticity (optimization recommendations)
 * - Similar to predictive coding (capacity forecasting)
 *
 * THREAD SAFETY: All analytics operations are thread-safe (read-only on KG)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_ANALYTICS_H
#define NIMCP_KG_ANALYTICS_H

#include "core/brain/nimcp_brain_kg.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum length of optimization description */
#define KG_ANALYTICS_MAX_DESC_LEN        256

/** Maximum length of optimization target name */
#define KG_ANALYTICS_MAX_TARGET_LEN      64

/** Maximum length of query explanation */
#define KG_ANALYTICS_MAX_EXPLAIN_LEN     4096

/** Maximum length of query string in slow query record */
#define KG_ANALYTICS_MAX_QUERY_LEN       512

/** Default hot node threshold (accesses per second) */
#define KG_ANALYTICS_HOT_THRESHOLD       10.0f

/** Default cold node threshold (accesses per second) */
#define KG_ANALYTICS_COLD_THRESHOLD      0.01f

/* ============================================================================
 * Access Pattern Analysis
 * ============================================================================ */

/**
 * @brief Access pattern record for a single node
 *
 * WHAT: Statistics tracking read/write/query access to a KG node
 * WHY:  Enable identification of hot/cold nodes for optimization
 * HOW:  Counters incremented on each access, frequency computed periodically
 *
 * Access frequency is computed as total accesses divided by observation window.
 * A node is considered "hot" if access_frequency exceeds the hot threshold.
 */
typedef struct {
    brain_kg_node_id_t node_id;          /**< KG node being tracked */
    uint64_t read_count;                  /**< Total read operations */
    uint64_t write_count;                 /**< Total write operations */
    uint64_t query_count;                 /**< Total query operations (traversals through this node) */
    uint64_t last_access;                 /**< Last access timestamp (ms since epoch) */
    float access_frequency;               /**< Computed accesses per second */
    bool is_hot;                          /**< True if frequently accessed (above threshold) */
} kg_access_pattern_t;

/* ============================================================================
 * Topology Health Metrics
 * ============================================================================ */

/**
 * @brief Topology health metrics for the knowledge graph
 *
 * WHAT: Structural health indicators for the graph topology
 * WHY:  Identify connectivity issues, bottlenecks, and structural problems
 * HOW:  Graph analysis algorithms (connectivity, centrality, path analysis)
 *
 * SCORING:
 * - connectivity_score: 1.0 = fully connected, 0.0 = completely disconnected
 * - balance_score: 1.0 = perfectly balanced degree distribution, 0.0 = star topology
 * - redundancy_score: 1.0 = every pair has multiple paths, 0.0 = tree structure
 */
typedef struct {
    float connectivity_score;             /**< Graph connectivity [0.0-1.0] (higher = better connected) */
    float balance_score;                  /**< Degree distribution balance [0.0-1.0] (higher = more balanced) */
    float redundancy_score;               /**< Path redundancy [0.0-1.0] (higher = more alternate paths) */
    uint32_t isolated_nodes;              /**< Count of nodes with no connections */
    uint32_t bottleneck_nodes;            /**< Count of single points of failure (high betweenness) */
    uint32_t dead_end_paths;              /**< Count of paths with no exit (terminal nodes) */
} kg_topology_health_t;

/* ============================================================================
 * Capacity Forecasting
 * ============================================================================ */

/**
 * @brief Capacity forecast for knowledge graph growth
 *
 * WHAT: Projections for node count and storage usage over time
 * WHY:  Enable proactive capacity planning and resource allocation
 * HOW:  Time series analysis of historical growth data
 *
 * Projections are based on observed growth patterns.
 * days_until_capacity assumes current growth rate continues linearly.
 */
typedef struct {
    uint64_t current_nodes;               /**< Current total node count */
    uint64_t projected_nodes_30d;         /**< Projected nodes in 30 days */
    uint64_t projected_nodes_90d;         /**< Projected nodes in 90 days */
    uint64_t current_storage_bytes;       /**< Current storage usage in bytes */
    uint64_t projected_storage_30d;       /**< Projected storage in 30 days (bytes) */
    uint64_t projected_storage_90d;       /**< Projected storage in 90 days (bytes) */
    float growth_rate_percent;            /**< Monthly growth rate as percentage */
    uint64_t days_until_capacity;         /**< Days until configured capacity limit (0 = unlimited) */
} kg_capacity_forecast_t;

/* ============================================================================
 * Optimization Recommendations
 * ============================================================================ */

/**
 * @brief Optimization recommendation type
 *
 * WHAT: Types of optimization actions that can be recommended
 * WHY:  Categorize recommendations for filtering and prioritization
 * HOW:  Enumeration of supported optimization strategies
 */
typedef enum {
    KG_OPT_CREATE_INDEX = 0,             /**< Create a new index for faster lookups */
    KG_OPT_DROP_INDEX,                    /**< Remove unused index to save space */
    KG_OPT_DENORMALIZE,                   /**< Denormalize data for query performance */
    KG_OPT_PARTITION,                     /**< Partition large tables/node sets */
    KG_OPT_ARCHIVE,                       /**< Archive cold data to secondary storage */
    KG_OPT_CACHE                          /**< Add caching layer for hot data */
} kg_optimization_type_t;

/**
 * @brief Optimization recommendation
 *
 * WHAT: A suggested optimization action with impact estimates
 * WHY:  Guide administrators in improving KG performance
 * HOW:  Analysis of access patterns, query plans, and structural metrics
 *
 * expected_improvement is the estimated percentage improvement in query time
 * or resource usage. estimated_cost_ms is the time to implement the change.
 */
typedef struct {
    kg_optimization_type_t type;          /**< Type of optimization */
    char description[KG_ANALYTICS_MAX_DESC_LEN]; /**< Human-readable description */
    char target[KG_ANALYTICS_MAX_TARGET_LEN]; /**< Target table/node type/field affected */
    float expected_improvement;           /**< Estimated improvement factor [0.0-1.0] (0.3 = 30% better) */
    uint32_t estimated_cost_ms;           /**< Estimated implementation time in milliseconds */
} kg_optimization_t;

/* ============================================================================
 * Query Analysis
 * ============================================================================ */

/**
 * @brief Slow query record
 *
 * WHAT: Record of a query that exceeded performance thresholds
 * WHY:  Enable identification and optimization of problematic queries
 * HOW:  Query execution monitoring with threshold-based logging
 *
 * Slow queries are tracked when execution_time_ms exceeds the configured
 * slow query threshold (typically 100ms for interactive queries).
 */
typedef struct {
    char query[KG_ANALYTICS_MAX_QUERY_LEN]; /**< Query string or pattern */
    uint64_t execution_time_ms;           /**< Execution time in milliseconds */
    uint64_t timestamp;                   /**< When the query was executed (ms since epoch) */
    uint32_t rows_examined;               /**< Number of nodes/rows examined */
    uint32_t rows_returned;               /**< Number of results returned */
    brain_kg_node_id_t start_node;        /**< Starting node for traversal queries */
    bool used_index;                      /**< Whether an index was used */
    char bottleneck[64];                  /**< Identified bottleneck (if any) */
} kg_slow_query_t;

/* ============================================================================
 * Access Pattern Analysis API
 * ============================================================================ */

/**
 * @brief Get access patterns for all tracked nodes
 *
 * WHAT: Retrieve access statistics for KG nodes
 * WHY:  Analyze usage patterns for optimization decisions
 * HOW:  Query internal access counters, compute frequencies
 *
 * @param kg Knowledge graph to analyze
 * @param patterns Output array for access patterns (caller allocated)
 * @param max Maximum number of patterns to return
 * @param count Output: actual number of patterns returned
 * @return 0 on success, -1 on error
 */
int kg_analytics_get_access_patterns(
    const brain_kg_t* kg,
    kg_access_pattern_t* patterns,
    uint32_t max,
    uint32_t* count
);

/**
 * @brief Get frequently accessed (hot) nodes
 *
 * WHAT: Identify nodes with high access frequency
 * WHY:  Candidates for caching, indexing, or denormalization
 * HOW:  Filter nodes by access_frequency > hot_threshold
 *
 * @param kg Knowledge graph to analyze
 * @param nodes Output array for hot node IDs (caller allocated)
 * @param max Maximum number of nodes to return
 * @param count Output: actual number of hot nodes found
 * @return 0 on success, -1 on error
 */
int kg_analytics_get_hot_nodes(
    const brain_kg_t* kg,
    brain_kg_node_id_t* nodes,
    uint32_t max,
    uint32_t* count
);

/**
 * @brief Get rarely accessed (cold) nodes
 *
 * WHAT: Identify nodes with low access frequency
 * WHY:  Candidates for archiving or removal
 * HOW:  Filter nodes by access_frequency < cold_threshold
 *
 * @param kg Knowledge graph to analyze
 * @param nodes Output array for cold node IDs (caller allocated)
 * @param max Maximum number of nodes to return
 * @param count Output: actual number of cold nodes found
 * @return 0 on success, -1 on error
 */
int kg_analytics_get_cold_nodes(
    const brain_kg_t* kg,
    brain_kg_node_id_t* nodes,
    uint32_t max,
    uint32_t* count
);

/* ============================================================================
 * Topology Health API
 * ============================================================================ */

/**
 * @brief Check overall topology health of the knowledge graph
 *
 * WHAT: Compute structural health metrics for the graph
 * WHY:  Identify connectivity issues and structural problems
 * HOW:  Graph analysis algorithms for connectivity, balance, redundancy
 *
 * @param kg Knowledge graph to analyze
 * @param health Output: topology health metrics
 * @return 0 on success, -1 on error
 */
int kg_analytics_check_topology_health(
    const brain_kg_t* kg,
    kg_topology_health_t* health
);

/**
 * @brief Find bottleneck nodes in the graph
 *
 * WHAT: Identify nodes that are single points of failure
 * WHY:  High betweenness centrality indicates critical paths
 * HOW:  Compute betweenness centrality, threshold for bottlenecks
 *
 * Bottleneck nodes are those whose removal would significantly
 * increase the average path length or disconnect components.
 *
 * @param kg Knowledge graph to analyze
 * @param nodes Output array for bottleneck node IDs (caller allocated)
 * @param max Maximum number of nodes to return
 * @param count Output: actual number of bottlenecks found
 * @return 0 on success, -1 on error
 */
int kg_analytics_find_bottlenecks(
    const brain_kg_t* kg,
    brain_kg_node_id_t* nodes,
    uint32_t max,
    uint32_t* count
);

/**
 * @brief Find isolated nodes in the graph
 *
 * WHAT: Identify nodes with no connections
 * WHY:  Isolated nodes may indicate data quality issues or orphaned data
 * HOW:  Filter nodes with degree = 0
 *
 * @param kg Knowledge graph to analyze
 * @param nodes Output array for isolated node IDs (caller allocated)
 * @param max Maximum number of nodes to return
 * @param count Output: actual number of isolated nodes found
 * @return 0 on success, -1 on error
 */
int kg_analytics_find_isolated(
    const brain_kg_t* kg,
    brain_kg_node_id_t* nodes,
    uint32_t max,
    uint32_t* count
);

/* ============================================================================
 * Capacity Planning API
 * ============================================================================ */

/**
 * @brief Generate capacity forecast for the knowledge graph
 *
 * WHAT: Project future node count and storage requirements
 * WHY:  Enable proactive capacity planning
 * HOW:  Analyze historical growth data, extrapolate trends
 *
 * @param kg Knowledge graph to analyze
 * @param forecast Output: capacity forecast data
 * @return 0 on success, -1 on error
 */
int kg_analytics_forecast_capacity(
    const brain_kg_t* kg,
    kg_capacity_forecast_t* forecast
);

/**
 * @brief Estimate graph size after a given number of days
 *
 * WHAT: Project total node count at a future date
 * WHY:  Answer "how big will the graph be in X days?"
 * HOW:  Apply observed growth rate to current size
 *
 * @param kg Knowledge graph to analyze
 * @param days Number of days to project
 * @param projected_size Output: estimated node count
 * @return 0 on success, -1 on error
 */
int kg_analytics_estimate_growth(
    const brain_kg_t* kg,
    uint32_t days,
    uint64_t* projected_size
);

/* ============================================================================
 * Optimization Recommendations API
 * ============================================================================ */

/**
 * @brief Get optimization recommendations for the knowledge graph
 *
 * WHAT: Generate actionable optimization suggestions
 * WHY:  Guide performance tuning decisions
 * HOW:  Analyze access patterns, query plans, and structural metrics
 *
 * Recommendations are sorted by expected_improvement (highest first).
 *
 * @param kg Knowledge graph to analyze
 * @param recommendations Output array for recommendations (caller allocated)
 * @param max Maximum number of recommendations to return
 * @param count Output: actual number of recommendations
 * @return 0 on success, -1 on error
 */
int kg_analytics_get_recommendations(
    const brain_kg_t* kg,
    kg_optimization_t* recommendations,
    uint32_t max,
    uint32_t* count
);

/**
 * @brief Apply an optimization recommendation
 *
 * WHAT: Execute a recommended optimization action
 * WHY:  Implement suggested improvements
 * HOW:  Dispatch to appropriate optimization handler based on type
 *
 * Note: This function may take significant time for large optimizations.
 * Consider running in a background thread for production use.
 *
 * @param kg Knowledge graph to optimize (mutable)
 * @param recommendation Recommendation to apply
 * @return 0 on success, -1 on error
 */
int kg_analytics_apply_recommendation(
    brain_kg_t* kg,
    const kg_optimization_t* recommendation
);

/* ============================================================================
 * Query Analysis API
 * ============================================================================ */

/**
 * @brief Get slow query records
 *
 * WHAT: Retrieve logged slow queries for analysis
 * WHY:  Identify queries needing optimization
 * HOW:  Query the slow query log, sorted by execution time (slowest first)
 *
 * @param kg Knowledge graph to query
 * @param queries Output array for slow query records (caller allocated)
 * @param max Maximum number of queries to return
 * @param count Output: actual number of slow queries
 * @return 0 on success, -1 on error
 */
int kg_analytics_get_slow_queries(
    const brain_kg_t* kg,
    kg_slow_query_t* queries,
    uint32_t max,
    uint32_t* count
);

/**
 * @brief Explain a query execution plan
 *
 * WHAT: Generate human-readable explanation of how a query would execute
 * WHY:  Debug query performance, understand index usage
 * HOW:  Parse query, generate execution plan, format as text
 *
 * The explanation includes:
 * - Query parsing and interpretation
 * - Index usage decisions
 * - Estimated node examination count
 * - Potential bottlenecks
 * - Optimization suggestions
 *
 * @param kg Knowledge graph context
 * @param query Query string to explain
 * @param explanation Output buffer for explanation text (caller allocated)
 * @param max_size Size of explanation buffer
 * @return 0 on success, -1 on error
 */
int kg_analytics_explain_query(
    const brain_kg_t* kg,
    const char* query,
    char* explanation,
    size_t max_size
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert optimization type to string
 *
 * @param type Optimization type enum value
 * @return String representation (e.g., "CREATE_INDEX", "ARCHIVE")
 */
const char* kg_optimization_type_to_string(kg_optimization_type_t type);

/**
 * @brief Reset access pattern counters
 *
 * WHAT: Clear all access pattern statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zero all counters, reset timestamps
 *
 * @param kg Knowledge graph to reset (mutable)
 * @return 0 on success, -1 on error
 */
int kg_analytics_reset_access_patterns(brain_kg_t* kg);

/**
 * @brief Clear slow query log
 *
 * WHAT: Remove all recorded slow queries
 * WHY:  Clear historical data after optimization
 * HOW:  Truncate slow query log
 *
 * @param kg Knowledge graph to clear (mutable)
 * @return 0 on success, -1 on error
 */
int kg_analytics_clear_slow_queries(brain_kg_t* kg);

/**
 * @brief Set the slow query threshold
 *
 * WHAT: Configure the minimum execution time for slow query logging
 * WHY:  Adjust sensitivity based on workload requirements
 * HOW:  Update threshold value in analytics configuration
 *
 * @param kg Knowledge graph to configure (mutable)
 * @param threshold_ms Minimum execution time in milliseconds to log as slow
 * @return 0 on success, -1 on error
 */
int kg_analytics_set_slow_query_threshold(brain_kg_t* kg, uint32_t threshold_ms);

/**
 * @brief Set hot/cold node thresholds
 *
 * WHAT: Configure access frequency thresholds for hot/cold classification
 * WHY:  Adjust classification sensitivity for specific workloads
 * HOW:  Update threshold values in analytics configuration
 *
 * @param kg Knowledge graph to configure (mutable)
 * @param hot_threshold Minimum accesses/sec to classify as hot
 * @param cold_threshold Maximum accesses/sec to classify as cold
 * @return 0 on success, -1 on error
 */
int kg_analytics_set_access_thresholds(
    brain_kg_t* kg,
    float hot_threshold,
    float cold_threshold
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_ANALYTICS_H */
