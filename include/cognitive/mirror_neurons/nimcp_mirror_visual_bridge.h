/**
 * @file nimcp_mirror_visual_bridge.h
 * @brief Mirror Neurons - Visual Cortex Integration Bridge
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Bidirectional integration between mirror neuron system and visual cortex
 * WHY:  Mirror neurons require visual input for action observation; visual cortex
 *       benefits from social salience feedback to prioritize agent-related stimuli
 * HOW:  Visual cortex detects agents/biological motion -> mirror neurons; mirror
 *       neurons modulate visual attention for social stimuli via STS pathway
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SUPERIOR TEMPORAL SULCUS (STS) PATHWAY:
 * ----------------------------------------
 * The STS is the neural "hub" connecting visual and social processing:
 * 1. Biological Motion Detection (Grossman & Blake 2002):
 *    - STS responds selectively to biological motion (point-light walkers)
 *    - Inputs from V5/MT (motion) and V4 (form) converge in STS
 *    - Output to mirror neuron regions (IFG, IPL)
 *
 * 2. Agent Detection and Gaze Processing:
 *    - STS fusiform face area detects faces/agents
 *    - Eye gaze direction processed in posterior STS
 *    - Triggers observation mode in mirror neurons
 *
 * 3. Action Observation Network:
 *    - Visual input (V1/V2/V4/V5) -> STS -> Mirror neurons (F5/IPL)
 *    - Mirror neurons understand observed actions
 *    - Reference: Rizzolatti & Craighero (2004)
 *
 * VISUAL -> MIRROR NEURONS PATHWAYS:
 * -----------------------------------
 * 1. Agent Detection -> Observation Mode:
 *    - Visual cortex detects human/agent in scene
 *    - Triggers mirror neuron observation mode
 *    - Prepares action recognition circuits
 *
 * 2. Biological Motion -> Action Recognition:
 *    - V5/MT motion signals feed biological motion detector
 *    - STS extracts action-related motion patterns
 *    - Mirror neurons match observed to known actions
 *
 * 3. Face/Expression -> Empathy:
 *    - Fusiform face area (V4/temporal) detects faces
 *    - Expression analysis feeds emotional mirror neurons
 *    - Enables empathic resonance
 *
 * MIRROR NEURONS -> VISUAL CORTEX PATHWAYS:
 * ------------------------------------------
 * 1. Social Salience -> Attention Modulation:
 *    - Mirror neuron activity indicates social relevance
 *    - Feeds back to visual cortex to boost agent processing
 *    - Implements social attention bias
 *
 * 2. Action Prediction -> Visual Enhancement:
 *    - Mirror neurons predict agent's next action
 *    - Primes visual cortex for expected motion/position
 *    - Enables predictive visual processing
 *
 * 3. Empathic State -> Expression Attention:
 *    - Emotional resonance state boosts face attention
 *    - Enhances expression-related visual processing
 *    - Implements empathy-driven visual focus
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |               MIRROR-VISUAL BRIDGE (STS Pathway Model)                     |
 * +===========================================================================+
 * |                                                                            |
 * |   +-----------------------------------------------------------------+     |
 * |   |              VISUAL CORTEX -> MIRROR NEURONS                     |     |
 * |   |                                                                  |     |
 * |   |   V1/V2 Features -----> Agent Detection -----> Observation Mode  |     |
 * |   |   V5/MT Motion -------> Bio Motion ---------> Action Recognition |     |
 * |   |   V4/FFA Face --------> Expression ---------> Empathic Resonance |     |
 * |   +-----------------------------------------------------------------+     |
 * |                                                                            |
 * |   +-----------------------------------------------------------------+     |
 * |   |              MIRROR NEURONS -> VISUAL CORTEX                     |     |
 * |   |                                                                  |     |
 * |   |   Social Salience -----> Attention Boost -----> Agent ROI        |     |
 * |   |   Action Prediction ---> Motion Priming ------> V5/MT Gain       |     |
 * |   |   Empathic State ------> Face Attention ------> FFA Gain         |     |
 * |   +-----------------------------------------------------------------+     |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * BIO-ASYNC MODULE ID: 0x027B (MIRROR_VISUAL_BRIDGE)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MIRROR_VISUAL_BRIDGE_H
#define NIMCP_MIRROR_VISUAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Base infrastructure */
#include "utils/bridge/nimcp_bridge_base.h"

/* Module integrations */
#include "cognitive/nimcp_mirror_neurons.h"
#include "perception/nimcp_visual_cortex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for health agent (B22) */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Note: BIO_MODULE_MIRROR_VISUAL_BRIDGE (0x027B) is defined in nimcp_bio_messages.h */

/* Agent detection thresholds */
#define MIRROR_VISUAL_AGENT_CONFIDENCE_MIN     0.3f  /**< Minimum agent detection confidence */
#define MIRROR_VISUAL_AGENT_CONFIDENCE_HIGH    0.7f  /**< High confidence agent detection */

/* Biological motion thresholds */
#define MIRROR_VISUAL_BIO_MOTION_THRESHOLD     0.4f  /**< Minimum biological motion score */
#define MIRROR_VISUAL_ACTION_RECOGNITION_MIN   0.5f  /**< Minimum action recognition score */

/* Social salience modulation */
#define MIRROR_VISUAL_SALIENCE_BOOST_MIN       1.0f  /**< No attention boost */
#define MIRROR_VISUAL_SALIENCE_BOOST_MAX       2.0f  /**< Maximum attention boost */

/* Face/expression processing */
#define MIRROR_VISUAL_FACE_CONFIDENCE_MIN      0.4f  /**< Minimum face detection confidence */
#define MIRROR_VISUAL_EMPATHY_THRESHOLD        0.5f  /**< Empathy activation threshold */

/* STS pathway timing (milliseconds) */
#define MIRROR_VISUAL_STS_LATENCY_MS           50    /**< Typical STS processing latency */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mirror_visual_bridge mirror_visual_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for mirror-visual bridge
 *
 * WHAT: Configuration parameters for visual-mirror neuron integration
 * WHY:  Allow tuning of pathway sensitivities and feature enables
 * HOW:  Set thresholds, gains, and pathway enables
 */
typedef struct {
    /* Agent detection */
    float agent_detection_threshold;       /**< Min confidence to trigger observation mode */
    float agent_attention_boost;           /**< Attention boost for detected agents [1.0-2.0] */
    bool enable_agent_detection;           /**< Enable agent -> observation mode pathway */

    /* Biological motion */
    float bio_motion_threshold;            /**< Min biological motion score */
    float bio_motion_sensitivity;          /**< Sensitivity to biological motion patterns */
    bool enable_bio_motion;                /**< Enable biological motion -> action recognition */

    /* Face/expression processing */
    float face_detection_threshold;        /**< Min face detection confidence */
    float empathy_gain;                    /**< Empathic resonance gain factor */
    bool enable_face_processing;           /**< Enable face/expression -> empathy pathway */

    /* Social salience feedback */
    float salience_feedback_gain;          /**< Social salience -> attention modulation gain */
    float attention_modulation_rate;       /**< Rate of attention modulation changes */
    bool enable_salience_feedback;         /**< Enable mirror -> visual attention feedback */

    /* STS pathway modeling */
    float sts_integration_weight;          /**< Weight for STS multimodal integration */
    uint32_t sts_latency_ms;               /**< STS processing latency in ms */
    bool enable_sts_modeling;              /**< Enable detailed STS pathway modeling */

    /* Action prediction */
    float prediction_visual_gain;          /**< Predicted action -> visual priming gain */
    bool enable_action_prediction;         /**< Enable predictive visual enhancement */

} mirror_visual_config_t;

/**
 * @brief Agent detection result from visual cortex
 *
 * WHAT: Visual detection of agent/person in scene
 * WHY:  Triggers mirror neuron observation mode
 * HOW:  Extracted from V4/temporal visual features
 */
typedef struct {
    bool agent_detected;                   /**< Agent present in visual field */
    float confidence;                      /**< Detection confidence [0-1] */
    float position_x;                      /**< Agent X position (normalized 0-1) */
    float position_y;                      /**< Agent Y position (normalized 0-1) */
    float bounding_size;                   /**< Estimated size of agent region */
    uint64_t timestamp_us;                 /**< Detection timestamp */
} agent_detection_t;

/**
 * @brief Biological motion analysis result
 *
 * WHAT: Analysis of biological (animate) motion patterns
 * WHY:  Feeds action recognition in mirror neurons
 * HOW:  V5/MT motion vectors analyzed for biological signatures
 */
typedef struct {
    bool bio_motion_present;               /**< Biological motion detected */
    float bio_motion_score;                /**< Biological motion strength [0-1] */
    float motion_direction;                /**< Primary motion direction (radians) */
    float motion_speed;                    /**< Motion speed estimate */
    float coherence;                       /**< Motion coherence (1 = all same direction) */
    uint32_t detected_action_id;           /**< Recognized action ID (0 = unknown) */
    float action_confidence;               /**< Action recognition confidence */
} bio_motion_analysis_t;

/**
 * @brief Face/expression detection result
 *
 * WHAT: Face and emotional expression detection
 * WHY:  Feeds empathic resonance in mirror neurons
 * HOW:  Fusiform face area (FFA) processing output
 */
typedef struct {
    bool face_detected;                    /**< Face present in visual field */
    float face_confidence;                 /**< Face detection confidence [0-1] */
    float face_x;                          /**< Face center X (normalized 0-1) */
    float face_y;                          /**< Face center Y (normalized 0-1) */
    float expression_valence;              /**< Expression valence (-1=negative, +1=positive) */
    float expression_arousal;              /**< Expression arousal (0=calm, 1=excited) */
    float gaze_direction_x;                /**< Estimated gaze X direction */
    float gaze_direction_y;                /**< Estimated gaze Y direction */
    bool gaze_at_observer;                 /**< Direct gaze detected (eye contact) */
} face_expression_t;

/**
 * @brief Social salience signal from mirror neurons
 *
 * WHAT: Mirror neuron output indicating social relevance
 * WHY:  Used to modulate visual attention for social stimuli
 * HOW:  Combines observation activation, empathy, and action prediction
 */
typedef struct {
    float overall_salience;                /**< Combined social salience [0-1] */
    float observation_activity;            /**< Mirror neuron observation activation */
    float empathic_resonance;              /**< Current empathic state strength */
    float action_prediction_strength;      /**< Strength of action prediction */
    float attention_boost_factor;          /**< Computed attention boost [1.0-2.0] */
    float focus_x;                         /**< Suggested focus X (normalized 0-1) */
    float focus_y;                         /**< Suggested focus Y (normalized 0-1) */
} social_salience_t;

/**
 * @brief STS (Superior Temporal Sulcus) integration state
 *
 * WHAT: Integrated state of the STS multimodal hub
 * WHY:  Models biological STS that integrates visual and social signals
 * HOW:  Combines agent, motion, face, and mirror neuron signals
 */
typedef struct {
    float integration_level;               /**< Overall STS integration [0-1] */
    float visual_input_strength;           /**< Strength of visual inputs */
    float mirror_input_strength;           /**< Strength of mirror neuron inputs */
    float agent_representation;            /**< Agent model strength in STS */
    float action_understanding;            /**< Action comprehension level */
    float social_context;                  /**< Inferred social context strength */
} sts_state_t;

/**
 * @brief Mirror-visual bridge effects
 *
 * WHAT: Current effects of the bridge on both systems
 * WHY:  Track bidirectional modulation
 * HOW:  Updated during bridge_update()
 */
typedef struct {
    /* Visual -> Mirror effects */
    bool observation_mode_triggered;       /**< Agent detection triggered observation */
    float action_recognition_input;        /**< Bio motion input to action recognition */
    float empathy_input;                   /**< Face/expression input to empathy */

    /* Mirror -> Visual effects */
    float attention_boost_applied;         /**< Current attention boost factor */
    float motion_priming_gain;             /**< Visual motion processing gain */
    float face_attention_gain;             /**< Face region attention gain */

    /* STS integration */
    sts_state_t sts_state;                 /**< Current STS state */
} mirror_visual_effects_t;

/**
 * @brief Current state of mirror-visual interaction
 *
 * WHAT: Runtime state of the bridge
 * WHY:  Monitor integration health and activity
 * HOW:  Updated each processing cycle
 */
typedef struct {
    /* Visual input state */
    agent_detection_t last_agent_detection;     /**< Most recent agent detection */
    bio_motion_analysis_t last_bio_motion;      /**< Most recent bio motion analysis */
    face_expression_t last_face_detection;      /**< Most recent face detection */

    /* Mirror neuron state */
    social_salience_t current_salience;         /**< Current social salience output */
    float mirror_activation_level;              /**< Overall mirror neuron activation */

    /* Processing state */
    uint64_t last_visual_input_time;            /**< Last visual input timestamp */
    uint64_t last_mirror_output_time;           /**< Last mirror output timestamp */
    uint32_t frames_processed;                  /**< Visual frames processed */
    bool systems_synchronized;                  /**< Both systems in sync */
} mirror_visual_state_t;

/**
 * @brief Statistics for mirror-visual bridge
 *
 * WHAT: Cumulative statistics for bridge operation
 * WHY:  Monitor long-term behavior and performance
 * HOW:  Accumulated across all update cycles
 */
typedef struct {
    /* Visual -> Mirror events */
    uint64_t agent_detections;                  /**< Total agent detections */
    uint64_t observation_mode_triggers;         /**< Times observation mode triggered */
    uint64_t bio_motion_detections;             /**< Biological motion detections */
    uint64_t action_recognitions;               /**< Actions recognized from motion */
    uint64_t face_detections;                   /**< Face detections */
    uint64_t empathy_activations;               /**< Empathy pathway activations */

    /* Mirror -> Visual events */
    uint64_t attention_boosts;                  /**< Times attention boost applied */
    uint64_t motion_priming_events;             /**< Motion priming applications */
    uint64_t face_attention_events;             /**< Face attention boost events */

    /* Performance metrics */
    float avg_agent_confidence;                 /**< Average agent detection confidence */
    float avg_bio_motion_score;                 /**< Average biological motion score */
    float avg_salience_level;                   /**< Average social salience */
    float avg_attention_boost;                  /**< Average attention boost applied */
    float avg_processing_time_ms;               /**< Average bridge update time */

    /* STS metrics */
    float avg_sts_integration;                  /**< Average STS integration level */
    uint64_t total_updates;                     /**< Total bridge updates */
} mirror_visual_stats_t;

/**
 * @brief Mirror-visual bridge internal structure
 *
 * WHAT: Main bridge structure connecting mirror neurons and visual cortex
 * WHY:  Encapsulates all bridge state and connections
 * HOW:  Uses bridge_base_t for common infrastructure
 */
struct mirror_visual_bridge {
    bridge_base_t base;                    /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    mirror_visual_config_t config;

    /* Connected systems */
    mirror_neurons_t mirror_neurons;       /**< Mirror neuron system */
    visual_cortex_t* visual_cortex;        /**< Visual cortex system */

    /* Current effects */
    mirror_visual_effects_t effects;
    mirror_visual_state_t state;

    /* Statistics */
    mirror_visual_stats_t stats;

    /* Internal buffers */
    float* visual_features_buffer;         /**< Buffer for visual features */
    uint32_t visual_features_size;         /**< Size of features buffer */

    /* Instance-level health agent (B22) */
    nimcp_health_agent_t* health_agent;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default mirror-visual bridge configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all pathways
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int mirror_visual_bridge_default_config(mirror_visual_config_t* config);

/**
 * @brief Create mirror-visual bridge
 *
 * WHAT: Initialize mirror-visual integration bridge
 * WHY:  Enable bidirectional visual-mirror neuron interaction
 * HOW:  Allocate bridge, initialize state, prepare buffers
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
mirror_visual_bridge_t* mirror_visual_bridge_create(
    const mirror_visual_config_t* config
);

/**
 * @brief Destroy mirror-visual bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free buffers and memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void mirror_visual_bridge_destroy(mirror_visual_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without recreation
 * HOW:  Clear state/effects, preserve connections
 *
 * @param bridge Mirror-visual bridge
 * @return 0 on success, -1 on error
 */
int mirror_visual_bridge_reset(mirror_visual_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect mirror neuron system
 *
 * WHAT: Link bridge to mirror neuron system
 * WHY:  Enable mirror neuron state monitoring and modulation
 * HOW:  Store mirror neuron system handle
 *
 * @param bridge Mirror-visual bridge
 * @param mirror_neurons Mirror neuron system
 * @return 0 on success, -1 on error
 */
int mirror_visual_bridge_connect_mirror_neurons(
    mirror_visual_bridge_t* bridge,
    mirror_neurons_t mirror_neurons
);

/**
 * @brief Connect visual cortex
 *
 * WHAT: Link bridge to visual cortex
 * WHY:  Enable visual processing monitoring and modulation
 * HOW:  Store visual cortex pointer
 *
 * @param bridge Mirror-visual bridge
 * @param visual_cortex Visual cortex system
 * @return 0 on success, -1 on error
 */
int mirror_visual_bridge_connect_visual_cortex(
    mirror_visual_bridge_t* bridge,
    visual_cortex_t* visual_cortex
);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Mirror-visual bridge
 * @return true if both systems connected
 */
bool mirror_visual_bridge_is_connected(const mirror_visual_bridge_t* bridge);

/* ============================================================================
 * Visual -> Mirror Neurons Direction
 * ============================================================================ */

/**
 * @brief Process agent detection from visual cortex
 *
 * WHAT: Handle visual detection of agent/person
 * WHY:  Triggers mirror neuron observation mode
 * HOW:  Analyze visual features for agent presence, activate observation
 *
 * @param bridge Mirror-visual bridge
 * @param features Visual features from cortex
 * @param num_features Number of features
 * @param detection Output agent detection result
 * @return 0 on success, -1 on error
 */
int mirror_visual_process_agent_detection(
    mirror_visual_bridge_t* bridge,
    const float* features,
    uint32_t num_features,
    agent_detection_t* detection
);

/**
 * @brief Process biological motion from visual cortex
 *
 * WHAT: Analyze motion patterns for biological signatures
 * WHY:  Feeds action recognition in mirror neurons
 * HOW:  Extract biological motion from V5/MT signals
 *
 * @param bridge Mirror-visual bridge
 * @param motion_vectors Motion vector data from V5/MT
 * @param num_vectors Number of motion vectors
 * @param analysis Output biological motion analysis
 * @return 0 on success, -1 on error
 */
int mirror_visual_process_bio_motion(
    mirror_visual_bridge_t* bridge,
    const float* motion_vectors,
    uint32_t num_vectors,
    bio_motion_analysis_t* analysis
);

/**
 * @brief Process face/expression from visual cortex
 *
 * WHAT: Analyze face and expression for empathy
 * WHY:  Triggers empathic resonance in mirror neurons
 * HOW:  Extract face/expression from V4/FFA features
 *
 * @param bridge Mirror-visual bridge
 * @param features Visual features from face region
 * @param num_features Number of features
 * @param face Output face/expression result
 * @return 0 on success, -1 on error
 */
int mirror_visual_process_face(
    mirror_visual_bridge_t* bridge,
    const float* features,
    uint32_t num_features,
    face_expression_t* face
);

/**
 * @brief Trigger mirror neuron observation mode
 *
 * WHAT: Activate observation mode when agent detected
 * WHY:  Prepares mirror neurons for action recognition
 * HOW:  Call mirror_neurons_activate_observation_mode()
 *
 * @param bridge Mirror-visual bridge
 * @param detection Agent detection that triggered activation
 * @return 0 on success, -1 on error
 */
int mirror_visual_trigger_observation_mode(
    mirror_visual_bridge_t* bridge,
    const agent_detection_t* detection
);

/* ============================================================================
 * Mirror Neurons -> Visual Cortex Direction
 * ============================================================================ */

/**
 * @brief Get social salience from mirror neurons
 *
 * WHAT: Query current social relevance from mirror neuron system
 * WHY:  Used to modulate visual attention
 * HOW:  Combine observation activity, empathy, and prediction
 *
 * @param bridge Mirror-visual bridge
 * @param salience Output social salience signal
 * @return 0 on success, -1 on error
 */
int mirror_visual_get_social_salience(
    mirror_visual_bridge_t* bridge,
    social_salience_t* salience
);

/**
 * @brief Apply social salience to visual attention
 *
 * WHAT: Modulate visual cortex attention based on social salience
 * WHY:  Prioritize agent-related visual processing
 * HOW:  Boost attention in visual cortex for social regions
 *
 * @param bridge Mirror-visual bridge
 * @param salience Social salience signal to apply
 * @return 0 on success, -1 on error
 */
int mirror_visual_apply_attention_modulation(
    mirror_visual_bridge_t* bridge,
    const social_salience_t* salience
);

/**
 * @brief Apply action prediction to visual priming
 *
 * WHAT: Prime visual processing based on predicted action
 * WHY:  Enhance visual processing of predicted motion/position
 * HOW:  Modulate V5/MT gain for predicted motion patterns
 *
 * @param bridge Mirror-visual bridge
 * @param predicted_action_id Predicted action from mirror neurons
 * @param prediction_confidence Confidence in prediction
 * @return 0 on success, -1 on error
 */
int mirror_visual_apply_action_prediction(
    mirror_visual_bridge_t* bridge,
    uint32_t predicted_action_id,
    float prediction_confidence
);

/**
 * @brief Boost face attention based on empathic state
 *
 * WHAT: Enhance face region processing when empathy active
 * WHY:  Empathic resonance increases face attention
 * HOW:  Increase FFA region gain in visual cortex
 *
 * @param bridge Mirror-visual bridge
 * @param empathy_level Current empathic resonance level
 * @return 0 on success, -1 on error
 */
int mirror_visual_boost_face_attention(
    mirror_visual_bridge_t* bridge,
    float empathy_level
);

/* ============================================================================
 * STS Pathway Modeling
 * ============================================================================ */

/**
 * @brief Update STS integration state
 *
 * WHAT: Compute integrated STS state from all inputs
 * WHY:  Models biological STS multimodal integration
 * HOW:  Weighted combination of visual and mirror neuron signals
 *
 * @param bridge Mirror-visual bridge
 * @return 0 on success, -1 on error
 */
int mirror_visual_update_sts(mirror_visual_bridge_t* bridge);

/**
 * @brief Get current STS state
 *
 * @param bridge Mirror-visual bridge
 * @param sts_state Output STS state
 * @return 0 on success, -1 on error
 */
int mirror_visual_get_sts_state(
    const mirror_visual_bridge_t* bridge,
    sts_state_t* sts_state
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update mirror-visual bridge
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep mirror neurons and visual cortex synchronized
 * HOW:  Process visual input, update mirror state, apply feedback
 *
 * @param bridge Mirror-visual bridge
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success, -1 on error
 */
int mirror_visual_bridge_update(
    mirror_visual_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Mirror-visual bridge
 * @param state Output state structure
 * @return 0 on success, -1 on error
 */
int mirror_visual_bridge_get_state(
    const mirror_visual_bridge_t* bridge,
    mirror_visual_state_t* state
);

/**
 * @brief Get current bridge effects
 *
 * @param bridge Mirror-visual bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int mirror_visual_bridge_get_effects(
    const mirror_visual_bridge_t* bridge,
    mirror_visual_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Mirror-visual bridge
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int mirror_visual_bridge_get_stats(
    const mirror_visual_bridge_t* bridge,
    mirror_visual_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for mirror-visual coordination
 * WHY:  Distributed social cognition signaling
 * HOW:  Register module ID 0x027B, set up handlers
 *
 * @param bridge Mirror-visual bridge
 * @return 0 on success, -1 on error
 */
int mirror_visual_bridge_connect_bio_async(mirror_visual_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Mirror-visual bridge
 * @return 0 on success, -1 on error
 */
int mirror_visual_bridge_disconnect_bio_async(mirror_visual_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Mirror-visual bridge
 * @return true if bio-async enabled
 */
bool mirror_visual_bridge_is_bio_async_connected(
    const mirror_visual_bridge_t* bridge
);

/**
 * @brief Broadcast agent detection via bio-async
 *
 * WHAT: Notify other modules of agent detection
 * WHY:  Enable distributed social cognition processing
 * HOW:  Send BIO_MSG_AGENT_DETECTED message
 *
 * @param bridge Mirror-visual bridge
 * @param detection Agent detection to broadcast
 * @return 0 on success, -1 on error
 */
int mirror_visual_broadcast_agent_detection(
    mirror_visual_bridge_t* bridge,
    const agent_detection_t* detection
);

/**
 * @brief Broadcast social salience via bio-async
 *
 * WHAT: Notify other modules of social salience state
 * WHY:  Enable coordinated attention to social stimuli
 * HOW:  Send BIO_MSG_SOCIAL_SALIENCE message
 *
 * @param bridge Mirror-visual bridge
 * @param salience Social salience to broadcast
 * @return 0 on success, -1 on error
 */
int mirror_visual_broadcast_social_salience(
    mirror_visual_bridge_t* bridge,
    const social_salience_t* salience
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_VISUAL_BRIDGE_H */
