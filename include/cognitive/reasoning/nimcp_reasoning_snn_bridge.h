/**
 * @file nimcp_reasoning_snn_bridge.h
 * @brief Reasoning - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between reasoning engine and spiking neural networks
 * WHY:  Enable biologically-plausible reasoning processes through
 *       population coding and spike-timing dynamics
 * HOW:  Encode reasoning dimensions as spike patterns, decode inference
 *       outputs from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Johnson-Laird (1983): Mental Models theory for deductive reasoning
 * - Kahneman (2011): Dual-process theory (System 1/2)
 * - Pearl (2000): Causal inference and counterfactual reasoning
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex (PFC) for logical operations
 * - Parietal cortex for evidence accumulation
 * - Anterior cingulate cortex (ACC) for conflict detection
 * - Hippocampus for analogical memory retrieval
 *
 * INTEGRATION WITH COGNITION:
 * - Deduction through sequential spike patterns
 * - Induction through population statistics
 * - Causal reasoning through temporal spike dependencies
 *
 * @see nimcp_reasoning_integration.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_REASONING_SNN_BRIDGE_H
#define NIMCP_REASONING_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum reasoning dimensions to encode */
#define REASONING_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per reasoning dimension */
#define REASONING_SNN_NEURONS_PER_DIM    32

/** @brief Default inference threshold */
#define REASONING_SNN_INFERENCE_THRESH   0.6f

/** @brief Default encoding window (ms) */
#define REASONING_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_REASONING_SNN         0x0D50

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Reasoning dimension types for SNN encoding
 */
typedef enum {
    REASON_DIM_DEDUCTION = 0,        /**< Deductive reasoning strength */
    REASON_DIM_INDUCTION,             /**< Inductive reasoning */
    REASON_DIM_ABDUCTION,             /**< Abductive inference */
    REASON_DIM_ANALOGY,               /**< Analogical reasoning */
    REASON_DIM_CAUSAL,                /**< Causal reasoning */
    REASON_DIM_COUNTERFACTUAL,        /**< Counterfactual thinking */
    REASON_DIM_PROBABILITY,           /**< Probabilistic reasoning */
    REASON_DIM_LOGICAL_VALIDITY,      /**< Logical validity */
    REASON_DIM_EVIDENCE_WEIGHT,       /**< Evidence weighting */
    REASON_DIM_INFERENCE_DEPTH,       /**< Inference chain depth */
    REASON_DIM_COUNT
} reasoning_snn_dimension_t;

/**
 * @brief Encoding methods for reasoning contexts
 */
typedef enum {
    REASONING_SNN_ENCODE_RATE = 0,    /**< Rate coding of dimensions */
    REASONING_SNN_ENCODE_TEMPORAL,     /**< Temporal spike patterns */
    REASONING_SNN_ENCODE_POPULATION,   /**< Population vector coding */
    REASONING_SNN_ENCODE_SYNCHRONY     /**< Synchrony-based encoding */
} reasoning_snn_encoding_t;

/**
 * @brief Decoding methods for reasoning outputs
 */
typedef enum {
    REASONING_SNN_DECODE_THRESHOLD = 0, /**< Threshold-based detection */
    REASONING_SNN_DECODE_COMPETITION,    /**< Winner-take-all */
    REASONING_SNN_DECODE_SOFTMAX,        /**< Soft probabilistic */
    REASONING_SNN_DECODE_INTEGRATION     /**< Evidence accumulation */
} reasoning_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    REASONING_SNN_STATE_IDLE = 0,
    REASONING_SNN_STATE_ENCODING,
    REASONING_SNN_STATE_PROCESSING,
    REASONING_SNN_STATE_DECODING,
    REASONING_SNN_STATE_SIMULATING,
    REASONING_SNN_STATE_ERROR
} reasoning_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Reasoning-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of reasoning dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    reasoning_snn_encoding_t encoding;   /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    reasoning_snn_decoding_t decoding;   /**< Decoding method */
    float inference_threshold;           /**< Threshold for inference detection */
    float confidence_threshold;          /**< Minimum confidence required */
    float conclusion_threshold;          /**< Threshold for conclusion validity */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_conflict_detection;      /**< Enable conflict signal detection */
    float conflict_threshold;            /**< Conflict detection threshold */

    /* Reasoning integration */
    bool enable_causal_chains;           /**< Enable causal chain tracking */
    float causal_decay;                  /**< Causal influence decay rate */
    bool enable_analogical_binding;      /**< Enable analogical binding */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} reasoning_snn_config_t;

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
} reasoning_dim_state_t;

/**
 * @brief Reasoning inference output
 */
typedef struct {
    float deduction_strength;            /**< Deductive reasoning [0-1] */
    float induction_strength;            /**< Inductive reasoning [0-1] */
    float abduction_strength;            /**< Abductive inference [0-1] */
    float causal_confidence;             /**< Causal reasoning confidence */
    float analogy_match;                 /**< Analogical match strength */
    float logical_validity;              /**< Logical validity score [0-1] */
    float evidence_weight;               /**< Evidence weighting score */
    float inference_depth;               /**< Inference chain depth */
    bool conclusion_valid;               /**< Valid conclusion reached */
    bool conflict_detected;              /**< Conflict signal detected */
    float conflict_magnitude;            /**< Conflict magnitude if detected */
    float counterfactual_score;          /**< Counterfactual thinking strength */
} reasoning_inference_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    reasoning_snn_state_t state;         /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_inference;                /**< Mean inference strength */
    float conflict_signal;               /**< Current conflict signal */
    float causal_signal;                 /**< Current causal signal */
} reasoning_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t valid_conclusions;          /**< Valid conclusions reached */
    uint64_t conflict_detections;        /**< Conflict signal detections */
    uint64_t causal_chains;              /**< Causal chains processed */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_inference_strength;       /**< Mean inference strength */
} reasoning_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct reasoning_snn_bridge reasoning_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Conflict detection callback */
typedef void (*reasoning_snn_conflict_callback_t)(
    reasoning_snn_bridge_t* bridge,
    float conflict_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Inference ready callback */
typedef void (*reasoning_snn_inference_callback_t)(
    reasoning_snn_bridge_t* bridge,
    const reasoning_inference_t* inference,
    void* user_data
);

/** @brief Conclusion callback */
typedef void (*reasoning_snn_conclusion_callback_t)(
    reasoning_snn_bridge_t* bridge,
    float validity,
    uint32_t conclusion_type,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
reasoning_snn_config_t reasoning_snn_config_default(void);

/**
 * @brief Create reasoning SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
reasoning_snn_bridge_t* reasoning_snn_create(const reasoning_snn_config_t* config);

/**
 * @brief Destroy reasoning SNN bridge
 * @param bridge Bridge to destroy
 */
void reasoning_snn_destroy(reasoning_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int reasoning_snn_reset(reasoning_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode reasoning state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int reasoning_snn_encode_state(
    reasoning_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode deductive reasoning
 * @param bridge Bridge handle
 * @param premise_strength Premise strength [0-1]
 * @param rule_validity Rule validity [0-1]
 * @return Spike count on success, -1 on failure
 */
int reasoning_snn_encode_deduction(
    reasoning_snn_bridge_t* bridge,
    float premise_strength,
    float rule_validity
);

/**
 * @brief Encode causal relationship
 * @param bridge Bridge handle
 * @param cause_strength Cause strength [0-1]
 * @param effect_probability Effect probability [0-1]
 * @return Spike count on success, -1 on failure
 */
int reasoning_snn_encode_causal(
    reasoning_snn_bridge_t* bridge,
    float cause_strength,
    float effect_probability
);

/**
 * @brief Encode evidence
 * @param bridge Bridge handle
 * @param evidence_strength Evidence strength [0-1]
 * @param evidence_count Number of evidence items
 * @return Spike count on success, -1 on failure
 */
int reasoning_snn_encode_evidence(
    reasoning_snn_bridge_t* bridge,
    float evidence_strength,
    uint32_t evidence_count
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate reasoning processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int reasoning_snn_simulate(reasoning_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int reasoning_snn_step(reasoning_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int reasoning_snn_forward(
    reasoning_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get reasoning inference from SNN activity
 * @param bridge Bridge handle
 * @param inference Output inference structure
 * @return 0 on success, -1 on failure
 */
int reasoning_snn_get_inference(
    reasoning_snn_bridge_t* bridge,
    reasoning_inference_t* inference
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int reasoning_snn_get_activations(
    reasoning_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for reasoning conflict
 * @param bridge Bridge handle
 * @param conflict_level Output conflict level
 * @return true if conflict detected, false otherwise
 */
bool reasoning_snn_check_conflict(
    reasoning_snn_bridge_t* bridge,
    float* conflict_level
);

/**
 * @brief Check for valid conclusion
 * @param bridge Bridge handle
 * @param validity Output validity score
 * @return true if valid conclusion, false otherwise
 */
bool reasoning_snn_check_conclusion(
    reasoning_snn_bridge_t* bridge,
    float* validity
);

/**
 * @brief Check for causal inference
 * @param bridge Bridge handle
 * @param causal_strength Output causal strength
 * @return true if causal inference detected, false otherwise
 */
bool reasoning_snn_check_causal(
    reasoning_snn_bridge_t* bridge,
    float* causal_strength
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
int reasoning_snn_get_dim_state(
    reasoning_snn_bridge_t* bridge,
    uint32_t dim,
    reasoning_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int reasoning_snn_get_state(
    reasoning_snn_bridge_t* bridge,
    reasoning_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int reasoning_snn_get_stats(reasoning_snn_bridge_t* bridge, reasoning_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int reasoning_snn_reset_stats(reasoning_snn_bridge_t* bridge);

/**
 * @brief Get current inference strength
 * @param bridge Bridge handle
 * @return Inference strength [0-1], -1 on error
 */
float reasoning_snn_get_inference_strength(reasoning_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float reasoning_snn_get_total_activity(reasoning_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register conflict detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int reasoning_snn_register_conflict_callback(
    reasoning_snn_bridge_t* bridge,
    reasoning_snn_conflict_callback_t callback,
    void* user_data
);

/**
 * @brief Register inference callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int reasoning_snn_register_inference_callback(
    reasoning_snn_bridge_t* bridge,
    reasoning_snn_inference_callback_t callback,
    void* user_data
);

/**
 * @brief Register conclusion callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int reasoning_snn_register_conclusion_callback(
    reasoning_snn_bridge_t* bridge,
    reasoning_snn_conclusion_callback_t callback,
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
int reasoning_snn_bio_async_connect(reasoning_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int reasoning_snn_bio_async_disconnect(reasoning_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool reasoning_snn_is_bio_async_connected(reasoning_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_SNN_BRIDGE_H */
