/**
 * @file nimcp_telemetry.c
 * @brief Device telemetry — collection, serialization, and analysis.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <time.h>
#include <math.h>

/* ============================================================================
 * Telemetry Collection
 * ============================================================================ */

int nimcp_telemetry_collect(const nimcp_edge_ctx_t* ctx,
                             nimcp_device_telemetry_t* telemetry)
{
    if (!ctx || !telemetry) {
        return -1;
    }

    memset(telemetry, 0, sizeof(*telemetry));

    /* Device identity */
    telemetry->timestamp = (uint64_t)time(NULL);

    /* Compute average inference time from circular buffer */
    uint32_t inference_count = 0;
    float inference_sum = 0.0f;
    float inference_max = 0.0f;

    for (uint32_t i = 0; i < 100; i++) {
        float t = ctx->inference_times[i];
        if (t > 0.0f) {
            inference_sum += t;
            inference_count++;
            if (t > inference_max) {
                inference_max = t;
            }
        }
    }

    /* Explicitly zero — memset at line 27 covers this, but be defensive */
    telemetry->avg_inference_ms = 0.0f;
    telemetry->p99_inference_ms = 0.0f;

    if (inference_count > 0) {
        telemetry->avg_inference_ms = inference_sum / (float)inference_count;

        /* Approximate p99: use max from the buffer as a rough upper bound.
         * A proper implementation would sort and pick the 99th percentile. */
        telemetry->p99_inference_ms = inference_max;
    }

    /* Compute average loss and loss trend from circular buffer */
    uint32_t loss_count = 0;
    float loss_sum = 0.0f;

    /* Count valid entries: if total_steps >= 100, all 100 entries are valid;
     * otherwise only total_steps entries have been written. This correctly
     * includes zero-loss values instead of skipping them. */
    uint32_t valid_losses = (ctx->total_steps >= 100) ? 100 : (uint32_t)ctx->total_steps;
    for (uint32_t i = 0; i < valid_losses; i++) {
        loss_sum += ctx->recent_losses[i];
        loss_count++;
    }

    if (loss_count > 0) {
        telemetry->avg_loss = loss_sum / (float)loss_count;
    }

    /* Loss trend: compute slope using linear regression on recent losses.
     * Use the circular buffer order: oldest to newest. */
    if (loss_count >= 3) {
        /* Simple slope estimate: compare average of second half to first half */
        uint32_t half = loss_count / 2;
        float first_half_sum = 0.0f;
        float second_half_sum = 0.0f;
        uint32_t first_count = 0;
        uint32_t second_count = 0;

        /* Walk from oldest to newest */
        uint32_t start = ctx->recent_loss_idx;  /* next write = oldest */
        for (uint32_t k = 0; k < loss_count; k++) {
            uint32_t idx = (start + k) % 100;
            float l = ctx->recent_losses[idx];
            if (k < half) {
                first_half_sum += l;
                first_count++;
            } else {
                second_half_sum += l;
                second_count++;
            }
        }

        if (first_count > 0 && second_count > 0) {
            float first_avg = first_half_sum / (float)first_count;
            float second_avg = second_half_sum / (float)second_count;
            telemetry->loss_trend = second_avg - first_avg;
        }
    }

    /* Confidence: estimate from loss (lower loss = higher confidence) */
    telemetry->avg_confidence = (telemetry->avg_loss < 1.0f)
        ? 1.0f - telemetry->avg_loss
        : 0.0f;

    /* Low confidence percentage */
    uint32_t low_conf_count = 0;
    for (uint32_t i = 0; i < 100 && i < loss_count; i++) {
        if (ctx->recent_losses[i] > 0.7f) {
            low_conf_count++;
        }
    }
    telemetry->low_confidence_pct = (loss_count > 0)
        ? (float)low_conf_count / (float)loss_count * 100.0f
        : 0.0f;

    /* Steps since sync */
    telemetry->steps_since_sync = (uint32_t)ctx->total_steps;

    /* Copy offline and power modes */
    telemetry->offline_mode = ctx->offline.current_mode;
    telemetry->power_mode = ctx->power.mode;

    /* Copy model version */
    telemetry->model_version = ctx->version;

    return 0;
}

/* ============================================================================
 * Serialization (flat binary)
 * ============================================================================ */

int nimcp_telemetry_serialize(const nimcp_device_telemetry_t* telemetry,
                               uint8_t* buffer, uint32_t buffer_size,
                               uint32_t* bytes_written)
{
    if (!telemetry || !buffer || !bytes_written) {
        return -1;
    }

    uint32_t required = (uint32_t)sizeof(nimcp_device_telemetry_t);
    if (buffer_size < required) {
        LOG_ERROR("[edge/telemetry] Buffer too small: need %u, have %u",
                  required, buffer_size);
        *bytes_written = 0;
        return -1;
    }

    memcpy(buffer, telemetry, required);
    *bytes_written = required;

    return 0;
}

int nimcp_telemetry_deserialize(const uint8_t* buffer, uint32_t size,
                                 nimcp_device_telemetry_t* telemetry)
{
    if (!buffer || !telemetry) {
        return -1;
    }

    uint32_t required = (uint32_t)sizeof(nimcp_device_telemetry_t);
    if (size < required) {
        LOG_ERROR("[edge/telemetry] Buffer too small for deserialization: "
                  "need %u, have %u", required, size);
        return -1;
    }

    memcpy(telemetry, buffer, required);
    return 0;
}

/* ============================================================================
 * Telemetry Analysis — Master-side
 * ============================================================================ */

uint32_t nimcp_telemetry_analyze(const nimcp_device_telemetry_t* telemetry)
{
    if (!telemetry) {
        return NIMCP_TELEMETRY_ACTION_NONE;
    }

    uint32_t actions = NIMCP_TELEMETRY_ACTION_NONE;

    /* Loss trending upward for sustained period → redistill */
    if (telemetry->loss_trend > 0.1f) {
        actions |= NIMCP_TELEMETRY_ACTION_REDISTILL;
    }

    /* High anomaly rate → alert */
    if (telemetry->anomaly_rate > 20.0f) {
        actions |= NIMCP_TELEMETRY_ACTION_ALERT_ANOMALY;
    }

    /* Low confidence → redistill */
    if (telemetry->avg_confidence < 0.3f) {
        actions |= NIMCP_TELEMETRY_ACTION_REDISTILL;
    }

    /* Too many rollbacks → stop updates */
    if (telemetry->rollbacks_triggered > 2) {
        actions |= NIMCP_TELEMETRY_ACTION_STOP_UPDATES;
    }

    /* Temperature too high → reduce compute */
    if (telemetry->temperature_c > 85.0f) {
        actions |= NIMCP_TELEMETRY_ACTION_REDUCE_COMPUTE;
    }

    /* Low battery → power save */
    if (telemetry->battery_pct < 10.0f && telemetry->battery_pct >= 0.0f) {
        actions |= NIMCP_TELEMETRY_ACTION_POWER_SAVE;
    }

    return actions;
}
