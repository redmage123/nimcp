/**
 * @file nimcp_meta_learning_snn_bridge.h
 * @brief Meta Learning - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between meta learning engine and spiking neural networks
 * WHY:  Enable biologically-plausible learning-to-learn through population coding
 *       and spike-timing dynamics
 * HOW:  Encode meta-learning states as spike patterns, decode adaptation insights
 *       from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Schmidhuber (1987): Learning to learn using gradient descent
 * - Finn (2017): Model-Agnostic Meta-Learning (MAML)
 * - Lake (2015): Human-level concept learning through probabilistic program induction
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex for strategy selection and task switching
 * - Hippocampus for rapid task encoding and similarity detection
 * - Dopaminergic signals modulate learning rate adaptation
 * - Cerebellar circuits for timing and procedural meta-learning
 *
 * INTEGRATION WITH LEARNING:
 * - Learning rate adaptation through population activity
 * - Strategy selection via winner-take-all competition
 * - Transfer learning through pattern similarity detection
 *
 * @see nimcp_meta_learning.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_META_LEARNING_SNN_BRIDGE_H
#define NIMCP_META_LEARNING_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum meta learning dimensions to encode */
#define META_LEARNING_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per meta learning dimension */
#define META_LEARNING_SNN_NEURONS_PER_DIM    32

/** @brief Default adaptation threshold */
#define META_LEARNING_SNN_ADAPTATION_THRESH  0.5f

/** @brief Default encoding window (ms) */
#define META_LEARNING_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_META_LEARNING_SNN         0x0D50

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Meta learning dimension types for SNN encoding
 */
typedef enum {
    META_DIM_LEARNING_RATE = 0,     /**< Learning rate adaptation level */
    META_DIM_STRATEGY_SELECT,        /**< Strategy selection signal */
    META_DIM_TRANSFER,               /**< Transfer learning potential */
    META_DIM_GENERALIZATION,         /**< Generalization ability */
    META_DIM_TASK_SIMILARITY,        /**< Task similarity detection */
    META_DIM_PRIOR_KNOWLEDGE,        /**< Prior knowledge utilization */
    META_DIM_ADAPTATION_SPEED,       /**< Adaptation speed */
    META_DIM_LEARNING_TO_LEARN,      /**< Meta-learning progress */
    META_DIM_CURRICULUM,             /**< Curriculum awareness */
    META_DIM_CONSOLIDATION,          /**< Knowledge consolidation */
    META_DIM_COUNT
} meta_learning_snn_dimension_t;

/**
 * @brief Encoding methods for meta learning contexts
 */
typedef enum {
    META_LEARNING_SNN_ENCODE_RATE = 0,   /**< Rate coding of dimensions */
    META_LEARNING_SNN_ENCODE_TEMPORAL,    /**< Temporal spike patterns */
    META_LEARNING_SNN_ENCODE_POPULATION,  /**< Population vector coding */
    META_LEARNING_SNN_ENCODE_SYNCHRONY    /**< Synchrony-based encoding */
} meta_learning_snn_encoding_t;

/**
 * @brief Decoding methods for meta learning states
 */
typedef enum {
    META_LEARNING_SNN_DECODE_THRESHOLD = 0, /**< Threshold-based detection */
    META_LEARNING_SNN_DECODE_COMPETITION,    /**< Winner-take-all */
    META_LEARNING_SNN_DECODE_SOFTMAX,        /**< Soft probabilistic */
    META_LEARNING_SNN_DECODE_INTEGRATION     /**< Evidence accumulation */
} meta_learning_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    META_LEARNING_SNN_STATE_IDLE = 0,
    META_LEARNING_SNN_STATE_ENCODING,
    META_LEARNING_SNN_STATE_PROCESSING,
    META_LEARNING_SNN_STATE_DECODING,
    META_LEARNING_SNN_STATE_SIMULATING,
    META_LEARNING_SNN_STATE_ERROR
} meta_learning_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Meta Learning-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of meta learning dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    meta_learning_snn_encoding_t encoding; /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    meta_learning_snn_decoding_t decoding; /**< Decoding method */
    float adaptation_threshold;          /**< Threshold for adaptation detection */
    float transfer_threshold;            /**< Transfer learning threshold */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_strategy_selection;      /**< Enable strategy selection */
    float strategy_threshold;            /**< Strategy selection threshold */

    /* Meta-learning integration */
    bool enable_curriculum;              /**< Enable curriculum awareness */
    float curriculum_gain;               /**< Curriculum signal gain */
    bool enable_transfer_detection;      /**< Enable transfer detection */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} meta_learning_snn_config_t;

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
} meta_learning_dim_state_t;

/**
 * @brief Meta learning insight output
 */
typedef struct {
    float learning_rate_level;           /**< Current learning rate [0-1] */
    float adaptation_level;              /**< Adaptation readiness [0-1] */
    float transfer_potential;            /**< Transfer learning potential [0-1] */
    float generalization_score;          /**< Generalization ability */
    float task_similarity;               /**< Task similarity score */
    bool strategy_change_detected;       /**< Strategy change detected */
    bool transfer_opportunity;           /**< Transfer opportunity detected */
    float transfer_magnitude;            /**< Transfer magnitude if detected */
    float meta_learning_progress;        /**< Meta-learning progress level */
    float consolidation_score;           /**< Knowledge consolidation score */
} meta_learning_insight_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    meta_learning_snn_state_t state;     /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_adaptation;               /**< Mean adaptation level */
    float transfer_signal;               /**< Current transfer signal */
    float strategy_signal;               /**< Current strategy signal */
} meta_learning_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t adaptation_detections;      /**< Adaptation events detected */
    uint64_t transfer_detections;        /**< Transfer opportunities detected */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_adaptation_level;         /**< Mean adaptation level */
} meta_learning_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct meta_learning_snn_bridge meta_learning_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Adaptation detection callback */
typedef void (*meta_learning_snn_adaptation_callback_t)(
    meta_learning_snn_bridge_t* bridge,
    float adaptation_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Insight ready callback */
typedef void (*meta_learning_snn_insight_callback_t)(
    meta_learning_snn_bridge_t* bridge,
    const meta_learning_insight_t* insight,
    void* user_data
);

/** @brief Transfer detection callback */
typedef void (*meta_learning_snn_transfer_callback_t)(
    meta_learning_snn_bridge_t* bridge,
    float transfer_level,
    uint32_t source_task,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
meta_learning_snn_config_t meta_learning_snn_config_default(void);

/**
 * @brief Create meta learning SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
meta_learning_snn_bridge_t* meta_learning_snn_create(const meta_learning_snn_config_t* config);

/**
 * @brief Destroy meta learning SNN bridge
 * @param bridge Bridge to destroy
 */
void meta_learning_snn_destroy(meta_learning_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int meta_learning_snn_reset(meta_learning_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode meta learning state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int meta_learning_snn_encode_state(
    meta_learning_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode learning rate adaptation
 * @param bridge Bridge handle
 * @param current_rate Current learning rate [0-1]
 * @param target_rate Target learning rate [0-1]
 * @return Spike count on success, -1 on failure
 */
int meta_learning_snn_encode_learning_rate(
    meta_learning_snn_bridge_t* bridge,
    float current_rate,
    float target_rate
);

/**
 * @brief Encode task similarity
 * @param bridge Bridge handle
 * @param similarity Task similarity score [0-1]
 * @param task_id Current task ID
 * @return Spike count on success, -1 on failure
 */
int meta_learning_snn_encode_task_similarity(
    meta_learning_snn_bridge_t* bridge,
    float similarity,
    uint32_t task_id
);

/**
 * @brief Encode transfer opportunity
 * @param bridge Bridge handle
 * @param transfer_potential Transfer potential [0-1]
 * @param source_task Source task for transfer
 * @return Spike count on success, -1 on failure
 */
int meta_learning_snn_encode_transfer(
    meta_learning_snn_bridge_t* bridge,
    float transfer_potential,
    uint32_t source_task
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate meta learning processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int meta_learning_snn_simulate(meta_learning_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int meta_learning_snn_step(meta_learning_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int meta_learning_snn_forward(
    meta_learning_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get meta learning insight from SNN activity
 * @param bridge Bridge handle
 * @param insight Output insight structure
 * @return 0 on success, -1 on failure
 */
int meta_learning_snn_get_insight(
    meta_learning_snn_bridge_t* bridge,
    meta_learning_insight_t* insight
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int meta_learning_snn_get_activations(
    meta_learning_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for high adaptation readiness
 * @param bridge Bridge handle
 * @param adaptation_level Output adaptation level
 * @return true if high adaptation detected, false otherwise
 */
bool meta_learning_snn_check_adaptation(
    meta_learning_snn_bridge_t* bridge,
    float* adaptation_level
);

/**
 * @brief Check for transfer opportunity
 * @param bridge Bridge handle
 * @param transfer_level Output transfer level
 * @return true if transfer detected, false otherwise
 */
bool meta_learning_snn_check_transfer(
    meta_learning_snn_bridge_t* bridge,
    float* transfer_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool meta_learning_snn_check_state_change(
    meta_learning_snn_bridge_t* bridge,
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
int meta_learning_snn_get_dim_state(
    meta_learning_snn_bridge_t* bridge,
    uint32_t dim,
    meta_learning_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int meta_learning_snn_get_state(
    meta_learning_snn_bridge_t* bridge,
    meta_learning_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int meta_learning_snn_get_stats(meta_learning_snn_bridge_t* bridge, meta_learning_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int meta_learning_snn_reset_stats(meta_learning_snn_bridge_t* bridge);

/**
 * @brief Get current adaptation level
 * @param bridge Bridge handle
 * @return Adaptation level [0-1], -1 on error
 */
float meta_learning_snn_get_adaptation(meta_learning_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float meta_learning_snn_get_total_activity(meta_learning_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register adaptation detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int meta_learning_snn_register_adaptation_callback(
    meta_learning_snn_bridge_t* bridge,
    meta_learning_snn_adaptation_callback_t callback,
    void* user_data
);

/**
 * @brief Register insight callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int meta_learning_snn_register_insight_callback(
    meta_learning_snn_bridge_t* bridge,
    meta_learning_snn_insight_callback_t callback,
    void* user_data
);

/**
 * @brief Register transfer detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int meta_learning_snn_register_transfer_callback(
    meta_learning_snn_bridge_t* bridge,
    meta_learning_snn_transfer_callback_t callback,
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
int meta_learning_snn_bio_async_connect(meta_learning_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int meta_learning_snn_bio_async_disconnect(meta_learning_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool meta_learning_snn_is_bio_async_connected(meta_learning_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_META_LEARNING_SNN_BRIDGE_H */
