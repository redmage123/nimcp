/**
 * @file nimcp_raphe_hub_bridge.h
 * @brief Raphe Nuclei - Central Hub Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between Raphe (serotonin) and central hub
 * WHY:  Enable 5-HT-mediated hub modulation for mood and behavioral control
 * HOW:  5-HT modulates hub connectivity; hub state influences Raphe activity
 *
 * THEORETICAL FOUNDATIONS:
 * - Dayan & Huys (2008): Serotonin and behavioral inhibition
 * - Cools et al. (2008): 5-HT and cognitive flexibility
 * - Carhart-Harris & Nutt (2017): 5-HT and default mode network
 *
 * BIOLOGICAL BASIS:
 * - 5-HT projections to PFC and limbic hubs
 * - Mood affects hub-to-hub connectivity patterns
 * - Behavioral inhibition via 5-HT hub modulation
 * - Default mode network influenced by 5-HT
 *
 * INTEGRATION FLOWS:
 *
 * Raphe --> Hub:
 *   1. 5-HT level modulates hub connectivity
 *   2. Mood state shapes processing priorities
 *   3. Impulse control affects hub dynamics
 *   4. Behavioral flexibility via 5-HT
 *
 * Hub --> Raphe:
 *   1. Hub load indicates processing demands
 *   2. Conflict requiring inhibition drives 5-HT
 *   3. DMN activity correlates with mood
 *   4. Rumination patterns affect 5-HT
 *
 * @see nimcp_raphe.h
 * @see nimcp_central_hub.h
 */

#ifndef NIMCP_RAPHE_HUB_BRIDGE_H
#define NIMCP_RAPHE_HUB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_raphe_adapter_struct;
typedef struct nimcp_raphe_adapter_struct* nimcp_raphe_adapter_t;
struct nimcp_central_hub;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default behavioral inhibition gain */
#define RAPHE_HUB_INHIBITION_GAIN       1.0f

/** @brief Maximum hub regions */
#define RAPHE_HUB_MAX_REGIONS           16

/** @brief Bio-async module ID */
#define BIO_MODULE_RAPHE_HUB_BRIDGE     0x0E40

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Hub modulation mode
 */
typedef enum {
    RAPHE_HUB_MODE_NEUTRAL = 0,      /**< Neutral modulation */
    RAPHE_HUB_MODE_INHIBITORY,       /**< Behavioral inhibition */
    RAPHE_HUB_MODE_FLEXIBLE,         /**< Cognitive flexibility */
    RAPHE_HUB_MODE_RUMINATIVE        /**< Rumination pattern */
} nimcp_raphe_hub_mode_t;

/**
 * @brief Default mode network state
 */
typedef enum {
    RAPHE_HUB_DMN_SUPPRESSED = 0,    /**< DMN suppressed (task) */
    RAPHE_HUB_DMN_ACTIVE,            /**< DMN active (rest) */
    RAPHE_HUB_DMN_OVERACTIVE,        /**< DMN overactive (rumination) */
    RAPHE_HUB_DMN_FLEXIBLE           /**< DMN flexible */
} nimcp_raphe_hub_dmn_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Raphe-Hub bridge configuration
 */
typedef struct {
    /* Behavioral inhibition */
    nimcp_raphe_hub_mode_t default_mode;
    float inhibition_gain;           /**< Behavioral inhibition strength */
    float inhibition_threshold;      /**< Threshold for inhibition */

    /* Flexibility */
    float flexibility_gain;          /**< Cognitive flexibility */
    float perseveration_threshold;   /**< Threshold for perseveration */

    /* DMN modulation */
    bool enable_dmn_modulation;      /**< Model DMN effects */
    float dmn_coupling;              /**< DMN-5HT coupling */

    /* Feedback */
    float conflict_gain;             /**< Conflict effect on 5-HT */
    float rumination_gain;           /**< Rumination effect */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_raphe_hub_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Hub connectivity modulation
 */
typedef struct {
    nimcp_raphe_hub_mode_t mode;     /**< Current mode */
    float connectivity_factor;       /**< Overall connectivity */
    float hub_weights[RAPHE_HUB_MAX_REGIONS]; /**< Per-hub weights */
    float inhibition_level;          /**< Behavioral inhibition [0-1] */
    float flexibility_level;         /**< Cognitive flexibility [0-1] */
    nimcp_raphe_hub_dmn_t dmn_state; /**< DMN state */
} nimcp_raphe_hub_modulation_t;

/**
 * @brief Hub feedback to Raphe
 */
typedef struct {
    float processing_load;           /**< Hub processing load */
    float conflict_level;            /**< Response conflict */
    nimcp_raphe_hub_dmn_t dmn_activity; /**< DMN activity state */
    float rumination_index;          /**< Rumination tendency */
    bool inhibition_needed;          /**< Inhibition required */
} nimcp_raphe_hub_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_raphe_hub_mode_t current_mode;
    float current_inhibition;
    float current_flexibility;
    nimcp_raphe_hub_dmn_t dmn_state;
    float accumulated_conflict;
} nimcp_raphe_hub_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t inhibition_events;
    uint64_t flexibility_switches;
    float avg_inhibition;
    float avg_conflict;
    float time_in_rumination;
} nimcp_raphe_hub_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_raphe_hub_bridge nimcp_raphe_hub_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_raphe_hub_config_t nimcp_raphe_hub_config_default(void);

nimcp_raphe_hub_bridge_t* nimcp_raphe_hub_create(
    const nimcp_raphe_hub_config_t* config
);

void nimcp_raphe_hub_destroy(nimcp_raphe_hub_bridge_t* bridge);

int nimcp_raphe_hub_reset(nimcp_raphe_hub_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_raphe_hub_connect_raphe(
    nimcp_raphe_hub_bridge_t* bridge,
    nimcp_raphe_adapter_t raphe_adapter
);

int nimcp_raphe_hub_connect_hub(
    nimcp_raphe_hub_bridge_t* bridge,
    struct nimcp_central_hub* hub
);

/*=============================================================================
 * Raphe --> Hub API
 *===========================================================================*/

/**
 * @brief Compute hub modulation from 5-HT state
 */
int nimcp_raphe_hub_compute_modulation(
    nimcp_raphe_hub_bridge_t* bridge,
    nimcp_raphe_hub_modulation_t* modulation
);

/**
 * @brief Set modulation mode
 */
int nimcp_raphe_hub_set_mode(
    nimcp_raphe_hub_bridge_t* bridge,
    nimcp_raphe_hub_mode_t mode
);

/**
 * @brief Apply behavioral inhibition
 */
int nimcp_raphe_hub_apply_inhibition(
    nimcp_raphe_hub_bridge_t* bridge,
    float inhibition_level
);

/**
 * @brief Get hub weight for specific region
 */
float nimcp_raphe_hub_get_hub_weight(
    nimcp_raphe_hub_bridge_t* bridge,
    uint32_t hub_index
);

/*=============================================================================
 * Hub --> Raphe API
 *===========================================================================*/

/**
 * @brief Receive hub feedback
 */
int nimcp_raphe_hub_receive_feedback(
    nimcp_raphe_hub_bridge_t* bridge,
    const nimcp_raphe_hub_feedback_t* feedback
);

/**
 * @brief Process conflict signal
 */
int nimcp_raphe_hub_process_conflict(
    nimcp_raphe_hub_bridge_t* bridge,
    float conflict_level
);

/**
 * @brief Get 5-HT response to hub state
 */
float nimcp_raphe_hub_get_ht5_response(nimcp_raphe_hub_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_raphe_hub_update(nimcp_raphe_hub_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_raphe_hub_get_state(
    const nimcp_raphe_hub_bridge_t* bridge,
    nimcp_raphe_hub_bridge_state_t* state
);

int nimcp_raphe_hub_get_stats(
    const nimcp_raphe_hub_bridge_t* bridge,
    nimcp_raphe_hub_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RAPHE_HUB_BRIDGE_H */
