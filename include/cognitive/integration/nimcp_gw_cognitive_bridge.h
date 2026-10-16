/**
 * @file nimcp_gw_cognitive_bridge.h
 * @brief Bridge between Global Workspace and Cognitive Modules
 *
 * WHAT: Integration enabling Global Workspace to broadcast to all cognitive modules
 *       and modules to compete for access to the Global Workspace.
 *
 * WHY: Global Workspace Theory (GWT) proposes that conscious awareness arises from
 *      a global broadcast mechanism where winning content is shared across all
 *      specialized cognitive modules. This bridge implements that architecture.
 *
 * HOW: Cognitive modules register as receivers and submit content to compete for
 *      GW access. Winning content is broadcast to all registered modules.
 *      Competition is based on priority, relevance, and current GW state.
 *
 * BIOLOGICAL BASIS:
 * - Global Workspace corresponds to widespread cortical activation
 * - Competition occurs in thalamo-cortical loops (attention competition)
 * - Broadcast implemented by synchronized gamma oscillations
 * - Prefrontal-parietal network coordinates conscious access
 * - "Ignition" threshold determines when content reaches awareness
 *
 * Integration Pattern:
 * GW -> Modules (Broadcast):
 *   - Winning content broadcast to all registered modules
 *   - Modules receive via registered callbacks
 *   - Broadcast includes content type, data, and priority
 *
 * Modules -> GW (Competition):
 *   - Modules submit content with priority
 *   - Competition resolved based on priority and relevance
 *   - Winners gain access to broadcast mechanism
 *   - Losers may resubmit with updated priority
 *
 * @author NIMCP Development Team
 * @date 2025-01
 */

#ifndef NIMCP_GW_COGNITIVE_BRIDGE_H
#define NIMCP_GW_COGNITIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Opaque Type
 * ============================================================================ */

/**
 * @brief Opaque GlobalWorkspace-Cognitive bridge structure
 *
 * WHAT: Forward declaration for GW-Cognitive bridge
 * WHY: Encapsulates implementation details
 * HOW: Full definition in implementation file
 */
typedef struct gw_cognitive_bridge gw_cognitive_bridge_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * @brief Maximum number of registered receiver modules
 */
#define GW_COGNITIVE_MAX_RECEIVERS 64

/**
 * @brief Default broadcast threshold
 */
#define GW_COGNITIVE_DEFAULT_BROADCAST_THRESHOLD 0.6f

/**
 * @brief Default competition timeout (ms)
 */
#define GW_COGNITIVE_DEFAULT_COMPETITION_TIMEOUT_MS 100

/**
 * @brief Content type identifiers for GW broadcast
 */
typedef enum {
    GW_COGNITIVE_CONTENT_PERCEPTION = 0,   /**< Perceptual content */
    GW_COGNITIVE_CONTENT_MEMORY,           /**< Memory retrieval */
    GW_COGNITIVE_CONTENT_THOUGHT,          /**< Abstract thought */
    GW_COGNITIVE_CONTENT_EMOTION,          /**< Emotional state */
    GW_COGNITIVE_CONTENT_INTENTION,        /**< Goal/intention */
    GW_COGNITIVE_CONTENT_MOTOR,            /**< Motor plan */
    GW_COGNITIVE_CONTENT_LANGUAGE,         /**< Linguistic content */
    GW_COGNITIVE_CONTENT_IMAGERY           /**< Mental imagery */
} gw_cognitive_content_type_t;

/**
 * @brief Callback function type for broadcast receivers
 *
 * @param content_type Type of broadcast content
 * @param content_data Pointer to content data
 * @param content_size Size of content data in bytes
 * @param user_data User-provided context
 */
typedef void (*gw_cognitive_receiver_callback_t)(
    gw_cognitive_content_type_t content_type,
    const void* content_data,
    size_t content_size,
    void* user_data
);

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Configuration for GW-Cognitive bridge
 *
 * WHAT: Parameters controlling GW broadcast and competition behavior
 *
 * WHY: Different scenarios require different competition dynamics
 *      and broadcast characteristics
 *
 * HOW: Configure thresholds, timeouts, and capacity limits
 */
typedef struct {
    /** Threshold for content to win broadcast [0-1] (default: 0.6)
     *  Higher values require stronger content to reach awareness */
    float broadcast_threshold;

    /** Timeout for competition resolution (ms) (default: 100)
     *  After this time, highest priority content wins */
    uint32_t competition_timeout_ms;

    /** Maximum number of simultaneous competitors (default: 16) */
    uint32_t max_competitors;

    /** Enable automatic broadcast on competition winner */
    bool enable_auto_broadcast;

    /** Enable priority decay for persistent competitors */
    bool enable_priority_decay;

    /** Priority decay rate per competition cycle [0-1] */
    float priority_decay_rate;

    /** Minimum priority to participate in competition [0-1] */
    float min_competition_priority;
} gw_cognitive_config_t;

/**
 * @brief Content submission for GW competition
 *
 * WHAT: Content submitted by a module for GW competition
 *
 * WHY: Modules need to submit content with priority for competition
 *
 * HOW: Contains content data, type, size, and priority
 */
typedef struct {
    /** Content type */
    gw_cognitive_content_type_t content_type;

    /** Pointer to content data */
    const void* content_data;

    /** Size of content data in bytes */
    size_t content_size;

    /** Priority of this content [0-1] (higher = more likely to win) */
    float priority;

    /** Relevance to current context [0-1] */
    float relevance;

    /** Urgency of broadcast [0-1] */
    float urgency;
} gw_cognitive_content_t;

/**
 * @brief Current GW content (conscious content)
 *
 * WHAT: Content currently in the Global Workspace
 *
 * WHY: Enables querying what is currently "conscious"
 *
 * HOW: Contains winning content from last competition
 */
typedef struct {
    /** Type of current content */
    gw_cognitive_content_type_t content_type;

    /** Module ID that submitted winning content */
    uint32_t source_module_id;

    /** Priority that won competition [0-1] */
    float winning_priority;

    /** Time content has been in GW (ms) */
    uint32_t duration_ms;

    /** Number of broadcasts of this content */
    uint32_t broadcast_count;

    /** Is there valid content in GW */
    bool has_content;

    /** Content data buffer (caller must provide adequate size) */
    void* content_buffer;

    /** Size of content_buffer */
    size_t buffer_size;

    /** Actual size of content (may be less than buffer_size) */
    size_t content_size;
} gw_cognitive_conscious_content_t;

/**
 * @brief Statistics for GW-Cognitive bridge
 *
 * WHAT: Performance and activity metrics for the bridge
 *
 * WHY: Monitor GW dynamics and broadcast effectiveness
 *
 * HOW: Accumulates counts during bridge operation
 */
typedef struct {
    /** Number of broadcasts sent to all modules */
    uint64_t broadcasts_sent;

    /** Number of competition cycles held */
    uint64_t competitions_held;

    /** Number of content updates in GW */
    uint64_t content_updates;

    /** Number of registered receiver modules */
    uint32_t registered_receivers;

    /** Number of current competitors */
    uint32_t active_competitors;

    /** Average winning priority [0-1] */
    float avg_winning_priority;

    /** Average competition duration (ms) */
    float avg_competition_time_ms;

    /** Number of broadcast failures */
    uint64_t broadcast_failures;

    /** Number of competition timeouts */
    uint64_t competition_timeouts;
} gw_cognitive_stats_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Initialize default GW-Cognitive configuration
 *
 * WHAT: Sets default parameters for GW-Cognitive bridge
 * WHY: Provides sensible defaults for typical use cases
 * HOW: Initializes config with balanced parameters
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int gw_cognitive_default_config(gw_cognitive_config_t* config);

/**
 * @brief Create GW-Cognitive bridge
 *
 * WHAT: Allocates and initializes GW-Cognitive integration bridge
 * WHY: Establishes broadcast and competition infrastructure
 * HOW: Creates bridge, initializes receiver registry, sets up competition
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Pointer to created bridge, NULL on failure
 */
gw_cognitive_bridge_t* gw_cognitive_bridge_create(
    const gw_cognitive_config_t* config
);

/**
 * @brief Destroy GW-Cognitive bridge
 *
 * WHAT: Cleans up and deallocates bridge
 * WHY: Prevents memory leaks and releases resources
 * HOW: Unregisters receivers, clears state, deallocates bridge
 *
 * @param bridge Bridge to destroy
 */
void gw_cognitive_bridge_destroy(gw_cognitive_bridge_t* bridge);

/**
 * @brief Broadcast content to all registered modules
 *
 * WHAT: Sends content from GW to all registered receiver modules
 * WHY: Implements GWT broadcast mechanism for conscious sharing
 * HOW: Invokes registered callbacks with content data
 *
 * @param bridge Bridge instance
 * @param content_type Type of content being broadcast
 * @param content_data Pointer to content data
 * @param content_size Size of content data in bytes
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: Corresponds to widespread synchronized gamma oscillations
 *                   that share winning content across cortical areas.
 */
int gw_cognitive_broadcast(
    gw_cognitive_bridge_t* bridge,
    gw_cognitive_content_type_t content_type,
    const void* content_data,
    size_t content_size
);

/**
 * @brief Submit content to compete for GW access
 *
 * WHAT: Module submits content to compete for global broadcast
 * WHY: Multiple modules compete for limited conscious access
 * HOW: Adds content to competition pool with priority
 *
 * @param bridge Bridge instance
 * @param module_id ID of submitting module
 * @param content Content to submit for competition
 * @param priority Priority weight for competition [0-1]
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: Corresponds to thalamo-cortical competition where
 *                   multiple neural coalitions vie for global ignition.
 */
int gw_cognitive_compete_for_access(
    gw_cognitive_bridge_t* bridge,
    uint32_t module_id,
    const gw_cognitive_content_t* content,
    float priority
);

/**
 * @brief Register module to receive broadcasts
 *
 * WHAT: Registers a cognitive module as a broadcast receiver
 * WHY: Modules need to receive GW broadcasts to access conscious content
 * HOW: Adds callback to receiver registry
 *
 * @param bridge Bridge instance
 * @param module_id Unique ID for the registering module
 * @param callback Function to call on broadcast
 * @param user_data User context passed to callback
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: Corresponds to cortical areas that participate in
 *                   the global workspace ignition network.
 */
int gw_cognitive_register_receiver(
    gw_cognitive_bridge_t* bridge,
    uint32_t module_id,
    gw_cognitive_receiver_callback_t callback,
    void* user_data
);

/**
 * @brief Unregister module from receiving broadcasts
 *
 * WHAT: Removes a module from broadcast receiver list
 * WHY: Modules may need to stop receiving broadcasts
 * HOW: Removes callback from receiver registry
 *
 * @param bridge Bridge instance
 * @param module_id ID of module to unregister
 * @return 0 on success, -1 on error
 */
int gw_cognitive_unregister_receiver(
    gw_cognitive_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Get current conscious content from GW
 *
 * WHAT: Retrieves content currently in the Global Workspace
 * WHY: Enables querying what is currently "in awareness"
 * HOW: Copies current GW content to output buffer
 *
 * @param bridge Bridge instance
 * @param content_out Output buffer for conscious content
 * @return 0 on success, -1 on error (no content or buffer too small)
 */
int gw_cognitive_get_conscious_content(
    gw_cognitive_bridge_t* bridge,
    gw_cognitive_conscious_content_t* content_out
);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves performance and activity metrics
 * WHY: Monitor GW dynamics and broadcast effectiveness
 * HOW: Copies current statistics to output buffer
 *
 * @param bridge Bridge instance
 * @param stats_out Output buffer for statistics
 * @return 0 on success, -1 on error
 */
int gw_cognitive_get_stats(
    const gw_cognitive_bridge_t* bridge,
    gw_cognitive_stats_t* stats_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GW_COGNITIVE_BRIDGE_H */
