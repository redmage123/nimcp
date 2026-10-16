/**
 * @file nimcp_vta_hub_bridge.h
 * @brief Ventral Tegmental Area - Central Hub Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between VTA (dopamine) and central hub
 * WHY:  Enable DA-mediated hub prioritization for reward-relevant processing
 * HOW:  DA modulates hub connectivity for goals; hub activity drives DA
 *
 * THEORETICAL FOUNDATIONS:
 * - Haber (2016): Cortico-basal ganglia circuits
 * - Cole et al. (2013): Cognitive control and DA
 * - Westbrook & Braver (2016): DA and cognitive effort
 *
 * BIOLOGICAL BASIS:
 * - DA projections to PFC and other hub regions
 * - Motivational salience shapes hub prioritization
 * - Goal-directed behavior requires DA-hub coordination
 * - Cognitive effort allocation via DA signaling
 *
 * INTEGRATION FLOWS:
 *
 * VTA --> Hub:
 *   1. DA level modulates hub-to-hub connectivity
 *   2. Incentive salience prioritizes goal-relevant hubs
 *   3. Motivational vigor affects processing intensity
 *   4. RPE signals update hub configurations
 *
 * Hub --> VTA:
 *   1. Cognitive demand signals affect DA
 *   2. Goal achievement triggers phasic DA
 *   3. Hub conflict may require DA arbitration
 *   4. Effort allocation feeds back to DA
 *
 * @see nimcp_vta.h
 * @see nimcp_central_hub.h
 */

#ifndef NIMCP_VTA_HUB_BRIDGE_H
#define NIMCP_VTA_HUB_BRIDGE_H

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
struct nimcp_central_hub;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default motivation-connectivity gain */
#define VTA_HUB_MOTIVATION_GAIN         1.2f

/** @brief Maximum hub regions */
#define VTA_HUB_MAX_REGIONS             16

/** @brief Bio-async module ID */
#define BIO_MODULE_VTA_HUB_BRIDGE       0x0D40

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Hub prioritization mode
 */
typedef enum {
    VTA_HUB_PRIORITY_NEUTRAL = 0,    /**< Neutral prioritization */
    VTA_HUB_PRIORITY_GOAL,           /**< Goal-directed priority */
    VTA_HUB_PRIORITY_REWARD,         /**< Reward-seeking priority */
    VTA_HUB_PRIORITY_EFFORT          /**< Effort-allocation priority */
} nimcp_vta_hub_priority_t;

/**
 * @brief Cognitive effort level
 */
typedef enum {
    VTA_HUB_EFFORT_LOW = 0,          /**< Low cognitive effort */
    VTA_HUB_EFFORT_MODERATE,         /**< Moderate effort */
    VTA_HUB_EFFORT_HIGH,             /**< High effort */
    VTA_HUB_EFFORT_MAXIMAL           /**< Maximum effort */
} nimcp_vta_hub_effort_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief VTA-Hub bridge configuration
 */
typedef struct {
    /* Priority modulation */
    nimcp_vta_hub_priority_t default_priority;
    float motivation_gain;           /**< Motivation-to-connectivity */
    float salience_gain;             /**< Salience priority boost */

    /* Effort allocation */
    bool enable_effort_allocation;   /**< Enable effort modeling */
    float effort_da_coupling;        /**< DA-effort coupling */
    float effort_cost_sensitivity;   /**< Sensitivity to effort costs */

    /* Goal integration */
    float goal_relevance_gain;       /**< Goal-relevance weighting */
    bool enable_subgoal_tracking;    /**< Track subgoal progress */

    /* Feedback */
    float achievement_gain;          /**< Goal achievement DA boost */
    float conflict_gain;             /**< Hub conflict effect */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_vta_hub_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Hub connectivity modulation
 */
typedef struct {
    nimcp_vta_hub_priority_t priority; /**< Current priority mode */
    float connectivity_strength;     /**< Overall connectivity */
    float hub_weights[VTA_HUB_MAX_REGIONS]; /**< Per-hub weights */
    float goal_relevance;            /**< Current goal relevance */
    nimcp_vta_hub_effort_t effort;   /**< Current effort level */
} nimcp_vta_hub_modulation_t;

/**
 * @brief Hub feedback to VTA
 */
typedef struct {
    float cognitive_demand;          /**< Current cognitive demand */
    float goal_progress;             /**< Progress toward goal */
    float effort_required;           /**< Required effort level */
    bool goal_achieved;              /**< Goal achievement signal */
    bool conflict_detected;          /**< Hub conflict present */
    float processing_efficiency;     /**< Hub processing efficiency */
} nimcp_vta_hub_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_vta_hub_priority_t current_priority;
    float current_connectivity;
    nimcp_vta_hub_effort_t current_effort;
    float accumulated_progress;
    float effort_invested;
    bool tracking_goal;
} nimcp_vta_hub_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t goals_achieved;
    uint64_t priority_changes;
    float total_effort;
    float avg_connectivity;
    float avg_goal_progress;
    float goal_achievement_rate;
} nimcp_vta_hub_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_vta_hub_bridge nimcp_vta_hub_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_vta_hub_config_t nimcp_vta_hub_config_default(void);

nimcp_vta_hub_bridge_t* nimcp_vta_hub_create(
    const nimcp_vta_hub_config_t* config
);

void nimcp_vta_hub_destroy(nimcp_vta_hub_bridge_t* bridge);

int nimcp_vta_hub_reset(nimcp_vta_hub_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_vta_hub_connect_vta(
    nimcp_vta_hub_bridge_t* bridge,
    nimcp_vta_adapter_t vta_adapter
);

int nimcp_vta_hub_connect_hub(
    nimcp_vta_hub_bridge_t* bridge,
    struct nimcp_central_hub* hub
);

/*=============================================================================
 * VTA --> Hub API
 *===========================================================================*/

/**
 * @brief Compute hub modulation from DA state
 */
int nimcp_vta_hub_compute_modulation(
    nimcp_vta_hub_bridge_t* bridge,
    nimcp_vta_hub_modulation_t* modulation
);

/**
 * @brief Set hub priority based on motivational state
 */
int nimcp_vta_hub_set_priority(
    nimcp_vta_hub_bridge_t* bridge,
    nimcp_vta_hub_priority_t priority
);

/**
 * @brief Allocate effort to hub processing
 */
int nimcp_vta_hub_allocate_effort(
    nimcp_vta_hub_bridge_t* bridge,
    nimcp_vta_hub_effort_t effort
);

/**
 * @brief Get weight for specific hub
 */
float nimcp_vta_hub_get_hub_weight(
    nimcp_vta_hub_bridge_t* bridge,
    uint32_t hub_index
);

/*=============================================================================
 * Hub --> VTA API
 *===========================================================================*/

/**
 * @brief Receive hub feedback
 */
int nimcp_vta_hub_receive_feedback(
    nimcp_vta_hub_bridge_t* bridge,
    const nimcp_vta_hub_feedback_t* feedback
);

/**
 * @brief Process goal achievement
 */
int nimcp_vta_hub_process_achievement(
    nimcp_vta_hub_bridge_t* bridge,
    float achievement_magnitude
);

/**
 * @brief Get DA response to hub state
 */
float nimcp_vta_hub_get_da_response(nimcp_vta_hub_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_vta_hub_update(nimcp_vta_hub_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_vta_hub_get_state(
    const nimcp_vta_hub_bridge_t* bridge,
    nimcp_vta_hub_bridge_state_t* state
);

int nimcp_vta_hub_get_stats(
    const nimcp_vta_hub_bridge_t* bridge,
    nimcp_vta_hub_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VTA_HUB_BRIDGE_H */
