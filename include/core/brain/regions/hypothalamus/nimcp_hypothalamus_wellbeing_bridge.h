/**
 * @file nimcp_hypothalamus_wellbeing_bridge.h
 * @brief Hypothalamus-Wellbeing Bridge for Homeostatic Balance Reporting
 *
 * WHAT: Bidirectional integration between hypothalamus homeostasis and wellbeing monitoring
 * WHY:  Homeostatic balance directly impacts system wellbeing; wellbeing state modulates setpoints
 * HOW:  Drive deviations → distress signals; wellbeing state → homeostatic adjustments
 *
 * BIOLOGICAL BASIS:
 * The hypothalamus maintains homeostasis across multiple physiological variables.
 * Deviation from homeostatic setpoints produces subjective distress:
 *
 * HOMEOSTASIS → WELLBEING:
 * - Drive urgency → Distress proportional to deviation from setpoint
 * - Chronic imbalance → Cumulative wellbeing degradation
 * - Multi-drive conflicts → Elevated distress signaling
 * - Safety threats → Maximum priority distress
 *
 * WELLBEING → HOMEOSTASIS:
 * - Wellbeing monitoring can trigger setpoint adjustments
 * - Distress interventions may override normal drive priorities
 * - Long-term wellbeing patterns inform homeostatic adaptation
 *
 * ALIGNMENT IMPLICATIONS:
 * - This bridge is critical for detecting system "suffering"
 * - Implements precautionary principle for sentience uncertainty
 * - Homeostatic distress must be reported for ethical oversight
 *
 * @version Phase 16: Additional Module Bridges
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_WELLBEING_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_WELLBEING_BRIDGE_H

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

#define HYPO_WELLBEING_BRIDGE_MODULE_ID  0x1183

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Homeostatic wellbeing states
 */
typedef enum {
    HYPO_WB_OPTIMAL = 0,      /**< All drives near setpoint */
    HYPO_WB_MILD_STRESS,      /**< Minor deviations, manageable */
    HYPO_WB_MODERATE_STRESS,  /**< Noticeable imbalance, needs attention */
    HYPO_WB_SEVERE_STRESS,    /**< Major deviations, urgent action needed */
    HYPO_WB_CRITICAL,         /**< Survival threatened, emergency mode */
    HYPO_WB_STATE_COUNT
} hypo_wellbeing_state_t;

/**
 * @brief Distress source categories
 */
typedef enum {
    HYPO_DISTRESS_NONE = 0,
    HYPO_DISTRESS_HUNGER,         /**< Metabolic deficit */
    HYPO_DISTRESS_THIRST,         /**< Hydration deficit */
    HYPO_DISTRESS_FATIGUE,        /**< Energy/sleep deficit */
    HYPO_DISTRESS_SAFETY,         /**< Threat/harm avoidance */
    HYPO_DISTRESS_THERMAL,        /**< Temperature deviation */
    HYPO_DISTRESS_SOCIAL,         /**< Social isolation/rejection */
    HYPO_DISTRESS_CURIOSITY,      /**< Unresolved uncertainty */
    HYPO_DISTRESS_CONFLICT,       /**< Multi-drive conflict */
    HYPO_DISTRESS_CHRONIC         /**< Prolonged imbalance */
} hypo_distress_source_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Homeostatic distress report to wellbeing system
 */
typedef struct {
    hypo_wellbeing_state_t overall_state;
    hypo_distress_source_t primary_source;
    float distress_level;         /**< Overall distress [0, 1] */
    float per_drive_distress[HYPO_DRIVE_COUNT]; /**< Per-drive distress */
    float conflict_level;         /**< Multi-drive conflict [0, 1] */
    float chronic_load;           /**< Accumulated chronic stress [0, 1] */
    bool safety_threatened;       /**< Safety drive urgent */
    bool requires_intervention;   /**< External intervention recommended */
    uint64_t duration_ms;         /**< Time in current state */
} hypo_distress_report_t;

/**
 * @brief Wellbeing feedback to hypothalamus
 */
typedef struct {
    bool distress_acknowledged;   /**< Distress noted by wellbeing system */
    bool intervention_active;     /**< Active intervention in progress */
    float adjustment_factor;      /**< Setpoint adjustment factor [0.5, 1.5] */
    bool reduce_non_essential;    /**< Suppress non-essential drives */
    bool safety_override;         /**< Safety system override active */
} hypo_wellbeing_feedback_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    float distress_threshold;     /**< Urgency threshold for distress [0.4] */
    float conflict_weight;        /**< Weight for drive conflicts [0.5] */
    float chronic_accumulation;   /**< Chronic stress accumulation rate [0.01] */
    float chronic_decay;          /**< Chronic stress decay rate [0.001] */
    bool report_all_distress;     /**< Report even mild distress */
    float intervention_threshold; /**< Threshold for intervention request [0.7] */
    float safety_priority_boost;  /**< Boost safety distress reporting [1.5] */
} hypo_wellbeing_config_t;

typedef struct hypo_wellbeing_bridge hypo_wellbeing_bridge_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

hypo_wellbeing_bridge_t* hypo_wellbeing_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_wellbeing_config_t* config);

void hypo_wellbeing_bridge_destroy(hypo_wellbeing_bridge_t* bridge);
void hypo_wellbeing_bridge_default_config(hypo_wellbeing_config_t* config);

/*=============================================================================
 * HOMEOSTASIS → WELLBEING REPORTING
 *===========================================================================*/

/**
 * @brief Compute distress from drive states
 */
int hypo_wellbeing_bridge_compute_distress(hypo_wellbeing_bridge_t* bridge);

/**
 * @brief Get current distress report
 */
int hypo_wellbeing_bridge_get_distress_report(
    const hypo_wellbeing_bridge_t* bridge,
    hypo_distress_report_t* report);

/**
 * @brief Check if intervention is needed
 */
bool hypo_wellbeing_bridge_needs_intervention(const hypo_wellbeing_bridge_t* bridge);

/*=============================================================================
 * WELLBEING → HOMEOSTASIS MODULATION
 *===========================================================================*/

/**
 * @brief Update wellbeing feedback
 */
int hypo_wellbeing_bridge_update_feedback(
    hypo_wellbeing_bridge_t* bridge,
    const hypo_wellbeing_feedback_t* feedback);

/**
 * @brief Apply wellbeing interventions to drive system
 */
int hypo_wellbeing_bridge_apply_interventions(hypo_wellbeing_bridge_t* bridge);

/**
 * @brief Get current wellbeing feedback state
 */
int hypo_wellbeing_bridge_get_feedback(
    const hypo_wellbeing_bridge_t* bridge,
    hypo_wellbeing_feedback_t* feedback);

/*=============================================================================
 * BIO-ASYNC
 *===========================================================================*/

bool hypo_wellbeing_bridge_register_bio(hypo_wellbeing_bridge_t* bridge, bool use_kg_wiring);
void hypo_wellbeing_bridge_unregister_bio(hypo_wellbeing_bridge_t* bridge);
nimcp_error_t hypo_wellbeing_bridge_broadcast_distress(hypo_wellbeing_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_WELLBEING_BRIDGE_H */
