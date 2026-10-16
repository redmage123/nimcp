/**
 * @file nimcp_predictive_protocol_fep_bridge.h
 * @brief FEP bridge for predictive protocol (anticipatory message passing)
 *
 * WHAT: Bidirectional integration between Free Energy Principle and predictive protocol
 * WHY:  Enable active inference for message prefetching and pattern learning
 * HOW:  FEP predicts message patterns, protocol statistics validate predictions
 *
 * BIOLOGICAL BASIS:
 * - Predictive protocol = anticipatory signaling (cerebellar prediction)
 * - Pattern learning = generative model learning (cortical predictive coding)
 * - Prefetching = active inference (prepare for predicted observations)
 * - Surprise threshold = precision-weighted prediction error
 *
 * INTEGRATION MECHANISMS:
 * 1. FEP → Protocol: Predictions guide prefetching decisions
 * 2. Protocol → FEP: Cache hits/misses update prediction accuracy
 * 3. Bidirectional: Pattern library aligns with FEP generative model
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 * @version 1.0.0
 */

#ifndef NIMCP_PREDICTIVE_PROTOCOL_FEP_BRIDGE_H
#define NIMCP_PREDICTIVE_PROTOCOL_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "async/nimcp_predictive_protocol.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Predictive protocol FEP bridge configuration
 */
typedef struct {
    /* FEP parameters */
    float prediction_confidence_threshold; /**< Min confidence for prefetch */
    float pattern_surprise_threshold;      /**< Surprise threshold for new patterns */
    uint32_t fep_hierarchy_levels;         /**< FEP hierarchy for pattern learning */

    /* Learning */
    float learning_rate;                   /**< Pattern learning rate */
    bool enable_pattern_learning;          /**< Learn patterns from FEP */
    uint32_t pattern_history_size;         /**< Pattern history window */

    /* Prefetch optimization */
    bool enable_fep_guided_prefetch;       /**< Use FEP to guide prefetching */
    float prefetch_confidence_boost;       /**< Confidence boost from FEP */
    uint32_t max_prefetch_depth;           /**< Max prediction depth */

    /* Integration */
    bool enable_cache_feedback;            /**< Feed cache stats to FEP */
    float cache_feedback_gain;             /**< Gain for cache feedback */
} predictive_protocol_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects on predictive protocol
 *
 * WHAT: How FEP predictions guide prefetching
 * WHY:  Active inference optimizes prefetch decisions
 * HOW:  FEP beliefs predict message patterns for caching
 */
typedef struct {
    /* Prefetch guidance */
    bool should_prefetch;                  /**< Whether to prefetch */
    float prefetch_confidence;             /**< Confidence in prefetch decision */
    bio_message_type_t predicted_msg_type; /**< Predicted next message type */

    /* Pattern prediction */
    uint32_t predicted_pattern_id;         /**< Predicted pattern ID */
    float pattern_match_probability;       /**< Probability of pattern match */

    /* Cache optimization */
    uint32_t predicted_cache_size;         /**< Optimal cache size prediction */
    float cache_hit_rate_prediction;       /**< Predicted hit rate */

    /* Modulation */
    float prefetch_urgency;                /**< How urgent to prefetch [0,1] */
    float exploration_factor;              /**< Exploration vs exploitation */
} predictive_protocol_fep_effects_t;

/**
 * @brief Predictive protocol effects on FEP
 *
 * WHAT: How protocol statistics inform FEP
 * WHY:  Cache performance validates predictions
 * HOW:  Hit/miss rates update FEP beliefs about patterns
 */
typedef struct {
    /* Cache observations */
    uint64_t cache_hits;                   /**< Cache hits this update */
    uint64_t cache_misses;                 /**< Cache misses this update */
    float hit_rate;                        /**< Current hit rate [0,1] */

    /* Prediction errors */
    float pattern_prediction_error;        /**< Pattern prediction error */
    float hit_rate_prediction_error;       /**< Hit rate prediction error */

    /* Surprise signals */
    float prefetch_surprise;               /**< Surprise from prefetch mismatch */
    bool unexpected_cache_miss;            /**< Unexpected cache miss */

    /* Learning signals */
    uint32_t novel_patterns_observed;      /**< New patterns detected */
    float prediction_accuracy;             /**< Overall prediction accuracy */
} fep_predictive_protocol_effects_t;

/* ============================================================================
 * State and Statistics
 * ============================================================================ */

/**
 * @brief Bridge state tracking
 */
typedef struct {
    /* Prediction state */
    uint32_t active_predictions;           /**< Active pattern predictions */
    uint64_t total_predictions;            /**< Total predictions made */
    uint64_t successful_predictions;       /**< Successful predictions */

    /* Pattern state */
    uint32_t learned_patterns;             /**< Patterns learned from FEP */
    uint64_t patterns_validated;           /**< Patterns validated by cache */
    uint32_t active_patterns;              /**< Currently active patterns */

    /* Integration state */
    bool fep_active;                       /**< FEP actively predicting */
    bool protocol_connected;               /**< Protocol connected */
    uint64_t last_update_us;               /**< Last update timestamp */
} predictive_protocol_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Prefetch performance */
    float avg_prefetch_hit_rate;           /**< Average prefetch hit rate */
    float avg_prefetch_surprise;           /**< Average prefetch surprise */
    uint64_t prefetches_guided_by_fep;     /**< Prefetches guided by FEP */

    /* Prediction accuracy */
    float avg_pattern_error;               /**< Average pattern prediction error */
    float avg_hit_rate_error;              /**< Average hit rate error */
    uint64_t accurate_prefetches;          /**< Accurate prefetch predictions */

    /* Pattern statistics */
    uint64_t pattern_hits;                 /**< Pattern predictions correct */
    uint64_t pattern_misses;               /**< Pattern predictions incorrect */
    float pattern_hit_rate;                /**< Pattern prediction accuracy */

    /* FEP metrics */
    float avg_free_energy;                 /**< Average free energy */
    float avg_precision;                   /**< Average precision */
} predictive_protocol_fep_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Predictive protocol FEP bridge
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* Configuration */
    predictive_protocol_fep_config_t config;

    /* Module connections */
    fep_system_t* fep_system;
    predictive_protocol_t protocol;

    /* Effects */
    predictive_protocol_fep_effects_t fep_effects;
    fep_predictive_protocol_effects_t protocol_effects;

    /* State */
    predictive_protocol_fep_state_t state;

    /* Statistics */
    predictive_protocol_fep_stats_t stats;

    } predictive_protocol_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default predictive protocol FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for protocol-FEP integration
 * WHY:  Easy initialization with effective predictive prefetching
 * HOW:  Returns config optimized for pattern prediction
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int predictive_protocol_fep_default_config(predictive_protocol_fep_config_t* config);

/**
 * @brief Create predictive protocol FEP bridge
 *
 * WHAT: Initialize bidirectional FEP-protocol integration
 * WHY:  Enable active inference for message prefetching
 * HOW:  Allocate bridge, connect FEP and protocol, initialize patterns
 *
 * @param config Bridge configuration
 * @param fep_system FEP system to integrate
 * @param protocol Predictive protocol instance
 * @return Bridge instance or NULL on failure
 */
predictive_protocol_fep_bridge_t* predictive_protocol_fep_create(
    const predictive_protocol_fep_config_t* config,
    fep_system_t* fep_system,
    predictive_protocol_t protocol
);

/**
 * @brief Destroy predictive protocol FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Free memory and disconnect integrations
 * HOW:  Unregister callbacks, free state, destroy mutex
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void predictive_protocol_fep_destroy(predictive_protocol_fep_bridge_t* bridge);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Update FEP effects on protocol
 *
 * WHAT: Compute how FEP predictions guide prefetching
 * WHY:  Apply active inference to prefetch decisions
 * HOW:  Use FEP to select patterns and prefetch targets
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int predictive_protocol_fep_update_effects(predictive_protocol_fep_bridge_t* bridge);

/**
 * @brief Observe prefetch result
 *
 * WHAT: Feed prefetch statistics into FEP
 * WHY:  Cache performance validates predictions
 * HOW:  Process cache hit/miss, update beliefs
 *
 * @param bridge FEP bridge
 * @param msg_type Message type that was accessed
 * @param cache_hit Whether cache hit occurred
 * @param latency_saved_ms Latency saved by hit (0 if miss)
 * @return 0 on success, error code on failure
 */
int predictive_protocol_fep_observe_prefetch(
    predictive_protocol_fep_bridge_t* bridge,
    bio_message_type_t msg_type,
    bool cache_hit,
    float latency_saved_ms
);

/**
 * @brief Predict next message pattern
 *
 * WHAT: Use FEP to predict next message pattern
 * WHY:  Enable proactive prefetching
 * HOW:  Active inference over message patterns
 *
 * @param bridge FEP bridge
 * @param current_msg Current message context
 * @param predicted_msg Output predicted message type
 * @param confidence Output prediction confidence
 * @return 0 on success, error code on failure
 */
int predictive_protocol_fep_predict_pattern(
    predictive_protocol_fep_bridge_t* bridge,
    const bio_message_header_t* current_msg,
    bio_message_type_t* predicted_msg,
    float* confidence
);

/**
 * @brief Learn message pattern from FEP beliefs
 *
 * WHAT: Extract message pattern from FEP generative model
 * WHY:  Align protocol patterns with FEP beliefs
 * HOW:  Convert FEP hierarchy level to message pattern
 *
 * @param bridge FEP bridge
 * @param level FEP hierarchy level
 * @param pattern_id Output pattern ID
 * @return 0 on success, error code on failure
 */
int predictive_protocol_fep_learn_pattern(
    predictive_protocol_fep_bridge_t* bridge,
    uint32_t level,
    uint32_t* pattern_id
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async for message handling
 * WHY:  Enable async pattern prediction
 * HOW:  Register module, set up message handlers
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int predictive_protocol_fep_connect_bio_async(predictive_protocol_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async messaging
 * WHY:  Clean shutdown of async integration
 * HOW:  Unregister module context
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int predictive_protocol_fep_disconnect_bio_async(predictive_protocol_fep_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge FEP bridge
 * @return true if connected
 */
bool predictive_protocol_fep_is_bio_async_connected(
    const predictive_protocol_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current FEP effects on protocol
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int predictive_protocol_fep_get_effects(
    const predictive_protocol_fep_bridge_t* bridge,
    predictive_protocol_fep_effects_t* effects
);

/**
 * @brief Get protocol effects on FEP
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int predictive_protocol_fep_get_protocol_effects(
    const predictive_protocol_fep_bridge_t* bridge,
    fep_predictive_protocol_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge FEP bridge
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int predictive_protocol_fep_get_stats(
    const predictive_protocol_fep_bridge_t* bridge,
    predictive_protocol_fep_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int predictive_protocol_fep_reset_stats(predictive_protocol_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_PROTOCOL_FEP_BRIDGE_H */
