/**
 * @file nimcp_swarm_byzantine.c
 * @brief Byzantine fault detection — statistical anomaly detection for
 *        corrupted or malicious edge nodes in the swarm.
 *
 * WHAT: Analyzes gradient submissions and telemetry from peer nodes to detect
 *       Byzantine behavior (poisoned gradients, impossible telemetry values).
 * WHY:  In a federated swarm, a compromised or malfunctioning device can send
 *       poisoned gradients that corrupt the aggregated model. Statistical
 *       detection isolates bad actors before they cause damage.
 * HOW:  Gradient norms are tracked via exponential moving average (EMA).
 *       Submissions whose norm deviates > 3 standard deviations from the EMA
 *       are flagged as anomalies. Peers exceeding the anomaly threshold are
 *       quarantined. Telemetry is checked for physically impossible values.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "edge/nimcp_swarm_runtime_types.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** EMA smoothing factor for gradient norm tracking. */
#define BYZANTINE_EMA_ALPHA          0.1f

/** Number of standard deviations from EMA to flag as anomalous. */
#define BYZANTINE_NORM_DEVIATION     3.0f

/** Default anomaly count before quarantine. */
#define BYZANTINE_ANOMALY_THRESHOLD  5

/** Maximum physically plausible temperature (Celsius). */
#define BYZANTINE_MAX_TEMPERATURE    150.0f

/* ============================================================================
 * Gradient Anomaly Detection
 * ============================================================================ */

int nimcp_byzantine_check_gradient(nimcp_peer_entry_t* peer,
                                    const float* gradients,
                                    uint32_t num_params) {
    if (!peer || !gradients || num_params == 0) {
        return -1;
    }

    /* Compute L2 norm of the submitted gradient */
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < num_params; i++) {
        float g = gradients[i];

        /* NaN or Inf in gradient is immediately anomalous */
        if (isnan(g) || isinf(g)) {
            peer->anomaly_count++;
            LOG_WARN("[swarm/byzantine] Device %u: NaN/Inf in gradient at param %u "
                     "(anomaly_count=%u)",
                     peer->device_id, i, peer->anomaly_count);

            if (peer->anomaly_count > BYZANTINE_ANOMALY_THRESHOLD) {
                peer->quarantined = true;
                peer->state = NIMCP_PEER_BYZANTINE;
                LOG_WARN("[swarm/byzantine] Device %u QUARANTINED — anomaly_count=%u",
                         peer->device_id, peer->anomaly_count);
                return 1;
            }
            return 1;
        }

        sum_sq += g * g;
    }

    float norm = sqrtf(sum_sq);

    /* First submission — initialize EMA, no deviation check.
     * Use total_syncs alone; checking gradient_norm_ema == 0.0f fails
     * if the first gradient legitimately has norm 0. */
    if (peer->total_syncs == 0) {
        peer->gradient_norm_ema = norm;
        peer->total_syncs = 1;
        return 0;
    }

    /* Check deviation from EMA.
     * We use a simple threshold: if the norm is more than BYZANTINE_NORM_DEVIATION
     * times the EMA, it's anomalous. This is a simplified z-score check where
     * we treat the EMA itself as the "expected" value and use a multiplicative
     * deviation rather than tracking variance explicitly. */
    float ema = peer->gradient_norm_ema;
    float deviation = fabsf(norm - ema);
    float threshold = BYZANTINE_NORM_DEVIATION * ema;

    /* Prevent division-by-zero for near-zero EMA — use additive floor */
    if (threshold < 1e-7f) {
        threshold = BYZANTINE_NORM_DEVIATION * 1e-3f;
    }

    bool anomalous = (deviation > threshold);

    /* Update EMA regardless of anomaly status */
    peer->gradient_norm_ema = (1.0f - BYZANTINE_EMA_ALPHA) * ema
                            + BYZANTINE_EMA_ALPHA * norm;

    if (anomalous) {
        peer->anomaly_count++;
        LOG_WARN("[swarm/byzantine] Device %u: gradient norm %.4f deviates from "
                 "EMA %.4f (deviation=%.4f, threshold=%.4f, anomaly_count=%u)",
                 peer->device_id, norm, ema, deviation, threshold,
                 peer->anomaly_count);

        if (peer->anomaly_count > BYZANTINE_ANOMALY_THRESHOLD) {
            peer->quarantined = true;
            peer->state = NIMCP_PEER_BYZANTINE;
            LOG_WARN("[swarm/byzantine] Device %u QUARANTINED — anomaly_count=%u "
                     "exceeds threshold %d",
                     peer->device_id, peer->anomaly_count,
                     BYZANTINE_ANOMALY_THRESHOLD);
            return 1;
        }
        return 1;
    }

    return 0;
}

/* ============================================================================
 * Telemetry Validation
 * ============================================================================ */

int nimcp_byzantine_check_telemetry(const nimcp_peer_entry_t* peer,
                                     const nimcp_device_telemetry_t* telemetry) {
    if (!peer || !telemetry) {
        return -1;
    }

    /* Check for NaN in critical float fields */
    if (isnan(telemetry->avg_inference_ms) || isnan(telemetry->avg_loss) ||
        isnan(telemetry->avg_confidence) || isnan(telemetry->battery_pct) ||
        isnan(telemetry->temperature_c) || isnan(telemetry->ram_usage_pct) ||
        isnan(telemetry->cpu_usage_pct)) {
        LOG_WARN("[swarm/byzantine] Device %u: NaN in telemetry", peer->device_id);
        return 1;
    }

    /* Check for Inf in critical float fields */
    if (isinf(telemetry->avg_inference_ms) || isinf(telemetry->avg_loss) ||
        isinf(telemetry->avg_confidence) || isinf(telemetry->battery_pct) ||
        isinf(telemetry->temperature_c)) {
        LOG_WARN("[swarm/byzantine] Device %u: Inf in telemetry", peer->device_id);
        return 1;
    }

    /* Negative battery is physically impossible */
    if (telemetry->battery_pct < 0.0f) {
        LOG_WARN("[swarm/byzantine] Device %u: negative battery %.1f%%",
                 peer->device_id, telemetry->battery_pct);
        return 1;
    }

    /* Temperature above plausible maximum */
    if (telemetry->temperature_c > BYZANTINE_MAX_TEMPERATURE) {
        LOG_WARN("[swarm/byzantine] Device %u: temperature %.1f°C exceeds %.1f°C max",
                 peer->device_id, telemetry->temperature_c, BYZANTINE_MAX_TEMPERATURE);
        return 1;
    }

    /* Negative loss is not physically meaningful */
    if (telemetry->avg_loss < 0.0f) {
        LOG_WARN("[swarm/byzantine] Device %u: negative loss %.4f",
                 peer->device_id, telemetry->avg_loss);
        return 1;
    }

    /* Negative inference time */
    if (telemetry->avg_inference_ms < 0.0f) {
        LOG_WARN("[swarm/byzantine] Device %u: negative inference time %.2f ms",
                 peer->device_id, telemetry->avg_inference_ms);
        return 1;
    }

    return 0;
}

/* ============================================================================
 * Peer Reset / Scoring
 * ============================================================================ */

void nimcp_byzantine_reset_peer(nimcp_peer_entry_t* peer) {
    if (!peer) {
        return;
    }

    peer->anomaly_count = 0;
    peer->gradient_norm_ema = 0.0f;
    peer->quarantined = false;

    /* If peer was BYZANTINE, transition back to ACTIVE on reset */
    if (peer->state == NIMCP_PEER_BYZANTINE) {
        peer->state = NIMCP_PEER_ACTIVE;
    }
}

float nimcp_byzantine_get_anomaly_score(const nimcp_peer_entry_t* peer) {
    if (!peer) {
        return 0.0f;
    }

    uint64_t total = peer->total_syncs;
    if (total == 0) {
        total = 1;
    }

    return (float)peer->anomaly_count / (float)total;
}

/* ============================================================================
 * Public API Wrappers (declared in nimcp_swarm_runtime.h)
 *
 * These provide the simplified public interface that doesn't require a
 * peer_entry_t. Used by external callers and the master runtime.
 * ============================================================================ */

bool nimcp_swarm_byzantine_check_gradient(
    const float* gradient,
    uint32_t num_params,
    float swarm_norm_ema,
    float threshold)
{
    if (!gradient || num_params == 0) {
        return false;
    }

    /* Compute L2 norm */
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < num_params; i++) {
        float g = gradient[i];
        if (isnan(g) || isinf(g)) {
            return true; /* NaN/Inf is always anomalous */
        }
        sum_sq += g * g;
    }

    float norm = sqrtf(sum_sq);

    /* Compare against swarm EMA with threshold factor */
    if (swarm_norm_ema > 0.0f && threshold > 0.0f) {
        float deviation = fabsf(norm - swarm_norm_ema);
        float limit = threshold * swarm_norm_ema;
        if (limit < 1e-7f) {
            limit = threshold * 1e-3f;
        }
        return deviation > limit;
    }

    return false;
}

bool nimcp_swarm_byzantine_check_telemetry(
    const nimcp_device_telemetry_t* telemetry)
{
    if (!telemetry) {
        return false;
    }

    /* Check for NaN in critical float fields */
    if (isnan(telemetry->avg_inference_ms) || isnan(telemetry->avg_loss) ||
        isnan(telemetry->avg_confidence) || isnan(telemetry->battery_pct) ||
        isnan(telemetry->temperature_c) || isnan(telemetry->ram_usage_pct) ||
        isnan(telemetry->cpu_usage_pct)) {
        return true;
    }

    /* Check for Inf */
    if (isinf(telemetry->avg_inference_ms) || isinf(telemetry->avg_loss) ||
        isinf(telemetry->avg_confidence) || isinf(telemetry->battery_pct) ||
        isinf(telemetry->temperature_c)) {
        return true;
    }

    /* Negative battery is physically impossible */
    if (telemetry->battery_pct < 0.0f) {
        return true;
    }

    /* Temperature above plausible maximum */
    if (telemetry->temperature_c > BYZANTINE_MAX_TEMPERATURE) {
        return true;
    }

    /* Negative loss or inference time */
    if (telemetry->avg_loss < 0.0f || telemetry->avg_inference_ms < 0.0f) {
        return true;
    }

    return false;
}
