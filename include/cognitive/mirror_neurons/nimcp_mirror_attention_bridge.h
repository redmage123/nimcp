/**
 * @file nimcp_mirror_attention_bridge.h
 * @brief Mirror Neuron - Attention System Bidirectional Bridge
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Bidirectional integration between mirror neurons and attention system
 * WHY:  Joint attention and gaze following are mediated by mirror neuron mechanisms
 * HOW:  Mirror neurons detect gaze/attention direction; attention system follows
 *
 * THEORETICAL FOUNDATIONS:
 * - Tomasello (1995): Joint attention in social cognition development
 * - Rizzolatti & Craighero (2004): Mirror neurons and gaze following
 * - Shepherd (2010): Gaze cueing and attention mechanisms
 * - Frischen et al. (2007): Gaze cueing of attention review
 *
 * BIOLOGICAL BASIS:
 * - Superior temporal sulcus (STS) detects gaze direction
 * - Mirror neurons in premotor cortex simulate observed gaze shifts
 * - Automatic attention shift to gazed-at location (gaze cueing)
 * - Joint attention enables shared reference for social learning
 *
 * INTEGRATION FLOW:
 * Mirror → Attention:
 *   1. Mirror neurons detect gaze shift/pointing gesture
 *   2. Compute gazed-at location/attended object
 *   3. Cue attention system to shift to that location
 *   4. Enable joint attention state
 *
 * Attention → Mirror:
 *   1. Current attention focus affects mirror neuron sensitivity
 *   2. Attended actions get enhanced mirror response
 *   3. Divided attention reduces mirror resonance
 *
 * SIMD OPTIMIZATIONS:
 * - Vectorized gaze vector intersection calculations
 * - Batch saliency map updates
 * - SIMD attention weight computations
 *
 * BIO-ASYNC MESSAGES:
 * - BIO_MSG_MIRROR_GAZE_DETECTED: Mirror detected gaze direction
 * - BIO_MSG_MIRROR_JOINT_ATTENTION: Joint attention established
 * - BIO_MSG_MIRROR_ATTENTION_CUE: Attention cue from mirror observation
 *
 * @see nimcp_attention.h
 * @see nimcp_emotion_attention.h
 * @see nimcp_mirror_neurons.h
 */

#ifndef NIMCP_MIRROR_ATTENTION_BRIDGE_H
#define NIMCP_MIRROR_ATTENTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Note: BIO_MODULE_MIRROR_ATTENTION_BRIDGE is defined in nimcp_bio_messages.h */

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum spatial positions tracked */
#define MIRROR_ATTENTION_MAX_POSITIONS      64

/** @brief Gaze vector dimension (3D direction) */
#define MIRROR_ATTENTION_GAZE_DIM           3

/** @brief Maximum tracked agents for joint attention */
#define MIRROR_ATTENTION_MAX_AGENTS         16

/** @brief SIMD batch threshold */
#define MIRROR_ATTENTION_SIMD_THRESHOLD     8

/** @brief Saliency map dimensions (for spatial attention) */
#define MIRROR_ATTENTION_SALIENCY_SIZE      32

/** @brief History size for attention trajectories */
#define MIRROR_ATTENTION_HISTORY_SIZE       16

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Joint attention state
 *
 * WHAT: Current state of joint attention with observed agent
 */
typedef enum {
    JOINT_ATTENTION_NONE = 0,           /**< No joint attention */
    JOINT_ATTENTION_INITIATING,         /**< Self initiating joint attention */
    JOINT_ATTENTION_RESPONDING,         /**< Responding to other's bid */
    JOINT_ATTENTION_ESTABLISHED,        /**< Joint attention active */
    JOINT_ATTENTION_BREAKING            /**< Joint attention ending */
} joint_attention_state_t;

/**
 * @brief Attention cue type from mirror observation
 */
typedef enum {
    MIRROR_CUE_NONE = 0,
    MIRROR_CUE_GAZE,                    /**< Eye gaze direction */
    MIRROR_CUE_HEAD_TURN,               /**< Head orientation */
    MIRROR_CUE_POINTING,                /**< Pointing gesture */
    MIRROR_CUE_REACH,                   /**< Reaching action */
    MIRROR_CUE_BODY_ORIENT              /**< Body orientation */
} mirror_attention_cue_type_t;

/**
 * @brief Attention shift response type
 */
typedef enum {
    ATTENTION_SHIFT_NONE = 0,
    ATTENTION_SHIFT_REFLEXIVE,          /**< Automatic/reflexive shift */
    ATTENTION_SHIFT_VOLITIONAL,         /**< Voluntary following */
    ATTENTION_SHIFT_SUPPRESSED          /**< Shift suppressed */
} attention_shift_type_t;

/**
 * @brief Mirror-attention bridge state
 */
typedef enum {
    MIRROR_ATTENTION_STATE_IDLE = 0,
    MIRROR_ATTENTION_STATE_CUE_DETECTED,
    MIRROR_ATTENTION_STATE_SHIFTING,
    MIRROR_ATTENTION_STATE_JOINT,
    MIRROR_ATTENTION_STATE_INDEPENDENT
} mirror_attention_state_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief 3D position/vector for spatial attention
 */
typedef struct {
    float x;
    float y;
    float z;
} mirror_attention_vec3_t;

/**
 * @brief Gaze observation from mirror neurons
 *
 * WHAT: Data when mirror neurons detect gaze/attention cue
 */
typedef struct {
    uint32_t agent_id;                          /**< Observed agent */
    mirror_attention_cue_type_t cue_type;       /**< Type of attention cue */

    /** Gaze/pointing direction (normalized) */
    mirror_attention_vec3_t direction;

    /** Agent's position in space */
    mirror_attention_vec3_t agent_position;

    /** Estimated target position */
    mirror_attention_vec3_t target_position;
    bool target_position_valid;

    /** Target object (if identified) */
    uint32_t target_object_id;
    bool target_object_valid;

    /** Observation metrics */
    float confidence;                           /**< Detection confidence */
    float duration_ms;                          /**< How long gaze held */
    uint64_t timestamp_us;

    /** Context */
    bool is_mutual_gaze;                        /**< Looking at observer */
    bool is_referential;                        /**< Referential intent detected */
} mirror_gaze_observation_t;

/**
 * @brief Attention cue result
 *
 * WHAT: Output from processing gaze observation
 */
typedef struct {
    /** Recommended attention target */
    mirror_attention_vec3_t cue_location;
    uint32_t cue_object_id;
    bool object_cued;

    /** Cue strength and validity */
    float cue_strength;                         /**< How strong the cue is */
    float cue_validity;                         /**< Expected validity */
    attention_shift_type_t recommended_shift;

    /** Joint attention */
    joint_attention_state_t joint_state;
    uint32_t joint_agent_id;

    /** Timing */
    float expected_soa_ms;                      /**< Expected SOA for reflexive shift */
    uint64_t timestamp_us;
} mirror_attention_cue_t;

/**
 * @brief Per-agent attention tracking
 */
typedef struct {
    uint32_t agent_id;
    bool active;

    /** Current attention state */
    joint_attention_state_t joint_state;
    mirror_attention_vec3_t last_gaze_direction;
    mirror_attention_vec3_t last_target;

    /** Joint attention history */
    uint32_t successful_joint_attention_count;
    uint32_t failed_joint_attention_count;
    float joint_attention_tendency;             /**< Likelihood of responding */

    /** Gaze following history */
    float gaze_validity;                        /**< How valid their gaze cues are */
    float gaze_following_rate;                  /**< How often we follow them */

    /** Temporal data */
    uint64_t last_cue_timestamp_us;
    uint64_t joint_attention_start_us;
    float avg_joint_duration_ms;
} mirror_attention_agent_t;

/**
 * @brief Saliency modulation from attention
 */
typedef struct {
    /** Spatial saliency boost map */
    float saliency_boost[MIRROR_ATTENTION_SALIENCY_SIZE][MIRROR_ATTENTION_SALIENCY_SIZE];

    /** Currently attended location */
    mirror_attention_vec3_t attention_focus;
    float attention_strength;

    /** Attention spread (spotlight width) */
    float attention_sigma;

    uint64_t last_update_us;
} mirror_saliency_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** Gaze cueing parameters */
    float cue_validity_threshold;           /**< Min validity to follow cue */
    float reflexive_soa_ms;                 /**< SOA for reflexive shifting */
    float voluntary_soa_ms;                 /**< SOA for voluntary shifting */
    float gaze_cue_strength;                /**< Weight of gaze cues */
    float pointing_cue_strength;            /**< Weight of pointing cues */

    /** Joint attention parameters */
    float joint_attention_threshold;        /**< Threshold for joint attention */
    float joint_attention_timeout_ms;       /**< Timeout for joint attention */
    bool enable_joint_attention_initiation; /**< Can initiate joint attention */
    bool enable_referential_gaze;           /**< Process referential gaze */

    /** Attention modulation */
    float attention_mirror_gain;            /**< How attention affects mirror */
    float mirror_attention_gain;            /**< How mirror affects attention */

    /** Saliency integration */
    bool enable_saliency_modulation;
    float saliency_decay_rate;

    /** SIMD */
    bool enable_simd;

    /** Bio-async */
    bool bio_async_enabled;
} mirror_attention_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t gaze_cues_detected;
    uint64_t pointing_cues_detected;
    uint64_t attention_shifts_triggered;
    uint64_t joint_attention_episodes;
    uint64_t mutual_gaze_events;

    float avg_cue_strength;
    float avg_gaze_validity;
    float avg_joint_attention_duration_ms;

    uint32_t active_agents;
    float successful_joint_rate;

    uint64_t simd_operations;
} mirror_attention_stats_t;

/** Forward declaration */
typedef struct mirror_attention_bridge mirror_attention_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 */
mirror_attention_config_t mirror_attention_config_default(void);

/**
 * @brief Create mirror-attention bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL
 */
mirror_attention_bridge_t* mirror_attention_create(
    const mirror_attention_config_t* config
);

/**
 * @brief Destroy bridge
 */
void mirror_attention_destroy(mirror_attention_bridge_t* bridge);

//=============================================================================
// Gaze Processing API
//=============================================================================

/**
 * @brief Process gaze/attention cue observation
 *
 * WHAT: Main entry - process observed gaze/pointing
 * WHY:  Compute attention cue from mirror observation
 *
 * @param bridge Bridge handle
 * @param observation Gaze observation
 * @param cue Output: Attention cue result
 * @return true on success
 */
bool mirror_attention_process_gaze(
    mirror_attention_bridge_t* bridge,
    const mirror_gaze_observation_t* observation,
    mirror_attention_cue_t* cue
);

/**
 * @brief Compute gaze-target intersection
 *
 * WHAT: Calculate where gaze vector intersects scene
 * WHY:  Determine gazed-at location
 *
 * @param agent_position Agent's eye position
 * @param gaze_direction Normalized gaze direction
 * @param target Output: Intersection point
 * @return Distance to intersection or -1 if no hit
 */
float mirror_attention_compute_gaze_target(
    const mirror_attention_vec3_t* agent_position,
    const mirror_attention_vec3_t* gaze_direction,
    mirror_attention_vec3_t* target
);

/**
 * @brief Batch process multiple gaze observations (SIMD)
 *
 * @param bridge Bridge handle
 * @param observations Array of observations
 * @param cues Output array of cues
 * @param count Number to process
 * @return Number processed
 */
uint32_t mirror_attention_process_batch(
    mirror_attention_bridge_t* bridge,
    const mirror_gaze_observation_t* observations,
    mirror_attention_cue_t* cues,
    uint32_t count
);

//=============================================================================
// Joint Attention API
//=============================================================================

/**
 * @brief Check for joint attention with agent
 *
 * @param bridge Bridge handle
 * @param agent_id Agent to check
 * @return Current joint attention state
 */
joint_attention_state_t mirror_attention_get_joint_state(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id
);

/**
 * @brief Initiate joint attention bid
 *
 * WHAT: Start joint attention with target
 * WHY:  Establish shared reference
 *
 * @param bridge Bridge handle
 * @param agent_id Target agent
 * @param target Location to share attention on
 * @return true if bid initiated
 */
bool mirror_attention_initiate_joint(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id,
    const mirror_attention_vec3_t* target
);

/**
 * @brief Respond to joint attention bid
 *
 * WHAT: Accept/follow other's joint attention bid
 * WHY:  Complete joint attention episode
 *
 * @param bridge Bridge handle
 * @param agent_id Agent who bid
 * @return true if responded successfully
 */
bool mirror_attention_respond_to_joint(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id
);

/**
 * @brief Break joint attention
 *
 * @param bridge Bridge handle
 * @param agent_id Agent to break joint attention with
 */
void mirror_attention_break_joint(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id
);

//=============================================================================
// Attention Modulation API
//=============================================================================

/**
 * @brief Get mirror neuron sensitivity modulation
 *
 * WHAT: Query how current attention affects mirror sensitivity
 * WHY:  Attended actions get enhanced mirroring
 *
 * @param bridge Bridge handle
 * @param position Position to query
 * @return Sensitivity multiplier
 */
float mirror_attention_get_sensitivity_at(
    mirror_attention_bridge_t* bridge,
    const mirror_attention_vec3_t* position
);

/**
 * @brief Update attention focus
 *
 * WHAT: Inform bridge of current attention location
 * WHY:  Enable attention→mirror modulation
 *
 * @param bridge Bridge handle
 * @param focus Current attention focus
 * @param strength Attention strength
 * @param sigma Attention spread
 */
void mirror_attention_set_focus(
    mirror_attention_bridge_t* bridge,
    const mirror_attention_vec3_t* focus,
    float strength,
    float sigma
);

/**
 * @brief Get saliency boost at location
 *
 * @param bridge Bridge handle
 * @param x X coordinate (0-1 normalized)
 * @param y Y coordinate (0-1 normalized)
 * @return Saliency boost factor
 */
float mirror_attention_get_saliency_boost(
    mirror_attention_bridge_t* bridge,
    float x,
    float y
);

//=============================================================================
// SIMD Operations
//=============================================================================

/**
 * @brief SIMD batch gaze-target calculation
 */
void mirror_attention_simd_gaze_targets(
    const float* positions,      /* [count * 3] */
    const float* directions,     /* [count * 3] */
    float* targets,              /* [count * 3] output */
    float* distances,            /* [count] output */
    uint32_t count
);

/**
 * @brief SIMD attention saliency update
 */
void mirror_attention_simd_update_saliency(
    float* saliency_map,         /* [size * size] */
    uint32_t size,
    float focus_x,
    float focus_y,
    float sigma,
    float strength
);

//=============================================================================
// Agent API
//=============================================================================

/**
 * @brief Get agent tracking state
 */
mirror_attention_agent_t* mirror_attention_get_agent(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id
);

/**
 * @brief Update agent gaze validity
 *
 * WHAT: Learn how reliable agent's gaze cues are
 * WHY:  Adjust cue weighting based on experience
 *
 * @param bridge Bridge handle
 * @param agent_id Agent to update
 * @param valid Whether last cue was valid
 */
void mirror_attention_update_validity(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id,
    bool valid
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Register with bio-async
 */
bool mirror_attention_register_bio_async(mirror_attention_bridge_t* bridge);

/**
 * @brief Unregister from bio-async
 */
void mirror_attention_unregister_bio_async(mirror_attention_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get statistics
 */
bool mirror_attention_get_stats(
    const mirror_attention_bridge_t* bridge,
    mirror_attention_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void mirror_attention_reset_stats(mirror_attention_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_ATTENTION_BRIDGE_H */
