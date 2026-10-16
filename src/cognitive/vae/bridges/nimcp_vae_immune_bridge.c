/**
 * @file nimcp_vae_immune_bridge.c
 * @brief VAE-Immune Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Implements bidirectional integration between VAE anomaly detection
 * and the brain immune system.
 *
 * BIO_MODULE: 0x1F11
 */

#include "cognitive/vae/bridges/nimcp_vae_immune_bridge.h"
#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/nimcp_vae_latent.h"
#include "cognitive/immune/nimcp_brain_immune.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread.h"
/* TODO: Fix immune path #include "immune/nimcp_immune.h" */

#include <math.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define VAE_IMMUNE_MODULE_ID            BIO_MODULE_VAE_IMMUNE_BRIDGE
#define VAE_IMMUNE_EMA_ALPHA            0.95f
#define VAE_IMMUNE_BASELINE_MIN_SAMPLES 100
#define VAE_IMMUNE_MAX_CONSEC_ANOMALIES 10

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Update EMA statistics
 */
static void update_ema(float* avg, float value, float alpha)
{
    if (*avg == 0.0f) {
        *avg = value;
    } else {
        *avg = alpha * (*avg) + (1.0f - alpha) * value;
    }
}

/**
 * @brief Check rate limiting
 */
static bool check_rate_limit(vae_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    uint64_t now = get_timestamp_us();
    uint64_t current_second = now / 1000000ULL;

    if (current_second != bridge->current_second) {
        bridge->current_second = current_second;
        bridge->reports_this_second = 0;
    }

    /* Check cooldown */
    if (now - bridge->last_report_time_us < bridge->config.cooldown_ms * 1000ULL) {
        return false;
    }

    /* Check rate limit */
    if (bridge->reports_this_second >= bridge->config.max_reports_per_second) {
        return false;
    }

    return true;
}

/**
 * @brief Record anomaly in history
 */
static void record_anomaly(vae_immune_bridge_t* bridge,
                           vae_anomaly_type_t type,
                           vae_anomaly_severity_t severity,
                           float value,
                           float threshold,
                           bool reported,
                           uint32_t antigen_id)
{
    if (!bridge || !bridge->anomaly_history) return;

    vae_anomaly_record_t* record = &bridge->anomaly_history[bridge->history_index];

    record->type = type;
    record->severity = severity;
    record->value = value;
    record->threshold = threshold;
    record->timestamp_us = get_timestamp_us();
    record->reported = reported;
    record->antigen_id = antigen_id;

    bridge->history_index = (bridge->history_index + 1) % VAE_IMMUNE_ANOMALY_HISTORY_SIZE;
    if (bridge->history_count < VAE_IMMUNE_ANOMALY_HISTORY_SIZE) {
        bridge->history_count++;
    }
}

/**
 * @brief Determine severity from value and threshold
 */
static vae_anomaly_severity_t determine_severity(float value, float threshold)
{
    float ratio = value / fmaxf(threshold, 1e-6f);

    if (ratio < 1.0f) return VAE_SEVERITY_NONE;
    if (ratio < 1.5f) return VAE_SEVERITY_LOW;
    if (ratio < 2.0f) return VAE_SEVERITY_MEDIUM;
    if (ratio < 3.0f) return VAE_SEVERITY_HIGH;
    return VAE_SEVERITY_CRITICAL;
}

/**
 * @brief Update health metrics
 */
static void update_health(vae_immune_bridge_t* bridge, bool anomaly_detected)
{
    if (!bridge) return;

    if (anomaly_detected) {
        bridge->health.consecutive_anomalies++;
        bridge->health.bridge_health = fmaxf(0.1f,
            bridge->health.bridge_health - 0.05f);

        if (bridge->health.consecutive_anomalies >= VAE_IMMUNE_MAX_CONSEC_ANOMALIES) {
            bridge->health.is_healthy = false;
        }
    } else {
        bridge->health.consecutive_anomalies = 0;
        bridge->health.bridge_health = fminf(1.0f,
            bridge->health.bridge_health + 0.01f);
        bridge->health.is_healthy = true;
    }

    /* Update detection rate */
    if (bridge->stats.total_checks > 0) {
        bridge->health.detection_rate = (float)bridge->stats.anomalies_detected /
                                        (float)bridge->stats.total_checks;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_immune_bridge_default_config(vae_immune_bridge_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_IMMUNE_MODULE_ID, NIMCP_ERROR_VAE_IMMUNE_NULL_BRIDGE,
                             "NULL config in vae_immune_bridge_default_config");
        return -1;
    }

    memset(config, 0, sizeof(vae_immune_bridge_config_t));

    /* Detection thresholds */
    config->thresholds.reconstruction_threshold = VAE_IMMUNE_DEFAULT_RECON_THRESHOLD;
    config->thresholds.kl_divergence_threshold = VAE_IMMUNE_DEFAULT_KL_THRESHOLD;
    config->thresholds.variance_threshold = VAE_IMMUNE_DEFAULT_VAR_THRESHOLD;
    config->thresholds.free_energy_threshold = VAE_IMMUNE_DEFAULT_FE_THRESHOLD;
    config->thresholds.ood_threshold = 3.0f;
    config->thresholds.latent_drift_threshold = 2.0f;

    /* Detection settings */
    config->enable_auto_detection = true;
    config->enable_latent_monitoring = true;

    /* Reporting settings */
    config->report_low_severity = false;
    config->cooldown_ms = VAE_IMMUNE_DEFAULT_COOLDOWN_MS;
    config->max_reports_per_second = 10;

    /* Immune integration */
    config->enable_immune_modulation = true;
    config->modulation_strength = 0.5f;

    /* Cytokine settings */
    config->enable_cytokine_signals = true;
    config->danger_signal_threshold = VAE_IMMUNE_DEFAULT_FE_THRESHOLD * 2.0f;

    /* Logging */
    config->enable_logging = false;
    config->log_all_anomalies = false;

    return 0;
}

vae_immune_bridge_t* vae_immune_bridge_create(const vae_immune_bridge_config_t* config)
{
    vae_immune_bridge_config_t default_config;

    if (!config) {
        if (vae_immune_bridge_default_config(&default_config) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_bridge_create: validation failed");
            return NULL;
        }
        config = &default_config;
    }

    vae_immune_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_IMMUNE_MODULE_ID, NIMCP_ERROR_VAE_NO_MEMORY,
                             "Failed to allocate VAE-Immune bridge");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = VAE_IMMUNE_STATE_DISCONNECTED;
    bridge->vae = NULL;
    bridge->immune = NULL;

    /* Allocate anomaly history */
    bridge->anomaly_history = nimcp_calloc(VAE_IMMUNE_ANOMALY_HISTORY_SIZE,
                                           sizeof(vae_anomaly_record_t));
    if (!bridge->anomaly_history) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_immune_bridge_create: bridge->anomaly_history is NULL");
        return NULL;
    }

    /* Create mutex */
    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    bridge->mutex = nimcp_mutex_create(&attr);
    if (!bridge->mutex) {
        NIMCP_LOG_ERROR("VAE-Immune Bridge: Failed to create mutex");
        nimcp_free(bridge->anomaly_history);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_immune_bridge_create: bridge->mutex is NULL");
        return NULL;
    }

    /* Initialize health */
    bridge->health.bridge_health = 1.0f;
    bridge->health.is_healthy = true;
    bridge->health.detection_rate = 0.0f;
    bridge->health.false_positive_rate = 0.0f;

    /* Initialize timing */
    bridge->creation_time_us = get_timestamp_us();
    bridge->current_second = get_timestamp_us() / 1000000ULL;

    bridge->is_initialized = true;

    NIMCP_LOG_INFO("VAE-Immune Bridge: Created");

    return bridge;
}

void vae_immune_bridge_destroy(vae_immune_bridge_t* bridge)
{
    if (!bridge) return;

    NIMCP_LOG_INFO("VAE-Immune Bridge: Destroying");

    vae_immune_bridge_disconnect(bridge);

    if (bridge->anomaly_history) {
        nimcp_free(bridge->anomaly_history);
    }

    if (bridge->baseline_latent_mean) {
        nimcp_free(bridge->baseline_latent_mean);
    }
    if (bridge->baseline_latent_var) {
        nimcp_free(bridge->baseline_latent_var);
    }

    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_immune_bridge_reset(vae_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_IMMUNE_MODULE_ID, NIMCP_ERROR_VAE_IMMUNE_NULL_BRIDGE,
                             "Invalid bridge in reset");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(vae_immune_bridge_stats_t));

    /* Reset health */
    bridge->health.bridge_health = 1.0f;
    bridge->health.is_healthy = true;
    bridge->health.consecutive_anomalies = 0;
    bridge->health.detection_rate = 0.0f;

    /* Reset history */
    bridge->history_count = 0;
    bridge->history_index = 0;

    /* Reset rate limiting */
    bridge->reports_this_second = 0;
    bridge->last_report_time_us = 0;

    /* Reset modulation */
    bridge->current_response = VAE_IMMUNE_RESPONSE_NONE;
    bridge->modulation_factor = 0.0f;

    /* Reset baseline */
    bridge->baseline_established = false;
    bridge->baseline_samples = 0;

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_DEBUG("VAE-Immune Bridge: Reset");

    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int vae_immune_bridge_connect_vae(vae_immune_bridge_t* bridge, vae_system_t* vae)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_IMMUNE_MODULE_ID, NIMCP_ERROR_VAE_IMMUNE_NULL_BRIDGE,
                             "Invalid bridge in connect_vae");
        return -1;
    }

    if (!vae) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_IMMUNE_MODULE_ID, NIMCP_ERROR_VAE_IMMUNE_NO_VAE,
                             "NULL VAE in connect_vae");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->vae = vae;

    /* Allocate baseline buffers based on VAE latent dim */
    uint32_t latent_dim = vae_get_latent_dim(vae);
    if (latent_dim > 0) {
        if (bridge->baseline_latent_mean) {
            nimcp_free(bridge->baseline_latent_mean);
        }
        if (bridge->baseline_latent_var) {
            nimcp_free(bridge->baseline_latent_var);
        }

        bridge->baseline_latent_mean = nimcp_calloc(latent_dim, sizeof(float));
        bridge->baseline_latent_var = nimcp_calloc(latent_dim, sizeof(float));
        if (!bridge->baseline_latent_mean || !bridge->baseline_latent_var) {
            if (bridge->baseline_latent_mean) {
                nimcp_free(bridge->baseline_latent_mean);
                bridge->baseline_latent_mean = NULL;
            }
            if (bridge->baseline_latent_var) {
                nimcp_free(bridge->baseline_latent_var);
                bridge->baseline_latent_var = NULL;
            }
            nimcp_mutex_unlock(bridge->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                                  "vae_immune_bridge_connect_vae: baseline buffer allocation failed");
            return -1;
        }
    }

    if (bridge->immune) {
        bridge->state = VAE_IMMUNE_STATE_CONNECTED;
    }

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_INFO("VAE-Immune Bridge: VAE connected (latent_dim=%u)", latent_dim);

    return 0;
}

int vae_immune_bridge_connect_immune(vae_immune_bridge_t* bridge,
                                      brain_immune_system_t* immune)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_IMMUNE_MODULE_ID, NIMCP_ERROR_VAE_IMMUNE_NULL_BRIDGE,
                             "Invalid bridge in connect_immune");
        return -1;
    }

    if (!immune) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_IMMUNE_MODULE_ID, NIMCP_ERROR_VAE_IMMUNE_NO_IMMUNE,
                             "NULL immune system in connect_immune");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->immune = immune;

    if (bridge->vae) {
        bridge->state = VAE_IMMUNE_STATE_CONNECTED;
    }

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_INFO("VAE-Immune Bridge: Immune system connected");

    return 0;
}

int vae_immune_bridge_disconnect(vae_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_bridge_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->vae = NULL;
    bridge->immune = NULL;
    bridge->state = VAE_IMMUNE_STATE_DISCONNECTED;

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_INFO("VAE-Immune Bridge: Disconnected");

    return 0;
}

bool vae_immune_bridge_is_connected(const vae_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return (bridge->vae != NULL) && (bridge->immune != NULL);
}

/* ============================================================================
 * Anomaly Detection API
 * ============================================================================ */

int vae_immune_check_anomalies(vae_immune_bridge_t* bridge,
                                const vae_loss_t* loss)
{
    if (!bridge || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_check_anomalies: required parameter is NULL (bridge, loss)");
        return -1;
    }
    if (!vae_immune_bridge_is_connected(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_immune_check_anomalies: vae_immune_bridge_is_connected is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->state = VAE_IMMUNE_STATE_MONITORING;
    bridge->stats.total_checks++;

    int anomalies_found = 0;
    const vae_immune_thresholds_t* thresh = &bridge->config.thresholds;

    /* Check reconstruction error */
    if (loss->reconstruction_loss > thresh->reconstruction_threshold) {
        vae_anomaly_severity_t sev = determine_severity(
            loss->reconstruction_loss, thresh->reconstruction_threshold);

        if (sev >= VAE_SEVERITY_MEDIUM || bridge->config.report_low_severity) {
            if (check_rate_limit(bridge)) {
                vae_immune_report_anomaly(bridge, VAE_ANOMALY_RECONSTRUCTION,
                                          sev, loss->reconstruction_loss);
            }
        }
        bridge->stats.reconstruction_anomalies++;
        anomalies_found++;
    }

    /* Check KL divergence */
    if (loss->kl_divergence > thresh->kl_divergence_threshold) {
        vae_anomaly_severity_t sev = determine_severity(
            loss->kl_divergence, thresh->kl_divergence_threshold);

        if (sev >= VAE_SEVERITY_MEDIUM || bridge->config.report_low_severity) {
            if (check_rate_limit(bridge)) {
                vae_immune_report_anomaly(bridge, VAE_ANOMALY_KL_DIVERGENCE,
                                          sev, loss->kl_divergence);
            }
        }
        bridge->stats.kl_anomalies++;
        anomalies_found++;
    }

    /* Check free energy */
    if (loss->free_energy > thresh->free_energy_threshold) {
        vae_anomaly_severity_t sev = determine_severity(
            loss->free_energy, thresh->free_energy_threshold);

        if (sev >= VAE_SEVERITY_MEDIUM) {
            if (check_rate_limit(bridge)) {
                vae_immune_report_anomaly(bridge, VAE_ANOMALY_FREE_ENERGY_SPIKE,
                                          sev, loss->free_energy);
            }
        }

        /* Send danger signal if critical */
        if (loss->free_energy > bridge->config.danger_signal_threshold &&
            bridge->config.enable_cytokine_signals) {
            vae_immune_send_danger_signal(bridge, loss->free_energy);
        }

        bridge->stats.free_energy_anomalies++;
        anomalies_found++;
    }

    /* Update statistics */
    update_ema(&bridge->stats.avg_reconstruction_error,
               loss->reconstruction_loss, VAE_IMMUNE_EMA_ALPHA);
    update_ema(&bridge->stats.avg_kl_divergence,
               loss->kl_divergence, VAE_IMMUNE_EMA_ALPHA);
    update_ema(&bridge->stats.avg_free_energy,
               loss->free_energy, VAE_IMMUNE_EMA_ALPHA);

    if (anomalies_found > 0) {
        bridge->stats.anomalies_detected += anomalies_found;
        bridge->stats.last_anomaly_us = get_timestamp_us();
        bridge->state = VAE_IMMUNE_STATE_ALERTING;
    }

    update_health(bridge, anomalies_found > 0);

    nimcp_mutex_unlock(bridge->mutex);

    return anomalies_found;
}

int vae_immune_check_latent(vae_immune_bridge_t* bridge,
                             const vae_latent_state_t* latent)
{
    if (!bridge || !latent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_check_latent: required parameter is NULL (bridge, latent)");
        return -1;
    }
    if (!bridge->config.enable_latent_monitoring) return 0;

    nimcp_mutex_lock(bridge->mutex);

    int anomalies_found = 0;
    const vae_immune_thresholds_t* thresh = &bridge->config.thresholds;

    /* Check for variance explosion */
    float max_var = 0.0f;
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        float var = expf(latent->log_var[i]);
        if (var > max_var) max_var = var;
    }

    if (max_var > thresh->variance_threshold) {
        vae_anomaly_severity_t sev = determine_severity(
            max_var, thresh->variance_threshold);

        if (check_rate_limit(bridge)) {
            vae_immune_report_anomaly(bridge, VAE_ANOMALY_VARIANCE_EXPLOSION,
                                      sev, max_var);
        }
        bridge->stats.variance_anomalies++;
        anomalies_found++;
    }

    /* Check for posterior collapse (all variances near 1, means near 0) */
    float avg_mu_abs = 0.0f;
    float avg_var = 0.0f;
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        avg_mu_abs += fabsf(latent->mu[i]);
        avg_var += expf(latent->log_var[i]);
    }
    avg_mu_abs /= latent->latent_dim;
    avg_var /= latent->latent_dim;

    /* Posterior collapse: means ~0, variance ~1 (like the prior) */
    if (avg_mu_abs < 0.1f && fabsf(avg_var - 1.0f) < 0.1f) {
        if (check_rate_limit(bridge)) {
            vae_immune_report_anomaly(bridge, VAE_ANOMALY_POSTERIOR_COLLAPSE,
                                      VAE_SEVERITY_HIGH, avg_mu_abs);
        }
        anomalies_found++;
    }

    /* Check for latent drift if baseline established */
    if (bridge->baseline_established && bridge->baseline_latent_mean) {
        float drift = 0.0f;
        for (uint32_t i = 0; i < latent->latent_dim; i++) {
            float diff = latent->mu[i] - bridge->baseline_latent_mean[i];
            drift += diff * diff;
        }
        drift = sqrtf(drift / latent->latent_dim);

        if (drift > thresh->latent_drift_threshold) {
            vae_anomaly_severity_t sev = determine_severity(
                drift, thresh->latent_drift_threshold);

            if (check_rate_limit(bridge)) {
                vae_immune_report_anomaly(bridge, VAE_ANOMALY_LATENT_DRIFT,
                                          sev, drift);
            }
            anomalies_found++;
        }
    }

    if (anomalies_found > 0) {
        bridge->stats.anomalies_detected += anomalies_found;
        bridge->stats.last_anomaly_us = get_timestamp_us();
    }

    update_health(bridge, anomalies_found > 0);

    nimcp_mutex_unlock(bridge->mutex);

    return anomalies_found;
}

int vae_immune_report_anomaly(vae_immune_bridge_t* bridge,
                               vae_anomaly_type_t type,
                               vae_anomaly_severity_t severity,
                               float value)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_report_anomaly: bridge is NULL");
        return -1;
    }

    /* Get appropriate threshold */
    float threshold = 0.0f;
    switch (type) {
        case VAE_ANOMALY_RECONSTRUCTION:
            threshold = bridge->config.thresholds.reconstruction_threshold;
            break;
        case VAE_ANOMALY_KL_DIVERGENCE:
            threshold = bridge->config.thresholds.kl_divergence_threshold;
            break;
        case VAE_ANOMALY_VARIANCE_EXPLOSION:
            threshold = bridge->config.thresholds.variance_threshold;
            break;
        case VAE_ANOMALY_FREE_ENERGY_SPIKE:
            threshold = bridge->config.thresholds.free_energy_threshold;
            break;
        case VAE_ANOMALY_OOD_SAMPLE:
            threshold = bridge->config.thresholds.ood_threshold;
            break;
        case VAE_ANOMALY_LATENT_DRIFT:
            threshold = bridge->config.thresholds.latent_drift_threshold;
            break;
        default:
            threshold = 1.0f;
    }

    /* Skip low severity if not configured to report */
    if (severity == VAE_SEVERITY_LOW && !bridge->config.report_low_severity) {
        record_anomaly(bridge, type, severity, value, threshold, false, 0);
        bridge->stats.anomalies_suppressed++;
        return 0;
    }

    /* Present to immune system */
    uint32_t antigen_id = 0;
    if (bridge->immune && severity >= VAE_SEVERITY_MEDIUM) {
        /* Create epitope from anomaly data */
        uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
        memset(epitope, 0, sizeof(epitope));

        /* Encode type and value into epitope */
        epitope[0] = (uint8_t)type;
        epitope[1] = (uint8_t)severity;
        memcpy(&epitope[2], &value, sizeof(float));
        memcpy(&epitope[6], &threshold, sizeof(float));

        antigen_id = vae_immune_present_antigen(bridge, type, severity,
                                                 epitope, 10);
    }

    record_anomaly(bridge, type, severity, value, threshold, antigen_id > 0, antigen_id);

    if (antigen_id > 0) {
        bridge->stats.anomalies_reported++;
        bridge->last_report_time_us = get_timestamp_us();
        bridge->reports_this_second++;
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOG_WARN("VAE-Immune: Anomaly %s (sev=%d, val=%.4f, thresh=%.4f)",
                       vae_anomaly_type_to_string(type), severity, value, threshold);
    }

    return 0;
}

bool vae_immune_check_ood(vae_immune_bridge_t* bridge,
                           const nimcp_tensor_t* latent,
                           float* ood_score)
{
    if (!bridge || !latent || !ood_score) {
        return false;
    }
    if (!bridge->baseline_established) {
        *ood_score = 0.0f;
        return false;
    }

    uint32_t dim = nimcp_tensor_numel(latent);
    const float* data = (const float*)latent->data;

    /* Compute Mahalanobis-like distance from baseline */
    float distance = 0.0f;
    for (uint32_t i = 0; i < dim && i < vae_get_latent_dim(bridge->vae); i++) {
        float diff = data[i] - bridge->baseline_latent_mean[i];
        float var = bridge->baseline_latent_var[i];
        if (var > 1e-6f) {
            distance += (diff * diff) / var;
        }
    }
    distance = sqrtf(distance / dim);

    *ood_score = distance;

    return distance > bridge->config.thresholds.ood_threshold;
}

/* ============================================================================
 * Immune Integration API
 * ============================================================================ */

uint32_t vae_immune_present_antigen(vae_immune_bridge_t* bridge,
                                     vae_anomaly_type_t type,
                                     vae_anomaly_severity_t severity,
                                     const uint8_t* epitope,
                                     size_t epitope_len)
{
    if (!bridge || !bridge->immune) return 0;

    /* Convert VAE severity to immune severity (1-10 scale) */
    uint32_t immune_severity = vae_severity_to_immune_severity(severity);

    /* Present antigen to immune system */
    uint32_t antigen_id = 0;
    int result = brain_immune_present_antigen(
        bridge->immune,
        ANTIGEN_SOURCE_ANOMALY,  /* Source: behavioral anomaly */
        epitope,
        epitope_len,
        immune_severity,
        0,                       /* source_node: 0 for VAE module */
        &antigen_id              /* Output antigen ID */
    );

    if (result == 0) {
        bridge->stats.antigens_presented++;
        return antigen_id > 0 ? antigen_id : 1;  /* Return antigen ID on success */
    }

    NIMCP_LOG_WARN("VAE-Immune: Failed to present antigen");
    return 0;
}

int vae_immune_send_danger_signal(vae_immune_bridge_t* bridge, float free_energy)
{
    if (!bridge || !bridge->immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_send_danger_signal: required parameter is NULL (bridge, bridge->immune)");
        return -1;
    }

    /* Calculate signal strength based on how much threshold is exceeded */
    float ratio = free_energy / bridge->config.danger_signal_threshold;
    float strength = fminf(1.0f, (ratio - 1.0f) * 0.5f + 0.5f);

    /* Release cytokine via immune system */
    uint32_t cytokine_id = 0;
    int result = brain_immune_release_cytokine(
        bridge->immune,
        BRAIN_CYTOKINE_IL6,  /* Pro-inflammatory, escalation */
        0,                    /* source_cell: 0 for VAE module */
        strength,
        0,                    /* target_region: 0 = broadcast */
        &cytokine_id          /* Output cytokine ID */
    );

    if (result == 0) {
        bridge->stats.cytokines_sent++;
        NIMCP_LOG_INFO("VAE-Immune: Danger signal sent (FE=%.2f, strength=%.2f)",
                       free_energy, strength);
    }

    return result;
}

int vae_immune_handle_response(vae_immune_bridge_t* bridge,
                                vae_immune_response_t response,
                                float strength)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_handle_response: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_immune_modulation) return 0;

    nimcp_mutex_lock(bridge->mutex);

    bridge->current_response = response;
    bridge->modulation_factor = strength * bridge->config.modulation_strength;
    bridge->stats.immune_responses++;

    switch (response) {
        case VAE_IMMUNE_RESPONSE_ADAPT:
            /* Could adjust VAE learning rate, beta, etc. */
            NIMCP_LOG_INFO("VAE-Immune: Adaptation response (strength=%.2f)", strength);
            break;

        case VAE_IMMUNE_RESPONSE_SUPPRESS:
            bridge->state = VAE_IMMUNE_STATE_SUPPRESSED;
            bridge->health.under_modulation = true;
            NIMCP_LOG_INFO("VAE-Immune: Suppression response (strength=%.2f)", strength);
            break;

        case VAE_IMMUNE_RESPONSE_ISOLATE:
            /* Could disable certain latent dimensions */
            NIMCP_LOG_INFO("VAE-Immune: Isolation response (strength=%.2f)", strength);
            break;

        case VAE_IMMUNE_RESPONSE_RESET:
            /* Request VAE reset through immune coordination */
            NIMCP_LOG_WARN("VAE-Immune: Reset response requested (strength=%.2f)", strength);
            break;

        default:
            break;
    }

    bridge->stats.modulations_applied++;

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Threshold API
 * ============================================================================ */

int vae_immune_set_thresholds(vae_immune_bridge_t* bridge,
                               const vae_immune_thresholds_t* thresholds)
{
    if (!bridge || !thresholds) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_set_thresholds: required parameter is NULL (bridge, thresholds)");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);
    bridge->config.thresholds = *thresholds;
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int vae_immune_get_thresholds(const vae_immune_bridge_t* bridge,
                               vae_immune_thresholds_t* thresholds)
{
    if (!bridge || !thresholds) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_get_thresholds: required parameter is NULL (bridge, thresholds)");
        return -1;
    }
    *thresholds = bridge->config.thresholds;
    return 0;
}

int vae_immune_adapt_thresholds(vae_immune_bridge_t* bridge, float num_std_devs)
{
    if (!bridge || !bridge->baseline_established) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_adapt_thresholds: required parameter is NULL (bridge, bridge->baseline_established)");
        return -1;
    }

    /* Adapt based on baseline statistics - placeholder implementation */
    /* Full implementation would use running mean/std of metrics */

    return 0;
}

/* ============================================================================
 * Baseline API
 * ============================================================================ */

int vae_immune_update_baseline(vae_immune_bridge_t* bridge,
                                const vae_latent_state_t* latent)
{
    if (!bridge || !latent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_update_baseline: required parameter is NULL (bridge, latent)");
        return -1;
    }
    if (!bridge->baseline_latent_mean || !bridge->baseline_latent_var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_update_baseline: required parameter is NULL (bridge->baseline_latent_mean, bridge->baseline_latent_var)");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    uint32_t dim = latent->latent_dim;
    float n = (float)(bridge->baseline_samples + 1);

    /* Online update of mean and variance */
    for (uint32_t i = 0; i < dim; i++) {
        float old_mean = bridge->baseline_latent_mean[i];
        float new_mean = old_mean + (latent->mu[i] - old_mean) / (fabsf(n) > 1e-7f ? n : 1e-7f);
        bridge->baseline_latent_mean[i] = new_mean;

        /* Welford's online algorithm for variance */
        if (bridge->baseline_samples > 0) {
            float delta = latent->mu[i] - old_mean;
            float delta2 = latent->mu[i] - new_mean;
            bridge->baseline_latent_var[i] += delta * delta2;
        }
    }

    bridge->baseline_samples++;

    /* Mark baseline as established after minimum samples */
    if (bridge->baseline_samples >= VAE_IMMUNE_BASELINE_MIN_SAMPLES) {
        bridge->baseline_established = true;

        /* Finalize variance calculation */
        if (bridge->baseline_samples == VAE_IMMUNE_BASELINE_MIN_SAMPLES) {
            for (uint32_t i = 0; i < dim; i++) {
                bridge->baseline_latent_var[i] /= (bridge->baseline_samples - 1);
            }
        }
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int vae_immune_reset_baseline(vae_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_reset_baseline: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->baseline_established = false;
    bridge->baseline_samples = 0;

    if (bridge->baseline_latent_mean) {
        uint32_t dim = bridge->vae ? vae_get_latent_dim(bridge->vae) : 0;
        memset(bridge->baseline_latent_mean, 0, dim * sizeof(float));
    }
    if (bridge->baseline_latent_var) {
        uint32_t dim = bridge->vae ? vae_get_latent_dim(bridge->vae) : 0;
        memset(bridge->baseline_latent_var, 0, dim * sizeof(float));
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

bool vae_immune_has_baseline(const vae_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->baseline_established;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int vae_immune_bridge_get_stats(const vae_immune_bridge_t* bridge,
                                 vae_immune_bridge_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    stats->uptime_us = get_timestamp_us() - bridge->creation_time_us;
    return 0;
}

int vae_immune_bridge_get_health(const vae_immune_bridge_t* bridge,
                                  vae_immune_bridge_health_t* health)
{
    if (!bridge || !health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_bridge_get_health: required parameter is NULL (bridge, health)");
        return -1;
    }
    *health = bridge->health;
    return 0;
}

vae_immune_bridge_state_t vae_immune_bridge_get_state(const vae_immune_bridge_t* bridge)
{
    if (!bridge) return VAE_IMMUNE_STATE_DISCONNECTED;
    return bridge->state;
}

int vae_immune_get_anomaly_history(const vae_immune_bridge_t* bridge,
                                    vae_anomaly_record_t* records,
                                    uint32_t max_records)
{
    if (!bridge || !records || max_records == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_get_anomaly_history: required parameter is NULL (bridge, records)");
        return -1;
    }

    uint32_t count = (max_records < bridge->history_count) ?
                      max_records : bridge->history_count;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (bridge->history_index - count + i + VAE_IMMUNE_ANOMALY_HISTORY_SIZE)
                       % VAE_IMMUNE_ANOMALY_HISTORY_SIZE;
        records[i] = bridge->anomaly_history[idx];
    }

    return count;
}

uint32_t vae_immune_get_recent_anomaly_count(const vae_immune_bridge_t* bridge,
                                              uint64_t window_ms)
{
    if (!bridge || !bridge->anomaly_history) return 0;

    uint64_t now = get_timestamp_us();
    uint64_t window_us = window_ms * 1000ULL;
    uint32_t count = 0;

    for (uint32_t i = 0; i < bridge->history_count; i++) {
        if (now - bridge->anomaly_history[i].timestamp_us <= window_us) {
            count++;
        }
    }

    return count;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int vae_immune_bridge_update(vae_immune_bridge_t* bridge, uint64_t delta_ms)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_bridge_update: required parameter is NULL (bridge, bridge->is_initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Update statistics timing */
    bridge->stats.uptime_us = get_timestamp_us() - bridge->creation_time_us;

    /* Decay modulation over time */
    if (bridge->modulation_factor > 0.0f) {
        bridge->modulation_factor -= 0.001f * delta_ms;
        if (bridge->modulation_factor <= 0.0f) {
            bridge->modulation_factor = 0.0f;
            bridge->current_response = VAE_IMMUNE_RESPONSE_NONE;
            bridge->health.under_modulation = false;

            if (bridge->state == VAE_IMMUNE_STATE_SUPPRESSED) {
                bridge->state = VAE_IMMUNE_STATE_CONNECTED;
            }
        }
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int vae_immune_process_responses(vae_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_immune_process_responses: required parameter is NULL (bridge, bridge->immune)");
        return -1;
    }

    /* Check for pending immune responses - placeholder */
    /* Full implementation would query immune system for VAE-relevant responses */

    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* vae_anomaly_type_to_string(vae_anomaly_type_t type)
{
    switch (type) {
        case VAE_ANOMALY_RECONSTRUCTION:    return "RECONSTRUCTION_ERROR";
        case VAE_ANOMALY_KL_DIVERGENCE:     return "KL_DIVERGENCE";
        case VAE_ANOMALY_VARIANCE_EXPLOSION: return "VARIANCE_EXPLOSION";
        case VAE_ANOMALY_POSTERIOR_COLLAPSE: return "POSTERIOR_COLLAPSE";
        case VAE_ANOMALY_OOD_SAMPLE:        return "OOD_SAMPLE";
        case VAE_ANOMALY_GRADIENT_NAN:      return "GRADIENT_NAN";
        case VAE_ANOMALY_FREE_ENERGY_SPIKE: return "FREE_ENERGY_SPIKE";
        case VAE_ANOMALY_LATENT_DRIFT:      return "LATENT_DRIFT";
        default:                            return "UNKNOWN";
    }
}

uint32_t vae_severity_to_immune_severity(vae_anomaly_severity_t severity)
{
    switch (severity) {
        case VAE_SEVERITY_NONE:     return 1;
        case VAE_SEVERITY_LOW:      return 3;
        case VAE_SEVERITY_MEDIUM:   return 5;
        case VAE_SEVERITY_HIGH:     return 8;
        case VAE_SEVERITY_CRITICAL: return 10;
        default:                    return 5;
    }
}
