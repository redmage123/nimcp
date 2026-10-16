/**
 * @file nimcp_wellbeing_snn_bridge.h
 * @brief Wellbeing - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between wellbeing system and spiking neural networks
 * WHY:  Enable biologically-plausible wellbeing state representation through
 *       population coding and spike-timing dynamics
 * HOW:  Encode wellbeing dimensions as spike patterns, decode flourishing states
 *       from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Ryff (1989): Psychological Wellbeing Model - six dimensions of wellness
 * - Seligman (2011): PERMA model - positive emotions, engagement, relationships,
 *   meaning, and accomplishment
 * - Kahneman (1999): Hedonic vs eudaimonic wellbeing distinction
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex (PFC) for self-regulation and goal pursuit
 * - Ventral striatum for reward and hedonic tone
 * - Insula for interoceptive awareness of bodily states
 * - Default mode network (DMN) for self-reflection and meaning
 * - Hypothalamus for homeostatic balance
 *
 * WELLBEING DIMENSIONS:
 * - Hedonic: Pleasure/pain balance, positive affect
 * - Eudaimonic: Meaning, purpose, self-actualization
 * - Vitality: Energy levels, physical vigor
 * - Resilience: Stress recovery, adaptive capacity
 * - Social connection: Belongingness, social support
 * - Autonomy: Self-determination, agency
 * - Competence: Mastery, effectiveness
 * - Balance: Life domain harmony
 * - Flourishing: Overall thriving state
 *
 * @see nimcp_wellbeing.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_WELLBEING_SNN_BRIDGE_H
#define NIMCP_WELLBEING_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum wellbeing dimensions to encode */
#define WELLBEING_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per wellbeing dimension */
#define WELLBEING_SNN_NEURONS_PER_DIM    32

/** @brief Default stress threshold */
#define WELLBEING_SNN_STRESS_THRESHOLD   0.7f

/** @brief Default encoding window (ms) */
#define WELLBEING_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_WELLBEING_SNN         0x0D50

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Wellbeing dimension types for SNN encoding
 */
typedef enum {
    WELLBEING_DIM_HEDONIC = 0,       /**< Hedonic tone (pleasure/pain) */
    WELLBEING_DIM_EUDAIMONIC,         /**< Eudaimonic wellbeing (meaning/purpose) */
    WELLBEING_DIM_VITALITY,           /**< Energy/vitality level */
    WELLBEING_DIM_RESILIENCE,         /**< Psychological resilience */
    WELLBEING_DIM_SOCIAL_CONNECTION,  /**< Social connectedness */
    WELLBEING_DIM_AUTONOMY,           /**< Sense of autonomy */
    WELLBEING_DIM_COMPETENCE,         /**< Competence satisfaction */
    WELLBEING_DIM_STRESS,             /**< Stress level (inverse) */
    WELLBEING_DIM_BALANCE,            /**< Life balance */
    WELLBEING_DIM_FLOURISHING,        /**< Overall flourishing */
    WELLBEING_DIM_COUNT
} wellbeing_snn_dimension_t;

/**
 * @brief Encoding methods for wellbeing states
 */
typedef enum {
    WELLBEING_SNN_ENCODE_RATE = 0,    /**< Rate coding of dimensions */
    WELLBEING_SNN_ENCODE_TEMPORAL,     /**< Temporal spike patterns */
    WELLBEING_SNN_ENCODE_POPULATION,   /**< Population vector coding */
    WELLBEING_SNN_ENCODE_SYNCHRONY     /**< Synchrony-based encoding */
} wellbeing_snn_encoding_t;

/**
 * @brief Decoding methods for flourishing assessment
 */
typedef enum {
    WELLBEING_SNN_DECODE_THRESHOLD = 0, /**< Threshold-based assessment */
    WELLBEING_SNN_DECODE_COMPETITION,    /**< Winner-take-all */
    WELLBEING_SNN_DECODE_SOFTMAX,        /**< Soft probabilistic */
    WELLBEING_SNN_DECODE_INTEGRATION     /**< Evidence accumulation */
} wellbeing_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    WELLBEING_SNN_STATE_IDLE = 0,
    WELLBEING_SNN_STATE_ENCODING,
    WELLBEING_SNN_STATE_PROCESSING,
    WELLBEING_SNN_STATE_DECODING,
    WELLBEING_SNN_STATE_SIMULATING,
    WELLBEING_SNN_STATE_ERROR
} wellbeing_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Wellbeing-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of wellbeing dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    wellbeing_snn_encoding_t encoding;   /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    wellbeing_snn_decoding_t decoding;   /**< Decoding method */
    float flourishing_threshold;         /**< Threshold for flourishing detection */
    float stress_threshold;              /**< Threshold for stress detection */
    float balance_threshold;             /**< Threshold for balance assessment */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_stress_detection;        /**< Enable stress signal detection */
    float vitality_weight;               /**< Weight for vitality assessment */

    /* Homeostatic integration */
    bool enable_homeostasis;             /**< Enable homeostatic monitoring */
    float homeostatic_gain;              /**< Homeostatic signal gain */
    bool enable_resilience_tracking;     /**< Enable resilience circuits */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} wellbeing_snn_config_t;

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
} wellbeing_dim_state_t;

/**
 * @brief Flourishing assessment output
 */
typedef struct {
    float hedonic_tone;                  /**< Hedonic wellbeing [0-1] */
    float eudaimonic_level;              /**< Eudaimonic wellbeing [0-1] */
    float vitality_level;                /**< Energy/vitality [0-1] */
    float resilience_score;              /**< Psychological resilience [0-1] */
    float social_connection;             /**< Social connectedness [0-1] */
    float autonomy_level;                /**< Autonomy satisfaction [0-1] */
    float competence_level;              /**< Competence satisfaction [0-1] */
    float stress_level;                  /**< Current stress [0-1] */
    bool stress_detected;                /**< High stress flag */
    bool balance_achieved;               /**< Life balance indicator */
    float flourishing_score;             /**< Overall flourishing [0-1] */
    float integration_score;             /**< Dimension integration (coherence) */
} wellbeing_assessment_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    wellbeing_snn_state_t state;         /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_wellbeing;                /**< Mean wellbeing score */
    float stress_signal;                 /**< Current stress signal strength */
    float balance_signal;                /**< Current balance signal */
} wellbeing_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t stress_detections;          /**< High stress detections */
    uint64_t flourishing_detections;     /**< Flourishing state detections */
    uint64_t balance_achievements;       /**< Balance state detections */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_flourishing;              /**< Mean flourishing score */
} wellbeing_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct wellbeing_snn_bridge wellbeing_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Stress detection callback */
typedef void (*wellbeing_snn_stress_callback_t)(
    wellbeing_snn_bridge_t* bridge,
    float stress_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Assessment ready callback */
typedef void (*wellbeing_snn_assessment_callback_t)(
    wellbeing_snn_bridge_t* bridge,
    const wellbeing_assessment_t* assessment,
    void* user_data
);

/** @brief Balance change callback */
typedef void (*wellbeing_snn_balance_callback_t)(
    wellbeing_snn_bridge_t* bridge,
    float balance_level,
    bool balance_achieved,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
wellbeing_snn_config_t wellbeing_snn_config_default(void);

/**
 * @brief Create wellbeing SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
wellbeing_snn_bridge_t* wellbeing_snn_create(const wellbeing_snn_config_t* config);

/**
 * @brief Destroy wellbeing SNN bridge
 * @param bridge Bridge to destroy
 */
void wellbeing_snn_destroy(wellbeing_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int wellbeing_snn_reset(wellbeing_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode wellbeing state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int wellbeing_snn_encode_state(
    wellbeing_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode hedonic state (pleasure/pain)
 * @param bridge Bridge handle
 * @param pleasure Pleasure level [0-1]
 * @param pain Pain level [0-1]
 * @return Spike count on success, -1 on failure
 */
int wellbeing_snn_encode_hedonic(
    wellbeing_snn_bridge_t* bridge,
    float pleasure,
    float pain
);

/**
 * @brief Encode eudaimonic state (meaning/purpose)
 * @param bridge Bridge handle
 * @param meaning Sense of meaning [0-1]
 * @param purpose Sense of purpose [0-1]
 * @param growth Personal growth [0-1]
 * @return Spike count on success, -1 on failure
 */
int wellbeing_snn_encode_eudaimonic(
    wellbeing_snn_bridge_t* bridge,
    float meaning,
    float purpose,
    float growth
);

/**
 * @brief Encode stress level
 * @param bridge Bridge handle
 * @param stress_level Current stress [0-1]
 * @param chronic Whether stress is chronic
 * @return Spike count on success, -1 on failure
 */
int wellbeing_snn_encode_stress(
    wellbeing_snn_bridge_t* bridge,
    float stress_level,
    bool chronic
);

/**
 * @brief Encode social connection state
 * @param bridge Bridge handle
 * @param belongingness Sense of belonging [0-1]
 * @param support Social support level [0-1]
 * @param loneliness Loneliness level [0-1]
 * @return Spike count on success, -1 on failure
 */
int wellbeing_snn_encode_social(
    wellbeing_snn_bridge_t* bridge,
    float belongingness,
    float support,
    float loneliness
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate wellbeing processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int wellbeing_snn_simulate(wellbeing_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int wellbeing_snn_step(wellbeing_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int wellbeing_snn_forward(
    wellbeing_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get wellbeing assessment from SNN activity
 * @param bridge Bridge handle
 * @param assessment Output assessment structure
 * @return 0 on success, -1 on failure
 */
int wellbeing_snn_get_assessment(
    wellbeing_snn_bridge_t* bridge,
    wellbeing_assessment_t* assessment
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int wellbeing_snn_get_activations(
    wellbeing_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for high stress
 * @param bridge Bridge handle
 * @param stress_level Output stress level
 * @return true if high stress detected, false otherwise
 */
bool wellbeing_snn_check_stress(
    wellbeing_snn_bridge_t* bridge,
    float* stress_level
);

/**
 * @brief Check for flourishing state
 * @param bridge Bridge handle
 * @param flourishing_level Output flourishing level
 * @return true if flourishing detected, false otherwise
 */
bool wellbeing_snn_check_flourishing(
    wellbeing_snn_bridge_t* bridge,
    float* flourishing_level
);

/**
 * @brief Check for life balance
 * @param bridge Bridge handle
 * @param balance_score Output balance score
 * @return true if balance achieved, false otherwise
 */
bool wellbeing_snn_check_balance(
    wellbeing_snn_bridge_t* bridge,
    float* balance_score
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
int wellbeing_snn_get_dim_state(
    wellbeing_snn_bridge_t* bridge,
    uint32_t dim,
    wellbeing_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int wellbeing_snn_get_state(
    wellbeing_snn_bridge_t* bridge,
    wellbeing_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int wellbeing_snn_get_stats(wellbeing_snn_bridge_t* bridge, wellbeing_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int wellbeing_snn_reset_stats(wellbeing_snn_bridge_t* bridge);

/**
 * @brief Get current flourishing level
 * @param bridge Bridge handle
 * @return Flourishing level [0-1], -1 on error
 */
float wellbeing_snn_get_flourishing(wellbeing_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float wellbeing_snn_get_total_activity(wellbeing_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register stress detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int wellbeing_snn_register_stress_callback(
    wellbeing_snn_bridge_t* bridge,
    wellbeing_snn_stress_callback_t callback,
    void* user_data
);

/**
 * @brief Register assessment callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int wellbeing_snn_register_assessment_callback(
    wellbeing_snn_bridge_t* bridge,
    wellbeing_snn_assessment_callback_t callback,
    void* user_data
);

/**
 * @brief Register balance change callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int wellbeing_snn_register_balance_callback(
    wellbeing_snn_bridge_t* bridge,
    wellbeing_snn_balance_callback_t callback,
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
int wellbeing_snn_bio_async_connect(wellbeing_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int wellbeing_snn_bio_async_disconnect(wellbeing_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool wellbeing_snn_is_bio_async_connected(wellbeing_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_SNN_BRIDGE_H */
