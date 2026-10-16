/**
 * @file nimcp_fractal_cognitive_thalamic_bridge.h
 * @brief Bridge between Fractal Cognitive system and thalamic router
 *
 * WHAT: Routes fractal cognitive signals through attention-gated thalamic pathways
 * WHY: Scale-free patterns require multi-scale thalamic routing
 * HOW: Packages fractal signals, routes via thalamic attention at multiple scales
 *
 * BIOLOGICAL BASIS:
 * - Thalamus mediates multi-scale integration of cortical activity
 * - Fractal patterns emerge from thalamo-cortical resonance loops
 * - Attention modulates which scales of fractal processing reach consciousness
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_FRACTAL_COGNITIVE_THALAMIC_BRIDGE_H
#define NIMCP_FRACTAL_COGNITIVE_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FRACTAL_SIGNAL_SCALE_UP     0x0701
#define FRACTAL_SIGNAL_SCALE_DOWN   0x0702
#define FRACTAL_SIGNAL_INTEGRATE    0x0703
#define FRACTAL_SIGNAL_RESONATE     0x0704

typedef struct {
    uint32_t signal_type;
    float scale_level;
    float complexity;
    float urgency;
    void* fractal_data;
    uint32_t data_size;
    uint64_t timestamp_us;
} fractal_cognitive_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_scale_boost;
    float min_complexity_threshold;
    float resonance_boost;
} fractal_cognitive_thalamic_config_t;

typedef struct fractal_cognitive_thalamic_bridge fractal_cognitive_thalamic_bridge_t;

fractal_cognitive_thalamic_config_t fractal_cognitive_thalamic_default_config(void);
fractal_cognitive_thalamic_bridge_t* fractal_cognitive_thalamic_bridge_create(void* fractal_cognitive, thalamic_router_t* router, const fractal_cognitive_thalamic_config_t* config);
void fractal_cognitive_thalamic_bridge_destroy(fractal_cognitive_thalamic_bridge_t* bridge);
int fractal_cognitive_thalamic_bridge_reset(fractal_cognitive_thalamic_bridge_t* bridge);
int fractal_cognitive_thalamic_route_scale(fractal_cognitive_thalamic_bridge_t* bridge, const fractal_cognitive_thalamic_signal_t* signal);
int fractal_cognitive_thalamic_route_integration(fractal_cognitive_thalamic_bridge_t* bridge, const void* scales, uint32_t num_scales);
int fractal_cognitive_thalamic_set_attention(fractal_cognitive_thalamic_bridge_t* bridge, float attention);
int fractal_cognitive_thalamic_get_attention(fractal_cognitive_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t scales_routed;
    uint64_t integrations_performed;
    uint64_t resonances_triggered;
    float avg_complexity;
} fractal_cognitive_thalamic_stats_t;

int fractal_cognitive_thalamic_bridge_get_stats(fractal_cognitive_thalamic_bridge_t* bridge, fractal_cognitive_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FRACTAL_COGNITIVE_THALAMIC_BRIDGE_H */
