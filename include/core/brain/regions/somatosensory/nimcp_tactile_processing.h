/**
 * @file nimcp_tactile_processing.h
 * @brief Tactile Sensory Processing for Somatosensory Cortex
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 *
 * This module implements tactile processing functions including:
 * - Mechanoreceptor signal processing
 * - Texture discrimination
 * - Object shape recognition through touch
 * - Vibration detection
 * - Active touch / haptic exploration
 *
 * Processing Hierarchy:
 * - Area 3b: Basic tactile features (edge, pressure)
 * - Area 1: Texture processing
 * - Area 2: Shape and size integration
 * - S2: Complex tactile object recognition
 */

#ifndef NIMCP_TACTILE_PROCESSING_H
#define NIMCP_TACTILE_PROCESSING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define TACTILE_MAX_FEATURE_DIM         128
#define TACTILE_MAX_CONTACTS            16
#define TACTILE_TEXTURE_BINS            32
#define TACTILE_SHAPE_DIM               64
#define TACTILE_VIBRATION_FREQ_MIN      5.0f    /* Hz */
#define TACTILE_VIBRATION_FREQ_MAX      500.0f  /* Hz */
#define TACTILE_FLUTTER_RANGE_MIN       10.0f   /* Hz (Meissner optimal) */
#define TACTILE_FLUTTER_RANGE_MAX       50.0f   /* Hz */
#define TACTILE_VIBRATION_RANGE_MIN     50.0f   /* Hz (Pacinian optimal) */
#define TACTILE_VIBRATION_RANGE_MAX     500.0f  /* Hz */

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Tactile feature types
 */
typedef enum {
    TACTILE_FEATURE_EDGE = 0,
    TACTILE_FEATURE_CORNER,
    TACTILE_FEATURE_CURVE,
    TACTILE_FEATURE_FLAT,
    TACTILE_FEATURE_TEXTURE,
    TACTILE_FEATURE_VIBRATION,
    TACTILE_FEATURE_PRESSURE_GRADIENT,
    TACTILE_FEATURE_SLIP,
    TACTILE_FEATURE_COUNT
} tactile_feature_t;

/**
 * @brief Texture categories
 */
typedef enum {
    TEXTURE_SMOOTH = 0,
    TEXTURE_ROUGH,
    TEXTURE_GRAINY,
    TEXTURE_STICKY,
    TEXTURE_SLIPPERY,
    TEXTURE_SOFT,
    TEXTURE_HARD,
    TEXTURE_FUZZY,
    TEXTURE_BUMPY,
    TEXTURE_RIBBED
} texture_category_t;

/**
 * @brief Shape categories for tactile object recognition
 */
typedef enum {
    SHAPE_UNKNOWN = 0,
    SHAPE_SPHERE,
    SHAPE_CYLINDER,
    SHAPE_CUBE,
    SHAPE_FLAT,
    SHAPE_EDGE,
    SHAPE_CORNER,
    SHAPE_IRREGULAR,
    SHAPE_CURVED_SURFACE,
    SHAPE_COMPLEX
} tactile_shape_t;

/**
 * @brief Grip type classification
 */
typedef enum {
    GRIP_NONE = 0,
    GRIP_PRECISION,         /* Thumb-index pinch */
    GRIP_POWER,             /* Full hand grasp */
    GRIP_LATERAL,           /* Key grip */
    GRIP_TRIPOD,            /* Three-finger */
    GRIP_HOOK               /* Finger hook */
} grip_type_t;

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief Tactile processing configuration
 */
typedef struct {
    float adaptation_rate;
    float lateral_inhibition_strength;
    float feature_threshold;
    float texture_sensitivity;
    bool enable_active_touch;
    bool enable_predictive_coding;
    float prediction_weight;
} tactile_config_t;

/**
 * @brief Single contact point information
 */
typedef struct {
    uint32_t contact_id;
    body_segment_t segment;
    float position[3];          /* Position on body surface */
    float normal[3];            /* Surface normal at contact */
    float force;                /* Contact force (N) */
    float area;                 /* Contact area (mm^2) */
    float velocity[3];          /* Relative motion velocity */
    float slip_detected;        /* Slip magnitude */
    uint64_t onset_time;
    uint64_t last_update;
    bool is_active;
} tactile_contact_t;

/**
 * @brief Mechanoreceptor response
 */
typedef struct {
    soma_receptor_type_t type;
    float response_amplitude;
    float adaptation_state;
    float frequency_tuning;     /* Optimal frequency */
    float bandwidth;            /* Frequency bandwidth */
    bool is_responding;
} mechanoreceptor_response_t;

/**
 * @brief Texture descriptor
 */
typedef struct {
    texture_category_t category;
    float roughness;            /* 0.0 = smooth, 1.0 = very rough */
    float hardness;             /* 0.0 = soft, 1.0 = hard */
    float friction;             /* Friction coefficient */
    float spatial_period;       /* Texture spatial frequency (mm) */
    float temporal_frequency;   /* Induced vibration frequency (Hz) */
    float* spectrum;            /* Roughness spectrum */
    uint32_t spectrum_bins;
    float confidence;
} texture_descriptor_t;

/**
 * @brief Tactile feature vector
 */
typedef struct {
    tactile_feature_t type;
    float position[3];          /* Position of feature */
    float orientation[3];       /* Feature orientation (for edges) */
    float magnitude;            /* Feature strength */
    float curvature;            /* Local curvature */
    float confidence;
} tactile_feature_vector_t;

/**
 * @brief Object shape from tactile exploration
 */
typedef struct {
    tactile_shape_t category;
    float* shape_embedding;     /* Learned shape representation */
    uint32_t embedding_dim;
    float estimated_size[3];    /* Estimated dimensions */
    float curvature_map[64];    /* Local curvatures */
    float symmetry_score;
    float regularity_score;
    float confidence;
    uint32_t exploration_touches;
} tactile_shape_descriptor_t;

/**
 * @brief Grip state information
 */
typedef struct {
    grip_type_t type;
    float grip_force;           /* Total grip force */
    float* finger_forces;       /* Force per finger */
    uint32_t num_fingers;
    float load_force;           /* Object weight */
    float safety_margin;        /* Force margin above slip */
    float slip_ratio;           /* Current slip detection */
    bool object_held;
    float object_mass_estimate;
} grip_state_t;

/**
 * @brief Tactile processing context
 */
typedef struct {
    tactile_config_t config;

    /* Contact tracking */
    tactile_contact_t contacts[TACTILE_MAX_CONTACTS];
    uint32_t num_contacts;

    /* Feature extraction */
    tactile_feature_vector_t* features;
    uint32_t num_features;
    uint32_t max_features;

    /* Current texture */
    texture_descriptor_t current_texture;

    /* Shape processing */
    tactile_shape_descriptor_t current_shape;
    float* shape_accumulator;
    uint32_t shape_samples;

    /* Grip control */
    grip_state_t grip_state;

    /* Predictive coding */
    float* predicted_sensation;
    float* prediction_error;
    uint32_t prediction_dim;

    /* Statistics */
    uint32_t touches_processed;
    uint32_t textures_identified;
    uint32_t shapes_recognized;
} tactile_ctx_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default tactile configuration
 */
tactile_config_t tactile_default_config(void);

/**
 * @brief Create tactile processing context
 */
tactile_ctx_t* tactile_create(const tactile_config_t* config);

/**
 * @brief Destroy tactile context
 */
void tactile_destroy(tactile_ctx_t* ctx);

/**
 * @brief Reset tactile context
 */
int tactile_reset(tactile_ctx_t* ctx);

/*=============================================================================
 * CONTACT PROCESSING
 *===========================================================================*/

/**
 * @brief Add new contact
 */
int tactile_add_contact(tactile_ctx_t* ctx,
                        body_segment_t segment,
                        const float* position,
                        float force,
                        uint32_t* contact_id_out);

/**
 * @brief Update existing contact
 */
int tactile_update_contact(tactile_ctx_t* ctx,
                           uint32_t contact_id,
                           const float* position,
                           float force,
                           const float* velocity);

/**
 * @brief Remove contact
 */
int tactile_remove_contact(tactile_ctx_t* ctx, uint32_t contact_id);

/**
 * @brief Get active contacts
 */
int tactile_get_contacts(tactile_ctx_t* ctx,
                         tactile_contact_t* contacts,
                         uint32_t max_contacts,
                         uint32_t* num_contacts);

/**
 * @brief Detect slip at contact
 */
int tactile_detect_slip(tactile_ctx_t* ctx,
                        uint32_t contact_id,
                        float* slip_magnitude,
                        float* slip_direction);

/*=============================================================================
 * MECHANORECEPTOR SIMULATION
 *===========================================================================*/

/**
 * @brief Simulate mechanoreceptor response
 */
int tactile_simulate_receptor(soma_receptor_type_t type,
                              float stimulus_intensity,
                              float stimulus_velocity,
                              float stimulus_frequency,
                              mechanoreceptor_response_t* response);

/**
 * @brief Apply receptor adaptation
 */
int tactile_apply_adaptation(mechanoreceptor_response_t* response, float dt);

/**
 * @brief Get population response from multiple receptors
 */
int tactile_population_response(tactile_ctx_t* ctx,
                                const tactile_contact_t* contact,
                                float* response_vector,
                                uint32_t vector_dim);

/*=============================================================================
 * FEATURE EXTRACTION
 *===========================================================================*/

/**
 * @brief Extract tactile features from contact
 */
int tactile_extract_features(tactile_ctx_t* ctx,
                             const tactile_contact_t* contact,
                             tactile_feature_vector_t* features,
                             uint32_t max_features,
                             uint32_t* num_features);

/**
 * @brief Detect edge from contact pattern
 */
int tactile_detect_edge(tactile_ctx_t* ctx,
                        const tactile_contact_t* contacts,
                        uint32_t num_contacts,
                        float* edge_position,
                        float* edge_orientation,
                        float* confidence);

/**
 * @brief Detect corner from contact pattern
 */
int tactile_detect_corner(tactile_ctx_t* ctx,
                          const tactile_contact_t* contacts,
                          uint32_t num_contacts,
                          float* corner_position,
                          float* confidence);

/**
 * @brief Estimate local curvature
 */
int tactile_estimate_curvature(tactile_ctx_t* ctx,
                               const tactile_contact_t* contact,
                               float* curvature);

/*=============================================================================
 * TEXTURE PROCESSING
 *===========================================================================*/

/**
 * @brief Process texture from contact
 */
int tactile_process_texture(tactile_ctx_t* ctx,
                            const tactile_contact_t* contact,
                            float exploration_velocity,
                            texture_descriptor_t* texture);

/**
 * @brief Classify texture category
 */
texture_category_t tactile_classify_texture(const texture_descriptor_t* texture);

/**
 * @brief Compare two textures
 */
float tactile_compare_textures(const texture_descriptor_t* a,
                               const texture_descriptor_t* b);

/**
 * @brief Get texture roughness from vibration
 */
float tactile_roughness_from_vibration(float temporal_frequency,
                                       float exploration_velocity);

/*=============================================================================
 * SHAPE PROCESSING
 *===========================================================================*/

/**
 * @brief Accumulate shape information from touch
 */
int tactile_accumulate_shape(tactile_ctx_t* ctx,
                             const tactile_contact_t* contact,
                             const tactile_feature_vector_t* features,
                             uint32_t num_features);

/**
 * @brief Estimate object shape
 */
int tactile_estimate_shape(tactile_ctx_t* ctx,
                           tactile_shape_descriptor_t* shape);

/**
 * @brief Classify shape category
 */
tactile_shape_t tactile_classify_shape(const tactile_shape_descriptor_t* shape);

/**
 * @brief Reset shape accumulator for new object
 */
int tactile_reset_shape_accumulator(tactile_ctx_t* ctx);

/**
 * @brief Get exploration suggestion for shape completion
 */
int tactile_suggest_exploration(tactile_ctx_t* ctx,
                                const tactile_shape_descriptor_t* partial_shape,
                                float* suggested_position,
                                float* confidence);

/*=============================================================================
 * GRIP PROCESSING
 *===========================================================================*/

/**
 * @brief Update grip state
 */
int tactile_update_grip(tactile_ctx_t* ctx,
                        const tactile_contact_t* finger_contacts,
                        uint32_t num_fingers);

/**
 * @brief Get recommended grip force adjustment
 */
int tactile_grip_force_adjustment(tactile_ctx_t* ctx,
                                  float* force_adjustment,
                                  float* safety_margin);

/**
 * @brief Classify grip type from contact pattern
 */
grip_type_t tactile_classify_grip(const tactile_contact_t* contacts,
                                  uint32_t num_contacts);

/**
 * @brief Detect object slip during grip
 */
bool tactile_grip_slip_detected(tactile_ctx_t* ctx);

/**
 * @brief Estimate held object mass from grip
 */
float tactile_estimate_object_mass(tactile_ctx_t* ctx);

/*=============================================================================
 * ACTIVE TOUCH / HAPTIC EXPLORATION
 *===========================================================================*/

/**
 * @brief Plan haptic exploration trajectory
 */
int tactile_plan_exploration(tactile_ctx_t* ctx,
                             body_segment_t effector,
                             const float* target_region,
                             float* trajectory,
                             uint32_t max_points,
                             uint32_t* num_points);

/**
 * @brief Process active touch sequence
 */
int tactile_process_active_touch(tactile_ctx_t* ctx,
                                 const soma_touch_event_t* touch_sequence,
                                 uint32_t sequence_length,
                                 float* object_representation,
                                 uint32_t* rep_dim);

/**
 * @brief Predict sensation from motor command
 */
int tactile_predict_sensation(tactile_ctx_t* ctx,
                              const float* motor_command,
                              uint32_t command_dim,
                              float* predicted_sensation,
                              uint32_t* sensation_dim);

/**
 * @brief Compute prediction error
 */
int tactile_compute_prediction_error(tactile_ctx_t* ctx,
                                     const float* actual_sensation,
                                     uint32_t sensation_dim,
                                     float* error);

/*=============================================================================
 * INTEGRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Integrate tactile with proprioception
 */
int tactile_integrate_proprioception(tactile_ctx_t* ctx,
                                     const soma_proprio_state_t* proprio,
                                     float* integrated_output,
                                     uint32_t* output_dim);

/**
 * @brief Integrate tactile with visual information
 */
int tactile_integrate_visual(tactile_ctx_t* ctx,
                             const float* visual_features,
                             uint32_t visual_dim,
                             float* integrated_output,
                             uint32_t* output_dim);

/*=============================================================================
 * UPDATE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update tactile processing (call each timestep)
 */
int tactile_update(tactile_ctx_t* ctx, float dt);

/**
 * @brief Get tactile output for downstream processing
 */
int tactile_get_output(tactile_ctx_t* ctx,
                       float* output,
                       uint32_t max_dim,
                       uint32_t* actual_dim);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* tactile_feature_name(tactile_feature_t feature);
const char* tactile_texture_name(texture_category_t texture);
const char* tactile_shape_name(tactile_shape_t shape);
const char* tactile_grip_name(grip_type_t grip);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TACTILE_PROCESSING_H */
