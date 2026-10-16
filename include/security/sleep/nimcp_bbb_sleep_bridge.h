/**
 * @file nimcp_bbb_sleep_bridge.h
 * @brief Sleep-BBB Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between sleep/wake system and Blood-Brain Barrier
 * WHY:  BBB permeability and threat vigilance change significantly with sleep state
 * HOW:  Sleep state modulates BBB permeability, detection thresholds, and response urgency
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full BBB protection, maximum threat vigilance, tight junctions
 * - DROWSY: Slightly relaxed vigilance, preparation for sleep clearance
 * - NREM: Increased permeability for glymphatic waste clearance
 * - REM: Maintained permeability with internal processing protection
 *
 * GLYMPHATIC SYSTEM:
 * During sleep, BBB permeability increases to allow cerebrospinal fluid (CSF) to
 * flush metabolic waste from the brain.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BBB_SLEEP_BRIDGE_H
#define NIMCP_BBB_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Permeability factors */
#define BBB_SLEEP_PERMEABILITY_AWAKE       1.0f
#define BBB_SLEEP_PERMEABILITY_DROWSY      1.1f
#define BBB_SLEEP_PERMEABILITY_LIGHT_NREM  1.4f
#define BBB_SLEEP_PERMEABILITY_DEEP_NREM   1.6f
#define BBB_SLEEP_PERMEABILITY_REM         1.3f

/* Detection threshold factors (higher = more permissive) */
#define BBB_SLEEP_DETECTION_AWAKE          1.0f
#define BBB_SLEEP_DETECTION_DROWSY         1.1f
#define BBB_SLEEP_DETECTION_LIGHT_NREM     1.3f
#define BBB_SLEEP_DETECTION_DEEP_NREM      1.5f
#define BBB_SLEEP_DETECTION_REM            1.2f

/* Response urgency factors (lower = less urgent) */
#define BBB_SLEEP_URGENCY_AWAKE            1.0f
#define BBB_SLEEP_URGENCY_DROWSY           0.9f
#define BBB_SLEEP_URGENCY_LIGHT_NREM       0.7f
#define BBB_SLEEP_URGENCY_DEEP_NREM        0.5f
#define BBB_SLEEP_URGENCY_REM              0.6f

typedef struct {
    bool enable_permeability_modulation;
    bool enable_detection_modulation;
    bool enable_response_modulation;
    float modulation_strength;
    bool maintain_critical_protection;
} bbb_sleep_config_t;

typedef struct {
    float permeability_factor;
    float detection_threshold_factor;
    float response_urgency_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool glymphatic_active;
    bool critical_protection_only;
} bbb_sleep_effects_t;

typedef struct bbb_sleep_bridge_struct* bbb_sleep_bridge_t;

int bbb_sleep_default_config(bbb_sleep_config_t* config);
bbb_sleep_bridge_t bbb_sleep_bridge_create(
    const bbb_sleep_config_t* config,
    sleep_system_t sleep_system);
void bbb_sleep_bridge_destroy(bbb_sleep_bridge_t bridge);
int bbb_sleep_update(bbb_sleep_bridge_t bridge);
int bbb_sleep_get_effects(const bbb_sleep_bridge_t bridge, bbb_sleep_effects_t* effects);
float bbb_sleep_get_permeability(const bbb_sleep_bridge_t bridge);
bool bbb_sleep_is_glymphatic_active(const bbb_sleep_bridge_t bridge);
float bbb_sleep_get_detection_threshold(const bbb_sleep_bridge_t bridge, float base);

float bbb_sleep_permeability_for_state(sleep_state_t state);
float bbb_sleep_detection_for_state(sleep_state_t state);
float bbb_sleep_urgency_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BBB_SLEEP_BRIDGE_H */
