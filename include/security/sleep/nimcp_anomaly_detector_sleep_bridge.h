/**
 * @file nimcp_anomaly_detector_sleep_bridge.h
 * @brief Sleep-Anomaly Detector Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between sleep/wake system and anomaly detector
 * WHY:  Sleep states affect threat detection sensitivity and learning rates
 * HOW:  Sleep state modulates anomaly threshold, learning rate, false positive tolerance
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full anomaly detection sensitivity
 * - DROWSY: Reduced sensitivity, more false positives tolerated
 * - LIGHT_NREM: Low sensitivity, consolidation mode
 * - DEEP_NREM: Minimal detection, learning disabled
 * - REM: Moderate sensitivity, learning exploration
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ANOMALY_DETECTOR_SLEEP_BRIDGE_H
#define NIMCP_ANOMALY_DETECTOR_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Anomaly threshold modulation (higher = less sensitive) */
#define ANOMALY_SLEEP_THRESH_AWAKE          1.0f
#define ANOMALY_SLEEP_THRESH_DROWSY         1.1f
#define ANOMALY_SLEEP_THRESH_LIGHT_NREM     1.3f
#define ANOMALY_SLEEP_THRESH_DEEP_NREM      1.5f
#define ANOMALY_SLEEP_THRESH_REM            1.2f

/* Learning rate modulation */
#define ANOMALY_SLEEP_LEARN_AWAKE           1.0f
#define ANOMALY_SLEEP_LEARN_DROWSY          0.8f
#define ANOMALY_SLEEP_LEARN_LIGHT_NREM      0.3f
#define ANOMALY_SLEEP_LEARN_DEEP_NREM       0.1f
#define ANOMALY_SLEEP_LEARN_REM             0.5f

/* False positive tolerance modulation (higher = more tolerant) */
#define ANOMALY_SLEEP_FP_AWAKE              1.0f
#define ANOMALY_SLEEP_FP_DROWSY             1.2f
#define ANOMALY_SLEEP_FP_LIGHT_NREM         1.5f
#define ANOMALY_SLEEP_FP_DEEP_NREM          2.0f
#define ANOMALY_SLEEP_FP_REM                1.3f

typedef struct {
    bool enable_threshold_modulation;
    bool enable_learning_modulation;
    bool enable_fp_tolerance_modulation;
    float modulation_strength;
} anomaly_detector_sleep_config_t;

typedef struct {
    float anomaly_threshold_factor;
    float learning_rate_factor;
    float fp_tolerance_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool learning_enabled;
} anomaly_detector_sleep_effects_t;

typedef struct anomaly_detector_sleep_bridge_struct* anomaly_detector_sleep_bridge_t;

int anomaly_detector_sleep_default_config(anomaly_detector_sleep_config_t* config);
anomaly_detector_sleep_bridge_t anomaly_detector_sleep_bridge_create(
    const anomaly_detector_sleep_config_t* config,
    sleep_system_t sleep_system);
void anomaly_detector_sleep_bridge_destroy(anomaly_detector_sleep_bridge_t bridge);
int anomaly_detector_sleep_update(anomaly_detector_sleep_bridge_t bridge);
int anomaly_detector_sleep_get_effects(const anomaly_detector_sleep_bridge_t bridge,
                                        anomaly_detector_sleep_effects_t* effects);
float anomaly_detector_sleep_get_threshold(const anomaly_detector_sleep_bridge_t bridge, float base);
float anomaly_detector_sleep_get_learning_rate(const anomaly_detector_sleep_bridge_t bridge, float base);
bool anomaly_detector_sleep_is_learning_enabled(const anomaly_detector_sleep_bridge_t bridge);

float anomaly_sleep_get_thresh_factor(sleep_state_t state);
float anomaly_sleep_get_learn_factor(sleep_state_t state);
float anomaly_sleep_get_fp_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ANOMALY_DETECTOR_SLEEP_BRIDGE_H */
