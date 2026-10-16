/**
 * @file nimcp_cortical_temporal_sleep_bridge.h
 * @brief Sleep-Cortical Temporal Processing Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Integration between sleep/wake system and cortical temporal processing
 * WHY:  Sleep affects temporal integration windows and sequence processing
 * HOW:  Sleep state modulates temporal integration time constants
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Fast temporal dynamics, precise timing
 * - NREM: Slowed temporal processing, replay at different speeds
 * - REM: Altered temporal dynamics
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_TEMPORAL_SLEEP_BRIDGE_H
#define NIMCP_CORTICAL_TEMPORAL_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEMPORAL_SLEEP_TIMESCALE_AWAKE      1.0f
#define TEMPORAL_SLEEP_TIMESCALE_NREM       0.5f   /* Slowed processing */
#define TEMPORAL_SLEEP_TIMESCALE_REM        0.8f

typedef struct {
    bool enable_timescale_modulation;
    float modulation_strength;
} cortical_temporal_sleep_config_t;

typedef struct {
    sleep_state_t current_state;
    float timescale_factor;
    bool temporal_offline;
} cortical_temporal_sleep_effects_t;

typedef struct cortical_temporal_sleep_bridge_struct* cortical_temporal_sleep_bridge_t;

int cortical_temporal_sleep_default_config(cortical_temporal_sleep_config_t* config);
cortical_temporal_sleep_bridge_t cortical_temporal_sleep_bridge_create(
    const cortical_temporal_sleep_config_t* config,
    void* temporal_module,
    sleep_system_t sleep);
void cortical_temporal_sleep_bridge_destroy(cortical_temporal_sleep_bridge_t bridge);
int cortical_temporal_sleep_update(cortical_temporal_sleep_bridge_t bridge);
float cortical_temporal_sleep_get_timescale(const cortical_temporal_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_TEMPORAL_SLEEP_BRIDGE_H */
