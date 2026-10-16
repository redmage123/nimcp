/**
 * @file nimcp_remorse_fep_bridge.h
 * @brief Free Energy Principle - Remorse/Regret Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and remorse/regret system
 * WHY:  Remorse arises from counterfactual inference - comparing actual outcomes
 *       to predicted better alternatives. Regret = learning from prediction errors.
 * HOW:  FEP counterfactual reasoning generates remorse; remorse modulates
 *       policy learning to avoid future violations.
 *
 * BIOLOGICAL BASIS:
 * - Counterfactual thinking = alternative generative model simulation
 * - Remorse = valence of counterfactual PE (actual - could have been)
 * - Moral learning via precision-weighted policy updates
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_REMORSE_FEP_BRIDGE_H
#define NIMCP_REMORSE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_remorse_regret.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float counterfactual_pe_gain;
    float moral_violation_precision_boost;
    bool enable_counterfactual_pe;
    bool enable_moral_precision;
    float remorse_policy_learning_gain;
    float guilt_value_update_gain;
    bool enable_remorse_policy_learning;
    bool enable_guilt_value_update;
    float fe_sensitivity;
    float emotion_sensitivity;
} remorse_fep_config_t;

typedef struct {
    float counterfactual_pe;
    float remorse_intensity;
    float moral_violation_precision;
} remorse_fep_effects_t;

typedef struct {
    float policy_learning_modifier;
    float value_aversion_strength;
    float avoidance_precision;
} fep_remorse_effects_t;

typedef struct {
    float current_counterfactual_pe;
    float remorse_level;
    float guilt_level;
    bool remorseful;
} remorse_fep_state_t;

typedef struct {
    uint64_t remorse_events;
    uint64_t counterfactual_simulations;
    float avg_remorse_intensity;
    float avg_counterfactual_pe;
} remorse_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    remorse_fep_config_t config;
    fep_system_t* fep_system;
    remorse_regret_system_t* remorse_system;
    remorse_fep_effects_t fep_effects;
    fep_remorse_effects_t emotion_effects;
    remorse_fep_state_t state;
    remorse_fep_stats_t stats;} remorse_fep_bridge_t;

int remorse_fep_default_config(remorse_fep_config_t* config);
remorse_fep_bridge_t* remorse_fep_create(const remorse_fep_config_t* config);
void remorse_fep_destroy(remorse_fep_bridge_t* bridge);

int remorse_fep_connect_fep(remorse_fep_bridge_t* bridge, fep_system_t* fep);
int remorse_fep_connect_remorse(remorse_fep_bridge_t* bridge, remorse_regret_system_t* remorse);
int remorse_fep_disconnect(remorse_fep_bridge_t* bridge);

int remorse_fep_compute_counterfactual_pe(remorse_fep_bridge_t* bridge, float actual_value, float alternative_value);
int remorse_fep_modulate_policy_learning(remorse_fep_bridge_t* bridge);
int remorse_fep_update(remorse_fep_bridge_t* bridge, uint64_t delta_ms);

int remorse_fep_get_state(remorse_fep_bridge_t* bridge, remorse_fep_state_t* state);
int remorse_fep_get_stats(remorse_fep_bridge_t* bridge, remorse_fep_stats_t* stats);

int remorse_fep_connect_bio_async(remorse_fep_bridge_t* bridge);
int remorse_fep_disconnect_bio_async(remorse_fep_bridge_t* bridge);
bool remorse_fep_is_bio_async_connected(const remorse_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REMORSE_FEP_BRIDGE_H */
