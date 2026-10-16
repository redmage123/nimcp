/**
 * @file nimcp_emotional_contagion_fep_bridge.h
 * @brief FEP Bridge for Emotional Contagion
 *
 * WHAT: Free Energy Principle integration for swarm emotional propagation
 * WHY:  Emotional contagion as affective inference and precision modulation
 * HOW:  Emotion intensity as precision, propagation as belief propagation
 *
 * BIOLOGICAL BASIS:
 * - Emotion as interoceptive prediction error
 * - Emotional intensity as precision/confidence
 * - Contagion as social inference (mirroring)
 * - Susceptibility as inverse precision of own emotional beliefs
 * - Resistance as strong emotional priors
 * - Collective emotion as shared affective model
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#ifndef NIMCP_EMOTIONAL_CONTAGION_FEP_BRIDGE_H
#define NIMCP_EMOTIONAL_CONTAGION_FEP_BRIDGE_H

#include "swarm/nimcp_emotional_contagion.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float intensity_precision_weight; /**< Emotion intensity as precision */
    float contagion_inference_gain;  /**< Contagion as inference strength */
    float resistance_prior_strength; /**< Resistance as prior strength */
    bool enable_affective_inference; /**< Enable affective FEP processing */
} emotional_contagion_fep_config_t;

typedef struct {
    float intensity_modulation;      /**< Emotion intensity adjustment */
    float contagion_rate_adjustment; /**< Contagion rate modulation */
    float susceptibility_adjustment; /**< Susceptibility modulation */
} emotional_contagion_fep_effects_t;

typedef struct {
    float precision_from_emotion;    /**< Precision from emotion intensity */
    float learning_modulation;       /**< Learning rate from emotion */
    float action_bias_from_emotion;  /**< Action selection bias */
} fep_emotional_contagion_effects_t;

typedef struct {
    emotion_type_t last_dominant_emotion;
    float last_intensity;
    uint32_t propagation_events;
    uint64_t last_update_time;
} emotional_contagion_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t emotional_transitions;
    float avg_collective_intensity;
    float avg_emotional_fe;
} emotional_contagion_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    emotional_contagion_fep_config_t config;
    fep_system_t* fep_system;
    emotional_contagion_t* contagion_system;
    emotional_contagion_fep_effects_t fep_effects;
    fep_emotional_contagion_effects_t contagion_effects;
    emotional_contagion_fep_state_t state;
    emotional_contagion_fep_stats_t stats;} emotional_contagion_fep_bridge_t;

void emotional_contagion_fep_default_config(emotional_contagion_fep_config_t* config);
emotional_contagion_fep_bridge_t* emotional_contagion_fep_create(const emotional_contagion_fep_config_t* config, emotional_contagion_t* contagion_system, fep_system_t* fep_system);
void emotional_contagion_fep_destroy(emotional_contagion_fep_bridge_t* bridge);
int emotional_contagion_fep_update(emotional_contagion_fep_bridge_t* bridge);
int emotional_contagion_fep_apply_modulation(emotional_contagion_fep_bridge_t* bridge);
int emotional_contagion_fep_get_effects(const emotional_contagion_fep_bridge_t* bridge, emotional_contagion_fep_effects_t* effects);
int emotional_contagion_fep_get_contagion_effects(const emotional_contagion_fep_bridge_t* bridge, fep_emotional_contagion_effects_t* effects);
int emotional_contagion_fep_get_stats(const emotional_contagion_fep_bridge_t* bridge, emotional_contagion_fep_stats_t* stats);
int emotional_contagion_fep_connect_bio_async(emotional_contagion_fep_bridge_t* bridge);
int emotional_contagion_fep_disconnect_bio_async(emotional_contagion_fep_bridge_t* bridge);
bool emotional_contagion_fep_is_bio_async_connected(const emotional_contagion_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTIONAL_CONTAGION_FEP_BRIDGE_H */
