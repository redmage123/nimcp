/**
 * @file nimcp_rsc_bio_async_bridge.h
 * @brief Retrosplenial Cortex Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for retrosplenial cortex that provides
 *       comprehensive message routing for spatial-contextual processing via
 *       the bio-router.
 *
 * WHY: The retrosplenial cortex processes spatial-contextual integration and needs to:
 *      - Route frame transformation results to parietal and hippocampus
 *      - Broadcast context changes for episodic memory encoding
 *      - Synchronize head direction signals with thalamus and entorhinal cortex
 *      - Coordinate landmark detections with navigation systems
 *      - Signal imagination state changes to prefrontal cortex
 *
 * HOW: Registers RSC as a bio-router module, maintains subscription registry,
 *      provides typed message broadcast APIs, and processes incoming spatial
 *      and contextual queries.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * RSC OUTPUT PATHWAYS:
 * --------------------
 * 1. Cortical projections:
 *    - Hippocampus: Context for memory encoding/retrieval
 *    - Parietal cortex: Transformed spatial coordinates
 *    - Prefrontal cortex: Planning and imagination state
 *    - Mapped to: RSC_BIO_MSG_CONTEXT, RSC_BIO_MSG_FRAME_TRANSFORM
 *
 * 2. Subcortical projections:
 *    - Entorhinal cortex: Path integration support
 *    - Thalamus (anterior nuclei): Head direction coordination
 *    - Mapped to: RSC_BIO_MSG_HEAD_DIRECTION, RSC_BIO_MSG_NAVIGATION
 *
 * 3. Scene/landmark signals:
 *    - Visual areas: Scene familiarity feedback
 *    - Navigation systems: Landmark anchoring
 *    - Mapped to: RSC_BIO_MSG_SCENE_FAMILIARITY, RSC_BIO_MSG_LANDMARK_DETECTED
 *
 * RSC INPUT PATHWAYS:
 * -------------------
 * 1. Visual (scene information):
 *    - Scene features from visual cortex
 *    - Landmark visual signatures
 *
 * 2. Parietal (egocentric space):
 *    - Body-centered spatial coordinates
 *    - Attention direction signals
 *
 * 3. Thalamic (head direction):
 *    - Head direction cell signals
 *    - Orientation updates
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RSC_BIO_ASYNC_BRIDGE_H
#define NIMCP_RSC_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/regions/retrosplenial/nimcp_retrosplenial.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Module ID for retrosplenial cortex in bio-async system (0x5000 - 0x50FF reserved) */
#define BIO_MODULE_ID_RSC               0x5001

/** Maximum number of module subscriptions */
#define RSC_BIO_MAX_SUBSCRIPTIONS       64

/** Maximum pending messages in inbox */
#define RSC_BIO_MAX_INBOX_SIZE          256

/** Maximum pending messages in outbox */
#define RSC_BIO_MAX_OUTBOX_SIZE         128

/** Default broadcast interval for navigation state (ms) */
#define RSC_BIO_DEFAULT_BROADCAST_INTERVAL_MS  50

/** Message expiry time (ms) */
#define RSC_BIO_MESSAGE_TTL_MS          5000

/** Context change significance threshold */
#define RSC_BIO_CONTEXT_CHANGE_THRESHOLD 0.3f

/** Scene familiarity novelty threshold */
#define RSC_BIO_NOVELTY_THRESHOLD       0.5f

/* ============================================================================
 * Message Types - Use nimcp_rsc_bio_msg_type_t from nimcp_retrosplenial.h
 *
 * Available types:
 *   RSC_BIO_MSG_CONTEXT           - Context encoding broadcast
 *   RSC_BIO_MSG_NAVIGATION        - Navigation state update
 *   RSC_BIO_MSG_SCENE_FAMILIARITY - Scene familiarity signal
 *   RSC_BIO_MSG_FRAME_TRANSFORM   - Reference frame transform result
 *   RSC_BIO_MSG_LANDMARK_DETECTED - Landmark detection event
 *   RSC_BIO_MSG_HEAD_DIRECTION    - Head direction update
 *   RSC_BIO_MSG_IMAGINATION_STATE - Imagination/planning state
 *   RSC_BIO_MSG_CONTEXT_REQUEST   - Request for current context
 *   RSC_BIO_MSG_TRANSFORM_REQUEST - Request frame transformation
 *   RSC_BIO_MSG_COUNT             - Total message type count
 *
 * Subscription bitmasks (from nimcp_retrosplenial.h):
 *   RSC_BIO_SUB_CONTEXT, RSC_BIO_SUB_NAVIGATION, RSC_BIO_SUB_SCENE_FAMILIARITY,
 *   RSC_BIO_SUB_FRAME_TRANSFORM, RSC_BIO_SUB_LANDMARK_DETECTED,
 *   RSC_BIO_SUB_HEAD_DIRECTION, RSC_BIO_SUB_IMAGINATION_STATE, RSC_BIO_SUB_ALL
 * ============================================================================ */

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Frame transformation message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Transformation result */
    float transform_matrix[16];         /**< 4x4 transformation matrix */
    nimcp_rsc_frame_t source_frame;     /**< Source reference frame */
    nimcp_rsc_frame_t target_frame;     /**< Target reference frame */
    float accuracy;                     /**< Transformation accuracy [0, 1] */

    /* Transformed coordinates */
    float input_position[3];            /**< Input position */
    float output_position[3];           /**< Output position */

    uint64_t timestamp_us;              /**< Transformation timestamp */
} rsc_bio_frame_transform_msg_t;

/**
 * @brief Context update message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Context state */
    float context_vector[128];          /**< Unified context encoding */
    uint32_t context_dim;               /**< Actual dimension used */
    nimcp_rsc_context_type_t dominant_type; /**< Dominant context type */
    float context_strength;             /**< Overall context strength [0, 1] */
    float context_stability;            /**< Context stability [0, 1] */

    /* Change information */
    float change_magnitude;             /**< Context change magnitude */
    bool is_significant_change;         /**< Change exceeds threshold */

    uint64_t timestamp_us;              /**< Context timestamp */
} rsc_bio_context_update_msg_t;

/**
 * @brief Head direction message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Head direction state */
    float head_direction;               /**< Head direction (radians) */
    float previous_direction;           /**< Previous direction for delta */
    float angular_velocity;             /**< Angular velocity (rad/s) */
    float confidence;                   /**< HD confidence [0, 1] */

    /* HD cell activations (compact) */
    uint32_t peak_cell_index;           /**< Index of most active HD cell */
    float peak_activation;              /**< Peak HD cell activation */
    uint32_t num_hd_cells;              /**< Total HD cells */

    uint64_t timestamp_us;              /**< HD update timestamp */
} rsc_bio_head_direction_msg_t;

/**
 * @brief Landmark detection message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Landmark identification */
    uint32_t landmark_id;               /**< Landmark identifier */
    char landmark_name[64];             /**< Landmark name */

    /* Landmark position */
    float position[3];                  /**< Allocentric position */
    float bearing;                      /**< Bearing from current pose */
    float distance;                     /**< Distance estimate */

    /* Recognition confidence */
    float recognition_strength;         /**< Recognition confidence [0, 1] */
    float salience;                     /**< Visual salience [0, 1] */
    bool is_anchored;                   /**< Used for spatial anchoring */

    uint64_t timestamp_us;              /**< Detection timestamp */
} rsc_bio_landmark_msg_t;

/**
 * @brief Scene familiarity message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Scene recognition */
    nimcp_rsc_familiarity_t familiarity_level; /**< Categorical familiarity */
    float familiarity_score;            /**< Familiarity score [0, 1] */
    float scene_coherence;              /**< Scene coherence [0, 1] */

    /* Scene components */
    uint32_t num_landmarks_detected;    /**< Landmarks in scene */
    float layout_confidence;            /**< Layout encoding confidence */
    bool is_novel;                      /**< Scene is novel */

    /* Compact scene encoding */
    float scene_vector[64];             /**< Compact scene representation */

    uint64_t timestamp_us;              /**< Recognition timestamp */
} rsc_bio_scene_familiarity_msg_t;

/**
 * @brief Imagination state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Imagination state */
    nimcp_rsc_imagine_mode_t mode;      /**< Current imagination mode */
    bool active;                        /**< Imagination active */
    float vividness;                    /**< Imagination vividness [0, 1] */
    float plausibility;                 /**< Scenario plausibility [0, 1] */

    /* Imagined location */
    float imagined_position[3];         /**< Imagined position */
    float imagined_heading;             /**< Imagined heading */
    float temporal_distance;            /**< Temporal distance (seconds) */

    /* Simulation progress */
    uint32_t steps_simulated;           /**< Simulation steps completed */
    float goal_proximity;               /**< How close to imagined goal */

    uint64_t timestamp_us;              /**< State timestamp */
} rsc_bio_imagination_state_msg_t;

/**
 * @brief Navigation state message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Current pose */
    float position[3];                  /**< Current position estimate */
    float heading;                      /**< Current heading (radians) */
    float speed;                        /**< Current speed */
    float pose_confidence;              /**< Pose estimate confidence [0, 1] */

    /* Goal state */
    bool goal_active;                   /**< Navigation goal is active */
    float goal_position[3];             /**< Goal position */
    float distance_to_goal;             /**< Distance to goal */
    float bearing_to_goal;              /**< Bearing to goal (radians) */

    /* Path state */
    uint32_t current_waypoint;          /**< Current waypoint index */
    uint32_t total_waypoints;           /**< Total waypoints */

    uint64_t timestamp_us;              /**< Navigation timestamp */
} rsc_bio_navigation_msg_t;

/**
 * @brief Context request message payload (incoming)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Request parameters */
    nimcp_rsc_context_type_t context_types; /**< Which context types requested */
    uint32_t requester_module;          /**< Requesting module ID */
    bool include_history;               /**< Include context history */

    uint64_t timestamp_us;              /**< Request timestamp */
} rsc_bio_context_request_msg_t;

/**
 * @brief Transform request message payload (incoming)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Transformation request */
    float input_position[3];            /**< Position to transform */
    nimcp_rsc_frame_t source_frame;     /**< Source reference frame */
    nimcp_rsc_frame_t target_frame;     /**< Target reference frame */

    uint32_t requester_module;          /**< Requesting module ID */
    uint32_t request_id;                /**< Request ID for matching response */

    uint64_t timestamp_us;              /**< Request timestamp */
} rsc_bio_transform_request_msg_t;

/**
 * @brief Pose update message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Pose state */
    float position[3];                  /**< Estimated position */
    float orientation[3];               /**< Yaw, pitch, roll (radians) */
    float confidence;                   /**< Pose confidence [0, 1] */

    /* Velocity state */
    float velocity[3];                  /**< Linear velocity */
    float angular_velocity[3];          /**< Angular velocity */

    uint64_t timestamp_us;              /**< Pose timestamp */
} rsc_bio_pose_update_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

/**
 * @brief Module subscription entry
 */
typedef struct {
    bio_module_id_t module_id;          /**< Subscribed module ID */
    uint32_t msg_type_mask;             /**< Bitmask of subscribed types */
    bool active;                        /**< Subscription active */
    uint64_t subscription_time;         /**< When subscribed */
    uint64_t messages_sent;             /**< Messages sent to this sub */
} rsc_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief RSC bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t navigation_broadcast_interval_ms;   /**< Navigation state interval */
    uint32_t context_broadcast_interval_ms;      /**< Context broadcast interval */
    bool enable_auto_broadcast;                  /**< Auto-broadcast state */

    /* Message handling */
    uint32_t max_inbox_process_per_update;       /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                     /**< Message time-to-live */

    /* Threshold settings */
    float context_change_threshold;              /**< Context change threshold */
    float novelty_threshold;                     /**< Scene novelty threshold */
    nimcp_bio_channel_type_t default_channel;    /**< Default channel */
    nimcp_bio_channel_type_t urgent_channel;     /**< Channel for urgent messages */

    /* Subscription limits */
    uint32_t max_subscriptions;                  /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_frame_transform_routing;         /**< Enable frame transform routing */
    bool enable_context_routing;                 /**< Enable context routing */
    bool enable_navigation_routing;              /**< Enable navigation routing */
    bool enable_landmark_routing;                /**< Enable landmark routing */
    bool enable_imagination_routing;             /**< Enable imagination routing */
    bool enable_logging;                         /**< Enable message logging */
} rsc_bio_async_bridge_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;                      /**< Total messages sent */
    uint64_t messages_received;                  /**< Total messages received */
    uint64_t messages_dropped;                   /**< Messages dropped (queue full) */
    uint64_t broadcasts_sent;                    /**< Broadcast messages sent */

    /* Per-type counts */
    uint64_t frame_transforms_sent;              /**< Frame transform broadcasts */
    uint64_t context_updates_sent;               /**< Context updates sent */
    uint64_t head_direction_sent;                /**< Head direction updates sent */
    uint64_t landmarks_sent;                     /**< Landmark detections sent */
    uint64_t scene_familiarity_sent;             /**< Scene familiarity sent */
    uint64_t imagination_updates_sent;           /**< Imagination updates sent */
    uint64_t navigation_updates_sent;            /**< Navigation updates sent */

    /* Subscription stats */
    uint32_t active_subscriptions;               /**< Currently active subs */
    uint32_t peak_subscriptions;                 /**< Peak subscription count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;             /**< Last broadcast timestamp */
    float avg_message_latency_us;                /**< Average message latency */
    float max_message_latency_us;                /**< Peak message latency */

    /* Error counts */
    uint64_t handler_errors;                     /**< Message handler errors */
    uint64_t routing_errors;                     /**< Routing failures */
} rsc_bio_async_bridge_stats_t;

/* ============================================================================
 * Bridge Structure (opaque handle)
 * ============================================================================ */

/**
 * @brief RSC bio-async bridge handle (opaque)
 *
 * Note: This is a separate opaque structure from the simple rsc_bio_async_bridge_t
 * defined in nimcp_retrosplenial.h. This provides full messaging capabilities.
 */
typedef struct rsc_bio_async_router_struct rsc_bio_async_router_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_default_config(rsc_bio_async_bridge_config_t* config);

/**
 * @brief Create RSC bio-async bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle, or NULL on error
 */
rsc_bio_async_router_t* rsc_bio_async_router_create(
    const rsc_bio_async_bridge_config_t* config
);

/**
 * @brief Destroy RSC bio-async bridge
 *
 * @param bridge Bridge to destroy
 */
void rsc_bio_async_router_destroy(rsc_bio_async_router_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to RSC and router
 *
 * @param bridge Bridge handle
 * @param rsc Retrosplenial cortex instance
 * @param router Bio-router for message dispatch
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_router_connect(
    rsc_bio_async_router_t* bridge,
    nimcp_retrosplenial_t* rsc,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_router_disconnect(rsc_bio_async_router_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool rsc_bio_async_router_is_connected(const rsc_bio_async_router_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed, or -1 on error
 */
int rsc_bio_async_router_process_inbox(
    rsc_bio_async_router_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update (ms)
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_router_update(
    rsc_bio_async_router_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Broadcast API - Frame Transformation
 * ============================================================================ */

/**
 * @brief Broadcast frame transformation result
 *
 * @param bridge Bridge handle
 * @param source_frame Source reference frame
 * @param target_frame Target reference frame
 * @param input_pos Input position
 * @param output_pos Output position
 * @param accuracy Transformation accuracy
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_broadcast_frame_transform(
    rsc_bio_async_router_t* bridge,
    nimcp_rsc_frame_t source_frame,
    nimcp_rsc_frame_t target_frame,
    const float* input_pos,
    const float* output_pos,
    float accuracy
);

/* ============================================================================
 * Broadcast API - Context
 * ============================================================================ */

/**
 * @brief Broadcast context update
 *
 * @param bridge Bridge handle
 * @param context_vector Context encoding vector
 * @param context_dim Context dimension
 * @param dominant_type Dominant context type
 * @param strength Context strength
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_broadcast_context(
    rsc_bio_async_router_t* bridge,
    const float* context_vector,
    uint32_t context_dim,
    nimcp_rsc_context_type_t dominant_type,
    float strength
);

/* ============================================================================
 * Broadcast API - Head Direction
 * ============================================================================ */

/**
 * @brief Broadcast head direction update
 *
 * @param bridge Bridge handle
 * @param head_direction Current head direction (radians)
 * @param angular_velocity Angular velocity
 * @param confidence Direction confidence
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_broadcast_head_direction(
    rsc_bio_async_router_t* bridge,
    float head_direction,
    float angular_velocity,
    float confidence
);

/* ============================================================================
 * Broadcast API - Landmarks
 * ============================================================================ */

/**
 * @brief Broadcast landmark detection
 *
 * @param bridge Bridge handle
 * @param landmark_id Landmark identifier
 * @param name Landmark name
 * @param position Allocentric position
 * @param recognition_strength Recognition confidence
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_broadcast_landmark(
    rsc_bio_async_router_t* bridge,
    uint32_t landmark_id,
    const char* name,
    const float* position,
    float recognition_strength
);

/* ============================================================================
 * Broadcast API - Scene Familiarity
 * ============================================================================ */

/**
 * @brief Broadcast scene familiarity result
 *
 * @param bridge Bridge handle
 * @param familiarity_level Categorical familiarity level
 * @param familiarity_score Familiarity score [0, 1]
 * @param scene_coherence Scene coherence
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_broadcast_scene_familiarity(
    rsc_bio_async_router_t* bridge,
    nimcp_rsc_familiarity_t familiarity_level,
    float familiarity_score,
    float scene_coherence
);

/* ============================================================================
 * Broadcast API - Imagination
 * ============================================================================ */

/**
 * @brief Broadcast imagination state change
 *
 * @param bridge Bridge handle
 * @param mode Imagination mode
 * @param active Whether imagination is active
 * @param vividness Imagination vividness
 * @param plausibility Scenario plausibility
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_broadcast_imagination_state(
    rsc_bio_async_router_t* bridge,
    nimcp_rsc_imagine_mode_t mode,
    bool active,
    float vividness,
    float plausibility
);

/* ============================================================================
 * Broadcast API - Navigation
 * ============================================================================ */

/**
 * @brief Broadcast navigation state update
 *
 * @param bridge Bridge handle
 * @param position Current position
 * @param heading Current heading
 * @param speed Current speed
 * @param pose_confidence Pose confidence
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_broadcast_navigation(
    rsc_bio_async_router_t* bridge,
    const float* position,
    float heading,
    float speed,
    float pose_confidence
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to RSC messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to subscribe
 * @param msg_types Bitmask of message types (use RSC_BIO_SUB_* macros)
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_subscribe_module(
    rsc_bio_async_router_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from RSC messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to unsubscribe
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_unsubscribe_module(
    rsc_bio_async_router_t* bridge,
    uint32_t module_id
);

/**
 * @brief Update module subscription types
 *
 * @param bridge Bridge handle
 * @param module_id Module to update
 * @param msg_types New bitmask of message types
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_update_subscription(
    rsc_bio_async_router_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Get subscription count for message type
 *
 * @param bridge Bridge handle
 * @param msg_type Message type to query
 * @return Number of subscribers
 */
uint32_t rsc_bio_async_get_subscriber_count(
    const rsc_bio_async_router_t* bridge,
    nimcp_rsc_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_get_stats(
    const rsc_bio_async_router_t* bridge,
    rsc_bio_async_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int rsc_bio_async_reset_stats(rsc_bio_async_router_t* bridge);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Static string name
 */
const char* rsc_bio_msg_type_name(nimcp_rsc_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bridge handle
 */
void rsc_bio_async_print_summary(const rsc_bio_async_router_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RSC_BIO_ASYNC_BRIDGE_H */
