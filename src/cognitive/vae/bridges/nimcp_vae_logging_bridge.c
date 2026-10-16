/**
 * @file nimcp_vae_logging_bridge.c
 * @brief VAE-Logging Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Implements comprehensive logging integration for VAE operations.
 *
 * BIO_MODULE: 0x1F13
 */

#include "cognitive/vae/bridges/nimcp_vae_logging_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/nimcp_vae_latent.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static const char* event_to_string(vae_log_event_t event)
{
    switch (event) {
        case VAE_LOG_EVENT_CREATED:             return "CREATED";
        case VAE_LOG_EVENT_DESTROYED:           return "DESTROYED";
        case VAE_LOG_EVENT_RESET:               return "RESET";
        case VAE_LOG_EVENT_FORWARD:             return "FORWARD";
        case VAE_LOG_EVENT_BACKWARD:            return "BACKWARD";
        case VAE_LOG_EVENT_TRAIN_START:         return "TRAIN_START";
        case VAE_LOG_EVENT_TRAIN_END:           return "TRAIN_END";
        case VAE_LOG_EVENT_EPOCH_START:         return "EPOCH_START";
        case VAE_LOG_EVENT_EPOCH_END:           return "EPOCH_END";
        case VAE_LOG_EVENT_BATCH:               return "BATCH";
        case VAE_LOG_EVENT_ANOMALY:             return "ANOMALY";
        case VAE_LOG_EVENT_HEALTH_CHECK:        return "HEALTH_CHECK";
        case VAE_LOG_EVENT_BRIDGE_CONNECTED:    return "BRIDGE_CONNECTED";
        case VAE_LOG_EVENT_BRIDGE_DISCONNECTED: return "BRIDGE_DISCONNECTED";
        default:                                return "UNKNOWN";
    }
}

static bool should_log(vae_logging_bridge_t* bridge, vae_log_verbosity_t level)
{
    if (!bridge) {
        return false;
    }
    return level <= bridge->config.verbosity;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_logging_bridge_default_config(vae_logging_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_logging_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(vae_logging_config_t));

    config->verbosity = VAE_LOG_VERBOSITY_INFO;

    config->log_training = true;
    config->log_inference = false;
    config->log_latent = false;
    config->log_anomalies = true;
    config->log_health = true;
    config->log_bridges = true;

    config->batch_log_interval = 100;
    config->health_log_interval_ms = 60000;  /* 1 minute */

    config->log_loss_components = true;
    config->log_latent_stats = false;
    config->log_gradient_norms = false;
    config->log_timing = true;

    config->include_timestamp = true;
    config->include_batch_info = true;
    config->format_json = false;

    return 0;
}

vae_logging_bridge_t* vae_logging_bridge_create(const vae_logging_config_t* config)
{
    vae_logging_config_t default_config;

    if (!config) {
        vae_logging_bridge_default_config(&default_config);
        config = &default_config;
    }

    vae_logging_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_logging_bridge_t));
    if (!bridge) {
        NIMCP_LOG_ERROR("VAE-Logging Bridge: Failed to allocate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_logging_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->config = *config;
    bridge->vae = NULL;
    bridge->is_initialized = true;
    bridge->creation_time_us = get_timestamp_us();
    bridge->last_health_log_us = bridge->creation_time_us;

    NIMCP_LOG_INFO("VAE-Logging Bridge: Created (verbosity=%d)",
                   config->verbosity);

    return bridge;
}

void vae_logging_bridge_destroy(vae_logging_bridge_t* bridge)
{
    if (!bridge) return;

    NIMCP_LOG_INFO("VAE-Logging Bridge: Destroyed (total_logs=%lu)",
                   (unsigned long)bridge->stats.total_logs);

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_logging_bridge_connect_vae(vae_logging_bridge_t* bridge, vae_system_t* vae)
{
    if (!bridge || !vae) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_logging_bridge_connect_vae: required parameter is NULL (bridge, vae)");
        return -1;
    }

    bridge->vae = vae;

    if (bridge->config.log_bridges) {
        NIMCP_LOG_INFO("[%s] VAE system connected", VAE_LOG_CATEGORY_BRIDGE);
    }

    return 0;
}

int vae_logging_bridge_disconnect(vae_logging_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_logging_bridge_disconnect: bridge is NULL");
        return -1;
    }

    bridge->vae = NULL;

    if (bridge->config.log_bridges) {
        NIMCP_LOG_INFO("[%s] VAE system disconnected", VAE_LOG_CATEGORY_BRIDGE);
    }

    return 0;
}

/* ============================================================================
 * Event Logging API
 * ============================================================================ */

void vae_log_event(vae_logging_bridge_t* bridge, vae_log_event_t event,
                   const char* format, ...)
{
    if (!bridge || !should_log(bridge, VAE_LOG_VERBOSITY_INFO)) {
        if (bridge) bridge->stats.suppressed_logs++;
        return;
    }

    char msg[NIMCP_ERROR_BUFFER_LARGE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    NIMCP_LOG_INFO("[VAE.Event.%s] %s", event_to_string(event), msg);

    bridge->stats.total_logs++;
    bridge->stats.info_logs++;
}

void vae_log_train_start(vae_logging_bridge_t* bridge,
                         uint32_t num_epochs, uint32_t batch_size)
{
    if (!bridge || !bridge->config.log_training) return;

    bridge->current_epoch = 0;
    bridge->current_batch = 0;
    bridge->train_start_time_us = get_timestamp_us();

    NIMCP_LOG_INFO("[%s] Training started (epochs=%u, batch_size=%u)",
                   VAE_LOG_CATEGORY_TRAINING, num_epochs, batch_size);

    bridge->stats.total_logs++;
    bridge->stats.training_logs++;
}

void vae_log_train_end(vae_logging_bridge_t* bridge,
                       float final_loss, uint64_t total_time_ms)
{
    if (!bridge || !bridge->config.log_training) return;

    NIMCP_LOG_INFO("[%s] Training completed (final_loss=%.6f, time=%lu ms)",
                   VAE_LOG_CATEGORY_TRAINING, final_loss,
                   (unsigned long)total_time_ms);

    bridge->stats.total_logs++;
    bridge->stats.training_logs++;
}

void vae_log_epoch_start(vae_logging_bridge_t* bridge, uint32_t epoch)
{
    if (!bridge || !bridge->config.log_training) return;

    bridge->current_epoch = epoch;
    bridge->current_batch = 0;

    if (should_log(bridge, VAE_LOG_VERBOSITY_DEBUG)) {
        NIMCP_LOG_DEBUG("[%s] Epoch %u started", VAE_LOG_CATEGORY_TRAINING, epoch);
        bridge->stats.total_logs++;
        bridge->stats.debug_logs++;
    }
}

void vae_log_epoch_end(vae_logging_bridge_t* bridge, uint32_t epoch,
                       float avg_loss, float avg_recon, float avg_kl)
{
    if (!bridge || !bridge->config.log_training) return;

    if (bridge->config.log_loss_components) {
        NIMCP_LOG_INFO("[%s] Epoch %u: loss=%.6f (recon=%.6f, kl=%.6f)",
                       VAE_LOG_CATEGORY_TRAINING, epoch, avg_loss, avg_recon, avg_kl);
    } else {
        NIMCP_LOG_INFO("[%s] Epoch %u: loss=%.6f",
                       VAE_LOG_CATEGORY_TRAINING, epoch, avg_loss);
    }

    bridge->stats.total_logs++;
    bridge->stats.training_logs++;
}

void vae_log_batch(vae_logging_bridge_t* bridge, uint32_t batch,
                   const vae_loss_t* loss, float learning_rate)
{
    if (!bridge || !bridge->config.log_training || !loss) return;

    bridge->current_batch = batch;

    /* Only log at specified intervals */
    if (batch % bridge->config.batch_log_interval != 0) {
        return;
    }

    if (should_log(bridge, VAE_LOG_VERBOSITY_DEBUG)) {
        if (bridge->config.log_loss_components) {
            NIMCP_LOG_DEBUG("[%s] Batch %u: loss=%.6f (recon=%.6f, kl=%.6f, weighted_kl=%.4f) lr=%.6f",
                           VAE_LOG_CATEGORY_TRAINING, batch,
                           loss->total_loss, loss->reconstruction_loss, loss->kl_divergence,
                           loss->weighted_kl, learning_rate);
        } else {
            NIMCP_LOG_DEBUG("[%s] Batch %u: loss=%.6f lr=%.6f",
                           VAE_LOG_CATEGORY_TRAINING, batch, loss->total_loss, learning_rate);
        }
        bridge->stats.total_logs++;
        bridge->stats.debug_logs++;
        bridge->stats.training_logs++;
    }
}

void vae_log_forward(vae_logging_bridge_t* bridge,
                     uint32_t input_dim, uint32_t latent_dim,
                     float recon_error, uint64_t time_us)
{
    if (!bridge || !bridge->config.log_inference) return;

    if (should_log(bridge, VAE_LOG_VERBOSITY_TRACE)) {
        if (bridge->config.log_timing) {
            NIMCP_LOG_DEBUG("[%s] Forward: %u -> %u -> %u (recon=%.6f, time=%lu us)",
                           VAE_LOG_CATEGORY_INFERENCE, input_dim, latent_dim, input_dim,
                           recon_error, (unsigned long)time_us);
        } else {
            NIMCP_LOG_DEBUG("[%s] Forward: %u -> %u -> %u (recon=%.6f)",
                           VAE_LOG_CATEGORY_INFERENCE, input_dim, latent_dim, input_dim,
                           recon_error);
        }
        bridge->stats.total_logs++;
        bridge->stats.debug_logs++;
    }
}

void vae_log_backward(vae_logging_bridge_t* bridge,
                      float gradient_norm, uint64_t time_us)
{
    if (!bridge || !bridge->config.log_training) return;

    if (should_log(bridge, VAE_LOG_VERBOSITY_TRACE) && bridge->config.log_gradient_norms) {
        NIMCP_LOG_DEBUG("[%s] Backward: grad_norm=%.6f time=%lu us",
                       VAE_LOG_CATEGORY_TRAINING, gradient_norm, (unsigned long)time_us);
        bridge->stats.total_logs++;
        bridge->stats.debug_logs++;
    }
}

/* ============================================================================
 * Latent Space Logging
 * ============================================================================ */

void vae_log_latent_stats(vae_logging_bridge_t* bridge,
                          const vae_latent_state_t* latent)
{
    if (!bridge || !bridge->config.log_latent || !latent) return;

    if (!should_log(bridge, VAE_LOG_VERBOSITY_DEBUG)) return;

    /* Compute statistics */
    float mean_mu = 0.0f, var_mu = 0.0f;
    float mean_logvar = 0.0f;

    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        mean_mu += latent->mu[i];
        mean_logvar += latent->log_var[i];
    }
    mean_mu /= latent->latent_dim;
    mean_logvar /= latent->latent_dim;

    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        float diff = latent->mu[i] - mean_mu;
        var_mu += diff * diff;
    }
    var_mu /= latent->latent_dim;

    NIMCP_LOG_DEBUG("[%s] dim=%u mean_mu=%.4f var_mu=%.4f mean_logvar=%.4f",
                   VAE_LOG_CATEGORY_LATENT, latent->latent_dim,
                   mean_mu, var_mu, mean_logvar);

    bridge->stats.total_logs++;
    bridge->stats.debug_logs++;
}

void vae_log_latent_histogram(vae_logging_bridge_t* bridge,
                              const vae_latent_state_t* latent,
                              uint32_t num_bins)
{
    if (!bridge || !bridge->config.log_latent || !latent) return;
    if (!should_log(bridge, VAE_LOG_VERBOSITY_TRACE)) return;

    (void)num_bins;  /* Simplified - just log range */

    float min_val = latent->mu[0], max_val = latent->mu[0];
    for (uint32_t i = 1; i < latent->latent_dim; i++) {
        if (latent->mu[i] < min_val) min_val = latent->mu[i];
        if (latent->mu[i] > max_val) max_val = latent->mu[i];
    }

    NIMCP_LOG_DEBUG("[%s] Latent range: [%.4f, %.4f]",
                   VAE_LOG_CATEGORY_LATENT, min_val, max_val);

    bridge->stats.total_logs++;
}

/* ============================================================================
 * Anomaly Logging
 * ============================================================================ */

void vae_log_anomaly(vae_logging_bridge_t* bridge,
                     const char* anomaly_type,
                     float severity,
                     float value,
                     float threshold)
{
    if (!bridge || !bridge->config.log_anomalies) return;

    if (severity >= 0.7f) {
        NIMCP_LOG_WARN("[%s] %s detected (severity=%.2f, value=%.4f, threshold=%.4f)",
                      VAE_LOG_CATEGORY_ANOMALY, anomaly_type, severity, value, threshold);
        bridge->stats.warning_logs++;
    } else {
        NIMCP_LOG_INFO("[%s] %s detected (severity=%.2f, value=%.4f, threshold=%.4f)",
                      VAE_LOG_CATEGORY_ANOMALY, anomaly_type, severity, value, threshold);
        bridge->stats.info_logs++;
    }

    bridge->stats.total_logs++;
    bridge->stats.anomaly_logs++;
}

void vae_log_ood(vae_logging_bridge_t* bridge,
                 float ood_score,
                 bool is_ood)
{
    if (!bridge || !bridge->config.log_anomalies) return;

    if (is_ood) {
        NIMCP_LOG_WARN("[%s] Out-of-distribution sample detected (score=%.4f)",
                      VAE_LOG_CATEGORY_ANOMALY, ood_score);
        bridge->stats.warning_logs++;
    } else if (should_log(bridge, VAE_LOG_VERBOSITY_DEBUG)) {
        NIMCP_LOG_DEBUG("[%s] In-distribution sample (score=%.4f)",
                       VAE_LOG_CATEGORY_ANOMALY, ood_score);
        bridge->stats.debug_logs++;
    }

    bridge->stats.total_logs++;
}

/* ============================================================================
 * Health Logging
 * ============================================================================ */

void vae_log_health(vae_logging_bridge_t* bridge,
                    float health_score,
                    uint32_t consecutive_failures,
                    const char* status)
{
    if (!bridge || !bridge->config.log_health) return;

    if (health_score < 0.5f) {
        NIMCP_LOG_WARN("[%s] Degraded health (score=%.2f, failures=%u, status=%s)",
                      VAE_LOG_CATEGORY_HEALTH, health_score, consecutive_failures,
                      status ? status : "unknown");
        bridge->stats.warning_logs++;
    } else {
        NIMCP_LOG_INFO("[%s] Health check (score=%.2f, status=%s)",
                      VAE_LOG_CATEGORY_HEALTH, health_score,
                      status ? status : "ok");
        bridge->stats.info_logs++;
    }

    bridge->stats.total_logs++;
    bridge->stats.health_logs++;
}

void vae_log_health_check(vae_logging_bridge_t* bridge)
{
    if (!bridge || !bridge->config.log_health) return;

    uint64_t now = get_timestamp_us();
    uint64_t elapsed_ms = (now - bridge->last_health_log_us) / 1000;

    if (elapsed_ms < bridge->config.health_log_interval_ms) {
        return;
    }

    bridge->last_health_log_us = now;

    /* Log stats summary */
    uint64_t uptime_s = (now - bridge->creation_time_us) / 1000000;

    NIMCP_LOG_INFO("[%s] Periodic report: uptime=%lu s, logs=%lu (err=%lu, warn=%lu)",
                   VAE_LOG_CATEGORY_HEALTH, (unsigned long)uptime_s,
                   (unsigned long)bridge->stats.total_logs,
                   (unsigned long)bridge->stats.error_logs,
                   (unsigned long)bridge->stats.warning_logs);

    bridge->stats.total_logs++;
    bridge->stats.health_logs++;
}

/* ============================================================================
 * Bridge Event Logging
 * ============================================================================ */

void vae_log_bridge_connected(vae_logging_bridge_t* bridge,
                               const char* bridge_name)
{
    if (!bridge || !bridge->config.log_bridges) return;

    NIMCP_LOG_INFO("[%s] Bridge connected: %s",
                   VAE_LOG_CATEGORY_BRIDGE, bridge_name ? bridge_name : "unknown");

    bridge->stats.total_logs++;
}

void vae_log_bridge_disconnected(vae_logging_bridge_t* bridge,
                                  const char* bridge_name)
{
    if (!bridge || !bridge->config.log_bridges) return;

    NIMCP_LOG_INFO("[%s] Bridge disconnected: %s",
                   VAE_LOG_CATEGORY_BRIDGE, bridge_name ? bridge_name : "unknown");

    bridge->stats.total_logs++;
}

void vae_log_bridge_sync(vae_logging_bridge_t* bridge,
                          const char* bridge_name,
                          const char* direction,
                          uint64_t time_us)
{
    if (!bridge || !bridge->config.log_bridges) return;

    if (should_log(bridge, VAE_LOG_VERBOSITY_DEBUG)) {
        NIMCP_LOG_DEBUG("[%s] %s sync %s (time=%lu us)",
                       VAE_LOG_CATEGORY_BRIDGE,
                       bridge_name ? bridge_name : "unknown",
                       direction ? direction : "bidirectional",
                       (unsigned long)time_us);
        bridge->stats.total_logs++;
        bridge->stats.debug_logs++;
    }
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int vae_logging_set_verbosity(vae_logging_bridge_t* bridge,
                               vae_log_verbosity_t verbosity)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_logging_set_verbosity: bridge is NULL");
        return -1;
    }
    bridge->config.verbosity = verbosity;
    return 0;
}

int vae_logging_set_category(vae_logging_bridge_t* bridge,
                              const char* category,
                              bool enabled)
{
    if (!bridge || !category) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_logging_set_category: required parameter is NULL (bridge, category)");
        return -1;
    }

    if (strcmp(category, VAE_LOG_CATEGORY_TRAINING) == 0) {
        bridge->config.log_training = enabled;
    } else if (strcmp(category, VAE_LOG_CATEGORY_INFERENCE) == 0) {
        bridge->config.log_inference = enabled;
    } else if (strcmp(category, VAE_LOG_CATEGORY_LATENT) == 0) {
        bridge->config.log_latent = enabled;
    } else if (strcmp(category, VAE_LOG_CATEGORY_ANOMALY) == 0) {
        bridge->config.log_anomalies = enabled;
    } else if (strcmp(category, VAE_LOG_CATEGORY_HEALTH) == 0) {
        bridge->config.log_health = enabled;
    } else if (strcmp(category, VAE_LOG_CATEGORY_BRIDGE) == 0) {
        bridge->config.log_bridges = enabled;
    } else {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_logging_set_category: operation failed");
        return -1;
    }

    return 0;
}

int vae_logging_get_stats(const vae_logging_bridge_t* bridge,
                           vae_logging_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_logging_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}
