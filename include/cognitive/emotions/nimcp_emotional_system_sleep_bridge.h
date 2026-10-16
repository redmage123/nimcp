/**
 * @file nimcp_emotional_system_sleep_bridge.h
 * @brief Sleep-Emotional System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and emotional system
 * WHY:  Emotional regulation and reactivity are fundamentally dependent on sleep
 * HOW:  Sleep state modulates emotional regulation, reactivity, and processing
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Normal emotional regulation, appropriate reactivity
 * - DROWSY: Reduced regulation (increased irritability, mood lability)
 * - LIGHT NREM: Emotional processing begins (sorting emotional memories)
 * - DEEP NREM: Emotional consolidation (reduces emotional charge)
 * - REM: Emotional memory integration (can enhance or reduce reactivity)
 *
 * Sleep deprivation effects:
 * - Impaired emotional regulation (60% increase in amygdala reactivity)
 * - Increased negative emotionality
 * - Reduced positive affect
 * - Heightened emotional reactivity to stressors
 * - Difficulty downregulating emotions
 * - Increased risk of emotional dysregulation
 *
 * Sleep benefits emotional system:
 * - NREM reduces emotional intensity of memories
 * - REM processes emotional experiences
 * - Adequate sleep restores prefrontal control over amygdala
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EMOTIONAL_SYSTEM_SLEEP_BRIDGE_H
#define NIMCP_EMOTIONAL_SYSTEM_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define EMOTIONAL_SLEEP_REGULATION_AWAKE       1.0f
#define EMOTIONAL_SLEEP_REGULATION_DROWSY      0.6f
#define EMOTIONAL_SLEEP_REGULATION_LIGHT_NREM  0.3f
#define EMOTIONAL_SLEEP_REGULATION_DEEP_NREM   0.2f
#define EMOTIONAL_SLEEP_REGULATION_REM         0.4f

#define EMOTIONAL_SLEEP_REACTIVITY_AWAKE       1.0f
#define EMOTIONAL_SLEEP_REACTIVITY_DROWSY      1.4f  /* Increased reactivity */
#define EMOTIONAL_SLEEP_REACTIVITY_NREM        0.5f
#define EMOTIONAL_SLEEP_REACTIVITY_REM         1.2f

typedef struct {
    bool enable_regulation_modulation;
    bool enable_reactivity_modulation;
    float modulation_strength;
} emotional_sleep_config_t;

typedef struct {
    float regulation_capacity_factor;
    float emotional_reactivity_factor;
    float positive_affect_factor;
    float negative_affect_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool emotional_processing_active;
} emotional_sleep_effects_t;

typedef struct emotional_sleep_bridge_struct* emotional_sleep_bridge_t;

int emotional_sleep_default_config(emotional_sleep_config_t* config);
emotional_sleep_bridge_t emotional_sleep_bridge_create(const emotional_sleep_config_t* config, sleep_system_t sleep);
void emotional_sleep_bridge_destroy(emotional_sleep_bridge_t bridge);
int emotional_sleep_update(emotional_sleep_bridge_t bridge);
int emotional_sleep_get_effects(const emotional_sleep_bridge_t bridge, emotional_sleep_effects_t* effects);
float emotional_sleep_get_regulation_capacity(const emotional_sleep_bridge_t bridge);
bool emotional_sleep_is_processing_active(const emotional_sleep_bridge_t bridge);

float emotional_sleep_regulation_for_state(sleep_state_t state);
float emotional_sleep_reactivity_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTIONAL_SYSTEM_SLEEP_BRIDGE_H */
