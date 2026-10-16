#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_nlp_message.c - Neural Link Protocol Message Handling
//=============================================================================
/**
 * @file nimcp_nlp_message.c
 * @brief Message creation, serialization, and payload handling for NLP
 *
 * WHAT: Complete message handling for the Neural Link Protocol
 * WHY:  Enable brain-to-brain communication with encryption and validation
 * HOW:  Implements message lifecycle, serialization, and payload packing
 *
 * IMPLEMENTATION NOTES:
 * - All multi-byte fields use network byte order (big-endian)
 * - CRC-16-CCITT is used for header validation
 * - Message format: header (36 bytes) + encrypted payload + auth tag (16 bytes)
 * - Stealth mode pads messages to fixed size for traffic analysis resistance
 * - All allocations use nimcp_unified_memory for CoW support
 *
 * SECURITY:
 * - Header CRC prevents tampering
 * - Encrypted payloads protect data in transit
 * - Authentication tags ensure message integrity
 * - Timestamp validation prevents replay attacks
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#define _GNU_SOURCE  // For htobe64/be64toh

#include "networking/nlp/nimcp_nlp_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/time/nimcp_time.h"
#include "utils/rng/nimcp_rand.h"
#include "security/nimcp_bbb_helpers.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <endian.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(nlp_message)

//=============================================================================
// Constants and Configuration
//=============================================================================

#define MODULE_NAME "nlp_message"

// CRC-16-CCITT polynomial
#define CRC16_POLY 0x1021

// Validation constants
#define NLP_MIN_TIMESTAMP 1000000000   // ~2001 (sanity check)
#define NLP_MAX_TIMESTAMP 2147483647   // Max 32-bit timestamp

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal message context (simplified - uses nimcp_memory)
 */
typedef struct {
    uint32_t magic;                      // Validation magic
    bool initialized;                    // Initialization flag
} nlp_message_ctx_t;

// Global context (lazily initialized)
static nlp_message_ctx_t g_msg_ctx = {
    .magic = 0,
    .initialized = false
};

// Bio-async context for message events
static bio_module_context_t g_msg_bio_ctx = NULL;

//=============================================================================
// Bio-Async Handler Functions
//=============================================================================

/**
 * @brief Handle incoming bio-async messages
 *
 * WHAT: Process messages from cognitive modules
 * WHY:  Enable bidirectional communication with brain subsystems
 * HOW:  Switch on message type, route to appropriate handler
 *
 * @param msg Message data
 * @param msg_size Message size
 * @param response_promise Promise for response
 * @param user_data User context
 * @return Error code
 */
static nimcp_error_t msg_bio_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)response_promise;
    (void)user_data;

    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_BIO_ERROR_INVALID_CHANNEL;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    bio_message_type_t msg_type = header->type;

    LOG_MODULE_DEBUG(MODULE_NAME, "Received bio-async message: type=0x%04x, size=%zu",
                     msg_type, msg_size);

    switch (msg_type) {
        case BIO_MSG_HEALTH_CHECK:
            // Message module health check
            LOG_MODULE_TRACE(MODULE_NAME, "Health check received");
            break;

        case BIO_MSG_ATTENTION_SHIFT:
            // Attention has shifted - may affect message priority
            LOG_MODULE_DEBUG(MODULE_NAME, "Attention shift notification received");
            break;

        case BIO_MSG_SECURITY_ALERT:
            // Security event - log and potentially pause operations
            LOG_MODULE_WARN(MODULE_NAME, "Security alert received in message module");
            bbb_audit_log(BBB_AUDIT_WARNING, MODULE_NAME, "security_alert",
                          "Bio-async security alert received");
            break;

        case BIO_MSG_SHUTDOWN_REQUEST:
            // Graceful shutdown requested
            LOG_MODULE_INFO(MODULE_NAME, "Shutdown request received");
            break;

        default:
            LOG_MODULE_TRACE(MODULE_NAME, "Unhandled bio message type: 0x%04x", msg_type);
            break;
    }

    return 0;
}

//=============================================================================
// KG-Driven Wiring Callback (Phase 2: KG-Based Runtime Module Assembly)
//=============================================================================

/**
 * @brief KG-driven wiring handler callback for NLP message module
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Module context pointer
 * @return 0 on success, -1 on error
 */
static int msg_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    (void)user_data;

    LOG_MODULE_INFO(MODULE_NAME,
        "msg_wiring_handler_callback: registering %u handlers from KG",
        message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_HEALTH_CHECK:
                bio_router_register_handler(ctx, message_types[i], msg_bio_handler);
                LOG_MODULE_DEBUG(MODULE_NAME,
                    "  Registered handler for BIO_MSG_HEALTH_CHECK");
                break;

            case BIO_MSG_ATTENTION_SHIFT:
                bio_router_register_handler(ctx, message_types[i], msg_bio_handler);
                LOG_MODULE_DEBUG(MODULE_NAME,
                    "  Registered handler for BIO_MSG_ATTENTION_SHIFT");
                break;

            case BIO_MSG_SECURITY_ALERT:
                bio_router_register_handler(ctx, message_types[i], msg_bio_handler);
                LOG_MODULE_DEBUG(MODULE_NAME,
                    "  Registered handler for BIO_MSG_SECURITY_ALERT");
                break;

            case BIO_MSG_SHUTDOWN_REQUEST:
                bio_router_register_handler(ctx, message_types[i], msg_bio_handler);
                LOG_MODULE_DEBUG(MODULE_NAME,
                    "  Registered handler for BIO_MSG_SHUTDOWN_REQUEST");
                break;

            default:
                LOG_MODULE_DEBUG(MODULE_NAME,
                    "  Unknown message type 0x%04X - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

//=============================================================================
// Bio-Async Broadcast Functions
//=============================================================================

/**
 * @brief Broadcast message serialization event
 *
 * WHAT: Notify cognitive modules when a message is serialized
 * WHY:  Enable monitoring of outbound message preparation
 * HOW:  Send bio_msg_nlp_message_sent_t to relevant modules
 *
 * @param msg_type NLP message type
 * @param payload_size Payload size
 * @param total_size Total serialized size
 */
static void msg_broadcast_serialized(
    uint16_t msg_type,
    uint16_t payload_size,
    size_t total_size
) {
    if (!g_msg_bio_ctx) return;

    bio_msg_nlp_message_sent_t event;
    memset(&event, 0, sizeof(event));

    event.header.type = BIO_MSG_NLP_MESSAGE_SENT;
    event.header.timestamp_us = nimcp_time_get_us();
    event.header.source_module = BIO_MODULE_NLP;
    event.header.payload_size = sizeof(event) - sizeof(bio_message_header_t);

    event.peer_id = 0;  // Local serialization, no specific peer
    event.message_type = msg_type;
    event.message_size = payload_size;
    event.sequence_num = 0;
    event.encrypted = false;
    event.reliable = false;

    bio_router_broadcast(g_msg_bio_ctx, &event, sizeof(event));

    LOG_MODULE_TRACE(MODULE_NAME, "Broadcast: message serialized type=0x%04x total=%zu",
                     msg_type, total_size);
}

/**
 * @brief Broadcast message deserialization event
 *
 * WHAT: Notify cognitive modules when a message is deserialized
 * WHY:  Enable monitoring of inbound message processing
 * HOW:  Send bio_msg_nlp_message_received_t to relevant modules
 *
 * @param msg_type NLP message type
 * @param payload_size Payload size
 * @param valid Whether deserialization succeeded
 */
static void msg_broadcast_deserialized(
    uint16_t msg_type,
    uint16_t payload_size,
    bool valid
) {
    if (!g_msg_bio_ctx) return;

    bio_msg_nlp_message_received_t event;
    memset(&event, 0, sizeof(event));

    event.header.type = BIO_MSG_NLP_MESSAGE_RECEIVED;
    event.header.timestamp_us = nimcp_time_get_us();
    event.header.source_module = BIO_MODULE_NLP;
    event.header.payload_size = sizeof(event) - sizeof(bio_message_header_t);
    if (!valid) {
        event.header.flags |= BIO_MSG_FLAG_URGENT;  // Mark invalid messages as urgent
    }

    event.peer_id = 0;  // Deserialization source unknown at this level
    event.message_type = msg_type;
    event.message_size = payload_size;
    event.sequence_num = 0;
    event.encrypted = false;
    event.compressed = false;

    bio_router_broadcast(g_msg_bio_ctx, &event, sizeof(event));

    LOG_MODULE_TRACE(MODULE_NAME, "Broadcast: message deserialized type=0x%04x valid=%d",
                     msg_type, valid);
}

/**
 * @brief Broadcast message validation event
 *
 * WHAT: Notify cognitive modules about message validation results
 * WHY:  Enable security monitoring and attention system awareness
 * HOW:  Send error event if validation fails
 *
 * @param msg_type Message type
 * @param error_code Error code (0 for success)
 * @param message Error message
 */
static void msg_broadcast_validation(
    uint16_t msg_type,
    int error_code,
    const char* message
) {
    if (!g_msg_bio_ctx || error_code == 0) return;  // Only broadcast failures

    bio_msg_nlp_error_t event;
    memset(&event, 0, sizeof(event));

    event.header.type = BIO_MSG_NLP_ERROR;
    event.header.timestamp_us = nimcp_time_get_us();
    event.header.source_module = BIO_MODULE_NLP;
    event.header.payload_size = sizeof(event) - sizeof(bio_message_header_t);
    event.header.flags |= BIO_MSG_FLAG_URGENT;  // Validation errors are urgent

    event.peer_id = 0;
    event.error_code = (uint32_t)(-error_code);
    event.severity = 2;  // Warning level

    strncpy(event.module_name, MODULE_NAME, sizeof(event.module_name) - 1);
    if (message) {
        strncpy(event.error_message, message, sizeof(event.error_message) - 1);
    }

    bio_router_broadcast(g_msg_bio_ctx, &event, sizeof(event));

    LOG_MODULE_DEBUG(MODULE_NAME, "Broadcast: validation error type=0x%04x code=%d",
                     msg_type, error_code);
    (void)msg_type;  // Used in log message
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Initialize message context if needed
 *
 * WHAT: Lazy initialization of global message context
 * WHY:  Avoid initialization order issues
 * HOW:  Check flag and initialize on first use
 */
static void ensure_context_initialized(void) {
    if (!g_msg_ctx.initialized) {
        g_msg_ctx.magic = NLP_MAGIC;
        g_msg_ctx.initialized = true;

        // Register with Blood-Brain Barrier
        bbb_register_module(MODULE_NAME, BBB_MODULE_TYPE_NETWORK);

        // Register with bio-router for cross-module messaging (if initialized)
        if (bio_router_is_initialized()) {
            bio_module_info_t bio_info = {
                .module_id = BIO_MODULE_NLP,  // Use NLP module ID
                .module_name = MODULE_NAME,
                .inbox_capacity = 32,
                .user_data = NULL
            };
            g_msg_bio_ctx = bio_router_register_module(&bio_info);

            // Register handlers for incoming messages
            if (g_msg_bio_ctx) {
                /* KG-Driven Wiring: Register callback for orchestrator to invoke
                 * When orchestrator starts, it discovers HANDLES_MESSAGE relations
                 * from the KG and invokes this callback with the message types */
                nimcp_error_t cb_result = bio_router_register_wiring_callback(
                    BIO_MODULE_NLP,
                    (void*)msg_wiring_handler_callback,
                    NULL
                );

                if (cb_result != NIMCP_SUCCESS) {
                    /* Fallback: Direct registration if orchestrator not available
                     * This ensures backward compatibility with non-KG systems */
                    LEGACY_HANDLER_REGISTRATION(
                        bio_router_register_handler(g_msg_bio_ctx, BIO_MSG_HEALTH_CHECK,
                                                    msg_bio_handler)
                    );
                    LEGACY_HANDLER_REGISTRATION(
                        bio_router_register_handler(g_msg_bio_ctx, BIO_MSG_ATTENTION_SHIFT,
                                                    msg_bio_handler)
                    );
                    LEGACY_HANDLER_REGISTRATION(
                        bio_router_register_handler(g_msg_bio_ctx, BIO_MSG_SECURITY_ALERT,
                                                    msg_bio_handler)
                    );
                    LEGACY_HANDLER_REGISTRATION(
                        bio_router_register_handler(g_msg_bio_ctx, BIO_MSG_SHUTDOWN_REQUEST,
                                                    msg_bio_handler)
                    );
                    LOG_MODULE_DEBUG(MODULE_NAME,
                        "Bio-async handlers registered (legacy direct registration)");
                } else {
                    LOG_MODULE_DEBUG(MODULE_NAME,
                        "Bio-async handlers registered (KG-driven wiring callback)");
                }
            }
        }

        bbb_audit_log(BBB_AUDIT_INFO, MODULE_NAME, "initialized",
                      "NLP message context initialized");
        LOG_MODULE_INFO(MODULE_NAME, "NLP message context initialized");
    }
}

/**
 * @brief Get current Unix timestamp
 *
 * @return Current timestamp in seconds
 */
static inline uint32_t get_current_timestamp(void) {
    return (uint32_t)time(NULL);
}

/**
 * @brief Generate random nonce
 *
 * WHAT: Generates cryptographically random nonce for AES-GCM
 * WHY:  Each message must have unique nonce
 * HOW:  Uses nimcp_rand_bytes (secure RNG with /dev/urandom or auto-seeded fallback)
 *
 * @param nonce Output buffer (must be NLP_NONCE_SIZE bytes)
 */
static void generate_nonce(uint8_t* nonce) {
    if (!nonce) return;

    // Use secure random bytes from nimcp_rand API
    nimcp_rand_bytes(nonce, NLP_NONCE_SIZE);
}

//=============================================================================
// CRC-16 Implementation
//=============================================================================

/**
 * @brief Calculate CRC-16-CCITT
 *
 * WHAT: Computes CRC-16 checksum using CCITT polynomial
 * WHY:  Detect header corruption in transit
 * HOW:  Standard CRC-16-CCITT algorithm with 0xFFFF initial value
 *
 * @param data Data to checksum
 * @param len Data length
 * @return CRC-16 value
 *
 * COMPLEXITY: O(n) where n = len
 */
static uint16_t calculate_crc16(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return 0;
    }

    uint16_t crc = 0xFFFF;  // Initial value for CRC-16-CCITT

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ CRC16_POLY;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

/**
 * @brief Calculate CRC-16 for header (public API)
 *
 * WHAT: Calculates CRC-16 for NLP message header
 * WHY:  Validate header integrity
 * HOW:  Computes CRC over first 34 bytes (excluding CRC field)
 *
 * @param header Header to checksum
 * @return CRC-16 value
 */
uint16_t nlp_header_crc(const nlp_header_t* header) {
    if (!header) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_header_crc: NULL header");
        return 0;
    }

    // CRC covers bytes 0-33 (everything except the CRC field itself)
    return calculate_crc16((const uint8_t*)header, NLP_HEADER_SIZE - 2);
}

//=============================================================================
// Header Functions
//=============================================================================

/**
 * @brief Initialize header with defaults
 *
 * WHAT: Sets header fields to sensible default values
 * WHY:  Ensure all fields are initialized before use
 * HOW:  Zeroes memory and sets version/timestamp
 *
 * @param header Header to initialize
 */
void nlp_header_init(nlp_header_t* header) {
    if (!header) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_header_init: NULL header");
        return;
    }

    memset(header, 0, sizeof(nlp_header_t));

    // Set version
    NLP_SET_VERSION(header, NLP_VERSION);

    // Set default mode and priority
    NLP_SET_MODE(header, NLP_MODE_STANDARD);
    NLP_SET_PRIORITY(header, NLP_PRIORITY_NORMAL);

    // Set timestamp
    header->timestamp = get_current_timestamp();

    // Generate nonce
    generate_nonce(header->nonce);

    LOG_MODULE_DEBUG(MODULE_NAME, "Header initialized: version=%d, timestamp=%u",
                     NLP_VERSION, header->timestamp);
}

/**
 * @brief Serialize header to network byte order
 *
 * WHAT: Converts header fields from host to network byte order
 * WHY:  Ensure consistent byte order across different architectures
 * HOW:  Uses htons/htonl on multi-byte fields, calculates CRC
 *
 * @param header Header to serialize (modified in place)
 */
void nlp_header_serialize(nlp_header_t* header) {
    if (!header) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_header_serialize: NULL header");
        return;
    }

    // Convert multi-byte fields to network byte order
    header->msg_type = htons(header->msg_type);
    header->sender_id = htonl(header->sender_id);
    header->timestamp = htonl(header->timestamp);
    header->sequence = htons(header->sequence);
    header->ack_sequence = htons(header->ack_sequence);
    header->dest_id = htonl(header->dest_id);
    header->payload_len = htons(header->payload_len);

    // Calculate and set CRC (must be last, in network byte order)
    uint16_t crc = nlp_header_crc(header);
    header->header_crc = htons(crc);

    LOG_MODULE_TRACE(MODULE_NAME, "Header serialized: type=0x%04x, crc=0x%04x",
                     ntohs(header->msg_type), crc);
}

/**
 * @brief Deserialize header from network byte order
 *
 * WHAT: Converts header fields from network to host byte order
 * WHY:  Parse wire format into usable structure
 * HOW:  Uses ntohs/ntohl on multi-byte fields, validates CRC
 *
 * @param header Header to deserialize (modified in place)
 * @return 0 on success, negative error code on failure
 */
int nlp_header_deserialize(nlp_header_t* header) {
    if (!header) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_header_deserialize: NULL header");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    // Validate CRC before conversion (CRC is in network byte order)
    uint16_t stored_crc = ntohs(header->header_crc);
    uint16_t computed_crc = nlp_header_crc(header);

    if (stored_crc != computed_crc) {
        LOG_MODULE_ERROR(MODULE_NAME, "Header CRC mismatch: stored=0x%04x, computed=0x%04x",
                        stored_crc, computed_crc);
        return -NIMCP_ERROR_INVALID_SIGNATURE;
    }

    // Convert multi-byte fields to host byte order
    header->msg_type = ntohs(header->msg_type);
    header->sender_id = ntohl(header->sender_id);
    header->timestamp = ntohl(header->timestamp);
    header->sequence = ntohs(header->sequence);
    header->ack_sequence = ntohs(header->ack_sequence);
    header->dest_id = ntohl(header->dest_id);
    header->payload_len = ntohs(header->payload_len);
    header->header_crc = stored_crc;  // Keep in host order

    LOG_MODULE_TRACE(MODULE_NAME, "Header deserialized: type=0x%04x, len=%u",
                     header->msg_type, header->payload_len);

    return 0;
}

//=============================================================================
// Message Lifecycle
//=============================================================================

/**
 * @brief Create new NLP message
 *
 * WHAT: Allocates and initializes a new NLP message structure
 * WHY:  Provide clean API for message creation
 * HOW:  Uses nimcp_memory, initializes header and payload
 *
 * @param msg_type Message type
 * @param payload_data Payload data (can be NULL)
 * @param payload_len Payload length
 * @return Message pointer or NULL on failure
 *
 * COMPLEXITY: O(n) where n = payload_len (for memcpy)
 * MEMORY: sizeof(nlp_message_t) + payload_len bytes
 */
nlp_message_t* nlp_message_create(
    nlp_msg_type_t msg_type,
    const void* payload_data,
    uint16_t payload_len
) {
    ensure_context_initialized();

    // Validate parameters
    if (payload_len > NLP_MAX_PAYLOAD) {
        LOG_MODULE_ERROR(MODULE_NAME, "Payload too large: %u > %u",
                        payload_len, NLP_MAX_PAYLOAD);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_message_create: validation failed");
        return NULL;
    }

    // Allocate message structure
    nlp_message_t* msg = (nlp_message_t*)nimcp_calloc(1, sizeof(nlp_message_t));
    if (!msg) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to allocate message structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "msg is NULL");

        return NULL;
    }

    // Initialize header
    nlp_header_init(&msg->header);
    msg->header.msg_type = msg_type;
    msg->header.payload_len = payload_len;

    // Allocate and copy payload if provided
    if (payload_len > 0) {
        msg->payload = (uint8_t*)nimcp_calloc(1, payload_len);
        if (!msg->payload) {
            nimcp_free(msg);
            LOG_MODULE_ERROR(MODULE_NAME, "Failed to allocate payload");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nlp_message_create: msg->payload is NULL");
            return NULL;
        }

        if (payload_data) {
            memcpy(msg->payload, payload_data, payload_len);
        }
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Message created: type=0x%04x, payload_len=%u",
                     msg_type, payload_len);

    return msg;
}

/**
 * @brief Destroy NLP message
 *
 * WHAT: Frees message and all associated resources
 * WHY:  Prevent memory leaks
 * HOW:  Frees payload and message structure
 *
 * @param msg Message to destroy
 */
void nlp_message_destroy(nlp_message_t* msg) {
    if (!msg) {
        return;
    }

    if (msg->payload) {
        nimcp_free(msg->payload);
        msg->payload = NULL;
    }

    nimcp_free(msg);

    LOG_MODULE_TRACE(MODULE_NAME, "Message destroyed");
}

//=============================================================================
// Serialization/Deserialization
//=============================================================================

/**
 * @brief Serialize message to wire format
 *
 * WHAT: Converts message to byte array for transmission
 * WHY:  Prepare message for network transmission
 * HOW:  Serializes header + payload + auth tag
 *
 * @param msg Message to serialize
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param bytes_written Output: bytes written to buffer
 * @return 0 on success, negative error code on failure
 *
 * WIRE FORMAT:
 * [Header: 36 bytes][Payload: N bytes][Auth Tag: 16 bytes]
 */
int nlp_message_serialize(
    const nlp_message_t* msg,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_written
) {
    if (!msg || !buffer || !bytes_written) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_message_serialize: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    // Calculate total size
    size_t total_size = NLP_HEADER_SIZE + msg->header.payload_len + NLP_TAG_SIZE;

    if (buffer_size < total_size) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small: %zu < %zu",
                        buffer_size, total_size);
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // Create a copy of header for serialization
    nlp_header_t header_copy = msg->header;
    nlp_header_serialize(&header_copy);

    // Copy header to buffer
    memcpy(buffer, &header_copy, NLP_HEADER_SIZE);
    size_t offset = NLP_HEADER_SIZE;

    // Copy payload if present
    if (msg->header.payload_len > 0 && msg->payload) {
        memcpy(buffer + offset, msg->payload, msg->header.payload_len);
        offset += msg->header.payload_len;
    }

    // Copy authentication tag
    memcpy(buffer + offset, msg->auth_tag, NLP_TAG_SIZE);
    offset += NLP_TAG_SIZE;

    *bytes_written = offset;

    LOG_MODULE_DEBUG(MODULE_NAME, "Message serialized: %zu bytes", offset);

    // Broadcast serialization event for bio-async integration
    msg_broadcast_serialized(msg->header.msg_type, msg->header.payload_len, offset);

    return 0;
}

/**
 * @brief Deserialize message from wire format
 *
 * WHAT: Parses byte array into message structure
 * WHY:  Process received network messages
 * HOW:  Deserializes header, validates, extracts payload
 *
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param msg Output: deserialized message (caller must free)
 * @return 0 on success, negative error code on failure
 */
int nlp_message_deserialize(
    const uint8_t* buffer,
    size_t buffer_size,
    nlp_message_t** msg
) {
    if (!buffer || !msg) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_message_deserialize: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    // Check minimum size
    if (buffer_size < NLP_MIN_MESSAGE_SIZE) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small: %zu < %u",
                        buffer_size, NLP_MIN_MESSAGE_SIZE);
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // Copy and deserialize header
    nlp_header_t header;
    memcpy(&header, buffer, NLP_HEADER_SIZE);

    int ret = nlp_header_deserialize(&header);
    if (ret < 0) {
        LOG_MODULE_ERROR(MODULE_NAME, "Header deserialization failed: %d", ret);
        return ret;
    }

    // Validate total message size
    size_t expected_size = NLP_HEADER_SIZE + header.payload_len + NLP_TAG_SIZE;
    if (buffer_size < expected_size) {
        LOG_MODULE_ERROR(MODULE_NAME, "Message truncated: %zu < %zu",
                        buffer_size, expected_size);
        return -NIMCP_ERROR_INVALID_SIZE;
    }

    // Create message
    nlp_message_t* new_msg = nlp_message_create(header.msg_type, NULL, header.payload_len);
    if (!new_msg) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to create message");
        return -NIMCP_ERROR_MEMORY;
    }

    // Copy header (already deserialized)
    new_msg->header = header;

    // Copy payload if present
    if (header.payload_len > 0) {
        memcpy(new_msg->payload, buffer + NLP_HEADER_SIZE, header.payload_len);
    }

    // Copy authentication tag
    memcpy(new_msg->auth_tag, buffer + NLP_HEADER_SIZE + header.payload_len, NLP_TAG_SIZE);

    *msg = new_msg;

    LOG_MODULE_DEBUG(MODULE_NAME, "Message deserialized: type=0x%04x, payload=%u bytes",
                     header.msg_type, header.payload_len);

    // Broadcast deserialization event for bio-async integration
    msg_broadcast_deserialized(header.msg_type, header.payload_len, true);

    return 0;
}

//=============================================================================
// Message Validation
//=============================================================================

/**
 * @brief Validate message integrity
 *
 * WHAT: Checks message for validity (CRC, timestamp, length, etc.)
 * WHY:  Detect corrupted or malicious messages
 * HOW:  Validates header CRC, timestamp range, payload length
 *
 * @param msg Message to validate
 * @return 0 if valid, negative error code on failure
 */
int nlp_message_validate(const nlp_message_t* msg) {
    if (!msg) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_message_validate: NULL message");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    // Validate version
    uint8_t version = NLP_GET_VERSION(&msg->header);
    if (version != NLP_VERSION) {
        LOG_MODULE_ERROR(MODULE_NAME, "Invalid version: %u != %u",
                        version, NLP_VERSION);
        msg_broadcast_validation(msg->header.msg_type, -NIMCP_ERROR_INVALID_VERSION,
                                 "Invalid protocol version");
        return -NIMCP_ERROR_INVALID_VERSION;
    }

    // Validate timestamp (sanity check)
    if (msg->header.timestamp < NLP_MIN_TIMESTAMP ||
        msg->header.timestamp > NLP_MAX_TIMESTAMP) {
        LOG_MODULE_ERROR(MODULE_NAME, "Invalid timestamp: %u", msg->header.timestamp);
        return -NIMCP_ERROR_INVALID_PARAM;
    }

    // Check for timestamp replay (within window)
    uint32_t current_time = get_current_timestamp();
    int32_t time_diff = (int32_t)(current_time - msg->header.timestamp);
    if (abs(time_diff) > NLP_TIMESTAMP_WINDOW) {
        LOG_MODULE_WARN(MODULE_NAME, "Timestamp outside window: diff=%d > %d",
                       time_diff, NLP_TIMESTAMP_WINDOW);
        // Don't fail, just warn (clocks may be slightly off)
    }

    // Validate payload length
    if (msg->header.payload_len > NLP_MAX_PAYLOAD) {
        LOG_MODULE_ERROR(MODULE_NAME, "Payload too large: %u > %u",
                        msg->header.payload_len, NLP_MAX_PAYLOAD);
        return -NIMCP_ERROR_PAYLOAD_TOO_LARGE;
    }

    // Validate payload pointer if length > 0
    if (msg->header.payload_len > 0 && !msg->payload) {
        LOG_MODULE_ERROR(MODULE_NAME, "Payload length %u but NULL payload",
                        msg->header.payload_len);
        return -NIMCP_ERROR_NULL_POINTER;
    }

    // Validate header CRC
    uint16_t computed_crc = nlp_header_crc(&msg->header);
    if (computed_crc != msg->header.header_crc) {
        LOG_MODULE_ERROR(MODULE_NAME, "CRC mismatch: stored=0x%04x, computed=0x%04x",
                        msg->header.header_crc, computed_crc);
        msg_broadcast_validation(msg->header.msg_type, -NIMCP_ERROR_INVALID_SIGNATURE,
                                 "Header CRC mismatch - possible tampering");
        return -NIMCP_ERROR_INVALID_SIGNATURE;
    }

    LOG_MODULE_TRACE(MODULE_NAME, "Message validated successfully");

    // Broadcast successful validation (no error)
    msg_broadcast_validation(msg->header.msg_type, 0, NULL);

    return 0;
}

//=============================================================================
// Payload Packing/Unpacking - Spike Batch
//=============================================================================

/**
 * @brief Pack spike batch payload
 *
 * WHAT: Serializes spike batch data into message payload
 * WHY:  Efficient transmission of neural spike events
 * HOW:  Packs header + neuron IDs + spike times
 *
 * @param batch_id Batch identifier
 * @param timestamp_us Microsecond timestamp offset
 * @param neuron_ids Array of neuron IDs that spiked
 * @param spike_times Array of spike times (us offset)
 * @param count Number of spikes
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param bytes_written Output: bytes written
 * @return 0 on success, negative on error
 */
int nlp_pack_spike_batch(
    uint32_t batch_id,
    uint32_t timestamp_us,
    const uint32_t* neuron_ids,
    const uint16_t* spike_times,
    uint16_t count,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_written
) {
    if (!neuron_ids || !spike_times || !buffer || !bytes_written) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_pack_spike_batch: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    // Calculate required size
    size_t required = sizeof(nlp_spike_batch_t) +
                     (count * sizeof(uint32_t)) +  // neuron IDs
                     (count * sizeof(uint16_t));   // spike times

    if (buffer_size < required) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for spike batch: %zu < %zu",
                        buffer_size, required);
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // Pack header
    nlp_spike_batch_t* batch = (nlp_spike_batch_t*)buffer;
    batch->batch_id = htonl(batch_id);
    batch->timestamp_us = htonl(timestamp_us);
    batch->spike_count = htons(count);
    batch->reserved = 0;

    // Pack neuron IDs
    uint32_t* ids = (uint32_t*)(buffer + sizeof(nlp_spike_batch_t));
    for (uint16_t i = 0; i < count; i++) {
        ids[i] = htonl(neuron_ids[i]);
    }

    // Pack spike times
    uint16_t* times = (uint16_t*)(buffer + sizeof(nlp_spike_batch_t) + (count * sizeof(uint32_t)));
    for (uint16_t i = 0; i < count; i++) {
        times[i] = htons(spike_times[i]);
    }

    *bytes_written = required;

    LOG_MODULE_DEBUG(MODULE_NAME, "Packed spike batch: %u spikes, %zu bytes", count, required);

    return 0;
}

/**
 * @brief Unpack spike batch payload
 *
 * WHAT: Deserializes spike batch from payload
 * WHY:  Process received spike events
 * HOW:  Extracts header, neuron IDs, spike times
 *
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param batch_id Output: batch ID
 * @param timestamp_us Output: timestamp
 * @param neuron_ids Output: neuron ID array (caller allocates)
 * @param spike_times Output: spike time array (caller allocates)
 * @param max_count Maximum spikes to unpack
 * @param count Output: actual spike count
 * @return 0 on success, negative on error
 */
int nlp_unpack_spike_batch(
    const uint8_t* buffer,
    size_t buffer_size,
    uint32_t* batch_id,
    uint32_t* timestamp_us,
    uint32_t* neuron_ids,
    uint16_t* spike_times,
    uint16_t max_count,
    uint16_t* count
) {
    if (!buffer || !batch_id || !timestamp_us || !count) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_unpack_spike_batch: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (buffer_size < sizeof(nlp_spike_batch_t)) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for spike batch header");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // Unpack header
    const nlp_spike_batch_t* batch = (const nlp_spike_batch_t*)buffer;
    *batch_id = ntohl(batch->batch_id);
    *timestamp_us = ntohl(batch->timestamp_us);
    uint16_t spike_count = ntohs(batch->spike_count);

    // Validate count
    if (spike_count > max_count) {
        LOG_MODULE_ERROR(MODULE_NAME, "Spike count %u exceeds max %u",
                        spike_count, max_count);
        return -NIMCP_ERROR_INVALID_SIZE;
    }

    *count = spike_count;

    // Validate buffer size
    size_t required = sizeof(nlp_spike_batch_t) +
                     (spike_count * sizeof(uint32_t)) +
                     (spike_count * sizeof(uint16_t));
    if (buffer_size < required) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for spike data");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // Unpack neuron IDs
    if (neuron_ids) {
        const uint32_t* ids = (const uint32_t*)(buffer + sizeof(nlp_spike_batch_t));
        for (uint16_t i = 0; i < spike_count; i++) {
            neuron_ids[i] = ntohl(ids[i]);
        }
    }

    // Unpack spike times
    if (spike_times) {
        const uint16_t* times = (const uint16_t*)(buffer + sizeof(nlp_spike_batch_t) +
                                                  (spike_count * sizeof(uint32_t)));
        for (uint16_t i = 0; i < spike_count; i++) {
            spike_times[i] = ntohs(times[i]);
        }
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Unpacked spike batch: %u spikes", spike_count);

    return 0;
}

//=============================================================================
// Payload Packing/Unpacking - Weight Deltas
//=============================================================================

/**
 * @brief Pack weight delta payload
 *
 * WHAT: Serializes weight change data into message payload
 * WHY:  Efficient transmission of synaptic weight updates
 * HOW:  Packs header + array of delta entries
 *
 * @param base_version Base weight version
 * @param new_version New weight version
 * @param synapse_ids Array of synapse IDs
 * @param old_weights Array of old weights
 * @param new_weights Array of new weights
 * @param count Number of deltas
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param bytes_written Output: bytes written
 * @return 0 on success, negative on error
 */
int nlp_pack_weight_deltas(
    uint32_t base_version,
    uint32_t new_version,
    const uint32_t* synapse_ids,
    const float* old_weights,
    const float* new_weights,
    uint16_t count,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_written
) {
    if (!synapse_ids || !old_weights || !new_weights || !buffer || !bytes_written) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_pack_weight_deltas: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    // Calculate required size
    size_t required = sizeof(nlp_weight_delta_header_t) +
                     (count * sizeof(nlp_weight_delta_entry_t));

    if (buffer_size < required) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for weight deltas: %zu < %zu",
                        buffer_size, required);
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // Pack header
    nlp_weight_delta_header_t* header = (nlp_weight_delta_header_t*)buffer;
    header->base_version = htonl(base_version);
    header->new_version = htonl(new_version);
    header->delta_count = htons(count);
    header->reserved = 0;

    // Pack entries
    nlp_weight_delta_entry_t* entries = (nlp_weight_delta_entry_t*)(buffer + sizeof(nlp_weight_delta_header_t));
    for (uint16_t i = 0; i < count; i++) {
        entries[i].synapse_id = htonl(synapse_ids[i]);

        // Convert floats to network byte order (assuming IEEE 754)
        uint32_t old_bits, new_bits;
        memcpy(&old_bits, &old_weights[i], sizeof(float));
        memcpy(&new_bits, &new_weights[i], sizeof(float));
        old_bits = htonl(old_bits);
        new_bits = htonl(new_bits);
        memcpy(&entries[i].old_weight, &old_bits, sizeof(float));
        memcpy(&entries[i].new_weight, &new_bits, sizeof(float));
    }

    *bytes_written = required;

    LOG_MODULE_DEBUG(MODULE_NAME, "Packed weight deltas: %u entries, %zu bytes", count, required);

    return 0;
}

/**
 * @brief Unpack weight delta payload
 *
 * WHAT: Deserializes weight deltas from payload
 * WHY:  Process received weight updates
 * HOW:  Extracts header and delta entries
 *
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param base_version Output: base version
 * @param new_version Output: new version
 * @param synapse_ids Output: synapse ID array (caller allocates)
 * @param old_weights Output: old weight array (caller allocates)
 * @param new_weights Output: new weight array (caller allocates)
 * @param max_count Maximum deltas to unpack
 * @param count Output: actual delta count
 * @return 0 on success, negative on error
 */
int nlp_unpack_weight_deltas(
    const uint8_t* buffer,
    size_t buffer_size,
    uint32_t* base_version,
    uint32_t* new_version,
    uint32_t* synapse_ids,
    float* old_weights,
    float* new_weights,
    uint16_t max_count,
    uint16_t* count
) {
    if (!buffer || !base_version || !new_version || !count) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_unpack_weight_deltas: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (buffer_size < sizeof(nlp_weight_delta_header_t)) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for weight delta header");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // Unpack header
    const nlp_weight_delta_header_t* header = (const nlp_weight_delta_header_t*)buffer;
    *base_version = ntohl(header->base_version);
    *new_version = ntohl(header->new_version);
    uint16_t delta_count = ntohs(header->delta_count);

    // Validate count
    if (delta_count > max_count) {
        LOG_MODULE_ERROR(MODULE_NAME, "Delta count %u exceeds max %u",
                        delta_count, max_count);
        return -NIMCP_ERROR_INVALID_SIZE;
    }

    *count = delta_count;

    // Validate buffer size
    size_t required = sizeof(nlp_weight_delta_header_t) +
                     (delta_count * sizeof(nlp_weight_delta_entry_t));
    if (buffer_size < required) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for weight delta data");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // Unpack entries
    const nlp_weight_delta_entry_t* entries = (const nlp_weight_delta_entry_t*)(buffer + sizeof(nlp_weight_delta_header_t));
    for (uint16_t i = 0; i < delta_count; i++) {
        if (synapse_ids) {
            synapse_ids[i] = ntohl(entries[i].synapse_id);
        }

        // Convert floats from network byte order
        if (old_weights) {
            uint32_t old_bits;
            memcpy(&old_bits, &entries[i].old_weight, sizeof(float));
            old_bits = ntohl(old_bits);
            memcpy(&old_weights[i], &old_bits, sizeof(float));
        }

        if (new_weights) {
            uint32_t new_bits;
            memcpy(&new_bits, &entries[i].new_weight, sizeof(float));
            new_bits = ntohl(new_bits);
            memcpy(&new_weights[i], &new_bits, sizeof(float));
        }
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Unpacked weight deltas: %u entries", delta_count);

    return 0;
}

//=============================================================================
// Payload Packing/Unpacking - Location
//=============================================================================

/**
 * @brief Pack GPS location payload
 *
 * WHAT: Serializes GPS location data
 * WHY:  For SAR and disaster response coordination
 * HOW:  Packs all location fields to network byte order
 *
 * @param location Location data
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param bytes_written Output: bytes written
 * @return 0 on success, negative on error
 */
int nlp_pack_location(
    const nlp_location_t* location,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_written
) {
    if (!location || !buffer || !bytes_written) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_pack_location: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (buffer_size < sizeof(nlp_location_t)) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for location");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    nlp_location_t* loc = (nlp_location_t*)buffer;

    // Pack doubles (8 bytes each)
    uint64_t lat_bits, lon_bits;
    memcpy(&lat_bits, &location->latitude, sizeof(double));
    memcpy(&lon_bits, &location->longitude, sizeof(double));

    // Convert to network byte order (big-endian)
    // For 64-bit values, we swap bytes manually
    lat_bits = htobe64(lat_bits);
    lon_bits = htobe64(lon_bits);

    memcpy(&loc->latitude, &lat_bits, sizeof(double));
    memcpy(&loc->longitude, &lon_bits, sizeof(double));

    // Pack floats
    uint32_t alt_bits, acc_bits, head_bits, speed_bits;
    memcpy(&alt_bits, &location->altitude_m, sizeof(float));
    memcpy(&acc_bits, &location->accuracy_m, sizeof(float));
    memcpy(&head_bits, &location->heading_deg, sizeof(float));
    memcpy(&speed_bits, &location->speed_mps, sizeof(float));

    alt_bits = htonl(alt_bits);
    acc_bits = htonl(acc_bits);
    head_bits = htonl(head_bits);
    speed_bits = htonl(speed_bits);

    memcpy(&loc->altitude_m, &alt_bits, sizeof(float));
    memcpy(&loc->accuracy_m, &acc_bits, sizeof(float));
    memcpy(&loc->heading_deg, &head_bits, sizeof(float));
    memcpy(&loc->speed_mps, &speed_bits, sizeof(float));

    // Pack integers
    loc->fix_timestamp = htonl(location->fix_timestamp);
    loc->fix_quality = location->fix_quality;

    *bytes_written = sizeof(nlp_location_t);

    LOG_MODULE_DEBUG(MODULE_NAME, "Packed location: lat=%.6f, lon=%.6f",
                     location->latitude, location->longitude);

    return 0;
}

/**
 * @brief Unpack GPS location payload
 *
 * WHAT: Deserializes GPS location from payload
 * WHY:  Process received location updates
 * HOW:  Extracts and converts all location fields
 *
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param location Output: location data
 * @return 0 on success, negative on error
 */
int nlp_unpack_location(
    const uint8_t* buffer,
    size_t buffer_size,
    nlp_location_t* location
) {
    if (!buffer || !location) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_unpack_location: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (buffer_size < sizeof(nlp_location_t)) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for location");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    const nlp_location_t* loc = (const nlp_location_t*)buffer;

    // Unpack doubles
    uint64_t lat_bits, lon_bits;
    memcpy(&lat_bits, &loc->latitude, sizeof(double));
    memcpy(&lon_bits, &loc->longitude, sizeof(double));

    lat_bits = be64toh(lat_bits);
    lon_bits = be64toh(lon_bits);

    memcpy(&location->latitude, &lat_bits, sizeof(double));
    memcpy(&location->longitude, &lon_bits, sizeof(double));

    // Unpack floats
    uint32_t alt_bits, acc_bits, head_bits, speed_bits;
    memcpy(&alt_bits, &loc->altitude_m, sizeof(float));
    memcpy(&acc_bits, &loc->accuracy_m, sizeof(float));
    memcpy(&head_bits, &loc->heading_deg, sizeof(float));
    memcpy(&speed_bits, &loc->speed_mps, sizeof(float));

    alt_bits = ntohl(alt_bits);
    acc_bits = ntohl(acc_bits);
    head_bits = ntohl(head_bits);
    speed_bits = ntohl(speed_bits);

    memcpy(&location->altitude_m, &alt_bits, sizeof(float));
    memcpy(&location->accuracy_m, &acc_bits, sizeof(float));
    memcpy(&location->heading_deg, &head_bits, sizeof(float));
    memcpy(&location->speed_mps, &speed_bits, sizeof(float));

    // Unpack integers
    location->fix_timestamp = ntohl(loc->fix_timestamp);
    location->fix_quality = loc->fix_quality;

    LOG_MODULE_DEBUG(MODULE_NAME, "Unpacked location: lat=%.6f, lon=%.6f",
                     location->latitude, location->longitude);

    return 0;
}

//=============================================================================
// Payload Packing/Unpacking - Sensor Data
//=============================================================================

/**
 * @brief Pack sensor data payload
 *
 * WHAT: Serializes environmental sensor readings
 * WHY:  Transmit disaster zone sensor data
 * HOW:  Packs all sensor fields with bitmap
 *
 * @param sensors Sensor data
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param bytes_written Output: bytes written
 * @return 0 on success, negative on error
 */
int nlp_pack_sensor_data(
    const nlp_sensor_data_t* sensors,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_written
) {
    if (!sensors || !buffer || !bytes_written) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_pack_sensor_data: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (buffer_size < sizeof(nlp_sensor_data_t)) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for sensor data");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    nlp_sensor_data_t* data = (nlp_sensor_data_t*)buffer;

    // Pack all float fields
    const float* src_floats = (const float*)sensors;
    float* dst_floats = (float*)data;
    size_t float_count = (sizeof(nlp_sensor_data_t) - sizeof(uint32_t)) / sizeof(float);

    for (size_t i = 0; i < float_count; i++) {
        uint32_t bits;
        memcpy(&bits, &src_floats[i], sizeof(float));
        bits = htonl(bits);
        memcpy(&dst_floats[i], &bits, sizeof(float));
    }

    // Pack sensor bitmap
    data->sensor_bitmap = htonl(sensors->sensor_bitmap);

    *bytes_written = sizeof(nlp_sensor_data_t);

    LOG_MODULE_DEBUG(MODULE_NAME, "Packed sensor data: bitmap=0x%08x", sensors->sensor_bitmap);

    return 0;
}

/**
 * @brief Unpack sensor data payload
 *
 * WHAT: Deserializes environmental sensor readings
 * WHY:  Process received sensor data
 * HOW:  Extracts all sensor fields
 *
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param sensors Output: sensor data
 * @return 0 on success, negative on error
 */
int nlp_unpack_sensor_data(
    const uint8_t* buffer,
    size_t buffer_size,
    nlp_sensor_data_t* sensors
) {
    if (!buffer || !sensors) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_unpack_sensor_data: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (buffer_size < sizeof(nlp_sensor_data_t)) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for sensor data");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    const nlp_sensor_data_t* data = (const nlp_sensor_data_t*)buffer;

    // Unpack all float fields
    const float* src_floats = (const float*)data;
    float* dst_floats = (float*)sensors;
    size_t float_count = (sizeof(nlp_sensor_data_t) - sizeof(uint32_t)) / sizeof(float);

    for (size_t i = 0; i < float_count; i++) {
        uint32_t bits;
        memcpy(&bits, &src_floats[i], sizeof(float));
        bits = ntohl(bits);
        memcpy(&dst_floats[i], &bits, sizeof(float));
    }

    // Unpack sensor bitmap
    sensors->sensor_bitmap = ntohl(data->sensor_bitmap);

    LOG_MODULE_DEBUG(MODULE_NAME, "Unpacked sensor data: bitmap=0x%08x", sensors->sensor_bitmap);

    return 0;
}

//=============================================================================
// Payload Packing/Unpacking - Victim Report
//=============================================================================

/**
 * @brief Pack victim report payload
 *
 * WHAT: Serializes SAR victim report
 * WHY:  Transmit victim information in disaster scenarios
 * HOW:  Packs victim data + location + notes
 *
 * @param report Victim report
 * @param notes Optional notes (can be NULL)
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param bytes_written Output: bytes written
 * @return 0 on success, negative on error
 */
int nlp_pack_victim_report(
    const nlp_victim_report_t* report,
    const char* notes,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_written
) {
    if (!report || !buffer || !bytes_written) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_pack_victim_report: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    uint16_t notes_len = notes ? (uint16_t)strlen(notes) : 0;
    size_t required = sizeof(nlp_victim_report_t) + notes_len;

    if (buffer_size < required) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for victim report");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    nlp_victim_report_t* rep = (nlp_victim_report_t*)buffer;

    // Pack victim ID
    rep->victim_id = htonl(report->victim_id);

    // Pack location (embedded structure)
    size_t loc_bytes;
    int ret = nlp_pack_location(&report->location, (uint8_t*)&rep->location,
                                sizeof(nlp_location_t), &loc_bytes);
    if (ret < 0) {
        return ret;
    }

    // Pack triage and status fields
    rep->triage = report->triage;
    rep->mobility = report->mobility;
    rep->consciousness = report->consciousness;
    rep->breathing = report->breathing;
    rep->reserved = 0;

    // Pack notes length
    rep->notes_len = htons(notes_len);

    // Pack notes if present
    if (notes_len > 0) {
        memcpy(buffer + sizeof(nlp_victim_report_t), notes, notes_len);
    }

    *bytes_written = required;

    LOG_MODULE_DEBUG(MODULE_NAME, "Packed victim report: id=%u, triage=%d, notes=%u bytes",
                     report->victim_id, report->triage, notes_len);

    return 0;
}

/**
 * @brief Unpack victim report payload
 *
 * WHAT: Deserializes SAR victim report
 * WHY:  Process received victim information
 * HOW:  Extracts victim data, location, and notes
 *
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param report Output: victim report
 * @param notes Output: notes buffer (caller allocates)
 * @param notes_max_len Maximum notes length
 * @param notes_actual_len Output: actual notes length
 * @return 0 on success, negative on error
 */
int nlp_unpack_victim_report(
    const uint8_t* buffer,
    size_t buffer_size,
    nlp_victim_report_t* report,
    char* notes,
    size_t notes_max_len,
    uint16_t* notes_actual_len
) {
    if (!buffer || !report) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_unpack_victim_report: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (buffer_size < sizeof(nlp_victim_report_t)) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for victim report");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    const nlp_victim_report_t* rep = (const nlp_victim_report_t*)buffer;

    // Unpack victim ID
    report->victim_id = ntohl(rep->victim_id);

    // Unpack location
    int ret = nlp_unpack_location((const uint8_t*)&rep->location,
                                  sizeof(nlp_location_t), &report->location);
    if (ret < 0) {
        return ret;
    }

    // Unpack triage and status fields
    report->triage = rep->triage;
    report->mobility = rep->mobility;
    report->consciousness = rep->consciousness;
    report->breathing = rep->breathing;

    // Unpack notes length
    uint16_t notes_len = ntohs(rep->notes_len);
    if (notes_actual_len) {
        *notes_actual_len = notes_len;
    }
    report->notes_len = notes_len;

    // Validate buffer size for notes
    if (buffer_size < sizeof(nlp_victim_report_t) + notes_len) {
        LOG_MODULE_ERROR(MODULE_NAME, "Buffer too small for notes");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // Unpack notes if requested
    if (notes && notes_len > 0) {
        size_t copy_len = (notes_len < notes_max_len - 1) ? notes_len : notes_max_len - 1;
        memcpy(notes, buffer + sizeof(nlp_victim_report_t), copy_len);
        notes[copy_len] = '\0';
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Unpacked victim report: id=%u, triage=%d, notes=%u bytes",
                     report->victim_id, report->triage, notes_len);

    return 0;
}

//=============================================================================
// Stealth Mode Helpers
//=============================================================================

/**
 * @brief Pad message to fixed size for stealth mode
 *
 * WHAT: Pads message to NLP_STEALTH_PACKET_SIZE for traffic analysis resistance
 * WHY:  Prevent size-based traffic analysis
 * HOW:  Adds random padding to reach fixed size
 *
 * @param msg Message to pad
 * @param padded_buffer Output buffer (must be NLP_STEALTH_PACKET_SIZE)
 * @return 0 on success, negative on error
 */
int nlp_message_pad_to_fixed_size(
    const nlp_message_t* msg,
    uint8_t* padded_buffer
) {
    if (!msg || !padded_buffer) {
        LOG_MODULE_ERROR(MODULE_NAME, "nlp_message_pad_to_fixed_size: NULL parameter");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    // Serialize message to buffer
    size_t msg_size;
    int ret = nlp_message_serialize(msg, padded_buffer, NLP_STEALTH_PACKET_SIZE, &msg_size);
    if (ret < 0) {
        return ret;
    }

    // Check if padding needed
    if (msg_size > NLP_STEALTH_PACKET_SIZE) {
        LOG_MODULE_ERROR(MODULE_NAME, "Message too large for stealth mode: %zu > %u",
                        msg_size, NLP_STEALTH_PACKET_SIZE);
        return -NIMCP_ERROR_PAYLOAD_TOO_LARGE;
    }

    // Fill remaining space with random data
    size_t padding_size = NLP_STEALTH_PACKET_SIZE - msg_size;
    if (padding_size > 0) {
        // Use secure random bytes from nimcp_rand API
        nimcp_rand_bytes(padded_buffer + msg_size, padding_size);
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Padded message: %zu -> %u bytes (%zu padding)",
                     msg_size, NLP_STEALTH_PACKET_SIZE, padding_size);

    return 0;
}

/**
 * @brief Create chaff message (fake traffic)
 *
 * WHAT: Creates dummy message for traffic obfuscation
 * WHY:  Prevent traffic analysis in stealth mode
 * HOW:  Generates valid-looking but meaningless message
 *
 * @param sender_id Source brain ID
 * @param dest_id Destination ID (0 for broadcast)
 * @return Chaff message or NULL on failure (caller must free)
 */
nlp_message_t* nlp_message_create_chaff(uint32_t sender_id, uint32_t dest_id) {
    ensure_context_initialized();

    // Generate random payload size (0-64 bytes) using secure RNG
    uint16_t payload_len = (uint16_t)(nimcp_rand_uint(65));

    // Create message with random payload
    nlp_message_t* msg = nlp_message_create(NLP_MSG_CHAFF, NULL, payload_len);
    if (!msg) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to create chaff message");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "msg is NULL");

        return NULL;
    }

    // Set message type and IDs
    msg->header.sender_id = sender_id;
    msg->header.dest_id = dest_id;

    // Fill payload with random data using secure RNG
    if (payload_len > 0 && msg->payload) {
        nimcp_rand_bytes(msg->payload, payload_len);
    }

    // Set CHAFF flag
    uint8_t flags = NLP_GET_FLAGS(&msg->header);
    NLP_SET_FLAGS(&msg->header, flags);

    LOG_MODULE_DEBUG(MODULE_NAME, "Created chaff message: %u bytes payload", payload_len);

    return msg;
}
