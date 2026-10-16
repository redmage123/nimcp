//=============================================================================
// nimcp_substantia_nigra.h - Substantia Nigra (SNc/SNr)
//=============================================================================
/**
 * @file nimcp_substantia_nigra.h
 * @brief Substantia nigra implementation (pars compacta and pars reticulata)
 *
 * WHAT: Substantia nigra model with dopamine production (SNc) and output (SNr)
 * WHY:  SNc provides dopamine for reward learning; SNr is BG output nucleus
 * HOW:  SNc neurons signal reward prediction error; SNr similar to GPi
 *
 * BIOLOGICAL BASIS:
 * - SNc (pars compacta): Contains dopaminergic neurons projecting to striatum
 *   - Fires tonically at ~4-5 Hz baseline
 *   - Phasic bursts (15-20 Hz) for positive RPE (reward > expected)
 *   - Phasic pauses (<1 Hz) for negative RPE (reward < expected)
 * - SNr (pars reticulata): GABAergic output, functionally similar to GPi
 *   - Projects to thalamus (VA/VL) and superior colliculus
 *   - High tonic firing provides constant inhibition
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#ifndef NIMCP_SUBSTANTIA_NIGRA_H
#define NIMCP_SUBSTANTIA_NIGRA_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define SN_MAX_NEURONS 256              /**< Maximum SN neurons */
#define SN_DEFAULT_NEURONS 64           /**< Default number of neurons */
#define SNC_TONIC_FIRING 4.5f           /**< SNc tonic firing rate (Hz) */
#define SNC_BURST_FIRING 18.0f          /**< SNc burst firing rate (Hz) */
#define SNC_PAUSE_FIRING 0.5f           /**< SNc pause firing rate (Hz) */
#define SNR_TONIC_FIRING 70.0f          /**< SNr tonic firing rate (Hz) */
#define DOPAMINE_RELEASE_RATE 0.1f      /**< Dopamine release per spike */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Substantia nigra part
 */
typedef enum {
    SN_PART_COMPACTA = 0,   /**< Pars compacta (dopamine) */
    SN_PART_RETICULATA      /**< Pars reticulata (output) */
} sn_part_t;

/**
 * @brief Dopamine neuron firing state
 */
typedef enum {
    DA_STATE_TONIC = 0,     /**< Tonic baseline firing */
    DA_STATE_BURST,         /**< Phasic burst (positive RPE) */
    DA_STATE_PAUSE          /**< Phasic pause (negative RPE) */
} da_firing_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Dopaminergic neuron (SNc)
 */
typedef struct {
    uint32_t id;                /**< Neuron ID */
    float firing_rate;          /**< Current firing rate (Hz) */
    da_firing_state_t state;    /**< Current state */
    float dopamine_released;    /**< Amount of dopamine released */
    float membrane_potential;   /**< Membrane potential */
} da_neuron_t;

/**
 * @brief SNr output neuron
 */
typedef struct {
    uint32_t id;                /**< Neuron ID */
    float firing_rate;          /**< Current firing rate (Hz) */
    float inhibition;           /**< Current inhibition level */
    uint32_t target_action;     /**< Action this neuron maps to */
} snr_neuron_t;

/**
 * @brief Substantia nigra configuration
 */
typedef struct {
    sn_part_t part;                 /**< SNc or SNr */
    uint32_t num_neurons;           /**< Number of neurons */
    uint32_t num_actions;           /**< Number of actions (SNr) */
    float tonic_firing_rate;        /**< Baseline tonic firing (Hz) */
    float burst_firing_rate;        /**< Burst firing rate (SNc) */
    float pause_firing_rate;        /**< Pause firing rate (SNc) */
    float dopamine_release_rate;    /**< DA release per spike (SNc) */
    float rpe_burst_threshold;      /**< RPE threshold for burst */
    float rpe_pause_threshold;      /**< RPE threshold for pause */
} substantia_nigra_config_t;

/**
 * @brief Substantia nigra statistics
 */
typedef struct {
    float avg_firing_rate;          /**< Average firing rate */
    float dopamine_level;           /**< Current dopamine level (SNc) */
    float avg_rpe;                  /**< Average reward prediction error */
    uint32_t burst_count;           /**< Number of bursts (SNc) */
    uint32_t pause_count;           /**< Number of pauses (SNc) */
} sn_stats_t;

/**
 * @brief Substantia nigra structure
 */
typedef struct {
    /* Properties */
    sn_part_t part;                 /**< SNc or SNr */

    /* SNc specific */
    da_neuron_t* da_neurons;        /**< Dopamine neurons (SNc only) */
    float dopamine_level;           /**< Current striatal dopamine level */
    float reward_prediction;        /**< Current reward prediction */
    float rpe;                      /**< Reward prediction error */
    da_firing_state_t da_state;     /**< Current DA neuron state */

    /* SNr specific */
    snr_neuron_t* snr_neurons;      /**< SNr output neurons */
    float* output;                  /**< Output levels (size num_actions) */
    uint32_t num_actions;           /**< Number of actions */

    /* Common */
    uint32_t num_neurons;           /**< Number of neurons */
    substantia_nigra_config_t config; /**< Configuration */

    /* Input buffers */
    float* striatal_input;          /**< Input from striatum (SNr) */
    float* stn_input;               /**< Input from STN (SNr) */

    /* Statistics */
    sn_stats_t stats;               /**< Runtime statistics */

    /* Thread safety */
    nimcp_mutex_t* mutex;           /**< Mutex for thread safety */
} substantia_nigra_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Initialize default configuration
 * @param config Configuration to initialize
 * @param part SNc or SNr
 */
void substantia_nigra_default_config(
    substantia_nigra_config_t* config,
    sn_part_t part
);

/**
 * @brief Create substantia nigra
 * @param config Configuration
 * @return New SN instance or NULL on failure
 */
substantia_nigra_t* substantia_nigra_create(const substantia_nigra_config_t* config);

/**
 * @brief Destroy substantia nigra
 * @param sn Instance to destroy
 */
void substantia_nigra_destroy(substantia_nigra_t* sn);

/**
 * @brief Reset to initial state
 * @param sn Instance to reset
 * @return 0 on success, negative on error
 */
int substantia_nigra_reset(substantia_nigra_t* sn);

//=============================================================================
// SNc (Dopamine) Functions
//=============================================================================

/**
 * @brief Update dopamine based on reward signal
 * @param sn SNc instance
 * @param reward Actual reward received [-1, 1]
 * @param expected Expected reward [-1, 1]
 * @return 0 on success, negative on error
 */
int snc_update_reward(
    substantia_nigra_t* sn,
    float reward,
    float expected
);

/**
 * @brief Get current dopamine level
 * @param sn SNc instance
 * @return Dopamine level [0-1]
 */
float snc_get_dopamine(const substantia_nigra_t* sn);

/**
 * @brief Get reward prediction error
 * @param sn SNc instance
 * @return Current RPE value
 */
float snc_get_rpe(const substantia_nigra_t* sn);

/**
 * @brief Get dopamine neuron state
 * @param sn SNc instance
 * @return Current dopamine state
 */
da_firing_state_t snc_get_state(const substantia_nigra_t* sn);

/**
 * @brief Set reward prediction
 * @param sn SNc instance
 * @param prediction Reward prediction value
 * @return 0 on success, negative on error
 */
int snc_set_prediction(substantia_nigra_t* sn, float prediction);

/**
 * @brief Step SNc simulation
 * @param sn SNc instance
 * @param dt Time step in milliseconds
 * @return 0 on success, negative on error
 */
int snc_step(substantia_nigra_t* sn, float dt);

//=============================================================================
// SNr (Output) Functions
//=============================================================================

/**
 * @brief Set striatal inhibition input
 * @param sn SNr instance
 * @param inhibition Inhibition levels (size num_actions)
 * @return 0 on success, negative on error
 */
int snr_set_striatal_input(
    substantia_nigra_t* sn,
    const float* inhibition
);

/**
 * @brief Set STN excitation input
 * @param sn SNr instance
 * @param excitation Excitation levels (size num_actions)
 * @return 0 on success, negative on error
 */
int snr_set_stn_input(
    substantia_nigra_t* sn,
    const float* excitation
);

/**
 * @brief Process SNr inputs and update output
 * @param sn SNr instance
 * @return 0 on success, negative on error
 */
int snr_process(substantia_nigra_t* sn);

/**
 * @brief Get SNr output (thalamic inhibition)
 * @param sn SNr instance
 * @param output Output buffer (size num_actions)
 * @return 0 on success, negative on error
 */
int snr_get_output(const substantia_nigra_t* sn, float* output);

/**
 * @brief Step SNr simulation
 * @param sn SNr instance
 * @param dt Time step in milliseconds
 * @return 0 on success, negative on error
 */
int snr_step(substantia_nigra_t* sn, float dt);

//=============================================================================
// Common Functions
//=============================================================================

/**
 * @brief Get statistics
 * @param sn SN instance
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int substantia_nigra_get_stats(const substantia_nigra_t* sn, sn_stats_t* stats);

/**
 * @brief Get part name
 * @param part SN part type
 * @return Part name string
 */
const char* substantia_nigra_part_name(sn_part_t part);

/**
 * @brief Get dopamine firing state name
 * @param state Dopamine firing state
 * @return State name string
 */
const char* da_firing_state_name(da_firing_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SUBSTANTIA_NIGRA_H */
