/**
 * @file nimcp_lc_immune_bridge.h
 * @brief Locus Coeruleus - Brain Immune System Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge between LC (norepinephrine) and brain immune system
 * WHY:  Enable NE-mediated immune modulation and immune-to-brain signaling
 * HOW:  NE modulates microglial activity; cytokines affect LC function
 *
 * THEORETICAL FOUNDATIONS:
 * - Bellinger & Lorton (2014): NE and immune regulation
 * - Heneka et al. (2010): Noradrenergic depletion and neuroinflammation
 * - O'Neill & Bhardwaj (2019): Neuroimmune interactions via catecholamines
 *
 * BIOLOGICAL BASIS:
 * - NE has potent anti-inflammatory effects on microglia
 * - LC degeneration leads to increased neuroinflammation
 * - Cytokines (IL-1β, IL-6, TNF-α) affect LC firing
 * - Sickness behavior involves LC suppression
 *
 * INTEGRATION FLOWS:
 *
 * LC --> Immune:
 *   1. NE suppresses pro-inflammatory cytokine release
 *   2. Arousal state modulates microglial surveillance
 *   3. Phasic responses clear danger signals
 *   4. Stress hormones trigger immune preparation
 *
 * Immune --> LC:
 *   1. Pro-inflammatory cytokines suppress LC firing
 *   2. Damage signals trigger defensive arousal
 *   3. Resolution signals restore normal LC function
 *   4. Chronic inflammation impairs NE release
 *
 * @see nimcp_locus_coeruleus.h
 * @see nimcp_brain_immune.h
 */

#ifndef NIMCP_LC_IMMUNE_BRIDGE_H
#define NIMCP_LC_IMMUNE_BRIDGE_H

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
struct nimcp_brain_immune_system;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Default NE anti-inflammatory threshold */
#define LC_IMMUNE_NE_THRESHOLD          0.5f

/** @brief Maximum immune suppression effect */
#define LC_IMMUNE_SUPPRESSION_MAX       0.8f

/** @brief Cytokine effect on LC threshold */
#define LC_IMMUNE_CYTOKINE_THRESHOLD    0.3f

/** @brief Bio-async module ID */
#define BIO_MODULE_LC_IMMUNE_BRIDGE     0x0C30

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Immune modulation direction
 */
typedef enum {
    LC_IMMUNE_SUPPRESS = 0,          /**< Anti-inflammatory action */
    LC_IMMUNE_NEUTRAL,               /**< Neutral/baseline */
    LC_IMMUNE_ENHANCE                /**< Pro-inflammatory permissive */
} nimcp_lc_immune_dir_t;

/**
 * @brief Immune state affecting LC
 */
typedef enum {
    LC_IMMUNE_STATE_HEALTHY = 0,     /**< Normal immune state */
    LC_IMMUNE_STATE_INFLAMED,        /**< Active inflammation */
    LC_IMMUNE_STATE_RESOLVING,       /**< Inflammation resolving */
    LC_IMMUNE_STATE_CHRONIC          /**< Chronic inflammation */
} nimcp_lc_immune_state_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief LC-Immune bridge configuration
 */
typedef struct {
    /* Anti-inflammatory */
    float ne_threshold;              /**< NE threshold for suppression */
    float suppression_max;           /**< Maximum suppression effect */
    float suppression_tau_ms;        /**< Suppression time constant */

    /* Cytokine effects */
    float cytokine_threshold;        /**< Cytokine threshold for LC effect */
    float cytokine_gain;             /**< Cytokine-to-LC gain */
    float sickness_behavior_gain;    /**< Sickness behavior effect */

    /* Stress response */
    bool enable_stress_immune;       /**< Enable stress-immune coupling */
    float stress_immune_gain;        /**< Stress effect on immunity */

    /* Update */
    float update_interval_ms;
    bool enable_bio_async;
} nimcp_lc_immune_config_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Immune modulation output
 */
typedef struct {
    nimcp_lc_immune_dir_t direction; /**< Modulation direction */
    float suppression_level;         /**< Anti-inflammatory strength */
    float microglial_state;          /**< Microglial activity [0-1] */
    float cytokine_modulation;       /**< Cytokine release modulation */
} nimcp_lc_immune_modulation_t;

/**
 * @brief Immune feedback to LC
 */
typedef struct {
    nimcp_lc_immune_state_t state;   /**< Current immune state */
    float inflammation_level;        /**< Overall inflammation [0-1] */
    float il1b_level;                /**< IL-1β level */
    float il6_level;                 /**< IL-6 level */
    float tnfa_level;                /**< TNF-α level */
    float lc_suppression;            /**< Net suppression of LC */
    bool sickness_behavior;          /**< Sickness behavior active */
} nimcp_lc_immune_feedback_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_lc_immune_state_t immune_state;
    float current_suppression;
    float accumulated_inflammation;
    float ne_contribution;
    bool anti_inflammatory_active;
} nimcp_lc_immune_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t inflammation_events;
    uint64_t suppression_activations;
    float avg_inflammation;
    float avg_suppression;
    float total_ne_contribution;
} nimcp_lc_immune_stats_t;

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_lc_immune_bridge nimcp_lc_immune_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_lc_immune_config_t nimcp_lc_immune_config_default(void);

nimcp_lc_immune_bridge_t* nimcp_lc_immune_create(
    const nimcp_lc_immune_config_t* config
);

void nimcp_lc_immune_destroy(nimcp_lc_immune_bridge_t* bridge);

int nimcp_lc_immune_reset(nimcp_lc_immune_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_lc_immune_connect_lc(
    nimcp_lc_immune_bridge_t* bridge,
    nimcp_lc_adapter_t lc_adapter
);

int nimcp_lc_immune_connect_immune(
    nimcp_lc_immune_bridge_t* bridge,
    struct nimcp_brain_immune_system* immune
);

/*=============================================================================
 * LC --> Immune API
 *===========================================================================*/

/**
 * @brief Compute immune modulation from NE state
 */
int nimcp_lc_immune_compute_modulation(
    nimcp_lc_immune_bridge_t* bridge,
    nimcp_lc_immune_modulation_t* modulation
);

/**
 * @brief Apply NE-mediated anti-inflammatory effect
 */
int nimcp_lc_immune_apply_suppression(
    nimcp_lc_immune_bridge_t* bridge,
    float ne_concentration
);

/**
 * @brief Get microglial surveillance modulation
 */
float nimcp_lc_immune_get_surveillance_mod(nimcp_lc_immune_bridge_t* bridge);

/*=============================================================================
 * Immune --> LC API
 *===========================================================================*/

/**
 * @brief Receive immune system feedback
 */
int nimcp_lc_immune_receive_feedback(
    nimcp_lc_immune_bridge_t* bridge,
    const nimcp_lc_immune_feedback_t* feedback
);

/**
 * @brief Get LC suppression from inflammation
 */
float nimcp_lc_immune_get_lc_suppression(nimcp_lc_immune_bridge_t* bridge);

/**
 * @brief Check if sickness behavior should be active
 */
bool nimcp_lc_immune_sickness_active(nimcp_lc_immune_bridge_t* bridge);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_lc_immune_update(nimcp_lc_immune_bridge_t* bridge, float dt_ms);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_lc_immune_get_state(
    const nimcp_lc_immune_bridge_t* bridge,
    nimcp_lc_immune_bridge_state_t* state
);

int nimcp_lc_immune_get_stats(
    const nimcp_lc_immune_bridge_t* bridge,
    nimcp_lc_immune_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LC_IMMUNE_BRIDGE_H */
