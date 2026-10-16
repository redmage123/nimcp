/**
 * @file nimcp_consolidation_snn_bridge.h
 * @brief Consolidation - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between memory consolidation and spiking neural networks
 * WHY:  Enable biologically-plausible memory consolidation through population coding
 *       and spike-timing dynamics during sleep-like phases
 * HOW:  Encode consolidation phases as spike patterns, decode memory stabilization
 *       signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Diekelmann & Born (2010): Sleep memory consolidation mechanisms
 * - Rasch & Born (2013): Sleep-dependent memory consolidation
 * - Walker & Stickgold (2006): Sleep-dependent processing and learning
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal sharp-wave ripples for memory replay
 * - Neocortical slow oscillations for systems consolidation
 * - Sleep spindles for synaptic strengthening
 * - Thalamocortical dialogue for memory transfer
 *
 * INTEGRATION WITH LEARNING:
 * - Replay sequence encoding through population activity
 * - LTP state detection via firing rate changes
 * - Schema integration through synchrony patterns
 *
 * @see nimcp_consolidation.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_CONSOLIDATION_SNN_BRIDGE_H
#define NIMCP_CONSOLIDATION_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum consolidation dimensions to encode */
#define CONSOLIDATION_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per consolidation dimension */
#define CONSOLIDATION_SNN_NEURONS_PER_DIM    32

/** @brief Default stabilization threshold */
#define CONSOLIDATION_SNN_STABILIZATION_THRESH  0.5f

/** @brief Default encoding window (ms) */
#define CONSOLIDATION_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_CONSOLIDATION_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Consolidation dimension types for SNN encoding
 */
typedef enum {
    CONSOLIDATION_DIM_REPLAY_STRENGTH = 0,    /**< Memory replay strength */
    CONSOLIDATION_DIM_STABILIZATION,          /**< Synaptic stabilization level */
    CONSOLIDATION_DIM_LTP_STATE,              /**< Long-term potentiation state */
    CONSOLIDATION_DIM_SCHEMA_INTEGRATION,     /**< Schema integration level */
    CONSOLIDATION_DIM_SLEEP_PHASE,            /**< Sleep phase (SWS/REM) */
    CONSOLIDATION_DIM_RIPPLE_ACTIVITY,        /**< Sharp-wave ripple activity */
    CONSOLIDATION_DIM_SPINDLE_DENSITY,        /**< Sleep spindle density */
    CONSOLIDATION_DIM_PRUNING_SIGNAL,         /**< Synaptic pruning signal */
    CONSOLIDATION_DIM_HOMEOSTATIC,            /**< Homeostatic pressure */
    CONSOLIDATION_DIM_TRANSFER_PROGRESS,      /**< Memory transfer progress */
    CONSOLIDATION_DIM_COUNT
} consolidation_snn_dimension_t;

/**
 * @brief Encoding methods for consolidation contexts
 */
typedef enum {
    CONSOLIDATION_SNN_ENCODE_RATE = 0,       /**< Rate coding of dimensions */
    CONSOLIDATION_SNN_ENCODE_TEMPORAL,        /**< Temporal spike patterns */
    CONSOLIDATION_SNN_ENCODE_POPULATION,      /**< Population vector coding */
    CONSOLIDATION_SNN_ENCODE_SYNCHRONY        /**< Synchrony-based encoding */
} consolidation_snn_encoding_t;

/**
 * @brief Decoding methods for memory states
 */
typedef enum {
    CONSOLIDATION_SNN_DECODE_THRESHOLD = 0,  /**< Threshold-based detection */
    CONSOLIDATION_SNN_DECODE_COMPETITION,     /**< Winner-take-all */
    CONSOLIDATION_SNN_DECODE_SOFTMAX,         /**< Soft probabilistic */
    CONSOLIDATION_SNN_DECODE_INTEGRATION      /**< Evidence accumulation */
} consolidation_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    CONSOLIDATION_SNN_STATE_IDLE = 0,
    CONSOLIDATION_SNN_STATE_ENCODING,
    CONSOLIDATION_SNN_STATE_PROCESSING,
    CONSOLIDATION_SNN_STATE_DECODING,
    CONSOLIDATION_SNN_STATE_SIMULATING,
    CONSOLIDATION_SNN_STATE_ERROR
} consolidation_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Consolidation-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of consolidation dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    consolidation_snn_encoding_t encoding;   /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    consolidation_snn_decoding_t decoding;   /**< Decoding method */
    float stabilization_threshold;       /**< Threshold for stabilization detection */
    float replay_threshold;              /**< Minimum replay strength */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_replay_detection;        /**< Enable replay signal detection */
    float replay_sensitivity;            /**< Replay detection sensitivity */

    /* Consolidation integration */
    bool enable_ltp_tracking;            /**< Enable LTP state tracking */
    float ltp_gain;                      /**< LTP signal gain */
    bool enable_schema_integration;      /**< Enable schema integration tracking */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} consolidation_snn_config_t;

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
} consolidation_dim_state_t;

/**
 * @brief Memory consolidation output
 */
typedef struct {
    float replay_strength;               /**< Current replay strength [0-1] */
    float stabilization_level;           /**< Synaptic stabilization [0-1] */
    float ltp_state;                     /**< LTP state [0-1] */
    float schema_integration;            /**< Schema integration level */
    float sleep_phase;                   /**< Sleep phase signal */
    bool replay_detected;                /**< Memory replay detected */
    bool consolidation_active;           /**< Consolidation is active */
    float consolidation_strength;        /**< Consolidation strength if active */
    float ripple_activity;               /**< Sharp-wave ripple activity */
    float transfer_progress;             /**< Memory transfer progress */
} consolidation_memory_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    consolidation_snn_state_t state;     /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_consolidation;            /**< Mean consolidation level */
    float replay_signal;                 /**< Current replay signal */
    float stabilization_signal;          /**< Current stabilization signal */
} consolidation_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t replay_detections;          /**< Replay detections */
    uint64_t consolidation_events;       /**< Consolidation events */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_consolidation;            /**< Mean consolidation level */
} consolidation_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct consolidation_snn_bridge consolidation_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Replay detection callback */
typedef void (*consolidation_snn_replay_callback_t)(
    consolidation_snn_bridge_t* bridge,
    float replay_strength,
    uint64_t latency_us,
    void* user_data
);

/** @brief Memory state ready callback */
typedef void (*consolidation_snn_state_callback_t)(
    consolidation_snn_bridge_t* bridge,
    const consolidation_memory_state_t* state,
    void* user_data
);

/** @brief Stabilization callback */
typedef void (*consolidation_snn_stabilization_callback_t)(
    consolidation_snn_bridge_t* bridge,
    float stabilization_level,
    uint32_t stabilization_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
consolidation_snn_config_t consolidation_snn_config_default(void);

/**
 * @brief Create consolidation SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
consolidation_snn_bridge_t* consolidation_snn_create(const consolidation_snn_config_t* config);

/**
 * @brief Destroy consolidation SNN bridge
 * @param bridge Bridge to destroy
 */
void consolidation_snn_destroy(consolidation_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int consolidation_snn_reset(consolidation_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode consolidation state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int consolidation_snn_encode_state(
    consolidation_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode replay sequence
 * @param bridge Bridge handle
 * @param replay_strength Replay strength [0-1]
 * @param sequence_idx Sequence index
 * @return Spike count on success, -1 on failure
 */
int consolidation_snn_encode_replay(
    consolidation_snn_bridge_t* bridge,
    float replay_strength,
    uint32_t sequence_idx
);

/**
 * @brief Encode LTP state
 * @param bridge Bridge handle
 * @param ltp_level LTP level [0-1]
 * @param synapse_count Number of potentiated synapses
 * @return Spike count on success, -1 on failure
 */
int consolidation_snn_encode_ltp(
    consolidation_snn_bridge_t* bridge,
    float ltp_level,
    uint32_t synapse_count
);

/**
 * @brief Encode schema integration
 * @param bridge Bridge handle
 * @param integration_level Schema integration level [0-1]
 * @param schema_type Schema type classification
 * @return Spike count on success, -1 on failure
 */
int consolidation_snn_encode_schema(
    consolidation_snn_bridge_t* bridge,
    float integration_level,
    uint32_t schema_type
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate consolidation processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int consolidation_snn_simulate(consolidation_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int consolidation_snn_step(consolidation_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int consolidation_snn_forward(
    consolidation_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get memory state from SNN activity
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int consolidation_snn_get_memory_state(
    consolidation_snn_bridge_t* bridge,
    consolidation_memory_state_t* state
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int consolidation_snn_get_activations(
    consolidation_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for replay detection
 * @param bridge Bridge handle
 * @param replay_strength Output replay strength
 * @return true if replay detected, false otherwise
 */
bool consolidation_snn_check_replay(
    consolidation_snn_bridge_t* bridge,
    float* replay_strength
);

/**
 * @brief Check for stabilization
 * @param bridge Bridge handle
 * @param stabilization_level Output stabilization level
 * @return true if stabilization detected, false otherwise
 */
bool consolidation_snn_check_stabilization(
    consolidation_snn_bridge_t* bridge,
    float* stabilization_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool consolidation_snn_check_state_change(
    consolidation_snn_bridge_t* bridge,
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
int consolidation_snn_get_dim_state(
    consolidation_snn_bridge_t* bridge,
    uint32_t dim,
    consolidation_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int consolidation_snn_get_state(
    consolidation_snn_bridge_t* bridge,
    consolidation_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int consolidation_snn_get_stats(consolidation_snn_bridge_t* bridge, consolidation_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int consolidation_snn_reset_stats(consolidation_snn_bridge_t* bridge);

/**
 * @brief Get current consolidation level
 * @param bridge Bridge handle
 * @return Consolidation level [0-1], -1 on error
 */
float consolidation_snn_get_consolidation_level(consolidation_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float consolidation_snn_get_total_activity(consolidation_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register replay detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int consolidation_snn_register_replay_callback(
    consolidation_snn_bridge_t* bridge,
    consolidation_snn_replay_callback_t callback,
    void* user_data
);

/**
 * @brief Register memory state callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int consolidation_snn_register_state_callback(
    consolidation_snn_bridge_t* bridge,
    consolidation_snn_state_callback_t callback,
    void* user_data
);

/**
 * @brief Register stabilization callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int consolidation_snn_register_stabilization_callback(
    consolidation_snn_bridge_t* bridge,
    consolidation_snn_stabilization_callback_t callback,
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
int consolidation_snn_bio_async_connect(consolidation_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int consolidation_snn_bio_async_disconnect(consolidation_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool consolidation_snn_is_bio_async_connected(consolidation_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONSOLIDATION_SNN_BRIDGE_H */
