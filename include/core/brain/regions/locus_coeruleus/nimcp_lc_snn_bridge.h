/**
 * @file nimcp_lc_snn_bridge.h
 * @brief Locus Coeruleus - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Bidirectional bridge between LC (norepinephrine) system and SNN module
 * WHY:  Enable spike-based arousal/attention processing and bio-plausible learning
 * HOW:  Convert NE states to spikes, decode SNN output to neuromodulation signals
 *
 * THEORETICAL FOUNDATIONS:
 * - Aston-Jones & Cohen (2005): LC-NE system and attention regulation
 * - Sara (2009): LC-NE system role in network reset and arousal
 * - Berridge & Waterhouse (2003): LC-NE effects on gain modulation
 *
 * BIOLOGICAL BASIS:
 * - LC neurons fire tonically for baseline vigilance
 * - Phasic bursts triggered by salient/novel stimuli
 * - NE release modulates cortical signal-to-noise ratio
 * - STDP eligibility traces gated by NE for attention-based learning
 *
 * INTEGRATION FLOWS:
 *
 * LC --> SNN:
 *   1. Tonic NE level encoded as baseline firing rate
 *   2. Phasic bursts encoded as synchronized spike volleys
 *   3. Arousal state modulates network gain
 *   4. Novelty signals trigger attention reset patterns
 *
 * SNN --> LC:
 *   1. Population activity indicates processing demands
 *   2. Synchrony patterns trigger phasic LC responses
 *   3. Network state feedback adjusts tonic/phasic mode
 *   4. Learning progress signals modulate NE baseline
 *
 * @see nimcp_locus_coeruleus.h
 * @see nimcp_lc_adapter.h
 * @see nimcp_snn.h
 */

#ifndef NIMCP_LC_SNN_BRIDGE_H
#define NIMCP_LC_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_lc_adapter_struct;
typedef struct nimcp_lc_adapter_struct* nimcp_lc_adapter_t;
struct nimcp_snn_network;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default neurons in LC population representation */
#define LC_SNN_POPULATION_SIZE          32

/** @brief Default input dimension for NE state encoding */
#define LC_SNN_INPUT_DIM                64

/** @brief Bio-async module ID for LC-SNN bridge */
#define BIO_MODULE_LC_SNN_BRIDGE        0x0C00

/** @brief Default simulation timestep (ms) */
#define LC_SNN_DEFAULT_DT               1.0f

/** @brief Default encoding window (ms) */
#define LC_SNN_ENCODING_WINDOW          50.0f

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Spike encoding method for NE state
 */
typedef enum {
    LC_SNN_ENCODE_RATE = 0,          /**< Rate coding (NE level = rate) */
    LC_SNN_ENCODE_BURST,             /**< Burst coding (phasic events) */
    LC_SNN_ENCODE_POPULATION,        /**< Population vector coding */
    LC_SNN_ENCODE_TEMPORAL           /**< Temporal pattern coding */
} nimcp_lc_snn_encoding_t;

/**
 * @brief Decoding method for SNN output to LC modulation
 */
typedef enum {
    LC_SNN_DECODE_AVERAGE = 0,       /**< Average firing rate */
    LC_SNN_DECODE_SYNCHRONY,         /**< Population synchrony */
    LC_SNN_DECODE_POPULATION,        /**< Population vector decoding */
    LC_SNN_DECODE_TEMPORAL           /**< Temporal pattern matching */
} nimcp_lc_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    LC_SNN_STATE_IDLE = 0,           /**< Ready for input */
    LC_SNN_STATE_ENCODING,           /**< Encoding NE state */
    LC_SNN_STATE_SIMULATING,         /**< Running SNN simulation */
    LC_SNN_STATE_DECODING,           /**< Decoding SNN output */
    LC_SNN_STATE_DISABLED            /**< Bridge disabled */
} nimcp_lc_snn_state_t;

/**
 * @brief LC mode for SNN encoding
 */
typedef enum {
    LC_SNN_MODE_TONIC = 0,           /**< Baseline vigilance mode */
    LC_SNN_MODE_PHASIC,              /**< Phasic burst mode */
    LC_SNN_MODE_EXPLORATORY,         /**< High tonic, low phasic */
    LC_SNN_MODE_EXPLOITATIVE         /**< Optimal tonic-phasic balance */
} nimcp_lc_snn_mode_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief LC-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t population_size;        /**< LC population neurons */
    uint32_t input_dim;              /**< Input feature dimension */
    uint32_t output_dim;             /**< Output modulation dimension */

    /* Encoding parameters */
    nimcp_lc_snn_encoding_t encoding;/**< Spike encoding method */
    float encoding_gain;             /**< NE-to-spike gain */
    float tonic_baseline_hz;         /**< Tonic baseline firing rate */
    float phasic_burst_hz;           /**< Phasic burst rate */
    float burst_duration_ms;         /**< Phasic burst duration */

    /* Decoding parameters */
    nimcp_lc_snn_decoding_t decoding;/**< Output decoding method */
    float decoding_threshold;        /**< Activation threshold */
    float temporal_smoothing;        /**< Temporal averaging alpha */

    /* Gain modulation */
    bool enable_gain_modulation;     /**< Enable NE gain modulation */
    float min_gain;                  /**< Minimum gain factor */
    float max_gain;                  /**< Maximum gain factor */
    float gain_time_constant_ms;     /**< Gain adaptation rate */

    /* Simulation */
    float dt_ms;                     /**< Simulation timestep */
    float simulation_window_ms;      /**< Simulation window per update */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_plasticity_bridge;   /**< Enable plasticity integration */
} nimcp_lc_snn_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Current NE encoding state
 */
typedef struct {
    float ne_level;                  /**< Current NE concentration */
    float arousal;                   /**< Current arousal [0, 1] */
    float vigilance;                 /**< Current vigilance [0, 1] */
    float novelty_response;          /**< Recent novelty response */
    nimcp_lc_snn_mode_t mode;        /**< Current operational mode */
    uint64_t last_burst_us;          /**< Last phasic burst timestamp */
} nimcp_lc_snn_ne_state_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_lc_snn_state_t state;      /**< Current operational state */
    nimcp_lc_snn_ne_state_t ne;      /**< Current NE state */
    float current_gain;              /**< Current gain modulation */
    float avg_firing_rate;           /**< Average network firing rate */
    float population_synchrony;      /**< Population synchronization */
    bool bio_async_connected;        /**< Bio-async connection status */
} nimcp_lc_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total update calls */
    uint64_t total_spikes_generated; /**< Total spikes generated */
    uint64_t total_bursts;           /**< Total phasic bursts */
    uint64_t novelty_events;         /**< Novelty detection events */
    float avg_ne_level;              /**< Average NE level */
    float avg_gain_modulation;       /**< Average gain applied */
    float avg_processing_time_ms;    /**< Average processing time */
} nimcp_lc_snn_stats_t;

/*=============================================================================
 * Modulation Output
 *===========================================================================*/

/**
 * @brief Gain modulation output for target networks
 */
typedef struct {
    float gain;                      /**< Global gain factor [0.5-2.0] */
    float attention_boost;           /**< Attention enhancement [0-1] */
    float noise_suppression;         /**< Signal-to-noise ratio boost [0-1] */
    bool trigger_reset;              /**< Trigger attention reset */
    float exploration_drive;         /**< Exploration vs exploitation [0-1] */
} nimcp_lc_snn_modulation_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_lc_snn_bridge nimcp_lc_snn_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
nimcp_lc_snn_config_t nimcp_lc_snn_config_default(void);

/**
 * @brief Create LC-SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
nimcp_lc_snn_bridge_t* nimcp_lc_snn_create(const nimcp_lc_snn_config_t* config);

/**
 * @brief Destroy LC-SNN bridge
 */
void nimcp_lc_snn_destroy(nimcp_lc_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int nimcp_lc_snn_reset(nimcp_lc_snn_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to LC adapter
 * @param bridge Bridge instance
 * @param lc_adapter LC adapter to connect
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_snn_connect_lc(
    nimcp_lc_snn_bridge_t* bridge,
    nimcp_lc_adapter_t lc_adapter
);

/**
 * @brief Connect to SNN network
 * @param bridge Bridge instance
 * @param snn SNN network to connect
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_snn_connect_snn(
    nimcp_lc_snn_bridge_t* bridge,
    struct nimcp_snn_network* snn
);

/*=============================================================================
 * Encoding Functions (LC --> SNN)
 *===========================================================================*/

/**
 * @brief Encode current NE state as spike train
 * @param bridge Bridge instance
 * @return Number of spikes generated, or -1 on failure
 */
int nimcp_lc_snn_encode_ne_state(nimcp_lc_snn_bridge_t* bridge);

/**
 * @brief Encode phasic burst event
 * @param bridge Bridge instance
 * @param intensity Burst intensity [0, 1]
 * @return Number of spikes generated, or -1 on failure
 */
int nimcp_lc_snn_encode_burst(
    nimcp_lc_snn_bridge_t* bridge,
    float intensity
);

/**
 * @brief Encode novelty signal
 * @param bridge Bridge instance
 * @param novelty_score Novelty magnitude [0, 1]
 * @return Number of spikes generated, or -1 on failure
 */
int nimcp_lc_snn_encode_novelty(
    nimcp_lc_snn_bridge_t* bridge,
    float novelty_score
);

/**
 * @brief Encode arousal state
 * @param bridge Bridge instance
 * @param arousal Arousal level [0, 1]
 * @param vigilance Vigilance level [0, 1]
 * @return Number of spikes generated, or -1 on failure
 */
int nimcp_lc_snn_encode_arousal(
    nimcp_lc_snn_bridge_t* bridge,
    float arousal,
    float vigilance
);

/*=============================================================================
 * Simulation Functions
 *===========================================================================*/

/**
 * @brief Run SNN simulation step
 * @param bridge Bridge instance
 * @param duration_ms Simulation duration in milliseconds
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_snn_simulate(nimcp_lc_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Process single timestep
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_snn_step(nimcp_lc_snn_bridge_t* bridge);

/*=============================================================================
 * Decoding Functions (SNN --> LC)
 *===========================================================================*/

/**
 * @brief Get gain modulation from SNN output
 * @param bridge Bridge instance
 * @param modulation Output modulation values
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_snn_get_modulation(
    nimcp_lc_snn_bridge_t* bridge,
    nimcp_lc_snn_modulation_t* modulation
);

/**
 * @brief Get decoded NE state feedback
 * @param bridge Bridge instance
 * @param ne_state Output NE state
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_snn_get_ne_feedback(
    nimcp_lc_snn_bridge_t* bridge,
    nimcp_lc_snn_ne_state_t* ne_state
);

/**
 * @brief Get attention reset signal
 * @param bridge Bridge instance
 * @return true if attention reset recommended
 */
bool nimcp_lc_snn_should_reset_attention(nimcp_lc_snn_bridge_t* bridge);

/**
 * @brief Get exploration/exploitation balance
 * @param bridge Bridge instance
 * @return Exploration drive [0=exploit, 1=explore]
 */
float nimcp_lc_snn_get_exploration_drive(nimcp_lc_snn_bridge_t* bridge);

/*=============================================================================
 * State and Statistics
 *===========================================================================*/

/**
 * @brief Get bridge state
 * @param bridge Bridge instance
 * @param state Output state
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_snn_get_state(
    const nimcp_lc_snn_bridge_t* bridge,
    nimcp_lc_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_snn_get_stats(
    const nimcp_lc_snn_bridge_t* bridge,
    nimcp_lc_snn_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void nimcp_lc_snn_reset_stats(nimcp_lc_snn_bridge_t* bridge);

/*=============================================================================
 * Bio-Async Integration
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 */
int nimcp_lc_snn_connect_bio_async(nimcp_lc_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int nimcp_lc_snn_disconnect_bio_async(nimcp_lc_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool nimcp_lc_snn_is_bio_async_connected(const nimcp_lc_snn_bridge_t* bridge);

/*=============================================================================
 * External Modulation
 *===========================================================================*/

/**
 * @brief Apply external gain modulation
 * @param bridge Bridge instance
 * @param external_gain External gain factor
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_snn_apply_external_gain(
    nimcp_lc_snn_bridge_t* bridge,
    float external_gain
);

/**
 * @brief Set mode directly
 * @param bridge Bridge instance
 * @param mode Operating mode
 * @return 0 on success, -1 on failure
 */
int nimcp_lc_snn_set_mode(
    nimcp_lc_snn_bridge_t* bridge,
    nimcp_lc_snn_mode_t mode
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LC_SNN_BRIDGE_H */
