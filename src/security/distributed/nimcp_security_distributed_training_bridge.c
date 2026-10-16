/**
 * @file nimcp_security_distributed_training_bridge.c
 * @brief Security-Distributed Training Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bidirectional integration between security systems and distributed training
 * WHY:  Protect distributed ML training from Byzantine workers and gradient poisoning
 * HOW:  Validate workers, detect Byzantine behavior, score trust, secure checkpoints
 *
 * @author NIMCP Development Team
 */

#include "security/distributed/nimcp_security_distributed_training_bridge.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "security_distributed_training_bridge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_distributed_training_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Statistics Tracking
 * ============================================================================ */

typedef struct {
    uint64_t total_workers_registered;
    uint64_t workers_quarantined;
    uint64_t workers_removed;
    uint64_t byzantine_checks;
    uint64_t byzantine_detections;
    uint64_t byzantine_by_type[SECURITY_BYZANTINE_COUNT];
    uint64_t gradients_validated;
    uint64_t gradients_rejected;
    uint64_t gradients_suspicious;
    float total_gradient_norm;
    float max_gradient_norm;
    uint64_t trust_updates;
    uint64_t trust_penalties;
    uint64_t trust_recoveries;
    uint64_t checkpoints_created;
    uint64_t checkpoints_verified;
    uint64_t checkpoints_failed;
    uint64_t consensus_rounds;
    uint64_t total_rounds;
    uint64_t total_updates;
    float total_byzantine_check_time_us;
    float total_validation_time_us;
} internal_stats_t;

static internal_stats_t* get_internal_stats(security_distributed_training_bridge_t* bridge) {
    return (internal_stats_t*)((char*)bridge +
        sizeof(security_distributed_training_bridge_t));
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static void compute_model_hash(
    const float* weights,
    uint32_t num_weights,
    uint8_t* hash_out)
{
    if (!weights || !hash_out || num_weights == 0) {
        memset(hash_out, 0, SECURITY_DISTRIBUTED_HASH_SIZE);
        return;
    }

    uint64_t state[4] = {0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
                         0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL};
    const uint8_t* data = (const uint8_t*)weights;
    size_t len = num_weights * sizeof(float);

    for (size_t i = 0; i < len; i++) {
        state[i % 4] ^= ((uint64_t)data[i] << ((i % 8) * 8));
        state[i % 4] = (state[i % 4] << 7) | (state[i % 4] >> 57);
        state[(i + 1) % 4] += state[i % 4];
    }

    for (int r = 0; r < 4; r++) {
        state[0] ^= state[2];
        state[1] ^= state[3];
        state[0] = (state[0] << 13) | (state[0] >> 51);
        state[1] = (state[1] << 17) | (state[1] >> 47);
    }

    memcpy(hash_out, state, SECURITY_DISTRIBUTED_HASH_SIZE);
}

static bool hash_equals(const uint8_t* h1, const uint8_t* h2) {
    if (!h1 || !h2) {
        return false;
    }
    return memcmp(h1, h2, SECURITY_DISTRIBUTED_HASH_SIZE) == 0;
}

static int find_worker(
    const security_distributed_training_bridge_t* bridge,
    const char* worker_id)
{
    if (!bridge || !worker_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_worker: required parameter is NULL (bridge, worker_id)");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->num_workers; i++) {
        if (strcmp(bridge->workers[i].worker_id, worker_id) == 0) {
            return (int)i;
        }
    }
    /* Worker not found - normal case during registration checks, not an error */
    return -1;
}

static int find_checkpoint(
    const security_distributed_training_bridge_t* bridge,
    const char* checkpoint_name)
{
    if (!bridge || !checkpoint_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_checkpoint: required parameter is NULL (bridge, checkpoint_name)");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->num_checkpoints; i++) {
        if (strcmp(bridge->checkpoints[i].name, checkpoint_name) == 0) {
            return (int)i;
        }
    }
    /* Checkpoint not found - normal case during first creation, not an error */
    return -1;
}

static float compute_gradient_norm(const float* gradients, uint32_t num_params) {
    if (!gradients || num_params == 0) return 0.0f;

    double sum_sq = 0.0;
    for (uint32_t i = 0; i < num_params; i++) {
        sum_sq += (double)gradients[i] * (double)gradients[i];
    }
    return (float)sqrt(sum_sq);
}

static float compute_trust_weight(security_worker_trust_t trust) {
    switch (trust) {
        case SECURITY_WORKER_TRUST_QUARANTINED: return 0.0f;
        case SECURITY_WORKER_TRUST_UNTRUSTED:   return 0.1f;
        case SECURITY_WORKER_TRUST_PROBATION:   return 0.3f;
        case SECURITY_WORKER_TRUST_VERIFIED:    return 0.7f;
        case SECURITY_WORKER_TRUST_TRUSTED:     return 1.0f;
        default: return 0.0f;
    }
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int security_distributed_training_default_config(
    security_distributed_training_config_t* config)
{
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(security_distributed_training_config_t));

    config->enable_byzantine_detection = true;
    config->enable_gradient_validation = true;
    config->enable_worker_trust_scoring = true;
    config->enable_secure_checkpointing = true;
    config->enable_secure_aggregation = false;

    config->byzantine_threshold = SECURITY_DISTRIBUTED_DEFAULT_BYZANTINE_THRESHOLD;
    config->anomaly_threshold = SECURITY_DISTRIBUTED_DEFAULT_ANOMALY_THRESHOLD;
    config->consistency_threshold = SECURITY_DISTRIBUTED_DEFAULT_CONSISTENCY_THRESHOLD;
    config->min_workers_for_detection = 3;

    config->gradient_norm_threshold = SECURITY_DISTRIBUTED_DEFAULT_GRAD_NORM_THRESHOLD;
    config->gradient_variance_threshold = SECURITY_DISTRIBUTED_DEFAULT_GRAD_VARIANCE_THRESHOLD;
    config->gradient_outlier_threshold = SECURITY_DISTRIBUTED_DEFAULT_GRAD_OUTLIER_THRESHOLD;
    config->aggregation_method = SECURITY_GRAD_AGG_TRIMMED_MEAN;

    config->min_worker_trust = SECURITY_WORKER_TRUST_PROBATION;
    config->trust_decay_rate = SECURITY_DISTRIBUTED_DEFAULT_TRUST_DECAY_RATE;
    config->trust_recovery_rate = SECURITY_DISTRIBUTED_DEFAULT_TRUST_RECOVERY_RATE;
    config->trust_violation_penalty = SECURITY_DISTRIBUTED_DEFAULT_TRUST_VIOLATION_PENALTY;
    config->auto_quarantine_byzantine = true;

    config->enable_differential_privacy = false;
    config->dp_epsilon = 1.0f;
    config->dp_delta = 1e-5f;

    strncpy(config->checkpoint_directory, "/tmp/nimcp_dist_checkpoints",
            sizeof(config->checkpoint_directory) - 1);
    config->checkpoint_consensus_quorum = 2;
    config->sign_checkpoints = true;

    config->enable_bio_async = true;
    config->bio_inbox_capacity = SECURITY_DISTRIBUTED_BIO_INBOX_CAPACITY;

    config->enable_logging = true;
    config->log_all_validations = false;

    return 0;
}

security_distributed_training_bridge_t* security_distributed_training_bridge_create(
    const security_distributed_training_config_t* config)
{
    size_t alloc_size = sizeof(security_distributed_training_bridge_t) +
                        sizeof(internal_stats_t);
    security_distributed_training_bridge_t* bridge = nimcp_malloc(alloc_size);
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_distributed_training_bridge_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate security-distributed training bridge");
        return NULL;
    }
    memset(bridge, 0, alloc_size);

    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY,
                         SECURITY_DISTRIBUTED_MODULE_NAME) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_distributed_training_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        security_distributed_training_default_config(&bridge->config);
    }

    bridge->phase = SECURITY_DISTRIBUTED_PHASE_MONITORING;
    bridge->creation_time_ms = nimcp_time_monotonic_ms();
    bridge->last_update_ms = bridge->creation_time_ms;

    bridge->security_effects.min_trust_level = bridge->config.min_worker_trust;
    bridge->security_effects.gradient_norm_limit = bridge->config.gradient_norm_threshold;
    bridge->security_effects.gradient_scale_factor = 1.0f;
    bridge->security_effects.required_aggregation = bridge->config.aggregation_method;
    bridge->security_effects.valid = true;
    bridge->security_effects.last_update_ms = bridge->creation_time_ms;

    bridge->training_effects.valid = false;

    if (bridge->config.enable_bio_async) {
        security_distributed_training_connect_bio_async(bridge);
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-distributed training bridge created");
    }

    return bridge;
}

void security_distributed_training_bridge_destroy(
    security_distributed_training_bridge_t* bridge)
{
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        security_distributed_training_disconnect_bio_async(bridge);
    }

    if (bridge->round_gradient_norms) {
        nimcp_free(bridge->round_gradient_norms);
        bridge->round_gradient_norms = NULL;
    }

    if (bridge->training_effects.anomalous_worker_indices) {
        nimcp_free(bridge->training_effects.anomalous_worker_indices);
    }
    if (bridge->training_effects.worker_anomaly_scores) {
        nimcp_free(bridge->training_effects.worker_anomaly_scores);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int security_distributed_training_connect_coordinator(
    security_distributed_training_bridge_t* bridge,
    distributed_coordinator_t coordinator)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(coordinator);

    BRIDGE_LOCK(bridge);
    bridge->coordinator = coordinator;
    bridge->coordinator_connected = true;
    bridge->phase = SECURITY_DISTRIBUTED_PHASE_PROTECTING;
    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Connected to distributed coordinator");
    }
    return 0;
}

int security_distributed_training_connect_bbb(
    security_distributed_training_bridge_t* bridge,
    bbb_system_t bbb)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(bbb);

    BRIDGE_LOCK(bridge);
    bridge->bbb = bbb;
    bridge->bbb_connected = true;
    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Connected to Blood-Brain Barrier");
    }
    return 0;
}

int security_distributed_training_disconnect_coordinator(
    security_distributed_training_bridge_t* bridge)
{
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->coordinator = NULL;
    bridge->coordinator_connected = false;
    if (!bridge->bbb_connected) {
        bridge->phase = SECURITY_DISTRIBUTED_PHASE_MONITORING;
    }
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_distributed_training_disconnect_bbb(
    security_distributed_training_bridge_t* bridge)
{
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->bbb = NULL;
    bridge->bbb_connected = false;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Worker Management API Implementation
 * ============================================================================ */

int security_distributed_training_register_worker(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    const uint8_t* worker_key)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(worker_id);

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    int idx = find_worker(bridge, worker_id);
    if (idx >= 0) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    if (bridge->num_workers >= SECURITY_DISTRIBUTED_MAX_WORKERS) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    idx = (int)bridge->num_workers;
    security_worker_info_t* worker = &bridge->workers[idx];

    memset(worker, 0, sizeof(security_worker_info_t));
    strncpy(worker->worker_id, worker_id, SECURITY_DISTRIBUTED_WORKER_ID_MAX - 1);
    if (worker_key) {
        memcpy(worker->worker_key, worker_key, SECURITY_DISTRIBUTED_WORKER_KEY_SIZE);
    }

    worker->trust_level = SECURITY_WORKER_TRUST_UNTRUSTED;
    worker->trust_score = 0.5f;
    worker->is_active = true;
    worker->registration_time_ms = nimcp_time_monotonic_ms();
    worker->last_activity_ms = worker->registration_time_ms;

    bridge->num_workers++;
    bridge->security_effects.active_worker_count++;
    stats->total_workers_registered++;

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Registered worker '%s'", worker_id);
    }

    return 0;
}

int security_distributed_training_unregister_worker(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(worker_id);

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    int idx = find_worker(bridge, worker_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    bridge->workers[idx].pending_removal = true;
    bridge->workers[idx].is_active = false;
    bridge->security_effects.active_worker_count--;

    BRIDGE_UNLOCK(bridge);

    stats->workers_removed++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Unregistered worker '%s'", worker_id);
    }

    return 0;
}

int security_distributed_training_get_worker_info(
    const security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    security_worker_info_t* info)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(worker_id);
    BRIDGE_NULL_CHECK(info);

    int idx = find_worker(bridge, worker_id);
    if (idx < 0) return NIMCP_ERROR_NOT_FOUND;

    *info = bridge->workers[idx];
    return 0;
}

security_worker_trust_t security_distributed_training_get_worker_trust(
    const security_distributed_training_bridge_t* bridge,
    const char* worker_id)
{
    if (!bridge || !worker_id) return SECURITY_WORKER_TRUST_QUARANTINED;

    int idx = find_worker(bridge, worker_id);
    if (idx < 0) return SECURITY_WORKER_TRUST_QUARANTINED;

    return bridge->workers[idx].trust_level;
}

int security_distributed_training_set_worker_trust(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    security_worker_trust_t trust_level)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(worker_id);

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    int idx = find_worker(bridge, worker_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    security_worker_trust_t old_trust = bridge->workers[idx].trust_level;
    bridge->workers[idx].trust_level = trust_level;
    bridge->workers[idx].trust_score = compute_trust_weight(trust_level);

    if (trust_level == SECURITY_WORKER_TRUST_QUARANTINED) {
        bridge->workers[idx].is_quarantined = true;
        bridge->workers[idx].quarantine_time_ms = nimcp_time_monotonic_ms();
        bridge->security_effects.quarantined_worker_count++;
        bridge->security_effects.active_worker_count--;
    } else if (old_trust == SECURITY_WORKER_TRUST_QUARANTINED) {
        bridge->workers[idx].is_quarantined = false;
        bridge->security_effects.quarantined_worker_count--;
        bridge->security_effects.active_worker_count++;
    }

    BRIDGE_UNLOCK(bridge);

    stats->trust_updates++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Set trust for '%s' to %s",
                         worker_id, security_worker_trust_to_string(trust_level));
    }

    return 0;
}

int security_distributed_training_score_worker(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    float* score)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(worker_id);
    BRIDGE_NULL_CHECK(score);

    BRIDGE_LOCK(bridge);

    int idx = find_worker(bridge, worker_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        *score = 0.0f;
        return NIMCP_ERROR_NOT_FOUND;
    }

    security_worker_info_t* worker = &bridge->workers[idx];
    float base_score = worker->trust_score;
    float contribution_factor = worker->contribution_score;
    float violation_factor = 1.0f;

    if (worker->violations_count > 0) {
        violation_factor = 1.0f / (1.0f + (float)worker->violations_count * 0.1f);
    }

    float final_score = base_score * contribution_factor * violation_factor;
    final_score = nimcp_clampf(final_score, 0.0f, 1.0f);

    worker->trust_score = final_score;
    *score = final_score;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_distributed_training_quarantine_worker(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    const char* reason)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(worker_id);

    internal_stats_t* stats = get_internal_stats(bridge);

    int result = security_distributed_training_set_worker_trust(
        bridge, worker_id, SECURITY_WORKER_TRUST_QUARANTINED);

    if (result == 0) {
        stats->workers_quarantined++;
        NIMCP_LOGGING_WARN("Quarantined worker '%s': %s",
                         worker_id, reason ? reason : "unspecified");
    }

    return result;
}

int security_distributed_training_release_worker(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(worker_id);

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    int idx = find_worker(bridge, worker_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (!bridge->workers[idx].is_quarantined) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_INVALID_STATE;
    }

    bridge->workers[idx].trust_level = SECURITY_WORKER_TRUST_PROBATION;
    bridge->workers[idx].trust_score = 0.3f;
    bridge->workers[idx].is_quarantined = false;
    bridge->security_effects.quarantined_worker_count--;
    bridge->security_effects.active_worker_count++;

    BRIDGE_UNLOCK(bridge);

    stats->trust_recoveries++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Released worker '%s' from quarantine", worker_id);
    }

    return 0;
}

int security_distributed_training_list_workers(
    const security_distributed_training_bridge_t* bridge,
    security_worker_info_t* workers,
    uint32_t max_workers)
{
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    uint32_t count = bridge->num_workers;
    if (count > max_workers) count = max_workers;

    if (workers && count > 0) {
        memcpy(workers, bridge->workers, count * sizeof(security_worker_info_t));
    }

    return (int)count;
}

/* ============================================================================
 * Byzantine Detection API Implementation
 * ============================================================================ */

int security_distributed_training_detect_byzantine(
    security_distributed_training_bridge_t* bridge,
    security_byzantine_result_t* result)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    if (!bridge->config.enable_byzantine_detection) {
        result->byzantine_detected = false;
        result->type = SECURITY_BYZANTINE_NONE;
        return 0;
    }

    uint64_t start_time = nimcp_time_monotonic_us();
    internal_stats_t* stats = get_internal_stats(bridge);

    memset(result, 0, sizeof(security_byzantine_result_t));

    BRIDGE_LOCK(bridge);

    uint32_t active_workers = 0;
    for (uint32_t i = 0; i < bridge->num_workers; i++) {
        if (bridge->workers[i].is_active && !bridge->workers[i].is_quarantined) {
            active_workers++;
        }
    }

    if (active_workers < bridge->config.min_workers_for_detection) {
        BRIDGE_UNLOCK(bridge);
        result->byzantine_detected = false;
        return 0;
    }

    float max_anomaly_score = 0.0f;
    security_byzantine_type_t detected_type = SECURITY_BYZANTINE_NONE;
    uint32_t byzantine_count = 0;

    for (uint32_t i = 0; i < bridge->num_workers; i++) {
        security_worker_info_t* worker = &bridge->workers[i];
        if (!worker->is_active) continue;

        /* Quarantined workers are already confirmed Byzantine */
        if (worker->is_quarantined) {
            byzantine_count++;
            if (max_anomaly_score < 1.0f) {
                max_anomaly_score = 1.0f;
                detected_type = SECURITY_BYZANTINE_GRADIENT_ATTACK;
            }
            continue;
        }

        float anomaly_score = 0.0f;

        if (worker->gradients_submitted > 0) {
            float reject_ratio = (float)worker->gradients_rejected /
                                (float)worker->gradients_submitted;
            if (reject_ratio > 0.5f) {
                anomaly_score += reject_ratio;
                detected_type = SECURITY_BYZANTINE_GRADIENT_ATTACK;
            }
        }

        if (worker->rounds_participated > 10 && worker->gradients_submitted == 0) {
            anomaly_score += 0.8f;
            detected_type = SECURITY_BYZANTINE_FREE_RIDER;
        }

        if (worker->byzantine_detections > 2) {
            anomaly_score += 0.3f * (float)worker->byzantine_detections;
        }

        if (anomaly_score > bridge->config.byzantine_threshold) {
            byzantine_count++;
            if (anomaly_score > max_anomaly_score) {
                max_anomaly_score = anomaly_score;
            }
        }
    }

    float byzantine_ratio = (float)byzantine_count / (float)active_workers;
    bridge->security_effects.byzantine_ratio = byzantine_ratio;

    BRIDGE_UNLOCK(bridge);

    result->byzantine_detected = (byzantine_count > 0);
    result->type = detected_type;
    result->confidence = nimcp_clampf(max_anomaly_score, 0.0f, 1.0f);
    result->num_byzantine = byzantine_count;
    result->detection_time_us = nimcp_time_monotonic_us() - start_time;

    if (result->byzantine_detected) {
        snprintf(result->description, sizeof(result->description),
                 "Detected %u Byzantine workers (%s)",
                 byzantine_count, security_byzantine_type_to_string(detected_type));
        result->quarantine_recommended = true;
        result->halt_training_recommended = (byzantine_ratio > 0.3f);
        result->rollback_recommended = (byzantine_ratio > 0.5f);
    }

    stats->byzantine_checks++;
    stats->total_byzantine_check_time_us += (float)result->detection_time_us;
    if (result->byzantine_detected) {
        stats->byzantine_detections++;
        if (detected_type < SECURITY_BYZANTINE_COUNT) {
            stats->byzantine_by_type[detected_type]++;
        }
    }

    bridge->last_byzantine_check_ms = nimcp_time_monotonic_ms();

    return 0;
}

int security_distributed_training_report_worker_anomaly(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    float anomaly_score,
    const char* reason)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(worker_id);

    BRIDGE_LOCK(bridge);

    int idx = find_worker(bridge, worker_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    security_worker_info_t* worker = &bridge->workers[idx];
    worker->violations_count++;
    worker->last_violation_ms = nimcp_time_monotonic_ms();

    if (anomaly_score > bridge->config.anomaly_threshold) {
        worker->byzantine_detections++;
    }

    worker->trust_score -= bridge->config.trust_violation_penalty * anomaly_score;
    worker->trust_score = nimcp_clampf(worker->trust_score, 0.0f, 1.0f);

    if (bridge->config.auto_quarantine_byzantine &&
        worker->byzantine_detections >= 3 &&
        !worker->is_quarantined) {
        worker->trust_level = SECURITY_WORKER_TRUST_QUARANTINED;
        worker->is_quarantined = true;
        worker->quarantine_time_ms = nimcp_time_monotonic_ms();
        bridge->security_effects.quarantined_worker_count++;
        bridge->security_effects.active_worker_count--;

        internal_stats_t* istats = get_internal_stats(bridge);
        istats->workers_quarantined++;
    }

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging && anomaly_score > 0.5f) {
        NIMCP_LOGGING_WARN("Worker '%s' anomaly (score: %.2f): %s",
                         worker_id, anomaly_score, reason ? reason : "unknown");
    }

    return 0;
}

/* ============================================================================
 * Gradient Validation API Implementation
 * ============================================================================ */

int security_distributed_training_validate_gradients(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    const float* gradients,
    uint32_t num_params,
    security_gradient_validation_result_t* result)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(worker_id);
    BRIDGE_NULL_CHECK(result);

    if (!bridge->config.enable_gradient_validation) {
        result->is_valid = true;
        result->is_suspicious = false;
        return 0;
    }

    uint64_t start_time = nimcp_time_monotonic_us();
    internal_stats_t* stats = get_internal_stats(bridge);

    memset(result, 0, sizeof(security_gradient_validation_result_t));

    if (!gradients || num_params == 0) {
        result->is_valid = false;
        result->reject_recommended = true;
        return 0;
    }

    BRIDGE_LOCK(bridge);

    int idx = find_worker(bridge, worker_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        result->is_valid = false;
        result->reject_recommended = true;
        return NIMCP_ERROR_NOT_FOUND;
    }

    security_worker_info_t* worker = &bridge->workers[idx];

    if (worker->is_quarantined ||
        worker->trust_level < bridge->config.min_worker_trust) {
        BRIDGE_UNLOCK(bridge);
        result->is_valid = false;
        result->reject_recommended = true;
        return 0;
    }

    float gradient_norm = compute_gradient_norm(gradients, num_params);
    result->gradient_norm = gradient_norm;

    for (uint32_t i = 0; i < num_params; i++) {
        if (isnan(gradients[i]) || isinf(gradients[i])) {
            result->nan_inf_detected = true;
            result->is_valid = false;
            result->reject_recommended = true;
            worker->gradients_rejected++;
            BRIDGE_UNLOCK(bridge);
            stats->gradients_validated++;
            stats->gradients_rejected++;
            return 0;
        }
    }

    if (gradient_norm > bridge->config.gradient_norm_threshold) {
        result->norm_exceeded = true;
        result->is_suspicious = true;
    }

    if (worker->gradients_submitted > 10) {
        float deviation = fabsf(gradient_norm - worker->avg_gradient_norm);
        result->norm_deviation = deviation;
        if (deviation > bridge->config.gradient_outlier_threshold *
            sqrtf(worker->gradient_norm_variance)) {
            result->outlier_detected = true;
            result->is_suspicious = true;
        }
    }

    result->anomaly_score = 0.0f;
    if (result->norm_exceeded) result->anomaly_score += 0.3f;
    if (result->outlier_detected) result->anomaly_score += 0.3f;
    result->anomaly_score = nimcp_clampf(result->anomaly_score, 0.0f, 1.0f);

    result->is_valid = !result->norm_exceeded || result->anomaly_score < 0.5f;

    if (result->is_valid) {
        float alpha = 0.1f;
        worker->avg_gradient_norm = alpha * gradient_norm +
                                   (1.0f - alpha) * worker->avg_gradient_norm;
        float diff = gradient_norm - worker->avg_gradient_norm;
        worker->gradient_norm_variance = alpha * (diff * diff) +
                                        (1.0f - alpha) * worker->gradient_norm_variance;
        worker->gradients_submitted++;
        worker->last_activity_ms = nimcp_time_monotonic_ms();
    } else {
        worker->gradients_rejected++;
        result->reject_recommended = true;
    }

    if (result->is_suspicious && result->anomaly_score > bridge->config.anomaly_threshold) {
        result->trust_penalty_recommended = true;
        result->quarantine_recommended = (result->anomaly_score > 0.9f);
    }

    BRIDGE_UNLOCK(bridge);

    stats->gradients_validated++;
    stats->total_validation_time_us += (float)(nimcp_time_monotonic_us() - start_time);
    if (!result->is_valid) stats->gradients_rejected++;
    if (result->is_suspicious) stats->gradients_suspicious++;
    stats->total_gradient_norm += gradient_norm;
    if (gradient_norm > stats->max_gradient_norm) {
        stats->max_gradient_norm = gradient_norm;
    }

    return 0;
}

bool security_distributed_training_validate_aggregated(
    security_distributed_training_bridge_t* bridge,
    const float* aggregated_gradients,
    uint32_t num_params,
    float* anomaly_score)
{
    BRIDGE_NULL_CHECK_BOOL(bridge);
    BRIDGE_NULL_CHECK_BOOL(aggregated_gradients);

    if (num_params == 0) {
        if (anomaly_score) *anomaly_score = 0.0f;
        return false;
    }

    float norm = compute_gradient_norm(aggregated_gradients, num_params);

    if (isnan(norm) || isinf(norm)) {
        if (anomaly_score) *anomaly_score = 1.0f;
        return true;
    }

    float threshold = bridge->config.gradient_norm_threshold * 2.0f;
    float score = norm / threshold;
    score = nimcp_clampf(score, 0.0f, 1.0f);

    if (anomaly_score) *anomaly_score = score;

    return (score > bridge->config.anomaly_threshold);
}

security_grad_aggregation_t security_distributed_training_get_aggregation_method(
    const security_distributed_training_bridge_t* bridge)
{
    if (!bridge) return SECURITY_GRAD_AGG_SIMPLE_AVERAGE;

    float threat_level = bridge->security_effects.threat_level;
    float byzantine_ratio = bridge->security_effects.byzantine_ratio;

    if (byzantine_ratio > 0.2f || threat_level > 0.7f) {
        return SECURITY_GRAD_AGG_KRUM;
    } else if (byzantine_ratio > 0.1f || threat_level > 0.5f) {
        return SECURITY_GRAD_AGG_BULYAN;
    } else if (byzantine_ratio > 0.05f || threat_level > 0.3f) {
        return SECURITY_GRAD_AGG_TRIMMED_MEAN;
    } else {
        return bridge->config.aggregation_method;
    }
}

/* ============================================================================
 * Secure Checkpointing API Implementation
 * ============================================================================ */

int security_distributed_training_secure_checkpoint(
    security_distributed_training_bridge_t* bridge,
    const char* checkpoint_name,
    const float* model_weights,
    uint32_t num_weights,
    uint64_t round)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(checkpoint_name);
    BRIDGE_NULL_CHECK(model_weights);

    if (num_weights == 0) return NIMCP_ERROR_INVALID_PARAM;

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    int idx = find_checkpoint(bridge, checkpoint_name);
    if (idx < 0) {
        if (bridge->num_checkpoints >= SECURITY_DISTRIBUTED_MAX_CHECKPOINTS) {
            idx = 0;
            for (uint32_t i = 1; i < bridge->num_checkpoints; i++) {
                if (bridge->checkpoints[i].timestamp_ms <
                    bridge->checkpoints[idx].timestamp_ms) {
                    idx = (int)i;
                }
            }
        } else {
            idx = (int)bridge->num_checkpoints;
            bridge->num_checkpoints++;
        }
    }

    security_distributed_checkpoint_t* cp = &bridge->checkpoints[idx];
    strncpy(cp->name, checkpoint_name, SECURITY_DISTRIBUTED_CHECKPOINT_NAME_MAX - 1);
    cp->round = round;
    cp->timestamp_ms = nimcp_time_monotonic_ms();
    compute_model_hash(model_weights, num_weights, cp->model_hash);

    cp->participating_workers = bridge->security_effects.active_worker_count;
    cp->agreeing_workers = cp->participating_workers;
    cp->consensus_ratio = 1.0f;
    cp->consensus_reached = true;
    cp->is_verified = true;
    cp->status = SECURITY_CHECKPOINT_OK;
    cp->loss_at_checkpoint = bridge->training_effects.current_loss;

    memcpy(cp->consensus_hash, cp->model_hash, SECURITY_DISTRIBUTED_HASH_SIZE);

    bridge->current_checkpoint_idx = (uint32_t)idx;
    bridge->security_effects.last_safe_checkpoint_round = round;

    BRIDGE_UNLOCK(bridge);

    stats->checkpoints_created++;
    stats->consensus_rounds++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created secure checkpoint '%s' at round %lu",
                         checkpoint_name, round);
    }

    return 0;
}

security_checkpoint_result_t security_distributed_training_verify_checkpoint(
    security_distributed_training_bridge_t* bridge,
    const char* checkpoint_name,
    const float* model_weights,
    uint32_t num_weights)
{
    if (!bridge) return SECURITY_CHECKPOINT_TAMPERED;
    if (!model_weights || num_weights == 0) return SECURITY_CHECKPOINT_TAMPERED;
    if (!checkpoint_name) return SECURITY_CHECKPOINT_TAMPERED;

    internal_stats_t* stats = get_internal_stats(bridge);
    stats->checkpoints_verified++;

    BRIDGE_LOCK(bridge);

    int idx = find_checkpoint(bridge, checkpoint_name);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        return SECURITY_CHECKPOINT_HASH_MISMATCH;
    }

    uint8_t current_hash[SECURITY_DISTRIBUTED_HASH_SIZE];
    compute_model_hash(model_weights, num_weights, current_hash);

    bool hash_match = hash_equals(current_hash, bridge->checkpoints[idx].model_hash);

    BRIDGE_UNLOCK(bridge);

    if (!hash_match) {
        stats->checkpoints_failed++;
        return SECURITY_CHECKPOINT_HASH_MISMATCH;
    }

    return SECURITY_CHECKPOINT_OK;
}

int security_distributed_training_get_checkpoint_info(
    const security_distributed_training_bridge_t* bridge,
    const char* checkpoint_name,
    security_distributed_checkpoint_t* info)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(checkpoint_name);
    BRIDGE_NULL_CHECK(info);

    int idx = find_checkpoint(bridge, checkpoint_name);
    if (idx < 0) return NIMCP_ERROR_NOT_FOUND;

    *info = bridge->checkpoints[idx];
    return 0;
}

int security_distributed_training_list_checkpoints(
    const security_distributed_training_bridge_t* bridge,
    security_distributed_checkpoint_t* checkpoints,
    uint32_t max_checkpoints)
{
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    uint32_t count = bridge->num_checkpoints;
    if (count > max_checkpoints) count = max_checkpoints;

    if (checkpoints && count > 0) {
        memcpy(checkpoints, bridge->checkpoints,
               count * sizeof(security_distributed_checkpoint_t));
    }

    return (int)count;
}

/* ============================================================================
 * Bidirectional Update API Implementation
 * ============================================================================ */

int security_distributed_training_update_security_effects(
    security_distributed_training_bridge_t* bridge)
{
    BRIDGE_NULL_CHECK(bridge);

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    float threat_level = 0.0f;

    if (bridge->security_effects.quarantined_worker_count > 0) {
        threat_level += 0.1f *
            (float)bridge->security_effects.quarantined_worker_count;
    }

    threat_level += bridge->security_effects.byzantine_ratio * 0.5f;

    if (bridge->training_effects.gradient_anomaly_detected) {
        threat_level += 0.2f;
    }

    if (bridge->training_effects.sync_failures_this_round > 5) {
        threat_level += 0.1f;
    }

    threat_level = nimcp_clampf(threat_level, 0.0f, 1.0f);

    bridge->security_effects.threat_level = threat_level;
    bridge->security_effects.under_attack = (threat_level > 0.7f);

    if (threat_level > 0.5f) {
        float reduction = 1.0f - (threat_level - 0.5f);
        bridge->security_effects.gradient_scale_factor = nimcp_clampf(reduction, 0.5f, 1.0f);
    } else {
        bridge->security_effects.gradient_scale_factor = 1.0f;
    }

    bridge->security_effects.required_aggregation =
        security_distributed_training_get_aggregation_method(bridge);

    bridge->security_effects.checkpoint_required = (threat_level > 0.6f);

    bridge->security_effects.last_update_ms = nimcp_time_monotonic_ms();
    bridge->security_effects.valid = true;

    if (threat_level > 0.8f) {
        bridge->phase = SECURITY_DISTRIBUTED_PHASE_RESPONDING;
    } else if (threat_level > 0.3f) {
        bridge->phase = SECURITY_DISTRIBUTED_PHASE_PROTECTING;
    } else {
        bridge->phase = SECURITY_DISTRIBUTED_PHASE_MONITORING;
    }

    BRIDGE_UNLOCK(bridge);

    stats->total_updates++;
    bridge->last_update_ms = nimcp_time_monotonic_ms();

    return 0;
}

int security_distributed_training_update_training_effects(
    security_distributed_training_bridge_t* bridge,
    uint64_t round,
    float loss,
    uint32_t active_workers)
{
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    bridge->training_effects.current_round = round;
    bridge->training_effects.current_loss = loss;
    bridge->training_effects.active_workers = active_workers;
    bridge->training_effects.total_workers = bridge->num_workers;

    bridge->training_effects.gradient_anomaly_detected = isnan(loss) || isinf(loss);

    bridge->training_effects.timestamp_ms = nimcp_time_monotonic_ms();
    bridge->training_effects.valid = true;

    bridge->current_round = round;

    BRIDGE_UNLOCK(bridge);

    internal_stats_t* stats = get_internal_stats(bridge);
    stats->total_rounds++;

    return 0;
}

int security_distributed_training_get_security_effects(
    const security_distributed_training_bridge_t* bridge,
    security_distributed_effects_t* effects)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(effects);

    *effects = bridge->security_effects;
    return 0;
}

int security_distributed_training_get_training_effects(
    const security_distributed_training_bridge_t* bridge,
    distributed_security_effects_t* effects)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(effects);

    *effects = bridge->training_effects;
    effects->anomalous_worker_indices = NULL;
    effects->worker_anomaly_scores = NULL;

    return 0;
}

/* ============================================================================
 * Bio-async Integration API Implementation
 * ============================================================================ */

int security_distributed_training_connect_bio_async(
    security_distributed_training_bridge_t* bridge)
{
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_connect_bio_async(&bridge->base);
}

int security_distributed_training_disconnect_bio_async(
    security_distributed_training_bridge_t* bridge)
{
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool security_distributed_training_is_bio_async_connected(
    const security_distributed_training_bridge_t* bridge)
{
    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Query and Statistics API Implementation
 * ============================================================================ */

security_distributed_phase_t security_distributed_training_get_phase(
    const security_distributed_training_bridge_t* bridge)
{
    if (!bridge) return SECURITY_DISTRIBUTED_PHASE_INACTIVE;
    return bridge->phase;
}

float security_distributed_training_get_threat_level(
    const security_distributed_training_bridge_t* bridge)
{
    if (!bridge) return 0.0f;
    return bridge->security_effects.threat_level;
}

float security_distributed_training_get_byzantine_ratio(
    const security_distributed_training_bridge_t* bridge)
{
    if (!bridge) return 0.0f;
    return bridge->security_effects.byzantine_ratio;
}

bool security_distributed_training_is_under_attack(
    const security_distributed_training_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->security_effects.under_attack;
}

int security_distributed_training_get_stats(
    const security_distributed_training_bridge_t* bridge,
    security_distributed_training_stats_t* stats)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(stats);

    const internal_stats_t* internal = get_internal_stats(
        (security_distributed_training_bridge_t*)bridge);

    memset(stats, 0, sizeof(security_distributed_training_stats_t));

    stats->total_workers_registered = internal->total_workers_registered;
    stats->workers_quarantined = internal->workers_quarantined;
    stats->workers_removed = internal->workers_removed;

    uint32_t active = 0, trusted = 0, quarantined = 0;
    for (uint32_t i = 0; i < bridge->num_workers; i++) {
        if (bridge->workers[i].is_active && !bridge->workers[i].is_quarantined) {
            active++;
            if (bridge->workers[i].trust_level >= SECURITY_WORKER_TRUST_TRUSTED) {
                trusted++;
            }
        }
        if (bridge->workers[i].is_quarantined) quarantined++;
    }
    stats->current_active_workers = active;
    stats->current_trusted_workers = trusted;
    stats->current_quarantined_workers = quarantined;

    stats->byzantine_checks = internal->byzantine_checks;
    stats->byzantine_detections = internal->byzantine_detections;
    memcpy(stats->byzantine_by_type, internal->byzantine_by_type,
           sizeof(stats->byzantine_by_type));

    stats->gradients_validated = internal->gradients_validated;
    stats->gradients_rejected = internal->gradients_rejected;
    stats->gradients_suspicious = internal->gradients_suspicious;
    if (internal->gradients_validated > 0) {
        stats->avg_gradient_norm = internal->total_gradient_norm /
                                  (float)internal->gradients_validated;
    }
    stats->max_gradient_norm = internal->max_gradient_norm;

    stats->trust_updates = internal->trust_updates;
    stats->trust_penalties = internal->trust_penalties;
    stats->trust_recoveries = internal->trust_recoveries;

    float total_trust = 0.0f;
    for (uint32_t i = 0; i < bridge->num_workers; i++) {
        total_trust += bridge->workers[i].trust_score;
    }
    if (bridge->num_workers > 0) {
        stats->avg_trust_score = total_trust / (float)bridge->num_workers;
    }

    stats->checkpoints_created = internal->checkpoints_created;
    stats->checkpoints_verified = internal->checkpoints_verified;
    stats->checkpoints_failed = internal->checkpoints_failed;
    stats->consensus_rounds = internal->consensus_rounds;

    stats->coordinator_connected = bridge->coordinator_connected;
    stats->bbb_connected = bridge->bbb_connected;
    stats->bio_async_connected = bridge->base.bio_async_enabled;

    stats->current_phase = bridge->phase;
    stats->current_threat_level = bridge->security_effects.threat_level;
    stats->current_byzantine_ratio = bridge->security_effects.byzantine_ratio;

    stats->total_rounds = internal->total_rounds;
    stats->total_updates = internal->total_updates;
    if (internal->byzantine_checks > 0) {
        stats->avg_byzantine_check_time_us = internal->total_byzantine_check_time_us /
                                            (float)internal->byzantine_checks;
    }
    if (internal->gradients_validated > 0) {
        stats->avg_validation_time_us = internal->total_validation_time_us /
                                       (float)internal->gradients_validated;
    }
    stats->uptime_ms = nimcp_time_monotonic_ms() - bridge->creation_time_ms;

    return 0;
}

int security_distributed_training_reset_stats(
    security_distributed_training_bridge_t* bridge)
{
    BRIDGE_NULL_CHECK(bridge);

    internal_stats_t* stats = get_internal_stats(bridge);
    memset(stats, 0, sizeof(internal_stats_t));

    return 0;
}

/* ============================================================================
 * String Conversion Utilities Implementation
 * ============================================================================ */

const char* security_worker_trust_to_string(security_worker_trust_t trust) {
    switch (trust) {
        case SECURITY_WORKER_TRUST_QUARANTINED: return "quarantined";
        case SECURITY_WORKER_TRUST_UNTRUSTED:   return "untrusted";
        case SECURITY_WORKER_TRUST_PROBATION:   return "probation";
        case SECURITY_WORKER_TRUST_VERIFIED:    return "verified";
        case SECURITY_WORKER_TRUST_TRUSTED:     return "trusted";
        default:                                return "unknown";
    }
}

const char* security_byzantine_type_to_string(security_byzantine_type_t type) {
    switch (type) {
        case SECURITY_BYZANTINE_NONE:           return "none";
        case SECURITY_BYZANTINE_GRADIENT_ATTACK: return "gradient_attack";
        case SECURITY_BYZANTINE_FREE_RIDER:     return "free_rider";
        case SECURITY_BYZANTINE_SYBIL:          return "sybil";
        case SECURITY_BYZANTINE_COLLUSION:      return "collusion";
        case SECURITY_BYZANTINE_MODEL_POISONING: return "model_poisoning";
        case SECURITY_BYZANTINE_DATA_LEAKAGE:   return "data_leakage";
        default:                                return "unknown";
    }
}

const char* security_grad_aggregation_to_string(security_grad_aggregation_t method) {
    switch (method) {
        case SECURITY_GRAD_AGG_SIMPLE_AVERAGE: return "simple_average";
        case SECURITY_GRAD_AGG_MEDIAN:         return "median";
        case SECURITY_GRAD_AGG_TRIMMED_MEAN:   return "trimmed_mean";
        case SECURITY_GRAD_AGG_KRUM:           return "krum";
        case SECURITY_GRAD_AGG_BULYAN:         return "bulyan";
        case SECURITY_GRAD_AGG_SECURE:         return "secure";
        case SECURITY_GRAD_AGG_MULTI_KRUM:     return "multi_krum";
        default:                               return "unknown";
    }
}

const char* security_distributed_phase_to_string(security_distributed_phase_t phase) {
    switch (phase) {
        case SECURITY_DISTRIBUTED_PHASE_INACTIVE:   return "inactive";
        case SECURITY_DISTRIBUTED_PHASE_MONITORING: return "monitoring";
        case SECURITY_DISTRIBUTED_PHASE_PROTECTING: return "protecting";
        case SECURITY_DISTRIBUTED_PHASE_RESPONDING: return "responding";
        case SECURITY_DISTRIBUTED_PHASE_RECOVERY:   return "recovery";
        default:                                    return "unknown";
    }
}

const char* security_checkpoint_result_to_string(security_checkpoint_result_t result) {
    switch (result) {
        case SECURITY_CHECKPOINT_OK:               return "ok";
        case SECURITY_CHECKPOINT_HASH_MISMATCH:    return "hash_mismatch";
        case SECURITY_CHECKPOINT_NO_CONSENSUS:     return "no_consensus";
        case SECURITY_CHECKPOINT_INSUFFICIENT_NODES: return "insufficient_nodes";
        case SECURITY_CHECKPOINT_TAMPERED:         return "tampered";
        default:                                   return "unknown";
    }
}
