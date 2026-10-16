/**
 * @file nimcp_somatosensory.h
 * @brief Somatosensory Cortex - Touch, Proprioception, and Body Awareness
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 *
 * The somatosensory cortex (S1/S2) processes tactile information from the body,
 * including touch, pressure, temperature, proprioception, and pain signals.
 * It maintains a topographic body map (homunculus) and integrates multimodal
 * sensory information for body awareness and motor control.
 *
 * Subregions:
 * - Area 3a: Proprioception (muscle spindles, joint receptors)
 * - Area 3b: Fine touch discrimination (Meissner, Merkel)
 * - Area 1: Texture processing
 * - Area 2: Size, shape integration
 * - S2 (Secondary): Complex tactile processing, bilateral integration
 *
 * Key Functions:
 * - Somatotopic body mapping (homunculus)
 * - Tactile discrimination and texture processing
 * - Proprioceptive body position sensing
 * - Pain processing (nociception)
 * - Temperature sensing (thermoreception)
 * - Cross-modal integration with motor cortex
 *
 * Full Bidirectional Integration:
 * - Prime Resonance Memory System
 * - Immune System & Bio-Async
 * - Brain Initialization
 * - Security Module
 * - Logging System
 * - Cognitive & Training Layers
 * - Omnidirectional Module
 * - Hypothalamus
 * - Neural Substrate
 * - Thalamus Layers (VPL/VPM nuclei)
 * - Motor Cortex (sensorimotor integration)
 * - Parietal Cortex
 * - Pain pathway integration
 */

#ifndef NIMCP_SOMATOSENSORY_H
#define NIMCP_SOMATOSENSORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define SOMA_DEFAULT_NEURONS            2048    /* Total cortical neurons */
#define SOMA_DEFAULT_AREA_3A_NEURONS    512     /* Proprioceptive area */
#define SOMA_DEFAULT_AREA_3B_NEURONS    512     /* Fine touch area */
#define SOMA_DEFAULT_AREA_1_NEURONS     256     /* Texture area */
#define SOMA_DEFAULT_AREA_2_NEURONS     256     /* Shape/size area */
#define SOMA_DEFAULT_S2_NEURONS         512     /* Secondary somatosensory */
#define SOMA_MAX_RECEPTORS              10000   /* Maximum skin receptors */
#define SOMA_MAX_BODY_SEGMENTS          64      /* Body map segments */
#define SOMA_RECEPTOR_DENSITY_MAX       100.0f  /* Max receptors/cm^2 (fingertips) */
#define SOMA_PAIN_THRESHOLD             0.7f    /* Nociceptive threshold */
#define SOMA_TWO_POINT_MIN_MM           2.0f    /* Minimum two-point discrimination */
#define SOMA_PROPRIOCEPTIVE_REFRESH_HZ  100.0f  /* Proprioceptive update rate */
#define SOMA_VERSION_MAJOR              1
#define SOMA_VERSION_MINOR              0
#define SOMA_VERSION_PATCH              0

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Somatosensory operational status
 */
typedef enum {
    SOMA_STATUS_IDLE = 0,
    SOMA_STATUS_READY,
    SOMA_STATUS_PROCESSING_TOUCH,
    SOMA_STATUS_PROCESSING_PAIN,
    SOMA_STATUS_PROCESSING_PROPRIO,
    SOMA_STATUS_PROCESSING_TEMP,
    SOMA_STATUS_INTEGRATING,
    SOMA_STATUS_CALIBRATING,
    SOMA_STATUS_ERROR
} soma_status_t;

/**
 * @brief Somatosensory error codes
 */
typedef enum {
    SOMA_ERROR_NONE = 0,
    SOMA_ERROR_INVALID_INPUT,
    SOMA_ERROR_RECEPTOR_OVERLOAD,
    SOMA_ERROR_BODY_MAP_ERROR,
    SOMA_ERROR_PROCESSING_FAILED,
    SOMA_ERROR_CALIBRATION_FAILED,
    SOMA_ERROR_BRIDGE_ERROR,
    SOMA_ERROR_THRESHOLD_EXCEEDED,
    SOMA_ERROR_INTERNAL
} soma_error_t;

/**
 * @brief Somatosensory cortical area identifiers
 */
typedef enum {
    SOMA_AREA_3A = 0,       /* Proprioception */
    SOMA_AREA_3B,           /* Fine touch */
    SOMA_AREA_1,            /* Texture */
    SOMA_AREA_2,            /* Size/shape */
    SOMA_AREA_S2,           /* Secondary/bilateral */
    SOMA_AREA_COUNT
} soma_area_t;

/**
 * @brief Receptor types in skin (somatosensory)
 *
 * Note: Named soma_receptor_type_t to avoid conflict with neuromodulator
 * receptor_type_t which represents neurotransmitter receptor subtypes.
 */
typedef enum {
    SOMA_RECEPTOR_MEISSNER = 0,  /* Light touch, rapid adaptation */
    SOMA_RECEPTOR_MERKEL,        /* Pressure, slow adaptation */
    SOMA_RECEPTOR_PACINIAN,      /* Vibration, very rapid */
    SOMA_RECEPTOR_RUFFINI,       /* Stretch, slow adaptation */
    SOMA_RECEPTOR_FREE_NERVE,    /* Pain, temperature */
    SOMA_RECEPTOR_MUSCLE_SPINDLE,/* Muscle length (proprioception) */
    SOMA_RECEPTOR_GOLGI_TENDON,  /* Muscle tension */
    SOMA_RECEPTOR_JOINT,         /* Joint position */
    SOMA_RECEPTOR_TYPE_COUNT
} soma_receptor_type_t;

/**
 * @brief Body segment identifiers for somatotopic map
 */
typedef enum {
    BODY_SEG_HEAD = 0,
    BODY_SEG_FACE,
    BODY_SEG_LIPS,
    BODY_SEG_TONGUE,
    BODY_SEG_NECK,
    BODY_SEG_SHOULDER_L,
    BODY_SEG_SHOULDER_R,
    BODY_SEG_UPPER_ARM_L,
    BODY_SEG_UPPER_ARM_R,
    BODY_SEG_ELBOW_L,
    BODY_SEG_ELBOW_R,
    BODY_SEG_FOREARM_L,
    BODY_SEG_FOREARM_R,
    BODY_SEG_WRIST_L,
    BODY_SEG_WRIST_R,
    BODY_SEG_HAND_L,
    BODY_SEG_HAND_R,
    BODY_SEG_THUMB_L,
    BODY_SEG_THUMB_R,
    BODY_SEG_INDEX_L,
    BODY_SEG_INDEX_R,
    BODY_SEG_MIDDLE_L,
    BODY_SEG_MIDDLE_R,
    BODY_SEG_RING_L,
    BODY_SEG_RING_R,
    BODY_SEG_PINKY_L,
    BODY_SEG_PINKY_R,
    BODY_SEG_CHEST,
    BODY_SEG_BACK,
    BODY_SEG_ABDOMEN,
    BODY_SEG_HIP_L,
    BODY_SEG_HIP_R,
    BODY_SEG_THIGH_L,
    BODY_SEG_THIGH_R,
    BODY_SEG_KNEE_L,
    BODY_SEG_KNEE_R,
    BODY_SEG_LOWER_LEG_L,
    BODY_SEG_LOWER_LEG_R,
    BODY_SEG_ANKLE_L,
    BODY_SEG_ANKLE_R,
    BODY_SEG_FOOT_L,
    BODY_SEG_FOOT_R,
    BODY_SEG_TOES_L,
    BODY_SEG_TOES_R,
    BODY_SEG_GENITALS,
    BODY_SEG_COUNT
} body_segment_t;

/**
 * @brief Touch modality types
 */
typedef enum {
    TOUCH_LIGHT = 0,
    TOUCH_PRESSURE,
    TOUCH_VIBRATION,
    TOUCH_STRETCH,
    TOUCH_TEXTURE,
    TOUCH_COUNT
} touch_modality_t;

/**
 * @brief Pain types
 */
typedef enum {
    PAIN_NONE = 0,
    PAIN_SHARP,             /* Fast pain (A-delta fibers) */
    PAIN_DULL,              /* Slow pain (C fibers) */
    PAIN_BURNING,
    PAIN_ACHING,
    PAIN_REFERRED
} pain_type_t;

/**
 * @brief Temperature sensation
 */
typedef enum {
    TEMP_COLD_EXTREME = 0,
    TEMP_COLD,
    TEMP_COOL,
    TEMP_NEUTRAL,
    TEMP_WARM,
    TEMP_HOT,
    TEMP_HOT_EXTREME
} temp_sensation_t;

/*=============================================================================
 * STRUCTURES - RECEPTOR AND SENSORY COMPONENTS
 *===========================================================================*/

/**
 * @brief Individual skin receptor
 */
typedef struct {
    uint32_t receptor_id;
    soma_receptor_type_t type;
    body_segment_t segment;
    float position[3];          /* Position on body surface */
    float receptive_field_size; /* In mm^2 */
    float sensitivity;          /* 0.0-1.0 */
    float adaptation_rate;      /* How fast it adapts */
    float current_activation;
    float last_activation;
    float threshold;
    uint64_t last_spike_time;
    bool is_active;

    /* SNN integration */
    uint32_t snn_neuron_id;
    float membrane_potential;
} soma_receptor_t;

/**
 * @brief Touch event structure
 */
typedef struct {
    uint32_t event_id;
    body_segment_t segment;
    touch_modality_t modality;
    float position[3];          /* Contact point */
    float intensity;            /* Force/pressure level */
    float velocity;             /* Movement velocity */
    float direction[3];         /* Movement direction */
    float texture_roughness;    /* Surface texture */
    float temperature;          /* Object temperature */
    float duration_ms;
    uint64_t timestamp;
    bool is_active;
} soma_touch_event_t;

/**
 * @brief Pain event structure
 */
typedef struct {
    uint32_t event_id;
    body_segment_t segment;
    pain_type_t type;
    float position[3];
    float intensity;            /* 0.0-10.0 scale */
    float duration_ms;
    bool is_chronic;
    float referred_location[3];
    bool has_referred;
    uint64_t onset_time;
    bool is_active;             /* Currently active pain event */

    /* Emotional and cognitive modulation */
    float attention_modulation;
    float emotional_modulation;
    float expected_pain;        /* Expectation affects perception */
} soma_pain_event_t;

/**
 * @brief Proprioceptive state for a joint/segment
 */
typedef struct {
    body_segment_t segment;
    float position[3];          /* Joint angles or position */
    float velocity[3];          /* Movement velocity */
    float acceleration[3];      /* Movement acceleration */
    float muscle_tension;       /* Golgi tendon feedback */
    float muscle_length;        /* Muscle spindle feedback */
    float joint_angle;          /* Degrees */
    float confidence;           /* Estimation confidence */
    uint64_t last_update;
} soma_proprio_state_t;

/**
 * @brief Body map segment in cortex
 */
typedef struct {
    body_segment_t segment;
    uint32_t cortical_area_size;    /* Neural territory in cortex */
    float receptor_density;          /* Receptors per cm^2 */
    float two_point_threshold;       /* Discrimination threshold mm */
    float* neuron_ids;               /* Neurons representing this segment */
    uint32_t num_neurons;
    float activation_level;
    float position_estimate[3];
    float sensitivity_modifier;      /* Attention/learning modulation */

    /* Neighbors for lateral interactions */
    body_segment_t* neighbors;
    uint32_t num_neighbors;
} soma_body_map_entry_t;

/**
 * @brief Cortical column in somatosensory cortex
 */
typedef struct {
    uint32_t column_id;
    soma_area_t area;
    body_segment_t primary_segment;
    float activation;
    float* neuron_activations;
    uint32_t num_neurons;

    /* Layer activities */
    float layer_4_input;        /* Thalamic input */
    float layer_2_3_output;     /* Cortico-cortical */
    float layer_5_output;       /* Motor/subcortical */
    float layer_6_feedback;     /* Thalamic feedback */

    /* SNN integration */
    uint32_t* snn_neuron_ids;
    float avg_firing_rate;
    float synchrony;

    /* Plasticity */
    float receptive_field_plasticity;
    float lateral_inhibition;
} soma_cortical_column_t;

/*=============================================================================
 * STRUCTURES - BRIDGE INTEGRATIONS
 *===========================================================================*/

/**
 * @brief Prime Resonance Memory bridge
 */
typedef struct {
    bool initialized;
    void* pr_memory_ctx;
    float resonance_frequency;
    float resonance_amplitude;
    float tactile_memory_enhancement;
    uint64_t last_resonance_tag;
    bool body_schema_resonance;
} soma_prime_resonance_bridge_t;

/**
 * @brief Immune System bridge
 */
typedef struct {
    bool initialized;
    void* immune_system;
    float health_score;
    float inflammation_level;
    float local_inflammation[BODY_SEG_COUNT];
    bool neuropathic_pain;
    float cytokine_modulation;
} soma_immune_bridge_t;

/**
 * @brief Bio-Async bridge
 */
typedef struct {
    bool initialized;
    void* runtime;
    uint32_t pending_touch_events;
    uint32_t pending_pain_events;
    float async_latency_ms;
    bool real_time_mode;
} soma_bio_async_bridge_t;

/**
 * @brief Brain Initialization bridge
 */
typedef struct {
    bool initialized;
    void* brain_init_ctx;
    uint32_t init_sequence_id;
    float initialization_progress;
    bool body_map_calibrated;
    uint64_t last_calibration;
} soma_brain_init_bridge_t;

/**
 * @brief Security bridge
 */
typedef struct {
    bool initialized;
    void* security_ctx;
    void* security_ops;
    uint32_t access_level;
    bool sensory_data_encrypted;
    uint32_t security_violations;
} soma_security_bridge_t;

/**
 * @brief Logging bridge
 */
typedef struct {
    bool initialized;
    void* logger;
    uint32_t log_level;
    bool trace_enabled;
    char log_prefix[32];
    uint64_t events_logged;
} soma_logging_bridge_t;

/**
 * @brief Cognitive Layer bridge
 */
typedef struct {
    bool initialized;
    void* cognitive_ctx;
    float attention_level;
    float body_attention[BODY_SEG_COUNT];   /* Attention per body part */
    float cognitive_load;
    float tactile_working_memory;
    uint32_t attended_segments;
} soma_cognitive_bridge_t;

/**
 * @brief Training Layer bridge
 */
typedef struct {
    bool initialized;
    void* training_ctx;
    float learning_rate;
    float error_signal;
    bool training_mode;
    float discriminative_learning;
    float sensorimotor_adaptation;
} soma_training_bridge_t;

/**
 * @brief Omnidirectional Module bridge
 */
typedef struct {
    bool initialized;
    void* omni_ctx;
    float integration_weight;
    float cross_modal_binding;
    float global_body_coherence;
    bool multimodal_integration;
} soma_omni_bridge_t;

/**
 * @brief Hypothalamus bridge
 */
typedef struct {
    bool initialized;
    void* hypothalamus;
    float stress_level;
    float pain_modulation;          /* Stress-induced analgesia */
    float temperature_regulation;
    float visceral_state;
    float arousal_level;
} soma_hypothalamus_bridge_t;

/**
 * @brief Neural Substrate bridge
 */
typedef struct {
    bool initialized;
    void* substrate;
    float atp_level;
    float oxygen_saturation;
    float neurotransmitter_levels[8];
    float synaptic_efficiency;
    bool metabolic_stress;
} soma_substrate_bridge_t;

/**
 * @brief Thalamus Layer bridge (VPL/VPM nuclei)
 */
typedef struct {
    bool initialized;
    void* thalamus;
    float vpl_activity;             /* Ventral posterolateral - body */
    float vpm_activity;             /* Ventral posteromedial - face */
    float relay_gain;
    float gating_level;
    uint32_t active_pathways;
    float spinothalamic_input;      /* Pain/temp pathway */
    float lemniscal_input;          /* Fine touch pathway */
} soma_thalamus_bridge_t;

/**
 * @brief Motor Cortex bridge
 */
typedef struct {
    bool initialized;
    void* motor_ctx;
    float efference_copy[BODY_SEG_COUNT];   /* Expected sensory feedback */
    float motor_command[BODY_SEG_COUNT];
    float sensorimotor_prediction_error;
    bool active_touch_mode;         /* Haptic exploration */
    float grip_force_feedback;
} soma_motor_bridge_t;

/**
 * @brief Parietal Cortex bridge
 */
typedef struct {
    bool initialized;
    void* parietal_ctx;
    float* body_schema;             /* Internal body model */
    uint32_t schema_dim;
    float spatial_attention[3];
    float peripersonal_space[3];    /* Near-body space representation */
    float tool_use_extension;       /* Body schema extends with tools */
} soma_parietal_bridge_t;

/**
 * @brief SNN bridge
 */
typedef struct {
    bool initialized;
    void* snn_network;
    float global_firing_rate;
    float network_synchrony;
    uint32_t total_neurons;
    uint32_t active_neurons;
    float cortical_oscillation;
} soma_snn_bridge_t;

/**
 * @brief Plasticity/STDP bridge
 */
typedef struct {
    bool initialized;
    void* plasticity_ctx;
    void* stdp_ctx;
    float ltp_magnitude;
    float ltd_magnitude;
    float receptive_field_plasticity;
    float cross_modal_plasticity;
    bool hebbian_learning;
    bool stdp_enabled;
} soma_plasticity_bridge_t;

/**
 * @brief Portia Module bridge
 */
typedef struct {
    bool initialized;
    void* portia_ctx;
    float tactile_planning_use;
    float haptic_exploration_strategy;
} soma_portia_bridge_t;

/**
 * @brief Dragonfly Module bridge
 */
typedef struct {
    bool initialized;
    void* dragonfly_ctx;
    float threat_from_touch;
    float rapid_withdrawal_threshold;
    float pain_escape_response;
} soma_dragonfly_bridge_t;

/**
 * @brief Perception Layer bridge
 */
typedef struct {
    bool initialized;
    void* perception_ctx;
    float* integrated_percept;
    uint32_t percept_dim;
    float haptic_salience;
    float tactile_novelty;
} soma_perception_bridge_t;

/*=============================================================================
 * MAIN STRUCTURES
 *===========================================================================*/

/**
 * @brief Configuration for somatosensory cortex
 */
typedef struct {
    uint32_t num_area_3a_neurons;
    uint32_t num_area_3b_neurons;
    uint32_t num_area_1_neurons;
    uint32_t num_area_2_neurons;
    uint32_t num_s2_neurons;
    uint32_t max_receptors;

    /* Processing parameters */
    float pain_threshold;
    float adaptation_rate;
    float lateral_inhibition_strength;
    float receptive_field_plasticity;

    /* Body map */
    bool enable_detailed_body_map;
    bool enable_tool_use_extension;

    /* Integration */
    bool enable_motor_efference_copy;
    bool enable_prime_resonance;
    bool enable_all_bridges;

    /* Pain processing */
    float gate_control_weight;      /* Gate control theory */
    bool enable_descending_modulation;
} soma_config_t;

/**
 * @brief Statistics for somatosensory cortex
 */
typedef struct {
    uint32_t touch_events_processed;
    uint32_t pain_events_processed;
    uint32_t proprio_updates;
    float avg_touch_intensity;
    float avg_pain_level;
    float body_map_accuracy;
    float two_point_discrimination_avg;
    float proprioceptive_accuracy;
    uint64_t last_update_time;
    uint32_t updates_processed;

    /* Per-area statistics */
    float area_3a_activity;
    float area_3b_activity;
    float area_1_activity;
    float area_2_activity;
    float s2_activity;
} soma_stats_t;

/**
 * @brief Main somatosensory cortex structure
 */
typedef struct {
    /* Configuration */
    soma_config_t config;

    /* Status */
    soma_status_t status;
    soma_error_t last_error;

    /* Receptors */
    soma_receptor_t* receptors;
    uint32_t num_receptors;

    /* Body map */
    soma_body_map_entry_t body_map[BODY_SEG_COUNT];

    /* Cortical columns by area */
    soma_cortical_column_t* area_3a_columns;
    uint32_t num_3a_columns;
    soma_cortical_column_t* area_3b_columns;
    uint32_t num_3b_columns;
    soma_cortical_column_t* area_1_columns;
    uint32_t num_1_columns;
    soma_cortical_column_t* area_2_columns;
    uint32_t num_2_columns;
    soma_cortical_column_t* s2_columns;
    uint32_t num_s2_columns;

    /* Current events */
    soma_touch_event_t* active_touch_events;
    uint32_t num_active_touch;
    uint32_t max_touch_events;

    soma_pain_event_t* active_pain_events;
    uint32_t num_active_pain;
    uint32_t max_pain_events;

    /* Proprioceptive state */
    soma_proprio_state_t proprio_state[BODY_SEG_COUNT];

    /* Activation buffers */
    float* area_3a_activation;
    float* area_3b_activation;
    float* area_1_activation;
    float* area_2_activation;
    float* s2_activation;

    /* Bridge integrations */
    soma_prime_resonance_bridge_t prime_resonance_bridge;
    soma_immune_bridge_t immune_bridge;
    soma_bio_async_bridge_t bio_async_bridge;
    soma_brain_init_bridge_t brain_init_bridge;
    soma_security_bridge_t security_bridge;
    soma_logging_bridge_t logging_bridge;
    soma_cognitive_bridge_t cognitive_bridge;
    soma_training_bridge_t training_bridge;
    soma_omni_bridge_t omni_bridge;
    soma_hypothalamus_bridge_t hypothalamus_bridge;
    soma_substrate_bridge_t substrate_bridge;
    soma_thalamus_bridge_t thalamus_bridge;
    soma_motor_bridge_t motor_bridge;
    soma_parietal_bridge_t parietal_bridge;
    soma_snn_bridge_t snn_bridge;
    soma_plasticity_bridge_t plasticity_bridge;
    soma_portia_bridge_t portia_bridge;
    soma_dragonfly_bridge_t dragonfly_bridge;
    soma_perception_bridge_t perception_bridge;

    /* Statistics */
    uint32_t updates_processed;
    uint32_t touch_events_total;
    uint32_t pain_events_total;
    uint64_t creation_time;
    uint64_t last_update_time;
} nimcp_somatosensory_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

soma_config_t soma_default_config(void);
nimcp_somatosensory_t* soma_create(const soma_config_t* config);
void soma_destroy(nimcp_somatosensory_t* soma);
int soma_reset(nimcp_somatosensory_t* soma);
int soma_update(nimcp_somatosensory_t* soma, float dt);

/*=============================================================================
 * TOUCH PROCESSING FUNCTIONS
 *===========================================================================*/

/**
 * @brief Process touch input at body location
 */
int soma_process_touch(nimcp_somatosensory_t* soma,
                       body_segment_t segment,
                       const float* position,
                       float intensity,
                       touch_modality_t modality,
                       uint32_t* event_id_out);

/**
 * @brief Process touch with full parameters
 */
int soma_process_touch_full(nimcp_somatosensory_t* soma,
                            const soma_touch_event_t* event,
                            uint32_t* event_id_out);

/**
 * @brief Get active touch events
 */
int soma_get_active_touches(nimcp_somatosensory_t* soma,
                            soma_touch_event_t* events,
                            uint32_t max_events,
                            uint32_t* num_events);

/**
 * @brief Discriminate between two touch points
 */
int soma_two_point_discrimination(nimcp_somatosensory_t* soma,
                                  body_segment_t segment,
                                  const float* point1,
                                  const float* point2,
                                  bool* can_discriminate,
                                  float* discrimination_confidence);

/**
 * @brief Process texture information
 */
int soma_process_texture(nimcp_somatosensory_t* soma,
                         body_segment_t segment,
                         float roughness,
                         float hardness,
                         float* texture_code,
                         uint32_t code_dim);

/**
 * @brief Process object shape through touch
 */
int soma_process_shape(nimcp_somatosensory_t* soma,
                       const soma_touch_event_t* touch_sequence,
                       uint32_t num_touches,
                       float* shape_representation,
                       uint32_t* shape_dim);

/*=============================================================================
 * PAIN PROCESSING FUNCTIONS
 *===========================================================================*/

/**
 * @brief Process pain signal
 */
int soma_process_pain(nimcp_somatosensory_t* soma,
                      body_segment_t segment,
                      pain_type_t type,
                      float intensity,
                      uint32_t* event_id_out);

/**
 * @brief Process pain with full parameters
 */
int soma_process_pain_full(nimcp_somatosensory_t* soma,
                           const soma_pain_event_t* event,
                           uint32_t* event_id_out);

/**
 * @brief Apply gate control modulation
 */
int soma_apply_gate_control(nimcp_somatosensory_t* soma,
                            uint32_t pain_event_id,
                            float touch_inhibition,
                            float descending_modulation);

/**
 * @brief Get current pain level
 */
float soma_get_pain_level(nimcp_somatosensory_t* soma,
                          body_segment_t segment);

/**
 * @brief Get overall pain state
 */
int soma_get_pain_state(nimcp_somatosensory_t* soma,
                        float* total_pain,
                        body_segment_t* max_pain_segment,
                        pain_type_t* dominant_type);

/**
 * @brief Check for referred pain
 */
int soma_check_referred_pain(nimcp_somatosensory_t* soma,
                             body_segment_t source,
                             body_segment_t* referred_to,
                             float* referred_intensity);

/*=============================================================================
 * PROPRIOCEPTION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update proprioceptive state for segment
 */
int soma_update_proprioception(nimcp_somatosensory_t* soma,
                               body_segment_t segment,
                               const float* position,
                               const float* velocity,
                               float muscle_tension,
                               float muscle_length);

/**
 * @brief Get proprioceptive state
 */
int soma_get_proprioception(nimcp_somatosensory_t* soma,
                            body_segment_t segment,
                            soma_proprio_state_t* state);

/**
 * @brief Get full body position estimate
 */
int soma_get_body_position(nimcp_somatosensory_t* soma,
                           float* positions,
                           uint32_t max_segments,
                           uint32_t* num_segments);

/**
 * @brief Estimate joint angle
 */
float soma_get_joint_angle(nimcp_somatosensory_t* soma,
                           body_segment_t segment);

/**
 * @brief Get movement velocity for segment
 */
int soma_get_movement_velocity(nimcp_somatosensory_t* soma,
                               body_segment_t segment,
                               float* velocity);

/**
 * @brief Predict proprioceptive state
 */
int soma_predict_proprioception(nimcp_somatosensory_t* soma,
                                body_segment_t segment,
                                float dt,
                                soma_proprio_state_t* predicted);

/*=============================================================================
 * TEMPERATURE PROCESSING
 *===========================================================================*/

/**
 * @brief Process temperature signal
 */
int soma_process_temperature(nimcp_somatosensory_t* soma,
                             body_segment_t segment,
                             float temperature,
                             temp_sensation_t* sensation_out);

/**
 * @brief Get temperature at body location
 */
temp_sensation_t soma_get_temperature_sensation(nimcp_somatosensory_t* soma,
                                                 body_segment_t segment);

/*=============================================================================
 * BODY MAP FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get body map entry
 */
const soma_body_map_entry_t* soma_get_body_map_entry(nimcp_somatosensory_t* soma,
                                                      body_segment_t segment);

/**
 * @brief Get cortical magnification for segment
 */
float soma_get_cortical_magnification(nimcp_somatosensory_t* soma,
                                      body_segment_t segment);

/**
 * @brief Update body map (plasticity)
 */
int soma_update_body_map(nimcp_somatosensory_t* soma,
                         body_segment_t segment,
                         float sensitivity_change);

/**
 * @brief Extend body schema for tool use
 */
int soma_extend_body_schema(nimcp_somatosensory_t* soma,
                            body_segment_t segment,
                            const float* tool_extension,
                            uint32_t extension_dim);

/**
 * @brief Get two-point discrimination threshold
 */
float soma_get_two_point_threshold(nimcp_somatosensory_t* soma,
                                   body_segment_t segment);

/**
 * @brief Calibrate body map
 */
int soma_calibrate_body_map(nimcp_somatosensory_t* soma);

/*=============================================================================
 * CORTICAL AREA ACCESS
 *===========================================================================*/

/**
 * @brief Get activation pattern for area
 */
int soma_get_area_activation(nimcp_somatosensory_t* soma,
                             soma_area_t area,
                             float* activation,
                             uint32_t max_size,
                             uint32_t* actual_size);

/**
 * @brief Set input to cortical area
 */
int soma_set_area_input(nimcp_somatosensory_t* soma,
                        soma_area_t area,
                        const float* input,
                        uint32_t input_size);

/**
 * @brief Propagate activity through areas
 */
int soma_propagate_activity(nimcp_somatosensory_t* soma);

/*=============================================================================
 * INTEGRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Process efference copy from motor cortex
 */
int soma_process_efference_copy(nimcp_somatosensory_t* soma,
                                body_segment_t segment,
                                const float* expected_sensation,
                                uint32_t sensation_dim);

/**
 * @brief Compute sensory prediction error
 */
int soma_compute_prediction_error(nimcp_somatosensory_t* soma,
                                  body_segment_t segment,
                                  float* prediction_error);

/**
 * @brief Active touch / haptic exploration
 */
int soma_active_touch(nimcp_somatosensory_t* soma,
                      body_segment_t segment,
                      const float* exploration_target,
                      float* tactile_features,
                      uint32_t* feature_dim);

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW
 *===========================================================================*/

int soma_process_incoming(nimcp_somatosensory_t* soma);
int soma_send_outgoing(nimcp_somatosensory_t* soma);
int soma_bidirectional_update(nimcp_somatosensory_t* soma, float dt);

/* Specific synchronization */
int soma_sync_thalamus(nimcp_somatosensory_t* soma);
int soma_sync_motor_cortex(nimcp_somatosensory_t* soma);
int soma_sync_parietal(nimcp_somatosensory_t* soma);
int soma_sync_hypothalamus(nimcp_somatosensory_t* soma);

/*=============================================================================
 * BRIDGE INITIALIZATION FUNCTIONS
 *===========================================================================*/

int soma_init_prime_resonance_bridge(nimcp_somatosensory_t* soma, void* pr_memory);
int soma_init_immune_bridge(nimcp_somatosensory_t* soma, void* immune);
int soma_init_bio_async_bridge(nimcp_somatosensory_t* soma, void* runtime);
int soma_init_brain_init_bridge(nimcp_somatosensory_t* soma, void* brain_init);
int soma_init_security_bridge(nimcp_somatosensory_t* soma, void* security_ctx, void* security_ops);
int soma_init_logging_bridge(nimcp_somatosensory_t* soma, void* logger);
int soma_init_cognitive_bridge(nimcp_somatosensory_t* soma, void* cognitive);
int soma_init_training_bridge(nimcp_somatosensory_t* soma, void* training);
int soma_init_omni_bridge(nimcp_somatosensory_t* soma, void* omni);
int soma_init_hypothalamus_bridge(nimcp_somatosensory_t* soma, void* hypothalamus);
int soma_init_substrate_bridge(nimcp_somatosensory_t* soma, void* substrate);
int soma_init_thalamus_bridge(nimcp_somatosensory_t* soma, void* thalamus);
int soma_init_motor_bridge(nimcp_somatosensory_t* soma, void* motor);
int soma_init_parietal_bridge(nimcp_somatosensory_t* soma, void* parietal);
int soma_init_snn_bridge(nimcp_somatosensory_t* soma, void* snn);
int soma_init_plasticity_bridge(nimcp_somatosensory_t* soma, void* plasticity, void* stdp);
int soma_init_portia_bridge(nimcp_somatosensory_t* soma, void* portia);
int soma_init_dragonfly_bridge(nimcp_somatosensory_t* soma, void* dragonfly);
int soma_init_perception_bridge(nimcp_somatosensory_t* soma, void* perception);

/**
 * @brief Initialize all bridges at once
 */
int soma_init_all_bridges(nimcp_somatosensory_t* soma, void** bridge_contexts);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

soma_status_t soma_get_status(nimcp_somatosensory_t* soma);
soma_error_t soma_get_last_error(nimcp_somatosensory_t* soma);
const char* soma_error_string(soma_error_t error);
const char* soma_status_string(soma_status_t status);
int soma_get_stats(nimcp_somatosensory_t* soma, soma_stats_t* stats);
int soma_get_config(nimcp_somatosensory_t* soma, soma_config_t* config);
float soma_get_health_status(nimcp_somatosensory_t* soma);
int soma_log_diagnostics(nimcp_somatosensory_t* soma);

/*=============================================================================
 * SERIALIZATION
 *===========================================================================*/

size_t soma_get_serialization_size(nimcp_somatosensory_t* soma);
int soma_serialize(nimcp_somatosensory_t* soma, uint8_t* buffer, size_t size, size_t* written);
nimcp_somatosensory_t* soma_deserialize(const uint8_t* buffer, size_t size, size_t* bytes_read);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* soma_segment_name(body_segment_t segment);
const char* soma_receptor_type_name(soma_receptor_type_t type);
const char* soma_area_name(soma_area_t area);
const char* soma_pain_type_name(pain_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOMATOSENSORY_H */
