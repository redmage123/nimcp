/**
 * @file nimcp_habenula_hub_bridge.h
 * @brief Habenula - Central Hub Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between Habenula and central hub
 * WHY:  Enable habenula-mediated hub modulation for aversive processing
 * HOW:  Habenula affects hub connectivity during negative states
 *
 * THEORETICAL FOUNDATIONS:
 * - Hikosaka (2010): Habenula in decision circuits
 * - Proulx et al. (2014): Habenula and behavioral control
 * - Lawson et al. (2017): Aversive learning networks
 *
 * BIOLOGICAL BASIS:
 * - Habenula connects to prefrontal and limbic hubs
 * - Negative outcomes reshape hub connectivity
 * - Avoidance behavior requires hub coordination
 * - Depression involves altered hub dynamics
 *
 * INTEGRATION FLOWS:
 *
 * Habenula --> Hub:
 *   1. Disappointment modulates hub connectivity
 *   2. Aversive state shapes processing priorities
 *   3. Avoidance mode affects hub configuration
 *   4. Negative PE inhibits reward-related hubs
 *
 * Hub --> Habenula:
 *   1. Conflict requiring resolution drives habenula
 *   2. Decision outcomes feed back to habenula
 *   3. Hub state indicates processing context
 *   4. Goal failure activates habenula
 *
 * @see nimcp_habenula.h
 * @see nimcp_central_hub.h
 */

#ifndef NIMCP_HABENULA_HUB_BRIDGE_H
#define NIMCP_HABENULA_HUB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_habenula_adapter_struct;
typedef struct nimcp_habenula_adapter_struct* nimcp_habenula_adapter_t;
struct nimcp_central_hub;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default aversive connectivity gain */
#define HAB_HUB_AVERSIVE_GAIN           0.8f

/** @brief Maximum hub regions */
#define HAB_HUB_MAX_REGIONS             16

/** @brief Bio-async module ID */
#define BIO_MODULE_HAB_HUB_BRIDGE       0x0F40

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Hub modulation mode
 */
typedef enum {
    HAB_HUB_MODE_NEUTRAL = 0,        /**< Neutral modulation */
    HAB_HUB_MODE_AVERSIVE,           /**< Aversive-focused */
    HAB_HUB_MODE_AVOIDANT,           /**< Avoidance mode */
    HAB_HUB_MODE_DEPRESSED           /**< Depression-like pattern */
} nimcp_hab_hub_mode_t;

/**
 * @brief Goal outcome state
 */
typedef enum {
    HAB_HUB_GOAL_PENDING = 0,        /**< Goal in progress */
    HAB_HUB_GOAL_ACHIEVED,           /**< Goal achieved */
    HAB_HUB_GOAL_FAILED,             /**< Goal failed */
    HAB_HUB_GOAL_ABANDONED           /**< Goal abandoned */
} nimcp_hab_hub_goal_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Habenula-Hub bridge configuration
 */
typedef struct {
    /* Aversive modulation */
    nimcp_hab_hub_mode_t default_mode;
    float aversive_gain;             /**< Aversive state effect */
    float reward_suppression;        /**< Reward hub suppression */

    /* Avoidance */
    bool enable_avoidance_mode;      /**< Enable avoidance patterns */
    float avoidance_threshold;       /**< Threshold for avoidance */

    /* Goal processing */
    float failure_gain;              /**< Goal failure effect */
    float disappointment_gain;       /**< Disappointment effect */

    /* Feedback */
    float conflict_gain;             /**< Conflict effect on habenula */
    float decision_feedback_gain;    /**< Decision outcome feedback */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_hab_hub_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Hub connectivity modulation
 */
typedef struct {
    nimcp_hab_hub_mode_t mode;       /**< Current mode */
    float connectivity_factor;       /**< Overall connectivity */
    float hub_weights[HAB_HUB_MAX_REGIONS]; /**< Per-hub weights */
    float reward_suppression;        /**< Reward hub suppression */
    float aversive_enhancement;      /**< Aversive hub enhancement */
} nimcp_hab_hub_modulation_t;

/**
 * @brief Hub feedback to Habenula
 */
typedef struct {
    float conflict_level;            /**< Decision conflict */
    nimcp_hab_hub_goal_t goal_state; /**< Goal outcome */
    float goal_value;                /**< Expected goal value */
    float outcome_value;             /**< Actual outcome value */
    bool failure_detected;           /**< Goal failure signal */
} nimcp_hab_hub_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_hab_hub_mode_t current_mode;
    float current_connectivity;
    float accumulated_failure;
    nimcp_hab_hub_goal_t last_goal_state;
    bool in_avoidance;
} nimcp_hab_hub_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t goal_failures;
    uint64_t avoidance_activations;
    uint64_t mode_changes;
    float avg_reward_suppression;
    float avg_conflict;
    float time_in_avoidance;
} nimcp_hab_hub_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_hab_hub_bridge nimcp_hab_hub_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_hab_hub_config_t nimcp_hab_hub_config_default(void);

nimcp_hab_hub_bridge_t* nimcp_hab_hub_create(
    const nimcp_hab_hub_config_t* config
);

void nimcp_hab_hub_destroy(nimcp_hab_hub_bridge_t* bridge);

int nimcp_hab_hub_reset(nimcp_hab_hub_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_hab_hub_connect_habenula(
    nimcp_hab_hub_bridge_t* bridge,
    nimcp_habenula_adapter_t hab_adapter
);

int nimcp_hab_hub_connect_hub(
    nimcp_hab_hub_bridge_t* bridge,
    struct nimcp_central_hub* hub
);

/*=============================================================================
 * Habenula --> Hub API
 *===========================================================================*/

/**
 * @brief Compute hub modulation from habenula state
 */
int nimcp_hab_hub_compute_modulation(
    nimcp_hab_hub_bridge_t* bridge,
    nimcp_hab_hub_modulation_t* modulation
);

/**
 * @brief Set modulation mode
 */
int nimcp_hab_hub_set_mode(
    nimcp_hab_hub_bridge_t* bridge,
    nimcp_hab_hub_mode_t mode
);

/**
 * @brief Apply reward suppression
 */
int nimcp_hab_hub_suppress_reward(
    nimcp_hab_hub_bridge_t* bridge,
    float suppression_level
);

/**
 * @brief Get hub weight for specific region
 */
float nimcp_hab_hub_get_hub_weight(
    nimcp_hab_hub_bridge_t* bridge,
    uint32_t hub_index
);

/*=============================================================================
 * Hub --> Habenula API
 *===========================================================================*/

/**
 * @brief Receive hub feedback
 */
int nimcp_hab_hub_receive_feedback(
    nimcp_hab_hub_bridge_t* bridge,
    const nimcp_hab_hub_feedback_t* feedback
);

/**
 * @brief Process goal failure
 */
int nimcp_hab_hub_process_failure(
    nimcp_hab_hub_bridge_t* bridge,
    float failure_magnitude
);

/**
 * @brief Get habenula response to hub state
 */
float nimcp_hab_hub_get_hab_response(nimcp_hab_hub_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_hab_hub_update(nimcp_hab_hub_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_hab_hub_get_state(
    const nimcp_hab_hub_bridge_t* bridge,
    nimcp_hab_hub_bridge_state_t* state
);

int nimcp_hab_hub_get_stats(
    const nimcp_hab_hub_bridge_t* bridge,
    nimcp_hab_hub_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HABENULA_HUB_BRIDGE_H */
