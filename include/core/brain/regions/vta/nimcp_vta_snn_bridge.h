/**
 * @file nimcp_vta_snn_bridge.h
 * @brief VTA - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Bidirectional bridge between VTA (dopamine) system and SNN module
 * WHY:  Enable spike-based reward processing and bio-plausible reinforcement learning
 * HOW:  Convert DA/RPE states to spikes, decode SNN output to motivation signals
 *
 * THEORETICAL FOUNDATIONS:
 * - Schultz (1998): Dopamine neurons and reward prediction error
 * - Montague et al. (1996): TD learning in midbrain dopamine neurons
 * - Berke (2018): Dopamine and motivation
 *
 * BIOLOGICAL BASIS:
 * - VTA DA neurons fire phasically for unexpected rewards
 * - Pause in firing for reward omission (negative RPE)
 * - Tonic DA sets motivation/effort baseline
 * - DA timing crucial for credit assignment in learning
 *
 * INTEGRATION FLOWS:
 *
 * VTA --> SNN:
 *   1. DA burst encoded as synchronized spike volleys
 *   2. RPE magnitude encoded as firing rate modulation
 *   3. Motivation level sets network gain
 *   4. Goal proximity modulates baseline activity
 *
 * SNN --> VTA:
 *   1. Population activity signals processing costs
 *   2. Success/failure patterns trigger DA response
 *   3. Network prediction error feeds back to VTA
 *   4. Goal achievement signals decoded from activity
 *
 * @see nimcp_vta.h
 * @see nimcp_vta_adapter.h
 * @see nimcp_snn.h
 */

#ifndef NIMCP_VTA_SNN_BRIDGE_H
#define NIMCP_VTA_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_vta_adapter;
typedef struct nimcp_vta_adapter* nimcp_vta_adapter_t;
struct nimcp_snn_network;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default neurons in VTA population representation */
#define VTA_SNN_POPULATION_SIZE         64

/** @brief Default input dimension for DA state encoding */
#define VTA_SNN_INPUT_DIM               128

/** @brief Bio-async module ID for VTA-SNN bridge */
#define BIO_MODULE_VTA_SNN_BRIDGE       0x0D00

/** @brief Default simulation timestep (ms) */
#define VTA_SNN_DEFAULT_DT              1.0f

/** @brief Default RPE encoding window (ms) */
#define VTA_SNN_ENCODING_WINDOW         100.0f

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Spike encoding method for DA/RPE state
 */
typedef enum {
    VTA_SNN_ENCODE_RATE = 0,         /**< Rate coding (DA level = rate) */
    VTA_SNN_ENCODE_BURST,            /**< Burst coding (phasic events) */
    VTA_SNN_ENCODE_RPE,              /**< RPE-specific encoding */
    VTA_SNN_ENCODE_TEMPORAL          /**< Temporal pattern coding */
} nimcp_vta_snn_encoding_t;

/**
 * @brief Decoding method for SNN output to VTA modulation
 */
typedef enum {
    VTA_SNN_DECODE_AVERAGE = 0,      /**< Average firing rate */
    VTA_SNN_DECODE_PEAK,             /**< Peak response */
    VTA_SNN_DECODE_POPULATION,       /**< Population vector decoding */
    VTA_SNN_DECODE_TEMPORAL          /**< Temporal pattern matching */
} nimcp_vta_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    VTA_SNN_STATE_IDLE = 0,          /**< Ready for input */
    VTA_SNN_STATE_ENCODING,          /**< Encoding DA state */
    VTA_SNN_STATE_SIMULATING,        /**< Running SNN simulation */
    VTA_SNN_STATE_DECODING,          /**< Decoding SNN output */
    VTA_SNN_STATE_DISABLED           /**< Bridge disabled */
} nimcp_vta_snn_state_t;

/**
 * @brief DA signal type for encoding
 */
typedef enum {
    VTA_SNN_DA_TONIC = 0,            /**< Baseline tonic DA */
    VTA_SNN_DA_PHASIC_POSITIVE,      /**< Positive RPE burst */
    VTA_SNN_DA_PHASIC_NEGATIVE,      /**< Negative RPE pause */
    VTA_SNN_DA_RAMPING               /**< Ramping DA (anticipation) */
} nimcp_vta_snn_da_type_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief VTA-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t population_size;        /**< VTA population neurons */
    uint32_t input_dim;              /**< Input feature dimension */
    uint32_t output_dim;             /**< Output modulation dimension */

    /* Encoding parameters */
    nimcp_vta_snn_encoding_t encoding;/**< Spike encoding method */
    float encoding_gain;             /**< DA-to-spike gain */
    float tonic_baseline_hz;         /**< Tonic baseline firing rate */
    float burst_rate_hz;             /**< Burst firing rate */
    float pause_suppression;         /**< Pause suppression factor */
    float rpe_encoding_scale;        /**< RPE encoding scale */

    /* Decoding parameters */
    nimcp_vta_snn_decoding_t decoding;/**< Output decoding method */
    float decoding_threshold;        /**< Activation threshold */
    float temporal_smoothing;        /**< Temporal averaging alpha */

    /* RPE computation */
    bool enable_rpe_computation;     /**< Compute RPE from SNN */
    float rpe_learning_rate;         /**< TD learning rate */
    float discount_factor;           /**< TD discount factor */

    /* Motivation modulation */
    bool enable_motivation_output;   /**< Output motivation signals */
    float motivation_gain;           /**< Motivation scaling */
    float effort_cost_scale;         /**< Effort cost scaling */

    /* Simulation */
    float dt_ms;                     /**< Simulation timestep */
    float simulation_window_ms;      /**< Simulation window per update */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_plasticity_bridge;   /**< Enable plasticity integration */
} nimcp_vta_snn_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Current DA encoding state
 */
typedef struct {
    float da_level;                  /**< Current DA concentration */
    float current_rpe;               /**< Current RPE value */
    float motivation;                /**< Current motivation [0, 1] */
    float wanting;                   /**< Incentive salience [0, 1] */
    float vigor;                     /**< Action vigor [0, 1] */
    nimcp_vta_snn_da_type_t da_type; /**< Current DA signal type */
    uint64_t last_reward_us;         /**< Last reward timestamp */
} nimcp_vta_snn_da_state_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_vta_snn_state_t state;     /**< Current operational state */
    nimcp_vta_snn_da_state_t da;     /**< Current DA state */
    float avg_firing_rate;           /**< Average network firing rate */
    float reward_prediction;         /**< Current reward prediction */
    bool bio_async_connected;        /**< Bio-async connection status */
} nimcp_vta_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total update calls */
    uint64_t total_spikes_generated; /**< Total spikes generated */
    uint64_t positive_rpe_events;    /**< Positive RPE events */
    uint64_t negative_rpe_events;    /**< Negative RPE events */
    uint64_t reward_events;          /**< Total reward events */
    float avg_da_level;              /**< Average DA level */
    float avg_rpe;                   /**< Average RPE magnitude */
    float total_reward;              /**< Total reward received */
} nimcp_vta_snn_stats_t;

/*=============================================================================
 * Modulation Output
 *===========================================================================*/

/**
 * @brief Motivation modulation output for target networks
 */
typedef struct {
    float motivation;                /**< Global motivation [0, 1] */
    float effort_willingness;        /**< Effort cost acceptance [0, 1] */
    float reward_sensitivity;        /**< Reward sensitivity [0.5-2.0] */
    float rpe_signal;                /**< Current RPE for learning */
    float vigor;                     /**< Action vigor [0, 1] */
    bool goal_achieved;              /**< Goal achievement flag */
} nimcp_vta_snn_modulation_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_vta_snn_bridge nimcp_vta_snn_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
nimcp_vta_snn_config_t nimcp_vta_snn_config_default(void);

/**
 * @brief Create VTA-SNN bridge
 */
nimcp_vta_snn_bridge_t* nimcp_vta_snn_create(const nimcp_vta_snn_config_t* config);

/**
 * @brief Destroy VTA-SNN bridge
 */
void nimcp_vta_snn_destroy(nimcp_vta_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int nimcp_vta_snn_reset(nimcp_vta_snn_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to VTA adapter
 */
int nimcp_vta_snn_connect_vta(
    nimcp_vta_snn_bridge_t* bridge,
    nimcp_vta_adapter_t vta_adapter
);

/**
 * @brief Connect to SNN network
 */
int nimcp_vta_snn_connect_snn(
    nimcp_vta_snn_bridge_t* bridge,
    struct nimcp_snn_network* snn
);

/*=============================================================================
 * Encoding Functions (VTA --> SNN)
 *===========================================================================*/

/**
 * @brief Encode current DA state as spike train
 */
int nimcp_vta_snn_encode_da_state(nimcp_vta_snn_bridge_t* bridge);

/**
 * @brief Encode reward/RPE event
 */
int nimcp_vta_snn_encode_reward(
    nimcp_vta_snn_bridge_t* bridge,
    float reward,
    float expected_reward
);

/**
 * @brief Encode phasic DA burst
 */
int nimcp_vta_snn_encode_burst(
    nimcp_vta_snn_bridge_t* bridge,
    float intensity
);

/**
 * @brief Encode DA pause (negative RPE)
 */
int nimcp_vta_snn_encode_pause(
    nimcp_vta_snn_bridge_t* bridge,
    float suppression
);

/**
 * @brief Encode motivation/wanting state
 */
int nimcp_vta_snn_encode_motivation(
    nimcp_vta_snn_bridge_t* bridge,
    float motivation,
    float wanting
);

/*=============================================================================
 * Simulation Functions
 *===========================================================================*/

/**
 * @brief Run SNN simulation step
 */
int nimcp_vta_snn_simulate(nimcp_vta_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Process single timestep
 */
int nimcp_vta_snn_step(nimcp_vta_snn_bridge_t* bridge);

/*=============================================================================
 * Decoding Functions (SNN --> VTA)
 *===========================================================================*/

/**
 * @brief Get motivation modulation from SNN output
 */
int nimcp_vta_snn_get_modulation(
    nimcp_vta_snn_bridge_t* bridge,
    nimcp_vta_snn_modulation_t* modulation
);

/**
 * @brief Get computed RPE from SNN
 */
float nimcp_vta_snn_get_computed_rpe(nimcp_vta_snn_bridge_t* bridge);

/**
 * @brief Get reward prediction
 */
float nimcp_vta_snn_get_reward_prediction(nimcp_vta_snn_bridge_t* bridge);

/**
 * @brief Check if goal achievement detected
 */
bool nimcp_vta_snn_goal_achieved(nimcp_vta_snn_bridge_t* bridge);

/*=============================================================================
 * State and Statistics
 *===========================================================================*/

/**
 * @brief Get bridge state
 */
int nimcp_vta_snn_get_state(
    const nimcp_vta_snn_bridge_t* bridge,
    nimcp_vta_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int nimcp_vta_snn_get_stats(
    const nimcp_vta_snn_bridge_t* bridge,
    nimcp_vta_snn_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void nimcp_vta_snn_reset_stats(nimcp_vta_snn_bridge_t* bridge);

/*=============================================================================
 * Bio-Async Integration
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 */
int nimcp_vta_snn_connect_bio_async(nimcp_vta_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int nimcp_vta_snn_disconnect_bio_async(nimcp_vta_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool nimcp_vta_snn_is_bio_async_connected(const nimcp_vta_snn_bridge_t* bridge);

/*=============================================================================
 * External Modulation
 *===========================================================================*/

/**
 * @brief Set external reward signal
 */
int nimcp_vta_snn_set_reward(
    nimcp_vta_snn_bridge_t* bridge,
    float reward
);

/**
 * @brief Set goal state
 */
int nimcp_vta_snn_set_goal(
    nimcp_vta_snn_bridge_t* bridge,
    uint32_t goal_id,
    float value,
    float distance
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VTA_SNN_BRIDGE_H */
