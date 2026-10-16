/**
 * @file nimcp_parietal_snn_bridge.h
 * @brief Parietal - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between parietal lobe and spiking neural networks
 * WHY:  Enable biologically-plausible spatial, numerical, and visuospatial
 *       processing through population coding and spike-timing dynamics
 * HOW:  Encode parietal dimensions as spike patterns, decode cognitive
 *       signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Dehaene (1997): The number sense - triple-code model
 * - Colby & Goldberg (1999): Space and attention in parietal cortex
 * - Andersen & Buneo (2002): Sensorimotor integration in posterior parietal cortex
 *
 * BIOLOGICAL BASIS:
 * - Intraparietal sulcus (IPS) for numerical magnitude representation
 * - Posterior parietal cortex (PPC) for spatial attention and coordinate transforms
 * - Superior parietal lobule for visuospatial processing
 * - Inferior parietal lobule for body schema and multi-sensory integration
 *
 * INTEGRATION WITH LEARNING:
 * - Spatial attention encoding through population activity
 * - Numerical magnitude via neural number lines
 * - Multi-sensory integration through cross-modal binding
 * - Body schema representation via proprioceptive populations
 *
 * @see nimcp_parietal.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_PARIETAL_SNN_BRIDGE_H
#define NIMCP_PARIETAL_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum parietal dimensions to encode */
#define PARIETAL_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per parietal dimension */
#define PARIETAL_SNN_NEURONS_PER_DIM    32

/** @brief Default spatial attention threshold */
#define PARIETAL_SNN_ATTENTION_THRESH   0.5f

/** @brief Default encoding window (ms) */
#define PARIETAL_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_PARIETAL_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Parietal dimension types for SNN encoding
 */
typedef enum {
    PARIETAL_DIM_SPATIAL_ATTENTION = 0,  /**< Spatial attention focus level */
    PARIETAL_DIM_NUMERICAL_MAGNITUDE,    /**< Numerical magnitude representation */
    PARIETAL_DIM_MULTISENSORY,           /**< Multi-sensory integration strength */
    PARIETAL_DIM_BODY_SCHEMA,            /**< Body schema representation */
    PARIETAL_DIM_VISUOSPATIAL,           /**< Visuospatial processing level */
    PARIETAL_DIM_COORDINATE_TRANSFORM,   /**< Coordinate transformation state */
    PARIETAL_DIM_MENTAL_ROTATION,        /**< Mental rotation activity */
    PARIETAL_DIM_PATTERN_DETECTION,      /**< Mathematical pattern detection */
    PARIETAL_DIM_PRECISION,              /**< Numerical precision level */
    PARIETAL_DIM_INTEGRATION,            /**< Cross-modal integration */
    PARIETAL_DIM_COUNT
} parietal_snn_dimension_t;

/**
 * @brief Encoding methods for parietal contexts
 */
typedef enum {
    PARIETAL_SNN_ENCODE_RATE = 0,        /**< Rate coding of dimensions */
    PARIETAL_SNN_ENCODE_TEMPORAL,         /**< Temporal spike patterns */
    PARIETAL_SNN_ENCODE_POPULATION,       /**< Population vector coding */
    PARIETAL_SNN_ENCODE_SYNCHRONY         /**< Synchrony-based encoding */
} parietal_snn_encoding_t;

/**
 * @brief Decoding methods for spatial states
 */
typedef enum {
    PARIETAL_SNN_DECODE_THRESHOLD = 0,   /**< Threshold-based detection */
    PARIETAL_SNN_DECODE_COMPETITION,      /**< Winner-take-all */
    PARIETAL_SNN_DECODE_SOFTMAX,          /**< Soft probabilistic */
    PARIETAL_SNN_DECODE_INTEGRATION       /**< Evidence accumulation */
} parietal_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    PARIETAL_SNN_STATE_IDLE = 0,
    PARIETAL_SNN_STATE_ENCODING,
    PARIETAL_SNN_STATE_PROCESSING,
    PARIETAL_SNN_STATE_DECODING,
    PARIETAL_SNN_STATE_SIMULATING,
    PARIETAL_SNN_STATE_ERROR
} parietal_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Parietal-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of parietal dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    parietal_snn_encoding_t encoding;    /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    parietal_snn_decoding_t decoding;    /**< Decoding method */
    float attention_threshold;           /**< Threshold for spatial attention */
    float magnitude_threshold;           /**< Minimum numerical magnitude */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_attention_detection;     /**< Enable spatial attention detection */
    float attention_sensitivity;         /**< Attention detection sensitivity */

    /* Spatial processing integration */
    bool enable_spatial_processing;      /**< Enable spatial processing circuits */
    float spatial_gain;                  /**< Spatial signal gain */
    bool enable_numerical_processing;    /**< Enable numerical processing */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} parietal_snn_config_t;

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
} parietal_dim_state_t;

/**
 * @brief Spatial processing output
 */
typedef struct {
    float attention_level;               /**< Current spatial attention [0-1] */
    float numerical_magnitude;           /**< Numerical magnitude representation [0-1] */
    float multisensory_integration;      /**< Multi-sensory integration [0-1] */
    float body_schema_strength;          /**< Body schema strength */
    float visuospatial_activity;         /**< Visuospatial processing level */
    bool attention_detected;             /**< Spatial attention detected */
    bool high_precision;                 /**< High precision state */
    float precision_magnitude;           /**< Precision magnitude if detected */
    float coordinate_transform_level;    /**< Coordinate transformation level */
    float mental_rotation_activity;      /**< Mental rotation activity level */
} parietal_spatial_output_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    parietal_snn_state_t state;          /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_spatial;                  /**< Mean spatial processing level */
    float attention_signal;              /**< Current attention signal */
    float numerical_signal;              /**< Current numerical signal */
} parietal_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t attention_detections;       /**< Spatial attention detections */
    uint64_t high_precision_events;      /**< High precision events */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_spatial_activity;         /**< Mean spatial processing activity */
} parietal_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct parietal_snn_bridge parietal_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Spatial attention detection callback */
typedef void (*parietal_snn_attention_callback_t)(
    parietal_snn_bridge_t* bridge,
    float attention_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Spatial output ready callback */
typedef void (*parietal_snn_spatial_callback_t)(
    parietal_snn_bridge_t* bridge,
    const parietal_spatial_output_t* spatial,
    void* user_data
);

/** @brief High precision callback */
typedef void (*parietal_snn_precision_callback_t)(
    parietal_snn_bridge_t* bridge,
    float precision_level,
    uint32_t precision_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
parietal_snn_config_t parietal_snn_config_default(void);

/**
 * @brief Create parietal SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
parietal_snn_bridge_t* parietal_snn_create(const parietal_snn_config_t* config);

/**
 * @brief Destroy parietal SNN bridge
 * @param bridge Bridge to destroy
 */
void parietal_snn_destroy(parietal_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int parietal_snn_reset(parietal_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode parietal state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int parietal_snn_encode_state(
    parietal_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode spatial attention
 * @param bridge Bridge handle
 * @param attention Attention level [0-1]
 * @param focus Focus intensity [0-1]
 * @return Spike count on success, -1 on failure
 */
int parietal_snn_encode_attention(
    parietal_snn_bridge_t* bridge,
    float attention,
    float focus
);

/**
 * @brief Encode numerical magnitude
 * @param bridge Bridge handle
 * @param magnitude Numerical magnitude [0-1]
 * @param precision Precision level
 * @return Spike count on success, -1 on failure
 */
int parietal_snn_encode_magnitude(
    parietal_snn_bridge_t* bridge,
    float magnitude,
    uint32_t precision
);

/**
 * @brief Encode multi-sensory integration
 * @param bridge Bridge handle
 * @param integration Integration level [0-1]
 * @param modality_count Number of active modalities
 * @return Spike count on success, -1 on failure
 */
int parietal_snn_encode_multisensory(
    parietal_snn_bridge_t* bridge,
    float integration,
    uint32_t modality_count
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate parietal processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int parietal_snn_simulate(parietal_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int parietal_snn_step(parietal_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int parietal_snn_forward(
    parietal_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get spatial output from SNN activity
 * @param bridge Bridge handle
 * @param spatial Output spatial structure
 * @return 0 on success, -1 on failure
 */
int parietal_snn_get_spatial_output(
    parietal_snn_bridge_t* bridge,
    parietal_spatial_output_t* spatial
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int parietal_snn_get_activations(
    parietal_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for high spatial attention
 * @param bridge Bridge handle
 * @param attention_level Output attention level
 * @return true if high attention detected, false otherwise
 */
bool parietal_snn_check_attention(
    parietal_snn_bridge_t* bridge,
    float* attention_level
);

/**
 * @brief Check for high precision
 * @param bridge Bridge handle
 * @param precision_level Output precision level
 * @return true if high precision detected, false otherwise
 */
bool parietal_snn_check_precision(
    parietal_snn_bridge_t* bridge,
    float* precision_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool parietal_snn_check_state_change(
    parietal_snn_bridge_t* bridge,
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
int parietal_snn_get_dim_state(
    parietal_snn_bridge_t* bridge,
    uint32_t dim,
    parietal_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int parietal_snn_get_state(
    parietal_snn_bridge_t* bridge,
    parietal_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int parietal_snn_get_stats(parietal_snn_bridge_t* bridge, parietal_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int parietal_snn_reset_stats(parietal_snn_bridge_t* bridge);

/**
 * @brief Get current spatial processing level
 * @param bridge Bridge handle
 * @return Spatial processing [0-1], -1 on error
 */
float parietal_snn_get_spatial_activity(parietal_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float parietal_snn_get_total_activity(parietal_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register spatial attention detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int parietal_snn_register_attention_callback(
    parietal_snn_bridge_t* bridge,
    parietal_snn_attention_callback_t callback,
    void* user_data
);

/**
 * @brief Register spatial output callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int parietal_snn_register_spatial_callback(
    parietal_snn_bridge_t* bridge,
    parietal_snn_spatial_callback_t callback,
    void* user_data
);

/**
 * @brief Register high precision callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int parietal_snn_register_precision_callback(
    parietal_snn_bridge_t* bridge,
    parietal_snn_precision_callback_t callback,
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
int parietal_snn_bio_async_connect(parietal_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int parietal_snn_bio_async_disconnect(parietal_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool parietal_snn_is_bio_async_connected(parietal_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_SNN_BRIDGE_H */
