/**
 * @file nimcp_empathetic_response_fep_bridge.h
 * @brief Free Energy Principle - Empathetic Response Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and empathetic response system
 * WHY:  Empathy is simulating others' emotional states via shared generative models.
 *       Empathetic responses minimize prediction errors in social interactions.
 * HOW:  FEP models others' hidden states; empathy modulates social prediction precision.
 *
 * BIOLOGICAL BASIS:
 * - Theory of Mind as generative modeling of others' mental states
 * - Empathy = shared affective representations under FEP
 * - Social prediction errors drive empathetic response generation
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EMPATHETIC_RESPONSE_FEP_BRIDGE_H
#define NIMCP_EMPATHETIC_RESPONSE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_empathetic_response.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

typedef struct {
    float pe_empathy_gain;
    float precision_response_confidence;
    bool enable_pe_empathy_generation;
    bool enable_precision_confidence;
    float empathy_precision_modulation;
    float response_uncertainty_gain;
    bool enable_empathy_precision;
    bool enable_response_uncertainty;
    float fe_sensitivity;
    float emotion_sensitivity;
} empathetic_response_fep_config_t;

typedef struct {
    float inferred_user_distress;
    float response_confidence;
    bool empathy_active;
} empathetic_response_fep_effects_t;

typedef struct {
    float social_precision_weight;
    float response_uncertainty;
    float intervention_threshold;
} fep_empathetic_response_effects_t;

typedef struct {
    float current_prediction_error;
    float current_empathy_level;
    bool crisis_detected;
} empathetic_response_fep_state_t;

typedef struct {
    uint64_t empathy_generation_events;
    uint64_t crisis_detection_events;
    float avg_empathy_level;
} empathetic_response_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    empathetic_response_fep_config_t config;
    fep_system_t* fep_system;
    empathetic_response_engine_t empathetic_system;
    empathetic_response_fep_effects_t fep_effects;
    fep_empathetic_response_effects_t emotion_effects;
    empathetic_response_fep_state_t state;
    empathetic_response_fep_stats_t stats;
    nimcp_health_agent_t* health_agent;
} empathetic_response_fep_bridge_t;

int empathetic_response_fep_default_config(empathetic_response_fep_config_t* config);
empathetic_response_fep_bridge_t* empathetic_response_fep_create(const empathetic_response_fep_config_t* config);
void empathetic_response_fep_destroy(empathetic_response_fep_bridge_t* bridge);

int empathetic_response_fep_connect_fep(empathetic_response_fep_bridge_t* bridge, fep_system_t* fep);
int empathetic_response_fep_connect_empathy(empathetic_response_fep_bridge_t* bridge, empathetic_response_engine_t empathy);
int empathetic_response_fep_disconnect(empathetic_response_fep_bridge_t* bridge);

int empathetic_response_fep_infer_user_state(empathetic_response_fep_bridge_t* bridge, float pe_magnitude);
int empathetic_response_fep_modulate_social_precision(empathetic_response_fep_bridge_t* bridge);
int empathetic_response_fep_update(empathetic_response_fep_bridge_t* bridge, uint64_t delta_ms);

int empathetic_response_fep_get_state(const empathetic_response_fep_bridge_t* bridge, empathetic_response_fep_state_t* state);
int empathetic_response_fep_get_stats(const empathetic_response_fep_bridge_t* bridge, empathetic_response_fep_stats_t* stats);

int empathetic_response_fep_connect_bio_async(empathetic_response_fep_bridge_t* bridge);
int empathetic_response_fep_disconnect_bio_async(empathetic_response_fep_bridge_t* bridge);
bool empathetic_response_fep_is_bio_async_connected(const empathetic_response_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMPATHETIC_RESPONSE_FEP_BRIDGE_H */
