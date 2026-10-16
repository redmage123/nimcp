/**
 * @file nimcp_executive_sleep_bridge.h
 * @brief Sleep-Executive Function Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake and executive functions
 * WHY:  Prefrontal cortex is highly sensitive to sleep deprivation
 * HOW:  Sleep state modulates inhibition, flexibility, planning capacity
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full executive control, inhibition, planning
 * - DROWSY: Impaired inhibition, reduced cognitive flexibility
 * - NREM: Executive functions offline (PFC recovery)
 * - REM: Reduced executive control (explains dream bizarreness)
 *
 * Sleep deprivation effects:
 * - Impaired inhibitory control (Stroop deficits)
 * - Reduced cognitive flexibility
 * - Poor decision making under uncertainty
 * - Impaired task switching
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EXECUTIVE_SLEEP_BRIDGE_H
#define NIMCP_EXECUTIVE_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Inhibitory control modulation */
#define EXEC_SLEEP_INHIBITION_AWAKE      1.0f
#define EXEC_SLEEP_INHIBITION_DROWSY     0.6f
#define EXEC_SLEEP_INHIBITION_LIGHT_NREM 0.1f
#define EXEC_SLEEP_INHIBITION_DEEP_NREM  0.0f
#define EXEC_SLEEP_INHIBITION_REM        0.3f  /* Reduced in dreams */

/* Cognitive flexibility modulation */
#define EXEC_SLEEP_FLEXIBILITY_AWAKE      1.0f
#define EXEC_SLEEP_FLEXIBILITY_DROWSY     0.5f
#define EXEC_SLEEP_FLEXIBILITY_NREM       0.0f
#define EXEC_SLEEP_FLEXIBILITY_REM        0.4f

/* Task switching cost (higher = worse, slower switches) */
#define EXEC_SLEEP_SWITCH_COST_AWAKE      1.0f
#define EXEC_SLEEP_SWITCH_COST_DROWSY     1.5f
#define EXEC_SLEEP_SWITCH_COST_NREM       10.0f  /* Essentially blocked */
#define EXEC_SLEEP_SWITCH_COST_REM        2.0f

typedef struct {
    bool enable_inhibition_modulation;
    bool enable_flexibility_modulation;
    bool enable_switch_cost_modulation;
    float modulation_strength;
} executive_sleep_config_t;

typedef struct {
    float inhibition_factor;
    float flexibility_factor;
    float switch_cost_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool executive_offline;
} executive_sleep_effects_t;

typedef struct executive_sleep_bridge_struct* executive_sleep_bridge_t;

int executive_sleep_default_config(executive_sleep_config_t* config);
executive_sleep_bridge_t executive_sleep_bridge_create(const executive_sleep_config_t* config, sleep_system_t sleep);
void executive_sleep_bridge_destroy(executive_sleep_bridge_t bridge);
int executive_sleep_update(executive_sleep_bridge_t bridge);
int executive_sleep_get_effects(const executive_sleep_bridge_t bridge, executive_sleep_effects_t* effects);
float executive_sleep_get_inhibition(const executive_sleep_bridge_t bridge);
bool executive_sleep_is_offline(const executive_sleep_bridge_t bridge);

float executive_sleep_inhibition_for_state(sleep_state_t state);
float executive_sleep_flexibility_for_state(sleep_state_t state);
float executive_sleep_switch_cost_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXECUTIVE_SLEEP_BRIDGE_H */
