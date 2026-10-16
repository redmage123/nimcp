/**
 * @file nimcp_cortical_predictive_coding_sleep_bridge.h
 * @brief Sleep-Cortical Predictive Coding Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Integration between sleep/wake system and cortical predictive coding
 * WHY:  Sleep alters prediction error processing and predictive model updating
 * HOW:  Sleep state modulates prediction weight, error sensitivity, and learning
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Active prediction error minimization, online learning
 * - NREM: Offline model consolidation, reduced error propagation
 * - REM: Model exploration, altered prediction weights
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_PREDICTIVE_CODING_SLEEP_BRIDGE_H
#define NIMCP_CORTICAL_PREDICTIVE_CODING_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRED_SLEEP_ERROR_AWAKE       1.0f
#define PRED_SLEEP_ERROR_NREM        0.2f  /* Reduced error propagation */
#define PRED_SLEEP_ERROR_REM         0.6f

#define PRED_SLEEP_LEARNING_AWAKE    1.0f
#define PRED_SLEEP_LEARNING_NREM     0.1f  /* Consolidation, not new learning */
#define PRED_SLEEP_LEARNING_REM      0.3f

typedef struct {
    bool enable_error_modulation;
    bool enable_learning_modulation;
    float modulation_strength;
} cortical_predictive_coding_sleep_config_t;

typedef struct {
    sleep_state_t current_state;
    float error_weight;
    float learning_rate_factor;
    bool offline_mode;
} cortical_predictive_coding_sleep_effects_t;

typedef struct cortical_predictive_coding_sleep_bridge_struct* cortical_predictive_coding_sleep_bridge_t;

int cortical_predictive_coding_sleep_default_config(cortical_predictive_coding_sleep_config_t* config);
cortical_predictive_coding_sleep_bridge_t cortical_predictive_coding_sleep_bridge_create(
    const cortical_predictive_coding_sleep_config_t* config,
    void* predictive_coding_module,
    sleep_system_t sleep);
void cortical_predictive_coding_sleep_bridge_destroy(cortical_predictive_coding_sleep_bridge_t bridge);
int cortical_predictive_coding_sleep_update(cortical_predictive_coding_sleep_bridge_t bridge);
float cortical_predictive_coding_sleep_get_error_weight(const cortical_predictive_coding_sleep_bridge_t bridge);
float cortical_predictive_coding_sleep_get_learning_rate(const cortical_predictive_coding_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_PREDICTIVE_CODING_SLEEP_BRIDGE_H */
