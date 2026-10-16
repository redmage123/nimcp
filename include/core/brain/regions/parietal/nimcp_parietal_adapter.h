/**
 * @file nimcp_parietal_adapter.h
 * @brief Brain adapter for Parietal Cortex integration
 *
 * WHAT: Unified adapter connecting parietal cortex sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, training, and event system
 * HOW:  Orchestrates somatosensory, spatial attention, and sensorimotor processors
 *
 * ARCHITECTURE:
 * - Wraps parietal sub-modules (somatosensory S1/S2, spatial attention, sensorimotor)
 * - Provides high-level API for spatial processing pipeline
 * - Integrates with working memory for spatial representations
 * - Connects to event bus for inter-module communication
 * - Supports training through backpropagation adapters
 *
 * BIOLOGICAL BASIS:
 * - Models posterior parietal cortex (PPC) Brodmann areas 5, 7
 * - Primary somatosensory cortex (S1) - area 3a, 3b, 1, 2
 * - Secondary somatosensory cortex (S2) - area 40
 * - Superior parietal lobule (SPL) - spatial attention and navigation
 * - Inferior parietal lobule (IPL) - sensorimotor integration
 * - Intraparietal sulcus (IPS) - reaching, grasping, attention
 * - Connections to motor cortex, visual cortex, prefrontal cortex
 *
 * NAMING CONVENTION:
 * All types and functions are prefixed with "parietal_cortex_" to distinguish
 * from the cognitive parietal lobe module (nimcp_parietal.h) which handles
 * math/science reasoning. This module handles spatial/sensorimotor processing.
 *
 * @version Phase PC1: Parietal Cortex Brain Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_PARIETAL_ADAPTER_H
#define NIMCP_PARIETAL_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Logging system */
#include "utils/logging/nimcp_logging.h"

/* Unified memory system */
#include "utils/memory/nimcp_unified_memory.h"

/* Forward declarations for sub-modules */
typedef struct parietal_cortex_somatosensory_processor parietal_cortex_somatosensory_processor_t;
typedef struct parietal_cortex_spatial_attention_processor parietal_cortex_spatial_attention_processor_t;
typedef struct parietal_cortex_sensorimotor_integrator parietal_cortex_sensorimotor_integrator_t;

/* Forward declaration for opaque adapter type */
typedef struct parietal_adapter parietal_adapter_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define PARIETAL_CORTEX_DEFAULT_MAX_SOMATOTOPIC_REGIONS    64
#define PARIETAL_CORTEX_DEFAULT_MAX_SPATIAL_TARGETS        128
#define PARIETAL_CORTEX_DEFAULT_MAX_MOTOR_PLANS            256
#define PARIETAL_CORTEX_DEFAULT_RECEPTIVE_FIELD_SIZE       32
#define PARIETAL_CORTEX_DEFAULT_ATTENTION_RESOLUTION       8
#define PARIETAL_CORTEX_DEFAULT_INTEGRATION_WINDOW_MS      50.0f

/**
 * @brief Somatosensory modality types (S1/S2 processing)
 */
typedef enum {
    PARIETAL_CORTEX_SOMATOSENSORY_TOUCH = 0,         /**< Tactile/pressure sensing */
    PARIETAL_CORTEX_SOMATOSENSORY_PROPRIOCEPTION,    /**< Body position sensing */
    PARIETAL_CORTEX_SOMATOSENSORY_TEMPERATURE,       /**< Thermal sensing */
    PARIETAL_CORTEX_SOMATOSENSORY_PAIN,              /**< Nociception */
    PARIETAL_CORTEX_SOMATOSENSORY_VIBRATION,         /**< Vibrotactile sensing */
    PARIETAL_CORTEX_SOMATOSENSORY_PRESSURE,          /**< Deep pressure */
    PARIETAL_CORTEX_SOMATOSENSORY_KINESTHESIA,       /**< Movement sensing */
    PARIETAL_CORTEX_SOMATOSENSORY_COUNT
} parietal_cortex_somatosensory_modality_t;

/**
 * @brief Spatial reference frames
 */
typedef enum {
    PARIETAL_CORTEX_SPATIAL_FRAME_EGOCENTRIC = 0,    /**< Body-centered coordinates */
    PARIETAL_CORTEX_SPATIAL_FRAME_ALLOCENTRIC,       /**< World-centered coordinates */
    PARIETAL_CORTEX_SPATIAL_FRAME_OBJECT_CENTERED,   /**< Object-relative coordinates */
    PARIETAL_CORTEX_SPATIAL_FRAME_RETINOTOPIC,       /**< Eye-centered coordinates */
    PARIETAL_CORTEX_SPATIAL_FRAME_HEAD_CENTERED,     /**< Head-centered coordinates */
    PARIETAL_CORTEX_SPATIAL_FRAME_HAND_CENTERED,     /**< Hand-centered (reaching) */
    PARIETAL_CORTEX_SPATIAL_FRAME_COUNT
} parietal_cortex_spatial_frame_t;

/**
 * @brief Parietal cortex adapter configuration
 */
typedef struct {
    /* Capacity limits */
    uint32_t max_somatotopic_regions;  /**< Maximum somatotopic map regions */
    uint32_t max_spatial_targets;      /**< Maximum tracked spatial targets */
    uint32_t max_motor_plans;          /**< Maximum motor plan candidates */

    /* Somatosensory processing */
    uint32_t receptive_field_size;     /**< Size of receptive fields */
    bool enable_tactile_acuity;        /**< Enable high-resolution touch */
    bool enable_proprioception;        /**< Enable body position sensing */
    bool enable_two_point_discrimination; /**< Enable 2-point discrimination */

    /* Spatial attention */
    uint32_t attention_resolution;     /**< Spatial attention grid resolution */
    bool enable_covert_attention;      /**< Enable covert spatial attention */
    bool enable_attention_shifting;    /**< Enable attention saccades */
    bool enable_spatial_neglect_model; /**< Model hemispatial neglect */

    /* Sensorimotor integration */
    bool enable_reaching;              /**< Enable reach planning */
    bool enable_grasping;              /**< Enable grasp planning */
    bool enable_tool_use;              /**< Enable tool representations */
    bool enable_coordinate_transforms; /**< Enable frame transformations */

    /* Event system */
    bool enable_events;                /**< Enable event bus integration */

    /* Training */
    bool enable_training;              /**< Enable learning capabilities */
    float learning_rate;               /**< Base learning rate */

    /* Timing */
    float integration_window_ms;       /**< Sensorimotor integration window */

    /* Bio-async communication */
    bool enable_bio_async;             /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
} parietal_cortex_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Processing status of the adapter
 */
typedef enum {
    PARIETAL_CORTEX_STATUS_IDLE = 0,            /**< Ready for input */
    PARIETAL_CORTEX_STATUS_SOMATOSENSORY,       /**< Processing touch/body input */
    PARIETAL_CORTEX_STATUS_SPATIAL_ATTENTION,   /**< Allocating spatial attention */
    PARIETAL_CORTEX_STATUS_SENSORIMOTOR,        /**< Integrating for motor planning */
    PARIETAL_CORTEX_STATUS_COORDINATE_TRANSFORM,/**< Transforming reference frames */
    PARIETAL_CORTEX_STATUS_READY,               /**< Output ready for retrieval */
    PARIETAL_CORTEX_STATUS_ERROR                /**< Error state */
} parietal_cortex_status_t;

/**
 * @brief Error codes for parietal cortex operations
 */
typedef enum {
    PARIETAL_CORTEX_ERROR_NONE = 0,
    PARIETAL_CORTEX_ERROR_INVALID_INPUT,
    PARIETAL_CORTEX_ERROR_SOMATOSENSORY_FAILURE,
    PARIETAL_CORTEX_ERROR_SPATIAL_FAILURE,
    PARIETAL_CORTEX_ERROR_SENSORIMOTOR_FAILURE,
    PARIETAL_CORTEX_ERROR_COORDINATE_FAILURE,
    PARIETAL_CORTEX_ERROR_ATTENTION_FAILURE,
    PARIETAL_CORTEX_ERROR_BUFFER_OVERFLOW,
    PARIETAL_CORTEX_ERROR_INTERNAL
} parietal_cortex_error_t;

/*=============================================================================
 * INPUT/OUTPUT STRUCTURES
 *===========================================================================*/

/**
 * @brief 3D spatial position
 */
typedef struct {
    float x;                     /**< X coordinate */
    float y;                     /**< Y coordinate */
    float z;                     /**< Z coordinate */
} parietal_cortex_position_t;

/**
 * @brief 3D orientation (quaternion)
 */
typedef struct {
    float w;                     /**< Scalar component */
    float x;                     /**< X component */
    float y;                     /**< Y component */
    float z;                     /**< Z component */
} parietal_cortex_orientation_t;

/**
 * @brief Somatosensory input sample
 */
typedef struct {
    parietal_cortex_somatosensory_modality_t modality; /**< Sensory modality */
    uint32_t body_region_id;                    /**< Somatotopic map region */
    parietal_cortex_position_t location;        /**< Body location */
    float intensity;                            /**< Stimulus intensity [0, 1] */
    float duration_ms;                          /**< Stimulus duration */
    double timestamp_ms;                        /**< When received */
} parietal_cortex_somatosensory_input_t;

/**
 * @brief Spatial target for attention
 */
typedef struct {
    uint32_t target_id;                      /**< Unique target identifier */
    parietal_cortex_position_t position;     /**< Target position */
    parietal_cortex_spatial_frame_t frame;   /**< Reference frame */
    float salience;                          /**< Attention priority [0, 1] */
    float size;                              /**< Target extent */
    bool is_active;                          /**< Currently attended */
} parietal_cortex_spatial_target_t;

/**
 * @brief Motor plan output (for reaching/grasping)
 */
typedef struct {
    uint32_t plan_id;                        /**< Motor plan identifier */
    parietal_cortex_position_t start_pos;    /**< Start position (hand) */
    parietal_cortex_position_t target_pos;   /**< Target position */
    parietal_cortex_orientation_t target_grip; /**< Grip orientation for grasping */
    float trajectory_duration_ms;            /**< Planned movement duration */
    float confidence;                        /**< Plan confidence [0, 1] */
    bool requires_grasp;                     /**< Is this a grasp plan? */
    uint8_t grip_aperture;                   /**< Grip width (0-255) */
} parietal_cortex_motor_plan_t;

/**
 * @brief Spatial attention result
 */
typedef struct {
    parietal_cortex_spatial_target_t* attended_targets; /**< Attended targets */
    uint32_t num_attended;                       /**< Number attended */
    float attention_map[64];                     /**< 8x8 spatial attention map */
    parietal_cortex_position_t focus_center;     /**< Center of attention */
    float focus_spread;                          /**< Attention spread (sigma) */
    bool left_hemifield_active;                  /**< Left visual field attention */
    bool right_hemifield_active;                 /**< Right visual field attention */
} parietal_cortex_attention_result_t;

/**
 * @brief Sensorimotor integration result
 */
typedef struct {
    /* Somatosensory summary */
    bool has_touch_input;            /**< Touch detected */
    bool has_proprioceptive_input;   /**< Body position detected */
    uint32_t active_body_regions;    /**< Number of active regions */

    /* Spatial attention summary */
    uint32_t attended_targets;       /**< Number of attended targets */
    parietal_cortex_spatial_frame_t current_frame; /**< Active reference frame */

    /* Motor planning summary */
    uint32_t motor_plan_count;       /**< Number of motor plans */
    bool ready_for_execution;        /**< Plans ready for motor cortex */
    float integration_confidence;    /**< Overall integration quality */

    /* Timing */
    float processing_latency_ms;     /**< Processing time */
} parietal_cortex_integration_result_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Adapter statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t somatosensory_samples;      /**< Total somatosensory inputs */
    uint64_t spatial_targets_tracked;    /**< Total spatial targets */
    uint64_t motor_plans_generated;      /**< Total motor plans */
    uint64_t coordinate_transforms;      /**< Total frame transforms */

    /* Success/failure */
    uint64_t successful_integrations;    /**< Successful processing */
    uint64_t somatosensory_errors;       /**< Somatosensory failures */
    uint64_t spatial_errors;             /**< Spatial attention failures */
    uint64_t sensorimotor_errors;        /**< Sensorimotor failures */
    uint64_t attention_shifts;           /**< Attention reallocation events */

    /* Timing */
    float avg_latency_ms;                /**< Average processing latency */
    float max_latency_ms;                /**< Maximum latency observed */

    /* Training */
    uint64_t training_iterations;        /**< Training updates */
    float training_loss;                 /**< Current training loss */
} parietal_cortex_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for motor output (integration with motor cortex)
 */
typedef void (*parietal_cortex_motor_callback_t)(
    const parietal_cortex_motor_plan_t* plan,
    void* user_data
);

/**
 * @brief Callback for attention shifts (integration with visual cortex)
 */
typedef void (*parietal_cortex_attention_callback_t)(
    const parietal_cortex_attention_result_t* attention,
    void* user_data
);

/**
 * @brief Callback for event notification
 */
typedef void (*parietal_cortex_event_callback_t)(
    uint32_t event_type,
    const void* event_data,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration for parietal cortex adapter
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Initialize all fields with biologically-motivated values
 *
 * @return Default configuration structure
 */
parietal_cortex_config_t parietal_cortex_adapter_default_config(void);

/**
 * @brief Create parietal cortex adapter
 *
 * WHAT: Allocate and initialize the adapter with all sub-modules
 * WHY:  Central point for spatial processing initialization
 * HOW:  Create somatosensory, spatial attention, and sensorimotor processors
 *
 * @param config Configuration (NULL for defaults)
 * @return New adapter instance, or NULL on failure
 */
parietal_adapter_t* parietal_cortex_adapter_create(const parietal_cortex_config_t* config);

/**
 * @brief Destroy parietal cortex adapter
 *
 * WHAT: Free all resources associated with the adapter
 * WHY:  Prevent memory leaks
 * HOW:  Destroy sub-modules, free buffers
 *
 * @param adapter Adapter to destroy
 */
void parietal_cortex_adapter_destroy(parietal_adapter_t* adapter);

/**
 * @brief Reset adapter state
 *
 * WHAT: Clear buffers and reset to idle state
 * WHY:  Prepare for new processing without full reinitialization
 * HOW:  Reset all sub-modules, clear attention map
 *
 * @param adapter Adapter instance
 * @return true on success, false on failure
 */
bool parietal_cortex_adapter_reset(parietal_adapter_t* adapter);

/*=============================================================================
 * SOMATOSENSORY PROCESSING (S1/S2)
 *===========================================================================*/

/**
 * @brief Add somatosensory input sample
 *
 * WHAT: Process touch/proprioception input
 * WHY:  Build tactile and body representation
 * HOW:  Update somatotopic map, detect patterns
 *
 * @param adapter Adapter instance
 * @param input Somatosensory sample
 * @return true on success, false if buffer full
 */
bool parietal_cortex_add_somatosensory_input(parietal_adapter_t* adapter,
                                              const parietal_cortex_somatosensory_input_t* input);

/**
 * @brief Get current body region activation
 *
 * WHAT: Retrieve activation level for a body region
 * WHY:  Query current somatotopic state
 * HOW:  Look up in somatotopic map
 *
 * @param adapter Adapter instance
 * @param region_id Body region identifier
 * @return Activation level [0, 1], or -1 on error
 */
float parietal_cortex_get_body_region_activation(const parietal_adapter_t* adapter,
                                                  uint32_t region_id);

/**
 * @brief Perform two-point discrimination test
 *
 * WHAT: Test tactile acuity at a body location
 * WHY:  Measure sensory resolution
 * HOW:  Apply spatial frequency discrimination
 *
 * @param adapter Adapter instance
 * @param region_id Body region to test
 * @param separation_mm Distance between points
 * @return true if discriminable, false otherwise
 */
bool parietal_cortex_two_point_discrimination(const parietal_adapter_t* adapter,
                                               uint32_t region_id,
                                               float separation_mm);

/*=============================================================================
 * SPATIAL ATTENTION AND NAVIGATION
 *===========================================================================*/

/**
 * @brief Add spatial target for tracking
 *
 * WHAT: Register target in spatial attention system
 * WHY:  Build scene representation
 * HOW:  Add to spatial target buffer
 *
 * @param adapter Adapter instance
 * @param target Spatial target specification
 * @return true on success, false if buffer full
 */
bool parietal_cortex_add_spatial_target(parietal_adapter_t* adapter,
                                         const parietal_cortex_spatial_target_t* target);

/**
 * @brief Update spatial target position
 *
 * WHAT: Modify position of tracked target
 * WHY:  Handle moving targets
 * HOW:  Update target buffer entry
 *
 * @param adapter Adapter instance
 * @param target_id Target to update
 * @param new_position New position
 * @return true on success, false if target not found
 */
bool parietal_cortex_update_target_position(parietal_adapter_t* adapter,
                                             uint32_t target_id,
                                             const parietal_cortex_position_t* new_position);

/**
 * @brief Allocate spatial attention
 *
 * WHAT: Shift attention to location/target
 * WHY:  Enable spatial selection
 * HOW:  Update attention spotlight
 *
 * @param adapter Adapter instance
 * @param target_id Target to attend (0 = use position)
 * @param position Position to attend (if target_id = 0)
 * @return true on success
 */
bool parietal_cortex_attend_to_location(parietal_adapter_t* adapter,
                                         uint32_t target_id,
                                         const parietal_cortex_position_t* position);

/**
 * @brief Perform covert attention shift
 *
 * WHAT: Shift attention without eye movement
 * WHY:  Enable attention saccades
 * HOW:  Recompute attention map
 *
 * @param adapter Adapter instance
 * @param new_focus New attention focus position
 * @param transition_ms Transition duration
 * @return true on success
 */
bool parietal_cortex_covert_attention_shift(parietal_adapter_t* adapter,
                                             const parietal_cortex_position_t* new_focus,
                                             float transition_ms);

/**
 * @brief Get current attention map
 *
 * WHAT: Retrieve spatial attention distribution
 * WHY:  Query current attention state
 * HOW:  Copy attention map to output
 *
 * @param adapter Adapter instance
 * @param result Output attention result structure
 * @return true on success
 */
bool parietal_cortex_get_attention_map(const parietal_adapter_t* adapter,
                                        parietal_cortex_attention_result_t* result);

/**
 * @brief Transform coordinates between reference frames
 *
 * WHAT: Convert position from one frame to another
 * WHY:  Enable multi-frame spatial reasoning
 * HOW:  Apply learned frame transformation
 *
 * @param adapter Adapter instance
 * @param position Input position
 * @param from_frame Source reference frame
 * @param to_frame Target reference frame
 * @param result Output transformed position
 * @return true on success, false on error
 */
bool parietal_cortex_transform_coordinates(parietal_adapter_t* adapter,
                                            const parietal_cortex_position_t* position,
                                            parietal_cortex_spatial_frame_t from_frame,
                                            parietal_cortex_spatial_frame_t to_frame,
                                            parietal_cortex_position_t* result);

/*=============================================================================
 * SENSORIMOTOR INTEGRATION
 *===========================================================================*/

/**
 * @brief Plan reaching movement
 *
 * WHAT: Generate motor plan for reaching target
 * WHY:  Prepare motor commands for motor cortex
 * HOW:  Compute optimal trajectory in egocentric space
 *
 * @param adapter Adapter instance
 * @param target_pos Target position to reach
 * @param hand_id Which hand (0 = left, 1 = right)
 * @param plan Output motor plan
 * @return true on success
 */
bool parietal_cortex_plan_reach(parietal_adapter_t* adapter,
                                 const parietal_cortex_position_t* target_pos,
                                 uint8_t hand_id,
                                 parietal_cortex_motor_plan_t* plan);

/**
 * @brief Plan grasping action
 *
 * WHAT: Generate motor plan for grasping object
 * WHY:  Compute grip parameters for object manipulation
 * HOW:  Estimate object size, compute grip aperture
 *
 * @param adapter Adapter instance
 * @param target_pos Target object position
 * @param object_size Estimated object size
 * @param plan Output motor plan with grip parameters
 * @return true on success
 */
bool parietal_cortex_plan_grasp(parietal_adapter_t* adapter,
                                 const parietal_cortex_position_t* target_pos,
                                 float object_size,
                                 parietal_cortex_motor_plan_t* plan);

/**
 * @brief Process complete sensorimotor integration
 *
 * WHAT: Run full integration pipeline
 * WHY:  Generate motor commands from sensory input
 * HOW:  Process somatosensory, allocate attention, plan motor
 *
 * @param adapter Adapter instance
 * @param result Output integration result (optional)
 * @return true on success, false on any pipeline failure
 */
bool parietal_cortex_process_integration(parietal_adapter_t* adapter,
                                          parietal_cortex_integration_result_t* result);

/**
 * @brief Get next motor plan
 *
 * WHAT: Retrieve next motor plan from output queue
 * WHY:  Feed motor cortex incrementally
 * HOW:  Pop from plan queue
 *
 * @param adapter Adapter instance
 * @param plan Output motor plan (filled on success)
 * @return true if plan available, false if queue empty
 */
bool parietal_cortex_get_next_motor_plan(parietal_adapter_t* adapter,
                                          parietal_cortex_motor_plan_t* plan);

/*=============================================================================
 * CALLBACKS AND EVENT INTEGRATION
 *===========================================================================*/

/**
 * @brief Set motor output callback
 *
 * WHAT: Register callback for motor plan output
 * WHY:  Allow integration with motor cortex
 * HOW:  Store callback, invoke on new plans
 *
 * @param adapter Adapter instance
 * @param callback Motor plan handler function
 * @param user_data User context passed to callback
 * @return true on success
 */
bool parietal_cortex_set_motor_callback(parietal_adapter_t* adapter,
                                         parietal_cortex_motor_callback_t callback,
                                         void* user_data);

/**
 * @brief Set attention callback
 *
 * WHAT: Register callback for attention shifts
 * WHY:  Allow integration with visual cortex
 * HOW:  Store callback, invoke on attention changes
 *
 * @param adapter Adapter instance
 * @param callback Attention handler function
 * @param user_data User context passed to callback
 * @return true on success
 */
bool parietal_cortex_set_attention_callback(parietal_adapter_t* adapter,
                                             parietal_cortex_attention_callback_t callback,
                                             void* user_data);

/**
 * @brief Set event callback
 *
 * WHAT: Register callback for event notification
 * WHY:  Allow external monitoring
 * HOW:  Store callback, invoke on events
 *
 * @param adapter Adapter instance
 * @param callback Event handler function
 * @param user_data User context passed to callback
 * @return true on success
 */
bool parietal_cortex_set_event_callback(parietal_adapter_t* adapter,
                                         parietal_cortex_event_callback_t callback,
                                         void* user_data);

/*=============================================================================
 * TRAINING INTERFACE
 *===========================================================================*/

/**
 * @brief Train coordinate transformation
 *
 * WHAT: Learn frame transformation from examples
 * WHY:  Improve frame conversion accuracy
 * HOW:  Update transformation weights
 *
 * @param adapter Adapter instance
 * @param from_frame Source frame
 * @param to_frame Target frame
 * @param input_pos Input position
 * @param target_pos Expected output position
 * @param learning_rate Learning rate (0 = use default)
 * @return true on success
 */
bool parietal_cortex_train_transform(parietal_adapter_t* adapter,
                                      parietal_cortex_spatial_frame_t from_frame,
                                      parietal_cortex_spatial_frame_t to_frame,
                                      const parietal_cortex_position_t* input_pos,
                                      const parietal_cortex_position_t* target_pos,
                                      float learning_rate);

/**
 * @brief Train reaching controller
 *
 * WHAT: Improve reaching accuracy from feedback
 * WHY:  Motor learning from error signals
 * HOW:  Update inverse kinematics model
 *
 * @param adapter Adapter instance
 * @param plan Original motor plan
 * @param actual_endpoint Where the reach actually ended
 * @param learning_rate Learning rate (0 = use default)
 * @return true on success
 */
bool parietal_cortex_train_reaching(parietal_adapter_t* adapter,
                                     const parietal_cortex_motor_plan_t* plan,
                                     const parietal_cortex_position_t* actual_endpoint,
                                     float learning_rate);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current processing status
 *
 * @param adapter Adapter instance
 * @return Current status
 */
parietal_cortex_status_t parietal_cortex_get_status(const parietal_adapter_t* adapter);

/**
 * @brief Get last error code
 *
 * @param adapter Adapter instance
 * @return Last error, or PARIETAL_CORTEX_ERROR_NONE
 */
parietal_cortex_error_t parietal_cortex_get_last_error(const parietal_adapter_t* adapter);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* parietal_cortex_error_string(parietal_cortex_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable status description
 */
const char* parietal_cortex_status_string(parietal_cortex_status_t status);

/**
 * @brief Get adapter statistics
 *
 * @param adapter Adapter instance
 * @param stats Output statistics structure
 * @return true on success
 */
bool parietal_cortex_get_stats(const parietal_adapter_t* adapter, parietal_cortex_stats_t* stats);

/**
 * @brief Get adapter configuration
 *
 * @param adapter Adapter instance
 * @param config Output configuration structure
 * @return true on success
 */
bool parietal_cortex_get_config(const parietal_adapter_t* adapter, parietal_cortex_config_t* config);

/*=============================================================================
 * SUB-MODULE ACCESS (Advanced)
 *===========================================================================*/

/**
 * @brief Get somatosensory processor handle
 *
 * @param adapter Adapter instance
 * @return Somatosensory processor, or NULL
 */
parietal_cortex_somatosensory_processor_t* parietal_cortex_get_somatosensory_processor(parietal_adapter_t* adapter);

/**
 * @brief Get spatial attention processor handle
 *
 * @param adapter Adapter instance
 * @return Spatial attention processor, or NULL
 */
parietal_cortex_spatial_attention_processor_t* parietal_cortex_get_spatial_attention_processor(parietal_adapter_t* adapter);

/**
 * @brief Get sensorimotor integrator handle
 *
 * @param adapter Adapter instance
 * @return Sensorimotor integrator, or NULL
 */
parietal_cortex_sensorimotor_integrator_t* parietal_cortex_get_sensorimotor_integrator(parietal_adapter_t* adapter);

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Get bio-async module context
 *
 * @param adapter Adapter instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t parietal_cortex_get_bio_context(parietal_adapter_t* adapter);

/**
 * @brief Process pending bio-async messages
 *
 * @param adapter Adapter instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t parietal_cortex_process_bio_messages(parietal_adapter_t* adapter, uint32_t max_messages);

/**
 * @brief Request coordinate transform asynchronously
 *
 * @param adapter Adapter instance
 * @param position Position to transform
 * @param from_frame Source frame
 * @param to_frame Target frame
 * @return Future for transform result, or NULL on failure
 */
nimcp_bio_future_t parietal_cortex_request_transform_async(
    parietal_adapter_t* adapter,
    const parietal_cortex_position_t* position,
    parietal_cortex_spatial_frame_t from_frame,
    parietal_cortex_spatial_frame_t to_frame
);

/**
 * @brief Request motor plan asynchronously
 *
 * @param adapter Adapter instance
 * @param target_pos Target position
 * @param action_type 0 = reach, 1 = grasp
 * @return Future for motor plan result, or NULL on failure
 */
nimcp_bio_future_t parietal_cortex_request_motor_plan_async(
    parietal_adapter_t* adapter,
    const parietal_cortex_position_t* target_pos,
    uint8_t action_type
);

/**
 * @brief Broadcast attention shift
 *
 * @param adapter Adapter instance
 * @param attention Attention result to broadcast
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t parietal_cortex_broadcast_attention_shift(
    parietal_adapter_t* adapter,
    const parietal_cortex_attention_result_t* attention
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_ADAPTER_H */
