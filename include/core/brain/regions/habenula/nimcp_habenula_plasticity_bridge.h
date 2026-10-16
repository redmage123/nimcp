/**
 * @file nimcp_habenula_plasticity_bridge.h
 * @brief Habenula - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Bidirectional bridge between Habenula and plasticity mechanisms
 * WHY:  Enable aversion-gated learning, avoidance STDP, and punishment-based plasticity
 * HOW:  Habenula modulates learning for aversive outcomes and avoidance behavior
 *
 * THEORETICAL FOUNDATIONS:
 * - Matsumoto & Hikosaka (2009): Habenula in aversive learning
 * - Stamatakis & Stuber (2012): Habenula-VTA pathway in aversion
 * - Baker et al. (2016): Habenula in active avoidance learning
 *
 * BIOLOGICAL BASIS:
 * - Habenula encodes prediction errors for punishment
 * - Drives learning of avoidance behaviors
 * - Modulates DA/5-HT to suppress approach, enhance avoidance
 * - Critical for learning from negative outcomes
 *
 * INTEGRATION FLOWS:
 *
 * Habenula --> Plasticity:
 *   1. Negative RPE drives punishment-based weight updates
 *   2. Aversive events gate avoidance pathway strengthening
 *   3. Disappointment signals enhance extinction
 *   4. Error signals modulate overall learning
 *
 * Plasticity --> Habenula:
 *   1. Learning progress signals prediction accuracy
 *   2. Successful avoidance reduces Habenula activity
 *   3. Repeated failures increase Habenula engagement
 *   4. Network stability feedback
 *
 * @see nimcp_habenula.h
 * @see nimcp_habenula_adapter.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_HABENULA_PLASTICITY_BRIDGE_H
#define NIMCP_HABENULA_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_habenula_adapter_impl;
typedef struct nimcp_habenula_adapter_impl* nimcp_habenula_adapter_t;
struct nimcp_plasticity_coordinator;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Maximum tracked synapses */
#define HABENULA_PLASTICITY_MAX_SYNAPSES 512

/** @brief Default STDP window (ms) */
#define HABENULA_PLASTICITY_STDP_WINDOW  50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_HABENULA_PLASTICITY   0x0F10

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Habenula-modulated synapse types
 */
typedef enum {
    HABENULA_SYNAPSE_AVOIDANCE = 0,  /**< Avoidance pathway */
    HABENULA_SYNAPSE_VTA_INHIBITORY, /**< VTA inhibitory */
    HABENULA_SYNAPSE_RAPHE_INHIBITORY, /**< Raphe inhibitory */
    HABENULA_SYNAPSE_PREFRONTAL      /**< Prefrontal connections */
} nimcp_habenula_synapse_type_t;

/**
 * @brief Learning events modulated by Habenula
 */
typedef enum {
    HABENULA_LEARN_PUNISHMENT = 0,   /**< Punishment-based learning */
    HABENULA_LEARN_AVOIDANCE,        /**< Avoidance learning */
    HABENULA_LEARN_DISAPPOINTMENT,   /**< Disappointment learning */
    HABENULA_LEARN_RELIEF,           /**< Relief-based learning */
    HABENULA_LEARN_EXTINCTION        /**< Extinction of approach */
} nimcp_habenula_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    HABENULA_PLASTICITY_STATE_IDLE = 0,
    HABENULA_PLASTICITY_STATE_OBSERVING,
    HABENULA_PLASTICITY_STATE_GATING,
    HABENULA_PLASTICITY_STATE_UPDATING,
    HABENULA_PLASTICITY_STATE_CONSOLIDATING,
    HABENULA_PLASTICITY_STATE_DISABLED
} nimcp_habenula_plasticity_state_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Habenula-Plasticity bridge configuration
 */
typedef struct {
    /* Habenula modulation parameters */
    bool enable_habenula_gating;     /**< Habenula gates learning */
    float habenula_lr_multiplier;    /**< LR multiplier for aversive */
    float gating_threshold;          /**< Habenula threshold for gating */

    /* Punishment-based learning */
    bool enable_punishment_learning; /**< Enable punishment learning */
    float punishment_learning_rate;  /**< Punishment LR */
    float negative_rpe_scale;        /**< Negative RPE scaling */
    float punishment_ltd_boost;      /**< LTD boost for punishment */

    /* STDP parameters */
    float stdp_ltp_window_ms;        /**< LTP time window */
    float stdp_ltd_window_ms;        /**< LTD time window */
    float aversive_stdp_modulation;  /**< Aversive modulation of STDP */

    /* Avoidance learning */
    bool enable_avoidance_learning;  /**< Enable avoidance pathway */
    float avoidance_learning_rate;   /**< Avoidance LR */
    float successful_avoidance_boost;/**< LTP boost on successful avoidance */
    float failed_avoidance_penalty;  /**< LTD penalty on failed avoidance */

    /* Disappointment learning */
    bool enable_disappointment;      /**< Enable disappointment learning */
    float disappointment_scale;      /**< Disappointment scaling */
    float omission_penalty;          /**< Penalty for reward omission */

    /* Relief learning */
    bool enable_relief_learning;     /**< Enable relief signals */
    float relief_learning_rate;      /**< Relief LR */
    float relief_ltp_boost;          /**< LTP boost on relief */

    /* Inhibitory pathway strengthening */
    bool enable_inhibition_strengthening; /**< Strengthen VTA/Raphe inhibition */
    float inhibition_learning_rate;  /**< Inhibition pathway LR */

    /* Weight bounds */
    float weight_min;                /**< Minimum weight */
    float weight_max;                /**< Maximum weight */
    float initial_weight;            /**< Initial weight */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
} nimcp_habenula_plasticity_config_t;

/*=============================================================================
 * Synapse Structure
 *===========================================================================*/

/**
 * @brief Habenula-modulated synapse state
 */
typedef struct {
    uint32_t synapse_id;             /**< Unique identifier */
    nimcp_habenula_synapse_type_t type; /**< Synapse type */

    /* Weight state */
    float weight;                    /**< Current weight */
    float initial_weight;            /**< Initial weight */

    /* Timing state */
    uint64_t last_pre_spike_us;      /**< Last pre-synaptic spike */
    uint64_t last_post_spike_us;     /**< Last post-synaptic spike */

    /* Eligibility trace */
    float eligibility_trace;         /**< Current trace value */
    float punishment_trace;          /**< Punishment eligibility */

    /* Avoidance state */
    float avoidance_strength;        /**< Learned avoidance strength */
    uint32_t avoidance_successes;    /**< Successful avoidances */
    uint32_t avoidance_failures;     /**< Failed avoidances */

    /* Aversion state */
    float aversion_accumulator;      /**< Accumulated aversion */
    uint32_t punishment_count;       /**< Times punished */
} nimcp_habenula_plasticity_synapse_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Current Habenula modulation state
 */
typedef struct {
    float aversive_level;            /**< Current aversive level */
    float negative_rpe;              /**< Current negative RPE */
    float disappointment;            /**< Current disappointment */
    float lr_multiplier;             /**< Current LR multiplier */
    float avoidance_gate;            /**< Avoidance learning gate */
    bool punishment_active;          /**< Active punishment signal */
    uint64_t last_punishment_us;     /**< Last punishment timestamp */
} nimcp_habenula_plasticity_state_data_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_habenula_plasticity_state_t state;
    nimcp_habenula_plasticity_state_data_t habenula;
    uint32_t registered_synapses;
    float global_lr_modulation;
    float avg_avoidance_strength;
    bool bio_async_connected;
} nimcp_habenula_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t total_pre_spikes;
    uint64_t total_post_spikes;
    uint64_t ltp_events;
    uint64_t ltd_events;
    uint64_t punishment_events;
    uint64_t avoidance_successes;
    uint64_t avoidance_failures;
    uint64_t relief_events;
    float total_punishment;
    float avg_weight_change;
    float avg_negative_rpe;
} nimcp_habenula_plasticity_stats_t;

/*=============================================================================
 * Modulation Output
 *===========================================================================*/

/**
 * @brief Learning modulation output
 */
typedef struct {
    float lr_multiplier;             /**< Learning rate multiplier */
    float ltd_boost;                 /**< LTD enhancement factor */
    float avoidance_gate;            /**< Avoidance learning gate */
    float punishment_signal;         /**< Punishment learning signal */
    float inhibition_strength;       /**< Inhibition pathway strength */
    bool trigger_avoidance_learning; /**< Trigger avoidance update */
} nimcp_habenula_plasticity_modulation_t;

/*=============================================================================
 * Callback Types
 *===========================================================================*/

/**
 * @brief Weight change callback
 */
typedef void (*nimcp_habenula_weight_change_cb)(
    uint32_t synapse_id,
    nimcp_habenula_synapse_type_t type,
    float old_weight,
    float new_weight,
    nimcp_habenula_learn_event_t event_type,
    float negative_rpe,
    void* user_data
);

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_habenula_plasticity_bridge nimcp_habenula_plasticity_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
nimcp_habenula_plasticity_config_t nimcp_habenula_plasticity_config_default(void);

/**
 * @brief Create Habenula-plasticity bridge
 */
nimcp_habenula_plasticity_bridge_t* nimcp_habenula_plasticity_create(
    const nimcp_habenula_plasticity_config_t* config
);

/**
 * @brief Destroy Habenula-plasticity bridge
 */
void nimcp_habenula_plasticity_destroy(nimcp_habenula_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int nimcp_habenula_plasticity_reset(nimcp_habenula_plasticity_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to Habenula adapter
 */
int nimcp_habenula_plasticity_connect_habenula(
    nimcp_habenula_plasticity_bridge_t* bridge,
    nimcp_habenula_adapter_t habenula_adapter
);

/**
 * @brief Connect to plasticity coordinator
 */
int nimcp_habenula_plasticity_connect_coordinator(
    nimcp_habenula_plasticity_bridge_t* bridge,
    struct nimcp_plasticity_coordinator* coordinator
);

/*=============================================================================
 * Synapse Management
 *===========================================================================*/

/**
 * @brief Register synapse for Habenula-modulated learning
 */
int nimcp_habenula_plasticity_register_synapse(
    nimcp_habenula_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    nimcp_habenula_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister synapse
 */
int nimcp_habenula_plasticity_unregister_synapse(
    nimcp_habenula_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 */
int nimcp_habenula_plasticity_get_synapse(
    nimcp_habenula_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    nimcp_habenula_plasticity_synapse_t* synapse
);

/*=============================================================================
 * Event Recording (Habenula --> Plasticity)
 *===========================================================================*/

/**
 * @brief Record pre-synaptic spike
 */
int nimcp_habenula_plasticity_pre_spike(
    nimcp_habenula_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
);

/**
 * @brief Record post-synaptic spike
 */
int nimcp_habenula_plasticity_post_spike(
    nimcp_habenula_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
);

/**
 * @brief Record punishment event
 */
int nimcp_habenula_plasticity_punishment(
    nimcp_habenula_plasticity_bridge_t* bridge,
    float punishment,
    uint64_t timestamp_us
);

/**
 * @brief Record negative RPE
 */
int nimcp_habenula_plasticity_negative_rpe(
    nimcp_habenula_plasticity_bridge_t* bridge,
    float negative_rpe,
    uint64_t timestamp_us
);

/**
 * @brief Record disappointment (reward omission)
 */
int nimcp_habenula_plasticity_disappointment(
    nimcp_habenula_plasticity_bridge_t* bridge,
    float expected_reward,
    uint64_t timestamp_us
);

/**
 * @brief Record successful avoidance
 */
int nimcp_habenula_plasticity_avoidance_success(
    nimcp_habenula_plasticity_bridge_t* bridge,
    uint64_t timestamp_us
);

/**
 * @brief Record failed avoidance
 */
int nimcp_habenula_plasticity_avoidance_failure(
    nimcp_habenula_plasticity_bridge_t* bridge,
    float punishment,
    uint64_t timestamp_us
);

/**
 * @brief Record relief event
 */
int nimcp_habenula_plasticity_relief(
    nimcp_habenula_plasticity_bridge_t* bridge,
    float relief,
    uint64_t timestamp_us
);

/**
 * @brief Update Habenula state
 */
int nimcp_habenula_plasticity_set_state(
    nimcp_habenula_plasticity_bridge_t* bridge,
    float aversive_level,
    float negative_rpe
);

/*=============================================================================
 * Update Functions
 *===========================================================================*/

/**
 * @brief Update all plasticity mechanisms
 */
int nimcp_habenula_plasticity_update(
    nimcp_habenula_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Convert eligibility traces (on aversive signal)
 */
int nimcp_habenula_plasticity_convert_traces(
    nimcp_habenula_plasticity_bridge_t* bridge,
    float aversive_signal
);

/*=============================================================================
 * Query Functions (Plasticity --> Habenula)
 *===========================================================================*/

/**
 * @brief Get current learning modulation
 */
int nimcp_habenula_plasticity_get_modulation(
    nimcp_habenula_plasticity_bridge_t* bridge,
    nimcp_habenula_plasticity_modulation_t* modulation
);

/**
 * @brief Get avoidance learning progress
 */
float nimcp_habenula_plasticity_get_avoidance_progress(
    nimcp_habenula_plasticity_bridge_t* bridge
);

/**
 * @brief Get prediction accuracy
 */
float nimcp_habenula_plasticity_get_prediction_accuracy(
    nimcp_habenula_plasticity_bridge_t* bridge
);

/**
 * @brief Get inhibition pathway strength
 */
float nimcp_habenula_plasticity_get_inhibition_strength(
    nimcp_habenula_plasticity_bridge_t* bridge
);

/*=============================================================================
 * State and Statistics
 *===========================================================================*/

/**
 * @brief Get bridge state
 */
int nimcp_habenula_plasticity_get_state(
    const nimcp_habenula_plasticity_bridge_t* bridge,
    nimcp_habenula_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int nimcp_habenula_plasticity_get_stats(
    const nimcp_habenula_plasticity_bridge_t* bridge,
    nimcp_habenula_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void nimcp_habenula_plasticity_reset_stats(nimcp_habenula_plasticity_bridge_t* bridge);

/*=============================================================================
 * Callbacks
 *===========================================================================*/

/**
 * @brief Register weight change callback
 */
int nimcp_habenula_plasticity_set_weight_callback(
    nimcp_habenula_plasticity_bridge_t* bridge,
    nimcp_habenula_weight_change_cb callback,
    void* user_data
);

/*=============================================================================
 * Bio-Async Integration
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 */
int nimcp_habenula_plasticity_connect_bio_async(
    nimcp_habenula_plasticity_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 */
int nimcp_habenula_plasticity_disconnect_bio_async(
    nimcp_habenula_plasticity_bridge_t* bridge
);

/**
 * @brief Check bio-async connection status
 */
bool nimcp_habenula_plasticity_is_bio_async_connected(
    const nimcp_habenula_plasticity_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HABENULA_PLASTICITY_BRIDGE_H */
