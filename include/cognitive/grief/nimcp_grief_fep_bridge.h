/**
 * @file nimcp_grief_fep_bridge.h
 * @brief Free Energy Principle - Grief Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and grief/loss system
 * WHY:  Grief is processing prediction errors from permanent loss. The world model
 *       must update to reflect the absence of an attachment figure.
 * HOW:  FEP drives grief intensity via persistent prediction errors; grief modulates
 *       learning rates and model updating.
 *
 * BIOLOGICAL BASIS:
 * - Grief = massive, persistent prediction error (person expected but absent)
 * - Prolonged grief = failure to update generative model
 * - Healthy grief = gradual model updating under high precision
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GRIEF_FEP_BRIDGE_H
#define NIMCP_GRIEF_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_grief_and_loss.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float pe_grief_intensity_gain;
    float persistent_pe_duration_threshold;
    bool enable_pe_grief_generation;
    bool enable_persistent_pe_detection;
    float grief_learning_rate_reduction;
    float emotional_pain_precision_gain;
    bool enable_grief_learning_slowdown;
    bool enable_pain_precision;
    float fe_sensitivity;
    float emotion_sensitivity;
} grief_fep_config_t;

typedef struct {
    float grief_intensity_from_pe;
    float emotional_pain_level;
    bool persistent_pe_detected;
} grief_fep_effects_t;

typedef struct {
    float learning_rate_modifier;
    float model_update_resistance;
    float pain_precision_weight;
} fep_grief_effects_t;

typedef struct {
    float current_prediction_error;
    float grief_intensity;
    float emotional_pain;
    bool grieving;
    uint64_t grief_onset_time;
} grief_fep_state_t;

typedef struct {
    uint64_t grief_episodes;
    uint64_t persistent_pe_events;
    float avg_grief_intensity;
    float avg_emotional_pain;
} grief_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    grief_fep_config_t config;
    fep_system_t* fep_system;
    grief_system_t* grief_system;
    grief_fep_effects_t fep_effects;
    fep_grief_effects_t emotion_effects;
    grief_fep_state_t state;
    grief_fep_stats_t stats;} grief_fep_bridge_t;

int grief_fep_default_config(grief_fep_config_t* config);
grief_fep_bridge_t* grief_fep_create(const grief_fep_config_t* config);
void grief_fep_destroy(grief_fep_bridge_t* bridge);

int grief_fep_connect_fep(grief_fep_bridge_t* bridge, fep_system_t* fep);
int grief_fep_connect_grief(grief_fep_bridge_t* bridge, grief_system_t* grief);
int grief_fep_disconnect(grief_fep_bridge_t* bridge);

int grief_fep_process_persistent_pe(grief_fep_bridge_t* bridge, float pe_magnitude, uint64_t duration_ms);
int grief_fep_modulate_learning_rate(grief_fep_bridge_t* bridge);
int grief_fep_update(grief_fep_bridge_t* bridge, uint64_t delta_ms);

int grief_fep_get_state(grief_fep_bridge_t* bridge, grief_fep_state_t* state);
int grief_fep_get_stats(grief_fep_bridge_t* bridge, grief_fep_stats_t* stats);

int grief_fep_connect_bio_async(grief_fep_bridge_t* bridge);
int grief_fep_disconnect_bio_async(grief_fep_bridge_t* bridge);
bool grief_fep_is_bio_async_connected(const grief_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRIEF_FEP_BRIDGE_H */
