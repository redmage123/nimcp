/**
 * @file nimcp_curiosity_snn_bridge.h
 * @brief Curiosity - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between curiosity engine and spiking neural networks
 * WHY:  Enable biologically-plausible curiosity-driven exploration through
 *       population coding and spike-timing dynamics
 * HOW:  Encode curiosity dimensions as spike patterns, decode exploration
 *       signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Gottlieb (2012): Curiosity and the neural mechanisms of uncertainty
 * - Kidd & Hayden (2015): Information-seeking in the brain
 * - Berlyne (1960): Curiosity and optimal arousal theory
 *
 * BIOLOGICAL BASIS:
 * - Dopaminergic VTA/SNc for novelty-driven exploration
 * - Hippocampus for novelty detection and memory comparison
 * - Prefrontal cortex for knowledge gap assessment
 * - Anterior cingulate cortex for exploration/exploitation trade-off
 *
 * INTEGRATION WITH LEARNING:
 * - Information gain estimation through population variability
 * - Novelty detection via firing rate comparisons
 * - Exploration drive through sustained activity patterns
 *
 * @see nimcp_curiosity.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_CURIOSITY_SNN_BRIDGE_H
#define NIMCP_CURIOSITY_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum curiosity dimensions to encode */
#define CURIOSITY_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per curiosity dimension */
#define CURIOSITY_SNN_NEURONS_PER_DIM    32

/** @brief Default novelty threshold */
#define CURIOSITY_SNN_NOVELTY_THRESH     0.5f

/** @brief Default encoding window (ms) */
#define CURIOSITY_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_CURIOSITY_SNN         0x0D50

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Curiosity dimension types for SNN encoding
 */
typedef enum {
    CURIOSITY_DIM_NOVELTY = 0,          /**< Novelty detection level */
    CURIOSITY_DIM_SURPRISE,              /**< Surprise/prediction error */
    CURIOSITY_DIM_INFORMATION_GAIN,      /**< Expected information gain */
    CURIOSITY_DIM_EXPLORATION,           /**< Exploration drive strength */
    CURIOSITY_DIM_KNOWLEDGE_GAP,         /**< Knowledge gap awareness */
    CURIOSITY_DIM_INTEREST,              /**< Interest level */
    CURIOSITY_DIM_SEEKING,               /**< Active seeking behavior */
    CURIOSITY_DIM_COMPLEXITY,            /**< Optimal complexity preference */
    CURIOSITY_DIM_UNCERTAINTY_REDUCTION, /**< Uncertainty reduction motivation */
    CURIOSITY_DIM_LEARNING_PROGRESS,     /**< Learning progress signal */
    CURIOSITY_DIM_COUNT
} curiosity_snn_dimension_t;

/**
 * @brief Encoding methods for curiosity contexts
 */
typedef enum {
    CURIOSITY_SNN_ENCODE_RATE = 0,       /**< Rate coding of dimensions */
    CURIOSITY_SNN_ENCODE_TEMPORAL,        /**< Temporal spike patterns */
    CURIOSITY_SNN_ENCODE_POPULATION,      /**< Population vector coding */
    CURIOSITY_SNN_ENCODE_SYNCHRONY        /**< Synchrony-based encoding */
} curiosity_snn_encoding_t;

/**
 * @brief Decoding methods for exploration states
 */
typedef enum {
    CURIOSITY_SNN_DECODE_THRESHOLD = 0,  /**< Threshold-based detection */
    CURIOSITY_SNN_DECODE_COMPETITION,     /**< Winner-take-all */
    CURIOSITY_SNN_DECODE_SOFTMAX,         /**< Soft probabilistic */
    CURIOSITY_SNN_DECODE_INTEGRATION      /**< Evidence accumulation */
} curiosity_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    CURIOSITY_SNN_STATE_IDLE = 0,
    CURIOSITY_SNN_STATE_ENCODING,
    CURIOSITY_SNN_STATE_PROCESSING,
    CURIOSITY_SNN_STATE_DECODING,
    CURIOSITY_SNN_STATE_SIMULATING,
    CURIOSITY_SNN_STATE_ERROR
} curiosity_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Curiosity-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of curiosity dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    curiosity_snn_encoding_t encoding;   /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    curiosity_snn_decoding_t decoding;   /**< Decoding method */
    float novelty_threshold;             /**< Threshold for novelty detection */
    float exploration_threshold;         /**< Minimum exploration drive */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_novelty_detection;       /**< Enable novelty signal detection */
    float novelty_sensitivity;           /**< Novelty detection sensitivity */

    /* Exploration integration */
    bool enable_exploration;             /**< Enable exploration circuits */
    float exploration_gain;              /**< Exploration signal gain */
    bool enable_learning_progress;       /**< Enable learning progress tracking */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} curiosity_snn_config_t;

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
} curiosity_dim_state_t;

/**
 * @brief Exploration drive output
 */
typedef struct {
    float novelty_level;                 /**< Current novelty [0-1] */
    float surprise_level;                /**< Current surprise [0-1] */
    float information_gain;              /**< Expected information gain [0-1] */
    float exploration_drive;             /**< Exploration drive strength */
    float knowledge_gap;                 /**< Knowledge gap awareness */
    bool novelty_detected;               /**< Novelty detected */
    bool high_interest;                  /**< High interest state */
    float interest_magnitude;            /**< Interest magnitude if detected */
    float seeking_level;                 /**< Active seeking level */
    float learning_progress;             /**< Learning progress signal */
} curiosity_drive_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    curiosity_snn_state_t state;         /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_exploration;              /**< Mean exploration drive */
    float novelty_signal;                /**< Current novelty signal */
    float information_signal;            /**< Current information signal */
} curiosity_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t novelty_detections;         /**< Novelty detections */
    uint64_t high_interest_events;       /**< High interest events */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_exploration;              /**< Mean exploration drive */
} curiosity_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct curiosity_snn_bridge curiosity_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Novelty detection callback */
typedef void (*curiosity_snn_novelty_callback_t)(
    curiosity_snn_bridge_t* bridge,
    float novelty_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Drive ready callback */
typedef void (*curiosity_snn_drive_callback_t)(
    curiosity_snn_bridge_t* bridge,
    const curiosity_drive_t* drive,
    void* user_data
);

/** @brief High interest callback */
typedef void (*curiosity_snn_interest_callback_t)(
    curiosity_snn_bridge_t* bridge,
    float interest_level,
    uint32_t interest_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
curiosity_snn_config_t curiosity_snn_config_default(void);

/**
 * @brief Create curiosity SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
curiosity_snn_bridge_t* curiosity_snn_create(const curiosity_snn_config_t* config);

/**
 * @brief Destroy curiosity SNN bridge
 * @param bridge Bridge to destroy
 */
void curiosity_snn_destroy(curiosity_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int curiosity_snn_reset(curiosity_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode curiosity state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int curiosity_snn_encode_state(
    curiosity_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode novelty level
 * @param bridge Bridge handle
 * @param novelty Novelty level [0-1]
 * @param surprise Surprise level [0-1]
 * @return Spike count on success, -1 on failure
 */
int curiosity_snn_encode_novelty(
    curiosity_snn_bridge_t* bridge,
    float novelty,
    float surprise
);

/**
 * @brief Encode knowledge gap
 * @param bridge Bridge handle
 * @param gap_size Knowledge gap size [0-1]
 * @param gap_count Number of active gaps
 * @return Spike count on success, -1 on failure
 */
int curiosity_snn_encode_knowledge_gap(
    curiosity_snn_bridge_t* bridge,
    float gap_size,
    uint32_t gap_count
);

/**
 * @brief Encode information gain
 * @param bridge Bridge handle
 * @param info_gain Expected information gain [0-1]
 * @param info_type Information type classification
 * @return Spike count on success, -1 on failure
 */
int curiosity_snn_encode_info_gain(
    curiosity_snn_bridge_t* bridge,
    float info_gain,
    uint32_t info_type
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate curiosity processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int curiosity_snn_simulate(curiosity_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int curiosity_snn_step(curiosity_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int curiosity_snn_forward(
    curiosity_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get exploration drive from SNN activity
 * @param bridge Bridge handle
 * @param drive Output drive structure
 * @return 0 on success, -1 on failure
 */
int curiosity_snn_get_drive(
    curiosity_snn_bridge_t* bridge,
    curiosity_drive_t* drive
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int curiosity_snn_get_activations(
    curiosity_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for high novelty
 * @param bridge Bridge handle
 * @param novelty_level Output novelty level
 * @return true if high novelty detected, false otherwise
 */
bool curiosity_snn_check_novelty(
    curiosity_snn_bridge_t* bridge,
    float* novelty_level
);

/**
 * @brief Check for high interest
 * @param bridge Bridge handle
 * @param interest_level Output interest level
 * @return true if high interest detected, false otherwise
 */
bool curiosity_snn_check_interest(
    curiosity_snn_bridge_t* bridge,
    float* interest_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool curiosity_snn_check_state_change(
    curiosity_snn_bridge_t* bridge,
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
int curiosity_snn_get_dim_state(
    curiosity_snn_bridge_t* bridge,
    uint32_t dim,
    curiosity_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int curiosity_snn_get_state(
    curiosity_snn_bridge_t* bridge,
    curiosity_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int curiosity_snn_get_stats(curiosity_snn_bridge_t* bridge, curiosity_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int curiosity_snn_reset_stats(curiosity_snn_bridge_t* bridge);

/**
 * @brief Get current exploration drive level
 * @param bridge Bridge handle
 * @return Exploration drive [0-1], -1 on error
 */
float curiosity_snn_get_exploration(curiosity_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float curiosity_snn_get_total_activity(curiosity_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register novelty detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int curiosity_snn_register_novelty_callback(
    curiosity_snn_bridge_t* bridge,
    curiosity_snn_novelty_callback_t callback,
    void* user_data
);

/**
 * @brief Register drive callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int curiosity_snn_register_drive_callback(
    curiosity_snn_bridge_t* bridge,
    curiosity_snn_drive_callback_t callback,
    void* user_data
);

/**
 * @brief Register high interest callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int curiosity_snn_register_interest_callback(
    curiosity_snn_bridge_t* bridge,
    curiosity_snn_interest_callback_t callback,
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
int curiosity_snn_bio_async_connect(curiosity_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int curiosity_snn_bio_async_disconnect(curiosity_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool curiosity_snn_is_bio_async_connected(curiosity_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CURIOSITY_SNN_BRIDGE_H */
