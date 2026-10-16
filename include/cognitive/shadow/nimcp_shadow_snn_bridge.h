/**
 * @file nimcp_shadow_snn_bridge.h
 * @brief Shadow Emotions - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between shadow emotions and spiking neural networks
 * WHY:  Enable biologically-plausible unconscious processing through population
 *       coding and spike-timing dynamics for repressed emotional content
 * HOW:  Encode suppressed emotion dimensions as spike patterns, decode defense
 *       mechanism activations from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Jung (1938): Shadow archetype and unconscious processing
 * - Freud (1915): Repression and defense mechanisms
 * - Hassin (2013): Unconscious emotional processing
 * - LeDoux (2000): Subcortical emotion circuits
 *
 * BIOLOGICAL BASIS:
 * - Amygdala for threat/fear processing (jealousy, envy detection)
 * - Orbitofrontal cortex for value/reward (greed circuits)
 * - Ventral striatum for social comparison (envy encoding)
 * - Prefrontal cortex for impulse control (defense mechanisms)
 * - Anterior cingulate for conflict detection (shadow integration)
 *
 * INTEGRATION WITH SHADOW SYSTEM:
 * - Suppressed emotion encoding via population variability
 * - Defense mechanism activation through firing rate comparisons
 * - Unconscious processing via sustained subthreshold activity
 *
 * @see nimcp_shadow_emotions.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_SHADOW_SNN_BRIDGE_H
#define NIMCP_SHADOW_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum shadow dimensions to encode */
#define SHADOW_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per shadow dimension */
#define SHADOW_SNN_NEURONS_PER_DIM    32

/** @brief Default suppression threshold */
#define SHADOW_SNN_SUPPRESSION_THRESH 0.5f

/** @brief Default encoding window (ms) */
#define SHADOW_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_SHADOW_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Shadow dimension types for SNN encoding
 *
 * THEORY: These dimensions represent unconscious emotional content
 * that may be actively suppressed or repressed (Jung, 1938)
 */
typedef enum {
    SHADOW_DIM_SUPPRESSED_ANGER = 0,     /**< Repressed anger/rage */
    SHADOW_DIM_SUPPRESSED_FEAR,           /**< Repressed fear/anxiety */
    SHADOW_DIM_SUPPRESSED_SHAME,          /**< Repressed shame/guilt */
    SHADOW_DIM_JEALOUSY_INTENSITY,        /**< Jealousy activation level */
    SHADOW_DIM_ENVY_INTENSITY,            /**< Envy activation level */
    SHADOW_DIM_NARCISSISM_LEVEL,          /**< Narcissistic activation */
    SHADOW_DIM_GREED_ACTIVATION,          /**< Greed circuit activation */
    SHADOW_DIM_HUBRIS_LEVEL,              /**< Hubris/overconfidence */
    SHADOW_DIM_DEFENSE_ACTIVATION,        /**< Defense mechanism strength */
    SHADOW_DIM_REPRESSION_STRENGTH,       /**< Repression intensity */
    SHADOW_DIM_COUNT
} shadow_snn_dimension_t;

/**
 * @brief Encoding methods for shadow contexts
 */
typedef enum {
    SHADOW_SNN_ENCODE_RATE = 0,          /**< Rate coding of dimensions */
    SHADOW_SNN_ENCODE_TEMPORAL,           /**< Temporal spike patterns */
    SHADOW_SNN_ENCODE_POPULATION,         /**< Population vector coding */
    SHADOW_SNN_ENCODE_SYNCHRONY           /**< Synchrony-based encoding */
} shadow_snn_encoding_t;

/**
 * @brief Decoding methods for defense mechanism states
 */
typedef enum {
    SHADOW_SNN_DECODE_THRESHOLD = 0,     /**< Threshold-based detection */
    SHADOW_SNN_DECODE_COMPETITION,        /**< Winner-take-all */
    SHADOW_SNN_DECODE_SOFTMAX,            /**< Soft probabilistic */
    SHADOW_SNN_DECODE_INTEGRATION         /**< Evidence accumulation */
} shadow_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SHADOW_SNN_STATE_IDLE = 0,
    SHADOW_SNN_STATE_ENCODING,
    SHADOW_SNN_STATE_PROCESSING,
    SHADOW_SNN_STATE_DECODING,
    SHADOW_SNN_STATE_SIMULATING,
    SHADOW_SNN_STATE_ERROR
} shadow_snn_state_t;

/**
 * @brief Defense mechanism types activated by shadow processing
 *
 * THEORY: Classic Freudian defense mechanisms (A. Freud, 1936)
 */
typedef enum {
    SHADOW_DEFENSE_NONE = 0,
    SHADOW_DEFENSE_REPRESSION,            /**< Push to unconscious */
    SHADOW_DEFENSE_PROJECTION,            /**< Attribute to others */
    SHADOW_DEFENSE_DENIAL,                /**< Refuse to acknowledge */
    SHADOW_DEFENSE_RATIONALIZATION,       /**< Logical justification */
    SHADOW_DEFENSE_DISPLACEMENT,          /**< Redirect to safe target */
    SHADOW_DEFENSE_SUBLIMATION            /**< Channel to positive */
} shadow_defense_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Shadow-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of shadow dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    shadow_snn_encoding_t encoding;      /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    shadow_snn_decoding_t decoding;      /**< Decoding method */
    float suppression_threshold;         /**< Threshold for suppression detection */
    float defense_threshold;             /**< Minimum defense activation */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_suppression_detection;   /**< Enable suppression signal detection */
    float suppression_sensitivity;       /**< Suppression detection sensitivity */

    /* Unconscious processing integration */
    bool enable_unconscious_processing;  /**< Enable unconscious circuits */
    float unconscious_gain;              /**< Unconscious signal gain */
    bool enable_defense_tracking;        /**< Enable defense mechanism tracking */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} shadow_snn_config_t;

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
} shadow_dim_state_t;

/**
 * @brief Shadow processing output
 *
 * Represents the decoded unconscious emotional state
 */
typedef struct {
    float suppression_level;             /**< Overall suppression [0-1] */
    float repression_strength;           /**< Repression intensity [0-1] */
    float shadow_integration;            /**< Integration progress [0-1] */
    float defense_activation;            /**< Defense mechanism strength */
    shadow_defense_type_t active_defense;/**< Currently active defense */
    bool suppression_detected;           /**< Significant suppression detected */
    bool shadow_breakthrough;            /**< Shadow material breaking through */
    float breakthrough_magnitude;        /**< Breakthrough intensity if detected */
    float unconscious_activity;          /**< Unconscious processing level */
    float integration_progress;          /**< Shadow integration progress */
} shadow_processing_output_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    shadow_snn_state_t state;            /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_suppression;              /**< Mean suppression level */
    float defense_signal;                /**< Current defense signal */
    float unconscious_signal;            /**< Current unconscious signal */
} shadow_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t suppression_detections;     /**< Suppression detections */
    uint64_t defense_activations;        /**< Defense mechanism activations */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_suppression;              /**< Mean suppression level */
} shadow_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct shadow_snn_bridge shadow_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Suppression detection callback */
typedef void (*shadow_snn_suppression_callback_t)(
    shadow_snn_bridge_t* bridge,
    float suppression_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Shadow processing ready callback */
typedef void (*shadow_snn_processing_callback_t)(
    shadow_snn_bridge_t* bridge,
    const shadow_processing_output_t* output,
    void* user_data
);

/** @brief Defense activation callback */
typedef void (*shadow_snn_defense_callback_t)(
    shadow_snn_bridge_t* bridge,
    shadow_defense_type_t defense,
    float activation_level,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
shadow_snn_config_t shadow_snn_config_default(void);

/**
 * @brief Create shadow SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
shadow_snn_bridge_t* shadow_snn_create(const shadow_snn_config_t* config);

/**
 * @brief Destroy shadow SNN bridge
 * @param bridge Bridge to destroy
 */
void shadow_snn_destroy(shadow_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int shadow_snn_reset(shadow_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode shadow emotion state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int shadow_snn_encode_state(
    shadow_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode suppressed emotion
 * @param bridge Bridge handle
 * @param suppression Suppression level [0-1]
 * @param repression Repression strength [0-1]
 * @return Spike count on success, -1 on failure
 */
int shadow_snn_encode_suppression(
    shadow_snn_bridge_t* bridge,
    float suppression,
    float repression
);

/**
 * @brief Encode unconscious processing state
 * @param bridge Bridge handle
 * @param unconscious_level Unconscious activity [0-1]
 * @param defense_count Number of active defenses
 * @return Spike count on success, -1 on failure
 */
int shadow_snn_encode_unconscious(
    shadow_snn_bridge_t* bridge,
    float unconscious_level,
    uint32_t defense_count
);

/**
 * @brief Encode defense mechanism activation
 * @param bridge Bridge handle
 * @param defense_strength Defense activation [0-1]
 * @param defense_type Active defense type
 * @return Spike count on success, -1 on failure
 */
int shadow_snn_encode_defense(
    shadow_snn_bridge_t* bridge,
    float defense_strength,
    shadow_defense_type_t defense_type
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate shadow processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int shadow_snn_simulate(shadow_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int shadow_snn_step(shadow_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int shadow_snn_forward(
    shadow_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get shadow processing output from SNN activity
 * @param bridge Bridge handle
 * @param output Output structure
 * @return 0 on success, -1 on failure
 */
int shadow_snn_get_output(
    shadow_snn_bridge_t* bridge,
    shadow_processing_output_t* output
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int shadow_snn_get_activations(
    shadow_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for significant suppression
 * @param bridge Bridge handle
 * @param suppression_level Output suppression level
 * @return true if significant suppression detected, false otherwise
 */
bool shadow_snn_check_suppression(
    shadow_snn_bridge_t* bridge,
    float* suppression_level
);

/**
 * @brief Check for defense activation
 * @param bridge Bridge handle
 * @param defense_level Output defense activation level
 * @return true if defense activated, false otherwise
 */
bool shadow_snn_check_defense(
    shadow_snn_bridge_t* bridge,
    float* defense_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool shadow_snn_check_state_change(
    shadow_snn_bridge_t* bridge,
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
int shadow_snn_get_dim_state(
    shadow_snn_bridge_t* bridge,
    uint32_t dim,
    shadow_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int shadow_snn_get_state(
    shadow_snn_bridge_t* bridge,
    shadow_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int shadow_snn_get_stats(shadow_snn_bridge_t* bridge, shadow_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int shadow_snn_reset_stats(shadow_snn_bridge_t* bridge);

/**
 * @brief Get current suppression level
 * @param bridge Bridge handle
 * @return Suppression level [0-1], -1 on error
 */
float shadow_snn_get_suppression(shadow_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float shadow_snn_get_total_activity(shadow_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register suppression detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int shadow_snn_register_suppression_callback(
    shadow_snn_bridge_t* bridge,
    shadow_snn_suppression_callback_t callback,
    void* user_data
);

/**
 * @brief Register processing callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int shadow_snn_register_processing_callback(
    shadow_snn_bridge_t* bridge,
    shadow_snn_processing_callback_t callback,
    void* user_data
);

/**
 * @brief Register defense activation callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int shadow_snn_register_defense_callback(
    shadow_snn_bridge_t* bridge,
    shadow_snn_defense_callback_t callback,
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
int shadow_snn_bio_async_connect(shadow_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int shadow_snn_bio_async_disconnect(shadow_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool shadow_snn_is_bio_async_connected(shadow_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SHADOW_SNN_BRIDGE_H */
