/**
 * @file nimcp_attention_sleep_bridge.h
 * @brief Sleep-Attention Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and attention
 * WHY:  Attention capacity is fundamentally dependent on sleep state
 * HOW:  Sleep state modulates attention bandwidth, vigilance, and spotlight size
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full attention capacity, normal selective attention
 * - DROWSY: Reduced capacity (attention lapses), slower orienting
 * - NREM: Minimal external attention (offline processing)
 * - REM: Internal attention only (dream focus)
 *
 * Sleep deprivation causes:
 * - Attention lapses (microsleeps)
 * - Reduced sustained attention
 * - Impaired selective attention
 * - Slower attentional orienting
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ATTENTION_SLEEP_BRIDGE_H
#define NIMCP_ATTENTION_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define ATTN_SLEEP_CAPACITY_AWAKE       1.0f
#define ATTN_SLEEP_CAPACITY_DROWSY      0.6f
#define ATTN_SLEEP_CAPACITY_LIGHT_NREM  0.1f
#define ATTN_SLEEP_CAPACITY_DEEP_NREM   0.0f
#define ATTN_SLEEP_CAPACITY_REM         0.3f  /* Internal only */

#define ATTN_SLEEP_VIGILANCE_AWAKE      1.0f
#define ATTN_SLEEP_VIGILANCE_DROWSY     0.5f
#define ATTN_SLEEP_VIGILANCE_NREM       0.0f
#define ATTN_SLEEP_VIGILANCE_REM        0.2f

typedef struct {
    bool enable_capacity_modulation;
    bool enable_vigilance_modulation;
    float modulation_strength;
} attention_sleep_config_t;

typedef struct {
    float capacity_factor;
    float vigilance_factor;
    float spotlight_size_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool attention_offline;
} attention_sleep_effects_t;

typedef struct attention_sleep_bridge_struct* attention_sleep_bridge_t;

int attention_sleep_default_config(attention_sleep_config_t* config);
attention_sleep_bridge_t attention_sleep_bridge_create(const attention_sleep_config_t* config, sleep_system_t sleep);
void attention_sleep_bridge_destroy(attention_sleep_bridge_t bridge);
int attention_sleep_update(attention_sleep_bridge_t bridge);
int attention_sleep_get_effects(const attention_sleep_bridge_t bridge, attention_sleep_effects_t* effects);
float attention_sleep_get_capacity(const attention_sleep_bridge_t bridge);
bool attention_sleep_is_offline(const attention_sleep_bridge_t bridge);

float attention_sleep_capacity_for_state(sleep_state_t state);
float attention_sleep_vigilance_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ATTENTION_SLEEP_BRIDGE_H */
