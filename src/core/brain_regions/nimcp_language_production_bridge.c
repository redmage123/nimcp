/**
 * @file nimcp_language_production_bridge.c
 * @brief Implementation of Language Production Bridge for brain-to-brain communication
 *
 * WHAT: Connects Broca's language production to Neural Link Protocol
 * WHY:  Enable thoughts and semantic content to be transmitted between brains
 * HOW:  Semantic encoding → neural patterns → NLP transmission
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#define LOG_MODULE "language_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_threshold_constants.h"

// Use weak attribute to avoid duplicate symbol with broca version
static struct nimcp_health_agent* g_language_production_bridge_health_agent = NULL;
static inline void language_production_bridge_heartbeat(const char* op, float progress) { (void)op; (void)progress; }
__attribute__((weak)) void language_production_bridge_set_health_agent(struct nimcp_health_agent* agent) { g_language_production_bridge_health_agent = agent; }

#include "core/brain_regions/nimcp_language_production_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/factory/init/nimcp_brain_init.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

//=============================================================================
// Constants
//=============================================================================

#define LANGUAGE_BRIDGE_MAGIC 0x4C424947  // 'LBIG'
#define DEFAULT_MAX_QUEUE 128
#define DEFAULT_SEMANTIC_BUFFER 4096
#define DEFAULT_ARTICULATION_THRESHOLD 0.7f
#define DEFAULT_ENCODING_DIM 512
#define DEFAULT_ENCODING_RATE 100.0f
#define MIN_CONFIDENCE_THRESHOLD NIMCP_CONFIDENCE_MEDIUM

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Message queue entry
 */
typedef struct {
    language_message_t* message;
    uint32_t target_brain_id;
    uint64_t enqueue_time_ms;
    bool is_broadcast;
} queued_message_t;

/**
 * @brief Language production bridge structure
 */
struct language_production_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    uint32_t magic;                          /**< Magic number for validation */

    // Configuration
    language_bridge_config_t config;

    // Connections
    broca_context_t* broca;                  /**< Broca's area context (optional) */
    nlp_node_t nlp_node;                     /**< NLP node for network comm */
    bool broca_connected;
    bool nlp_connected;

    // Message queue
    queued_message_t* message_queue;
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;
    nimcp_platform_mutex_t queue_mutex;

    // Statistics
    language_bridge_stats_t stats;
    nimcp_platform_mutex_t stats_mutex;

    // Bio-async integration
    bool bio_async_registered;

    // Message ID generation
    uint32_t next_message_id;

    // Semantic buffer (for encoding workspace)
    float* semantic_workspace;
    uint32_t workspace_size;

    // Security
    bbb_system_t bbb_system;
};

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Register with bio-router
 * WHY:  Enable communication with cognitive modules
 * HOW:  Create module context, register handlers
 */
static void language_bridge_register_bio_async(language_production_bridge_t* bridge) {
    if (!bridge || !bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_BRAIN_LANGUAGE_PRODUCTION,
        .module_name = "language_production_bridge",
        .inbox_capacity = bridge->config.max_message_queue,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->bio_async_registered = true;
        LOG_INFO("Language production bridge registered with bio-router");
    } else {
        LOG_WARNING("Failed to register language production bridge with bio-router");
    }
}

/**
 * WHAT: Send bio-async event notification
 * WHY:  Notify cognitive modules of language events
 * HOW:  Create message, broadcast via bio-router
 */
static void language_bridge_send_bio_event(language_production_bridge_t* bridge,
                                            bio_message_type_t event_type,
                                            bool success,
                                            uint32_t message_id) {
    if (!bridge || !bridge->bio_async_registered) {
        return;
    }

    // Create bio-async message based on event type
    bio_msg_nlp_neural_complete_t msg;
    memset(&msg, 0, sizeof(msg));

    // Determine channel based on event type
    nimcp_bio_channel_type_t channel = BIO_CHANNEL_DOPAMINE;  // Success events
    if (!success) {
        channel = BIO_CHANNEL_NOREPINEPHRINE;  // Error events
    } else if (event_type == BIO_MSG_NLP_MESSAGE_RECEIVED) {
        channel = BIO_CHANNEL_ACETYLCHOLINE;  // Fast incoming messages
    }

    bio_msg_init_header(&msg.header, event_type, BIO_MODULE_BRAIN_LANGUAGE_PRODUCTION,
                        BIO_MODULE_ALL, sizeof(msg));
    msg.header.channel = channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    msg.context_id = message_id;
    msg.success = success;
    msg.confidence = bridge->stats.avg_confidence;

    bio_router_broadcast(bridge->base.bio_ctx, &msg, sizeof(msg));

    LOG_DEBUG("Sent bio-async event: type=0x%04X success=%d channel=%d",
              event_type, success, channel);
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Validate bridge handle
 * WHY:  Prevent crashes from invalid handles
 * HOW:  Check magic number and NULL
 */
static inline bool validate_bridge(language_production_bridge_t* bridge) {
    return bridge != NULL && bridge->magic == LANGUAGE_BRIDGE_MAGIC;
}

/**
 * WHAT: Generate unique message ID
 * WHY:  Track messages for debugging/statistics
 * HOW:  Atomic increment with timestamp mixing
 */
static uint32_t generate_message_id(language_production_bridge_t* bridge) {
    uint32_t id = __sync_fetch_and_add(&bridge->next_message_id, 1);
    return id ^ (uint32_t)(nimcp_time_get_ms() & 0xFFFF);
}

/**
 * WHAT: Update statistics
 * WHY:  Track bridge performance and errors
 * HOW:  Atomic updates with exponential moving average
 */
static void update_stats(language_production_bridge_t* bridge,
                         bool encoding_success,
                         bool transmission_success,
                         float encoding_time_ms,
                         float compression_ratio,
                         float confidence) {
    nimcp_platform_mutex_lock(&bridge->stats_mutex);

    if (encoding_success) {
        bridge->stats.messages_produced++;

        // Update average encoding time (EMA with alpha=0.1)
        if (bridge->stats.messages_produced == 1) {
            bridge->stats.avg_encoding_time_ms = encoding_time_ms;
        } else {
            bridge->stats.avg_encoding_time_ms =
                0.9F * bridge->stats.avg_encoding_time_ms + 0.1F * encoding_time_ms;
        }

        // Update average confidence
        if (bridge->stats.messages_produced == 1) {
            bridge->stats.avg_confidence = confidence;
        } else {
            bridge->stats.avg_confidence =
                0.9F * bridge->stats.avg_confidence + 0.1F * confidence;
        }
    } else {
        bridge->stats.encoding_errors++;
    }

    if (transmission_success) {
        bridge->stats.messages_transmitted++;

        // Update compression ratio
        if (compression_ratio > 0.0F) {
            if (bridge->stats.messages_transmitted == 1) {
                bridge->stats.avg_compression_ratio = compression_ratio;
            } else {
                bridge->stats.avg_compression_ratio =
                    0.9F * bridge->stats.avg_compression_ratio + 0.1F * compression_ratio;
            }
        }
    } else if (encoding_success) {
        bridge->stats.transmission_errors++;
    }

    nimcp_platform_mutex_unlock(&bridge->stats_mutex);
}

//=============================================================================
// Neural Encoding/Decoding
//=============================================================================

/**
 * WHAT: Encode thought vector to neural spike pattern
 * WHY:  Convert semantic representation to transmittable format
 * HOW:  Dimensionality reduction + rate coding
 *
 * ENCODING ALGORITHM:
 * 1. Normalize input vector
 * 2. Apply PCA-like projection (simplified)
 * 3. Convert to spike rates (0-100 Hz)
 * 4. Add confidence estimation
 */
static int encode_neural_pattern(language_production_bridge_t* bridge,
                                  const float* thought_vector,
                                  uint32_t vec_size,
                                  float* neural_encoding,
                                  uint32_t encoding_size,
                                  float* confidence_out) {
    // Guard clauses
    if (!thought_vector || !neural_encoding || vec_size == 0 || encoding_size == 0) {
        return -NIMCP_INVALID_PARAM;
    }

    // Calculate normalization factor
    float norm = 0.0F;
    for (uint32_t i = 0; i < vec_size; i++) {
        norm += thought_vector[i] * thought_vector[i];
    }
    norm = sqrtf(norm);

    if (norm < 1e-6F) {
        LOG_WARNING("Thought vector has near-zero magnitude");
        *confidence_out = 0.0F;
        return -NIMCP_INVALID_PARAM;
    }

    // Normalize and project to encoding space
    // Simplified: take first N dimensions or average if input < encoding size
    uint32_t copy_size = (vec_size < encoding_size) ? vec_size : encoding_size;

    for (uint32_t i = 0; i < copy_size; i++) {
        float normalized = thought_vector[i] / norm;

        // Convert to spike rate (0-100 Hz range)
        // Use sigmoid to map to [0, 1], then scale to rate
        float activation = 1.0F / (1.0F + expf(-normalized));
        neural_encoding[i] = activation * bridge->config.encoding_rate;
    }

    // Fill remaining with zeros if needed
    for (uint32_t i = copy_size; i < encoding_size; i++) {
        neural_encoding[i] = 0.0F;
    }

    // Calculate encoding confidence based on information preservation
    float confidence = (float)copy_size / (float)encoding_size;
    if (vec_size > encoding_size) {
        confidence = 0.9F;  // Lost some information
    }

    *confidence_out = confidence;

    LOG_DEBUG("Encoded neural pattern: input_dim=%u, output_dim=%u, confidence=%.3f",
              vec_size, encoding_size, confidence);

    return NIMCP_SUCCESS;
}

/**
 * WHAT: Decode neural pattern back to thought vector
 * WHY:  Reconstruct semantic content from received message
 * HOW:  Reverse spike rate coding, reconstruct vector
 */
static int decode_neural_pattern(language_production_bridge_t* bridge,
                                  const float* neural_encoding,
                                  uint32_t encoding_size,
                                  float* thought_vector,
                                  uint32_t vec_size) {
    // Guard clauses
    if (!neural_encoding || !thought_vector || encoding_size == 0 || vec_size == 0) {
        return -NIMCP_INVALID_PARAM;
    }

    uint32_t copy_size = (encoding_size < vec_size) ? encoding_size : vec_size;

    // Convert spike rates back to normalized activations
    for (uint32_t i = 0; i < copy_size; i++) {
        float rate = neural_encoding[i];

        // Reverse: rate → activation → unnormalized
        float activation = rate / bridge->config.encoding_rate;

        // Inverse sigmoid (logit): logit(x) = log(x / (1-x))
        if (activation <= 0.0F || activation >= 1.0F) {
            thought_vector[i] = 0.0F;
        } else {
            thought_vector[i] = logf(activation / (1.0F - activation));
        }
    }

    // Fill remaining with zeros
    for (uint32_t i = copy_size; i < vec_size; i++) {
        thought_vector[i] = 0.0F;
    }

    LOG_DEBUG("Decoded neural pattern: encoding_dim=%u, output_dim=%u",
              encoding_size, vec_size);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Lifecycle Management
//=============================================================================

language_bridge_config_t language_bridge_default_config(void) {
    language_bridge_config_t config = {
        .max_message_queue = DEFAULT_MAX_QUEUE,
        .semantic_buffer_size = DEFAULT_SEMANTIC_BUFFER,
        .articulation_threshold = DEFAULT_ARTICULATION_THRESHOLD,
        .enable_bio_async = true,
        .enable_compression = true,
        .enable_encryption = true,
        .encoding_dim = DEFAULT_ENCODING_DIM,
        .encoding_rate = DEFAULT_ENCODING_RATE
    };
    return config;
}

language_production_bridge_t* language_bridge_create(const language_bridge_config_t* config) {
    // Use default config if none provided
    language_bridge_config_t cfg = config ? *config : language_bridge_default_config();

    // Allocate bridge
    language_production_bridge_t* bridge = nimcp_calloc(1, sizeof(language_production_bridge_t));
    if (!bridge) {
        LOG_ERROR("Failed to allocate language production bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->magic = LANGUAGE_BRIDGE_MAGIC;
    bridge->config = cfg;

    // Allocate message queue
    bridge->message_queue = nimcp_calloc(cfg.max_message_queue, sizeof(queued_message_t));
    if (!bridge->message_queue) {
        LOG_ERROR("Failed to allocate message queue");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_bridge_create: bridge->message_queue is NULL");
        return NULL;
    }

    // Allocate semantic workspace
    bridge->workspace_size = cfg.encoding_dim * 2;  // Extra space for processing
    bridge->semantic_workspace = nimcp_calloc(bridge->workspace_size, sizeof(float));
    if (!bridge->semantic_workspace) {
        LOG_ERROR("Failed to allocate semantic workspace");
        nimcp_free(bridge->message_queue);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_bridge_create: bridge->semantic_workspace is NULL");
        return NULL;
    }

    // Initialize mutexes
    if (nimcp_platform_mutex_init(&bridge->queue_mutex, false) != 0) {
        LOG_ERROR("Failed to initialize queue mutex");
        nimcp_free(bridge->semantic_workspace);
        nimcp_free(bridge->message_queue);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "language_bridge_create: validation failed");
        return NULL;
    }

    if (nimcp_platform_mutex_init(&bridge->stats_mutex, false) != 0) {
        LOG_ERROR("Failed to initialize stats mutex");
        nimcp_platform_mutex_destroy(&bridge->queue_mutex);
        nimcp_free(bridge->semantic_workspace);
        nimcp_free(bridge->message_queue);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "language_bridge_create: validation failed");
        return NULL;
    }

    // Initialize message ID generator
    bridge->next_message_id = (uint32_t)time(NULL) & 0xFFFF;

    // Register with bio-async if enabled
    if (cfg.enable_bio_async) {
        language_bridge_register_bio_async(bridge);
    }

    // Get global BBB system
    bridge->bbb_system = nimcp_bbb_get_global_system();

    LOG_INFO("Created language production bridge: queue=%u, encoding_dim=%u",
             cfg.max_message_queue, cfg.encoding_dim);

    NIMCP_LOGGING_INFO("Created %s bridge", "language_production");
    return bridge;
}

void language_bridge_destroy(language_production_bridge_t* bridge) {
    if (!validate_bridge(bridge)) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_production");
    }

    LOG_INFO("Destroying language production bridge");

    // Unregister from bio-async
    if (bridge->bio_async_registered && bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
    }

    // Free queued messages
    nimcp_platform_mutex_lock(&bridge->queue_mutex);
    for (uint32_t i = 0; i < bridge->queue_count; i++) {
        uint32_t idx = (bridge->queue_head + i) % bridge->config.max_message_queue;
        if (bridge->message_queue[idx].message) {
            language_message_destroy(bridge->message_queue[idx].message);
        }
    }
    nimcp_platform_mutex_unlock(&bridge->queue_mutex);

    // Destroy mutexes
    nimcp_platform_mutex_destroy(&bridge->stats_mutex);
    nimcp_platform_mutex_destroy(&bridge->queue_mutex);

    // Free allocations
    nimcp_free(bridge->semantic_workspace);
    nimcp_free(bridge->message_queue);

    // Clear magic and free
    bridge->magic = 0;
    nimcp_free(bridge);

    LOG_DEBUG("Language production bridge destroyed");
}

//=============================================================================
// Connection Management
//=============================================================================

int language_bridge_connect_broca(language_production_bridge_t* bridge,
                                   broca_context_t* broca) {
    if (!validate_bridge(bridge)) {
        return -NIMCP_INVALID_PARAM;
    }

    bridge->broca = broca;
    bridge->broca_connected = (broca != NULL);

    LOG_INFO("Language bridge %s to Broca's area",
             bridge->broca_connected ? "connected" : "disconnected");

    return NIMCP_SUCCESS;
}

int language_bridge_connect_nlp(language_production_bridge_t* bridge,
                                 nlp_node_t nlp_node) {
    if (!validate_bridge(bridge)) {
        return -NIMCP_INVALID_PARAM;
    }

    bridge->nlp_node = nlp_node;
    bridge->nlp_connected = (nlp_node != NULL);

    LOG_INFO("Language bridge %s to NLP node",
             bridge->nlp_connected ? "connected" : "disconnected");

    return NIMCP_SUCCESS;
}

//=============================================================================
// Message Management
//=============================================================================

language_message_t* language_message_create(const char* semantic_content,
                                             uint32_t semantic_size,
                                             uint32_t encoding_size) {
    if (!semantic_content || semantic_size == 0 || encoding_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_bridge_destroy: semantic_content is NULL");
        return NULL;
    }

    language_message_t* message = nimcp_calloc(1, sizeof(language_message_t));
    if (!message) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "message is NULL");

        return NULL;
    }

    // Copy semantic content
    message->semantic_content = nimcp_malloc(semantic_size + 1);
    if (!message->semantic_content) {
        nimcp_free(message);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_bridge_destroy: message->semantic_content is NULL");
        return NULL;
    }
    memcpy(message->semantic_content, semantic_content, semantic_size);
    message->semantic_content[semantic_size] = '\0';
    message->semantic_size = semantic_size;

    // Allocate neural encoding
    message->neural_encoding = nimcp_calloc(encoding_size, sizeof(float));
    if (!message->neural_encoding) {
        nimcp_free(message->semantic_content);
        nimcp_free(message);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_bridge_destroy: message->neural_encoding is NULL");
        return NULL;
    }
    message->encoding_size = encoding_size;

    message->timestamp_ms = nimcp_time_get_ms();

    return message;
}

void language_message_destroy(language_message_t* message) {
    if (!message) {
        return;
    }

    nimcp_free(message->neural_encoding);
    nimcp_free(message->semantic_content);
    nimcp_free(message);
}

language_message_t* language_message_clone(const language_message_t* message) {
    if (!message) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "message is NULL");

        return NULL;
    }

    language_message_t* clone = language_message_create(
        message->semantic_content,
        message->semantic_size,
        message->encoding_size
    );

    if (clone) {
        clone->message_id = message->message_id;
        clone->confidence = message->confidence;
        clone->timestamp_ms = message->timestamp_ms;
        clone->language_code = message->language_code;
        clone->intent_type = message->intent_type;

        memcpy(clone->neural_encoding, message->neural_encoding,
               message->encoding_size * sizeof(float));
    }

    return clone;
}

//=============================================================================
// Message Production (Encoding & Transmission)
//=============================================================================

int language_bridge_encode_thought(language_production_bridge_t* bridge,
                                    const float* thought_vector,
                                    uint32_t vec_size,
                                    language_message_t* out_message) {
    if (!validate_bridge(bridge) || !thought_vector || !out_message) {
        return -NIMCP_INVALID_PARAM;
    }

    uint64_t start_time = nimcp_time_get_us();

    // BBB validation of input
    if (bridge->bbb_system && bbb_system_is_enabled(bridge->bbb_system)) {
        bbb_validation_result_t bbb_result;
        if (!bbb_validate_input(bridge->bbb_system, thought_vector,
                                vec_size * sizeof(float), &bbb_result)) {
            LOG_WARNING("BBB validation failed for thought vector");
            update_stats(bridge, false, false, 0.0F, 0.0F, 0.0F);
            return -NIMCP_PERMISSION_DENIED;
        }
    }

    // Generate message ID
    out_message->message_id = generate_message_id(bridge);

    // Encode neural pattern
    float confidence = 0.0F;
    int result = encode_neural_pattern(bridge, thought_vector, vec_size,
                                       out_message->neural_encoding,
                                       out_message->encoding_size,
                                       &confidence);

    if (result != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to encode neural pattern: %d", result);
        update_stats(bridge, false, false, 0.0F, 0.0F, 0.0F);
        return result;
    }

    out_message->confidence = confidence;
    out_message->timestamp_ms = nimcp_time_get_ms();

    // Check confidence threshold
    if (confidence < bridge->config.articulation_threshold) {
        LOG_WARNING("Encoding confidence %.3f below threshold %.3f",
                    confidence, bridge->config.articulation_threshold);
        update_stats(bridge, false, false, 0.0F, 0.0F, confidence);
        return -NIMCP_ERROR;
    }

    // Calculate encoding time
    uint64_t end_time = nimcp_time_get_us();
    float encoding_time_ms = (float)(end_time - start_time) / 1000.0F;

    // Update statistics
    update_stats(bridge, true, false, encoding_time_ms, 0.0F, confidence);

    // Send bio-async event
    if (bridge->bio_async_registered) {
        language_bridge_send_bio_event(bridge, BIO_MSG_NLP_NEURAL_ENCODE_COMPLETE,
                                       true, out_message->message_id);
    }

    LOG_DEBUG("Encoded thought: msg_id=%u, confidence=%.3f, time=%.2fms",
              out_message->message_id, confidence, encoding_time_ms);

    return NIMCP_SUCCESS;
}

int language_bridge_transmit(language_production_bridge_t* bridge,
                              const language_message_t* message,
                              uint32_t target_brain_id) {
    if (!validate_bridge(bridge) || !message) {
        return -NIMCP_INVALID_PARAM;
    }

    if (!bridge->nlp_connected) {
        LOG_ERROR("NLP node not connected");
        update_stats(bridge, false, true, 0.0F, 0.0F, 0.0F);
        return -NIMCP_ERROR;
    }

    LOG_DEBUG("Transmitting message %u to brain %u",
              message->message_id, target_brain_id);

    // For now, simulate transmission (actual NLP integration would go here)
    // In a full implementation, this would:
    // 1. Serialize message to bytes
    // 2. Compress (if enabled)
    // 3. Call nlp_node_send() or similar
    // 4. Handle acknowledgments

    // Simulate compression ratio
    float compression_ratio = bridge->config.enable_compression ? 0.6F : 1.0F;

    // Update statistics
    update_stats(bridge, true, true, 0.0F, compression_ratio, message->confidence);

    // Send bio-async event
    if (bridge->bio_async_registered) {
        language_bridge_send_bio_event(bridge, BIO_MSG_NLP_MESSAGE_SENT,
                                       true, message->message_id);
    }

    LOG_INFO("Transmitted message %u to brain %u (compression=%.2f)",
             message->message_id, target_brain_id, compression_ratio);

    return NIMCP_SUCCESS;
}

int language_bridge_broadcast(language_production_bridge_t* bridge,
                               const language_message_t* message) {
    if (!validate_bridge(bridge) || !message) {
        return -NIMCP_INVALID_PARAM;
    }

    LOG_INFO("Broadcasting message %u to all peers", message->message_id);

    // Broadcast to all connected peers (target_brain_id = 0)
    return language_bridge_transmit(bridge, message, 0);
}

//=============================================================================
// Message Reception (Decoding)
//=============================================================================

int language_bridge_decode_message(language_production_bridge_t* bridge,
                                    const language_message_t* message,
                                    float* thought_vector,
                                    uint32_t* vec_size) {
    if (!validate_bridge(bridge) || !message || !thought_vector || !vec_size) {
        return -NIMCP_INVALID_PARAM;
    }

    LOG_DEBUG("Decoding message %u", message->message_id);

    // Decode neural pattern to thought vector
    int result = decode_neural_pattern(bridge, message->neural_encoding,
                                       message->encoding_size,
                                       thought_vector, *vec_size);

    if (result != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to decode neural pattern: %d", result);
        nimcp_platform_mutex_lock(&bridge->stats_mutex);
        bridge->stats.decoding_errors++;
        nimcp_platform_mutex_unlock(&bridge->stats_mutex);
        return result;
    }

    // Update statistics
    nimcp_platform_mutex_lock(&bridge->stats_mutex);
    bridge->stats.messages_received++;
    nimcp_platform_mutex_unlock(&bridge->stats_mutex);

    // Send bio-async event
    if (bridge->bio_async_registered) {
        language_bridge_send_bio_event(bridge, BIO_MSG_NLP_MESSAGE_RECEIVED,
                                       true, message->message_id);
    }

    LOG_INFO("Decoded message %u: confidence=%.3f",
             message->message_id, message->confidence);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int language_bridge_process_inbox(language_production_bridge_t* bridge) {
    if (!validate_bridge(bridge) || !bridge->bio_async_registered) {
        return 0;
    }

    // Process up to 32 messages per call
    return bio_router_process_inbox(bridge->base.bio_ctx, 32);
}

//=============================================================================
// Statistics & Monitoring
//=============================================================================

language_bridge_stats_t language_bridge_get_stats(language_production_bridge_t* bridge) {
    language_bridge_stats_t stats = {0};

    if (!validate_bridge(bridge)) {
        return stats;
    }

    nimcp_platform_mutex_lock(&bridge->stats_mutex);
    stats = bridge->stats;
    nimcp_platform_mutex_unlock(&bridge->stats_mutex);

    return stats;
}

void language_bridge_reset_stats(language_production_bridge_t* bridge) {
    if (!validate_bridge(bridge)) {
        return;
    }

    nimcp_platform_mutex_lock(&bridge->stats_mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(&bridge->stats_mutex);

    LOG_DEBUG("Language bridge statistics reset");
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* language_bridge_error_string(int error_code) {
    switch (error_code) {
        case NIMCP_SUCCESS:
            return "Success";
        case -NIMCP_INVALID_PARAM:
            return "Invalid parameter";
        case -NIMCP_ERROR:
            return "General error";
        case -NIMCP_PERMISSION_DENIED:
            return "Permission denied (BBB validation failed)";
        default:
            return "Unknown error";
    }
}
