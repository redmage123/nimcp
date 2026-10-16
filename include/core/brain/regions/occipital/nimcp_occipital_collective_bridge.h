//=============================================================================
// nimcp_occipital_collective_bridge.h - Occipital-Collective Cognition Integration
//=============================================================================
/**
 * @file nimcp_occipital_collective_bridge.h
 * @brief Bidirectional integration between Occipital Visual Cortex and
 *        Collective Cognition distributed consciousness system
 *
 * WHAT: Integration layer connecting occipital visual processing (V1-V5)
 *       with collective cognition for distributed visual perception
 * WHY:  Enable joint attention, shared visual processing, and collective
 *       perception across multiple brain instances
 * HOW:  Share visual features, coordinate attention, enable distributed
 *       scene understanding across the collective
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * JOINT ATTENTION (Tomasello):
 * ----------------------------
 * 1. Shared Gaze:
 *    - Multiple brains attend to same visual target
 *    - Gaze following and joint attention tracking
 *    - Synchronized visual attention across collective
 *
 * 2. Distributed Visual Processing:
 *    - Different instances process different parts of scene
 *    - Share extracted features for collective understanding
 *    - Merge visual representations across perspectives
 *
 * 3. Collective Scene Understanding:
 *    - Combine V1-V5 outputs from multiple viewers
 *    - Build shared model of visual environment
 *    - Detect occluded objects via multiple viewpoints
 *
 * HYPERSCANNING VISUAL SYNC:
 * --------------------------
 * - Gamma-band synchronization for visual binding
 * - Theta-band sync for attentional coordination
 * - Cross-instance visual feature alignment
 *
 * ARCHITECTURE:
 * ```
 * +==========================================================================+
 * |                   OCCIPITAL-COLLECTIVE COGNITION BRIDGE                   |
 * +==========================================================================+
 * |                                                                           |
 * |   +---------------------------+      +-------------------------------+   |
 * |   |   OCCIPITAL CORTEX        |      |    COLLECTIVE COGNITION       |   |
 * |   |   (V1-V5 Hierarchy)       |      |                               |   |
 * |   |                           |<---->|  - Joint Attention            |   |
 * |   |  - Edge detection (V1)    |      |  - Shared Intentionality      |   |
 * |   |  - Contour integ (V2)     |      |  - Hyperscanning sync         |   |
 * |   |  - Color/form (V4)        |      |  - Extended mind              |   |
 * |   |  - Motion (V5/MT)         |      |                               |   |
 * |   +---------------------------+      +-------------------------------+   |
 * |                 |                                   |                     |
 * |                 v                                   v                     |
 * |   +---------------------------+      +-------------------------------+   |
 * |   |   SHARED VISUAL STATE     |      |   ATTENTION COORDINATION      |   |
 * |   |                           |      |                               |   |
 * |   |  - Feature maps           |      |  - Joint attention targets    |   |
 * |   |  - Confidence scores      |----->|  - Gaze synchronization       |   |
 * |   |  - Attention weights      |      |  - Salience sharing           |   |
 * |   +---------------------------+      +-------------------------------+   |
 * |                                                                           |
 * +==========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_OCCIPITAL_COLLECTIVE_BRIDGE_H
#define NIMCP_OCCIPITAL_COLLECTIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct occipital_collective_bridge occipital_collective_bridge_t;
typedef struct occipital_adapter occipital_adapter_t;
typedef struct collective_cognition collective_cognition_t;

//=============================================================================
// Constants
//=============================================================================

/** Bio-async module ID */
#define BIO_MODULE_OCCIPITAL_COLLECTIVE 0x2E20

/** Maximum instances tracked */
#define OCCIPITAL_COLLECTIVE_MAX_INSTANCES 16

/** Maximum joint attention targets */
#define OCCIPITAL_COLLECTIVE_MAX_TARGETS 8

/** Maximum shared features */
#define OCCIPITAL_COLLECTIVE_MAX_FEATURES 64

/** Default update interval */
#define OCCIPITAL_COLLECTIVE_DEFAULT_UPDATE_MS 50

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Joint attention mode
 */
typedef enum {
    JOINT_ATTENTION_NONE = 0,       /**< No joint attention */
    JOINT_ATTENTION_FOLLOW,         /**< Follow others' attention */
    JOINT_ATTENTION_LEAD,           /**< Lead others' attention */
    JOINT_ATTENTION_COORDINATE      /**< Coordinate mutual attention */
} joint_attention_mode_t;

/**
 * @brief Visual sharing strategy
 */
typedef enum {
    VISUAL_SHARE_NONE = 0,          /**< No visual sharing */
    VISUAL_SHARE_FEATURES,          /**< Share extracted features */
    VISUAL_SHARE_ATTENTION,         /**< Share attention maps */
    VISUAL_SHARE_FULL               /**< Share full visual state */
} visual_sharing_strategy_t;

/* visual_area_t is defined in nimcp_occipital_adapter.h - include it */
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"

/**
 * @brief Joint attention target
 */
typedef struct {
    uint32_t target_id;             /**< Unique target identifier */
    float x;                        /**< Target X position (normalized 0-1) */
    float y;                        /**< Target Y position (normalized 0-1) */
    float salience;                 /**< Target salience (0-1) */
    uint32_t attending_count;       /**< Number of instances attending */
    uint32_t initiator_id;          /**< Instance that initiated attention */
    uint64_t created_ms;            /**< When target was created */
    bool active;                    /**< Whether target is still active */
} joint_attention_target_t;

/**
 * @brief Shared visual feature
 */
typedef struct {
    uint32_t feature_id;            /**< Feature identifier */
    visual_area_t source_area;      /**< Which V-area produced this */
    float confidence;               /**< Detection confidence (0-1) */
    float x;                        /**< Feature X location */
    float y;                        /**< Feature Y location */
    float orientation;              /**< Feature orientation (radians) */
    float scale;                    /**< Feature scale */
    uint32_t source_instance;       /**< Source instance ID */
    uint64_t timestamp_ms;          /**< When feature was detected */
} shared_visual_feature_t;

/**
 * @brief Per-instance visual state
 */
typedef struct {
    uint32_t instance_id;           /**< Instance identifier */
    float attention_x;              /**< Current attention X (0-1) */
    float attention_y;              /**< Current attention Y (0-1) */
    float v1_confidence;            /**< V1 processing confidence */
    float v2_confidence;            /**< V2 processing confidence */
    float v4_confidence;            /**< V4 processing confidence */
    float v5_confidence;            /**< V5 processing confidence */
    float overall_confidence;       /**< Overall visual confidence */
    uint32_t active_targets;        /**< Number of attended targets */
    bool is_leading_attention;      /**< Currently leading attention */
    uint64_t last_update_ms;        /**< Last state update */
} collective_visual_state_t;

/**
 * @brief Collective visual summary
 */
typedef struct {
    uint32_t total_instances;       /**< Connected instances */
    uint32_t active_joint_targets;  /**< Active joint attention targets */
    uint32_t shared_feature_count;  /**< Shared features */
    float average_confidence;       /**< Average visual confidence */
    float attention_coherence;      /**< How aligned is attention (0-1) */
    float gamma_sync;               /**< Gamma-band visual sync (0-1) */
    bool collective_attending;      /**< Majority attending same target */
    uint32_t attention_leader_id;   /**< Current attention leader */
} collective_visual_summary_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Attention settings */
    joint_attention_mode_t attention_mode;
    float attention_threshold;      /**< Salience threshold for attention */
    float follow_threshold;         /**< Threshold to follow others */

    /* Sharing settings */
    visual_sharing_strategy_t sharing_strategy;
    bool share_v1_features;         /**< Share V1 edge features */
    bool share_v2_features;         /**< Share V2 contour features */
    bool share_v4_features;         /**< Share V4 color/form features */
    bool share_v5_features;         /**< Share V5 motion features */

    /* Sync settings */
    float gamma_sync_threshold;     /**< Gamma sync for visual binding */
    float theta_sync_threshold;     /**< Theta sync for attention coord */

    /* Update settings */
    uint32_t update_interval_ms;    /**< State broadcast interval */
    uint32_t feature_timeout_ms;    /**< Shared feature timeout */
    uint32_t target_timeout_ms;     /**< Joint target timeout */

    /* Feature flags */
    bool enable_bio_async;          /**< Enable bio-async messaging */
    bool enable_gaze_following;     /**< Enable gaze following */
    bool enable_feature_merge;      /**< Merge features across instances */
} occipital_collective_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t attention_events_sent; /**< Attention events broadcast */
    uint64_t attention_events_recv; /**< Attention events received */
    uint64_t features_shared;       /**< Features shared */
    uint64_t features_received;     /**< Features received from others */
    uint64_t joint_targets_created; /**< Joint targets initiated */
    uint64_t gaze_follows;          /**< Times followed others' gaze */
    float avg_attention_coherence;  /**< Average attention coherence */
    float avg_gamma_sync;           /**< Average gamma synchronization */
} occipital_collective_stats_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 * @param config Output configuration structure
 */
void occipital_collective_default_config(occipital_collective_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create occipital-collective bridge
 *
 * @param config Configuration (NULL for defaults)
 * @param occipital Occipital adapter (optional)
 * @param collective Collective cognition system (optional)
 * @return Bridge handle or NULL on failure
 */
occipital_collective_bridge_t* occipital_collective_create(
    const occipital_collective_config_t* config,
    occipital_adapter_t* occipital,
    collective_cognition_t* collective
);

/**
 * @brief Destroy bridge
 * @param bridge Bridge to destroy
 */
void occipital_collective_destroy(occipital_collective_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int occipital_collective_reset(occipital_collective_bridge_t* bridge);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect to occipital adapter
 *
 * @param bridge Bridge to connect
 * @param occipital Occipital adapter
 * @return 0 on success, -1 on error
 */
int occipital_collective_connect_occipital(
    occipital_collective_bridge_t* bridge,
    occipital_adapter_t* occipital
);

/**
 * @brief Connect to collective cognition
 *
 * @param bridge Bridge to connect
 * @param collective Collective cognition system
 * @return 0 on success, -1 on error
 */
int occipital_collective_connect_collective(
    occipital_collective_bridge_t* bridge,
    collective_cognition_t* collective
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Main update cycle
 *
 * @param bridge Bridge to update
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int occipital_collective_update(
    occipital_collective_bridge_t* bridge,
    uint64_t delta_ms
);

//=============================================================================
// Joint Attention API
//=============================================================================

/**
 * @brief Initiate joint attention at location
 *
 * @param bridge Bridge to use
 * @param x Target X position (0-1)
 * @param y Target Y position (0-1)
 * @param salience Salience of target (0-1)
 * @param target_id Output: assigned target ID
 * @return 0 on success, -1 on error
 */
int occipital_collective_initiate_attention(
    occipital_collective_bridge_t* bridge,
    float x,
    float y,
    float salience,
    uint32_t* target_id
);

/**
 * @brief Follow joint attention target
 *
 * @param bridge Bridge to use
 * @param target_id Target to follow
 * @return 0 on success, -1 on error
 */
int occipital_collective_follow_attention(
    occipital_collective_bridge_t* bridge,
    uint32_t target_id
);

/**
 * @brief Release attention from target
 *
 * @param bridge Bridge to use
 * @param target_id Target to release
 * @return 0 on success, -1 on error
 */
int occipital_collective_release_attention(
    occipital_collective_bridge_t* bridge,
    uint32_t target_id
);

/**
 * @brief Get active joint attention targets
 *
 * @param bridge Bridge to query
 * @param targets Output array of targets
 * @param max_targets Maximum targets to return
 * @param count Output: actual count returned
 * @return 0 on success, -1 on error
 */
int occipital_collective_get_targets(
    const occipital_collective_bridge_t* bridge,
    joint_attention_target_t* targets,
    uint32_t max_targets,
    uint32_t* count
);

//=============================================================================
// Feature Sharing API
//=============================================================================

/**
 * @brief Share visual feature with collective
 *
 * @param bridge Bridge to use
 * @param feature Feature to share
 * @return 0 on success, -1 on error
 */
int occipital_collective_share_feature(
    occipital_collective_bridge_t* bridge,
    const shared_visual_feature_t* feature
);

/**
 * @brief Get shared features from collective
 *
 * @param bridge Bridge to query
 * @param area Visual area filter (or -1 for all)
 * @param features Output array of features
 * @param max_features Maximum features to return
 * @param count Output: actual count returned
 * @return 0 on success, -1 on error
 */
int occipital_collective_get_features(
    const occipital_collective_bridge_t* bridge,
    int area,
    shared_visual_feature_t* features,
    uint32_t max_features,
    uint32_t* count
);

/**
 * @brief Merge features from same location across instances
 *
 * @param bridge Bridge to use
 * @param tolerance Position tolerance for merging
 * @return Number of features merged, -1 on error
 */
int occipital_collective_merge_features(
    occipital_collective_bridge_t* bridge,
    float tolerance
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get collective visual summary
 *
 * @param bridge Bridge to query
 * @param summary Output summary structure
 * @return 0 on success, -1 on error
 */
int occipital_collective_get_summary(
    const occipital_collective_bridge_t* bridge,
    collective_visual_summary_t* summary
);

/**
 * @brief Get instance visual state
 *
 * @param bridge Bridge to query
 * @param instance_id Instance to query
 * @param state Output state structure
 * @return 0 on success, -1 if instance not found
 */
int occipital_collective_get_instance_state(
    const occipital_collective_bridge_t* bridge,
    uint32_t instance_id,
    collective_visual_state_t* state
);

/**
 * @brief Get local instance ID
 *
 * @param bridge Bridge to query
 * @return Local instance ID
 */
uint32_t occipital_collective_get_local_id(
    const occipital_collective_bridge_t* bridge
);

/**
 * @brief Check if local instance is leading attention
 *
 * @param bridge Bridge to query
 * @return true if leading attention
 */
bool occipital_collective_is_attention_leader(
    const occipital_collective_bridge_t* bridge
);

/**
 * @brief Get attention coherence score
 *
 * @param bridge Bridge to query
 * @return Coherence score (0-1)
 */
float occipital_collective_get_coherence(
    const occipital_collective_bridge_t* bridge
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int occipital_collective_get_stats(
    const occipital_collective_bridge_t* bridge,
    occipital_collective_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge to reset
 */
void occipital_collective_reset_stats(occipital_collective_bridge_t* bridge);

//=============================================================================
// Bio-Async API
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, -1 on error
 */
int occipital_collective_connect_bio_async(occipital_collective_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, -1 on error
 */
int occipital_collective_disconnect_bio_async(occipital_collective_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_COLLECTIVE_BRIDGE_H */
