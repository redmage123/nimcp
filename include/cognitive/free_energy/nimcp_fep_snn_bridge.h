/**
 * @file nimcp_fep_snn_bridge.h
 * @brief Free Energy Principle - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between FEP engine and spiking neural networks
 * WHY:  Enable biologically-plausible free energy minimization through
 *       population coding and spike-timing dynamics
 * HOW:  Encode prediction error dimensions as spike patterns, decode belief
 *       updates from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): The free-energy principle: a unified brain theory
 * - Parr & Friston (2018): Active inference and neural computation
 * - Rao & Ballard (1999): Predictive coding in visual cortex
 *
 * BIOLOGICAL BASIS:
 * - Cortical pyramidal neurons for prediction error signaling
 * - Thalamo-cortical loops for precision weighting
 * - Hierarchical message passing through dendritic compartments
 * - Gamma oscillations for belief propagation
 *
 * INTEGRATION WITH ACTIVE INFERENCE:
 * - Prediction error encoding through burst firing
 * - Precision modulation via NMDA/GABA balance
 * - Active inference action through motor population activity
 * - Variational belief updating via attractor dynamics
 *
 * @see nimcp_free_energy.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_FEP_SNN_BRIDGE_H
#define NIMCP_FEP_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum FEP dimensions to encode */
#define FEP_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per FEP dimension */
#define FEP_SNN_NEURONS_PER_DIM    32

/** @brief Default prediction error threshold */
#define FEP_SNN_PRED_ERROR_THRESH  0.3f

/** @brief Default encoding window (ms) */
#define FEP_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_FEP_SNN         0x0E50

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief FEP dimension types for SNN encoding
 */
typedef enum {
    FEP_DIM_PREDICTION_ERROR = 0,       /**< Prediction error magnitude */
    FEP_DIM_FREE_ENERGY,                /**< Current free energy level */
    FEP_DIM_PRECISION,                  /**< Precision weighting */
    FEP_DIM_BELIEF_STRENGTH,            /**< Belief confidence */
    FEP_DIM_ACTIVE_INFERENCE,           /**< Active inference drive */
    FEP_DIM_VARIATIONAL_BOUND,          /**< Variational bound tightness */
    FEP_DIM_ENTROPY,                    /**< Entropy estimate */
    FEP_DIM_KL_DIVERGENCE,              /**< KL divergence from prior */
    FEP_DIM_MODEL_COMPLEXITY,           /**< Model complexity penalty */
    FEP_DIM_EVIDENCE_LOWER_BOUND,       /**< ELBO estimate */
    FEP_DIM_COUNT
} fep_snn_dimension_t;

/**
 * @brief Encoding methods for FEP contexts
 */
typedef enum {
    FEP_SNN_ENCODE_RATE = 0,            /**< Rate coding of dimensions */
    FEP_SNN_ENCODE_TEMPORAL,            /**< Temporal spike patterns */
    FEP_SNN_ENCODE_POPULATION,          /**< Population vector coding */
    FEP_SNN_ENCODE_PREDICTIVE           /**< Predictive coding scheme */
} fep_snn_encoding_t;

/**
 * @brief Decoding methods for belief states
 */
typedef enum {
    FEP_SNN_DECODE_THRESHOLD = 0,       /**< Threshold-based detection */
    FEP_SNN_DECODE_COMPETITION,         /**< Winner-take-all */
    FEP_SNN_DECODE_VARIATIONAL,         /**< Variational inference */
    FEP_SNN_DECODE_INTEGRATION          /**< Evidence accumulation */
} fep_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FEP_SNN_STATE_IDLE = 0,
    FEP_SNN_STATE_ENCODING,
    FEP_SNN_STATE_PROCESSING,
    FEP_SNN_STATE_DECODING,
    FEP_SNN_STATE_SIMULATING,
    FEP_SNN_STATE_ERROR
} fep_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief FEP-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of FEP dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    fep_snn_encoding_t encoding;         /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    fep_snn_decoding_t decoding;         /**< Decoding method */
    float pred_error_threshold;          /**< Threshold for prediction error */
    float belief_threshold;              /**< Minimum belief strength */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_pred_error_detection;    /**< Enable prediction error detection */
    float pred_error_sensitivity;        /**< Prediction error sensitivity */

    /* Active inference integration */
    bool enable_active_inference;        /**< Enable active inference circuits */
    float active_inference_gain;         /**< Active inference signal gain */
    bool enable_precision_weighting;     /**< Enable precision weighting */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} fep_snn_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Per-dimension state tracking
 */
typedef struct {
    float activation;                    /**< Current activation level */
    float accumulated_evidence;          /**< Accumulated evidence */
    uint32_t spike_count;                /**< Recent spike count */
    float mean_rate_hz;                  /**< Mean firing rate */
    uint64_t last_spike_time_us;         /**< Last spike timestamp */
} fep_dim_state_t;

/**
 * @brief Belief state output from active inference
 */
typedef struct {
    float prediction_error;              /**< Current prediction error [0-1] */
    float free_energy;                   /**< Current free energy [0-1] */
    float precision;                     /**< Precision weighting [0-1] */
    float belief_strength;               /**< Belief confidence */
    float active_inference_drive;        /**< Active inference drive strength */
    float variational_bound;             /**< Variational bound tightness */
    bool high_pred_error;                /**< High prediction error detected */
    bool belief_update_needed;           /**< Belief update is needed */
    float entropy_estimate;              /**< Entropy estimate */
    float kl_divergence;                 /**< KL divergence from prior */
} fep_belief_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    fep_snn_state_t state;               /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_free_energy;              /**< Mean free energy level */
    float pred_error_signal;             /**< Current prediction error signal */
    float precision_signal;              /**< Current precision signal */
} fep_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t high_pred_error_events;     /**< High prediction error events */
    uint64_t belief_updates;             /**< Belief update events */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_free_energy;              /**< Mean free energy */
} fep_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct fep_snn_bridge fep_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Prediction error detection callback */
typedef void (*fep_snn_pred_error_callback_t)(
    fep_snn_bridge_t* bridge,
    float pred_error_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Belief state ready callback */
typedef void (*fep_snn_belief_callback_t)(
    fep_snn_bridge_t* bridge,
    const fep_belief_state_t* belief,
    void* user_data
);

/** @brief Free energy change callback */
typedef void (*fep_snn_free_energy_callback_t)(
    fep_snn_bridge_t* bridge,
    float old_free_energy,
    float new_free_energy,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
fep_snn_config_t fep_snn_config_default(void);

/**
 * @brief Create FEP SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
fep_snn_bridge_t* fep_snn_create(const fep_snn_config_t* config);

/**
 * @brief Destroy FEP SNN bridge
 * @param bridge Bridge to destroy
 */
void fep_snn_destroy(fep_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int fep_snn_reset(fep_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode FEP state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int fep_snn_encode_state(
    fep_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode prediction error
 * @param bridge Bridge handle
 * @param pred_error Prediction error magnitude [0-1]
 * @param precision Precision weighting [0-1]
 * @return Spike count on success, -1 on failure
 */
int fep_snn_encode_prediction_error(
    fep_snn_bridge_t* bridge,
    float pred_error,
    float precision
);

/**
 * @brief Encode free energy level
 * @param bridge Bridge handle
 * @param free_energy Free energy level [0-1]
 * @param complexity Model complexity [0-1]
 * @return Spike count on success, -1 on failure
 */
int fep_snn_encode_free_energy(
    fep_snn_bridge_t* bridge,
    float free_energy,
    float complexity
);

/**
 * @brief Encode active inference signal
 * @param bridge Bridge handle
 * @param action_drive Action drive strength [0-1]
 * @param action_type Action type classification
 * @return Spike count on success, -1 on failure
 */
int fep_snn_encode_active_inference(
    fep_snn_bridge_t* bridge,
    float action_drive,
    uint32_t action_type
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate FEP processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int fep_snn_simulate(fep_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int fep_snn_step(fep_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int fep_snn_forward(
    fep_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get belief state from SNN activity
 * @param bridge Bridge handle
 * @param belief Output belief state structure
 * @return 0 on success, -1 on failure
 */
int fep_snn_get_belief(
    fep_snn_bridge_t* bridge,
    fep_belief_state_t* belief
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int fep_snn_get_activations(
    fep_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for high prediction error
 * @param bridge Bridge handle
 * @param pred_error_level Output prediction error level
 * @return true if high prediction error detected, false otherwise
 */
bool fep_snn_check_pred_error(
    fep_snn_bridge_t* bridge,
    float* pred_error_level
);

/**
 * @brief Check for belief update needed
 * @param bridge Bridge handle
 * @param update_magnitude Output update magnitude
 * @return true if belief update needed, false otherwise
 */
bool fep_snn_check_belief_update(
    fep_snn_bridge_t* bridge,
    float* update_magnitude
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool fep_snn_check_state_change(
    fep_snn_bridge_t* bridge,
    float* change_magnitude
);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get dimension state
 * @param bridge Bridge handle
 * @param dim Dimension index
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int fep_snn_get_dim_state(
    fep_snn_bridge_t* bridge,
    uint32_t dim,
    fep_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int fep_snn_get_state(
    fep_snn_bridge_t* bridge,
    fep_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int fep_snn_get_stats(fep_snn_bridge_t* bridge, fep_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int fep_snn_reset_stats(fep_snn_bridge_t* bridge);

/**
 * @brief Get current free energy level
 * @param bridge Bridge handle
 * @return Free energy [0-1], -1 on error
 */
float fep_snn_get_free_energy(fep_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float fep_snn_get_total_activity(fep_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register prediction error detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int fep_snn_register_pred_error_callback(
    fep_snn_bridge_t* bridge,
    fep_snn_pred_error_callback_t callback,
    void* user_data
);

/**
 * @brief Register belief callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int fep_snn_register_belief_callback(
    fep_snn_bridge_t* bridge,
    fep_snn_belief_callback_t callback,
    void* user_data
);

/**
 * @brief Register free energy change callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int fep_snn_register_free_energy_callback(
    fep_snn_bridge_t* bridge,
    fep_snn_free_energy_callback_t callback,
    void* user_data
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int fep_snn_bio_async_connect(fep_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int fep_snn_bio_async_disconnect(fep_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool fep_snn_is_bio_async_connected(fep_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_SNN_BRIDGE_H */
