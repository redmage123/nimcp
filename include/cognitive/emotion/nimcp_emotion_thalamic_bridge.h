/**
 * @file nimcp_emotion_thalamic_bridge.h
 * @brief Bridge between Emotion system and thalamic router
 *
 * WHAT: Routes emotional signals through thalamic attention pathways
 * WHY: Emotions require attention-gated routing for conscious experience
 * HOW: Packages emotion signals, routes via amygdala-thalamic circuits
 *
 * BIOLOGICAL BASIS:
 * - Amygdala-thalamic pathway for emotional arousal
 * - Mediodorsal nucleus integrates emotion with cognition
 * - Anterior thalamus links emotion to memory
 * - Pulvinar routes emotional salience
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EMOTION_THALAMIC_BRIDGE_H
#define NIMCP_EMOTION_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMOTION_SIGNAL_AROUSAL       0x2C01  /**< Emotional arousal */
#define EMOTION_SIGNAL_VALENCE       0x2C02  /**< Valence update */
#define EMOTION_SIGNAL_REGULATION    0x2C03  /**< Regulation attempt */
#define EMOTION_SIGNAL_EXPRESSION    0x2C04  /**< Emotion expression */

typedef struct {
    uint32_t signal_type;
    float emotional_intensity;
    float valence;
    float arousal_level;
    float regulation_effort;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} emotion_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_intensity_boost;
    bool enable_regulation_priority;
    float min_intensity_threshold;
    float regulation_boost;
} emotion_thalamic_config_t;

typedef struct {
    uint64_t arousal_signals;
    uint64_t valence_updates;
    uint64_t regulations_attempted;
    uint64_t expressions_routed;
    uint64_t signals_gated;
    float avg_intensity;
    float avg_arousal;
} emotion_thalamic_stats_t;

typedef struct emotion_thalamic_bridge emotion_thalamic_bridge_t;

emotion_thalamic_config_t emotion_thalamic_default_config(void);
emotion_thalamic_bridge_t* emotion_thalamic_bridge_create(
    void* emotion, thalamic_router_t* router,
    const emotion_thalamic_config_t* config);
void emotion_thalamic_bridge_destroy(emotion_thalamic_bridge_t* bridge);
int emotion_thalamic_bridge_reset(emotion_thalamic_bridge_t* bridge);

int emotion_thalamic_route_signal(
    emotion_thalamic_bridge_t* bridge,
    const emotion_thalamic_signal_t* signal);
int emotion_thalamic_route_arousal(
    emotion_thalamic_bridge_t* bridge,
    float intensity, float arousal);
int emotion_thalamic_route_regulation(
    emotion_thalamic_bridge_t* bridge,
    float effort, float urgency);

int emotion_thalamic_set_attention(emotion_thalamic_bridge_t* bridge, float attention);
int emotion_thalamic_get_attention(emotion_thalamic_bridge_t* bridge, float* attention);
int emotion_thalamic_bridge_get_stats(
    const emotion_thalamic_bridge_t* bridge,
    emotion_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_THALAMIC_BRIDGE_H */
