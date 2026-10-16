/**
 * @file nimcp_hypothalamus_sleep_bridge.h
 * @brief Hypothalamus-Sleep/Wake Bridge for Circadian Integration
 *
 * WHAT: Bidirectional integration between hypothalamus SCN and sleep/wake system
 * WHY:  Circadian timing drives sleep propensity; sleep state modulates drives
 * HOW:  SCN phase → sleep pressure/timing; sleep state → drive modulation
 *
 * BIOLOGICAL BASIS:
 * The suprachiasmatic nucleus (SCN) in the hypothalamus is the master circadian
 * pacemaker that coordinates sleep-wake timing:
 *
 * SCN → SLEEP SYSTEM:
 * - Circadian phase gates sleep propensity (melatonin release timing)
 * - SCN output inhibits sleep-promoting areas during "wake window"
 * - Phase determines optimal sleep windows
 *
 * SLEEP STATE → HYPOTHALAMUS:
 * - Sleep state modulates drive urgency (reduced during sleep)
 * - REM sleep affects emotional processing
 * - Deep sleep enhances HPA axis recovery
 *
 * ALIGNMENT IMPLICATIONS:
 * - Sleep deprivation increases impulsivity and compromises decision-making
 * - Proper circadian alignment improves rational behavior
 * - Alertness states affect safety-critical processing
 *
 * @version Phase 16: Additional Module Bridges
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_SLEEP_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_SLEEP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "nimcp_hypothalamus_drives.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define HYPO_SLEEP_BRIDGE_MODULE_ID  0x1181

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Circadian phase zones
 */
typedef enum {
    HYPO_CIRCADIAN_NIGHT = 0,     /**< Low activity, high melatonin */
    HYPO_CIRCADIAN_DAWN,          /**< Transition to wake */
    HYPO_CIRCADIAN_DAY,           /**< High activity, alertness */
    HYPO_CIRCADIAN_DUSK,          /**< Transition to sleep */
    HYPO_CIRCADIAN_PHASE_COUNT
} hypo_circadian_phase_t;

/**
 * @brief Alertness levels from hypothalamus perspective
 */
typedef enum {
    HYPO_ALERTNESS_UNCONSCIOUS = 0,  /**< Deep sleep/anesthesia */
    HYPO_ALERTNESS_DROWSY,           /**< Low vigilance */
    HYPO_ALERTNESS_NORMAL,           /**< Baseline alertness */
    HYPO_ALERTNESS_VIGILANT,         /**< Heightened awareness */
    HYPO_ALERTNESS_HYPERAROUSED      /**< Stress/threat response */
} hypo_alertness_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief SCN circadian state output
 */
typedef struct {
    float phase;                  /**< Circadian phase [0, 24] hours */
    hypo_circadian_phase_t zone;  /**< Current phase zone */
    float amplitude;              /**< Circadian amplitude [0, 1] */
    float melatonin_level;        /**< Melatonin signal [0, 1] */
    float cortisol_anticipation;  /**< Morning cortisol prep [0, 1] */
    float wake_propensity;        /**< Circadian wake drive [0, 1] */
    float sleep_propensity;       /**< Circadian sleep drive [0, 1] */
} hypo_scn_output_t;

/**
 * @brief Sleep state feedback to hypothalamus
 */
typedef struct {
    int sleep_state;              /**< Current sleep_state_t value */
    float sleep_pressure;         /**< Current pressure [0, 1] */
    float sleep_debt;             /**< Accumulated debt [0, 1] */
    bool is_asleep;               /**< Currently sleeping */
    float time_in_state_ms;       /**< Duration in current state */
} hypo_sleep_feedback_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    float phase_wake_threshold;     /**< Phase threshold for wake [6.0] hours */
    float phase_sleep_threshold;    /**< Phase threshold for sleep [22.0] hours */
    float pressure_to_drive_weight; /**< Sleep pressure → rest drive [0.8] */
    float melatonin_arousal_weight; /**< Melatonin → arousal suppression [0.6] */
    bool enable_drive_suppression;  /**< Suppress drives during sleep */
    float drive_suppression_factor; /**< How much to suppress [0.2] */
} hypo_sleep_config_t;

typedef struct hypo_sleep_bridge hypo_sleep_bridge_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

hypo_sleep_bridge_t* hypo_sleep_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_sleep_config_t* config);

void hypo_sleep_bridge_destroy(hypo_sleep_bridge_t* bridge);
void hypo_sleep_bridge_default_config(hypo_sleep_config_t* config);

/*=============================================================================
 * SCN → SLEEP MODULATION
 *===========================================================================*/

/**
 * @brief Update SCN circadian state
 */
int hypo_sleep_bridge_update_scn(
    hypo_sleep_bridge_t* bridge,
    const hypo_scn_output_t* scn);

/**
 * @brief Compute sleep propensity from circadian phase
 */
int hypo_sleep_bridge_compute_sleep_propensity(hypo_sleep_bridge_t* bridge);

/**
 * @brief Get current SCN output
 */
int hypo_sleep_bridge_get_scn(
    const hypo_sleep_bridge_t* bridge,
    hypo_scn_output_t* scn);

/**
 * @brief Get current alertness level
 */
hypo_alertness_t hypo_sleep_bridge_get_alertness(const hypo_sleep_bridge_t* bridge);

/*=============================================================================
 * SLEEP STATE → DRIVE MODULATION
 *===========================================================================*/

/**
 * @brief Update sleep state feedback
 */
int hypo_sleep_bridge_update_sleep_state(
    hypo_sleep_bridge_t* bridge,
    const hypo_sleep_feedback_t* feedback);

/**
 * @brief Apply sleep state effects to drives
 */
int hypo_sleep_bridge_apply_sleep_effects(hypo_sleep_bridge_t* bridge);

/**
 * @brief Get sleep feedback state
 */
int hypo_sleep_bridge_get_sleep_feedback(
    const hypo_sleep_bridge_t* bridge,
    hypo_sleep_feedback_t* feedback);

/*=============================================================================
 * BIO-ASYNC
 *===========================================================================*/

bool hypo_sleep_bridge_register_bio(hypo_sleep_bridge_t* bridge, bool use_kg_wiring);
void hypo_sleep_bridge_unregister_bio(hypo_sleep_bridge_t* bridge);
nimcp_error_t hypo_sleep_bridge_broadcast_scn(hypo_sleep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_SLEEP_BRIDGE_H */
