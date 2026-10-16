/**
 * @file nimcp_mirror_empathy_bridge.h
 * @brief Mirror Neurons - Empathetic Response Cognitive Hub Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bridge connecting mirror neurons to empathetic response via the cognitive hub
 * WHY:  Enables mirror neuron action understanding and emotional resonance to drive
 *       empathetic responses, creating a unified social cognition pipeline.
 * HOW:  Registers as COG_CATEGORY_SOCIAL module, subscribes to social/state events,
 *       publishes mirrored actions, emotional resonance, and empathetic responses.
 *
 * BIOLOGICAL BASIS:
 * - Mirror neuron system (premotor and parietal cortex) enables action understanding
 * - Anterior insula and cingulate cortex process emotional resonance
 * - Empathetic responses integrate action understanding with emotional simulation
 * - Right temporoparietal junction supports perspective-taking
 *
 * INTEGRATION PATTERNS:
 * - Subscribe: COG_EVENT_SOCIAL_SIGNAL for observed actions
 * - Subscribe: COG_EVENT_STATE_CHANGE for emotional resonance
 * - Subscribe: COG_EVENT_INPUT_RECEIVED for social stimuli
 * - Publish: COG_EVENT_OUTPUT_READY when empathetic response generated
 * - Publish: COG_EVENT_PREDICTION for intention prediction
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MIRROR_EMPATHY_BRIDGE_H
#define NIMCP_MIRROR_EMPATHY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum events to buffer */
#define MIRROR_EMPATHY_MAX_EVENT_BUFFER     64

/** Default module ID for mirror-empathy bridge in the hub */
#define MIRROR_EMPATHY_DEFAULT_MODULE_ID    0x4D49454D  /* "MIEM" (Mirror-Empathy) */

/** Maximum subscribed event types */
#define MIRROR_EMPATHY_MAX_SUBSCRIPTIONS    16

/** Maximum agents to track for empathetic responses */
#define MIRROR_EMPATHY_MAX_AGENTS           32

/* ============================================================================
 * Mirror-Empathy Event Types
 * ============================================================================ */

/**
 * @brief Event types specific to mirror-empathy integration
 *
 * WHAT: Events specific to mirror neuron and empathetic response processing
 * WHY:  Inform other modules about social understanding and empathy lifecycle
 * HOW:  Published through cognitive hub with specific payloads
 */
typedef enum {
    MIRROR_EMPATHY_EVENT_ACTION_MIRRORED = 0,   /**< Observed action understood */
    MIRROR_EMPATHY_EVENT_EMOTION_RESONATED,     /**< Emotional state mirrored */
    MIRROR_EMPATHY_EVENT_EMPATHY_GENERATED,     /**< Empathetic response produced */
    MIRROR_EMPATHY_EVENT_INTENTION_PREDICTED,   /**< Agent intention inferred */
    MIRROR_EMPATHY_EVENT_SOCIAL_UNDERSTOOD,     /**< Social context understood */
    MIRROR_EMPATHY_EVENT_COMPASSION_ACTIVATED,  /**< Compassionate response triggered */
    MIRROR_EMPATHY_EVENT_BOND_STRENGTHENED,     /**< Social bond reinforced */
    MIRROR_EMPATHY_EVENT_COUNT
} mirror_empathy_event_type_t;

/**
 * @brief Action types that mirror neurons can understand
 */
typedef enum {
    MIRROR_ACTION_GRASP = 0,        /**< Grasping/reaching actions */
    MIRROR_ACTION_FACIAL,           /**< Facial expressions */
    MIRROR_ACTION_GESTURE,          /**< Communicative gestures */
    MIRROR_ACTION_POSTURAL,         /**< Body posture changes */
    MIRROR_ACTION_VOCAL,            /**< Vocal/speech actions */
    MIRROR_ACTION_COUNT
} mirror_action_type_t;

/**
 * @brief Emotion categories for resonance processing
 */
typedef enum {
    MIRROR_EMOTION_JOY = 0,         /**< Happiness/joy */
    MIRROR_EMOTION_SADNESS,         /**< Sadness/grief */
    MIRROR_EMOTION_FEAR,            /**< Fear/anxiety */
    MIRROR_EMOTION_ANGER,           /**< Anger/frustration */
    MIRROR_EMOTION_SURPRISE,        /**< Surprise/startle */
    MIRROR_EMOTION_DISGUST,         /**< Disgust/rejection */
    MIRROR_EMOTION_NEUTRAL,         /**< Neutral affect */
    MIRROR_EMOTION_COUNT
} mirror_emotion_type_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mirror_empathy_bridge mirror_empathy_bridge_t;

/* Forward declare external types */
struct cognitive_integration_hub_struct;
typedef struct cognitive_integration_hub_struct* cognitive_integration_hub_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Configuration for mirror-empathy bridge
 *
 * WHAT: Parameters controlling mirror-empathy integration behavior
 * WHY:  Different scenarios require different weightings
 * HOW:  Configure weights and processing options
 */
typedef struct {
    uint32_t module_id;                      /**< Module ID for hub registration */
    bool enable_logging;                     /**< Enable debug logging */
    float action_understanding_weight;       /**< Weight for action understanding [0-1] */
    float emotional_resonance_weight;        /**< Weight for emotional resonance [0-1] */
    float empathy_generation_weight;         /**< Weight for empathy generation [0-1] */
    bool auto_subscribe_social;              /**< Subscribe to SOCIAL_SIGNAL events */
    bool auto_subscribe_state;               /**< Subscribe to STATE_CHANGE events */
    bool auto_subscribe_input;               /**< Subscribe to INPUT_RECEIVED events */
    bool publish_predictions;                /**< Publish intention predictions */
    uint32_t event_buffer_size;              /**< Size of internal event buffer */
    float empathy_threshold;                 /**< Min threshold to generate response [0-1] */
    uint32_t agent_capacity;                 /**< Max agents to track */
} mirror_empathy_config_t;

/**
 * @brief Statistics for mirror-empathy bridge operations
 */
typedef struct {
    uint64_t total_events;                   /**< Total events processed */
    uint64_t actions_mirrored;               /**< Actions successfully mirrored */
    uint64_t emotions_resonated;             /**< Emotions resonated with */
    uint64_t empathetic_responses;           /**< Empathetic responses generated */
    uint64_t intentions_predicted;           /**< Intentions predicted */
    uint64_t social_insights;                /**< Social understanding events */
    uint64_t events_published;               /**< Events published to hub */
    uint64_t events_received;                /**< Events received from hub */
    float avg_empathy_intensity;             /**< Average empathy intensity [0-1] */
    float avg_resonance_strength;            /**< Average emotional resonance [0-1] */
    uint64_t last_event_timestamp;           /**< Timestamp of last event */
} mirror_empathy_stats_t;

/**
 * @brief Mirrored action data structure
 */
typedef struct {
    uint32_t agent_id;                       /**< Observed agent identifier */
    mirror_action_type_t action_type;        /**< Type of action observed */
    float understanding_confidence;           /**< Confidence in understanding [0-1] */
    float goal_inference_confidence;          /**< Confidence in goal inference [0-1] */
    uint64_t timestamp;                      /**< Event timestamp */
    char action_description[128];            /**< Human-readable description */
} mirror_empathy_action_t;

/**
 * @brief Emotional resonance data structure
 */
typedef struct {
    uint32_t agent_id;                       /**< Agent whose emotion is mirrored */
    mirror_emotion_type_t emotion_type;      /**< Type of emotion resonated */
    float valence;                           /**< Emotional valence [-1, +1] */
    float arousal;                           /**< Emotional arousal [0, 1] */
    float resonance_strength;                /**< Strength of resonance [0-1] */
    uint64_t timestamp;                      /**< Event timestamp */
} mirror_empathy_resonance_t;

/**
 * @brief Empathetic response context
 */
typedef struct {
    uint32_t target_agent_id;                /**< Agent targeted for empathy */
    mirror_emotion_type_t perceived_emotion; /**< Agent's perceived emotion */
    float empathy_intensity;                 /**< Intensity of empathetic response */
    float compassion_level;                  /**< Level of compassionate feeling */
    bool helping_motivation;                 /**< Whether motivated to help */
    char response_suggestion[256];           /**< Suggested response text */
    uint64_t timestamp;                      /**< Event timestamp */
} mirror_empathy_response_t;

/**
 * @brief Intention prediction data
 */
typedef struct {
    uint32_t agent_id;                       /**< Agent whose intention is predicted */
    uint32_t predicted_goal;                 /**< Predicted goal/intention ID */
    float confidence;                        /**< Prediction confidence [0-1] */
    float time_to_action_ms;                 /**< Estimated time to action */
    char intention_description[128];         /**< Human-readable intention */
    uint64_t timestamp;                      /**< Event timestamp */
} mirror_empathy_intention_t;

/**
 * @brief Social understanding insight
 */
typedef struct {
    uint32_t agent_id;                       /**< Agent being understood */
    float rapport_level;                     /**< Level of social rapport [0-1] */
    float trust_level;                       /**< Trust assessment [0-1] */
    float familiarity;                       /**< How familiar the agent is [0-1] */
    bool cooperation_likely;                 /**< Whether cooperation expected */
    uint64_t timestamp;                      /**< Event timestamp */
} mirror_empathy_social_t;

/* ============================================================================
 * Callback Type Definitions
 * ============================================================================ */

/**
 * @brief Callback for mirrored action events
 */
typedef void (*mirror_empathy_action_callback_t)(
    const mirror_empathy_action_t* action,
    void* user_data
);

/**
 * @brief Callback for emotional resonance events
 */
typedef void (*mirror_empathy_resonance_callback_t)(
    const mirror_empathy_resonance_t* resonance,
    void* user_data
);

/**
 * @brief Callback for empathetic response events
 */
typedef void (*mirror_empathy_response_callback_t)(
    const mirror_empathy_response_t* response,
    void* user_data
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default mirror-empathy bridge configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with standard settings
 * HOW:  Set default weights and options
 *
 * DEFAULT VALUES:
 * - module_id: MIRROR_EMPATHY_DEFAULT_MODULE_ID
 * - enable_logging: false
 * - action_understanding_weight: 0.7
 * - emotional_resonance_weight: 0.8
 * - empathy_generation_weight: 0.75
 * - auto_subscribe_social: true
 * - auto_subscribe_state: true
 * - auto_subscribe_input: true
 * - publish_predictions: true
 * - event_buffer_size: MIRROR_EMPATHY_MAX_EVENT_BUFFER
 * - empathy_threshold: 0.3
 * - agent_capacity: MIRROR_EMPATHY_MAX_AGENTS
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int mirror_empathy_bridge_default_config(mirror_empathy_config_t* config);

/**
 * @brief Create mirror-empathy bridge
 *
 * WHAT: Initialize bridge for mirror neuron-empathy integration
 * WHY:  Enable mirror neurons to drive empathetic responses via hub
 * HOW:  Allocate bridge, initialize state, prepare event handling
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
mirror_empathy_bridge_t* mirror_empathy_bridge_create(
    const mirror_empathy_config_t* config
);

/**
 * @brief Destroy mirror-empathy bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect from hub, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * NOTES: Automatically disconnects from hub if connected
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void mirror_empathy_bridge_destroy(mirror_empathy_bridge_t* bridge);

/* ============================================================================
 * Hub Registration API
 * ============================================================================ */

/**
 * @brief Register bridge with cognitive hub
 *
 * WHAT: Connect bridge to cognitive integration hub
 * WHY:  Enable event subscription/publication for mirror-empathy integration
 * HOW:  Register module, subscribe to configured events, set up handlers
 *
 * @param bridge Mirror-empathy bridge
 * @param hub Cognitive integration hub
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if bridge or hub is NULL
 * - Returns -1 if already registered
 * - Returns -1 if hub registration fails
 *
 * SIDE EFFECTS:
 * - Registers as COG_CATEGORY_SOCIAL module
 * - Subscribes to configured event types
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
int mirror_empathy_bridge_register_with_hub(
    mirror_empathy_bridge_t* bridge,
    cognitive_integration_hub_t hub
);

/**
 * @brief Unregister bridge from cognitive hub
 *
 * WHAT: Disconnect bridge from hub and cleanup subscriptions
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unsubscribe from events, unregister module
 *
 * @param bridge Mirror-empathy bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
int mirror_empathy_bridge_unregister_from_hub(mirror_empathy_bridge_t* bridge);

/**
 * @brief Check if bridge is registered with hub
 *
 * @param bridge Mirror-empathy bridge
 * @return true if registered, false otherwise
 */
bool mirror_empathy_bridge_is_registered(mirror_empathy_bridge_t* bridge);

/* ============================================================================
 * Event Publication API
 * ============================================================================ */

/**
 * @brief Publish mirrored action understanding
 *
 * WHAT: Share observed action understanding through hub
 * WHY:  Allow other modules to react to action understanding
 * HOW:  Create event with action data, publish via hub
 *
 * @param bridge Mirror-empathy bridge
 * @param action Mirrored action data
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: Mirror neurons fire both when performing and
 * observing an action, enabling action understanding.
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int mirror_empathy_publish_mirrored_action(
    mirror_empathy_bridge_t* bridge,
    const mirror_empathy_action_t* action
);

/**
 * @brief Publish emotional resonance state
 *
 * WHAT: Share mirrored emotional state through hub
 * WHY:  Allow emotional contagion to propagate
 * HOW:  Create event with resonance data, publish via hub
 *
 * @param bridge Mirror-empathy bridge
 * @param resonance Emotional resonance data
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: Anterior insula activation during emotional
 * observation creates shared affective states.
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int mirror_empathy_publish_emotional_resonance(
    mirror_empathy_bridge_t* bridge,
    const mirror_empathy_resonance_t* resonance
);

/**
 * @brief Request empathetic response generation
 *
 * WHAT: Generate empathetic response based on context
 * WHY:  Produce appropriate social/emotional response
 * HOW:  Process context, generate response, publish result
 *
 * @param bridge Mirror-empathy bridge
 * @param target_agent_id Agent to empathize with
 * @param context_emotion Current emotional context
 * @param response_out Output: Generated empathetic response
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1) + O(subscribers) for publication
 * THREAD-SAFE: Yes
 */
int mirror_empathy_request_empathetic_response(
    mirror_empathy_bridge_t* bridge,
    uint32_t target_agent_id,
    mirror_emotion_type_t context_emotion,
    mirror_empathy_response_t* response_out
);

/**
 * @brief Notify about predicted intention
 *
 * WHAT: Share intention prediction through hub
 * WHY:  Allow predictive social responses
 * HOW:  Create prediction event, publish via hub
 *
 * @param bridge Mirror-empathy bridge
 * @param intention Intention prediction data
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int mirror_empathy_notify_action_intention(
    mirror_empathy_bridge_t* bridge,
    const mirror_empathy_intention_t* intention
);

/**
 * @brief Publish social understanding insight
 *
 * WHAT: Share social understanding through hub
 * WHY:  Enable coordinated social cognition
 * HOW:  Create social insight event, publish via hub
 *
 * @param bridge Mirror-empathy bridge
 * @param understanding Social understanding data
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int mirror_empathy_publish_social_understanding(
    mirror_empathy_bridge_t* bridge,
    const mirror_empathy_social_t* understanding
);

/* ============================================================================
 * Callback Registration API
 * ============================================================================ */

/**
 * @brief Set callback for mirrored action events
 *
 * @param bridge Mirror-empathy bridge
 * @param callback Action event callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int mirror_empathy_set_action_callback(
    mirror_empathy_bridge_t* bridge,
    mirror_empathy_action_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for emotional resonance events
 *
 * @param bridge Mirror-empathy bridge
 * @param callback Resonance event callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int mirror_empathy_set_resonance_callback(
    mirror_empathy_bridge_t* bridge,
    mirror_empathy_resonance_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for empathetic response events
 *
 * @param bridge Mirror-empathy bridge
 * @param callback Response event callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int mirror_empathy_set_response_callback(
    mirror_empathy_bridge_t* bridge,
    mirror_empathy_response_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current empathy state for an agent
 *
 * @param bridge Mirror-empathy bridge
 * @param agent_id Agent to query
 * @param empathy_level Output: Current empathy level [0-1]
 * @param last_emotion Output: Last resonated emotion
 * @return 0 on success, -1 on error
 */
int mirror_empathy_get_agent_state(
    const mirror_empathy_bridge_t* bridge,
    uint32_t agent_id,
    float* empathy_level,
    mirror_emotion_type_t* last_emotion
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve operational statistics
 * WHY:  Monitor bridge performance and activity
 * HOW:  Copy current stats to output struct
 *
 * @param bridge Mirror-empathy bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int mirror_empathy_bridge_get_stats(
    const mirror_empathy_bridge_t* bridge,
    mirror_empathy_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Start fresh measurement period
 * HOW:  Zero all stat counters
 *
 * @param bridge Mirror-empathy bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int mirror_empathy_bridge_reset_stats(mirror_empathy_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get string name for action type
 *
 * @param action_type Action type
 * @return String name or "UNKNOWN" if invalid
 */
const char* mirror_empathy_action_type_to_string(mirror_action_type_t action_type);

/**
 * @brief Get string name for emotion type
 *
 * @param emotion_type Emotion type
 * @return String name or "UNKNOWN" if invalid
 */
const char* mirror_empathy_emotion_type_to_string(mirror_emotion_type_t emotion_type);

/**
 * @brief Get string name for event type
 *
 * @param event_type Event type
 * @return String name or "UNKNOWN" if invalid
 */
const char* mirror_empathy_event_type_to_string(mirror_empathy_event_type_t event_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_EMPATHY_BRIDGE_H */
