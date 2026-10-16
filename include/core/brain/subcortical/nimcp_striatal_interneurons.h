//=============================================================================
// nimcp_striatal_interneurons.h - Striatal Interneuron Networks
//=============================================================================
/**
 * @file nimcp_striatal_interneurons.h
 * @brief Striatal interneuron implementation for enhanced action selection
 *
 * BIOLOGICAL BASIS:
 * Striatal interneurons (~5% of striatal neurons) critically shape MSN activity:
 *
 * INTERNEURON TYPES:
 * 1. Fast-Spiking Interneurons (FSIs / PV+)
 *    - GABAergic, powerful feedforward inhibition
 *    - Receive cortical input, inhibit MSNs
 *    - Temporal precision, winner-take-all
 *
 * 2. Tonically Active Neurons (TANs / ChAT+)
 *    - Cholinergic, tonic firing ~5 Hz
 *    - Pause with salient stimuli
 *    - Gate DA effects, state transitions
 *
 * 3. Low-Threshold Spiking (LTS / SOM+)
 *    - GABAergic, dendritic inhibition
 *    - Modulate MSN integration
 *
 * 4. Neurogliaform (NGF)
 *    - GABAergic, volume transmission
 *    - Slow inhibition
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_STRIATAL_INTERNEURONS_H
#define NIMCP_STRIATAL_INTERNEURONS_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define SINT_DEFAULT_FSI_COUNT      32      /**< Fast-spiking interneurons */
#define SINT_DEFAULT_TAN_COUNT      16      /**< Tonically active neurons */
#define SINT_DEFAULT_LTS_COUNT      16      /**< Low-threshold spiking */
#define SINT_DEFAULT_NGF_COUNT      8       /**< Neurogliaform */

/** FSI parameters */
#define SINT_FSI_MAX_RATE_HZ        150.0f  /**< Max firing rate */
#define SINT_FSI_THRESHOLD          0.4f    /**< Activation threshold */
#define SINT_FSI_INHIBITION_WEIGHT  0.8f    /**< Inhibition strength */

/** TAN parameters */
#define SINT_TAN_TONIC_RATE_HZ      5.0f    /**< Baseline firing */
#define SINT_TAN_PAUSE_DURATION_MS  200.0f  /**< Pause duration */
#define SINT_TAN_ACH_RELEASE        0.5f    /**< ACh release per spike */

/** LTS parameters */
#define SINT_LTS_THRESHOLD          0.3f
#define SINT_LTS_BURST_RATE_HZ      30.0f

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief Interneuron type
 */
typedef enum {
    SINT_TYPE_FSI,                  /**< Fast-spiking (PV+) */
    SINT_TYPE_TAN,                  /**< Tonically active (ChAT+) */
    SINT_TYPE_LTS,                  /**< Low-threshold spiking (SOM+) */
    SINT_TYPE_NGF,                  /**< Neurogliaform */
    SINT_TYPE_COUNT
} sint_type_t;

/**
 * @brief TAN state
 */
typedef enum {
    SINT_TAN_STATE_TONIC,           /**< Normal tonic firing */
    SINT_TAN_STATE_PAUSING,         /**< Pause in progress */
    SINT_TAN_STATE_BURST,           /**< Post-pause burst */
    SINT_TAN_STATE_RECOVERING,      /**< Returning to tonic */
    SINT_TAN_STATE_COUNT
} sint_tan_state_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief FSI state
 */
typedef struct {
    float activation;               /**< Current activation */
    float firing_rate;              /**< Current firing rate */
    float* msn_inhibition;          /**< Inhibition to each MSN */
    uint32_t num_msn_targets;
    float feedforward_delay_ms;     /**< Delay from cortical input */
} sint_fsi_state_t;

/**
 * @brief TAN state
 */
typedef struct {
    sint_tan_state_t state;
    float tonic_rate;
    float pause_timer;              /**< Remaining pause time */
    float ach_level;                /**< Current ACh release */
    float pause_depth;              /**< How complete is pause */
    bool salience_triggered;        /**< Was pause triggered by salience */
} sint_tan_unit_t;

/**
 * @brief LTS state
 */
typedef struct {
    float activation;
    float dendritic_inhibition;     /**< Inhibition to MSN dendrites */
    bool is_bursting;
} sint_lts_state_t;

/**
 * @brief NGF state
 */
typedef struct {
    float activation;
    float gaba_volume;              /**< Volume transmission GABA */
    float decay_rate;
} sint_ngf_state_t;

/**
 * @brief Interneuron network configuration
 */
typedef struct {
    uint32_t num_fsi;
    uint32_t num_tan;
    uint32_t num_lts;
    uint32_t num_ngf;
    uint32_t num_msn;               /**< Number of MSNs to innervate */

    float fsi_cortical_weight;
    float fsi_lateral_inhibition;
    float tan_pause_threshold;
    float tan_da_sensitivity;

    bool enable_fsi_wta;            /**< Enable winner-take-all */
    bool enable_tan_gating;         /**< Enable ACh gating of DA */
} sint_config_t;

/**
 * @brief Network output
 */
typedef struct {
    float* msn_inhibition;          /**< Total inhibition per MSN */
    float ach_level;                /**< Aggregate ACh level */
    float fsi_population_rate;      /**< Mean FSI firing rate */
    float tan_population_rate;      /**< Mean TAN firing rate */
    bool tan_pausing;               /**< Any TANs in pause */
    float wta_strength;             /**< Winner-take-all strength */
} sint_output_t;

/**
 * @brief Statistics
 */
typedef struct {
    float avg_fsi_rate;
    float avg_tan_rate;
    uint32_t pause_count;
    float avg_pause_duration;
    float ach_modulation;
    float inhibition_strength;
} sint_stats_t;

/**
 * @brief Main handle
 */
typedef struct striatal_interneurons striatal_interneurons_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

void sint_default_config(sint_config_t* config);
striatal_interneurons_t* sint_create(const sint_config_t* config);
void sint_destroy(striatal_interneurons_t* sint);
int sint_reset(striatal_interneurons_t* sint);

/* ============================================================================
 * INPUT API
 * ============================================================================ */

/**
 * @brief Set cortical input (drives FSIs)
 */
int sint_set_cortical_input(striatal_interneurons_t* sint,
                             const float* cortical,
                             uint32_t size);

/**
 * @brief Set dopamine level (modulates TANs)
 */
int sint_set_dopamine(striatal_interneurons_t* sint, float dopamine);

/**
 * @brief Signal salient event (triggers TAN pause)
 */
int sint_signal_salience(striatal_interneurons_t* sint, float salience);

/**
 * @brief Set thalamic input (can trigger TAN burst)
 */
int sint_set_thalamic_input(striatal_interneurons_t* sint, float input);

/* ============================================================================
 * PROCESSING API
 * ============================================================================ */

/**
 * @brief Step interneuron dynamics
 */
int sint_step(striatal_interneurons_t* sint, float dt_ms);

/**
 * @brief Get output to MSNs
 */
int sint_get_output(const striatal_interneurons_t* sint, sint_output_t* output);

/**
 * @brief Get inhibition for specific MSN
 */
float sint_get_msn_inhibition(const striatal_interneurons_t* sint,
                               uint32_t msn_idx);

/**
 * @brief Get ACh level for DA gating
 */
float sint_get_ach_level(const striatal_interneurons_t* sint);

/**
 * @brief Check if TANs are pausing
 */
bool sint_is_tan_pausing(const striatal_interneurons_t* sint);

/* ============================================================================
 * FSI-SPECIFIC API
 * ============================================================================ */

/**
 * @brief Get FSI population activity
 */
int sint_get_fsi_activity(const striatal_interneurons_t* sint,
                           float* activity,
                           uint32_t* count);

/**
 * @brief Apply lateral inhibition between FSIs
 */
int sint_apply_fsi_lateral_inhibition(striatal_interneurons_t* sint);

/**
 * @brief Get winner-take-all result
 */
int sint_get_wta_winner(const striatal_interneurons_t* sint,
                         uint32_t* winner_idx,
                         float* winner_strength);

/* ============================================================================
 * TAN-SPECIFIC API
 * ============================================================================ */

/**
 * @brief Get TAN population state
 */
sint_tan_state_t sint_get_tan_state(const striatal_interneurons_t* sint);

/**
 * @brief Force TAN pause
 */
int sint_force_tan_pause(striatal_interneurons_t* sint, float duration_ms);

/**
 * @brief Get TAN pause depth
 */
float sint_get_tan_pause_depth(const striatal_interneurons_t* sint);

/* ============================================================================
 * QUERY API
 * ============================================================================ */

int sint_get_stats(const striatal_interneurons_t* sint, sint_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STRIATAL_INTERNEURONS_H */
