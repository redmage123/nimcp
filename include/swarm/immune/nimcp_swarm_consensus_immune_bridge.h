/**
 * @file nimcp_swarm_consensus_immune_bridge.h
 * @brief Swarm Consensus-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between brain immune system and swarm consensus voting
 * WHY:  Inflammation affects decision-making; vote failures trigger immune responses
 * HOW:  Cytokines delay voting; failed consensus triggers inflammation
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines increase voting delays and quorum requirements
 * - Inflammation impairs collective decision-making
 * - Vote failures and consensus breaks trigger stress response
 * - Successful consensus releases anti-inflammatory signals
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SWARM_CONSENSUS_IMMUNE_BRIDGE_H
#define NIMCP_SWARM_CONSENSUS_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cytokine voting impact factors */
#define CYTOKINE_IL1_VOTING_DELAY             1.3f
#define CYTOKINE_IL6_VOTING_DELAY             1.2f
#define CYTOKINE_TNF_VOTING_DELAY             1.4f
#define CYTOKINE_IL10_VOTING_BOOST            0.9f

/* Inflammation consensus factors */
#define INFLAMMATION_NONE_QUORUM_FACTOR       1.0f
#define INFLAMMATION_LOCAL_QUORUM_FACTOR      1.1f
#define INFLAMMATION_REGIONAL_QUORUM_FACTOR   1.2f
#define INFLAMMATION_SYSTEMIC_QUORUM_FACTOR   1.4f
#define INFLAMMATION_STORM_QUORUM_FACTOR      1.6f

typedef struct {
    float voting_delay_factor;
    float quorum_increase_factor;
    float agreement_threshold_factor;
    float timeout_multiplier;
} cytokine_consensus_effects_t;

typedef struct {
    brain_inflammation_level_t current_level;
    float quorum_factor;
    float decision_impairment;
    bool consensus_blocked;
} inflammation_consensus_state_t;

typedef struct {
    float vote_success_rate;
    uint32_t failed_votes;
    bool stress_triggered;
    float il10_from_success;
} consensus_immune_modulation_t;

typedef struct {
    bool enable_cytokine_effects;
    bool enable_inflammation_effects;
    bool enable_failure_stress;
    bool enable_success_boost;
    float cytokine_sensitivity;
} swarm_consensus_immune_config_t;

typedef struct swarm_consensus_immune_bridge_struct swarm_consensus_immune_bridge_t;

int swarm_consensus_immune_default_config(swarm_consensus_immune_config_t* config);
swarm_consensus_immune_bridge_t* swarm_consensus_immune_bridge_create(
    const swarm_consensus_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_consensus);
void swarm_consensus_immune_bridge_destroy(swarm_consensus_immune_bridge_t* bridge);

int swarm_consensus_immune_apply_cytokine_effects(swarm_consensus_immune_bridge_t* bridge);
int swarm_consensus_immune_apply_inflammation_effects(swarm_consensus_immune_bridge_t* bridge);

int swarm_consensus_immune_trigger_from_failure(swarm_consensus_immune_bridge_t* bridge);
int swarm_consensus_immune_boost_from_success(swarm_consensus_immune_bridge_t* bridge);

int swarm_consensus_immune_bridge_update(swarm_consensus_immune_bridge_t* bridge, uint64_t delta_ms);

float swarm_consensus_immune_get_voting_delay(const swarm_consensus_immune_bridge_t* bridge);
float swarm_consensus_immune_get_quorum_factor(const swarm_consensus_immune_bridge_t* bridge);
bool swarm_consensus_immune_is_blocked(const swarm_consensus_immune_bridge_t* bridge);

int swarm_consensus_immune_connect_bio_async(swarm_consensus_immune_bridge_t* bridge);
int swarm_consensus_immune_disconnect_bio_async(swarm_consensus_immune_bridge_t* bridge);
bool swarm_consensus_immune_is_bio_async_connected(const swarm_consensus_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_CONSENSUS_IMMUNE_BRIDGE_H */
