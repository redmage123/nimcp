#ifndef NIMCP_SWARM_RUNTIME_TYPES_H
#define NIMCP_SWARM_RUNTIME_TYPES_H

/**
 * @file nimcp_swarm_runtime_types.h
 * @brief Swarm runtime type definitions — peer management, sync rounds,
 *        master election, and discovery/edge configuration.
 *
 * These types extend the edge subsystem with a runtime orchestrator that
 * manages peer lifecycles, federated sync rounds, Byzantine detection,
 * and master election across a multi-device swarm.
 *
 * Depends on: nimcp_edge_types.h (device profiles, telemetry, federated,
 *             transport, model versioning, message types)
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge_types.h"
#include "utils/thread/nimcp_thread.h"

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Extended Swarm Message Types
 *
 * nimcp_swarm_msg_type_t in nimcp_edge_types.h defines values 0-14.
 * We add DISCOVERY and ELECTION with non-conflicting values.
 * ============================================================================ */

#define NIMCP_SWARM_MSG_DISCOVERY  15  /* Peer discovery announce/response */
#define NIMCP_SWARM_MSG_ELECTION   16  /* Master election ballot/result */

/* ============================================================================
 * Peer State Machine
 * ============================================================================ */

typedef enum {
    NIMCP_PEER_UNKNOWN = 0,     /* Initial / uninitialized */
    NIMCP_PEER_DISCOVERING,     /* Sending/receiving discovery beacons */
    NIMCP_PEER_JOINING,         /* Handshake in progress */
    NIMCP_PEER_ACTIVE,          /* Fully connected, participating */
    NIMCP_PEER_SUSPECTED,       /* Missed heartbeats, may be dead */
    NIMCP_PEER_BYZANTINE,       /* Detected anomalous behavior */
    NIMCP_PEER_LEAVING,         /* Graceful shutdown in progress */
    NIMCP_PEER_DEAD             /* Removed from active roster */
} nimcp_peer_state_t;

/* ============================================================================
 * Peer Entry — single device in the peer registry
 * ============================================================================ */

typedef struct {
    uint32_t                device_id;
    nimcp_peer_state_t      state;
    nimcp_device_profile_t  profile;
    nimcp_model_version_t   version;
    nimcp_device_telemetry_t last_telemetry;

    /* Network address */
    char                    address[256];
    uint16_t                port;
    int                     socket_fd;

    /* Heartbeat tracking */
    uint64_t                last_heartbeat_ts;   /* ms since epoch */
    uint32_t                missed_heartbeats;

    /* Membership */
    uint64_t                join_timestamp;       /* ms since epoch */

    /* Health / Byzantine detection */
    float                   gradient_norm_ema;    /* EMA of submitted gradient norms */
    uint32_t                anomaly_count;        /* Cumulative anomalies detected */

    /* Sync stats */
    uint64_t                total_syncs;          /* Rounds this peer participated in */

    /* Quarantine flag (set when BYZANTINE, cleared on re-join) */
    bool                    quarantined;
} nimcp_peer_entry_t;

/* ============================================================================
 * Peer Registry — thread-safe collection of known peers
 * ============================================================================ */

typedef struct {
    nimcp_peer_entry_t*     peers;
    uint32_t                count;
    uint32_t                capacity;
    nimcp_mutex_t*          lock;    /* Heap-allocated via nimcp_mutex_create */
} nimcp_peer_registry_t;

/* ============================================================================
 * Sync Phase & Sync Round
 * ============================================================================ */

typedef enum {
    NIMCP_SYNC_IDLE = 0,       /* Waiting for next round */
    NIMCP_SYNC_COLLECTING,     /* Gathering gradients from peers */
    NIMCP_SYNC_AGGREGATING,    /* Running aggregation algorithm */
    NIMCP_SYNC_PUSHING,        /* Distributing aggregated weights */
    NIMCP_SYNC_COMPLETE        /* Round finished, results committed */
} nimcp_sync_phase_t;

typedef struct {
    uint64_t                round_id;
    nimcp_sync_phase_t      phase;
    uint64_t                round_start_ts;       /* ms since epoch */
    uint32_t                timeout_ms;

    /* Gradient collection */
    nimcp_federated_gradient_t* gradients;         /* Array, one per contributing peer */
    uint32_t                gradients_received;
    uint32_t                gradients_expected;

    /* Aggregation output */
    float*                  aggregated_gradients;  /* num_params floats */
    uint32_t                num_params;

    /* Snapshot for rollback */
    float*                  pre_round_weights;     /* num_params floats */
} nimcp_sync_round_t;

/* ============================================================================
 * Master Election (Bully Algorithm)
 * ============================================================================ */

typedef struct {
    uint32_t                current_master_id;
    uint32_t                backup_master_id;
    uint64_t                election_epoch;        /* Monotonically increasing */
    bool                    election_in_progress;
    uint32_t                highest_id_seen;       /* For bully algorithm */
} nimcp_master_election_t;

/* ============================================================================
 * Discovery Configuration
 * ============================================================================ */

typedef struct {
    bool                    use_mdns;
    char                    multicast_group[64];
    uint16_t                discovery_port;
    uint32_t                announce_interval_ms;
    uint32_t                discovery_timeout_ms;

    /* Manual peer list (fallback when mDNS is unavailable) */
    char**                  manual_peers;           /* Array of "host:port" strings */
    uint32_t                manual_peer_count;
} nimcp_discovery_config_t;

/* ============================================================================
 * Master Configuration
 * ============================================================================ */

typedef struct {
    uint32_t                device_id;
    uint16_t                listen_port;

    /* Sub-configs */
    nimcp_discovery_config_t discovery;
    nimcp_federated_config_t federated;

    /* Timing */
    uint32_t                sync_interval_ms;       /* ms between sync rounds */
    uint32_t                heartbeat_timeout_ms;    /* Mark SUSPECTED after this */
    uint32_t                dead_timeout_ms;         /* Mark DEAD after this */

    /* Quorum */
    uint32_t                min_devices_for_sync;
    uint32_t                max_devices;

    /* Byzantine detection thresholds */
    float                   byzantine_norm_threshold; /* Gradient norm outlier factor */
    uint32_t                byzantine_anomaly_limit;  /* Max anomalies before quarantine */

    /* Aggregation */
    nimcp_fed_aggregation_t aggregation_method;
} nimcp_master_config_t;

/* ============================================================================
 * Edge Runtime Configuration (per-device)
 * ============================================================================ */

typedef struct {
    uint32_t                device_id;
    nimcp_device_profile_t  profile;

    /* Discovery */
    nimcp_discovery_config_t discovery;

    /* Timing */
    uint32_t                heartbeat_interval_ms;
    uint32_t                reconnect_delay_ms;
    uint32_t                max_reconnect_attempts;

    /* Features */
    bool                    enable_gossip;
    bool                    enable_local_learning;
} nimcp_edge_runtime_config_t;

/* ============================================================================
 * Opaque Runtime Handles (defined in implementation)
 * ============================================================================ */

typedef struct nimcp_swarm_master      nimcp_swarm_master_t;
typedef struct nimcp_swarm_edge_runtime nimcp_swarm_edge_runtime_t;

#endif /* NIMCP_SWARM_RUNTIME_TYPES_H */
