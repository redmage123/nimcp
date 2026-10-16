/**
 * @file nimcp_mental_health_sleep_bridge.h
 * @brief Sleep-Mental Health Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and mental health
 * WHY:  Psychiatric stability is fundamentally dependent on sleep quality
 * HOW:  Sleep state modulates disorder risk, symptom severity, and resilience
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Normal psychiatric baseline, symptoms at baseline
 * - DROWSY: Increased vulnerability to mood/anxiety symptoms
 * - NREM: Restoration of emotional regulation circuits
 * - REM: Emotional memory processing, nightmare risk in PTSD
 *
 * Sleep deprivation effects:
 * - Increased risk of mania, psychosis, paranoia
 * - Worsened anxiety and PTSD symptoms
 * - Reduced emotional regulation (borderline symptoms)
 * - Impaired reality testing (schizophrenia risk)
 * - Greater impulsivity (ADHD, conduct disorder)
 *
 * Sleep benefits mental health:
 * - NREM consolidates emotional regulation
 * - REM processes emotional memories (reduces PTSD when healthy)
 * - Adequate sleep reduces disorder severity across all categories
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MENTAL_HEALTH_SLEEP_BRIDGE_H
#define NIMCP_MENTAL_HEALTH_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define MENTAL_HEALTH_SLEEP_STABILITY_AWAKE       1.0f
#define MENTAL_HEALTH_SLEEP_STABILITY_DROWSY      0.7f
#define MENTAL_HEALTH_SLEEP_STABILITY_LIGHT_NREM  0.9f  /* Restorative */
#define MENTAL_HEALTH_SLEEP_STABILITY_DEEP_NREM   1.1f  /* Most restorative */
#define MENTAL_HEALTH_SLEEP_STABILITY_REM         0.8f  /* Can trigger nightmares */

#define MENTAL_HEALTH_SLEEP_DISORDER_RISK_AWAKE      1.0f
#define MENTAL_HEALTH_SLEEP_DISORDER_RISK_DROWSY     1.3f
#define MENTAL_HEALTH_SLEEP_DISORDER_RISK_NREM       0.7f  /* Protective */
#define MENTAL_HEALTH_SLEEP_DISORDER_RISK_REM        0.9f

typedef struct {
    bool enable_stability_modulation;
    bool enable_risk_modulation;
    float modulation_strength;
} mental_health_sleep_config_t;

typedef struct {
    float psychiatric_stability_factor;
    float disorder_risk_factor;
    float emotional_regulation_factor;
    float reality_testing_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool restoration_active;
} mental_health_sleep_effects_t;

typedef struct mental_health_sleep_bridge_struct* mental_health_sleep_bridge_t;

int mental_health_sleep_default_config(mental_health_sleep_config_t* config);
mental_health_sleep_bridge_t mental_health_sleep_bridge_create(const mental_health_sleep_config_t* config, sleep_system_t sleep);
void mental_health_sleep_bridge_destroy(mental_health_sleep_bridge_t bridge);
int mental_health_sleep_update(mental_health_sleep_bridge_t bridge);
int mental_health_sleep_get_effects(const mental_health_sleep_bridge_t bridge, mental_health_sleep_effects_t* effects);
float mental_health_sleep_get_stability(const mental_health_sleep_bridge_t bridge);
bool mental_health_sleep_is_restoration_active(const mental_health_sleep_bridge_t bridge);

float mental_health_sleep_stability_for_state(sleep_state_t state);
float mental_health_sleep_disorder_risk_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MENTAL_HEALTH_SLEEP_BRIDGE_H */
