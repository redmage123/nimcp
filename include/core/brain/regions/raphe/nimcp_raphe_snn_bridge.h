/**
 * @file nimcp_raphe_snn_bridge.h
 * @brief Raphe Nuclei - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Bidirectional bridge between Raphe (serotonin) system and SNN module
 * WHY:  Enable spike-based mood/impulse processing and bio-plausible regulation
 * HOW:  Convert 5-HT states to spikes, decode SNN output to behavioral modulation
 *
 * THEORETICAL FOUNDATIONS:
 * - Dayan & Huys (2008): Serotonin, inhibition, and negative mood
 * - Cools et al. (2008): 5-HT modulation of impulsivity
 * - Miyazaki et al. (2012): 5-HT and patience/waiting
 *
 * BIOLOGICAL BASIS:
 * - Raphe neurons fire tonically at 1-5 Hz
 * - 5-HT modulates temporal discounting
 * - Serotonin promotes behavioral inhibition
 * - 5-HT/DA balance affects risk-taking
 *
 * INTEGRATION FLOWS:
 *
 * Raphe --> SNN:
 *   1. Tonic 5-HT encoded as baseline firing rate
 *   2. Mood state encoded as population patterns
 *   3. Impulse control level modulates inhibitory gain
 *   4. Temporal horizon affects delay-related activity
 *
 * SNN --> Raphe:
 *   1. Network activity signals processing demands
 *   2. Decision patterns affect 5-HT release
 *   3. Error/conflict signals modulate mood
 *   4. Temporal patterns inform patience levels
 *
 * @see nimcp_raphe_nuclei.h
 * @see nimcp_raphe_adapter.h
 * @see nimcp_snn.h
 */

#ifndef NIMCP_RAPHE_SNN_BRIDGE_H
#define NIMCP_RAPHE_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_raphe_adapter;
typedef struct nimcp_raphe_adapter* nimcp_raphe_adapter_t;
struct nimcp_snn_network;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default neurons in Raphe population representation */
#define RAPHE_SNN_POPULATION_SIZE       32

/** @brief Default input dimension for 5-HT state encoding */
#define RAPHE_SNN_INPUT_DIM             64

/** @brief Bio-async module ID for Raphe-SNN bridge */
#define BIO_MODULE_RAPHE_SNN_BRIDGE     0x0E00

/** @brief Default simulation timestep (ms) */
#define RAPHE_SNN_DEFAULT_DT            1.0f

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Spike encoding method for 5-HT state
 */
typedef enum {
    RAPHE_SNN_ENCODE_RATE = 0,       /**< Rate coding (5-HT = rate) */
    RAPHE_SNN_ENCODE_MOOD,           /**< Mood-pattern encoding */
    RAPHE_SNN_ENCODE_POPULATION,     /**< Population vector coding */
    RAPHE_SNN_ENCODE_TEMPORAL        /**< Temporal pattern coding */
} nimcp_raphe_snn_encoding_t;

/**
 * @brief Decoding method for SNN output
 */
typedef enum {
    RAPHE_SNN_DECODE_AVERAGE = 0,    /**< Average firing rate */
    RAPHE_SNN_DECODE_INHIBITION,     /**< Inhibitory output level */
    RAPHE_SNN_DECODE_POPULATION,     /**< Population vector decoding */
    RAPHE_SNN_DECODE_TEMPORAL        /**< Temporal pattern matching */
} nimcp_raphe_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    RAPHE_SNN_STATE_IDLE = 0,
    RAPHE_SNN_STATE_ENCODING,
    RAPHE_SNN_STATE_SIMULATING,
    RAPHE_SNN_STATE_DECODING,
    RAPHE_SNN_STATE_DISABLED
} nimcp_raphe_snn_state_t;

/**
 * @brief Mood states for encoding
 */
typedef enum {
    RAPHE_SNN_MOOD_NEUTRAL = 0,
    RAPHE_SNN_MOOD_POSITIVE,
    RAPHE_SNN_MOOD_NEGATIVE,
    RAPHE_SNN_MOOD_ANXIOUS,
    RAPHE_SNN_MOOD_CALM
} nimcp_raphe_snn_mood_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Raphe-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t population_size;        /**< Raphe population neurons */
    uint32_t input_dim;              /**< Input feature dimension */
    uint32_t output_dim;             /**< Output modulation dimension */

    /* Encoding parameters */
    nimcp_raphe_snn_encoding_t encoding; /**< Spike encoding method */
    float encoding_gain;             /**< 5-HT-to-spike gain */
    float tonic_baseline_hz;         /**< Tonic baseline firing rate */
    float mood_encoding_scale;       /**< Mood encoding scale */

    /* Decoding parameters */
    nimcp_raphe_snn_decoding_t decoding; /**< Output decoding method */
    float decoding_threshold;        /**< Activation threshold */
    float temporal_smoothing;        /**< Temporal averaging alpha */

    /* Impulse control */
    bool enable_impulse_output;      /**< Enable impulse control output */
    float inhibition_gain;           /**< Inhibitory output gain */
    float patience_scale;            /**< Patience modulation scale */

    /* Mood modulation */
    bool enable_mood_output;         /**< Enable mood modulation */
    float mood_smoothing;            /**< Mood state smoothing */
    float mood_bias_scale;           /**< Mood bias scaling */

    /* Simulation */
    float dt_ms;                     /**< Simulation timestep */
    float simulation_window_ms;      /**< Simulation window per update */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_plasticity_bridge;   /**< Enable plasticity integration */
} nimcp_raphe_snn_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Current 5-HT encoding state
 */
typedef struct {
    float serotonin_level;           /**< Current 5-HT concentration */
    float mood_valence;              /**< Current mood valence [-1, 1] */
    float impulse_control;           /**< Impulse control level [0, 1] */
    float patience;                  /**< Patience/waiting tolerance [0, 1] */
    float temporal_horizon;          /**< Temporal discount horizon */
    nimcp_raphe_snn_mood_t mood;     /**< Current mood state */
} nimcp_raphe_snn_ht_state_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_raphe_snn_state_t state;   /**< Current operational state */
    nimcp_raphe_snn_ht_state_t ht;   /**< Current 5-HT state */
    float avg_firing_rate;           /**< Average network firing rate */
    float inhibitory_output;         /**< Current inhibitory output */
    bool bio_async_connected;        /**< Bio-async connection status */
} nimcp_raphe_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total update calls */
    uint64_t total_spikes_generated; /**< Total spikes generated */
    uint64_t impulse_inhibitions;    /**< Times impulse inhibited */
    uint64_t mood_transitions;       /**< Mood state transitions */
    float avg_serotonin_level;       /**< Average 5-HT level */
    float avg_impulse_control;       /**< Average impulse control */
    float avg_processing_time_ms;    /**< Average processing time */
} nimcp_raphe_snn_stats_t;

/*=============================================================================
 * Modulation Output
 *===========================================================================*/

/**
 * @brief Behavioral modulation output
 */
typedef struct {
    float inhibition_strength;       /**< Behavioral inhibition [0, 1] */
    float patience_level;            /**< Patience/delay tolerance [0, 1] */
    float mood_bias;                 /**< Mood-based decision bias [-1, 1] */
    float risk_aversion;             /**< Risk aversion level [0, 1] */
    float temporal_discount;         /**< Temporal discounting factor */
    bool suggest_wait;               /**< Suggest waiting/patience */
} nimcp_raphe_snn_modulation_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_raphe_snn_bridge nimcp_raphe_snn_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
nimcp_raphe_snn_config_t nimcp_raphe_snn_config_default(void);

/**
 * @brief Create Raphe-SNN bridge
 */
nimcp_raphe_snn_bridge_t* nimcp_raphe_snn_create(const nimcp_raphe_snn_config_t* config);

/**
 * @brief Destroy Raphe-SNN bridge
 */
void nimcp_raphe_snn_destroy(nimcp_raphe_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int nimcp_raphe_snn_reset(nimcp_raphe_snn_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to Raphe adapter
 */
int nimcp_raphe_snn_connect_raphe(
    nimcp_raphe_snn_bridge_t* bridge,
    nimcp_raphe_adapter_t raphe_adapter
);

/**
 * @brief Connect to SNN network
 */
int nimcp_raphe_snn_connect_snn(
    nimcp_raphe_snn_bridge_t* bridge,
    struct nimcp_snn_network* snn
);

/*=============================================================================
 * Encoding Functions (Raphe --> SNN)
 *===========================================================================*/

/**
 * @brief Encode current 5-HT state as spike train
 */
int nimcp_raphe_snn_encode_ht_state(nimcp_raphe_snn_bridge_t* bridge);

/**
 * @brief Encode mood state
 */
int nimcp_raphe_snn_encode_mood(
    nimcp_raphe_snn_bridge_t* bridge,
    nimcp_raphe_snn_mood_t mood,
    float intensity
);

/**
 * @brief Encode impulse control state
 */
int nimcp_raphe_snn_encode_impulse_control(
    nimcp_raphe_snn_bridge_t* bridge,
    float control_level
);

/**
 * @brief Encode temporal horizon
 */
int nimcp_raphe_snn_encode_temporal_horizon(
    nimcp_raphe_snn_bridge_t* bridge,
    float horizon
);

/*=============================================================================
 * Simulation Functions
 *===========================================================================*/

/**
 * @brief Run SNN simulation step
 */
int nimcp_raphe_snn_simulate(nimcp_raphe_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Process single timestep
 */
int nimcp_raphe_snn_step(nimcp_raphe_snn_bridge_t* bridge);

/*=============================================================================
 * Decoding Functions (SNN --> Raphe)
 *===========================================================================*/

/**
 * @brief Get behavioral modulation from SNN output
 */
int nimcp_raphe_snn_get_modulation(
    nimcp_raphe_snn_bridge_t* bridge,
    nimcp_raphe_snn_modulation_t* modulation
);

/**
 * @brief Get impulse inhibition signal
 */
float nimcp_raphe_snn_get_inhibition(nimcp_raphe_snn_bridge_t* bridge);

/**
 * @brief Get patience/waiting signal
 */
float nimcp_raphe_snn_get_patience(nimcp_raphe_snn_bridge_t* bridge);

/**
 * @brief Get mood-based decision bias
 */
float nimcp_raphe_snn_get_mood_bias(nimcp_raphe_snn_bridge_t* bridge);

/*=============================================================================
 * State and Statistics
 *===========================================================================*/

/**
 * @brief Get bridge state
 */
int nimcp_raphe_snn_get_state(
    const nimcp_raphe_snn_bridge_t* bridge,
    nimcp_raphe_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int nimcp_raphe_snn_get_stats(
    const nimcp_raphe_snn_bridge_t* bridge,
    nimcp_raphe_snn_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void nimcp_raphe_snn_reset_stats(nimcp_raphe_snn_bridge_t* bridge);

/*=============================================================================
 * Bio-Async Integration
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 */
int nimcp_raphe_snn_connect_bio_async(nimcp_raphe_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int nimcp_raphe_snn_disconnect_bio_async(nimcp_raphe_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool nimcp_raphe_snn_is_bio_async_connected(const nimcp_raphe_snn_bridge_t* bridge);

/*=============================================================================
 * External Modulation
 *===========================================================================*/

/**
 * @brief Set external mood signal
 */
int nimcp_raphe_snn_set_mood(
    nimcp_raphe_snn_bridge_t* bridge,
    nimcp_raphe_snn_mood_t mood,
    float intensity
);

/**
 * @brief Set impulse trigger (test patience)
 */
int nimcp_raphe_snn_impulse_trigger(
    nimcp_raphe_snn_bridge_t* bridge,
    float urgency
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RAPHE_SNN_BRIDGE_H */
