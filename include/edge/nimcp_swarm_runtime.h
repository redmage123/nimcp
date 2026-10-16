#ifndef NIMCP_SWARM_RUNTIME_H
#define NIMCP_SWARM_RUNTIME_H

/**
 * @file nimcp_swarm_runtime.h
 * @brief Swarm runtime public API — multi-device orchestration layer.
 *
 * Provides master-side and edge-side runtime management:
 *   - Master: peer registry, sync rounds, Byzantine detection, discovery
 *   - Edge: connection lifecycle, gradient submission, local learning
 *   - Discovery: mDNS/multicast announce and listen
 *   - Byzantine: gradient and telemetry anomaly detection
 *
 * Usage:
 *   Master node:
 *     nimcp_master_config_t cfg = nimcp_swarm_master_config_default();
 *     nimcp_swarm_master_t* master = nimcp_swarm_master_create(brain, &cfg);
 *     nimcp_swarm_master_start(master);
 *     ...
 *     nimcp_swarm_master_stop(master);
 *     nimcp_swarm_master_destroy(master);
 *
 *   Edge node:
 *     nimcp_edge_runtime_config_t cfg = nimcp_swarm_edge_config_default();
 *     nimcp_swarm_edge_runtime_t* rt = nimcp_swarm_edge_create(brain, &cfg);
 *     nimcp_swarm_edge_start(rt);
 *     ...
 *     nimcp_swarm_edge_submit_gradients(rt, grads, num_params);
 *     ...
 *     nimcp_swarm_edge_stop(rt);
 *     nimcp_swarm_edge_destroy(rt);
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_swarm_runtime_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Master Lifecycle
 * ============================================================================ */

/**
 * @brief Create a swarm master runtime.
 * @param brain  Public brain handle (nimcp_brain_t) — the master model
 * @param config Master configuration (copied internally)
 * @return Opaque master handle, or NULL on failure
 */
nimcp_swarm_master_t* nimcp_swarm_master_create(
    nimcp_brain_t brain,
    const nimcp_master_config_t* config);

/**
 * @brief Destroy master runtime and free all resources.
 * Must be stopped first via nimcp_swarm_master_stop().
 */
void nimcp_swarm_master_destroy(nimcp_swarm_master_t* master);

/**
 * @brief Start the master runtime (listener thread, heartbeat monitor, sync loop).
 * @return 0 on success, -1 on failure
 */
int nimcp_swarm_master_start(nimcp_swarm_master_t* master);

/**
 * @brief Stop the master runtime gracefully.
 * Waits for in-flight sync round to complete, then shuts down threads.
 * @return 0 on success, -1 on failure
 */
int nimcp_swarm_master_stop(nimcp_swarm_master_t* master);

/**
 * @brief Remove a peer from the swarm and close its connection.
 * @param device_id Device to remove
 * @return 0 on success, -1 if device not found
 */
int nimcp_swarm_master_kick(nimcp_swarm_master_t* master, uint32_t device_id);

/**
 * @brief Trigger an immediate sync round, bypassing the sync interval timer.
 * No-op if a sync round is already in progress.
 * @return 0 on success, -1 on failure
 */
int nimcp_swarm_master_force_sync(nimcp_swarm_master_t* master);

/**
 * @brief Get the number of currently active peers.
 * Thread-safe.
 */
uint32_t nimcp_swarm_master_get_peer_count(const nimcp_swarm_master_t* master);

/**
 * @brief Copy peer info for a specific device.
 * @param device_id  Device to query
 * @param entry_out  Output: peer entry (caller-owned)
 * @return 0 on success, -1 if device not found
 */
int nimcp_swarm_master_get_peer_info(
    const nimcp_swarm_master_t* master,
    uint32_t device_id,
    nimcp_peer_entry_t* entry_out);

/**
 * @brief Get default master configuration with sensible defaults.
 */
nimcp_master_config_t nimcp_swarm_master_config_default(void);

/* ============================================================================
 * Edge Runtime Lifecycle
 * ============================================================================ */

/**
 * @brief Create an edge runtime instance.
 * @param brain  Public brain handle (nimcp_brain_t) — the edge model
 * @param config Edge runtime configuration (copied internally)
 * @return Opaque edge runtime handle, or NULL on failure
 */
nimcp_swarm_edge_runtime_t* nimcp_swarm_edge_create(
    nimcp_brain_t brain,
    const nimcp_edge_runtime_config_t* config);

/**
 * @brief Destroy edge runtime and free all resources.
 * Must be stopped first via nimcp_swarm_edge_stop().
 */
void nimcp_swarm_edge_destroy(nimcp_swarm_edge_runtime_t* rt);

/**
 * @brief Start the edge runtime (discovery, heartbeat sender, master connection).
 * @return 0 on success, -1 on failure
 */
int nimcp_swarm_edge_start(nimcp_swarm_edge_runtime_t* rt);

/**
 * @brief Stop the edge runtime gracefully.
 * Sends LEAVING notification to master, then shuts down threads.
 * @return 0 on success, -1 on failure
 */
int nimcp_swarm_edge_stop(nimcp_swarm_edge_runtime_t* rt);

/**
 * @brief Check if the edge runtime is connected to a master.
 * Thread-safe.
 */
bool nimcp_swarm_edge_is_connected(const nimcp_swarm_edge_runtime_t* rt);

/**
 * @brief Submit local gradients to the master for aggregation.
 * Gradients are queued and sent on the next sync round.
 * @param gradients  Array of num_params floats
 * @param num_params Number of parameters
 * @return 0 on success, -1 on failure (not connected, queue full)
 */
int nimcp_swarm_edge_submit_gradients(
    nimcp_swarm_edge_runtime_t* rt,
    const float* gradients,
    uint32_t num_params);

/**
 * @brief Get default edge runtime configuration with sensible defaults.
 */
nimcp_edge_runtime_config_t nimcp_swarm_edge_config_default(void);

/* ============================================================================
 * Discovery
 * ============================================================================ */

/**
 * @brief Send a discovery announcement on the multicast group.
 * @param transport Active swarm transport
 * @param profile   Device profile to advertise
 * @return 0 on success, -1 on failure
 */
int nimcp_swarm_discovery_announce(
    nimcp_swarm_transport_t* transport,
    const nimcp_device_profile_t* profile);

/**
 * @brief Listen for discovery announcements.
 * Blocks up to timeout_ms. Fills discovered_out with found peers.
 * @param transport       Active swarm transport
 * @param discovered_out  Output array of peer entries (caller-allocated)
 * @param max_peers       Capacity of discovered_out
 * @param found_count     Output: number of peers discovered
 * @param timeout_ms      Max time to listen (0 = non-blocking poll)
 * @return 0 on success, -1 on failure
 */
int nimcp_swarm_discovery_listen(
    nimcp_swarm_transport_t* transport,
    nimcp_peer_entry_t* discovered_out,
    uint32_t max_peers,
    uint32_t* found_count,
    uint32_t timeout_ms);

/* ============================================================================
 * Byzantine Detection
 * ============================================================================ */

/**
 * @brief Check a peer's submitted gradient for anomalies.
 * Compares gradient L2 norm against the swarm EMA. Flags outliers
 * beyond byzantine_norm_threshold standard deviations.
 * @param gradient    Submitted gradient array
 * @param num_params  Number of parameters
 * @param swarm_norm_ema  Current swarm-wide gradient norm EMA
 * @param threshold   Outlier factor (e.g., 3.0 = 3x the EMA)
 * @return true if gradient is anomalous (potential Byzantine)
 */
bool nimcp_swarm_byzantine_check_gradient(
    const float* gradient,
    uint32_t num_params,
    float swarm_norm_ema,
    float threshold);

/**
 * @brief Check a peer's telemetry for anomalous patterns.
 * Detects impossible resource usage, sudden accuracy drops, or
 * suspiciously perfect metrics.
 * @param telemetry  Device telemetry to analyze
 * @return true if telemetry appears anomalous
 */
bool nimcp_swarm_byzantine_check_telemetry(
    const nimcp_device_telemetry_t* telemetry);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_RUNTIME_H */
