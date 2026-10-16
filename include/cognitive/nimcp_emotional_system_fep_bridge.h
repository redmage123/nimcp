/**
 * @file nimcp_emotional_system_fep_bridge.h
 * @brief Free Energy Principle - Emotional System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and unified emotional system
 * WHY:  Emotions are interoceptive inferences about bodily states. The emotional system
 *       integrates all emotional subsystems under active inference framework.
 * HOW:  FEP drives emotional state updates; emotions modulate precision and learning.
 *
 * BIOLOGICAL BASIS:
 * - Barrett's theory of constructed emotion: Emotions are predictions about body states
 * - Interoceptive inference: Brain predicts and explains bodily sensations
 * - Emotional system integrates tagging, recognition, regulation under FEP
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EMOTIONAL_SYSTEM_FEP_BRIDGE_H
#define NIMCP_EMOTIONAL_SYSTEM_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_emotional_system.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float pe_emotional_state_gain;
    float precision_regulation_threshold;
    bool enable_pe_emotional_update;
    bool enable_precision_regulation;
    float emotional_state_precision_gain;
    float valence_value_modulation;
    bool enable_emotional_precision;
    bool enable_valence_value;
    float fe_sensitivity;
    float emotion_sensitivity;
} emotional_system_fep_config_t;

typedef struct {
    float valence_from_pe;
    float arousal_from_precision;
    bool regulation_triggered;
} emotional_system_fep_effects_t;

typedef struct {
    float precision_weight;
    float value_estimate;
    float learning_rate_modifier;
} fep_emotional_system_effects_t;

typedef struct {
    float current_valence;
    float current_arousal;
    float current_precision;
    bool emotion_active;
} emotional_system_fep_state_t;

typedef struct {
    uint64_t emotional_update_events;
    uint64_t regulation_events;
    float avg_valence;
    float avg_arousal;
} emotional_system_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    emotional_system_fep_config_t config;
    fep_system_t* fep_system;
    emotional_system_t* emotional_system;
    emotional_system_fep_effects_t fep_effects;
    fep_emotional_system_effects_t emotion_effects;
    emotional_system_fep_state_t state;
    emotional_system_fep_stats_t stats;} emotional_system_fep_bridge_t;

int emotional_system_fep_default_config(emotional_system_fep_config_t* config);
emotional_system_fep_bridge_t* emotional_system_fep_create(const emotional_system_fep_config_t* config);
void emotional_system_fep_destroy(emotional_system_fep_bridge_t* bridge);

int emotional_system_fep_connect_fep(emotional_system_fep_bridge_t* bridge, fep_system_t* fep);
int emotional_system_fep_connect_emotional_system(emotional_system_fep_bridge_t* bridge, emotional_system_t* system);
int emotional_system_fep_disconnect(emotional_system_fep_bridge_t* bridge);

int emotional_system_fep_update_from_pe(emotional_system_fep_bridge_t* bridge, float pe_magnitude);
int emotional_system_fep_modulate_precision(emotional_system_fep_bridge_t* bridge);
int emotional_system_fep_update(emotional_system_fep_bridge_t* bridge, uint64_t delta_ms);

int emotional_system_fep_get_state(const emotional_system_fep_bridge_t* bridge, emotional_system_fep_state_t* state);
int emotional_system_fep_get_stats(const emotional_system_fep_bridge_t* bridge, emotional_system_fep_stats_t* stats);

int emotional_system_fep_connect_bio_async(emotional_system_fep_bridge_t* bridge);
int emotional_system_fep_disconnect_bio_async(emotional_system_fep_bridge_t* bridge);
bool emotional_system_fep_is_bio_async_connected(const emotional_system_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTIONAL_SYSTEM_FEP_BRIDGE_H */
