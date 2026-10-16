/**
 * @file nimcp_knowledge_snn_bridge.h
 * @brief Knowledge - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between knowledge system and spiking neural networks
 * WHY:  Enable biologically-plausible semantic knowledge encoding through
 *       population coding and spike-timing dynamics
 * HOW:  Encode knowledge dimensions as spike patterns, decode retrieval
 *       signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Collins & Quillian (1969): Hierarchical semantic memory
 * - McClelland & Rogers (2003): Semantic cognition and distributed representations
 * - Patterson et al. (2007): Semantic dementia and hub-and-spoke model
 *
 * BIOLOGICAL BASIS:
 * - Temporal cortex for semantic knowledge storage
 * - Hippocampus for episodic-semantic interactions
 * - Prefrontal cortex for controlled semantic retrieval
 * - Angular gyrus for multimodal concept integration
 *
 * INTEGRATION WITH KNOWLEDGE:
 * - Semantic encoding through population activity
 * - Concept activation via spreading neural activity
 * - Knowledge retrieval through pattern completion
 * - Association strength through synaptic weights
 *
 * @see nimcp_knowledge.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_KNOWLEDGE_SNN_BRIDGE_H
#define NIMCP_KNOWLEDGE_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum knowledge dimensions to encode */
#define KNOWLEDGE_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per knowledge dimension */
#define KNOWLEDGE_SNN_NEURONS_PER_DIM    32

/** @brief Default activation threshold */
#define KNOWLEDGE_SNN_ACTIVATION_THRESH  0.5f

/** @brief Default encoding window (ms) */
#define KNOWLEDGE_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_KNOWLEDGE_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Knowledge dimension types for SNN encoding
 */
typedef enum {
    KNOWLEDGE_DIM_SEMANTIC = 0,          /**< Semantic content level */
    KNOWLEDGE_DIM_ACTIVATION,             /**< Concept activation strength */
    KNOWLEDGE_DIM_RETRIEVAL,              /**< Retrieval signal strength */
    KNOWLEDGE_DIM_ASSOCIATION,            /**< Association strength */
    KNOWLEDGE_DIM_CATEGORICAL,            /**< Categorical organization */
    KNOWLEDGE_DIM_HIERARCHICAL,           /**< Hierarchical position */
    KNOWLEDGE_DIM_CONFIDENCE,             /**< Confidence in knowledge */
    KNOWLEDGE_DIM_RECENCY,                /**< Temporal recency */
    KNOWLEDGE_DIM_FREQUENCY,              /**< Access frequency */
    KNOWLEDGE_DIM_INTEGRATION,            /**< Cross-domain integration */
    KNOWLEDGE_DIM_COUNT
} knowledge_snn_dimension_t;

/**
 * @brief Encoding methods for knowledge contexts
 */
typedef enum {
    KNOWLEDGE_SNN_ENCODE_RATE = 0,       /**< Rate coding of dimensions */
    KNOWLEDGE_SNN_ENCODE_TEMPORAL,        /**< Temporal spike patterns */
    KNOWLEDGE_SNN_ENCODE_POPULATION,      /**< Population vector coding */
    KNOWLEDGE_SNN_ENCODE_SYNCHRONY        /**< Synchrony-based encoding */
} knowledge_snn_encoding_t;

/**
 * @brief Decoding methods for knowledge states
 */
typedef enum {
    KNOWLEDGE_SNN_DECODE_THRESHOLD = 0,  /**< Threshold-based detection */
    KNOWLEDGE_SNN_DECODE_COMPETITION,     /**< Winner-take-all */
    KNOWLEDGE_SNN_DECODE_SOFTMAX,         /**< Soft probabilistic */
    KNOWLEDGE_SNN_DECODE_INTEGRATION      /**< Evidence accumulation */
} knowledge_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    KNOWLEDGE_SNN_STATE_IDLE = 0,
    KNOWLEDGE_SNN_STATE_ENCODING,
    KNOWLEDGE_SNN_STATE_PROCESSING,
    KNOWLEDGE_SNN_STATE_DECODING,
    KNOWLEDGE_SNN_STATE_SIMULATING,
    KNOWLEDGE_SNN_STATE_ERROR
} knowledge_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Knowledge-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of knowledge dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    knowledge_snn_encoding_t encoding;   /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    knowledge_snn_decoding_t decoding;   /**< Decoding method */
    float activation_threshold;          /**< Threshold for concept activation */
    float retrieval_threshold;           /**< Minimum retrieval strength */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_spreading_activation;    /**< Enable spreading activation */
    float spreading_decay;               /**< Spreading activation decay rate */

    /* Knowledge integration */
    bool enable_retrieval;               /**< Enable retrieval circuits */
    float retrieval_gain;                /**< Retrieval signal gain */
    bool enable_association;             /**< Enable association tracking */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} knowledge_snn_config_t;

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
} knowledge_dim_state_t;

/**
 * @brief Knowledge retrieval output
 */
typedef struct {
    float semantic_level;                /**< Current semantic encoding [0-1] */
    float activation_level;              /**< Current activation [0-1] */
    float retrieval_strength;            /**< Retrieval strength [0-1] */
    float association_strength;          /**< Association strength */
    float categorical_coherence;         /**< Categorical coherence */
    bool concept_activated;              /**< Concept activated */
    bool retrieval_success;              /**< Successful retrieval state */
    float retrieval_magnitude;           /**< Retrieval magnitude if success */
    float integration_level;             /**< Cross-domain integration level */
    float confidence_level;              /**< Knowledge confidence signal */
} knowledge_retrieval_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    knowledge_snn_state_t state;         /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_activation;               /**< Mean concept activation */
    float semantic_signal;               /**< Current semantic signal */
    float retrieval_signal;              /**< Current retrieval signal */
} knowledge_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t concept_activations;        /**< Concept activations */
    uint64_t retrieval_successes;        /**< Successful retrievals */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_activation;               /**< Mean activation level */
} knowledge_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct knowledge_snn_bridge knowledge_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Concept activation callback */
typedef void (*knowledge_snn_activation_callback_t)(
    knowledge_snn_bridge_t* bridge,
    float activation_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Retrieval ready callback */
typedef void (*knowledge_snn_retrieval_callback_t)(
    knowledge_snn_bridge_t* bridge,
    const knowledge_retrieval_t* retrieval,
    void* user_data
);

/** @brief High association callback */
typedef void (*knowledge_snn_association_callback_t)(
    knowledge_snn_bridge_t* bridge,
    float association_level,
    uint32_t association_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
knowledge_snn_config_t knowledge_snn_config_default(void);

/**
 * @brief Create knowledge SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
knowledge_snn_bridge_t* knowledge_snn_create(const knowledge_snn_config_t* config);

/**
 * @brief Destroy knowledge SNN bridge
 * @param bridge Bridge to destroy
 */
void knowledge_snn_destroy(knowledge_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int knowledge_snn_reset(knowledge_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode knowledge state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int knowledge_snn_encode_state(
    knowledge_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode semantic knowledge level
 * @param bridge Bridge handle
 * @param semantic Semantic encoding level [0-1]
 * @param activation Activation level [0-1]
 * @return Spike count on success, -1 on failure
 */
int knowledge_snn_encode_semantic(
    knowledge_snn_bridge_t* bridge,
    float semantic,
    float activation
);

/**
 * @brief Encode concept retrieval cue
 * @param bridge Bridge handle
 * @param retrieval_strength Retrieval strength [0-1]
 * @param concept_count Number of active concepts
 * @return Spike count on success, -1 on failure
 */
int knowledge_snn_encode_retrieval(
    knowledge_snn_bridge_t* bridge,
    float retrieval_strength,
    uint32_t concept_count
);

/**
 * @brief Encode association strength
 * @param bridge Bridge handle
 * @param association Association strength [0-1]
 * @param association_type Association type classification
 * @return Spike count on success, -1 on failure
 */
int knowledge_snn_encode_association(
    knowledge_snn_bridge_t* bridge,
    float association,
    uint32_t association_type
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate knowledge processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int knowledge_snn_simulate(knowledge_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int knowledge_snn_step(knowledge_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int knowledge_snn_forward(
    knowledge_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get knowledge retrieval from SNN activity
 * @param bridge Bridge handle
 * @param retrieval Output retrieval structure
 * @return 0 on success, -1 on failure
 */
int knowledge_snn_get_retrieval(
    knowledge_snn_bridge_t* bridge,
    knowledge_retrieval_t* retrieval
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int knowledge_snn_get_activations(
    knowledge_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for concept activation
 * @param bridge Bridge handle
 * @param activation_level Output activation level
 * @return true if concept activated, false otherwise
 */
bool knowledge_snn_check_activation(
    knowledge_snn_bridge_t* bridge,
    float* activation_level
);

/**
 * @brief Check for retrieval success
 * @param bridge Bridge handle
 * @param retrieval_level Output retrieval level
 * @return true if retrieval successful, false otherwise
 */
bool knowledge_snn_check_retrieval(
    knowledge_snn_bridge_t* bridge,
    float* retrieval_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool knowledge_snn_check_state_change(
    knowledge_snn_bridge_t* bridge,
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
int knowledge_snn_get_dim_state(
    knowledge_snn_bridge_t* bridge,
    uint32_t dim,
    knowledge_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int knowledge_snn_get_state(
    knowledge_snn_bridge_t* bridge,
    knowledge_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int knowledge_snn_get_stats(knowledge_snn_bridge_t* bridge, knowledge_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int knowledge_snn_reset_stats(knowledge_snn_bridge_t* bridge);

/**
 * @brief Get current activation level
 * @param bridge Bridge handle
 * @return Activation level [0-1], -1 on error
 */
float knowledge_snn_get_activation(knowledge_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float knowledge_snn_get_total_activity(knowledge_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register concept activation callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int knowledge_snn_register_activation_callback(
    knowledge_snn_bridge_t* bridge,
    knowledge_snn_activation_callback_t callback,
    void* user_data
);

/**
 * @brief Register retrieval callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int knowledge_snn_register_retrieval_callback(
    knowledge_snn_bridge_t* bridge,
    knowledge_snn_retrieval_callback_t callback,
    void* user_data
);

/**
 * @brief Register high association callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int knowledge_snn_register_association_callback(
    knowledge_snn_bridge_t* bridge,
    knowledge_snn_association_callback_t callback,
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
int knowledge_snn_bio_async_connect(knowledge_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int knowledge_snn_bio_async_disconnect(knowledge_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool knowledge_snn_is_bio_async_connected(knowledge_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KNOWLEDGE_SNN_BRIDGE_H */
