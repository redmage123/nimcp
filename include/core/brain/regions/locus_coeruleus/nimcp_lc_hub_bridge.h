/**
 * @file nimcp_lc_hub_bridge.h
 * @brief Locus Coeruleus - Central Hub Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between LC and central processing hub
 * WHY:  Enable NE-mediated global coordination and hub feedback
 * HOW:  LC modulates hub connectivity; hub state drives LC mode
 *
 * THEORETICAL FOUNDATIONS:
 * - Bouret & Sara (2005): LC-NE system and network reset
 * - van den Heuvel & Sporns (2013): Hub connectivity in brain networks
 * - Corbetta et al. (2008): Attention networks and LC involvement
 *
 * BIOLOGICAL BASIS:
 * - LC projects to major hub regions (PFC, parietal, cingulate)
 * - NE enhances hub-to-hub communication during attention
 * - Phasic LC activity resets network configurations
 * - Hub engagement triggers tonic-phasic transitions
 *
 * INTEGRATION FLOWS:
 *
 * LC --> Hub:
 *   1. NE level modulates hub connectivity strength
 *   2. Arousal state gates hub processing mode
 *   3. Phasic bursts trigger network reconfiguration
 *   4. Exploration drive affects hub flexibility
 *
 * Hub --> LC:
 *   1. Hub load indicates processing demands
 *   2. Network conflicts trigger attention allocation
 *   3. Task switches drive phasic responses
 *   4. Hub synchrony feeds back to tonic level
 *
 * @see nimcp_locus_coeruleus.h
 * @see nimcp_central_hub.h
 */

#ifndef NIMCP_LC_HUB_BRIDGE_H
#define NIMCP_LC_HUB_BRIDGE_H

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
struct nimcp_central_hub;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default connectivity modulation gain */
#define LC_HUB_CONNECTIVITY_GAIN        1.5f

/** @brief Maximum hub regions tracked */
#define LC_HUB_MAX_REGIONS              16

/** @brief Bio-async module ID */
#define BIO_MODULE_LC_HUB_BRIDGE        0x0C40

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Hub processing mode
 */
typedef enum {
    LC_HUB_MODE_INTEGRATED = 0,      /**< Global integration mode */
    LC_HUB_MODE_SEGREGATED,          /**< Local segregation mode */
    LC_HUB_MODE_TRANSITION,          /**< Transitioning between modes */
    LC_HUB_MODE_FLEXIBLE             /**< Dynamic/flexible mode */
} nimcp_lc_hub_mode_t;

/**
 * @brief Network state requiring LC response
 */
typedef enum {
    LC_HUB_NET_STABLE = 0,           /**< Stable network state */
    LC_HUB_NET_CONFLICTING,          /**< Competing network activations */
    LC_HUB_NET_SWITCHING,            /**< Task/network switching */
    LC_HUB_NET_OVERLOADED            /**< Processing overload */
} nimcp_lc_hub_net_state_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief LC-Hub bridge configuration
 */
typedef struct {
    /* Connectivity modulation */
    float connectivity_gain;         /**< NE-to-connectivity gain */
    float integration_threshold;     /**< NE threshold for integration mode */
    float segregation_threshold;     /**< NE threshold for segregation */

    /* Network reset */
    bool enable_network_reset;       /**< Enable phasic reset */
    float reset_threshold;           /**< Threshold for reset trigger */
    float reset_duration_ms;         /**< Reset duration */

    /* Flexibility */
    float flexibility_baseline;      /**< Baseline network flexibility */
    float exploration_flexibility;   /**< Flexibility in exploration mode */

    /* Feedback */
    float load_gain;                 /**< Hub load effect on LC */
    float conflict_gain;             /**< Network conflict effect */
    float switch_gain;               /**< Task switch effect */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_lc_hub_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Hub connectivity modulation output
 */
typedef struct {
    nimcp_lc_hub_mode_t mode;        /**< Current hub mode */
    float connectivity_strength;     /**< Global connectivity factor */
    float hub_gain[LC_HUB_MAX_REGIONS]; /**< Per-hub gain factors */
    float network_flexibility;       /**< Network reconfiguration ease */
    bool reset_triggered;            /**< Network reset in progress */
} nimcp_lc_hub_modulation_t;

/**
 * @brief Hub feedback to LC
 */
typedef struct {
    nimcp_lc_hub_net_state_t network_state; /**< Current network state */
    float processing_load;           /**< Hub processing load [0-1] */
    float network_synchrony;         /**< Cross-hub synchrony [0-1] */
    float task_demand;               /**< Current task demands [0-1] */
    bool conflict_detected;          /**< Network conflict present */
    bool task_switch;                /**< Task switch occurring */
} nimcp_lc_hub_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_lc_hub_mode_t current_mode;
    float current_connectivity;
    float current_flexibility;
    float accumulated_load;
    bool reset_in_progress;
    float time_since_reset;
} nimcp_lc_hub_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t mode_transitions;
    uint64_t network_resets;
    uint64_t conflict_events;
    uint64_t task_switches;
    float avg_connectivity;
    float avg_load;
    float time_in_integrated;
    float time_in_segregated;
} nimcp_lc_hub_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_lc_hub_bridge nimcp_lc_hub_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_lc_hub_config_t nimcp_lc_hub_config_default(void);

nimcp_lc_hub_bridge_t* nimcp_lc_hub_create(const nimcp_lc_hub_config_t* config);

void nimcp_lc_hub_destroy(nimcp_lc_hub_bridge_t* bridge);

int nimcp_lc_hub_reset(nimcp_lc_hub_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_lc_hub_connect_lc(
    nimcp_lc_hub_bridge_t* bridge,
    nimcp_lc_adapter_t lc_adapter
);

int nimcp_lc_hub_connect_hub(
    nimcp_lc_hub_bridge_t* bridge,
    struct nimcp_central_hub* hub
);

/*=============================================================================
 * LC --> Hub API
 *===========================================================================*/

/**
 * @brief Compute hub connectivity modulation from LC state
 */
int nimcp_lc_hub_compute_modulation(
    nimcp_lc_hub_bridge_t* bridge,
    nimcp_lc_hub_modulation_t* modulation
);

/**
 * @brief Trigger network reconfiguration
 */
int nimcp_lc_hub_trigger_reset(nimcp_lc_hub_bridge_t* bridge);

/**
 * @brief Set hub mode based on NE level
 */
int nimcp_lc_hub_set_mode(
    nimcp_lc_hub_bridge_t* bridge,
    float ne_concentration
);

/**
 * @brief Get connectivity strength for specific hub
 */
float nimcp_lc_hub_get_region_gain(
    nimcp_lc_hub_bridge_t* bridge,
    uint32_t hub_index
);

/*=============================================================================
 * Hub --> LC API
 *===========================================================================*/

/**
 * @brief Receive hub feedback
 */
int nimcp_lc_hub_receive_feedback(
    nimcp_lc_hub_bridge_t* bridge,
    const nimcp_lc_hub_feedback_t* feedback
);

/**
 * @brief Get recommended LC response
 */
float nimcp_lc_hub_get_lc_response(nimcp_lc_hub_bridge_t* bridge);

/**
 * @brief Check if phasic response recommended
 */
bool nimcp_lc_hub_should_trigger_phasic(nimcp_lc_hub_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_lc_hub_update(nimcp_lc_hub_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_lc_hub_get_state(
    const nimcp_lc_hub_bridge_t* bridge,
    nimcp_lc_hub_bridge_state_t* state
);

int nimcp_lc_hub_get_stats(
    const nimcp_lc_hub_bridge_t* bridge,
    nimcp_lc_hub_stats_t* stats
);

nimcp_lc_hub_mode_t nimcp_lc_hub_get_mode(const nimcp_lc_hub_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LC_HUB_BRIDGE_H */
