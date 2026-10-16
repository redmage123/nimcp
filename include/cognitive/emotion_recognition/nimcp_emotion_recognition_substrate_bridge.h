/**
 * @file nimcp_emotion_recognition_substrate_bridge.h
 * @brief Bridge between Emotion Recognition system and neural substrate
 *
 * WHAT: Links emotion recognition to metabolic state
 * WHY: Emotion recognition requires fusiform face area and amygdala resources
 * HOW: Monitors ATP/fatigue; modulates recognition accuracy, speed, sensitivity
 *
 * BIOLOGICAL BASIS:
 * - Emotion recognition involves FFA, STS, and amygdala
 * - ATP depletion reduces recognition accuracy
 * - Fatigue impairs subtle emotion detection
 * - Metabolic stress may bias toward threat detection
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EMOTION_RECOGNITION_SUBSTRATE_BRIDGE_H
#define NIMCP_EMOTION_RECOGNITION_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_EMOTION_RECOGNITION 0x122B

typedef struct {
    float recognition_accuracy;   /* Accuracy of emotion recognition [0-1] */
    float detection_speed;        /* Speed of emotion detection [0-1] */
    float subtle_sensitivity;     /* Sensitivity to subtle emotions [0-1] */
    float context_integration;    /* Integration of context [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} emotion_recognition_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} emotion_recognition_substrate_config_t;

typedef struct emotion_recognition_substrate_bridge emotion_recognition_substrate_bridge_t;

emotion_recognition_substrate_config_t emotion_recognition_substrate_default_config(void);
emotion_recognition_substrate_bridge_t* emotion_recognition_substrate_bridge_create(void* emotion_recognition, neural_substrate_t* substrate, const emotion_recognition_substrate_config_t* config);
void emotion_recognition_substrate_bridge_destroy(emotion_recognition_substrate_bridge_t* bridge);
int emotion_recognition_substrate_bridge_update(emotion_recognition_substrate_bridge_t* bridge);
int emotion_recognition_substrate_bridge_get_effects(const emotion_recognition_substrate_bridge_t* bridge, emotion_recognition_substrate_effects_t* effects);
int emotion_recognition_substrate_bridge_apply_effects(emotion_recognition_substrate_bridge_t* bridge);
int emotion_recognition_substrate_bridge_register_bio_async(emotion_recognition_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_RECOGNITION_SUBSTRATE_BRIDGE_H */
