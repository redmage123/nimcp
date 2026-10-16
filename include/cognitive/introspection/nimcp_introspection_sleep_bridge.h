/**
 * @file nimcp_introspection_sleep_bridge.h
 * @brief Sleep-Introspection Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and introspection
 * WHY:  Metacognition and consciousness metrics depend on arousal state
 * HOW:  Sleep state modulates introspective accuracy, consciousness level, and self-monitoring
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full conscious introspection, accurate metacognition
 * - DROWSY: Reduced introspective accuracy, metacognitive lapses
 * - LIGHT NREM: Minimal consciousness, introspection offline
 * - DEEP NREM: Unconscious state, no introspection
 * - REM: Dream consciousness (altered introspection, reduced critical thinking)
 *
 * Sleep deprivation effects:
 * - Impaired metacognitive accuracy
 * - Reduced consciousness metrics (lower integrated information)
 * - Poor self-monitoring of cognitive states
 * - Difficulty detecting errors
 * - Reduced awareness of uncertainty
 *
 * Sleep benefits introspection:
 * - NREM restores metacognitive circuits
 * - Adequate sleep maintains integrated information (Phi)
 * - Sleep consolidates introspective knowledge
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_INTROSPECTION_SLEEP_BRIDGE_H
#define NIMCP_INTROSPECTION_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define INTROSPECTION_SLEEP_METACOG_AWAKE       1.0f
#define INTROSPECTION_SLEEP_METACOG_DROWSY      0.6f
#define INTROSPECTION_SLEEP_METACOG_LIGHT_NREM  0.1f
#define INTROSPECTION_SLEEP_METACOG_DEEP_NREM   0.0f
#define INTROSPECTION_SLEEP_METACOG_REM         0.3f  /* Dream awareness */

#define INTROSPECTION_SLEEP_CONSCIOUSNESS_AWAKE       1.0f
#define INTROSPECTION_SLEEP_CONSCIOUSNESS_DROWSY      0.7f
#define INTROSPECTION_SLEEP_CONSCIOUSNESS_LIGHT_NREM  0.2f
#define INTROSPECTION_SLEEP_CONSCIOUSNESS_DEEP_NREM   0.0f
#define INTROSPECTION_SLEEP_CONSCIOUSNESS_REM         0.5f

typedef struct {
    bool enable_metacognition_modulation;
    bool enable_consciousness_modulation;
    float modulation_strength;
} introspection_sleep_config_t;

typedef struct {
    float metacognitive_accuracy_factor;
    float consciousness_level_factor;
    float introspective_access_factor;
    float uncertainty_awareness_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool introspection_offline;
} introspection_sleep_effects_t;

typedef struct introspection_sleep_bridge_struct* introspection_sleep_bridge_t;

int introspection_sleep_default_config(introspection_sleep_config_t* config);
introspection_sleep_bridge_t introspection_sleep_bridge_create(const introspection_sleep_config_t* config, sleep_system_t sleep);
void introspection_sleep_bridge_destroy(introspection_sleep_bridge_t bridge);
int introspection_sleep_update(introspection_sleep_bridge_t bridge);
int introspection_sleep_get_effects(const introspection_sleep_bridge_t bridge, introspection_sleep_effects_t* effects);
float introspection_sleep_get_metacognitive_accuracy(const introspection_sleep_bridge_t bridge);
bool introspection_sleep_is_offline(const introspection_sleep_bridge_t bridge);

float introspection_sleep_metacognition_for_state(sleep_state_t state);
float introspection_sleep_consciousness_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTROSPECTION_SLEEP_BRIDGE_H */
