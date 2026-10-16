/**
 * @file nimcp_working_memory_snn_bridge.h
 * @brief Working Memory - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between working memory and spiking neural networks
 * WHY:  Enable biologically-plausible memory maintenance through sustained activity
 * HOW:  Encode memory items as spike patterns, decode from population activity
 *
 * THEORETICAL FOUNDATIONS:
 * - Goldman-Rakic (1995): Cellular basis of working memory
 * - Wang (2001): Synaptic reverberation underlying mnemonic persistent activity
 * - Compte et al. (2000): Bump attractor networks for working memory
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex maintains information through sustained firing
 * - Recurrent excitation and balanced inhibition create persistent activity
 * - Memory capacity is limited by inhibitory network stability
 * - Items compete for representation through lateral inhibition
 *
 * @see nimcp_working_memory.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_WORKING_MEMORY_SNN_BRIDGE_H
#define NIMCP_WORKING_MEMORY_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum memory slots */
#define WM_SNN_MAX_SLOTS                 16

/** @brief Neurons per memory slot */
#define WM_SNN_NEURONS_PER_SLOT          32

/** @brief Default memory slot dimension */
#define WM_SNN_SLOT_DIM                  64

/** @brief Default hidden layer dimension */
#define WM_SNN_HIDDEN_DIM                128

/** @brief Bio-async module ID */
#define BIO_MODULE_WM_SNN                0x0D20

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Memory item encoding methods
 */
typedef enum {
    WM_SNN_ENCODE_RATE = 0,       /**< Rate coding of item features */
    WM_SNN_ENCODE_TEMPORAL,        /**< Temporal spike patterns */
    WM_SNN_ENCODE_POPULATION,      /**< Population vector coding */
    WM_SNN_ENCODE_SPARSE           /**< Sparse distributed representation */
} wm_snn_encoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    WM_SNN_STATE_IDLE = 0,
    WM_SNN_STATE_ENCODING,
    WM_SNN_STATE_MAINTAINING,
    WM_SNN_STATE_RETRIEVING,
    WM_SNN_STATE_SIMULATING,
    WM_SNN_STATE_ERROR
} wm_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Working Memory-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t max_slots;              /**< Maximum memory slots */
    uint32_t neurons_per_slot;       /**< Neurons per memory slot */
    uint32_t slot_dim;               /**< Feature dimension per slot */
    uint32_t hidden_dim;             /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                     /**< Simulation timestep (ms) */
    float maintenance_rate;          /**< Baseline maintenance firing rate */
    float decay_tau_ms;              /**< Decay time constant */

    /* Encoding parameters */
    wm_snn_encoding_t encoding_type; /**< Encoding method */
    float encoding_gain;             /**< Encoding strength gain */
    float noise_stddev;              /**< Input noise level */

    /* Network dynamics */
    bool enable_lateral_inhibition;  /**< Enable item competition */
    float inhibition_strength;       /**< Lateral inhibition weight */
    bool enable_recurrence;          /**< Enable recurrent connections */
    float recurrence_strength;       /**< Recurrent connection weight */

    /* Bio-async integration */
    bool enable_bio_async;           /**< Enable bio-async callbacks */
} wm_snn_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Per-slot state tracking
 */
typedef struct {
    bool occupied;                   /**< Slot has active item */
    float activity_level;            /**< Current population activity */
    float persistence;               /**< How long item has persisted */
    float salience;                  /**< Item salience/priority */
    uint64_t encode_time;            /**< When item was encoded */
    uint32_t retrieval_count;        /**< Number of retrievals */
} wm_slot_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    wm_snn_state_t state;            /**< Current operational state */
    uint32_t active_slots;           /**< Number of occupied slots */
    float total_activity;            /**< Total network activity */
    float mean_persistence;          /**< Mean item persistence */
    float capacity_used;             /**< Fraction of capacity used */
} wm_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_encodings;        /**< Total encoding operations */
    uint64_t total_retrievals;       /**< Total retrieval operations */
    uint64_t total_simulations;      /**< Total simulation steps */
    uint64_t total_spikes;           /**< Total spikes generated */
    uint64_t evictions;              /**< Items evicted due to capacity */
    uint64_t successful_retrievals;  /**< Successful retrievals */
    float mean_encoding_spikes;      /**< Mean spikes per encoding */
    float mean_retrieval_accuracy;   /**< Mean retrieval accuracy */
} wm_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct wm_snn_bridge wm_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Spike activity callback */
typedef void (*wm_snn_spike_callback_t)(
    wm_snn_bridge_t* bridge,
    uint32_t slot,
    float rate,
    void* user_data
);

/** @brief Encoding callback */
typedef void (*wm_snn_encoding_callback_t)(
    wm_snn_bridge_t* bridge,
    uint32_t slot,
    int spike_count,
    void* user_data
);

/** @brief Retrieval callback */
typedef void (*wm_snn_retrieval_callback_t)(
    wm_snn_bridge_t* bridge,
    uint32_t slot,
    float confidence,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
wm_snn_config_t wm_snn_config_default(void);

/**
 * @brief Create working memory SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
wm_snn_bridge_t* wm_snn_create(const wm_snn_config_t* config);

/**
 * @brief Destroy working memory SNN bridge
 * @param bridge Bridge to destroy
 */
void wm_snn_destroy(wm_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int wm_snn_reset(wm_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode item into memory slot
 * @param bridge Bridge handle
 * @param slot Target slot index
 * @param features Item feature vector
 * @param feature_count Number of features
 * @param salience Item salience/priority
 * @return Spike count on success, -1 on failure
 */
int wm_snn_encode_item(
    wm_snn_bridge_t* bridge,
    uint32_t slot,
    const float* features,
    uint32_t feature_count,
    float salience
);

/**
 * @brief Update existing item in slot
 * @param bridge Bridge handle
 * @param slot Slot index
 * @param features Updated features (NULL to keep existing)
 * @param feature_count Feature count
 * @return 0 on success, -1 on failure
 */
int wm_snn_update_item(
    wm_snn_bridge_t* bridge,
    uint32_t slot,
    const float* features,
    uint32_t feature_count
);

/**
 * @brief Clear memory slot
 * @param bridge Bridge handle
 * @param slot Slot to clear
 * @return 0 on success, -1 on failure
 */
int wm_snn_clear_slot(wm_snn_bridge_t* bridge, uint32_t slot);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate maintenance activity
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int wm_snn_simulate(wm_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int wm_snn_step(wm_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input feature vector
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int wm_snn_forward(
    wm_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Retrieval Functions
//=============================================================================

/**
 * @brief Retrieve item from slot
 * @param bridge Bridge handle
 * @param slot Slot to retrieve from
 * @param output Output feature buffer
 * @param output_size Output buffer size
 * @return 0 on success, -1 on failure
 */
int wm_snn_retrieve_item(
    wm_snn_bridge_t* bridge,
    uint32_t slot,
    float* output,
    uint32_t output_size
);

/**
 * @brief Get slot activity levels
 * @param bridge Bridge handle
 * @param activities Output buffer for activity levels
 * @param slot_count Number of slots to query
 * @return 0 on success, -1 on failure
 */
int wm_snn_get_slot_activities(
    wm_snn_bridge_t* bridge,
    float* activities,
    uint32_t slot_count
);

/**
 * @brief Get most active slot
 * @param bridge Bridge handle
 * @param confidence Output confidence level
 * @return Slot index, -1 if none active
 */
int wm_snn_get_most_active_slot(
    wm_snn_bridge_t* bridge,
    float* confidence
);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get slot state
 * @param bridge Bridge handle
 * @param slot Slot index
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int wm_snn_get_slot_state(
    wm_snn_bridge_t* bridge,
    uint32_t slot,
    wm_slot_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int wm_snn_get_state(
    wm_snn_bridge_t* bridge,
    wm_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int wm_snn_get_stats(wm_snn_bridge_t* bridge, wm_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int wm_snn_reset_stats(wm_snn_bridge_t* bridge);

/**
 * @brief Get capacity utilization
 * @param bridge Bridge handle
 * @return Capacity fraction (0-1), -1 on error
 */
float wm_snn_get_capacity(wm_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float wm_snn_get_total_activity(wm_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register spike callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int wm_snn_register_spike_callback(
    wm_snn_bridge_t* bridge,
    wm_snn_spike_callback_t callback,
    void* user_data
);

/**
 * @brief Register encoding callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int wm_snn_register_encoding_callback(
    wm_snn_bridge_t* bridge,
    wm_snn_encoding_callback_t callback,
    void* user_data
);

/**
 * @brief Register retrieval callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int wm_snn_register_retrieval_callback(
    wm_snn_bridge_t* bridge,
    wm_snn_retrieval_callback_t callback,
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
int wm_snn_bio_async_connect(wm_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int wm_snn_bio_async_disconnect(wm_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool wm_snn_is_bio_async_connected(wm_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORKING_MEMORY_SNN_BRIDGE_H */
