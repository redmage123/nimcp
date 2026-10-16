/**
 * @file nimcp_collective_workspace_fep_bridge.h
 * @brief FEP Bridge for Collective Workspace
 *
 * WHAT: Free Energy Principle integration for swarm collective attention
 * WHY:  Workspace salience as precision, broadcasting as inference
 * HOW:  Item competition as free energy minimization, CRDT merge as belief update
 *
 * BIOLOGICAL BASIS:
 * - Workspace items as hypotheses under consideration
 * - Salience as precision weighting (attention)
 * - Broadcasting as precision-weighted prediction error propagation
 * - CRDT merge as Bayesian belief update
 * - Collective focus as shared generative model
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#ifndef NIMCP_COLLECTIVE_WORKSPACE_FEP_BRIDGE_H
#define NIMCP_COLLECTIVE_WORKSPACE_FEP_BRIDGE_H

#include "swarm/nimcp_collective_workspace.h"
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
    float salience_precision_weight; /**< Salience as precision */
    float coherence_fe_coupling;     /**< Coherence affects FE */
    float broadcast_threshold_adaptation; /**< Adapt broadcast by FE */
    bool enable_fe_pruning;          /**< Prune by free energy */
} collective_workspace_fep_config_t;

typedef struct {
    float salience_modulation;       /**< Salience adjustment for items */
    float broadcast_urgency;         /**< Broadcast urgency modulation */
    float pruning_threshold_adjustment; /**< Pruning threshold modulation */
} collective_workspace_fep_effects_t;

typedef struct {
    float precision_from_coherence;  /**< Precision from workspace coherence */
    float attention_from_salience;   /**< Attention weighting from salience */
    float collective_focus_strength; /**< Strength of collective focus */
} fep_collective_workspace_effects_t;

typedef struct {
    float last_coherence;
    uint32_t items_broadcast;
    uint32_t items_pruned;
    uint64_t last_update_time;
} collective_workspace_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t total_items_processed;
    float avg_workspace_fe;
} collective_workspace_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    collective_workspace_fep_config_t config;
    fep_system_t* fep_system;
    collective_workspace_t* workspace;
    collective_workspace_fep_effects_t fep_effects;
    fep_collective_workspace_effects_t workspace_effects;
    collective_workspace_fep_state_t state;
    collective_workspace_fep_stats_t stats;} collective_workspace_fep_bridge_t;

void collective_workspace_fep_default_config(collective_workspace_fep_config_t* config);
collective_workspace_fep_bridge_t* collective_workspace_fep_create(const collective_workspace_fep_config_t* config, collective_workspace_t* workspace, fep_system_t* fep_system);
void collective_workspace_fep_destroy(collective_workspace_fep_bridge_t* bridge);
int collective_workspace_fep_update(collective_workspace_fep_bridge_t* bridge);
int collective_workspace_fep_apply_modulation(collective_workspace_fep_bridge_t* bridge);
int collective_workspace_fep_get_effects(const collective_workspace_fep_bridge_t* bridge, collective_workspace_fep_effects_t* effects);
int collective_workspace_fep_get_workspace_effects(const collective_workspace_fep_bridge_t* bridge, fep_collective_workspace_effects_t* effects);
int collective_workspace_fep_get_stats(const collective_workspace_fep_bridge_t* bridge, collective_workspace_fep_stats_t* stats);
int collective_workspace_fep_connect_bio_async(collective_workspace_fep_bridge_t* bridge);
int collective_workspace_fep_disconnect_bio_async(collective_workspace_fep_bridge_t* bridge);
bool collective_workspace_fep_is_bio_async_connected(const collective_workspace_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COLLECTIVE_WORKSPACE_FEP_BRIDGE_H */
