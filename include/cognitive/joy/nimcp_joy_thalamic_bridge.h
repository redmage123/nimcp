/**
 * @file nimcp_joy_thalamic_bridge.h
 * @brief Bridge between Joy system and thalamic router
 *
 * WHAT: Routes joy/positive affect through attention-gated thalamic pathways
 * WHY: Joy experience requires conscious awareness via thalamic gating
 * HOW: Packages joy signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Joy involves ventral striatum and orbitofrontal cortex
 * - Thalamus gates reward signals to conscious experience
 * - Attention to positive events enhances joy
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_JOY_THALAMIC_BRIDGE_H
#define NIMCP_JOY_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JOY_SIGNAL_PLEASURE      0x1201
#define JOY_SIGNAL_ANTICIPATION  0x1202
#define JOY_SIGNAL_SAVORING      0x1203
#define JOY_SIGNAL_GRATITUDE     0x1204

typedef struct {
    uint32_t signal_type;
    float joy_intensity;
    float hedonic_value;
    float duration_expectation;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} joy_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_savoring_boost;
    float min_hedonic_threshold;
    float anticipation_threshold;
} joy_thalamic_config_t;

typedef struct joy_thalamic_bridge joy_thalamic_bridge_t;

joy_thalamic_config_t joy_thalamic_default_config(void);
joy_thalamic_bridge_t* joy_thalamic_bridge_create(void* joy, thalamic_router_t* router, const joy_thalamic_config_t* config);
void joy_thalamic_bridge_destroy(joy_thalamic_bridge_t* bridge);
int joy_thalamic_bridge_reset(joy_thalamic_bridge_t* bridge);
int joy_thalamic_route_pleasure(joy_thalamic_bridge_t* bridge, const joy_thalamic_signal_t* signal);
int joy_thalamic_route_savoring(joy_thalamic_bridge_t* bridge, const void* experience, float duration);
int joy_thalamic_set_attention(joy_thalamic_bridge_t* bridge, float attention);
int joy_thalamic_get_attention(joy_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t pleasures_routed;
    uint64_t anticipations_triggered;
    uint64_t savoring_episodes;
    float avg_joy_intensity;
} joy_thalamic_stats_t;

int joy_thalamic_bridge_get_stats(joy_thalamic_bridge_t* bridge, joy_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JOY_THALAMIC_BRIDGE_H */
