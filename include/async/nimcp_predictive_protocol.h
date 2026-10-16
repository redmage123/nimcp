/**
 * @file nimcp_predictive_protocol.h
 * @brief Predictive communication protocol for anticipatory messaging
 *
 * WHAT: Predict and prefetch messages before they're explicitly requested
 * WHY:  Reduce latency by anticipating communication patterns
 * HOW:  Pattern learning -> prediction model -> prefetch -> cache
 *
 * ARCHITECTURE:
 * ┌──────────────────────────────────────────────────────────────┐
 * │                 PREDICTIVE PROTOCOL                          │
 * ├──────────────────────────────────────────────────────────────┤
 * │  1. OBSERVE: Learn from message traffic patterns             │
 * │     - Track source->target->type sequences                   │
 * │     - Measure inter-message intervals                        │
 * │     - Build frequency histograms                             │
 * │                                                              │
 * │  2. PREDICT: Markov chain prediction                         │
 * │     - State = (prev_source, prev_target, prev_type)          │
 * │     - Transition probabilities P(next | current)             │
 * │     - Confidence = frequency / total_observations            │
 * │                                                              │
 * │  3. PREFETCH: Proactive data loading                         │
 * │     - If confidence > threshold, prefetch next message       │
 * │     - Store in LRU cache                                     │
 * │                                                              │
 * │  4. SERVE: Fast retrieval from cache                         │
 * │     - Check cache before actual fetch                        │
 * │     - Track hit/miss statistics                             │
 * └──────────────────────────────────────────────────────────────┘
 *
 * USAGE:
 * ```c
 * // Initialize protocol
 * predictive_config_t config = {
 *     .prediction_horizon_ms = 100,
 *     .cache_size = 256,
 *     .learning_rate = 0.1f,
 *     .min_confidence = 0.7f
 * };
 * predictive_protocol_t proto = predictive_protocol_create(&config);
 *
 * // Observe traffic
 * bio_message_header_t* hdr = ...;
 * predictive_protocol_observe(proto, hdr);
 *
 * // Predict next message
 * prediction_t predictions[10];
 * uint32_t count = predictive_protocol_predict_next(proto, hdr, predictions, 10);
 *
 * // Prefetch if confident
 * for (uint32_t i = 0; i < count; i++) {
 *     if (predictions[i].confidence >= 0.8f) {
 *         predictive_protocol_prefetch(proto, &predictions[i]);
 *     }
 * }
 *
 * // Check cache before fetch
 * void* cached = predictive_protocol_get_prefetched(proto, msg_type, target);
 * if (cached) {
 *     // Use cached data
 * } else {
 *     // Perform actual fetch
 * }
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#ifndef NIMCP_PREDICTIVE_PROTOCOL_H
#define NIMCP_PREDICTIVE_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

typedef struct predictive_protocol_struct* predictive_protocol_t;

/*=============================================================================
 * CONFIGURATION
 *============================================================================*/

/**
 * @brief Predictive protocol configuration
 */
typedef struct {
    uint32_t prediction_horizon_ms;  /**< How far ahead to predict (ms) */
    uint32_t cache_size;             /**< Maximum cached prefetches */
    float learning_rate;             /**< Pattern learning rate [0,1] */
    float min_confidence;            /**< Minimum confidence for prefetch [0,1] */
    uint32_t max_patterns;           /**< Maximum tracked patterns */
    uint32_t markov_order;           /**< Markov chain order (1-3) */
    bool enable_statistics;          /**< Track performance stats */
} predictive_config_t;

/*=============================================================================
 * PATTERN AND PREDICTION STRUCTURES
 *============================================================================*/

/**
 * @brief Message pattern observation
 */
typedef struct {
    bio_module_id_t source_module;   /**< Source module */
    bio_module_id_t target_module;   /**< Target module */
    bio_message_type_t msg_type;     /**< Message type */
    uint32_t frequency;              /**< Observed count */
    float avg_interval_ms;           /**< Average time since last occurrence */
    uint64_t last_seen_us;           /**< Last observation timestamp */
} message_pattern_t;

/**
 * @brief Predicted message
 */
typedef struct {
    bio_message_type_t predicted_msg_type;  /**< Predicted message type */
    bio_module_id_t predicted_target;       /**< Predicted target module */
    float confidence;                       /**< Prediction confidence [0,1] */
    uint32_t prefetch_size;                 /**< Expected message size */
    void* prefetch_data;                    /**< Prefetched data (if any) */
} prediction_t;

/*=============================================================================
 * STATISTICS
 *============================================================================*/

/**
 * @brief Prefetch performance statistics
 */
typedef struct {
    uint64_t predictions_made;        /**< Total predictions generated */
    uint64_t prefetches_attempted;    /**< Prefetch attempts */
    uint64_t cache_hits;              /**< Cache hit count */
    uint64_t cache_misses;            /**< Cache miss count */
    uint64_t wasted_prefetches;       /**< Prefetched but never used */
    float avg_latency_saved_ms;       /**< Average latency saved per hit */
    float hit_rate;                   /**< Cache hit rate [0,1] */
    float prediction_accuracy;        /**< Prediction accuracy [0,1] */
    uint32_t current_cache_size;      /**< Current entries in cache */
} prefetch_result_t;

/*=============================================================================
 * LIFECYCLE API
 *============================================================================*/

/**
 * WHAT: Get default predictive protocol configuration
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Returns config with biologically-realistic prediction parameters
 *
 * @return Default configuration structure
 */
predictive_config_t predictive_protocol_default_config(void);

/**
 * WHAT: Create predictive protocol instance
 * WHY:  Initialize pattern learning and prediction system
 * HOW:  Allocates state, initializes Markov chain, sets up LRU cache
 *
 * @param config Configuration (NULL for defaults)
 * @return Protocol handle or NULL on failure
 *
 * THREAD SAFETY: Thread-safe, can create multiple instances
 */
predictive_protocol_t predictive_protocol_create(const predictive_config_t* config);

/**
 * WHAT: Destroy predictive protocol instance
 * WHY:  Free resources and cleanup state
 * HOW:  Frees patterns, cache entries, statistics
 *
 * @param protocol Protocol handle (NULL safe)
 */
void predictive_protocol_destroy(predictive_protocol_t protocol);

/*=============================================================================
 * OBSERVATION AND LEARNING API
 *============================================================================*/

/**
 * WHAT: Observe message traffic for pattern learning
 * WHY:  Learn communication patterns to improve predictions
 * HOW:  Updates pattern frequencies, intervals, Markov transitions
 *
 * @param protocol Protocol handle
 * @param msg_header Message header to observe
 * @return 0 on success, -1 on failure
 *
 * SIDE EFFECTS:
 * - Updates pattern frequencies (exponential moving average)
 * - Updates Markov chain transition probabilities
 * - Updates average inter-message intervals
 *
 * THREAD SAFETY: Thread-safe (uses internal locking)
 */
int predictive_protocol_observe(predictive_protocol_t protocol,
                                 const bio_message_header_t* msg_header);

/**
 * WHAT: Reset learned patterns
 * WHY:  Clear stale patterns when traffic changes
 * HOW:  Resets frequencies, clears Markov chain, flushes cache
 *
 * @param protocol Protocol handle
 */
void predictive_protocol_reset_patterns(predictive_protocol_t protocol);

/*=============================================================================
 * PREDICTION API
 *============================================================================*/

/**
 * WHAT: Predict next messages based on current message
 * WHY:  Anticipate upcoming messages for prefetching
 * HOW:  Uses Markov chain to predict likely next states
 *
 * @param protocol Protocol handle
 * @param current_msg Current message header (context)
 * @param predictions Output array for predictions
 * @param max_predictions Maximum predictions to generate
 * @return Number of predictions generated
 *
 * ALGORITHM:
 * 1. Extract current state (source, target, type)
 * 2. Lookup Markov chain transitions from current state
 * 3. Sort by probability (descending)
 * 4. Return top N predictions above confidence threshold
 *
 * COMPLEXITY: O(P log P) where P = number of possible next states
 * THREAD SAFETY: Thread-safe (read-only access to patterns)
 */
uint32_t predictive_protocol_predict_next(predictive_protocol_t protocol,
                                           const bio_message_header_t* current_msg,
                                           prediction_t* predictions,
                                           uint32_t max_predictions);

/**
 * WHAT: Get confidence for specific next message
 * WHY:  Evaluate likelihood of a particular message
 * HOW:  Queries Markov chain for transition probability
 *
 * @param protocol Protocol handle
 * @param current_msg Current message
 * @param next_type Predicted next message type
 * @param next_target Predicted next target module
 * @return Confidence [0,1] or -1.0 if not enough data
 */
float predictive_protocol_get_confidence(predictive_protocol_t protocol,
                                         const bio_message_header_t* current_msg,
                                         bio_message_type_t next_type,
                                         bio_module_id_t next_target);

/*=============================================================================
 * PREFETCH API
 *============================================================================*/

/**
 * WHAT: Prefetch predicted message data
 * WHY:  Proactively load data to reduce future latency
 * HOW:  Allocates cache entry, optionally loads data, stores in LRU cache
 *
 * @param protocol Protocol handle
 * @param prediction Prediction to prefetch
 * @return 0 on success, -1 on failure
 *
 * SIDE EFFECTS:
 * - Allocates cache entry
 * - May evict LRU entry if cache full
 * - Updates prefetch statistics
 *
 * THREAD SAFETY: Thread-safe (uses internal locking)
 */
int predictive_protocol_prefetch(predictive_protocol_t protocol,
                                 const prediction_t* prediction);

/**
 * WHAT: Get prefetched data from cache
 * WHY:  Fast retrieval of predicted messages
 * HOW:  Lookup in cache by (msg_type, target), update LRU order
 *
 * @param protocol Protocol handle
 * @param msg_type Message type to retrieve
 * @param target Target module
 * @return Cached data or NULL if not found
 *
 * SIDE EFFECTS:
 * - Updates LRU order (moves entry to MRU position)
 * - Increments cache hit/miss counters
 *
 * COMPLEXITY: O(1) average case (hash lookup)
 * THREAD SAFETY: Thread-safe (uses internal locking)
 */
void* predictive_protocol_get_prefetched(predictive_protocol_t protocol,
                                         bio_message_type_t msg_type,
                                         bio_module_id_t target);

/**
 * WHAT: Invalidate cached data
 * WHY:  Remove stale or incorrect predictions
 * HOW:  Removes entry from cache, frees data
 *
 * @param protocol Protocol handle
 * @param msg_type Message type to invalidate (0 = all)
 * @return Number of entries invalidated
 */
uint32_t predictive_protocol_invalidate(predictive_protocol_t protocol,
                                        bio_message_type_t msg_type);

/*=============================================================================
 * STATISTICS API
 *============================================================================*/

/**
 * WHAT: Get prefetch performance statistics
 * WHY:  Monitor effectiveness of predictions
 * HOW:  Returns copy of internal statistics
 *
 * @param protocol Protocol handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int predictive_protocol_get_stats(predictive_protocol_t protocol,
                                  prefetch_result_t* stats);

/**
 * WHAT: Reset statistics counters
 * WHY:  Clear stats for new measurement period
 * HOW:  Zeros counters, preserves patterns
 *
 * @param protocol Protocol handle
 */
void predictive_protocol_reset_stats(predictive_protocol_t protocol);

/*=============================================================================
 * PATTERN QUERY API
 *============================================================================*/

/**
 * WHAT: Get observed message patterns
 * WHY:  Inspect learned communication patterns
 * HOW:  Returns array of most frequent patterns
 *
 * @param protocol Protocol handle
 * @param patterns Output array for patterns
 * @param max_patterns Maximum patterns to return
 * @return Number of patterns returned
 */
uint32_t predictive_protocol_get_patterns(predictive_protocol_t protocol,
                                          message_pattern_t* patterns,
                                          uint32_t max_patterns);

/**
 * WHAT: Get pattern for specific message type
 * WHY:  Query specific pattern statistics
 * HOW:  Lookup pattern by (source, target, type)
 *
 * @param protocol Protocol handle
 * @param source Source module
 * @param target Target module
 * @param msg_type Message type
 * @param pattern Output pattern structure
 * @return 0 if found, -1 if not found
 */
int predictive_protocol_get_pattern(predictive_protocol_t protocol,
                                    bio_module_id_t source,
                                    bio_module_id_t target,
                                    bio_message_type_t msg_type,
                                    message_pattern_t* pattern);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_PROTOCOL_H */
