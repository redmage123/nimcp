/**
 * @file nimcp_lc_plasticity_bridge.h
 * @brief Locus Coeruleus - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Bidirectional bridge between LC (norepinephrine) and plasticity mechanisms
 * WHY:  Enable NE-gated learning, attention-modulated STDP, and arousal-based consolidation
 * HOW:  NE modulates learning rate, eligibility traces, and consolidation timing
 *
 * THEORETICAL FOUNDATIONS:
 * - Harley (2004): Noradrenergic long-term potentiation and memory
 * - Bouret & Sara (2005): LC-NE and learning/memory functions
 * - Mather et al. (2016): NE amplifies salient information for memory
 *
 * BIOLOGICAL BASIS:
 * - NE gates eligibility traces during learning
 * - Arousal level determines consolidation probability
 * - Phasic NE bursts enhance STDP for salient events
 * - NE modulates LTP/LTD threshold (BCM-like)
 *
 * INTEGRATION FLOWS:
 *
 * LC --> Plasticity:
 *   1. NE level modulates global learning rate
 *   2. Phasic bursts gate eligibility trace conversion
 *   3. Arousal affects consolidation strength
 *   4. Novelty signals trigger metaplasticity
 *
 * Plasticity --> LC:
 *   1. Learning progress affects NE baseline
 *   2. Synaptic changes signal novelty/familiarity
 *   3. Consolidation completion signals reset
 *   4. Network stability affects tonic/phasic balance
 *
 * @see nimcp_locus_coeruleus.h
 * @see nimcp_lc_adapter.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_LC_PLASTICITY_BRIDGE_H
#define NIMCP_LC_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_lc_adapter_struct;
typedef struct nimcp_lc_adapter_struct* nimcp_lc_adapter_t;
struct nimcp_plasticity_coordinator;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Maximum tracked synapses */
#define LC_PLASTICITY_MAX_SYNAPSES      512

/** @brief Default STDP window (ms) */
#define LC_PLASTICITY_STDP_WINDOW       50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_LC_PLASTICITY        0x0C10

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief NE-modulated synapse types
 */
typedef enum {
    LC_SYNAPSE_CORTICAL = 0,         /**< Cortical connections */
    LC_SYNAPSE_HIPPOCAMPAL,          /**< Hippocampal connections */
    LC_SYNAPSE_THALAMIC,             /**< Thalamic connections */
    LC_SYNAPSE_AMYGDALA              /**< Amygdala connections */
} nimcp_lc_synapse_type_t;

/**
 * @brief Learning events modulated by NE
 */
typedef enum {
    LC_LEARN_ATTENTION = 0,          /**< Attention-based learning */
    LC_LEARN_NOVELTY,                /**< Novelty-driven learning */
    LC_LEARN_AROUSAL,                /**< Arousal-enhanced learning */
    LC_LEARN_CONSOLIDATION,          /**< Memory consolidation */
    LC_LEARN_EXTINCTION              /**< Extinction learning */
} nimcp_lc_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    LC_PLASTICITY_STATE_IDLE = 0,
    LC_PLASTICITY_STATE_OBSERVING,
    LC_PLASTICITY_STATE_GATING,
    LC_PLASTICITY_STATE_UPDATING,
    LC_PLASTICITY_STATE_CONSOLIDATING,
    LC_PLASTICITY_STATE_DISABLED
} nimcp_lc_plasticity_state_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief LC-Plasticity bridge configuration
 */
typedef struct {
    /* NE modulation parameters */
    bool enable_ne_gating;           /**< NE gates learning */
    float ne_lr_multiplier_min;      /**< Min LR multiplier at low NE */
    float ne_lr_multiplier_max;      /**< Max LR multiplier at high NE */
    float ne_gating_threshold;       /**< NE threshold for gating */

    /* STDP modulation */
    float stdp_ltp_window_ms;        /**< LTP time window */
    float stdp_ltd_window_ms;        /**< LTD time window */
    float ne_ltp_boost;              /**< NE boost for LTP */
    float ne_ltd_suppression;        /**< NE suppression of LTD */
    float arousal_stdp_gain;         /**< Arousal effect on STDP */

    /* Eligibility trace parameters */
    bool enable_eligibility_gating;  /**< NE gates eligibility conversion */
    float eligibility_decay_base;    /**< Base decay rate */
    float ne_eligibility_extension;  /**< NE extends trace lifetime */
    float phasic_conversion_boost;   /**< Phasic burst conversion boost */

    /* BCM metaplasticity */
    bool enable_bcm_modulation;      /**< NE modulates BCM threshold */
    float ne_bcm_threshold_shift;    /**< How NE shifts threshold */
    float bcm_adaptation_rate;       /**< Threshold adaptation rate */

    /* Consolidation */
    bool enable_consolidation_gating;/**< NE gates consolidation */
    float consolidation_threshold;   /**< Arousal threshold for consolidation */
    float consolidation_rate;        /**< Base consolidation rate */
    float arousal_consolidation_boost; /**< Arousal boost for consolidation */

    /* Homeostatic */
    bool enable_homeostatic;         /**< Enable homeostatic plasticity */
    float target_activity;           /**< Target activity level */
    float homeostatic_tau_ms;        /**< Time constant */

    /* Weight bounds */
    float weight_min;                /**< Minimum weight */
    float weight_max;                /**< Maximum weight */
    float initial_weight;            /**< Initial weight */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
} nimcp_lc_plasticity_config_t;

/*=============================================================================
 * Synapse Structure
 *===========================================================================*/

/**
 * @brief NE-modulated synapse state
 */
typedef struct {
    uint32_t synapse_id;             /**< Unique identifier */
    nimcp_lc_synapse_type_t type;    /**< Synapse type */

    /* Weight state */
    float weight;                    /**< Current weight */
    float initial_weight;            /**< Initial weight */

    /* Timing state */
    uint64_t last_pre_spike_us;      /**< Last pre-synaptic spike */
    uint64_t last_post_spike_us;     /**< Last post-synaptic spike */

    /* Eligibility */
    float eligibility_trace;         /**< Current trace value */
    bool trace_gated;                /**< Trace conversion gated by NE */

    /* BCM state */
    float bcm_threshold;             /**< Sliding threshold */
    float avg_activity;              /**< Activity average */

    /* Consolidation state */
    float consolidation_level;       /**< Consolidation progress [0,1] */
    bool consolidated;               /**< Fully consolidated */
} nimcp_lc_plasticity_synapse_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Current NE modulation state
 */
typedef struct {
    float ne_level;                  /**< Current NE level */
    float arousal;                   /**< Current arousal */
    float lr_multiplier;             /**< Current LR multiplier */
    float eligibility_gate;          /**< Eligibility gating factor */
    bool phasic_burst_active;        /**< Phasic burst ongoing */
    uint64_t last_burst_us;          /**< Last burst timestamp */
} nimcp_lc_plasticity_ne_state_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_lc_plasticity_state_t state;
    nimcp_lc_plasticity_ne_state_t ne;
    uint32_t registered_synapses;
    float global_lr_modulation;
    bool bio_async_connected;
} nimcp_lc_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t total_pre_spikes;
    uint64_t total_post_spikes;
    uint64_t ltp_events;
    uint64_t ltd_events;
    uint64_t gated_conversions;
    uint64_t consolidation_events;
    float avg_weight_change;
    float avg_ne_during_learning;
} nimcp_lc_plasticity_stats_t;

/*=============================================================================
 * Modulation Output
 *===========================================================================*/

/**
 * @brief Learning modulation output
 */
typedef struct {
    float lr_multiplier;             /**< Learning rate multiplier */
    float ltp_boost;                 /**< LTP enhancement factor */
    float ltd_suppression;           /**< LTD suppression factor */
    float eligibility_gate;          /**< Eligibility conversion gate */
    float consolidation_rate;        /**< Current consolidation rate */
    bool trigger_consolidation;      /**< Trigger consolidation */
} nimcp_lc_plasticity_modulation_t;

/*=============================================================================
 * Callback Types
 *===========================================================================*/

/**
 * @brief Weight change callback
 */
typedef void (*nimcp_lc_weight_change_cb)(
    uint32_t synapse_id,
    nimcp_lc_synapse_type_t type,
    float old_weight,
    float new_weight,
    nimcp_lc_learn_event_t event_type,
    void* user_data
);

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_lc_plasticity_bridge nimcp_lc_plasticity_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
nimcp_lc_plasticity_config_t nimcp_lc_plasticity_config_default(void);

/**
 * @brief Create LC-plasticity bridge
 */
nimcp_lc_plasticity_bridge_t* nimcp_lc_plasticity_create(
    const nimcp_lc_plasticity_config_t* config
);

/**
 * @brief Destroy LC-plasticity bridge
 */
void nimcp_lc_plasticity_destroy(nimcp_lc_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int nimcp_lc_plasticity_reset(nimcp_lc_plasticity_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to LC adapter
 */
int nimcp_lc_plasticity_connect_lc(
    nimcp_lc_plasticity_bridge_t* bridge,
    nimcp_lc_adapter_t lc_adapter
);

/**
 * @brief Connect to plasticity coordinator
 */
int nimcp_lc_plasticity_connect_coordinator(
    nimcp_lc_plasticity_bridge_t* bridge,
    struct nimcp_plasticity_coordinator* coordinator
);

/*=============================================================================
 * Synapse Management
 *===========================================================================*/

/**
 * @brief Register synapse for NE-modulated learning
 */
int nimcp_lc_plasticity_register_synapse(
    nimcp_lc_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    nimcp_lc_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister synapse
 */
int nimcp_lc_plasticity_unregister_synapse(
    nimcp_lc_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 */
int nimcp_lc_plasticity_get_synapse(
    nimcp_lc_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    nimcp_lc_plasticity_synapse_t* synapse
);

/*=============================================================================
 * Event Recording (LC --> Plasticity)
 *===========================================================================*/

/**
 * @brief Record pre-synaptic spike
 */
int nimcp_lc_plasticity_pre_spike(
    nimcp_lc_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
);

/**
 * @brief Record post-synaptic spike
 */
int nimcp_lc_plasticity_post_spike(
    nimcp_lc_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
);

/**
 * @brief Record NE burst event
 */
int nimcp_lc_plasticity_ne_burst(
    nimcp_lc_plasticity_bridge_t* bridge,
    float intensity,
    uint64_t timestamp_us
);

/**
 * @brief Update NE level
 */
int nimcp_lc_plasticity_set_ne_level(
    nimcp_lc_plasticity_bridge_t* bridge,
    float ne_level,
    float arousal
);

/*=============================================================================
 * Update Functions
 *===========================================================================*/

/**
 * @brief Update all plasticity mechanisms
 */
int nimcp_lc_plasticity_update(
    nimcp_lc_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Trigger memory consolidation
 */
int nimcp_lc_plasticity_consolidate(nimcp_lc_plasticity_bridge_t* bridge);

/*=============================================================================
 * Query Functions (Plasticity --> LC)
 *===========================================================================*/

/**
 * @brief Get current learning modulation
 */
int nimcp_lc_plasticity_get_modulation(
    nimcp_lc_plasticity_bridge_t* bridge,
    nimcp_lc_plasticity_modulation_t* modulation
);

/**
 * @brief Get learning progress signal
 */
float nimcp_lc_plasticity_get_learning_progress(
    nimcp_lc_plasticity_bridge_t* bridge
);

/**
 * @brief Get novelty/familiarity signal
 */
float nimcp_lc_plasticity_get_novelty_signal(
    nimcp_lc_plasticity_bridge_t* bridge
);

/**
 * @brief Get consolidation status
 */
float nimcp_lc_plasticity_get_consolidation_progress(
    nimcp_lc_plasticity_bridge_t* bridge
);

/*=============================================================================
 * State and Statistics
 *===========================================================================*/

/**
 * @brief Get bridge state
 */
int nimcp_lc_plasticity_get_state(
    const nimcp_lc_plasticity_bridge_t* bridge,
    nimcp_lc_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int nimcp_lc_plasticity_get_stats(
    const nimcp_lc_plasticity_bridge_t* bridge,
    nimcp_lc_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void nimcp_lc_plasticity_reset_stats(nimcp_lc_plasticity_bridge_t* bridge);

/*=============================================================================
 * Callbacks
 *===========================================================================*/

/**
 * @brief Register weight change callback
 */
int nimcp_lc_plasticity_set_weight_callback(
    nimcp_lc_plasticity_bridge_t* bridge,
    nimcp_lc_weight_change_cb callback,
    void* user_data
);

/*=============================================================================
 * Bio-Async Integration
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 */
int nimcp_lc_plasticity_connect_bio_async(nimcp_lc_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int nimcp_lc_plasticity_disconnect_bio_async(nimcp_lc_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool nimcp_lc_plasticity_is_bio_async_connected(const nimcp_lc_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LC_PLASTICITY_BRIDGE_H */
