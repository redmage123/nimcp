/**
 * @file nimcp_self_model_snn_bridge.h
 * @brief Self Model - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between self model engine and spiking neural networks
 * WHY:  Enable biologically-plausible self-representation through
 *       population coding and spike-timing dynamics
 * HOW:  Encode self-awareness states as spike patterns, decode self-model
 *       insights from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Damasio (1999): Core and autobiographical self representation
 * - Gallagher (2000): Sense of agency and ownership
 * - Northoff (2011): Self-specificity and neural coding
 *
 * BIOLOGICAL BASIS:
 * - Medial prefrontal cortex (mPFC) for self-referential processing
 * - Temporoparietal junction (TPJ) for self-other distinction
 * - Insula for interoceptive self-awareness
 * - Default mode network (DMN) for autobiographical self
 *
 * INTEGRATION WITH CONSCIOUSNESS:
 * - Body state encoding through interoceptive signals
 * - Agency estimation through efference copy comparison
 * - Self-continuity through temporal integration
 *
 * @see nimcp_self_model.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_SELF_MODEL_SNN_BRIDGE_H
#define NIMCP_SELF_MODEL_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum self model dimensions to encode */
#define SELF_MODEL_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per self model dimension */
#define SELF_MODEL_SNN_NEURONS_PER_DIM    32

/** @brief Default boundary threshold */
#define SELF_MODEL_SNN_BOUNDARY_THRESH    0.5f

/** @brief Default encoding window (ms) */
#define SELF_MODEL_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_SELF_MODEL_SNN         0x0D50

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Self model dimension types for SNN encoding
 */
typedef enum {
    SELF_DIM_BODY_STATE = 0,     /**< Physical/bodily awareness */
    SELF_DIM_AGENCY,              /**< Sense of agency/control */
    SELF_DIM_OWNERSHIP,           /**< Body ownership */
    SELF_DIM_AUTOBIOGRAPHICAL,    /**< Autobiographical memory access */
    SELF_DIM_NARRATIVE,           /**< Self-narrative coherence */
    SELF_DIM_IDENTITY,            /**< Core identity representation */
    SELF_DIM_CAPABILITY,          /**< Self-capability assessment */
    SELF_DIM_BOUNDARY,            /**< Self-other boundary */
    SELF_DIM_CONTINUITY,          /**< Temporal self-continuity */
    SELF_DIM_REFLECTION,          /**< Self-reflection depth */
    SELF_DIM_COUNT
} self_model_snn_dimension_t;

/**
 * @brief Encoding methods for self model contexts
 */
typedef enum {
    SELF_MODEL_SNN_ENCODE_RATE = 0,   /**< Rate coding of dimensions */
    SELF_MODEL_SNN_ENCODE_TEMPORAL,    /**< Temporal spike patterns */
    SELF_MODEL_SNN_ENCODE_POPULATION,  /**< Population vector coding */
    SELF_MODEL_SNN_ENCODE_SYNCHRONY    /**< Synchrony-based encoding */
} self_model_snn_encoding_t;

/**
 * @brief Decoding methods for self model states
 */
typedef enum {
    SELF_MODEL_SNN_DECODE_THRESHOLD = 0, /**< Threshold-based detection */
    SELF_MODEL_SNN_DECODE_COMPETITION,    /**< Winner-take-all */
    SELF_MODEL_SNN_DECODE_SOFTMAX,        /**< Soft probabilistic */
    SELF_MODEL_SNN_DECODE_INTEGRATION     /**< Evidence accumulation */
} self_model_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SELF_MODEL_SNN_STATE_IDLE = 0,
    SELF_MODEL_SNN_STATE_ENCODING,
    SELF_MODEL_SNN_STATE_PROCESSING,
    SELF_MODEL_SNN_STATE_DECODING,
    SELF_MODEL_SNN_STATE_SIMULATING,
    SELF_MODEL_SNN_STATE_ERROR
} self_model_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Self Model-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of self model dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    self_model_snn_encoding_t encoding;  /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    self_model_snn_decoding_t decoding;  /**< Decoding method */
    float boundary_threshold;            /**< Threshold for self-other boundary */
    float agency_threshold;              /**< Minimum agency required */
    float continuity_threshold;          /**< Threshold for continuity detection */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_boundary_detection;      /**< Enable self-other boundary detection */
    float boundary_sensitivity;          /**< Boundary detection sensitivity */

    /* Self model integration */
    bool enable_identity_core;           /**< Enable core identity processing */
    float identity_gain;                 /**< Identity signal gain */
    bool enable_autobiographical;        /**< Enable autobiographical circuits */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} self_model_snn_config_t;

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
} self_model_dim_state_t;

/**
 * @brief Self model insight output
 */
typedef struct {
    float body_state_level;              /**< Current bodily awareness [0-1] */
    float agency_level;                  /**< Current sense of agency [0-1] */
    float ownership_level;               /**< Body ownership strength [0-1] */
    float identity_coherence;            /**< Identity coherence level */
    float boundary_clarity;              /**< Self-other boundary clarity */
    bool boundary_violation_detected;    /**< Boundary violation detected */
    bool agency_disruption_detected;     /**< Agency disruption detected */
    float disruption_magnitude;          /**< Disruption magnitude if detected */
    float narrative_coherence;           /**< Self-narrative coherence level */
    float continuity_score;              /**< Temporal self-continuity (phi-like) */
} self_model_insight_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    self_model_snn_state_t state;        /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_agency;                   /**< Mean agency level */
    float boundary_signal;               /**< Current boundary signal */
    float identity_signal;               /**< Current identity signal */
} self_model_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t boundary_detections;        /**< Boundary violation detections */
    uint64_t agency_disruptions;         /**< Agency disruption detections */
    uint64_t identity_changes;           /**< Identity state changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_agency;                   /**< Mean agency score */
} self_model_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct self_model_snn_bridge self_model_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Boundary violation callback */
typedef void (*self_model_snn_boundary_callback_t)(
    self_model_snn_bridge_t* bridge,
    float boundary_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Insight ready callback */
typedef void (*self_model_snn_insight_callback_t)(
    self_model_snn_bridge_t* bridge,
    const self_model_insight_t* insight,
    void* user_data
);

/** @brief Agency disruption callback */
typedef void (*self_model_snn_agency_callback_t)(
    self_model_snn_bridge_t* bridge,
    float agency_level,
    uint32_t disruption_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
self_model_snn_config_t self_model_snn_config_default(void);

/**
 * @brief Create self model SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
self_model_snn_bridge_t* self_model_snn_create(const self_model_snn_config_t* config);

/**
 * @brief Destroy self model SNN bridge
 * @param bridge Bridge to destroy
 */
void self_model_snn_destroy(self_model_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_model_snn_reset(self_model_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode self model state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int self_model_snn_encode_state(
    self_model_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode body state
 * @param bridge Bridge handle
 * @param interoceptive Interoceptive signal [0-1]
 * @param proprioceptive Proprioceptive signal [0-1]
 * @return Spike count on success, -1 on failure
 */
int self_model_snn_encode_body_state(
    self_model_snn_bridge_t* bridge,
    float interoceptive,
    float proprioceptive
);

/**
 * @brief Encode agency signal
 * @param bridge Bridge handle
 * @param agency_strength Agency strength [0-1]
 * @param efference_match Efference copy match [0-1]
 * @return Spike count on success, -1 on failure
 */
int self_model_snn_encode_agency(
    self_model_snn_bridge_t* bridge,
    float agency_strength,
    float efference_match
);

/**
 * @brief Encode boundary signal
 * @param bridge Bridge handle
 * @param boundary_strength Boundary strength [0-1]
 * @param boundary_type Boundary type classification
 * @return Spike count on success, -1 on failure
 */
int self_model_snn_encode_boundary(
    self_model_snn_bridge_t* bridge,
    float boundary_strength,
    uint32_t boundary_type
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate self model processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int self_model_snn_simulate(self_model_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_model_snn_step(self_model_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int self_model_snn_forward(
    self_model_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get self model insight from SNN activity
 * @param bridge Bridge handle
 * @param insight Output insight structure
 * @return 0 on success, -1 on failure
 */
int self_model_snn_get_insight(
    self_model_snn_bridge_t* bridge,
    self_model_insight_t* insight
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int self_model_snn_get_activations(
    self_model_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for boundary violation
 * @param bridge Bridge handle
 * @param boundary_level Output boundary level
 * @return true if boundary violation detected, false otherwise
 */
bool self_model_snn_check_boundary(
    self_model_snn_bridge_t* bridge,
    float* boundary_level
);

/**
 * @brief Check for agency disruption
 * @param bridge Bridge handle
 * @param agency_level Output agency level
 * @return true if agency disruption detected, false otherwise
 */
bool self_model_snn_check_agency(
    self_model_snn_bridge_t* bridge,
    float* agency_level
);

/**
 * @brief Check for identity change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if identity change detected, false otherwise
 */
bool self_model_snn_check_identity_change(
    self_model_snn_bridge_t* bridge,
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
int self_model_snn_get_dim_state(
    self_model_snn_bridge_t* bridge,
    uint32_t dim,
    self_model_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int self_model_snn_get_state(
    self_model_snn_bridge_t* bridge,
    self_model_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int self_model_snn_get_stats(self_model_snn_bridge_t* bridge, self_model_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_model_snn_reset_stats(self_model_snn_bridge_t* bridge);

/**
 * @brief Get current agency level
 * @param bridge Bridge handle
 * @return Agency level [0-1], -1 on error
 */
float self_model_snn_get_agency(self_model_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float self_model_snn_get_total_activity(self_model_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register boundary violation callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int self_model_snn_register_boundary_callback(
    self_model_snn_bridge_t* bridge,
    self_model_snn_boundary_callback_t callback,
    void* user_data
);

/**
 * @brief Register insight callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int self_model_snn_register_insight_callback(
    self_model_snn_bridge_t* bridge,
    self_model_snn_insight_callback_t callback,
    void* user_data
);

/**
 * @brief Register agency disruption callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int self_model_snn_register_agency_callback(
    self_model_snn_bridge_t* bridge,
    self_model_snn_agency_callback_t callback,
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
int self_model_snn_bio_async_connect(self_model_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_model_snn_bio_async_disconnect(self_model_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool self_model_snn_is_bio_async_connected(self_model_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_MODEL_SNN_BRIDGE_H */
