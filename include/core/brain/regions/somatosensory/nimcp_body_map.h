/**
 * @file nimcp_body_map.h
 * @brief Somatotopic Body Map (Homunculus) Implementation
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 *
 * This module implements the somatotopic body map (sensory homunculus),
 * which represents the topographic organization of body surfaces in the
 * somatosensory cortex. Different body parts have different cortical
 * magnification factors based on their sensory acuity.
 *
 * Key Features:
 * - Cortical magnification (fingers, lips have larger representations)
 * - Two-point discrimination thresholds per body region
 * - Receptive field organization
 * - Body schema plasticity (phantom limbs, tool use extension)
 * - Neighboring segment interactions
 */

#ifndef NIMCP_BODY_MAP_H
#define NIMCP_BODY_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/* Cortical magnification factors (relative to trunk) */
#define BODY_MAP_MAGNIFICATION_TRUNK        1.0f
#define BODY_MAP_MAGNIFICATION_ARM          2.0f
#define BODY_MAP_MAGNIFICATION_HAND         8.0f
#define BODY_MAP_MAGNIFICATION_FINGERS      15.0f
#define BODY_MAP_MAGNIFICATION_THUMB        20.0f
#define BODY_MAP_MAGNIFICATION_FACE         10.0f
#define BODY_MAP_MAGNIFICATION_LIPS         25.0f
#define BODY_MAP_MAGNIFICATION_TONGUE       30.0f
#define BODY_MAP_MAGNIFICATION_GENITALS     12.0f
#define BODY_MAP_MAGNIFICATION_FOOT         3.0f
#define BODY_MAP_MAGNIFICATION_TOES         5.0f

/* Two-point discrimination thresholds (mm) */
#define TWO_POINT_FINGERTIP     2.0f
#define TWO_POINT_PALM          10.0f
#define TWO_POINT_FOREARM       40.0f
#define TWO_POINT_BACK          60.0f
#define TWO_POINT_LIPS          4.0f
#define TWO_POINT_TONGUE        1.5f
#define TWO_POINT_FOOT_SOLE     15.0f

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief Body map configuration
 */
typedef struct {
    bool enable_plasticity;
    float plasticity_rate;
    float lateral_inhibition_strength;
    bool enable_tool_extension;
    float tool_extension_rate;
    bool enable_phantom_limb_simulation;
} body_map_config_t;

/**
 * @brief Cortical representation of a body region
 */
typedef struct {
    body_segment_t segment;

    /* Cortical territory */
    float magnification_factor;     /* Relative cortical area */
    uint32_t num_neurons;           /* Neurons dedicated to this segment */
    float* neuron_positions;        /* Positions in cortical space (x, y) */
    uint32_t cortical_layer;        /* Primary cortical layer */

    /* Sensory properties */
    float receptor_density;         /* Receptors per cm^2 */
    float two_point_threshold;      /* Minimum discriminable distance (mm) */
    float spatial_resolution;       /* Spatial acuity */
    float temporal_resolution;      /* Temporal acuity (Hz) */

    /* Receptive fields */
    float* receptive_field_centers; /* RF center positions on body */
    float* receptive_field_sizes;   /* RF sizes */
    uint32_t num_receptive_fields;

    /* Activation state */
    float current_activation;
    float baseline_activation;
    float adaptation_state;

    /* Plasticity */
    float plasticity_state;         /* Current plastic modification */
    float use_dependent_expansion;  /* Expansion from use */
    float denervation_shrinkage;    /* Shrinkage from disuse */

    /* Tool use extension */
    bool has_tool_extension;
    float tool_extension_size;
    float* tool_representation;
    uint32_t tool_rep_dim;

    /* Phantom limb state */
    bool is_phantom;
    float phantom_intensity;
} body_map_region_t;

/**
 * @brief Connection between adjacent body regions
 */
typedef struct {
    body_segment_t segment_a;
    body_segment_t segment_b;
    float connection_strength;
    float lateral_inhibition;
    float spread_factor;            /* Activity spread between regions */
} body_map_connection_t;

/**
 * @brief Complete body map structure
 */
typedef struct {
    body_map_config_t config;

    /* Regions */
    body_map_region_t regions[BODY_SEG_COUNT];

    /* Connectivity */
    body_map_connection_t* connections;
    uint32_t num_connections;

    /* Global state */
    float total_cortical_area;
    float activity_normalization;
    float global_plasticity_rate;

    /* Statistics */
    uint32_t updates_processed;
    float avg_activation;
    float max_activation;
    body_segment_t most_active_segment;

    /* Reference frames */
    float body_center[3];           /* Body-centered coordinates */
    float head_direction;           /* Current head direction */
} body_map_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default body map configuration
 */
body_map_config_t body_map_default_config(void);

/**
 * @brief Create body map
 */
body_map_t* body_map_create(const body_map_config_t* config);

/**
 * @brief Destroy body map
 */
void body_map_destroy(body_map_t* map);

/**
 * @brief Initialize body map with default magnifications
 */
int body_map_init_default(body_map_t* map);

/**
 * @brief Reset body map to baseline
 */
int body_map_reset(body_map_t* map);

/*=============================================================================
 * REGION ACCESS
 *===========================================================================*/

/**
 * @brief Get region by segment
 */
body_map_region_t* body_map_get_region(body_map_t* map, body_segment_t segment);

/**
 * @brief Get magnification factor
 */
float body_map_get_magnification(body_map_t* map, body_segment_t segment);

/**
 * @brief Get two-point threshold
 */
float body_map_get_two_point_threshold(body_map_t* map, body_segment_t segment);

/**
 * @brief Get receptor density
 */
float body_map_get_receptor_density(body_map_t* map, body_segment_t segment);

/**
 * @brief Get neighboring segments
 */
int body_map_get_neighbors(body_map_t* map,
                           body_segment_t segment,
                           body_segment_t* neighbors,
                           uint32_t max_neighbors,
                           uint32_t* num_neighbors);

/*=============================================================================
 * ACTIVATION AND PROCESSING
 *===========================================================================*/

/**
 * @brief Activate body region
 */
int body_map_activate(body_map_t* map,
                      body_segment_t segment,
                      float activation_level);

/**
 * @brief Activate at specific position within segment
 */
int body_map_activate_position(body_map_t* map,
                               body_segment_t segment,
                               const float* position,
                               float activation_level,
                               float spread_sigma);

/**
 * @brief Get activation level
 */
float body_map_get_activation(body_map_t* map, body_segment_t segment);

/**
 * @brief Apply lateral inhibition
 */
int body_map_apply_lateral_inhibition(body_map_t* map);

/**
 * @brief Propagate activity to neighbors
 */
int body_map_propagate_activity(body_map_t* map, float spread_factor);

/**
 * @brief Update body map (decay, normalize)
 */
int body_map_update(body_map_t* map, float dt);

/*=============================================================================
 * SPATIAL FUNCTIONS
 *===========================================================================*/

/**
 * @brief Convert body coordinates to cortical coordinates
 */
int body_map_body_to_cortical(body_map_t* map,
                              body_segment_t segment,
                              const float* body_pos,
                              float* cortical_pos);

/**
 * @brief Convert cortical coordinates to body coordinates
 */
int body_map_cortical_to_body(body_map_t* map,
                              const float* cortical_pos,
                              body_segment_t* segment,
                              float* body_pos);

/**
 * @brief Get receptive field at cortical position
 */
int body_map_get_receptive_field(body_map_t* map,
                                 body_segment_t segment,
                                 uint32_t neuron_idx,
                                 float* rf_center,
                                 float* rf_size);

/**
 * @brief Test if stimulus falls within receptive field
 */
bool body_map_stimulus_in_rf(body_map_t* map,
                             body_segment_t segment,
                             uint32_t neuron_idx,
                             const float* stimulus_pos);

/*=============================================================================
 * PLASTICITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Apply use-dependent plasticity
 */
int body_map_apply_plasticity(body_map_t* map,
                              body_segment_t segment,
                              float use_intensity,
                              float dt);

/**
 * @brief Expand cortical representation
 */
int body_map_expand_region(body_map_t* map,
                           body_segment_t segment,
                           float expansion_factor);

/**
 * @brief Contract cortical representation (disuse)
 */
int body_map_contract_region(body_map_t* map,
                             body_segment_t segment,
                             float contraction_factor);

/**
 * @brief Shift receptive field center
 */
int body_map_shift_rf(body_map_t* map,
                      body_segment_t segment,
                      uint32_t neuron_idx,
                      const float* new_center);

/**
 * @brief Modify receptive field size
 */
int body_map_resize_rf(body_map_t* map,
                       body_segment_t segment,
                       uint32_t neuron_idx,
                       float size_factor);

/*=============================================================================
 * TOOL USE EXTENSION
 *===========================================================================*/

/**
 * @brief Extend body schema with tool
 */
int body_map_extend_with_tool(body_map_t* map,
                              body_segment_t segment,
                              const float* tool_geometry,
                              uint32_t geometry_dim);

/**
 * @brief Update tool representation
 */
int body_map_update_tool(body_map_t* map,
                         body_segment_t segment,
                         const float* tool_position,
                         const float* tool_tip);

/**
 * @brief Remove tool extension
 */
int body_map_remove_tool(body_map_t* map, body_segment_t segment);

/**
 * @brief Check if position is within tool-extended body schema
 */
bool body_map_in_extended_schema(body_map_t* map,
                                 body_segment_t segment,
                                 const float* position);

/*=============================================================================
 * PHANTOM LIMB SIMULATION
 *===========================================================================*/

/**
 * @brief Create phantom limb representation
 */
int body_map_create_phantom(body_map_t* map,
                            body_segment_t segment,
                            float initial_intensity);

/**
 * @brief Update phantom limb state
 */
int body_map_update_phantom(body_map_t* map,
                            body_segment_t segment,
                            float referred_activation);

/**
 * @brief Get phantom limb intensity
 */
float body_map_get_phantom_intensity(body_map_t* map, body_segment_t segment);

/**
 * @brief Remove phantom limb (therapy simulation)
 */
int body_map_reduce_phantom(body_map_t* map,
                            body_segment_t segment,
                            float reduction_rate);

/*=============================================================================
 * VISUALIZATION AND EXPORT
 *===========================================================================*/

/**
 * @brief Get homunculus representation
 * Returns relative sizes of body parts for visualization
 */
int body_map_get_homunculus(body_map_t* map,
                            float* sizes,
                            uint32_t max_segments,
                            uint32_t* num_segments);

/**
 * @brief Export body map state
 */
int body_map_export_state(body_map_t* map,
                          float* activations,
                          float* magnifications,
                          uint32_t max_segments);

/**
 * @brief Get most active regions
 */
int body_map_get_top_active(body_map_t* map,
                            body_segment_t* segments,
                            float* activations,
                            uint32_t top_n);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default magnification for segment
 */
float body_map_default_magnification(body_segment_t segment);

/**
 * @brief Get default two-point threshold for segment
 */
float body_map_default_two_point(body_segment_t segment);

/**
 * @brief Get default receptor density for segment
 */
float body_map_default_receptor_density(body_segment_t segment);

/**
 * @brief Check if segments are adjacent
 */
bool body_map_are_adjacent(body_segment_t a, body_segment_t b);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BODY_MAP_H */
