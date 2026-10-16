#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_nlp_protocol_bridge.c - Neural Language to NLP Protocol Bridge
//=============================================================================
/**
 * @file nimcp_nlp_protocol_bridge.c
 * @brief Integrates Neural Language on top of Neural Link Protocol
 *
 * WHAT: Bridge layer connecting semantic neural language to transport protocol
 * WHY:  Enable meaningful brain-to-brain communication with compression
 * HOW:  Serialize expressions, apply compression, send via NLP protocol
 *
 * ARCHITECTURE:
 *   Neural Language Expression
 *           |
 *           v
 *   [Serialization] - nlang_expr_serialize()
 *           |
 *           v
 *   [Compression] - nlp_compress() / nimcp_compress()
 *           |
 *           v
 *   [Encryption] - NLP crypto layer
 *           |
 *           v
 *   NLP Protocol Transport
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "networking/nlp/nimcp_neural_language.h"
#include "utils/serialization/nimcp_serialization.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "security/nimcp_bbb_helpers.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(nlp_protocol_bridge)

#define NLP_BRIDGE_MODULE "nlp_bridge"

//=============================================================================
// Forward declarations for NLP compression (from nimcp_nlp_compression.c)
//=============================================================================

extern int nlp_compress(nlp_msg_type_t msg_type,
                        const uint8_t* input, size_t input_len,
                        uint8_t* output, size_t output_max);

extern int nlp_decompress(const uint8_t* input, size_t input_len,
                          uint8_t* output, size_t output_max);

//=============================================================================
// Protocol Bridge Structure
//=============================================================================

/**
 * @brief Neural language protocol bridge
 *
 * Manages the connection between neural language expressions
 * and the underlying NLP transport protocol.
 */
typedef struct nlp_protocol_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    nlp_node_t node;                    // Underlying NLP node
    nlang_shared_context_t context;     // Shared context for compression
    bool use_nlp_compression;           // Use NLP-specific compression
    bool use_utils_compression;         // Use utils/serialization compression
    uint64_t expressions_sent;          // Statistics
    uint64_t expressions_received;
    uint64_t bytes_saved;               // Compression savings
} nlp_protocol_bridge_t;

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create protocol bridge for neural language
 *
 * @param node NLP node to use for transport
 * @return Bridge instance or NULL on failure
 */
nlp_protocol_bridge_t* nlp_bridge_create(nlp_node_t node) {
    if (!node) {
        NIMCP_LOGGING_ERROR("nlp_bridge", "NULL node provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;
    }

    nlp_protocol_bridge_t* bridge = nimcp_calloc(1, sizeof(nlp_protocol_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("nlp_bridge", "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->node = node;
    nlang_context_init(&bridge->context);
    bridge->use_nlp_compression = true;   // Prefer NLP-specific compression
    bridge->use_utils_compression = false; // Fallback

    // Register with Blood-Brain Barrier for security auditing
    bbb_register_module(NLP_BRIDGE_MODULE, BBB_MODULE_TYPE_NETWORK);

    bbb_audit_log(BBB_AUDIT_INFO, NLP_BRIDGE_MODULE, "bridge_created",
                  "Protocol bridge created for node %p", (void*)node);
    NIMCP_LOGGING_INFO("nlp_bridge", "Protocol bridge created");
    return bridge;
}

/**
 * @brief Destroy protocol bridge
 */
void nlp_bridge_destroy(nlp_protocol_bridge_t* bridge) {
    if (!bridge) return;

    NIMCP_LOGGING_INFO("nlp_bridge", "Bridge stats: sent=%lu, received=%lu, bytes_saved=%lu",
                   bridge->expressions_sent, bridge->expressions_received,
                   bridge->bytes_saved);

    nimcp_free(bridge);
}

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Set compression mode
 *
 * @param bridge Protocol bridge
 * @param use_nlp_compression Use NLP-specific compression (RLE, delta, dict)
 * @param use_utils_compression Use utils zlib compression as fallback
 */
void nlp_bridge_set_compression(nlp_protocol_bridge_t* bridge,
                                bool use_nlp_compression,
                                bool use_utils_compression) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_bridge_set_compression: bridge is NULL");
        return;
    }

    bridge->use_nlp_compression = use_nlp_compression;
    bridge->use_utils_compression = use_utils_compression;

    NIMCP_LOGGING_DEBUG("nlp_bridge", "Compression: nlp=%d, utils=%d",
                    use_nlp_compression, use_utils_compression);
}

/**
 * @brief Set shared context reference point
 *
 * All short coordinates will be relative to this point.
 *
 * @param bridge Protocol bridge
 * @param lat_deg Reference latitude
 * @param lon_deg Reference longitude
 */
void nlp_bridge_set_reference_point(nlp_protocol_bridge_t* bridge,
                                    double lat_deg, double lon_deg) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_bridge_set_reference_point: bridge is NULL");
        return;
    }

    nlang_context_set_reference(&bridge->context, lat_deg, lon_deg);
}

/**
 * @brief Define context slot for compression
 *
 * @param bridge Protocol bridge
 * @param slot Slot number (0-7)
 * @param primitives Primitive sequence to store
 * @param count Number of primitives
 * @return 0 on success
 */
int nlp_bridge_define_context(nlp_protocol_bridge_t* bridge,
                              uint8_t slot,
                              const uint8_t* primitives,
                              size_t count) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    return nlang_context_define(&bridge->context, slot, primitives, count);
}

//=============================================================================
// Send Operations
//=============================================================================

/**
 * @brief Send neural language expression to peer
 *
 * Serializes the expression, applies compression, and sends via NLP.
 *
 * @param bridge Protocol bridge
 * @param peer_id Target peer ID
 * @param expr Expression to send
 * @return 0 on success, -1 on error
 */
int nlp_bridge_send_expression(nlp_protocol_bridge_t* bridge,
                               uint64_t peer_id,
                               const nlang_expression_t* expr) {
    if (!bridge || !expr) {
        NIMCP_LOGGING_ERROR("nlp_bridge", "Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_bridge: required parameter is NULL (bridge, expr)");
        return -1;
    }

    // Validate expression
    if (!nlang_expr_validate(expr)) {
        NIMCP_LOGGING_ERROR("nlp_bridge", "Invalid expression");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_bridge: nlang_expr_validate failed");
        return -1;
    }

    // Serialize expression
    uint8_t serial_buf[256];
    int serial_len = nlang_expr_serialize(expr, serial_buf, sizeof(serial_buf));
    if (serial_len < 0) {
        NIMCP_LOGGING_ERROR("nlp_bridge", "Failed to serialize expression");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_bridge: serialization failed");
        return -1;
    }

    // Apply compression
    uint8_t* send_data = serial_buf;
    size_t send_len = (size_t)serial_len;
    uint8_t compress_buf[512];
    bool compressed = false;

    if (bridge->use_nlp_compression) {
        int comp_len = nlp_compress(NLP_MSG_STATE_SYNC, serial_buf, (size_t)serial_len,
                                    compress_buf, sizeof(compress_buf));
        if (comp_len > 0 && (size_t)comp_len < (size_t)serial_len) {
            send_data = compress_buf;
            send_len = (size_t)comp_len;
            compressed = true;
            bridge->bytes_saved += serial_len - comp_len;
        }
    }
    // Note: utils compression fallback removed - use NLP-specific compression only

    // Send via NLP protocol
    // The message type indicates whether it's compressed
    nlp_msg_type_t msg_type = NLP_MSG_STATE_SYNC;

    int result = nlp_send(bridge->node, (uint32_t)peer_id, msg_type,
                          send_data, send_len, NLP_PRIORITY_NORMAL);

    if (result == 0) {
        bridge->expressions_sent++;
        NIMCP_LOGGING_DEBUG("nlp_bridge", "Sent expression: %zu bytes (compressed=%d)",
                        send_len, compressed);
    }

    return result;
}

/**
 * @brief Broadcast neural language expression to all peers
 *
 * @param bridge Protocol bridge
 * @param expr Expression to broadcast
 * @return 0 on success
 */
int nlp_bridge_broadcast_expression(nlp_protocol_bridge_t* bridge,
                                    const nlang_expression_t* expr) {
    if (!bridge || !expr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_bridge: required parameter is NULL (bridge, expr)");
        return -1;
    }

    // Validate expression
    if (!nlang_expr_validate(expr)) {
        NIMCP_LOGGING_ERROR("nlp_bridge", "Invalid expression for broadcast");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_bridge: nlang_expr_validate failed");
        return -1;
    }

    // Serialize
    uint8_t serial_buf[256];
    int serial_len = nlang_expr_serialize(expr, serial_buf, sizeof(serial_buf));
    if (serial_len < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_bridge: serialization failed");
        return -1;
    }

    // Apply compression
    uint8_t compress_buf[512];
    uint8_t* send_data = serial_buf;
    size_t send_len = (size_t)serial_len;

    if (bridge->use_nlp_compression) {
        int comp_len = nlp_compress(NLP_MSG_PEER_ANNOUNCE, serial_buf, (size_t)serial_len,
                                    compress_buf, sizeof(compress_buf));
        if (comp_len > 0 && (size_t)comp_len < (size_t)serial_len) {
            send_data = compress_buf;
            send_len = (size_t)comp_len;
            bridge->bytes_saved += serial_len - comp_len;
        }
    }

    // Broadcast
    int result = nlp_broadcast(bridge->node, NLP_MSG_PEER_ANNOUNCE,
                               send_data, send_len, NLP_PRIORITY_NORMAL);

    if (result == 0) {
        bridge->expressions_sent++;
    }

    return result;
}

/**
 * @brief Send urgent command expression
 *
 * @param bridge Protocol bridge
 * @param peer_id Target peer (0 for broadcast)
 * @param expr Expression to send
 * @return 0 on success
 */
int nlp_bridge_send_urgent(nlp_protocol_bridge_t* bridge,
                           uint64_t peer_id,
                           const nlang_expression_t* expr) {
    if (!bridge || !expr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, expr)");
        return -1;
    }

    // Serialize (no compression for urgent - speed matters)
    uint8_t serial_buf[256];
    int serial_len = nlang_expr_serialize(expr, serial_buf, sizeof(serial_buf));
    if (serial_len < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    int result;
    if (peer_id == 0) {
        result = nlp_broadcast(bridge->node, NLP_MSG_EMERGENCY,
                               serial_buf, (size_t)serial_len,
                               NLP_PRIORITY_CRITICAL);
    } else {
        result = nlp_send(bridge->node, (uint32_t)peer_id, NLP_MSG_PRIORITY_CMD,
                          serial_buf, (size_t)serial_len,
                          NLP_PRIORITY_CRITICAL);
    }

    if (result == 0) {
        bridge->expressions_sent++;
    }

    return result;
}

//=============================================================================
// Receive Operations
//=============================================================================

/**
 * @brief Receive callback context (stored in bridge)
 */
typedef struct {
    nlp_protocol_bridge_t* bridge;
    void (*callback)(const nlang_expression_t* expr,
                    uint64_t sender_id,
                    void* user_data);
    void* user_data;
} nlp_bridge_rx_context_t;

// Global context for callback (set via nlp_bridge_set_receive_callback)
static nlp_bridge_rx_context_t g_rx_ctx;

/**
 * @brief Internal receive callback matching nlp_message_callback_t signature
 */
static void bridge_receive_callback(nlp_node_t node,
                                    const nlp_peer_t* peer,
                                    const nlp_message_t* msg,
                                    void* user_data) {
    (void)node;
    (void)user_data;

    if (!peer || !msg || !g_rx_ctx.callback) return;

    const uint8_t* payload = msg->payload;
    size_t payload_len = msg->header.payload_len;
    uint64_t sender_id = msg->header.sender_id;

    // Decompress if needed
    uint8_t decomp_buf[512];
    const uint8_t* data = payload;
    size_t data_len = payload_len;

    // Try NLP decompression first (check for compression header)
    if (payload && payload_len > 8) {
        int decomp_len = nlp_decompress(payload, payload_len,
                                        decomp_buf, sizeof(decomp_buf));
        if (decomp_len > 0) {
            data = decomp_buf;
            data_len = (size_t)decomp_len;
        }
    }

    if (!data || data_len == 0) return;

    // Deserialize expression
    nlang_expression_t expr;
    int consumed = nlang_expr_deserialize(data, data_len, &expr);
    if (consumed < 0) {
        NIMCP_LOGGING_WARN("nlp_bridge", "Failed to deserialize expression from peer %lu",
                       sender_id);
        return;
    }

    // Verify checksum
    if (!nlang_expr_verify_checksum(&expr)) {
        NIMCP_LOGGING_WARN("nlp_bridge", "Checksum mismatch from peer %lu", sender_id);
        return;
    }

    if (g_rx_ctx.bridge) {
        g_rx_ctx.bridge->expressions_received++;
    }

    // Invoke user callback
    g_rx_ctx.callback(&expr, sender_id, g_rx_ctx.user_data);
}

/**
 * @brief Register expression receive callback
 *
 * @param bridge Protocol bridge
 * @param callback Function to call when expression received
 * @param user_data User context for callback
 * @return 0 on success
 */
int nlp_bridge_set_receive_callback(nlp_protocol_bridge_t* bridge,
                                    void (*callback)(const nlang_expression_t* expr,
                                                    uint64_t sender_id,
                                                    void* user_data),
                                    void* user_data) {
    if (!bridge || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, callback)");
        return -1;
    }

    // Store callback context in global (single bridge supported)
    g_rx_ctx.bridge = bridge;
    g_rx_ctx.callback = callback;
    g_rx_ctx.user_data = user_data;

    // Register with NLP node using correct API
    nlp_set_message_callback(bridge->node, bridge_receive_callback);
    return 0;
}

//=============================================================================
// Template Send Helpers
//=============================================================================

/**
 * @brief Send "move to" command
 */
int nlp_bridge_send_move_to(nlp_protocol_bridge_t* bridge,
                            uint64_t peer_id,
                            int16_t lat_m, int16_t lon_m,
                            bool urgent) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nlang_expression_t expr;
    nlang_template_move_to(&expr, lat_m, lon_m,
                           urgent ? NLANG_PAD_URGENT : NLANG_PAD_NEUTRAL);

    if (urgent) {
        return nlp_bridge_send_urgent(bridge, peer_id, &expr);
    }
    return nlp_bridge_send_expression(bridge, peer_id, &expr);
}

/**
 * @brief Send threat report
 */
int nlp_bridge_send_threat_report(nlp_protocol_bridge_t* bridge,
                                  int16_t lat_m, int16_t lon_m,
                                  uint8_t threat_type) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nlang_expression_t expr;
    nlang_template_threat_report(&expr, lat_m, lon_m, threat_type);

    // Threat reports are always broadcast and urgent
    return nlp_bridge_send_urgent(bridge, 0, &expr);
}

/**
 * @brief Send victim report (SAR)
 */
int nlp_bridge_send_victim_report(nlp_protocol_bridge_t* bridge,
                                  int16_t lat_m, int16_t lon_m,
                                  uint8_t triage, uint8_t count) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nlang_expression_t expr;
    nlang_template_victim_report(&expr, lat_m, lon_m, triage, count);

    // Victim reports are broadcast
    return nlp_bridge_broadcast_expression(bridge, &expr);
}

/**
 * @brief Send status query
 */
int nlp_bridge_send_status_query(nlp_protocol_bridge_t* bridge,
                                 uint64_t peer_id,
                                 nlang_query_t query_type) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nlang_expression_t expr;
    nlang_template_status_query(&expr, query_type);

    return nlp_bridge_send_expression(bridge, peer_id, &expr);
}

/**
 * @brief Send acknowledgment
 */
int nlp_bridge_send_ack(nlp_protocol_bridge_t* bridge,
                        uint64_t peer_id,
                        bool success) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nlang_expression_t expr;
    nlang_template_acknowledge(&expr, success,
                               success ? NLANG_PAD_CONFIDENT : NLANG_PAD_CAUTIOUS);

    return nlp_bridge_send_expression(bridge, peer_id, &expr);
}

/**
 * @brief Send help request
 */
int nlp_bridge_send_help_request(nlp_protocol_bridge_t* bridge,
                                 bool desperate) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nlang_expression_t expr;
    nlang_template_help_request(&expr,
                                desperate ? NLANG_PAD_DESPERATE : NLANG_PAD_URGENT);

    // Help requests are always broadcast and urgent
    return nlp_bridge_send_urgent(bridge, 0, &expr);
}

//=============================================================================
// Context Synchronization
//=============================================================================

/**
 * @brief Synchronize shared context with peer
 *
 * Sends current context state to peer for compression optimization.
 *
 * @param bridge Protocol bridge
 * @param peer_id Target peer (0 for broadcast)
 * @return 0 on success
 */
int nlp_bridge_sync_context(nlp_protocol_bridge_t* bridge, uint64_t peer_id) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    uint8_t ctx_buf[256];
    int ctx_len = nlang_context_serialize(&bridge->context, ctx_buf, sizeof(ctx_buf));
    if (ctx_len < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_bridge_sync_context: validation failed");
        return -1;
    }

    if (peer_id == 0) {
        return nlp_broadcast(bridge->node, NLP_MSG_STATE_SYNC,
                             ctx_buf, (size_t)ctx_len, NLP_PRIORITY_LOW);
    }
    return nlp_send(bridge->node, (uint32_t)peer_id, NLP_MSG_STATE_SYNC,
                    ctx_buf, (size_t)ctx_len, NLP_PRIORITY_LOW);
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 */
void nlp_bridge_get_stats(nlp_protocol_bridge_t* bridge,
                          uint64_t* expressions_sent,
                          uint64_t* expressions_received,
                          uint64_t* bytes_saved) {
    if (!bridge) return;

    if (expressions_sent) *expressions_sent = bridge->expressions_sent;
    if (expressions_received) *expressions_received = bridge->expressions_received;
    if (bytes_saved) *bytes_saved = bridge->bytes_saved;
}

/**
 * @brief Calculate compression ratio
 */
float nlp_bridge_get_compression_ratio(nlp_protocol_bridge_t* bridge) {
    if (!bridge || bridge->expressions_sent == 0) return 1.0F;

    // Estimate: average expression is ~20 bytes uncompressed
    size_t estimated_uncompressed = bridge->expressions_sent * 20;
    size_t estimated_compressed = estimated_uncompressed - bridge->bytes_saved;

    if (estimated_uncompressed == 0) return 1.0F;
    return (float)estimated_compressed / (float)estimated_uncompressed;
}
