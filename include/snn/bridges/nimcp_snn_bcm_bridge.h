/**
 * @file nimcp_snn_bcm_bridge.h
 * @brief SNN-BCM Plasticity Integration Bridge
 *
 * WHAT: Bidirectional integration between SNN and BCM (Bienenstock-Cooper-Munro) plasticity
 * WHY:  Coordinate sliding threshold plasticity across SNN and BCM systems
 * HOW:  Share spike rates, sliding thresholds, and rate-dependent LTP/LTD selection
 *
 * BIOLOGICAL BASIS:
 * - BCM theory models visual cortex development (Bienenstock et al., 1982)
 * - Sliding modification threshold based on average activity squared
 * - LTP when post > threshold, LTD when post < threshold
 * - Self-stabilizing without explicit normalization
 * - Winner-take-all dynamics emerge naturally
 *
 * INTEGRATION:
 * - SNN provides spike-based activity estimates
 * - BCM module provides sliding threshold computation
 * - Bridge converts spike rates to activity levels for BCM rule
 * - Bio-async for threshold updates
 *
 * @author NIMCP Team
 * @date 2024
 */

#ifndef NIMCP_SNN_BCM_BRIDGE_H
#define NIMCP_SNN_BCM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief SNN-BCM bridge configuration
 *
 * WHAT: Parameters for coordinating SNN and BCM plasticity
 * WHY:  Control how spike rates affect BCM threshold and plasticity
 * HOW:  Rate estimation, threshold coordination, update intervals
 */
typedef struct snn_bcm_bridge_config_s {
    /* Spike rate estimation */
    float rate_window_ms;           /**< Time window for rate estimation (ms) */
    float rate_tau_ms;              /**< Time constant for exponential averaging */
    float min_rate_hz;              /**< Minimum firing rate (Hz) */
    float max_rate_hz;              /**< Maximum firing rate (Hz) */

    /* Threshold coordination */
    bool sync_thresholds;           /**< Synchronize BCM thresholds */
    float threshold_update_interval_ms; /**< How often to update thresholds */
    float min_threshold;            /**< Minimum modification threshold */
    float max_threshold;            /**< Maximum modification threshold */

    /* Learning rate coordination */
    float lr_scaling_factor;        /**< Scale BCM LR for SNN (default 1.0) */
    bool rate_dependent_lr;         /**< Adjust LR based on spike rate */

    /* Weight update control */
    float weight_update_interval_ms; /**< How often to sync weight changes */
    bool bidirectional_updates;     /**< SNN ↔ BCM or SNN → BCM only */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
    uint32_t event_buffer_size;     /**< Event buffer size */
} snn_bcm_bridge_config_t;

/**
 * @brief BCM effects applied to SNN
 *
 * WHAT: Computed effects of BCM on SNN learning
 * WHY:  Cache computed modulations for efficiency
 * HOW:  Updated when BCM parameters change
 */
typedef struct snn_bcm_effects_s {
    float effective_learning_rate;  /**< Effective learning rate */
    float avg_threshold;            /**< Average modification threshold */
    float threshold_range;          /**< Threshold variance across synapses */
    float avg_activity;             /**< Average post-synaptic activity */
    bool ltp_dominant;              /**< Whether LTP or LTD dominates */
} snn_bcm_effects_t;

/**
 * @brief Spike rate history for BCM computation
 *
 * WHAT: Estimated firing rates for BCM activity
 * WHY:  Convert spikes to continuous activity for BCM rule
 * HOW:  Exponential moving average of spike counts
 */
typedef struct spike_rate_history_s {
    uint32_t neuron_id;             /**< Neuron ID */
    float instantaneous_rate;       /**< Current rate estimate (Hz) */
    float averaged_rate;            /**< Exponentially averaged rate */
    float activity_squared_avg;     /**< Average of squared activity */
    uint64_t last_spike_time_us;    /**< Last spike timestamp */
    uint32_t spike_count;           /**< Spike count in current window */
} spike_rate_history_t;

/**
 * @brief SNN-BCM bridge structure
 *
 * WHAT: Context for SNN-BCM integration
 * WHY:  Maintain state of bidirectional bridge
 * HOW:  Store references and cached effects
 */
typedef struct snn_bcm_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* network;             /**< SNN network */
    bcm_synapse_t* bcm_synapses;        /**< BCM synapse array */
    uint32_t n_synapses;                /**< Number of synapses */
    uint32_t n_neurons;                 /**< Number of neurons */

    snn_bcm_bridge_config_t config;     /**< Bridge configuration */
    snn_bcm_effects_t effects;          /**< Current BCM effects */

    /* Rate tracking */
    spike_rate_history_t* rate_history; /**< Rate history per neuron */

    /* State */
    bool connected;                     /**< Bridge active */
    float last_update_time_ms;          /**< Last update timestamp */
    uint32_t plasticity_events;         /**< Number of plasticity events */
    uint32_t threshold_updates;         /**< Number of threshold updates */

    /* Bio-async */
    bool bio_async_enabled;             /**< Bio-async connected */
    bio_module_context_t bio_ctx;       /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_bcm_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize bridge config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from BCM literature (visual cortex)
 *
 * @param config Config to initialize
 */
void snn_bcm_bridge_config_default(snn_bcm_bridge_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-BCM bridge
 *
 * WHAT: Initialize bidirectional bridge
 * WHY:  Enable coordinated BCM plasticity across SNN and BCM systems
 * HOW:  Allocate context, set up connections
 *
 * @param config Bridge configuration
 * @param network SNN network to integrate
 * @param bcm_synapses BCM synapse array
 * @param n_synapses Number of synapses
 * @param n_neurons Number of neurons
 * @return Bridge instance or NULL on failure
 */
snn_bcm_bridge_t* snn_bcm_bridge_create(
    const snn_bcm_bridge_config_t* config,
    snn_network_t* network,
    bcm_synapse_t* bcm_synapses,
    uint32_t n_synapses,
    uint32_t n_neurons
);

/**
 * @brief Destroy SNN-BCM bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect and free
 *
 * @param bridge Bridge to destroy
 */
void snn_bcm_bridge_destroy(snn_bcm_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging for threshold updates
 * WHY:  Distributed coordination of plasticity
 * HOW:  Register with bio-router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_bcm_bridge_connect_bio_async(snn_bcm_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_bcm_bridge_disconnect_bio_async(snn_bcm_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_bcm_bridge_is_bio_async_connected(const snn_bcm_bridge_t* bridge);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update spike rate estimates
 *
 * WHAT: Estimate firing rates from spike trains
 * WHY:  BCM needs continuous activity levels, not discrete spikes
 * HOW:  Exponential moving average of spike counts
 *
 * @param bridge Bridge to update
 * @param dt Time step (ms)
 * @return 0 on success, error code on failure
 */
int snn_bcm_bridge_update_rates(snn_bcm_bridge_t* bridge, float dt);

/**
 * @brief Update BCM sliding thresholds
 *
 * WHAT: Update modification thresholds based on activity history
 * WHY:  BCM threshold should track <r^2>
 * HOW:  Use spike rate estimates to update BCM synapse thresholds
 *
 * @param bridge Bridge to update
 * @param dt Time step (ms)
 * @return 0 on success, error code on failure
 */
int snn_bcm_bridge_update_thresholds(snn_bcm_bridge_t* bridge, float dt);

/**
 * @brief Apply BCM plasticity to SNN synapses
 *
 * WHAT: Update SNN synapse weights based on BCM rule
 * WHY:  Implement BCM learning in SNN
 * HOW:  Use spike rates and BCM thresholds
 *
 * @param bridge Bridge with BCM parameters
 * @param dt Time step (ms)
 * @return 0 on success, error code on failure
 */
int snn_bcm_bridge_apply_plasticity(snn_bcm_bridge_t* bridge, float dt);

/**
 * @brief Full update cycle
 *
 * WHAT: Update rates, thresholds, and apply plasticity
 * WHY:  Single call for regular updates
 * HOW:  Combines all update steps
 *
 * @param bridge Bridge to update
 * @param dt Time step (ms)
 * @return 0 on success, error code on failure
 */
int snn_bcm_bridge_update(snn_bcm_bridge_t* bridge, float dt);

//=============================================================================
// Weight Change Functions
//=============================================================================

/**
 * @brief Get weight changes from BCM plasticity
 *
 * WHAT: Retrieve pending weight changes
 * WHY:  Apply BCM-computed weight changes to SNN
 * HOW:  Query BCM synapses for weight deltas
 *
 * @param bridge Bridge to query
 * @param synapse_ids Output array of synapse IDs (caller allocates)
 * @param weight_deltas Output array of weight changes (caller allocates)
 * @param max_changes Maximum changes to return
 * @param n_changes Output: number of changes returned
 * @return 0 on success
 */
int snn_bcm_bridge_get_weight_changes(
    const snn_bcm_bridge_t* bridge,
    uint32_t* synapse_ids,
    float* weight_deltas,
    uint32_t max_changes,
    uint32_t* n_changes
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current BCM effects
 *
 * @param bridge Bridge to query
 * @param effects Output for effects (copied)
 * @return 0 on success
 */
int snn_bcm_bridge_get_effects(
    const snn_bcm_bridge_t* bridge,
    snn_bcm_effects_t* effects
);

/**
 * @brief Get average modification threshold
 *
 * @param bridge Bridge to query
 * @return Average threshold across synapses
 */
float snn_bcm_bridge_get_avg_threshold(const snn_bcm_bridge_t* bridge);

/**
 * @brief Get neuron firing rate estimate
 *
 * @param bridge Bridge to query
 * @param neuron_id Neuron to query
 * @return Estimated firing rate (Hz) or -1 on error
 */
float snn_bcm_bridge_get_neuron_rate(const snn_bcm_bridge_t* bridge, uint32_t neuron_id);

/**
 * @brief Check if LTP is dominant
 *
 * @param bridge Bridge to query
 * @return true if LTP > LTD on average
 */
bool snn_bcm_bridge_is_ltp_dominant(const snn_bcm_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param plasticity_events Output: total plasticity events
 * @param threshold_updates Output: threshold update count
 * @param updates Output: update cycles completed
 * @return 0 on success
 */
int snn_bcm_bridge_get_stats(
    const snn_bcm_bridge_t* bridge,
    uint32_t* plasticity_events,
    uint32_t* threshold_updates,
    uint32_t* updates
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge to reset
 */
void snn_bcm_bridge_reset_stats(snn_bcm_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_BCM_BRIDGE_H */
