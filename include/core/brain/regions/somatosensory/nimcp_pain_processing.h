/**
 * @file nimcp_pain_processing.h
 * @brief Nociceptive Pain Processing Module
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 *
 * This module implements pain (nociceptive) processing including:
 * - Fast and slow pain pathways (A-delta and C fibers)
 * - Gate control theory modulation
 * - Descending pain modulation (PAG, endorphins)
 * - Referred pain mapping
 * - Pain memory and sensitization
 * - Affective and cognitive pain modulation
 *
 * Pain Pathways:
 * - Spinothalamic tract (lateral): Pain and temperature
 * - Spinoreticular tract: Arousal response
 * - Spinomesencephalic tract: Descending modulation
 *
 * Cortical Areas:
 * - S1/S2: Sensory-discriminative (location, intensity)
 * - Insula: Affective component
 * - ACC: Emotional suffering
 * - PFC: Cognitive modulation
 */

#ifndef NIMCP_PAIN_PROCESSING_H
#define NIMCP_PAIN_PROCESSING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define PAIN_MAX_EVENTS             32
#define PAIN_MAX_REFERRED_ZONES     16
#define PAIN_INTENSITY_MAX          10.0f   /* Standard 0-10 scale */
#define PAIN_GATE_CONTROL_WINDOW_MS 100.0f
#define PAIN_A_DELTA_VELOCITY       15.0f   /* m/s - fast pain */
#define PAIN_C_FIBER_VELOCITY       1.0f    /* m/s - slow pain */
#define PAIN_SENSITIZATION_TAU      3600.0f /* seconds - 1 hour */
#define PAIN_MEMORY_DECAY_TAU       86400.0f /* seconds - 24 hours */
#define PAIN_ENDORPHIN_HALF_LIFE    300.0f  /* seconds - 5 minutes */

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Pain fiber types
 */
typedef enum {
    PAIN_FIBER_A_DELTA = 0,     /* Fast, sharp, well-localized */
    PAIN_FIBER_C                /* Slow, dull, diffuse */
} pain_fiber_t;

/**
 * @brief Pain modality
 */
typedef enum {
    PAIN_MODALITY_MECHANICAL = 0,
    PAIN_MODALITY_THERMAL_HEAT,
    PAIN_MODALITY_THERMAL_COLD,
    PAIN_MODALITY_CHEMICAL,
    PAIN_MODALITY_POLYMODAL       /* Responds to multiple stimuli */
} pain_modality_t;

/**
 * @brief Chronic pain state
 */
typedef enum {
    CHRONIC_NONE = 0,
    CHRONIC_INFLAMMATORY,       /* Tissue damage */
    CHRONIC_NEUROPATHIC,        /* Nerve damage */
    CHRONIC_NOCIPLASTIC,        /* Central sensitization */
    CHRONIC_MIXED
} chronic_pain_state_t;

/**
 * @brief Pain processing stage
 */
typedef enum {
    PAIN_STAGE_TRANSDUCTION = 0,    /* Receptor activation */
    PAIN_STAGE_TRANSMISSION,        /* Spinal cord */
    PAIN_STAGE_MODULATION,          /* Gate control */
    PAIN_STAGE_PERCEPTION           /* Cortical processing */
} pain_stage_t;

/**
 * @brief Descending modulation type
 */
typedef enum {
    DESCENDING_NONE = 0,
    DESCENDING_PAG,             /* Periaqueductal gray */
    DESCENDING_RVM,             /* Rostral ventromedial medulla */
    DESCENDING_LOCUS_COERULEUS, /* Noradrenergic */
    DESCENDING_CORTICAL         /* PFC top-down */
} descending_mod_t;

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief Pain processing configuration
 */
typedef struct {
    float pain_threshold;
    float tolerance_threshold;
    float gate_control_weight;
    float descending_modulation_strength;
    bool enable_referred_pain;
    bool enable_sensitization;
    bool enable_pain_memory;
    float endorphin_effectiveness;
    float cognitive_modulation_weight;
    float emotional_modulation_weight;
} pain_config_t;

/**
 * @brief Nociceptor (pain receptor)
 */
typedef struct {
    uint32_t nociceptor_id;
    body_segment_t segment;
    pain_fiber_t fiber_type;
    pain_modality_t modality;

    /* Physical properties */
    float position[3];
    float receptive_field_size;
    float threshold;
    float gain;

    /* State */
    float current_activation;
    float sensitization_level;      /* 1.0 = normal, >1.0 = sensitized */
    bool is_sensitized;

    /* Output */
    float firing_rate;
    uint64_t last_spike_time;

    /* SNN integration */
    uint32_t snn_neuron_id;
} nociceptor_t;

/**
 * @brief Detailed pain event
 */
typedef struct {
    uint32_t event_id;
    body_segment_t segment;
    pain_type_t type;
    pain_modality_t modality;
    pain_fiber_t fiber;

    /* Location and intensity */
    float position[3];
    float intensity;                /* 0-10 scale */
    float localization_precision;   /* How well localized */

    /* Temporal properties */
    uint64_t onset_time;
    float duration_ms;
    float latency_ms;               /* Time from stimulus to perception */

    /* Gate control modulation */
    float gate_state;               /* 0.0 = closed, 1.0 = fully open */
    float touch_inhibition;
    float descending_inhibition;

    /* Referred pain */
    bool is_referred;
    body_segment_t referred_from;
    float referred_probability;

    /* Affective components */
    float unpleasantness;           /* Affective dimension */
    float suffering;                /* Cognitive-emotional suffering */
    float fear_component;
    float anxiety_component;

    /* Modulation factors */
    float attention_factor;         /* Attention increases pain */
    float distraction_factor;       /* Distraction decreases pain */
    float expectation_factor;       /* Expectation modulates */
    float placebo_effect;
    float nocebo_effect;

    /* State flags */
    bool is_active;
    bool is_chronic;
    chronic_pain_state_t chronic_state;
} pain_event_detailed_t;

/**
 * @brief Gate control state at spinal level
 */
typedef struct {
    body_segment_t segment;

    /* Gate state */
    float gate_position;            /* 0.0 = closed, 1.0 = open */

    /* Inputs */
    float c_fiber_input;            /* Small fiber (pain) */
    float a_delta_input;            /* Small fiber (fast pain) */
    float a_beta_input;             /* Large fiber (touch) */

    /* Transmission cell */
    float transmission_cell_activity;

    /* Inhibitory interneuron */
    float sg_cell_activity;         /* Substantia gelatinosa */

    /* Descending modulation */
    float descending_input;
    descending_mod_t descending_source;

    /* Temporal integration */
    float* recent_touch_input;
    float* recent_pain_input;
    uint32_t history_length;
} gate_control_state_t;

/**
 * @brief Referred pain zone mapping
 */
typedef struct {
    body_segment_t visceral_source; /* Internal organ */
    body_segment_t referred_target; /* Skin area */
    float probability;              /* Referral probability */
    char description[64];
} referred_pain_zone_t;

/**
 * @brief Descending modulation state
 */
typedef struct {
    /* PAG state */
    float pag_activation;
    float pag_analgesia;

    /* Endogenous opioids */
    float endorphin_level;
    float enkephalin_level;

    /* Noradrenergic */
    float norepinephrine_level;

    /* Serotonergic */
    float serotonin_level;

    /* Total descending inhibition */
    float total_inhibition;

    /* Timing */
    uint64_t last_activation;
    float decay_rate;
} descending_modulation_state_t;

/**
 * @brief Pain memory entry
 */
typedef struct {
    body_segment_t segment;
    float max_intensity_experienced;
    float avg_intensity;
    uint32_t event_count;
    float sensitization_history;
    uint64_t last_occurrence;
    float memory_strength;          /* Decays over time */
} pain_memory_entry_t;

/**
 * @brief Pain processing context
 */
typedef struct {
    pain_config_t config;

    /* Nociceptors */
    nociceptor_t* nociceptors;
    uint32_t num_nociceptors;

    /* Active events */
    pain_event_detailed_t events[PAIN_MAX_EVENTS];
    uint32_t num_events;

    /* Gate control */
    gate_control_state_t gate_states[BODY_SEG_COUNT];

    /* Referred pain mapping */
    referred_pain_zone_t referred_zones[PAIN_MAX_REFERRED_ZONES];
    uint32_t num_referred_zones;

    /* Descending modulation */
    descending_modulation_state_t descending_state;

    /* Pain memory */
    pain_memory_entry_t pain_memory[BODY_SEG_COUNT];

    /* Global state */
    float total_pain_level;
    float emotional_suffering;
    bool chronic_pain_active;
    chronic_pain_state_t chronic_state;

    /* Statistics */
    uint32_t events_processed;
    float avg_pain_intensity;
    float peak_pain_intensity;
} pain_ctx_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default pain configuration
 */
pain_config_t pain_default_config(void);

/**
 * @brief Create pain processing context
 */
pain_ctx_t* pain_create(const pain_config_t* config);

/**
 * @brief Destroy pain context
 */
void pain_destroy(pain_ctx_t* ctx);

/**
 * @brief Reset pain context
 */
int pain_reset(pain_ctx_t* ctx);

/**
 * @brief Initialize nociceptors for segment
 */
int pain_init_segment(pain_ctx_t* ctx,
                      body_segment_t segment,
                      uint32_t num_nociceptors);

/*=============================================================================
 * PAIN EVENT PROCESSING
 *===========================================================================*/

/**
 * @brief Process new pain stimulus
 */
int pain_process_stimulus(pain_ctx_t* ctx,
                          body_segment_t segment,
                          const float* position,
                          float intensity,
                          pain_modality_t modality,
                          uint32_t* event_id_out);

/**
 * @brief Get detailed pain event
 */
int pain_get_event(pain_ctx_t* ctx,
                   uint32_t event_id,
                   pain_event_detailed_t* event);

/**
 * @brief Update pain event (for ongoing stimuli)
 */
int pain_update_event(pain_ctx_t* ctx,
                      uint32_t event_id,
                      float new_intensity);

/**
 * @brief End pain event
 */
int pain_end_event(pain_ctx_t* ctx, uint32_t event_id);

/**
 * @brief Get all active pain events
 */
int pain_get_active_events(pain_ctx_t* ctx,
                           pain_event_detailed_t* events,
                           uint32_t max_events,
                           uint32_t* num_events);

/*=============================================================================
 * GATE CONTROL FUNCTIONS
 *===========================================================================*/

/**
 * @brief Apply touch-based gate control
 */
int pain_apply_touch_gate(pain_ctx_t* ctx,
                          body_segment_t segment,
                          float touch_intensity);

/**
 * @brief Apply descending modulation
 */
int pain_apply_descending_modulation(pain_ctx_t* ctx,
                                     body_segment_t segment,
                                     descending_mod_t source,
                                     float modulation_strength);

/**
 * @brief Get current gate state
 */
float pain_get_gate_state(pain_ctx_t* ctx, body_segment_t segment);

/**
 * @brief Compute gated pain intensity
 */
float pain_compute_gated_intensity(pain_ctx_t* ctx,
                                   uint32_t event_id);

/**
 * @brief Update gate control (call each timestep)
 */
int pain_update_gate_control(pain_ctx_t* ctx, float dt);

/*=============================================================================
 * REFERRED PAIN
 *===========================================================================*/

/**
 * @brief Check for referred pain
 */
int pain_check_referred(pain_ctx_t* ctx,
                        body_segment_t source,
                        body_segment_t* referred_target,
                        float* probability);

/**
 * @brief Add referred pain zone mapping
 */
int pain_add_referred_zone(pain_ctx_t* ctx,
                           body_segment_t source,
                           body_segment_t target,
                           float probability,
                           const char* description);

/**
 * @brief Get referred pain source from target location
 */
int pain_get_referred_source(pain_ctx_t* ctx,
                             body_segment_t target,
                             body_segment_t* possible_sources,
                             float* probabilities,
                             uint32_t max_sources,
                             uint32_t* num_sources);

/*=============================================================================
 * DESCENDING MODULATION
 *===========================================================================*/

/**
 * @brief Activate PAG-mediated analgesia
 */
int pain_activate_pag_analgesia(pain_ctx_t* ctx, float activation_level);

/**
 * @brief Release endorphins
 */
int pain_release_endorphins(pain_ctx_t* ctx, float amount);

/**
 * @brief Get descending modulation state
 */
int pain_get_descending_state(pain_ctx_t* ctx,
                              descending_modulation_state_t* state);

/**
 * @brief Apply stress-induced analgesia
 */
int pain_apply_stress_analgesia(pain_ctx_t* ctx, float stress_level);

/**
 * @brief Update descending modulation (decay)
 */
int pain_update_descending(pain_ctx_t* ctx, float dt);

/*=============================================================================
 * COGNITIVE/EMOTIONAL MODULATION
 *===========================================================================*/

/**
 * @brief Apply attention modulation
 */
int pain_apply_attention(pain_ctx_t* ctx,
                         uint32_t event_id,
                         float attention_level);

/**
 * @brief Apply distraction effect
 */
int pain_apply_distraction(pain_ctx_t* ctx,
                           uint32_t event_id,
                           float distraction_level);

/**
 * @brief Apply expectation modulation
 */
int pain_apply_expectation(pain_ctx_t* ctx,
                           uint32_t event_id,
                           float expected_intensity);

/**
 * @brief Apply placebo effect
 */
int pain_apply_placebo(pain_ctx_t* ctx, float placebo_strength);

/**
 * @brief Apply nocebo effect
 */
int pain_apply_nocebo(pain_ctx_t* ctx, float nocebo_strength);

/**
 * @brief Compute emotional suffering component
 */
float pain_compute_suffering(pain_ctx_t* ctx, uint32_t event_id);

/*=============================================================================
 * SENSITIZATION
 *===========================================================================*/

/**
 * @brief Apply peripheral sensitization
 */
int pain_apply_peripheral_sensitization(pain_ctx_t* ctx,
                                        body_segment_t segment,
                                        float sensitization_factor);

/**
 * @brief Apply central sensitization
 */
int pain_apply_central_sensitization(pain_ctx_t* ctx,
                                     float sensitization_factor);

/**
 * @brief Get sensitization level
 */
float pain_get_sensitization(pain_ctx_t* ctx, body_segment_t segment);

/**
 * @brief Check for allodynia (pain from non-painful stimulus)
 */
bool pain_check_allodynia(pain_ctx_t* ctx,
                          body_segment_t segment,
                          float touch_intensity);

/**
 * @brief Check for hyperalgesia (increased pain sensitivity)
 */
bool pain_check_hyperalgesia(pain_ctx_t* ctx,
                             body_segment_t segment,
                             float pain_response,
                             float expected_response);

/*=============================================================================
 * CHRONIC PAIN
 *===========================================================================*/

/**
 * @brief Check if pain has become chronic
 */
bool pain_is_chronic(pain_ctx_t* ctx, body_segment_t segment);

/**
 * @brief Get chronic pain state
 */
chronic_pain_state_t pain_get_chronic_state(pain_ctx_t* ctx);

/**
 * @brief Update chronic pain state
 */
int pain_update_chronic(pain_ctx_t* ctx, float dt);

/*=============================================================================
 * PAIN MEMORY
 *===========================================================================*/

/**
 * @brief Record pain event to memory
 */
int pain_record_memory(pain_ctx_t* ctx,
                       body_segment_t segment,
                       float intensity);

/**
 * @brief Get pain memory for segment
 */
int pain_get_memory(pain_ctx_t* ctx,
                    body_segment_t segment,
                    pain_memory_entry_t* memory);

/**
 * @brief Update pain memory decay
 */
int pain_decay_memory(pain_ctx_t* ctx, float dt);

/*=============================================================================
 * AGGREGATE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get total pain level
 */
float pain_get_total_level(pain_ctx_t* ctx);

/**
 * @brief Get pain level by segment
 */
float pain_get_segment_level(pain_ctx_t* ctx, body_segment_t segment);

/**
 * @brief Get highest pain segment
 */
body_segment_t pain_get_max_segment(pain_ctx_t* ctx, float* intensity);

/**
 * @brief Get pain map (all segments)
 */
int pain_get_pain_map(pain_ctx_t* ctx,
                      float* levels,
                      uint32_t max_segments);

/*=============================================================================
 * UPDATE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update pain processing (call each timestep)
 */
int pain_update(pain_ctx_t* ctx, float dt);

/**
 * @brief Get pain output for downstream processing
 */
int pain_get_output(pain_ctx_t* ctx,
                    float* output,
                    uint32_t max_dim,
                    uint32_t* actual_dim);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* pain_fiber_name(pain_fiber_t fiber);
const char* pain_modality_name(pain_modality_t modality);
const char* pain_chronic_state_name(chronic_pain_state_t state);
const char* pain_stage_name(pain_stage_t stage);

/**
 * @brief Convert intensity to verbal rating
 */
const char* pain_intensity_verbal(float intensity);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PAIN_PROCESSING_H */
