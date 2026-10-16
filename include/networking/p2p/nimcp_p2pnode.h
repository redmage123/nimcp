// nimcp_p2pnode.h

#ifndef NIMCP_P2PNODE_H
#define NIMCP_P2PNODE_H

#include <Python.h>  // Use CMake-provided Python includes
#include "common/nimcp_export.h"
#include "utils/containers/nimcp_graph.h"  // Include NimcpGraph definition

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enumeration of possible node states
 */
typedef enum {
    NODE_STATUS_INIT,    /**< Node is initializing */
    NODE_STATUS_RUNNING, /**< Node is operational */
    NODE_STATUS_ERROR,   /**< Node has encountered an error */
    NODE_STATUS_SHUTDOWN /**< Node is shutting down */
} node_status_t;

/**
 * @brief Structure containing peer connection information
 */
typedef struct {
    char ip[16];        /**< IPv4 address as string */
    uint16_t port;      /**< Port number */
    int socket_fd;      /**< Stores the socket file descriptor */
    bool connected;     /**< Connection status */

    // Heartbeat tracking
    uint64_t last_ping_sent;     /**< Timestamp of last ping sent (microseconds) */
    uint64_t last_pong_received; /**< Timestamp of last pong received (microseconds) */
    uint32_t missed_pings;       /**< Number of consecutive missed pongs */
    bool healthy;                /**< Health status (true if responding to pings) */
} peer_info_t;

/**
 * @brief Configuration structure for node initialization
 */
typedef struct {
    uint16_t listen_port;         /**< Port to listen on */
    uint32_t max_peers;           /**< Maximum number of peer connections */
    uint32_t keepalive_interval;  /**< Keepalive interval */
    uint32_t discovery_interval;  /**< Discovery interval */
    uint32_t reconnect_interval;  /**< Reconnect interval */
    uint32_t max_retries;         /**< Maximum retries */
    uint32_t ping_interval;       /**< Health check interval in milliseconds */
    bool enable_bio_async;        /**< Enable bio-async communication */
} node_config_t;

/**
 * @brief Opaque pointer type for node instance
 */
typedef struct p2p_node_struct* p2p_node_t;

/**
 * @brief Creates a new P2P node instance
 * @param config Pointer to configuration structure
 * @return Handle to the created node, NULL if creation fails
 */
p2p_node_t p2p_node_create(const node_config_t* config);

/**
 * @brief Destroys a P2P node instance and frees resources
 * @param node Handle to the node to destroy
 */
void p2p_node_destroy(p2p_node_t node);

/**
 * @brief Gets current status of the node
 * @param node Handle to the node
 * @return Current node status
 */
node_status_t p2p_node_get_status(p2p_node_t node);

/**
 * @brief Attempts to connect to a peer
 * @param node Handle to the node
 * @param peer_ip IP address of peer
 * @param peer_port Port number of peer
 * @return true if connection initiated successfully, false otherwise
 */
bool p2p_node_connect_peer(p2p_node_t node, const char* peer_ip, uint16_t peer_port);

/**
 * @brief Checks if a peer is currently connected
 * @param node Handle to the node
 * @param peer_ip IP address of peer
 * @param peer_port Port number of peer
 * @return true if peer is connected, false otherwise
 */
bool p2p_node_is_peer_connected(p2p_node_t node, const char* peer_ip, uint16_t peer_port);

/**
 * @brief Disconnects from a peer
 * @param node Handle to the node
 * @param peer_ip IP address of peer
 * @param peer_port Port number of peer
 * @return true if disconnection successful, false otherwise
 */
bool p2p_node_disconnect_peer(p2p_node_t node, const char* peer_ip, uint16_t peer_port);

/**
 * @brief Starts the node's main operation
 * @param node Handle to the node
 * @return true if startup successful, false otherwise
 */
bool p2p_node_start(p2p_node_t node);

/**
 * @brief Stops the node's operation
 * @param node Handle to the node
 * @return true if shutdown successful, false otherwise
 */
bool p2p_node_stop(p2p_node_t node);

/**
 * @brief Get topology graph for network analysis
 *
 * Returns the internal topology graph that tracks peer connections.
 * Useful for introspection, pathfinding, and network analysis.
 *
 * @param node Handle to the node
 * @return Pointer to topology graph, or NULL if node is invalid
 *
 * @note The graph is owned by the node - do not destroy it
 * @note Graph operations are thread-safe (protected by internal mutex)
 * @note Use nimcp_graph.h API functions to query the graph
 */
NimcpGraph* p2p_node_get_topology_graph(p2p_node_t node);

//=============================================================================
// Heartbeat System API
//=============================================================================

/**
 * @brief Send heartbeat pings to all connected peers
 *
 * Sends PING messages to all connected peers and updates last_ping_sent timestamp.
 * Should be called periodically based on node_config_t.ping_interval.
 *
 * @param node Handle to the node
 * @return Number of pings sent, or 0 on error
 *
 * ALGORITHM:
 * - O(n) where n = number of connected peers
 * - Each ping is a lightweight MSG_TYPE_PING message
 * - Non-blocking operation
 */
uint32_t p2p_node_send_heartbeats(p2p_node_t node);

/**
 * @brief Process heartbeat pong response from peer
 *
 * Updates peer health status and resets missed ping counter.
 *
 * @param node Handle to the node
 * @param peer_ip IP address of peer
 * @param peer_port Port of peer
 * @return true if pong processed successfully
 *
 * ALGORITHM:
 * - O(1) hash table lookup
 * - Updates last_pong_received timestamp
 * - Resets missed_pings counter
 * - Marks peer as healthy
 */
bool p2p_node_process_pong(p2p_node_t node, const char* peer_ip, uint16_t peer_port);

/**
 * @brief Check peer health and mark unhealthy peers
 *
 * Checks all peers for missed pongs and marks unhealthy peers.
 * Should be called after expected pong timeout period.
 *
 * @param node Handle to the node
 * @param timeout_ms Maximum time to wait for pong (milliseconds)
 * @return Number of unhealthy peers detected
 *
 * ALGORITHM:
 * - O(n) where n = number of peers
 * - Compares current time to last_pong_received + timeout
 * - Increments missed_pings counter
 * - Marks peer unhealthy after max_retries consecutive misses
 */
uint32_t p2p_node_check_peer_health(p2p_node_t node, uint32_t timeout_ms);

/**
 * @brief Reconnect to unhealthy peers
 *
 * Attempts to reconnect to peers marked as unhealthy.
 *
 * @param node Handle to the node
 * @return Number of reconnection attempts made
 *
 * ALGORITHM:
 * - O(n) where n = number of unhealthy peers
 * - Attempts reconnection to unhealthy peers
 * - Resets health status on successful reconnection
 */
uint32_t p2p_node_reconnect_unhealthy(p2p_node_t node);

/**
 * @brief Get peer health status
 *
 * Returns health status of specific peer.
 *
 * @param node Handle to the node
 * @param peer_ip IP address of peer
 * @param peer_port Port of peer
 * @param out_health Output: peer health status
 * @return true if peer found, false otherwise
 */
bool p2p_node_get_peer_health(p2p_node_t node, const char* peer_ip, uint16_t peer_port,
                                bool* out_health);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_P2PNODE_H
