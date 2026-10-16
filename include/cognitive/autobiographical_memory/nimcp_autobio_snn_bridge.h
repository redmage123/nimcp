/**
 * @file nimcp_autobio_snn_bridge.h
 * @brief Autobiographical Memory - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between autobiographical memory and spiking neural networks
 * WHY:  Enable biologically-plausible episodic memory encoding/retrieval through
 *       population coding and spike-timing dynamics
 * HOW:  Encode memory dimensions as spike patterns, decode recall
 *       signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Tulving (1972): Episodic memory as distinct from semantic memory
 * - Conway (2005): Self-Memory System (SMS) model
 * - Moscovitch (2005): Multiple trace theory of memory consolidation
 *
 * BIOLOGICAL BASIS:
 * - Hippocampus for episodic memory encoding and retrieval
 * - Prefrontal cortex for autobiographical retrieval organization
 * - Medial temporal lobe for temporal context encoding
 * - Amygdala for emotional memory consolidation
 * - Default Mode Network for self-referential processing
 *
 * INTEGRATION WITH LEARNING:
 * - Memory trace encoding through population activity patterns
 * - Temporal context via spike timing correlations
 * - Emotional consolidation through modulated plasticity
 *
 * @see nimcp_autobiographical_memory.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_AUTOBIO_SNN_BRIDGE_H
#define NIMCP_AUTOBIO_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum memory dimensions to encode */
#define AUTOBIO_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per memory dimension */
#define AUTOBIO_SNN_NEURONS_PER_DIM    32

/** @brief Default recall threshold */
#define AUTOBIO_SNN_RECALL_THRESH      0.5f

/** @brief Default encoding window (ms) */
#define AUTOBIO_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_AUTOBIO_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Autobiographical memory dimension types for SNN encoding
 */
typedef enum {
    AUTOBIO_DIM_TEMPORAL_CONTEXT = 0,    /**< When did this happen (recency) */
    AUTOBIO_DIM_EMOTIONAL_VALENCE,        /**< Positive/negative emotional tone */
    AUTOBIO_DIM_EMOTIONAL_INTENSITY,      /**< How intense was the emotion */
    AUTOBIO_DIM_SELF_RELEVANCE,           /**< How much about "me" */
    AUTOBIO_DIM_IMPORTANCE,               /**< How significant to identity */
    AUTOBIO_DIM_VIVIDNESS,                /**< Memory clarity/strength */
    AUTOBIO_DIM_CERTAINTY,                /**< Confidence in memory accuracy */
    AUTOBIO_DIM_AROUSAL,                  /**< Calm vs excited state */
    AUTOBIO_DIM_ENCODING_DEPTH,           /**< How deeply processed */
    AUTOBIO_DIM_RETRIEVAL_STRENGTH,       /**< How easily recalled */
    AUTOBIO_DIM_COUNT
} autobio_snn_dimension_t;

/**
 * @brief Encoding methods for memory contexts
 */
typedef enum {
    AUTOBIO_SNN_ENCODE_RATE = 0,         /**< Rate coding of dimensions */
    AUTOBIO_SNN_ENCODE_TEMPORAL,          /**< Temporal spike patterns */
    AUTOBIO_SNN_ENCODE_POPULATION,        /**< Population vector coding */
    AUTOBIO_SNN_ENCODE_SYNCHRONY          /**< Synchrony-based encoding */
} autobio_snn_encoding_t;

/**
 * @brief Decoding methods for recall states
 */
typedef enum {
    AUTOBIO_SNN_DECODE_THRESHOLD = 0,    /**< Threshold-based detection */
    AUTOBIO_SNN_DECODE_COMPETITION,       /**< Winner-take-all */
    AUTOBIO_SNN_DECODE_SOFTMAX,           /**< Soft probabilistic */
    AUTOBIO_SNN_DECODE_INTEGRATION        /**< Evidence accumulation */
} autobio_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    AUTOBIO_SNN_STATE_IDLE = 0,
    AUTOBIO_SNN_STATE_ENCODING,
    AUTOBIO_SNN_STATE_PROCESSING,
    AUTOBIO_SNN_STATE_DECODING,
    AUTOBIO_SNN_STATE_SIMULATING,
    AUTOBIO_SNN_STATE_ERROR
} autobio_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Autobiographical Memory-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of memory dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    autobio_snn_encoding_t encoding;     /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    autobio_snn_decoding_t decoding;     /**< Decoding method */
    float recall_threshold;              /**< Threshold for successful recall */
    float vividness_threshold;           /**< Minimum memory vividness */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_temporal_context;        /**< Enable temporal context encoding */
    float temporal_sensitivity;          /**< Temporal encoding sensitivity */

    /* Memory integration */
    bool enable_emotional_modulation;    /**< Enable emotional modulation */
    float emotional_gain;                /**< Emotional signal gain */
    bool enable_consolidation;           /**< Enable memory consolidation */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} autobio_snn_config_t;

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
} autobio_dim_state_t;

/**
 * @brief Memory recall output
 */
typedef struct {
    float temporal_context;              /**< Temporal context strength [0-1] */
    float emotional_valence;             /**< Emotional valence [-1, 1] */
    float emotional_intensity;           /**< Emotional intensity [0-1] */
    float self_relevance;                /**< Self-relevance [0-1] */
    float vividness;                     /**< Memory vividness [0-1] */
    float recall_confidence;             /**< Recall confidence [0-1] */
    bool recall_successful;              /**< Was recall successful */
    bool emotional_memory;               /**< Is this emotionally significant */
    float importance_level;              /**< Importance level if recalled */
    float encoding_depth;                /**< Encoding depth signal */
} autobio_recall_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    autobio_snn_state_t state;           /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_recall_strength;          /**< Mean recall strength */
    float temporal_signal;               /**< Current temporal signal */
    float emotional_signal;              /**< Current emotional signal */
} autobio_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_encodings;            /**< Total encodings performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t successful_recalls;         /**< Successful recall events */
    uint64_t emotional_memories;         /**< Emotional memory encodings */
    uint64_t state_changes;              /**< State changes detected */
    float mean_encoding_time_ms;         /**< Mean encoding latency */
    float mean_recall_strength;          /**< Mean recall strength */
} autobio_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct autobio_snn_bridge autobio_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Recall detection callback */
typedef void (*autobio_snn_recall_callback_t)(
    autobio_snn_bridge_t* bridge,
    float recall_strength,
    uint64_t latency_us,
    void* user_data
);

/** @brief Memory encoded callback */
typedef void (*autobio_snn_encoded_callback_t)(
    autobio_snn_bridge_t* bridge,
    const autobio_recall_t* recall,
    void* user_data
);

/** @brief Emotional memory callback */
typedef void (*autobio_snn_emotional_callback_t)(
    autobio_snn_bridge_t* bridge,
    float emotional_intensity,
    float valence,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
autobio_snn_config_t autobio_snn_config_default(void);

/**
 * @brief Create autobiographical memory SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
autobio_snn_bridge_t* autobio_snn_create(const autobio_snn_config_t* config);

/**
 * @brief Destroy autobiographical memory SNN bridge
 * @param bridge Bridge to destroy
 */
void autobio_snn_destroy(autobio_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int autobio_snn_reset(autobio_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode memory state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int autobio_snn_encode_state(
    autobio_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode episodic memory trace
 * @param bridge Bridge handle
 * @param importance Memory importance [0-1]
 * @param self_relevance Self-relevance [0-1]
 * @param vividness Memory vividness [0-1]
 * @return Spike count on success, -1 on failure
 */
int autobio_snn_encode_episodic(
    autobio_snn_bridge_t* bridge,
    float importance,
    float self_relevance,
    float vividness
);

/**
 * @brief Encode temporal context
 * @param bridge Bridge handle
 * @param recency How recent (0=distant, 1=now)
 * @param temporal_tag Temporal ordering tag
 * @return Spike count on success, -1 on failure
 */
int autobio_snn_encode_temporal(
    autobio_snn_bridge_t* bridge,
    float recency,
    uint64_t temporal_tag
);

/**
 * @brief Encode emotional memory consolidation
 * @param bridge Bridge handle
 * @param valence Emotional valence [-1, 1]
 * @param intensity Emotional intensity [0-1]
 * @param arousal Arousal level [0-1]
 * @return Spike count on success, -1 on failure
 */
int autobio_snn_encode_emotional(
    autobio_snn_bridge_t* bridge,
    float valence,
    float intensity,
    float arousal
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate memory processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int autobio_snn_simulate(autobio_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int autobio_snn_step(autobio_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int autobio_snn_forward(
    autobio_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get recall state from SNN activity
 * @param bridge Bridge handle
 * @param recall Output recall structure
 * @return 0 on success, -1 on failure
 */
int autobio_snn_get_recall(
    autobio_snn_bridge_t* bridge,
    autobio_recall_t* recall
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int autobio_snn_get_activations(
    autobio_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for successful recall
 * @param bridge Bridge handle
 * @param recall_strength Output recall strength
 * @return true if recall successful, false otherwise
 */
bool autobio_snn_check_recall(
    autobio_snn_bridge_t* bridge,
    float* recall_strength
);

/**
 * @brief Check for emotional memory
 * @param bridge Bridge handle
 * @param emotional_intensity Output intensity level
 * @return true if emotional memory detected, false otherwise
 */
bool autobio_snn_check_emotional(
    autobio_snn_bridge_t* bridge,
    float* emotional_intensity
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool autobio_snn_check_state_change(
    autobio_snn_bridge_t* bridge,
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
int autobio_snn_get_dim_state(
    autobio_snn_bridge_t* bridge,
    uint32_t dim,
    autobio_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int autobio_snn_get_state(
    autobio_snn_bridge_t* bridge,
    autobio_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int autobio_snn_get_stats(autobio_snn_bridge_t* bridge, autobio_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int autobio_snn_reset_stats(autobio_snn_bridge_t* bridge);

/**
 * @brief Get current recall strength level
 * @param bridge Bridge handle
 * @return Recall strength [0-1], -1 on error
 */
float autobio_snn_get_recall_strength(autobio_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float autobio_snn_get_total_activity(autobio_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register recall detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int autobio_snn_register_recall_callback(
    autobio_snn_bridge_t* bridge,
    autobio_snn_recall_callback_t callback,
    void* user_data
);

/**
 * @brief Register encoded callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int autobio_snn_register_encoded_callback(
    autobio_snn_bridge_t* bridge,
    autobio_snn_encoded_callback_t callback,
    void* user_data
);

/**
 * @brief Register emotional memory callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int autobio_snn_register_emotional_callback(
    autobio_snn_bridge_t* bridge,
    autobio_snn_emotional_callback_t callback,
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
int autobio_snn_bio_async_connect(autobio_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int autobio_snn_bio_async_disconnect(autobio_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool autobio_snn_is_bio_async_connected(autobio_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUTOBIO_SNN_BRIDGE_H */
