/**
 * @file nimcp_predictive_protocol.h
 * @brief Predictive Communication Protocol for NIMCP
 *
 * WHAT: Protocol that predicts and prefetches messages before they're sent
 * WHY:  Reduce latency by anticipating communication patterns between modules
 * HOW:  Learn historical sequences, predict future messages, prefetch data
 *
 * CONCEPT:
 * The predictive protocol learns message patterns across brain modules and
 * swarm agents, then uses this knowledge to:
 * 1. Predict which messages will be sent next
 * 2. Prefetch data that will likely be needed
 * 3. Preemptively prepare communication channels
 * 4. Learn from prediction errors to improve accuracy
 *
 * INTEGRATION:
 * - Works with NLP message routing
 * - Uses predictive coding module for prediction generation
 * - Monitors swarm coordination patterns
 * - Learns from training feedback
 *
 * ARCHITECTURE:
 * Pattern Learning → Sequence Tracking → Prediction Generation → Prefetch
 *                                                                     ↓
 * Bio-Async Events ← Accuracy Tracking ← Prediction Verification ←───┘
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_PREDICTIVE_PROTOCOL_H
#define NIMCP_PREDICTIVE_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Named Constants
//=============================================================================

#define PRED_PROTO_MAX_SEQUENCE_LENGTH 32    /**< Max tracked sequence length */
#define PRED_PROTO_MAX_PREDICTIONS 64        /**< Max concurrent predictions */
#define PRED_PROTO_DEFAULT_WINDOW_MS 1000    /**< Default prediction window */
#define PRED_PROTO_DEFAULT_HISTORY_SIZE 512  /**< Default history buffer */
#define PRED_PROTO_MIN_CONFIDENCE 0.1f       /**< Minimum useful confidence */
#define PRED_PROTO_MAX_CONFIDENCE 1.0f       /**< Maximum confidence */
#define PRED_PROTO_DEFAULT_CONFIDENCE 0.5f   /**< Default threshold */
#define PRED_PROTO_PREFETCH_BUFFER_SIZE 4096 /**< Default prefetch buffer */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Opaque handle to predictive protocol
 *
 * WHAT: Complete predictive communication system
 * WHY:  Encapsulation of learning and prediction state
 * HOW:  Opaque pointer pattern
 */
typedef struct predictive_protocol predictive_protocol_t;

/**
 * @brief Predictive protocol configuration
 *
 * WHAT: Hyperparameters for predictive communication
 * WHY:  Flexible configuration for different scenarios
 * HOW:  Struct with learning rates and prediction windows
 */
typedef struct {
    uint32_t prediction_window_ms;      /**< How far ahead to predict (ms) */
    uint32_t history_buffer_size;       /**< Historical patterns to track */
    float confidence_threshold;         /**< Min confidence for action (0-1) */
    bool enable_prefetch;               /**< Enable data prefetching */
    bool enable_bio_async;              /**< Enable bio-async integration */
} predictive_protocol_config_t;

/**
 * @brief Predicted message structure
 *
 * WHAT: A predicted future message with confidence
 * WHY:  Represent anticipated communication before it happens
 * HOW:  Store message metadata and predicted payload
 */
typedef struct {
    uint32_t message_type;              /**< Predicted message type */
    uint32_t source_module;             /**< Expected source module */
    uint32_t target_module;             /**< Expected target module */
    float confidence;                   /**< Prediction confidence (0-1) */
    uint64_t predicted_time_ms;         /**< When message will arrive (ms) */
    void* predicted_payload;            /**< Predicted payload data */
    uint32_t payload_size;              /**< Size of predicted payload */
} predicted_message_t;

/**
 * @brief Predictive protocol statistics
 *
 * WHAT: Performance metrics for prediction system
 * WHY:  Track accuracy and optimize prediction strategy
 * HOW:  Counters for predictions, hits, misses
 */
typedef struct {
    uint64_t predictions_made;          /**< Total predictions generated */
    uint64_t predictions_correct;       /**< Predictions that matched reality */
    uint64_t predictions_wrong;         /**< Predictions that didn't match */
    float prediction_accuracy;          /**< Overall accuracy (0-1) */
    float avg_prediction_lead_time_ms;  /**< Average lead time in ms */
    uint64_t prefetch_hits;             /**< Prefetched data was used */
    uint64_t prefetch_misses;           /**< Prefetched data was wasted */
} predictive_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create predictive protocol instance
 *
 * WHAT: Initialize predictive communication system
 * WHY:  Set up pattern learning and prediction generation
 * HOW:  Allocate structures, initialize history buffer
 *
 * @param config Configuration parameters (NULL for defaults)
 * @return Protocol handle on success, NULL on failure
 *
 * EXAMPLE:
 * ```c
 * predictive_protocol_config_t config = {
 *     .prediction_window_ms = 2000,
 *     .history_buffer_size = 1024,
 *     .confidence_threshold = 0.7f,
 *     .enable_prefetch = true,
 *     .enable_bio_async = true
 * };
 * predictive_protocol_t* protocol = predictive_protocol_create(&config);
 * ```
 */
predictive_protocol_t* predictive_protocol_create(
    const predictive_protocol_config_t* config);

/**
 * @brief Destroy predictive protocol
 *
 * WHAT: Clean up predictive protocol resources
 * WHY:  Free memory, close connections, stop threads
 * HOW:  Release all internal structures
 *
 * @param protocol Protocol handle (may be NULL)
 */
void predictive_protocol_destroy(predictive_protocol_t* protocol);

//=============================================================================
// Pattern Learning Functions
//=============================================================================

/**
 * @brief Observe a message for pattern learning
 *
 * WHAT: Record message occurrence in history
 * WHY:  Build statistical model of communication patterns
 * HOW:  Update internal sequence tracking and statistics
 *
 * @param protocol Protocol handle
 * @param msg_type Message type identifier
 * @param source Source module ID
 * @param target Target module ID
 * @param timestamp_ms When message occurred (milliseconds)
 * @return 0 on success, negative on error
 *
 * EXAMPLE:
 * ```c
 * predictive_protocol_observe_message(protocol,
 *     BIO_MSG_BRAIN_STATE_QUERY,
 *     BIO_MODULE_ATTENTION,
 *     BIO_MODULE_BRAIN,
 *     get_timestamp_ms());
 * ```
 */
int predictive_protocol_observe_message(
    predictive_protocol_t* protocol,
    uint32_t msg_type,
    uint32_t source,
    uint32_t target,
    uint64_t timestamp_ms);

/**
 * @brief Learn from message sequence
 *
 * WHAT: Train on complete message sequence for pattern recognition
 * WHY:  Batch learning from historical or replayed sequences
 * HOW:  Process sequence and update transition probabilities
 *
 * @param protocol Protocol handle
 * @param message_sequence Array of message type IDs
 * @param seq_len Length of sequence
 * @return 0 on success, negative on error
 *
 * EXAMPLE:
 * ```c
 * uint32_t sequence[] = {
 *     BIO_MSG_VISUAL_INPUT,
 *     BIO_MSG_ATTENTION_SHIFT,
 *     BIO_MSG_WORKING_MEMORY_STORE
 * };
 * predictive_protocol_learn_sequence(protocol, sequence, 3);
 * ```
 */
int predictive_protocol_learn_sequence(
    predictive_protocol_t* protocol,
    const uint32_t* message_sequence,
    uint32_t seq_len);

//=============================================================================
// Prediction Functions
//=============================================================================

/**
 * @brief Predict next message
 *
 * WHAT: Generate prediction for next message after current one
 * WHY:  Anticipate communication for latency reduction
 * HOW:  Use learned patterns and current context
 *
 * @param protocol Protocol handle
 * @param current_msg_type Type of current message
 * @param prediction Output prediction structure
 * @return 0 on success, negative on error or no prediction available
 *
 * EXAMPLE:
 * ```c
 * predicted_message_t pred;
 * if (predictive_protocol_predict_next(protocol,
 *         BIO_MSG_VISUAL_INPUT, &pred) == 0) {
 *     if (pred.confidence > 0.8f) {
 *         // High confidence prediction
 *         prefetch_for_message(&pred);
 *     }
 * }
 * ```
 */
int predictive_protocol_predict_next(
    predictive_protocol_t* protocol,
    uint32_t current_msg_type,
    predicted_message_t* prediction);

/**
 * @brief Get all predictions within time window
 *
 * WHAT: Generate all likely messages in upcoming time window
 * WHY:  Comprehensive prediction for advanced prefetching
 * HOW:  Scan learned patterns for all matches in window
 *
 * @param protocol Protocol handle
 * @param time_window_ms Time window for predictions (ms)
 * @param predictions Output array of predictions (allocated by function)
 * @param count Output count of predictions
 * @return 0 on success, negative on error
 *
 * NOTE: Caller must free predictions array with nimcp_free()
 *
 * EXAMPLE:
 * ```c
 * predicted_message_t* preds;
 * uint32_t count;
 * if (predictive_protocol_get_predictions(protocol, 1000,
 *         &preds, &count) == 0) {
 *     for (uint32_t i = 0; i < count; i++) {
 *         LOG_DEBUG("Predicted: type=%u conf=%.2f",
 *             preds[i].message_type, preds[i].confidence);
 *     }
 *     nimcp_free(preds);
 * }
 * ```
 */
int predictive_protocol_get_predictions(
    predictive_protocol_t* protocol,
    uint64_t time_window_ms,
    predicted_message_t** predictions,
    uint32_t* count);

//=============================================================================
// Prefetching Functions
//=============================================================================

/**
 * @brief Prefetch data for predicted message
 *
 * WHAT: Fetch and cache data that will likely be needed
 * WHY:  Reduce latency when predicted message arrives
 * HOW:  Load data into internal cache based on prediction
 *
 * @param protocol Protocol handle
 * @param prediction Prediction to prefetch for
 * @return 0 on success, negative on error
 *
 * EXAMPLE:
 * ```c
 * predicted_message_t pred;
 * if (predictive_protocol_predict_next(protocol, current_type, &pred) == 0) {
 *     if (pred.confidence > 0.75f) {
 *         predictive_protocol_prefetch_data(protocol, &pred);
 *     }
 * }
 * ```
 */
int predictive_protocol_prefetch_data(
    predictive_protocol_t* protocol,
    const predicted_message_t* prediction);

/**
 * @brief Check if data was prefetched
 *
 * WHAT: Query prefetch cache for message type
 * WHY:  Retrieve prefetched data when message arrives
 * HOW:  Lookup in internal cache by message type
 *
 * @param protocol Protocol handle
 * @param msg_type Message type to check
 * @param data Output pointer to prefetched data (NULL if not cached)
 * @param size Output size of prefetched data
 * @return 0 if found, negative if not cached
 *
 * EXAMPLE:
 * ```c
 * void* data;
 * uint32_t size;
 * if (predictive_protocol_check_prefetch(protocol,
 *         BIO_MSG_BRAIN_STATE_QUERY, &data, &size) == 0) {
 *     // Use prefetched data
 *     process_prefetched_data(data, size);
 * }
 * ```
 */
int predictive_protocol_check_prefetch(
    predictive_protocol_t* protocol,
    uint32_t msg_type,
    void** data,
    uint32_t* size);

//=============================================================================
// Integration Functions
//=============================================================================

/**
 * @brief Connect to predictive coding module
 *
 * WHAT: Integrate with cognitive predictive coding system
 * WHY:  Use brain's prediction machinery for protocol prediction
 * HOW:  Register callbacks and share prediction context
 *
 * @param protocol Protocol handle
 * @param predictive_context Predictive coding context
 * @return 0 on success, negative on error
 *
 * EXAMPLE:
 * ```c
 * predictive_network_t* pred_net = get_brain_predictive_network();
 * predictive_protocol_connect_predictive_coding(protocol, pred_net);
 * ```
 */
int predictive_protocol_connect_predictive_coding(
    predictive_protocol_t* protocol,
    void* predictive_context);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Process bio-async inbox
 *
 * WHAT: Handle bio-async messages for this module
 * WHY:  Respond to prediction verification, errors, and requests
 * HOW:  Process inbox messages from bio-async router
 *
 * @param protocol Protocol handle
 * @return Number of messages processed, negative on error
 *
 * MESSAGES HANDLED:
 * - BIO_MSG_PREDICTION_MADE: Prediction was made
 * - BIO_MSG_PREDICTION_VERIFIED: Prediction was correct
 * - BIO_MSG_PREDICTION_ERROR: Prediction was wrong
 * - BIO_MSG_PREFETCH_REQUEST: Request to prefetch data
 * - BIO_MSG_PREFETCH_READY: Prefetch completed
 *
 * EXAMPLE:
 * ```c
 * // In main loop
 * while (running) {
 *     int processed = predictive_protocol_process_inbox(protocol);
 *     if (processed > 0) {
 *         LOG_DEBUG("Processed %d bio-async messages", processed);
 *     }
 * }
 * ```
 */
int predictive_protocol_process_inbox(predictive_protocol_t* protocol);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get predictive protocol statistics
 *
 * WHAT: Retrieve performance and accuracy metrics
 * WHY:  Monitor prediction quality and optimize parameters
 * HOW:  Return copy of internal statistics
 *
 * @param protocol Protocol handle
 * @return Statistics structure (zeroed on error)
 *
 * EXAMPLE:
 * ```c
 * predictive_stats_t stats = predictive_protocol_get_stats(protocol);
 * LOG_INFO("Prediction accuracy: %.2f%% (%llu/%llu)",
 *     stats.prediction_accuracy * 100.0f,
 *     stats.predictions_correct,
 *     stats.predictions_made);
 * LOG_INFO("Prefetch hit rate: %.2f%% (%llu/%llu)",
 *     (float)stats.prefetch_hits /
 *     (stats.prefetch_hits + stats.prefetch_misses) * 100.0f,
 *     stats.prefetch_hits,
 *     stats.prefetch_hits + stats.prefetch_misses);
 * ```
 */
predictive_stats_t predictive_protocol_get_stats(
    predictive_protocol_t* protocol);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_PROTOCOL_H */
