/**
 * @file nimcp_genius_snn_bridge.h
 * @brief Mathematical Genius - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-24
 *
 * WHAT: Bidirectional bridge between mathematical genius module and spiking neural networks
 * WHY:  Enable biologically-plausible mathematical reasoning through population coding
 *       and spike-timing dynamics for theorem proving and conjecture generation
 * HOW:  Encode mathematical concepts as spike patterns, decode proof insights from
 *       neural activity patterns using STDP-based learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Dehaene (2011): The Number Sense - mathematical cognition in parietal cortex
 * - Amalric & Dehaene (2019): Origins of mathematical intuition
 * - Butterworth (2010): Foundational numerical capacities
 *
 * BIOLOGICAL BASIS:
 * - Intraparietal sulcus (IPS) for mathematical magnitude processing
 * - Angular gyrus for symbolic mathematical processing
 * - Prefrontal-parietal network for mathematical reasoning
 * - Hippocampal-cortical loops for mathematical memory
 *
 * INTEGRATION WITH GENIUS MODES:
 * - Gauss mode: Number theory patterns through population coding
 * - Newton mode: Calculus intuition via continuous spike representations
 * - Erdos mode: Combinatorial insights through ensemble dynamics
 *
 * @see nimcp_mathematical_genius.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_GENIUS_SNN_BRIDGE_H
#define NIMCP_GENIUS_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/parietal/nimcp_genius_modes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum genius dimensions to encode */
#define GENIUS_SNN_MAX_DIMENSIONS       32

/** @brief Neurons per genius concept */
#define GENIUS_SNN_NEURONS_PER_CONCEPT  64

/** @brief Default insight threshold */
#define GENIUS_SNN_INSIGHT_THRESH       0.7f

/** @brief Default encoding window (ms) */
#define GENIUS_SNN_ENCODING_WINDOW      100.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_GENIUS_SNN           0x0398

//=============================================================================
// KG Wiring Constants
//=============================================================================

/** @brief KG module name */
#define KG_GENIUS_SNN_MODULE_NAME       "genius_snn_bridge"

/** @brief KG module type */
#define KG_GENIUS_SNN_MODULE_TYPE       "MATHEMATICAL_REASONING"

/* Input message types */
#define KG_MSG_GENIUS_MATH_STATE        "MATH_STATE"
#define KG_MSG_GENIUS_MODE_ACTIVATION   "MODE_ACTIVATION"
#define KG_MSG_GENIUS_PATTERN_INPUT     "PATTERN_INPUT"
#define KG_MSG_GENIUS_PROOF_STEP        "PROOF_STEP"

/* Output message types */
#define KG_MSG_GENIUS_INSIGHT_DETECTED  "INSIGHT_DETECTED"
#define KG_MSG_GENIUS_MODE_RECOMMEND    "MODE_RECOMMENDATION"
#define KG_MSG_GENIUS_SPIKE_PATTERN     "SPIKE_PATTERN"
#define KG_MSG_GENIUS_BREAKTHROUGH      "BREAKTHROUGH_EVENT"

/* Handler message types */
#define KG_MSG_GENIUS_ENCODE_REQUEST    "ENCODE_REQUEST"
#define KG_MSG_GENIUS_SIMULATE_REQUEST  "SIMULATE_REQUEST"
#define KG_MSG_GENIUS_DECODE_REQUEST    "DECODE_REQUEST"

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Genius dimension types for SNN encoding
 */
typedef enum {
    GENIUS_DIM_PATTERN_RECOGNITION = 0, /**< Pattern recognition activity */
    GENIUS_DIM_PROOF_SEARCH,            /**< Theorem proving search depth */
    GENIUS_DIM_CONJECTURE_CONFIDENCE,   /**< Conjecture confidence level */
    GENIUS_DIM_ANALOGY_STRENGTH,        /**< Cross-domain analogy strength */
    GENIUS_DIM_MATHEMATICAL_INTUITION,  /**< Mathematical intuition signal */
    GENIUS_DIM_CREATIVITY_LEVEL,        /**< Creative exploration level */
    GENIUS_DIM_RIGOR_LEVEL,             /**< Proof rigor level */
    GENIUS_DIM_GAUSS_ACTIVITY,          /**< Gauss mode activity */
    GENIUS_DIM_NEWTON_ACTIVITY,         /**< Newton mode activity */
    GENIUS_DIM_ERDOS_ACTIVITY,          /**< Erdos mode activity */
    GENIUS_DIM_INSIGHT_STRENGTH,        /**< Insight emergence strength */
    GENIUS_DIM_ELEGANCE_SIGNAL,         /**< Proof elegance signal */
    GENIUS_DIM_COUNT
} genius_snn_dimension_t;

/**
 * @brief Encoding methods for mathematical concepts
 */
typedef enum {
    GENIUS_SNN_ENCODE_RATE = 0,         /**< Rate coding of concepts */
    GENIUS_SNN_ENCODE_TEMPORAL,         /**< Temporal spike patterns */
    GENIUS_SNN_ENCODE_POPULATION,       /**< Population vector coding */
    GENIUS_SNN_ENCODE_PHASE             /**< Phase-based encoding */
} genius_snn_encoding_t;

/**
 * @brief Decoding methods for mathematical insights
 */
typedef enum {
    GENIUS_SNN_DECODE_THRESHOLD = 0,    /**< Threshold-based insight detection */
    GENIUS_SNN_DECODE_COMPETITION,      /**< Winner-take-all for mode selection */
    GENIUS_SNN_DECODE_INTEGRATION,      /**< Evidence accumulation */
    GENIUS_SNN_DECODE_SYNCHRONY         /**< Synchrony-based binding */
} genius_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    GENIUS_SNN_STATE_IDLE = 0,
    GENIUS_SNN_STATE_ENCODING,
    GENIUS_SNN_STATE_PROCESSING,
    GENIUS_SNN_STATE_INSIGHT_EMERGING,
    GENIUS_SNN_STATE_DECODING,
    GENIUS_SNN_STATE_ERROR
} genius_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Genius-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of genius dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */
    float insight_tau_ms;                /**< Insight emergence time constant */

    /* Encoding parameters */
    genius_snn_encoding_t encoding;      /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    genius_snn_decoding_t decoding;      /**< Decoding method */
    float insight_threshold;             /**< Threshold for insight detection */
    float elegance_threshold;            /**< Elegance signal threshold */
    float mode_switch_threshold;         /**< Threshold for mode switching */

    /* Network dynamics */
    bool enable_competition;             /**< Enable mode competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_insight_detection;       /**< Enable insight emergence detection */
    float insight_sensitivity;           /**< Insight detection sensitivity */

    /* Mode-specific processing */
    bool enable_gauss_circuits;          /**< Enable Gauss mode circuits */
    bool enable_newton_circuits;         /**< Enable Newton mode circuits */
    bool enable_erdos_circuits;          /**< Enable Erdos mode circuits */
    float mode_coupling_strength;        /**< Cross-mode coupling strength */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} genius_snn_config_t;

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
    float insight_contribution;          /**< Contribution to insight */
} genius_dim_state_t;

/**
 * @brief Mathematical insight output
 */
typedef struct {
    float insight_strength;              /**< Current insight strength [0-1] */
    float elegance_signal;               /**< Proof elegance signal [0-1] */
    float pattern_confidence;            /**< Pattern recognition confidence */
    float conjecture_strength;           /**< Conjecture confidence */
    genius_mode_t active_mode;           /**< Currently active genius mode */
    float gauss_activity;                /**< Gauss mode activity */
    float newton_activity;               /**< Newton mode activity */
    float erdos_activity;                /**< Erdos mode activity */
    bool insight_detected;               /**< Insight emergence detected */
    bool breakthrough_imminent;          /**< Major breakthrough imminent */
    float creativity_level;              /**< Current creativity level */
    float rigor_level;                   /**< Current rigor level */
} genius_insight_output_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    genius_snn_state_t state;            /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_insight;                  /**< Mean insight level */
    float mode_coherence;                /**< Mode coherence score */
    genius_mode_t dominant_mode;         /**< Dominant genius mode */
} genius_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t insights_detected;          /**< Insights detected */
    uint64_t breakthroughs;              /**< Major breakthroughs */
    uint64_t mode_switches;              /**< Mode switches */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_insight_strength;         /**< Mean insight strength */
    float mode_usage[GENIUS_MODE_COUNT]; /**< Mode usage distribution */
} genius_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct genius_snn_bridge genius_snn_bridge_t;

//=============================================================================
// Forward Declarations
//=============================================================================

struct mathematical_genius;
struct snn_network;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Insight emergence callback */
typedef void (*genius_snn_insight_callback_t)(
    genius_snn_bridge_t* bridge,
    float insight_strength,
    genius_mode_t mode,
    void* user_data
);

/** @brief Breakthrough detection callback */
typedef void (*genius_snn_breakthrough_callback_t)(
    genius_snn_bridge_t* bridge,
    const genius_insight_output_t* insight,
    void* user_data
);

/** @brief Mode switch callback */
typedef void (*genius_snn_mode_callback_t)(
    genius_snn_bridge_t* bridge,
    genius_mode_t old_mode,
    genius_mode_t new_mode,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
genius_snn_config_t genius_snn_config_default(void);

/**
 * @brief Create genius SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
genius_snn_bridge_t* genius_snn_create(const genius_snn_config_t* config);

/**
 * @brief Destroy genius SNN bridge
 * @param bridge Bridge to destroy
 */
void genius_snn_destroy(genius_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int genius_snn_reset(genius_snn_bridge_t* bridge);

/**
 * @brief Link to mathematical genius module
 * @param bridge Bridge handle
 * @param genius Mathematical genius handle
 * @return 0 on success, -1 on failure
 */
int genius_snn_link_genius(
    genius_snn_bridge_t* bridge,
    struct mathematical_genius* genius
);

/**
 * @brief Link to SNN network
 * @param bridge Bridge handle
 * @param snn SNN network handle
 * @return 0 on success, -1 on failure
 */
int genius_snn_link_snn(
    genius_snn_bridge_t* bridge,
    struct snn_network* snn
);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode genius state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int genius_snn_encode_state(
    genius_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode mathematical pattern
 * @param bridge Bridge handle
 * @param pattern_strength Pattern strength [0-1]
 * @param pattern_type Pattern type identifier
 * @return Spike count on success, -1 on failure
 */
int genius_snn_encode_pattern(
    genius_snn_bridge_t* bridge,
    float pattern_strength,
    uint32_t pattern_type
);

/**
 * @brief Encode proof progress
 * @param bridge Bridge handle
 * @param progress Proof progress [0-1]
 * @param elegance Elegance estimate [0-1]
 * @param depth Current proof depth
 * @return Spike count on success, -1 on failure
 */
int genius_snn_encode_proof_state(
    genius_snn_bridge_t* bridge,
    float progress,
    float elegance,
    uint32_t depth
);

/**
 * @brief Encode genius mode activation
 * @param bridge Bridge handle
 * @param mode Genius mode
 * @param activation Activation level [0-1]
 * @return Spike count on success, -1 on failure
 */
int genius_snn_encode_mode(
    genius_snn_bridge_t* bridge,
    genius_mode_t mode,
    float activation
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate mathematical processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int genius_snn_simulate(genius_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int genius_snn_step(genius_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int genius_snn_forward(
    genius_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get insight output from SNN activity
 * @param bridge Bridge handle
 * @param insight Output insight structure
 * @return 0 on success, -1 on failure
 */
int genius_snn_get_insight_output(
    genius_snn_bridge_t* bridge,
    genius_insight_output_t* insight
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int genius_snn_get_activations(
    genius_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for insight emergence
 * @param bridge Bridge handle
 * @param insight_level Output insight level
 * @return true if insight detected, false otherwise
 */
bool genius_snn_check_insight(
    genius_snn_bridge_t* bridge,
    float* insight_level
);

/**
 * @brief Get recommended genius mode from SNN activity
 * @param bridge Bridge handle
 * @param confidence Output confidence level
 * @return Recommended genius mode
 */
genius_mode_t genius_snn_recommend_mode(
    genius_snn_bridge_t* bridge,
    float* confidence
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
int genius_snn_get_dim_state(
    genius_snn_bridge_t* bridge,
    uint32_t dim,
    genius_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int genius_snn_get_state(
    genius_snn_bridge_t* bridge,
    genius_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int genius_snn_get_stats(genius_snn_bridge_t* bridge, genius_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int genius_snn_reset_stats(genius_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register insight emergence callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int genius_snn_register_insight_callback(
    genius_snn_bridge_t* bridge,
    genius_snn_insight_callback_t callback,
    void* user_data
);

/**
 * @brief Register breakthrough callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int genius_snn_register_breakthrough_callback(
    genius_snn_bridge_t* bridge,
    genius_snn_breakthrough_callback_t callback,
    void* user_data
);

/**
 * @brief Register mode switch callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int genius_snn_register_mode_callback(
    genius_snn_bridge_t* bridge,
    genius_snn_mode_callback_t callback,
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
int genius_snn_bio_async_connect(genius_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int genius_snn_bio_async_disconnect(genius_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool genius_snn_is_bio_async_connected(genius_snn_bridge_t* bridge);

//=============================================================================
// Heartbeat and State Serialization (Phase 8)
//=============================================================================

/** @brief Default heartbeat interval in milliseconds */
#define GENIUS_SNN_HEARTBEAT_INTERVAL_MS  1000

/** @brief Heartbeat timeout multiplier */
#define GENIUS_SNN_HEARTBEAT_TIMEOUT_MULT 3.0f

/**
 * @brief Serialized state structure for persistence/recovery
 */
typedef struct {
    uint32_t version;                    /**< Serialization version */
    uint32_t num_dimensions;             /**< Number of dimensions */
    genius_snn_bridge_state_t state;     /**< Bridge state snapshot */
    genius_snn_stats_t stats;            /**< Statistics snapshot */
    uint64_t timestamp_us;               /**< Serialization timestamp */
    uint32_t checksum;                   /**< Data integrity checksum */
} genius_snn_serialized_t;

/**
 * @brief Send heartbeat signal
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int genius_snn_send_heartbeat(genius_snn_bridge_t* bridge);

/**
 * @brief Get last heartbeat timestamp
 * @param bridge Bridge handle (logically const — mutex lock is an
 *               implementation detail)
 * @return Last heartbeat timestamp in microseconds, 0 on error
 */
uint64_t genius_snn_get_last_heartbeat(const genius_snn_bridge_t* bridge);

/**
 * @brief Check if heartbeat is stale
 * @param bridge Bridge handle
 * @param timeout_ms Timeout threshold in milliseconds
 * @return true if stale (timeout exceeded), false otherwise
 */
bool genius_snn_is_heartbeat_stale(
    const genius_snn_bridge_t* bridge,
    uint32_t timeout_ms
);

/**
 * @brief Serialize bridge state for persistence
 * @param bridge Bridge handle
 * @param serialized Output serialized state
 * @return 0 on success, -1 on failure
 */
int genius_snn_serialize_state(
    genius_snn_bridge_t* bridge,
    genius_snn_serialized_t* serialized
);

/**
 * @brief Deserialize and restore bridge state
 * @param bridge Bridge handle
 * @param serialized Serialized state to restore
 * @return 0 on success, -1 on failure
 */
int genius_snn_deserialize_state(
    genius_snn_bridge_t* bridge,
    const genius_snn_serialized_t* serialized
);

/**
 * @brief Compute checksum for state verification
 * @param serialized Serialized state
 * @return Computed checksum
 */
uint32_t genius_snn_compute_checksum(const genius_snn_serialized_t* serialized);

/**
 * @brief Verify serialized state integrity
 * @param serialized Serialized state to verify
 * @return true if checksum matches, false otherwise
 */
bool genius_snn_verify_checksum(const genius_snn_serialized_t* serialized);

//=============================================================================
// KG Wiring Integration
//=============================================================================

/* Forward declaration */
struct kg_module_wiring;

/**
 * @brief Create KG wiring descriptor for this bridge
 *
 * Creates a module wiring descriptor that enables brain self-awareness
 * of this bridge's topology, connections, inputs/outputs, and weights.
 *
 * @return Wiring descriptor or NULL on failure (caller owns memory)
 */
struct kg_module_wiring* genius_snn_create_kg_wiring(void);

/**
 * @brief Get KG wiring from bridge instance
 * @param bridge Bridge handle
 * @return Wiring descriptor or NULL if not initialized
 */
struct kg_module_wiring* genius_snn_get_kg_wiring(genius_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GENIUS_SNN_BRIDGE_H */
