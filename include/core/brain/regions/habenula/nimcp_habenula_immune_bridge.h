/**
 * @file nimcp_habenula_immune_bridge.h
 * @brief Habenula - Brain Immune System Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between Habenula and brain immune system
 * WHY:  Enable habenula-mediated stress-immune interactions
 * HOW:  Habenula stress signals affect immunity; inflammation affects habenula
 *
 * THEORETICAL FOUNDATIONS:
 * - Yang et al. (2018): Habenula and stress-induced depression
 * - Cui et al. (2014): Habenula inflammation link
 * - Dantzer (2009): Stress and neuroimmune interactions
 *
 * BIOLOGICAL BASIS:
 * - Chronic stress activates habenula
 * - Stress hormones affect immune function
 * - Inflammation can activate habenula
 * - Sickness behavior involves habenula
 *
 * INTEGRATION FLOWS:
 *
 * Habenula --> Immune:
 *   1. Chronic activation promotes inflammation
 *   2. Stress signals affect immune vigilance
 *   3. Disappointment/stress impacts immunity
 *   4. Depression-inflammation link via habenula
 *
 * Immune --> Habenula:
 *   1. Inflammation activates habenula
 *   2. Cytokines enhance aversive processing
 *   3. Sickness behavior via habenula
 *   4. Chronic inflammation-depression link
 *
 * @see nimcp_habenula.h
 * @see nimcp_brain_immune.h
 */

#ifndef NIMCP_HABENULA_IMMUNE_BRIDGE_H
#define NIMCP_HABENULA_IMMUNE_BRIDGE_H

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
struct nimcp_brain_immune_system;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Stress threshold for immune effects */
#define HAB_IMMUNE_STRESS_THRESHOLD     0.4f

/** @brief Inflammation activation threshold */
#define HAB_IMMUNE_INFLAM_THRESHOLD     0.3f

/** @brief Bio-async module ID */
#define BIO_MODULE_HAB_IMMUNE_BRIDGE    0x0F30

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Stress-immune interaction mode
 */
typedef enum {
    HAB_IMMUNE_MODE_NORMAL = 0,      /**< Normal interaction */
    HAB_IMMUNE_MODE_STRESSED,        /**< Stress-activated */
    HAB_IMMUNE_MODE_INFLAMED,        /**< Inflammation-driven */
    HAB_IMMUNE_MODE_DEPRESSIVE       /**< Depression-inflammation loop */
} nimcp_hab_immune_mode_t;

/**
 * @brief Sickness behavior level
 */
typedef enum {
    HAB_IMMUNE_SICKNESS_NONE = 0,    /**< No sickness behavior */
    HAB_IMMUNE_SICKNESS_MILD,        /**< Mild malaise */
    HAB_IMMUNE_SICKNESS_MODERATE,    /**< Moderate sickness */
    HAB_IMMUNE_SICKNESS_SEVERE       /**< Severe sickness behavior */
} nimcp_hab_immune_sickness_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Habenula-Immune bridge configuration
 */
typedef struct {
    /* Stress effects */
    float stress_threshold;          /**< Threshold for immune effects */
    float stress_immune_gain;        /**< Stress-to-immune gain */
    float chronic_stress_tau_ms;     /**< Chronic stress accumulation */

    /* Inflammation effects */
    float inflammation_threshold;    /**< Threshold for habenula activation */
    float inflammation_hab_gain;     /**< Inflammation-to-habenula gain */

    /* Sickness behavior */
    bool enable_sickness_behavior;   /**< Model sickness behavior */
    float sickness_threshold;        /**< Threshold for sickness */

    /* Depression link */
    bool enable_depression_loop;     /**< Model depression-inflammation */
    float depression_loop_gain;      /**< Loop amplification */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_hab_immune_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Immune modulation output
 */
typedef struct {
    float stress_immune_signal;      /**< Stress effect on immune */
    float inflammation_tendency;     /**< Pro-inflammatory tendency */
    float immune_vigilance;          /**< Overall immune state */
} nimcp_hab_immune_modulation_t;

/**
 * @brief Immune feedback to Habenula
 */
typedef struct {
    nimcp_hab_immune_mode_t mode;    /**< Current interaction mode */
    float inflammation_level;        /**< Overall inflammation [0-1] */
    float habenula_activation;       /**< Inflammation-driven activation */
    nimcp_hab_immune_sickness_t sickness; /**< Sickness behavior level */
    bool depression_risk;            /**< Depression risk elevated */
} nimcp_hab_immune_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_hab_immune_mode_t current_mode;
    float accumulated_stress;
    float accumulated_inflammation;
    nimcp_hab_immune_sickness_t sickness_level;
    bool in_depression_loop;
} nimcp_hab_immune_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t stress_events;
    uint64_t inflammation_activations;
    uint64_t sickness_episodes;
    float avg_stress_level;
    float avg_inflammation;
    float time_in_depression_loop;
} nimcp_hab_immune_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_hab_immune_bridge nimcp_hab_immune_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_hab_immune_config_t nimcp_hab_immune_config_default(void);

nimcp_hab_immune_bridge_t* nimcp_hab_immune_create(
    const nimcp_hab_immune_config_t* config
);

void nimcp_hab_immune_destroy(nimcp_hab_immune_bridge_t* bridge);

int nimcp_hab_immune_reset(nimcp_hab_immune_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_hab_immune_connect_habenula(
    nimcp_hab_immune_bridge_t* bridge,
    nimcp_habenula_adapter_t hab_adapter
);

int nimcp_hab_immune_connect_immune(
    nimcp_hab_immune_bridge_t* bridge,
    struct nimcp_brain_immune_system* immune
);

/*=============================================================================
 * Habenula --> Immune API
 *===========================================================================*/

/**
 * @brief Compute immune modulation from habenula state
 */
int nimcp_hab_immune_compute_modulation(
    nimcp_hab_immune_bridge_t* bridge,
    nimcp_hab_immune_modulation_t* modulation
);

/**
 * @brief Apply stress-induced immune effects
 */
int nimcp_hab_immune_apply_stress(
    nimcp_hab_immune_bridge_t* bridge,
    float stress_level
);

/*=============================================================================
 * Immune --> Habenula API
 *===========================================================================*/

/**
 * @brief Receive immune feedback
 */
int nimcp_hab_immune_receive_feedback(
    nimcp_hab_immune_bridge_t* bridge,
    const nimcp_hab_immune_feedback_t* feedback
);

/**
 * @brief Get habenula activation from inflammation
 */
float nimcp_hab_immune_get_activation(nimcp_hab_immune_bridge_t* bridge);

/**
 * @brief Check if sickness behavior active
 */
bool nimcp_hab_immune_is_sick(nimcp_hab_immune_bridge_t* bridge);

/**
 * @brief Check depression risk
 */
bool nimcp_hab_immune_depression_risk(nimcp_hab_immune_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_hab_immune_update(nimcp_hab_immune_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_hab_immune_get_state(
    const nimcp_hab_immune_bridge_t* bridge,
    nimcp_hab_immune_bridge_state_t* state
);

int nimcp_hab_immune_get_stats(
    const nimcp_hab_immune_bridge_t* bridge,
    nimcp_hab_immune_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HABENULA_IMMUNE_BRIDGE_H */
