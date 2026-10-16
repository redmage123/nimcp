/**
 * @file nimcp_ethics_fep_bridge.h
 * @brief Free Energy Principle - Ethics Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and Ethics system
 * WHY:  Ethical reasoning = value-based preferences in expected free energy (EFE).
 *       Ethical priors constrain action selection; FEP optimizes actions under
 *       ethical constraints to minimize value-weighted surprise.
 * HOW:  Ethics → FEP preferred outcomes (p(o)); FEP → ethical policy selection
 *
 * BIOLOGICAL BASIS:
 * - Ventromedial Prefrontal Cortex (vmPFC) encodes value-based priors
 * - Ethical decisions minimize value-weighted expected free energy
 * - Deontological constraints = hard priors (p(a|ethical) = 0 for forbidden acts)
 * - Consequentialist optimization = EFE minimization with ethical outcomes
 * - Reference: Friston et al. (2015) "Active inference and epistemic value"
 */

#ifndef NIMCP_ETHICS_FEP_BRIDGE_H
#define NIMCP_ETHICS_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ETHICS_FEP_HARM_THRESHOLD            0.7f
#define ETHICS_FEP_VALUE_PRIOR_WEIGHT        0.9f
#define ETHICS_FEP_DEONTOLOGICAL_PENALTY     100.0f

typedef struct ethics_fep_bridge ethics_fep_bridge_t;

typedef struct {
    float harm_threshold;
    float value_prior_weight;
    float deontological_penalty;
    bool enable_value_priors;
    bool enable_deontological_constraints;
    bool enable_harm_prediction;
    float pe_sensitivity;
} ethics_fep_config_t;

typedef struct {
    float value_prior;
    float harm_prediction;
    bool ethical_constraint_active;
} ethics_fep_effects_t;

typedef struct {
    uint32_t ethical_policies_selected;
    uint32_t harmful_actions_blocked;
    float current_value_alignment;
} ethics_fep_state_t;

typedef struct {
    uint64_t ethical_selections_total;
    uint64_t harm_preventions_total;
    float avg_value_alignment;
} ethics_fep_stats_t;

struct ethics_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    ethics_fep_config_t config;
    fep_system_t* fep_system;
    void* ethics_system;  /* ethics_engine_t* */
    ethics_fep_effects_t effects;
    ethics_fep_state_t state;
    ethics_fep_stats_t stats;
};

int ethics_fep_bridge_default_config(ethics_fep_config_t* config);
ethics_fep_bridge_t* ethics_fep_bridge_create(const ethics_fep_config_t* config);
void ethics_fep_bridge_destroy(ethics_fep_bridge_t* bridge);

int ethics_fep_bridge_connect_fep(ethics_fep_bridge_t* bridge, fep_system_t* fep);
int ethics_fep_bridge_connect_ethics(ethics_fep_bridge_t* bridge, void* ethics);

int ethics_fep_apply_value_priors(ethics_fep_bridge_t* bridge);
int ethics_fep_constrain_policy(ethics_fep_bridge_t* bridge, bool is_ethical);
int ethics_fep_predict_harm(ethics_fep_bridge_t* bridge, float* harm_score);

int ethics_fep_bridge_update(ethics_fep_bridge_t* bridge, uint64_t delta_ms);
int ethics_fep_bridge_get_state(ethics_fep_bridge_t* bridge, ethics_fep_state_t* state);
int ethics_fep_bridge_get_stats(ethics_fep_bridge_t* bridge, ethics_fep_stats_t* stats);

int ethics_fep_bridge_connect_bio_async(ethics_fep_bridge_t* bridge);
int ethics_fep_bridge_disconnect_bio_async(ethics_fep_bridge_t* bridge);
bool ethics_fep_bridge_is_bio_async_connected(const ethics_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ETHICS_FEP_BRIDGE_H */
