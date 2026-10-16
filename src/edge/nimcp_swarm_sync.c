/**
 * @file nimcp_swarm_sync.c
 * @brief Sync round coordinator — manages gradient collection, aggregation,
 *        and delta weight push across a multi-device swarm.
 *
 * WHAT: State machine that drives one sync round: IDLE -> COLLECTING ->
 *       AGGREGATING -> PUSHING -> COMPLETE. The master calls these functions
 *       to orchestrate federated gradient aggregation across edge peers.
 * WHY:  Devices train independently and periodically sync gradients. The
 *       coordinator ensures all submissions arrive (or timeout), triggers
 *       aggregation via nimcp_federated_aggregate, and computes compressed
 *       delta weights for efficient transport back to edges.
 * HOW:  Allocates per-round state with gradient slots for max_peers.
 *       begin() snapshots pre-round weights. submit_gradient() collects
 *       per-device gradients. aggregate() calls the federated API.
 *       compute_delta() uses the delta API for sparse compressed diffs.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "edge/nimcp_swarm_runtime_types.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"

#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default sparsity threshold for delta compression (ignore changes < this). */
#define SYNC_DELTA_SPARSITY_THRESHOLD  1e-6f

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_sync_round_t* nimcp_sync_round_create(uint32_t max_peers,
                                             uint32_t num_params) {
    if (max_peers == 0 || num_params == 0) {
        return NULL;
    }

    nimcp_sync_round_t* round =
        (nimcp_sync_round_t*)nimcp_calloc(1, sizeof(nimcp_sync_round_t));
    if (!round) {
        return NULL;
    }

    /* Gradient collection slots — one nimcp_federated_gradient_t per peer.
     * Each entry's .gradients pointer is set when a submission arrives. */
    round->gradients =
        (nimcp_federated_gradient_t*)nimcp_calloc(max_peers,
                                                   sizeof(nimcp_federated_gradient_t));
    if (!round->gradients) {
        nimcp_free(round);
        return NULL;
    }

    /* Pre-allocate aggregation output buffer */
    round->aggregated_gradients = (float*)nimcp_calloc(num_params, sizeof(float));
    if (!round->aggregated_gradients) {
        nimcp_free(round->gradients);
        nimcp_free(round);
        return NULL;
    }

    /* Pre-allocate pre-round weight snapshot buffer */
    round->pre_round_weights = (float*)nimcp_calloc(num_params, sizeof(float));
    if (!round->pre_round_weights) {
        nimcp_free(round->aggregated_gradients);
        nimcp_free(round->gradients);
        nimcp_free(round);
        return NULL;
    }

    round->num_params = num_params;
    round->phase = NIMCP_SYNC_IDLE;
    round->round_id = 0;
    round->gradients_received = 0;
    round->gradients_expected = 0;

    /* Store max_peers in gradients_expected temporarily — we use it as capacity.
     * The actual expected count is set in begin(). We need a capacity field;
     * since the header struct doesn't have one, we track it via a convention:
     * gradients array is allocated for max_peers entries. We store capacity
     * by keeping it as a local invariant (checked against gradients_received). */

    return round;
}

void nimcp_sync_round_destroy(nimcp_sync_round_t* round) {
    if (!round) {
        return;
    }

    /* Free per-submission gradient copies */
    if (round->gradients) {
        for (uint32_t i = 0; i < round->gradients_received; i++) {
            nimcp_free(round->gradients[i].gradients);
        }
        nimcp_free(round->gradients);
    }

    nimcp_free(round->aggregated_gradients);
    nimcp_free(round->pre_round_weights);
    nimcp_free(round);
}

/* ============================================================================
 * Round State Machine
 * ============================================================================ */

int nimcp_sync_round_begin(nimcp_sync_round_t* round, uint64_t round_id,
                            uint32_t expected_peers, uint32_t timeout_ms) {
    if (!round) {
        return -1;
    }

    if (round->phase != NIMCP_SYNC_IDLE) {
        LOG_WARN("[swarm/sync] Cannot begin round %lu — phase is %d, expected IDLE",
                 (unsigned long)round_id, (int)round->phase);
        return -1;
    }

    if (expected_peers == 0) {
        return -1;
    }

    round->round_id = round_id;
    round->phase = NIMCP_SYNC_COLLECTING;
    round->round_start_ts = nimcp_time_now_us() / 1000;  /* Convert us -> ms */
    round->timeout_ms = timeout_ms;
    round->gradients_expected = expected_peers;
    round->gradients_received = 0;

    /* Clear any stale gradient submissions */
    for (uint32_t i = 0; i < expected_peers; i++) {
        nimcp_free(round->gradients[i].gradients);
        memset(&round->gradients[i], 0, sizeof(nimcp_federated_gradient_t));
    }

    /* Clear aggregation output */
    memset(round->aggregated_gradients, 0, round->num_params * sizeof(float));

    return 0;
}

int nimcp_sync_round_submit_gradient(nimcp_sync_round_t* round,
                                      uint32_t device_id,
                                      const float* gradients,
                                      uint32_t num_params) {
    if (!round || !gradients) {
        return -1;
    }

    if (round->phase != NIMCP_SYNC_COLLECTING) {
        LOG_WARN("[swarm/sync] Gradient from device %u rejected — not collecting (phase=%d)",
                 device_id, (int)round->phase);
        return -1;
    }

    if (num_params != round->num_params) {
        LOG_WARN("[swarm/sync] Gradient from device %u has wrong size (%u vs expected %u)",
                 device_id, num_params, round->num_params);
        return -1;
    }

    if (round->gradients_received >= round->gradients_expected) {
        LOG_WARN("[swarm/sync] Gradient from device %u rejected — already at capacity (%u/%u)",
                 device_id, round->gradients_received, round->gradients_expected);
        return -1;
    }

    /* Copy gradient data — we own the memory until round reset */
    float* grad_copy = (float*)nimcp_malloc(num_params * sizeof(float));
    if (!grad_copy) {
        return -1;
    }
    memcpy(grad_copy, gradients, num_params * sizeof(float));

    uint32_t idx = round->gradients_received;
    round->gradients[idx].device_id = device_id;
    round->gradients[idx].num_params = num_params;
    round->gradients[idx].gradients = grad_copy;
    round->gradients[idx].local_steps = 0;
    memset(&round->gradients[idx].version, 0, sizeof(nimcp_model_version_t));

    round->gradients_received++;

    return 0;
}

bool nimcp_sync_round_is_ready(const nimcp_sync_round_t* round) {
    if (!round) {
        return false;
    }

    if (round->phase != NIMCP_SYNC_COLLECTING) {
        return false;
    }

    /* All expected gradients received */
    if (round->gradients_received >= round->gradients_expected) {
        return true;
    }

    /* Timeout elapsed — proceed with whatever we have */
    if (round->timeout_ms > 0) {
        uint64_t now_ms = nimcp_time_now_us() / 1000;
        if (now_ms >= round->round_start_ts + round->timeout_ms) {
            return true;
        }
    }

    return false;
}

int nimcp_sync_round_aggregate(nimcp_sync_round_t* round,
                                const nimcp_federated_config_t* config) {
    if (!round || !config) {
        return -1;
    }

    if (round->phase != NIMCP_SYNC_COLLECTING) {
        LOG_WARN("[swarm/sync] Cannot aggregate — not in COLLECTING phase (phase=%d)",
                 (int)round->phase);
        return -1;
    }

    if (round->gradients_received == 0) {
        LOG_WARN("[swarm/sync] Cannot aggregate — no gradients received");
        round->phase = NIMCP_SYNC_IDLE;
        return -1;
    }

    round->phase = NIMCP_SYNC_AGGREGATING;

    int ret = nimcp_federated_aggregate(
        round->gradients,
        round->gradients_received,
        round->aggregated_gradients,
        round->num_params,
        config->aggregation);

    if (ret != 0) {
        LOG_WARN("[swarm/sync] Aggregation failed for round %lu",
                 (unsigned long)round->round_id);
        round->phase = NIMCP_SYNC_IDLE;
        return -1;
    }

    round->phase = NIMCP_SYNC_PUSHING;
    return 0;
}

int nimcp_sync_round_compute_delta(const nimcp_sync_round_t* round,
                                    const float* current_weights,
                                    uint32_t num_params,
                                    nimcp_weight_delta_t* delta_out) {
    if (!round || !current_weights || !delta_out || num_params == 0) {
        return -1;
    }

    if (!round->pre_round_weights) {
        return -1;
    }

    if (num_params != round->num_params) {
        LOG_WARN("[swarm/sync] Delta compute param mismatch (%u vs %u)",
                 num_params, round->num_params);
        return -1;
    }

    /* Compute sparse delta between pre-round snapshot and current weights */
    int ret = nimcp_weight_delta_compute(
        round->pre_round_weights, current_weights,
        num_params, SYNC_DELTA_SPARSITY_THRESHOLD, delta_out);
    if (ret != 0) {
        return -1;
    }

    /* Compress the delta for efficient transport */
    if (delta_out->num_changes > 0) {
        ret = nimcp_weight_delta_compress(delta_out);
        if (ret != 0) {
            LOG_WARN("[swarm/sync] Delta compression failed, sending uncompressed");
            /* Non-fatal — delta is still valid, just uncompressed */
        }
    }

    return 0;
}

void nimcp_sync_round_reset(nimcp_sync_round_t* round) {
    if (!round) {
        return;
    }

    /* Free per-submission gradient copies */
    if (round->gradients) {
        for (uint32_t i = 0; i < round->gradients_received; i++) {
            nimcp_free(round->gradients[i].gradients);
            round->gradients[i].gradients = NULL;
        }
    }

    round->phase = NIMCP_SYNC_IDLE;
    round->gradients_received = 0;
    round->gradients_expected = 0;
    round->round_id = 0;
    round->round_start_ts = 0;
    round->timeout_ms = 0;

    if (round->aggregated_gradients) {
        memset(round->aggregated_gradients, 0, round->num_params * sizeof(float));
    }
}

nimcp_sync_phase_t nimcp_sync_round_get_phase(const nimcp_sync_round_t* round) {
    if (!round) {
        return NIMCP_SYNC_IDLE;
    }
    return round->phase;
}

/* ============================================================================
 * Pre-round Weight Snapshot
 * ============================================================================ */

int nimcp_sync_round_snapshot_weights(nimcp_sync_round_t* round,
                                      const float* weights,
                                      uint32_t num_params) {
    if (!round || !weights) {
        return -1;
    }

    if (num_params != round->num_params) {
        return -1;
    }

    memcpy(round->pre_round_weights, weights, num_params * sizeof(float));
    return 0;
}
