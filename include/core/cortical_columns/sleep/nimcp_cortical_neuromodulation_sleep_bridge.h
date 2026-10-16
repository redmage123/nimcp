/**
 * @file nimcp_cortical_neuromodulation_sleep_bridge.h
 * @brief Sleep-Cortical Neuromodulation Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Integration between sleep/wake system and cortical neuromodulation
 * WHY:  Sleep states are defined by neuromodulator levels (ACh, NE, 5-HT)
 * HOW:  Sleep state drives neuromodulator availability in cortical processing
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: High ACh, NE, 5-HT (aminergic mode)
 * - NREM: Low ACh, reduced NE/5-HT (cholinergic withdrawal)
 * - REM: High ACh, low NE/5-HT (paradoxical state)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_NEUROMODULATION_SLEEP_BRIDGE_H
#define NIMCP_CORTICAL_NEUROMODULATION_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Neuromodulator levels by sleep state */
#define NEUROMOD_SLEEP_ACH_AWAKE        1.0f
#define NEUROMOD_SLEEP_ACH_NREM         0.2f
#define NEUROMOD_SLEEP_ACH_REM          1.0f

#define NEUROMOD_SLEEP_NE_AWAKE         1.0f
#define NEUROMOD_SLEEP_NE_NREM          0.1f
#define NEUROMOD_SLEEP_NE_REM           0.0f  /* Essentially zero in REM */

#define NEUROMOD_SLEEP_SEROTONIN_AWAKE  1.0f
#define NEUROMOD_SLEEP_SEROTONIN_NREM   0.3f
#define NEUROMOD_SLEEP_SEROTONIN_REM    0.0f

typedef struct {
    bool enable_ach_modulation;
    bool enable_ne_modulation;
    bool enable_serotonin_modulation;
    float modulation_strength;
} cortical_neuromodulation_sleep_config_t;

typedef struct {
    sleep_state_t current_state;
    float acetylcholine_level;
    float norepinephrine_level;
    float serotonin_level;
} cortical_neuromodulation_sleep_effects_t;

typedef struct cortical_neuromodulation_sleep_bridge_struct* cortical_neuromodulation_sleep_bridge_t;

int cortical_neuromodulation_sleep_default_config(cortical_neuromodulation_sleep_config_t* config);
cortical_neuromodulation_sleep_bridge_t cortical_neuromodulation_sleep_bridge_create(
    const cortical_neuromodulation_sleep_config_t* config,
    void* neuromodulation_module,
    sleep_system_t sleep);
void cortical_neuromodulation_sleep_bridge_destroy(cortical_neuromodulation_sleep_bridge_t bridge);
int cortical_neuromodulation_sleep_update(cortical_neuromodulation_sleep_bridge_t bridge);
float cortical_neuromodulation_sleep_get_ach(const cortical_neuromodulation_sleep_bridge_t bridge);
float cortical_neuromodulation_sleep_get_ne(const cortical_neuromodulation_sleep_bridge_t bridge);
float cortical_neuromodulation_sleep_get_serotonin(const cortical_neuromodulation_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_NEUROMODULATION_SLEEP_BRIDGE_H */
