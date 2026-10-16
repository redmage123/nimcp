/**
 * @file nimcp_self_awareness_snn_bridge.h
 * @brief Self-Awareness - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between self-awareness engine and spiking neural networks
 * WHY:  Enable biologically-plausible self-awareness through population coding
 *       and spike-timing dynamics for self-recognition and agency
 * HOW:  Encode self-awareness dimensions as spike patterns, decode metacognitive
 *       signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Dehaene (2014): Consciousness and the brain
 * - Damasio (1999): The feeling of what happens - somatic markers
 * - Gallagher (2000): Philosophical conceptions of the self
 * - Metzinger (2003): Being No One - self-model theory
 *
 * BIOLOGICAL BASIS:
 * - Medial prefrontal cortex for self-referential processing
 * - Insula for interoceptive awareness and body ownership
 * - Temporoparietal junction for agency attribution
 * - Anterior cingulate cortex for metacognitive monitoring
 * - Posterior cingulate cortex for self-reflection
 *
 * INTEGRATION WITH LEARNING:
 * - Self-recognition via consistent neural patterns
 * - Body ownership through proprioceptive integration
 * - Agency detection via prediction-outcome matching
 * - Metacognitive state through sustained activity patterns
 *
 * @see nimcp_self_awareness_extended.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_SELF_AWARENESS_SNN_BRIDGE_H
#define NIMCP_SELF_AWARENESS_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum self-awareness dimensions to encode */
#define SELF_AWARENESS_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per self-awareness dimension */
#define SELF_AWARENESS_SNN_NEURONS_PER_DIM    32

/** @brief Default self-recognition threshold */
#define SELF_AWARENESS_SNN_RECOGNITION_THRESH 0.5f

/** @brief Default encoding window (ms) */
#define SELF_AWARENESS_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_SELF_AWARENESS_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Self-awareness dimension types for SNN encoding
 */
typedef enum {
    SELF_DIM_SELF_RECOGNITION = 0,       /**< Self-recognition level */
    SELF_DIM_BODY_OWNERSHIP,             /**< Body ownership sense */
    SELF_DIM_AGENCY_SENSE,               /**< Agency attribution strength */
    SELF_DIM_METACOGNITIVE,              /**< Metacognitive state level */
    SELF_DIM_SELF_REFLECTION,            /**< Self-reflection depth */
    SELF_DIM_TEMPORAL_CONTINUITY,        /**< Temporal self-continuity */
    SELF_DIM_SELF_BOUNDARY,              /**< Self-other boundary clarity */
    SELF_DIM_INTEROCEPTION,              /**< Internal state awareness */
    SELF_DIM_SELF_NARRATIVE,             /**< Narrative coherence */
    SELF_DIM_SELF_EFFICACY,              /**< Self-efficacy level */
    SELF_DIM_COUNT
} self_awareness_snn_dimension_t;

/**
 * @brief Encoding methods for self-awareness contexts
 */
typedef enum {
    SELF_SNN_ENCODE_RATE = 0,            /**< Rate coding of dimensions */
    SELF_SNN_ENCODE_TEMPORAL,             /**< Temporal spike patterns */
    SELF_SNN_ENCODE_POPULATION,           /**< Population vector coding */
    SELF_SNN_ENCODE_SYNCHRONY             /**< Synchrony-based encoding */
} self_awareness_snn_encoding_t;

/**
 * @brief Decoding methods for self-awareness states
 */
typedef enum {
    SELF_SNN_DECODE_THRESHOLD = 0,       /**< Threshold-based detection */
    SELF_SNN_DECODE_COMPETITION,          /**< Winner-take-all */
    SELF_SNN_DECODE_SOFTMAX,              /**< Soft probabilistic */
    SELF_SNN_DECODE_INTEGRATION           /**< Evidence accumulation */
} self_awareness_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SELF_SNN_STATE_IDLE = 0,
    SELF_SNN_STATE_ENCODING,
    SELF_SNN_STATE_PROCESSING,
    SELF_SNN_STATE_DECODING,
    SELF_SNN_STATE_SIMULATING,
    SELF_SNN_STATE_ERROR
} self_awareness_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Self-awareness-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of self-awareness dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    self_awareness_snn_encoding_t encoding; /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    self_awareness_snn_decoding_t decoding; /**< Decoding method */
    float recognition_threshold;         /**< Threshold for self-recognition */
    float agency_threshold;              /**< Minimum agency sense */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_self_recognition;        /**< Enable self-recognition detection */
    float recognition_sensitivity;       /**< Self-recognition sensitivity */

    /* Body ownership integration */
    bool enable_body_ownership;          /**< Enable body ownership circuits */
    float body_ownership_gain;           /**< Body ownership signal gain */
    bool enable_interoception;           /**< Enable interoceptive awareness */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} self_awareness_snn_config_t;

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
} self_awareness_dim_state_t;

/**
 * @brief Self-awareness state output
 */
typedef struct {
    float self_recognition;              /**< Self-recognition level [0-1] */
    float body_ownership;                /**< Body ownership sense [0-1] */
    float agency_sense;                  /**< Agency sense strength [0-1] */
    float metacognitive_level;           /**< Metacognitive state level */
    float self_reflection;               /**< Self-reflection depth */
    bool self_recognized;                /**< Self recognized */
    bool agency_detected;                /**< Agency detected */
    float recognition_magnitude;         /**< Recognition magnitude if detected */
    float temporal_continuity;           /**< Temporal continuity sense */
    float self_boundary_clarity;         /**< Self-boundary clarity */
} self_awareness_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    self_awareness_snn_state_t state;    /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_self_awareness;           /**< Mean self-awareness level */
    float recognition_signal;            /**< Current recognition signal */
    float agency_signal;                 /**< Current agency signal */
} self_awareness_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t recognition_events;         /**< Self-recognition events */
    uint64_t agency_events;              /**< Agency detection events */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_self_awareness;           /**< Mean self-awareness level */
} self_awareness_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct self_awareness_snn_bridge self_awareness_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Self-recognition detection callback */
typedef void (*self_awareness_snn_recognition_callback_t)(
    self_awareness_snn_bridge_t* bridge,
    float recognition_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Awareness state ready callback */
typedef void (*self_awareness_snn_state_callback_t)(
    self_awareness_snn_bridge_t* bridge,
    const self_awareness_state_t* awareness_state,
    void* user_data
);

/** @brief Agency detection callback */
typedef void (*self_awareness_snn_agency_callback_t)(
    self_awareness_snn_bridge_t* bridge,
    float agency_level,
    uint32_t agency_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
self_awareness_snn_config_t self_awareness_snn_config_default(void);

/**
 * @brief Create self-awareness SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
self_awareness_snn_bridge_t* self_awareness_snn_create(const self_awareness_snn_config_t* config);

/**
 * @brief Destroy self-awareness SNN bridge
 * @param bridge Bridge to destroy
 */
void self_awareness_snn_destroy(self_awareness_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_awareness_snn_reset(self_awareness_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode self-awareness state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int self_awareness_snn_encode_state(
    self_awareness_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode self-recognition level
 * @param bridge Bridge handle
 * @param recognition Self-recognition level [0-1]
 * @param body_ownership Body ownership sense [0-1]
 * @return Spike count on success, -1 on failure
 */
int self_awareness_snn_encode_self_recognition(
    self_awareness_snn_bridge_t* bridge,
    float recognition,
    float body_ownership
);

/**
 * @brief Encode agency sense
 * @param bridge Bridge handle
 * @param agency_level Agency level [0-1]
 * @param agency_type Type of agency (0=self, 1=external, 2=joint)
 * @return Spike count on success, -1 on failure
 */
int self_awareness_snn_encode_agency(
    self_awareness_snn_bridge_t* bridge,
    float agency_level,
    uint32_t agency_type
);

/**
 * @brief Encode metacognitive state
 * @param bridge Bridge handle
 * @param metacog_level Metacognitive level [0-1]
 * @param reflection_depth Reflection depth [0-1]
 * @return Spike count on success, -1 on failure
 */
int self_awareness_snn_encode_metacognitive(
    self_awareness_snn_bridge_t* bridge,
    float metacog_level,
    float reflection_depth
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate self-awareness processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int self_awareness_snn_simulate(self_awareness_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_awareness_snn_step(self_awareness_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int self_awareness_snn_forward(
    self_awareness_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get self-awareness state from SNN activity
 * @param bridge Bridge handle
 * @param awareness_state Output awareness state structure
 * @return 0 on success, -1 on failure
 */
int self_awareness_snn_get_awareness_state(
    self_awareness_snn_bridge_t* bridge,
    self_awareness_state_t* awareness_state
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int self_awareness_snn_get_activations(
    self_awareness_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for self-recognition
 * @param bridge Bridge handle
 * @param recognition_level Output recognition level
 * @return true if self recognized, false otherwise
 */
bool self_awareness_snn_check_recognition(
    self_awareness_snn_bridge_t* bridge,
    float* recognition_level
);

/**
 * @brief Check for agency
 * @param bridge Bridge handle
 * @param agency_level Output agency level
 * @return true if agency detected, false otherwise
 */
bool self_awareness_snn_check_agency(
    self_awareness_snn_bridge_t* bridge,
    float* agency_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool self_awareness_snn_check_state_change(
    self_awareness_snn_bridge_t* bridge,
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
int self_awareness_snn_get_dim_state(
    self_awareness_snn_bridge_t* bridge,
    uint32_t dim,
    self_awareness_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int self_awareness_snn_get_state(
    self_awareness_snn_bridge_t* bridge,
    self_awareness_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int self_awareness_snn_get_stats(self_awareness_snn_bridge_t* bridge, self_awareness_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_awareness_snn_reset_stats(self_awareness_snn_bridge_t* bridge);

/**
 * @brief Get current self-awareness level
 * @param bridge Bridge handle
 * @return Self-awareness level [0-1], -1 on error
 */
float self_awareness_snn_get_awareness_level(self_awareness_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float self_awareness_snn_get_total_activity(self_awareness_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register self-recognition detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int self_awareness_snn_register_recognition_callback(
    self_awareness_snn_bridge_t* bridge,
    self_awareness_snn_recognition_callback_t callback,
    void* user_data
);

/**
 * @brief Register awareness state callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int self_awareness_snn_register_state_callback(
    self_awareness_snn_bridge_t* bridge,
    self_awareness_snn_state_callback_t callback,
    void* user_data
);

/**
 * @brief Register agency detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int self_awareness_snn_register_agency_callback(
    self_awareness_snn_bridge_t* bridge,
    self_awareness_snn_agency_callback_t callback,
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
int self_awareness_snn_bio_async_connect(self_awareness_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_awareness_snn_bio_async_disconnect(self_awareness_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool self_awareness_snn_is_bio_async_connected(self_awareness_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_AWARENESS_SNN_BRIDGE_H */
