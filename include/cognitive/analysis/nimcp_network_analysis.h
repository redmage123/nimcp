//=============================================================================
// nimcp_network_analysis.h - Brain Network Analysis Module
//=============================================================================

#ifndef NIMCP_NETWORK_ANALYSIS_H
#define NIMCP_NETWORK_ANALYSIS_H

#include <stdbool.h>
#include <stdint.h>
#include "core/brain/nimcp_brain.h"
#include "common/nimcp_export.h"
#include "core/topology/nimcp_community_detection.h"  // Use existing community types
#include "async/nimcp_bio_router.h"  // Bio-async integration

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_network_analysis.h
 * @brief Cognitive module for analyzing brain network topology
 *
 * WHAT: Analyzes functional organization of brain networks
 * WHY:  Understand how learning shapes network topology
 * HOW:  Community detection + hub analysis + metrics tracking
 *
 * BIOLOGY: Brain networks organize into functional modules (communities)
 *          with hub neurons coordinating information flow
 *
 * INTEGRATION: Works with curiosity, consolidation, meta-learning
 *
 * Example:
 * ```c
 * network_analyzer_t* analyzer = network_analyzer_create(brain);
 * network_analyzer_run(analyzer);  // Detect communities + hubs
 * network_analyzer_print_report(analyzer);  // Show topology
 * ```
 */

//=============================================================================
// Core Types
//=============================================================================

// NOTE: community_structure_t is defined in core/topology/nimcp_community_detection.h
// We use the existing topology module's definition to avoid conflicts

/**
 * @brief Community structure (functional module)
 * NOTE: This is a helper type for network_analyzer, distinct from topology's community_structure_t
 */
typedef struct {
    uint32_t* neurons;       /**< Neuron IDs in this community */
    uint32_t size;           /**< Number of neurons */
    float internal_density;  /**< Connection density within community */
    float external_density;  /**< Connection density to other communities */
    char label[64];          /**< Human-readable label (e.g., "visual", "motor") */
} analyzer_community_t;

/**
 * @brief Hub neuron (high centrality)
 */
typedef struct {
    uint32_t neuron_id;       /**< Neuron ID */
    float degree_centrality;  /**< Normalized degree centrality [0, 1] */
    float betweenness;        /**< Betweenness centrality [0, 1] */
    uint32_t community_id;    /**< Which community this hub belongs to */
    bool is_connector_hub;    /**< Connects multiple communities */
} hub_neuron_t;

/**
 * @brief Hub detection results
 */
typedef struct {
    hub_neuron_t* hubs;       /**< Array of hub neurons */
    uint32_t num_hubs;        /**< Number of hubs detected */
    float hub_threshold;      /**< Centrality threshold used */
    uint64_t timestamp;       /**< When analysis was performed */
} hub_detection_t;

/**
 * @brief Network topology metrics
 */
typedef struct {
    float clustering_coefficient;  /**< Global clustering [0, 1] */
    float avg_path_length;         /**< Average shortest path length */
    float small_worldness;         /**< Small-world coefficient */
    float assortativity;           /**< Degree assortativity [-1, 1] */
    uint32_t num_edges;            /**< Total edges */
    float density;                 /**< Network density [0, 1] */
} topology_metrics_t;

/**
 * @brief Network analyzer (cognitive module)
 */
typedef struct network_analyzer_struct {
    brain_t brain;  /**< Parent brain */

    // Analysis results
    community_structure_t* communities;  /**< Current community structure */
    hub_detection_t* hubs;               /**< Current hub neurons */
    topology_metrics_t metrics;          /**< Current topology metrics */

    // Analysis history
    uint32_t analysis_count;            /**< Number of analyses performed */
    float* modularity_history;          /**< Track Q over time */
    uint32_t* community_count_history;  /**< Track # modules over time */
    uint32_t history_capacity;          /**< Max history entries */

    // Configuration
    bool auto_analyze;              /**< Run after each learning event */
    uint32_t analysis_interval;     /**< Analyze every N iterations */
    float hub_threshold;            /**< Centrality threshold [0.5, 1.0] */
    uint32_t iteration_counter;     /**< Track iterations since last analysis */

    // Validation
    bool topology_is_valid;         /**< Is topology healthy? */
    char last_error[256];           /**< Last validation error */

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */
} network_analyzer_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create network analyzer
 *
 * WHAT: Initialize network analysis module
 * WHY:  Enable topology tracking for brain
 * HOW:  Allocate analyzer, set defaults
 *
 * @param brain Parent brain to analyze
 * @return Analyzer or NULL on error
 */
NIMCP_EXPORT network_analyzer_t* network_analyzer_create(brain_t brain);

/**
 * @brief Destroy network analyzer
 *
 * WHAT: Free all analyzer resources
 * WHY:  Clean shutdown, prevent leaks
 * HOW:  Free communities, hubs, history
 *
 * @param analyzer Analyzer to destroy
 */
NIMCP_EXPORT void network_analyzer_destroy(network_analyzer_t* analyzer);

//=============================================================================
// Analysis Operations
//=============================================================================

/**
 * @brief Run complete network analysis
 *
 * WHAT: Detect communities + hubs + compute metrics
 * WHY:  Understand current network organization
 * HOW:  Louvain algorithm + degree/betweenness + topology stats
 *
 * BIOLOGY: Brain networks reorganize during learning
 *
 * @param analyzer Network analyzer
 * @return true on success, false on error
 */
NIMCP_EXPORT bool network_analyzer_run(network_analyzer_t* analyzer);

/**
 * @brief Detect communities only (faster)
 *
 * WHAT: Run community detection algorithm
 * WHY:  Quick topology check without full analysis
 * HOW:  Louvain or label propagation
 *
 * @param analyzer Network analyzer
 * @return true on success, false on error
 */
NIMCP_EXPORT bool network_analyzer_detect_communities(network_analyzer_t* analyzer);

/**
 * @brief Detect hub neurons only
 *
 * WHAT: Find high-centrality neurons
 * WHY:  Identify critical neurons for information flow
 * HOW:  Compute degree + betweenness centrality
 *
 * @param analyzer Network analyzer
 * @return true on success, false on error
 */
NIMCP_EXPORT bool network_analyzer_detect_hubs(network_analyzer_t* analyzer);

/**
 * @brief Compute topology metrics
 *
 * WHAT: Calculate clustering, path length, small-worldness
 * WHY:  Quantify network organization
 * HOW:  Graph theory algorithms
 *
 * @param analyzer Network analyzer
 * @return true on success, false on error
 */
NIMCP_EXPORT bool network_analyzer_compute_metrics(network_analyzer_t* analyzer);

//=============================================================================
// Validation
//=============================================================================

/**
 * @brief Validate learning didn't break topology
 *
 * WHAT: Check if STDP/pruning damaged network
 * WHY:  Catch pathological reorganization
 * HOW:  Check modularity, connectivity, hub preservation
 *
 * EXAMPLE CHECKS:
 * - Modularity didn't collapse (Q > 0.2)
 * - No isolated communities
 * - Hub neurons still connected
 * - No excessive fragmentation
 *
 * @param analyzer Network analyzer
 * @return true if topology is healthy, false if damaged
 */
NIMCP_EXPORT bool network_analyzer_validate_learning(network_analyzer_t* analyzer);

/**
 * @brief Get last validation error
 *
 * WHAT: Retrieve error message from failed validation
 * WHY:  Understand what went wrong
 * HOW:  Return error string
 *
 * @param analyzer Network analyzer
 * @return Error message (empty if no error)
 */
NIMCP_EXPORT const char* network_analyzer_get_error(network_analyzer_t* analyzer);

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Enable/disable auto-analysis
 *
 * WHAT: Set whether to analyze after each learning event
 * WHY:  Track topology changes during learning
 * HOW:  Set flag + interval
 *
 * @param analyzer Network analyzer
 * @param enable Enable auto-analysis
 * @param interval Analyze every N iterations (0 = every iteration)
 */
NIMCP_EXPORT void network_analyzer_set_auto_analyze(network_analyzer_t* analyzer,
                                                     bool enable,
                                                     uint32_t interval);

/**
 * @brief Set hub detection threshold
 *
 * WHAT: Configure centrality threshold for hubs
 * WHY:  Control sensitivity of hub detection
 * HOW:  Set threshold [0.5, 1.0]
 *
 * @param analyzer Network analyzer
 * @param threshold Centrality threshold (0.5 = top 50%, 0.9 = top 10%)
 */
NIMCP_EXPORT void network_analyzer_set_hub_threshold(network_analyzer_t* analyzer,
                                                      float threshold);

//=============================================================================
// Query Results
//=============================================================================

/**
 * @brief Get current community structure
 *
 * WHAT: Retrieve latest community detection results
 * WHY:  Query functional organization
 * HOW:  Return community structure pointer
 *
 * @param analyzer Network analyzer
 * @return Community structure (NULL if not analyzed yet)
 */
NIMCP_EXPORT const community_structure_t* network_analyzer_get_communities(
    network_analyzer_t* analyzer);

/**
 * @brief Get current hub neurons
 *
 * WHAT: Retrieve latest hub detection results
 * WHY:  Query critical neurons
 * HOW:  Return hub detection pointer
 *
 * @param analyzer Network analyzer
 * @return Hub detection results (NULL if not analyzed yet)
 */
NIMCP_EXPORT const hub_detection_t* network_analyzer_get_hubs(network_analyzer_t* analyzer);

/**
 * @brief Get current topology metrics
 *
 * WHAT: Retrieve latest topology statistics
 * WHY:  Query network organization
 * HOW:  Return metrics struct
 *
 * @param analyzer Network analyzer
 * @return Topology metrics
 */
NIMCP_EXPORT topology_metrics_t network_analyzer_get_metrics(network_analyzer_t* analyzer);

/**
 * @brief Get modularity history
 *
 * WHAT: Retrieve how modularity changed over time
 * WHY:  Track learning's effect on organization
 * HOW:  Return history array
 *
 * @param analyzer Network analyzer
 * @param count Output: number of entries
 * @return Modularity history array (NULL if no history)
 */
NIMCP_EXPORT const float* network_analyzer_get_modularity_history(network_analyzer_t* analyzer,
                                                                   uint32_t* count);

//=============================================================================
// Reporting
//=============================================================================

/**
 * @brief Print analysis report
 *
 * WHAT: Print communities, hubs, metrics
 * WHY:  Human-readable topology summary
 * HOW:  Format and print to stdout
 *
 * OUTPUT EXAMPLE:
 * ```
 * === Network Topology Analysis ===
 * Communities: 5
 * Modularity Q: 0.62
 * Hub neurons: 12
 * Clustering: 0.45
 * Small-worldness: 2.3
 *
 * Community 1 (234 neurons): visual processing
 * Community 2 (189 neurons): motor control
 * ...
 * ```
 *
 * @param analyzer Network analyzer
 */
NIMCP_EXPORT void network_analyzer_print_report(network_analyzer_t* analyzer);

/**
 * @brief Print modularity trend
 *
 * WHAT: Print how modularity changed over learning
 * WHY:  Visualize network reorganization
 * HOW:  Print ASCII chart of modularity history
 *
 * @param analyzer Network analyzer
 */
NIMCP_EXPORT void network_analyzer_print_modularity_trend(network_analyzer_t* analyzer);

//=============================================================================
// Integration Hooks (for cognitive modules)
//=============================================================================

/**
 * @brief Notify analyzer of learning event
 *
 * WHAT: Called after STDP/pruning/learning
 * WHY:  Trigger auto-analysis if enabled
 * HOW:  Increment counter, maybe run analysis
 *
 * CALLED BY: Plasticity modules, consolidation, meta-learning
 *
 * @param analyzer Network analyzer
 */
NIMCP_EXPORT void network_analyzer_on_learning_event(network_analyzer_t* analyzer);

/**
 * @brief Check if new community emerged
 *
 * WHAT: Compare current communities to previous
 * WHY:  Detect novel functional modules
 * HOW:  Compare community counts + composition
 *
 * USED BY: Curiosity (novelty detection)
 *
 * @param analyzer Network analyzer
 * @return true if new community emerged
 */
NIMCP_EXPORT bool network_analyzer_check_new_community(network_analyzer_t* analyzer);

#ifdef __cplusplus
}
#endif
#endif  // NIMCP_NETWORK_ANALYSIS_H
