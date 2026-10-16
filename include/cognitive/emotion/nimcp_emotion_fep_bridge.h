/**
 * @file nimcp_emotion_fep_bridge.h
 * @brief Free Energy Principle - Emotion Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and emotion system
 * WHY:  Emotions are valenced prediction errors. Affective states reflect
 *       precision-weighted surprise. Interoceptive inference = emotional awareness.
 * HOW:  FEP prediction errors generate emotional responses; emotions modulate
 *       FEP precision and learning rates.
 *
 * BIOLOGICAL BASIS:
 * - Barrett & Simmons (2015): Interoceptive predictions and emotion
 * - Seth (2013): Interoceptive inference and emotion
 * - Positive PE → Positive emotions; Negative PE → Negative emotions
 * - Precision = emotional intensity
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EMOTION_FEP_BRIDGE_H
#define NIMCP_EMOTION_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_emotion_recognition.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EMOTION_FEP_VALENCE_THRESHOLD     0.0f
#define EMOTION_FEP_AROUSAL_PE_SCALING    1.0f

typedef struct emotion_fep_bridge emotion_fep_bridge_t;

typedef struct {
    float pe_valence_scaling;
    float pe_arousal_scaling;
    float precision_intensity_scaling;
    bool enable_pe_emotion_generation;
    bool enable_precision_intensity;
    bool enable_interoceptive_inference;
    float emotion_precision_modulation;
    float emotion_learning_modulation;
    bool enable_emotion_precision;
    bool enable_emotion_learning;
    float fe_sensitivity;
    float emotion_sensitivity;
} emotion_fep_config_t;

typedef struct {
    float prediction_error_valence;
    float prediction_error_arousal;
    float precision_intensity;
    bool emotion_generated;
} emotion_fep_effects_t;

typedef struct {
    float emotion_precision_modifier;
    float emotion_learning_modifier;
    float emotional_arousal;
    float emotional_valence;
} fep_emotion_effects_t;

typedef struct {
    float current_prediction_error;
    float current_precision;
    float current_valence;
    float current_arousal;
    bool emotion_active;
    uint64_t last_emotion_time;
} emotion_fep_state_t;

typedef struct {
    uint64_t emotion_generation_events;
    uint64_t precision_modulation_events;
    float avg_valence;
    float avg_arousal;
    float avg_prediction_error;
    float avg_free_energy;
} emotion_fep_stats_t;

struct emotion_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    emotion_fep_config_t config;
    fep_system_t* fep_system;
    emotion_recognition_system_t* emotion_system;
    emotion_fep_effects_t fep_effects;
    fep_emotion_effects_t emotion_effects;
    emotion_fep_state_t state;
    emotion_fep_stats_t stats;
};

int emotion_fep_bridge_default_config(emotion_fep_config_t* config);
emotion_fep_bridge_t* emotion_fep_bridge_create(const emotion_fep_config_t* config);
void emotion_fep_bridge_destroy(emotion_fep_bridge_t* bridge);

int emotion_fep_bridge_connect_fep(emotion_fep_bridge_t* bridge, fep_system_t* fep);
int emotion_fep_bridge_connect_emotion(emotion_fep_bridge_t* bridge, emotion_recognition_system_t* emotion);
int emotion_fep_bridge_disconnect(emotion_fep_bridge_t* bridge);

int emotion_fep_generate_valenced_pe(emotion_fep_bridge_t* bridge, float pe_magnitude);
int emotion_fep_modulate_precision_by_intensity(emotion_fep_bridge_t* bridge);

int emotion_fep_apply_emotion_precision_modulation(emotion_fep_bridge_t* bridge);
int emotion_fep_apply_emotion_learning_modulation(emotion_fep_bridge_t* bridge);

int emotion_fep_bridge_update(emotion_fep_bridge_t* bridge, uint64_t delta_ms);

int emotion_fep_bridge_get_state(emotion_fep_bridge_t* bridge, emotion_fep_state_t* state);
int emotion_fep_bridge_get_stats(emotion_fep_bridge_t* bridge, emotion_fep_stats_t* stats);

int emotion_fep_bridge_connect_bio_async(emotion_fep_bridge_t* bridge);
int emotion_fep_bridge_disconnect_bio_async(emotion_fep_bridge_t* bridge);
bool emotion_fep_bridge_is_bio_async_connected(const emotion_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_FEP_BRIDGE_H */
