/**
 * @file nimcp_bio_async_fep_bridge.h
 * @brief FEP bridge for bio-async system
 *
 * WHAT: Bidirectional integration between Free Energy Principle and bio-async messaging
 * WHY:  Enable prediction error minimization for asynchronous message passing
 * HOW:  FEP predicts message timing/content, bio-async confidence maps to precision
 *
 * BIOLOGICAL BASIS:
 * - Bio-async neuromodulator decay = precision decay in FEP framework
 * - Promise completion = observation that minimizes prediction error
 * - Future confidence = precision weighting for belief updates
 * - Phase synchronization = hierarchical predictive coding convergence
 *
 * INTEGRATION MECHANISMS:
 * 1. FEP → Bio-Async: Prediction errors modulate neuromodulator release strength
 * 2. Bio-Async → FEP: Confidence decay informs precision estimates
 * 3. Bidirectional: Message timing predictions guide prefetching
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 * @version 1.0.0
 */

#ifndef NIMCP_BIO_ASYNC_FEP_BRIDGE_H
#define NIMCP_BIO_ASYNC_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
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
 * @brief Bio-async FEP bridge configuration
 */
typedef struct {
    /* FEP parameters */
    float prediction_horizon_ms;        /**< How far ahead to predict message arrival */
    float precision_decay_rate;         /**< Rate precision decays with confidence */
    float surprise_threshold;           /**< Threshold for high-surprise messages */

    /* Bio-async channel mapping */
    nimcp_bio_channel_type_t primary_channel; /**< Primary channel for predictions */
    bool enable_channel_switching;      /**< Switch channels based on surprise */

    /* Learning */
    float learning_rate;                /**< Belief update learning rate */
    bool enable_precision_learning;     /**< Learn precision from confidence */

    /* Integration */
    bool enable_prefetch;               /**< Use FEP predictions for prefetching */
    uint32_t max_predictions;           /**< Maximum concurrent predictions */
} bio_async_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects on bio-async system
 *
 * WHAT: How FEP modulates bio-async behavior
 * WHY:  Predictions guide message timing and channel selection
 * HOW:  Free energy minimization optimizes async coordination
 */
typedef struct {
    /* Message timing prediction */
    float predicted_arrival_ms;         /**< Predicted next message arrival */
    float prediction_confidence;        /**< Confidence in prediction [0,1] */

    /* Channel selection */
    nimcp_bio_channel_type_t predicted_channel; /**< Predicted optimal channel */
    float channel_preference[BIO_CHANNEL_COUNT]; /**< Preference per channel */

    /* Prefetch guidance */
    bool should_prefetch;               /**< Whether to prefetch next message */
    float prefetch_urgency;             /**< How urgent the prefetch [0,1] */

    /* Modulation */
    float concentration_modulation;     /**< Modulate release strength [0,2] */
    float refractory_modulation;        /**< Modulate refractory period [0,2] */
} bio_async_fep_effects_t;

/**
 * @brief Bio-async effects on FEP system
 *
 * WHAT: How bio-async informs FEP processing
 * WHY:  Confidence decay provides precision estimates
 * HOW:  Neuromodulator dynamics inform belief uncertainty
 */
typedef struct {
    /* Precision updates from confidence */
    float confidence_based_precision;   /**< Precision derived from confidence */
    float precision_decay_rate;         /**< Current precision decay rate */

    /* Observation updates */
    float last_observation;             /**< Last observed message timing */
    float observation_precision;        /**< Precision of timing observation */

    /* Surprise signals */
    float timing_surprise;              /**< Surprise from timing mismatch */
    float content_surprise;             /**< Surprise from content mismatch */

    /* Learning signals */
    float prediction_error_magnitude;   /**< Magnitude of prediction error */
    bool high_surprise_event;           /**< Whether surprise exceeded threshold */
} fep_bio_async_effects_t;

/* ============================================================================
 * State and Statistics
 * ============================================================================ */

/**
 * @brief Bridge state tracking
 */
typedef struct {
    /* Prediction state */
    uint32_t active_predictions;        /**< Number of active predictions */
    uint64_t total_predictions;         /**< Total predictions made */
    uint64_t correct_predictions;       /**< Predictions within threshold */

    /* Integration state */
    bool fep_active;                    /**< Whether FEP is actively predicting */
    bool bio_async_connected;          /**< Whether bio-async is connected */
    uint64_t last_update_us;            /**< Last bridge update timestamp */

    /* Channel state */
    nimcp_bio_channel_type_t current_channel; /**< Currently active channel */
    uint64_t channel_switches;          /**< Number of channel switches */
} bio_async_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Prediction accuracy */
    float avg_prediction_error_ms;      /**< Average timing prediction error */
    float avg_surprise;                 /**< Average surprise level */
    uint64_t high_surprise_count;       /**< High surprise events */

    /* Channel statistics */
    uint64_t messages_per_channel[BIO_CHANNEL_COUNT];
    float avg_confidence_per_channel[BIO_CHANNEL_COUNT];

    /* Performance */
    uint64_t prefetch_hits;             /**< Successful prefetches */
    uint64_t prefetch_misses;           /**< Failed prefetches */
    float prefetch_hit_rate;            /**< Hit rate [0,1] */

    /* FEP metrics */
    float avg_free_energy;              /**< Average free energy */
    float avg_precision;                /**< Average precision estimate */
} bio_async_fep_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Bio-async FEP bridge
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* Configuration */
    bio_async_fep_config_t config;

    /* Module connections */
    fep_system_t* fep_system;
    /* Note: bio-async is global, accessed via nimcp_bio_async_* APIs */

    /* Effects */
    bio_async_fep_effects_t fep_effects;
    fep_bio_async_effects_t bio_async_effects;

    /* State */
    bio_async_fep_state_t state;

    /* Statistics */
    bio_async_fep_stats_t stats;

    } bio_async_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bio-async FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for bio-async-FEP integration
 * WHY:  Easy initialization with biologically-realistic parameters
 * HOW:  Returns configuration optimized for async message prediction
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int bio_async_fep_default_config(bio_async_fep_config_t* config);

/**
 * @brief Create bio-async FEP bridge
 *
 * WHAT: Initialize bidirectional FEP-bio-async integration
 * WHY:  Enable predictive message passing with confidence tracking
 * HOW:  Allocate bridge, connect FEP system, register bio-async callbacks
 *
 * @param config Bridge configuration
 * @param fep_system FEP system to integrate
 * @return Bridge instance or NULL on failure
 */
bio_async_fep_bridge_t* bio_async_fep_create(
    const bio_async_fep_config_t* config,
    fep_system_t* fep_system
);

/**
 * @brief Destroy bio-async FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Free memory and disconnect integrations
 * HOW:  Unregister callbacks, free state, destroy mutex
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void bio_async_fep_destroy(bio_async_fep_bridge_t* bridge);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Update FEP effects on bio-async
 *
 * WHAT: Compute how FEP predictions modulate bio-async
 * WHY:  Apply predictive guidance to message timing
 * HOW:  Use prediction errors to modulate channel selection
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int bio_async_fep_update_effects(bio_async_fep_bridge_t* bridge);

/**
 * @brief Update bio-async effects on FEP
 *
 * WHAT: Feed bio-async confidence into FEP precision
 * WHY:  Confidence decay informs belief uncertainty
 * HOW:  Map neuromodulator concentration to precision
 *
 * @param bridge FEP bridge
 * @param future Bio-future to extract confidence from
 * @return 0 on success, error code on failure
 */
int bio_async_fep_observe_future(
    bio_async_fep_bridge_t* bridge,
    nimcp_bio_future_t future
);

/**
 * @brief Predict next message timing
 *
 * WHAT: Use FEP to predict when next message will arrive
 * WHY:  Enable proactive prefetching and resource allocation
 * HOW:  FEP prediction of temporal dynamics
 *
 * @param bridge FEP bridge
 * @param channel Channel to predict for
 * @param predicted_ms Output predicted arrival time
 * @param confidence Output prediction confidence [0,1]
 * @return 0 on success, error code on failure
 */
int bio_async_fep_predict_timing(
    bio_async_fep_bridge_t* bridge,
    nimcp_bio_channel_type_t channel,
    float* predicted_ms,
    float* confidence
);

/**
 * @brief Select optimal channel based on FEP
 *
 * WHAT: Choose best channel for message based on predictions
 * WHY:  Minimize expected free energy for communication
 * HOW:  Evaluate EFE for each channel, select minimum
 *
 * @param bridge FEP bridge
 * @param message_urgency Urgency of message [0,1]
 * @return Optimal channel
 */
nimcp_bio_channel_type_t bio_async_fep_select_channel(
    bio_async_fep_bridge_t* bridge,
    float message_urgency
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async for message handling
 * WHY:  Enable async message processing and callbacks
 * HOW:  Register module, set up message handlers
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int bio_async_fep_connect_bio_async(bio_async_fep_bridge_t* bridge);

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
int bio_async_fep_disconnect_bio_async(bio_async_fep_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge FEP bridge
 * @return true if connected
 */
bool bio_async_fep_is_bio_async_connected(const bio_async_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current FEP effects on bio-async
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int bio_async_fep_get_effects(
    const bio_async_fep_bridge_t* bridge,
    bio_async_fep_effects_t* effects
);

/**
 * @brief Get bio-async effects on FEP
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int bio_async_fep_get_bio_async_effects(
    const bio_async_fep_bridge_t* bridge,
    fep_bio_async_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge FEP bridge
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int bio_async_fep_get_stats(
    const bio_async_fep_bridge_t* bridge,
    bio_async_fep_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int bio_async_fep_reset_stats(bio_async_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIO_ASYNC_FEP_BRIDGE_H */
