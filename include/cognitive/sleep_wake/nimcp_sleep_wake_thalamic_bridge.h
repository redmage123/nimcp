/**
 * @file nimcp_sleep_wake_thalamic_bridge.h
 * @brief Bridge between Sleep-Wake system and thalamic router
 *
 * WHAT: Routes arousal signals through thalamic pathways
 * WHY: Thalamus is critical for sleep-wake transitions and arousal
 * HOW: Modulates thalamic gating based on arousal state
 *
 * BIOLOGICAL BASIS:
 * - Thalamus is key relay for arousal system
 * - TRN modulates during sleep-wake transitions
 * - Burst vs tonic firing modes change with arousal
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SLEEP_WAKE_THALAMIC_BRIDGE_H
#define NIMCP_SLEEP_WAKE_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SLEEP_WAKE_SIGNAL_AROUSAL    0x0D01
#define SLEEP_WAKE_SIGNAL_TRANSITION 0x0D02
#define SLEEP_WAKE_SIGNAL_CIRCADIAN  0x0D03
#define SLEEP_WAKE_SIGNAL_PRESSURE   0x0D04

typedef struct {
    uint32_t signal_type;
    float arousal_level;
    float sleep_pressure;
    float circadian_phase;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} sleep_wake_thalamic_signal_t;

typedef struct {
    bool enable_arousal_modulation;
    bool enable_transition_gating;
    float min_arousal_threshold;
    float transition_threshold;
} sleep_wake_thalamic_config_t;

typedef struct sleep_wake_thalamic_bridge sleep_wake_thalamic_bridge_t;

sleep_wake_thalamic_config_t sleep_wake_thalamic_default_config(void);
sleep_wake_thalamic_bridge_t* sleep_wake_thalamic_bridge_create(void* sleep_wake, thalamic_router_t* router, const sleep_wake_thalamic_config_t* config);
void sleep_wake_thalamic_bridge_destroy(sleep_wake_thalamic_bridge_t* bridge);
int sleep_wake_thalamic_bridge_reset(sleep_wake_thalamic_bridge_t* bridge);
int sleep_wake_thalamic_route_arousal(sleep_wake_thalamic_bridge_t* bridge, const sleep_wake_thalamic_signal_t* signal);
int sleep_wake_thalamic_modulate_gating(sleep_wake_thalamic_bridge_t* bridge, float arousal_level);
int sleep_wake_thalamic_set_attention(sleep_wake_thalamic_bridge_t* bridge, float attention);
int sleep_wake_thalamic_get_attention(const sleep_wake_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t arousal_updates;
    uint64_t state_transitions;
    uint64_t circadian_updates;
    float avg_arousal_level;
} sleep_wake_thalamic_stats_t;

int sleep_wake_thalamic_bridge_get_stats(const sleep_wake_thalamic_bridge_t* bridge, sleep_wake_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SLEEP_WAKE_THALAMIC_BRIDGE_H */
