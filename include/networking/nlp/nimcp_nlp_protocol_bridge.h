//=============================================================================
// nimcp_nlp_protocol_bridge.h - Neural Language to NLP Protocol Bridge API
//=============================================================================
/**
 * @file nimcp_nlp_protocol_bridge.h
 * @brief High-level API for sending neural language over NLP protocol
 *
 * WHAT: Bridge connecting neural language expressions to NLP transport
 * WHY:  Enable semantic brain-to-brain communication with compression
 * HOW:  Serialize → Compress → Encrypt → Transport
 *
 * USAGE:
 *   // Create bridge on top of NLP node
 *   nlp_protocol_bridge_t* bridge = nlp_bridge_create(node);
 *
 *   // Set reference point for coordinate compression
 *   nlp_bridge_set_reference_point(bridge, 45.5, -122.6);
 *
 *   // Send commands using templates
 *   nlp_bridge_send_move_to(bridge, peer_id, 100, 200, false);
 *   nlp_bridge_send_threat_report(bridge, 50, 75, PERCEPT_ENEMY);
 *
 *   // Or build custom expressions
 *   nlang_expression_t expr;
 *   nlang_expr_init(&expr, INTENT_INFORM);
 *   nlang_expr_add(&expr, PERCEPT_TARGET);
 *   nlang_expr_finalize(&expr);
 *   nlp_bridge_send_expression(bridge, peer_id, &expr);
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#ifndef NIMCP_NLP_PROTOCOL_BRIDGE_H
#define NIMCP_NLP_PROTOCOL_BRIDGE_H

#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "networking/nlp/nimcp_neural_language.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Opaque Types
//=============================================================================

typedef struct nlp_protocol_bridge_struct nlp_protocol_bridge_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create protocol bridge for neural language
 *
 * @param node NLP node to use for transport
 * @return Bridge instance or NULL on failure
 */
nlp_protocol_bridge_t* nlp_bridge_create(nlp_node_t node);

/**
 * @brief Destroy protocol bridge
 */
void nlp_bridge_destroy(nlp_protocol_bridge_t* bridge);

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
                                bool use_utils_compression);

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
                                    double lat_deg, double lon_deg);

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
                              size_t count);

//=============================================================================
// Send Operations
//=============================================================================

/**
 * @brief Send neural language expression to peer
 *
 * @param bridge Protocol bridge
 * @param peer_id Target peer ID
 * @param expr Expression to send
 * @return 0 on success, -1 on error
 */
int nlp_bridge_send_expression(nlp_protocol_bridge_t* bridge,
                               uint64_t peer_id,
                               const nlang_expression_t* expr);

/**
 * @brief Broadcast neural language expression to all peers
 *
 * @param bridge Protocol bridge
 * @param expr Expression to broadcast
 * @return 0 on success
 */
int nlp_bridge_broadcast_expression(nlp_protocol_bridge_t* bridge,
                                    const nlang_expression_t* expr);

/**
 * @brief Send urgent command expression (no compression, high priority)
 *
 * @param bridge Protocol bridge
 * @param peer_id Target peer (0 for broadcast)
 * @param expr Expression to send
 * @return 0 on success
 */
int nlp_bridge_send_urgent(nlp_protocol_bridge_t* bridge,
                           uint64_t peer_id,
                           const nlang_expression_t* expr);

//=============================================================================
// Template Send Helpers
//=============================================================================

/**
 * @brief Send "move to" command
 */
int nlp_bridge_send_move_to(nlp_protocol_bridge_t* bridge,
                            uint64_t peer_id,
                            int16_t lat_m, int16_t lon_m,
                            bool urgent);

/**
 * @brief Send threat report (broadcast, urgent)
 */
int nlp_bridge_send_threat_report(nlp_protocol_bridge_t* bridge,
                                  int16_t lat_m, int16_t lon_m,
                                  uint8_t threat_type);

/**
 * @brief Send victim report for SAR (broadcast)
 */
int nlp_bridge_send_victim_report(nlp_protocol_bridge_t* bridge,
                                  int16_t lat_m, int16_t lon_m,
                                  uint8_t triage, uint8_t count);

/**
 * @brief Send status query
 */
int nlp_bridge_send_status_query(nlp_protocol_bridge_t* bridge,
                                 uint64_t peer_id,
                                 nlang_query_t query_type);

/**
 * @brief Send acknowledgment
 */
int nlp_bridge_send_ack(nlp_protocol_bridge_t* bridge,
                        uint64_t peer_id,
                        bool success);

/**
 * @brief Send help request (broadcast, urgent)
 */
int nlp_bridge_send_help_request(nlp_protocol_bridge_t* bridge,
                                 bool desperate);

//=============================================================================
// Receive Operations
//=============================================================================

/**
 * @brief Expression receive callback type
 */
typedef void (*nlp_bridge_receive_callback_t)(const nlang_expression_t* expr,
                                              uint64_t sender_id,
                                              void* user_data);

/**
 * @brief Register expression receive callback
 *
 * @param bridge Protocol bridge
 * @param callback Function to call when expression received
 * @param user_data User context for callback
 * @return 0 on success
 */
int nlp_bridge_set_receive_callback(nlp_protocol_bridge_t* bridge,
                                    nlp_bridge_receive_callback_t callback,
                                    void* user_data);

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
int nlp_bridge_sync_context(nlp_protocol_bridge_t* bridge, uint64_t peer_id);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 */
void nlp_bridge_get_stats(nlp_protocol_bridge_t* bridge,
                          uint64_t* expressions_sent,
                          uint64_t* expressions_received,
                          uint64_t* bytes_saved);

/**
 * @brief Get compression ratio (0.0-1.0, lower is better)
 */
float nlp_bridge_get_compression_ratio(nlp_protocol_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NLP_PROTOCOL_BRIDGE_H
