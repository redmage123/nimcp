/**
 * @file nimcp_swarm_quorum_fep_bridge.h
 * @brief FEP Bridge for Swarm Quorum Sensing
 *
 * WHAT: Free Energy Principle integration for quorum sensing decisions
 * WHY:  Quorum as collective belief threshold, sensing as inference
 * HOW:  Signal accumulation as evidence integration, threshold as certainty
 *
 * BIOLOGICAL BASIS:
 * - Signal molecules as evidence accumulation
 * - Threshold activation as belief certainty criterion
 * - Positive feedback as precision amplification
 * - Cross-inhibition as mutual exclusivity enforcement
 * - Commitment cascade as belief propagation
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#ifndef NIMCP_SWARM_QUORUM_FEP_BRIDGE_H
#define NIMCP_SWARM_QUORUM_FEP_BRIDGE_H

#include "swarm/nimcp_swarm_quorum.h"
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
    float signal_evidence_weight;    /**< Signal as evidence strength */
    float threshold_certainty_level; /**< Threshold as certainty criterion */
    float cascade_precision_gain;    /**< Precision from commitment cascade */
    bool enable_fe_threshold_adaptation; /**< Adapt threshold by FE */
} swarm_quorum_fep_config_t;

typedef struct {
    float signal_amplification;      /**< Signal strength boost */
    float threshold_adjustment;      /**< Threshold modulation */
    float cascade_trigger_bias;      /**< Cascade triggering bias */
} swarm_quorum_fep_effects_t;

typedef struct {
    float precision_from_consensus;  /**< Precision from quorum achievement */
    float belief_strength_from_commitment; /**< Belief from commitment level */
    uint32_t quorum_state;           /**< Quorum achievement state */
} fep_swarm_quorum_effects_t;

typedef struct {
    uint32_t signals_processed;
    uint32_t quorums_achieved;
    float last_quorum_fe;
    uint64_t last_update_time;
} swarm_quorum_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t decisions_made;
    float avg_quorum_fe;
} swarm_quorum_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    swarm_quorum_fep_config_t config;
    fep_system_t* fep_system;
    nimcp_swarm_quorum_t* quorum_system;
    swarm_quorum_fep_effects_t fep_effects;
    fep_swarm_quorum_effects_t quorum_effects;
    swarm_quorum_fep_state_t state;
    swarm_quorum_fep_stats_t stats;} swarm_quorum_fep_bridge_t;

void swarm_quorum_fep_default_config(swarm_quorum_fep_config_t* config);
swarm_quorum_fep_bridge_t* swarm_quorum_fep_create(const swarm_quorum_fep_config_t* config, nimcp_swarm_quorum_t* quorum_system, fep_system_t* fep_system);
void swarm_quorum_fep_destroy(swarm_quorum_fep_bridge_t* bridge);
int swarm_quorum_fep_update(swarm_quorum_fep_bridge_t* bridge);
int swarm_quorum_fep_apply_modulation(swarm_quorum_fep_bridge_t* bridge);
int swarm_quorum_fep_get_effects(const swarm_quorum_fep_bridge_t* bridge, swarm_quorum_fep_effects_t* effects);
int swarm_quorum_fep_get_quorum_effects(const swarm_quorum_fep_bridge_t* bridge, fep_swarm_quorum_effects_t* effects);
int swarm_quorum_fep_get_stats(const swarm_quorum_fep_bridge_t* bridge, swarm_quorum_fep_stats_t* stats);
int swarm_quorum_fep_connect_bio_async(swarm_quorum_fep_bridge_t* bridge);
int swarm_quorum_fep_disconnect_bio_async(swarm_quorum_fep_bridge_t* bridge);
bool swarm_quorum_fep_is_bio_async_connected(const swarm_quorum_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_QUORUM_FEP_BRIDGE_H */
