/**
 * @file nimcp_habenula.h
 * @brief Habenula system - Aversive learning and disappointment signaling
 *
 * The habenula is a small bilateral structure in the epithalamus that serves
 * as a critical hub for encoding negative outcomes and guiding avoidance behavior.
 *
 * Two main subdivisions:
 * - Lateral Habenula (LHb): Encodes negative reward prediction errors,
 *   disappointment, and inhibits dopamine neurons in VTA
 * - Medial Habenula (MHb): Involved in aversion, withdrawal, and
 *   interpeduncular nucleus projections
 *
 * Key functions:
 * - Negative reward prediction error computation
 * - Avoidance learning and aversive memory
 * - VTA dopamine inhibition during negative outcomes
 * - Depression/learned helplessness modeling
 * - Raphe nuclei modulation
 */

#ifndef NIMCP_HABENULA_H
#define NIMCP_HABENULA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/* Default configuration values */
#define HABENULA_DEFAULT_BASELINE_FIRING        5.0f    /* Hz */
#define HABENULA_DEFAULT_MAX_FIRING             50.0f   /* Hz */
#define HABENULA_DEFAULT_LHB_WEIGHT             0.7f    /* LHb contribution */
#define HABENULA_DEFAULT_MHB_WEIGHT             0.3f    /* MHb contribution */
#define HABENULA_DEFAULT_VTA_INHIBITION_GAIN    0.8f    /* VTA inhibition strength */
#define HABENULA_DEFAULT_RAPHE_MODULATION_GAIN  0.5f    /* Raphe modulation strength */
#define HABENULA_DEFAULT_DISAPPOINTMENT_DECAY   0.1f    /* Decay rate per second */
#define HABENULA_DEFAULT_AVERSION_THRESHOLD     0.3f    /* Threshold for aversive response */

/* Maximum projections */
#define HABENULA_MAX_PROJECTIONS 16

/*=============================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    HABENULA_OK = 0,
    HABENULA_ERROR_NULL = -1,
    HABENULA_ERROR_NOT_INITIALIZED = -2,
    HABENULA_ERROR_ALREADY_INITIALIZED = -3,
    HABENULA_ERROR_INVALID_PARAM = -4,
    HABENULA_ERROR_RESOURCE = -5,
    HABENULA_ERROR_STATE = -6,
    HABENULA_ERROR_FULL = -7
} nimcp_habenula_error_t;

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * Habenula operating mode
 */
typedef enum {
    HABENULA_MODE_BASELINE,     /* Normal baseline activity */
    HABENULA_MODE_DISAPPOINTED, /* Active due to negative RPE */
    HABENULA_MODE_HYPERACTIVE,  /* Sustained hyperactivity (depression-like) */
    HABENULA_MODE_SUPPRESSED    /* Inhibited state (positive outcome) */
} nimcp_habenula_mode_t;

/**
 * Habenula subdivision
 */
typedef enum {
    HABENULA_REGION_LHB,        /* Lateral habenula */
    HABENULA_REGION_MHB,        /* Medial habenula */
    HABENULA_REGION_BOTH        /* Both subdivisions */
} nimcp_habenula_region_t;

/**
 * Projection target type
 */
typedef enum {
    HABENULA_TARGET_VTA,        /* Ventral tegmental area (inhibitory) */
    HABENULA_TARGET_RAPHE,      /* Raphe nuclei */
    HABENULA_TARGET_RMTg,       /* Rostromedial tegmental nucleus */
    HABENULA_TARGET_IPN,        /* Interpeduncular nucleus */
    HABENULA_TARGET_OTHER
} nimcp_habenula_target_t;

/**
 * System status
 */
typedef enum {
    HABENULA_STATUS_NORMAL,
    HABENULA_STATUS_HYPERACTIVE,    /* Depression-like state */
    HABENULA_STATUS_HYPOACTIVE,     /* Below normal activity */
    HABENULA_STATUS_UNSTABLE
} nimcp_habenula_status_t;

/*=============================================================================
 * Data Structures
 *===========================================================================*/

/**
 * Habenula configuration
 */
typedef struct {
    float baseline_firing_rate;     /* Baseline firing rate (Hz) */
    float max_firing_rate;          /* Maximum firing rate (Hz) */
    float lhb_weight;               /* LHb contribution weight */
    float mhb_weight;               /* MHb contribution weight */
    float vta_inhibition_gain;      /* Gain for VTA inhibition */
    float raphe_modulation_gain;    /* Gain for Raphe modulation */
    float disappointment_decay;     /* Decay rate for disappointment */
    float aversion_threshold;       /* Threshold for aversive response */
    bool enable_depression_model;   /* Enable learned helplessness */
    bool enable_vta_feedback;       /* Enable VTA feedback loop */
} nimcp_habenula_config_t;

/**
 * Lateral Habenula state
 */
typedef struct {
    float firing_rate;              /* Current firing rate (Hz) */
    float disappointment;           /* Current disappointment level [0,1] */
    float negative_rpe;             /* Most recent negative RPE */
    float expected_reward;          /* Expected reward value */
    float received_reward;          /* Actually received reward */
    float vta_inhibition_output;    /* Output signal to VTA */
    float cumulative_disappointment; /* Accumulated disappointment */
} nimcp_lhb_state_t;

/**
 * Medial Habenula state
 */
typedef struct {
    float firing_rate;              /* Current firing rate (Hz) */
    float aversion_level;           /* Current aversion [0,1] */
    float withdrawal_state;         /* Withdrawal-like state [0,1] */
    float ipn_output;               /* Output to interpeduncular nucleus */
    float nicotinic_sensitivity;    /* Nicotinic receptor sensitivity */
} nimcp_mhb_state_t;

/**
 * Neuron pool state
 */
typedef struct {
    float combined_firing_rate;     /* Combined LHb+MHb rate */
    float excitatory_input;         /* External excitation */
    float inhibitory_input;         /* External inhibition */
    float adaptation;               /* Firing rate adaptation */
    uint32_t burst_count;           /* Number of bursts detected */
} nimcp_habenula_neurons_t;

/**
 * Projection to target structure
 */
typedef struct {
    nimcp_habenula_target_t target;
    nimcp_habenula_region_t source;
    float weight;                   /* Projection strength */
    float delay_ms;                 /* Transmission delay */
    bool is_inhibitory;             /* True for inhibitory projection */
    bool active;                    /* Whether projection is active */
    void* target_data;              /* Optional target data */
} nimcp_habenula_projection_t;

/**
 * Depression/learned helplessness model
 */
typedef struct {
    float helplessness_index;       /* Learned helplessness [0,1] */
    float chronic_hyperactivity;    /* Sustained hyperactivity duration */
    float coping_failure_count;     /* Count of coping failures */
    float anhedonia_level;          /* Inability to feel pleasure [0,1] */
    bool is_depressed;              /* Depression state flag */
    float recovery_rate;            /* Rate of recovery from depression */
} nimcp_depression_model_t;

/**
 * Performance metrics
 */
typedef struct {
    uint64_t update_count;
    uint64_t disappointment_events;
    uint64_t aversion_events;
    uint64_t vta_inhibitions;
    float avg_negative_rpe;
    float peak_disappointment;
    float total_aversion_time;
} nimcp_habenula_metrics_t;

/**
 * Main Habenula system structure
 */
typedef struct {
    nimcp_habenula_config_t config;
    nimcp_lhb_state_t lhb;                  /* Lateral habenula state */
    nimcp_mhb_state_t mhb;                  /* Medial habenula state */
    nimcp_habenula_neurons_t neurons;        /* Combined neuron state */
    nimcp_depression_model_t depression;     /* Depression model */
    nimcp_habenula_mode_t mode;
    nimcp_habenula_projection_t projections[HABENULA_MAX_PROJECTIONS];
    uint32_t projection_count;
    nimcp_habenula_metrics_t metrics;
    float simulation_time;
    float mode_duration;
    bool initialized;
} nimcp_habenula_system_t;

/*=============================================================================
 * Configuration API
 *===========================================================================*/

/**
 * Get default configuration
 */
void nimcp_habenula_default_config(nimcp_habenula_config_t* config);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * Initialize habenula system
 * @param habenula System structure to initialize
 * @param config Configuration (NULL for defaults)
 * @return HABENULA_OK on success
 */
nimcp_habenula_error_t nimcp_habenula_init(nimcp_habenula_system_t* habenula,
                                           const nimcp_habenula_config_t* config);

/**
 * Shutdown habenula system
 */
nimcp_habenula_error_t nimcp_habenula_shutdown(nimcp_habenula_system_t* habenula);

/**
 * Reset habenula to initial state
 */
nimcp_habenula_error_t nimcp_habenula_reset(nimcp_habenula_system_t* habenula);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * Update habenula state
 * @param habenula System structure
 * @param dt_ms Time step in milliseconds
 * @return HABENULA_OK on success
 */
nimcp_habenula_error_t nimcp_habenula_update(nimcp_habenula_system_t* habenula,
                                              float dt_ms);

/*=============================================================================
 * Reward Prediction Error API
 *===========================================================================*/

/**
 * Compute negative reward prediction error
 * @param habenula System structure
 * @param expected Expected reward value
 * @param received Actually received reward
 * @param negative_rpe Output: computed negative RPE (0 if positive)
 * @return HABENULA_OK on success
 */
nimcp_habenula_error_t nimcp_habenula_compute_negative_rpe(
    nimcp_habenula_system_t* habenula,
    float expected,
    float received,
    float* negative_rpe);

/**
 * Process reward outcome
 * @param habenula System structure
 * @param expected Expected reward
 * @param received Received reward
 * @return HABENULA_OK on success
 */
nimcp_habenula_error_t nimcp_habenula_process_outcome(
    nimcp_habenula_system_t* habenula,
    float expected,
    float received);

/**
 * Get current disappointment level
 */
nimcp_habenula_error_t nimcp_habenula_get_disappointment(
    nimcp_habenula_system_t* habenula,
    float* disappointment);

/**
 * Apply aversive stimulus
 * @param habenula System structure
 * @param intensity Aversive intensity [0,1]
 */
nimcp_habenula_error_t nimcp_habenula_apply_aversive(
    nimcp_habenula_system_t* habenula,
    float intensity);

/*=============================================================================
 * VTA Inhibition API
 *===========================================================================*/

/**
 * Get VTA inhibition signal
 * @param habenula System structure
 * @param inhibition Output: inhibition signal [0,1]
 */
nimcp_habenula_error_t nimcp_habenula_get_vta_inhibition(
    nimcp_habenula_system_t* habenula,
    float* inhibition);

/**
 * Apply VTA feedback (dopamine level)
 * @param habenula System structure
 * @param da_level Current dopamine level
 */
nimcp_habenula_error_t nimcp_habenula_apply_vta_feedback(
    nimcp_habenula_system_t* habenula,
    float da_level);

/*=============================================================================
 * Raphe Modulation API
 *===========================================================================*/

/**
 * Get Raphe modulation signal
 * @param habenula System structure
 * @param modulation Output: modulation signal
 */
nimcp_habenula_error_t nimcp_habenula_get_raphe_modulation(
    nimcp_habenula_system_t* habenula,
    float* modulation);

/*=============================================================================
 * Avoidance Learning API
 *===========================================================================*/

/**
 * Get avoidance signal strength
 * @param habenula System structure
 * @param avoidance Output: avoidance signal [0,1]
 */
nimcp_habenula_error_t nimcp_habenula_get_avoidance_signal(
    nimcp_habenula_system_t* habenula,
    float* avoidance);

/**
 * Check if stimulus should be avoided
 * @param habenula System structure
 * @param stimulus_value Value associated with stimulus
 * @param should_avoid Output: true if should avoid
 */
nimcp_habenula_error_t nimcp_habenula_should_avoid(
    nimcp_habenula_system_t* habenula,
    float stimulus_value,
    bool* should_avoid);

/*=============================================================================
 * Depression Model API
 *===========================================================================*/

/**
 * Get learned helplessness index
 * @param habenula System structure
 * @param helplessness Output: helplessness level [0,1]
 */
nimcp_habenula_error_t nimcp_habenula_get_helplessness(
    nimcp_habenula_system_t* habenula,
    float* helplessness);

/**
 * Get anhedonia level
 * @param habenula System structure
 * @param anhedonia Output: anhedonia level [0,1]
 */
nimcp_habenula_error_t nimcp_habenula_get_anhedonia(
    nimcp_habenula_system_t* habenula,
    float* anhedonia);

/**
 * Check if system is in depressed state
 * @param habenula System structure
 * @param is_depressed Output: depression state
 */
nimcp_habenula_error_t nimcp_habenula_is_depressed(
    nimcp_habenula_system_t* habenula,
    bool* is_depressed);

/**
 * Record coping failure (contributes to learned helplessness)
 * @param habenula System structure
 */
nimcp_habenula_error_t nimcp_habenula_record_coping_failure(
    nimcp_habenula_system_t* habenula);

/**
 * Record coping success (reduces helplessness)
 * @param habenula System structure
 */
nimcp_habenula_error_t nimcp_habenula_record_coping_success(
    nimcp_habenula_system_t* habenula);

/*=============================================================================
 * Mode API
 *===========================================================================*/

/**
 * Get current operating mode
 */
nimcp_habenula_error_t nimcp_habenula_get_mode(nimcp_habenula_system_t* habenula,
                                                nimcp_habenula_mode_t* mode);

/**
 * Set operating mode
 */
nimcp_habenula_error_t nimcp_habenula_set_mode(nimcp_habenula_system_t* habenula,
                                                nimcp_habenula_mode_t mode);

/*=============================================================================
 * Input API
 *===========================================================================*/

/**
 * Apply excitatory input
 */
nimcp_habenula_error_t nimcp_habenula_apply_excitation(
    nimcp_habenula_system_t* habenula,
    float strength);

/**
 * Apply inhibitory input
 */
nimcp_habenula_error_t nimcp_habenula_apply_inhibition(
    nimcp_habenula_system_t* habenula,
    float strength);

/*=============================================================================
 * Firing Rate API
 *===========================================================================*/

/**
 * Get combined firing rate
 */
nimcp_habenula_error_t nimcp_habenula_get_firing_rate(
    nimcp_habenula_system_t* habenula,
    float* rate);

/**
 * Get region-specific firing rate
 */
nimcp_habenula_error_t nimcp_habenula_get_region_firing_rate(
    nimcp_habenula_system_t* habenula,
    nimcp_habenula_region_t region,
    float* rate);

/*=============================================================================
 * Projection API
 *===========================================================================*/

/**
 * Add projection to target structure
 */
nimcp_habenula_error_t nimcp_habenula_add_projection(
    nimcp_habenula_system_t* habenula,
    nimcp_habenula_target_t target,
    nimcp_habenula_region_t source,
    float weight,
    bool is_inhibitory);

/**
 * Get projection by index
 */
nimcp_habenula_error_t nimcp_habenula_get_projection(
    nimcp_habenula_system_t* habenula,
    uint32_t index,
    nimcp_habenula_projection_t* projection);

/**
 * Get output signal for target
 */
nimcp_habenula_error_t nimcp_habenula_get_output_to_target(
    nimcp_habenula_system_t* habenula,
    nimcp_habenula_target_t target,
    float* output);

/*=============================================================================
 * State API
 *===========================================================================*/

/**
 * Get system state
 */
nimcp_habenula_error_t nimcp_habenula_get_state(
    nimcp_habenula_system_t* habenula,
    float* firing_rate,
    float* disappointment,
    float* aversion,
    float* vta_inhibition);

/**
 * Get system status
 */
nimcp_habenula_error_t nimcp_habenula_get_status(
    nimcp_habenula_system_t* habenula,
    nimcp_habenula_status_t* status);

/*=============================================================================
 * Metrics API
 *===========================================================================*/

/**
 * Get performance metrics
 */
nimcp_habenula_error_t nimcp_habenula_get_metrics(
    nimcp_habenula_system_t* habenula,
    nimcp_habenula_metrics_t* metrics);

/**
 * Reset performance metrics
 */
nimcp_habenula_error_t nimcp_habenula_reset_metrics(
    nimcp_habenula_system_t* habenula);

/*=============================================================================
 * Utility API
 *===========================================================================*/

/**
 * Get error string
 */
const char* nimcp_habenula_error_string(nimcp_habenula_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HABENULA_H */
