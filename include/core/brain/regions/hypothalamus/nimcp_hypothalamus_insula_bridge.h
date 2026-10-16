/**
 * @file nimcp_hypothalamus_insula_bridge.h
 * @brief Hypothalamus-Insula Bridge for Interoceptive Integration
 *
 * WHAT: Bidirectional integration between hypothalamus drives and insula interoception
 * WHY:  Body state awareness drives homeostatic setpoint adjustments
 * HOW:  Interoceptive signals inform drive deviations; drives bias interoceptive attention
 *
 * BIOLOGICAL BASIS:
 * The insula provides interoceptive awareness (internal body states) that feeds into
 * hypothalamic homeostatic regulation:
 *
 * INSULA → HYPOTHALAMUS:
 * - Cardiac signals → Stress/arousal drive modulation
 * - Gastric signals → Hunger drive calibration
 * - Thermal signals → Temperature setpoint validation
 * - Pain signals → Safety drive activation
 * - Fatigue signals → Rest drive urgency
 *
 * HYPOTHALAMUS → INSULA:
 * - High drive states → Enhanced interoceptive attention to relevant signals
 * - Survival mode → Amplified body awareness
 * - Satiation → Reduced interoceptive salience
 *
 * @version Phase 16: Additional Module Bridges
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_INSULA_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_INSULA_BRIDGE_H

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

#define HYPO_INSULA_BRIDGE_MODULE_ID  0x1180
#define HYPO_INSULA_MAX_CHANNELS      16

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Interoceptive channel types mapped to drives
 */
typedef enum {
    HYPO_INTERO_CARDIAC = 0,      /**< Heart rate → stress/arousal */
    HYPO_INTERO_RESPIRATORY,      /**< Breathing → arousal/fatigue */
    HYPO_INTERO_GASTRIC,          /**< Gut → hunger */
    HYPO_INTERO_THERMAL,          /**< Temperature → thermoregulation */
    HYPO_INTERO_PAIN,             /**< Pain → safety */
    HYPO_INTERO_HUNGER,           /**< Metabolic → hunger */
    HYPO_INTERO_THIRST,           /**< Hydration → thirst */
    HYPO_INTERO_FATIGUE,          /**< Energy → fatigue */
    HYPO_INTERO_COUNT
} hypo_intero_channel_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Interoceptive state from insula
 */
typedef struct {
    float cardiac_state;          /**< Heart rate/HRV [0, 1] */
    float respiratory_state;      /**< Breathing rate/depth [0, 1] */
    float gastric_state;          /**< Gut signals [0, 1] */
    float thermal_state;          /**< Body temperature deviation [-1, 1] */
    float pain_level;             /**< Nociceptive input [0, 1] */
    float hunger_signal;          /**< Metabolic hunger [0, 1] */
    float thirst_signal;          /**< Hydration need [0, 1] */
    float fatigue_signal;         /**< Energy depletion [0, 1] */
    uint64_t last_update_us;      /**< Last update timestamp */
} hypo_intero_state_t;

/**
 * @brief Interoceptive attention modulation (hypothalamus → insula)
 */
typedef struct {
    float channel_gains[HYPO_INTERO_COUNT]; /**< Attention gain per channel */
    float overall_salience;               /**< Overall body awareness */
    bool survival_mode;                   /**< Enhanced awareness mode */
} hypo_intero_attention_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    float cardiac_stress_weight;    /**< Cardiac → stress sensitivity */
    float pain_safety_weight;       /**< Pain → safety sensitivity */
    float gastric_hunger_weight;    /**< Gastric → hunger sensitivity */
    float thermal_weight;           /**< Thermal → temperature sensitivity */
    bool enable_attention_modulation; /**< Enable drive → attention */
    float attention_gain_range;     /**< Max attention gain [1, 3] */
} hypo_insula_config_t;

typedef struct hypo_insula_bridge hypo_insula_bridge_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

hypo_insula_bridge_t* hypo_insula_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_insula_config_t* config);

void hypo_insula_bridge_destroy(hypo_insula_bridge_t* bridge);
void hypo_insula_bridge_default_config(hypo_insula_config_t* config);

/*=============================================================================
 * INTEROCEPTION → DRIVE MODULATION
 *===========================================================================*/

int hypo_insula_bridge_update_interoception(
    hypo_insula_bridge_t* bridge,
    const hypo_intero_state_t* state);

int hypo_insula_bridge_apply_interoceptive_effects(hypo_insula_bridge_t* bridge);

int hypo_insula_bridge_get_intero_state(
    const hypo_insula_bridge_t* bridge,
    hypo_intero_state_t* state);

/*=============================================================================
 * DRIVE → ATTENTION MODULATION
 *===========================================================================*/

int hypo_insula_bridge_compute_attention(hypo_insula_bridge_t* bridge);

int hypo_insula_bridge_get_attention(
    const hypo_insula_bridge_t* bridge,
    hypo_intero_attention_t* attention);

/*=============================================================================
 * BIO-ASYNC
 *===========================================================================*/

bool hypo_insula_bridge_register_bio(hypo_insula_bridge_t* bridge, bool use_kg_wiring);
void hypo_insula_bridge_unregister_bio(hypo_insula_bridge_t* bridge);
nimcp_error_t hypo_insula_bridge_broadcast_attention(hypo_insula_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_INSULA_BRIDGE_H */
