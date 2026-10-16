/**
 * @file nimcp_swarm_comm.c
 * @brief Swarm communication — peer mesh and master link transport.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>

/* ============================================================================
 * CRC32 (table-based)
 * ============================================================================ */

static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

static void crc32_init_table(void) {
    if (crc32_table_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

static uint32_t crc32_compute(const uint8_t* data, uint32_t length) {
    crc32_init_table();
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

/* ============================================================================
 * Transport Lifecycle
 * ============================================================================ */

nimcp_swarm_transport_t* nimcp_swarm_transport_create(
    uint32_t device_id, const char* multicast_group, uint16_t peer_port,
    const char* master_host, uint16_t master_port)
{
    nimcp_swarm_transport_t* transport =
        (nimcp_swarm_transport_t*)nimcp_calloc(1, sizeof(nimcp_swarm_transport_t));
    if (!transport) {
        return NULL;
    }

    transport->device_id = device_id;
    transport->socket_fd = -1;  /* Not connected yet */
    transport->master_fd = -1;

    if (multicast_group) {
        strncpy(transport->multicast_group, multicast_group,
                sizeof(transport->multicast_group) - 1);
        transport->multicast_group[sizeof(transport->multicast_group) - 1] = '\0';
    }
    transport->peer_port = peer_port;

    if (master_host) {
        strncpy(transport->master_host, master_host,
                sizeof(transport->master_host) - 1);
        transport->master_host[sizeof(transport->master_host) - 1] = '\0';
    }
    transport->master_port = master_port;

    transport->connected_to_master = false;
    transport->peer_mesh_active = false;
    transport->last_heartbeat_sent = 0;
    transport->heartbeat_interval_ms = 1000; /* Default 1s heartbeat */

    /* Create UDP socket for peer mesh */
    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd >= 0) {
        int optval = 1;
        setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        struct sockaddr_in bind_addr;
        memset(&bind_addr, 0, sizeof(bind_addr));
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(peer_port);
        bind_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(udp_fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) == 0) {
            transport->socket_fd = udp_fd;
            transport->peer_mesh_active = true;
        } else {
            LOG_WARN("[edge/swarm] Failed to bind UDP port %u: %s",
                     peer_port, strerror(errno));
            close(udp_fd);
        }
    } else {
        LOG_WARN("[edge/swarm] Failed to create UDP socket: %s", strerror(errno));
    }

    /* Create TCP socket for master connection */
    if (master_host && master_port > 0) {
        int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_fd >= 0) {
            struct sockaddr_in master_addr;
            memset(&master_addr, 0, sizeof(master_addr));
            master_addr.sin_family = AF_INET;
            master_addr.sin_port = htons(master_port);

            if (inet_pton(AF_INET, master_host, &master_addr.sin_addr) > 0) {
                if (connect(tcp_fd, (struct sockaddr*)&master_addr,
                            sizeof(master_addr)) == 0) {
                    transport->master_fd = tcp_fd;
                    transport->connected_to_master = true;
                } else {
                    LOG_WARN("[edge/swarm] Failed to connect to master %s:%u: %s",
                             master_host, master_port, strerror(errno));
                    close(tcp_fd);
                }
            } else {
                LOG_WARN("[edge/swarm] Invalid master host: %s", master_host);
                close(tcp_fd);
            }
        } else {
            LOG_WARN("[edge/swarm] Failed to create TCP socket: %s", strerror(errno));
        }
    }

    LOG_INFO("[edge/swarm] Transport created: device=%u, multicast=%s:%u, master=%s:%u, "
             "peer_mesh=%s, master_connected=%s",
             device_id,
             multicast_group ? multicast_group : "(none)", peer_port,
             master_host ? master_host : "(none)", master_port,
             transport->peer_mesh_active ? "true" : "false",
             transport->connected_to_master ? "true" : "false");

    return transport;
}

void nimcp_swarm_transport_destroy(nimcp_swarm_transport_t* transport)
{
    if (!transport) {
        return;
    }

    /* Close sockets */
    if (transport->socket_fd >= 0) {
        close(transport->socket_fd);
        transport->socket_fd = -1;
    }
    if (transport->master_fd >= 0) {
        close(transport->master_fd);
        transport->master_fd = -1;
    }
    transport->peer_mesh_active = false;
    transport->connected_to_master = false;

    LOG_INFO("[edge/swarm] Transport destroyed: device=%u", transport->device_id);
    nimcp_free(transport);
}

/* ============================================================================
 * Message Send/Receive
 * ============================================================================ */

int nimcp_swarm_send_peer(nimcp_swarm_transport_t* transport,
                           const nimcp_swarm_message_t* msg)
{
    if (!transport) {
        LOG_ERROR("[edge/swarm] NULL transport");
        return -1;
    }
    if (!msg) {
        LOG_ERROR("[edge/swarm] NULL message");
        return -1;
    }
    if (!transport->peer_mesh_active) {
        LOG_WARN("[edge/swarm] Peer mesh not active, cannot send (device=%u)",
                 transport->device_id);
        return -1;
    }

    if (transport->socket_fd < 0) {
        LOG_WARN("[edge/swarm] No UDP socket available for peer send");
        return -1;
    }

    /* Serialize: type(4) + sender_id(4) + payload_size(4) + payload(N) */
    uint32_t header_size = 12;
    uint32_t total_size = header_size + msg->payload_size;
    uint8_t* buf = (uint8_t*)nimcp_malloc(total_size);
    if (!buf) {
        return -1;
    }

    memcpy(buf, &msg->type, 4);
    memcpy(buf + 4, &msg->sender_id, 4);
    memcpy(buf + 8, &msg->payload_size, 4);
    if (msg->payload && msg->payload_size > 0) {
        memcpy(buf + 12, msg->payload, msg->payload_size);
    }

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(transport->peer_port);
    inet_pton(AF_INET, transport->multicast_group, &dest.sin_addr);

    ssize_t sent = sendto(transport->socket_fd, buf, total_size, 0,
                          (struct sockaddr*)&dest, sizeof(dest));
    nimcp_free(buf);

    if (sent < 0) {
        LOG_WARN("[edge/swarm] sendto failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

int nimcp_swarm_send_master(nimcp_swarm_transport_t* transport,
                             const nimcp_swarm_message_t* msg)
{
    if (!transport) {
        LOG_ERROR("[edge/swarm] NULL transport");
        return -1;
    }
    if (!msg) {
        LOG_ERROR("[edge/swarm] NULL message");
        return -1;
    }
    if (!transport->connected_to_master) {
        LOG_WARN("[edge/swarm] Not connected to master, cannot send (device=%u)",
                 transport->device_id);
        return -1;
    }

    if (transport->master_fd < 0) {
        LOG_WARN("[edge/swarm] No TCP socket available for master send");
        return -1;
    }

    /* Serialize: type(4) + sender_id(4) + payload_size(4) + payload(N) */
    uint32_t header_size = 12;
    uint32_t total_size = header_size + msg->payload_size;
    uint8_t* buf = (uint8_t*)nimcp_malloc(total_size);
    if (!buf) {
        return -1;
    }

    memcpy(buf, &msg->type, 4);
    memcpy(buf + 4, &msg->sender_id, 4);
    memcpy(buf + 8, &msg->payload_size, 4);
    if (msg->payload && msg->payload_size > 0) {
        memcpy(buf + 12, msg->payload, msg->payload_size);
    }

    ssize_t sent = send(transport->master_fd, buf, total_size, 0);
    nimcp_free(buf);

    if (sent < 0) {
        LOG_WARN("[edge/swarm] TCP send to master failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

int nimcp_swarm_recv(nimcp_swarm_transport_t* transport,
                      nimcp_swarm_message_t* msg, uint32_t timeout_ms)
{
    if (!transport || !msg) {
        return -1;
    }

    /* Build pollfd array for available sockets */
    struct pollfd fds[2];
    int nfds = 0;

    if (transport->socket_fd >= 0) {
        fds[nfds].fd = transport->socket_fd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        nfds++;
    }
    if (transport->master_fd >= 0) {
        fds[nfds].fd = transport->master_fd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        nfds++;
    }

    if (nfds == 0) {
        return -1;
    }

    int ret = poll(fds, (nfds_t)nfds, (int)timeout_ms);
    if (ret <= 0) {
        return -1; /* Timeout or error */
    }

    /* Read from first socket with data */
    for (int i = 0; i < nfds; i++) {
        if (!(fds[i].revents & POLLIN)) continue;

        uint8_t header[12];
        ssize_t n = recv(fds[i].fd, header, 12, MSG_PEEK);
        if (n < 12) continue;

        /* Deserialize header */
        uint32_t msg_type, sender_id, payload_size;
        memcpy(&msg_type, header, 4);
        memcpy(&sender_id, header + 4, 4);
        memcpy(&payload_size, header + 8, 4);

        /* Sanity check payload size */
        if (payload_size > 1024 * 1024) continue;

        uint32_t total = 12 + payload_size;
        uint8_t* buf = (uint8_t*)nimcp_malloc(total);
        if (!buf) return -1;

        n = recv(fds[i].fd, buf, total, 0);
        if (n < (ssize_t)total) {
            nimcp_free(buf);
            continue;
        }

        msg->type = (nimcp_swarm_msg_type_t)msg_type;
        msg->sender_id = sender_id;
        msg->recipient_id = 0;
        msg->timestamp = (uint64_t)time(NULL);
        msg->payload_size = payload_size;

        if (payload_size > 0) {
            msg->payload = (uint8_t*)nimcp_malloc(payload_size);
            if (msg->payload) {
                memcpy(msg->payload, buf + 12, payload_size);
            }
        } else {
            msg->payload = NULL;
        }

        nimcp_free(buf);
        return 0;
    }

    return -1;
}

/* ============================================================================
 * Heartbeat
 * ============================================================================ */

int nimcp_swarm_send_heartbeat(nimcp_swarm_transport_t* transport,
                                const float* position)
{
    if (!transport) {
        return -1;
    }

    /* Build heartbeat payload: device_id + position[3] */
    typedef struct {
        uint32_t device_id;
        float position[3];
        uint64_t timestamp;
    } heartbeat_payload_t;

    heartbeat_payload_t hb;
    hb.device_id = transport->device_id;
    if (position) {
        hb.position[0] = position[0];
        hb.position[1] = position[1];
        hb.position[2] = position[2];
    } else {
        hb.position[0] = 0.0f;
        hb.position[1] = 0.0f;
        hb.position[2] = 0.0f;
    }
    hb.timestamp = (uint64_t)time(NULL);

    /* Create message */
    nimcp_swarm_message_t* msg = nimcp_swarm_message_create(
        NIMCP_SWARM_MSG_HEARTBEAT,
        transport->device_id,
        0, /* broadcast */
        &hb, sizeof(hb));
    if (!msg) {
        return -1;
    }

    int rc = nimcp_swarm_send_peer(transport, msg);

    transport->last_heartbeat_sent = hb.timestamp;

    nimcp_swarm_message_destroy(msg);
    return rc;
}

/* ============================================================================
 * Message Create / Destroy
 * ============================================================================ */

nimcp_swarm_message_t* nimcp_swarm_message_create(
    nimcp_swarm_msg_type_t type, uint32_t sender_id,
    uint32_t recipient_id, const void* payload, uint32_t payload_size)
{
    nimcp_swarm_message_t* msg =
        (nimcp_swarm_message_t*)nimcp_calloc(1, sizeof(nimcp_swarm_message_t));
    if (!msg) {
        return NULL;
    }

    msg->type = type;
    msg->sender_id = sender_id;
    msg->recipient_id = recipient_id;
    msg->timestamp = (uint64_t)time(NULL);
    msg->payload_size = payload_size;

    if (payload && payload_size > 0) {
        msg->payload = (uint8_t*)nimcp_malloc(payload_size);
        if (!msg->payload) {
            nimcp_free(msg);
            return NULL;
        }
        memcpy(msg->payload, payload, payload_size);
    } else {
        msg->payload = NULL;
        msg->payload_size = 0;
    }

    /* CRC32 checksum of payload */
    memset(msg->checksum, 0, sizeof(msg->checksum));
    if (msg->payload && msg->payload_size > 0) {
        uint32_t crc = crc32_compute(msg->payload, msg->payload_size);
        memcpy(msg->checksum, &crc, sizeof(crc));
    }

    return msg;
}

void nimcp_swarm_message_destroy(nimcp_swarm_message_t* msg)
{
    if (!msg) {
        return;
    }
    if (msg->payload) {
        nimcp_free(msg->payload);
    }
    nimcp_free(msg);
}
