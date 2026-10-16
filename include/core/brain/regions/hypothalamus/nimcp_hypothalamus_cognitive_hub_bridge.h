/**
 * @file nimcp_hypothalamus_cognitive_hub_bridge.h
 * @brief Hypothalamus - Cognitive Integration Hub Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridge connecting the hypothalamus orchestrator to the cognitive integration hub,
 *       enabling bidirectional communication between drive states and cognitive processing.
 *
 * WHY: The hypothalamus (Byrnes' "Steering Subsystem") must coordinate with all cognitive
 *      modules to influence behavior through drives. This bridge enables:
 *      - Drive state broadcasting to cognitive modules (hunger affects attention)
 *      - Receiving cognitive state updates that affect drives (learning affects curiosity)
 *      - Coordinating homeostatic regulation with cognitive processing
 *      - Stress/fatigue propagation to modulate cognitive capacity
 *
 * HOW: Registers hypothalamus as COG_CATEGORY_EXECUTIVE module (drives steer all behavior),
 *      subscribes to relevant cognitive events, and publishes drive state changes. The bridge
 *      translates between hypo_event_data_t and cognitive_event_data_t formats.
 *
 * BIOLOGICAL BASIS:
 * - Hypothalamus-cortex connections: Drive states influence all cognitive processing
 * - Prefrontal-hypothalamus feedback: Executive control modulates drive expression
 * - Stress axis (HPA): Cortisol affects memory, attention, decision-making
 * - Energy regulation: Glucose availability affects cognitive capacity
 * - Circadian influence: Alertness affects all cognitive functions
 *
 * BYRNES' ALIGNMENT CONNECTION:
 * The steering subsystem (hypothalamus) shapes the learning subsystem (cortex) through
 * reward signals. This bridge ensures drives properly influence cognitive processing
 * while cognitive states provide feedback to the drive system.
 *
 * INTEGRATION PATTERNS:
 * - Subscribe: COG_EVENT_EMOTION_UPDATE for emotional feedback on drives
 * - Subscribe: COG_EVENT_ATTENTION_SHIFT for attention allocation info
 * - Subscribe: COG_EVENT_DECISION_MADE for goal completion feedback
 * - Subscribe: COG_EVENT_LEARNING_COMPLETE for curiosity/competence drives
 * - Publish: COG_EVENT_STATE_CHANGE for drive state changes
 * - Publish: COG_EVENT_DECISION_MADE for drive prioritization decisions
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPOTHALAMUS_COGNITIVE_HUB_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_COGNITIVE_HUB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum drive events to buffer */
#define HYPO_COG_HUB_MAX_EVENT_BUFFER       128

/** Default module ID for hypothalamus in the cognitive hub */
#define HYPO_COG_HUB_DEFAULT_MODULE_ID      0x4859504F  /* "HYPO" */

/** Maximum subscribed event types */
#define HYPO_COG_HUB_MAX_SUBSCRIPTIONS      16

/** Default drive update interval in milliseconds */
#define HYPO_COG_HUB_DEFAULT_UPDATE_INTERVAL_MS   100.0f

/** Default cognitive weight for feedback modulation */
#define HYPO_COG_HUB_DEFAULT_COGNITIVE_WEIGHT     0.3f

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

/** Opaque bridge handle */
typedef struct hypo_cognitive_hub_bridge hypo_cognitive_hub_bridge_t;

/** External type declarations */
struct hypo_orchestrator_struct;
typedef struct hypo_orchestrator_struct* hypo_orchestrator_t;

struct cognitive_integration_hub_struct;
typedef struct cognitive_integration_hub_struct* cognitive_integration_hub_t;

struct bio_router_struct;
typedef struct bio_router_struct* bio_router_t;

struct bio_module_context_struct;
typedef struct bio_module_context_struct* bio_module_context_t;

/* ============================================================================
 * DRIVE-COGNITIVE EVENT TYPES
 * ============================================================================ */

/**
 * @brief Hypothalamus-specific event types for cognitive hub communication
 *
 * WHAT: Events specific to drive-cognitive interactions
 * WHY:  Inform cognitive modules about drive state changes
 * HOW:  Published through cognitive hub with specific payloads
 */
typedef enum {
    HYPO_COG_EVENT_DRIVE_STATE_CHANGED = 0,   /**< Drive state changed */
    HYPO_COG_EVENT_DRIVE_URGENT,              /**< Urgent drive requires attention */
    HYPO_COG_EVENT_DRIVE_SATISFIED,           /**< Drive was satisfied */
    HYPO_COG_EVENT_DRIVE_CONFLICT,            /**< Multiple drives in conflict */
    HYPO_COG_EVENT_STRESS_CHANGED,            /**< Stress level changed */
    HYPO_COG_EVENT_FATIGUE_CHANGED,           /**< Fatigue level changed */
    HYPO_COG_EVENT_AROUSAL_CHANGED,           /**< Arousal level changed */
    HYPO_COG_EVENT_HOMEOSTATIC_ALERT,         /**< Homeostatic deviation */
    HYPO_COG_EVENT_COUNT
} hypo_cog_hub_event_type_t;

/* ============================================================================
 * CONFIGURATION STRUCTURES
 * ============================================================================ */

/**
 * @brief Configuration for hypothalamus-cognitive hub bridge
 *
 * WHAT: Configuration parameters for bridge behavior
 * WHY:  Customize how drives integrate with cognitive processing
 * HOW:  Struct with flags and parameters for bridge operation
 */
typedef struct {
    /** Module registration */
    uint32_t module_id;                  /**< Module ID for hub registration */

    /** Drive broadcasting */
    bool enable_drive_broadcast;         /**< Broadcast drive states to cognitive hub */
    float drive_update_interval_ms;      /**< How often to broadcast drives (0 = on change) */
    bool broadcast_on_change;            /**< Broadcast immediately on drive change */
    bool broadcast_urgency_only;         /**< Only broadcast urgent drive changes */

    /** Cognitive feedback */
    bool enable_cognitive_feedback;      /**< Receive cognitive state updates */
    float cognitive_weight;              /**< Weight of cognitive input on drives [0,1] */
    bool auto_subscribe_emotion;         /**< Subscribe to EMOTION_UPDATE */
    bool auto_subscribe_attention;       /**< Subscribe to ATTENTION_SHIFT */
    bool auto_subscribe_decision;        /**< Subscribe to DECISION_MADE */
    bool auto_subscribe_learning;        /**< Subscribe to LEARNING_COMPLETE */

    /** Stress/fatigue propagation */
    bool enable_stress_propagation;      /**< Propagate stress to cognitive modules */
    bool enable_fatigue_modulation;      /**< Modulate cognitive capacity by fatigue */
    float stress_propagation_threshold;  /**< Min stress level to propagate [0,1] */
    float fatigue_threshold;             /**< Min fatigue to affect cognition [0,1] */

    /** Query handling */
    bool enable_query_handler;           /**< Register query handler */

    /** Event buffering */
    uint32_t event_buffer_size;          /**< Size of internal event buffer */
} hypo_cognitive_hub_config_t;

/* ============================================================================
 * STATE STRUCTURES
 * ============================================================================ */

/**
 * @brief Cognitive state received from hub modules
 *
 * WHAT: Aggregated cognitive state information
 * WHY:  Used to modulate drives based on cognitive processing
 * HOW:  Updated from cognitive hub events
 */
typedef struct {
    float cognitive_load;            /**< Current cognitive load [0,1] */
    float attention_demand;          /**< Attention being demanded [0,1] */
    float emotional_arousal;         /**< Emotional state from hub [0,1] */
    float executive_control;         /**< Executive control level [0,1] */
    float memory_consolidation;      /**< Memory consolidation pressure [0,1] */
    float social_engagement;         /**< Social processing level [0,1] */
    float learning_activity;         /**< Current learning intensity [0,1] */
    uint64_t last_update_timestamp;  /**< When state was last updated */
} hypo_cognitive_hub_state_t;

/**
 * @brief Drive state broadcast payload
 *
 * WHAT: Payload for drive state broadcasts to cognitive modules
 * WHY:  Allow cognitive modules to adapt to current drive state
 * HOW:  Sent as cognitive event payload
 */
typedef struct {
    /** Primary drive information */
    uint32_t primary_drive_type;     /**< Currently dominant drive */
    float primary_drive_level;       /**< Level of dominant drive [0,1] */
    uint32_t urgency_level;          /**< Overall urgency (hypo_urgency_t) */

    /** Global state */
    float unified_drive_level;       /**< Combined drive level [0,1] */
    float arousal_level;             /**< Current arousal [0,1] */
    float stress_level;              /**< Current stress [0,1] */
    float fatigue_level;             /**< Current fatigue [0,1] */

    /** Active drives summary */
    uint32_t active_drive_count;     /**< Number of active drives */
    bool in_conflict;                /**< Multiple drives competing */
    bool in_stress_response;         /**< Stress response active */

    /** Timestamp */
    uint64_t timestamp;              /**< Event timestamp */
} hypo_drive_broadcast_payload_t;

/**
 * @brief Stress propagation payload
 *
 * WHAT: Payload for stress propagation to cognitive modules
 * WHY:  Allow cognitive modules to adapt to stress state
 * HOW:  Sent as cognitive event when stress changes significantly
 */
typedef struct {
    float stress_level;              /**< Current stress level [0,1] */
    float cortisol_level;            /**< Simulated cortisol [0,1] */
    uint32_t stressor_type;          /**< Type of stressor */
    bool is_acute;                   /**< Acute vs chronic stress */
    float cognitive_capacity_mod;    /**< Suggested cognitive capacity modifier */
    float attention_bias;            /**< Attention bias toward threat */
    float memory_encoding_boost;     /**< Emotional memory encoding boost */
    uint64_t timestamp;              /**< Event timestamp */
} hypo_stress_propagation_payload_t;

/**
 * @brief Fatigue modulation payload
 *
 * WHAT: Payload for fatigue effects on cognitive processing
 * WHY:  Allow cognitive modules to adapt to fatigue state
 * HOW:  Sent as cognitive event when fatigue changes
 */
typedef struct {
    float fatigue_level;             /**< Current fatigue [0,1] */
    float alertness;                 /**< Current alertness [0,1] */
    float sleep_pressure;            /**< Sleep pressure [0,1] */
    float cognitive_capacity_mod;    /**< Suggested capacity modifier [0,1] */
    float attention_capacity_mod;    /**< Attention capacity modifier [0,1] */
    float processing_speed_mod;      /**< Processing speed modifier [0,1] */
    uint64_t timestamp;              /**< Event timestamp */
} hypo_fatigue_modulation_payload_t;

/* ============================================================================
 * STATISTICS STRUCTURES
 * ============================================================================ */

/**
 * @brief Statistics for hypothalamus-cognitive hub bridge
 */
typedef struct {
    /** Event counts */
    uint32_t events_received;            /**< Total events received from hub */
    uint32_t events_published;           /**< Total events published to hub */
    uint32_t drive_broadcasts;           /**< Drive state broadcasts sent */
    uint32_t stress_propagations;        /**< Stress propagation events */
    uint32_t fatigue_modulations;        /**< Fatigue modulation events */

    /** Feedback processing */
    uint32_t emotion_updates_received;   /**< Emotion updates received */
    uint32_t attention_updates_received; /**< Attention updates received */
    uint32_t decision_updates_received;  /**< Decision updates received */
    uint32_t learning_updates_received;  /**< Learning updates received */

    /** Query handling */
    uint32_t queries_handled;            /**< Total queries handled */

    /** Drive modulations */
    uint32_t curiosity_modulations;      /**< Curiosity drive modulations */
    uint32_t social_modulations;         /**< Social drive modulations */
    uint32_t competence_modulations;     /**< Competence drive modulations */

    /** Timing */
    uint64_t last_broadcast_timestamp;   /**< Last drive broadcast time */
    uint64_t last_receive_timestamp;     /**< Last event receive time */
    float avg_broadcast_latency_us;      /**< Average broadcast latency */
} hypo_cognitive_hub_stats_t;

/* ============================================================================
 * CALLBACK TYPES
 * ============================================================================ */

/**
 * @brief Callback for emotional state updates from cognitive hub
 *
 * @param emotional_valence Emotional valence [-1, 1]
 * @param emotional_arousal Emotional arousal [0, 1]
 * @param emotion_type Specific emotion type
 * @param user_data User context
 */
typedef void (*hypo_cog_emotion_callback_t)(
    float emotional_valence,
    float emotional_arousal,
    uint32_t emotion_type,
    void* user_data
);

/**
 * @brief Callback for attention state updates
 *
 * @param attention_focus_id ID of attention focus
 * @param attention_level Attention intensity [0, 1]
 * @param user_data User context
 */
typedef void (*hypo_cog_attention_callback_t)(
    uint64_t attention_focus_id,
    float attention_level,
    void* user_data
);

/**
 * @brief Callback for decision/goal completion updates
 *
 * @param decision_id Decision identifier
 * @param success Whether decision succeeded
 * @param reward_signal Associated reward
 * @param user_data User context
 */
typedef void (*hypo_cog_decision_callback_t)(
    uint64_t decision_id,
    bool success,
    float reward_signal,
    void* user_data
);

/**
 * @brief Callback for learning completion updates
 *
 * @param learning_id Learning episode identifier
 * @param skill_acquisition Skill improvement [0, 1]
 * @param information_gain Information gained
 * @param user_data User context
 */
typedef void (*hypo_cog_learning_callback_t)(
    uint64_t learning_id,
    float skill_acquisition,
    float information_gain,
    void* user_data
);

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default configuration for hypothalamus-cognitive hub bridge
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with standard settings
 * HOW:  Set reasonable defaults for all parameters
 *
 * DEFAULT VALUES:
 * - module_id: HYPO_COG_HUB_DEFAULT_MODULE_ID
 * - enable_drive_broadcast: true
 * - drive_update_interval_ms: 100.0
 * - broadcast_on_change: true
 * - broadcast_urgency_only: false
 * - enable_cognitive_feedback: true
 * - cognitive_weight: 0.3
 * - auto_subscribe_emotion: true
 * - auto_subscribe_attention: true
 * - auto_subscribe_decision: true
 * - auto_subscribe_learning: true
 * - enable_stress_propagation: true
 * - enable_fatigue_modulation: true
 * - stress_propagation_threshold: 0.5
 * - fatigue_threshold: 0.6
 * - enable_query_handler: true
 * - event_buffer_size: HYPO_COG_HUB_MAX_EVENT_BUFFER
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_default_config(hypo_cognitive_hub_config_t* config);

/**
 * @brief Create hypothalamus-cognitive hub bridge
 *
 * WHAT: Initialize bridge for hypothalamus-hub integration
 * WHY:  Enable drive states to coordinate with cognitive processing
 * HOW:  Allocate bridge, initialize state, prepare for connection
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 *
 * MEMORY: Caller must call hypo_cognitive_hub_destroy() when done
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
hypo_cognitive_hub_bridge_t* hypo_cognitive_hub_create(
    const hypo_cognitive_hub_config_t* config
);

/**
 * @brief Destroy hypothalamus-cognitive hub bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect from hub and orchestrator, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * NOTES: Automatically disconnects if connected
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
void hypo_cognitive_hub_destroy(hypo_cognitive_hub_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Reset bridge state without destroying
 * WHY:  Clear state for fresh start while maintaining connections
 * HOW:  Reset statistics and cognitive state, preserve connections
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_reset(hypo_cognitive_hub_bridge_t* bridge);

/* ============================================================================
 * CONNECTION API
 * ============================================================================ */

/**
 * @brief Connect bridge to hypothalamus orchestrator and cognitive hub
 *
 * WHAT: Establish bidirectional connection between hypothalamus and cognitive hub
 * WHY:  Enable drive-cognitive integration
 * HOW:  Register with hub, subscribe to events, connect to orchestrator
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param orch Hypothalamus orchestrator
 * @param hub Cognitive integration hub
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if bridge, orch, or hub is NULL
 * - Returns -1 if already connected
 * - Returns -1 if hub registration fails
 *
 * SIDE EFFECTS:
 * - Registers hypothalamus as COG_CATEGORY_EXECUTIVE module
 * - Subscribes to configured cognitive event types
 * - Registers with hypothalamus orchestrator for drive events
 * - Registers query handler if enabled
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_connect(
    hypo_cognitive_hub_bridge_t* bridge,
    hypo_orchestrator_t orch,
    cognitive_integration_hub_t hub
);

/**
 * @brief Disconnect bridge from cognitive hub and orchestrator
 *
 * WHAT: Unregister from hub and disconnect from orchestrator
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unsubscribe from events, unregister module
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_disconnect(hypo_cognitive_hub_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @return true if connected to both hub and orchestrator
 */
bool hypo_cognitive_hub_is_connected(const hypo_cognitive_hub_bridge_t* bridge);

/* ============================================================================
 * UPDATE API
 * ============================================================================ */

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update for drive broadcasting and state maintenance
 * WHY:  Enable time-based drive broadcasts and state synchronization
 * HOW:  Check elapsed time, broadcast drives if interval exceeded
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param delta_ms Time elapsed since last update
 * @return 0 on success, -1 on error
 *
 * NOTES: Call this regularly from main loop or timer
 *
 * COMPLEXITY: O(1) average, O(subscribers) on broadcast
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_update(
    hypo_cognitive_hub_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Broadcast current drive states to cognitive hub
 *
 * WHAT: Immediately broadcast drive states to all cognitive modules
 * WHY:  Allow immediate notification of drive state changes
 * HOW:  Get drive state from orchestrator, publish to hub
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_broadcast_drives(hypo_cognitive_hub_bridge_t* bridge);

/**
 * @brief Receive and process cognitive state updates
 *
 * WHAT: Process pending cognitive state updates from hub
 * WHY:  Apply cognitive feedback to drive modulation
 * HOW:  Read cognitive state, apply to drive system
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_receive_cognitive_state(hypo_cognitive_hub_bridge_t* bridge);

/* ============================================================================
 * DRIVE MODULATION API
 * ============================================================================ */

/**
 * @brief Modulate curiosity drive based on cognitive information gain
 *
 * WHAT: Adjust curiosity drive based on learning/information gain
 * WHY:  Curiosity should be satisfied by information acquisition
 * HOW:  Apply info_gain as satisfaction signal to curiosity drive
 *
 * BIOLOGICAL BASIS:
 * - Information gain activates dopaminergic reward circuits
 * - Curiosity drive decreases when information needs are met
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param info_gain Information gain amount [0, 1]
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_modulate_curiosity(
    hypo_cognitive_hub_bridge_t* bridge,
    float info_gain
);

/**
 * @brief Modulate social drive based on social interaction reward
 *
 * WHAT: Adjust social drive based on social engagement quality
 * WHY:  Social drive should respond to social interaction outcomes
 * HOW:  Apply social_reward as satisfaction signal to social drive
 *
 * BIOLOGICAL BASIS:
 * - Social interaction releases oxytocin (paraventricular nucleus)
 * - Positive social outcomes satisfy social connection needs
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param social_reward Social reward signal [0, 1]
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_modulate_social(
    hypo_cognitive_hub_bridge_t* bridge,
    float social_reward
);

/**
 * @brief Modulate competence drive based on skill acquisition
 *
 * WHAT: Adjust competence/mastery drive based on skill improvement
 * WHY:  Competence drive should respond to mastery achievement
 * HOW:  Apply skill_acquisition as satisfaction signal
 *
 * BIOLOGICAL BASIS:
 * - Mastery/competence activates intrinsic reward circuits
 * - Skill acquisition satisfies self-determination needs
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param skill_acquisition Skill improvement amount [0, 1]
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_modulate_competence(
    hypo_cognitive_hub_bridge_t* bridge,
    float skill_acquisition
);

/* ============================================================================
 * STRESS/FATIGUE PROPAGATION API
 * ============================================================================ */

/**
 * @brief Propagate stress state to cognitive modules
 *
 * WHAT: Broadcast stress level changes to cognitive modules
 * WHY:  Stress affects cognitive capacity and processing
 * HOW:  Publish stress event with modulation parameters
 *
 * BIOLOGICAL BASIS:
 * - HPA axis activation affects prefrontal cortex function
 * - Cortisol impairs working memory, enhances threat detection
 * - Chronic stress impairs hippocampal function
 *
 * COGNITIVE EFFECTS:
 * - Reduced working memory capacity
 * - Increased attention to threat-related stimuli
 * - Enhanced emotional memory encoding
 * - Impaired cognitive flexibility
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param stress_level Current stress level [0, 1]
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_propagate_stress(
    hypo_cognitive_hub_bridge_t* bridge,
    float stress_level
);

/**
 * @brief Propagate fatigue state to cognitive modules
 *
 * WHAT: Broadcast fatigue level changes to cognitive modules
 * WHY:  Fatigue affects cognitive capacity and performance
 * HOW:  Publish fatigue event with capacity modulation
 *
 * BIOLOGICAL BASIS:
 * - Sleep pressure accumulates with wakefulness (adenosine)
 * - Fatigue impairs executive function and attention
 * - Circadian phase affects alertness and performance
 *
 * COGNITIVE EFFECTS:
 * - Reduced processing speed
 * - Impaired attention and vigilance
 * - Reduced executive control
 * - Increased errors and lapses
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param fatigue_level Current fatigue level [0, 1]
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_propagate_fatigue(
    hypo_cognitive_hub_bridge_t* bridge,
    float fatigue_level
);

/* ============================================================================
 * CALLBACK REGISTRATION API
 * ============================================================================ */

/**
 * @brief Set callback for emotional state updates
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param callback Emotion callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int hypo_cognitive_hub_set_emotion_callback(
    hypo_cognitive_hub_bridge_t* bridge,
    hypo_cog_emotion_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for attention state updates
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param callback Attention callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int hypo_cognitive_hub_set_attention_callback(
    hypo_cognitive_hub_bridge_t* bridge,
    hypo_cog_attention_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for decision completion updates
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param callback Decision callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int hypo_cognitive_hub_set_decision_callback(
    hypo_cognitive_hub_bridge_t* bridge,
    hypo_cog_decision_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for learning completion updates
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param callback Learning callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int hypo_cognitive_hub_set_learning_callback(
    hypo_cognitive_hub_bridge_t* bridge,
    hypo_cog_learning_callback_t callback,
    void* user_data
);

/* ============================================================================
 * BIO-ASYNC INTEGRATION API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Enable bio-async message routing for drive coordination
 * WHY:  Allow hypothalamus to participate in bio-async messaging
 * HOW:  Register as bio-async module, set up handlers
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param router Bio-async router (NULL for global router)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_connect_bio_async(
    hypo_cognitive_hub_bridge_t* bridge,
    bio_router_t router
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @return 0 on success, -1 on error
 */
int hypo_cognitive_hub_disconnect_bio_async(hypo_cognitive_hub_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @return true if bio-async is connected
 */
bool hypo_cognitive_hub_bio_async_connected(
    const hypo_cognitive_hub_bridge_t* bridge
);

/* ============================================================================
 * STATE QUERY API
 * ============================================================================ */

/**
 * @brief Get current cognitive state received from hub
 *
 * WHAT: Get aggregated cognitive state
 * WHY:  Allow inspection of cognitive state affecting drives
 * HOW:  Copy current cognitive state to output
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param state Output cognitive state
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_get_cognitive_state(
    const hypo_cognitive_hub_bridge_t* bridge,
    hypo_cognitive_hub_state_t* state
);

/**
 * @brief Get bridge configuration
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_cognitive_hub_get_config(
    const hypo_cognitive_hub_bridge_t* bridge,
    hypo_cognitive_hub_config_t* config
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve operational statistics
 * WHY:  Monitor bridge performance and activity
 * HOW:  Copy current stats to output struct
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_get_stats(
    const hypo_cognitive_hub_bridge_t* bridge,
    hypo_cognitive_hub_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Start fresh measurement period
 * HOW:  Zero all stat counters
 *
 * @param bridge Hypothalamus-cognitive hub bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int hypo_cognitive_hub_reset_stats(hypo_cognitive_hub_bridge_t* bridge);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get string name for hypothalamus-cognitive event type
 *
 * @param event_type Event type
 * @return String name (never NULL)
 */
const char* hypo_cog_hub_event_name(hypo_cog_hub_event_type_t event_type);

/**
 * @brief Print bridge state summary to stdout
 *
 * @param bridge Bridge to print (NULL safe)
 */
void hypo_cognitive_hub_print_summary(const hypo_cognitive_hub_bridge_t* bridge);

/**
 * @brief Print bridge statistics to stdout
 *
 * @param stats Statistics to print (NULL safe)
 */
void hypo_cognitive_hub_print_stats(const hypo_cognitive_hub_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_COGNITIVE_HUB_BRIDGE_H */
