/**
 * @file nimcp_msg_framing.h
 * @brief Message framing for hybrid collective cognition protocol
 *
 * WHAT: Universal message framing for inter-brain communication
 * WHY: Support both fast 24-byte messages and rich protobuf messages
 * HOW: 8-byte header + variable payload with format flags
 *
 * PROTOCOL DESIGN:
 * - Layer 1: Transport (TCP/UDP/SHM/LoRa)
 * - Layer 2: Fast messages (24-byte phoneme format, ~10ns parse)
 * - Layer 3: Rich messages (protobuf via nanopb, ~1-10us parse)
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#ifndef NIMCP_MSG_FRAMING_H
#define NIMCP_MSG_FRAMING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Magic bytes for NIMCP protocol ("NI") */
#define NIMCP_MSG_MAGIC_0           0x4E
#define NIMCP_MSG_MAGIC_1           0x49

/** Current protocol version */
#define NIMCP_MSG_VERSION           1

/** Maximum payload size (64KB) */
#define NIMCP_MSG_MAX_PAYLOAD       65535

/** Fast message payload size (legacy 24-byte format) */
#define NIMCP_MSG_FAST_PAYLOAD      16

/** Header size */
#define NIMCP_MSG_HEADER_SIZE       8

/*=============================================================================
 * Message Flags
 *===========================================================================*/

/** Message format flags */
#define MSG_FLAG_FAST_PATH      0x00  /**< Legacy 24-byte phoneme format */
#define MSG_FLAG_PROTOBUF       0x01  /**< Payload is protobuf-encoded */
#define MSG_FLAG_COMPRESSED     0x02  /**< Payload is LZ4-compressed */
#define MSG_FLAG_ENCRYPTED      0x04  /**< Payload is AES-256 encrypted */
#define MSG_FLAG_RELIABLE       0x08  /**< Requires ACK */
#define MSG_FLAG_BROADCAST      0x10  /**< Send to all peers */
#define MSG_FLAG_FRAGMENT       0x20  /**< Message is fragmented */
#define MSG_FLAG_LAST_FRAGMENT  0x40  /**< Last fragment of message */

/*=============================================================================
 * Message Types
 *===========================================================================*/

/**
 * @brief Message type categories
 */
typedef enum {
    /* Fast path messages (0x0000 - 0x00FF) */
    MSG_TYPE_HEARTBEAT          = 0x0001,
    MSG_TYPE_SYNC               = 0x0002,
    MSG_TYPE_DANGER             = 0x0003,
    MSG_TYPE_PING               = 0x0004,
    MSG_TYPE_PONG               = 0x0005,
    MSG_TYPE_ACK                = 0x0006,
    MSG_TYPE_NACK               = 0x0007,

    /* Hyperscanning messages (0x0100 - 0x01FF) */
    MSG_TYPE_NEURAL_STATE       = 0x0100,
    MSG_TYPE_HYPERSCAN_METRICS  = 0x0101,
    MSG_TYPE_ENTRAINMENT_REQ    = 0x0102,
    MSG_TYPE_ENTRAINMENT_RESP   = 0x0103,
    MSG_TYPE_GLOBAL_SYNC        = 0x0104,

    /* Shared intentionality messages (0x0200 - 0x02FF) */
    MSG_TYPE_SHARED_GOAL        = 0x0200,
    MSG_TYPE_JOINT_ATTENTION    = 0x0201,
    MSG_TYPE_COMMITMENT         = 0x0202,
    MSG_TYPE_WE_MODE            = 0x0203,
    MSG_TYPE_INTENTION          = 0x0204,
    MSG_TYPE_ROLE_REQUEST       = 0x0205,
    MSG_TYPE_ROLE_RESPONSE      = 0x0206,

    /* Belief messages (0x0300 - 0x03FF) */
    MSG_TYPE_BELIEF_GOSSIP      = 0x0300,
    MSG_TYPE_CONTRADICTION      = 0x0301,
    MSG_TYPE_CONSENSUS_REQ      = 0x0302,
    MSG_TYPE_CONSENSUS_VOTE     = 0x0303,
    MSG_TYPE_CONSENSUS_RESULT   = 0x0304,
    MSG_TYPE_KNOWLEDGE_QUERY    = 0x0305,
    MSG_TYPE_KNOWLEDGE_RESP     = 0x0306,
    MSG_TYPE_CREDIBILITY        = 0x0307,

    /* Extended mind messages (0x0400 - 0x04FF) */
    MSG_TYPE_EXT_QUERY          = 0x0400,
    MSG_TYPE_EXT_RESPONSE       = 0x0401,
    MSG_TYPE_EXT_STATUS         = 0x0402,
    MSG_TYPE_EXT_REGISTER       = 0x0403,
    MSG_TYPE_OFFLOAD            = 0x0404,
    MSG_TYPE_OFFLOAD_RESULT     = 0x0405,
    MSG_TYPE_EXT_MIND_STATE     = 0x0406,
    MSG_TYPE_EXT_TRUST          = 0x0407,

    /* Phi/consciousness messages (0x0500 - 0x05FF) */
    MSG_TYPE_PHI_UPDATE         = 0x0500,
    MSG_TYPE_PHI_CONTRIBUTION   = 0x0501,
    MSG_TYPE_PHI_REQUEST        = 0x0502,
    MSG_TYPE_INFO_FLOW          = 0x0503,
    MSG_TYPE_QUALIA             = 0x0504,
    MSG_TYPE_EMERGENCE          = 0x0505,
    MSG_TYPE_TOPOLOGY           = 0x0506,

    /* Control messages (0x0600 - 0x06FF) */
    MSG_TYPE_REGISTRATION       = 0x0600,
    MSG_TYPE_DEREGISTRATION     = 0x0601,
    MSG_TYPE_ERROR              = 0x0602
} nimcp_msg_type_t;

/*=============================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Universal message header (8 bytes)
 *
 * All messages start with this header, regardless of format.
 */
typedef struct __attribute__((packed)) {
    uint8_t  magic[2];        /**< "NI" (0x4E, 0x49) */
    uint8_t  version;         /**< Protocol version */
    uint8_t  flags;           /**< Format flags (MSG_FLAG_*) */
    uint16_t msg_type;        /**< Message type (nimcp_msg_type_t) */
    uint16_t payload_len;     /**< Payload length in bytes */
} nimcp_msg_header_t;

/**
 * @brief Fast path message (24 bytes total)
 *
 * Used for latency-critical communication (heartbeats, sync, alerts)
 */
typedef struct __attribute__((packed)) {
    nimcp_msg_header_t header;  /**< 8-byte header */
    uint8_t payload[16];        /**< 16-byte payload */
} nimcp_fast_msg_t;

/**
 * @brief Message buffer for receiving
 */
typedef struct {
    nimcp_msg_header_t header;
    uint8_t* payload;           /**< Pointer to payload buffer */
    size_t payload_capacity;    /**< Capacity of payload buffer */
    size_t payload_size;        /**< Actual payload size */
} nimcp_msg_buffer_t;

/**
 * @brief Message framing statistics
 */
typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t fast_messages;
    uint64_t protobuf_messages;
    uint64_t compressed_messages;
    uint64_t encrypted_messages;
    uint64_t parse_errors;
    uint64_t checksum_errors;
    uint64_t bytes_sent;
    uint64_t bytes_received;
} nimcp_msg_stats_t;

/*=============================================================================
 * Header API
 *===========================================================================*/

/**
 * @brief Initialize a message header
 *
 * @param header Header to initialize
 * @param msg_type Message type
 * @param flags Format flags
 * @param payload_len Payload length
 */
void nimcp_msg_header_init(
    nimcp_msg_header_t* header,
    nimcp_msg_type_t msg_type,
    uint8_t flags,
    uint16_t payload_len
);

/**
 * @brief Validate a message header
 *
 * Checks magic bytes, version, and payload length.
 *
 * @param header Header to validate
 * @return true if valid, false otherwise
 */
bool nimcp_msg_header_validate(const nimcp_msg_header_t* header);

/**
 * @brief Check if message is fast path (24-byte format)
 *
 * @param header Message header
 * @return true if fast path, false if protobuf
 */
static inline bool nimcp_msg_is_fast_path(const nimcp_msg_header_t* header) {
    return (header->flags & MSG_FLAG_PROTOBUF) == 0;
}

/**
 * @brief Check if message is protobuf-encoded
 *
 * @param header Message header
 * @return true if protobuf, false if fast path
 */
static inline bool nimcp_msg_is_protobuf(const nimcp_msg_header_t* header) {
    return (header->flags & MSG_FLAG_PROTOBUF) != 0;
}

/**
 * @brief Check if message is compressed
 *
 * @param header Message header
 * @return true if compressed
 */
static inline bool nimcp_msg_is_compressed(const nimcp_msg_header_t* header) {
    return (header->flags & MSG_FLAG_COMPRESSED) != 0;
}

/**
 * @brief Check if message is encrypted
 *
 * @param header Message header
 * @return true if encrypted
 */
static inline bool nimcp_msg_is_encrypted(const nimcp_msg_header_t* header) {
    return (header->flags & MSG_FLAG_ENCRYPTED) != 0;
}

/**
 * @brief Check if message requires ACK
 *
 * @param header Message header
 * @return true if reliable delivery required
 */
static inline bool nimcp_msg_is_reliable(const nimcp_msg_header_t* header) {
    return (header->flags & MSG_FLAG_RELIABLE) != 0;
}

/**
 * @brief Check if message is broadcast
 *
 * @param header Message header
 * @return true if broadcast to all peers
 */
static inline bool nimcp_msg_is_broadcast(const nimcp_msg_header_t* header) {
    return (header->flags & MSG_FLAG_BROADCAST) != 0;
}

/*=============================================================================
 * Fast Message API
 *===========================================================================*/

/**
 * @brief Initialize a fast path message
 *
 * @param msg Message to initialize
 * @param msg_type Message type
 * @param flags Additional flags (MSG_FLAG_PROTOBUF will be cleared)
 */
void nimcp_fast_msg_init(
    nimcp_fast_msg_t* msg,
    nimcp_msg_type_t msg_type,
    uint8_t flags
);

/**
 * @brief Create heartbeat message
 *
 * @param msg Message to fill
 * @param brain_id Sender brain ID
 * @param atp_level Current ATP level [0-1]
 * @param load Current cognitive load [0-1]
 */
void nimcp_fast_msg_heartbeat(
    nimcp_fast_msg_t* msg,
    uint32_t brain_id,
    float atp_level,
    float load
);

/**
 * @brief Create sync message (for hyperscanning fast path)
 *
 * @param msg Message to fill
 * @param brain_id Sender brain ID
 * @param gamma_power Gamma band power [0-1]
 * @param gamma_phase Gamma phase [0-2pi]
 */
void nimcp_fast_msg_sync(
    nimcp_fast_msg_t* msg,
    uint32_t brain_id,
    float gamma_power,
    float gamma_phase
);

/**
 * @brief Create danger alert message
 *
 * @param msg Message to fill
 * @param brain_id Sender brain ID
 * @param danger_type Type of danger (application-defined)
 * @param severity Severity [0-1]
 */
void nimcp_fast_msg_danger(
    nimcp_fast_msg_t* msg,
    uint32_t brain_id,
    uint32_t danger_type,
    float severity
);

/**
 * @brief Create ACK message
 *
 * @param msg Message to fill
 * @param brain_id Sender brain ID
 * @param acked_seq Sequence number being acknowledged
 */
void nimcp_fast_msg_ack(
    nimcp_fast_msg_t* msg,
    uint32_t brain_id,
    uint32_t acked_seq
);

/*=============================================================================
 * Buffer API
 *===========================================================================*/

/**
 * @brief Initialize a message buffer
 *
 * @param buffer Buffer to initialize
 * @param payload_buf Pointer to payload storage
 * @param payload_capacity Capacity of payload storage
 */
void nimcp_msg_buffer_init(
    nimcp_msg_buffer_t* buffer,
    uint8_t* payload_buf,
    size_t payload_capacity
);

/**
 * @brief Reset buffer for new message
 *
 * @param buffer Buffer to reset
 */
void nimcp_msg_buffer_reset(nimcp_msg_buffer_t* buffer);

/**
 * @brief Get total message size (header + payload)
 *
 * @param buffer Message buffer
 * @return Total size in bytes
 */
static inline size_t nimcp_msg_buffer_total_size(const nimcp_msg_buffer_t* buffer) {
    return NIMCP_MSG_HEADER_SIZE + buffer->payload_size;
}

/*=============================================================================
 * Serialization API
 *===========================================================================*/

/**
 * @brief Serialize header to bytes (big-endian)
 *
 * @param header Header to serialize
 * @param out Output buffer (must be >= NIMCP_MSG_HEADER_SIZE)
 * @return Number of bytes written (always NIMCP_MSG_HEADER_SIZE)
 */
size_t nimcp_msg_header_serialize(
    const nimcp_msg_header_t* header,
    uint8_t* out
);

/**
 * @brief Deserialize header from bytes (big-endian)
 *
 * @param data Input buffer (must be >= NIMCP_MSG_HEADER_SIZE)
 * @param header Output header
 * @return 0 on success, -1 on error
 */
int nimcp_msg_header_deserialize(
    const uint8_t* data,
    nimcp_msg_header_t* header
);

/**
 * @brief Serialize fast message to bytes
 *
 * @param msg Fast message to serialize
 * @param out Output buffer (must be >= 24 bytes)
 * @return Number of bytes written (always 24)
 */
size_t nimcp_fast_msg_serialize(
    const nimcp_fast_msg_t* msg,
    uint8_t* out
);

/**
 * @brief Deserialize fast message from bytes
 *
 * @param data Input buffer (must be >= 24 bytes)
 * @param msg Output message
 * @return 0 on success, -1 on error
 */
int nimcp_fast_msg_deserialize(
    const uint8_t* data,
    nimcp_fast_msg_t* msg
);

/*=============================================================================
 * Statistics API
 *===========================================================================*/

/**
 * @brief Get global framing statistics
 *
 * @param stats Output statistics
 */
void nimcp_msg_get_stats(nimcp_msg_stats_t* stats);

/**
 * @brief Reset framing statistics
 */
void nimcp_msg_reset_stats(void);

/*=============================================================================
 * Utility API
 *===========================================================================*/

/**
 * @brief Get human-readable name for message type
 *
 * @param msg_type Message type
 * @return Type name string
 */
const char* nimcp_msg_type_name(nimcp_msg_type_t msg_type);

/**
 * @brief Get message type category
 *
 * @param msg_type Message type
 * @return Category string ("fast", "hyperscanning", "intentionality", etc.)
 */
const char* nimcp_msg_type_category(nimcp_msg_type_t msg_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MSG_FRAMING_H */
