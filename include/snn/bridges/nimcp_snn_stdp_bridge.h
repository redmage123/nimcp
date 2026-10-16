/**
 * @file nimcp_snn_stdp_bridge.h
 * @brief SNN-STDP Plasticity Integration Bridge
 *
 * WHAT: Bidirectional integration between SNN and STDP plasticity module
 * WHY:  Coordinate spike-timing dependent plasticity across SNN and plasticity systems
 * HOW:  Share timing windows, learning rates, and spike events via bio-async
 *
 * BIOLOGICAL BASIS:
 * - STDP is the primary learning mechanism in spiking networks
 * - Pre-before-post timing → LTP (Bi & Poo, 1998)
 * - Post-before-pre timing → LTD
 * - Timing windows typically 20ms for LTP, 20ms for LTD
 * - Dopamine modulation gates plasticity (three-factor learning)
 *
 * INTEGRATION:
 * - SNN provides spike timing information
 * - STDP module provides plasticity parameters and modulation
 * - Bridge synchronizes weight updates
 * - Bio-async for spike event notification
 *
 * @author NIMCP Team
 * @date 2024
 */

#ifndef NIMCP_SNN_STDP_BRIDGE_H
#define NIMCP_SNN_STDP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief SNN-STDP bridge configuration
 *
 * WHAT: Parameters for coordinating SNN and STDP
 * WHY:  Control how spike timing affects synaptic plasticity
 * HOW:  Timing windows, learning rate coordination, update intervals
 */
typedef struct snn_stdp_bridge_config_s {
    /* Timing window coordination */
    float ltp_window_ms;            /**< LTP timing window (ms) - default 20ms */
    float ltd_window_ms;            /**< LTD timing window (ms) - default 20ms */
    float min_spike_interval_ms;    /**< Minimum interval between spikes (ms) */

    /* Learning rate coordination */
    bool sync_learning_rates;       /**< Synchronize LR between SNN and STDP */
    float lr_scaling_factor;        /**< Scale STDP LR for SNN (default 1.0) */

    /* Weight update control */
    float weight_update_interval_ms; /**< How often to sync weight changes */
    float max_weight_change_per_step; /**< Limit weight change magnitude */
    bool bidirectional_updates;     /**< SNN ↔ STDP or SNN → STDP only */

    /* Dopamine modulation */
    bool enable_da_modulation;      /**< Use dopamine from STDP module */
    float da_threshold;             /**< Minimum DA for plasticity */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
    uint32_t spike_buffer_size;     /**< Spike event buffer size */
} snn_stdp_bridge_config_t;

/**
 * @brief STDP effects applied to SNN
 *
 * WHAT: Computed effects of STDP on SNN learning
 * WHY:  Cache computed modulations for efficiency
 * HOW:  Updated when STDP parameters change
 */
typedef struct snn_stdp_effects_s {
    float effective_a_plus;         /**< Effective LTP amplitude */
    float effective_a_minus;        /**< Effective LTD amplitude */
    float effective_tau_plus;       /**< Effective LTP time constant */
    float effective_tau_minus;      /**< Effective LTD time constant */
    float da_modulation_factor;     /**< Current dopamine modulation [0, 10+] */
    float learning_rate_factor;     /**< LR factor from STDP module */
} snn_stdp_effects_t;

/**
 * @brief Weight change record
 *
 * WHAT: Record of a synaptic weight change from STDP
 * WHY:  Track changes for synchronization
 * HOW:  Circular buffer of changes
 */
typedef struct weight_change_record_s {
    uint32_t synapse_id;            /**< Synapse that changed */
    float delta_weight;             /**< Weight change magnitude */
    uint64_t timestamp_us;          /**< When change occurred */
    bool applied_to_snn;            /**< Whether synced to SNN */
} weight_change_record_t;

/**
 * @brief SNN-STDP bridge structure
 *
 * WHAT: Context for SNN-STDP integration
 * WHY:  Maintain state of bidirectional bridge
 * HOW:  Store references and cached effects
 */
typedef struct snn_stdp_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* network;             /**< SNN network */
    stdp_synapse_t* stdp_synapses;      /**< STDP synapse array */
    uint32_t n_synapses;                /**< Number of synapses */

    snn_stdp_bridge_config_t config;    /**< Bridge configuration */
    snn_stdp_effects_t effects;         /**< Current STDP effects */

    /* Weight change tracking */
    weight_change_record_t* weight_changes; /**< Circular buffer */
    uint32_t weight_change_capacity;    /**< Buffer capacity */
    uint32_t weight_change_count;       /**< Current count */
    uint32_t weight_change_write_idx;   /**< Write index */

    /* State */
    bool connected;                     /**< Bridge active */
    float last_update_time_ms;          /**< Last update timestamp */
    uint32_t plasticity_events;         /**< Number of plasticity events */
    uint32_t weight_syncs;              /**< Number of weight syncs */

    /* Bio-async */
    bool bio_async_enabled;             /**< Bio-async connected */
    bio_module_context_t bio_ctx;       /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_stdp_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize bridge config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from STDP literature (Bi & Poo, 1998)
 *
 * @param config Config to initialize
 */
void snn_stdp_bridge_config_default(snn_stdp_bridge_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-STDP bridge
 *
 * WHAT: Initialize bidirectional bridge
 * WHY:  Enable coordinated STDP across SNN and plasticity systems
 * HOW:  Allocate context, set up connections
 *
 * @param config Bridge configuration
 * @param network SNN network to integrate
 * @param stdp_synapses STDP synapse array
 * @param n_synapses Number of synapses
 * @return Bridge instance or NULL on failure
 */
snn_stdp_bridge_t* snn_stdp_bridge_create(
    const snn_stdp_bridge_config_t* config,
    snn_network_t* network,
    stdp_synapse_t* stdp_synapses,
    uint32_t n_synapses
);

/**
 * @brief Destroy SNN-STDP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect and free
 *
 * @param bridge Bridge to destroy
 */
void snn_stdp_bridge_destroy(snn_stdp_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging for spike events
 * WHY:  Distributed coordination of plasticity
 * HOW:  Register with bio-router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_stdp_bridge_connect_bio_async(snn_stdp_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_stdp_bridge_disconnect_bio_async(snn_stdp_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_stdp_bridge_is_bio_async_connected(const snn_stdp_bridge_t* bridge);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update STDP effects from plasticity module
 *
 * WHAT: Get current STDP parameters and compute effects
 * WHY:  Keep SNN plasticity current with STDP module state
 * HOW:  Query STDP synapses, compute effective parameters
 *
 * @param bridge Bridge to update
 * @return 0 on success, error code on failure
 */
int snn_stdp_bridge_update_effects(snn_stdp_bridge_t* bridge);

/**
 * @brief Apply STDP plasticity to SNN synapses
 *
 * WHAT: Update SNN synapse weights based on spike timing
 * WHY:  Implement STDP learning in SNN
 * HOW:  Use spike trains and STDP parameters
 *
 * @param bridge Bridge with STDP parameters
 * @param dt Time step (ms)
 * @return 0 on success, error code on failure
 */
int snn_stdp_bridge_apply_plasticity(snn_stdp_bridge_t* bridge, float dt);

/**
 * @brief Full update cycle
 *
 * WHAT: Update effects and apply plasticity
 * WHY:  Single call for regular updates
 * HOW:  Combines effect update and weight changes
 *
 * @param bridge Bridge to update
 * @param dt Time step (ms)
 * @return 0 on success, error code on failure
 */
int snn_stdp_bridge_update(snn_stdp_bridge_t* bridge, float dt);

//=============================================================================
// Weight Synchronization
//=============================================================================

/**
 * @brief Get weight changes since last sync
 *
 * WHAT: Retrieve pending weight changes
 * WHY:  Synchronize weights between SNN and STDP
 * HOW:  Return records from circular buffer
 *
 * @param bridge Bridge to query
 * @param changes Output array (caller allocates)
 * @param max_changes Maximum changes to return
 * @param n_changes Output: number of changes returned
 * @return 0 on success
 */
int snn_stdp_bridge_get_weight_changes(
    const snn_stdp_bridge_t* bridge,
    weight_change_record_t* changes,
    uint32_t max_changes,
    uint32_t* n_changes
);

/**
 * @brief Mark weight changes as synced
 *
 * WHAT: Mark changes as applied to SNN
 * WHY:  Prevent duplicate application
 * HOW:  Update applied_to_snn flags
 *
 * @param bridge Bridge to update
 * @param n_synced Number of changes synced
 * @return 0 on success
 */
int snn_stdp_bridge_mark_synced(snn_stdp_bridge_t* bridge, uint32_t n_synced);

/**
 * @brief Record a weight change
 *
 * WHAT: Add weight change to tracking buffer
 * WHY:  Track for synchronization
 * HOW:  Add to circular buffer
 *
 * @param bridge Bridge to update
 * @param synapse_id Synapse that changed
 * @param delta_weight Weight change magnitude
 * @return 0 on success
 */
int snn_stdp_bridge_record_weight_change(
    snn_stdp_bridge_t* bridge,
    uint32_t synapse_id,
    float delta_weight
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current STDP effects
 *
 * @param bridge Bridge to query
 * @param effects Output for effects (copied)
 * @return 0 on success
 */
int snn_stdp_bridge_get_effects(
    const snn_stdp_bridge_t* bridge,
    snn_stdp_effects_t* effects
);

/**
 * @brief Get effective LTP amplitude
 *
 * @param bridge Bridge to query
 * @return Effective a_plus (modulated by dopamine)
 */
float snn_stdp_bridge_get_effective_a_plus(const snn_stdp_bridge_t* bridge);

/**
 * @brief Get effective LTD amplitude
 *
 * @param bridge Bridge to query
 * @return Effective a_minus (modulated by dopamine)
 */
float snn_stdp_bridge_get_effective_a_minus(const snn_stdp_bridge_t* bridge);

/**
 * @brief Get dopamine modulation factor
 *
 * @param bridge Bridge to query
 * @return Current DA modulation factor [0, 10+]
 */
float snn_stdp_bridge_get_da_modulation(const snn_stdp_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param plasticity_events Output: total plasticity events
 * @param weight_syncs Output: weight synchronizations
 * @param updates Output: update cycles completed
 * @return 0 on success
 */
int snn_stdp_bridge_get_stats(
    const snn_stdp_bridge_t* bridge,
    uint32_t* plasticity_events,
    uint32_t* weight_syncs,
    uint32_t* updates
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge to reset
 */
void snn_stdp_bridge_reset_stats(snn_stdp_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_STDP_BRIDGE_H */
