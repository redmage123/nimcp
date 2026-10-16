/**
 * @file nimcp_sleep_wake_plasticity_bridge.h
 * @brief Sleep-Wake - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between sleep-wake system and synaptic plasticity
 * WHY:  Enable sleep-dependent memory consolidation through synaptic scaling,
 *       homeostatic plasticity, and sleep-stage-specific learning modulation
 * HOW:  STDP modulation by sleep state, BCM for stabilization, homeostatic
 *       scaling during deep sleep, replay-triggered consolidation
 *
 * THEORETICAL FOUNDATIONS:
 * - Tononi & Cirelli (2006): Synaptic homeostasis hypothesis
 * - Diekelmann & Born (2010): Memory consolidation during sleep
 * - Walker (2017): Sleep-dependent memory processing
 *
 * BIOLOGICAL BASIS:
 * - Slow-wave sleep: Synaptic downscaling for energy efficiency
 * - REM sleep: Selective strengthening of emotionally relevant memories
 * - Sleep spindles: Memory reactivation and consolidation
 * - Replay during sleep: Hippocampal-cortical memory transfer
 *
 * PLASTICITY TYPES:
 * - STDP: Modulated by sleep state (reduced during deep sleep)
 * - BCM: Threshold shifts during sleep-wake cycles
 * - Homeostatic: Global synaptic scaling during slow-wave sleep
 * - Consolidation: Selective strengthening during replay
 *
 * @see nimcp_sleep_wake.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_SLEEP_WAKE_PLASTICITY_BRIDGE_H
#define NIMCP_SLEEP_WAKE_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum sleep-wake synapses */
#define SLEEP_WAKE_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define SLEEP_WAKE_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_SLEEP_WAKE_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Sleep-wake synapse types
 */
typedef enum {
    SLEEP_WAKE_SYNAPSE_AROUSAL = 0,      /**< Arousal regulation */
    SLEEP_WAKE_SYNAPSE_SLEEP_DRIVE,       /**< Sleep drive (PROTECTED) */
    SLEEP_WAKE_SYNAPSE_WAKE_DRIVE,        /**< Wake drive (PROTECTED) */
    SLEEP_WAKE_SYNAPSE_CIRCADIAN,         /**< Circadian rhythm */
    SLEEP_WAKE_SYNAPSE_CONSOLIDATION,     /**< Memory consolidation */
    SLEEP_WAKE_SYNAPSE_HOMEOSTATIC        /**< Homeostatic regulation */
} sleep_wake_synapse_type_t;

/**
 * @brief Learning event types for sleep-wake
 */
typedef enum {
    SLEEP_WAKE_LEARN_SLEEP_ONSET = 0,     /**< Sleep onset detected */
    SLEEP_WAKE_LEARN_WAKE_ONSET,           /**< Wake onset detected */
    SLEEP_WAKE_LEARN_STAGE_TRANSITION,     /**< Sleep stage changed */
    SLEEP_WAKE_LEARN_PRESSURE_HIGH,        /**< High sleep pressure */
    SLEEP_WAKE_LEARN_PRESSURE_CLEARED,     /**< Sleep pressure cleared */
    SLEEP_WAKE_LEARN_CONSOLIDATION_SUCCESS,/**< Memory consolidation successful */
    SLEEP_WAKE_LEARN_CONSOLIDATION_PARTIAL,/**< Partial consolidation */
    SLEEP_WAKE_LEARN_AROUSAL_SPIKE,        /**< Sudden arousal increase */
    SLEEP_WAKE_LEARN_CIRCADIAN_ALIGNED,    /**< Sleep aligned with circadian */
    SLEEP_WAKE_LEARN_CIRCADIAN_MISALIGNED  /**< Sleep misaligned with circadian */
} sleep_wake_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SLEEP_WAKE_PLASTICITY_STATE_IDLE = 0,
    SLEEP_WAKE_PLASTICITY_STATE_LEARNING,
    SLEEP_WAKE_PLASTICITY_STATE_CONSOLIDATING,
    SLEEP_WAKE_PLASTICITY_STATE_SCALING,
    SLEEP_WAKE_PLASTICITY_STATE_ERROR
} sleep_wake_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Sleep-Wake-Plasticity bridge configuration
 */
typedef struct {
    /* Learning parameters */
    float base_learning_rate;            /**< Base learning rate */
    float stdp_tau_plus_ms;              /**< STDP potentiation time constant */
    float stdp_tau_minus_ms;             /**< STDP depression time constant */
    float stdp_a_plus;                   /**< STDP potentiation magnitude */
    float stdp_a_minus;                  /**< STDP depression magnitude */

    /* BCM parameters */
    float bcm_tau_ms;                    /**< BCM threshold time constant */
    float bcm_target_rate;               /**< BCM target activity */

    /* Homeostatic parameters */
    float homeostatic_tau_ms;            /**< Homeostatic time constant */
    float target_arousal;                /**< Target arousal level */

    /* Sleep-specific modulation */
    float sleep_learning_reduction;      /**< Learning reduction during sleep */
    float consolidation_boost;           /**< Boost for consolidation events */
    float downscaling_factor;            /**< Synaptic downscaling factor */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_sleep_drive;            /**< Protect sleep drive weights */
    bool protect_wake_drive;             /**< Protect wake drive weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} sleep_wake_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Sleep-wake synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    sleep_wake_synapse_type_t type;      /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} sleep_wake_plasticity_synapse_t;

/**
 * @brief Sleep-wake regulation state
 */
typedef struct {
    float arousal_sensitivity;           /**< Sensitivity to arousal changes */
    float sleep_drive_strength;          /**< Sleep drive modulation strength */
    float wake_drive_strength;           /**< Wake drive modulation strength */
    float circadian_coupling;            /**< Coupling to circadian rhythm */
    float consolidation_efficiency;      /**< Memory consolidation efficiency */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} sleep_wake_regulation_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    sleep_wake_plasticity_state_t state; /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} sleep_wake_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t sleep_onset_events;         /**< Sleep onset events */
    uint64_t wake_onset_events;          /**< Wake onset events */
    uint64_t consolidation_events;       /**< Consolidation events */
    uint64_t stage_transitions;          /**< Stage transition learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} sleep_wake_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct sleep_wake_plasticity_bridge sleep_wake_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*sleep_wake_plasticity_learn_callback_t)(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Regulation update callback */
typedef void (*sleep_wake_plasticity_regulation_callback_t)(
    sleep_wake_plasticity_bridge_t* bridge,
    float old_arousal,
    float new_arousal,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
sleep_wake_plasticity_config_t sleep_wake_plasticity_config_default(void);

/**
 * @brief Create sleep-wake plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
sleep_wake_plasticity_bridge_t* sleep_wake_plasticity_create(
    const sleep_wake_plasticity_config_t* config
);

/**
 * @brief Destroy sleep-wake plasticity bridge
 * @param bridge Bridge to destroy
 */
void sleep_wake_plasticity_destroy(sleep_wake_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_reset(sleep_wake_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management
//=============================================================================

/**
 * @brief Register a synapse for plasticity tracking
 * @param bridge Bridge handle
 * @param synapse_id Unique synapse ID
 * @param type Synapse type
 * @param initial_weight Initial weight
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_register_synapse(
    sleep_wake_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    sleep_wake_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_unregister_synapse(
    sleep_wake_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_get_synapse(
    sleep_wake_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    sleep_wake_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_protect_synapse(
    sleep_wake_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
);

//=============================================================================
// Learning Functions
//=============================================================================

/**
 * @brief Apply learning event
 * @param bridge Bridge handle
 * @param event Event type
 * @param magnitude Event magnitude [0-1]
 * @param synapse_id Target synapse
 * @param context Context strength
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_learn(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
);

/**
 * @brief Apply STDP to synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param pre_time Pre-synaptic spike time (ms)
 * @param post_time Post-synaptic spike time (ms)
 * @return Weight change, NAN on failure
 */
float sleep_wake_plasticity_apply_stdp(
    sleep_wake_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
);

/**
 * @brief Apply consolidation signal
 * @param bridge Bridge handle
 * @param consolidation_strength Consolidation signal [-1, 1]
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_apply_consolidation(
    sleep_wake_plasticity_bridge_t* bridge,
    float consolidation_strength
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_update_bcm(
    sleep_wake_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling (during deep sleep)
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_homeostatic_update(
    sleep_wake_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply synaptic downscaling (slow-wave sleep)
 * @param bridge Bridge handle
 * @param downscale_factor Factor to multiply weights by (e.g., 0.85)
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_apply_downscaling(
    sleep_wake_plasticity_bridge_t* bridge,
    float downscale_factor
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_update_traces(
    sleep_wake_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_consolidate(sleep_wake_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get regulation state
 * @param bridge Bridge handle
 * @param state Output regulation state
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_get_regulation_state(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_regulation_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_get_state(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_get_stats(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_reset_stats(sleep_wake_plasticity_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register learning event callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_register_learn_callback(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register regulation update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_register_regulation_callback(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_plasticity_regulation_callback_t callback,
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
int sleep_wake_plasticity_bio_async_connect(sleep_wake_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int sleep_wake_plasticity_bio_async_disconnect(sleep_wake_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool sleep_wake_plasticity_is_bio_async_connected(sleep_wake_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SLEEP_WAKE_PLASTICITY_BRIDGE_H */
