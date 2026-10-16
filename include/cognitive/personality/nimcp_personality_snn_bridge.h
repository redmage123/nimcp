/**
 * @file nimcp_personality_snn_bridge.h
 * @brief Personality - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between personality engine and spiking neural networks
 * WHY:  Enable biologically-plausible personality trait encoding through
 *       population coding and spike-timing dynamics
 * HOW:  Encode Big Five traits as spike patterns, decode behavioral
 *       tendencies from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - DeYoung (2010): Personality neuroscience and the Big Five
 * - Canli (2004): Functional brain mapping of Extraversion and Neuroticism
 * - Gray (1970): BIS/BAS theory of personality
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex for conscientiousness/planning
 * - Amygdala for neuroticism/threat sensitivity
 * - Dopaminergic circuits for extraversion/reward seeking
 * - Serotonergic system for agreeableness/impulse control
 * - Hippocampal novelty for openness
 *
 * INTEGRATION WITH BEHAVIOR:
 * - Trait stability through sustained firing patterns
 * - State fluctuations via transient activity
 * - Behavioral tendencies through population coding
 *
 * @see nimcp_personality.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_PERSONALITY_SNN_BRIDGE_H
#define NIMCP_PERSONALITY_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum personality dimensions to encode */
#define PERSONALITY_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per personality dimension */
#define PERSONALITY_SNN_NEURONS_PER_DIM    32

/** @brief Default trait stability threshold */
#define PERSONALITY_SNN_STABILITY_THRESH   0.7f

/** @brief Default encoding window (ms) */
#define PERSONALITY_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_PERSONALITY_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Personality dimension types for SNN encoding (OCEAN + Temperament)
 */
typedef enum {
    PERSONALITY_DIM_OPENNESS = 0,          /**< Openness to experience */
    PERSONALITY_DIM_CONSCIENTIOUSNESS,      /**< Conscientiousness */
    PERSONALITY_DIM_EXTRAVERSION,           /**< Extraversion */
    PERSONALITY_DIM_AGREEABLENESS,          /**< Agreeableness */
    PERSONALITY_DIM_NEUROTICISM,            /**< Neuroticism */
    PERSONALITY_DIM_APPROACH,               /**< Behavioral approach (BAS) */
    PERSONALITY_DIM_AVOIDANCE,              /**< Behavioral avoidance (BIS) */
    PERSONALITY_DIM_IMPULSIVITY,            /**< Impulse control */
    PERSONALITY_DIM_SOCIABILITY,            /**< Social drive */
    PERSONALITY_DIM_EMOTIONALITY,           /**< Emotional reactivity */
    PERSONALITY_DIM_COUNT
} personality_snn_dimension_t;

/**
 * @brief Encoding methods for personality traits
 */
typedef enum {
    PERSONALITY_SNN_ENCODE_RATE = 0,       /**< Rate coding of traits */
    PERSONALITY_SNN_ENCODE_TEMPORAL,        /**< Temporal spike patterns */
    PERSONALITY_SNN_ENCODE_POPULATION,      /**< Population vector coding */
    PERSONALITY_SNN_ENCODE_SYNCHRONY        /**< Synchrony-based encoding */
} personality_snn_encoding_t;

/**
 * @brief Decoding methods for behavioral outputs
 */
typedef enum {
    PERSONALITY_SNN_DECODE_THRESHOLD = 0,  /**< Threshold-based detection */
    PERSONALITY_SNN_DECODE_COMPETITION,     /**< Winner-take-all */
    PERSONALITY_SNN_DECODE_SOFTMAX,         /**< Soft probabilistic */
    PERSONALITY_SNN_DECODE_INTEGRATION      /**< Evidence accumulation */
} personality_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    PERSONALITY_SNN_STATE_IDLE = 0,
    PERSONALITY_SNN_STATE_ENCODING,
    PERSONALITY_SNN_STATE_PROCESSING,
    PERSONALITY_SNN_STATE_DECODING,
    PERSONALITY_SNN_STATE_SIMULATING,
    PERSONALITY_SNN_STATE_ERROR
} personality_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Personality-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of personality dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    personality_snn_encoding_t encoding; /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    personality_snn_decoding_t decoding; /**< Decoding method */
    float stability_threshold;           /**< Threshold for trait stability */
    float fluctuation_threshold;         /**< Threshold for state fluctuation */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_trait_stability;         /**< Enable trait stability detection */
    float stability_sensitivity;         /**< Trait stability sensitivity */

    /* Behavioral integration */
    bool enable_behavioral_output;       /**< Enable behavioral tendency output */
    float behavioral_gain;               /**< Behavioral signal gain */
    bool enable_temperament;             /**< Enable temperament dimensions */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} personality_snn_config_t;

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
} personality_dim_state_t;

/**
 * @brief Behavioral tendency output
 */
typedef struct {
    float openness_level;                /**< Current openness [0-1] */
    float conscientiousness_level;       /**< Current conscientiousness [0-1] */
    float extraversion_level;            /**< Current extraversion [0-1] */
    float agreeableness_level;           /**< Current agreeableness [0-1] */
    float neuroticism_level;             /**< Current neuroticism [0-1] */
    float approach_tendency;             /**< Approach behavior strength */
    float avoidance_tendency;            /**< Avoidance behavior strength */
    bool trait_stable;                   /**< Traits are stable */
    bool high_fluctuation;               /**< High state fluctuation detected */
    float fluctuation_magnitude;         /**< Fluctuation magnitude if detected */
    float social_drive;                  /**< Social drive level */
    float emotional_reactivity;          /**< Emotional reactivity level */
} personality_tendency_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    personality_snn_state_t state;       /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_trait_level;              /**< Mean trait activation */
    float stability_signal;              /**< Current stability signal */
    float behavioral_signal;             /**< Current behavioral signal */
} personality_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t stability_detections;       /**< Trait stability detections */
    uint64_t fluctuation_events;         /**< High fluctuation events */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_trait_stability;          /**< Mean trait stability */
} personality_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct personality_snn_bridge personality_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Trait stability callback */
typedef void (*personality_snn_stability_callback_t)(
    personality_snn_bridge_t* bridge,
    float stability_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Tendency ready callback */
typedef void (*personality_snn_tendency_callback_t)(
    personality_snn_bridge_t* bridge,
    const personality_tendency_t* tendency,
    void* user_data
);

/** @brief Fluctuation callback */
typedef void (*personality_snn_fluctuation_callback_t)(
    personality_snn_bridge_t* bridge,
    float fluctuation_level,
    uint32_t fluctuation_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
personality_snn_config_t personality_snn_config_default(void);

/**
 * @brief Create personality SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
personality_snn_bridge_t* personality_snn_create(const personality_snn_config_t* config);

/**
 * @brief Destroy personality SNN bridge
 * @param bridge Bridge to destroy
 */
void personality_snn_destroy(personality_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int personality_snn_reset(personality_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode personality state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int personality_snn_encode_state(
    personality_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode Big Five traits
 * @param bridge Bridge handle
 * @param openness Openness level [0-1]
 * @param conscientiousness Conscientiousness level [0-1]
 * @param extraversion Extraversion level [0-1]
 * @param agreeableness Agreeableness level [0-1]
 * @param neuroticism Neuroticism level [0-1]
 * @return Spike count on success, -1 on failure
 */
int personality_snn_encode_ocean(
    personality_snn_bridge_t* bridge,
    float openness,
    float conscientiousness,
    float extraversion,
    float agreeableness,
    float neuroticism
);

/**
 * @brief Encode temperament dimensions
 * @param bridge Bridge handle
 * @param approach Approach tendency [0-1]
 * @param avoidance Avoidance tendency [0-1]
 * @return Spike count on success, -1 on failure
 */
int personality_snn_encode_temperament(
    personality_snn_bridge_t* bridge,
    float approach,
    float avoidance
);

/**
 * @brief Encode behavioral tendency
 * @param bridge Bridge handle
 * @param sociability Social drive [0-1]
 * @param emotionality Emotional reactivity [0-1]
 * @return Spike count on success, -1 on failure
 */
int personality_snn_encode_behavioral(
    personality_snn_bridge_t* bridge,
    float sociability,
    float emotionality
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate personality processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int personality_snn_simulate(personality_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int personality_snn_step(personality_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int personality_snn_forward(
    personality_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get behavioral tendency from SNN activity
 * @param bridge Bridge handle
 * @param tendency Output tendency structure
 * @return 0 on success, -1 on failure
 */
int personality_snn_get_tendency(
    personality_snn_bridge_t* bridge,
    personality_tendency_t* tendency
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int personality_snn_get_activations(
    personality_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for trait stability
 * @param bridge Bridge handle
 * @param stability_level Output stability level
 * @return true if traits are stable, false otherwise
 */
bool personality_snn_check_stability(
    personality_snn_bridge_t* bridge,
    float* stability_level
);

/**
 * @brief Check for high fluctuation
 * @param bridge Bridge handle
 * @param fluctuation_level Output fluctuation level
 * @return true if high fluctuation detected, false otherwise
 */
bool personality_snn_check_fluctuation(
    personality_snn_bridge_t* bridge,
    float* fluctuation_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool personality_snn_check_state_change(
    personality_snn_bridge_t* bridge,
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
int personality_snn_get_dim_state(
    personality_snn_bridge_t* bridge,
    uint32_t dim,
    personality_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int personality_snn_get_state(
    personality_snn_bridge_t* bridge,
    personality_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int personality_snn_get_stats(personality_snn_bridge_t* bridge, personality_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int personality_snn_reset_stats(personality_snn_bridge_t* bridge);

/**
 * @brief Get current trait stability level
 * @param bridge Bridge handle
 * @return Stability level [0-1], -1 on error
 */
float personality_snn_get_stability(personality_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float personality_snn_get_total_activity(personality_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register stability detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int personality_snn_register_stability_callback(
    personality_snn_bridge_t* bridge,
    personality_snn_stability_callback_t callback,
    void* user_data
);

/**
 * @brief Register tendency callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int personality_snn_register_tendency_callback(
    personality_snn_bridge_t* bridge,
    personality_snn_tendency_callback_t callback,
    void* user_data
);

/**
 * @brief Register fluctuation callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int personality_snn_register_fluctuation_callback(
    personality_snn_bridge_t* bridge,
    personality_snn_fluctuation_callback_t callback,
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
int personality_snn_bio_async_connect(personality_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int personality_snn_bio_async_disconnect(personality_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool personality_snn_is_bio_async_connected(personality_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PERSONALITY_SNN_BRIDGE_H */
