/**
 * @file nimcp_working_memory_plasticity_bridge.h
 * @brief Working Memory - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between working memory and plasticity mechanisms
 * WHY:  Enable memory consolidation through STDP, BCM, and rehearsal-based learning
 * HOW:  Track memory maintenance for spike-timing dependent synaptic changes
 *
 * THEORETICAL FOUNDATIONS:
 * - Baddeley (2000): Working memory model and central executive
 * - Ruchkin et al. (2003): Sustained activity and synaptic plasticity
 * - Mongillo et al. (2008): Synaptic theory of working memory
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal persistent activity depends on recurrent synaptic strength
 * - Memory maintenance modulates long-term consolidation
 * - Rehearsal strengthens item-specific synapses
 * - Capacity limits emerge from lateral inhibition plasticity
 *
 * INTEGRATION FLOWS:
 *
 * Working Memory --> Plasticity:
 *   1. Memory maintenance triggers pre-synaptic spike timing
 *   2. Salience modulates learning rate for consolidated items
 *   3. Rehearsal events strengthen recurrent connections
 *   4. Capacity overflow triggers competitive LTD
 *
 * Plasticity --> Working Memory:
 *   1. Weight changes affect memory persistence
 *   2. Consolidated items gain retrieval priority
 *   3. Synaptic scaling affects capacity
 *   4. LTP during encoding enhances retention
 *
 * @see nimcp_working_memory.h
 * @see nimcp_plasticity.h
 * @see nimcp_snn_memory_bridge.h
 */

#ifndef NIMCP_WORKING_MEMORY_PLASTICITY_BRIDGE_H
#define NIMCP_WORKING_MEMORY_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/nimcp_working_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum tracked memory synapses */
#define WM_PLASTICITY_MAX_SYNAPSES           512

/** @brief Maximum memory slots tracked */
#define WM_PLASTICITY_MAX_SLOTS              20

/** @brief Default STDP time window (ms) */
#define WM_PLASTICITY_STDP_WINDOW            50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_WM_PLASTICITY             0x0D10

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Working memory synapse types
 */
typedef enum {
    WM_SYNAPSE_ENCODING = 0,          /**< Sensory -> WM encoding */
    WM_SYNAPSE_MAINTENANCE,           /**< Recurrent maintenance synapses */
    WM_SYNAPSE_RETRIEVAL,             /**< WM -> output retrieval */
    WM_SYNAPSE_GATE,                  /**< Gating control synapses */
    WM_SYNAPSE_LATERAL_INHIBITION     /**< Competition synapses */
} wm_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    WM_LEARN_ENCODING = 0,       /**< Item encoding event */
    WM_LEARN_MAINTENANCE,        /**< Maintenance/rehearsal event */
    WM_LEARN_RETRIEVAL,          /**< Retrieval/recall event */
    WM_LEARN_EVICTION,           /**< Capacity-driven eviction */
    WM_LEARN_CONSOLIDATION,      /**< Long-term consolidation */
    WM_LEARN_DECAY               /**< Decay-driven forgetting */
} wm_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    WM_PLASTICITY_STATE_IDLE = 0,
    WM_PLASTICITY_STATE_ENCODING,
    WM_PLASTICITY_STATE_MAINTAINING,
    WM_PLASTICITY_STATE_RETRIEVING,
    WM_PLASTICITY_STATE_CONSOLIDATING,
    WM_PLASTICITY_STATE_DISABLED
} wm_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Working Memory-Plasticity bridge configuration
 */
typedef struct {
    /* STDP parameters */
    float stdp_ltp_window_ms;        /**< LTP time window */
    float stdp_ltd_window_ms;        /**< LTD time window */
    float stdp_a_plus;               /**< LTP amplitude */
    float stdp_a_minus;              /**< LTD amplitude */
    float stdp_tau_plus;             /**< LTP time constant */
    float stdp_tau_minus;            /**< LTD time constant */

    /* Maintenance modulation */
    bool enable_maintenance_ltp;     /**< Maintenance strengthens synapses */
    float maintenance_ltp_rate;      /**< LTP rate per maintenance cycle */
    float maintenance_interval_ms;   /**< Expected maintenance interval */

    /* Rehearsal learning */
    bool enable_rehearsal_boost;     /**< Rehearsal boosts weights */
    float rehearsal_ltp_gain;        /**< LTP gain during rehearsal */
    float rehearsal_window_ms;       /**< Window for rehearsal detection */

    /* Consolidation parameters */
    bool enable_consolidation;       /**< Enable long-term consolidation */
    float consolidation_threshold;   /**< Time threshold for consolidation */
    float consolidation_ltp_boost;   /**< LTP boost for consolidated items */

    /* Capacity-based plasticity */
    bool enable_capacity_ltd;        /**< LTD on capacity overflow */
    float capacity_ltd_rate;         /**< LTD rate for evicted items */
    float lateral_inhibition_gain;   /**< Inhibition plasticity gain */

    /* BCM metaplasticity */
    bool enable_bcm;                 /**< Enable BCM threshold sliding */
    float bcm_threshold_tau;         /**< Threshold adaptation rate */
    float bcm_activity_tau;          /**< Activity averaging rate */

    /* Homeostatic plasticity */
    bool enable_homeostatic;         /**< Enable synaptic scaling */
    float target_capacity_utilization; /**< Target capacity use [0, 1] */
    float homeostatic_tau_ms;        /**< Scaling time constant */

    /* Eligibility traces */
    bool enable_eligibility;         /**< Enable eligibility traces */
    float eligibility_decay;         /**< Trace decay rate */
    float reward_modulation_gain;    /**< Reward scaling */

    /* Weight bounds */
    float weight_min;                /**< Minimum weight */
    float weight_max;                /**< Maximum weight */
    float initial_weight;            /**< Initial weight */

    /* Salience modulation */
    bool enable_salience_modulation; /**< Salience affects learning */
    float salience_ltp_gain;         /**< How salience boosts LTP */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_immune_modulation;   /**< Enable immune system effects */
    bool enable_sleep_consolidation; /**< Enable sleep-dependent consolidation */
} wm_plasticity_config_t;

//=============================================================================
// Synapse Structure
//=============================================================================

/**
 * @brief Working memory-associated synapse state
 */
typedef struct {
    uint32_t synapse_id;             /**< Unique synapse identifier */
    wm_synapse_type_t type;          /**< Synapse type */
    uint32_t slot_idx;               /**< Associated memory slot (-1 if none) */

    /* Weight state */
    float weight;                    /**< Current synaptic weight */
    float initial_weight;            /**< Initial weight */

    /* Timing state */
    uint64_t last_pre_spike_us;      /**< Last pre-synaptic spike time */
    uint64_t last_post_spike_us;     /**< Last post-synaptic spike time */

    /* Eligibility */
    float eligibility_trace;         /**< Current eligibility trace */

    /* BCM state */
    float bcm_threshold;             /**< Sliding threshold */
    float avg_activity;              /**< Running activity average */

    /* Consolidation state */
    float consolidation_level;       /**< How consolidated [0, 1] */
    uint64_t encoding_time_us;       /**< When synapse was formed */
    uint32_t maintenance_count;      /**< Number of maintenance events */
} wm_plasticity_synapse_t;

//=============================================================================
// Per-Slot State
//=============================================================================

/**
 * @brief Per-slot plasticity state
 */
typedef struct {
    bool occupied;                   /**< Slot currently holds item */
    float encoding_strength;         /**< Initial encoding strength */
    float current_strength;          /**< Current maintenance strength */
    float consolidation_progress;    /**< Progress toward consolidation */
    uint32_t rehearsal_count;        /**< Number of rehearsals */
    uint64_t last_access_time_us;    /**< Last access time */
    float salience;                  /**< Item salience */
} wm_slot_plasticity_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Bridge runtime state
 */
typedef struct {
    wm_plasticity_state_t state;
    uint32_t registered_synapses;
    uint32_t active_slots;
    float global_learning_rate;
    float current_capacity_pressure;  /**< How full WM is [0, 1] */
    float current_salience_mod;
    bool bio_async_connected;
} wm_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_encodings;
    uint64_t total_maintenance_cycles;
    uint64_t total_retrievals;
    uint64_t total_evictions;
    uint64_t total_consolidations;
    uint64_t ltp_events;
    uint64_t ltd_events;
    float avg_weight_change;
    float avg_consolidation_time_ms;
    float avg_capacity_utilization;
} wm_plasticity_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Weight change callback
 */
typedef void (*wm_weight_change_cb)(
    uint32_t synapse_id,
    uint32_t slot_idx,
    float old_weight,
    float new_weight,
    wm_learn_event_t event_type,
    void* user_data
);

/**
 * @brief Consolidation callback
 */
typedef void (*wm_consolidation_cb)(
    uint32_t slot_idx,
    float consolidation_level,
    void* user_data
);

//=============================================================================
// Main Bridge Structure
//=============================================================================

/** @brief Forward declaration */
typedef struct wm_plasticity_bridge wm_plasticity_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 */
wm_plasticity_config_t wm_plasticity_config_default(void);

/**
 * @brief Create working memory-plasticity bridge
 */
wm_plasticity_bridge_t* wm_plasticity_create(
    const wm_plasticity_config_t* config
);

/**
 * @brief Destroy bridge
 */
void wm_plasticity_destroy(wm_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int wm_plasticity_reset(wm_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management
//=============================================================================

/**
 * @brief Register synapse for WM-based learning
 */
int wm_plasticity_register_synapse(
    wm_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    wm_synapse_type_t type,
    int32_t slot_idx,  /* -1 for unassigned */
    float initial_weight
);

/**
 * @brief Unregister synapse
 */
int wm_plasticity_unregister_synapse(
    wm_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 */
int wm_plasticity_get_synapse(
    wm_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    wm_plasticity_synapse_t* synapse
);

//=============================================================================
// Event Recording (Working Memory --> Plasticity)
//=============================================================================

/**
 * @brief Record item encoding event
 */
int wm_plasticity_encode(
    wm_plasticity_bridge_t* bridge,
    uint32_t slot_idx,
    float salience,
    uint64_t timestamp_us
);

/**
 * @brief Record maintenance/rehearsal event
 */
int wm_plasticity_maintain(
    wm_plasticity_bridge_t* bridge,
    uint32_t slot_idx,
    float activity_level,
    uint64_t timestamp_us
);

/**
 * @brief Record retrieval event
 */
int wm_plasticity_retrieve(
    wm_plasticity_bridge_t* bridge,
    uint32_t slot_idx,
    float retrieval_strength,
    uint64_t timestamp_us
);

/**
 * @brief Record eviction event
 */
int wm_plasticity_evict(
    wm_plasticity_bridge_t* bridge,
    uint32_t slot_idx,
    uint64_t timestamp_us
);

/**
 * @brief Record decay event
 */
int wm_plasticity_decay(
    wm_plasticity_bridge_t* bridge,
    uint32_t slot_idx,
    float new_strength,
    uint64_t timestamp_us
);

/**
 * @brief Record reward/punishment signal
 */
int wm_plasticity_reward(
    wm_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update all plasticity mechanisms
 */
int wm_plasticity_update(
    wm_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Trigger consolidation for slot
 */
int wm_plasticity_consolidate_slot(
    wm_plasticity_bridge_t* bridge,
    uint32_t slot_idx
);

/**
 * @brief Trigger global consolidation (e.g., during sleep)
 */
int wm_plasticity_consolidate_all(
    wm_plasticity_bridge_t* bridge
);

//=============================================================================
// Query Functions (Plasticity --> Working Memory)
//=============================================================================

/**
 * @brief Get learned encoding strength for slot
 */
float wm_plasticity_get_encoding_strength(
    wm_plasticity_bridge_t* bridge,
    uint32_t slot_idx
);

/**
 * @brief Get consolidation level for slot
 */
float wm_plasticity_get_consolidation_level(
    wm_plasticity_bridge_t* bridge,
    uint32_t slot_idx
);

/**
 * @brief Get retrieval priority based on plasticity
 */
float wm_plasticity_get_retrieval_priority(
    wm_plasticity_bridge_t* bridge,
    uint32_t slot_idx
);

/**
 * @brief Get maintenance modulation
 */
int wm_plasticity_get_maintenance_modulation(
    wm_plasticity_bridge_t* bridge,
    float* modulation,
    uint32_t num_slots
);

//=============================================================================
// State and Statistics
//=============================================================================

/**
 * @brief Get bridge state
 */
int wm_plasticity_get_state(
    const wm_plasticity_bridge_t* bridge,
    wm_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int wm_plasticity_get_stats(
    const wm_plasticity_bridge_t* bridge,
    wm_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void wm_plasticity_reset_stats(wm_plasticity_bridge_t* bridge);

//=============================================================================
// Callbacks
//=============================================================================

/**
 * @brief Register weight change callback
 */
int wm_plasticity_set_weight_callback(
    wm_plasticity_bridge_t* bridge,
    wm_weight_change_cb callback,
    void* user_data
);

/**
 * @brief Register consolidation callback
 */
int wm_plasticity_set_consolidation_callback(
    wm_plasticity_bridge_t* bridge,
    wm_consolidation_cb callback,
    void* user_data
);

//=============================================================================
// Modulation Functions
//=============================================================================

/**
 * @brief Set capacity pressure
 */
int wm_plasticity_set_capacity_pressure(
    wm_plasticity_bridge_t* bridge,
    float pressure  /* 0 = empty, 1 = full */
);

/**
 * @brief Set salience modulation
 */
int wm_plasticity_set_salience_modulation(
    wm_plasticity_bridge_t* bridge,
    float salience_level
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 */
int wm_plasticity_connect_bio_async(wm_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int wm_plasticity_disconnect_bio_async(wm_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool wm_plasticity_is_bio_async_connected(const wm_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORKING_MEMORY_PLASTICITY_BRIDGE_H */
