// nimcp_protocol.h

#ifndef NIMCP_PROTOCOL_H
#define NIMCP_PROTOCOL_H

#include <Python.h>  // Use CMake-provided Python includes
#include "common/nimcp_export.h"

/**
 * @file nimcp_protocol.h
 * @brief Protocol definition for NIMCP 2.0 (Neuro-Inspired Message Communication Protocol)
 *
 * This module implements the NIMCP 2.0 specification with:
 * - Event Packets for high-frequency neural spikes
 * - Control Messages for configuration and management
 * - Feature code system for semantic routing
 * - Subscription-based filtering
 * - Confidence-weighted propagation
 *
 * @author Braun Brelin
 * @date 2025-02-04
 * @version 2.0
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// NIMCP 2.0 Protocol Constants
//=============================================================================

/**
 * @brief Protocol version number (NIMCP 2.0)
 */
#define PROTOCOL_VERSION 2

/**
 * @brief Magic number for message validation
 * ASCII representation of "NIMC" used to identify valid protocol messages
 */
#define PROTOCOL_MAGIC 0x4E494D43

/**
 * @brief Maximum message payload size
 * Defines the upper limit for message payload size in bytes
 */
#define MAX_PAYLOAD_SIZE 65536

/**
 * @brief Message types for node communication
 *
 * Enumeration of all possible message types that can be exchanged between nodes.
 * Each type serves a specific purpose in the protocol.
 */
typedef enum {
    MSG_TYPE_HELLO,
    MSG_TYPE_HANDSHAKE,    /**< Initial connection handshake */
    MSG_TYPE_STATE_UPDATE, /**< Neuron state update */
    MSG_TYPE_PING,         /**< Health check ping */
    MSG_TYPE_PONG,         /**< Health check response */
    MSG_TYPE_DISCONNECT,   /**< Graceful disconnect notice */
    MSG_TYPE_MAX
} msg_type_t;

/**
 * @brief Header structure for all messages
 *
 * Common header format that precedes all message payloads.
 * Contains metadata about the message including type and size information.
 */
typedef struct {
    uint32_t magic;    /**< Magic number for validation */
    uint8_t version;   /**< Protocol version */
    msg_type_t type;   /**< Message type */
    uint32_t length;   /**< Length of payload */
    uint32_t sequence; /**< Message sequence number */
    uint32_t checksum; /**< Message checksum */
} msg_header_t;

/**
 * @brief Handshake message payload structure
 *
 * Contains information exchanged during initial connection setup
 * between two nodes.
 */
typedef struct {
    uint32_t node_id;     /**< Unique identifier of sending node */
    uint16_t listen_port; /**< Port the sender is listening on */
    uint8_t capabilities; /**< Bitmap of supported features */
} handshake_payload_t;

/**
 * @brief State update message payload structure
 *
 * Contains neural state information to be propagated through
 * the network.
 */
typedef struct {
    uint32_t neuron_id; /**< ID of the neuron being updated */
    float state_value;  /**< New state value */
    uint64_t timestamp; /**< Update timestamp */
} state_update_payload_t;

/**
 * @brief Serializes a message into a buffer
 *
 * @param type Message type to be serialized
 * @param payload Pointer to payload data
 * @param payload_len Length of payload in bytes
 * @param buffer Output buffer for serialized message
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
int protocol_serialize_message(msg_type_t type, const void* payload, uint32_t payload_len,
                               uint8_t* buffer, uint32_t buffer_size);

/**
 * @brief Deserializes a message from a buffer
 *
 * @param buffer Input buffer containing serialized message
 * @param buffer_size Size of input buffer
 * @param header Pointer to store deserialized header
 * @param payload Buffer to store deserialized payload
 * @param payload_size Size of payload buffer
 * @return Number of bytes read, or -1 on error
 */
int protocol_deserialize_message(const uint8_t* buffer, uint32_t buffer_size, msg_header_t* header,
                                 void* payload, uint32_t payload_size);

/**
 * @brief Validates a message header
 *
 * @param header Pointer to header structure to validate
 * @return true if header is valid, false otherwise
 */
bool protocol_validate_header(const msg_header_t* header);

/**
 * @brief Calculates checksum for a message
 *
 * @param header Pointer to message header
 * @param payload Pointer to message payload
 * @param payload_len Length of payload in bytes
 * @return Calculated checksum value
 */
uint32_t protocol_calculate_checksum(const msg_header_t* header, const void* payload,
                                     uint32_t payload_len);

//=============================================================================
// NIMCP 2.0: Feature Code System
//=============================================================================

/**
 * @brief Feature code domains (high 8 bits of 24-bit feature code)
 */
typedef enum {
    FEATURE_DOMAIN_SYSTEM = 0x00,      /**< System/Control */
    FEATURE_DOMAIN_VISION = 0x10,      /**< Vision processing */
    FEATURE_DOMAIN_AUDITORY = 0x20,    /**< Auditory processing */
    FEATURE_DOMAIN_LANGUAGE = 0x30,    /**< Language processing */
    FEATURE_DOMAIN_MOTOR = 0x40,       /**< Motor control */
    FEATURE_DOMAIN_MEMORY = 0x50,      /**< Memory operations */
    FEATURE_DOMAIN_EMOTION = 0x60,     /**< Emotional processing */
    FEATURE_DOMAIN_ETHICS = 0x70,      /**< Ethical regulation */
    FEATURE_DOMAIN_USER_MIN = 0x80,    /**< User-defined domains start */
    FEATURE_DOMAIN_NEUROMOD = 0x90,    /**< Neuromodulator signaling (dopamine, serotonin, etc.) */
    FEATURE_DOMAIN_GLIAL = 0xA0,       /**< Glial cell coordination (astrocytes, microglia) */
    FEATURE_DOMAIN_BRAIN_REGION = 0xB0,/**< Brain region synchronization and state */
    FEATURE_DOMAIN_USER_MAX = 0xFF     /**< User-defined domains end */
} feature_domain_t;

/**
 * @brief Feature code type (32-bit hierarchical namespace)
 */
typedef uint32_t feature_code_t;

/**
 * @brief Create a feature code from domain and sub-feature
 * Domain is in bits 24-31, subfeature in bits 0-23
 */
#define MAKE_FEATURE_CODE(domain, subfeature) \
    ((feature_code_t) (((domain) << 24) | ((subfeature) &0xFFFFFF)))

/**
 * @brief Extract domain from feature code (bits 24-31)
 */
#define GET_FEATURE_DOMAIN(code) ((uint8_t) (((code) >> 24) & 0xFF))

/**
 * @brief Extract sub-feature from feature code (bits 0-23)
 */
#define GET_FEATURE_SUBCODE(code) ((uint32_t) ((code) &0xFFFFFF))

//=============================================================================
// NIMCP 2.0: Event Packet (High-Frequency Neural Spikes)
//=============================================================================

/**
 * @brief Event packet flags
 */
#define EVENT_FLAG_EXCITATORY (1 << 0) /**< E: Excitatory spike */
#define EVENT_FLAG_INHIBITORY (1 << 1) /**< I: Inhibitory spike */
#define EVENT_FLAG_PLASTICITY (1 << 2) /**< P: Plasticity trigger included */
#define EVENT_FLAG_ROUTE_REC (1 << 3)  /**< R: Route recording requested */

/**
 * @brief Event Packet structure (optimized for high-frequency transmission)
 *
 * Wire format (24 bytes minimum):
 * - 1 byte:  Version (4 bits) + Flags (4 bits)
 * - 1 byte:  Reserved
 * - 4 bytes: Feature Code (32 bits full)
 * - 4 bytes: Source Node ID
 * - 8 bytes: Timestamp (microseconds)
 * - 2 bytes: Confidence (0-65535 maps to 0.0-1.0)
 * - 1 byte:  Hop Count
 * - 1 byte:  Reserved2
 * - N bytes: Optional Payload
 */
typedef struct __attribute__((packed)) {
    uint8_t version_flags;   /**< Version (4 bits) + Flags (4 bits) */
    uint8_t reserved;        /**< Reserved for alignment */
    uint16_t feature_high;   /**< Feature code high 16 bits */
    uint16_t feature_low;    /**< Feature code low 16 bits */
    uint32_t source_node_id; /**< Source node identifier */
    uint64_t timestamp;      /**< Timestamp in microseconds */
    uint16_t confidence;     /**< Confidence: 0-65535 maps to 0.0-1.0 */
    uint8_t hop_count;       /**< Hop count for TTL */
    uint8_t reserved2;       /**< Reserved for future use */
    uint32_t payload_length; /**< Length of optional payload */
    // Followed by optional payload data
} event_packet_t;

/**
 * @brief Get version from event packet
 */
#define EVENT_GET_VERSION(pkt) ((pkt)->version_flags >> 4)

/**
 * @brief Get flags from event packet
 */
#define EVENT_GET_FLAGS(pkt) ((pkt)->version_flags & 0x0F)

/**
 * @brief Set version in event packet
 */
#define EVENT_SET_VERSION(pkt, ver) \
    ((pkt)->version_flags = ((ver) << 4) | ((pkt)->version_flags & 0x0F))

/**
 * @brief Set flags in event packet
 */
#define EVENT_SET_FLAGS(pkt, flags) \
    ((pkt)->version_flags = ((pkt)->version_flags & 0xF0) | ((flags) &0x0F))

/**
 * @brief Get full feature code from event packet (32-bit)
 */
#define EVENT_GET_FEATURE_CODE(pkt) \
    ((feature_code_t) (((uint32_t) (pkt)->feature_high << 16) | (pkt)->feature_low))

/**
 * @brief Set feature code in event packet (32-bit)
 */
#define EVENT_SET_FEATURE_CODE(pkt, code)                           \
    do {                                                            \
        (pkt)->feature_high = (uint16_t) (((code) >> 16) & 0xFFFF); \
        (pkt)->feature_low = (uint16_t) ((code) &0xFFFF);           \
    } while (0)

/**
 * @brief Convert confidence to float (0.0-1.0)
 */
#define EVENT_CONFIDENCE_TO_FLOAT(conf) ((float) (conf) / 65535.0f)

/**
 * @brief Convert float to confidence (0-65535)
 */
#define EVENT_FLOAT_TO_CONFIDENCE(f) ((uint16_t) ((f) *65535.0f))

//=============================================================================
// NIMCP 2.0: Control Messages
//=============================================================================

/**
 * @brief Control message types
 */
typedef enum {
    CTRL_MSG_VERSION_NEGOTIATION = 0x01,
    CTRL_MSG_ADD_LINK = 0x02,
    CTRL_MSG_REMOVE_LINK = 0x03,
    CTRL_MSG_UPDATE_LINK = 0x04,
    CTRL_MSG_SET_SUBSCRIPTION = 0x05,
    CTRL_MSG_DEFINE_FEATURE_NS = 0x06,
    CTRL_MSG_SET_LEARNING_RATE = 0x07,
    CTRL_MSG_SET_PLASTICITY_RULE = 0x08,
    CTRL_MSG_CLUSTER_ANNOUNCE = 0x09,
    CTRL_MSG_ETHICS_POLICY = 0x0A,
    CTRL_MSG_ERROR_REPORT = 0x0B,
    CTRL_MSG_TOPOLOGY_QUERY = 0x0C,
    CTRL_MSG_HEARTBEAT = 0x0D,
    CTRL_MSG_PARTITION_DETECTED = 0x0E,
    CTRL_MSG_RECOVERY_SYNC = 0x0F,
    CTRL_MSG_NEUROMOD_LEVEL = 0x10,      /**< Set neuromodulator concentration level */
    CTRL_MSG_NEUROMOD_DIFFUSION = 0x11,  /**< Propagate neuromodulator diffusion */
    CTRL_MSG_GLIAL_PRUNING = 0x12,       /**< Coordinate synaptic pruning (microglia) */
    CTRL_MSG_GLIAL_CALCIUM = 0x13,       /**< Astrocyte calcium wave propagation */
    CTRL_MSG_REGION_SYNC = 0x14,         /**< Brain region state synchronization */
    CTRL_MSG_REGION_ACTIVITY = 0x15,     /**< Brain region activity statistics */
    CTRL_MSG_MAX
} control_msg_type_t;

/**
 * @brief Control message flags
 */
#define CTRL_FLAG_ACK_REQUIRED (1 << 0) /**< A: Acknowledgment required */
#define CTRL_FLAG_GLOBAL (1 << 1)       /**< G: Global broadcast */
#define CTRL_FLAG_SIGNED (1 << 2)       /**< S: Signed message */
#define CTRL_FLAG_RELAY (1 << 3)        /**< R: Relay to neighbors */

/**
 * @brief Control Message structure
 */
typedef struct __attribute__((packed)) {
    uint8_t version;           /**< Protocol version */
    uint8_t msg_type;          /**< Control message type */
    uint8_t flags;             /**< Message flags */
    uint8_t reserved;          /**< Reserved */
    uint32_t message_length;   /**< Total message length */
    uint32_t source_node_id;   /**< Source node ID */
    uint32_t target_specifier; /**< Target node/cluster/global */
    uint32_t sequence_number;  /**< Sequence number */
    uint16_t param_count;      /**< Number of parameters (TLV) */
    uint16_t reserved2;        /**< Reserved */
    // Followed by TLV-encoded parameters
} control_message_t;

//=============================================================================
// NIMCP 2.0: Neuromodulator and Glial Extensions
//=============================================================================

/**
 * @brief Neuromodulator level message payload
 *
 * Used with CTRL_MSG_NEUROMOD_LEVEL to synchronize neuromodulator
 * concentrations across distributed brain regions.
 *
 * NOTE: neuromod_type values map to neuromodulator_type_t enum from nimcp_neuromodulators.h:
 *       0 = NEUROMOD_DOPAMINE, 1 = NEUROMOD_SEROTONIN, 2 = NEUROMOD_ACETYLCHOLINE,
 *       3 = NEUROMOD_NOREPINEPHRINE, 4 = NEUROMOD_GABA, 5 = NEUROMOD_GLUTAMATE
 */
typedef struct __attribute__((packed)) {
    uint8_t neuromod_type;      /**< Neuromodulator type (0-5, maps to neuromodulator_type_t) */
    uint8_t reserved;           /**< Reserved for alignment */
    uint16_t region_id;         /**< Brain region ID */
    float concentration;        /**< Concentration level (0.0-1.0) */
    uint64_t timestamp;         /**< Timestamp (microseconds) */
} neuromod_level_payload_t;

/**
 * @brief Glial pruning coordination message payload
 *
 * Used with CTRL_MSG_GLIAL_PRUNING to coordinate synaptic pruning
 * decisions across microglia in distributed nodes.
 */
typedef struct __attribute__((packed)) {
    uint32_t source_neuron_id;  /**< Source neuron of synapse */
    uint32_t target_neuron_id;  /**< Target neuron of synapse */
    float activity_score;       /**< Activity score (0.0-1.0) */
    uint8_t pruning_action;     /**< 0=monitor, 1=prune, 2=preserve */
    uint8_t reserved[3];        /**< Reserved for alignment */
    uint64_t timestamp;         /**< Timestamp (microseconds) */
} glial_pruning_payload_t;

/**
 * @brief Astrocyte calcium wave message payload
 *
 * Used with CTRL_MSG_GLIAL_CALCIUM to propagate calcium waves
 * through astrocyte networks across nodes.
 */
typedef struct __attribute__((packed)) {
    uint32_t astrocyte_id;      /**< Source astrocyte ID */
    float calcium_level;        /**< Calcium concentration (0.0-1.0) */
    float wave_velocity;        /**< Propagation velocity (µm/s) */
    uint16_t affected_synapses; /**< Number of modulated synapses */
    uint16_t reserved;          /**< Reserved */
    uint64_t timestamp;         /**< Timestamp (microseconds) */
} glial_calcium_payload_t;

/**
 * @brief Brain region activity statistics payload
 *
 * Used with CTRL_MSG_REGION_ACTIVITY to share aggregate region
 * statistics for multi-node brain coordination.
 */
typedef struct __attribute__((packed)) {
    uint16_t region_type;       /**< Brain region type (brain_region_type_t) */
    uint16_t reserved;          /**< Reserved */
    float avg_activity;         /**< Average activity level (0.0-1.0) */
    float spike_rate;           /**< Average spike rate (Hz) */
    uint32_t active_neurons;    /**< Number of active neurons */
    uint32_t total_neurons;     /**< Total neurons in region */
    uint64_t timestamp;         /**< Timestamp (microseconds) */
} region_activity_payload_t;

//=============================================================================
// NIMCP 2.0: Subscription and Filtering
//=============================================================================

/**
 * @brief Subscription filter structure
 */
typedef struct {
    feature_code_t feature_code; /**< Feature code to subscribe to */
    uint32_t feature_mask;       /**< Mask for matching (0xFF0000 = domain only) */
    float confidence_threshold;  /**< Minimum confidence to accept */
    uint32_t max_rate_hz;        /**< Rate limiting (0 = unlimited) */
} subscription_filter_t;

/**
 * @brief Maximum subscriptions per node
 */
#define MAX_SUBSCRIPTIONS 256

//=============================================================================
// NIMCP 2.0: Function Declarations
//=============================================================================

// Event Packet Functions
int event_packet_serialize(const event_packet_t* packet, const void* payload, uint8_t* buffer,
                           uint32_t buffer_size);
int event_packet_deserialize(const uint8_t* buffer, uint32_t buffer_size, event_packet_t* packet,
                             void* payload, uint32_t payload_size);
bool event_packet_validate(const event_packet_t* packet);

// Control Message Functions
int control_message_serialize(const control_message_t* msg, const void* params, uint8_t* buffer,
                              uint32_t buffer_size);
int control_message_deserialize(const uint8_t* buffer, uint32_t buffer_size, control_message_t* msg,
                                void* params, uint32_t param_size);
bool control_message_validate(const control_message_t* msg);

// Feature Code Functions
const char* feature_domain_name(feature_domain_t domain);
bool feature_code_matches(feature_code_t code, feature_code_t filter, uint32_t mask);

// Subscription Functions
bool subscription_matches(const subscription_filter_t* filter, const event_packet_t* packet);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_PROTOCOL_H
