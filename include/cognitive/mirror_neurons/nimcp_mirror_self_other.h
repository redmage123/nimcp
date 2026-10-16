/**
 * @file nimcp_mirror_self_other.h
 * @brief Mirror Neuron Self-Other Distinction Module
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Distinguishes self-generated actions from observed other actions
 * WHY:  Mirror neurons respond to both own and others' actions; disambiguation
 *       is critical for agency, imitation learning, and social cognition
 * HOW:  Integrates efference copy, proprioception, body schema, and temporal
 *       contingency to determine action ownership
 *
 * THEORETICAL FOUNDATIONS:
 * - Jeannerod (2003): The motor cognition hypothesis
 * - Frith (2005): Efference copy and agency attribution
 * - Decety & Sommerville (2003): Shared representations model
 * - Gallese (2007): Before and below theory of mind
 * - Tsakiris (2010): Body schema and self-recognition
 *
 * BIOLOGICAL BASIS:
 * - Efference copy from motor cortex predicts sensory consequences
 * - Comparator mechanism in parietal cortex (forward model)
 * - Mismatch = external agent (other's action)
 * - Match = self-generated action (suppress mirror response)
 * - Inferior parietal lobule (IPL) critical for self-other distinction
 * - Temporo-parietal junction (TPJ) for agency attribution
 *
 * SELF-OTHER DISCRIMINATION FLOW:
 * 1. Action observed or produced
 * 2. If motor command present (efference copy):
 *    - Forward model predicts sensory outcome
 *    - Compare prediction with actual sensory input
 *    - Match → self-generated, suppress mirror resonance
 *    - Mismatch → error signal, possible external perturbation
 * 3. If no efference copy:
 *    - Action is observed (other)
 *    - Full mirror resonance activated
 *
 * AGENCY ATTRIBUTION:
 * - Temporal contingency: Actions correlated with intent = self
 * - Spatial contingency: Actions from own body space = self
 * - Intentional binding: Compressed time perception for self-actions
 *
 * BIO-ASYNC MESSAGES:
 * - BIO_MSG_MIRROR_AGENCY_DETERMINED: Self vs other classification
 * - BIO_MSG_MIRROR_EFFERENCE_COPY: Motor command for prediction
 * - BIO_MSG_MIRROR_BODY_SCHEMA_UPDATE: Body representation change
 *
 * @see nimcp_mirror_motor_bridge.h
 * @see nimcp_mirror_tom_bridge.h
 */

#ifndef NIMCP_MIRROR_SELF_OTHER_H
#define NIMCP_MIRROR_SELF_OTHER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum tracked body parts in schema */
#define SELF_OTHER_MAX_BODY_PARTS       32

/** @brief Maximum efference copy buffer size */
#define SELF_OTHER_EFFERENCE_BUFFER     64

/** @brief Spatial position dimensions (3D + orientation) */
#define SELF_OTHER_SPATIAL_DIM          6

/** @brief Maximum history for agency decisions */
#define SELF_OTHER_HISTORY_SIZE         32

/** @brief Temporal window for contingency (microseconds) */
#define SELF_OTHER_CONTINGENCY_WINDOW_US 200000  /* 200ms */

/** @brief SIMD batch threshold */
#define SELF_OTHER_SIMD_THRESHOLD       8

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Agency attribution result
 *
 * WHAT: Classification of action ownership
 */
typedef enum {
    AGENCY_UNDETERMINED = 0,        /**< Cannot determine ownership */
    AGENCY_SELF,                    /**< Self-generated action */
    AGENCY_OTHER,                   /**< Other's action (observed) */
    AGENCY_SHARED,                  /**< Collaborative action */
    AGENCY_IMAGINED                 /**< Imagined/simulated action */
} agency_type_t;

/**
 * @brief Source of agency evidence
 */
typedef enum {
    AGENCY_EVIDENCE_NONE = 0,
    AGENCY_EVIDENCE_EFFERENCE_COPY,     /**< Motor command match */
    AGENCY_EVIDENCE_TEMPORAL,           /**< Temporal contingency */
    AGENCY_EVIDENCE_SPATIAL,            /**< Body space location */
    AGENCY_EVIDENCE_PROPRIOCEPTIVE,     /**< Body feedback match */
    AGENCY_EVIDENCE_VISUAL,             /**< Visual perspective */
    AGENCY_EVIDENCE_INTENTIONAL         /**< Intent-action binding */
} agency_evidence_t;

/**
 * @brief Body part identifier
 */
typedef enum {
    BODY_PART_HEAD = 0,
    BODY_PART_NECK,
    BODY_PART_TORSO,
    BODY_PART_LEFT_SHOULDER,
    BODY_PART_LEFT_ELBOW,
    BODY_PART_LEFT_WRIST,
    BODY_PART_LEFT_HAND,
    BODY_PART_RIGHT_SHOULDER,
    BODY_PART_RIGHT_ELBOW,
    BODY_PART_RIGHT_WRIST,
    BODY_PART_RIGHT_HAND,
    BODY_PART_LEFT_HIP,
    BODY_PART_LEFT_KNEE,
    BODY_PART_LEFT_ANKLE,
    BODY_PART_LEFT_FOOT,
    BODY_PART_RIGHT_HIP,
    BODY_PART_RIGHT_KNEE,
    BODY_PART_RIGHT_ANKLE,
    BODY_PART_RIGHT_FOOT,
    BODY_PART_COUNT
} body_part_id_t;

/**
 * @brief Self-other module state
 */
typedef enum {
    SELF_OTHER_STATE_IDLE = 0,
    SELF_OTHER_STATE_MONITORING,        /**< Monitoring for actions */
    SELF_OTHER_STATE_COMPARING,         /**< Comparing efference/sensory */
    SELF_OTHER_STATE_DECIDING           /**< Making agency decision */
} self_other_state_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief 3D position with orientation
 */
typedef struct {
    float x, y, z;              /**< Position */
    float rx, ry, rz;           /**< Rotation (Euler angles) */
} body_pose_t;

/**
 * @brief Body part state in schema
 */
typedef struct {
    body_part_id_t part_id;
    body_pose_t pose;           /**< Current pose */
    body_pose_t velocity;       /**< Current velocity */
    float confidence;           /**< Tracking confidence */
    uint64_t timestamp_us;
} body_part_state_t;

/**
 * @brief Body schema representation
 *
 * WHAT: Internal model of own body configuration
 * WHY:  Compare observed actions against own body possibilities
 */
typedef struct {
    body_part_state_t parts[SELF_OTHER_MAX_BODY_PARTS];
    uint32_t active_part_count;

    /** Body space boundaries (peripersonal space) */
    float reach_distance;           /**< Max arm reach */
    float personal_space_radius;    /**< Peripersonal boundary */

    /** Body capabilities */
    float max_joint_velocities[BODY_PART_COUNT];

    uint64_t last_update_us;
} body_schema_t;

/**
 * @brief Efference copy entry
 *
 * WHAT: Copy of motor command for prediction
 */
typedef struct {
    uint32_t action_id;
    body_part_id_t effector;        /**< Which body part */
    body_pose_t intended_pose;      /**< Intended end state */
    body_pose_t predicted_sensory;  /**< Predicted sensory feedback */
    float predicted_duration_ms;
    uint64_t command_time_us;
    bool awaiting_feedback;
    bool consumed;
} efference_copy_t;

/**
 * @brief Sensory feedback for comparison
 */
typedef struct {
    body_part_id_t effector;
    body_pose_t observed_pose;
    float observation_confidence;
    uint64_t observation_time_us;

    /** Visual perspective */
    bool is_first_person;           /**< First-person view */
    float visual_angle;             /**< Viewing angle to action */
} sensory_feedback_t;

/**
 * @brief Agency decision result
 */
typedef struct {
    agency_type_t agency;           /**< Self/other/shared classification */
    float confidence;               /**< Decision confidence [0-1] */

    /** Evidence breakdown */
    float efference_match;          /**< Efference copy match score */
    float temporal_match;           /**< Temporal contingency score */
    float spatial_match;            /**< Spatial ownership score */
    float proprioceptive_match;     /**< Body feedback match */

    /** Primary evidence source */
    agency_evidence_t primary_evidence;

    /** Prediction error (for learning) */
    float prediction_error;

    /** Timing */
    uint64_t decision_time_us;
    float processing_latency_ms;

    /** Agent identification (if other) */
    uint32_t attributed_agent_id;
} agency_decision_t;

/**
 * @brief Action observation for classification
 */
typedef struct {
    uint32_t observation_id;
    body_part_id_t effector;

    /** Observed action parameters */
    body_pose_t start_pose;
    body_pose_t end_pose;
    float duration_ms;

    /** Spatial context */
    float distance_from_self;       /**< Distance from observer */
    bool in_peripersonal_space;     /**< Within reach */
    float visual_angle;

    /** Temporal context */
    uint64_t onset_time_us;
    bool preceded_by_intent;        /**< Did observer intend this? */
    uint64_t intent_time_us;        /**< When intent formed */

    float observation_confidence;
} action_observation_t;

/**
 * @brief Configuration
 */
typedef struct {
    /** Matching thresholds */
    float efference_match_threshold;    /**< Min match for self */
    float temporal_contingency_ms;      /**< Max delay for self-attribution */
    float spatial_self_radius;          /**< Radius for self body space */

    /** Evidence weights */
    float weight_efference;
    float weight_temporal;
    float weight_spatial;
    float weight_proprioceptive;
    float weight_visual;

    /** Agency attribution */
    float self_threshold;               /**< Combined score for self */
    float other_threshold;              /**< Combined score for other */

    /** Mirror modulation */
    float self_suppression_gain;        /**< How much to suppress self-mirrors */
    bool enable_automatic_suppression;

    /** SIMD */
    bool enable_simd;

    /** Bio-async */
    bool bio_async_enabled;
} self_other_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t total_classifications;
    uint64_t self_attributions;
    uint64_t other_attributions;
    uint64_t shared_attributions;
    uint64_t undetermined;

    float avg_confidence;
    float avg_prediction_error;
    float avg_processing_latency_ms;

    /** Evidence usage */
    uint64_t efference_based_decisions;
    uint64_t temporal_based_decisions;
    uint64_t spatial_based_decisions;

    uint64_t simd_operations;
} self_other_stats_t;

/** Forward declaration */
typedef struct self_other_system self_other_system_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 */
self_other_config_t self_other_config_default(void);

/**
 * @brief Create self-other distinction system
 *
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on error
 */
self_other_system_t* self_other_create(const self_other_config_t* config);

/**
 * @brief Destroy system
 */
void self_other_destroy(self_other_system_t* system);

//=============================================================================
// Efference Copy API
//=============================================================================

/**
 * @brief Register efference copy (motor command)
 *
 * WHAT: Store motor command for forward model prediction
 * WHY:  Compare predicted outcome with actual feedback
 *
 * @param system System handle
 * @param action_id Unique action identifier
 * @param effector Body part being moved
 * @param intended_pose Target pose
 * @param predicted_duration_ms Expected movement duration
 * @return true if registered
 */
bool self_other_register_efference(
    self_other_system_t* system,
    uint32_t action_id,
    body_part_id_t effector,
    const body_pose_t* intended_pose,
    float predicted_duration_ms
);

/**
 * @brief Get pending efference copy for action
 */
efference_copy_t* self_other_get_efference(
    self_other_system_t* system,
    uint32_t action_id
);

/**
 * @brief Clear expired efference copies
 */
void self_other_clear_expired_efference(self_other_system_t* system);

//=============================================================================
// Body Schema API
//=============================================================================

/**
 * @brief Update body part state
 *
 * @param system System handle
 * @param part Body part ID
 * @param pose Current pose
 * @param velocity Current velocity
 * @param confidence Tracking confidence
 */
void self_other_update_body_part(
    self_other_system_t* system,
    body_part_id_t part,
    const body_pose_t* pose,
    const body_pose_t* velocity,
    float confidence
);

/**
 * @brief Get current body schema
 */
const body_schema_t* self_other_get_body_schema(
    const self_other_system_t* system
);

/**
 * @brief Check if position is in peripersonal space
 *
 * @param system System handle
 * @param x, y, z Position to check
 * @return true if within peripersonal space
 */
bool self_other_in_peripersonal_space(
    const self_other_system_t* system,
    float x, float y, float z
);

/**
 * @brief Compute distance from body center
 */
float self_other_distance_from_body(
    const self_other_system_t* system,
    float x, float y, float z
);

//=============================================================================
// Agency Classification API
//=============================================================================

/**
 * @brief Classify action agency (self vs other)
 *
 * WHAT: Main entry - determine who performed observed action
 * WHY:  Modulate mirror neuron response based on ownership
 *
 * @param system System handle
 * @param observation Action observation
 * @param sensory Sensory feedback (optional, NULL if unavailable)
 * @param decision Output: Agency decision
 * @return true on success
 */
bool self_other_classify_agency(
    self_other_system_t* system,
    const action_observation_t* observation,
    const sensory_feedback_t* sensory,
    agency_decision_t* decision
);

/**
 * @brief Classify with explicit efference match
 *
 * @param system System handle
 * @param observation Action observation
 * @param efference Efference copy to compare
 * @param sensory Sensory feedback
 * @param decision Output: Agency decision
 */
bool self_other_classify_with_efference(
    self_other_system_t* system,
    const action_observation_t* observation,
    const efference_copy_t* efference,
    const sensory_feedback_t* sensory,
    agency_decision_t* decision
);

/**
 * @brief Batch classify multiple observations (SIMD)
 */
uint32_t self_other_classify_batch(
    self_other_system_t* system,
    const action_observation_t* observations,
    const sensory_feedback_t* sensory,  /* Array or NULL */
    agency_decision_t* decisions,
    uint32_t count
);

//=============================================================================
// Mirror Modulation API
//=============================================================================

/**
 * @brief Get mirror suppression factor
 *
 * WHAT: Query how much to suppress mirror response
 * WHY:  Self-generated actions should not trigger full mirror resonance
 *
 * @param system System handle
 * @param agency Agency type
 * @return Suppression factor [0-1] (0 = full suppression, 1 = no suppression)
 */
float self_other_get_mirror_suppression(
    const self_other_system_t* system,
    agency_type_t agency
);

/**
 * @brief Compute mirror gain based on agency
 *
 * @param system System handle
 * @param decision Agency decision
 * @return Mirror gain multiplier
 */
float self_other_compute_mirror_gain(
    const self_other_system_t* system,
    const agency_decision_t* decision
);

//=============================================================================
// Comparator Functions (Forward Model)
//=============================================================================

/**
 * @brief Compare efference copy with sensory feedback
 *
 * @param efference Predicted state from motor command
 * @param sensory Actual sensory observation
 * @return Match score [0-1]
 */
float self_other_compare_efference_sensory(
    const efference_copy_t* efference,
    const sensory_feedback_t* sensory
);

/**
 * @brief Compute temporal contingency
 *
 * @param intent_time When intent was formed
 * @param action_time When action was observed
 * @param config Configuration for window
 * @return Contingency score [0-1]
 */
float self_other_compute_temporal_contingency(
    uint64_t intent_time,
    uint64_t action_time,
    const self_other_config_t* config
);

/**
 * @brief SIMD pose comparison
 */
void self_other_simd_compare_poses(
    const float* poses_a,       /* [count * 6] */
    const float* poses_b,       /* [count * 6] */
    float* similarities,        /* [count] output */
    uint32_t count
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Register with bio-async
 */
bool self_other_register_bio_async(self_other_system_t* system);

/**
 * @brief Unregister from bio-async
 */
void self_other_unregister_bio_async(self_other_system_t* system);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get statistics
 */
bool self_other_get_stats(
    const self_other_system_t* system,
    self_other_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void self_other_reset_stats(self_other_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_SELF_OTHER_H */
