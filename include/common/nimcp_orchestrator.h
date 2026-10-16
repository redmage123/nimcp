//=============================================================================
// nimcp_orchestrator.h - Network Orchestration Layer
//=============================================================================

#ifndef NIMCP_ORCHESTRATOR_H
#define NIMCP_ORCHESTRATOR_H

#include "common/nimcp_export.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "networking/p2p/nimcp_p2pnode.h"
#include "networking/protocol/nimcp_protocol.h"

/**
 * @file nimcp_orchestrator.h
 * @brief High-level network orchestration for NIMCP 2.0
 *
 * Provides optional orchestration capabilities including:
 * - Global learning parameter management
 * - Topology optimization
 * - Feature namespace management
 * - System health monitoring
 * - LLM-driven semantic grounding
 */

//=============================================================================
// Orchestration Types
//=============================================================================

/**
 * @brief Orchestration modes
 */
typedef enum {
    ORCHESTRATION_MODE_AUTONOMOUS, /**< Fully autonomous, no orchestrator */
    ORCHESTRATION_MODE_GUIDED,     /**< Orchestrator provides hints */
    ORCHESTRATION_MODE_MANAGED     /**< Orchestrator actively controls */
} orchestration_mode_t;

/**
 * @brief Cluster information
 */
typedef struct {
    uint32_t cluster_id;           /**< Unique cluster identifier */
    feature_code_t specialization; /**< Primary feature specialization */
    uint32_t* member_nodes;        /**< Array of member node IDs */
    uint32_t num_members;          /**< Number of members */
    uint32_t* hub_nodes;           /**< Array of hub node IDs */
    uint32_t num_hubs;             /**< Number of hubs */
    float density;                 /**< Internal connectivity (0.0-1.0) */
    float mean_spike_rate;         /**< Average spike rate */
    float synchrony_index;         /**< Synchronization measure */
} cluster_info_t;

/**
 * @brief Network topology statistics
 */
typedef struct {
    uint32_t total_nodes;         /**< Total nodes in network */
    uint32_t total_links;         /**< Total synaptic connections */
    uint32_t num_clusters;        /**< Number of detected clusters */
    float avg_path_length;        /**< Average path length */
    float clustering_coefficient; /**< Network clustering */
    float network_efficiency;     /**< Communication efficiency */
    uint64_t observation_time;    /**< When stats were collected */
} topology_stats_t;

/**
 * @brief Learning rate policy
 */
typedef struct {
    uint32_t target_spec;    /**< Target (node ID, cluster ID, or 0xFFFFFFFF for global) */
    float learning_rate;     /**< Learning rate value */
    uint64_t effective_time; /**< When to apply */
    uint32_t duration_ms;    /**< How long to apply (0 = permanent) */
    bool adaptive;           /**< Adjust based on performance */
} learning_rate_policy_t;

//=============================================================================
// Orchestrator Interface
//=============================================================================

/**
 * @brief Orchestrator decision callback
 *
 * Called when orchestrator needs to make a decision.
 *
 * @param stats Current network statistics
 * @param clusters Current cluster information
 * @param num_clusters Number of clusters
 * @param context User context
 */
typedef void (*orchestrator_callback_t)(const topology_stats_t* stats,
                                        const cluster_info_t* clusters, uint32_t num_clusters,
                                        void* context);

/**
 * @brief Orchestrator configuration
 */
typedef struct {
    orchestration_mode_t mode;        /**< Orchestration mode */
    uint32_t update_interval_ms;      /**< How often to run orchestrator */
    float optimization_threshold;     /**< Min improvement to apply changes */
    orchestrator_callback_t callback; /**< Decision callback */
    void* callback_context;           /**< Callback context */
    bool enable_llm_integration;      /**< Enable LLM for semantic tasks */
} orchestrator_config_t;

/**
 * @brief Orchestrator state
 */
typedef struct orchestrator_struct* orchestrator_t;

/**
 * @brief Create an orchestrator
 *
 * @param config Orchestrator configuration
 * @return Orchestrator handle, or NULL on error
 */
orchestrator_t orchestrator_create(const orchestrator_config_t* config);

/**
 * @brief Destroy an orchestrator
 *
 * @param orch Orchestrator to destroy
 */
void orchestrator_destroy(orchestrator_t orch);

/**
 * @brief Register a node with the orchestrator
 *
 * @param orch Orchestrator
 * @param node_id Node ID to register
 * @param network Neural network for this node
 * @return true on success
 */
bool orchestrator_register_node(orchestrator_t orch, uint32_t node_id, neural_network_t network);

/**
 * @brief Unregister a node from the orchestrator
 *
 * @param orch Orchestrator
 * @param node_id Node ID to unregister
 * @return true on success
 */
bool orchestrator_unregister_node(orchestrator_t orch, uint32_t node_id);

/**
 * @brief Update orchestrator with network observations
 *
 * Should be called periodically to provide orchestrator with network state.
 *
 * @param orch Orchestrator
 * @param timestamp Current time
 * @return true on success
 */
bool orchestrator_update(orchestrator_t orch, uint64_t timestamp);

/**
 * @brief Get current network topology statistics
 *
 * @param orch Orchestrator
 * @param stats Output: topology statistics
 * @return true on success
 */
bool orchestrator_get_topology_stats(orchestrator_t orch, topology_stats_t* stats);

/**
 * @brief Get detected clusters
 *
 * @param orch Orchestrator
 * @param clusters Output: array of cluster info
 * @param max_clusters Maximum clusters to return
 * @return Number of clusters returned
 */
uint32_t orchestrator_get_clusters(orchestrator_t orch, cluster_info_t* clusters,
                                   uint32_t max_clusters);

//=============================================================================
// Learning Rate Management
//=============================================================================

/**
 * @brief Set global learning rate
 *
 * @param orch Orchestrator
 * @param learning_rate New learning rate
 * @param duration_ms How long to apply (0 = permanent)
 * @return true on success
 */
bool orchestrator_set_global_learning_rate(orchestrator_t orch, float learning_rate,
                                           uint32_t duration_ms);

/**
 * @brief Set learning rate for specific cluster
 *
 * @param orch Orchestrator
 * @param cluster_id Target cluster
 * @param learning_rate New learning rate
 * @param duration_ms How long to apply
 * @return true on success
 */
bool orchestrator_set_cluster_learning_rate(orchestrator_t orch, uint32_t cluster_id,
                                            float learning_rate, uint32_t duration_ms);

/**
 * @brief Apply learning rate policy
 *
 * @param orch Orchestrator
 * @param policy Policy to apply
 * @return true on success
 */
bool orchestrator_apply_learning_policy(orchestrator_t orch, const learning_rate_policy_t* policy);

//=============================================================================
// Topology Optimization
//=============================================================================

/**
 * @brief Suggest new connections for optimization
 *
 * @param orch Orchestrator
 * @param from_node Source node
 * @param to_node Target node
 * @param weight Suggested weight
 * @return true if connection should be added
 */
bool orchestrator_suggest_connection(orchestrator_t orch, uint32_t from_node, uint32_t to_node,
                                     float* weight);

/**
 * @brief Optimize network topology
 *
 * Analyzes current topology and suggests improvements.
 *
 * @param orch Orchestrator
 * @param max_changes Maximum changes to suggest
 * @return Number of changes suggested
 */
uint32_t orchestrator_optimize_topology(orchestrator_t orch, uint32_t max_changes);

//=============================================================================
// Feature Namespace Management
//=============================================================================

/**
 * @brief Feature namespace definition
 */
typedef struct {
    uint8_t domain;             /**< Domain code */
    char name[64];              /**< Domain name */
    feature_code_t range_start; /**< Start of range */
    feature_code_t range_end;   /**< End of range */
    char description[256];      /**< Description */
} feature_namespace_t;

/**
 * @brief Define a feature namespace
 *
 * @param orch Orchestrator
 * @param ns Namespace definition
 * @return true on success
 */
bool orchestrator_define_namespace(orchestrator_t orch, const feature_namespace_t* ns);

/**
 * @brief Get feature namespace by domain
 *
 * @param orch Orchestrator
 * @param domain Domain code
 * @param ns Output: namespace definition
 * @return true if found
 */
bool orchestrator_get_namespace(orchestrator_t orch, uint8_t domain, feature_namespace_t* ns);

//=============================================================================
// LLM Integration
//=============================================================================

/**
 * @brief LLM query type
 */
typedef enum {
    LLM_QUERY_SEMANTIC_GROUNDING, /**< Ground feature codes in semantics */
    LLM_QUERY_TOPOLOGY_ADVICE,    /**< Get topology suggestions */
    LLM_QUERY_POLICY_GENERATION,  /**< Generate ethical policies */
    LLM_QUERY_EXPLANATION         /**< Explain network behavior */
} llm_query_type_t;

/**
 * @brief LLM query result
 */
typedef struct {
    llm_query_type_t query_type; /**< Type of query */
    char response[4096];         /**< LLM response text */
    float confidence;            /**< Confidence in response */
    uint64_t timestamp;          /**< When response was generated */
} llm_result_t;

/**
 * @brief Query LLM for assistance
 *
 * @param orch Orchestrator
 * @param query_type Type of query
 * @param query_data Query-specific data
 * @param result Output: LLM result
 * @return true on success
 */
bool orchestrator_llm_query(orchestrator_t orch, llm_query_type_t query_type,
                            const void* query_data, llm_result_t* result);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Create SET_LEARNING_RATE control message
 *
 * @param policy Learning rate policy
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written, or -1 on error
 */
int orchestrator_create_learning_rate_message(const learning_rate_policy_t* policy, uint8_t* buffer,
                                              uint32_t buffer_size);

/**
 * @brief Create CLUSTER_ANNOUNCE control message
 *
 * @param cluster Cluster info
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written, or -1 on error
 */
int orchestrator_create_cluster_announce(const cluster_info_t* cluster, uint8_t* buffer,
                                         uint32_t buffer_size);

/**
 * @brief Get human-readable name for orchestration mode
 *
 * @param mode Orchestration mode
 * @return Mode name string
 */
const char* orchestrator_mode_name(orchestration_mode_t mode);

#endif  // NIMCP_ORCHESTRATOR_H
