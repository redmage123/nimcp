/**
 * @file nimcp_swarm_master_runtime.c
 * @brief Swarm master daemon — TCP listener, sync coordinator, health monitor.
 *
 * WHAT: Event-loop daemon that accepts edge device connections, coordinates
 *       federated sync rounds, pushes delta weights, and monitors peer health.
 * WHY:  The master holds the full brain; edge devices learn locally and
 *       periodically synchronize via the master's aggregation pipeline.
 * HOW:  A single event thread uses poll() over the TCP listen socket, all
 *       connected peer sockets, and a UDP discovery socket. Timer-based
 *       triggers drive sync rounds, heartbeat sweeps, and discovery announces.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "edge/nimcp_swarm_runtime_types.h"

/* Forward declaration to avoid including full nimcp_swarm_runtime.h
 * (which conflicts with local nimcp_swarm_discovery_announce variant) */
bool nimcp_swarm_byzantine_check_telemetry(
    const nimcp_device_telemetry_t* telemetry);
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>

/* ============================================================================
 * Forward Declarations — functions from peer modules compiled in same library
 *
 * These are defined in nimcp_swarm_lifecycle.c, nimcp_swarm_sync.c,
 * nimcp_swarm_discovery.c, and nimcp_swarm_byzantine.c but not yet
 * declared in a shared header.
 * ============================================================================ */

/* nimcp_swarm_lifecycle.c */
nimcp_peer_registry_t*  nimcp_peer_registry_create(uint32_t capacity);
void                    nimcp_peer_registry_destroy(nimcp_peer_registry_t* registry);
int                     nimcp_peer_registry_add(nimcp_peer_registry_t* registry,
                            uint32_t device_id, const char* address,
                            uint16_t port, const nimcp_device_profile_t* profile);
int                     nimcp_peer_registry_remove(nimcp_peer_registry_t* registry,
                            uint32_t device_id);
nimcp_peer_entry_t*     nimcp_peer_registry_find(nimcp_peer_registry_t* registry,
                            uint32_t device_id);
int                     nimcp_peer_registry_update_heartbeat(
                            nimcp_peer_registry_t* registry, uint32_t device_id);
int                     nimcp_peer_registry_set_state(
                            nimcp_peer_registry_t* registry,
                            uint32_t device_id, nimcp_peer_state_t new_state);
uint32_t                nimcp_peer_registry_sweep(nimcp_peer_registry_t* registry,
                            uint32_t heartbeat_timeout_ms,
                            uint32_t dead_timeout_ms);
uint32_t                nimcp_peer_registry_get_active_count(
                            nimcp_peer_registry_t* registry);
uint32_t                nimcp_peer_registry_get_active_peers(
                            nimcp_peer_registry_t* registry,
                            uint32_t* out_ids, uint32_t max_count);

/* nimcp_swarm_sync.c */
nimcp_sync_round_t*     nimcp_sync_round_create(uint32_t max_peers,
                            uint32_t num_params);
void                    nimcp_sync_round_destroy(nimcp_sync_round_t* round);
int                     nimcp_sync_round_begin(nimcp_sync_round_t* round,
                            uint64_t round_id, uint32_t expected_peers,
                            uint32_t timeout_ms);
int                     nimcp_sync_round_submit_gradient(nimcp_sync_round_t* round,
                            uint32_t device_id, const float* gradients,
                            uint32_t num_params);
bool                    nimcp_sync_round_is_ready(const nimcp_sync_round_t* round);
int                     nimcp_sync_round_aggregate(nimcp_sync_round_t* round,
                            const nimcp_federated_config_t* config);
void                    nimcp_sync_round_reset(nimcp_sync_round_t* round);
nimcp_sync_phase_t      nimcp_sync_round_get_phase(const nimcp_sync_round_t* round);

/* nimcp_swarm_discovery.c — actual implementation signature */
int nimcp_swarm_discovery_announce(uint32_t device_id, bool is_master,
                                   uint16_t tcp_port,
                                   uint16_t version_major, uint16_t version_minor,
                                   uint8_t device_role,
                                   const char* multicast_group,
                                   uint16_t discovery_port);

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of file descriptors to poll (listen + discovery + peers). */
#define MASTER_MAX_POLLFDS       66

/** Poll timeout — controls event loop responsiveness for timer checks. */
#define MASTER_POLL_TIMEOUT_MS   100

/** Default sync round collection timeout (how long to wait for gradients). */
#define SYNC_COLLECTION_TIMEOUT_MS  10000

/** Default number of model parameters (placeholder until brain API exposes this). */
#define DEFAULT_NUM_PARAMS       4096

/** Discovery announce interval for the master (ms). */
#define MASTER_ANNOUNCE_INTERVAL_MS  5000

/** Heartbeat sweep interval (ms). */
#define MASTER_SWEEP_INTERVAL_MS     5000

/* ============================================================================
 * Internal Structure — opaque to callers
 * ============================================================================ */

struct nimcp_swarm_master {
    nimcp_brain_t           brain;            /* Full brain handle */
    nimcp_master_config_t   config;

    /* Networking */
    int                     listen_fd;        /* TCP listen socket */
    int                     discovery_fd;     /* UDP multicast socket */

    /* State */
    nimcp_peer_registry_t*  registry;
    nimcp_sync_round_t*     current_round;
    nimcp_master_election_t election;

    /* Event loop */
    nimcp_thread_t          event_thread;
    volatile bool           running;
    volatile bool           started;

    /* Timers (microsecond timestamps from nimcp_time_now_us) */
    uint64_t                last_sync_ts;
    uint64_t                last_sweep_ts;
    uint64_t                last_announce_ts;

    /* Sync round counter */
    uint64_t                next_round_id;
};

/* ============================================================================
 * Forward Declarations (internal)
 * ============================================================================ */

static void* _master_event_loop(void* arg);
static void  _handle_peer_message(nimcp_swarm_master_t* master,
                                  nimcp_peer_entry_t* peer,
                                  nimcp_swarm_msg_type_t msg_type,
                                  const uint8_t* payload,
                                  uint32_t payload_size);
static void  _trigger_sync_round(nimcp_swarm_master_t* master);
static void  _complete_sync_round(nimcp_swarm_master_t* master);
static void  _accept_new_connection(nimcp_swarm_master_t* master);
static void  _handle_discovery(nimcp_swarm_master_t* master);

/* Forward declaration for self-referential call in destroy */
int nimcp_swarm_master_stop(nimcp_swarm_master_t* master);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

nimcp_master_config_t nimcp_swarm_master_config_default(void)
{
    nimcp_master_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.device_id              = 1;
    cfg.listen_port            = 9421;

    /* Discovery sub-config */
    cfg.discovery.use_mdns           = true;
    strncpy(cfg.discovery.multicast_group, "239.42.42.1",
            sizeof(cfg.discovery.multicast_group) - 1);
    cfg.discovery.discovery_port     = 9420;
    cfg.discovery.announce_interval_ms = MASTER_ANNOUNCE_INTERVAL_MS;
    cfg.discovery.discovery_timeout_ms = 10000;
    cfg.discovery.manual_peers       = NULL;
    cfg.discovery.manual_peer_count  = 0;

    /* Federated sub-config */
    cfg.federated.aggregation       = NIMCP_FED_AVG;
    cfg.federated.blend_ratio       = 0.7f;
    cfg.federated.sync_interval_steps = 100;
    cfg.federated.min_devices_for_agg = 1;
    cfg.federated.enable_ewc        = false;

    /* Timing */
    cfg.sync_interval_ms          = 30000;
    cfg.heartbeat_timeout_ms      = 15000;
    cfg.dead_timeout_ms           = 30000;

    /* Quorum */
    cfg.min_devices_for_sync      = 1;
    cfg.max_devices               = 64;

    /* Byzantine */
    cfg.byzantine_norm_threshold  = 3.0f;
    cfg.byzantine_anomaly_limit   = 5;

    /* Aggregation */
    cfg.aggregation_method        = NIMCP_FED_AVG;

    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

nimcp_swarm_master_t* nimcp_swarm_master_create(
    nimcp_brain_t brain,
    const nimcp_master_config_t* config)
{
    if (!brain || !config) {
        LOG_ERROR("[SWARM_MASTER] NULL brain or config in create");
        return NULL;
    }

    nimcp_swarm_master_t* master =
        (nimcp_swarm_master_t*)nimcp_calloc(1, sizeof(nimcp_swarm_master_t));
    if (!master) {
        LOG_ERROR("[SWARM_MASTER] Failed to allocate master runtime");
        return NULL;
    }

    master->brain = brain;
    memcpy(&master->config, config, sizeof(nimcp_master_config_t));

    master->listen_fd    = -1;
    master->discovery_fd = -1;
    master->running      = false;
    master->started      = false;
    master->next_round_id = 1;

    /* Validate max_devices against poll limit */
    if (master->config.max_devices > 64) {
        LOG_WARN("[SWARM_MASTER] max_devices %u exceeds poll limit 64, clamping",
                 master->config.max_devices);
        master->config.max_devices = 64;
    }

    /* Create peer registry */
    master->registry = nimcp_peer_registry_create(master->config.max_devices);
    if (!master->registry) {
        LOG_ERROR("[SWARM_MASTER] Failed to create peer registry");
        nimcp_free(master);
        return NULL;
    }

    /* Create sync round coordinator */
    master->current_round = nimcp_sync_round_create(
        config->max_devices, DEFAULT_NUM_PARAMS);
    if (!master->current_round) {
        LOG_ERROR("[SWARM_MASTER] Failed to create sync round");
        nimcp_peer_registry_destroy(master->registry);
        nimcp_free(master);
        return NULL;
    }

    /* Initialize election state */
    memset(&master->election, 0, sizeof(master->election));
    master->election.current_master_id = config->device_id;

    /* Initialize timers */
    master->last_sync_ts     = 0;
    master->last_sweep_ts    = 0;
    master->last_announce_ts = 0;

    LOG_INFO("[SWARM_MASTER] Master runtime created: device_id=%u, "
             "listen_port=%u, max_devices=%u, sync_interval=%u ms",
             config->device_id, config->listen_port,
             config->max_devices, config->sync_interval_ms);

    return master;
}

void nimcp_swarm_master_destroy(nimcp_swarm_master_t* master)
{
    if (!master) {
        return;
    }

    /* Stop if still running */
    if (master->running) {
        nimcp_swarm_master_stop(master);
    }

    /* Close sockets */
    if (master->listen_fd >= 0) {
        close(master->listen_fd);
        master->listen_fd = -1;
    }
    if (master->discovery_fd >= 0) {
        close(master->discovery_fd);
        master->discovery_fd = -1;
    }

    /* Destroy subsystems */
    if (master->registry) {
        nimcp_peer_registry_destroy(master->registry);
        master->registry = NULL;
    }
    if (master->current_round) {
        nimcp_sync_round_destroy(master->current_round);
        master->current_round = NULL;
    }

    LOG_INFO("[SWARM_MASTER] Master runtime destroyed");
    nimcp_free(master);
}

/* ============================================================================
 * Start / Stop
 * ============================================================================ */

int nimcp_swarm_master_start(nimcp_swarm_master_t* master)
{
    if (!master) {
        LOG_ERROR("[SWARM_MASTER] NULL master in start");
        return -1;
    }
    if (master->running) {
        LOG_WARN("[SWARM_MASTER] Already running");
        return -1;
    }

    /* Create TCP listen socket */
    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_fd < 0) {
        LOG_ERROR("[SWARM_MASTER] Failed to create TCP socket: %s",
                  strerror(errno));
        return -1;
    }

    int optval = 1;
    setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_port        = htons(master->config.listen_port);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(tcp_fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        LOG_ERROR("[SWARM_MASTER] Failed to bind TCP port %u: %s",
                  master->config.listen_port, strerror(errno));
        close(tcp_fd);
        return -1;
    }

    if (listen(tcp_fd, 16) < 0) {
        LOG_ERROR("[SWARM_MASTER] Failed to listen: %s", strerror(errno));
        close(tcp_fd);
        return -1;
    }

    master->listen_fd = tcp_fd;

    /* Create UDP multicast socket for discovery */
    if (master->config.discovery.use_mdns) {
        int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_fd >= 0) {
            int reuseaddr = 1;
            setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR,
                       &reuseaddr, sizeof(reuseaddr));

            struct sockaddr_in disc_addr;
            memset(&disc_addr, 0, sizeof(disc_addr));
            disc_addr.sin_family      = AF_INET;
            disc_addr.sin_port        = htons(master->config.discovery.discovery_port);
            disc_addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(udp_fd, (struct sockaddr*)&disc_addr, sizeof(disc_addr)) == 0) {
                /* Join multicast group */
                struct ip_mreq mreq;
                memset(&mreq, 0, sizeof(mreq));
                inet_pton(AF_INET, master->config.discovery.multicast_group,
                          &mreq.imr_multiaddr);
                mreq.imr_interface.s_addr = INADDR_ANY;
                setsockopt(udp_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                           &mreq, sizeof(mreq));

                master->discovery_fd = udp_fd;
            } else {
                LOG_WARN("[SWARM_MASTER] Failed to bind discovery port %u: %s",
                         master->config.discovery.discovery_port, strerror(errno));
                close(udp_fd);
            }
        } else {
            LOG_WARN("[SWARM_MASTER] Failed to create discovery socket: %s",
                     strerror(errno));
        }
    }

    /* Initialize timer baselines */
    uint64_t now = nimcp_time_now_us();
    master->last_sync_ts     = now;
    master->last_sweep_ts    = now;
    master->last_announce_ts = now;

    /* Start event loop thread */
    master->running = true;
    master->started = false;

    nimcp_result_t rc = nimcp_thread_create(
        &master->event_thread, _master_event_loop, master, NULL);
    if (rc != 0) {
        LOG_ERROR("[SWARM_MASTER] Failed to create event thread");
        master->running = false;
        close(master->listen_fd);
        master->listen_fd = -1;
        if (master->discovery_fd >= 0) {
            close(master->discovery_fd);
            master->discovery_fd = -1;
        }
        return -1;
    }

    /* Wait for thread to signal started */
    while (!master->started && master->running) {
        usleep(1000); /* 1ms — avoid busy-wait */
    }

    LOG_INFO("[SWARM_MASTER] Master started: listening on port %u",
             master->config.listen_port);
    return 0;
}

int nimcp_swarm_master_stop(nimcp_swarm_master_t* master)
{
    if (!master) {
        LOG_ERROR("[SWARM_MASTER] NULL master in stop");
        return -1;
    }
    if (!master->running) {
        LOG_WARN("[SWARM_MASTER] Not running");
        return 0;
    }

    master->running = false;

    /* Close listen_fd to unblock poll() */
    if (master->listen_fd >= 0) {
        close(master->listen_fd);
        master->listen_fd = -1;
    }

    /* Join event thread */
    nimcp_thread_join(master->event_thread, NULL);
    master->started = false;

    LOG_INFO("[SWARM_MASTER] Master stopped");
    return 0;
}

/* ============================================================================
 * Accept New Connection
 * ============================================================================ */

static void _accept_new_connection(nimcp_swarm_master_t* master)
{
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    memset(&peer_addr, 0, sizeof(peer_addr));

    int client_fd = accept(master->listen_fd,
                           (struct sockaddr*)&peer_addr, &addr_len);
    if (client_fd < 0) {
        if (errno != EINVAL && errno != EBADF) {
            LOG_WARN("[SWARM_MASTER] accept() failed: %s", strerror(errno));
        }
        return;
    }

    /* Extract peer address as string */
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peer_addr.sin_addr, addr_str, sizeof(addr_str));
    uint16_t peer_port = ntohs(peer_addr.sin_port);

    /* Generate a provisional device_id from a monotonic counter.
     * The real device_id will arrive in the JOIN_REQUEST message.
     * High bit set = provisional (avoids collision with real device IDs). */
    static volatile uint32_t _next_provisional_id = 0x80000000;  /* High bit set = provisional */
    uint32_t provisional_id = __atomic_fetch_add(&_next_provisional_id, 1, __ATOMIC_RELAXED);

    int ret = nimcp_peer_registry_add(master->registry, provisional_id,
                                       addr_str, peer_port, NULL);
    if (ret != 0) {
        LOG_WARN("[SWARM_MASTER] Failed to register peer from %s:%u",
                 addr_str, peer_port);
        close(client_fd);
        return;
    }

    /* Store the socket fd on the peer entry */
    nimcp_peer_entry_t* entry =
        nimcp_peer_registry_find(master->registry, provisional_id);
    if (entry) {
        entry->socket_fd = client_fd;
    }

    LOG_INFO("[SWARM_MASTER] Accepted connection from %s:%u (fd=%d, provisional_id=%u)",
             addr_str, peer_port, client_fd, provisional_id);
}

/* ============================================================================
 * Handle Discovery Queries
 * ============================================================================ */

static void _handle_discovery(nimcp_swarm_master_t* master)
{
    uint8_t buf[64];
    struct sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);
    memset(&src_addr, 0, sizeof(src_addr));

    ssize_t n = recvfrom(master->discovery_fd, buf, sizeof(buf), 0,
                         (struct sockaddr*)&src_addr, &src_len);
    if (n <= 0) {
        return;
    }

    /* Respond with our own discovery announcement */
    nimcp_swarm_discovery_announce(
        master->config.device_id,
        true,  /* is_master */
        master->config.listen_port,
        0, 0,  /* version major/minor — placeholder */
        (uint8_t)NIMCP_DEVICE_COORDINATOR,
        master->config.discovery.multicast_group,
        master->config.discovery.discovery_port);
}

/* ============================================================================
 * Message Reading from a Peer Socket
 * ============================================================================ */

/**
 * @brief Read a framed message from a peer's TCP socket.
 *
 * Wire format (matching nimcp_swarm_comm.c):
 *   type(4) + sender_id(4) + payload_size(4) + payload(N)
 *
 * @return 0 on success, -1 on error or disconnect.
 */
static int _read_peer_message(int fd,
                              nimcp_swarm_msg_type_t* out_type,
                              uint32_t* out_sender_id,
                              uint8_t** out_payload,
                              uint32_t* out_payload_size)
{
    uint8_t header[12];
    ssize_t n = recv(fd, header, 12, MSG_WAITALL);
    if (n < 12) {
        return -1;  /* Disconnect or error */
    }

    uint32_t msg_type, sender_id, payload_size;
    memcpy(&msg_type, header, 4);
    memcpy(&sender_id, header + 4, 4);
    memcpy(&payload_size, header + 8, 4);

    /* Sanity check */
    if (payload_size > 1024 * 1024) {
        LOG_WARN("[SWARM_MASTER] Payload too large from fd=%d: %u bytes",
                 fd, payload_size);
        return -1;
    }

    *out_type      = (nimcp_swarm_msg_type_t)msg_type;
    *out_sender_id = sender_id;
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

/* ============================================================================
 * Send Message to a Peer
 * ============================================================================ */

/**
 * @brief Send a framed message to a peer via its TCP socket.
 *
 * Uses the same wire format as nimcp_swarm_comm.c:
 *   type(4) + sender_id(4) + payload_size(4) + payload(N)
 *
 * NOTE: Wire format assumes little-endian byte order.
 * This is correct for x86/x64 and ARM (default LE mode).
 * Big-endian platforms would need htonl/ntohl conversion.
 */
static int _send_to_peer(int fd, nimcp_swarm_msg_type_t type,
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
 * Handle Peer Message — dispatch by type
 * ============================================================================ */

static void _handle_peer_message(nimcp_swarm_master_t* master,
                                 nimcp_peer_entry_t* peer,
                                 nimcp_swarm_msg_type_t msg_type,
                                 const uint8_t* payload,
                                 uint32_t payload_size)
{
    switch (msg_type) {

    case NIMCP_SWARM_MSG_HEARTBEAT:
        /* Update heartbeat timestamp in registry */
        nimcp_peer_registry_update_heartbeat(master->registry, peer->device_id);
        /* Send heartbeat ACK back */
        _send_to_peer(peer->socket_fd, NIMCP_SWARM_MSG_HEARTBEAT,
                       master->config.device_id, NULL, 0);
        break;

    case NIMCP_SWARM_MSG_GRADIENT:
        /* Submit gradient to the current sync round */
        if (payload && payload_size >= sizeof(uint32_t)) {
            /* Payload layout: num_params(4) + float[num_params] */
            uint32_t num_params;
            memcpy(&num_params, payload, sizeof(uint32_t));

            /* Integer overflow guard: num_params * sizeof(float) must not wrap */
            if (num_params > (UINT32_MAX - sizeof(uint32_t)) / sizeof(float)) {
                LOG_WARN("[SWARM_MASTER] Gradient num_params overflow: %u", num_params);
                break;
            }
            uint32_t expected_size = (uint32_t)(sizeof(uint32_t) + num_params * sizeof(float));
            if (payload_size >= expected_size && num_params > 0) {
                const float* gradients = (const float*)(payload + sizeof(uint32_t));

                /* Byzantine check BEFORE aggregation */
                extern int nimcp_byzantine_check_gradient(nimcp_peer_entry_t* peer,
                                                          const float* gradients,
                                                          uint32_t num_params);
                if (nimcp_byzantine_check_gradient(peer, gradients, num_params) == 1) {
                    LOG_WARN("[SWARM_MASTER] Byzantine gradient detected from device %u — rejected",
                             peer->device_id);
                    break;  /* Don't submit poisoned gradients */
                }

                nimcp_sync_round_submit_gradient(master->current_round,
                                                  peer->device_id,
                                                  gradients, num_params);
                LOG_INFO("[SWARM_MASTER] Gradient received from peer %u (%u params)",
                         peer->device_id, num_params);
            }
        }
        break;

    case NIMCP_SWARM_MSG_REPORT:
        /* Telemetry report — deserialize and check for Byzantine behavior */
        if (payload && payload_size >= sizeof(nimcp_device_telemetry_t)) {
            nimcp_device_telemetry_t telemetry;
            memcpy(&telemetry, payload, sizeof(nimcp_device_telemetry_t));

            /* Byzantine telemetry check */
            bool anomalous = nimcp_swarm_byzantine_check_telemetry(&telemetry);
            if (anomalous) {
                peer->anomaly_count++;
                LOG_WARN("[SWARM_MASTER] Anomalous telemetry from peer %u "
                         "(anomaly_count=%u)", peer->device_id, peer->anomaly_count);

                if (peer->anomaly_count >= master->config.byzantine_anomaly_limit) {
                    nimcp_peer_registry_set_state(master->registry,
                                                   peer->device_id,
                                                   NIMCP_PEER_BYZANTINE);
                    peer->quarantined = true;
                    LOG_WARN("[SWARM_MASTER] Peer %u quarantined (BYZANTINE)",
                             peer->device_id);
                }
            }

            /* Store latest telemetry only if not anomalous */
            if (!anomalous) {
                memcpy(&peer->last_telemetry, &telemetry,
                       sizeof(nimcp_device_telemetry_t));
            }
        }
        break;

    case NIMCP_SWARM_MSG_DIRECTIVE:
        /* JOIN_REQUEST — payload may contain device profile */
        if (peer->state == NIMCP_PEER_JOINING) {
            /* Transition peer to ACTIVE */
            nimcp_peer_registry_set_state(master->registry,
                                           peer->device_id, NIMCP_PEER_ACTIVE);

            /* Send welcome/acknowledgment back */
            _send_to_peer(peer->socket_fd, NIMCP_SWARM_MSG_DIRECTIVE,
                           master->config.device_id, NULL, 0);

            LOG_INFO("[SWARM_MASTER] Peer %u joined swarm (ACTIVE)",
                     peer->device_id);
        }
        break;

    case NIMCP_SWARM_MSG_HEARTBEAT + 100:
        /* LEAVE — using a sentinel offset; real protocol would use a sub-type.
         * For now, any unrecognized message from a peer in LEAVING state
         * triggers removal. Handled below in default. */
        break;

    default:
        LOG_WARN("[SWARM_MASTER] Unhandled message type %d from peer %u",
                 (int)msg_type, peer->device_id);
        break;
    }
}

/* ============================================================================
 * Trigger Sync Round
 * ============================================================================ */

static void _trigger_sync_round(nimcp_swarm_master_t* master)
{
    uint32_t active_count =
        nimcp_peer_registry_get_active_count(master->registry);

    if (active_count < master->config.min_devices_for_sync) {
        return;
    }

    /* Only start if the current round is idle */
    nimcp_sync_phase_t phase =
        nimcp_sync_round_get_phase(master->current_round);
    if (phase != NIMCP_SYNC_IDLE) {
        return;
    }

    /* Begin a new sync round */
    int ret = nimcp_sync_round_begin(master->current_round,
                                      master->next_round_id,
                                      active_count,
                                      SYNC_COLLECTION_TIMEOUT_MS);
    if (ret != 0) {
        LOG_WARN("[SWARM_MASTER] Failed to begin sync round %lu",
                 (unsigned long)master->next_round_id);
        return;
    }

    master->next_round_id++;

    LOG_INFO("[SWARM_MASTER] Sync round %lu started, expecting %u peers",
             (unsigned long)(master->next_round_id - 1), active_count);

    /* Broadcast COLLECT_GRADIENTS directive to all active peers */
    uint32_t peer_ids[64];
    uint32_t n_active = nimcp_peer_registry_get_active_peers(
        master->registry, peer_ids, 64);

    for (uint32_t i = 0; i < n_active; i++) {
        nimcp_peer_entry_t* entry =
            nimcp_peer_registry_find(master->registry, peer_ids[i]);
        if (entry && entry->socket_fd >= 0) {
            _send_to_peer(entry->socket_fd, NIMCP_SWARM_MSG_DIRECTIVE,
                           master->config.device_id, "COLLECT", 7);
        }
    }
}

/* ============================================================================
 * Complete Sync Round — aggregate + push
 * ============================================================================ */

static void _complete_sync_round(nimcp_swarm_master_t* master)
{
    /* Aggregate gradients */
    int ret = nimcp_sync_round_aggregate(master->current_round,
                                          &master->config.federated);
    if (ret != 0) {
        LOG_WARN("[SWARM_MASTER] Sync round aggregation failed");
        nimcp_sync_round_reset(master->current_round);
        return;
    }

    LOG_INFO("[SWARM_MASTER] Sync round aggregated (%u gradients)",
             master->current_round->gradients_received);

    /* TODO: Apply aggregated gradients to master brain weights.
     * This requires a brain API like nimcp_brain_apply_gradients().
     * For now, we compute a delta and push it to peers. */

    /* Compute delta weights for distribution.
     * In a full implementation, pre_round_weights would be snapshotted
     * before the round, and we'd compare against post-aggregation weights. */

    /* Push aggregated gradients to all active peers as WEIGHT_PUSH */
    uint32_t peer_ids[64];
    uint32_t n_active = nimcp_peer_registry_get_active_peers(
        master->registry, peer_ids, 64);

    uint32_t num_params = master->current_round->num_params;
    uint32_t push_size = sizeof(uint32_t) + num_params * sizeof(float);
    uint8_t* push_buf = (uint8_t*)nimcp_malloc(push_size);

    if (!push_buf) {
        LOG_ERROR("[SWARM_MASTER] Failed to allocate sync push buffer (%u bytes)", push_size);
    } else {
        memcpy(push_buf, &num_params, sizeof(uint32_t));
        memcpy(push_buf + sizeof(uint32_t),
               master->current_round->aggregated_gradients,
               num_params * sizeof(float));

        for (uint32_t i = 0; i < n_active; i++) {
            nimcp_peer_entry_t* entry =
                nimcp_peer_registry_find(master->registry, peer_ids[i]);
            if (entry && entry->socket_fd >= 0 && !entry->quarantined) {
                _send_to_peer(entry->socket_fd, NIMCP_SWARM_MSG_WEIGHT_PUSH,
                               master->config.device_id, push_buf, push_size);
                entry->total_syncs++;
            }
        }

        nimcp_free(push_buf);
    }

    /* Reset the round for next cycle */
    nimcp_sync_round_reset(master->current_round);

    LOG_INFO("[SWARM_MASTER] Sync round complete, pushed to %u peers", n_active);
}

/* ============================================================================
 * Master Event Loop — main thread function
 * ============================================================================ */

static void* _master_event_loop(void* arg)
{
    nimcp_swarm_master_t* master = (nimcp_swarm_master_t*)arg;

    master->started = true;

    LOG_INFO("[SWARM_MASTER] Event loop started");

    while (master->running) {
        /* Build pollfd array: [listen_fd, discovery_fd, peer_fds...] */
        struct pollfd fds[MASTER_MAX_POLLFDS];
        int nfds = 0;

        /* Slot 0: TCP listen socket */
        int listen_slot = -1;
        if (master->listen_fd >= 0) {
            listen_slot = nfds;
            fds[nfds].fd = master->listen_fd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }

        /* Slot 1: UDP discovery socket */
        int discovery_slot = -1;
        if (master->discovery_fd >= 0) {
            discovery_slot = nfds;
            fds[nfds].fd = master->discovery_fd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }

        /* Remaining slots: connected peer sockets */
        typedef struct {
            uint32_t device_id;
            int poll_idx;
        } peer_poll_map_t;

        peer_poll_map_t peer_map[MASTER_MAX_POLLFDS];
        int peer_count = 0;

        /* Snapshot peer fds under lock to avoid TOCTOU races */
        if (master->registry) {
            nimcp_mutex_lock(master->registry->lock);
            if (master->registry->peers) {
                for (uint32_t i = 0; i < master->registry->count &&
                                       nfds < MASTER_MAX_POLLFDS; i++) {
                    nimcp_peer_entry_t* entry = &master->registry->peers[i];
                    if (entry->socket_fd >= 0 &&
                        entry->state != NIMCP_PEER_DEAD) {
                        peer_map[peer_count].device_id = entry->device_id;
                        peer_map[peer_count].poll_idx  = nfds;
                        peer_count++;

                        fds[nfds].fd = entry->socket_fd;
                        fds[nfds].events = POLLIN;
                        fds[nfds].revents = 0;
                        nfds++;
                    }
                }
            }
            nimcp_mutex_unlock(master->registry->lock);
        }

        /* Poll with timeout */
        int poll_ret = poll(fds, (nfds_t)nfds, MASTER_POLL_TIMEOUT_MS);

        if (poll_ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (master->running) {
                LOG_WARN("[SWARM_MASTER] poll() error: %s", strerror(errno));
            }
            continue;
        }

        /* Handle events */
        if (poll_ret > 0) {
            /* New connection on listen socket */
            if (listen_slot >= 0 && (fds[listen_slot].revents & POLLIN)) {
                _accept_new_connection(master);
            }

            /* Discovery query */
            if (discovery_slot >= 0 && (fds[discovery_slot].revents & POLLIN)) {
                _handle_discovery(master);
            }

            /* Peer data */
            for (int p = 0; p < peer_count; p++) {
                int slot = peer_map[p].poll_idx;
                if (fds[slot].revents & (POLLIN | POLLHUP | POLLERR)) {
                    nimcp_peer_entry_t* entry =
                        nimcp_peer_registry_find(master->registry,
                                                  peer_map[p].device_id);
                    if (!entry) {
                        continue;
                    }

                    if (fds[slot].revents & (POLLHUP | POLLERR)) {
                        /* Peer disconnected */
                        LOG_INFO("[SWARM_MASTER] Peer %u disconnected (fd=%d)",
                                 entry->device_id, entry->socket_fd);
                        close(entry->socket_fd);
                        entry->socket_fd = -1;
                        nimcp_peer_registry_set_state(
                            master->registry, entry->device_id, NIMCP_PEER_DEAD);
                        continue;
                    }

                    /* Read message from peer */
                    nimcp_swarm_msg_type_t msg_type;
                    uint32_t sender_id;
                    uint8_t* payload = NULL;
                    uint32_t payload_size = 0;

                    int read_ret = _read_peer_message(
                        entry->socket_fd, &msg_type, &sender_id,
                        &payload, &payload_size);

                    if (read_ret != 0) {
                        /* Read failed — peer probably disconnected */
                        LOG_INFO("[SWARM_MASTER] Read failed for peer %u, "
                                 "marking DEAD", entry->device_id);
                        close(entry->socket_fd);
                        entry->socket_fd = -1;
                        nimcp_peer_registry_set_state(
                            master->registry, entry->device_id, NIMCP_PEER_DEAD);
                        continue;
                    }

                    /* Update device_id if this is a JOIN with a real id */
                    if (sender_id != 0 && sender_id != entry->device_id) {
                        entry->device_id = sender_id;
                    }

                    _handle_peer_message(master, entry, msg_type,
                                         payload, payload_size);

                    if (payload) {
                        nimcp_free(payload);
                    }
                }
            }
        }

        /* Timer checks */
        uint64_t now = nimcp_time_now_us();
        uint64_t sync_interval_us =
            (uint64_t)master->config.sync_interval_ms * 1000ULL;
        uint64_t sweep_interval_us =
            (uint64_t)MASTER_SWEEP_INTERVAL_MS * 1000ULL;
        uint64_t announce_interval_us =
            (uint64_t)MASTER_ANNOUNCE_INTERVAL_MS * 1000ULL;

        /* Sync round trigger */
        if (now - master->last_sync_ts >= sync_interval_us) {
            master->last_sync_ts = now;
            _trigger_sync_round(master);
        }

        /* Check if an active sync round is ready for completion */
        nimcp_sync_phase_t phase =
            nimcp_sync_round_get_phase(master->current_round);
        if (phase == NIMCP_SYNC_COLLECTING &&
            nimcp_sync_round_is_ready(master->current_round)) {
            _complete_sync_round(master);
        }

        /* Heartbeat sweep */
        if (now - master->last_sweep_ts >= sweep_interval_us) {
            master->last_sweep_ts = now;
            nimcp_peer_registry_sweep(master->registry,
                                       master->config.heartbeat_timeout_ms,
                                       master->config.dead_timeout_ms);

            /* Remove dead peers after sweep */
            for (uint32_t i = 0; i < master->registry->count; ) {
                if (master->registry->peers[i].state == NIMCP_PEER_DEAD) {
                    nimcp_peer_registry_remove(master->registry,
                        master->registry->peers[i].device_id);
                } else {
                    i++;
                }
            }
        }

        /* Discovery announcement */
        if (master->discovery_fd >= 0 &&
            now - master->last_announce_ts >= announce_interval_us) {
            master->last_announce_ts = now;
            nimcp_swarm_discovery_announce(
                master->config.device_id,
                true,
                master->config.listen_port,
                0, 0,
                (uint8_t)NIMCP_DEVICE_COORDINATOR,
                master->config.discovery.multicast_group,
                master->config.discovery.discovery_port);
        }
    }

    LOG_INFO("[SWARM_MASTER] Event loop exited");
    return NULL;
}

/* ============================================================================
 * Public Query / Control Functions
 * ============================================================================ */

int nimcp_swarm_master_kick(nimcp_swarm_master_t* master, uint32_t device_id)
{
    if (!master) {
        LOG_ERROR("[SWARM_MASTER] NULL master in kick");
        return -1;
    }

    nimcp_peer_entry_t* entry =
        nimcp_peer_registry_find(master->registry, device_id);
    if (!entry) {
        LOG_WARN("[SWARM_MASTER] Cannot kick unknown peer %u", device_id);
        return -1;
    }

    /* Close the peer's socket */
    if (entry->socket_fd >= 0) {
        close(entry->socket_fd);
        entry->socket_fd = -1;
    }

    /* Remove from registry */
    int ret = nimcp_peer_registry_remove(master->registry, device_id);
    if (ret == 0) {
        LOG_INFO("[SWARM_MASTER] Kicked peer %u", device_id);
    }
    return ret;
}

int nimcp_swarm_master_force_sync(nimcp_swarm_master_t* master)
{
    if (!master) {
        LOG_ERROR("[SWARM_MASTER] NULL master in force_sync");
        return -1;
    }

    nimcp_sync_phase_t phase =
        nimcp_sync_round_get_phase(master->current_round);
    if (phase != NIMCP_SYNC_IDLE) {
        LOG_WARN("[SWARM_MASTER] Sync round already in progress (phase=%d)",
                 (int)phase);
        return 0;
    }

    _trigger_sync_round(master);
    return 0;
}

uint32_t nimcp_swarm_master_get_peer_count(const nimcp_swarm_master_t* master)
{
    if (!master || !master->registry) {
        return 0;
    }

    /* Cast away const — nimcp_peer_registry_get_active_count acquires lock */
    return nimcp_peer_registry_get_active_count(
        (nimcp_peer_registry_t*)master->registry);
}

int nimcp_swarm_master_get_peer_info(const nimcp_swarm_master_t* master,
                                     uint32_t device_id,
                                     nimcp_peer_entry_t* entry_out)
{
    if (!master || !entry_out) {
        return -1;
    }

    nimcp_peer_entry_t* found =
        nimcp_peer_registry_find((nimcp_peer_registry_t*)master->registry,
                                 device_id);
    if (!found) {
        return -1;
    }

    memcpy(entry_out, found, sizeof(nimcp_peer_entry_t));
    return 0;
}
