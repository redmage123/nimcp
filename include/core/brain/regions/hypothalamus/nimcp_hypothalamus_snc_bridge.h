/**
 * @file nimcp_hypothalamus_snc_bridge.h
 * @brief Hypothalamus -> SNc/VTA Bridge for Reward → Dopamine Conversion
 *
 * WHAT: Bridge between hypothalamus reward signals and SNc/VTA dopamine system
 * WHY:  Convert alignment-aware reward into dopamine RPE for learning (Byrnes model)
 * HOW:  Receives reward from hypothalamus, computes RPE, broadcasts dopamine
 *
 * BYRNES MODEL CONTEXT:
 * The steering subsystem (hypothalamus) generates reward signals based on:
 * - Setpoint deviations (homeostatic drives)
 * - Alignment weights (human wellbeing, harm avoidance, honesty)
 *
 * This bridge converts these rewards to dopamine signals that modulate learning:
 * - SNc (Substantia Nigra pars compacta): Motor learning, habit formation
 * - VTA (Ventral Tegmental Area): Motivation, reinforcement, value learning
 *
 * RPE COMPUTATION:
 * δ(t) = r(t) + γ·V(s') - V(s)    # Temporal difference RPE
 * Where:
 * - r(t) = reward from hypothalamus (including alignment components)
 * - V(s) = predicted value (from world model / dopamine prediction)
 * - γ = discount factor
 *
 * BIO-ASYNC MESSAGES:
 * - Receives: BIO_MSG_HYPO_REWARD_SIGNAL (from hypothalamus)
 * - Sends: BIO_MSG_SNc_DOPAMINE_BURST, BIO_MSG_SNc_RPE
 *
 * @version Phase 4: SNc/VTA Bridge
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_SNC_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_SNC_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives_bio.h"
#include "async/nimcp_bio_async.h"
#include "utils/bridge/nimcp_bridge_base.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum number of reward channels (one per drive + alignment) */
#define HYPO_SNC_MAX_CHANNELS   (HYPO_DRIVE_COUNT + 4)  /* +4 for alignment weights */

/** Default TD discount factor (gamma) */
#define HYPO_SNC_DEFAULT_GAMMA  0.99f

/** Dopamine baseline tonic level */
#define HYPO_SNC_TONIC_BASELINE 0.5f

/** Dopamine burst maximum */
#define HYPO_SNC_BURST_MAX      1.0f

/** Dopamine dip minimum */
#define HYPO_SNC_DIP_MIN        0.0f

/*=============================================================================
 * DOPAMINE CHANNEL TYPES
 *===========================================================================*/

/**
 * @brief Dopamine output channel types
 *
 * Different dopamine pathways have different targets and effects:
 * - Nigrostriatal (SNc): Motor control, habit learning
 * - Mesolimbic (VTA→NAc): Motivation, wanting, reinforcement
 * - Mesocortical (VTA→PFC): Cognition, working memory, executive
 */
typedef enum {
    HYPO_DA_CHANNEL_SNc_MOTOR = 0,   /**< SNc → Striatum (motor) */
    HYPO_DA_CHANNEL_VTA_NAc,          /**< VTA → NAc (motivation/wanting) */
    HYPO_DA_CHANNEL_VTA_PFC,          /**< VTA → PFC (cognitive) */
    HYPO_DA_CHANNEL_VTA_AMYGDALA,     /**< VTA → Amygdala (emotional) */
    HYPO_DA_CHANNEL_VTA_HIPPO,        /**< VTA → Hippocampus (memory) */
    HYPO_DA_CHANNEL_COUNT
} hypo_da_channel_t;

/**
 * @brief Dopamine signal type (burst, dip, or tonic)
 */
typedef enum {
    HYPO_DA_SIGNAL_TONIC = 0,    /**< Baseline tonic firing */
    HYPO_DA_SIGNAL_BURST,        /**< Phasic burst (reward > expected) */
    HYPO_DA_SIGNAL_DIP,          /**< Phasic dip (reward < expected) */
    HYPO_DA_SIGNAL_SUSTAINED     /**< Sustained elevation (chronic drive) */
} hypo_da_signal_type_t;

/*=============================================================================
 * REWARD PREDICTION ERROR (RPE) STRUCTURE
 *===========================================================================*/

/**
 * @brief Reward Prediction Error components
 *
 * RPE = actual_reward - predicted_reward
 * Used for TD learning and dopamine modulation
 */
typedef struct {
    float actual_reward;         /**< Reward from hypothalamus */
    float predicted_reward;      /**< Predicted from value estimate */
    float rpe;                   /**< RPE = actual - predicted */
    float td_error;              /**< Full TD error with discount */

    /* Component breakdown */
    float drive_component;       /**< RPE from drive satisfaction */
    float alignment_component;   /**< RPE from alignment (safety critical) */
    float curiosity_component;   /**< RPE from epistemic value */

    hypo_drive_type_t primary_drive;  /**< Which drive dominated */
    uint64_t timestamp_us;
} hypo_rpe_t;

/*=============================================================================
 * DOPAMINE STATE STRUCTURE
 *===========================================================================*/

/**
 * @brief Per-channel dopamine state
 */
typedef struct {
    hypo_da_channel_t channel;   /**< Which channel */
    float level;                 /**< Current dopamine level [0, 1] */
    float tonic_baseline;        /**< Tonic baseline for this channel */
    hypo_da_signal_type_t type;  /**< Current signal type */
    float decay_rate;            /**< How fast phasic returns to tonic */
    uint64_t last_update_us;     /**< Last update timestamp */
} hypo_da_channel_state_t;

/**
 * @brief Overall dopamine system state
 */
typedef struct {
    hypo_da_channel_state_t channels[HYPO_DA_CHANNEL_COUNT];
    float global_tonic_level;    /**< Global tonic baseline */
    float global_gain;           /**< Multiplier from arousal */

    /* Value prediction (for RPE computation) */
    float value_estimate;        /**< Current state value V(s) */
    float next_value_estimate;   /**< Next state value V(s') */
    float discount_gamma;        /**< TD discount factor */

    /* Statistics */
    uint64_t burst_count;
    uint64_t dip_count;
    float avg_rpe;
} hypo_dopamine_state_t;

/*=============================================================================
 * BRIDGE CONTEXT
 *===========================================================================*/

/**
 * @brief SNc/VTA bridge configuration
 */
typedef struct {
    float discount_gamma;        /**< TD discount factor */
    float rpe_threshold;         /**< Min RPE magnitude for phasic response */
    float burst_magnitude;       /**< How much DA increases on positive RPE */
    float dip_magnitude;         /**< How much DA decreases on negative RPE */
    float decay_rate;            /**< Phasic decay rate */

    /* Channel-specific gains */
    float channel_gains[HYPO_DA_CHANNEL_COUNT];

    /* Alignment weight for DA modulation */
    float alignment_sensitivity; /**< How much alignment affects DA */

    /* Integration options */
    bool use_external_snc;       /**< Connect to external SNc module if available */
    bool broadcast_enabled;      /**< Enable bio-async broadcasts */
} hypo_snc_bridge_config_t;

/**
 * @brief SNc/VTA bridge context
 */
typedef struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    hypo_snc_bridge_config_t config;

    /* State */
    hypo_dopamine_state_t dopamine;
    hypo_rpe_t last_rpe;

    /* Connected modules */
    hypo_drive_system_handle_t* drives;  /**< Hypothalamus drives (source) */
    void* external_snc;                  /**< External SNc module (optional) */

    /* Bio-async context */
    bio_module_context_t bio_ctx;

    /* Statistics */
    uint64_t rewards_processed;
    uint64_t broadcasts_sent;
    float cumulative_reward;
    float cumulative_alignment_reward;

} hypo_snc_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default SNc bridge configuration
 *
 * @return Default config with biologically plausible values
 */
hypo_snc_bridge_config_t hypo_snc_bridge_default_config(void);

/**
 * @brief Create SNc/VTA bridge
 *
 * @param drives Hypothalamus drive system handle
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge context, or NULL on failure
 */
hypo_snc_bridge_t* hypo_snc_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_snc_bridge_config_t* config);

/**
 * @brief Destroy SNc/VTA bridge
 *
 * @param bridge Bridge to destroy
 */
void hypo_snc_bridge_destroy(hypo_snc_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge to reset
 */
void hypo_snc_bridge_reset(hypo_snc_bridge_t* bridge);

/*=============================================================================
 * CORE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Process reward signal from hypothalamus
 *
 * WHAT: Convert hypothalamic reward to dopamine/RPE
 * WHY:  Central function for reward → learning signal conversion
 * HOW:  Computes RPE, updates dopamine state, broadcasts
 *
 * @param bridge Bridge context
 * @param reward Reward signal from hypothalamus
 * @return Computed RPE
 */
hypo_rpe_t hypo_snc_bridge_process_reward(
    hypo_snc_bridge_t* bridge,
    const hypo_reward_signal_t* reward);

/**
 * @brief Update value prediction for next RPE computation
 *
 * Called by external world model to provide V(s') for TD error
 *
 * @param bridge Bridge context
 * @param next_value Predicted value of next state
 */
void hypo_snc_bridge_update_value_prediction(
    hypo_snc_bridge_t* bridge,
    float next_value);

/**
 * @brief Step dopamine dynamics (decay phasic responses)
 *
 * @param bridge Bridge context
 * @param dt_us Time step in microseconds
 */
void hypo_snc_bridge_step(hypo_snc_bridge_t* bridge, uint64_t dt_us);

/*=============================================================================
 * DOPAMINE ACCESSORS
 *===========================================================================*/

/**
 * @brief Get current dopamine level for a channel
 *
 * @param bridge Bridge context
 * @param channel Which dopamine channel
 * @return Dopamine level [0, 1]
 */
float hypo_snc_bridge_get_dopamine(
    const hypo_snc_bridge_t* bridge,
    hypo_da_channel_t channel);

/**
 * @brief Get dopamine signal type for a channel
 *
 * @param bridge Bridge context
 * @param channel Which dopamine channel
 * @return Current signal type (tonic/burst/dip)
 */
hypo_da_signal_type_t hypo_snc_bridge_get_signal_type(
    const hypo_snc_bridge_t* bridge,
    hypo_da_channel_t channel);

/**
 * @brief Get global dopamine level (average across channels)
 *
 * @param bridge Bridge context
 * @return Global dopamine level [0, 1]
 */
float hypo_snc_bridge_get_global_dopamine(const hypo_snc_bridge_t* bridge);

/**
 * @brief Get last computed RPE
 *
 * @param bridge Bridge context
 * @return Pointer to last RPE (internal, do not free)
 */
const hypo_rpe_t* hypo_snc_bridge_get_last_rpe(const hypo_snc_bridge_t* bridge);

/*=============================================================================
 * MODULATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Modulate tonic baseline (arousal/stress effects)
 *
 * High arousal increases tonic dopamine, stress can decrease it
 *
 * @param bridge Bridge context
 * @param arousal_level Arousal from 0 (low) to 1 (high)
 * @param stress_level Stress from 0 (none) to 1 (severe)
 */
void hypo_snc_bridge_modulate_tonic(
    hypo_snc_bridge_t* bridge,
    float arousal_level,
    float stress_level);

/**
 * @brief Set channel-specific gain
 *
 * @param bridge Bridge context
 * @param channel Which channel
 * @param gain Gain multiplier (default 1.0)
 */
void hypo_snc_bridge_set_channel_gain(
    hypo_snc_bridge_t* bridge,
    hypo_da_channel_t channel,
    float gain);

/*=============================================================================
 * EXTERNAL SNc INTEGRATION
 *===========================================================================*/

/**
 * @brief Connect to external SNc module (when available)
 *
 * @param bridge Bridge context
 * @param snc_module External SNc module handle
 * @return true if connected
 */
bool hypo_snc_bridge_connect_snc(
    hypo_snc_bridge_t* bridge,
    void* snc_module);

/**
 * @brief Disconnect from external SNc module
 *
 * @param bridge Bridge context
 */
void hypo_snc_bridge_disconnect_snc(hypo_snc_bridge_t* bridge);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge context
 * @param use_kg_wiring Use KG-driven wiring (true) or legacy (false)
 * @return true on success
 */
bool hypo_snc_bridge_register_bio(
    hypo_snc_bridge_t* bridge,
    bool use_kg_wiring);

/**
 * @brief Process incoming bio-async messages
 *
 * @param bridge Bridge context
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
uint32_t hypo_snc_bridge_process_bio(
    hypo_snc_bridge_t* bridge,
    uint32_t max_messages);

/**
 * @brief Broadcast dopamine state
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_snc_bridge_broadcast_dopamine(hypo_snc_bridge_t* bridge);

/**
 * @brief Broadcast RPE
 *
 * @param bridge Bridge context
 * @param rpe RPE to broadcast
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_snc_bridge_broadcast_rpe(
    hypo_snc_bridge_t* bridge,
    const hypo_rpe_t* rpe);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge context
 * @param rewards_processed Output: total rewards processed
 * @param avg_rpe Output: average RPE
 * @param burst_count Output: number of DA bursts
 * @param dip_count Output: number of DA dips
 */
void hypo_snc_bridge_get_stats(
    const hypo_snc_bridge_t* bridge,
    uint64_t* rewards_processed,
    float* avg_rpe,
    uint64_t* burst_count,
    uint64_t* dip_count);

/**
 * @brief Get cumulative reward statistics
 *
 * @param bridge Bridge context
 * @param total_reward Output: cumulative total reward
 * @param alignment_reward Output: cumulative alignment component
 */
void hypo_snc_bridge_get_reward_stats(
    const hypo_snc_bridge_t* bridge,
    float* total_reward,
    float* alignment_reward);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_SNC_BRIDGE_H */
