/**
 * @file nimcp_imagination_snn_bridge.h
 * @brief Imagination - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between imagination engine and spiking neural networks
 * WHY:  Enable biologically-plausible mental imagery through population coding
 *       and spike-timing dynamics
 * HOW:  Encode imagination dimensions as spike patterns, decode generative
 *       signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Pearson (2019): The human imagination - neural mechanisms
 * - Kosslyn et al. (2001): Neural foundations of mental imagery
 * - Hassabis & Maguire (2009): The construction system of the brain
 *
 * BIOLOGICAL BASIS:
 * - Visual cortex reactivation during mental imagery
 * - Prefrontal-parietal control network for directed imagery
 * - Hippocampal scene construction and pattern completion
 * - Default mode network for spontaneous imagination
 *
 * INTEGRATION WITH IMAGINATION:
 * - Mental imagery vividness through firing rate modulation
 * - Scene coherence via synchronized population activity
 * - Creative combination through stochastic spike patterns
 *
 * @see nimcp_imagination_engine.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_IMAGINATION_SNN_BRIDGE_H
#define NIMCP_IMAGINATION_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum imagination dimensions to encode */
#define IMAGINATION_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per imagination dimension */
#define IMAGINATION_SNN_NEURONS_PER_DIM    32

/** @brief Default vividness threshold */
#define IMAGINATION_SNN_VIVIDNESS_THRESH   0.5f

/** @brief Default encoding window (ms) */
#define IMAGINATION_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_IMAGINATION_SNN         0x1A50

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Imagination dimension types for SNN encoding
 */
typedef enum {
    IMAGINATION_DIM_VIVIDNESS = 0,       /**< Mental imagery vividness */
    IMAGINATION_DIM_DETAIL,              /**< Level of detail/resolution */
    IMAGINATION_DIM_COHERENCE,           /**< Scene coherence level */
    IMAGINATION_DIM_CREATIVITY,          /**< Creative combination strength */
    IMAGINATION_DIM_COUNTERFACTUAL,      /**< Counterfactual divergence */
    IMAGINATION_DIM_PROSPECTIVE,         /**< Future simulation depth */
    IMAGINATION_DIM_CONTROLLABILITY,     /**< Ease of manipulation */
    IMAGINATION_DIM_NOVELTY,             /**< Novelty of generated content */
    IMAGINATION_DIM_REALITY_DISTANCE,    /**< Distance from reality */
    IMAGINATION_DIM_SCENARIO_COMPLEXITY, /**< Scenario complexity level */
    IMAGINATION_DIM_COUNT
} imagination_snn_dimension_t;

/**
 * @brief Encoding methods for imagination contexts
 */
typedef enum {
    IMAGINATION_SNN_ENCODE_RATE = 0,     /**< Rate coding of dimensions */
    IMAGINATION_SNN_ENCODE_TEMPORAL,      /**< Temporal spike patterns */
    IMAGINATION_SNN_ENCODE_POPULATION,    /**< Population vector coding */
    IMAGINATION_SNN_ENCODE_SYNCHRONY      /**< Synchrony-based encoding */
} imagination_snn_encoding_t;

/**
 * @brief Decoding methods for generation states
 */
typedef enum {
    IMAGINATION_SNN_DECODE_THRESHOLD = 0, /**< Threshold-based detection */
    IMAGINATION_SNN_DECODE_COMPETITION,   /**< Winner-take-all */
    IMAGINATION_SNN_DECODE_SOFTMAX,       /**< Soft probabilistic */
    IMAGINATION_SNN_DECODE_INTEGRATION    /**< Evidence accumulation */
} imagination_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    IMAGINATION_SNN_STATE_IDLE = 0,
    IMAGINATION_SNN_STATE_ENCODING,
    IMAGINATION_SNN_STATE_PROCESSING,
    IMAGINATION_SNN_STATE_DECODING,
    IMAGINATION_SNN_STATE_SIMULATING,
    IMAGINATION_SNN_STATE_ERROR
} imagination_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Imagination-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of imagination dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    imagination_snn_encoding_t encoding; /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    imagination_snn_decoding_t decoding; /**< Decoding method */
    float vividness_threshold;           /**< Threshold for vivid imagery */
    float coherence_threshold;           /**< Minimum scene coherence */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_vividness_detection;     /**< Enable vividness signal detection */
    float vividness_sensitivity;         /**< Vividness detection sensitivity */

    /* Generation integration */
    bool enable_generation;              /**< Enable generation circuits */
    float generation_gain;               /**< Generation signal gain */
    bool enable_creativity_modulation;   /**< Enable creativity modulation */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} imagination_snn_config_t;

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
} imagination_dim_state_t;

/**
 * @brief Mental imagery output
 */
typedef struct {
    float vividness_level;               /**< Current vividness [0-1] */
    float detail_level;                  /**< Current detail [0-1] */
    float coherence_level;               /**< Scene coherence [0-1] */
    float creativity_level;              /**< Creativity strength [0-1] */
    float counterfactual_level;          /**< Counterfactual divergence */
    bool vivid_imagery_active;           /**< Vivid imagery detected */
    bool creative_mode_active;           /**< High creativity state */
    float creative_magnitude;            /**< Creativity magnitude if active */
    float controllability_level;         /**< Control ease level */
    float scenario_complexity;           /**< Scenario complexity signal */
} imagination_imagery_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    imagination_snn_state_t state;       /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_vividness;                /**< Mean vividness level */
    float coherence_signal;              /**< Current coherence signal */
    float creativity_signal;             /**< Current creativity signal */
} imagination_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t vivid_imagery_events;       /**< Vivid imagery detections */
    uint64_t creative_mode_events;       /**< Creative mode events */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_vividness;                /**< Mean vividness level */
} imagination_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct imagination_snn_bridge imagination_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Vividness detection callback */
typedef void (*imagination_snn_vividness_callback_t)(
    imagination_snn_bridge_t* bridge,
    float vividness_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Imagery ready callback */
typedef void (*imagination_snn_imagery_callback_t)(
    imagination_snn_bridge_t* bridge,
    const imagination_imagery_t* imagery,
    void* user_data
);

/** @brief Creative mode callback */
typedef void (*imagination_snn_creative_callback_t)(
    imagination_snn_bridge_t* bridge,
    float creative_level,
    uint32_t creative_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
imagination_snn_config_t imagination_snn_config_default(void);

/**
 * @brief Create imagination SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
imagination_snn_bridge_t* imagination_snn_create(const imagination_snn_config_t* config);

/**
 * @brief Destroy imagination SNN bridge
 * @param bridge Bridge to destroy
 */
void imagination_snn_destroy(imagination_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int imagination_snn_reset(imagination_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode imagination state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int imagination_snn_encode_state(
    imagination_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode vividness level
 * @param bridge Bridge handle
 * @param vividness Vividness level [0-1]
 * @param detail Detail level [0-1]
 * @return Spike count on success, -1 on failure
 */
int imagination_snn_encode_vividness(
    imagination_snn_bridge_t* bridge,
    float vividness,
    float detail
);

/**
 * @brief Encode scenario generation parameters
 * @param bridge Bridge handle
 * @param coherence Scene coherence [0-1]
 * @param complexity Scenario complexity [0-1]
 * @return Spike count on success, -1 on failure
 */
int imagination_snn_encode_scenario(
    imagination_snn_bridge_t* bridge,
    float coherence,
    float complexity
);

/**
 * @brief Encode creative combination state
 * @param bridge Bridge handle
 * @param creativity Creativity level [0-1]
 * @param novelty Novelty of combination [0-1]
 * @return Spike count on success, -1 on failure
 */
int imagination_snn_encode_creativity(
    imagination_snn_bridge_t* bridge,
    float creativity,
    float novelty
);

/**
 * @brief Encode counterfactual simulation state
 * @param bridge Bridge handle
 * @param divergence Counterfactual divergence [0-1]
 * @param steps_ahead Simulation depth
 * @return Spike count on success, -1 on failure
 */
int imagination_snn_encode_counterfactual(
    imagination_snn_bridge_t* bridge,
    float divergence,
    uint32_t steps_ahead
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate imagination processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int imagination_snn_simulate(imagination_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int imagination_snn_step(imagination_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int imagination_snn_forward(
    imagination_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get mental imagery output from SNN activity
 * @param bridge Bridge handle
 * @param imagery Output imagery structure
 * @return 0 on success, -1 on failure
 */
int imagination_snn_get_imagery(
    imagination_snn_bridge_t* bridge,
    imagination_imagery_t* imagery
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int imagination_snn_get_activations(
    imagination_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for vivid imagery
 * @param bridge Bridge handle
 * @param vividness_level Output vividness level
 * @return true if vivid imagery detected, false otherwise
 */
bool imagination_snn_check_vividness(
    imagination_snn_bridge_t* bridge,
    float* vividness_level
);

/**
 * @brief Check for creative mode
 * @param bridge Bridge handle
 * @param creative_level Output creative level
 * @return true if creative mode detected, false otherwise
 */
bool imagination_snn_check_creative(
    imagination_snn_bridge_t* bridge,
    float* creative_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool imagination_snn_check_state_change(
    imagination_snn_bridge_t* bridge,
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
int imagination_snn_get_dim_state(
    imagination_snn_bridge_t* bridge,
    uint32_t dim,
    imagination_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int imagination_snn_get_state(
    imagination_snn_bridge_t* bridge,
    imagination_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int imagination_snn_get_stats(imagination_snn_bridge_t* bridge, imagination_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int imagination_snn_reset_stats(imagination_snn_bridge_t* bridge);

/**
 * @brief Get current vividness level
 * @param bridge Bridge handle
 * @return Vividness [0-1], -1 on error
 */
float imagination_snn_get_vividness(imagination_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float imagination_snn_get_total_activity(imagination_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register vividness detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int imagination_snn_register_vividness_callback(
    imagination_snn_bridge_t* bridge,
    imagination_snn_vividness_callback_t callback,
    void* user_data
);

/**
 * @brief Register imagery callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int imagination_snn_register_imagery_callback(
    imagination_snn_bridge_t* bridge,
    imagination_snn_imagery_callback_t callback,
    void* user_data
);

/**
 * @brief Register creative mode callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int imagination_snn_register_creative_callback(
    imagination_snn_bridge_t* bridge,
    imagination_snn_creative_callback_t callback,
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
int imagination_snn_bio_async_connect(imagination_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int imagination_snn_bio_async_disconnect(imagination_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool imagination_snn_is_bio_async_connected(imagination_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMAGINATION_SNN_BRIDGE_H */
