/**
 * @file nimcp_retrosplenial.h
 * @brief Retrosplenial Cortex (RSC) - Spatial-Contextual Integration Hub
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Retrosplenial cortex implementation with spatial reference frame
 *       transformations, contextual memory encoding, scene recognition,
 *       navigation support, and imagination/planning capabilities.
 *
 * WHY:  The RSC is critical for:
 *       - Transforming between egocentric and allocentric spatial frames
 *       - Encoding contextual/episodic memories with spatial tags
 *       - Scene recognition and environmental familiarity
 *       - Supporting navigation through head direction integration
 *       - Imagination and mental simulation of future scenarios
 *       - Landmark anchoring and spatial orientation
 *
 * HOW:  Implements reference frame transformation circuits, context encoders,
 *       scene familiarity detectors, and bidirectional integration with
 *       hippocampus, entorhinal cortex, parietal cortex, and visual areas.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * ANATOMICAL LOCATION:
 * - Brodmann areas 29, 30 (granular and agranular retrosplenial cortex)
 * - Located in posterior cingulate region
 * - Strong reciprocal connections with hippocampus, parahippocampal cortex,
 *   parietal cortex, and visual areas
 *
 * FUNCTIONAL ROLES:
 * -----------------
 * 1. Spatial Reference Frame Transformation:
 *    - Converts egocentric (body-centered) to allocentric (world-centered) frames
 *    - Essential for navigation and spatial memory
 *    - Head direction signal integration from thalamus
 *
 * 2. Contextual Memory:
 *    - Encodes context for episodic memories
 *    - Provides "where" and "when" tags for hippocampal memories
 *    - Scene and environmental context binding
 *
 * 3. Scene Recognition:
 *    - Processes scene/place familiarity
 *    - Landmark recognition and anchoring
 *    - Environmental layout encoding
 *
 * 4. Navigation Support:
 *    - Route planning and path integration support
 *    - Head direction cell integration
 *    - Goal-directed spatial behavior
 *
 * 5. Imagination and Planning:
 *    - Mental simulation of future scenarios
 *    - Episodic future thinking
 *    - Spatial imagination (imagining being elsewhere)
 *
 * CONNECTIVITY:
 * -------------
 * - Hippocampus: Bidirectional (memory encoding/retrieval)
 * - Entorhinal cortex: Grid cell and path integration signals
 * - Parietal cortex: Egocentric spatial processing
 * - Visual areas: Scene information
 * - Anterior thalamic nuclei: Head direction signals
 * - Prefrontal cortex: Goal and planning information
 *
 * FULL BIDIRECTIONAL INTEGRATION:
 * - Security Module: Access control, threat detection
 * - Immune System: Anomaly detection, self-healing
 * - Bio-Async System: Neuromodulator channels (DA/5-HT/NE/ACh)
 * - Brain Factory/KG: Self-awareness, component registration
 * - Logging Module: Full audit trail, metrics
 * - SNN Module: Spiking neural network integration
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

#ifndef NIMCP_RETROSPLENIAL_H
#define NIMCP_RETROSPLENIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/platform/nimcp_platform_tier.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

/* Core brain systems */
typedef struct nimcp_brain nimcp_brain_t;
typedef struct nimcp_brain_kg nimcp_brain_kg_t;
typedef struct brain_immune_system brain_immune_system_t;

/* Bio-async communication */
typedef struct nimcp_bio_async_handler nimcp_bio_async_handler_t;
typedef struct nimcp_bio_router nimcp_bio_router_t;

/* Security */
typedef struct nimcp_security_context nimcp_security_context_t;

/* Neural systems */
typedef struct nimcp_snn_network nimcp_snn_network_t;

/* Connected brain regions */
typedef struct hippocampus_adapter hippocampus_adapter_t;
typedef struct nimcp_entorhinal nimcp_entorhinal_t;
typedef struct parietal_adapter parietal_adapter_t;
typedef struct thalamus_adapter thalamus_adapter_t;

/*=============================================================================
 * CONFIGURATION CONSTANTS
 *===========================================================================*/

/** Number of reference frame transformation neurons */
#define RSC_DEFAULT_TRANSFORM_NEURONS       256

/** Number of context encoding neurons */
#define RSC_DEFAULT_CONTEXT_NEURONS         512

/** Number of scene recognition neurons */
#define RSC_DEFAULT_SCENE_NEURONS           256

/** Number of head direction integration neurons */
#define RSC_DEFAULT_HD_NEURONS              60

/** Number of landmark neurons */
#define RSC_DEFAULT_LANDMARK_NEURONS        128

/** Default spatial dimensionality */
#define RSC_DEFAULT_SPATIAL_DIM             3

/** Default feature vector dimension */
#define RSC_DEFAULT_FEATURE_DIM             256

/** Maximum number of landmarks */
#define RSC_MAX_LANDMARKS                   256

/** Maximum context history size */
#define RSC_MAX_CONTEXT_HISTORY             64

/** Context encoding dimension */
#define RSC_CONTEXT_DIM                     128

/** Scene representation dimension */
#define RSC_SCENE_DIM                       256

/*=============================================================================
 * ERROR CODES
 *===========================================================================*/

/**
 * @brief RSC-specific error codes
 */
typedef enum {
    RSC_OK = 0,                             /**< Success */
    RSC_ERR_NULL_PTR = -1,                  /**< Null pointer parameter */
    RSC_ERR_INVALID_PARAM = -2,             /**< Invalid parameter value */
    RSC_ERR_NOT_INITIALIZED = -3,           /**< Module not initialized */
    RSC_ERR_ALREADY_INITIALIZED = -4,       /**< Already initialized */
    RSC_ERR_NO_MEMORY = -5,                 /**< Memory allocation failed */
    RSC_ERR_TRANSFORM_FAILED = -6,          /**< Reference frame transform failed */
    RSC_ERR_CONTEXT_ENCODING_FAILED = -7,   /**< Context encoding failed */
    RSC_ERR_SCENE_RECOGNITION_FAILED = -8,  /**< Scene recognition failed */
    RSC_ERR_NAVIGATION_FAILED = -9,         /**< Navigation computation failed */
    RSC_ERR_SECURITY_VIOLATION = -10,       /**< Security check failed */
    RSC_ERR_IMMUNE_REJECTION = -11,         /**< Immune system rejection */
    RSC_ERR_CAPACITY_EXCEEDED = -12,        /**< Capacity limit exceeded */
    RSC_ERR_INVALID_STATE = -13,            /**< Invalid state for operation */
    RSC_ERR_INTERNAL = -14                  /**< Internal error */
} nimcp_rsc_error_t;

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief RSC processing status
 */
typedef enum {
    RSC_STATUS_IDLE = 0,                    /**< Ready for processing */
    RSC_STATUS_TRANSFORMING,                /**< Reference frame transformation */
    RSC_STATUS_ENCODING_CONTEXT,            /**< Context encoding active */
    RSC_STATUS_RECOGNIZING_SCENE,           /**< Scene recognition active */
    RSC_STATUS_NAVIGATING,                  /**< Navigation support active */
    RSC_STATUS_IMAGINING,                   /**< Imagination/planning active */
    RSC_STATUS_RECALLING,                   /**< Context recall active */
    RSC_STATUS_READY,                       /**< Output ready */
    RSC_STATUS_ERROR                        /**< Error state */
} nimcp_rsc_status_t;

/**
 * @brief Spatial reference frame types
 */
typedef enum {
    RSC_FRAME_EGOCENTRIC = 0,               /**< Body-centered (viewer-relative) */
    RSC_FRAME_ALLOCENTRIC,                  /**< World-centered (environment-relative) */
    RSC_FRAME_OBJECT_CENTERED,              /**< Object-relative */
    RSC_FRAME_ROUTE_CENTERED,               /**< Route/path-relative */
    RSC_FRAME_COUNT
} nimcp_rsc_frame_t;

/**
 * @brief Context type classification
 */
typedef enum {
    RSC_CONTEXT_SPATIAL = 0,                /**< Spatial/location context */
    RSC_CONTEXT_TEMPORAL,                   /**< Temporal/time context */
    RSC_CONTEXT_ENVIRONMENTAL,              /**< Environmental features */
    RSC_CONTEXT_SOCIAL,                     /**< Social context */
    RSC_CONTEXT_EMOTIONAL,                  /**< Emotional context */
    RSC_CONTEXT_TASK,                       /**< Task/goal context */
    RSC_CONTEXT_COUNT
} nimcp_rsc_context_type_t;

/**
 * @brief Scene familiarity levels
 */
typedef enum {
    RSC_SCENE_NOVEL = 0,                    /**< Never seen before */
    RSC_SCENE_VAGUELY_FAMILIAR,             /**< Weak recognition */
    RSC_SCENE_FAMILIAR,                     /**< Clear recognition */
    RSC_SCENE_VERY_FAMILIAR,                /**< Strong recognition */
    RSC_SCENE_HIGHLY_FAMILIAR               /**< Extremely well known */
} nimcp_rsc_familiarity_t;

/**
 * @brief Imagination mode types
 */
typedef enum {
    RSC_IMAGINE_PROSPECTIVE = 0,            /**< Future scenario imagination */
    RSC_IMAGINE_RETROSPECTIVE,              /**< Past event reconstruction */
    RSC_IMAGINE_COUNTERFACTUAL,             /**< "What if" scenarios */
    RSC_IMAGINE_SPATIAL_SELF,               /**< Imagining self elsewhere */
    RSC_IMAGINE_PERSPECTIVE_TAKING          /**< Another's viewpoint */
} nimcp_rsc_imagine_mode_t;

/*=============================================================================
 * BIO-ASYNC MESSAGE TYPES
 *===========================================================================*/

/**
 * @brief RSC bio-async message types
 *
 * WHAT: Message types for RSC bio-async communication
 * WHY:  Enable typed message routing and subscription filtering
 * HOW:  Each type corresponds to a specific RSC function
 */
typedef enum {
    RSC_BIO_MSG_CONTEXT = 0,                /**< Context encoding broadcast */
    RSC_BIO_MSG_NAVIGATION,                 /**< Navigation state update */
    RSC_BIO_MSG_SCENE_FAMILIARITY,          /**< Scene familiarity signal */
    RSC_BIO_MSG_FRAME_TRANSFORM,            /**< Reference frame transform result */
    RSC_BIO_MSG_LANDMARK_DETECTED,          /**< Landmark detection event */
    RSC_BIO_MSG_HEAD_DIRECTION,             /**< Head direction update */
    RSC_BIO_MSG_IMAGINATION_STATE,          /**< Imagination/planning state */
    RSC_BIO_MSG_CONTEXT_REQUEST,            /**< Request for current context */
    RSC_BIO_MSG_TRANSFORM_REQUEST,          /**< Request frame transformation */
    RSC_BIO_MSG_COUNT
} nimcp_rsc_bio_msg_type_t;

/**
 * @brief Subscription bitmasks for RSC message types
 */
#define RSC_BIO_SUB_CONTEXT             (1U << RSC_BIO_MSG_CONTEXT)
#define RSC_BIO_SUB_NAVIGATION          (1U << RSC_BIO_MSG_NAVIGATION)
#define RSC_BIO_SUB_SCENE_FAMILIARITY   (1U << RSC_BIO_MSG_SCENE_FAMILIARITY)
#define RSC_BIO_SUB_FRAME_TRANSFORM     (1U << RSC_BIO_MSG_FRAME_TRANSFORM)
#define RSC_BIO_SUB_LANDMARK_DETECTED   (1U << RSC_BIO_MSG_LANDMARK_DETECTED)
#define RSC_BIO_SUB_HEAD_DIRECTION      (1U << RSC_BIO_MSG_HEAD_DIRECTION)
#define RSC_BIO_SUB_IMAGINATION_STATE   (1U << RSC_BIO_MSG_IMAGINATION_STATE)
#define RSC_BIO_SUB_ALL                 (0xFFFFFFFFU)

/*=============================================================================
 * CORE DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief 3D spatial position
 */
typedef struct {
    float x;                                /**< X coordinate */
    float y;                                /**< Y coordinate */
    float z;                                /**< Z coordinate */
} nimcp_rsc_position_t;

/**
 * @brief 3D orientation (Euler angles)
 */
typedef struct {
    float yaw;                              /**< Heading/yaw (radians) */
    float pitch;                            /**< Pitch (radians) */
    float roll;                             /**< Roll (radians) */
} nimcp_rsc_orientation_t;

/**
 * @brief Spatial pose (position + orientation)
 */
typedef struct {
    nimcp_rsc_position_t position;          /**< Spatial position */
    nimcp_rsc_orientation_t orientation;    /**< Spatial orientation */
    float confidence;                       /**< Pose confidence [0, 1] */
    uint64_t timestamp_us;                  /**< Timestamp */
} nimcp_rsc_pose_t;

/**
 * @brief Reference frame transformation matrix (4x4 homogeneous)
 */
typedef struct {
    float matrix[16];                       /**< 4x4 transformation matrix */
    nimcp_rsc_frame_t source_frame;         /**< Source reference frame */
    nimcp_rsc_frame_t target_frame;         /**< Target reference frame */
    float accuracy;                         /**< Transformation accuracy [0, 1] */
} nimcp_rsc_transform_t;

/**
 * @brief Landmark representation
 */
typedef struct {
    uint32_t id;                            /**< Landmark identifier */
    char name[64];                          /**< Landmark name */
    nimcp_rsc_position_t position;          /**< Allocentric position */
    float salience;                         /**< Visual salience [0, 1] */
    float stability;                        /**< Positional stability [0, 1] */
    float* visual_features;                 /**< Visual feature vector */
    uint32_t feature_dim;                   /**< Feature vector dimension */
    float recognition_strength;             /**< Recognition confidence [0, 1] */
    uint64_t last_seen_us;                  /**< Last observation timestamp */
    bool is_anchored;                       /**< Used for spatial anchoring */
} nimcp_rsc_landmark_t;

/**
 * @brief Context encoding state
 */
typedef struct {
    /* Context vectors per type */
    float* spatial_context;                 /**< Spatial context encoding */
    float* temporal_context;                /**< Temporal context encoding */
    float* environmental_context;           /**< Environmental features */
    float* social_context;                  /**< Social context encoding */
    float* emotional_context;               /**< Emotional context encoding */
    float* task_context;                    /**< Task/goal context encoding */

    /* Unified context representation */
    float* unified_context;                 /**< Combined context vector */
    uint32_t context_dim;                   /**< Context vector dimension */

    /* Context metadata */
    float context_strength;                 /**< Overall context strength [0, 1] */
    float context_stability;                /**< Context stability [0, 1] */
    nimcp_rsc_context_type_t dominant_type; /**< Dominant context type */
    uint64_t encoding_time_us;              /**< Encoding timestamp */
} nimcp_rsc_context_t;

/**
 * @brief Scene representation
 */
typedef struct {
    float* scene_vector;                    /**< Scene encoding vector */
    uint32_t scene_dim;                     /**< Scene vector dimension */
    nimcp_rsc_familiarity_t familiarity;    /**< Familiarity level */
    float familiarity_score;                /**< Familiarity score [0, 1] */
    float scene_coherence;                  /**< Scene coherence [0, 1] */
    float layout_confidence;                /**< Layout encoding confidence */

    /* Scene components */
    uint32_t num_landmarks;                 /**< Landmarks in scene */
    uint32_t* landmark_ids;                 /**< IDs of recognized landmarks */
    float* landmark_positions;              /**< Relative landmark positions */

    uint64_t timestamp_us;                  /**< Scene timestamp */
} nimcp_rsc_scene_t;

/**
 * @brief Navigation state
 */
typedef struct {
    /* Current state */
    nimcp_rsc_pose_t current_pose;          /**< Current estimated pose */
    float heading;                          /**< Current heading (radians) */
    float speed;                            /**< Current speed */
    float angular_velocity;                 /**< Angular velocity */

    /* Head direction integration */
    float head_direction;                   /**< Integrated head direction */
    float hd_confidence;                    /**< HD confidence [0, 1] */
    float* hd_cell_activations;             /**< HD cell activation pattern */
    uint32_t num_hd_cells;                  /**< Number of HD cells */

    /* Goal state */
    nimcp_rsc_pose_t goal_pose;             /**< Target pose */
    float distance_to_goal;                 /**< Distance to goal */
    float bearing_to_goal;                  /**< Bearing to goal */
    bool goal_set;                          /**< Goal is active */

    /* Path state */
    float* planned_path;                    /**< Planned waypoints */
    uint32_t path_length;                   /**< Number of waypoints */
    uint32_t current_waypoint;              /**< Current waypoint index */

    /* Frame transform state */
    nimcp_rsc_transform_t ego_to_allo;      /**< Egocentric to allocentric */
    nimcp_rsc_transform_t allo_to_ego;      /**< Allocentric to egocentric */
} nimcp_rsc_navigation_t;

/**
 * @brief Imagination/mental simulation state
 */
typedef struct {
    nimcp_rsc_imagine_mode_t mode;          /**< Current imagination mode */
    bool active;                            /**< Imagination active */

    /* Imagined state */
    nimcp_rsc_pose_t imagined_pose;         /**< Imagined spatial pose */
    nimcp_rsc_context_t imagined_context;   /**< Imagined context */
    nimcp_rsc_scene_t imagined_scene;       /**< Imagined scene */

    /* Simulation parameters */
    float vividness;                        /**< Imagination vividness [0, 1] */
    float plausibility;                     /**< Scenario plausibility [0, 1] */
    float temporal_distance;                /**< Temporal distance (seconds) */

    /* Planning support */
    float goal_proximity;                   /**< How close to goal */
    float obstacle_awareness;               /**< Obstacle detection level */
    uint32_t steps_simulated;               /**< Simulation steps completed */
} nimcp_rsc_imagination_t;

/*=============================================================================
 * BRIDGE STRUCTURES
 *===========================================================================*/

/**
 * @brief Security bridge state
 */
typedef struct {
    nimcp_security_context_t* security_ctx; /**< Security context */
    uint32_t access_level;                  /**< Current access level */
    bool threat_detected;                   /**< Threat detected flag */
    float threat_level;                     /**< Threat severity [0, 1] */
    uint64_t last_validation_us;            /**< Last validation timestamp */
} rsc_security_bridge_t;

/**
 * @brief Immune bridge state
 */
typedef struct {
    brain_immune_system_t* immune;          /**< Immune system reference */
    float health_score;                     /**< Module health [0, 1] */
    bool anomaly_detected;                  /**< Anomaly flag */
    float inflammation_level;               /**< Inflammation [0, 1] */
    uint64_t last_scan_us;                  /**< Last scan timestamp */
} rsc_immune_bridge_t;

/**
 * @brief Bio-async bridge state
 */
typedef struct {
    nimcp_bio_router_t* router;             /**< Bio-router reference */
    float dopamine_level;                   /**< DA modulation [0, 1] */
    float serotonin_level;                  /**< 5-HT modulation [0, 1] */
    float norepinephrine_level;             /**< NE modulation [0, 1] */
    float acetylcholine_level;              /**< ACh modulation [0, 1] */
    uint32_t pending_messages;              /**< Pending message count */
    uint64_t messages_sent;                 /**< Total messages sent */
    uint64_t messages_received;             /**< Total messages received */
} rsc_bio_async_bridge_t;

/**
 * @brief Brain KG bridge state
 */
typedef struct {
    nimcp_brain_kg_t* kg;                   /**< Knowledge graph reference */
    uint32_t node_id;                       /**< RSC node ID in KG */
    float health_status;                    /**< Health for KG reporting */
    uint32_t edge_count;                    /**< Connected edges */
} rsc_kg_bridge_t;

/**
 * @brief SNN bridge state
 */
typedef struct {
    nimcp_snn_network_t* snn;               /**< SNN network reference */
    uint32_t transform_layer_id;            /**< Transform neurons layer */
    uint32_t context_layer_id;              /**< Context neurons layer */
    uint32_t scene_layer_id;                /**< Scene neurons layer */
    uint32_t hd_layer_id;                   /**< HD neurons layer */
    float spike_rate;                       /**< Average spike rate */
    float mean_membrane_potential;          /**< Average membrane potential */
} rsc_snn_bridge_t;

/**
 * @brief Logging bridge state
 */
typedef struct {
    nimcp_logger_t logger;                 /**< Logger reference */
    bool verbose_logging;                   /**< Verbose mode flag */
    uint64_t log_entries;                   /**< Total log entries */
} rsc_logging_bridge_t;

/**
 * @brief Hippocampus bridge state
 */
typedef struct {
    hippocampus_adapter_t* hippocampus;     /**< Hippocampus reference */
    float context_binding_strength;         /**< Context-memory binding */
    float encoding_gate;                    /**< Memory encoding gate [0, 1] */
    float retrieval_gate;                   /**< Memory retrieval gate [0, 1] */
    uint64_t contexts_sent;                 /**< Contexts sent to hippocampus */
    uint64_t memories_received;             /**< Memories received */
} rsc_hippocampus_bridge_t;

/**
 * @brief Entorhinal bridge state
 */
typedef struct {
    nimcp_entorhinal_t* entorhinal;         /**< Entorhinal cortex reference */
    float grid_signal_strength;             /**< Grid cell signal strength */
    float path_integration_input;           /**< Path integration input */
    float heading_from_ec;                  /**< Heading from EC */
} rsc_entorhinal_bridge_t;

/**
 * @brief Parietal bridge state
 */
typedef struct {
    parietal_adapter_t* parietal;           /**< Parietal cortex reference */
    float egocentric_input_strength;        /**< Egocentric signal strength */
    float attention_modulation;             /**< Attention modulation [0, 1] */
    float spatial_attention_x;              /**< Spatial attention X */
    float spatial_attention_y;              /**< Spatial attention Y */
} rsc_parietal_bridge_t;

/**
 * @brief Thalamic bridge state (anterior thalamic nuclei)
 */
typedef struct {
    thalamus_adapter_t* thalamus;           /**< Thalamus reference */
    float hd_signal;                        /**< Head direction signal */
    float relay_gain;                       /**< Relay gain [0, 1] */
    float attention_gate;                   /**< Attention gating [0, 1] */
} rsc_thalamic_bridge_t;

/*=============================================================================
 * CONFIGURATION STRUCTURE
 *===========================================================================*/

/**
 * @brief RSC configuration
 */
typedef struct {
    /* Neuron counts */
    uint32_t num_transform_neurons;         /**< Reference frame transform neurons */
    uint32_t num_context_neurons;           /**< Context encoding neurons */
    uint32_t num_scene_neurons;             /**< Scene recognition neurons */
    uint32_t num_hd_neurons;                /**< Head direction neurons */
    uint32_t num_landmark_neurons;          /**< Landmark encoding neurons */

    /* Spatial parameters */
    uint32_t spatial_dim;                   /**< Spatial dimensionality (2 or 3) */
    uint32_t feature_dim;                   /**< Feature vector dimension */
    uint32_t context_dim;                   /**< Context vector dimension */
    uint32_t scene_dim;                     /**< Scene vector dimension */

    /* Transformation parameters */
    float transform_learning_rate;          /**< Transform learning rate */
    float transform_smoothing;              /**< Transform temporal smoothing */
    float transform_error_threshold;        /**< Error threshold for recalibration */

    /* Context encoding parameters */
    float context_decay_rate;               /**< Context decay rate */
    float context_update_rate;              /**< Context update rate */
    float context_threshold;                /**< Context activation threshold */
    uint32_t max_context_history;           /**< Maximum context history size */

    /* Scene recognition parameters */
    float scene_familiarity_threshold;      /**< Familiarity threshold */
    float scene_learning_rate;              /**< Scene learning rate */
    float landmark_salience_threshold;      /**< Landmark salience threshold */
    uint32_t max_landmarks;                 /**< Maximum tracked landmarks */

    /* Navigation parameters */
    float hd_integration_gain;              /**< HD integration gain */
    float path_integration_coupling;        /**< Coupling to path integration */
    float visual_calibration_rate;          /**< Visual landmark calibration rate */

    /* Imagination parameters */
    float imagination_vividness_default;    /**< Default imagination vividness */
    float temporal_projection_max;          /**< Max temporal projection (seconds) */
    bool enable_prospective_coding;         /**< Enable future scenario simulation */

    /* Integration enables */
    bool enable_security;                   /**< Enable security integration */
    bool enable_immune;                     /**< Enable immune integration */
    bool enable_bio_async;                  /**< Enable bio-async messaging */
    bool enable_kg;                         /**< Enable knowledge graph */
    bool enable_snn;                        /**< Enable SNN integration */
    bool enable_logging;                    /**< Enable detailed logging */
    bool enable_hippocampus;                /**< Enable hippocampus bridge */
    bool enable_entorhinal;                 /**< Enable entorhinal bridge */
    bool enable_parietal;                   /**< Enable parietal bridge */
    bool enable_thalamic;                   /**< Enable thalamic bridge */

    /* Platform tier */
    platform_tier_t min_tier;               /**< Minimum platform tier */
} nimcp_rsc_config_t;

/*=============================================================================
 * STATISTICS STRUCTURE
 *===========================================================================*/

/**
 * @brief RSC statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t updates_processed;             /**< Total update cycles */
    uint64_t transforms_computed;           /**< Reference frame transforms */
    uint64_t contexts_encoded;              /**< Contexts encoded */
    uint64_t contexts_recalled;             /**< Contexts recalled */
    uint64_t scenes_recognized;             /**< Scenes recognized */
    uint64_t landmarks_detected;            /**< Landmarks detected */
    uint64_t navigation_updates;            /**< Navigation state updates */
    uint64_t imagination_episodes;          /**< Imagination episodes */

    /* Quality metrics */
    float mean_transform_accuracy;          /**< Average transform accuracy */
    float mean_context_strength;            /**< Average context strength */
    float mean_scene_familiarity;           /**< Average scene familiarity */
    float mean_hd_confidence;               /**< Average HD confidence */

    /* Error tracking */
    uint64_t transform_errors;              /**< Transform computation errors */
    uint64_t context_errors;                /**< Context encoding errors */
    uint64_t recognition_failures;          /**< Scene recognition failures */

    /* Integration metrics */
    uint64_t bio_messages_sent;             /**< Bio-async messages sent */
    uint64_t bio_messages_received;         /**< Bio-async messages received */
    uint64_t hippocampus_interactions;      /**< Hippocampus interactions */
    uint64_t security_validations;          /**< Security validations */
    uint64_t immune_scans;                  /**< Immune system scans */

    /* Timing metrics */
    float mean_update_latency_us;           /**< Average update latency */
    float max_update_latency_us;            /**< Maximum update latency */
    float mean_transform_latency_us;        /**< Average transform latency */
    float mean_encoding_latency_us;         /**< Average encoding latency */

    /* Resource metrics */
    uint32_t active_landmarks;              /**< Currently tracked landmarks */
    uint32_t context_history_size;          /**< Current context history size */
    float memory_usage_bytes;               /**< Estimated memory usage */
} nimcp_rsc_stats_t;

/*=============================================================================
 * MAIN RSC STRUCTURE
 *===========================================================================*/

/**
 * @brief Complete Retrosplenial Cortex system
 */
typedef struct nimcp_retrosplenial {
    /* Configuration */
    nimcp_rsc_config_t config;

    /* Status */
    nimcp_rsc_status_t status;
    nimcp_rsc_error_t last_error;
    bool initialized;

    /* Core state */
    nimcp_rsc_navigation_t navigation;      /**< Navigation state */
    nimcp_rsc_context_t current_context;    /**< Current context */
    nimcp_rsc_scene_t current_scene;        /**< Current scene */
    nimcp_rsc_imagination_t imagination;    /**< Imagination state */

    /* Landmarks */
    nimcp_rsc_landmark_t* landmarks;        /**< Landmark array */
    uint32_t num_landmarks;                 /**< Active landmark count */
    uint32_t landmark_capacity;             /**< Landmark array capacity */

    /* Context history */
    nimcp_rsc_context_t* context_history;   /**< Context history buffer */
    uint32_t context_history_size;          /**< Current history size */
    uint32_t context_history_index;         /**< Current history index */

    /* Neural activations (for SNN/training) */
    float* transform_activations;           /**< Transform neuron activations */
    float* context_activations;             /**< Context neuron activations */
    float* scene_activations;               /**< Scene neuron activations */
    float* hd_activations;                  /**< HD neuron activations */

    /* Integration bridges */
    rsc_security_bridge_t security_bridge;
    rsc_immune_bridge_t immune_bridge;
    rsc_bio_async_bridge_t bio_async_bridge;
    rsc_kg_bridge_t kg_bridge;
    rsc_snn_bridge_t snn_bridge;
    rsc_logging_bridge_t logging_bridge;
    rsc_hippocampus_bridge_t hippocampus_bridge;
    rsc_entorhinal_bridge_t entorhinal_bridge;
    rsc_parietal_bridge_t parietal_bridge;
    rsc_thalamic_bridge_t thalamic_bridge;

    /* Statistics */
    nimcp_rsc_stats_t stats;

    /* Timing */
    uint64_t creation_time_us;              /**< Creation timestamp */
    uint64_t last_update_us;                /**< Last update timestamp */
    float simulation_dt_ms;                 /**< Simulation timestep */

    /* Thread safety */
    nimcp_mutex_t* mutex;                   /**< Thread safety mutex */
    bool mutex_owned;                       /**< Mutex ownership flag */
} nimcp_retrosplenial_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for context encoding events
 */
typedef void (*rsc_context_callback_t)(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_context_t* context,
    void* user_data
);

/**
 * @brief Callback for scene recognition events
 */
typedef void (*rsc_scene_callback_t)(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_scene_t* scene,
    nimcp_rsc_familiarity_t familiarity,
    void* user_data
);

/**
 * @brief Callback for landmark detection events
 */
typedef void (*rsc_landmark_callback_t)(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_landmark_t* landmark,
    void* user_data
);

/**
 * @brief Callback for navigation events
 */
typedef void (*rsc_navigation_callback_t)(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_navigation_t* nav_state,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default RSC configuration
 *
 * WHAT: Returns default configuration with biologically-motivated parameters
 * WHY:  Provide sensible defaults for typical use cases
 * HOW:  Initialize all fields with evidence-based values
 *
 * @return Default configuration structure
 */
nimcp_rsc_config_t nimcp_rsc_default_config(void);

/**
 * @brief Create RSC instance
 *
 * WHAT: Allocate and initialize retrosplenial cortex module
 * WHY:  Central entry point for RSC functionality
 * HOW:  Allocate structures, initialize state, prepare bridges
 *
 * @param config Configuration (NULL for defaults)
 * @return New RSC instance, or NULL on failure
 */
nimcp_retrosplenial_t* nimcp_rsc_create(const nimcp_rsc_config_t* config);

/**
 * @brief Destroy RSC instance
 *
 * WHAT: Free all resources associated with RSC
 * WHY:  Proper cleanup and resource deallocation
 * HOW:  Disconnect bridges, free memory, release mutex
 *
 * @param rsc RSC instance to destroy (NULL-safe)
 */
void nimcp_rsc_destroy(nimcp_retrosplenial_t* rsc);

/**
 * @brief Reset RSC to initial state
 *
 * WHAT: Clear state while preserving configuration
 * WHY:  Prepare for new session without full reinitialization
 * HOW:  Reset navigation, context, scene; clear history
 *
 * @param rsc RSC instance
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_reset(nimcp_retrosplenial_t* rsc);

/**
 * @brief Main update function
 *
 * WHAT: Perform one RSC processing cycle
 * WHY:  Central update point for all RSC functions
 * HOW:  Update transforms, context, scene, navigation
 *
 * @param rsc RSC instance
 * @param dt_ms Time delta in milliseconds
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_update(nimcp_retrosplenial_t* rsc, float dt_ms);

/*=============================================================================
 * REFERENCE FRAME TRANSFORMATION API
 *===========================================================================*/

/**
 * @brief Transform position between reference frames
 *
 * WHAT: Convert position from one reference frame to another
 * WHY:  Core RSC function for spatial processing
 * HOW:  Apply learned transformation matrix
 *
 * @param rsc RSC instance
 * @param input_pos Input position
 * @param source_frame Source reference frame
 * @param target_frame Target reference frame
 * @param output_pos Output position
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_transform_position(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_position_t* input_pos,
    nimcp_rsc_frame_t source_frame,
    nimcp_rsc_frame_t target_frame,
    nimcp_rsc_position_t* output_pos
);

/**
 * @brief Transform full pose between reference frames
 *
 * WHAT: Convert pose (position + orientation) between frames
 * WHY:  Complete spatial transformation
 * HOW:  Apply transformation to both position and orientation
 *
 * @param rsc RSC instance
 * @param input_pose Input pose
 * @param source_frame Source reference frame
 * @param target_frame Target reference frame
 * @param output_pose Output pose
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_transform_pose(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_pose_t* input_pose,
    nimcp_rsc_frame_t source_frame,
    nimcp_rsc_frame_t target_frame,
    nimcp_rsc_pose_t* output_pose
);

/**
 * @brief Update transformation with new calibration data
 *
 * WHAT: Learn/update reference frame transformation
 * WHY:  Improve transformation accuracy over time
 * HOW:  Update transformation matrix based on paired observations
 *
 * @param rsc RSC instance
 * @param ego_pos Egocentric position
 * @param allo_pos Corresponding allocentric position
 * @param heading Current heading
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_calibrate_transform(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_position_t* ego_pos,
    const nimcp_rsc_position_t* allo_pos,
    float heading
);

/**
 * @brief Get current transformation matrix
 *
 * WHAT: Retrieve current transformation state
 * WHY:  Allow inspection and external use
 * HOW:  Copy current transformation to output
 *
 * @param rsc RSC instance
 * @param source_frame Source frame
 * @param target_frame Target frame
 * @param transform Output transformation
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_get_transform(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_frame_t source_frame,
    nimcp_rsc_frame_t target_frame,
    nimcp_rsc_transform_t* transform
);

/*=============================================================================
 * CONTEXT ENCODING API
 *===========================================================================*/

/**
 * @brief Encode current context
 *
 * WHAT: Create context encoding from current state
 * WHY:  Generate contextual tag for memory operations
 * HOW:  Combine spatial, temporal, environmental information
 *
 * @param rsc RSC instance
 * @param spatial_features Spatial feature input (optional)
 * @param spatial_dim Spatial feature dimension
 * @param temporal_features Temporal feature input (optional)
 * @param temporal_dim Temporal feature dimension
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_encode_context(
    nimcp_retrosplenial_t* rsc,
    const float* spatial_features,
    uint32_t spatial_dim,
    const float* temporal_features,
    uint32_t temporal_dim
);

/**
 * @brief Get current context encoding
 *
 * WHAT: Retrieve current context state
 * WHY:  Provide context for memory encoding
 * HOW:  Copy current context to output
 *
 * @param rsc RSC instance
 * @param context Output context structure
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_get_context(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_context_t* context
);

/**
 * @brief Recall context from cue
 *
 * WHAT: Retrieve past context matching cue
 * WHY:  Support context-dependent memory retrieval
 * HOW:  Pattern match against context history
 *
 * @param rsc RSC instance
 * @param cue Context cue vector
 * @param cue_dim Cue dimension
 * @param recalled_context Output recalled context
 * @param similarity Output similarity score
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_recall_context(
    nimcp_retrosplenial_t* rsc,
    const float* cue,
    uint32_t cue_dim,
    nimcp_rsc_context_t* recalled_context,
    float* similarity
);

/**
 * @brief Update context with new information
 *
 * WHAT: Integrate new information into current context
 * WHY:  Context evolves with experience
 * HOW:  Blend new information with existing context
 *
 * @param rsc RSC instance
 * @param context_type Type of context to update
 * @param features New feature vector
 * @param feature_dim Feature dimension
 * @param blend_factor Blending factor [0, 1]
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_update_context(
    nimcp_retrosplenial_t* rsc,
    nimcp_rsc_context_type_t context_type,
    const float* features,
    uint32_t feature_dim,
    float blend_factor
);

/*=============================================================================
 * SCENE RECOGNITION API
 *===========================================================================*/

/**
 * @brief Process scene input
 *
 * WHAT: Analyze visual scene for recognition
 * WHY:  Determine scene familiarity and extract landmarks
 * HOW:  Pattern match against stored scenes
 *
 * @param rsc RSC instance
 * @param scene_features Visual scene features
 * @param feature_dim Feature dimension
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_process_scene(
    nimcp_retrosplenial_t* rsc,
    const float* scene_features,
    uint32_t feature_dim
);

/**
 * @brief Get current scene state
 *
 * WHAT: Retrieve current scene analysis
 * WHY:  Access scene recognition results
 * HOW:  Copy current scene state to output
 *
 * @param rsc RSC instance
 * @param scene Output scene structure
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_get_scene(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_scene_t* scene
);

/**
 * @brief Get scene familiarity
 *
 * WHAT: Query familiarity of current scene
 * WHY:  Quick familiarity check
 * HOW:  Return current familiarity level and score
 *
 * @param rsc RSC instance
 * @param familiarity Output familiarity level
 * @param score Output familiarity score [0, 1]
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_get_familiarity(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_familiarity_t* familiarity,
    float* score
);

/*=============================================================================
 * NAVIGATION SUPPORT API
 *===========================================================================*/

/**
 * @brief Update navigation state
 *
 * WHAT: Integrate new navigation information
 * WHY:  Maintain current spatial state
 * HOW:  Fuse position, heading, velocity inputs
 *
 * @param rsc RSC instance
 * @param position Current position
 * @param heading Current heading (radians)
 * @param velocity Current velocity
 * @param angular_velocity Angular velocity
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_update_navigation(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_position_t* position,
    float heading,
    float velocity,
    float angular_velocity
);

/**
 * @brief Integrate head direction signal
 *
 * WHAT: Process head direction input from thalamus
 * WHY:  HD integration is key RSC function
 * HOW:  Update HD cell activations and heading estimate
 *
 * @param rsc RSC instance
 * @param hd_signal Head direction signal (radians)
 * @param confidence Signal confidence [0, 1]
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_integrate_head_direction(
    nimcp_retrosplenial_t* rsc,
    float hd_signal,
    float confidence
);

/**
 * @brief Set navigation goal
 *
 * WHAT: Set target destination for navigation
 * WHY:  Enable goal-directed spatial behavior
 * HOW:  Store goal and compute initial path
 *
 * @param rsc RSC instance
 * @param goal_position Goal position
 * @param goal_heading Goal heading (optional, -1 to ignore)
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_set_navigation_goal(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_position_t* goal_position,
    float goal_heading
);

/**
 * @brief Get navigation guidance
 *
 * WHAT: Get navigation assistance toward goal
 * WHY:  Support spatial decision making
 * HOW:  Compute bearing and distance to goal
 *
 * @param rsc RSC instance
 * @param bearing Output bearing to goal (radians)
 * @param distance Output distance to goal
 * @param confidence Output confidence [0, 1]
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_get_navigation_guidance(
    const nimcp_retrosplenial_t* rsc,
    float* bearing,
    float* distance,
    float* confidence
);

/**
 * @brief Get current navigation state
 *
 * WHAT: Retrieve complete navigation state
 * WHY:  Full access to navigation information
 * HOW:  Copy navigation state to output
 *
 * @param rsc RSC instance
 * @param nav_state Output navigation state
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_get_navigation_state(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_navigation_t* nav_state
);

/*=============================================================================
 * LANDMARK API
 *===========================================================================*/

/**
 * @brief Add landmark
 *
 * WHAT: Register new landmark for spatial anchoring
 * WHY:  Landmarks support navigation and orientation
 * HOW:  Store landmark with position and features
 *
 * @param rsc RSC instance
 * @param position Landmark allocentric position
 * @param name Landmark name
 * @param visual_features Visual feature vector (optional)
 * @param feature_dim Feature dimension
 * @param landmark_id Output landmark ID
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_add_landmark(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_position_t* position,
    const char* name,
    const float* visual_features,
    uint32_t feature_dim,
    uint32_t* landmark_id
);

/**
 * @brief Detect landmarks in scene
 *
 * WHAT: Find known landmarks in current scene
 * WHY:  Support spatial anchoring and orientation
 * HOW:  Match scene features against landmark database
 *
 * @param rsc RSC instance
 * @param scene_features Scene feature vector
 * @param feature_dim Feature dimension
 * @param detected_ids Output array of detected landmark IDs
 * @param max_detections Maximum detections to return
 * @param num_detected Output number of detections
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_detect_landmarks(
    nimcp_retrosplenial_t* rsc,
    const float* scene_features,
    uint32_t feature_dim,
    uint32_t* detected_ids,
    uint32_t max_detections,
    uint32_t* num_detected
);

/**
 * @brief Get landmark by ID
 *
 * WHAT: Retrieve landmark information
 * WHY:  Access landmark details
 * HOW:  Lookup landmark in database
 *
 * @param rsc RSC instance
 * @param landmark_id Landmark identifier
 * @param landmark Output landmark structure
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_get_landmark(
    const nimcp_retrosplenial_t* rsc,
    uint32_t landmark_id,
    nimcp_rsc_landmark_t* landmark
);

/**
 * @brief Use landmark for spatial anchoring
 *
 * WHAT: Calibrate position using recognized landmark
 * WHY:  Correct drift in navigation estimates
 * HOW:  Update transforms using known landmark position
 *
 * @param rsc RSC instance
 * @param landmark_id Recognized landmark ID
 * @param observed_direction Direction to landmark (radians)
 * @param observed_distance Estimated distance to landmark
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_anchor_to_landmark(
    nimcp_retrosplenial_t* rsc,
    uint32_t landmark_id,
    float observed_direction,
    float observed_distance
);

/*=============================================================================
 * IMAGINATION AND PLANNING API
 *===========================================================================*/

/**
 * @brief Start imagination episode
 *
 * WHAT: Begin mental simulation
 * WHY:  Support planning and prospective thinking
 * HOW:  Initialize imagination state with mode and target
 *
 * @param rsc RSC instance
 * @param mode Imagination mode
 * @param target_pose Target pose to imagine (optional)
 * @param temporal_distance Temporal distance (seconds, for future/past)
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_start_imagination(
    nimcp_retrosplenial_t* rsc,
    nimcp_rsc_imagine_mode_t mode,
    const nimcp_rsc_pose_t* target_pose,
    float temporal_distance
);

/**
 * @brief Step imagination forward
 *
 * WHAT: Advance mental simulation one step
 * WHY:  Progress through imagined scenario
 * HOW:  Update imagined state based on mode
 *
 * @param rsc RSC instance
 * @param dt_ms Time step in milliseconds
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_step_imagination(
    nimcp_retrosplenial_t* rsc,
    float dt_ms
);

/**
 * @brief Stop imagination episode
 *
 * WHAT: End mental simulation
 * WHY:  Return to perception-based processing
 * HOW:  Deactivate imagination state
 *
 * @param rsc RSC instance
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_stop_imagination(nimcp_retrosplenial_t* rsc);

/**
 * @brief Get imagination state
 *
 * WHAT: Retrieve current imagination state
 * WHY:  Access imagined scenario details
 * HOW:  Copy imagination state to output
 *
 * @param rsc RSC instance
 * @param imagination Output imagination state
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_get_imagination_state(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_imagination_t* imagination
);

/*=============================================================================
 * BRIDGE INITIALIZATION API
 *===========================================================================*/

/**
 * @brief Initialize security bridge
 */
nimcp_rsc_error_t nimcp_rsc_init_security_bridge(
    nimcp_retrosplenial_t* rsc,
    nimcp_security_context_t* security_ctx
);

/**
 * @brief Initialize immune bridge
 */
nimcp_rsc_error_t nimcp_rsc_init_immune_bridge(
    nimcp_retrosplenial_t* rsc,
    brain_immune_system_t* immune
);

/**
 * @brief Initialize bio-async bridge
 */
nimcp_rsc_error_t nimcp_rsc_init_bio_async_bridge(
    nimcp_retrosplenial_t* rsc,
    nimcp_bio_router_t* router
);

/**
 * @brief Initialize brain KG bridge
 */
nimcp_rsc_error_t nimcp_rsc_init_kg_bridge(
    nimcp_retrosplenial_t* rsc,
    nimcp_brain_kg_t* kg
);

/**
 * @brief Initialize SNN bridge
 */
nimcp_rsc_error_t nimcp_rsc_init_snn_bridge(
    nimcp_retrosplenial_t* rsc,
    nimcp_snn_network_t* snn
);

/**
 * @brief Initialize logging bridge
 */
nimcp_rsc_error_t nimcp_rsc_init_logging_bridge(
    nimcp_retrosplenial_t* rsc,
    nimcp_logger_t logger
);

/**
 * @brief Initialize hippocampus bridge
 */
nimcp_rsc_error_t nimcp_rsc_init_hippocampus_bridge(
    nimcp_retrosplenial_t* rsc,
    hippocampus_adapter_t* hippocampus
);

/**
 * @brief Initialize entorhinal bridge
 */
nimcp_rsc_error_t nimcp_rsc_init_entorhinal_bridge(
    nimcp_retrosplenial_t* rsc,
    nimcp_entorhinal_t* entorhinal
);

/**
 * @brief Initialize parietal bridge
 */
nimcp_rsc_error_t nimcp_rsc_init_parietal_bridge(
    nimcp_retrosplenial_t* rsc,
    parietal_adapter_t* parietal
);

/**
 * @brief Initialize thalamic bridge
 */
nimcp_rsc_error_t nimcp_rsc_init_thalamic_bridge(
    nimcp_retrosplenial_t* rsc,
    thalamus_adapter_t* thalamus
);

/**
 * @brief Initialize all bridges from brain instance
 */
nimcp_rsc_error_t nimcp_rsc_init_all_bridges(
    nimcp_retrosplenial_t* rsc,
    nimcp_brain_t* brain
);

/*=============================================================================
 * BIO-ASYNC MESSAGING API
 *===========================================================================*/

/**
 * @brief Process pending bio-async messages
 *
 * @param rsc RSC instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed, or negative on error
 */
int nimcp_rsc_process_bio_messages(
    nimcp_retrosplenial_t* rsc,
    uint32_t max_messages
);

/**
 * @brief Broadcast context update
 *
 * @param rsc RSC instance
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_broadcast_context(nimcp_retrosplenial_t* rsc);

/**
 * @brief Broadcast navigation state
 *
 * @param rsc RSC instance
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_broadcast_navigation(nimcp_retrosplenial_t* rsc);

/**
 * @brief Broadcast scene familiarity
 *
 * @param rsc RSC instance
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_broadcast_familiarity(nimcp_retrosplenial_t* rsc);

/**
 * @brief Broadcast landmark detection
 *
 * @param rsc RSC instance
 * @param landmark_id Detected landmark ID
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_broadcast_landmark(
    nimcp_retrosplenial_t* rsc,
    uint32_t landmark_id
);

/*=============================================================================
 * STATUS AND DIAGNOSTICS API
 *===========================================================================*/

/**
 * @brief Get current status
 */
nimcp_rsc_status_t nimcp_rsc_get_status(const nimcp_retrosplenial_t* rsc);

/**
 * @brief Get last error
 */
nimcp_rsc_error_t nimcp_rsc_get_last_error(const nimcp_retrosplenial_t* rsc);

/**
 * @brief Get error string
 */
const char* nimcp_rsc_error_string(nimcp_rsc_error_t error);

/**
 * @brief Get status string
 */
const char* nimcp_rsc_status_string(nimcp_rsc_status_t status);

/**
 * @brief Get reference frame string
 */
const char* nimcp_rsc_frame_string(nimcp_rsc_frame_t frame);

/**
 * @brief Get context type string
 */
const char* nimcp_rsc_context_type_string(nimcp_rsc_context_type_t type);

/**
 * @brief Get familiarity level string
 */
const char* nimcp_rsc_familiarity_string(nimcp_rsc_familiarity_t familiarity);

/**
 * @brief Get imagination mode string
 */
const char* nimcp_rsc_imagine_mode_string(nimcp_rsc_imagine_mode_t mode);

/**
 * @brief Get bio-async message type string
 */
const char* nimcp_rsc_bio_msg_type_string(nimcp_rsc_bio_msg_type_t type);

/**
 * @brief Get statistics
 */
nimcp_rsc_error_t nimcp_rsc_get_stats(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_stats_t* stats
);

/**
 * @brief Get configuration
 */
nimcp_rsc_error_t nimcp_rsc_get_config(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_config_t* config
);

/**
 * @brief Get health status (for brain KG)
 */
float nimcp_rsc_get_health_status(const nimcp_retrosplenial_t* rsc);

/**
 * @brief Log diagnostic information
 */
nimcp_rsc_error_t nimcp_rsc_log_diagnostics(const nimcp_retrosplenial_t* rsc);

/**
 * @brief Print summary to stdout
 */
void nimcp_rsc_print_summary(const nimcp_retrosplenial_t* rsc);

/*=============================================================================
 * THREAD SAFETY API
 *===========================================================================*/

/**
 * @brief Get RSC mutex
 *
 * @param rsc RSC instance
 * @return Mutex pointer, or NULL if not thread-safe
 */
nimcp_mutex_t* nimcp_rsc_get_mutex(nimcp_retrosplenial_t* rsc);

/**
 * @brief Lock RSC for exclusive access
 *
 * @param rsc RSC instance
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_lock(nimcp_retrosplenial_t* rsc);

/**
 * @brief Unlock RSC
 *
 * @param rsc RSC instance
 * @return RSC_OK on success
 */
nimcp_rsc_error_t nimcp_rsc_unlock(nimcp_retrosplenial_t* rsc);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RETROSPLENIAL_H */
