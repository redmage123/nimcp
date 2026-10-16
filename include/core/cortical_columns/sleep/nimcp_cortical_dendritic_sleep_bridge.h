/**
 * @file nimcp_cortical_dendritic_sleep_bridge.h
 * @brief Sleep-Cortical Dendritic Processing Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Integration between sleep/wake system and cortical dendritic processing
 * WHY:  Sleep affects dendritic integration and calcium dynamics
 * HOW:  Sleep state modulates dendritic excitability and integration windows
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Active dendritic integration, coincidence detection
 * - NREM: Reduced dendritic excitability, homeostatic downscaling
 * - REM: Altered dendritic integration patterns
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_DENDRITIC_SLEEP_BRIDGE_H
#define NIMCP_CORTICAL_DENDRITIC_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DENDRITIC_SLEEP_EXCITABILITY_AWAKE      1.0f
#define DENDRITIC_SLEEP_EXCITABILITY_NREM       0.6f
#define DENDRITIC_SLEEP_EXCITABILITY_REM        0.8f

typedef struct {
    bool enable_excitability_modulation;
    float modulation_strength;
} cortical_dendritic_sleep_config_t;

typedef struct {
    sleep_state_t current_state;
    float dendritic_excitability;
    float integration_window;
    bool dendritic_offline;
} cortical_dendritic_sleep_effects_t;

typedef struct cortical_dendritic_sleep_bridge_struct* cortical_dendritic_sleep_bridge_t;

int cortical_dendritic_sleep_default_config(cortical_dendritic_sleep_config_t* config);
cortical_dendritic_sleep_bridge_t cortical_dendritic_sleep_bridge_create(
    const cortical_dendritic_sleep_config_t* config,
    void* dendritic_module,
    sleep_system_t sleep);
void cortical_dendritic_sleep_bridge_destroy(cortical_dendritic_sleep_bridge_t bridge);
int cortical_dendritic_sleep_update(cortical_dendritic_sleep_bridge_t bridge);
float cortical_dendritic_sleep_get_excitability(const cortical_dendritic_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_DENDRITIC_SLEEP_BRIDGE_H */
