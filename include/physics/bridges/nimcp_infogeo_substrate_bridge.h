/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_infogeo_substrate_bridge.h - Information Geometry to Bio-Async Bridge
//=============================================================================
/**
 * @file nimcp_infogeo_substrate_bridge.h
 * @brief Bridge connecting Information Geometry with Bio-Async Messaging
 *
 * WHAT: Provides bio-async message routing for Information Geometry module,
 *       enabling asynchronous communication of geometric computations.
 *
 * WHY:  Information geometry computations need distributed communication:
 *       - Fisher matrix updates from multiple neural populations
 *       - Natural gradient broadcasts to learning modules
 *       - Manifold state sharing across brain regions
 *       - KL divergence alerts for distribution drift
 *
 * HOW:  Registers InfoGeo as bio-router module:
 *       1. Broadcasts Fisher information updates
 *       2. Sends natural gradient directions
 *       3. Publishes manifold state changes
 *       4. Routes geometric metrics to subscribers
 *
 * MESSAGE TYPES:
 * ```
 * INFORMATION GEOMETRY OUTPUT             BIO-ASYNC MESSAGE
 * -----------------------------------------------------------------------
 * Fisher Matrix Update                ->  Fisher information broadcast
 * Natural Gradient Computed           ->  Gradient direction message
 * Manifold State Change               ->  Embedding update notification
 * KL Divergence Alert                 ->  Distribution drift warning
 * Convergence Status                  ->  Learning progress report
 * Curvature Change                    ->  Geometry metric update
 *
 * INFORMATION GEOMETRY INPUT              FROM MODULES
 * -----------------------------------------------------------------------
 * Spike Statistics                    <-  SNN populations
 * Weight Gradients                    <-  Plasticity modules
 * Distribution Samples                <-  FEP inference
 * Activity Patterns                   <-  Cognitive regions
 * ```
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_INFOGEO_SUBSTRATE_BRIDGE_H
#define NIMCP_INFOGEO_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define INFOGEO_SUBSTRATE_MODULE_NAME    "infogeo_substrate_bridge"

/** Maximum subscriptions */
#define INFOGEO_SUBSTRATE_MAX_SUBS       64

/** Maximum inbox messages */
#define INFOGEO_SUBSTRATE_MAX_INBOX      256

/** Maximum outbox messages */
#define INFOGEO_SUBSTRATE_MAX_OUTBOX     128

/** Default broadcast interval (ms) */
#define INFOGEO_SUBSTRATE_BROADCAST_MS   50

/** Message time-to-live (ms) */
#define INFOGEO_SUBSTRATE_MSG_TTL_MS     5000

/** Maximum dimensions in message */
#define INFOGEO_SUBSTRATE_MAX_DIM        64

//=============================================================================
// Message Types
//=============================================================================

/**
 * @brief Information Geometry bio-async message types
 */
typedef enum {
    INFOGEO_MSG_FISHER_UPDATE = 0,      /**< Fisher matrix update */
    INFOGEO_MSG_NATURAL_GRADIENT,       /**< Natural gradient direction */
    INFOGEO_MSG_MANIFOLD_STATE,         /**< Manifold embedding state */
    INFOGEO_MSG_KL_DIVERGENCE,          /**< KL divergence measurement */
    INFOGEO_MSG_GEODESIC_DISTANCE,      /**< Geodesic distance computed */
    INFOGEO_MSG_CURVATURE_UPDATE,       /**< Curvature metric change */
    INFOGEO_MSG_CONVERGENCE_STATUS,     /**< Optimization convergence */
    INFOGEO_MSG_SPIKE_STATS_REQUEST,    /**< Request spike statistics */
    INFOGEO_MSG_GRADIENT_REQUEST,       /**< Request weight gradients */
    INFOGEO_MSG_SAMPLE_REQUEST,         /**< Request distribution samples */
    INFOGEO_MSG_COUNT
} infogeo_substrate_msg_type_t;

/**
 * @brief Subscription bitmasks
 */
#define INFOGEO_SUB_FISHER_UPDATE       (1U << INFOGEO_MSG_FISHER_UPDATE)
#define INFOGEO_SUB_NATURAL_GRADIENT    (1U << INFOGEO_MSG_NATURAL_GRADIENT)
#define INFOGEO_SUB_MANIFOLD_STATE      (1U << INFOGEO_MSG_MANIFOLD_STATE)
#define INFOGEO_SUB_KL_DIVERGENCE       (1U << INFOGEO_MSG_KL_DIVERGENCE)
#define INFOGEO_SUB_GEODESIC_DISTANCE   (1U << INFOGEO_MSG_GEODESIC_DISTANCE)
#define INFOGEO_SUB_CURVATURE_UPDATE    (1U << INFOGEO_MSG_CURVATURE_UPDATE)
#define INFOGEO_SUB_CONVERGENCE_STATUS  (1U << INFOGEO_MSG_CONVERGENCE_STATUS)
#define INFOGEO_SUB_ALL                 (0xFFFFFFFFU)

//=============================================================================
// Message Payload Structures
//=============================================================================

/**
 * @brief Common message header
 */
typedef struct {
    infogeo_substrate_msg_type_t type;  /**< Message type */
    uint32_t source_module;             /**< Source module ID */
    uint32_t target_module;             /**< Target module (0 = broadcast) */
    uint64_t timestamp_us;              /**< Message timestamp */
    uint32_t sequence_number;           /**< Sequence for ordering */
    uint16_t payload_size;              /**< Size of payload data */
    uint16_t flags;                     /**< Message flags */
} infogeo_msg_header_t;

/**
 * @brief Fisher information update message
 */
typedef struct {
    infogeo_msg_header_t header;        /**< Message header */

    /** Fisher matrix data (diagonal for efficiency) */
    float fisher_diagonal[INFOGEO_SUBSTRATE_MAX_DIM];  /**< Diagonal elements */
    uint32_t dim;                       /**< Dimensionality */

    /** Matrix properties */
    float condition_number;             /**< Matrix condition number */
    float trace;                        /**< Matrix trace */
    float determinant;                  /**< Matrix determinant */
    float regularization_used;          /**< Regularization applied */

    /** Context */
    uint32_t population_id;             /**< Source population */
    uint64_t sample_count;              /**< Samples used */
    bool is_empirical;                  /**< Empirical vs exact Fisher */
} infogeo_fisher_msg_t;

/**
 * @brief Natural gradient message
 */
typedef struct {
    infogeo_msg_header_t header;        /**< Message header */

    /** Gradient data */
    float natural_gradient[INFOGEO_SUBSTRATE_MAX_DIM]; /**< Natural gradient */
    float standard_gradient[INFOGEO_SUBSTRATE_MAX_DIM]; /**< Original gradient */
    uint32_t dim;                       /**< Dimensionality */

    /** Gradient properties */
    float natural_norm;                 /**< Natural gradient norm */
    float standard_norm;                /**< Standard gradient norm */
    float speedup_ratio;                /**< Natural/standard ratio */
    float learning_rate;                /**< Recommended learning rate */

    /** Status */
    bool gradient_clipped;              /**< Was gradient clipped */
    bool fisher_valid;                  /**< Was Fisher invertible */
} infogeo_natural_grad_msg_t;

/**
 * @brief Manifold state message
 */
typedef struct {
    infogeo_msg_header_t header;        /**< Message header */

    /** Embedding state */
    float embedding[INFOGEO_SUBSTRATE_MAX_DIM]; /**< Manifold coordinates */
    uint32_t embedding_dim;             /**< Embedding dimensionality */
    uint32_t ambient_dim;               /**< Original space dimension */

    /** Manifold properties */
    float intrinsic_dim_estimate;       /**< Estimated intrinsic dim */
    float explained_variance;           /**< Variance explained */
    float ricci_curvature;              /**< Local Ricci curvature */
    float local_density;                /**< Local point density */

    /** Context */
    uint32_t region_id;                 /**< Brain region */
    uint64_t sample_count;              /**< Samples in manifold */
} infogeo_manifold_msg_t;

/**
 * @brief KL divergence message
 */
typedef struct {
    infogeo_msg_header_t header;        /**< Message header */

    /** Divergence values */
    float kl_forward;                   /**< KL(P||Q) */
    float kl_reverse;                   /**< KL(Q||P) */
    float kl_symmetric;                 /**< Symmetric KL */
    float js_divergence;                /**< Jensen-Shannon divergence */

    /** Distribution info */
    uint32_t distribution_p_id;         /**< P distribution source */
    uint32_t distribution_q_id;         /**< Q distribution source */
    uint32_t sample_size;               /**< Samples for computation */

    /** Alert status */
    bool drift_detected;                /**< Significant drift detected */
    float drift_threshold;              /**< Threshold used */
} infogeo_kl_msg_t;

/**
 * @brief Geodesic distance message
 */
typedef struct {
    infogeo_msg_header_t header;        /**< Message header */

    /** Distance data */
    float geodesic_distance;            /**< Geodesic distance computed */
    float euclidean_distance;           /**< Euclidean for comparison */
    float curvature_effect;             /**< Geo/Euclidean ratio - 1 */

    /** Endpoint info */
    float point_a[INFOGEO_SUBSTRATE_MAX_DIM]; /**< First endpoint */
    float point_b[INFOGEO_SUBSTRATE_MAX_DIM]; /**< Second endpoint */
    uint32_t dim;                       /**< Point dimensionality */

    /** Path info */
    uint32_t path_steps;                /**< Steps in geodesic */
} infogeo_geodesic_msg_t;

/**
 * @brief Curvature update message
 */
typedef struct {
    infogeo_msg_header_t header;        /**< Message header */

    /** Curvature metrics */
    float ricci_curvature;              /**< Ricci scalar curvature */
    float sectional_curvature;          /**< Sectional curvature */
    float mean_curvature;               /**< Mean curvature */

    /** Location */
    float point[INFOGEO_SUBSTRATE_MAX_DIM]; /**< Point of measurement */
    uint32_t dim;                       /**< Dimensionality */

    /** Context */
    uint32_t region_id;                 /**< Brain region */
    bool high_curvature_alert;          /**< High curvature warning */
} infogeo_curvature_msg_t;

/**
 * @brief Convergence status message
 */
typedef struct {
    infogeo_msg_header_t header;        /**< Message header */

    /** Convergence info */
    bool converged;                     /**< Has optimization converged */
    float current_loss;                 /**< Current loss/free energy */
    float loss_change;                  /**< Loss change from last step */
    uint32_t iteration;                 /**< Current iteration */

    /** Rates */
    float convergence_rate;             /**< Estimated convergence rate */
    float estimated_iterations_left;    /**< Iterations to convergence */

    /** Context */
    uint32_t optimizer_id;              /**< Optimizer identifier */
} infogeo_convergence_msg_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** Broadcast settings */
    uint32_t broadcast_interval_ms;     /**< Auto-broadcast interval */
    bool enable_auto_broadcast;         /**< Enable auto broadcasting */
    bool enable_fisher_broadcast;       /**< Broadcast Fisher updates */
    bool enable_gradient_broadcast;     /**< Broadcast gradients */
    bool enable_manifold_broadcast;     /**< Broadcast manifold state */

    /** Message handling */
    uint32_t max_inbox_per_update;      /**< Max inbox messages per update */
    uint32_t message_ttl_ms;            /**< Message time-to-live */

    /** Subscription limits */
    uint32_t max_subscriptions;         /**< Maximum subscriptions */

    /** Alert thresholds */
    float kl_drift_threshold;           /**< KL for drift alert */
    float curvature_alert_threshold;    /**< High curvature threshold */

    /** Feature flags */
    bool enable_compression;            /**< Compress large messages */
    bool enable_logging;                /**< Enable message logging */
} infogeo_substrate_config_t;

/**
 * @brief Module subscription entry
 */
typedef struct {
    uint32_t module_id;                 /**< Subscribed module ID */
    uint32_t msg_type_mask;             /**< Subscribed message types */
    bool active;                        /**< Subscription active */
    uint64_t subscription_time;         /**< When subscribed */
    uint64_t messages_sent;             /**< Messages sent to this sub */
} infogeo_subscription_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /** Message counts */
    uint64_t messages_sent;             /**< Total messages sent */
    uint64_t messages_received;         /**< Total messages received */
    uint64_t messages_dropped;          /**< Messages dropped (queue full) */
    uint64_t broadcasts_sent;           /**< Broadcast messages */

    /** Per-type counts */
    uint64_t fisher_broadcasts;         /**< Fisher update messages */
    uint64_t gradient_broadcasts;       /**< Natural gradient messages */
    uint64_t manifold_broadcasts;       /**< Manifold state messages */
    uint64_t kl_broadcasts;             /**< KL divergence messages */
    uint64_t alerts_sent;               /**< Alert messages sent */

    /** Subscription stats */
    uint32_t active_subscriptions;      /**< Currently active subs */
    uint32_t peak_subscriptions;        /**< Peak subscription count */

    /** Timing stats */
    uint64_t last_broadcast_us;         /**< Last broadcast timestamp */
    float avg_message_latency_us;       /**< Average message latency */

    /** Error counts */
    uint64_t handler_errors;            /**< Message handler errors */
    uint64_t routing_errors;            /**< Routing failures */
} infogeo_substrate_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct infogeo_substrate_bridge_struct infogeo_substrate_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_default_config(
    infogeo_substrate_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Information Geometry-Substrate bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT infogeo_substrate_bridge_t* infogeo_substrate_bridge_create(
    const infogeo_substrate_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void infogeo_substrate_bridge_destroy(
    infogeo_substrate_bridge_t* bridge
);

/**
 * @brief Connect bridge to bio-router
 *
 * WHAT: Registers InfoGeo with bio-async message router
 * WHY:  Enables distributed communication
 * HOW:  Registers as module, sets up message handlers
 *
 * @param bridge Bridge handle
 * @param router Bio-router handle (opaque)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_connect(
    infogeo_substrate_bridge_t* bridge,
    void* router
);

/**
 * @brief Disconnect bridge from router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_disconnect(
    infogeo_substrate_bridge_t* bridge
);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge to check
 * @return true if connected to router
 */
NIMCP_EXPORT bool infogeo_substrate_is_connected(
    const infogeo_substrate_bridge_t* bridge
);

//=============================================================================
// Broadcast API
//=============================================================================

/**
 * @brief Broadcast Fisher matrix update
 *
 * WHAT: Sends Fisher information to subscribers
 * WHY:  Other modules need Fisher for natural gradients
 * HOW:  Packages Fisher diagonal and broadcasts
 *
 * @param bridge Bridge handle
 * @param fisher_diagonal Fisher matrix diagonal
 * @param dim Dimensionality
 * @param condition_number Matrix condition number
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_broadcast_fisher(
    infogeo_substrate_bridge_t* bridge,
    const float* fisher_diagonal,
    uint32_t dim,
    float condition_number
);

/**
 * @brief Broadcast natural gradient
 *
 * WHAT: Sends computed natural gradient to subscribers
 * WHY:  Learning modules use natural gradient for updates
 * HOW:  Packages gradient vectors and metadata
 *
 * @param bridge Bridge handle
 * @param natural_gradient Natural gradient vector
 * @param standard_gradient Original gradient vector
 * @param dim Dimensionality
 * @param learning_rate Recommended learning rate
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_broadcast_gradient(
    infogeo_substrate_bridge_t* bridge,
    const float* natural_gradient,
    const float* standard_gradient,
    uint32_t dim,
    float learning_rate
);

/**
 * @brief Broadcast manifold state
 *
 * WHAT: Sends manifold embedding state to subscribers
 * WHY:  Cognitive modules track neural manifold structure
 * HOW:  Packages embedding and manifold properties
 *
 * @param bridge Bridge handle
 * @param embedding Manifold embedding coordinates
 * @param embedding_dim Embedding dimensionality
 * @param curvature Local Ricci curvature
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_broadcast_manifold(
    infogeo_substrate_bridge_t* bridge,
    const float* embedding,
    uint32_t embedding_dim,
    float curvature
);

/**
 * @brief Broadcast KL divergence alert
 *
 * WHAT: Sends KL divergence measurement to subscribers
 * WHY:  Alerts modules of distribution drift
 * HOW:  Packages divergence metrics with alert status
 *
 * @param bridge Bridge handle
 * @param kl_forward KL(P||Q) divergence
 * @param kl_reverse KL(Q||P) divergence
 * @param drift_detected Whether drift exceeds threshold
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_broadcast_kl(
    infogeo_substrate_bridge_t* bridge,
    float kl_forward,
    float kl_reverse,
    bool drift_detected
);

/**
 * @brief Broadcast convergence status
 *
 * WHAT: Sends optimization convergence status
 * WHY:  Coordination of distributed learning
 * HOW:  Packages convergence metrics
 *
 * @param bridge Bridge handle
 * @param converged Whether optimization converged
 * @param current_loss Current loss value
 * @param iteration Current iteration count
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_broadcast_convergence(
    infogeo_substrate_bridge_t* bridge,
    bool converged,
    float current_loss,
    uint32_t iteration
);

//=============================================================================
// Subscription API
//=============================================================================

/**
 * @brief Subscribe module to InfoGeo messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to subscribe
 * @param msg_types Bitmask of message types
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_subscribe(
    infogeo_substrate_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from InfoGeo messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to unsubscribe
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_unsubscribe(
    infogeo_substrate_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Get subscriber count for message type
 *
 * @param bridge Bridge handle
 * @param msg_type Message type
 * @return Number of subscribers
 */
NIMCP_EXPORT uint32_t infogeo_substrate_subscriber_count(
    const infogeo_substrate_bridge_t* bridge,
    infogeo_substrate_msg_type_t msg_type
);

//=============================================================================
// Message Processing API
//=============================================================================

/**
 * @brief Process incoming messages
 *
 * WHAT: Processes messages from inbox
 * WHY:  Handles requests from other modules
 * HOW:  Dispatches to appropriate handlers
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_process_inbox(
    infogeo_substrate_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * @param bridge Bridge handle
 * @param dt_ms Time since last update (ms)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_update(
    infogeo_substrate_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_get_stats(
    const infogeo_substrate_bridge_t* bridge,
    infogeo_substrate_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_substrate_reset_stats(
    infogeo_substrate_bridge_t* bridge
);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Static string name
 */
NIMCP_EXPORT const char* infogeo_substrate_msg_type_name(
    infogeo_substrate_msg_type_t msg_type
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFOGEO_SUBSTRATE_BRIDGE_H */