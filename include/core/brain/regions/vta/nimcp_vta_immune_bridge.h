/**
 * @file nimcp_vta_immune_bridge.h
 * @brief Ventral Tegmental Area - Brain Immune System Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between VTA (dopamine) and brain immune system
 * WHY:  Enable DA-mediated immune modulation and cytokine effects on motivation
 * HOW:  DA modulates microglial state; inflammation affects reward processing
 *
 * THEORETICAL FOUNDATIONS:
 * - Felger & Treadway (2017): Inflammation and anhedonia
 * - Capuron & Miller (2011): Cytokines and motivation
 * - Russo & Bhardwaj (2020): DA-immune interactions
 *
 * BIOLOGICAL BASIS:
 * - Inflammation reduces striatal DA release
 * - Pro-inflammatory cytokines impair reward motivation
 * - DA has immunomodulatory effects on microglia
 * - Sickness behavior involves reduced DA signaling
 *
 * INTEGRATION FLOWS:
 *
 * VTA --> Immune:
 *   1. DA modulates microglial activation state
 *   2. Reward processing affects neuroimmune tone
 *   3. Motivational state influences immune vigilance
 *   4. Stress-DA interactions affect inflammation
 *
 * Immune --> VTA:
 *   1. Inflammation reduces DA synthesis/release
 *   2. Cytokines impair reward processing
 *   3. Immune activation causes anhedonia-like states
 *   4. Resolution allows reward function recovery
 *
 * @see nimcp_vta.h
 * @see nimcp_brain_immune.h
 */

#ifndef NIMCP_VTA_IMMUNE_BRIDGE_H
#define NIMCP_VTA_IMMUNE_BRIDGE_H

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
struct nimcp_brain_immune_system;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Inflammation threshold for DA impairment */
#define VTA_IMMUNE_INFLAM_THRESHOLD     0.4f

/** @brief Maximum DA suppression by inflammation */
#define VTA_IMMUNE_DA_SUPPRESS_MAX      0.7f

/** @brief Bio-async module ID */
#define BIO_MODULE_VTA_IMMUNE_BRIDGE    0x0D30

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief DA-immune interaction mode
 */
typedef enum {
    VTA_IMMUNE_MODE_NORMAL = 0,      /**< Normal DA-immune balance */
    VTA_IMMUNE_MODE_INFLAMED,        /**< Inflammation-suppressed DA */
    VTA_IMMUNE_MODE_ANHEDONIC,       /**< Anhedonia-like state */
    VTA_IMMUNE_MODE_RECOVERING       /**< Recovery from inflammation */
} nimcp_vta_immune_mode_t;

/**
 * @brief Motivation impairment level
 */
typedef enum {
    VTA_IMMUNE_MOTIV_NORMAL = 0,     /**< Normal motivation */
    VTA_IMMUNE_MOTIV_REDUCED,        /**< Mildly reduced */
    VTA_IMMUNE_MOTIV_IMPAIRED,       /**< Significantly impaired */
    VTA_IMMUNE_MOTIV_ABSENT          /**< Severe anhedonia */
} nimcp_vta_immune_motiv_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief VTA-Immune bridge configuration
 */
typedef struct {
    /* Inflammation effects */
    float inflammation_threshold;    /**< Threshold for DA effects */
    float da_suppress_max;           /**< Maximum DA suppression */
    float suppress_tau_ms;           /**< Suppression time constant */

    /* Anhedonia modeling */
    bool enable_anhedonia;           /**< Model anhedonic states */
    float anhedonia_threshold;       /**< Threshold for anhedonia */
    float reward_sensitivity_min;    /**< Minimum reward sensitivity */

    /* DA immunomodulation */
    float da_immune_gain;            /**< DA effect on immune */
    bool enable_da_protection;       /**< DA protective effects */

    /* Recovery */
    float recovery_rate;             /**< Recovery rate after inflammation */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_vta_immune_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Immune modulation output
 */
typedef struct {
    float microglial_modulation;     /**< DA effect on microglia */
    float cytokine_modulation;       /**< DA effect on cytokines */
    float neuroprotection;           /**< DA protective signal */
} nimcp_vta_immune_modulation_t;

/**
 * @brief Immune feedback to VTA
 */
typedef struct {
    nimcp_vta_immune_mode_t mode;    /**< Current interaction mode */
    float inflammation_level;        /**< Overall inflammation [0-1] */
    float da_suppression;            /**< DA suppression factor */
    float reward_sensitivity;        /**< Current reward sensitivity */
    nimcp_vta_immune_motiv_t motivation; /**< Motivation state */
    bool anhedonia_active;           /**< Anhedonia present */
} nimcp_vta_immune_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_vta_immune_mode_t current_mode;
    float current_suppression;
    float current_reward_sensitivity;
    float accumulated_inflammation;
    bool in_anhedonic_state;
    float recovery_progress;
} nimcp_vta_immune_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t anhedonia_episodes;
    float total_inflammation_time;
    float avg_da_suppression;
    float avg_reward_sensitivity;
    float max_inflammation;
} nimcp_vta_immune_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_vta_immune_bridge nimcp_vta_immune_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_vta_immune_config_t nimcp_vta_immune_config_default(void);

nimcp_vta_immune_bridge_t* nimcp_vta_immune_create(
    const nimcp_vta_immune_config_t* config
);

void nimcp_vta_immune_destroy(nimcp_vta_immune_bridge_t* bridge);

int nimcp_vta_immune_reset(nimcp_vta_immune_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_vta_immune_connect_vta(
    nimcp_vta_immune_bridge_t* bridge,
    nimcp_vta_adapter_t vta_adapter
);

int nimcp_vta_immune_connect_immune(
    nimcp_vta_immune_bridge_t* bridge,
    struct nimcp_brain_immune_system* immune
);

/*=============================================================================
 * VTA --> Immune API
 *===========================================================================*/

/**
 * @brief Compute immune modulation from DA state
 */
int nimcp_vta_immune_compute_modulation(
    nimcp_vta_immune_bridge_t* bridge,
    nimcp_vta_immune_modulation_t* modulation
);

/**
 * @brief Apply DA-mediated neuroprotection
 */
int nimcp_vta_immune_apply_protection(
    nimcp_vta_immune_bridge_t* bridge,
    float da_concentration
);

/*=============================================================================
 * Immune --> VTA API
 *===========================================================================*/

/**
 * @brief Receive immune feedback
 */
int nimcp_vta_immune_receive_feedback(
    nimcp_vta_immune_bridge_t* bridge,
    const nimcp_vta_immune_feedback_t* feedback
);

/**
 * @brief Get DA suppression from inflammation
 */
float nimcp_vta_immune_get_da_suppression(nimcp_vta_immune_bridge_t* bridge);

/**
 * @brief Get current reward sensitivity
 */
float nimcp_vta_immune_get_reward_sensitivity(nimcp_vta_immune_bridge_t* bridge);

/**
 * @brief Check if anhedonia is active
 */
bool nimcp_vta_immune_is_anhedonic(nimcp_vta_immune_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_vta_immune_update(nimcp_vta_immune_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_vta_immune_get_state(
    const nimcp_vta_immune_bridge_t* bridge,
    nimcp_vta_immune_bridge_state_t* state
);

int nimcp_vta_immune_get_stats(
    const nimcp_vta_immune_bridge_t* bridge,
    nimcp_vta_immune_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VTA_IMMUNE_BRIDGE_H */
