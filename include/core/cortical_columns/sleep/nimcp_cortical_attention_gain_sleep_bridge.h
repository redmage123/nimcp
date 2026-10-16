/**
 * @file nimcp_cortical_attention_gain_sleep_bridge.h
 * @brief Sleep-Cortical Attention Gain Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Integration between sleep/wake system and cortical attention gain control
 * WHY:  Sleep reduces attentional gain modulation in cortical areas
 * HOW:  Sleep state modulates gain factors applied to cortical activations
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full gain modulation for selective attention
 * - DROWSY: Reduced gain control, attention lapses
 * - NREM: Minimal gain modulation, offline processing
 * - REM: Internal gain modulation for dream content
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_ATTENTION_GAIN_SLEEP_BRIDGE_H
#define NIMCP_CORTICAL_ATTENTION_GAIN_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAIN_SLEEP_AWAKE       1.0f
#define GAIN_SLEEP_DROWSY      0.6f
#define GAIN_SLEEP_LIGHT_NREM  0.2f
#define GAIN_SLEEP_DEEP_NREM   0.0f
#define GAIN_SLEEP_REM         0.4f

typedef struct {
    bool enable_gain_modulation;
    float modulation_strength;
} cortical_attention_gain_sleep_config_t;

typedef struct {
    sleep_state_t current_state;
    float gain_factor;
    bool gain_offline;
} cortical_attention_gain_sleep_effects_t;

typedef struct cortical_attention_gain_sleep_bridge_struct* cortical_attention_gain_sleep_bridge_t;

int cortical_attention_gain_sleep_default_config(cortical_attention_gain_sleep_config_t* config);
cortical_attention_gain_sleep_bridge_t cortical_attention_gain_sleep_bridge_create(
    const cortical_attention_gain_sleep_config_t* config,
    void* attention_gain_module,
    sleep_system_t sleep);
void cortical_attention_gain_sleep_bridge_destroy(cortical_attention_gain_sleep_bridge_t bridge);
int cortical_attention_gain_sleep_update(cortical_attention_gain_sleep_bridge_t bridge);
float cortical_attention_gain_sleep_get_gain_factor(const cortical_attention_gain_sleep_bridge_t bridge);
bool cortical_attention_gain_sleep_is_offline(const cortical_attention_gain_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_ATTENTION_GAIN_SLEEP_BRIDGE_H */
