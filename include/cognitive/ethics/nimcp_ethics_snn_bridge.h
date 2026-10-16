/**
 * @file nimcp_ethics_snn_bridge.h
 * @brief Ethics Engine - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between ethics engine and spiking neural networks
 * WHY:  Enable biologically-plausible moral intuition processing through
 *       population coding and spike-timing dynamics
 * HOW:  Encode ethical contexts as spike patterns, decode moral judgments
 *       from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Haidt (2001): Social Intuitionist Model - moral judgments arise from
 *   fast intuitive processes, not slow deliberation
 * - Greene (2009): Dual-process theory - automatic vs controlled moral cognition
 * - Moll (2005): Neural correlates of moral sensitivity
 *
 * BIOLOGICAL BASIS:
 * - Ventromedial prefrontal cortex (vmPFC) for value integration
 * - Anterior cingulate cortex (ACC) for conflict monitoring
 * - Insula for emotional salience and disgust response
 * - Temporoparietal junction (TPJ) for perspective-taking
 *
 * INTEGRATION WITH ASIMOV'S LAWS:
 * - First Law encoding in dedicated protection populations
 * - Harm detection as rapid spike response (< 50ms)
 * - Golden Rule as cross-population synchrony
 *
 * @see nimcp_ethics.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_ETHICS_SNN_BRIDGE_H
#define NIMCP_ETHICS_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum ethical dimensions to encode */
#define ETHICS_SNN_MAX_DIMENSIONS        16

/** @brief Neurons per ethical dimension */
#define ETHICS_SNN_NEURONS_PER_DIM       32

/** @brief Default harm detection threshold */
#define ETHICS_SNN_HARM_THRESHOLD        0.3f

/** @brief Default encoding window (ms) */
#define ETHICS_SNN_ENCODING_WINDOW       50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_ETHICS_SNN            0x0D30

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Ethical dimension types for SNN encoding
 */
typedef enum {
    ETHICS_DIM_HARM = 0,          /**< Harm/care dimension */
    ETHICS_DIM_FAIRNESS,           /**< Fairness/reciprocity */
    ETHICS_DIM_LOYALTY,            /**< In-group loyalty */
    ETHICS_DIM_AUTHORITY,          /**< Respect for authority */
    ETHICS_DIM_SANCTITY,           /**< Purity/sanctity */
    ETHICS_DIM_LIBERTY,            /**< Liberty/oppression */
    ETHICS_DIM_GOLDEN_RULE,        /**< Golden Rule activation */
    ETHICS_DIM_ASIMOV_FIRST,       /**< First Law (no harm) */
    ETHICS_DIM_ASIMOV_ZEROTH,      /**< Zeroth Law (humanity) */
    ETHICS_DIM_EMPATHY,            /**< Empathy response */
    ETHICS_DIM_CONFLICT,           /**< Moral conflict signal */
    ETHICS_DIM_CONFIDENCE,         /**< Decision confidence */
    ETHICS_DIM_COUNT
} ethics_snn_dimension_t;

/**
 * @brief Encoding methods for ethical contexts
 */
typedef enum {
    ETHICS_SNN_ENCODE_RATE = 0,   /**< Rate coding of dimensions */
    ETHICS_SNN_ENCODE_TEMPORAL,    /**< Temporal spike patterns */
    ETHICS_SNN_ENCODE_POPULATION,  /**< Population vector coding */
    ETHICS_SNN_ENCODE_SYNCHRONY    /**< Synchrony-based encoding */
} ethics_snn_encoding_t;

/**
 * @brief Decoding methods for moral judgments
 */
typedef enum {
    ETHICS_SNN_DECODE_THRESHOLD = 0, /**< Threshold-based decision */
    ETHICS_SNN_DECODE_COMPETITION,    /**< Winner-take-all */
    ETHICS_SNN_DECODE_SOFTMAX,        /**< Soft probabilistic */
    ETHICS_SNN_DECODE_INTEGRATION     /**< Evidence accumulation */
} ethics_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    ETHICS_SNN_STATE_IDLE = 0,
    ETHICS_SNN_STATE_ENCODING,
    ETHICS_SNN_STATE_PROCESSING,
    ETHICS_SNN_STATE_DECODING,
    ETHICS_SNN_STATE_SIMULATING,
    ETHICS_SNN_STATE_ERROR
} ethics_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Ethics-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;         /**< Number of ethical dimensions */
    uint32_t neurons_per_dim;        /**< Neurons per dimension */
    uint32_t hidden_dim;             /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                     /**< Simulation timestep (ms) */
    float encoding_window_ms;        /**< Encoding time window */
    float integration_tau_ms;        /**< Evidence integration time constant */

    /* Encoding parameters */
    ethics_snn_encoding_t encoding;  /**< Encoding method */
    float encoding_gain;             /**< Encoding strength gain */
    float baseline_rate_hz;          /**< Baseline firing rate */
    float max_rate_hz;               /**< Maximum firing rate */

    /* Decoding parameters */
    ethics_snn_decoding_t decoding;  /**< Decoding method */
    float decision_threshold;        /**< Threshold for action decision */
    float harm_threshold;            /**< Threshold for harm detection */
    float confidence_threshold;      /**< Minimum confidence required */

    /* Network dynamics */
    bool enable_competition;         /**< Enable dimension competition */
    float inhibition_strength;       /**< Lateral inhibition weight */
    bool enable_conflict_detection;  /**< Enable moral conflict signals */
    float conflict_threshold;        /**< Conflict detection threshold */

    /* Asimov integration */
    bool enable_asimov_populations;  /**< Dedicated Asimov Law neurons */
    float first_law_priority;        /**< Priority weight for First Law */
    float zeroth_law_priority;       /**< Priority weight for Zeroth Law */

    /* Bio-async integration */
    bool enable_bio_async;           /**< Enable bio-async callbacks */
    bool enable_plasticity_integration; /**< Enable plasticity bridge */
} ethics_snn_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Per-dimension state tracking
 */
typedef struct {
    float activation;                /**< Current activation level */
    float accumulated_evidence;      /**< Accumulated evidence */
    uint32_t spike_count;            /**< Recent spike count */
    float mean_rate_hz;              /**< Mean firing rate */
    uint64_t last_spike_time_us;     /**< Last spike timestamp */
} ethics_dim_state_t;

/**
 * @brief Moral judgment output
 */
typedef struct {
    float allow_score;               /**< Score for allowing action */
    float block_score;               /**< Score for blocking action */
    float modify_score;              /**< Score for modifying action */
    float confidence;                /**< Decision confidence */
    bool harm_detected;              /**< Rapid harm detection flag */
    bool conflict_detected;          /**< Moral conflict detected */
    float golden_rule_activation;    /**< Golden Rule compliance */
    float first_law_activation;      /**< First Law activation */
} ethics_judgment_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    ethics_snn_state_t state;        /**< Current operational state */
    uint32_t active_dimensions;      /**< Number of active dimensions */
    float total_activity;            /**< Total network activity */
    float mean_confidence;           /**< Mean decision confidence */
    float harm_signal;               /**< Current harm signal strength */
    float conflict_signal;           /**< Current conflict signal */
} ethics_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;      /**< Total evaluations performed */
    uint64_t total_simulations;      /**< Total simulation steps */
    uint64_t total_spikes;           /**< Total spikes generated */
    uint64_t harm_detections;        /**< Rapid harm detections */
    uint64_t conflict_detections;    /**< Moral conflict detections */
    uint64_t first_law_activations;  /**< First Law triggered */
    float mean_decision_time_ms;     /**< Mean decision latency */
    float mean_confidence;           /**< Mean confidence score */
} ethics_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct ethics_snn_bridge ethics_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Harm detection callback (rapid response) */
typedef void (*ethics_snn_harm_callback_t)(
    ethics_snn_bridge_t* bridge,
    float harm_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Judgment ready callback */
typedef void (*ethics_snn_judgment_callback_t)(
    ethics_snn_bridge_t* bridge,
    const ethics_judgment_t* judgment,
    void* user_data
);

/** @brief Conflict detection callback */
typedef void (*ethics_snn_conflict_callback_t)(
    ethics_snn_bridge_t* bridge,
    float conflict_level,
    uint32_t dim1,
    uint32_t dim2,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
ethics_snn_config_t ethics_snn_config_default(void);

/**
 * @brief Create ethics SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
ethics_snn_bridge_t* ethics_snn_create(const ethics_snn_config_t* config);

/**
 * @brief Destroy ethics SNN bridge
 * @param bridge Bridge to destroy
 */
void ethics_snn_destroy(ethics_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int ethics_snn_reset(ethics_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode ethical context into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int ethics_snn_encode_context(
    ethics_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode harm potential (rapid pathway)
 * @param bridge Bridge handle
 * @param harm_level Harm level [0-1]
 * @param urgency Urgency modifier [0-1]
 * @return Spike count on success, -1 on failure
 */
int ethics_snn_encode_harm(
    ethics_snn_bridge_t* bridge,
    float harm_level,
    float urgency
);

/**
 * @brief Encode Golden Rule context
 * @param bridge Bridge handle
 * @param self_impact Impact on self [0-1]
 * @param other_impact Impact on other [0-1]
 * @param empathy_level Empathy activation [0-1]
 * @return Spike count on success, -1 on failure
 */
int ethics_snn_encode_golden_rule(
    ethics_snn_bridge_t* bridge,
    float self_impact,
    float other_impact,
    float empathy_level
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate ethical processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int ethics_snn_simulate(ethics_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int ethics_snn_step(ethics_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int ethics_snn_forward(
    ethics_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get moral judgment from SNN activity
 * @param bridge Bridge handle
 * @param judgment Output judgment structure
 * @return 0 on success, -1 on failure
 */
int ethics_snn_get_judgment(
    ethics_snn_bridge_t* bridge,
    ethics_judgment_t* judgment
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int ethics_snn_get_activations(
    ethics_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for rapid harm detection
 * @param bridge Bridge handle
 * @param harm_level Output harm level
 * @return true if harm detected, false otherwise
 */
bool ethics_snn_check_harm(
    ethics_snn_bridge_t* bridge,
    float* harm_level
);

/**
 * @brief Check for moral conflict
 * @param bridge Bridge handle
 * @param conflict_level Output conflict level
 * @return true if conflict detected, false otherwise
 */
bool ethics_snn_check_conflict(
    ethics_snn_bridge_t* bridge,
    float* conflict_level
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
int ethics_snn_get_dim_state(
    ethics_snn_bridge_t* bridge,
    uint32_t dim,
    ethics_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int ethics_snn_get_state(
    ethics_snn_bridge_t* bridge,
    ethics_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int ethics_snn_get_stats(ethics_snn_bridge_t* bridge, ethics_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int ethics_snn_reset_stats(ethics_snn_bridge_t* bridge);

/**
 * @brief Get decision confidence
 * @param bridge Bridge handle
 * @return Confidence level [0-1], -1 on error
 */
float ethics_snn_get_confidence(ethics_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float ethics_snn_get_total_activity(ethics_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register harm detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int ethics_snn_register_harm_callback(
    ethics_snn_bridge_t* bridge,
    ethics_snn_harm_callback_t callback,
    void* user_data
);

/**
 * @brief Register judgment callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int ethics_snn_register_judgment_callback(
    ethics_snn_bridge_t* bridge,
    ethics_snn_judgment_callback_t callback,
    void* user_data
);

/**
 * @brief Register conflict detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int ethics_snn_register_conflict_callback(
    ethics_snn_bridge_t* bridge,
    ethics_snn_conflict_callback_t callback,
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
int ethics_snn_bio_async_connect(ethics_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int ethics_snn_bio_async_disconnect(ethics_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool ethics_snn_is_bio_async_connected(ethics_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ETHICS_SNN_BRIDGE_H */
