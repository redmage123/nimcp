/**
 * @file nimcp_regions_gpu.h
 * @brief GPU-accelerated Brain Region Specialization Kernels
 *
 * WHAT: CUDA kernels for specialized brain region computations
 * WHY:  GPU acceleration for region-specific neural processing
 * HOW:  Custom kernels for cortical areas, subcortical structures
 *
 * ARCHITECTURE:
 * - Cortical Columns: Canonical microcircuit computation
 * - Prefrontal: Working memory, executive function
 * - Parietal: Spatial processing, attention
 * - Temporal: Object recognition, auditory processing
 * - Motor: Movement planning and execution
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_REGIONS_GPU_H
#define NIMCP_REGIONS_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Brain Region Types
//=============================================================================

/**
 * @brief Major brain region types
 */
typedef enum {
    NIMCP_REGION_PFC = 0,         /**< Prefrontal cortex */
    NIMCP_REGION_MOTOR = 1,       /**< Motor cortex */
    NIMCP_REGION_PARIETAL = 2,    /**< Parietal cortex */
    NIMCP_REGION_TEMPORAL = 3,    /**< Temporal cortex */
    NIMCP_REGION_OCCIPITAL = 4,   /**< Occipital/visual cortex */
    NIMCP_REGION_HIPPOCAMPUS = 5, /**< Hippocampal formation */
    NIMCP_REGION_THALAMUS = 6,    /**< Thalamus */
    NIMCP_REGION_BASAL_GANGLIA = 7, /**< Basal ganglia */
    NIMCP_REGION_CEREBELLUM = 8,  /**< Cerebellum */
    NIMCP_REGION_COUNT = 9
} nimcp_brain_region_t;

/**
 * @brief Cortical layer types
 */
typedef enum {
    NIMCP_LAYER_1 = 0,   /**< Molecular layer */
    NIMCP_LAYER_2_3 = 1, /**< Supragranular */
    NIMCP_LAYER_4 = 2,   /**< Granular (input) */
    NIMCP_LAYER_5 = 3,   /**< Infragranular (output) */
    NIMCP_LAYER_6 = 4,   /**< Deep cortical */
    NIMCP_LAYER_COUNT = 5
} nimcp_cortical_layer_t;

//=============================================================================
// Cortical Column Parameters
//=============================================================================

/**
 * @brief Cortical column parameters
 */
typedef struct {
    int n_columns;                /**< Number of columns */
    int n_neurons_per_column;     /**< Neurons per column */
    float lateral_inhibition;     /**< Lateral inhibition strength */
    float recurrent_excitation;   /**< Recurrent excitation */
    float feedforward_gain;       /**< Feedforward input gain */
    float feedback_gain;          /**< Feedback modulation gain */
    float adaptation_rate;        /**< Spike frequency adaptation */
    float layer_connectivity[NIMCP_LAYER_COUNT][NIMCP_LAYER_COUNT];
} nimcp_gpu_column_params_t;

/**
 * @brief Cortical column state
 */
typedef struct {
    nimcp_gpu_tensor_t* layer_activity[NIMCP_LAYER_COUNT];
    nimcp_gpu_tensor_t* column_output;
    nimcp_gpu_tensor_t* lateral_connections;
    nimcp_gpu_tensor_t* adaptation_state;
    nimcp_gpu_tensor_t* input_weights;
    nimcp_gpu_tensor_t* output_weights;
    size_t n_columns;
    size_t n_neurons;
} nimcp_gpu_column_state_t;

//=============================================================================
// Prefrontal Cortex Parameters
//=============================================================================

/**
 * @brief PFC working memory parameters
 */
typedef struct {
    int n_slots;                  /**< Working memory slots */
    float maintenance_gain;       /**< Maintenance/rehearsal gain */
    float gating_threshold;       /**< Input gating threshold */
    float decay_rate;             /**< WM decay rate */
    float interference_factor;    /**< Proactive interference */
    float dopamine_modulation;    /**< DA modulation of gating */
    bool recurrent_maintenance;   /**< Use recurrent maintenance */
} nimcp_gpu_pfc_params_t;

/**
 * @brief PFC state
 */
typedef struct {
    nimcp_gpu_tensor_t* working_memory;   /**< WM contents */
    nimcp_gpu_tensor_t* gate_state;       /**< Gating state */
    nimcp_gpu_tensor_t* attention_weights;/**< Attention over WM */
    nimcp_gpu_tensor_t* task_context;     /**< Current task context */
    nimcp_gpu_tensor_t* output;           /**< PFC output */
    size_t n_slots;                       /**< Number of WM slots */
    size_t slot_dim;                      /**< Slot dimension */
} nimcp_gpu_pfc_state_t;

//=============================================================================
// Motor Cortex Parameters
//=============================================================================

/**
 * @brief Motor cortex parameters
 */
typedef struct {
    int n_actions;                /**< Number of action primitives */
    int n_muscles;                /**< Number of muscle groups */
    float population_coding_sigma;/**< Population coding width */
    float motor_noise;            /**< Motor output noise */
    float planning_horizon;       /**< Action planning horizon */
    float sequence_learning_rate; /**< Motor sequence learning */
    bool use_internal_model;      /**< Use forward model */
} nimcp_gpu_motor_params_t;

/**
 * @brief Motor cortex state
 */
typedef struct {
    nimcp_gpu_tensor_t* action_plan;      /**< Planned action sequence */
    nimcp_gpu_tensor_t* motor_output;     /**< Motor command output */
    nimcp_gpu_tensor_t* efference_copy;   /**< Efference copy signal */
    nimcp_gpu_tensor_t* population_code;  /**< Population activity */
    nimcp_gpu_tensor_t* forward_model;    /**< Internal forward model */
    size_t n_actions;                     /**< Number of actions */
    size_t n_muscles;                     /**< Number of muscles */
} nimcp_gpu_motor_state_t;

//=============================================================================
// Parietal Cortex Parameters
//=============================================================================

/**
 * @brief Parietal cortex parameters
 */
typedef struct {
    int spatial_resolution;       /**< Spatial map resolution */
    float attention_gain;         /**< Attention modulation gain */
    float coordinate_transform_lr;/**< Coord transform learning rate */
    float multisensory_weight;    /**< Multisensory integration */
    float egocentric_weight;      /**< Egocentric reference weight */
    float allocentric_weight;     /**< Allocentric reference weight */
} nimcp_gpu_parietal_params_t;

/**
 * @brief Parietal state
 */
typedef struct {
    nimcp_gpu_tensor_t* spatial_map;      /**< Spatial attention map */
    nimcp_gpu_tensor_t* egocentric_rep;   /**< Egocentric representation */
    nimcp_gpu_tensor_t* allocentric_rep;  /**< Allocentric representation */
    nimcp_gpu_tensor_t* attention_map;    /**< Attention priority map */
    nimcp_gpu_tensor_t* transform_weights;/**< Coordinate transforms */
    size_t map_size;                      /**< Spatial map size */
} nimcp_gpu_parietal_state_t;

//=============================================================================
// Inter-Region Communication
//=============================================================================

/**
 * @brief Inter-region connection parameters
 */
typedef struct {
    float connection_strength;    /**< Base connection strength */
    float transmission_delay;     /**< Signal transmission delay (ms) */
    float plasticity_rate;        /**< Connection plasticity rate */
    bool bidirectional;           /**< Bidirectional connections */
    float feedback_ratio;         /**< Feedback vs feedforward ratio */
} nimcp_gpu_interregion_params_t;

/**
 * @brief Inter-region connection state
 */
typedef struct {
    nimcp_gpu_tensor_t* connection_weights;
    nimcp_gpu_tensor_t* delay_buffer;
    nimcp_gpu_tensor_t* activity_buffer;
    nimcp_brain_region_t source_region;
    nimcp_brain_region_t target_region;
    size_t source_dim;
    size_t target_dim;
} nimcp_gpu_interregion_state_t;

//=============================================================================
// Default Parameter Functions
//=============================================================================

NIMCP_EXPORT nimcp_gpu_column_params_t nimcp_gpu_column_params_default(void);
NIMCP_EXPORT nimcp_gpu_pfc_params_t nimcp_gpu_pfc_params_default(void);
NIMCP_EXPORT nimcp_gpu_motor_params_t nimcp_gpu_motor_params_default(void);
NIMCP_EXPORT nimcp_gpu_parietal_params_t nimcp_gpu_parietal_params_default(void);
NIMCP_EXPORT nimcp_gpu_interregion_params_t nimcp_gpu_interregion_params_default(void);

//=============================================================================
// Cortical Column Functions
//=============================================================================

/**
 * @brief Update cortical column activity
 */
NIMCP_EXPORT bool nimcp_gpu_column_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_column_state_t* state,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* feedback,
    float dt,
    const nimcp_gpu_column_params_t* params);

/**
 * @brief Apply lateral inhibition within columns
 */
NIMCP_EXPORT bool nimcp_gpu_column_lateral_inhibition(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_column_state_t* state,
    const nimcp_gpu_column_params_t* params);

/**
 * @brief Propagate activity through cortical layers
 */
NIMCP_EXPORT bool nimcp_gpu_column_layer_propagate(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_column_state_t* state,
    const nimcp_gpu_column_params_t* params);

//=============================================================================
// PFC Functions
//=============================================================================

/**
 * @brief Update working memory contents
 */
NIMCP_EXPORT bool nimcp_gpu_pfc_wm_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_pfc_state_t* state,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* dopamine,
    float dt,
    const nimcp_gpu_pfc_params_t* params);

/**
 * @brief Gate input to working memory
 */
NIMCP_EXPORT bool nimcp_gpu_pfc_gating(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_pfc_state_t* state,
    const nimcp_gpu_tensor_t* gate_signal,
    const nimcp_gpu_pfc_params_t* params);

/**
 * @brief Maintain WM through recurrent activity
 */
NIMCP_EXPORT bool nimcp_gpu_pfc_maintenance(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_pfc_state_t* state,
    float dt,
    const nimcp_gpu_pfc_params_t* params);

/**
 * @brief Compute attention over WM slots
 */
NIMCP_EXPORT bool nimcp_gpu_pfc_attention(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_pfc_state_t* state,
    const nimcp_gpu_tensor_t* query,
    const nimcp_gpu_pfc_params_t* params);

//=============================================================================
// Motor Cortex Functions
//=============================================================================

/**
 * @brief Plan motor action sequence
 */
NIMCP_EXPORT bool nimcp_gpu_motor_plan(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_motor_state_t* state,
    const nimcp_gpu_tensor_t* goal,
    const nimcp_gpu_motor_params_t* params);

/**
 * @brief Generate motor commands
 */
NIMCP_EXPORT bool nimcp_gpu_motor_execute(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_motor_state_t* state,
    nimcp_gpu_tensor_t* motor_output,
    const nimcp_gpu_motor_params_t* params);

/**
 * @brief Update forward model with sensory feedback
 */
NIMCP_EXPORT bool nimcp_gpu_motor_update_forward_model(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_motor_state_t* state,
    const nimcp_gpu_tensor_t* predicted,
    const nimcp_gpu_tensor_t* actual,
    const nimcp_gpu_motor_params_t* params);

/**
 * @brief Population coding for motor output
 */
NIMCP_EXPORT bool nimcp_gpu_motor_population_code(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_motor_state_t* state,
    const nimcp_gpu_tensor_t* direction,
    const nimcp_gpu_motor_params_t* params);

//=============================================================================
// Parietal Functions
//=============================================================================

/**
 * @brief Update spatial attention map
 */
NIMCP_EXPORT bool nimcp_gpu_parietal_attention(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_parietal_state_t* state,
    const nimcp_gpu_tensor_t* visual_input,
    const nimcp_gpu_tensor_t* top_down,
    const nimcp_gpu_parietal_params_t* params);

/**
 * @brief Coordinate transformation
 */
NIMCP_EXPORT bool nimcp_gpu_parietal_transform(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_parietal_state_t* state,
    const nimcp_gpu_tensor_t* eye_position,
    const nimcp_gpu_parietal_params_t* params);

/**
 * @brief Multisensory integration
 */
NIMCP_EXPORT bool nimcp_gpu_parietal_multisensory(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_parietal_state_t* state,
    const nimcp_gpu_tensor_t* visual,
    const nimcp_gpu_tensor_t* auditory,
    const nimcp_gpu_tensor_t* proprioceptive,
    const nimcp_gpu_parietal_params_t* params);

//=============================================================================
// Inter-Region Communication Functions
//=============================================================================

/**
 * @brief Transmit signal between regions
 */
NIMCP_EXPORT bool nimcp_gpu_interregion_transmit(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_interregion_state_t* state,
    const nimcp_gpu_tensor_t* source_activity,
    nimcp_gpu_tensor_t* target_input,
    float dt,
    const nimcp_gpu_interregion_params_t* params);

/**
 * @brief Update inter-region connection weights
 */
NIMCP_EXPORT bool nimcp_gpu_interregion_plasticity(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_interregion_state_t* state,
    const nimcp_gpu_tensor_t* source_activity,
    const nimcp_gpu_tensor_t* target_activity,
    float dt,
    const nimcp_gpu_interregion_params_t* params);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_REGIONS_GPU_H
