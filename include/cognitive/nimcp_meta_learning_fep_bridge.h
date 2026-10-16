/**
 * @file nimcp_meta_learning_fep_bridge.h
 * @brief Free Energy Principle - Meta-Learning Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and meta-learning (MAML)
 * WHY:  Meta-learning = learning to minimize free energy efficiently across tasks.
 *       FEP provides theoretical grounding for few-shot adaptation.
 * HOW:  FEP task similarity guides meta-learning transfer; meta-learning optimizes
 *       FEP generative model initialization for fast adaptation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * META-LEARNING AS HIERARCHICAL FEP:
 * -----------------------------------
 * - Friston et al. (2016): Meta-learning = learning optimal priors for fast inference
 * - MAML inner loop = task-specific free energy minimization
 * - MAML outer loop = meta-optimization of initial generative model
 * - Few-shot learning = efficient belief updating with minimal observations
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_META_LEARNING_FEP_BRIDGE_H
#define NIMCP_META_LEARNING_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_meta_learning.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct meta_learning_fep_bridge meta_learning_fep_bridge_t;

typedef struct {
    bool enable_task_similarity_fe;
    bool enable_adaptation_speed_fe;
    bool enable_meta_prior_optimization;
    float meta_sensitivity;
    float fep_sensitivity;
} meta_learning_fep_config_t;

typedef struct {
    float task_similarity_fe;
    float adaptation_fe_reduction;
    uint32_t adaptation_steps_needed;
} meta_learning_fep_effects_t;

typedef struct {
    float meta_prior_precision;
    float adaptation_lr;
    bool transfer_active;
} fep_meta_learning_effects_t;

typedef struct {
    float current_free_energy;
    float adaptation_speed;
    uint32_t tasks_seen;
} meta_learning_fep_state_t;

typedef struct {
    uint64_t adaptation_events;
    uint64_t transfer_events;
    float avg_free_energy;
    float avg_adaptation_speed;
} meta_learning_fep_stats_t;

struct meta_learning_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    meta_learning_fep_config_t config;
    fep_system_t* fep_system;
    meta_learner_t meta_learner;
    meta_learning_fep_effects_t fep_effects;
    fep_meta_learning_effects_t meta_effects;
    meta_learning_fep_state_t state;
    meta_learning_fep_stats_t stats;
};

int meta_learning_fep_bridge_default_config(meta_learning_fep_config_t* config);
meta_learning_fep_bridge_t* meta_learning_fep_bridge_create(const meta_learning_fep_config_t* config);
void meta_learning_fep_bridge_destroy(meta_learning_fep_bridge_t* bridge);
int meta_learning_fep_bridge_connect_fep(meta_learning_fep_bridge_t* bridge, fep_system_t* fep);
int meta_learning_fep_bridge_connect_meta_learning(meta_learning_fep_bridge_t* bridge,
                                                    meta_learner_t meta);
int meta_learning_fep_bridge_disconnect(meta_learning_fep_bridge_t* bridge);
int meta_learning_fep_bridge_update(meta_learning_fep_bridge_t* bridge);
int meta_learning_fep_bridge_get_state(meta_learning_fep_bridge_t* bridge,
                                        meta_learning_fep_state_t* state);
int meta_learning_fep_bridge_get_stats(meta_learning_fep_bridge_t* bridge,
                                        meta_learning_fep_stats_t* stats);
int meta_learning_fep_bridge_connect_bio_async(meta_learning_fep_bridge_t* bridge);
int meta_learning_fep_bridge_disconnect_bio_async(meta_learning_fep_bridge_t* bridge);
bool meta_learning_fep_bridge_is_bio_async_connected(const meta_learning_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_META_LEARNING_FEP_BRIDGE_H */
