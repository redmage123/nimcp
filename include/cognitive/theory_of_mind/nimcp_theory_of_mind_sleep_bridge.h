/**
 * @file nimcp_theory_of_mind_sleep_bridge.h
 * @brief Sleep-Theory of Mind Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and theory of mind
 * WHY:  Social cognition and perspective-taking depend on alertness and rest
 * HOW:  Sleep state modulates ToM inference accuracy, empathy, and mentalizing
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full social cognition, accurate perspective-taking
 * - DROWSY: Reduced mentalizing (egocentric bias increases)
 * - NREM: Social cognition offline (consolidates social learning)
 * - REM: Internal simulation of social scenarios (theory rehearsal)
 *
 * Sleep deprivation effects:
 * - Reduced ability to infer others' mental states
 * - Egocentric bias (difficulty taking others' perspectives)
 * - Impaired emotion recognition
 * - Reduced empathy and social sensitivity
 * - Difficulty with false belief tasks
 *
 * Sleep benefits ToM:
 * - NREM consolidates social learning and perspective-taking skills
 * - REM rehearses social scenarios and theory building
 * - Adequate sleep restores mentalizing accuracy
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_THEORY_OF_MIND_SLEEP_BRIDGE_H
#define NIMCP_THEORY_OF_MIND_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define TOM_SLEEP_MENTALIZING_AWAKE       1.0f
#define TOM_SLEEP_MENTALIZING_DROWSY      0.6f
#define TOM_SLEEP_MENTALIZING_LIGHT_NREM  0.1f
#define TOM_SLEEP_MENTALIZING_DEEP_NREM   0.0f
#define TOM_SLEEP_MENTALIZING_REM         0.5f  /* Internal simulation */

#define TOM_SLEEP_EMPATHY_AWAKE      1.0f
#define TOM_SLEEP_EMPATHY_DROWSY     0.7f
#define TOM_SLEEP_EMPATHY_NREM       0.2f
#define TOM_SLEEP_EMPATHY_REM        0.8f  /* Dreams enhance emotional processing */

typedef struct {
    bool enable_mentalizing_modulation;
    bool enable_empathy_modulation;
    float modulation_strength;
} tom_sleep_config_t;

typedef struct {
    float mentalizing_accuracy_factor;
    float empathy_factor;
    float perspective_taking_factor;
    float egocentric_bias_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool social_cognition_offline;
} tom_sleep_effects_t;

typedef struct tom_sleep_bridge_struct* tom_sleep_bridge_t;

int tom_sleep_default_config(tom_sleep_config_t* config);
tom_sleep_bridge_t tom_sleep_bridge_create(const tom_sleep_config_t* config, sleep_system_t sleep);
void tom_sleep_bridge_destroy(tom_sleep_bridge_t bridge);
int tom_sleep_update(tom_sleep_bridge_t bridge);
int tom_sleep_get_effects(const tom_sleep_bridge_t bridge, tom_sleep_effects_t* effects);
float tom_sleep_get_mentalizing_accuracy(const tom_sleep_bridge_t bridge);
bool tom_sleep_is_offline(const tom_sleep_bridge_t bridge);

float tom_sleep_mentalizing_for_state(sleep_state_t state);
float tom_sleep_empathy_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THEORY_OF_MIND_SLEEP_BRIDGE_H */
