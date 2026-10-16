//=============================================================================
// nimcp_pr_cognitive_bridge.h - Prime Resonant Cognitive Bridge
//=============================================================================
/**
 * @file nimcp_pr_cognitive_bridge.h
 * @brief Integration bridge between Prime Resonant memory and cognitive modules
 *
 * WHAT: Bidirectional bridge connecting PR memory system with cognitive modules
 *       (attention, emotion, executive) and the cognitive integration hub
 * WHY:  Memory is not isolated - it interacts with attention for salience,
 *       emotion for valence, and executive for encoding/retrieval control
 * HOW:  Provides connection points for cognitive modules to modulate memory
 *       parameters and receive memory events via the cognitive hub
 *
 * ARCHITECTURE:
 *
 *   Prime Resonant Cognitive Bridge:
 *   +-----------------------------------------------------------------------+
 *   |                        PR Cognitive Bridge                            |
 *   |                                                                        |
 *   |  +------------------+       +------------------+                       |
 *   |  | Attention Link   |<----->| Salience (quat.y)|                       |
 *   |  | - Focus weight   |       | modulation       |                       |
 *   |  +------------------+       +------------------+                       |
 *   |                                                                        |
 *   |  +------------------+       +------------------+                       |
 *   |  | Emotion Link     |<----->| Valence (quat.x) |                       |
 *   |  | - Arousal/valence|       | emotional boost  |                       |
 *   |  +------------------+       +------------------+                       |
 *   |                                                                        |
 *   |  +------------------+       +------------------+                       |
 *   |  | Executive Link   |<----->| Encoding/Retrieval|                      |
 *   |  | - Control signals|       | gating control    |                      |
 *   |  +------------------+       +------------------+                       |
 *   |                                                                        |
 *   |  +------------------+       +------------------+                       |
 *   |  | Working Memory   |<----->| Z0 Tier          |                       |
 *   |  | - Slot sync      |       | working items    |                       |
 *   |  +------------------+       +------------------+                       |
 *   |                                                                        |
 *   |  +------------------+       +------------------+                       |
 *   |  | Cognitive Hub    |<----->| Memory Events    |                       |
 *   |  | - Event routing  |       | broadcast        |                       |
 *   |  +------------------+       +------------------+                       |
 *   +-----------------------------------------------------------------------+
 *
 *   Memory-Cognitive Interaction Flow:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  ENCODING PATH:                                                        |
 *   |  [Executive Control] ---> [Attention Gate] ---> [PR Node Create]      |
 *   |                                   |                    |               |
 *   |                          [Emotion Tag]                 v               |
 *   |                                   +-----------> [Z-Ladder Insert]      |
 *   |                                                                        |
 *   |  RETRIEVAL PATH:                                                       |
 *   |  [Query] ---> [PR Resonance Match] ---> [Executive Filter]            |
 *   |                      |                         |                       |
 *   |             [Attention Boost]                  v                       |
 *   |                      +----------------> [Return Memory]                |
 *   |                                                                        |
 *   |  MODULATION:                                                           |
 *   |  - Attention focus (high) --> Increased salience (quat.y)             |
 *   |  - Emotional arousal (high) --> Increased consolidation (quat.w)      |
 *   |  - Executive inhibition --> Gate encoding/retrieval                   |
 *   +-----------------------------------------------------------------------+
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex: Executive control over memory encoding/retrieval
 * - Amygdala: Emotional modulation of memory strength
 * - Parietal attention: Salience-based memory prioritization
 * - Hippocampus: Working memory integration with long-term storage
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe via internal mutex
 * - Callbacks are invoked with mutex released
 *
 * INTEGRATION:
 * - nimcp_pr_memory_node.h: Memory nodes with quaternion state
 * - nimcp_z_ladder.h: Tiered memory storage
 * - nimcp_cognitive_integration_hub.h: Event routing
 * - nimcp_attention_substrate_bridge.h: Attention system
 * - nimcp_emotion_substrate_bridge.h: Emotion system
 * - nimcp_executive_substrate_bridge.h: Executive control
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_COGNITIVE_BRIDGE_H
#define NIMCP_PR_COGNITIVE_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Core dependencies
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_z_ladder.h"
#include "cognitive/memory/core/nimcp_quaternion.h"

// Cognitive module dependencies
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module ID for cognitive hub registration */
#define PR_COG_BRIDGE_MODULE_ID         0x5000

/** Default attention modulation strength */
#define PR_COG_ATTENTION_STRENGTH       0.8f

/** Default emotion modulation strength */
#define PR_COG_EMOTION_STRENGTH         0.7f

/** Default executive gating threshold */
#define PR_COG_EXECUTIVE_THRESHOLD      0.5f

/** Maximum working memory sync slots */
#define PR_COG_MAX_WM_SLOTS             16

/** Default update interval (ms) */
#define PR_COG_DEFAULT_UPDATE_MS        100

/** Minimum salience for attention boost */
#define PR_COG_MIN_SALIENCE             0.1f

/** Maximum salience cap */
#define PR_COG_MAX_SALIENCE             1.0f

/** Emotional arousal threshold for memory boost */
#define PR_COG_EMOTION_AROUSAL_THRESHOLD    0.3f

/** Executive inhibition threshold */
#define PR_COG_INHIBITION_THRESHOLD     0.7f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Memory event types for cognitive hub broadcasts
 */
typedef enum {
    PR_MEM_EVENT_ENCODED = 0,       /**< New memory encoded */
    PR_MEM_EVENT_RETRIEVED,         /**< Memory retrieved */
    PR_MEM_EVENT_CONSOLIDATED,      /**< Memory promoted to higher tier */
    PR_MEM_EVENT_FORGOTTEN,         /**< Memory evicted/decayed */
    PR_MEM_EVENT_UPDATED,           /**< Memory content updated */
    PR_MEM_EVENT_WM_SYNC,           /**< Working memory synchronized */
    PR_MEM_EVENT_COUNT              /**< Number of event types */
} pr_memory_event_type_t;

/**
 * @brief Connection state for cognitive links
 */
typedef enum {
    PR_COG_LINK_DISCONNECTED = 0,   /**< Not connected */
    PR_COG_LINK_CONNECTED,          /**< Connected and active */
    PR_COG_LINK_SUSPENDED           /**< Connected but temporarily disabled */
} pr_cognitive_link_state_t;

/**
 * @brief Error codes for cognitive bridge operations
 */
typedef enum {
    PR_COG_SUCCESS = 0,                     /**< Operation succeeded */
    PR_COG_ERROR_NULL_POINTER = -1,         /**< NULL pointer argument */
    PR_COG_ERROR_NOT_INITIALIZED = -2,      /**< Bridge not initialized */
    PR_COG_ERROR_ALREADY_CONNECTED = -3,    /**< Link already connected */
    PR_COG_ERROR_NOT_CONNECTED = -4,        /**< Link not connected */
    PR_COG_ERROR_HUB_FAILED = -5,           /**< Cognitive hub operation failed */
    PR_COG_ERROR_INVALID_CONFIG = -6,       /**< Invalid configuration */
    PR_COG_ERROR_NO_MEMORY = -7,            /**< Memory allocation failed */
    PR_COG_ERROR_SYNC_FAILED = -8,          /**< Working memory sync failed */
    PR_COG_ERROR_GATE_BLOCKED = -9,         /**< Operation blocked by executive gate */
    PR_COG_ERROR_CAPACITY = -10             /**< Capacity limit reached */
} pr_cognitive_error_t;

/**
 * @brief Memory event payload for cognitive hub broadcasts
 */
typedef struct {
    uint64_t node_id;                       /**< Memory node identifier */
    pr_memory_event_type_t event_type;      /**< Type of memory event */
    pr_memory_tier_t tier;                  /**< Current/affected tier */
    nimcp_quaternion_t state;               /**< Quaternion state at event time */
    float strength;                         /**< Memory strength at event time */
    uint64_t timestamp_ms;                  /**< Event timestamp */
} pr_memory_event_payload_t;

/**
 * @brief Attention link state
 *
 * Tracks connection to attention system for salience modulation
 */
typedef struct {
    pr_cognitive_link_state_t state;        /**< Connection state */
    float focus_weight;                     /**< Current attention focus [0-1] */
    float salience_gain;                    /**< Salience modulation gain */
    float filter_threshold;                 /**< Attention filter threshold */
    uint64_t last_update_ms;                /**< Last update timestamp */
    void* attention_context;                /**< Attention system context */
} pr_attention_link_t;

/**
 * @brief Emotion link state
 *
 * Tracks connection to emotion system for valence modulation
 */
typedef struct {
    pr_cognitive_link_state_t state;        /**< Connection state */
    float current_valence;                  /**< Current emotional valence [-1,+1] */
    float current_arousal;                  /**< Current emotional arousal [0-1] */
    float valence_sensitivity;              /**< Valence modulation sensitivity */
    float arousal_threshold;                /**< Arousal threshold for boost */
    uint64_t last_update_ms;                /**< Last update timestamp */
    void* emotion_context;                  /**< Emotion system context */
} pr_emotion_link_t;

/**
 * @brief Executive link state
 *
 * Tracks connection to executive system for encoding/retrieval control
 */
typedef struct {
    pr_cognitive_link_state_t state;        /**< Connection state */
    float encoding_gate;                    /**< Encoding gate level [0-1] */
    float retrieval_gate;                   /**< Retrieval gate level [0-1] */
    float inhibition_level;                 /**< Current inhibition [0-1] */
    bool encoding_permitted;                /**< Whether encoding is allowed */
    bool retrieval_permitted;               /**< Whether retrieval is allowed */
    uint64_t last_update_ms;                /**< Last update timestamp */
    void* executive_context;                /**< Executive system context */
} pr_executive_link_t;

/**
 * @brief Working memory slot state
 *
 * Tracks synchronization between WM module and Z0 tier
 */
typedef struct {
    uint64_t node_id;                       /**< PR node ID (0 if empty) */
    uint32_t wm_slot_index;                 /**< WM module slot index */
    float activity_level;                   /**< Current activity [0-1] */
    float salience;                         /**< Slot salience */
    bool is_synced;                         /**< Whether in sync with WM */
    uint64_t last_sync_ms;                  /**< Last sync timestamp */
} pr_wm_slot_t;

/**
 * @brief Working memory link state
 *
 * Tracks synchronization with working memory module
 */
typedef struct {
    pr_cognitive_link_state_t state;        /**< Connection state */
    pr_wm_slot_t slots[PR_COG_MAX_WM_SLOTS]; /**< Slot states */
    uint32_t active_slots;                  /**< Number of active slots */
    uint32_t max_slots;                     /**< Maximum slots configured */
    float total_activity;                   /**< Total WM activity */
    uint64_t last_sync_ms;                  /**< Last sync timestamp */
    void* wm_context;                       /**< Working memory context */
} pr_wm_link_t;

/**
 * @brief Cognitive hub link state
 *
 * Tracks connection to cognitive integration hub
 */
typedef struct {
    pr_cognitive_link_state_t state;        /**< Connection state */
    cognitive_integration_hub_t hub;        /**< Hub handle */
    uint32_t module_id;                     /**< Registered module ID */
    uint64_t events_published;              /**< Events published count */
    uint64_t events_received;               /**< Events received count */
    bool subscribed_attention;              /**< Subscribed to attention events */
    bool subscribed_emotion;                /**< Subscribed to emotion events */
    bool subscribed_memory;                 /**< Subscribed to memory events */
} pr_hub_link_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    // Operation counts
    uint64_t encodings_modulated;           /**< Encodings affected by attention */
    uint64_t retrievals_modulated;          /**< Retrievals affected by attention */
    uint64_t emotional_boosts;              /**< Emotional memory boosts */
    uint64_t executive_blocks;              /**< Operations blocked by executive */
    uint64_t wm_syncs;                      /**< Working memory synchronizations */
    uint64_t events_broadcast;              /**< Events broadcast to hub */

    // Modulation averages
    float avg_salience_modulation;          /**< Average salience change */
    float avg_valence_modulation;           /**< Average valence change */
    float avg_executive_gate;               /**< Average gate level */

    // Timing
    uint64_t total_update_time_us;          /**< Total update time (microseconds) */
    uint64_t update_count;                  /**< Number of updates */
    uint64_t last_update_ms;                /**< Last update timestamp */
} pr_cognitive_stats_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    // Attention configuration
    bool enable_attention_link;             /**< Enable attention modulation */
    float attention_strength;               /**< Attention modulation strength */
    float salience_decay_rate;              /**< Salience decay per second */

    // Emotion configuration
    bool enable_emotion_link;               /**< Enable emotion modulation */
    float emotion_strength;                 /**< Emotion modulation strength */
    float arousal_boost_factor;             /**< Arousal-based boost factor */

    // Executive configuration
    bool enable_executive_link;             /**< Enable executive control */
    float encoding_threshold;               /**< Gate threshold for encoding */
    float retrieval_threshold;              /**< Gate threshold for retrieval */

    // Working memory configuration
    bool enable_wm_sync;                    /**< Enable WM synchronization */
    uint32_t max_wm_slots;                  /**< Maximum WM slots to sync */
    uint32_t wm_sync_interval_ms;           /**< WM sync interval */

    // Hub configuration
    bool enable_hub_events;                 /**< Enable hub event broadcast */
    bool subscribe_attention_events;        /**< Subscribe to attention events */
    bool subscribe_emotion_events;          /**< Subscribe to emotion events */

    // General
    uint32_t update_interval_ms;            /**< Update interval */
} pr_cognitive_config_t;

/**
 * @brief Opaque cognitive bridge handle
 */
typedef struct pr_cognitive_bridge_struct* pr_cognitive_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/**
 * @brief Callback for attention modulation events
 *
 * @param bridge Bridge instance
 * @param node_id Affected memory node
 * @param old_salience Previous salience value
 * @param new_salience New salience value
 * @param user_data User context
 */
typedef void (*pr_attention_modulate_cb_t)(
    pr_cognitive_bridge_t bridge,
    uint64_t node_id,
    float old_salience,
    float new_salience,
    void* user_data
);

/**
 * @brief Callback for emotion modulation events
 *
 * @param bridge Bridge instance
 * @param node_id Affected memory node
 * @param valence Emotional valence applied
 * @param arousal Emotional arousal level
 * @param user_data User context
 */
typedef void (*pr_emotion_modulate_cb_t)(
    pr_cognitive_bridge_t bridge,
    uint64_t node_id,
    float valence,
    float arousal,
    void* user_data
);

/**
 * @brief Callback for executive gate events
 *
 * @param bridge Bridge instance
 * @param is_encoding True if encoding operation, false if retrieval
 * @param permitted True if operation was permitted
 * @param gate_level Current gate level
 * @param user_data User context
 */
typedef void (*pr_executive_gate_cb_t)(
    pr_cognitive_bridge_t bridge,
    bool is_encoding,
    bool permitted,
    float gate_level,
    void* user_data
);

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default cognitive bridge configuration
 *
 * @return Default configuration with sensible values
 */
NIMCP_EXPORT pr_cognitive_config_t pr_cognitive_bridge_default_config(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool pr_cognitive_bridge_validate_config(
    const pr_cognitive_config_t* config
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create cognitive bridge
 *
 * WHAT: Creates bridge for memory-cognitive integration
 * WHY:  Central coordinator for all cognitive modulation of memory
 * HOW:  Allocates bridge, initializes links, prepares for connections
 *
 * @param config Configuration (NULL for defaults)
 * @param z_ladder Z-Ladder memory system to integrate
 * @return Bridge handle or NULL on failure
 *
 * Performance: O(1)
 * Thread safety: Thread-safe
 */
NIMCP_EXPORT pr_cognitive_bridge_t pr_cognitive_bridge_create(
    const pr_cognitive_config_t* config,
    z_ladder_t z_ladder
);

/**
 * @brief Destroy cognitive bridge
 *
 * WHAT: Releases bridge and all resources
 * WHY:  Clean shutdown
 * HOW:  Disconnects all links, unregisters from hub, frees memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Performance: O(active_links)
 * Thread safety: Thread-safe
 */
NIMCP_EXPORT void pr_cognitive_bridge_destroy(pr_cognitive_bridge_t bridge);

/**
 * @brief Reset bridge to initial state
 *
 * Disconnects all links but keeps configuration.
 *
 * @param bridge Bridge handle
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_reset(
    pr_cognitive_bridge_t bridge
);

//=============================================================================
// Attention Integration
//=============================================================================

/**
 * @brief Connect attention system to bridge
 *
 * WHAT: Establishes attention-memory link for salience modulation
 * WHY:  Attention focus should boost memory salience (quat.y)
 * HOW:  Stores attention context, enables salience modulation
 *
 * @param bridge Cognitive bridge
 * @param attention_context Attention system context (can be NULL)
 * @return PR_COG_SUCCESS or error code
 *
 * Effect: Memory nodes under attention get increased salience
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_connect_attention(
    pr_cognitive_bridge_t bridge,
    void* attention_context
);

/**
 * @brief Disconnect attention system
 *
 * @param bridge Cognitive bridge
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_disconnect_attention(
    pr_cognitive_bridge_t bridge
);

/**
 * @brief Update attention state
 *
 * WHAT: Updates attention parameters from attention system
 * WHY:  Keep memory salience in sync with attention focus
 * HOW:  Reads attention state, modulates affected memory nodes
 *
 * @param bridge Cognitive bridge
 * @param focus_weight Current attention focus weight [0-1]
 * @param filter_threshold Current attention filter threshold [0-1]
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_update_attention(
    pr_cognitive_bridge_t bridge,
    float focus_weight,
    float filter_threshold
);

/**
 * @brief Apply attention boost to specific memory node
 *
 * WHAT: Increases salience of a memory node based on attention
 * WHY:  Attended memories should be more salient
 * HOW:  Modulates quat.y based on attention focus
 *
 * @param bridge Cognitive bridge
 * @param node_id Memory node to boost
 * @param attention_weight How much attention (0-1)
 * @return New salience value, or -1 on error
 */
NIMCP_EXPORT float pr_cognitive_bridge_apply_attention_boost(
    pr_cognitive_bridge_t bridge,
    uint64_t node_id,
    float attention_weight
);

/**
 * @brief Get attention link state
 *
 * @param bridge Cognitive bridge
 * @param link_state Output: attention link state
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_get_attention_state(
    pr_cognitive_bridge_t bridge,
    pr_attention_link_t* link_state
);

//=============================================================================
// Emotion Integration
//=============================================================================

/**
 * @brief Connect emotion system to bridge
 *
 * WHAT: Establishes emotion-memory link for valence modulation
 * WHY:  Emotional arousal enhances memory encoding, valence tags memories
 * HOW:  Stores emotion context, enables valence/arousal modulation
 *
 * @param bridge Cognitive bridge
 * @param emotion_context Emotion system context (can be NULL)
 * @return PR_COG_SUCCESS or error code
 *
 * Effect: Emotional memories get valence tags and consolidation boost
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_connect_emotion(
    pr_cognitive_bridge_t bridge,
    void* emotion_context
);

/**
 * @brief Disconnect emotion system
 *
 * @param bridge Cognitive bridge
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_disconnect_emotion(
    pr_cognitive_bridge_t bridge
);

/**
 * @brief Update emotion state
 *
 * WHAT: Updates emotion parameters from emotion system
 * WHY:  Emotional state affects memory encoding strength
 * HOW:  Reads emotion state, applies to ongoing encoding
 *
 * @param bridge Cognitive bridge
 * @param valence Current emotional valence [-1, +1]
 * @param arousal Current emotional arousal [0-1]
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_update_emotion(
    pr_cognitive_bridge_t bridge,
    float valence,
    float arousal
);

/**
 * @brief Apply emotional tag to memory node
 *
 * WHAT: Tags memory with emotional valence and applies arousal boost
 * WHY:  Emotional memories are better remembered
 * HOW:  Sets quat.x (valence), boosts quat.w (consolidation) if high arousal
 *
 * @param bridge Cognitive bridge
 * @param node_id Memory node to tag
 * @param valence Emotional valence [-1, +1]
 * @param arousal Emotional arousal [0-1]
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_apply_emotion_tag(
    pr_cognitive_bridge_t bridge,
    uint64_t node_id,
    float valence,
    float arousal
);

/**
 * @brief Get emotion link state
 *
 * @param bridge Cognitive bridge
 * @param link_state Output: emotion link state
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_get_emotion_state(
    pr_cognitive_bridge_t bridge,
    pr_emotion_link_t* link_state
);

//=============================================================================
// Executive Integration
//=============================================================================

/**
 * @brief Connect executive system to bridge
 *
 * WHAT: Establishes executive-memory link for encoding/retrieval control
 * WHY:  Executive control gates what gets encoded and retrieved
 * HOW:  Stores executive context, enables gating control
 *
 * @param bridge Cognitive bridge
 * @param executive_context Executive system context (can be NULL)
 * @return PR_COG_SUCCESS or error code
 *
 * Effect: Executive can inhibit encoding/retrieval operations
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_connect_executive(
    pr_cognitive_bridge_t bridge,
    void* executive_context
);

/**
 * @brief Disconnect executive system
 *
 * @param bridge Cognitive bridge
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_disconnect_executive(
    pr_cognitive_bridge_t bridge
);

/**
 * @brief Update executive control state
 *
 * WHAT: Updates executive gating parameters
 * WHY:  Executive control varies with cognitive load and goals
 * HOW:  Sets gate levels, determines what operations are permitted
 *
 * @param bridge Cognitive bridge
 * @param encoding_gate Encoding gate level [0-1] (0=blocked, 1=open)
 * @param retrieval_gate Retrieval gate level [0-1]
 * @param inhibition Inhibition level [0-1]
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_update_executive(
    pr_cognitive_bridge_t bridge,
    float encoding_gate,
    float retrieval_gate,
    float inhibition
);

/**
 * @brief Check if encoding is permitted
 *
 * WHAT: Queries executive gate for encoding permission
 * WHY:  Executive control can block encoding during high load
 * HOW:  Compares encoding gate level against threshold
 *
 * @param bridge Cognitive bridge
 * @return true if encoding permitted, false if blocked
 */
NIMCP_EXPORT bool pr_cognitive_bridge_encoding_permitted(
    pr_cognitive_bridge_t bridge
);

/**
 * @brief Check if retrieval is permitted
 *
 * WHAT: Queries executive gate for retrieval permission
 * WHY:  Executive control can block retrieval during focused tasks
 * HOW:  Compares retrieval gate level against threshold
 *
 * @param bridge Cognitive bridge
 * @return true if retrieval permitted, false if blocked
 */
NIMCP_EXPORT bool pr_cognitive_bridge_retrieval_permitted(
    pr_cognitive_bridge_t bridge
);

/**
 * @brief Get executive link state
 *
 * @param bridge Cognitive bridge
 * @param link_state Output: executive link state
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_get_executive_state(
    pr_cognitive_bridge_t bridge,
    pr_executive_link_t* link_state
);

//=============================================================================
// Working Memory Integration
//=============================================================================

/**
 * @brief Synchronize with working memory module
 *
 * WHAT: Syncs PR Z0 tier with working memory module slots
 * WHY:  Working memory and Z0 should reflect the same active items
 * HOW:  Compares WM slots with Z0 nodes, updates as needed
 *
 * @param bridge Cognitive bridge
 * @param wm_context Working memory module context
 * @return PR_COG_SUCCESS or error code
 *
 * Synchronization:
 * - WM slot filled -> Ensure corresponding PR node in Z0
 * - WM slot cleared -> Consider demotion of PR node
 * - Z0 node added -> Update corresponding WM slot
 * - Z0 node evicted -> Clear WM slot
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_sync_working_memory(
    pr_cognitive_bridge_t bridge,
    void* wm_context
);

/**
 * @brief Map WM slot to Z0 node
 *
 * @param bridge Cognitive bridge
 * @param wm_slot_index WM slot index
 * @param node_id PR node ID to associate
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_map_wm_slot(
    pr_cognitive_bridge_t bridge,
    uint32_t wm_slot_index,
    uint64_t node_id
);

/**
 * @brief Unmap WM slot
 *
 * @param bridge Cognitive bridge
 * @param wm_slot_index WM slot to unmap
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_unmap_wm_slot(
    pr_cognitive_bridge_t bridge,
    uint32_t wm_slot_index
);

/**
 * @brief Get WM link state
 *
 * @param bridge Cognitive bridge
 * @param link_state Output: WM link state
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_get_wm_state(
    pr_cognitive_bridge_t bridge,
    pr_wm_link_t* link_state
);

//=============================================================================
// Cognitive Hub Integration
//=============================================================================

/**
 * @brief Connect to cognitive integration hub
 *
 * WHAT: Registers bridge with cognitive hub for event routing
 * WHY:  Enables broadcasting memory events and receiving cognitive events
 * HOW:  Registers as COG_CATEGORY_MEMORY module, sets up subscriptions
 *
 * @param bridge Cognitive bridge
 * @param hub Cognitive integration hub
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_connect_hub(
    pr_cognitive_bridge_t bridge,
    cognitive_integration_hub_t hub
);

/**
 * @brief Disconnect from cognitive hub
 *
 * @param bridge Cognitive bridge
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_disconnect_hub(
    pr_cognitive_bridge_t bridge
);

/**
 * @brief Broadcast memory event to cognitive hub
 *
 * WHAT: Publishes memory event to cognitive hub for other modules
 * WHY:  Other cognitive modules may need to know about memory events
 * HOW:  Creates event payload, publishes via hub
 *
 * @param bridge Cognitive bridge
 * @param event_type Type of memory event
 * @param node_id Affected memory node ID
 * @param tier Current or affected tier
 * @return PR_COG_SUCCESS or error code
 *
 * Events broadcast:
 * - PR_MEM_EVENT_ENCODED: New memory created
 * - PR_MEM_EVENT_RETRIEVED: Memory accessed
 * - PR_MEM_EVENT_CONSOLIDATED: Memory promoted
 * - PR_MEM_EVENT_FORGOTTEN: Memory evicted
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_broadcast_memory_event(
    pr_cognitive_bridge_t bridge,
    pr_memory_event_type_t event_type,
    uint64_t node_id,
    pr_memory_tier_t tier
);

/**
 * @brief Get hub link state
 *
 * @param bridge Cognitive bridge
 * @param link_state Output: hub link state
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_get_hub_state(
    pr_cognitive_bridge_t bridge,
    pr_hub_link_t* link_state
);

//=============================================================================
// Update and Maintenance
//=============================================================================

/**
 * @brief Perform periodic update
 *
 * WHAT: Updates all active cognitive links
 * WHY:  Keeps modulation in sync with cognitive state
 * HOW:  Updates attention, emotion, executive, WM links as configured
 *
 * @param bridge Cognitive bridge
 * @return PR_COG_SUCCESS or error code
 *
 * Should be called periodically (e.g., every 100ms)
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_update(
    pr_cognitive_bridge_t bridge
);

/**
 * @brief Apply salience decay to all memories
 *
 * WHAT: Reduces salience of memories not under attention
 * WHY:  Unattended memories should lose salience over time
 * HOW:  Applies decay rate to quat.y of non-focused memories
 *
 * @param bridge Cognitive bridge
 * @param dt_seconds Time since last decay
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_apply_salience_decay(
    pr_cognitive_bridge_t bridge,
    float dt_seconds
);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register attention modulation callback
 *
 * @param bridge Cognitive bridge
 * @param callback Callback function
 * @param user_data User context
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_set_attention_callback(
    pr_cognitive_bridge_t bridge,
    pr_attention_modulate_cb_t callback,
    void* user_data
);

/**
 * @brief Register emotion modulation callback
 *
 * @param bridge Cognitive bridge
 * @param callback Callback function
 * @param user_data User context
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_set_emotion_callback(
    pr_cognitive_bridge_t bridge,
    pr_emotion_modulate_cb_t callback,
    void* user_data
);

/**
 * @brief Register executive gate callback
 *
 * @param bridge Cognitive bridge
 * @param callback Callback function
 * @param user_data User context
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_set_executive_callback(
    pr_cognitive_bridge_t bridge,
    pr_executive_gate_cb_t callback,
    void* user_data
);

//=============================================================================
// Statistics and Queries
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Cognitive bridge
 * @param stats Output: statistics structure
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_get_stats(
    pr_cognitive_bridge_t bridge,
    pr_cognitive_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Cognitive bridge
 * @return PR_COG_SUCCESS or error code
 */
NIMCP_EXPORT pr_cognitive_error_t pr_cognitive_bridge_reset_stats(
    pr_cognitive_bridge_t bridge
);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Cognitive bridge
 * @return true if all enabled links are connected
 */
NIMCP_EXPORT bool pr_cognitive_bridge_is_connected(
    pr_cognitive_bridge_t bridge
);

/**
 * @brief Get error string
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_cognitive_error_string(pr_cognitive_error_t error);

/**
 * @brief Get memory event type string
 *
 * @param event_type Memory event type
 * @return Human-readable event type string
 */
NIMCP_EXPORT const char* pr_memory_event_type_string(
    pr_memory_event_type_t event_type
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_COGNITIVE_BRIDGE_H
