/**
 * @file nimcp_social_snn_bridge.h
 * @brief Social Cognition - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between social cognition and spiking neural networks
 * WHY:  Enable biologically-plausible social bond encoding through population
 *       coding and spike-timing dynamics
 * HOW:  Encode social dimensions as spike patterns, decode relationship signals
 *       from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Dunbar (1998): Social brain hypothesis and group size constraints
 * - Adolphs (2003): Cognitive neuroscience of human social behavior
 * - Frith & Frith (2012): Mechanisms of social cognition
 *
 * BIOLOGICAL BASIS:
 * - Oxytocin system for bonding and trust
 * - Medial prefrontal cortex for social evaluation
 * - Temporoparietal junction for perspective taking
 * - Anterior cingulate cortex for social pain/pleasure
 * - Fusiform face area for social recognition
 *
 * INTEGRATION WITH SOCIAL BONDS:
 * - Trust level encoding through population variability
 * - Relationship strength via sustained activity patterns
 * - Cooperation/competition balance through lateral inhibition
 *
 * @see nimcp_love_loyalty_friendship.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_SOCIAL_SNN_BRIDGE_H
#define NIMCP_SOCIAL_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum social dimensions to encode */
#define SOCIAL_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per social dimension */
#define SOCIAL_SNN_NEURONS_PER_DIM    32

/** @brief Default trust threshold */
#define SOCIAL_SNN_TRUST_THRESH       0.5f

/** @brief Default encoding window (ms) */
#define SOCIAL_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_SOCIAL_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Social dimension types for SNN encoding
 */
typedef enum {
    SOCIAL_DIM_TRUST = 0,               /**< Trust level */
    SOCIAL_DIM_CLOSENESS,               /**< Relationship closeness */
    SOCIAL_DIM_AFFECTION,               /**< Warmth and fondness */
    SOCIAL_DIM_RECIPROCITY,             /**< Give/take balance */
    SOCIAL_DIM_LOYALTY,                 /**< Commitment strength */
    SOCIAL_DIM_COOPERATION,             /**< Cooperation tendency */
    SOCIAL_DIM_COMPETITION,             /**< Competition tendency */
    SOCIAL_DIM_HIERARCHY,               /**< Social rank awareness */
    SOCIAL_DIM_BONDING,                 /**< Oxytocin-mediated bonding */
    SOCIAL_DIM_EMPATHY,                 /**< Empathic resonance */
    SOCIAL_DIM_COUNT
} social_snn_dimension_t;

/**
 * @brief Encoding methods for social contexts
 */
typedef enum {
    SOCIAL_SNN_ENCODE_RATE = 0,         /**< Rate coding of dimensions */
    SOCIAL_SNN_ENCODE_TEMPORAL,         /**< Temporal spike patterns */
    SOCIAL_SNN_ENCODE_POPULATION,       /**< Population vector coding */
    SOCIAL_SNN_ENCODE_SYNCHRONY         /**< Synchrony-based encoding */
} social_snn_encoding_t;

/**
 * @brief Decoding methods for relationship states
 */
typedef enum {
    SOCIAL_SNN_DECODE_THRESHOLD = 0,    /**< Threshold-based detection */
    SOCIAL_SNN_DECODE_COMPETITION,      /**< Winner-take-all */
    SOCIAL_SNN_DECODE_SOFTMAX,          /**< Soft probabilistic */
    SOCIAL_SNN_DECODE_INTEGRATION       /**< Evidence accumulation */
} social_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SOCIAL_SNN_STATE_IDLE = 0,
    SOCIAL_SNN_STATE_ENCODING,
    SOCIAL_SNN_STATE_PROCESSING,
    SOCIAL_SNN_STATE_DECODING,
    SOCIAL_SNN_STATE_SIMULATING,
    SOCIAL_SNN_STATE_ERROR
} social_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Social-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of social dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    social_snn_encoding_t encoding;      /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    social_snn_decoding_t decoding;      /**< Decoding method */
    float trust_threshold;               /**< Threshold for trust detection */
    float bonding_threshold;             /**< Minimum bonding strength */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_hierarchy_processing;    /**< Enable social hierarchy */
    float hierarchy_sensitivity;         /**< Hierarchy detection sensitivity */

    /* Social integration */
    bool enable_bonding;                 /**< Enable bonding circuits */
    float bonding_gain;                  /**< Bonding signal gain */
    bool enable_cooperation_balance;     /**< Enable cooperation/competition */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} social_snn_config_t;

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
} social_dim_state_t;

/**
 * @brief Social relationship output
 */
typedef struct {
    float trust_level;                   /**< Current trust [0-1] */
    float closeness_level;               /**< Current closeness [0-1] */
    float affection_level;               /**< Current affection [0-1] */
    float bonding_strength;              /**< Bonding strength */
    float hierarchy_position;            /**< Perceived hierarchy position */
    bool high_trust;                     /**< High trust detected */
    bool strong_bond;                    /**< Strong bond state */
    float bond_magnitude;                /**< Bond magnitude if detected */
    float cooperation_level;             /**< Cooperation tendency */
    float competition_level;             /**< Competition tendency */
} social_relationship_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    social_snn_state_t state;            /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_bonding;                  /**< Mean bonding strength */
    float trust_signal;                  /**< Current trust signal */
    float cooperation_signal;            /**< Current cooperation signal */
} social_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t trust_detections;           /**< High trust detections */
    uint64_t strong_bond_events;         /**< Strong bond events */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_bonding;                  /**< Mean bonding strength */
} social_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct social_snn_bridge social_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Trust detection callback */
typedef void (*social_snn_trust_callback_t)(
    social_snn_bridge_t* bridge,
    float trust_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Relationship ready callback */
typedef void (*social_snn_relationship_callback_t)(
    social_snn_bridge_t* bridge,
    const social_relationship_t* relationship,
    void* user_data
);

/** @brief Strong bond callback */
typedef void (*social_snn_bond_callback_t)(
    social_snn_bridge_t* bridge,
    float bond_level,
    uint32_t bond_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
social_snn_config_t social_snn_config_default(void);

/**
 * @brief Create social SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
social_snn_bridge_t* social_snn_create(const social_snn_config_t* config);

/**
 * @brief Destroy social SNN bridge
 * @param bridge Bridge to destroy
 */
void social_snn_destroy(social_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int social_snn_reset(social_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode social state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int social_snn_encode_state(
    social_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode trust level
 * @param bridge Bridge handle
 * @param trust Trust level [0-1]
 * @param reliability Reliability measure [0-1]
 * @return Spike count on success, -1 on failure
 */
int social_snn_encode_trust(
    social_snn_bridge_t* bridge,
    float trust,
    float reliability
);

/**
 * @brief Encode relationship closeness
 * @param bridge Bridge handle
 * @param closeness Closeness level [0-1]
 * @param affection Affection level [0-1]
 * @return Spike count on success, -1 on failure
 */
int social_snn_encode_closeness(
    social_snn_bridge_t* bridge,
    float closeness,
    float affection
);

/**
 * @brief Encode social hierarchy
 * @param bridge Bridge handle
 * @param hierarchy_position Position in hierarchy [0-1]
 * @param hierarchy_count Number of hierarchy levels
 * @return Spike count on success, -1 on failure
 */
int social_snn_encode_hierarchy(
    social_snn_bridge_t* bridge,
    float hierarchy_position,
    uint32_t hierarchy_count
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate social processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int social_snn_simulate(social_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int social_snn_step(social_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int social_snn_forward(
    social_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get relationship state from SNN activity
 * @param bridge Bridge handle
 * @param relationship Output relationship structure
 * @return 0 on success, -1 on failure
 */
int social_snn_get_relationship(
    social_snn_bridge_t* bridge,
    social_relationship_t* relationship
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int social_snn_get_activations(
    social_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for high trust
 * @param bridge Bridge handle
 * @param trust_level Output trust level
 * @return true if high trust detected, false otherwise
 */
bool social_snn_check_trust(
    social_snn_bridge_t* bridge,
    float* trust_level
);

/**
 * @brief Check for strong bond
 * @param bridge Bridge handle
 * @param bond_level Output bond level
 * @return true if strong bond detected, false otherwise
 */
bool social_snn_check_bond(
    social_snn_bridge_t* bridge,
    float* bond_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool social_snn_check_state_change(
    social_snn_bridge_t* bridge,
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
int social_snn_get_dim_state(
    social_snn_bridge_t* bridge,
    uint32_t dim,
    social_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int social_snn_get_state(
    social_snn_bridge_t* bridge,
    social_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int social_snn_get_stats(social_snn_bridge_t* bridge, social_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int social_snn_reset_stats(social_snn_bridge_t* bridge);

/**
 * @brief Get current bonding strength
 * @param bridge Bridge handle
 * @return Bonding strength [0-1], -1 on error
 */
float social_snn_get_bonding(social_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float social_snn_get_total_activity(social_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register trust detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int social_snn_register_trust_callback(
    social_snn_bridge_t* bridge,
    social_snn_trust_callback_t callback,
    void* user_data
);

/**
 * @brief Register relationship callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int social_snn_register_relationship_callback(
    social_snn_bridge_t* bridge,
    social_snn_relationship_callback_t callback,
    void* user_data
);

/**
 * @brief Register strong bond callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int social_snn_register_bond_callback(
    social_snn_bridge_t* bridge,
    social_snn_bond_callback_t callback,
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
int social_snn_bio_async_connect(social_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int social_snn_bio_async_disconnect(social_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool social_snn_is_bio_async_connected(social_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOCIAL_SNN_BRIDGE_H */
