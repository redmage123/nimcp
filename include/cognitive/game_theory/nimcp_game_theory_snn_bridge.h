/**
 * @file nimcp_game_theory_snn_bridge.h
 * @brief Game Theory - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between game theory engine and spiking neural networks
 * WHY:  Enable biologically-plausible strategic reasoning through
 *       population coding and spike-timing dynamics
 * HOW:  Encode strategy dimensions as spike patterns, decode equilibrium
 *       states from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Camerer (2003): Behavioral game theory and neural coding
 * - Lee (2008): Game theory and neural mechanisms of strategic choice
 * - Sanfey (2007): Social decision-making and the brain
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex for strategy representation
 * - Anterior cingulate cortex for conflict monitoring
 * - Striatum for reward prediction and opponent modeling
 * - Insular cortex for fairness detection
 *
 * INTEGRATION WITH LEARNING:
 * - Nash equilibrium detection through population dynamics
 * - Opponent modeling via predictive coding
 * - Cooperation/defection through lateral inhibition
 *
 * @see nimcp_game_theory.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_GAME_THEORY_SNN_BRIDGE_H
#define NIMCP_GAME_THEORY_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum strategy dimensions to encode */
#define GAME_THEORY_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per strategy dimension */
#define GAME_THEORY_SNN_NEURONS_PER_DIM    32

/** @brief Default equilibrium threshold */
#define GAME_THEORY_SNN_EQUILIBRIUM_THRESH 0.5f

/** @brief Default encoding window (ms) */
#define GAME_THEORY_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_GAME_THEORY_SNN         0x1510

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Game theory dimension types for SNN encoding
 */
typedef enum {
    GT_DIM_COOPERATION = 0,              /**< Cooperation tendency */
    GT_DIM_DEFECTION,                    /**< Defection tendency */
    GT_DIM_PAYOFF_EXPECTATION,           /**< Expected payoff level */
    GT_DIM_OPPONENT_MODEL,               /**< Opponent strategy estimate */
    GT_DIM_RISK_TOLERANCE,               /**< Risk tolerance level */
    GT_DIM_FAIRNESS,                     /**< Fairness perception */
    GT_DIM_TRUST,                        /**< Trust level */
    GT_DIM_RECIPROCITY,                  /**< Reciprocity tendency */
    GT_DIM_EQUILIBRIUM_DISTANCE,         /**< Distance from Nash equilibrium */
    GT_DIM_STRATEGY_ENTROPY,             /**< Strategy randomness/entropy */
    GT_DIM_COUNT
} game_theory_snn_dimension_t;

/**
 * @brief Encoding methods for strategy contexts
 */
typedef enum {
    GT_SNN_ENCODE_RATE = 0,              /**< Rate coding of dimensions */
    GT_SNN_ENCODE_TEMPORAL,               /**< Temporal spike patterns */
    GT_SNN_ENCODE_POPULATION,             /**< Population vector coding */
    GT_SNN_ENCODE_SYNCHRONY               /**< Synchrony-based encoding */
} game_theory_snn_encoding_t;

/**
 * @brief Decoding methods for strategic states
 */
typedef enum {
    GT_SNN_DECODE_THRESHOLD = 0,         /**< Threshold-based detection */
    GT_SNN_DECODE_COMPETITION,            /**< Winner-take-all */
    GT_SNN_DECODE_SOFTMAX,                /**< Soft probabilistic */
    GT_SNN_DECODE_INTEGRATION             /**< Evidence accumulation */
} game_theory_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    GT_SNN_STATE_IDLE = 0,
    GT_SNN_STATE_ENCODING,
    GT_SNN_STATE_PROCESSING,
    GT_SNN_STATE_DECODING,
    GT_SNN_STATE_SIMULATING,
    GT_SNN_STATE_ERROR
} game_theory_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Game Theory-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of strategy dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    game_theory_snn_encoding_t encoding; /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    game_theory_snn_decoding_t decoding; /**< Decoding method */
    float equilibrium_threshold;         /**< Threshold for Nash equilibrium detection */
    float cooperation_threshold;         /**< Threshold for cooperation detection */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_equilibrium_detection;   /**< Enable equilibrium signal detection */
    float equilibrium_sensitivity;       /**< Equilibrium detection sensitivity */

    /* Strategy integration */
    bool enable_opponent_modeling;       /**< Enable opponent modeling circuits */
    float opponent_model_gain;           /**< Opponent model signal gain */
    bool enable_payoff_tracking;         /**< Enable payoff expectation tracking */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} game_theory_snn_config_t;

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
} game_theory_dim_state_t;

/**
 * @brief Strategic decision output
 */
typedef struct {
    float cooperation_level;             /**< Cooperation tendency [0-1] */
    float defection_level;               /**< Defection tendency [0-1] */
    float payoff_expectation;            /**< Expected payoff [0-1] */
    float equilibrium_distance;          /**< Distance from Nash [0-1] */
    float opponent_prediction;           /**< Predicted opponent action */
    bool equilibrium_detected;           /**< Near Nash equilibrium */
    bool cooperation_dominant;           /**< Cooperation is best response */
    float trust_level;                   /**< Trust estimate */
    float fairness_level;                /**< Fairness perception */
    float risk_tolerance;                /**< Risk tolerance */
} game_theory_decision_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    game_theory_snn_state_t state;       /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_strategy;                 /**< Mean strategy activation */
    float equilibrium_signal;            /**< Current equilibrium signal */
    float payoff_signal;                 /**< Current payoff signal */
} game_theory_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t equilibrium_detections;     /**< Equilibrium detections */
    uint64_t cooperation_events;         /**< Cooperation dominant events */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_cooperation;              /**< Mean cooperation level */
} game_theory_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct game_theory_snn_bridge game_theory_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Equilibrium detection callback */
typedef void (*game_theory_snn_equilibrium_callback_t)(
    game_theory_snn_bridge_t* bridge,
    float equilibrium_distance,
    uint64_t latency_us,
    void* user_data
);

/** @brief Decision ready callback */
typedef void (*game_theory_snn_decision_callback_t)(
    game_theory_snn_bridge_t* bridge,
    const game_theory_decision_t* decision,
    void* user_data
);

/** @brief Cooperation event callback */
typedef void (*game_theory_snn_cooperation_callback_t)(
    game_theory_snn_bridge_t* bridge,
    float cooperation_level,
    uint32_t strategy_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
game_theory_snn_config_t game_theory_snn_config_default(void);

/**
 * @brief Create game theory SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
game_theory_snn_bridge_t* game_theory_snn_create(const game_theory_snn_config_t* config);

/**
 * @brief Destroy game theory SNN bridge
 * @param bridge Bridge to destroy
 */
void game_theory_snn_destroy(game_theory_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int game_theory_snn_reset(game_theory_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode strategy state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int game_theory_snn_encode_state(
    game_theory_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode payoff matrix representation
 * @param bridge Bridge handle
 * @param payoffs Payoff values (normalized)
 * @param num_actions Number of actions
 * @return Spike count on success, -1 on failure
 */
int game_theory_snn_encode_payoff(
    game_theory_snn_bridge_t* bridge,
    const float* payoffs,
    uint32_t num_actions
);

/**
 * @brief Encode opponent model
 * @param bridge Bridge handle
 * @param opponent_strategy Predicted opponent strategy [0-1]
 * @param confidence Prediction confidence [0-1]
 * @return Spike count on success, -1 on failure
 */
int game_theory_snn_encode_opponent(
    game_theory_snn_bridge_t* bridge,
    float opponent_strategy,
    float confidence
);

/**
 * @brief Encode cooperation/defection state
 * @param bridge Bridge handle
 * @param cooperation Cooperation level [0-1]
 * @param defection Defection level [0-1]
 * @return Spike count on success, -1 on failure
 */
int game_theory_snn_encode_cooperation(
    game_theory_snn_bridge_t* bridge,
    float cooperation,
    float defection
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate strategic processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int game_theory_snn_simulate(game_theory_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int game_theory_snn_step(game_theory_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int game_theory_snn_forward(
    game_theory_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get strategic decision from SNN activity
 * @param bridge Bridge handle
 * @param decision Output decision structure
 * @return 0 on success, -1 on failure
 */
int game_theory_snn_get_decision(
    game_theory_snn_bridge_t* bridge,
    game_theory_decision_t* decision
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int game_theory_snn_get_activations(
    game_theory_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for Nash equilibrium proximity
 * @param bridge Bridge handle
 * @param distance Output equilibrium distance
 * @return true if near equilibrium, false otherwise
 */
bool game_theory_snn_check_equilibrium(
    game_theory_snn_bridge_t* bridge,
    float* distance
);

/**
 * @brief Check for cooperation dominance
 * @param bridge Bridge handle
 * @param cooperation_level Output cooperation level
 * @return true if cooperation dominant, false otherwise
 */
bool game_theory_snn_check_cooperation(
    game_theory_snn_bridge_t* bridge,
    float* cooperation_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool game_theory_snn_check_state_change(
    game_theory_snn_bridge_t* bridge,
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
int game_theory_snn_get_dim_state(
    game_theory_snn_bridge_t* bridge,
    uint32_t dim,
    game_theory_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int game_theory_snn_get_state(
    game_theory_snn_bridge_t* bridge,
    game_theory_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int game_theory_snn_get_stats(game_theory_snn_bridge_t* bridge, game_theory_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int game_theory_snn_reset_stats(game_theory_snn_bridge_t* bridge);

/**
 * @brief Get current cooperation level
 * @param bridge Bridge handle
 * @return Cooperation level [0-1], -1 on error
 */
float game_theory_snn_get_cooperation(game_theory_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float game_theory_snn_get_total_activity(game_theory_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register equilibrium detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int game_theory_snn_register_equilibrium_callback(
    game_theory_snn_bridge_t* bridge,
    game_theory_snn_equilibrium_callback_t callback,
    void* user_data
);

/**
 * @brief Register decision callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int game_theory_snn_register_decision_callback(
    game_theory_snn_bridge_t* bridge,
    game_theory_snn_decision_callback_t callback,
    void* user_data
);

/**
 * @brief Register cooperation event callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int game_theory_snn_register_cooperation_callback(
    game_theory_snn_bridge_t* bridge,
    game_theory_snn_cooperation_callback_t callback,
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
int game_theory_snn_bio_async_connect(game_theory_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int game_theory_snn_bio_async_disconnect(game_theory_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool game_theory_snn_is_bio_async_connected(game_theory_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GAME_THEORY_SNN_BRIDGE_H */
