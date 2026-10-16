/**
 * @file nimcp_emotion_recognition_thalamic_bridge.h
 * @brief Bridge between Emotion Recognition and thalamic router
 *
 * WHAT: Routes emotion recognition signals through attention-gated thalamic pathways
 * WHY: Recognized emotions require conscious access via thalamic gating
 * HOW: Packages emotion signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Emotion recognition involves FFA, STS, amygdala
 * - Thalamus gates emotional face processing
 * - Pulvinar coordinates attention to emotional expressions
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EMOTION_RECOGNITION_THALAMIC_BRIDGE_H
#define NIMCP_EMOTION_RECOGNITION_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMOTION_REC_SIGNAL_FACE       0x0B01
#define EMOTION_REC_SIGNAL_VOICE      0x0B02
#define EMOTION_REC_SIGNAL_BODY       0x0B03
#define EMOTION_REC_SIGNAL_CONTEXT    0x0B04

typedef struct {
    uint32_t signal_type;
    float recognition_confidence;
    float emotional_intensity;
    uint32_t emotion_category;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} emotion_recognition_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_threat_priority;
    float min_recognition_confidence;
    float threat_boost;
} emotion_recognition_thalamic_config_t;

typedef struct emotion_recognition_thalamic_bridge emotion_recognition_thalamic_bridge_t;

emotion_recognition_thalamic_config_t emotion_recognition_thalamic_default_config(void);
emotion_recognition_thalamic_bridge_t* emotion_recognition_thalamic_bridge_create(void* emotion_rec, thalamic_router_t* router, const emotion_recognition_thalamic_config_t* config);
void emotion_recognition_thalamic_bridge_destroy(emotion_recognition_thalamic_bridge_t* bridge);
int emotion_recognition_thalamic_bridge_reset(emotion_recognition_thalamic_bridge_t* bridge);
int emotion_recognition_thalamic_route_recognition(emotion_recognition_thalamic_bridge_t* bridge, const emotion_recognition_thalamic_signal_t* signal);
int emotion_recognition_thalamic_route_context(emotion_recognition_thalamic_bridge_t* bridge, const void* context, float relevance);
int emotion_recognition_thalamic_set_attention(emotion_recognition_thalamic_bridge_t* bridge, float attention);
int emotion_recognition_thalamic_get_attention(const emotion_recognition_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t recognitions_routed;
    uint64_t threat_detections;
    uint64_t context_integrations;
    float avg_recognition_confidence;
} emotion_recognition_thalamic_stats_t;

int emotion_recognition_thalamic_bridge_get_stats(const emotion_recognition_thalamic_bridge_t* bridge, emotion_recognition_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_RECOGNITION_THALAMIC_BRIDGE_H */
