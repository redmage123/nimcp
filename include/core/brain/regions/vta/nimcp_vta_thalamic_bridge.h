/**
 * @file nimcp_vta_thalamic_bridge.h
 * @brief Ventral Tegmental Area - Thalamic Relay Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between VTA (dopamine) and thalamic systems
 * WHY:  Enable DA-mediated thalamic gating for reward-relevant processing
 * HOW:  DA modulates thalamic filtering; thalamic state affects DA release
 *
 * THEORETICAL FOUNDATIONS:
 * - Haber & Knutson (2010): Basal ganglia-thalamic circuits
 * - Saalmann (2014): Cognitive thalamic control by DA
 * - Shine (2019): Neuromodulation of thalamocortical dynamics
 *
 * BIOLOGICAL BASIS:
 * - DA affects mediodorsal thalamic nucleus (MD)
 * - Reward-relevant information prioritized via DA
 * - Thalamic relay adjusts based on motivational state
 * - Goal-relevant sensory filtering via DA-thalamic interaction
 *
 * INTEGRATION FLOWS:
 *
 * VTA --> Thalamus:
 *   1. DA level modulates reward-relevant relay gain
 *   2. Incentive salience prioritizes certain inputs
 *   3. Motivational state shapes thalamic filtering
 *   4. Phasic DA signals important events
 *
 * Thalamus --> VTA:
 *   1. Novel sensory input triggers DA response
 *   2. Thalamic synchrony indicates processing state
 *   3. Reward-predictive cues route through thalamus
 *   4. Thalamic gating affects DA firing
 *
 * @see nimcp_vta.h
 * @see nimcp_thalamic_relay.h
 */

#ifndef NIMCP_VTA_THALAMIC_BRIDGE_H
#define NIMCP_VTA_THALAMIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_vta_adapter_struct;
typedef struct nimcp_vta_adapter_struct* nimcp_vta_adapter_t;
struct nimcp_thalamic_relay;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default reward relay gain */
#define VTA_THAL_REWARD_GAIN            1.5f

/** @brief Maximum relay enhancement */
#define VTA_THAL_RELAY_MAX              3.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_VTA_THALAMIC_BRIDGE  0x0D20

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Thalamic filtering mode for DA
 */
typedef enum {
    VTA_THAL_FILTER_NEUTRAL = 0,     /**< Neutral filtering */
    VTA_THAL_FILTER_REWARD,          /**< Reward-biased filtering */
    VTA_THAL_FILTER_AVERSIVE,        /**< Aversion-biased filtering */
    VTA_THAL_FILTER_GOAL             /**< Goal-directed filtering */
} nimcp_vta_thal_filter_t;

/**
 * @brief Relay priority level
 */
typedef enum {
    VTA_THAL_PRIORITY_LOW = 0,       /**< Low priority relay */
    VTA_THAL_PRIORITY_NORMAL,        /**< Normal priority */
    VTA_THAL_PRIORITY_HIGH,          /**< High priority (rewarding) */
    VTA_THAL_PRIORITY_URGENT         /**< Urgent (salient reward) */
} nimcp_vta_thal_priority_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief VTA-Thalamic bridge configuration
 */
typedef struct {
    /* Reward filtering */
    nimcp_vta_thal_filter_t default_filter;
    float reward_gain;               /**< Gain for reward-relevant relay */
    float salience_threshold;        /**< Threshold for priority boost */

    /* Relay modulation */
    float relay_min;                 /**< Minimum relay gain */
    float relay_max;                 /**< Maximum relay gain */
    float da_relay_sensitivity;      /**< DA-to-relay sensitivity */

    /* Feedback */
    float novel_cue_gain;            /**< Novel cue effect on DA */
    float predictive_cue_gain;       /**< Predictive cue effect */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_vta_thal_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Thalamic modulation output
 */
typedef struct {
    nimcp_vta_thal_filter_t filter;  /**< Current filter mode */
    nimcp_vta_thal_priority_t priority; /**< Current priority */
    float relay_gain;                /**< Overall relay gain */
    float reward_channel_boost;      /**< Reward channel enhancement */
    float goal_relevance;            /**< Goal-relevance weighting */
} nimcp_vta_thal_modulation_t;

/**
 * @brief Thalamic feedback to VTA
 */
typedef struct {
    float novel_input_strength;      /**< Novelty of current input */
    float predictive_cue_strength;   /**< Strength of predictive cues */
    float processing_load;           /**< Thalamic processing load */
    float synchrony;                 /**< Thalamic synchrony */
    bool reward_predictive_cue;      /**< Reward-predictive cue detected */
} nimcp_vta_thal_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_vta_thal_filter_t current_filter;
    nimcp_vta_thal_priority_t current_priority;
    float current_relay_gain;
    float accumulated_novelty;
    float accumulated_prediction;
} nimcp_vta_thal_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t priority_boosts;
    uint64_t filter_changes;
    float avg_relay_gain;
    float avg_novelty;
    float time_in_reward_filter;
} nimcp_vta_thal_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_vta_thal_bridge nimcp_vta_thal_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_vta_thal_config_t nimcp_vta_thal_config_default(void);

nimcp_vta_thal_bridge_t* nimcp_vta_thal_create(
    const nimcp_vta_thal_config_t* config
);

void nimcp_vta_thal_destroy(nimcp_vta_thal_bridge_t* bridge);

int nimcp_vta_thal_reset(nimcp_vta_thal_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_vta_thal_connect_vta(
    nimcp_vta_thal_bridge_t* bridge,
    nimcp_vta_adapter_t vta_adapter
);

int nimcp_vta_thal_connect_thalamus(
    nimcp_vta_thal_bridge_t* bridge,
    struct nimcp_thalamic_relay* thalamus
);

/*=============================================================================
 * VTA --> Thalamus API
 *===========================================================================*/

/**
 * @brief Compute thalamic modulation from DA state
 */
int nimcp_vta_thal_compute_modulation(
    nimcp_vta_thal_bridge_t* bridge,
    nimcp_vta_thal_modulation_t* modulation
);

/**
 * @brief Set filter mode based on motivational state
 */
int nimcp_vta_thal_set_filter(
    nimcp_vta_thal_bridge_t* bridge,
    nimcp_vta_thal_filter_t filter
);

/**
 * @brief Boost priority for reward-relevant input
 */
int nimcp_vta_thal_boost_priority(
    nimcp_vta_thal_bridge_t* bridge,
    float salience
);

/*=============================================================================
 * Thalamus --> VTA API
 *===========================================================================*/

/**
 * @brief Receive thalamic feedback
 */
int nimcp_vta_thal_receive_feedback(
    nimcp_vta_thal_bridge_t* bridge,
    const nimcp_vta_thal_feedback_t* feedback
);

/**
 * @brief Process reward-predictive cue
 */
int nimcp_vta_thal_process_predictive_cue(
    nimcp_vta_thal_bridge_t* bridge,
    float cue_strength
);

/**
 * @brief Get DA response to thalamic state
 */
float nimcp_vta_thal_get_da_response(nimcp_vta_thal_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_vta_thal_update(nimcp_vta_thal_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_vta_thal_get_state(
    const nimcp_vta_thal_bridge_t* bridge,
    nimcp_vta_thal_bridge_state_t* state
);

int nimcp_vta_thal_get_stats(
    const nimcp_vta_thal_bridge_t* bridge,
    nimcp_vta_thal_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VTA_THALAMIC_BRIDGE_H */
