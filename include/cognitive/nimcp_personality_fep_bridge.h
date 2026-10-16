/**
 * @file nimcp_personality_fep_bridge.h
 * @brief Free Energy Principle - Personality Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and personality system
 * WHY:  Personality traits influence how the brain minimizes free energy - openness affects
 *       exploration-exploitation balance, neuroticism affects precision expectations.
 * HOW:  FEP policies modulated by personality; personality expressed through FEP parameters.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PERSONALITY AS FEP PARAMETERIZATION:
 * -------------------------------------
 * - DeYoung (2015): Personality traits = individual differences in FEP parameters
 * - Openness = low prior precision (explore more)
 * - Neuroticism = high expected precision (intolerant of surprise)
 * - Extraversion = low precision on rewards (seek more stimulation)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PERSONALITY_FEP_BRIDGE_H
#define NIMCP_PERSONALITY_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_personality.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct personality_fep_bridge personality_fep_bridge_t;

typedef struct {
    bool enable_openness_exploration;
    bool enable_neuroticism_precision;
    bool enable_conscientiousness_planning;
    float personality_sensitivity;
    float fep_sensitivity;
} personality_fep_config_t;

typedef struct {
    float openness_exploration_boost;
    float neuroticism_precision_modifier;
    float conscientiousness_planning_depth;
    float extraversion_reward_seeking;
    float agreeableness_social_prior;
} personality_fep_effects_t;

typedef struct {
    float current_free_energy;
    float exploration_rate;
    float planning_horizon;
} fep_personality_effects_t;

typedef struct {
    float current_free_energy;
    float exploration_rate;
    float precision_modifier;
    personality_traits_t active_traits;
} personality_fep_state_t;

typedef struct {
    uint64_t modulation_events;
    float avg_free_energy;
    float avg_exploration_rate;
} personality_fep_stats_t;

struct personality_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    personality_fep_config_t config;
    fep_system_t* fep_system;
    personality_profile_t* personality;
    personality_fep_effects_t fep_effects;
    fep_personality_effects_t personality_effects;
    personality_fep_state_t state;
    personality_fep_stats_t stats;
};

int personality_fep_bridge_default_config(personality_fep_config_t* config);
personality_fep_bridge_t* personality_fep_bridge_create(const personality_fep_config_t* config);
void personality_fep_bridge_destroy(personality_fep_bridge_t* bridge);
int personality_fep_bridge_connect_fep(personality_fep_bridge_t* bridge, fep_system_t* fep);
int personality_fep_bridge_connect_personality(personality_fep_bridge_t* bridge,
                                                personality_profile_t* personality);
int personality_fep_bridge_disconnect(personality_fep_bridge_t* bridge);
int personality_fep_bridge_update(personality_fep_bridge_t* bridge);
int personality_fep_bridge_get_state(personality_fep_bridge_t* bridge,
                                      personality_fep_state_t* state);
int personality_fep_bridge_get_stats(personality_fep_bridge_t* bridge,
                                      personality_fep_stats_t* stats);
int personality_fep_bridge_connect_bio_async(personality_fep_bridge_t* bridge);
int personality_fep_bridge_disconnect_bio_async(personality_fep_bridge_t* bridge);
bool personality_fep_bridge_is_bio_async_connected(const personality_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PERSONALITY_FEP_BRIDGE_H */
