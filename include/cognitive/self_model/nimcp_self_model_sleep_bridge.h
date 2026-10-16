/**
 * @file nimcp_self_model_sleep_bridge.h
 * @brief Sleep-Self Model Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and self-model
 * WHY:  Self-awareness and metacognition depend on arousal and conscious state
 * HOW:  Sleep state modulates self-reflection, self-monitoring, and self-concept
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full self-awareness and metacognitive monitoring
 * - DROWSY: Reduced self-reflection (mind wandering increases)
 * - NREM: Self-awareness minimal (unconscious state)
 * - REM: Altered self-concept (dream self, reduced critical reflection)
 *
 * Sleep deprivation effects:
 * - Impaired self-monitoring and insight
 * - Reduced metacognitive accuracy
 * - Difficulty updating self-model
 * - Poor self-regulation
 * - Fragmented sense of self
 *
 * Sleep benefits self-model:
 * - NREM consolidates self-relevant memories
 * - REM integrates new experiences into self-concept
 * - Adequate sleep maintains coherent self-model
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SELF_MODEL_SLEEP_BRIDGE_H
#define NIMCP_SELF_MODEL_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define SELF_MODEL_SLEEP_AWARENESS_AWAKE       1.0f
#define SELF_MODEL_SLEEP_AWARENESS_DROWSY      0.6f
#define SELF_MODEL_SLEEP_AWARENESS_LIGHT_NREM  0.2f
#define SELF_MODEL_SLEEP_AWARENESS_DEEP_NREM   0.0f
#define SELF_MODEL_SLEEP_AWARENESS_REM         0.3f  /* Dream self */

#define SELF_MODEL_SLEEP_REFLECTION_AWAKE      1.0f
#define SELF_MODEL_SLEEP_REFLECTION_DROWSY     0.5f
#define SELF_MODEL_SLEEP_REFLECTION_NREM       0.0f
#define SELF_MODEL_SLEEP_REFLECTION_REM        0.4f

typedef struct {
    bool enable_awareness_modulation;
    bool enable_reflection_modulation;
    float modulation_strength;
} self_model_sleep_config_t;

typedef struct {
    float self_awareness_factor;
    float metacognition_factor;
    float self_reflection_factor;
    float self_monitoring_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool self_awareness_offline;
} self_model_sleep_effects_t;

typedef struct self_model_sleep_bridge_struct* self_model_sleep_bridge_t;

int self_model_sleep_default_config(self_model_sleep_config_t* config);
self_model_sleep_bridge_t self_model_sleep_bridge_create(const self_model_sleep_config_t* config, sleep_system_t sleep);
void self_model_sleep_bridge_destroy(self_model_sleep_bridge_t bridge);
int self_model_sleep_update(self_model_sleep_bridge_t bridge);
int self_model_sleep_get_effects(const self_model_sleep_bridge_t bridge, self_model_sleep_effects_t* effects);
float self_model_sleep_get_awareness(const self_model_sleep_bridge_t bridge);
bool self_model_sleep_is_offline(const self_model_sleep_bridge_t bridge);

float self_model_sleep_awareness_for_state(sleep_state_t state);
float self_model_sleep_reflection_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_MODEL_SLEEP_BRIDGE_H */
