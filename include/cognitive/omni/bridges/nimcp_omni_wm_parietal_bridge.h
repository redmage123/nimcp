/**
 * @file nimcp_omni_wm_parietal_bridge.h
 * @brief World Model Parietal Bridge - Spatial/Physics Reasoning Integration
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with Parietal Cortex systems
 * WHY:  Enable physics-informed world modeling and spatial-aware predictions
 * HOW:  Integrate spatial attention, coordinate transforms, and physics constraints
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * SPATIAL WORLD MODELS (Milner & Goodale, 1995):
 * ----------------------------------------------
 * The parietal cortex maintains spatial representations in multiple reference
 * frames. The world model leverages these for:
 *
 *   1. EGOCENTRIC PREDICTIONS: Body-centered spatial forecasting
 *   2. ALLOCENTRIC PREDICTIONS: World-centered spatial reasoning
 *   3. COORDINATE TRANSFORMS: Convert between reference frames for prediction
 *
 * PHYSICS-INFORMED NEURAL NETWORKS (Raissi et al., 2019):
 * -------------------------------------------------------
 * Physical constraints improve prediction accuracy:
 *
 *   L_total = L_data + lambda * L_physics
 *
 * Where L_physics encodes:
 *   - Conservation laws (momentum, energy)
 *   - Geometric constraints (collision, containment)
 *   - Dynamical equations (F = ma, trajectory)
 *
 * SPATIAL ATTENTION GATING:
 * -------------------------
 * Parietal spatial attention gates which regions of state space receive
 * world model prediction resources:
 *
 *   prediction_weight[i] = spatial_attention[i] * salience[i]
 *
 * DATA FLOW:
 * ----------
 *   WM -> Parietal: Predicted spatial states, trajectory forecasts
 *   Parietal -> WM: Spatial attention, coordinate transforms, physics constraints
 *   Physics Engine <-> WM: Physical constraints on predictions
 *
 * INTEGRATION POINTS:
 * -------------------
 *   - Parietal Lobe (nimcp_parietal.h): Math/science/spatial reasoning
 *   - Parietal Adapter (nimcp_parietal_adapter.h): Sensorimotor integration
 *   - World Model (nimcp_omni_world_model.h): RSSM predictions
 *   - Spatial Reasoning (nimcp_spatial_reasoning.h): Coordinate transforms
 *
 * BIO-ASYNC:
 *   Module ID: 0x0E66
 *   Message Range: 0x6600-0x66FF
 */

#ifndef NIMCP_OMNI_WM_PARIETAL_BRIDGE_H
#define NIMCP_OMNI_WM_PARIETAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_messages.h"  /* BIO_MSG_WM_PARIETAL_* message types */
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* World Model (from nimcp_omni_world_model.h) */
typedef struct omni_world_model omni_world_model_t;

/* Parietal Lobe (from nimcp_parietal.h) - mathematical/scientific reasoning */
typedef struct parietal_lobe parietal_lobe_t;

/* Parietal Adapter (from nimcp_parietal_adapter.h) - sensorimotor/spatial */
typedef struct parietal_adapter parietal_adapter_t;

/* Spatial Reasoning (from nimcp_spatial_reasoning.h) */
typedef struct spatial_reasoning spatial_reasoning_t;

/* Physics Engine - internal physics simulator */
typedef struct wm_physics_engine wm_physics_engine_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for World Model Parietal Bridge */
#define BIO_MODULE_WM_PARIETAL_BRIDGE       0x0E66

/** Maximum spatial dimensions for coordinates */
#define WM_PARIETAL_MAX_SPATIAL_DIM         3

/** Maximum trajectory prediction horizon */
#define WM_PARIETAL_MAX_TRAJECTORY_HORIZON  64

/** Maximum number of tracked spatial objects */
#define WM_PARIETAL_MAX_TRACKED_OBJECTS     32

/** Maximum physics constraint count */
#define WM_PARIETAL_MAX_PHYSICS_CONSTRAINTS 16

/** Default physics time step (seconds) */
#define WM_PARIETAL_DEFAULT_PHYSICS_DT      0.01f

/** Default spatial attention resolution */
#define WM_PARIETAL_DEFAULT_ATTENTION_RES   8

/* ============================================================================
 * Bio-Async Message Types (0x6600-0x66FF)
 * ============================================================================
 * Message types are defined in nimcp_bio_messages.h to avoid duplication.
 * Key message types used by this bridge:
 *   - BIO_MSG_WM_PARIETAL_SPATIAL_PRED (0x6600): Spatial state prediction
 *   - BIO_MSG_WM_PARIETAL_COORD_TRANSFORM (0x6610): Coordinate transformation
 *   - BIO_MSG_WM_PARIETAL_PHYSICS_QUERY: Physics constraint query
 * ============================================================================ */

/** @brief Message type alias for Parietal bridge (uses bio_message_type_t from nimcp_bio_messages.h) */
typedef bio_message_type_t omni_wm_parietal_msg_type_t;

/* ============================================================================
 * Spatial Reference Frame Types
 * ============================================================================ */

/**
 * @brief Spatial reference frame enumeration
 *
 * WHAT: Define coordinate systems for spatial prediction
 * WHY:  Predictions must be frame-aware for motor planning
 * HOW:  Enumerate standard neuroscience reference frames
 */
typedef enum {
    WM_PARIETAL_FRAME_EGOCENTRIC    = 0,  /**< Body-centered coordinates */
    WM_PARIETAL_FRAME_ALLOCENTRIC   = 1,  /**< World-centered coordinates */
    WM_PARIETAL_FRAME_OBJECT        = 2,  /**< Object-relative coordinates */
    WM_PARIETAL_FRAME_RETINOTOPIC   = 3,  /**< Eye-centered coordinates */
    WM_PARIETAL_FRAME_HEAD          = 4,  /**< Head-centered coordinates */
    WM_PARIETAL_FRAME_HAND          = 5,  /**< Hand-centered (reaching) */
    WM_PARIETAL_FRAME_COUNT
} wm_parietal_frame_t;

/**
 * @brief Physics constraint type
 */
typedef enum {
    WM_PARIETAL_PHYSICS_GRAVITY         = 0,  /**< Gravitational acceleration */
    WM_PARIETAL_PHYSICS_COLLISION       = 1,  /**< Collision constraint */
    WM_PARIETAL_PHYSICS_MOMENTUM        = 2,  /**< Momentum conservation */
    WM_PARIETAL_PHYSICS_ENERGY          = 3,  /**< Energy conservation */
    WM_PARIETAL_PHYSICS_FRICTION        = 4,  /**< Surface friction */
    WM_PARIETAL_PHYSICS_BOUNDARY        = 5,  /**< Spatial boundary constraint */
    WM_PARIETAL_PHYSICS_CUSTOM          = 6   /**< Custom physics rule */
} wm_parietal_physics_type_t;

/* ============================================================================
 * Spatial State Structures
 * ============================================================================ */

/**
 * @brief 3D spatial position
 */
typedef struct {
    float x;                        /**< X coordinate */
    float y;                        /**< Y coordinate */
    float z;                        /**< Z coordinate */
} wm_parietal_vec3_t;

/**
 * @brief 3D velocity
 */
typedef struct {
    float vx;                       /**< Velocity X component */
    float vy;                       /**< Velocity Y component */
    float vz;                       /**< Velocity Z component */
} wm_parietal_velocity_t;

/**
 * @brief Quaternion for 3D orientation
 */
typedef struct {
    float w;                        /**< Scalar component */
    float x;                        /**< X component */
    float y;                        /**< Y component */
    float z;                        /**< Z component */
} wm_parietal_quaternion_t;

/**
 * @brief Full spatial state for an object
 *
 * WHAT: Complete 6DOF spatial state
 * WHY:  World model needs full state for trajectory prediction
 * HOW:  Position, velocity, orientation, angular velocity
 */
typedef struct {
    uint32_t object_id;             /**< Object identifier */
    wm_parietal_vec3_t position;    /**< 3D position */
    wm_parietal_velocity_t velocity;/**< 3D velocity */
    wm_parietal_quaternion_t orientation; /**< Orientation quaternion */
    wm_parietal_vec3_t angular_vel; /**< Angular velocity */
    float mass;                     /**< Object mass (kg) */
    float bounding_radius;          /**< Bounding sphere radius */
    wm_parietal_frame_t frame;      /**< Reference frame */
    float confidence;               /**< State estimation confidence */
    double timestamp;               /**< State timestamp */
} wm_parietal_spatial_state_t;

/**
 * @brief Predicted trajectory
 *
 * WHAT: Sequence of predicted spatial states
 * WHY:  Motor planning needs trajectory forecasts
 * HOW:  Array of states at regular time intervals
 */
typedef struct {
    uint32_t object_id;             /**< Object being tracked */
    wm_parietal_spatial_state_t* states; /**< Predicted state sequence */
    uint32_t length;                /**< Number of predicted states */
    float dt;                       /**< Time step between states */
    float total_duration;           /**< Total prediction duration */
    wm_parietal_frame_t frame;      /**< Reference frame */
    float overall_confidence;       /**< Trajectory confidence */
    bool physics_constrained;       /**< Physics constraints applied */
} wm_parietal_trajectory_t;

/**
 * @brief Physics constraint specification
 *
 * WHAT: Physical law or constraint for prediction
 * WHY:  Physics-informed predictions are more accurate
 * HOW:  Specify constraint type and parameters
 */
typedef struct {
    wm_parietal_physics_type_t type;/**< Constraint type */
    float parameters[8];            /**< Type-specific parameters */
    float strength;                 /**< Constraint enforcement strength */
    bool enabled;                   /**< Constraint active */
    uint32_t object_id;             /**< Specific object (0 = global) */
} wm_parietal_physics_constraint_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief World Model Parietal Bridge configuration
 *
 * WHAT: Parameters controlling WM-Parietal integration
 * WHY:  Tune spatial prediction, physics constraints, attention gating
 * HOW:  Configurable physics dt, attention resolution, prediction horizon
 */
typedef struct {
    /* General Settings */
    bool enable_modulation;             /**< Enable bidirectional modulation */
    float sensitivity;                  /**< General sensitivity [0.5-2.0] */

    /* Spatial Prediction Settings */
    bool enable_spatial_prediction;     /**< Enable spatial state predictions */
    uint32_t max_prediction_horizon;    /**< Max trajectory prediction steps */
    float prediction_dt;                /**< Prediction time step (seconds) */
    wm_parietal_frame_t default_frame;  /**< Default reference frame */
    bool enable_coordinate_transforms;  /**< Enable frame transformations */

    /* Physics Integration Settings */
    bool enable_physics_constraints;    /**< Apply physics to predictions */
    float physics_dt;                   /**< Physics simulation time step */
    float gravity_magnitude;            /**< Gravity strength (m/s^2) */
    bool enable_collision_prediction;   /**< Predict collisions */
    float collision_epsilon;            /**< Collision detection threshold */
    bool enable_momentum_conservation;  /**< Enforce momentum conservation */
    bool enable_energy_constraints;     /**< Apply energy constraints */

    /* Spatial Attention Settings */
    bool enable_attention_gating;       /**< Gate predictions by attention */
    uint32_t attention_resolution;      /**< Spatial attention grid resolution */
    float attention_decay_rate;         /**< Attention decay per timestep */
    float salience_threshold;           /**< Min salience for prediction */

    /* Mathematical Reasoning Settings */
    bool enable_math_reasoning;         /**< Enable parietal math integration */
    bool enable_pattern_extrapolation;  /**< Use pattern detection for prediction */
    bool enable_numerical_estimation;   /**< Use number sense for quantities */

    /* Training Settings */
    bool enable_physics_learning;       /**< Learn physics parameters */
    float physics_learning_rate;        /**< Learning rate for physics params */

    /* Bio-async Settings */
    bool enable_bio_async;              /**< Enable bio-async messaging */
} omni_wm_parietal_bridge_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief Effects from World Model to Parietal Systems
 *
 * WHAT: WM predictions flowing to parietal systems
 * WHY:  Provide predicted spatial states for motor planning
 * HOW:  Trajectory forecasts, collision predictions, state estimates
 */
typedef struct {
    /* Spatial Predictions */
    wm_parietal_spatial_state_t* predicted_states; /**< Predicted object states */
    uint32_t num_predicted_states;      /**< Number of predictions */

    /* Trajectory Forecasts */
    wm_parietal_trajectory_t** trajectories; /**< Predicted trajectories */
    uint32_t num_trajectories;          /**< Number of trajectories */

    /* Collision Predictions */
    bool collision_predicted;           /**< Collision detected in forecast */
    float time_to_collision;            /**< Time until collision (seconds) */
    uint32_t collision_object_a;        /**< First colliding object */
    uint32_t collision_object_b;        /**< Second colliding object */
    wm_parietal_vec3_t collision_point; /**< Predicted collision location */

    /* Physics Analysis */
    float total_kinetic_energy;         /**< Total KE in scene */
    float total_potential_energy;       /**< Total PE in scene */
    wm_parietal_vec3_t center_of_mass;  /**< Scene center of mass */
    wm_parietal_vec3_t total_momentum;  /**< Total linear momentum */

    /* Prediction Confidence */
    float overall_confidence;           /**< Overall prediction confidence */
    float physics_consistency;          /**< Physics consistency score */
    double prediction_timestamp;        /**< When predictions were made */
} omni_wm_to_parietal_effects_t;

/**
 * @brief Effects from Parietal Systems to World Model
 *
 * WHAT: Parietal information flowing to world model
 * WHY:  Provide spatial attention, transforms, and physics constraints
 * HOW:  Attention weights, coordinate transforms, constraint specs
 */
typedef struct {
    /* Spatial Attention */
    float* attention_map;               /**< Spatial attention weights */
    uint32_t attention_map_size;        /**< Attention map dimensions */
    wm_parietal_vec3_t attention_focus; /**< Current attention focus */
    float attention_spread;             /**< Attention distribution width */

    /* Coordinate Transform */
    wm_parietal_frame_t source_frame;   /**< Current source frame */
    wm_parietal_frame_t target_frame;   /**< Requested target frame */
    float transform_matrix[16];         /**< 4x4 transformation matrix */
    bool transform_valid;               /**< Transform matrix valid */

    /* Physics Constraints */
    wm_parietal_physics_constraint_t* constraints; /**< Active constraints */
    uint32_t num_constraints;           /**< Number of constraints */
    float constraint_violation;         /**< Total constraint violation */

    /* Object State Updates */
    wm_parietal_spatial_state_t* observed_states; /**< Observed object states */
    uint32_t num_observed_states;       /**< Number of observations */

    /* Mathematical Context */
    float estimated_quantity;           /**< Parietal quantity estimate */
    float quantity_confidence;          /**< Estimate confidence */
    bool pattern_detected;              /**< Spatial pattern detected */
    uint32_t pattern_type;              /**< Type of pattern detected */

    /* Sensorimotor Context */
    wm_parietal_vec3_t hand_position;   /**< Current hand position */
    wm_parietal_vec3_t gaze_direction;  /**< Current gaze direction */
    bool reaching_active;               /**< Reaching movement active */
    uint32_t reaching_target_id;        /**< Target of reach */
} parietal_to_omni_wm_effects_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief World Model Parietal Bridge statistics
 *
 * WHAT: Metrics for monitoring bridge operation
 * WHY:  Performance tracking, debugging, optimization
 * HOW:  Counters, averages, error metrics
 */
typedef struct {
    /* Spatial Prediction Statistics */
    uint64_t spatial_predictions_made;  /**< Total spatial predictions */
    uint64_t trajectory_predictions_made;/**< Total trajectory predictions */
    float mean_prediction_error;        /**< Average prediction error (m) */
    float mean_trajectory_error;        /**< Average trajectory error (m) */

    /* Coordinate Transform Statistics */
    uint64_t coordinate_transforms;     /**< Total transforms performed */
    float mean_transform_error;         /**< Average transform error */

    /* Physics Statistics */
    uint64_t physics_queries;           /**< Total physics queries */
    uint64_t collisions_predicted;      /**< Collisions predicted */
    uint64_t collisions_actual;         /**< Actual collisions observed */
    float physics_accuracy;             /**< Physics prediction accuracy */
    float mean_constraint_violation;    /**< Average constraint violation */

    /* Attention Statistics */
    uint64_t attention_updates;         /**< Attention map updates */
    uint64_t attention_shifts;          /**< Attention focus shifts */
    float mean_attention_coverage;      /**< Average attention coverage */

    /* Mathematical Reasoning Statistics */
    uint64_t math_predictions;          /**< Math-informed predictions */
    uint64_t pattern_extrapolations;    /**< Pattern-based extrapolations */
    float mean_estimation_error;        /**< Quantity estimation error */

    /* Timing Statistics */
    uint64_t total_updates;             /**< Total update cycles */
    double total_processing_time_ms;    /**< Total processing time */
    double mean_update_time_ms;         /**< Average update duration */
    uint64_t last_update_time_us;       /**< Last update timestamp */

    /* Error Statistics */
    uint64_t errors_total;              /**< Total errors encountered */
    uint64_t errors_physics;            /**< Physics-related errors */
    uint64_t errors_transform;          /**< Transform-related errors */
    uint64_t errors_attention;          /**< Attention-related errors */
} omni_wm_parietal_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief World Model Parietal Bridge
 *
 * WHAT: Main bridge structure connecting WM with parietal systems
 * WHY:  Orchestrates bidirectional spatial/physics information flow
 * HOW:  Maintains connections, effects, physics engine, and state
 *
 * Memory Layout:
 *   bridge_base_t base MUST be first for pointer casting compatibility
 */
typedef struct omni_wm_parietal_bridge {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    omni_wm_parietal_bridge_config_t config; /**< Bridge configuration */

    /* Connected Systems */
    omni_world_model_t* world_model;    /**< World model (RSSM) */
    parietal_lobe_t* parietal_lobe;     /**< Parietal math/science reasoning */
    parietal_adapter_t* parietal_adapter; /**< Parietal sensorimotor adapter */
    spatial_reasoning_t* spatial_reasoning; /**< Spatial reasoning module */
    wm_physics_engine_t* physics_engine; /**< Internal physics simulator */

    /* Bidirectional Effects */
    omni_wm_to_parietal_effects_t wm_to_parietal; /**< Effects: WM -> Parietal */
    parietal_to_omni_wm_effects_t parietal_to_wm; /**< Effects: Parietal -> WM */

    /* Tracked Objects */
    wm_parietal_spatial_state_t* tracked_objects; /**< Tracked object states */
    uint32_t num_tracked_objects;       /**< Number of tracked objects */
    uint32_t tracked_objects_capacity;  /**< Capacity for tracked objects */

    /* Physics Constraints */
    wm_parietal_physics_constraint_t* constraints; /**< Physics constraints */
    uint32_t num_constraints;           /**< Number of constraints */
    uint32_t constraints_capacity;      /**< Constraint array capacity */

    /* Spatial Attention */
    float* attention_map;               /**< Current attention map */
    uint32_t attention_map_dim;         /**< Attention map dimension */
    wm_parietal_vec3_t current_focus;   /**< Current attention focus */

    /* Trajectory Cache */
    wm_parietal_trajectory_t** trajectory_cache; /**< Cached trajectories */
    uint32_t trajectory_cache_size;     /**< Number of cached trajectories */
    uint32_t trajectory_cache_capacity; /**< Cache capacity */

    /* Statistics */
    omni_wm_parietal_bridge_stats_t stats; /**< Bridge statistics */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;     /**< Per-instance health agent */
} omni_wm_parietal_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Convenient initialization with biologically-plausible values
 * HOW:  Sets all config fields to defaults
 *
 * @param config Configuration structure to initialize
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_default_config(
    omni_wm_parietal_bridge_config_t* config);

/**
 * @brief Create World Model Parietal Bridge
 *
 * WHAT: Allocate and initialize bridge
 * WHY:  Required before connecting systems
 * HOW:  Allocate structure, initialize base, set config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
omni_wm_parietal_bridge_t* omni_wm_parietal_bridge_create(
    const omni_wm_parietal_bridge_config_t* config);

/**
 * @brief Destroy World Model Parietal Bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource management
 * HOW:  Disconnect systems, free buffers, cleanup base
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void omni_wm_parietal_bridge_destroy(omni_wm_parietal_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset effects and statistics, keep connections
 * WHY:  Allow fresh start without reconnection
 * HOW:  Zero effects, reset stats, preserve config
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_reset(omni_wm_parietal_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect all parietal systems to bridge
 *
 * WHAT: Establish connections to WM, parietal lobe, adapter, and spatial reasoning
 * WHY:  Single call to wire up all systems
 * HOW:  Store pointers, validate connections, activate bridge
 *
 * @param bridge Bridge instance
 * @param world_model World model (RSSM) - required
 * @param parietal_lobe Parietal lobe (math reasoning) - optional
 * @param parietal_adapter Parietal adapter (sensorimotor) - optional
 * @param spatial_reasoning Spatial reasoning module - optional
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_connect(
    omni_wm_parietal_bridge_t* bridge,
    omni_world_model_t* world_model,
    parietal_lobe_t* parietal_lobe,
    parietal_adapter_t* parietal_adapter,
    spatial_reasoning_t* spatial_reasoning);

/**
 * @brief Connect world model
 *
 * @param bridge Bridge instance
 * @param world_model World model to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_connect_world_model(
    omni_wm_parietal_bridge_t* bridge,
    omni_world_model_t* world_model);

/**
 * @brief Connect parietal lobe
 *
 * @param bridge Bridge instance
 * @param parietal_lobe Parietal lobe to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_connect_parietal_lobe(
    omni_wm_parietal_bridge_t* bridge,
    parietal_lobe_t* parietal_lobe);

/**
 * @brief Connect parietal adapter
 *
 * @param bridge Bridge instance
 * @param parietal_adapter Parietal adapter to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_connect_parietal_adapter(
    omni_wm_parietal_bridge_t* bridge,
    parietal_adapter_t* parietal_adapter);

/**
 * @brief Connect spatial reasoning
 *
 * @param bridge Bridge instance
 * @param spatial_reasoning Spatial reasoning module to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_connect_spatial_reasoning(
    omni_wm_parietal_bridge_t* bridge,
    spatial_reasoning_t* spatial_reasoning);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge to check
 * @return true if world model connected (minimum requirement)
 */
bool omni_wm_parietal_bridge_is_connected(const omni_wm_parietal_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main update cycle
 *
 * WHAT: Process bidirectional information flow
 * WHY:  Called each timestep to sync WM and parietal systems
 * HOW:  Gather parietal effects, compute WM predictions, apply physics
 *
 * @param bridge Bridge instance
 * @param dt Time delta in seconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_update(
    omni_wm_parietal_bridge_t* bridge,
    float dt);

/* ============================================================================
 * Spatial Prediction API
 * ============================================================================ */

/**
 * @brief Predict spatial state of object
 *
 * WHAT: Use world model to predict future spatial state
 * WHY:  Motor planning needs predicted object positions
 * HOW:  Query WM with current state, apply physics constraints
 *
 * @param bridge Bridge instance
 * @param object_id Object to predict
 * @param horizon_steps Number of steps to predict
 * @param target_frame Target reference frame
 * @param predicted_state Output: predicted state (caller allocates)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_predict_spatial_state(
    omni_wm_parietal_bridge_t* bridge,
    uint32_t object_id,
    uint32_t horizon_steps,
    wm_parietal_frame_t target_frame,
    wm_parietal_spatial_state_t* predicted_state);

/**
 * @brief Predict trajectory of object
 *
 * WHAT: Generate full trajectory prediction
 * WHY:  Reaching and grasping need trajectory forecasts
 * HOW:  Roll out WM predictions with physics constraints
 *
 * @param bridge Bridge instance
 * @param object_id Object to track
 * @param horizon_steps Number of steps to predict
 * @param dt Time step between predictions
 * @param target_frame Target reference frame
 * @param trajectory Output: predicted trajectory (caller allocates states array)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_predict_trajectory(
    omni_wm_parietal_bridge_t* bridge,
    uint32_t object_id,
    uint32_t horizon_steps,
    float dt,
    wm_parietal_frame_t target_frame,
    wm_parietal_trajectory_t* trajectory);

/**
 * @brief Predict multiple object trajectories jointly
 *
 * WHAT: Predict trajectories considering object interactions
 * WHY:  Multi-object scenes need joint predictions
 * HOW:  WM prediction with collision and physics constraints
 *
 * @param bridge Bridge instance
 * @param object_ids Array of object IDs
 * @param num_objects Number of objects
 * @param horizon_steps Prediction horizon
 * @param trajectories Output: array of trajectory pointers
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_predict_joint_trajectories(
    omni_wm_parietal_bridge_t* bridge,
    const uint32_t* object_ids,
    uint32_t num_objects,
    uint32_t horizon_steps,
    wm_parietal_trajectory_t** trajectories);

/* ============================================================================
 * Coordinate Transform API
 * ============================================================================ */

/**
 * @brief Transform position between reference frames
 *
 * WHAT: Convert coordinates between egocentric/allocentric/etc.
 * WHY:  WM predictions may be in different frame than needed
 * HOW:  Use parietal spatial reasoning for transformation
 *
 * @param bridge Bridge instance
 * @param position Input position
 * @param from_frame Source reference frame
 * @param to_frame Target reference frame
 * @param result Output: transformed position
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_transform_position(
    omni_wm_parietal_bridge_t* bridge,
    const wm_parietal_vec3_t* position,
    wm_parietal_frame_t from_frame,
    wm_parietal_frame_t to_frame,
    wm_parietal_vec3_t* result);

/**
 * @brief Transform full spatial state between frames
 *
 * WHAT: Convert complete state (position, velocity, orientation)
 * WHY:  Motor planning needs state in specific frame
 * HOW:  Apply frame transformation to all state components
 *
 * @param bridge Bridge instance
 * @param state Input state
 * @param to_frame Target reference frame
 * @param result Output: transformed state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_transform_state(
    omni_wm_parietal_bridge_t* bridge,
    const wm_parietal_spatial_state_t* state,
    wm_parietal_frame_t to_frame,
    wm_parietal_spatial_state_t* result);

/**
 * @brief Get transformation matrix between frames
 *
 * WHAT: Get 4x4 transformation matrix
 * WHY:  Allow batch transformations
 * HOW:  Query parietal spatial reasoning
 *
 * @param bridge Bridge instance
 * @param from_frame Source reference frame
 * @param to_frame Target reference frame
 * @param matrix Output: 4x4 transformation matrix (row-major)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_get_transform_matrix(
    omni_wm_parietal_bridge_t* bridge,
    wm_parietal_frame_t from_frame,
    wm_parietal_frame_t to_frame,
    float* matrix);

/* ============================================================================
 * Physics Constraint API
 * ============================================================================ */

/**
 * @brief Add physics constraint
 *
 * WHAT: Add physical law constraint to predictions
 * WHY:  Physics-informed predictions are more accurate
 * HOW:  Store constraint for application during prediction
 *
 * @param bridge Bridge instance
 * @param constraint Constraint specification
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_add_physics_constraint(
    omni_wm_parietal_bridge_t* bridge,
    const wm_parietal_physics_constraint_t* constraint);

/**
 * @brief Remove physics constraint
 *
 * WHAT: Remove a physics constraint
 * WHY:  Constraints may become invalid
 * HOW:  Remove from constraint list by type and object
 *
 * @param bridge Bridge instance
 * @param type Constraint type to remove
 * @param object_id Object ID (0 = global)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_remove_physics_constraint(
    omni_wm_parietal_bridge_t* bridge,
    wm_parietal_physics_type_t type,
    uint32_t object_id);

/**
 * @brief Clear all physics constraints
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_clear_physics_constraints(
    omni_wm_parietal_bridge_t* bridge);

/**
 * @brief Check for collision between objects
 *
 * WHAT: Predict if and when objects will collide
 * WHY:  Collision avoidance for motor planning
 * HOW:  Physics-based collision detection
 *
 * @param bridge Bridge instance
 * @param object_a First object ID
 * @param object_b Second object ID
 * @param horizon_seconds Time horizon for collision check
 * @param will_collide Output: true if collision predicted
 * @param time_to_collision Output: time until collision (if any)
 * @param collision_point Output: predicted collision point
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_check_collision(
    omni_wm_parietal_bridge_t* bridge,
    uint32_t object_a,
    uint32_t object_b,
    float horizon_seconds,
    bool* will_collide,
    float* time_to_collision,
    wm_parietal_vec3_t* collision_point);

/**
 * @brief Apply physics step to prediction
 *
 * WHAT: Advance physics simulation one step
 * WHY:  Generate physics-informed predictions
 * HOW:  Run physics engine with current constraints
 *
 * @param bridge Bridge instance
 * @param dt Time step (seconds)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_physics_step(
    omni_wm_parietal_bridge_t* bridge,
    float dt);

/* ============================================================================
 * Spatial Attention API
 * ============================================================================ */

/**
 * @brief Update spatial attention map
 *
 * WHAT: Set current spatial attention distribution
 * WHY:  Attention gates which predictions are computed
 * HOW:  Update internal attention map
 *
 * @param bridge Bridge instance
 * @param attention_map Attention weights (flattened grid)
 * @param map_dim Attention map dimension
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_update_attention(
    omni_wm_parietal_bridge_t* bridge,
    const float* attention_map,
    uint32_t map_dim);

/**
 * @brief Set attention focus point
 *
 * WHAT: Set center of spatial attention
 * WHY:  Focus predictions on attended region
 * HOW:  Update attention center and spread
 *
 * @param bridge Bridge instance
 * @param focus Attention focus point
 * @param spread Attention spread (sigma)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_set_attention_focus(
    omni_wm_parietal_bridge_t* bridge,
    const wm_parietal_vec3_t* focus,
    float spread);

/**
 * @brief Get attention weight for position
 *
 * WHAT: Query attention at specific location
 * WHY:  Check if position is attended
 * HOW:  Interpolate from attention map
 *
 * @param bridge Bridge instance
 * @param position Position to query
 * @return Attention weight [0, 1]
 */
float omni_wm_parietal_bridge_get_attention_at(
    const omni_wm_parietal_bridge_t* bridge,
    const wm_parietal_vec3_t* position);

/* ============================================================================
 * Object Tracking API
 * ============================================================================ */

/**
 * @brief Register object for tracking
 *
 * WHAT: Add object to tracked set
 * WHY:  Need to track objects for prediction
 * HOW:  Add to tracked objects array
 *
 * @param bridge Bridge instance
 * @param initial_state Initial object state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_track_object(
    omni_wm_parietal_bridge_t* bridge,
    const wm_parietal_spatial_state_t* initial_state);

/**
 * @brief Update tracked object state
 *
 * WHAT: Update state of tracked object
 * WHY:  Integrate new observations
 * HOW:  Update in tracked objects array
 *
 * @param bridge Bridge instance
 * @param object_id Object to update
 * @param new_state New observed state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_update_object(
    omni_wm_parietal_bridge_t* bridge,
    uint32_t object_id,
    const wm_parietal_spatial_state_t* new_state);

/**
 * @brief Remove object from tracking
 *
 * WHAT: Stop tracking an object
 * WHY:  Object no longer relevant
 * HOW:  Remove from tracked objects
 *
 * @param bridge Bridge instance
 * @param object_id Object to remove
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_untrack_object(
    omni_wm_parietal_bridge_t* bridge,
    uint32_t object_id);

/**
 * @brief Get current state of tracked object
 *
 * WHAT: Query current tracked state
 * WHY:  Get latest state estimate
 * HOW:  Look up in tracked objects
 *
 * @param bridge Bridge instance
 * @param object_id Object to query
 * @param state Output: current state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_get_object_state(
    const omni_wm_parietal_bridge_t* bridge,
    uint32_t object_id,
    wm_parietal_spatial_state_t* state);

/* ============================================================================
 * Mathematical Reasoning API
 * ============================================================================ */

/**
 * @brief Request math-informed prediction
 *
 * WHAT: Use parietal mathematical reasoning for prediction
 * WHY:  Mathematical patterns improve extrapolation
 * HOW:  Query parietal lobe for pattern-based prediction
 *
 * @param bridge Bridge instance
 * @param observation_sequence Sequence of observed values
 * @param sequence_length Length of observation sequence
 * @param prediction_steps Number of steps to predict
 * @param predictions Output: predicted values
 * @param confidence Output: prediction confidence
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_math_predict(
    omni_wm_parietal_bridge_t* bridge,
    const float* observation_sequence,
    uint32_t sequence_length,
    uint32_t prediction_steps,
    float* predictions,
    float* confidence);

/**
 * @brief Estimate quantity using number sense
 *
 * WHAT: Use parietal number sense for quantity estimation
 * WHY:  Quick approximate numerical reasoning
 * HOW:  Query parietal number sense module
 *
 * @param bridge Bridge instance
 * @param values Values to estimate from
 * @param num_values Number of values
 * @param estimate Output: quantity estimate
 * @param confidence Output: estimate confidence
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_estimate_quantity(
    omni_wm_parietal_bridge_t* bridge,
    const float* values,
    uint32_t num_values,
    float* estimate,
    float* confidence);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effects from WM to parietal
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const omni_wm_to_parietal_effects_t* omni_wm_parietal_bridge_get_wm_effects(
    const omni_wm_parietal_bridge_t* bridge);

/**
 * @brief Get current effects from parietal to WM
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const parietal_to_omni_wm_effects_t* omni_wm_parietal_bridge_get_parietal_effects(
    const omni_wm_parietal_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_get_stats(
    const omni_wm_parietal_bridge_t* bridge,
    omni_wm_parietal_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_reset_stats(
    omni_wm_parietal_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_connect_bio_async(
    omni_wm_parietal_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_bridge_disconnect_bio_async(
    omni_wm_parietal_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool omni_wm_parietal_bridge_is_bio_async_connected(
    const omni_wm_parietal_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get message type name string
 *
 * @param msg_type Message type
 * @return Human-readable message name
 */
const char* omni_wm_parietal_msg_type_to_string(omni_wm_parietal_msg_type_t msg_type);

/**
 * @brief Get reference frame name string
 *
 * @param frame Reference frame
 * @return Human-readable frame name
 */
const char* omni_wm_parietal_frame_to_string(wm_parietal_frame_t frame);

/**
 * @brief Get physics constraint type name string
 *
 * @param type Physics constraint type
 * @return Human-readable constraint name
 */
const char* omni_wm_parietal_physics_type_to_string(wm_parietal_physics_type_t type);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS or error code describing issue
 */
nimcp_error_t omni_wm_parietal_bridge_validate_config(
    const omni_wm_parietal_bridge_config_t* config);

/**
 * @brief Create spatial state
 *
 * @param object_id Object ID
 * @param x X position
 * @param y Y position
 * @param z Z position
 * @param frame Reference frame
 * @return Initialized spatial state
 */
wm_parietal_spatial_state_t omni_wm_parietal_create_state(
    uint32_t object_id,
    float x, float y, float z,
    wm_parietal_frame_t frame);

/**
 * @brief Create trajectory structure
 *
 * @param max_length Maximum trajectory length
 * @return Allocated trajectory or NULL
 */
wm_parietal_trajectory_t* omni_wm_parietal_trajectory_create(uint32_t max_length);

/**
 * @brief Destroy trajectory structure
 *
 * @param trajectory Trajectory to free
 */
void omni_wm_parietal_trajectory_destroy(wm_parietal_trajectory_t* trajectory);

/**
 * @brief Compute distance between positions
 *
 * @param a First position
 * @param b Second position
 * @return Euclidean distance
 */
float omni_wm_parietal_distance(
    const wm_parietal_vec3_t* a,
    const wm_parietal_vec3_t* b);

/**
 * @brief Normalize vector
 *
 * @param v Vector to normalize
 * @param result Output: normalized vector
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_parietal_normalize(
    const wm_parietal_vec3_t* v,
    wm_parietal_vec3_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_PARIETAL_BRIDGE_H */
