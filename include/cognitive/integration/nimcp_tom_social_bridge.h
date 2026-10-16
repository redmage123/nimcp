/**
 * @file nimcp_tom_social_bridge.h
 * @brief Bridge between Theory of Mind and Social Cognition systems
 *
 * WHAT: Bidirectional integration enabling ToM to inform social responses and
 *       social cues to trigger ToM inference processes.
 *
 * WHY: Theory of Mind and social cognition are deeply interdependent. Understanding
 *      others' mental states (ToM) is essential for appropriate social responses,
 *      while social cues provide input for mental state inference.
 *
 * HOW: Maintains shared agent models that both systems can query and update.
 *      ToM inferences guide social response selection; social cues trigger
 *      ToM updates about observed agents.
 *
 * BIOLOGICAL BASIS:
 * - ToM relies on medial prefrontal cortex (mPFC) and temporo-parietal junction (TPJ)
 * - Social cognition involves superior temporal sulcus (STS) and fusiform gyrus
 * - Bidirectional connections enable social perception to inform mental state inference
 *   and mental state understanding to guide social behavior
 * - Mirror neuron system bridges action observation and mental state attribution
 *
 * Integration Pattern:
 * ToM -> Social:
 *   - Mental state inferences inform response appropriateness
 *   - Belief/desire attribution guides interaction strategy
 *   - Emotional state inference enables empathic responses
 *
 * Social -> ToM:
 *   - Facial expressions trigger emotion inference
 *   - Body language updates belief about intent
 *   - Social context refines agent mental models
 *
 * @author NIMCP Development Team
 * @date 2025-01
 */

#ifndef NIMCP_TOM_SOCIAL_BRIDGE_H
#define NIMCP_TOM_SOCIAL_BRIDGE_H

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
 * @brief Opaque ToM-Social bridge structure
 *
 * WHAT: Forward declaration for ToM-Social bridge
 * WHY: Encapsulates implementation details
 * HOW: Full definition in implementation file
 */
typedef struct tom_social_bridge tom_social_bridge_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * @brief Maximum number of agents the bridge can track
 */
#define TOM_SOCIAL_MAX_AGENTS 64

/**
 * @brief Default inference depth for ToM processing
 */
#define TOM_SOCIAL_DEFAULT_INFERENCE_DEPTH 3

/**
 * @brief Social cue type identifiers
 */
typedef enum {
    TOM_SOCIAL_CUE_FACIAL_EXPRESSION = 0,   /**< Facial expression detected */
    TOM_SOCIAL_CUE_BODY_LANGUAGE,           /**< Body language/posture cue */
    TOM_SOCIAL_CUE_VOCAL_TONE,              /**< Voice prosody/tone cue */
    TOM_SOCIAL_CUE_GAZE_DIRECTION,          /**< Eye gaze direction cue */
    TOM_SOCIAL_CUE_PROXIMITY,               /**< Spatial proximity cue */
    TOM_SOCIAL_CUE_GESTURE,                 /**< Hand/body gesture cue */
    TOM_SOCIAL_CUE_VERBAL_CONTENT,          /**< Linguistic content cue */
    TOM_SOCIAL_CUE_CONTEXT                  /**< Situational context cue */
} tom_social_cue_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Configuration for ToM-Social bridge
 *
 * WHAT: Parameters controlling ToM-Social integration behavior
 *
 * WHY: Different scenarios require different balances between ToM depth
 *      and social responsiveness
 *
 * HOW: Configure inference depth, weighting, and agent capacity
 */
typedef struct {
    /** Maximum depth of recursive mental state inference (default: 3)
     *  Level 1: "Agent believes X"
     *  Level 2: "Agent believes I believe X"
     *  Level 3: "Agent believes I believe agent believes X" */
    uint32_t inference_depth;

    /** Weight given to social context in ToM inference [0-1] (default: 0.5)
     *  Higher values make ToM more responsive to social cues */
    float social_weight;

    /** Maximum number of agents to maintain models for (default: 32) */
    uint32_t agent_capacity;

    /** Enable automatic ToM inference on social cue reception */
    bool enable_auto_inference;

    /** Enable social response suggestions based on ToM state */
    bool enable_response_suggestions;

    /** Minimum confidence threshold for ToM inferences [0-1] */
    float inference_confidence_threshold;
} tom_social_config_t;

/**
 * @brief Mental state representation for an agent
 *
 * WHAT: Inferred mental state of a tracked agent
 *
 * WHY: ToM inferences need to be communicated to social cognition
 *
 * HOW: Contains beliefs, desires, emotions, and intentions attributed to agent
 */
typedef struct {
    /** Agent identifier */
    uint32_t agent_id;

    /** Confidence in this mental state model [0-1] */
    float confidence;

    /** Inferred emotional state valence [-1 to +1] */
    float emotional_valence;

    /** Inferred emotional arousal [0-1] */
    float emotional_arousal;

    /** Inferred intent toward self [-1=hostile, 0=neutral, +1=friendly] */
    float intent_toward_self;

    /** Inferred attention level toward self [0-1] */
    float attention_to_self;

    /** Inferred cognitive load [0-1] */
    float cognitive_load;

    /** Number of belief attributions */
    uint32_t belief_count;

    /** Timestamp of last update (ms) */
    uint64_t last_update_ms;
} tom_social_mental_state_t;

/**
 * @brief Belief update structure for agent model updates
 *
 * WHAT: Represents an update to an agent's mental model
 *
 * WHY: Both ToM and social systems need to update shared agent representations
 *
 * HOW: Contains update type, content, and confidence
 */
typedef struct {
    /** Type of belief being updated */
    uint32_t belief_type;

    /** Update content (interpretation depends on belief_type) */
    float belief_value;

    /** Confidence in this update [0-1] */
    float confidence;

    /** Source of update (0=ToM, 1=social, 2=external) */
    uint32_t source;
} tom_social_belief_update_t;

/**
 * @brief Agent state query result
 *
 * WHAT: Complete state information for a tracked agent
 *
 * WHY: Enables comprehensive queries about agent mental models
 *
 * HOW: Combines mental state with tracking metadata
 */
typedef struct {
    /** Current mental state inference */
    tom_social_mental_state_t mental_state;

    /** Time since last observation (ms) */
    uint64_t time_since_observation;

    /** Number of observations recorded */
    uint32_t observation_count;

    /** Model stability [0-1] (higher = more consistent observations) */
    float model_stability;

    /** Is agent currently being observed */
    bool is_observed;

    /** Is agent model valid/initialized */
    bool is_valid;
} tom_social_agent_state_t;

/**
 * @brief Statistics for ToM-Social bridge
 *
 * WHAT: Performance and activity metrics for the bridge
 *
 * WHY: Monitor integration health and detect issues
 *
 * HOW: Accumulates counts during bridge operation
 */
typedef struct {
    /** Number of ToM inferences performed for social responses */
    uint64_t inferences_made;

    /** Number of social cues processed by ToM */
    uint64_t social_cues_processed;

    /** Number of agent model updates */
    uint64_t agent_models_updated;

    /** Number of agents currently being tracked */
    uint32_t active_agents;

    /** Average inference confidence [0-1] */
    float avg_inference_confidence;

    /** Average time per inference (ms) */
    float avg_inference_time_ms;

    /** Number of failed inferences */
    uint64_t inference_failures;

    /** Number of cue processing failures */
    uint64_t cue_failures;
} tom_social_stats_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Initialize default ToM-Social configuration
 *
 * WHAT: Sets default parameters for ToM-Social bridge
 * WHY: Provides sensible defaults for typical use cases
 * HOW: Initializes config with balanced parameters
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int tom_social_default_config(tom_social_config_t* config);

/**
 * @brief Create ToM-Social bridge
 *
 * WHAT: Allocates and initializes ToM-Social integration bridge
 * WHY: Establishes bidirectional link between ToM and social systems
 * HOW: Creates bridge, initializes agent tracking, sets up inference pipeline
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Pointer to created bridge, NULL on failure
 */
tom_social_bridge_t* tom_social_bridge_create(const tom_social_config_t* config);

/**
 * @brief Destroy ToM-Social bridge
 *
 * WHAT: Cleans up and deallocates bridge
 * WHY: Prevents memory leaks and releases resources
 * HOW: Frees agent models, clears state, deallocates bridge
 *
 * @param bridge Bridge to destroy
 */
void tom_social_bridge_destroy(tom_social_bridge_t* bridge);

/**
 * @brief Infer mental state for social response
 *
 * WHAT: Uses ToM to infer agent mental state for guiding social response
 * WHY: Social responses should be informed by understanding of agent's mental state
 * HOW: Runs ToM inference pipeline, returns mental state relevant for response
 *
 * @param bridge Bridge instance
 * @param agent_id ID of agent to infer about
 * @param mental_state_out Output buffer for inferred mental state
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: mPFC/TPJ activation for mental state inference guides
 *                   appropriate social behavior selection in STS/temporal cortex.
 */
int tom_social_infer_for_response(
    tom_social_bridge_t* bridge,
    uint32_t agent_id,
    tom_social_mental_state_t* mental_state_out
);

/**
 * @brief Process social cue to trigger ToM update
 *
 * WHAT: Social cue detection triggers ToM inference/update
 * WHY: Social cues provide evidence for updating mental state models
 * HOW: Routes cue to appropriate ToM inference mechanisms
 *
 * @param bridge Bridge instance
 * @param cue_type Type of social cue observed
 * @param cue_data Cue-specific data (interpretation depends on cue_type)
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: STS/fusiform face area detections trigger TPJ/mPFC
 *                   inference processes to update agent mental models.
 */
int tom_social_on_social_cue(
    tom_social_bridge_t* bridge,
    tom_social_cue_type_t cue_type,
    const void* cue_data
);

/**
 * @brief Update shared agent model
 *
 * WHAT: Updates mental model for a tracked agent
 * WHY: Both ToM and social systems need to update shared representations
 * HOW: Applies belief update with confidence-weighted integration
 *
 * @param bridge Bridge instance
 * @param agent_id ID of agent to update
 * @param belief_update Belief update to apply
 * @return 0 on success, -1 on error
 */
int tom_social_update_agent_model(
    tom_social_bridge_t* bridge,
    uint32_t agent_id,
    const tom_social_belief_update_t* belief_update
);

/**
 * @brief Get agent state from shared model
 *
 * WHAT: Retrieves complete state for a tracked agent
 * WHY: Both systems need access to current agent mental models
 * HOW: Copies current agent state to output buffer
 *
 * @param bridge Bridge instance
 * @param agent_id ID of agent to query
 * @param state_out Output buffer for agent state
 * @return 0 on success, -1 on error (e.g., agent not found)
 */
int tom_social_get_agent_state(
    tom_social_bridge_t* bridge,
    uint32_t agent_id,
    tom_social_agent_state_t* state_out
);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves performance and activity metrics
 * WHY: Monitor bridge health and integration effectiveness
 * HOW: Copies current statistics to output buffer
 *
 * @param bridge Bridge instance
 * @param stats_out Output buffer for statistics
 * @return 0 on success, -1 on error
 */
int tom_social_get_stats(
    const tom_social_bridge_t* bridge,
    tom_social_stats_t* stats_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOM_SOCIAL_BRIDGE_H */
