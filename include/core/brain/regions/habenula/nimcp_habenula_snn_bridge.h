/**
 * @file nimcp_habenula_snn_bridge.h
 * @brief Habenula - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Bidirectional bridge between Habenula (aversive learning) system and SNN
 * WHY:  Enable spike-based aversive processing and bio-plausible avoidance learning
 * HOW:  Convert aversive states to spikes, decode SNN output to avoidance signals
 *
 * THEORETICAL FOUNDATIONS:
 * - Matsumoto & Hikosaka (2007): Habenula encodes negative reward
 * - Proulx et al. (2014): Habenula in depression and aversion
 * - Hikosaka (2010): Habenula as the "anti-reward" center
 *
 * BIOLOGICAL BASIS:
 * - LHb neurons encode negative RPE (opposite of VTA)
 * - Habenula inhibits VTA/Raphe during aversive events
 * - Firing increases with disappointment, decreases with reward
 * - Critical for learning what to avoid
 *
 * INTEGRATION FLOWS:
 *
 * Habenula --> SNN:
 *   1. Aversive signals encoded as spike volleys
 *   2. Disappointment encoded as elevated firing rate
 *   3. Relief encoded as firing pause
 *   4. Error signals encoded as population patterns
 *
 * SNN --> Habenula:
 *   1. Network activity signals prediction errors
 *   2. Avoidance decisions affect Habenula state
 *   3. Successful avoidance triggers relief
 *   4. Failure patterns trigger learning signals
 *
 * @see nimcp_habenula.h
 * @see nimcp_habenula_adapter.h
 * @see nimcp_snn.h
 */

#ifndef NIMCP_HABENULA_SNN_BRIDGE_H
#define NIMCP_HABENULA_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_habenula_adapter_impl;
typedef struct nimcp_habenula_adapter_impl* nimcp_habenula_adapter_t;
struct nimcp_snn_network;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default neurons in Habenula population representation */
#define HABENULA_SNN_POPULATION_SIZE    32

/** @brief Default input dimension for aversive state encoding */
#define HABENULA_SNN_INPUT_DIM          64

/** @brief Bio-async module ID for Habenula-SNN bridge */
#define BIO_MODULE_HABENULA_SNN_BRIDGE  0x0F00

/** @brief Default simulation timestep (ms) */
#define HABENULA_SNN_DEFAULT_DT         1.0f

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Spike encoding method for aversive state
 */
typedef enum {
    HABENULA_SNN_ENCODE_RATE = 0,    /**< Rate coding (aversion = rate) */
    HABENULA_SNN_ENCODE_NEGATIVE_RPE,/**< Negative RPE encoding */
    HABENULA_SNN_ENCODE_POPULATION,  /**< Population vector coding */
    HABENULA_SNN_ENCODE_TEMPORAL     /**< Temporal pattern coding */
} nimcp_habenula_snn_encoding_t;

/**
 * @brief Decoding method for SNN output
 */
typedef enum {
    HABENULA_SNN_DECODE_AVERAGE = 0, /**< Average firing rate */
    HABENULA_SNN_DECODE_AVOIDANCE,   /**< Avoidance signal level */
    HABENULA_SNN_DECODE_POPULATION,  /**< Population vector decoding */
    HABENULA_SNN_DECODE_TEMPORAL     /**< Temporal pattern matching */
} nimcp_habenula_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    HABENULA_SNN_STATE_IDLE = 0,
    HABENULA_SNN_STATE_ENCODING,
    HABENULA_SNN_STATE_SIMULATING,
    HABENULA_SNN_STATE_DECODING,
    HABENULA_SNN_STATE_DISABLED
} nimcp_habenula_snn_state_t;

/**
 * @brief Aversive event types for encoding
 */
typedef enum {
    HABENULA_SNN_AVERSIVE_NONE = 0,
    HABENULA_SNN_AVERSIVE_PUNISHMENT,
    HABENULA_SNN_AVERSIVE_DISAPPOINTMENT,
    HABENULA_SNN_AVERSIVE_FRUSTRATION,
    HABENULA_SNN_AVERSIVE_OMISSION
} nimcp_habenula_snn_aversive_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Habenula-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t population_size;        /**< Habenula population neurons */
    uint32_t input_dim;              /**< Input feature dimension */
    uint32_t output_dim;             /**< Output modulation dimension */

    /* Encoding parameters */
    nimcp_habenula_snn_encoding_t encoding; /**< Spike encoding method */
    float encoding_gain;             /**< Aversion-to-spike gain */
    float baseline_rate_hz;          /**< Baseline firing rate */
    float aversive_rate_boost;       /**< Rate boost for aversive events */
    float relief_suppression;        /**< Suppression for relief */

    /* Decoding parameters */
    nimcp_habenula_snn_decoding_t decoding; /**< Output decoding method */
    float decoding_threshold;        /**< Activation threshold */
    float temporal_smoothing;        /**< Temporal averaging alpha */

    /* Avoidance output */
    bool enable_avoidance_output;    /**< Enable avoidance signals */
    float avoidance_gain;            /**< Avoidance signal gain */
    float inhibition_strength;       /**< VTA/Raphe inhibition strength */

    /* Error signal output */
    bool enable_error_output;        /**< Enable error signals */
    float error_amplification;       /**< Error signal amplification */

    /* Simulation */
    float dt_ms;                     /**< Simulation timestep */
    float simulation_window_ms;      /**< Simulation window per update */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_plasticity_bridge;   /**< Enable plasticity integration */
} nimcp_habenula_snn_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Current Habenula encoding state
 */
typedef struct {
    float aversive_level;            /**< Current aversive level [0, 1] */
    float negative_rpe;              /**< Negative reward prediction error */
    float disappointment;            /**< Disappointment level [0, 1] */
    float relief;                    /**< Relief level [0, 1] */
    nimcp_habenula_snn_aversive_t event_type; /**< Current aversive event */
    uint64_t last_aversive_us;       /**< Last aversive event timestamp */
} nimcp_habenula_snn_state_data_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_habenula_snn_state_t state; /**< Current operational state */
    nimcp_habenula_snn_state_data_t habenula; /**< Current Habenula state */
    float avg_firing_rate;           /**< Average network firing rate */
    float avoidance_output;          /**< Current avoidance output */
    float vta_inhibition;            /**< Current VTA inhibition */
    float raphe_inhibition;          /**< Current Raphe inhibition */
    bool bio_async_connected;        /**< Bio-async connection status */
} nimcp_habenula_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total update calls */
    uint64_t total_spikes_generated; /**< Total spikes generated */
    uint64_t aversive_events;        /**< Total aversive events */
    uint64_t relief_events;          /**< Total relief events */
    uint64_t avoidance_signals;      /**< Times avoidance signaled */
    float avg_aversive_level;        /**< Average aversive level */
    float avg_negative_rpe;          /**< Average negative RPE */
    float avg_processing_time_ms;    /**< Average processing time */
} nimcp_habenula_snn_stats_t;

/*=============================================================================
 * Modulation Output
 *===========================================================================*/

/**
 * @brief Avoidance modulation output
 */
typedef struct {
    float avoidance_signal;          /**< Avoidance drive [0, 1] */
    float vta_inhibition;            /**< VTA inhibition level [0, 1] */
    float raphe_inhibition;          /**< Raphe inhibition level [0, 1] */
    float error_signal;              /**< Prediction error signal */
    float frustration;               /**< Frustration level [0, 1] */
    bool trigger_avoidance;          /**< Trigger avoidance behavior */
} nimcp_habenula_snn_modulation_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_habenula_snn_bridge nimcp_habenula_snn_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
nimcp_habenula_snn_config_t nimcp_habenula_snn_config_default(void);

/**
 * @brief Create Habenula-SNN bridge
 */
nimcp_habenula_snn_bridge_t* nimcp_habenula_snn_create(
    const nimcp_habenula_snn_config_t* config
);

/**
 * @brief Destroy Habenula-SNN bridge
 */
void nimcp_habenula_snn_destroy(nimcp_habenula_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int nimcp_habenula_snn_reset(nimcp_habenula_snn_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to Habenula adapter
 */
int nimcp_habenula_snn_connect_habenula(
    nimcp_habenula_snn_bridge_t* bridge,
    nimcp_habenula_adapter_t habenula_adapter
);

/**
 * @brief Connect to SNN network
 */
int nimcp_habenula_snn_connect_snn(
    nimcp_habenula_snn_bridge_t* bridge,
    struct nimcp_snn_network* snn
);

/*=============================================================================
 * Encoding Functions (Habenula --> SNN)
 *===========================================================================*/

/**
 * @brief Encode current Habenula state as spike train
 */
int nimcp_habenula_snn_encode_state(nimcp_habenula_snn_bridge_t* bridge);

/**
 * @brief Encode aversive event
 */
int nimcp_habenula_snn_encode_aversive(
    nimcp_habenula_snn_bridge_t* bridge,
    nimcp_habenula_snn_aversive_t event_type,
    float intensity
);

/**
 * @brief Encode negative RPE
 */
int nimcp_habenula_snn_encode_negative_rpe(
    nimcp_habenula_snn_bridge_t* bridge,
    float negative_rpe
);

/**
 * @brief Encode disappointment (expected reward not received)
 */
int nimcp_habenula_snn_encode_disappointment(
    nimcp_habenula_snn_bridge_t* bridge,
    float expected_reward,
    float actual_reward
);

/**
 * @brief Encode relief (aversive outcome avoided)
 */
int nimcp_habenula_snn_encode_relief(
    nimcp_habenula_snn_bridge_t* bridge,
    float relief_magnitude
);

/*=============================================================================
 * Simulation Functions
 *===========================================================================*/

/**
 * @brief Run SNN simulation step
 */
int nimcp_habenula_snn_simulate(
    nimcp_habenula_snn_bridge_t* bridge,
    float duration_ms
);

/**
 * @brief Process single timestep
 */
int nimcp_habenula_snn_step(nimcp_habenula_snn_bridge_t* bridge);

/*=============================================================================
 * Decoding Functions (SNN --> Habenula)
 *===========================================================================*/

/**
 * @brief Get avoidance modulation from SNN output
 */
int nimcp_habenula_snn_get_modulation(
    nimcp_habenula_snn_bridge_t* bridge,
    nimcp_habenula_snn_modulation_t* modulation
);

/**
 * @brief Get avoidance signal
 */
float nimcp_habenula_snn_get_avoidance(nimcp_habenula_snn_bridge_t* bridge);

/**
 * @brief Get VTA inhibition level
 */
float nimcp_habenula_snn_get_vta_inhibition(nimcp_habenula_snn_bridge_t* bridge);

/**
 * @brief Get Raphe inhibition level
 */
float nimcp_habenula_snn_get_raphe_inhibition(nimcp_habenula_snn_bridge_t* bridge);

/**
 * @brief Check if avoidance behavior should trigger
 */
bool nimcp_habenula_snn_should_avoid(nimcp_habenula_snn_bridge_t* bridge);

/*=============================================================================
 * State and Statistics
 *===========================================================================*/

/**
 * @brief Get bridge state
 */
int nimcp_habenula_snn_get_state(
    const nimcp_habenula_snn_bridge_t* bridge,
    nimcp_habenula_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int nimcp_habenula_snn_get_stats(
    const nimcp_habenula_snn_bridge_t* bridge,
    nimcp_habenula_snn_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void nimcp_habenula_snn_reset_stats(nimcp_habenula_snn_bridge_t* bridge);

/*=============================================================================
 * Bio-Async Integration
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 */
int nimcp_habenula_snn_connect_bio_async(nimcp_habenula_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int nimcp_habenula_snn_disconnect_bio_async(nimcp_habenula_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool nimcp_habenula_snn_is_bio_async_connected(const nimcp_habenula_snn_bridge_t* bridge);

/*=============================================================================
 * External Modulation
 *===========================================================================*/

/**
 * @brief Set punishment signal
 */
int nimcp_habenula_snn_set_punishment(
    nimcp_habenula_snn_bridge_t* bridge,
    float punishment
);

/**
 * @brief Set reward omission
 */
int nimcp_habenula_snn_set_omission(
    nimcp_habenula_snn_bridge_t* bridge,
    float expected_reward
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HABENULA_SNN_BRIDGE_H */
