/**
 * @file nimcp_working_memory_sleep_bridge.h
 * @brief Sleep-Working Memory Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake and working memory
 * WHY:  Working memory capacity is highly sleep-dependent
 * HOW:  Sleep state modulates WM capacity, rehearsal efficiency, decay rate
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full WM capacity (~7±2 items), normal rehearsal
 * - DROWSY: Reduced capacity (~5 items), impaired maintenance
 * - LIGHT_NREM: WM contents transferred to consolidation buffer
 * - DEEP_NREM: WM offline, hippocampal replay active
 * - REM: Limited WM, dream narrative processing
 *
 * Sleep deprivation effects on WM:
 * - Reduced effective capacity
 * - Faster decay without rehearsal
 * - Impaired phonological loop
 * - Executive function degradation
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WORKING_MEMORY_SLEEP_BRIDGE_H
#define NIMCP_WORKING_MEMORY_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Capacity modulation (fraction of normal 7±2) */
#define WM_SLEEP_CAPACITY_AWAKE         1.0f   /* Full 7±2 */
#define WM_SLEEP_CAPACITY_DROWSY        0.7f   /* ~5 items */
#define WM_SLEEP_CAPACITY_LIGHT_NREM    0.3f   /* ~2 items (residual) */
#define WM_SLEEP_CAPACITY_DEEP_NREM     0.0f   /* Offline */
#define WM_SLEEP_CAPACITY_REM           0.4f   /* Dream WM */

/* Decay rate modulation (higher = faster decay) */
#define WM_SLEEP_DECAY_AWAKE            1.0f
#define WM_SLEEP_DECAY_DROWSY           1.5f
#define WM_SLEEP_DECAY_LIGHT_NREM       2.0f
#define WM_SLEEP_DECAY_DEEP_NREM        0.0f   /* No decay (offline) */
#define WM_SLEEP_DECAY_REM              1.2f

typedef struct {
    bool enable_capacity_modulation;
    bool enable_decay_modulation;
    bool enable_transfer_on_sleep;
    float modulation_strength;
} working_memory_sleep_config_t;

typedef struct {
    float capacity_factor;
    float decay_rate_factor;
    float rehearsal_efficiency;
    sleep_state_t current_state;
    float sleep_pressure;
    bool wm_offline;
    bool consolidation_active;
} working_memory_sleep_effects_t;

typedef struct working_memory_sleep_bridge_struct* working_memory_sleep_bridge_t;

int working_memory_sleep_default_config(working_memory_sleep_config_t* config);
working_memory_sleep_bridge_t working_memory_sleep_bridge_create(const working_memory_sleep_config_t* config, sleep_system_t sleep);
void working_memory_sleep_bridge_destroy(working_memory_sleep_bridge_t bridge);
int working_memory_sleep_update(working_memory_sleep_bridge_t bridge);
int working_memory_sleep_get_effects(const working_memory_sleep_bridge_t bridge, working_memory_sleep_effects_t* effects);
float working_memory_sleep_get_capacity(const working_memory_sleep_bridge_t bridge);
bool working_memory_sleep_is_offline(const working_memory_sleep_bridge_t bridge);

float working_memory_sleep_capacity_for_state(sleep_state_t state);
float working_memory_sleep_decay_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORKING_MEMORY_SLEEP_BRIDGE_H */
