/**
 * @file nimcp_language_production_bridge.h
 * @brief Language Production Bridge connecting Broca's area to Neural Link Protocol
 *
 * WHAT: Bidirectional bridge for brain-to-brain language communication
 * WHY:  Enable thoughts and language to be transmitted between brains via NLP
 * HOW:  Encode semantic content → neural patterns → NLP messages → remote brain
 *
 * ARCHITECTURE:
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │                    LANGUAGE PRODUCTION BRIDGE                        │
 * │                                                                       │
 * │  ┌────────────┐     ┌──────────────┐     ┌────────────────────┐    │
 * │  │   Broca    │────▶│   Encoder    │────▶│  NLP Transmitter   │────┼──▶ Network
 * │  │   Area     │     │  (Semantic   │     │  (Compression +    │    │
 * │  └────────────┘     │   + Neural)  │     │   Encryption)      │    │
 * │                     └──────────────┘     └────────────────────┘    │
 * │                                                                      │
 * │  ┌────────────┐     ┌──────────────┐     ┌────────────────────┐    │
 * │  │ Wernicke   │◀────│   Decoder    │◀────│  NLP Receiver      │◀───┼─── Network
 * │  │  (Receive) │     │  (Neural +   │     │  (Decrypt +        │    │
 * │  └────────────┘     │   Semantic)  │     │   Decompress)      │    │
 * │                     └──────────────┘     └────────────────────┘    │
 * └─────────────────────────────────────────────────────────────────────┘
 *
 * SEMANTIC ENCODING:
 * - Text → embedding vector (thought representation)
 * - Semantic compression (key concepts only)
 * - Neural pattern encoding (spike timing)
 * - Optional encryption + compression
 *
 * NEURAL PATTERNS:
 * - 300-512 dimensional neural vectors
 * - Rate coding: firing rates encode semantic features
 * - Temporal coding: spike timing encodes syntax/structure
 * - Population coding: distributed representation
 *
 * BIO-ASYNC INTEGRATION:
 * - BIO_MSG_LANGUAGE_THOUGHT_ENCODED: Thought encoded successfully
 * - BIO_MSG_LANGUAGE_MESSAGE_TRANSMITTED: Message sent to network
 * - BIO_MSG_LANGUAGE_MESSAGE_RECEIVED: Message received from network
 * - BIO_MSG_LANGUAGE_ENCODING_ERROR: Encoding/decoding error
 *
 * CHANNELS:
 * - DOPAMINE: Successful encoding/transmission (reward)
 * - ACETYLCHOLINE: Fast query/response communication
 * - NOREPINEPHRINE: Transmission errors, failed decoding
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_LANGUAGE_PRODUCTION_BRIDGE_H
#define NIMCP_LANGUAGE_PRODUCTION_BRIDGE_H

#include "utils/validation/nimcp_common.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "networking/nlp/nimcp_neural_link_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_production_bridge language_production_bridge_t;
typedef struct broca_context broca_context_t;  // Forward declare for now

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for language production bridge
 */
typedef struct {
    uint32_t max_message_queue;          /**< Max pending messages to queue */
    uint32_t semantic_buffer_size;       /**< Buffer size for semantic content */
    float articulation_threshold;        /**< Confidence threshold for transmission (0-1) */
    bool enable_bio_async;               /**< Enable bio-async integration */
    bool enable_compression;             /**< Enable message compression */
    bool enable_encryption;              /**< Enable message encryption */
    uint32_t encoding_dim;               /**< Neural encoding dimensionality (300-512) */
    float encoding_rate;                 /**< Neural encoding rate (Hz) */
} language_bridge_config_t;

//=============================================================================
// Language Message Structure
//=============================================================================

/**
 * @brief Language message for transmission
 */
typedef struct {
    uint32_t message_id;                 /**< Unique message identifier */

    // Semantic content
    char* semantic_content;              /**< Text/semantic representation */
    uint32_t semantic_size;              /**< Size of semantic content */

    // Neural encoding
    float* neural_encoding;              /**< Neural pattern (spike rates) */
    uint32_t encoding_size;              /**< Number of neurons/dimensions */

    // Metadata
    float confidence;                    /**< Encoding confidence (0-1) */
    uint64_t timestamp_ms;               /**< Message timestamp */
    uint8_t language_code;               /**< Language identifier */
    uint8_t intent_type;                 /**< Message intent (command, query, etc.) */
} language_message_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t messages_produced;          /**< Total messages encoded */
    uint64_t messages_transmitted;       /**< Total messages sent via NLP */
    uint64_t messages_received;          /**< Total messages received */
    uint64_t encoding_errors;            /**< Encoding failures */
    uint64_t decoding_errors;            /**< Decoding failures */
    uint64_t transmission_errors;        /**< NLP transmission failures */

    float avg_encoding_time_ms;          /**< Average time to encode */
    float avg_transmission_time_ms;      /**< Average transmission latency */
    float avg_compression_ratio;         /**< Average compression ratio */
    float avg_confidence;                /**< Average encoding confidence */
} language_bridge_stats_t;

//=============================================================================
// Lifecycle Management
//=============================================================================

/**
 * @brief Create language production bridge
 *
 * WHAT: Allocate and initialize bridge for brain-to-brain communication
 * WHY:  Enable connection between Broca's area and NLP protocol
 * HOW:  Allocate structure, setup queues, register with bio-async
 *
 * @param config Bridge configuration (NULL = use defaults)
 * @return Bridge handle or NULL on error
 */
language_production_bridge_t* language_bridge_create(const language_bridge_config_t* config);

/**
 * @brief Destroy language production bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Free memory, close connections, unregister from bio-async
 * HOW:  Flush queues, disconnect, free allocations
 *
 * @param bridge Bridge handle (NULL-safe)
 */
void language_bridge_destroy(language_production_bridge_t* bridge);

//=============================================================================
// Connection Management
//=============================================================================

/**
 * @brief Connect bridge to Broca's area
 *
 * WHAT: Link bridge to Broca's language production module
 * WHY:  Enable thought encoding from Broca's area
 * HOW:  Store Broca context, register callbacks
 *
 * @param bridge Language production bridge
 * @param broca Broca's area context (can be NULL - placeholder)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int language_bridge_connect_broca(language_production_bridge_t* bridge,
                                   broca_context_t* broca);

/**
 * @brief Connect bridge to NLP node
 *
 * WHAT: Link bridge to Neural Link Protocol node
 * WHY:  Enable network transmission of encoded thoughts
 * HOW:  Store NLP node handle, register message callbacks
 *
 * @param bridge Language production bridge
 * @param nlp_node NLP node for network communication
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int language_bridge_connect_nlp(language_production_bridge_t* bridge,
                                 nlp_node_t nlp_node);

//=============================================================================
// Message Production (Encoding & Transmission)
//=============================================================================

/**
 * @brief Encode thought vector into language message
 *
 * WHAT: Convert abstract thought into transmittable message
 * WHY:  Bridge semantic representation to neural encoding
 * HOW:  Semantic embedding → neural spike pattern → compression
 *
 * @param bridge Language production bridge
 * @param thought_vector Abstract thought representation (semantic vector)
 * @param vec_size Size of thought vector
 * @param out_message Output message (pre-allocated)
 * @return NIMCP_SUCCESS on success, error code otherwise
 *
 * ENCODING PIPELINE:
 * 1. Semantic analysis: extract key concepts
 * 2. Neural encoding: convert to spike rates
 * 3. Compression: reduce dimensionality
 * 4. Quality check: verify confidence threshold
 */
int language_bridge_encode_thought(language_production_bridge_t* bridge,
                                    const float* thought_vector,
                                    uint32_t vec_size,
                                    language_message_t* out_message);

/**
 * @brief Transmit language message to specific brain
 *
 * WHAT: Send encoded message via NLP to target brain
 * WHY:  Enable brain-to-brain communication
 * HOW:  Serialize message → NLP packet → network transmission
 *
 * @param bridge Language production bridge
 * @param message Message to transmit
 * @param target_brain_id Target brain identifier (NLP peer ID)
 * @return NIMCP_SUCCESS on success, error code otherwise
 *
 * TRANSMISSION PIPELINE:
 * 1. Serialize message to bytes
 * 2. Compress (if enabled)
 * 3. Encrypt (if enabled)
 * 4. Send via NLP
 * 5. Update statistics
 */
int language_bridge_transmit(language_production_bridge_t* bridge,
                              const language_message_t* message,
                              uint32_t target_brain_id);

/**
 * @brief Broadcast language message to all connected brains
 *
 * WHAT: Send message to all peers in swarm
 * WHY:  Enable group communication (announcements, alerts)
 * HOW:  Iterate peers, transmit to each
 *
 * @param bridge Language production bridge
 * @param message Message to broadcast
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int language_bridge_broadcast(language_production_bridge_t* bridge,
                               const language_message_t* message);

//=============================================================================
// Message Reception (Decoding)
//=============================================================================

/**
 * @brief Decode received message into thought vector
 *
 * WHAT: Convert received message back to thought representation
 * WHY:  Enable understanding of remote brain's communication
 * HOW:  Decrypt → decompress → neural decode → semantic extraction
 *
 * @param bridge Language production bridge
 * @param message Received message
 * @param thought_vector Output thought vector (pre-allocated)
 * @param vec_size Size of output buffer
 * @return NIMCP_SUCCESS on success, error code otherwise
 *
 * DECODING PIPELINE:
 * 1. Decrypt message (if encrypted)
 * 2. Decompress (if compressed)
 * 3. Reconstruct neural pattern
 * 4. Extract semantic content
 * 5. Convert to thought vector
 */
int language_bridge_decode_message(language_production_bridge_t* bridge,
                                    const language_message_t* message,
                                    float* thought_vector,
                                    uint32_t* vec_size);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Process incoming bio-async messages
 *
 * WHAT: Handle messages from bio-router inbox
 * WHY:  Integrate with cognitive modules via bio-async
 * HOW:  Dequeue messages, dispatch to handlers, send responses
 *
 * @param bridge Language production bridge
 * @return Number of messages processed
 *
 * HANDLED MESSAGE TYPES:
 * - BIO_MSG_LANGUAGE_ENCODE_REQUEST: Encode thought
 * - BIO_MSG_LANGUAGE_DECODE_REQUEST: Decode message
 * - BIO_MSG_NLP_MESSAGE_RECEIVED: Incoming NLP message
 */
int language_bridge_process_inbox(language_production_bridge_t* bridge);

//=============================================================================
// Statistics & Monitoring
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Language production bridge
 * @return Statistics structure
 */
language_bridge_stats_t language_bridge_get_stats(language_production_bridge_t* bridge);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Language production bridge
 */
void language_bridge_reset_stats(language_production_bridge_t* bridge);

//=============================================================================
// Message Management
//=============================================================================

/**
 * @brief Create language message
 *
 * @param semantic_content Text/semantic content (copied)
 * @param semantic_size Size of semantic content
 * @param encoding_size Neural encoding dimensionality
 * @return Message handle or NULL on error
 */
language_message_t* language_message_create(const char* semantic_content,
                                             uint32_t semantic_size,
                                             uint32_t encoding_size);

/**
 * @brief Destroy language message
 *
 * @param message Message handle (NULL-safe)
 */
void language_message_destroy(language_message_t* message);

/**
 * @brief Clone language message
 *
 * @param message Message to clone
 * @return Cloned message or NULL on error
 */
language_message_t* language_message_clone(const language_message_t* message);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * @return Default configuration structure
 */
language_bridge_config_t language_bridge_default_config(void);

/**
 * @brief Get human-readable error description
 *
 * @param error_code Error code
 * @return Error description string
 */
const char* language_bridge_error_string(int error_code);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_PRODUCTION_BRIDGE_H */
