/**
 * @file nimcp_swarm_consensus_quantum_bridge.h
 * @brief Quantum-accelerated swarm consensus with Byzantine detection
 *
 * WHAT: Integrates quantum consensus algorithms with swarm voting
 * WHY:  Faster consensus with collusion/Byzantine fault detection
 * HOW:  Amplitude-weighted voting with Grover-based anomaly detection
 *
 * BIOLOGICAL INSPIRATION:
 * - Quorum sensing in bacterial colonies
 * - Democratic decision-making in honeybee swarms
 * - Neural population voting in motor cortex
 */

#ifndef NIMCP_SWARM_CONSENSUS_QUANTUM_BRIDGE_H
#define NIMCP_SWARM_CONSENSUS_QUANTUM_BRIDGE_H

#include "swarm/nimcp_quantum_consensus.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct swarm_quantum_bridge swarm_quantum_bridge_t;

typedef struct {
    bool enabled;
    bool detect_collusion;
    float agreement_threshold;
    uint32_t max_voters;
} swarm_quantum_config_t;

typedef struct {
    uint64_t quantum_votes;
    uint64_t collusion_detected;
    float avg_consensus_confidence;
} swarm_quantum_stats_t;

//=============================================================================
// API
//=============================================================================

swarm_quantum_config_t swarm_quantum_default_config(void);

swarm_quantum_bridge_t* swarm_quantum_bridge_create(
    const swarm_quantum_config_t* config
);

void swarm_quantum_bridge_destroy(swarm_quantum_bridge_t* bridge);

bool swarm_quantum_bridge_is_enabled(const swarm_quantum_bridge_t* bridge);

void swarm_quantum_bridge_set_enabled(swarm_quantum_bridge_t* bridge, bool enabled);

/**
 * WHAT: Create a proposal for voting
 */
int swarm_quantum_propose(
    swarm_quantum_bridge_t* bridge,
    const char* topic,
    uint32_t* proposal_id_out
);

/**
 * WHAT: Submit vote with quantum amplitude weighting
 */
int swarm_quantum_vote(
    swarm_quantum_bridge_t* bridge,
    uint32_t proposal_id,
    uint32_t voter_id,
    quantum_vote_choice_t choice,
    float weight
);

/**
 * WHAT: Compute consensus with quantum acceleration
 */
int swarm_quantum_compute_consensus(
    swarm_quantum_bridge_t* bridge,
    uint32_t proposal_id,
    quantum_consensus_result_t* result_out
);

int swarm_quantum_get_stats(
    const swarm_quantum_bridge_t* bridge,
    swarm_quantum_stats_t* stats
);

void swarm_quantum_reset_stats(swarm_quantum_bridge_t* bridge);

//=============================================================================
// Implementation
//=============================================================================

#ifdef NIMCP_SWARM_QUANTUM_BRIDGE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include "utils/memory/nimcp_memory.h"

struct swarm_quantum_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    swarm_quantum_config_t config;
    quantum_consensus_t qconsensus;  /* Direct handle */
    swarm_quantum_stats_t stats;
    uint32_t next_proposer_id;
};

swarm_quantum_config_t swarm_quantum_default_config(void) {
    return (swarm_quantum_config_t){
        .enabled = true,
        .detect_collusion = true,
        .agreement_threshold = 0.5f,
        .max_voters = 256
    };
}

swarm_quantum_bridge_t* swarm_quantum_bridge_create(
    const swarm_quantum_config_t* config
) {
    swarm_quantum_bridge_t* bridge = (swarm_quantum_bridge_t*)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) return NULL;

    bridge->config = config ? *config : swarm_quantum_default_config();

    quantum_consensus_config_t qconfig = quantum_consensus_default_config();
    qconfig.max_voters = bridge->config.max_voters;
    qconfig.agreement_threshold = bridge->config.agreement_threshold;
    qconfig.enable_collusion_detection = bridge->config.detect_collusion;

    bridge->qconsensus = quantum_consensus_create(&qconfig);
    if (!bridge->qconsensus) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->next_proposer_id = 1;
    return bridge;
}

void swarm_quantum_bridge_destroy(swarm_quantum_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->qconsensus) quantum_consensus_destroy(bridge->qconsensus);
    nimcp_free(bridge);
}

bool swarm_quantum_bridge_is_enabled(const swarm_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void swarm_quantum_bridge_set_enabled(swarm_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

int swarm_quantum_propose(
    swarm_quantum_bridge_t* bridge,
    const char* topic,
    uint32_t* proposal_id_out
) {
    if (!bridge || !topic || !proposal_id_out) return -1;

    uint32_t proposer_id = bridge->next_proposer_id++;
    float value = 1.0f;  /* Default value */
    uint64_t deadline = 0;  /* No deadline */

    return quantum_consensus_propose(bridge->qconsensus, proposer_id,
                                      topic, value, deadline, proposal_id_out);
}

int swarm_quantum_vote(
    swarm_quantum_bridge_t* bridge,
    uint32_t proposal_id,
    uint32_t voter_id,
    quantum_vote_choice_t choice,
    float weight
) {
    if (!bridge) return -1;
    bridge->stats.quantum_votes++;
    return quantum_consensus_vote(bridge->qconsensus, proposal_id, voter_id, choice, weight);
}

int swarm_quantum_compute_consensus(
    swarm_quantum_bridge_t* bridge,
    uint32_t proposal_id,
    quantum_consensus_result_t* result_out
) {
    if (!bridge || !result_out) return -1;

    int status = quantum_consensus_run(bridge->qconsensus, proposal_id, result_out);
    if (status < 0) return status;

    bridge->stats.avg_consensus_confidence =
        (bridge->stats.avg_consensus_confidence * (bridge->stats.quantum_votes - 1)
         + result_out->weighted_agreement) / bridge->stats.quantum_votes;

    if (result_out->collusion_detected) {
        bridge->stats.collusion_detected++;
    }

    return 0;
}

int swarm_quantum_get_stats(
    const swarm_quantum_bridge_t* bridge,
    swarm_quantum_stats_t* stats
) {
    if (!bridge || !stats) return -1;
    *stats = bridge->stats;
    return 0;
}

void swarm_quantum_reset_stats(swarm_quantum_bridge_t* bridge) {
    if (bridge) memset(&bridge->stats, 0, sizeof(bridge->stats));
}

#endif // NIMCP_SWARM_QUANTUM_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SWARM_CONSENSUS_QUANTUM_BRIDGE_H
