/**
 * @file nimcp_swarm_consensus_fep_bridge.h
 * @brief FEP Bridge for Swarm Consensus Voting
 *
 * WHAT: Free Energy Principle integration for Byzantine fault-tolerant voting
 * WHY:  Consensus as collective inference, voting as belief aggregation
 * HOW:  Vote confidence as precision, agreement as belief convergence
 *
 * BIOLOGICAL BASIS:
 * - Voting as collective Bayesian inference
 * - Confidence weighting as precision weighting
 * - Quorum thresholds as belief certainty thresholds
 * - Consensus emergence as free energy minimization
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#ifndef NIMCP_SWARM_CONSENSUS_FEP_BRIDGE_H
#define NIMCP_SWARM_CONSENSUS_FEP_BRIDGE_H

#include "swarm/nimcp_swarm_consensus.h"
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
    float confidence_precision_weight; /**< Confidence as precision */
    float agreement_fe_threshold;    /**< FE threshold for agreement */
    float quorum_certainty_gain;     /**< Certainty gain from quorum */
    bool enable_bayesian_voting;     /**< Bayesian vote aggregation */
} swarm_consensus_fep_config_t;

typedef struct {
    float vote_confidence_boost;     /**< Vote confidence adjustment */
    float quorum_threshold_adjustment; /**< Quorum threshold modulation */
    float agreement_bias;            /**< Bias toward agreement */
} swarm_consensus_fep_effects_t;

typedef struct {
    float precision_from_consensus;  /**< Precision from vote agreement */
    float belief_strength_from_quorum; /**< Belief strength from quorum */
    uint32_t consensus_state;        /**< Consensus achievement state */
} fep_swarm_consensus_effects_t;

typedef struct {
    uint32_t votes_processed;
    uint32_t quorums_reached;
    float last_consensus_fe;
    uint64_t last_update_time;
} swarm_consensus_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t total_votes;
    uint64_t successful_quorums;
    float avg_consensus_fe;
} swarm_consensus_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    swarm_consensus_fep_config_t config;
    fep_system_t* fep_system;
    swarm_consensus_t consensus_ctx;
    swarm_consensus_fep_effects_t fep_effects;
    fep_swarm_consensus_effects_t consensus_effects;
    swarm_consensus_fep_state_t state;
    swarm_consensus_fep_stats_t stats;} swarm_consensus_fep_bridge_t;

void swarm_consensus_fep_default_config(swarm_consensus_fep_config_t* config);
swarm_consensus_fep_bridge_t* swarm_consensus_fep_create(const swarm_consensus_fep_config_t* config, swarm_consensus_t consensus_ctx, fep_system_t* fep_system);
void swarm_consensus_fep_destroy(swarm_consensus_fep_bridge_t* bridge);
int swarm_consensus_fep_update(swarm_consensus_fep_bridge_t* bridge);
int swarm_consensus_fep_apply_modulation(swarm_consensus_fep_bridge_t* bridge);
int swarm_consensus_fep_get_effects(const swarm_consensus_fep_bridge_t* bridge, swarm_consensus_fep_effects_t* effects);
int swarm_consensus_fep_get_consensus_effects(const swarm_consensus_fep_bridge_t* bridge, fep_swarm_consensus_effects_t* effects);
int swarm_consensus_fep_get_stats(const swarm_consensus_fep_bridge_t* bridge, swarm_consensus_fep_stats_t* stats);
int swarm_consensus_fep_connect_bio_async(swarm_consensus_fep_bridge_t* bridge);
int swarm_consensus_fep_disconnect_bio_async(swarm_consensus_fep_bridge_t* bridge);
bool swarm_consensus_fep_is_bio_async_connected(const swarm_consensus_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_CONSENSUS_FEP_BRIDGE_H */
