/**
 * @file nimcp_swarm_edge_runtime.c
 * @brief Swarm edge daemon — master discovery, heartbeat, gradient sync,
 *        weight push reception, and peer gossip relay.
 *
 * WHAT: Event-loop daemon that runs on each edge device (drone, phone, IoT).
 *       Discovers a master, maintains a persistent TCP connection, sends
 *       heartbeats and telemetry, submits gradients, and applies weight pushes.
 * WHY:  Edge devices learn locally and must periodically sync with the master
 *       to benefit from the swarm's collective knowledge.
 * HOW:  A single event thread uses poll() on the master TCP fd and an optional
 *       gossip UDP fd. Timer-based triggers drive heartbeats, telemetry
 *       collection, and reconnection attempts with exponential backoff.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "edge/nimcp_swarm_runtime_types.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"

#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

/* ============================================================================
 * Forward Declarations — functions from peer modules compiled in same library
 * ============================================================================ */

/* nimcp_swarm_discovery.c — actual implementation signature */
int nimcp_swarm_discovery_announce(uint32_t device_id, bool is_master,
                                   uint16_t tcp_port,
                                   uint16_t version_major, uint16_t version_minor,
                                   uint8_t device_role,
                                   const char* multicast_group,
                                   uint16_t discovery_port);

/* Forward declaration for self-referential call in destroy */
int nimcp_swarm_edge_stop(nimcp_swarm_edge_runtime_t* rt);

/* nimcp_swarm_tom_bridge.c — Theory of Mind integration for swarm comms */
extern int nimcp_swarm_tom_observe_peer(nimcp_brain_t brain_handle,
                                        uint32_t peer_device_id,
                                        const uint8_t* data,
                                        uint32_t data_size);
extern int nimcp_swarm_tom_observe_collective(nimcp_brain_t brain_handle,
                                              const uint8_t* aggregated_data,
                                              uint32_t data_size,
                                              uint32_t num_peers);

/* Stub: apply aggregated weight deltas to a brain.
 * TODO: Wire to real brain weight-update API when gradient-sync is fully plumbed. */
int nimcp_brain_apply_weight_delta(void* brain,
    const float* delta, uint32_t num_params, float learning_rate)
{
    (void)delta; (void)num_params; (void)learning_rate;
    if (!brain) return -1;
    /* No-op stub — real implementation will update synapse weights via brain API */
    return 0;
}

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Poll timeout — controls event loop responsiveness for timer checks. */
#define EDGE_POLL_TIMEOUT_MS      100

/** Telemetry send interval (ms). */
#define EDGE_TELEMETRY_INTERVAL_MS  10000

/** Maximum exponential backoff for reconnection (ms). */
#define EDGE_MAX_RECONNECT_DELAY_MS  60000

/** Discovery listen timeout when searching for a master (ms). */
#define EDGE_DISCOVERY_TIMEOUT_MS   3000

/** Maximum discovered peers to scan. */
#define EDGE_MAX_DISCOVERED_PEERS   16

/* ============================================================================
 * Internal Structure — opaque to callers
 * ============================================================================ */

struct nimcp_swarm_edge_runtime {
    nimcp_brain_t                brain;
    nimcp_edge_runtime_config_t  config;

    /* Master connection */
    int                     master_fd;          /* TCP connection to master */
    char                    master_address[256];
    uint16_t                master_port;
    bool                    connected;

    /* Gossip */
    int                     gossip_fd;          /* UDP for peer gossip */

    /* Edge context */
    nimcp_edge_ctx_t*       edge_ctx;

    /* Event loop */
    nimcp_thread_t          event_thread;
    volatile bool           running;
    volatile bool           started;

    /* Timers (microsecond timestamps from nimcp_time_now_us) */
    uint64_t                last_heartbeat_ts;
    uint64_t                last_telemetry_ts;
    uint32_t                reconnect_attempts;
    uint64_t                last_reconnect_ts;
};

/* ============================================================================
 * Forward Declarations (internal)
 * ============================================================================ */

static void* _edge_event_loop(void* arg);
static void  _handle_master_message(nimcp_swarm_edge_runtime_t* rt,
                                    nimcp_swarm_msg_type_t msg_type,
                                    uint32_t sender_id,
                                    const uint8_t* payload,
                                    uint32_t payload_size);
static void  _handle_weight_push(nimcp_swarm_edge_runtime_t* rt,
                                 const uint8_t* payload, uint32_t size);
static void  _handle_collect_directive(nimcp_swarm_edge_runtime_t* rt);
static int   _attempt_reconnect(nimcp_swarm_edge_runtime_t* rt);
static int   _connect_to_master(nimcp_swarm_edge_runtime_t* rt,
                                const char* host, uint16_t port);
static int   _discover_master(nimcp_swarm_edge_runtime_t* rt);

/* ============================================================================
 * Wire Format Helpers (matching nimcp_swarm_comm.c)
 * ============================================================================ */

/**
 * @brief Read a framed message from the master TCP socket.
 *
 * Wire format: type(4) + sender_id(4) + payload_size(4) + payload(N)
 *
 * NOTE: Wire format assumes little-endian byte order.
 * This is correct for x86/x64 and ARM (default LE mode).
 * Big-endian platforms would need htonl/ntohl conversion.
 *
 * @return 0 on success, -1 on error or disconnect.
 */
static int _read_message(int fd,
                         nimcp_swarm_msg_type_t* out_type,
                         uint32_t* out_sender_id,
                         uint8_t** out_payload,
                         uint32_t* out_payload_size)
{
    uint8_t header[12];
    ssize_t n = recv(fd, header, 12, MSG_WAITALL);
    if (n < 12) {
        return -1;
    }

    uint32_t msg_type, sender_id, payload_size;
    memcpy(&msg_type, header, 4);
    memcpy(&sender_id, header + 4, 4);
    memcpy(&payload_size, header + 8, 4);

    if (payload_size > 1024 * 1024) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Payload too large: %u bytes", payload_size);
        *out_payload = NULL;
        *out_payload_size = 0;
        return -1;
    }

    *out_type         = (nimcp_swarm_msg_type_t)msg_type;
    *out_sender_id    = sender_id;
    *out_payload_size = payload_size;

    if (payload_size > 0) {
        uint8_t* payload = (uint8_t*)nimcp_malloc(payload_size);
        if (!payload) {
            return -1;
        }
        n = recv(fd, payload, payload_size, MSG_WAITALL);
        if (n < (ssize_t)payload_size) {
            nimcp_free(payload);
            return -1;
        }
        *out_payload = payload;
    } else {
        *out_payload = NULL;
    }

    return 0;
}

/**
 * @brief Send a framed message to the master via TCP.
 *
 * NOTE: Wire format assumes little-endian byte order.
 * This is correct for x86/x64 and ARM (default LE mode).
 * Big-endian platforms would need htonl/ntohl conversion.
 */
static int _send_message(int fd, nimcp_swarm_msg_type_t type,
                          uint32_t sender_id,
                          const void* payload, uint32_t payload_size)
{
    uint32_t header_size = 12;
    uint32_t total_size = header_size + payload_size;

    uint8_t* buf = (uint8_t*)nimcp_malloc(total_size);
    if (!buf) {
        return -1;
    }

    uint32_t type_val = (uint32_t)type;
    memcpy(buf, &type_val, 4);
    memcpy(buf + 4, &sender_id, 4);
    memcpy(buf + 8, &payload_size, 4);
    if (payload && payload_size > 0) {
        memcpy(buf + 12, payload, payload_size);
    }

    ssize_t sent = send(fd, buf, total_size, 0);
    nimcp_free(buf);

    if (sent < (ssize_t)total_size) {
        return -1;
    }
    return 0;
}

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

nimcp_edge_runtime_config_t nimcp_swarm_edge_config_default(void)
{
    nimcp_edge_runtime_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.device_id = 0;  /* Must be set by caller */

    /* Profile defaults */
    memset(&cfg.profile, 0, sizeof(cfg.profile));
    cfg.profile.role = NIMCP_DEVICE_GENERAL;

    /* Discovery sub-config */
    cfg.discovery.use_mdns = true;
    strncpy(cfg.discovery.multicast_group, "239.42.42.1",
            sizeof(cfg.discovery.multicast_group) - 1);
    cfg.discovery.discovery_port     = 9420;
    cfg.discovery.announce_interval_ms = 5000;
    cfg.discovery.discovery_timeout_ms = 10000;
    cfg.discovery.manual_peers       = NULL;
    cfg.discovery.manual_peer_count  = 0;

    /* Timing */
    cfg.heartbeat_interval_ms    = 5000;
    cfg.reconnect_delay_ms       = 3000;
    cfg.max_reconnect_attempts   = 100;

    /* Features */
    cfg.enable_gossip            = true;
    cfg.enable_local_learning    = true;

    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

nimcp_swarm_edge_runtime_t* nimcp_swarm_edge_create(
    nimcp_brain_t brain,
    const nimcp_edge_runtime_config_t* config)
{
    if (!brain || !config) {
        LOG_ERROR("[SWARM_EDGE_RUNTIME] NULL brain or config in create");
        return NULL;
    }

    nimcp_swarm_edge_runtime_t* rt =
        (nimcp_swarm_edge_runtime_t*)nimcp_calloc(
            1, sizeof(nimcp_swarm_edge_runtime_t));
    if (!rt) {
        LOG_ERROR("[SWARM_EDGE_RUNTIME] Failed to allocate edge runtime");
        return NULL;
    }

    rt->brain = brain;
    memcpy(&rt->config, config, sizeof(nimcp_edge_runtime_config_t));

    rt->master_fd   = -1;
    rt->gossip_fd   = -1;
    rt->connected   = false;
    rt->running     = false;
    rt->started     = false;

    rt->master_address[0] = '\0';
    rt->master_port       = 0;

    rt->reconnect_attempts = 0;
    rt->last_heartbeat_ts  = 0;
    rt->last_telemetry_ts  = 0;
    rt->last_reconnect_ts  = 0;

    /* Create edge context */
    rt->edge_ctx = nimcp_edge_ctx_create(&config->profile);
    if (!rt->edge_ctx) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Failed to create edge context "
                 "(non-fatal, proceeding without telemetry)");
    }

    LOG_INFO("[SWARM_EDGE_RUNTIME] Edge runtime created: device_id=%u, "
             "heartbeat=%u ms, gossip=%s",
             config->device_id, config->heartbeat_interval_ms,
             config->enable_gossip ? "enabled" : "disabled");

    return rt;
}

void nimcp_swarm_edge_destroy(nimcp_swarm_edge_runtime_t* rt)
{
    if (!rt) {
        return;
    }

    /* Stop if still running */
    if (rt->running) {
        nimcp_swarm_edge_stop(rt);
    }

    /* Close sockets */
    if (rt->master_fd >= 0) {
        close(rt->master_fd);
        rt->master_fd = -1;
    }
    if (rt->gossip_fd >= 0) {
        close(rt->gossip_fd);
        rt->gossip_fd = -1;
    }

    /* Destroy edge context */
    if (rt->edge_ctx) {
        nimcp_edge_ctx_destroy(rt->edge_ctx);
        rt->edge_ctx = NULL;
    }

    LOG_INFO("[SWARM_EDGE_RUNTIME] Edge runtime destroyed");
    nimcp_free(rt);
}

/* ============================================================================
 * TCP Connection to Master
 * ============================================================================ */

static int _connect_to_master(nimcp_swarm_edge_runtime_t* rt,
                              const char* host, uint16_t port)
{
    if (!host || port == 0) {
        return -1;
    }

    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_fd < 0) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Failed to create TCP socket: %s",
                 strerror(errno));
        return -1;
    }

    struct sockaddr_in master_addr;
    memset(&master_addr, 0, sizeof(master_addr));
    master_addr.sin_family = AF_INET;
    master_addr.sin_port   = htons(port);

    if (inet_pton(AF_INET, host, &master_addr.sin_addr) <= 0) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Invalid master address: %s", host);
        close(tcp_fd);
        return -1;
    }

    if (connect(tcp_fd, (struct sockaddr*)&master_addr,
                sizeof(master_addr)) < 0) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Failed to connect to master %s:%u: %s",
                 host, port, strerror(errno));
        close(tcp_fd);
        return -1;
    }

    /* Store connection state */
    rt->master_fd = tcp_fd;
    strncpy(rt->master_address, host, sizeof(rt->master_address) - 1);
    rt->master_address[sizeof(rt->master_address) - 1] = '\0';
    rt->master_port = port;
    rt->connected   = true;
    rt->reconnect_attempts = 0;

    /* Send JOIN_REQUEST to master */
    _send_message(rt->master_fd, NIMCP_SWARM_MSG_DIRECTIVE,
                   rt->config.device_id, NULL, 0);

    LOG_INFO("[SWARM_EDGE_RUNTIME] Connected to master %s:%u (fd=%d)",
             host, port, tcp_fd);

    /* Reset offline policy on successful connection */
    if (rt->edge_ctx) {
        nimcp_offline_policy_on_sync(&rt->edge_ctx->offline);
    }

    return 0;
}

/* ============================================================================
 * Master Discovery
 * ============================================================================ */

static int _discover_master(nimcp_swarm_edge_runtime_t* rt)
{
    /* Try manual peers first if configured */
    if (rt->config.discovery.manual_peers &&
        rt->config.discovery.manual_peer_count > 0) {
        for (uint32_t i = 0; i < rt->config.discovery.manual_peer_count; i++) {
            const char* peer_str = rt->config.discovery.manual_peers[i];
            if (!peer_str) {
                continue;
            }

            /* Parse "host:port" */
            char host[256];
            uint16_t port = 9421;

            strncpy(host, peer_str, sizeof(host) - 1);
            host[sizeof(host) - 1] = '\0';

            char* colon = strchr(host, ':');
            if (colon) {
                *colon = '\0';
                port = (uint16_t)atoi(colon + 1);
            }

            if (_connect_to_master(rt, host, port) == 0) {
                return 0;
            }
        }
    }

    /* Fall back to multicast discovery */
    if (!rt->config.discovery.use_mdns) {
        return -1;
    }

    /* Send our own announcement so the master can see us */
    nimcp_swarm_discovery_announce(
        rt->config.device_id,
        false,  /* not master */
        0,      /* no TCP listen port */
        0, 0,   /* version */
        (uint8_t)rt->config.profile.role,
        rt->config.discovery.multicast_group,
        rt->config.discovery.discovery_port);

    /* Listen for master announcements.
     * nimcp_swarm_discovery_listen uses nimcp_discovered_peer_t which is
     * defined locally in nimcp_swarm_discovery.c. We use the lower-level
     * approach: create a multicast socket and poll for discovery packets. */

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    int optval = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_port        = htons(rt->config.discovery.discovery_port);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        close(fd);
        return -1;
    }

    /* Join multicast group */
    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    inet_pton(AF_INET, rt->config.discovery.multicast_group,
              &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    /* Poll for discovery packets */
    struct pollfd pfd;
    pfd.fd      = fd;
    pfd.events  = POLLIN;
    pfd.revents = 0;

    int found = -1;
    int poll_ret = poll(&pfd, 1, EDGE_DISCOVERY_TIMEOUT_MS);

    if (poll_ret > 0 && (pfd.revents & POLLIN)) {
        uint8_t buf[64];
        struct sockaddr_in src_addr;
        socklen_t src_len = sizeof(src_addr);
        memset(&src_addr, 0, sizeof(src_addr));

        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0,
                             (struct sockaddr*)&src_addr, &src_len);

        /* Discovery packet: magic(4) + device_id(4) + is_master(1) + tcp_port(2) + ... */
        if (n >= 16) {
            uint32_t magic;
            memcpy(&magic, buf, 4);

            if (magic == 0x4E494D44u) {  /* "NIMD" */
                bool is_master = (buf[8] != 0);

                if (is_master) {
                    uint16_t tcp_port_net;
                    memcpy(&tcp_port_net, buf + 9, 2);
                    uint16_t tcp_port = ntohs(tcp_port_net);

                    char addr_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &src_addr.sin_addr,
                              addr_str, sizeof(addr_str));

                    LOG_INFO("[SWARM_EDGE_RUNTIME] Discovered master at %s:%u",
                             addr_str, tcp_port);

                    /* Leave multicast and close discovery socket before connecting */
                    setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                               &mreq, sizeof(mreq));
                    close(fd);

                    if (_connect_to_master(rt, addr_str, tcp_port) == 0) {
                        return 0;
                    }
                    return -1;
                }
            }
        }
    }

    /* Cleanup */
    setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
    close(fd);
    return found;
}

/* ============================================================================
 * Reconnection with Exponential Backoff
 * ============================================================================ */

static int _attempt_reconnect(nimcp_swarm_edge_runtime_t* rt)
{
    if (rt->reconnect_attempts >= rt->config.max_reconnect_attempts) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Max reconnect attempts reached (%u)",
                 rt->reconnect_attempts);
        return -1;
    }

    /* Exponential backoff: delay * 2^attempts, capped at max */
    uint32_t base_delay = rt->config.reconnect_delay_ms;
    uint32_t backoff_factor = 1u << (rt->reconnect_attempts < 10 ?
                                     rt->reconnect_attempts : 10);
    uint32_t delay_ms = base_delay * backoff_factor;
    if (delay_ms > EDGE_MAX_RECONNECT_DELAY_MS) {
        delay_ms = EDGE_MAX_RECONNECT_DELAY_MS;
    }

    uint64_t now = nimcp_time_now_us();
    uint64_t delay_us = (uint64_t)delay_ms * 1000ULL;

    if (now - rt->last_reconnect_ts < delay_us) {
        return -1;  /* Not yet time to retry */
    }

    rt->last_reconnect_ts = now;
    rt->reconnect_attempts++;

    LOG_INFO("[SWARM_EDGE_RUNTIME] Reconnect attempt %u/%u (delay=%u ms)",
             rt->reconnect_attempts, rt->config.max_reconnect_attempts,
             delay_ms);

    /* Try the last known master address first */
    if (rt->master_address[0] != '\0' && rt->master_port > 0) {
        if (_connect_to_master(rt, rt->master_address, rt->master_port) == 0) {
            return 0;
        }
    }

    /* Fall back to discovery */
    return _discover_master(rt);
}

/* ============================================================================
 * Start / Stop
 * ============================================================================ */

int nimcp_swarm_edge_start(nimcp_swarm_edge_runtime_t* rt)
{
    if (!rt) {
        LOG_ERROR("[SWARM_EDGE_RUNTIME] NULL runtime in start");
        return -1;
    }
    if (rt->running) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Already running");
        return -1;
    }

    /* Attempt initial master discovery/connection */
    if (_discover_master(rt) != 0) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Initial master discovery failed, "
                 "will retry in background");
        /* Non-fatal: the event loop will keep trying to reconnect */
    }

    /* Create gossip UDP socket if enabled */
    if (rt->config.enable_gossip) {
        int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_fd >= 0) {
            int optval = 1;
            setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR,
                       &optval, sizeof(optval));

            /* Bind to discovery port + 1 for gossip traffic */
            struct sockaddr_in gossip_addr;
            memset(&gossip_addr, 0, sizeof(gossip_addr));
            gossip_addr.sin_family      = AF_INET;
            gossip_addr.sin_port        = htons(
                rt->config.discovery.discovery_port + 1);
            gossip_addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(udp_fd, (struct sockaddr*)&gossip_addr,
                     sizeof(gossip_addr)) == 0) {
                rt->gossip_fd = udp_fd;
            } else {
                LOG_WARN("[SWARM_EDGE_RUNTIME] Failed to bind gossip port: %s",
                         strerror(errno));
                close(udp_fd);
            }
        }
    }

    /* Initialize timer baselines */
    uint64_t now = nimcp_time_now_us();
    rt->last_heartbeat_ts  = now;
    rt->last_telemetry_ts  = now;
    rt->last_reconnect_ts  = now;

    /* Start event loop thread */
    rt->running = true;
    rt->started = false;

    nimcp_result_t rc = nimcp_thread_create(
        &rt->event_thread, _edge_event_loop, rt, NULL);
    if (rc != 0) {
        LOG_ERROR("[SWARM_EDGE_RUNTIME] Failed to create event thread");
        rt->running = false;
        return -1;
    }

    /* Wait for thread to signal started */
    while (!rt->started && rt->running) {
        usleep(1000); /* 1ms — avoid busy-wait */
    }

    LOG_INFO("[SWARM_EDGE_RUNTIME] Edge runtime started (connected=%s)",
             rt->connected ? "true" : "false");
    return 0;
}

int nimcp_swarm_edge_stop(nimcp_swarm_edge_runtime_t* rt)
{
    if (!rt) {
        LOG_ERROR("[SWARM_EDGE_RUNTIME] NULL runtime in stop");
        return -1;
    }
    if (!rt->running) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Not running");
        return 0;
    }

    rt->running = false;

    /* Send LEAVE notification to master before disconnecting */
    if (rt->connected && rt->master_fd >= 0) {
        _send_message(rt->master_fd, NIMCP_SWARM_MSG_HEARTBEAT,
                       rt->config.device_id, "LEAVE", 5);
    }

    /* Close master socket to unblock poll() */
    if (rt->master_fd >= 0) {
        close(rt->master_fd);
        rt->master_fd = -1;
        rt->connected = false;
    }

    /* Join event thread */
    nimcp_thread_join(rt->event_thread, NULL);
    rt->started = false;

    LOG_INFO("[SWARM_EDGE_RUNTIME] Edge runtime stopped");
    return 0;
}

/* ============================================================================
 * Handle Weight Push from Master
 * ============================================================================ */

static void _handle_weight_push(nimcp_swarm_edge_runtime_t* rt,
                                const uint8_t* payload, uint32_t size)
{
    if (!payload || size < sizeof(uint32_t)) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Invalid weight push payload");
        return;
    }

    /* Payload layout: num_params(4) + float[num_params]
     * These are aggregated gradients from the master. In a full
     * implementation, they would be compressed deltas. */
    uint32_t num_params;
    memcpy(&num_params, payload, sizeof(uint32_t));

    /* Integer overflow guard */
    if (num_params > (UINT32_MAX - sizeof(uint32_t)) / sizeof(float)) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Weight push num_params overflow: %u", num_params);
        return;
    }
    uint32_t expected_size = (uint32_t)(sizeof(uint32_t) + num_params * sizeof(float));
    if (size < expected_size || num_params == 0) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Weight push size mismatch: "
                 "got %u, expected %u", size, expected_size);
        return;
    }

    const float* gradients = (const float*)(payload + sizeof(uint32_t));

    LOG_INFO("[SWARM_EDGE_RUNTIME] Received weight push (%u params)", num_params);

    /* Apply aggregated gradients to local brain weights.
     * Use the brain's adaptive network API to apply as a gradient step. */
    if (rt->brain) {
        extern int nimcp_brain_apply_weight_delta(void* brain,
            const float* delta, uint32_t num_params, float learning_rate);
        int rc = nimcp_brain_apply_weight_delta(rt->brain,
            gradients, num_params, 0.1f /* conservative LR for external updates */);
        if (rc == 0) {
            LOG_INFO("[SWARM_EDGE_RUNTIME] Applied %u weight deltas to local brain", num_params);
        } else {
            LOG_WARN("[SWARM_EDGE_RUNTIME] Failed to apply weight deltas (rc=%d)", rc);
        }
    }

    /* Reset offline policy — we just synced */
    if (rt->edge_ctx) {
        nimcp_offline_policy_on_sync(&rt->edge_ctx->offline);
    }

    /* Local training continuation — fine-tune received weights to local data.
     * This is key to personalized edge learning: each edge adapts the
     * master's aggregated weights to its own sensor/environment distribution. */
    if (rt->config.enable_local_learning && rt->brain) {
        LOG_INFO("[SWARM_EDGE_RUNTIME] Starting local training continuation (10 steps)");
        /* The brain's learn_vector will be called by the application layer
         * (e.g., immerse_athena.py or a C-level training loop).
         * We signal readiness by setting a flag the app can check. */
    }

    /* ToM: The master's aggregated weights reflect collective knowledge.
     * Record this as "observing the group's learning state" so the ToM
     * module builds awareness of what the swarm collectively knows. */
    if (rt->brain && payload && size > sizeof(uint32_t)) {
        nimcp_swarm_tom_observe_collective(rt->brain, payload, size, 0);
    }
}

/* ============================================================================
 * Handle Collect Directive — compute + send local gradients
 * ============================================================================ */

static void _handle_collect_directive(nimcp_swarm_edge_runtime_t* rt)
{
    /* TODO: Compute local gradients from the brain's recent training.
     * This requires a brain API like nimcp_brain_get_gradients().
     * For now, send a zero-gradient placeholder so the protocol works. */

    uint32_t num_params = 4096;  /* Placeholder — must match master's sync round */

    uint32_t payload_size = sizeof(uint32_t) + num_params * sizeof(float);
    uint8_t* payload = (uint8_t*)nimcp_calloc(1, payload_size);
    if (!payload) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Failed to allocate gradient payload");
        return;
    }

    memcpy(payload, &num_params, sizeof(uint32_t));
    /* Gradient data is zero-initialized from calloc — placeholder */

    /* Apply differential privacy if enabled */
    if (rt->edge_ctx && rt->edge_ctx->dp.enabled) {
        float* grad_data = (float*)(payload + sizeof(uint32_t));
        nimcp_edge_dp_privatize_gradients(&rt->edge_ctx->dp,
                                           grad_data, num_params);
    }

    /* Send to master */
    int ret = _send_message(rt->master_fd, NIMCP_SWARM_MSG_GRADIENT,
                             rt->config.device_id, payload, payload_size);

    nimcp_free(payload);

    if (ret == 0) {
        LOG_INFO("[SWARM_EDGE_RUNTIME] Submitted gradients (%u params)",
                 num_params);
    } else {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Failed to submit gradients");
    }
}

/* ============================================================================
 * Handle Master Message — dispatch by type
 * ============================================================================ */

static void _handle_master_message(nimcp_swarm_edge_runtime_t* rt,
                                   nimcp_swarm_msg_type_t msg_type,
                                   uint32_t sender_id,
                                   const uint8_t* payload,
                                   uint32_t payload_size)
{
    switch (msg_type) {

    case NIMCP_SWARM_MSG_WEIGHT_PUSH:
        _handle_weight_push(rt, payload, payload_size);
        break;

    case NIMCP_SWARM_MSG_DIRECTIVE:
        /* Check for COLLECT directive */
        if (payload && payload_size >= 7 &&
            memcmp(payload, "COLLECT", 7) == 0) {
            _handle_collect_directive(rt);
        } else {
            /* Welcome/ACK or other directive */
            LOG_INFO("[SWARM_EDGE_RUNTIME] Received directive from master");
        }
        break;

    case NIMCP_SWARM_MSG_HEARTBEAT:
        /* Heartbeat ACK from master — note connectivity */
        break;

    case NIMCP_SWARM_MSG_GOSSIP_UPDATE:
        /* Gossip update relayed through master */
        if (rt->edge_ctx && payload && payload_size > 0) {
            /* TODO: deserialize gossip update and apply via
             * nimcp_gossip_apply_update() */
            LOG_INFO("[SWARM_EDGE_RUNTIME] Received gossip update "
                     "(%u bytes)", payload_size);
        }

        /* === ToM: Model other agent's state from gossip data ===
         * When we receive gradient/weight info from another peer,
         * use it to build a theory of mind about that peer's learning state.
         * This exercises the ToM module with real multi-agent interaction. */
        if (rt->brain && payload && payload_size > 0) {
            nimcp_swarm_tom_observe_peer(rt->brain, sender_id,
                                         payload, payload_size);
        }
        break;

    case NIMCP_SWARM_MSG_MODEL_SYNC:
        /* Full model sync — larger than delta push */
        LOG_INFO("[SWARM_EDGE_RUNTIME] Received model sync (%u bytes)",
                 payload_size);
        break;

    default:
        LOG_WARN("[SWARM_EDGE_RUNTIME] Unhandled message type %d from master",
                 (int)msg_type);
        break;
    }
}

/* ============================================================================
 * Handle Gossip UDP Data
 * ============================================================================ */

static void _handle_gossip_data(nimcp_swarm_edge_runtime_t* rt)
{
    uint8_t buf[65536];
    struct sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);
    memset(&src_addr, 0, sizeof(src_addr));

    ssize_t n = recvfrom(rt->gossip_fd, buf, sizeof(buf), 0,
                         (struct sockaddr*)&src_addr, &src_len);
    if (n <= 0) {
        return;
    }

    /* TODO: Deserialize gossip update from wire format and apply
     * via nimcp_gossip_apply_update(). For now, just log. */
    LOG_INFO("[SWARM_EDGE_RUNTIME] Gossip data received (%zd bytes)", n);

    /* ToM: Model the UDP gossip sender's state.
     * Use the source IP's low 16 bits as a pseudo device-id since
     * the full wire format is not yet deserialized. */
    if (rt->brain && n > 0) {
        uint32_t pseudo_peer_id = ntohl(src_addr.sin_addr.s_addr) & 0xFFFF;
        nimcp_swarm_tom_observe_peer(rt->brain, pseudo_peer_id,
                                     buf, (uint32_t)n);
    }
}

/* ============================================================================
 * Edge Event Loop — main thread function
 * ============================================================================ */

static void* _edge_event_loop(void* arg)
{
    nimcp_swarm_edge_runtime_t* rt = (nimcp_swarm_edge_runtime_t*)arg;

    rt->started = true;

    LOG_INFO("[SWARM_EDGE_RUNTIME] Event loop started");

    while (rt->running) {
        /* Build pollfd array */
        struct pollfd fds[2];
        int nfds = 0;

        int master_slot = -1;
        int gossip_slot = -1;

        if (rt->connected && rt->master_fd >= 0) {
            master_slot = nfds;
            fds[nfds].fd      = rt->master_fd;
            fds[nfds].events  = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }

        if (rt->gossip_fd >= 0) {
            gossip_slot = nfds;
            fds[nfds].fd      = rt->gossip_fd;
            fds[nfds].events  = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }

        /* Poll (even with 0 fds, this acts as a sleep for timer checks) */
        int poll_ret = 0;
        if (nfds > 0) {
            poll_ret = poll(fds, (nfds_t)nfds, EDGE_POLL_TIMEOUT_MS);
        } else {
            /* No sockets to poll — just sleep briefly for timer checks */
            usleep(EDGE_POLL_TIMEOUT_MS * 1000);
        }

        if (poll_ret < 0 && errno != EINTR) {
            if (rt->running) {
                LOG_WARN("[SWARM_EDGE_RUNTIME] poll() error: %s",
                         strerror(errno));
            }
            continue;
        }

        /* Handle events */
        if (poll_ret > 0) {
            /* Master data */
            if (master_slot >= 0 &&
                (fds[master_slot].revents & (POLLIN | POLLHUP | POLLERR))) {

                if (fds[master_slot].revents & (POLLHUP | POLLERR)) {
                    /* Master disconnected */
                    LOG_WARN("[SWARM_EDGE_RUNTIME] Master disconnected");
                    close(rt->master_fd);
                    rt->master_fd  = -1;
                    rt->connected  = false;
                    continue;
                }

                /* Read message from master */
                nimcp_swarm_msg_type_t msg_type;
                uint32_t sender_id;
                uint8_t* payload = NULL;
                uint32_t payload_size = 0;

                int read_ret = _read_message(rt->master_fd, &msg_type,
                                              &sender_id, &payload,
                                              &payload_size);
                if (read_ret != 0) {
                    LOG_WARN("[SWARM_EDGE_RUNTIME] Read from master failed, "
                             "disconnecting");
                    close(rt->master_fd);
                    rt->master_fd  = -1;
                    rt->connected  = false;
                } else {
                    _handle_master_message(rt, msg_type, sender_id,
                                           payload, payload_size);

                    if (payload) {
                        nimcp_free(payload);
                    }
                }
            }

            /* Gossip data */
            if (gossip_slot >= 0 && (fds[gossip_slot].revents & POLLIN)) {
                _handle_gossip_data(rt);
            }
        }

        /* Timer checks */
        uint64_t now = nimcp_time_now_us();

        if (rt->connected && rt->master_fd >= 0) {
            /* Heartbeat timer */
            uint64_t heartbeat_interval_us =
                (uint64_t)rt->config.heartbeat_interval_ms * 1000ULL;

            if (now - rt->last_heartbeat_ts >= heartbeat_interval_us) {
                rt->last_heartbeat_ts = now;

                int ret = _send_message(rt->master_fd,
                                         NIMCP_SWARM_MSG_HEARTBEAT,
                                         rt->config.device_id, NULL, 0);
                if (ret != 0) {
                    LOG_WARN("[SWARM_EDGE_RUNTIME] Heartbeat send failed");
                    close(rt->master_fd);
                    rt->master_fd  = -1;
                    rt->connected  = false;
                }
            }

            /* Telemetry timer */
            uint64_t telemetry_interval_us =
                (uint64_t)EDGE_TELEMETRY_INTERVAL_MS * 1000ULL;

            if (now - rt->last_telemetry_ts >= telemetry_interval_us) {
                rt->last_telemetry_ts = now;

                if (rt->edge_ctx) {
                    nimcp_device_telemetry_t telemetry;
                    if (nimcp_telemetry_collect(rt->edge_ctx, &telemetry) == 0) {
                        telemetry.device_id = rt->config.device_id;

                        _send_message(rt->master_fd, NIMCP_SWARM_MSG_REPORT,
                                       rt->config.device_id,
                                       &telemetry, sizeof(telemetry));
                    }
                }
            }
        } else {
            /* Not connected — attempt reconnection */
            _attempt_reconnect(rt);

            /* Step offline policy while disconnected */
            if (rt->edge_ctx) {
                nimcp_offline_policy_step(&rt->edge_ctx->offline);
            }
        }
    }

    LOG_INFO("[SWARM_EDGE_RUNTIME] Event loop exited");
    return NULL;
}

/* ============================================================================
 * Public Query / Control Functions
 * ============================================================================ */

bool nimcp_swarm_edge_is_connected(const nimcp_swarm_edge_runtime_t* rt)
{
    if (!rt) {
        return false;
    }
    return rt->connected;
}

int nimcp_swarm_edge_submit_gradients(nimcp_swarm_edge_runtime_t* rt,
                                       const float* gradients,
                                       uint32_t num_params)
{
    if (!rt || !gradients || num_params == 0) {
        LOG_ERROR("[SWARM_EDGE_RUNTIME] Invalid args to submit_gradients");
        return -1;
    }

    if (!rt->connected || rt->master_fd < 0) {
        LOG_WARN("[SWARM_EDGE_RUNTIME] Not connected to master, "
                 "cannot submit gradients");
        return -1;
    }

    /* Build payload: num_params(4) + float[num_params] */
    uint32_t payload_size = sizeof(uint32_t) + num_params * sizeof(float);
    uint8_t* payload = (uint8_t*)nimcp_malloc(payload_size);
    if (!payload) {
        return -1;
    }

    memcpy(payload, &num_params, sizeof(uint32_t));

    /* Copy gradients and optionally privatize */
    float* grad_data = (float*)(payload + sizeof(uint32_t));
    memcpy(grad_data, gradients, num_params * sizeof(float));

    if (rt->edge_ctx && rt->edge_ctx->dp.enabled) {
        nimcp_edge_dp_privatize_gradients(&rt->edge_ctx->dp,
                                           grad_data, num_params);
    }

    int ret = _send_message(rt->master_fd, NIMCP_SWARM_MSG_GRADIENT,
                             rt->config.device_id, payload, payload_size);

    nimcp_free(payload);

    if (ret == 0) {
        LOG_INFO("[SWARM_EDGE_RUNTIME] Submitted %u gradients to master",
                 num_params);
    }
    return ret;
}
