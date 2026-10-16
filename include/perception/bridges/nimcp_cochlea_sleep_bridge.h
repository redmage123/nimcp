/**
 * @file nimcp_cochlea_sleep_bridge.h
 * @brief Cochlea-Sleep integration bridge
 *
 * WHAT: Modulate cochlear processing based on sleep/wake state
 * WHY:  Auditory gating during sleep, arousal from important sounds
 * HOW:  Sleep stage-dependent gain and threshold modulation
 *
 * BIOLOGICAL BASIS:
 * - Auditory processing persists during sleep (arousal possible)
 * - Thalamic gating reduces sensory throughput in NREM
 * - Name recognition and meaningful sounds penetrate sleep
 * - REM: Increased auditory activation (dream incorporation)
 *
 * SLEEP STAGES:
 * - Wake: Full processing
 * - N1: Slight attenuation, easy arousal
 * - N2: K-complexes from sudden sounds
 * - N3 (SWS): Maximum gating, hard arousal
 * - REM: Variable, context-dependent
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_SLEEP_BRIDGE_H
#define NIMCP_COCHLEA_SLEEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_cochlea.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct sleep_controller sleep_controller_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Sleep stage
 */
typedef enum {
    COCHLEA_SLEEP_WAKE,         /**< Fully awake */
    COCHLEA_SLEEP_N1,           /**< Light sleep */
    COCHLEA_SLEEP_N2,           /**< Intermediate sleep */
    COCHLEA_SLEEP_N3,           /**< Deep/slow-wave sleep */
    COCHLEA_SLEEP_REM           /**< REM sleep */
} cochlea_sleep_stage_t;

/**
 * @brief Arousal event type
 */
typedef enum {
    AROUSAL_NONE,               /**< No arousal */
    AROUSAL_PARTIAL,            /**< Lightening of sleep */
    AROUSAL_FULL,               /**< Full awakening */
    AROUSAL_K_COMPLEX           /**< K-complex response */
} arousal_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Sleep modulation state
 */
typedef struct {
    cochlea_sleep_stage_t stage;    /**< Current sleep stage */
    float gating_factor;            /**< Attenuation [0-1] */
    float arousal_threshold_db;     /**< Threshold for arousal */
    float sensitivity_factor;       /**< Overall sensitivity */
} sleep_modulation_t;

/**
 * @brief Arousal event
 */
typedef struct {
    arousal_type_t type;            /**< Arousal type */
    float trigger_level_db;         /**< Sound level that triggered */
    float trigger_frequency_hz;     /**< Dominant frequency */
    uint64_t timestamp_ms;          /**< When occurred */
    bool name_detected;             /**< Own name detected */
} arousal_event_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Stage-specific gating */
    float wake_gating;              /**< Gating in wake (usually 1.0) */
    float n1_gating;                /**< Light sleep gating */
    float n2_gating;                /**< N2 gating */
    float n3_gating;                /**< Deep sleep gating */
    float rem_gating;               /**< REM gating */

    /* Arousal thresholds (dB SPL) */
    float wake_threshold_db;
    float n1_threshold_db;
    float n2_threshold_db;
    float n3_threshold_db;
    float rem_threshold_db;

    /* Special sounds */
    bool enable_name_detection;     /**< Respond to own name */
    bool enable_alarm_detection;    /**< Respond to alarms */
    float name_threshold_reduction; /**< Lower threshold for name */
} cochlea_sleep_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_sleep_bridge cochlea_sleep_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

cochlea_sleep_config_t cochlea_sleep_config_default(void);

//=============================================================================
// Core API
//=============================================================================

cochlea_sleep_bridge_t* cochlea_sleep_bridge_create(
    cochlea_t* cochlea,
    sleep_controller_t* sleep,
    const cochlea_sleep_config_t* config
);

void cochlea_sleep_bridge_destroy(cochlea_sleep_bridge_t* bridge);

nimcp_error_t cochlea_sleep_bridge_update(
    cochlea_sleep_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

nimcp_error_t cochlea_sleep_bridge_reset(cochlea_sleep_bridge_t* bridge);

//=============================================================================
// Sleep Stage Control
//=============================================================================

nimcp_error_t cochlea_sleep_set_stage(
    cochlea_sleep_bridge_t* bridge,
    cochlea_sleep_stage_t stage
);

cochlea_sleep_stage_t cochlea_sleep_get_stage(
    const cochlea_sleep_bridge_t* bridge
);

nimcp_error_t cochlea_sleep_get_modulation(
    const cochlea_sleep_bridge_t* bridge,
    sleep_modulation_t* modulation
);

//=============================================================================
// Arousal Detection
//=============================================================================

bool cochlea_sleep_check_arousal(
    const cochlea_sleep_bridge_t* bridge,
    arousal_event_t* event
);

nimcp_error_t cochlea_sleep_clear_arousal(
    cochlea_sleep_bridge_t* bridge
);

//=============================================================================
// Special Sound Detection
//=============================================================================

nimcp_error_t cochlea_sleep_set_name_template(
    cochlea_sleep_bridge_t* bridge,
    const float* template_features,
    uint32_t feature_dim
);

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_sleep_verify_bidirectional(const cochlea_sleep_bridge_t* bridge);
uint64_t cochlea_sleep_get_last_outbound(const cochlea_sleep_bridge_t* bridge);
uint64_t cochlea_sleep_get_last_inbound(const cochlea_sleep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_SLEEP_BRIDGE_H */
