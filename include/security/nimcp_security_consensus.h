/**
 * @file nimcp_security_consensus.h
 * @brief NIMCP Distributed Security Consensus
 *
 * WHAT: Multi-node security coordination using Raft-like consensus
 * WHY: Distributed systems need coordinated security decisions
 * HOW: Leader election, log replication, threat sharing
 *
 * Features:
 * - Raft-like consensus protocol
 * - Leader election for security coordinator
 * - Security policy replication across nodes
 * - Distributed threat information sharing
 * - Coordinated response to security events
 * - Split-brain protection
 * - Node authentication
 * - Bio-async integration for messaging
 * - Comprehensive logging
 */

#ifndef NIMCP_SECURITY_CONSENSUS_H
#define NIMCP_SECURITY_CONSENSUS_H

#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic number for consensus validation */
#define NIMCP_CONSENSUS_MAGIC 0x434F4E53  /* "CONS" */

/* Node ID type */
typedef uint64_t nimcp_node_id_t;

/* Log index type */
typedef uint64_t nimcp_log_index_t;

/* Term type (monotonically increasing) */
typedef uint64_t nimcp_term_t;

/**
 * Consensus role
 *
 * WHAT: Role of node in consensus cluster
 * WHY: Different roles have different responsibilities
 * HOW: State machine with transitions
 */
typedef enum {
    NIMCP_CONSENSUS_FOLLOWER,    /**< Follows leader */
    NIMCP_CONSENSUS_CANDIDATE,   /**< Requesting votes */
    NIMCP_CONSENSUS_LEADER       /**< Coordinates cluster */
} nimcp_consensus_role_t;

/**
 * Log entry type
 *
 * WHAT: Type of replicated log entry
 * WHY: Different entries trigger different actions
 * HOW: Tagged union pattern
 */
typedef enum {
    NIMCP_LOG_ENTRY_POLICY,      /**< Security policy update */
    NIMCP_LOG_ENTRY_THREAT,      /**< Threat information */
    NIMCP_LOG_ENTRY_RESPONSE,    /**< Coordinated response */
    NIMCP_LOG_ENTRY_CONFIG       /**< Configuration change */
} nimcp_log_entry_type_t;

/**
 * Security policy
 *
 * WHAT: Replicated security policy
 * WHY: All nodes must enforce same policies
 * HOW: Policy ID, rules, timestamps
 */
typedef struct {
    uint64_t policy_id;                    /**< Unique policy identifier */
    char name[128];                        /**< Policy name */
    char rules[1024];                      /**< Policy rules (JSON or config) */
    time_t created_at;                     /**< Creation timestamp */
    nimcp_node_id_t author;                /**< Node that proposed */
} nimcp_security_policy_t;

/**
 * Threat information
 *
 * WHAT: Security threat detected by a node
 * WHY: Share threats across cluster
 * HOW: Threat type, severity, evidence
 */
typedef struct {
    uint64_t threat_id;                    /**< Unique threat identifier */
    char type[64];                         /**< Threat type */
    uint32_t severity;                     /**< Severity level 0-100 */
    char source[256];                      /**< Threat source */
    char description[512];                 /**< Description */
    time_t detected_at;                    /**< Detection timestamp */
    nimcp_node_id_t detector;              /**< Node that detected */
} nimcp_threat_info_t;

/**
 * Response type
 *
 * WHAT: Type of coordinated response
 * WHY: Different threats require different responses
 * HOW: Enum of response types
 */
typedef enum {
    NIMCP_RESPONSE_BLOCK_IP,               /**< Block IP address */
    NIMCP_RESPONSE_RATE_LIMIT,             /**< Apply rate limiting */
    NIMCP_RESPONSE_ELEVATE_SECURITY,       /**< Raise security level */
    NIMCP_RESPONSE_ISOLATE_NODE,           /**< Isolate compromised node */
    NIMCP_RESPONSE_SHUTDOWN                /**< Emergency shutdown */
} nimcp_response_type_t;

/**
 * Consensus configuration
 *
 * WHAT: Configuration for consensus instance
 * WHY: Customizable behavior
 * HOW: Timeouts, limits, networking
 */
typedef struct {
    nimcp_node_id_t node_id;               /**< This node's ID */
    const char* bind_address;              /**< Bind address for cluster */
    uint16_t port;                         /**< Cluster communication port */
    uint32_t election_timeout_min_ms;      /**< Min election timeout */
    uint32_t election_timeout_max_ms;      /**< Max election timeout */
    uint32_t heartbeat_interval_ms;        /**< Leader heartbeat interval */
    size_t max_nodes;                      /**< Maximum cluster nodes */
    size_t max_log_entries;                /**< Maximum log size */
    bio_router_t* router;                  /**< Bio-async router */
    void* user_data;                       /**< User context */
} nimcp_consensus_config_t;

/**
 * Consensus statistics
 *
 * WHAT: Runtime statistics
 * WHY: Monitoring and diagnostics
 * HOW: Counters for events
 */
typedef struct {
    nimcp_consensus_role_t current_role;   /**< Current role */
    nimcp_term_t current_term;             /**< Current term */
    nimcp_node_id_t voted_for;             /**< Vote in current term */
    nimcp_node_id_t current_leader;        /**< Current leader ID */
    nimcp_log_index_t commit_index;        /**< Highest committed index */
    nimcp_log_index_t last_applied;        /**< Highest applied index */
    size_t log_size;                       /**< Number of log entries */
    size_t cluster_size;                   /**< Number of nodes */
    uint64_t elections_started;            /**< Elections initiated */
    uint64_t elections_won;                /**< Elections won */
    uint64_t votes_granted;                /**< Votes granted to others */
    uint64_t heartbeats_sent;              /**< Heartbeats sent */
    uint64_t heartbeats_received;          /**< Heartbeats received */
    uint64_t policies_proposed;            /**< Policies proposed */
    uint64_t policies_committed;           /**< Policies committed */
    uint64_t threats_shared;               /**< Threats shared */
    uint64_t responses_coordinated;        /**< Responses coordinated */
    time_t last_heartbeat_time;            /**< Last heartbeat received */
} nimcp_consensus_stats_t;

/**
 * Node information
 *
 * WHAT: Information about cluster node
 * WHY: Track cluster membership
 * HOW: Node ID, address, status
 */
typedef struct {
    nimcp_node_id_t node_id;               /**< Node identifier */
    char address[256];                     /**< Node address */
    uint16_t port;                         /**< Node port */
    bool is_alive;                         /**< Is node responding */
    time_t last_contact;                   /**< Last contact time */
    nimcp_log_index_t next_index;          /**< Next log index to send */
    nimcp_log_index_t match_index;         /**< Highest replicated index */
} nimcp_node_info_t;

/* Opaque consensus handle */
typedef struct nimcp_security_consensus* nimcp_security_consensus_t;

/**
 * Create consensus instance
 *
 * WHAT: Allocates and initializes consensus
 * WHY: Participate in security cluster
 * HOW: Allocates state, starts as follower, begins election timer
 *
 * @param config Configuration
 * @return Consensus handle or NULL on failure
 */
nimcp_security_consensus_t nimcp_consensus_create(const nimcp_consensus_config_t* config);

/**
 * Destroy consensus instance
 *
 * WHAT: Frees consensus resources
 * WHY: Clean shutdown
 * HOW: Stops timers, leaves cluster, frees memory
 *
 * @param consensus Consensus instance
 */
void nimcp_consensus_destroy(nimcp_security_consensus_t consensus);

/**
 * Join cluster
 *
 * WHAT: Adds node to existing cluster
 * WHY: Expand cluster or rejoin after restart
 * HOW: Contacts peer, receives cluster state, begins participation
 *
 * @param consensus Consensus instance
 * @param peer_address Address of existing cluster member
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_consensus_join(
    nimcp_security_consensus_t consensus,
    const char* peer_address
);

/**
 * Leave cluster
 *
 * WHAT: Gracefully leaves cluster
 * WHY: Planned shutdown or maintenance
 * HOW: Notifies leader, waits for config change, disconnects
 *
 * @param consensus Consensus instance
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_consensus_leave(nimcp_security_consensus_t consensus);

/**
 * Propose security policy
 *
 * WHAT: Proposes new security policy to cluster
 * WHY: Enforce policy across all nodes
 * HOW: Sends to leader, leader replicates, commits when majority ack
 *
 * @param consensus Consensus instance
 * @param policy Policy to propose
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_consensus_propose_policy(
    nimcp_security_consensus_t consensus,
    const nimcp_security_policy_t* policy
);

/**
 * Share threat information
 *
 * WHAT: Shares detected threat with cluster
 * WHY: Coordinated threat response
 * HOW: Replicates threat info via consensus
 *
 * @param consensus Consensus instance
 * @param threat Threat information
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_consensus_share_threat(
    nimcp_security_consensus_t consensus,
    const nimcp_threat_info_t* threat
);

/**
 * Initiate coordinated response
 *
 * WHAT: Proposes coordinated response to security event
 * WHY: All nodes must respond together
 * HOW: Replicates response command via consensus
 *
 * @param consensus Consensus instance
 * @param response Response type
 * @param params Response parameters (type-specific)
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_consensus_initiate_response(
    nimcp_security_consensus_t consensus,
    nimcp_response_type_t response,
    const void* params
);

/**
 * Get current role
 *
 * WHAT: Returns node's current consensus role
 * WHY: Determine if node is leader/follower/candidate
 * HOW: Returns role from state
 *
 * @param consensus Consensus instance
 * @return Current role
 */
nimcp_consensus_role_t nimcp_consensus_get_role(nimcp_security_consensus_t consensus);

/**
 * Get current leader
 *
 * WHAT: Returns ID of current leader
 * WHY: Direct requests to leader
 * HOW: Returns leader ID from state
 *
 * @param consensus Consensus instance
 * @return Leader node ID (0 if no leader)
 */
nimcp_node_id_t nimcp_consensus_get_leader(nimcp_security_consensus_t consensus);

/**
 * Get statistics
 *
 * WHAT: Retrieves consensus statistics
 * WHY: Monitoring and diagnostics
 * HOW: Copies stats structure
 *
 * @param consensus Consensus instance
 * @param stats Output statistics
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_consensus_get_stats(
    nimcp_security_consensus_t consensus,
    nimcp_consensus_stats_t* stats
);

/**
 * Get cluster nodes
 *
 * WHAT: Retrieves information about cluster nodes
 * WHY: Monitor cluster health
 * HOW: Copies node info array
 *
 * @param consensus Consensus instance
 * @param nodes Output node array
 * @param max_nodes Maximum nodes to retrieve
 * @param count_out Number of nodes returned
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_consensus_get_nodes(
    nimcp_security_consensus_t consensus,
    nimcp_node_info_t* nodes,
    size_t max_nodes,
    size_t* count_out
);

/**
 * Process pending messages
 *
 * WHAT: Processes received consensus messages
 * WHY: React to cluster events
 * HOW: Drains inbox, handles RPCs
 *
 * @param consensus Consensus instance
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_consensus_process(nimcp_security_consensus_t consensus);

/**
 * Get role name
 *
 * WHAT: Converts role enum to string
 * WHY: Logging and display
 * HOW: Static string table
 *
 * @param role Consensus role
 * @return String name of role
 */
const char* nimcp_consensus_role_name(nimcp_consensus_role_t role);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_CONSENSUS_H */
