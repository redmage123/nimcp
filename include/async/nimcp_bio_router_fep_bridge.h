/**
 * @file nimcp_bio_router_fep_bridge.h
 * @brief FEP bridge for bio-router message routing system
 *
 * WHAT: Bidirectional integration between Free Energy Principle and bio-router
 * WHY:  Enable prediction error minimization for message routing decisions
 * HOW:  FEP predicts routing paths, router statistics update FEP beliefs
 *
 * BIOLOGICAL BASIS:
 * - Message routing = hierarchical predictive coding (cortical routing)
 * - Routing latency = prediction error magnitude
 * - Router statistics = observations for belief updates
 * - Route selection = active inference (minimize expected free energy)
 *
 * INTEGRATION MECHANISMS:
 * 1. FEP → Router: Predict optimal routes based on traffic patterns
 * 2. Router → FEP: Routing latency and errors update predictions
 * 3. Bidirectional: Route selection minimizes expected communication costs
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 * @version 1.0.0
 */

#ifndef NIMCP_BIO_ROUTER_FEP_BRIDGE_H
#define NIMCP_BIO_ROUTER_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
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
 * @brief Bio-router FEP bridge configuration
 */
typedef struct {
    /* FEP parameters */
    float route_prediction_confidence; /**< Confidence threshold for routing */
    float latency_tolerance_ms;        /**< Acceptable latency variation */
    float surprise_threshold;          /**< Routing surprise threshold */

    /* Learning */
    float learning_rate;               /**< Route pattern learning rate */
    bool enable_route_learning;        /**< Learn routing patterns */
    uint32_t history_window;           /**< Routing history window size */

    /* Optimization */
    bool enable_route_optimization;    /**< Optimize routes via FEP */
    float exploration_rate;            /**< Route exploration vs exploitation */
    uint32_t max_routes_evaluated;     /**< Max routes to evaluate */

    /* Integration */
    bool enable_latency_prediction;    /**< Predict routing latency */
    bool enable_congestion_avoidance;  /**< Use FEP to avoid congestion */
} bio_router_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects on bio-router
 *
 * WHAT: How FEP modulates routing decisions
 * WHY:  Predictions guide route selection for minimum free energy
 * HOW:  Active inference selects routes minimizing expected cost
 */
typedef struct {
    /* Route prediction */
    bio_module_id_t predicted_next_hop; /**< Predicted next module in route */
    float route_confidence;             /**< Confidence in route [0,1] */

    /* Latency prediction */
    float predicted_latency_ms;         /**< Predicted routing latency */
    float latency_uncertainty;          /**< Uncertainty in prediction */

    /* Route optimization */
    bool avoid_congestion;              /**< Whether to avoid congested routes */
    float congestion_estimate;          /**< Estimated congestion [0,1] */

    /* Modulation */
    float routing_priority_boost;       /**< Priority boost for predicted route */
    float exploration_factor;           /**< Exploration vs exploitation [0,1] */
} bio_router_fep_effects_t;

/**
 * @brief Bio-router effects on FEP
 *
 * WHAT: How routing statistics inform FEP
 * WHY:  Actual routing performance updates predictions
 * HOW:  Latency and errors are observations for belief updates
 */
typedef struct {
    /* Routing observations */
    float actual_latency_ms;            /**< Measured routing latency */
    uint32_t messages_routed;           /**< Messages routed this update */
    uint32_t routing_errors;            /**< Routing errors encountered */

    /* Prediction errors */
    float latency_prediction_error;     /**< Latency prediction error */
    float route_prediction_error;       /**< Route correctness error [0,1] */

    /* Surprise signals */
    float routing_surprise;             /**< Surprise from routing mismatch */
    bool high_latency_event;            /**< Latency spike detected */

    /* Learning signals */
    float prediction_accuracy;          /**< Recent prediction accuracy [0,1] */
    uint32_t correct_predictions;       /**< Correct route predictions */
} fep_bio_router_effects_t;

/* ============================================================================
 * State and Statistics
 * ============================================================================ */

/**
 * @brief Bridge state tracking
 */
typedef struct {
    /* Prediction state */
    uint32_t active_route_predictions;  /**< Active routing predictions */
    uint64_t total_route_predictions;   /**< Total predictions made */
    uint64_t successful_predictions;    /**< Successful predictions */

    /* Routing state */
    bio_module_id_t last_predicted_route; /**< Last predicted route */
    bio_module_id_t last_actual_route;    /**< Last actual route taken */
    float last_latency_ms;                /**< Last observed latency */

    /* Integration state */
    bool fep_active;                    /**< FEP actively predicting */
    bool router_connected;              /**< Router connected */
    uint64_t last_update_us;            /**< Last update timestamp */
} bio_router_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Prediction performance */
    float avg_route_prediction_error;   /**< Average route prediction error */
    float avg_latency_error_ms;         /**< Average latency prediction error */
    float avg_routing_surprise;         /**< Average routing surprise */

    /* Routing performance */
    uint64_t total_messages_routed;     /**< Total messages routed */
    uint64_t total_routing_errors;      /**< Total routing errors */
    float avg_routing_latency_ms;       /**< Average routing latency */

    /* Optimization metrics */
    uint64_t routes_optimized;          /**< Routes optimized by FEP */
    float latency_improvement_pct;      /**< Latency improvement percentage */
    uint64_t congestion_avoidances;     /**< Congestion avoidances */

    /* FEP metrics */
    float avg_free_energy;              /**< Average free energy */
    float avg_precision;                /**< Average precision */
} bio_router_fep_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Bio-router FEP bridge
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* Configuration */
    bio_router_fep_config_t config;

    /* Module connections */
    fep_system_t* fep_system;
    bio_router_t router;

    /* Effects */
    bio_router_fep_effects_t fep_effects;
    fep_bio_router_effects_t router_effects;

    /* State */
    bio_router_fep_state_t state;

    /* Statistics */
    bio_router_fep_stats_t stats;

    } bio_router_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bio-router FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for router-FEP integration
 * WHY:  Easy initialization with effective routing prediction
 * HOW:  Returns config optimized for routing optimization
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int bio_router_fep_default_config(bio_router_fep_config_t* config);

/**
 * @brief Create bio-router FEP bridge
 *
 * WHAT: Initialize bidirectional FEP-router integration
 * WHY:  Enable predictive routing with active inference
 * HOW:  Allocate bridge, connect FEP and router, initialize state
 *
 * @param config Bridge configuration
 * @param fep_system FEP system to integrate
 * @param router Bio-router instance
 * @return Bridge instance or NULL on failure
 */
bio_router_fep_bridge_t* bio_router_fep_create(
    const bio_router_fep_config_t* config,
    fep_system_t* fep_system,
    bio_router_t router
);

/**
 * @brief Destroy bio-router FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Free memory and disconnect integrations
 * HOW:  Unregister callbacks, free state, destroy mutex
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void bio_router_fep_destroy(bio_router_fep_bridge_t* bridge);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Update FEP effects on router
 *
 * WHAT: Compute how FEP predictions modulate routing
 * WHY:  Apply predictive guidance to route selection
 * HOW:  Use free energy to optimize routing decisions
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int bio_router_fep_update_effects(bio_router_fep_bridge_t* bridge);

/**
 * @brief Observe routing event
 *
 * WHAT: Feed routing statistics into FEP as observations
 * WHY:  Actual routing performance updates predictions
 * HOW:  Process observation, compute prediction error
 *
 * @param bridge FEP bridge
 * @param target Target module that was routed to
 * @param latency_ms Actual routing latency
 * @param success Whether routing succeeded
 * @return 0 on success, error code on failure
 */
int bio_router_fep_observe_routing(
    bio_router_fep_bridge_t* bridge,
    bio_module_id_t target,
    float latency_ms,
    bool success
);

/**
 * @brief Predict optimal route
 *
 * WHAT: Use FEP to predict best routing path
 * WHY:  Minimize expected routing cost (latency + errors)
 * HOW:  Active inference selects route minimizing EFE
 *
 * @param bridge FEP bridge
 * @param source Source module
 * @param target Target module
 * @param predicted_route Output predicted next hop
 * @param confidence Output prediction confidence
 * @return 0 on success, error code on failure
 */
int bio_router_fep_predict_route(
    bio_router_fep_bridge_t* bridge,
    bio_module_id_t source,
    bio_module_id_t target,
    bio_module_id_t* predicted_route,
    float* confidence
);

/**
 * @brief Predict routing latency
 *
 * WHAT: Predict expected latency for route
 * WHY:  Enable latency-aware routing decisions
 * HOW:  FEP belief about latency distribution
 *
 * @param bridge FEP bridge
 * @param target Target module
 * @param predicted_latency_ms Output predicted latency
 * @param uncertainty Output uncertainty estimate
 * @return 0 on success, error code on failure
 */
int bio_router_fep_predict_latency(
    bio_router_fep_bridge_t* bridge,
    bio_module_id_t target,
    float* predicted_latency_ms,
    float* uncertainty
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async for message handling
 * WHY:  Enable async routing prediction and observation
 * HOW:  Register module, set up message handlers
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int bio_router_fep_connect_bio_async(bio_router_fep_bridge_t* bridge);

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
int bio_router_fep_disconnect_bio_async(bio_router_fep_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge FEP bridge
 * @return true if connected
 */
bool bio_router_fep_is_bio_async_connected(const bio_router_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current FEP effects on router
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int bio_router_fep_get_effects(
    const bio_router_fep_bridge_t* bridge,
    bio_router_fep_effects_t* effects
);

/**
 * @brief Get router effects on FEP
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int bio_router_fep_get_router_effects(
    const bio_router_fep_bridge_t* bridge,
    fep_bio_router_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge FEP bridge
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int bio_router_fep_get_stats(
    const bio_router_fep_bridge_t* bridge,
    bio_router_fep_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int bio_router_fep_reset_stats(bio_router_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIO_ROUTER_FEP_BRIDGE_H */
