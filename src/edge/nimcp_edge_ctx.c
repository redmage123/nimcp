/**
 * @file nimcp_edge_ctx.c
 * @brief Edge context lifecycle — create, destroy, and inference recording.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>

/* ============================================================================
 * Edge Context Create
 * ============================================================================ */

nimcp_edge_ctx_t* nimcp_edge_ctx_create(const nimcp_device_profile_t* profile)
{
    if (!profile) {
        LOG_ERROR("[edge/ctx] NULL device profile");
        return NULL;
    }

    nimcp_edge_ctx_t* ctx =
        (nimcp_edge_ctx_t*)nimcp_calloc(1, sizeof(nimcp_edge_ctx_t));
    if (!ctx) {
        LOG_ERROR("[edge/ctx] Failed to allocate edge context");
        return NULL;
    }

    /* Copy device profile */
    memcpy(&ctx->profile, profile, sizeof(nimcp_device_profile_t));

    /* Initialize model version to 0.0.0 */
    memset(&ctx->version, 0, sizeof(ctx->version));

    /* Initialize rollback state */
    memset(&ctx->rollback, 0, sizeof(ctx->rollback));
    ctx->rollback.active = false;
    ctx->rollback.rollback_threshold = 1.5f; /* Default: rollback if 1.5x baseline */
    ctx->rollback.validation_steps = 100;

    /* Initialize early exit — NULL until configured */
    ctx->early_exit = NULL;

    /* Initialize offline policy with defaults */
    ctx->offline.last_sync_timestamp = 0;
    ctx->offline.steps_since_sync = 0;
    ctx->offline.confidence_decay_rate = 0.001f;
    ctx->offline.min_confidence_multiplier = 0.3f;
    ctx->offline.current_confidence = 1.0f;
    ctx->offline.cautious_after_steps = 100;
    ctx->offline.conservative_after_steps = 500;
    ctx->offline.frozen_after_steps = 2000;
    ctx->offline.current_mode = NIMCP_OFFLINE_NORMAL;

    /* Initialize power config based on device profile */
    ctx->power.mode = NIMCP_POWER_FULL;
    ctx->power.inference_hz = profile->target_hz > 0.0f ? profile->target_hz : 30.0f;
    ctx->power.learning_rate_scale = 1.0f;
    ctx->power.subsystem_mask = 0xFFFFFFFF; /* All enabled initially */
    ctx->power.early_exit_forced = false;
    ctx->power.early_exit_threshold = 0.9f;
    ctx->power.gpu_enabled = profile->has_gpu;
    ctx->power.auto_manage = (profile->power_budget_watts > 0.0f);
    ctx->power.balanced_battery_pct = 80.0f;
    ctx->power.saving_battery_pct = 50.0f;
    ctx->power.critical_battery_pct = 20.0f;
    ctx->power.thermal_throttle_c = 80.0f;

    /* EWC — NULL until initialized by federated learning */
    ctx->ewc = NULL;

    /* Maturation — NULL until a resize-expand creates one */
    ctx->maturation = NULL;

    /* Gossip config defaults */
    ctx->gossip.gossip_blend_ratio = 0.1f;
    ctx->gossip.urgency_threshold = 0.5f;
    ctx->gossip.max_ttl = 3;
    ctx->gossip.broadcast_loss_ratio = 1.5f;
    ctx->gossip.rate_limit_ms = 5000;
    ctx->gossip.seen_hashes = NULL;
    ctx->gossip.seen_hash_count = 0;
    ctx->gossip.seen_hash_capacity = 0;

    /* OTA state */
    memset(&ctx->ota, 0, sizeof(ctx->ota));
    ctx->ota.stage = NIMCP_OTA_IDLE;

    /* Swarm transport — NULL until connected */
    ctx->transport = NULL;

    /* Differential privacy defaults */
    ctx->dp.noise_scale = 0.01f;
    ctx->dp.gradient_clip_norm = 1.0f;
    ctx->dp.privacy_budget_epsilon = 10.0f;
    ctx->dp.privacy_spent = 0.0f;
    ctx->dp.enabled = false;

    /* Zero telemetry buffers */
    memset(ctx->inference_times, 0, sizeof(ctx->inference_times));
    ctx->inference_time_idx = 0;
    memset(ctx->recent_losses, 0, sizeof(ctx->recent_losses));
    ctx->recent_loss_idx = 0;
    ctx->total_steps = 0;

    LOG_INFO("[edge/ctx] Edge context created for device '%s' "
             "(ram=%u MB, gpu=%s, role=%d)",
             profile->device_name, profile->ram_mb,
             profile->has_gpu ? "yes" : "no", profile->role);

    return ctx;
}

/* ============================================================================
 * Edge Context Destroy
 * ============================================================================ */

void nimcp_edge_ctx_destroy(nimcp_edge_ctx_t* ctx)
{
    if (!ctx) {
        return;
    }

    /* Free early exit */
    if (ctx->early_exit) {
        nimcp_early_exit_destroy(ctx->early_exit);
        ctx->early_exit = NULL;
    }

    /* Free EWC state */
    if (ctx->ewc) {
        nimcp_ewc_destroy(ctx->ewc);
        ctx->ewc = NULL;
    }

    /* Free maturation tracker */
    if (ctx->maturation) {
        nimcp_maturation_destroy(ctx->maturation);
        ctx->maturation = NULL;
    }

    /* Free swarm transport */
    if (ctx->transport) {
        nimcp_swarm_transport_destroy(ctx->transport);
        ctx->transport = NULL;
    }

    /* Free gossip seen_hashes if allocated */
    if (ctx->gossip.seen_hashes) {
        nimcp_free(ctx->gossip.seen_hashes);
        ctx->gossip.seen_hashes = NULL;
    }

    /* Free rollback buffer if active */
    if (ctx->rollback.active && ctx->rollback.previous_weights) {
        nimcp_free(ctx->rollback.previous_weights);
        ctx->rollback.previous_weights = NULL;
    }

    /* Free OTA staged weights if present */
    if (ctx->ota.staged_weights) {
        nimcp_free(ctx->ota.staged_weights);
        ctx->ota.staged_weights = NULL;
    }

    LOG_INFO("[edge/ctx] Edge context destroyed");
    nimcp_free(ctx);
}

/* ============================================================================
 * Inference Recording
 * ============================================================================ */

void nimcp_edge_record_inference(nimcp_edge_ctx_t* ctx,
                                  float inference_ms, float loss)
{
    if (!ctx) {
        return;
    }

    /* Store inference time in circular buffer */
    ctx->inference_times[ctx->inference_time_idx] = inference_ms;
    ctx->inference_time_idx = (ctx->inference_time_idx + 1) % 100;

    /* Store loss in circular buffer */
    ctx->recent_losses[ctx->recent_loss_idx] = loss;
    ctx->recent_loss_idx = (ctx->recent_loss_idx + 1) % 100;

    /* Increment total steps */
    ctx->total_steps++;
}
