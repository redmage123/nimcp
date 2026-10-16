/**
 * @file nimcp_hypothalamus_emotion_bridge.h
 * @brief Hypothalamus-Emotion Bridge for HPA Axis Stress Response
 *
 * WHAT: Bidirectional integration between hypothalamus HPA axis and emotional system
 * WHY:  Emotions drive stress response; HPA output modulates emotional processing
 * HOW:  Emotional arousal → CRH release; cortisol → emotional dampening
 *
 * BIOLOGICAL BASIS:
 * The hypothalamic-pituitary-adrenal (HPA) axis is the primary stress response
 * system, tightly coupled to emotional processing:
 *
 * EMOTION → HPA AXIS:
 * - Fear/anxiety → Rapid CRH release from PVN
 * - Anger/frustration → Moderate CRH + autonomic activation
 * - Sadness/despair → Chronic HPA activation pattern
 * - Joy/excitement → HPA suppression, reward system activation
 *
 * HPA AXIS → EMOTIONAL PROCESSING:
 * - Acute cortisol → Enhanced emotional memory consolidation
 * - Chronic cortisol → Emotional blunting, anhedonia
 * - CRH → Heightened fear/anxiety processing
 * - Recovery phase → Emotional resilience building
 *
 * ALIGNMENT IMPLICATIONS:
 * - Chronic stress impairs rational decision-making
 * - Emotional dysregulation affects value alignment
 * - Proper HPA regulation supports ethical behavior
 *
 * @version Phase 16: Additional Module Bridges
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_EMOTION_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_EMOTION_BRIDGE_H

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

#define HYPO_EMOTION_BRIDGE_MODULE_ID  0x1182

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Emotional categories affecting HPA response
 */
typedef enum {
    HYPO_EMO_FEAR = 0,        /**< Fear/anxiety → strong HPA activation */
    HYPO_EMO_ANGER,           /**< Anger/frustration → moderate HPA + SNS */
    HYPO_EMO_SADNESS,         /**< Sadness/despair → chronic HPA pattern */
    HYPO_EMO_JOY,             /**< Joy/excitement → HPA suppression */
    HYPO_EMO_DISGUST,         /**< Disgust → defensive withdrawal */
    HYPO_EMO_SURPRISE,        /**< Surprise → transient arousal */
    HYPO_EMO_NEUTRAL,         /**< Baseline neutral state */
    HYPO_EMO_COUNT
} hypo_emotion_category_t;

/**
 * @brief HPA axis response states
 */
typedef enum {
    HYPO_HPA_BASELINE = 0,    /**< Normal, unstressed */
    HYPO_HPA_ALERT,           /**< Heightened awareness, mild stress */
    HYPO_HPA_ACUTE_STRESS,    /**< Active stress response */
    HYPO_HPA_RECOVERY,        /**< Post-stress recovery phase */
    HYPO_HPA_CHRONIC_STRESS,  /**< Prolonged stress (maladaptive) */
    HYPO_HPA_EXHAUSTION       /**< HPA axis exhaustion/burnout */
} hypo_hpa_response_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Emotional state input from emotional system
 */
typedef struct {
    float valence;                /**< Positive/negative [-1, 1] */
    float arousal;                /**< Activation level [0, 1] */
    float dominance;              /**< Control/agency [0, 1] */
    hypo_emotion_category_t primary_emotion;
    float emotion_intensities[HYPO_EMO_COUNT]; /**< Per-category intensities */
    bool shadow_active;           /**< Maladaptive pattern detected */
    float emotional_regulation;   /**< Self-regulation capacity [0, 1] */
} hypo_emotion_input_t;

/**
 * @brief HPA axis output state
 */
typedef struct {
    hypo_hpa_response_t response_state;
    float crh_level;              /**< CRH from PVN [0, 1] */
    float acth_level;             /**< ACTH from pituitary [0, 1] */
    float cortisol_level;         /**< Cortisol [0, 1] */
    float cortisol_accumulated;   /**< Chronic cortisol load [0, 1] */
    float recovery_progress;      /**< Recovery phase progress [0, 1] */
    float stress_resilience;      /**< Built resilience [0, 1] */
} hypo_hpa_output_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    float fear_crh_weight;        /**< Fear → CRH sensitivity [0.9] */
    float anger_snc_weight;       /**< Anger → SNS activation [0.7] */
    float joy_suppression;        /**< Joy → HPA suppression [0.6] */
    float arousal_amplification;  /**< Arousal → HPA amplification [0.8] */
    float cortisol_emotion_dampening; /**< Cortisol → emotion suppression [0.5] */
    float chronic_threshold;      /**< Time to chronic state (hours) [2.0] */
    float recovery_rate;          /**< Recovery speed [0.1] */
    bool enable_emotional_dampening; /**< Enable cortisol → emotion feedback */
} hypo_emotion_config_t;

typedef struct hypo_emotion_bridge hypo_emotion_bridge_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

hypo_emotion_bridge_t* hypo_emotion_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_emotion_config_t* config);

void hypo_emotion_bridge_destroy(hypo_emotion_bridge_t* bridge);
void hypo_emotion_bridge_default_config(hypo_emotion_config_t* config);

/*=============================================================================
 * EMOTION → HPA AXIS
 *===========================================================================*/

/**
 * @brief Update emotional input state
 */
int hypo_emotion_bridge_update_emotion(
    hypo_emotion_bridge_t* bridge,
    const hypo_emotion_input_t* emotion);

/**
 * @brief Process emotions and update HPA response
 */
int hypo_emotion_bridge_process_hpa_response(hypo_emotion_bridge_t* bridge);

/**
 * @brief Get current HPA output
 */
int hypo_emotion_bridge_get_hpa_output(
    const hypo_emotion_bridge_t* bridge,
    hypo_hpa_output_t* output);

/*=============================================================================
 * HPA AXIS → EMOTIONAL MODULATION
 *===========================================================================*/

/**
 * @brief Compute emotional dampening from cortisol
 */
int hypo_emotion_bridge_compute_emotional_modulation(
    hypo_emotion_bridge_t* bridge,
    float* dampening_factor);

/**
 * @brief Get current emotional input state
 */
int hypo_emotion_bridge_get_emotion_input(
    const hypo_emotion_bridge_t* bridge,
    hypo_emotion_input_t* emotion);

/*=============================================================================
 * BIO-ASYNC
 *===========================================================================*/

bool hypo_emotion_bridge_register_bio(hypo_emotion_bridge_t* bridge, bool use_kg_wiring);
void hypo_emotion_bridge_unregister_bio(hypo_emotion_bridge_t* bridge);
nimcp_error_t hypo_emotion_bridge_broadcast_hpa(hypo_emotion_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_EMOTION_BRIDGE_H */
