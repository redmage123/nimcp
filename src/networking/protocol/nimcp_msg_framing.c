/**
 * @file nimcp_msg_framing.c
 * @brief Implementation of message framing for collective cognition protocol
 *
 * WHAT: Universal message framing for inter-brain communication
 * WHY: Support both fast 24-byte messages and rich protobuf messages
 * HOW: 8-byte header + variable payload with format flags
 */

#include "networking/protocol/nimcp_msg_framing.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdatomic.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(msg_framing)

/*=============================================================================
 * Global Statistics (Thread-Safe with Atomics)
 *===========================================================================*/

/* SECURITY: Use atomic operations for thread-safe statistics */
static _Atomic uint64_t g_messages_sent = 0;
static _Atomic uint64_t g_messages_received = 0;
static _Atomic uint64_t g_fast_messages = 0;
static _Atomic uint64_t g_protobuf_messages = 0;
static _Atomic uint64_t g_compressed_messages = 0;
static _Atomic uint64_t g_encrypted_messages = 0;
static _Atomic uint64_t g_parse_errors = 0;
static _Atomic uint64_t g_checksum_errors = 0;
static _Atomic uint64_t g_bytes_sent = 0;
static _Atomic uint64_t g_bytes_received = 0;

/*=============================================================================
 * Header API Implementation
 *===========================================================================*/

void nimcp_msg_header_init(
    nimcp_msg_header_t* header,
    nimcp_msg_type_t msg_type,
    uint8_t flags,
    uint16_t payload_len
) {
    if (!header) return;

    header->magic[0] = NIMCP_MSG_MAGIC_0;
    header->magic[1] = NIMCP_MSG_MAGIC_1;
    header->version = NIMCP_MSG_VERSION;
    header->flags = flags;
    header->msg_type = msg_type;
    header->payload_len = payload_len;
}

bool nimcp_msg_header_validate(const nimcp_msg_header_t* header) {
    if (!header) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_msg_header_validate: header is NULL");

            return false;

        }

    /* Check magic bytes */
    if (header->magic[0] != NIMCP_MSG_MAGIC_0 ||
        header->magic[1] != NIMCP_MSG_MAGIC_1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_msg_header_validate: operation failed");
        return false;
    }

    /* Check version */
    if (header->version != NIMCP_MSG_VERSION) {
        return false;
    }

    /* Check payload length */
    if (header->payload_len > NIMCP_MSG_MAX_PAYLOAD) {
        return false;
    }

    /* Fast path messages must have 16-byte payload */
    if (nimcp_msg_is_fast_path(header) && header->payload_len != NIMCP_MSG_FAST_PAYLOAD) {
        return false;
    }

    return true;
}

/*=============================================================================
 * Fast Message API Implementation
 *===========================================================================*/

void nimcp_fast_msg_init(
    nimcp_fast_msg_t* msg,
    nimcp_msg_type_t msg_type,
    uint8_t flags
) {
    if (!msg) return;

    /* Clear protobuf flag for fast path */
    flags &= ~MSG_FLAG_PROTOBUF;

    nimcp_msg_header_init(&msg->header, msg_type, flags, NIMCP_MSG_FAST_PAYLOAD);
    memset(msg->payload, 0, sizeof(msg->payload));
}

void nimcp_fast_msg_heartbeat(
    nimcp_fast_msg_t* msg,
    uint32_t brain_id,
    float atp_level,
    float load
) {
    if (!msg) return;

    nimcp_fast_msg_init(msg, MSG_TYPE_HEARTBEAT, 0);

    /* Pack payload: brain_id (4) + atp_level (4) + load (4) + state (1) + padding (3) */
    uint8_t* p = msg->payload;

    /* Brain ID (big-endian) */
    p[0] = (brain_id >> 24) & 0xFF;
    p[1] = (brain_id >> 16) & 0xFF;
    p[2] = (brain_id >> 8) & 0xFF;
    p[3] = brain_id & 0xFF;

    /* ATP level (IEEE 754 float, big-endian) */
    uint32_t atp_bits;
    memcpy(&atp_bits, &atp_level, sizeof(float));
    p[4] = (atp_bits >> 24) & 0xFF;
    p[5] = (atp_bits >> 16) & 0xFF;
    p[6] = (atp_bits >> 8) & 0xFF;
    p[7] = atp_bits & 0xFF;

    /* Load (IEEE 754 float, big-endian) */
    uint32_t load_bits;
    memcpy(&load_bits, &load, sizeof(float));
    p[8] = (load_bits >> 24) & 0xFF;
    p[9] = (load_bits >> 16) & 0xFF;
    p[10] = (load_bits >> 8) & 0xFF;
    p[11] = load_bits & 0xFF;

    /* State (default: ACTIVE = 0) */
    p[12] = 0;

    /* Padding */
    p[13] = 0;
    p[14] = 0;
    p[15] = 0;
}

void nimcp_fast_msg_sync(
    nimcp_fast_msg_t* msg,
    uint32_t brain_id,
    float gamma_power,
    float gamma_phase
) {
    if (!msg) return;

    nimcp_fast_msg_init(msg, MSG_TYPE_SYNC, 0);

    uint8_t* p = msg->payload;

    /* Brain ID (big-endian) */
    p[0] = (brain_id >> 24) & 0xFF;
    p[1] = (brain_id >> 16) & 0xFF;
    p[2] = (brain_id >> 8) & 0xFF;
    p[3] = brain_id & 0xFF;

    /* Gamma power (IEEE 754 float, big-endian) */
    uint32_t power_bits;
    memcpy(&power_bits, &gamma_power, sizeof(float));
    p[4] = (power_bits >> 24) & 0xFF;
    p[5] = (power_bits >> 16) & 0xFF;
    p[6] = (power_bits >> 8) & 0xFF;
    p[7] = power_bits & 0xFF;

    /* Gamma phase (IEEE 754 float, big-endian) */
    uint32_t phase_bits;
    memcpy(&phase_bits, &gamma_phase, sizeof(float));
    p[8] = (phase_bits >> 24) & 0xFF;
    p[9] = (phase_bits >> 16) & 0xFF;
    p[10] = (phase_bits >> 8) & 0xFF;
    p[11] = phase_bits & 0xFF;

    /* Padding */
    p[12] = 0;
    p[13] = 0;
    p[14] = 0;
    p[15] = 0;
}

void nimcp_fast_msg_danger(
    nimcp_fast_msg_t* msg,
    uint32_t brain_id,
    uint32_t danger_type,
    float severity
) {
    if (!msg) return;

    nimcp_fast_msg_init(msg, MSG_TYPE_DANGER, MSG_FLAG_BROADCAST);

    uint8_t* p = msg->payload;

    /* Brain ID (big-endian) */
    p[0] = (brain_id >> 24) & 0xFF;
    p[1] = (brain_id >> 16) & 0xFF;
    p[2] = (brain_id >> 8) & 0xFF;
    p[3] = brain_id & 0xFF;

    /* Danger type (big-endian) */
    p[4] = (danger_type >> 24) & 0xFF;
    p[5] = (danger_type >> 16) & 0xFF;
    p[6] = (danger_type >> 8) & 0xFF;
    p[7] = danger_type & 0xFF;

    /* Severity (IEEE 754 float, big-endian) */
    uint32_t sev_bits;
    memcpy(&sev_bits, &severity, sizeof(float));
    p[8] = (sev_bits >> 24) & 0xFF;
    p[9] = (sev_bits >> 16) & 0xFF;
    p[10] = (sev_bits >> 8) & 0xFF;
    p[11] = sev_bits & 0xFF;

    /* Padding */
    p[12] = 0;
    p[13] = 0;
    p[14] = 0;
    p[15] = 0;
}

void nimcp_fast_msg_ack(
    nimcp_fast_msg_t* msg,
    uint32_t brain_id,
    uint32_t acked_seq
) {
    if (!msg) return;

    nimcp_fast_msg_init(msg, MSG_TYPE_ACK, 0);

    uint8_t* p = msg->payload;

    /* Brain ID (big-endian) */
    p[0] = (brain_id >> 24) & 0xFF;
    p[1] = (brain_id >> 16) & 0xFF;
    p[2] = (brain_id >> 8) & 0xFF;
    p[3] = brain_id & 0xFF;

    /* Acked sequence (big-endian) */
    p[4] = (acked_seq >> 24) & 0xFF;
    p[5] = (acked_seq >> 16) & 0xFF;
    p[6] = (acked_seq >> 8) & 0xFF;
    p[7] = acked_seq & 0xFF;

    /* Padding */
    memset(&p[8], 0, 8);
}

/*=============================================================================
 * Buffer API Implementation
 *===========================================================================*/

void nimcp_msg_buffer_init(
    nimcp_msg_buffer_t* buffer,
    uint8_t* payload_buf,
    size_t payload_capacity
) {
    if (!buffer) return;

    memset(&buffer->header, 0, sizeof(buffer->header));
    buffer->payload = payload_buf;
    buffer->payload_capacity = payload_capacity;
    buffer->payload_size = 0;
}

void nimcp_msg_buffer_reset(nimcp_msg_buffer_t* buffer) {
    if (!buffer) return;

    memset(&buffer->header, 0, sizeof(buffer->header));
    buffer->payload_size = 0;
}

/*=============================================================================
 * Serialization API Implementation
 *===========================================================================*/

size_t nimcp_msg_header_serialize(
    const nimcp_msg_header_t* header,
    uint8_t* out
) {
    if (!header || !out) return 0;

    /* Magic bytes */
    out[0] = header->magic[0];
    out[1] = header->magic[1];

    /* Version and flags */
    out[2] = header->version;
    out[3] = header->flags;

    /* Message type (big-endian) */
    out[4] = (header->msg_type >> 8) & 0xFF;
    out[5] = header->msg_type & 0xFF;

    /* Payload length (big-endian) */
    out[6] = (header->payload_len >> 8) & 0xFF;
    out[7] = header->payload_len & 0xFF;

    atomic_fetch_add(&g_bytes_sent, NIMCP_MSG_HEADER_SIZE);

    return NIMCP_MSG_HEADER_SIZE;
}

int nimcp_msg_header_deserialize(
    const uint8_t* data,
    nimcp_msg_header_t* header
) {
    if (!data || !header) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_msg_header_deserialize: required parameter is NULL (data, header)");
        return -1;
    }

    /* Magic bytes */
    header->magic[0] = data[0];
    header->magic[1] = data[1];

    /* Version and flags */
    header->version = data[2];
    header->flags = data[3];

    /* Message type (big-endian) */
    header->msg_type = ((uint16_t)data[4] << 8) | data[5];

    /* Payload length (big-endian) */
    header->payload_len = ((uint16_t)data[6] << 8) | data[7];

    /* Validate */
    if (!nimcp_msg_header_validate(header)) {
        atomic_fetch_add(&g_parse_errors, 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_msg_header_deserialize: nimcp_msg_header_validate is NULL");
        return -1;
    }

    atomic_fetch_add(&g_bytes_received, NIMCP_MSG_HEADER_SIZE);

    return 0;
}

size_t nimcp_fast_msg_serialize(
    const nimcp_fast_msg_t* msg,
    uint8_t* out
) {
    if (!msg || !out) return 0;

    /* Serialize header */
    nimcp_msg_header_serialize(&msg->header, out);

    /* Copy payload */
    memcpy(out + NIMCP_MSG_HEADER_SIZE, msg->payload, NIMCP_MSG_FAST_PAYLOAD);

    atomic_fetch_add(&g_messages_sent, 1);
    atomic_fetch_add(&g_fast_messages, 1);
    atomic_fetch_add(&g_bytes_sent, NIMCP_MSG_FAST_PAYLOAD);

    return NIMCP_MSG_HEADER_SIZE + NIMCP_MSG_FAST_PAYLOAD;
}

int nimcp_fast_msg_deserialize(
    const uint8_t* data,
    nimcp_fast_msg_t* msg
) {
    if (!data || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_fast_msg_deserialize: required parameter is NULL (data, msg)");
        return -1;
    }

    /* Deserialize header */
    if (nimcp_msg_header_deserialize(data, &msg->header) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_fast_msg_deserialize: validation failed");
        return -1;
    }

    /* Verify it's a fast path message */
    if (!nimcp_msg_is_fast_path(&msg->header)) {
        atomic_fetch_add(&g_parse_errors, 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_fast_msg_deserialize: nimcp_msg_is_fast_path is NULL");
        return -1;
    }

    /* Copy payload */
    memcpy(msg->payload, data + NIMCP_MSG_HEADER_SIZE, NIMCP_MSG_FAST_PAYLOAD);

    atomic_fetch_add(&g_messages_received, 1);
    atomic_fetch_add(&g_fast_messages, 1);
    atomic_fetch_add(&g_bytes_received, NIMCP_MSG_FAST_PAYLOAD);

    return 0;
}

/*=============================================================================
 * Statistics API Implementation
 *===========================================================================*/

void nimcp_msg_get_stats(nimcp_msg_stats_t* stats) {
    if (!stats) return;
    stats->messages_sent = atomic_load(&g_messages_sent);
    stats->messages_received = atomic_load(&g_messages_received);
    stats->fast_messages = atomic_load(&g_fast_messages);
    stats->protobuf_messages = atomic_load(&g_protobuf_messages);
    stats->compressed_messages = atomic_load(&g_compressed_messages);
    stats->encrypted_messages = atomic_load(&g_encrypted_messages);
    stats->parse_errors = atomic_load(&g_parse_errors);
    stats->checksum_errors = atomic_load(&g_checksum_errors);
    stats->bytes_sent = atomic_load(&g_bytes_sent);
    stats->bytes_received = atomic_load(&g_bytes_received);
}

void nimcp_msg_reset_stats(void) {
    atomic_store(&g_messages_sent, 0);
    atomic_store(&g_messages_received, 0);
    atomic_store(&g_fast_messages, 0);
    atomic_store(&g_protobuf_messages, 0);
    atomic_store(&g_compressed_messages, 0);
    atomic_store(&g_encrypted_messages, 0);
    atomic_store(&g_parse_errors, 0);
    atomic_store(&g_checksum_errors, 0);
    atomic_store(&g_bytes_sent, 0);
    atomic_store(&g_bytes_received, 0);
}

/*=============================================================================
 * Utility API Implementation
 *===========================================================================*/

const char* nimcp_msg_type_name(nimcp_msg_type_t msg_type) {
    switch (msg_type) {
        /* Fast path */
        case MSG_TYPE_HEARTBEAT:        return "HEARTBEAT";
        case MSG_TYPE_SYNC:             return "SYNC";
        case MSG_TYPE_DANGER:           return "DANGER";
        case MSG_TYPE_PING:             return "PING";
        case MSG_TYPE_PONG:             return "PONG";
        case MSG_TYPE_ACK:              return "ACK";
        case MSG_TYPE_NACK:             return "NACK";

        /* Hyperscanning */
        case MSG_TYPE_NEURAL_STATE:     return "NEURAL_STATE";
        case MSG_TYPE_HYPERSCAN_METRICS: return "HYPERSCAN_METRICS";
        case MSG_TYPE_ENTRAINMENT_REQ:  return "ENTRAINMENT_REQ";
        case MSG_TYPE_ENTRAINMENT_RESP: return "ENTRAINMENT_RESP";
        case MSG_TYPE_GLOBAL_SYNC:      return "GLOBAL_SYNC";

        /* Shared intentionality */
        case MSG_TYPE_SHARED_GOAL:      return "SHARED_GOAL";
        case MSG_TYPE_JOINT_ATTENTION:  return "JOINT_ATTENTION";
        case MSG_TYPE_COMMITMENT:       return "COMMITMENT";
        case MSG_TYPE_WE_MODE:          return "WE_MODE";
        case MSG_TYPE_INTENTION:        return "INTENTION";
        case MSG_TYPE_ROLE_REQUEST:     return "ROLE_REQUEST";
        case MSG_TYPE_ROLE_RESPONSE:    return "ROLE_RESPONSE";

        /* Beliefs */
        case MSG_TYPE_BELIEF_GOSSIP:    return "BELIEF_GOSSIP";
        case MSG_TYPE_CONTRADICTION:    return "CONTRADICTION";
        case MSG_TYPE_CONSENSUS_REQ:    return "CONSENSUS_REQ";
        case MSG_TYPE_CONSENSUS_VOTE:   return "CONSENSUS_VOTE";
        case MSG_TYPE_CONSENSUS_RESULT: return "CONSENSUS_RESULT";
        case MSG_TYPE_KNOWLEDGE_QUERY:  return "KNOWLEDGE_QUERY";
        case MSG_TYPE_KNOWLEDGE_RESP:   return "KNOWLEDGE_RESP";
        case MSG_TYPE_CREDIBILITY:      return "CREDIBILITY";

        /* Extended mind */
        case MSG_TYPE_EXT_QUERY:        return "EXT_QUERY";
        case MSG_TYPE_EXT_RESPONSE:     return "EXT_RESPONSE";
        case MSG_TYPE_EXT_STATUS:       return "EXT_STATUS";
        case MSG_TYPE_EXT_REGISTER:     return "EXT_REGISTER";
        case MSG_TYPE_OFFLOAD:          return "OFFLOAD";
        case MSG_TYPE_OFFLOAD_RESULT:   return "OFFLOAD_RESULT";
        case MSG_TYPE_EXT_MIND_STATE:   return "EXT_MIND_STATE";
        case MSG_TYPE_EXT_TRUST:        return "EXT_TRUST";

        /* Phi/consciousness */
        case MSG_TYPE_PHI_UPDATE:       return "PHI_UPDATE";
        case MSG_TYPE_PHI_CONTRIBUTION: return "PHI_CONTRIBUTION";
        case MSG_TYPE_PHI_REQUEST:      return "PHI_REQUEST";
        case MSG_TYPE_INFO_FLOW:        return "INFO_FLOW";
        case MSG_TYPE_QUALIA:           return "QUALIA";
        case MSG_TYPE_EMERGENCE:        return "EMERGENCE";
        case MSG_TYPE_TOPOLOGY:         return "TOPOLOGY";

        /* Control */
        case MSG_TYPE_REGISTRATION:     return "REGISTRATION";
        case MSG_TYPE_DEREGISTRATION:   return "DEREGISTRATION";
        case MSG_TYPE_ERROR:            return "ERROR";

        default:                        return "UNKNOWN";
    }
}

const char* nimcp_msg_type_category(nimcp_msg_type_t msg_type) {
    uint16_t category = msg_type & 0xFF00;

    switch (category) {
        case 0x0000: return "fast";
        case 0x0100: return "hyperscanning";
        case 0x0200: return "intentionality";
        case 0x0300: return "beliefs";
        case 0x0400: return "extended_mind";
        case 0x0500: return "consciousness";
        case 0x0600: return "control";
        default:     return "unknown";
    }
}
