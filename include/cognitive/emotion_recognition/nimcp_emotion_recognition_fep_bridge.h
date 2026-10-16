/**
 * @file nimcp_emotion_recognition_fep_bridge.h
 * @brief Free Energy Principle - Emotion Recognition Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and multimodal emotion recognition
 * WHY:  Emotion recognition is inferring hidden emotional causes from observations.
 *       Prediction errors drive emotional state inference.
 * HOW:  FEP provides precision-weighted emotional inference; emotions modulate
 *       sensory precision and active sensing.
 *
 * BIOLOGICAL BASIS:
 * - Inferring others' emotions = hidden state inference under FEP
 * - Multimodal cues (facial, vocal, text) = observations
 * - Emotional state = hidden cause being inferred
 * - Precision weighting = confidence in each modality
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EMOTION_RECOGNITION_FEP_BRIDGE_H
#define NIMCP_EMOTION_RECOGNITION_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_emotion_recognition.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for health agent (Phase 8) */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

typedef struct {
    /* FEP → Emotion Recognition */
    float pe_inference_gain;
    float precision_confidence_gain;
    bool enable_pe_emotion_inference;
    bool enable_precision_confidence;

    /* Emotion Recognition → FEP */
    float emotion_precision_modulation;
    float distress_uncertainty_gain;
    bool enable_emotion_precision;
    bool enable_distress_uncertainty;

    float fe_sensitivity;
    float emotion_sensitivity;
} emotion_recognition_fep_config_t;

typedef struct {
    float inferred_valence;
    float inferred_arousal;
    float inference_confidence;
    emotion_category_t inferred_emotion;
    bool inference_active;
} emotion_recognition_fep_effects_t;

typedef struct {
    float modality_precision_weights[4];  /**< For facial, vocal, text, physiological */
    float uncertainty_estimate;
    float active_sensing_drive;
} fep_emotion_recognition_effects_t;

typedef struct {
    float current_prediction_error;
    float current_precision;
    emotion_category_t current_inferred_emotion;
    float inference_confidence;
    bool recognition_active;
    uint64_t last_recognition_time;
} emotion_recognition_fep_state_t;

typedef struct {
    uint64_t emotion_inference_events;
    uint64_t precision_modulation_events;
    float avg_inference_confidence;
    float avg_prediction_error;
} emotion_recognition_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    emotion_recognition_fep_config_t config;
    fep_system_t* fep_system;
    emotion_recognition_system_t* emotion_system;
    emotion_recognition_fep_effects_t fep_effects;
    fep_emotion_recognition_effects_t emotion_effects;
    emotion_recognition_fep_state_t state;
    emotion_recognition_fep_stats_t stats;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent (Phase 8) */
} emotion_recognition_fep_bridge_t;

int emotion_recognition_fep_default_config(emotion_recognition_fep_config_t* config);
emotion_recognition_fep_bridge_t* emotion_recognition_fep_create(const emotion_recognition_fep_config_t* config);
void emotion_recognition_fep_destroy(emotion_recognition_fep_bridge_t* bridge);

int emotion_recognition_fep_connect_fep(emotion_recognition_fep_bridge_t* bridge, fep_system_t* fep);
int emotion_recognition_fep_connect_emotion(emotion_recognition_fep_bridge_t* bridge, emotion_recognition_system_t* emotion);
int emotion_recognition_fep_disconnect(emotion_recognition_fep_bridge_t* bridge);

int emotion_recognition_fep_infer_emotion(emotion_recognition_fep_bridge_t* bridge, float pe_magnitude);
int emotion_recognition_fep_modulate_modality_precision(emotion_recognition_fep_bridge_t* bridge);
int emotion_recognition_fep_update(emotion_recognition_fep_bridge_t* bridge, uint64_t delta_ms);

int emotion_recognition_fep_get_state(const emotion_recognition_fep_bridge_t* bridge, emotion_recognition_fep_state_t* state);
int emotion_recognition_fep_get_stats(const emotion_recognition_fep_bridge_t* bridge, emotion_recognition_fep_stats_t* stats);

int emotion_recognition_fep_connect_bio_async(emotion_recognition_fep_bridge_t* bridge);
int emotion_recognition_fep_disconnect_bio_async(emotion_recognition_fep_bridge_t* bridge);
bool emotion_recognition_fep_is_bio_async_connected(const emotion_recognition_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_RECOGNITION_FEP_BRIDGE_H */
