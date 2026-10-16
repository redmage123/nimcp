/**
 * @file nimcp_swarm_discovery.c
 * @brief Swarm peer discovery via UDP multicast + manual config fallback.
 *
 * Devices announce themselves on a multicast group and listen for peers.
 * Discovery packets use a compact 16-byte wire format with magic validation.
 *
 * Wire format:
 *   Bytes 0-3:   Magic "NIMD" (0x4E494D44)
 *   Bytes 4-7:   device_id (uint32_t, network byte order)
 *   Byte  8:     is_master (uint8_t, 0 or 1)
 *   Bytes 9-10:  tcp_port (uint16_t, network byte order)
 *   Bytes 11-12: version_major (uint16_t, network byte order)
 *   Bytes 13-14: version_minor (uint16_t, network byte order)
 *   Byte  15:    device_role (uint8_t)
 *   Total: 16 bytes
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
 * Constants
 * ============================================================================ */

#define DISCOVERY_MAGIC          0x4E494D44u  /* "NIMD" */
#define DISCOVERY_PACKET_SIZE    16
#define DISCOVERY_MAX_PEERS      64

#define DISCOVERY_DEFAULT_MULTICAST_GROUP  "239.42.42.1"
#define DISCOVERY_DEFAULT_PORT            9420
#define DISCOVERY_DEFAULT_ANNOUNCE_MS     5000
#define DISCOVERY_DEFAULT_TIMEOUT_MS      10000

/* ============================================================================
 * Discovery Config
 * ============================================================================ */

typedef struct {
    bool use_mdns;
    char multicast_group[64];
    uint16_t discovery_port;
    uint32_t announce_interval_ms;
    uint32_t discovery_timeout_ms;
    char** manual_peers;          /* Array of "host:port" strings */
    uint32_t manual_peer_count;
} nimcp_discovery_config_t;

/* ============================================================================
 * Discovered Peer Info
 * ============================================================================ */

typedef struct {
    uint32_t device_id;
    char address[64];             /* Source IP address */
    uint16_t tcp_port;
    bool is_master;
    uint16_t version_major;
    uint16_t version_minor;
    uint8_t device_role;
} nimcp_discovered_peer_t;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Create a UDP socket bound to the discovery port, joined to multicast.
 * @param multicast_group  Multicast group address (e.g. "239.42.42.1")
 * @param port             UDP port to bind
 * @return Socket fd on success, -1 on failure.
 */
static int _create_multicast_socket(const char* multicast_group, uint16_t port)
{
    if (!multicast_group) {
        LOG_ERROR("[edge/discovery] NULL multicast_group");
        return -1;
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        LOG_ERROR("[edge/discovery] Failed to create UDP socket: %s",
                  strerror(errno));
        return -1;
    }

    /* Allow address reuse so multiple processes can bind */
    int optval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        LOG_WARN("[edge/discovery] setsockopt SO_REUSEADDR failed: %s",
                 strerror(errno));
        /* Non-fatal, continue */
    }

    /* Bind to the discovery port on all interfaces */
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(port);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        LOG_ERROR("[edge/discovery] Failed to bind UDP port %u: %s",
                  (unsigned)port, strerror(errno));
        close(fd);
        return -1;
    }

    /* Join multicast group via IP_ADD_MEMBERSHIP */
    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    if (inet_pton(AF_INET, multicast_group, &mreq.imr_multiaddr) <= 0) {
        LOG_ERROR("[edge/discovery] Invalid multicast group: %s", multicast_group);
        close(fd);
        return -1;
    }
    mreq.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &mreq, sizeof(mreq)) < 0) {
        LOG_ERROR("[edge/discovery] Failed to join multicast group %s: %s",
                  multicast_group, strerror(errno));
        close(fd);
        return -1;
    }

    LOG_INFO("[edge/discovery] Multicast socket created: group=%s, port=%u, fd=%d",
             multicast_group, (unsigned)port, fd);
    return fd;
}

/**
 * @brief Parse a 16-byte discovery packet into peer info.
 * @param data        Raw packet data (must be at least DISCOVERY_PACKET_SIZE bytes)
 * @param data_len    Length of received data
 * @param src_addr    Source address of the packet sender
 * @param peer        Output: parsed peer info
 * @return 0 on success, -1 if packet is invalid.
 */
static int _parse_discovery_packet(const uint8_t* data, uint32_t data_len,
                                   const struct sockaddr_in* src_addr,
                                   nimcp_discovered_peer_t* peer)
{
    if (!data || !peer) {
        return -1;
    }

    /* Validate packet size */
    if (data_len < DISCOVERY_PACKET_SIZE) {
        LOG_WARN("[edge/discovery] Packet too short: %u bytes (expected %d)",
                 data_len, DISCOVERY_PACKET_SIZE);
        return -1;
    }

    /* Validate magic */
    uint32_t magic;
    memcpy(&magic, data, 4);
    if (magic != DISCOVERY_MAGIC) {
        LOG_WARN("[edge/discovery] Invalid magic: 0x%08X (expected 0x%08X)",
                 magic, DISCOVERY_MAGIC);
        return -1;
    }

    /* Parse fields */
    memset(peer, 0, sizeof(*peer));

    uint32_t device_id_net;
    memcpy(&device_id_net, data + 4, 4);
    peer->device_id = ntohl(device_id_net);

    peer->is_master = (data[8] != 0);

    uint16_t tcp_port_net;
    memcpy(&tcp_port_net, data + 9, 2);
    peer->tcp_port = ntohs(tcp_port_net);

    uint16_t ver_major_net, ver_minor_net;
    memcpy(&ver_major_net, data + 11, 2);
    memcpy(&ver_minor_net, data + 13, 2);
    peer->version_major = ntohs(ver_major_net);
    peer->version_minor = ntohs(ver_minor_net);

    peer->device_role = data[15];

    /* Extract source IP address */
    if (src_addr) {
        inet_ntop(AF_INET, &src_addr->sin_addr,
                  peer->address, sizeof(peer->address));
    } else {
        strncpy(peer->address, "0.0.0.0", sizeof(peer->address) - 1);
    }

    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

nimcp_discovery_config_t nimcp_discovery_config_default(void)
{
    nimcp_discovery_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.use_mdns = true;
    strncpy(cfg.multicast_group, DISCOVERY_DEFAULT_MULTICAST_GROUP,
            sizeof(cfg.multicast_group) - 1);
    cfg.discovery_port = DISCOVERY_DEFAULT_PORT;
    cfg.announce_interval_ms = DISCOVERY_DEFAULT_ANNOUNCE_MS;
    cfg.discovery_timeout_ms = DISCOVERY_DEFAULT_TIMEOUT_MS;
    cfg.manual_peers = NULL;
    cfg.manual_peer_count = 0;

    return cfg;
}

int nimcp_swarm_discovery_announce(uint32_t device_id, bool is_master,
                                   uint16_t tcp_port,
                                   uint16_t version_major, uint16_t version_minor,
                                   uint8_t device_role,
                                   const char* multicast_group, uint16_t discovery_port)
{
    if (!multicast_group) {
        LOG_ERROR("[edge/discovery] NULL multicast_group in announce");
        return -1;
    }

    /* Build the 16-byte discovery packet */
    uint8_t packet[DISCOVERY_PACKET_SIZE];
    memset(packet, 0, sizeof(packet));

    /* Magic */
    uint32_t magic = DISCOVERY_MAGIC;
    memcpy(packet, &magic, 4);

    /* Device ID (network byte order) */
    uint32_t device_id_net = htonl(device_id);
    memcpy(packet + 4, &device_id_net, 4);

    /* Is master */
    packet[8] = is_master ? 1 : 0;

    /* TCP port (network byte order) */
    uint16_t tcp_port_net = htons(tcp_port);
    memcpy(packet + 9, &tcp_port_net, 2);

    /* Version (network byte order) */
    uint16_t ver_major_net = htons(version_major);
    uint16_t ver_minor_net = htons(version_minor);
    memcpy(packet + 11, &ver_major_net, 2);
    memcpy(packet + 13, &ver_minor_net, 2);

    /* Device role */
    packet[15] = device_role;

    /* Create a UDP socket for sending */
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        LOG_ERROR("[edge/discovery] Failed to create send socket: %s",
                  strerror(errno));
        return -1;
    }

    /* Set multicast TTL to 1 (LAN only) */
    uint8_t ttl = 1;
    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        LOG_WARN("[edge/discovery] setsockopt IP_MULTICAST_TTL failed: %s",
                 strerror(errno));
        /* Non-fatal */
    }

    /* Build destination address */
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(discovery_port);
    if (inet_pton(AF_INET, multicast_group, &dest.sin_addr) <= 0) {
        LOG_ERROR("[edge/discovery] Invalid multicast group for announce: %s",
                  multicast_group);
        close(fd);
        return -1;
    }

    /* Send the announcement */
    ssize_t sent = sendto(fd, packet, DISCOVERY_PACKET_SIZE, 0,
                          (struct sockaddr*)&dest, sizeof(dest));
    close(fd);

    if (sent < 0) {
        LOG_ERROR("[edge/discovery] sendto failed: %s", strerror(errno));
        return -1;
    }
    if (sent != DISCOVERY_PACKET_SIZE) {
        LOG_WARN("[edge/discovery] Partial send: %zd/%d bytes",
                 sent, DISCOVERY_PACKET_SIZE);
        return -1;
    }

    LOG_INFO("[edge/discovery] Announced: device=%u, master=%s, port=%u, "
             "version=%u.%u, role=%u, group=%s:%u",
             device_id, is_master ? "true" : "false",
             (unsigned)tcp_port, (unsigned)version_major, (unsigned)version_minor,
             (unsigned)device_role, multicast_group, (unsigned)discovery_port);

    return 0;
}

int nimcp_swarm_discovery_listen(nimcp_discovered_peer_t* peers, uint32_t max_peers,
                                 uint32_t* out_count,
                                 const char* multicast_group, uint16_t discovery_port,
                                 uint32_t timeout_ms, uint32_t self_device_id)
{
    if (!peers || !out_count) {
        LOG_ERROR("[edge/discovery] NULL output parameters in listen");
        return -1;
    }
    if (max_peers == 0) {
        LOG_ERROR("[edge/discovery] max_peers must be > 0");
        return -1;
    }
    if (!multicast_group) {
        LOG_ERROR("[edge/discovery] NULL multicast_group in listen");
        return -1;
    }

    *out_count = 0;

    /* Create and bind multicast socket */
    int fd = _create_multicast_socket(multicast_group, discovery_port);
    if (fd < 0) {
        return -1;
    }

    /* Poll for incoming announcements until timeout */
    uint32_t elapsed_ms = 0;
    uint32_t peer_count = 0;

    while (elapsed_ms < timeout_ms && peer_count < max_peers) {
        uint32_t remaining_ms = timeout_ms - elapsed_ms;

        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int poll_ret = poll(&pfd, 1, (int)remaining_ms);
        if (poll_ret < 0) {
            if (errno == EINTR) {
                continue;  /* Interrupted, retry */
            }
            LOG_ERROR("[edge/discovery] poll failed: %s", strerror(errno));
            break;
        }
        if (poll_ret == 0) {
            /* Timeout reached */
            break;
        }

        if (!(pfd.revents & POLLIN)) {
            break;
        }

        /* Receive packet */
        uint8_t buf[64];
        struct sockaddr_in src_addr;
        socklen_t src_len = sizeof(src_addr);
        memset(&src_addr, 0, sizeof(src_addr));

        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0,
                             (struct sockaddr*)&src_addr, &src_len);
        if (n < (ssize_t)DISCOVERY_PACKET_SIZE) {
            continue;  /* Too short or error */
        }

        /* Parse the packet */
        nimcp_discovered_peer_t candidate;
        if (_parse_discovery_packet(buf, (uint32_t)n, &src_addr, &candidate) != 0) {
            continue;  /* Invalid packet */
        }

        /* Skip self */
        if (candidate.device_id == self_device_id) {
            continue;
        }

        /* Deduplicate by device_id */
        bool duplicate = false;
        for (uint32_t i = 0; i < peer_count; i++) {
            if (peers[i].device_id == candidate.device_id) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        /* Add to results */
        memcpy(&peers[peer_count], &candidate, sizeof(nimcp_discovered_peer_t));
        peer_count++;

        LOG_INFO("[edge/discovery] Discovered peer: device=%u, addr=%s:%u, "
                 "master=%s, version=%u.%u",
                 candidate.device_id, candidate.address,
                 (unsigned)candidate.tcp_port,
                 candidate.is_master ? "true" : "false",
                 (unsigned)candidate.version_major,
                 (unsigned)candidate.version_minor);
    }

    /* Leave multicast group and close socket */
    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    inet_pton(AF_INET, multicast_group, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
        LOG_WARN("[edge/discovery] Failed to leave multicast group: %s",
                 strerror(errno));
    }
    close(fd);

    *out_count = peer_count;

    LOG_INFO("[edge/discovery] Listen complete: discovered %u peer(s) in %ums",
             peer_count, timeout_ms);

    return 0;
}
