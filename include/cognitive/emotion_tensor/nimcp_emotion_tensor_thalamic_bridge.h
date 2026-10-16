/**
 * @file nimcp_emotion_tensor_thalamic_bridge.h
 * @brief Bridge between Emotion Tensor system and thalamic router
 *
 * WHAT: Routes emotion tensor signals through attention-gated thalamic pathways
 * WHY: Emotional signals require thalamic gating for conscious awareness
 * HOW: Packages tensor signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Thalamus mediates emotional signals between limbic system and cortex
 * - High-arousal emotions get enhanced thalamic routing priority
 * - Attention modulates which emotional components reach consciousness
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EMOTION_TENSOR_THALAMIC_BRIDGE_H
#define NIMCP_EMOTION_TENSOR_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMOTION_TENSOR_SIGNAL_UPDATE    0x0401
#define EMOTION_TENSOR_SIGNAL_BLEND     0x0402
#define EMOTION_TENSOR_SIGNAL_DECAY     0x0403
#define EMOTION_TENSOR_SIGNAL_CONTAGION 0x0404

typedef struct {
    uint32_t signal_type;
    float arousal_level;
    float valence;
    float urgency;
    void* tensor_data;
    uint32_t tensor_size;
    uint64_t timestamp_us;
} emotion_tensor_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_arousal_boost;
    float min_arousal_threshold;
    float contagion_boost;
} emotion_tensor_thalamic_config_t;

typedef struct emotion_tensor_thalamic_bridge emotion_tensor_thalamic_bridge_t;

emotion_tensor_thalamic_config_t emotion_tensor_thalamic_default_config(void);
emotion_tensor_thalamic_bridge_t* emotion_tensor_thalamic_bridge_create(void* emotion_tensor, thalamic_router_t* router, const emotion_tensor_thalamic_config_t* config);
void emotion_tensor_thalamic_bridge_destroy(emotion_tensor_thalamic_bridge_t* bridge);
int emotion_tensor_thalamic_bridge_reset(emotion_tensor_thalamic_bridge_t* bridge);
int emotion_tensor_thalamic_route_update(emotion_tensor_thalamic_bridge_t* bridge, const emotion_tensor_thalamic_signal_t* signal);
int emotion_tensor_thalamic_route_blend(emotion_tensor_thalamic_bridge_t* bridge, const void* tensor, float blend_weight);
int emotion_tensor_thalamic_set_attention(emotion_tensor_thalamic_bridge_t* bridge, float attention);
int emotion_tensor_thalamic_get_attention(emotion_tensor_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t updates_routed;
    uint64_t blends_processed;
    uint64_t contagions_triggered;
    float avg_arousal_level;
} emotion_tensor_thalamic_stats_t;

int emotion_tensor_thalamic_bridge_get_stats(emotion_tensor_thalamic_bridge_t* bridge, emotion_tensor_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_TENSOR_THALAMIC_BRIDGE_H */
