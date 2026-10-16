/**
 * @file nimcp_raphe_plasticity_bridge.h
 * @brief Raphe Nuclei - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Bidirectional bridge between Raphe (serotonin) and plasticity mechanisms
 * WHY:  Enable 5-HT-gated learning, mood-modulated STDP, and impulse-control learning
 * HOW:  5-HT modulates learning rate, mood affects weight update bias
 *
 * THEORETICAL FOUNDATIONS:
 * - Branchi (2011): Serotonin and neuroplasticity
 * - Lesch & Waider (2012): 5-HT in developmental plasticity
 * - Carhart-Harris & Nutt (2017): 5-HT and cognitive flexibility
 *
 * BIOLOGICAL BASIS:
 * - 5-HT promotes synaptic plasticity in limbic circuits
 * - Mood state biases learning toward positive/negative outcomes
 * - 5-HT modulates fear extinction and safety learning
 * - Impulse control learning strengthens prefrontal inhibition
 *
 * INTEGRATION FLOWS:
 *
 * Raphe --> Plasticity:
 *   1. 5-HT level modulates global plasticity rate
 *   2. Mood valence biases LTP/LTD balance
 *   3. Impulse control strengthens inhibitory pathways
 *   4. Patience signals modulate temporal credit assignment
 *
 * Plasticity --> Raphe:
 *   1. Learning outcomes affect mood state
 *   2. Successful inhibition reinforces 5-HT release
 *   3. Extinction learning signals to Raphe
 *   4. Network stability affects baseline 5-HT
 *
 * @see nimcp_raphe_nuclei.h
 * @see nimcp_raphe_adapter.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_RAPHE_PLASTICITY_BRIDGE_H
#define NIMCP_RAPHE_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_raphe_adapter;
typedef struct nimcp_raphe_adapter* nimcp_raphe_adapter_t;
struct nimcp_plasticity_coordinator;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Maximum tracked synapses */
#define RAPHE_PLASTICITY_MAX_SYNAPSES   512

/** @brief Default STDP window (ms) */
#define RAPHE_PLASTICITY_STDP_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_RAPHE_PLASTICITY     0x0E10

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief 5-HT-modulated synapse types
 */
typedef enum {
    RAPHE_SYNAPSE_LIMBIC = 0,        /**< Limbic (mood) connections */
    RAPHE_SYNAPSE_PREFRONTAL,        /**< PFC (impulse control) */
    RAPHE_SYNAPSE_AMYGDALA,          /**< Amygdala (fear/safety) */
    RAPHE_SYNAPSE_STRIATAL           /**< Striatal (habit) connections */
} nimcp_raphe_synapse_type_t;

/**
 * @brief Learning events modulated by 5-HT
 */
typedef enum {
    RAPHE_LEARN_MOOD = 0,            /**< Mood-based learning */
    RAPHE_LEARN_INHIBITION,          /**< Impulse control learning */
    RAPHE_LEARN_EXTINCTION,          /**< Fear extinction */
    RAPHE_LEARN_SAFETY,              /**< Safety signal learning */
    RAPHE_LEARN_PATIENCE             /**< Patience/delay learning */
} nimcp_raphe_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    RAPHE_PLASTICITY_STATE_IDLE = 0,
    RAPHE_PLASTICITY_STATE_OBSERVING,
    RAPHE_PLASTICITY_STATE_GATING,
    RAPHE_PLASTICITY_STATE_UPDATING,
    RAPHE_PLASTICITY_STATE_CONSOLIDATING,
    RAPHE_PLASTICITY_STATE_DISABLED
} nimcp_raphe_plasticity_state_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Raphe-Plasticity bridge configuration
 */
typedef struct {
    /* 5-HT modulation parameters */
    bool enable_ht_gating;           /**< 5-HT gates learning */
    float ht_lr_multiplier_min;      /**< Min LR multiplier at low 5-HT */
    float ht_lr_multiplier_max;      /**< Max LR multiplier at high 5-HT */
    float ht_gating_threshold;       /**< 5-HT threshold for gating */

    /* Mood modulation */
    bool enable_mood_modulation;     /**< Mood affects learning bias */
    float positive_mood_ltp_boost;   /**< LTP boost for positive mood */
    float negative_mood_ltd_boost;   /**< LTD boost for negative mood */
    float mood_bias_strength;        /**< Strength of mood bias */

    /* STDP parameters */
    float stdp_ltp_window_ms;        /**< LTP time window */
    float stdp_ltd_window_ms;        /**< LTD time window */
    float ht_stdp_modulation;        /**< 5-HT modulation of STDP */

    /* Impulse control learning */
    bool enable_inhibition_learning; /**< Learn impulse control */
    float inhibition_learning_rate;  /**< Inhibition learning rate */
    float successful_inhibition_boost; /**< LTP boost on successful inhibition */

    /* Extinction learning */
    bool enable_extinction;          /**< Enable fear extinction */
    float extinction_rate;           /**< Extinction learning rate */
    float safety_learning_rate;      /**< Safety signal learning rate */
    float extinction_ht_requirement; /**< 5-HT level for extinction */

    /* Patience/temporal learning */
    bool enable_patience_learning;   /**< Learn patience/waiting */
    float patience_learning_rate;    /**< Patience learning rate */
    float delayed_reward_boost;      /**< Boost for delayed reward */

    /* Weight bounds */
    float weight_min;                /**< Minimum weight */
    float weight_max;                /**< Maximum weight */
    float initial_weight;            /**< Initial weight */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
} nimcp_raphe_plasticity_config_t;

/*=============================================================================
 * Synapse Structure
 *===========================================================================*/

/**
 * @brief 5-HT-modulated synapse state
 */
typedef struct {
    uint32_t synapse_id;             /**< Unique identifier */
    nimcp_raphe_synapse_type_t type; /**< Synapse type */

    /* Weight state */
    float weight;                    /**< Current weight */
    float initial_weight;            /**< Initial weight */

    /* Timing state */
    uint64_t last_pre_spike_us;      /**< Last pre-synaptic spike */
    uint64_t last_post_spike_us;     /**< Last post-synaptic spike */

    /* Mood-affected state */
    float mood_bias_accumulator;     /**< Accumulated mood bias */
    float current_mood_bias;         /**< Current mood-based bias */

    /* Inhibition state */
    float inhibition_strength;       /**< Learned inhibition strength */
    uint32_t inhibition_successes;   /**< Successful inhibitions */

    /* Extinction state */
    float extinction_level;          /**< Fear extinction level [0,1] */
    float safety_signal;             /**< Safety signal strength */
} nimcp_raphe_plasticity_synapse_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Current 5-HT modulation state
 */
typedef struct {
    float serotonin_level;           /**< Current 5-HT level */
    float mood_valence;              /**< Current mood valence */
    float impulse_control;           /**< Current impulse control */
    float lr_multiplier;             /**< Current LR multiplier */
    float mood_bias;                 /**< Current mood-based bias */
    bool patience_active;            /**< Actively waiting */
    uint64_t patience_start_us;      /**< When waiting started */
} nimcp_raphe_plasticity_ht_state_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_raphe_plasticity_state_t state;
    nimcp_raphe_plasticity_ht_state_t ht;
    uint32_t registered_synapses;
    float global_lr_modulation;
    float avg_inhibition_strength;
    bool bio_async_connected;
} nimcp_raphe_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t total_pre_spikes;
    uint64_t total_post_spikes;
    uint64_t ltp_events;
    uint64_t ltd_events;
    uint64_t inhibition_successes;
    uint64_t inhibition_failures;
    uint64_t extinction_trials;
    float avg_weight_change;
    float avg_mood_bias_applied;
} nimcp_raphe_plasticity_stats_t;

/*=============================================================================
 * Modulation Output
 *===========================================================================*/

/**
 * @brief Learning modulation output
 */
typedef struct {
    float lr_multiplier;             /**< Learning rate multiplier */
    float mood_bias;                 /**< Mood-based LTP/LTD bias */
    float inhibition_gate;           /**< Inhibition learning gate */
    float extinction_rate;           /**< Current extinction rate */
    float patience_bonus;            /**< Patience reward bonus */
    bool enable_extinction;          /**< Enable extinction now */
} nimcp_raphe_plasticity_modulation_t;

/*=============================================================================
 * Callback Types
 *===========================================================================*/

/**
 * @brief Weight change callback
 */
typedef void (*nimcp_raphe_weight_change_cb)(
    uint32_t synapse_id,
    nimcp_raphe_synapse_type_t type,
    float old_weight,
    float new_weight,
    nimcp_raphe_learn_event_t event_type,
    void* user_data
);

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_raphe_plasticity_bridge nimcp_raphe_plasticity_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
nimcp_raphe_plasticity_config_t nimcp_raphe_plasticity_config_default(void);

/**
 * @brief Create Raphe-plasticity bridge
 */
nimcp_raphe_plasticity_bridge_t* nimcp_raphe_plasticity_create(
    const nimcp_raphe_plasticity_config_t* config
);

/**
 * @brief Destroy Raphe-plasticity bridge
 */
void nimcp_raphe_plasticity_destroy(nimcp_raphe_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int nimcp_raphe_plasticity_reset(nimcp_raphe_plasticity_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to Raphe adapter
 */
int nimcp_raphe_plasticity_connect_raphe(
    nimcp_raphe_plasticity_bridge_t* bridge,
    nimcp_raphe_adapter_t raphe_adapter
);

/**
 * @brief Connect to plasticity coordinator
 */
int nimcp_raphe_plasticity_connect_coordinator(
    nimcp_raphe_plasticity_bridge_t* bridge,
    struct nimcp_plasticity_coordinator* coordinator
);

/*=============================================================================
 * Synapse Management
 *===========================================================================*/

/**
 * @brief Register synapse for 5-HT-modulated learning
 */
int nimcp_raphe_plasticity_register_synapse(
    nimcp_raphe_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    nimcp_raphe_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister synapse
 */
int nimcp_raphe_plasticity_unregister_synapse(
    nimcp_raphe_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 */
int nimcp_raphe_plasticity_get_synapse(
    nimcp_raphe_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    nimcp_raphe_plasticity_synapse_t* synapse
);

/*=============================================================================
 * Event Recording (Raphe --> Plasticity)
 *===========================================================================*/

/**
 * @brief Record pre-synaptic spike
 */
int nimcp_raphe_plasticity_pre_spike(
    nimcp_raphe_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
);

/**
 * @brief Record post-synaptic spike
 */
int nimcp_raphe_plasticity_post_spike(
    nimcp_raphe_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
);

/**
 * @brief Record successful impulse inhibition
 */
int nimcp_raphe_plasticity_inhibition_success(
    nimcp_raphe_plasticity_bridge_t* bridge,
    uint64_t timestamp_us
);

/**
 * @brief Record failed impulse inhibition
 */
int nimcp_raphe_plasticity_inhibition_failure(
    nimcp_raphe_plasticity_bridge_t* bridge,
    uint64_t timestamp_us
);

/**
 * @brief Record extinction trial
 */
int nimcp_raphe_plasticity_extinction_trial(
    nimcp_raphe_plasticity_bridge_t* bridge,
    uint64_t timestamp_us
);

/**
 * @brief Update 5-HT level and mood
 */
int nimcp_raphe_plasticity_set_ht_state(
    nimcp_raphe_plasticity_bridge_t* bridge,
    float serotonin_level,
    float mood_valence
);

/*=============================================================================
 * Update Functions
 *===========================================================================*/

/**
 * @brief Update all plasticity mechanisms
 */
int nimcp_raphe_plasticity_update(
    nimcp_raphe_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Start patience trial
 */
int nimcp_raphe_plasticity_start_patience(
    nimcp_raphe_plasticity_bridge_t* bridge,
    uint64_t timestamp_us
);

/**
 * @brief Complete patience trial (with delayed reward)
 */
int nimcp_raphe_plasticity_complete_patience(
    nimcp_raphe_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us
);

/*=============================================================================
 * Query Functions (Plasticity --> Raphe)
 *===========================================================================*/

/**
 * @brief Get current learning modulation
 */
int nimcp_raphe_plasticity_get_modulation(
    nimcp_raphe_plasticity_bridge_t* bridge,
    nimcp_raphe_plasticity_modulation_t* modulation
);

/**
 * @brief Get learned impulse control strength
 */
float nimcp_raphe_plasticity_get_inhibition_strength(
    nimcp_raphe_plasticity_bridge_t* bridge
);

/**
 * @brief Get extinction progress
 */
float nimcp_raphe_plasticity_get_extinction_progress(
    nimcp_raphe_plasticity_bridge_t* bridge
);

/**
 * @brief Get mood feedback signal
 */
float nimcp_raphe_plasticity_get_mood_feedback(
    nimcp_raphe_plasticity_bridge_t* bridge
);

/*=============================================================================
 * State and Statistics
 *===========================================================================*/

/**
 * @brief Get bridge state
 */
int nimcp_raphe_plasticity_get_state(
    const nimcp_raphe_plasticity_bridge_t* bridge,
    nimcp_raphe_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int nimcp_raphe_plasticity_get_stats(
    const nimcp_raphe_plasticity_bridge_t* bridge,
    nimcp_raphe_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void nimcp_raphe_plasticity_reset_stats(nimcp_raphe_plasticity_bridge_t* bridge);

/*=============================================================================
 * Callbacks
 *===========================================================================*/

/**
 * @brief Register weight change callback
 */
int nimcp_raphe_plasticity_set_weight_callback(
    nimcp_raphe_plasticity_bridge_t* bridge,
    nimcp_raphe_weight_change_cb callback,
    void* user_data
);

/*=============================================================================
 * Bio-Async Integration
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 */
int nimcp_raphe_plasticity_connect_bio_async(nimcp_raphe_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int nimcp_raphe_plasticity_disconnect_bio_async(nimcp_raphe_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool nimcp_raphe_plasticity_is_bio_async_connected(const nimcp_raphe_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RAPHE_PLASTICITY_BRIDGE_H */
