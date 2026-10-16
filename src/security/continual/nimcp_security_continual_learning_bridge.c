/**
 * @file nimcp_security_continual_learning_bridge.c
 * @brief Security-Continual Learning Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bidirectional integration between security and continual learning
 * WHY:  Protect always-on learning from forgetting attacks, drift exploitation,
 *       replay poisoning, and learning rate manipulation
 * HOW:  Security monitors patterns, validates drift, verifies replay integrity;
 *       CL reports retention metrics, drift signals, and anomalies
 *
 * @author NIMCP Development Team
 */

#include "security/continual/nimcp_security_continual_learning_bridge.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "security/nimcp_security_math.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "security_cl_bridge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_continual_learning_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Statistics Tracking
 * ============================================================================ */

typedef struct {
    uint64_t knowledge_protections;
    uint64_t ewc_boosts_applied;
    uint64_t si_boosts_applied;
    uint64_t weights_locked;
    uint64_t drift_checks;
    uint64_t drift_detections;
    uint64_t adversarial_drifts_blocked;
    float max_drift_score;
    uint64_t replay_verifications;
    uint64_t replay_failures;
    uint64_t samples_quarantined;
    uint64_t lr_checks;
    uint64_t lr_manipulations_detected;
    uint64_t lr_overrides_applied;
    uint64_t retention_checks;
    uint64_t retention_anomalies;
    uint64_t emergency_responses;
    float min_retention_observed;
    uint64_t tasks_registered;
    uint64_t task_switches_blocked;
    uint64_t task_sequence_violations;
    uint64_t forgetting_attacks_detected;
    uint64_t forgetting_by_type[SECURITY_CL_FORGETTING_COUNT];
    uint64_t total_updates;
    float total_verification_time_us;
} internal_stats_t;

/* Stats embedded after bridge struct in allocated memory */
static internal_stats_t* get_internal_stats(security_cl_bridge_t* bridge) {
    return (internal_stats_t*)((char*)bridge + sizeof(security_cl_bridge_t));
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute SHA-256 hash of data (simplified)
 */
static void compute_buffer_hash(
    const void* data,
    size_t data_size,
    uint8_t* hash_out)
{
    if (!data || !hash_out || data_size == 0) {
        memset(hash_out, 0, SECURITY_CL_HASH_SIZE);
        return;
    }

    /* Simple hash via XOR folding (use real crypto in production) */
    uint64_t state[4] = {0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
                         0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL};

    const uint8_t* bytes = (const uint8_t*)data;
    for (size_t i = 0; i < data_size; i++) {
        state[i % 4] ^= ((uint64_t)bytes[i] << ((i % 8) * 8));
        state[i % 4] = (state[i % 4] << 7) | (state[i % 4] >> 57);
        state[(i + 1) % 4] += state[i % 4];
    }

    /* Final mixing */
    for (int r = 0; r < 4; r++) {
        state[0] ^= state[2];
        state[1] ^= state[3];
        state[0] = (state[0] << 13) | (state[0] >> 51);
        state[1] = (state[1] << 17) | (state[1] >> 47);
    }

    memcpy(hash_out, state, SECURITY_CL_HASH_SIZE);
}

/**
 * @brief Compare two hashes
 */
static bool hash_equals(const uint8_t* h1, const uint8_t* h2) {
    if (!h1 || !h2) {
        return false;
    }
    return memcmp(h1, h2, SECURITY_CL_HASH_SIZE) == 0;
}

/**
 * @brief Find task by ID
 */
static int find_task(const security_cl_bridge_t* bridge, uint32_t task_id) {
    if (!bridge) {
        return -1;
    }
    for (uint32_t i = 0; i < bridge->num_tasks; i++) {
        if (bridge->tasks[i].task_id == task_id) {
            return (int)i;
        }
    }
    /* Task not found - normal case, not an error */
    return -1;
}

/**
 * @brief Find replay buffer by ID
 */
static int find_replay_buffer(
    const security_cl_bridge_t* bridge,
    uint32_t buffer_id)
{
    if (!bridge) {
        return -1;
    }
    for (uint32_t i = 0; i < bridge->num_replay_buffers; i++) {
        if (bridge->replay_buffers[i].buffer_id == buffer_id) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Compute mean of float array
 */
static float compute_mean(const float* data, uint32_t count) {
    if (!data || count == 0) return 0.0f;
    double sum = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        sum += (double)data[i];
    }
    return (float)(sum / (double)count);
}

/**
 * @brief Compute variance of float array
 */
static float compute_variance(const float* data, uint32_t count) {
    if (!data || count < 2) return 0.0f;
    float mean = compute_mean(data, count);
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        double diff = (double)data[i] - (double)mean;
        sum_sq += diff * diff;
    }
    return (float)(sum_sq / (double)(count - 1));
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int security_cl_default_config(security_cl_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(security_cl_config_t));

    /* Feature enables */
    config->enable_forgetting_protection = true;
    config->enable_drift_detection = true;
    config->enable_replay_verification = true;
    config->enable_lr_monitoring = true;
    config->enable_task_validation = true;
    config->enable_ewc_boost = true;

    /* Knowledge retention */
    config->retention_threshold = SECURITY_CL_DEFAULT_RETENTION_THRESHOLD;
    config->critical_threshold = SECURITY_CL_DEFAULT_CRITICAL_THRESHOLD;
    config->max_forgetting_rate = SECURITY_CL_DEFAULT_FORGETTING_RATE;

    /* Concept drift */
    config->drift_threshold = SECURITY_CL_DEFAULT_DRIFT_THRESHOLD;
    config->sudden_drift_threshold = SECURITY_CL_DEFAULT_SUDDEN_DRIFT_THRESHOLD;
    config->drift_window_size = SECURITY_CL_DRIFT_WINDOW_SIZE;

    /* Learning rate */
    config->lr_min_bound = SECURITY_CL_DEFAULT_LR_MIN;
    config->lr_max_bound = SECURITY_CL_DEFAULT_LR_MAX;
    config->lr_change_threshold = SECURITY_CL_DEFAULT_LR_CHANGE_THRESHOLD;

    /* Replay verification */
    config->replay_verify_rate = SECURITY_CL_DEFAULT_REPLAY_VERIFY_RATE;
    config->use_hash_chains = true;

    /* Bio-async */
    config->enable_bio_async = true;
    config->bio_inbox_capacity = SECURITY_CL_BIO_INBOX_CAPACITY;

    /* Logging */
    config->enable_logging = true;
    config->log_all_verifications = false;

    return 0;
}

security_cl_bridge_t* security_cl_bridge_create(
    const security_cl_config_t* config)
{
    /* Allocate bridge + internal stats */
    size_t alloc_size = sizeof(security_cl_bridge_t) + sizeof(internal_stats_t);
    security_cl_bridge_t* bridge = nimcp_malloc(alloc_size);
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate security-CL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_cl_bridge_create: failed to allocate bridge");

        return NULL;
    }
    memset(bridge, 0, alloc_size);

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY,
                         SECURITY_CL_MODULE_NAME) != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_cl_bridge_create: bridge_base_init failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_cl_default_config(&bridge->config);
    }

    /* Initialize state */
    bridge->phase = SECURITY_CL_PHASE_MONITORING;
    bridge->creation_time_ms = nimcp_time_monotonic_ms();
    bridge->last_update_ms = bridge->creation_time_ms;

    /* Initialize security effects with safe defaults */
    bridge->security_effects.ewc_lambda_boost = 1.0f;
    bridge->security_effects.si_importance_boost = 1.0f;
    bridge->security_effects.lr_max_allowed = bridge->config.lr_max_bound;
    bridge->security_effects.lr_min_allowed = bridge->config.lr_min_bound;
    bridge->security_effects.lr_scale_factor = 1.0f;
    bridge->security_effects.retention_status = SECURITY_CL_RETENTION_HEALTHY;
    bridge->security_effects.valid = true;
    bridge->security_effects.last_update_ms = bridge->creation_time_ms;

    /* Initialize CL effects */
    bridge->cl_effects.valid = false;
    bridge->cl_effects.current_retention = 1.0f;

    /* Initialize stats */
    internal_stats_t* stats = get_internal_stats(bridge);
    stats->min_retention_observed = 1.0f;

    /* Allocate drift baseline if enabled */
    if (bridge->config.enable_drift_detection) {
        bridge->drift_baseline = nimcp_malloc(
            bridge->config.drift_window_size * sizeof(float));
        if (!bridge->drift_baseline) {
            NIMCP_LOGGING_WARN("Failed to allocate drift baseline");
        }
    }

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async) {
        security_cl_connect_bio_async(bridge);
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-CL bridge created");
    }

    return bridge;
}

void security_cl_bridge_destroy(security_cl_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        security_cl_disconnect_bio_async(bridge);
    }

    /* Free drift baseline */
    if (bridge->drift_baseline) {
        nimcp_free(bridge->drift_baseline);
        bridge->drift_baseline = NULL;
    }

    /* Free weight tracking */
    if (bridge->weight_importance) {
        nimcp_free(bridge->weight_importance);
        bridge->weight_importance = NULL;
    }
    if (bridge->weight_baseline) {
        nimcp_free(bridge->weight_baseline);
        bridge->weight_baseline = NULL;
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int security_cl_connect_continual_learning(
    security_cl_bridge_t* bridge,
    void* continual_learning)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(continual_learning);

    BRIDGE_LOCK(bridge);
    bridge->continual_learning = continual_learning;
    bridge->cl_connected = true;
    bridge->phase = SECURITY_CL_PHASE_PROTECTING;
    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Connected to continual learning pipeline");
    }
    return 0;
}

int security_cl_connect_bbb(
    security_cl_bridge_t* bridge,
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

int security_cl_connect_anomaly_detector(
    security_cl_bridge_t* bridge,
    nimcp_anomaly_detector_t detector)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(detector);

    BRIDGE_LOCK(bridge);
    bridge->anomaly_detector = detector;
    bridge->anomaly_connected = true;
    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Connected to anomaly detector");
    }
    return 0;
}

int security_cl_disconnect_continual_learning(security_cl_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->continual_learning = NULL;
    bridge->cl_connected = false;
    if (!bridge->bbb_connected) {
        bridge->phase = SECURITY_CL_PHASE_MONITORING;
    }
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_cl_disconnect_bbb(security_cl_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->bbb = NULL;
    bridge->bbb_connected = false;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_cl_disconnect_anomaly_detector(security_cl_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->anomaly_detector = NULL;
    bridge->anomaly_connected = false;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Knowledge Protection API Implementation
 * ============================================================================ */

int security_cl_protect_knowledge(
    security_cl_bridge_t* bridge,
    const float* weights,
    uint32_t num_weights,
    uint32_t task_id)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(weights);

    if (num_weights == 0) return NIMCP_ERROR_INVALID_PARAM;
    if (!bridge->config.enable_forgetting_protection) return 0;

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    /* Allocate or reallocate weight tracking arrays */
    if (bridge->num_weights != num_weights) {
        if (bridge->weight_importance) {
            nimcp_free(bridge->weight_importance);
        }
        if (bridge->weight_baseline) {
            nimcp_free(bridge->weight_baseline);
        }

        bridge->weight_importance = nimcp_malloc(num_weights * sizeof(float));
        bridge->weight_baseline = nimcp_malloc(num_weights * sizeof(float));

        if (!bridge->weight_importance || !bridge->weight_baseline) {
            if (bridge->weight_importance) nimcp_free(bridge->weight_importance);
            if (bridge->weight_baseline) nimcp_free(bridge->weight_baseline);
            bridge->weight_importance = NULL;
            bridge->weight_baseline = NULL;
            BRIDGE_UNLOCK(bridge);
            return NIMCP_ERROR_NO_MEMORY;
        }

        bridge->num_weights = num_weights;
    }

    /* Store baseline weights */
    memcpy(bridge->weight_baseline, weights, num_weights * sizeof(float));

    /* Compute weight importance (simplified EWC Fisher approximation) */
    /* In production, this would use actual Fisher information */
    for (uint32_t i = 0; i < num_weights; i++) {
        /* Importance based on weight magnitude (simplified) */
        float abs_weight = fabsf(weights[i]);
        bridge->weight_importance[i] = abs_weight * abs_weight;
    }

    /* Normalize importance */
    float max_importance = 0.0f;
    for (uint32_t i = 0; i < num_weights; i++) {
        if (bridge->weight_importance[i] > max_importance) {
            max_importance = bridge->weight_importance[i];
        }
    }
    if (max_importance > 0.0f) {
        for (uint32_t i = 0; i < num_weights; i++) {
            bridge->weight_importance[i] /= max_importance;
        }
    }

    /* Update task protection if registered */
    int task_idx = find_task(bridge, task_id);
    if (task_idx >= 0) {
        bridge->tasks[task_idx].is_protected = true;
        bridge->tasks[task_idx].protection_strength = 1.0f;
    }

    bridge->security_effects.protected_task_count++;

    BRIDGE_UNLOCK(bridge);

    stats->knowledge_protections++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Protected knowledge for task %u (%u weights)",
                          task_id, num_weights);
    }

    return 0;
}

int security_cl_register_task(
    security_cl_bridge_t* bridge,
    uint32_t task_id,
    const char* task_name,
    float baseline_accuracy)
{
    BRIDGE_NULL_CHECK(bridge);

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    /* Check if task already exists */
    int existing = find_task(bridge, task_id);
    if (existing >= 0) {
        /* Update existing task */
        bridge->tasks[existing].baseline_accuracy = baseline_accuracy;
        bridge->tasks[existing].current_accuracy = baseline_accuracy;
        bridge->tasks[existing].retention_rate = 1.0f;
        bridge->tasks[existing].learned_at_ms = nimcp_time_monotonic_ms();
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Check capacity */
    if (bridge->num_tasks >= SECURITY_CL_MAX_TASKS) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Add new task */
    uint32_t idx = bridge->num_tasks;
    security_cl_task_info_t* task = &bridge->tasks[idx];

    task->task_id = task_id;
    if (task_name) {
        strncpy(task->name, task_name, sizeof(task->name) - 1);
    } else {
        snprintf(task->name, sizeof(task->name), "task_%u", task_id);
    }
    task->baseline_accuracy = baseline_accuracy;
    task->current_accuracy = baseline_accuracy;
    task->retention_rate = 1.0f;
    task->learned_at_ms = nimcp_time_monotonic_ms();
    task->last_evaluated_ms = task->learned_at_ms;
    task->is_protected = false;
    task->protection_strength = 0.0f;

    /* Compute task fingerprint (simplified) */
    compute_buffer_hash(task_name, task_name ? strlen(task_name) : 0,
                       task->fingerprint);

    bridge->num_tasks++;

    BRIDGE_UNLOCK(bridge);

    stats->tasks_registered++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Registered task %u: %s (baseline: %.2f)",
                          task_id, task->name, baseline_accuracy);
    }

    return 0;
}

int security_cl_get_task_info(
    const security_cl_bridge_t* bridge,
    uint32_t task_id,
    security_cl_task_info_t* info)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(info);

    int idx = find_task(bridge, task_id);
    if (idx < 0) return NIMCP_ERROR_NOT_FOUND;

    *info = bridge->tasks[idx];
    return 0;
}

int security_cl_compute_protection_penalty(
    const security_cl_bridge_t* bridge,
    const float* new_weights,
    uint32_t num_weights,
    float* penalty)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(new_weights);
    BRIDGE_NULL_CHECK(penalty);

    *penalty = 0.0f;

    if (!bridge->weight_baseline || !bridge->weight_importance) {
        return 0;  /* No protection baseline established */
    }

    if (num_weights != bridge->num_weights) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Compute EWC-style penalty */
    float lambda = bridge->security_effects.ewc_lambda_boost;
    double total_penalty = 0.0;

    for (uint32_t i = 0; i < num_weights; i++) {
        double diff = (double)new_weights[i] - (double)bridge->weight_baseline[i];
        double importance = (double)bridge->weight_importance[i];
        total_penalty += importance * diff * diff;
    }

    *penalty = (float)(lambda * total_penalty / (double)num_weights);
    return 0;
}

/* ============================================================================
 * Concept Drift API Implementation
 * ============================================================================ */

bool security_cl_validate_drift(
    security_cl_bridge_t* bridge,
    const float* current_features,
    uint32_t num_features,
    security_cl_drift_type_t* drift_type,
    float* drift_score)
{
    BRIDGE_NULL_CHECK_BOOL(bridge);

    if (drift_type) *drift_type = SECURITY_CL_DRIFT_NONE;
    if (drift_score) *drift_score = 0.0f;

    if (!bridge->config.enable_drift_detection) return true;
    if (!current_features || num_features == 0) return true;

    internal_stats_t* stats = get_internal_stats(bridge);
    stats->drift_checks++;

    BRIDGE_LOCK(bridge);

    /* Check if baseline exists */
    if (!bridge->drift_baseline || bridge->drift_baseline_size == 0) {
        BRIDGE_UNLOCK(bridge);
        return true;  /* No baseline, allow */
    }

    /* Compute drift score (mean absolute difference) */
    uint32_t compare_len = num_features < bridge->drift_baseline_size ?
                           num_features : bridge->drift_baseline_size;

    double total_diff = 0.0;
    double max_single_diff = 0.0;

    for (uint32_t i = 0; i < compare_len; i++) {
        double diff = fabs((double)current_features[i] -
                          (double)bridge->drift_baseline[i]);
        total_diff += diff;
        if (diff > max_single_diff) {
            max_single_diff = diff;
        }
    }

    float score = (float)(total_diff / (double)compare_len);
    score = nimcp_clampf(score, 0.0f, 1.0f);

    /* Classify drift type */
    security_cl_drift_type_t type = SECURITY_CL_DRIFT_NONE;
    bool allow = true;

    if (score > bridge->config.drift_threshold) {
        stats->drift_detections++;

        /* Check if drift is sudden (adversarial indicator) */
        if (max_single_diff > bridge->config.sudden_drift_threshold) {
            type = SECURITY_CL_DRIFT_ADVERSARIAL;
            allow = false;
            stats->adversarial_drifts_blocked++;
            NIMCP_LOGGING_WARN("Adversarial drift detected (score: %.3f)", score);
        } else {
            type = SECURITY_CL_DRIFT_NATURAL;
            /* Natural drift is allowed */
        }

        if (score > stats->max_drift_score) {
            stats->max_drift_score = score;
        }
    }

    /* Update effects */
    bridge->cl_effects.current_drift_score = score;
    bridge->cl_effects.drift_type = type;
    bridge->cl_effects.drift_anomaly = !allow;

    BRIDGE_UNLOCK(bridge);

    if (drift_type) *drift_type = type;
    if (drift_score) *drift_score = score;

    return allow;
}

int security_cl_update_drift_baseline(
    security_cl_bridge_t* bridge,
    const float* features,
    uint32_t num_features)
{
    BRIDGE_NULL_CHECK(bridge);

    if (num_features == 0) return 0;

    BRIDGE_NULL_CHECK(features);

    BRIDGE_LOCK(bridge);

    /* Allocate baseline if needed */
    if (!bridge->drift_baseline) {
        bridge->drift_baseline = nimcp_malloc(
            bridge->config.drift_window_size * sizeof(float));
        if (!bridge->drift_baseline) {
            BRIDGE_UNLOCK(bridge);
            return NIMCP_ERROR_NO_MEMORY;
        }
    }

    /* Determine storage length */
    uint32_t store_len = num_features;
    if (store_len > bridge->config.drift_window_size) {
        store_len = bridge->config.drift_window_size;
    }

    float alpha = 0.1f;  /* EMA smoothing factor */

    if (bridge->drift_baseline_size == 0) {
        /* First update: copy directly */
        memcpy(bridge->drift_baseline, features, store_len * sizeof(float));
    } else {
        /* Subsequent updates: EMA */
        uint32_t update_len = store_len < bridge->drift_baseline_size ?
                              store_len : bridge->drift_baseline_size;
        for (uint32_t i = 0; i < update_len; i++) {
            bridge->drift_baseline[i] =
                alpha * features[i] + (1.0f - alpha) * bridge->drift_baseline[i];
        }
    }

    bridge->drift_baseline_size = store_len;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_cl_reset_drift_baseline(security_cl_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    if (bridge->drift_baseline) {
        memset(bridge->drift_baseline, 0,
               bridge->config.drift_window_size * sizeof(float));
    }
    bridge->drift_baseline_size = 0;
    bridge->cl_effects.current_drift_score = 0.0f;
    bridge->cl_effects.drift_type = SECURITY_CL_DRIFT_NONE;
    bridge->cl_effects.drift_anomaly = false;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Memory Replay API Implementation
 * ============================================================================ */

bool security_cl_verify_replay(
    security_cl_bridge_t* bridge,
    const void* buffer,
    size_t buffer_size,
    uint32_t sample_count,
    security_cl_replay_status_t* status)
{
    BRIDGE_NULL_CHECK_BOOL(bridge);

    if (status) *status = SECURITY_CL_REPLAY_OK;

    if (!bridge->config.enable_replay_verification) return true;
    if (!buffer || buffer_size == 0) return true;

    internal_stats_t* stats = get_internal_stats(bridge);
    stats->replay_verifications++;

    BRIDGE_LOCK(bridge);

    /* Compute current hash */
    uint8_t current_hash[SECURITY_CL_HASH_SIZE];
    compute_buffer_hash(buffer, buffer_size, current_hash);

    /* Check against registered buffers */
    bool found_match = false;
    security_cl_replay_status_t result = SECURITY_CL_REPLAY_OK;

    for (uint32_t i = 0; i < bridge->num_replay_buffers; i++) {
        security_cl_replay_info_t* info = &bridge->replay_buffers[i];
        if (!info->active) continue;

        if (info->buffer_ptr == buffer) {
            found_match = true;

            /* Compare with baseline hash */
            if (!hash_equals(current_hash, info->baseline_hash)) {
                result = SECURITY_CL_REPLAY_HASH_MISMATCH;
                info->verification_failures++;
                stats->replay_failures++;

                if (info->verification_failures > 3) {
                    result = SECURITY_CL_REPLAY_TAMPERED;
                }
            }

            info->last_verified_ms = nimcp_time_monotonic_ms();
            break;
        }
    }

    /* Update effects */
    if (result != SECURITY_CL_REPLAY_OK) {
        bridge->cl_effects.replay_integrity_failures++;
        bridge->cl_effects.replay_anomaly = true;
    }

    BRIDGE_UNLOCK(bridge);

    if (status) *status = result;

    if (result != SECURITY_CL_REPLAY_OK && bridge->config.enable_logging) {
        NIMCP_LOGGING_WARN("Replay verification failed: %s",
                          security_cl_replay_status_to_string(result));
    }

    return (result == SECURITY_CL_REPLAY_OK);
}

uint32_t security_cl_register_replay_buffer(
    security_cl_bridge_t* bridge,
    const char* buffer_name,
    const void* buffer,
    size_t buffer_size,
    uint32_t sample_count)
{
    if (!bridge || !buffer || buffer_size == 0) return 0;

    BRIDGE_LOCK(bridge);

    if (bridge->num_replay_buffers >= SECURITY_CL_MAX_REPLAY_BUFFERS) {
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    uint32_t idx = bridge->num_replay_buffers;
    security_cl_replay_info_t* info = &bridge->replay_buffers[idx];

    info->buffer_id = idx + 1;  /* IDs start at 1 */
    if (buffer_name) {
        strncpy(info->name, buffer_name, sizeof(info->name) - 1);
    } else {
        snprintf(info->name, sizeof(info->name), "replay_%u", info->buffer_id);
    }
    info->buffer_ptr = (void*)buffer;
    info->buffer_size = buffer_size;
    info->sample_count = sample_count;
    info->verification_failures = 0;
    info->active = true;
    info->last_verified_ms = nimcp_time_monotonic_ms();

    /* Compute baseline hash */
    compute_buffer_hash(buffer, buffer_size, info->baseline_hash);
    memcpy(info->current_hash, info->baseline_hash, SECURITY_CL_HASH_SIZE);

    bridge->num_replay_buffers++;

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Registered replay buffer %u: %s (%zu bytes, %u samples)",
                          info->buffer_id, info->name, buffer_size, sample_count);
    }

    return info->buffer_id;
}

int security_cl_update_replay_hash(
    security_cl_bridge_t* bridge,
    uint32_t buffer_id,
    const void* buffer,
    size_t buffer_size,
    uint32_t sample_count)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(buffer);

    BRIDGE_LOCK(bridge);

    int idx = find_replay_buffer(bridge, buffer_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    security_cl_replay_info_t* info = &bridge->replay_buffers[idx];
    info->buffer_ptr = (void*)buffer;
    info->buffer_size = buffer_size;
    info->sample_count = sample_count;
    info->verification_failures = 0;
    info->last_verified_ms = nimcp_time_monotonic_ms();

    /* Update baseline hash */
    compute_buffer_hash(buffer, buffer_size, info->baseline_hash);
    memcpy(info->current_hash, info->baseline_hash, SECURITY_CL_HASH_SIZE);

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_cl_quarantine_replay_samples(
    security_cl_bridge_t* bridge,
    const uint32_t* sample_indices,
    uint32_t num_samples)
{
    BRIDGE_NULL_CHECK(bridge);

    if (!sample_indices || num_samples == 0) return 0;

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);
    bridge->security_effects.quarantined_samples += num_samples;
    BRIDGE_UNLOCK(bridge);

    stats->samples_quarantined += num_samples;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Quarantined %u replay samples", num_samples);
    }

    return 0;
}

/* ============================================================================
 * Retention Monitoring API Implementation
 * ============================================================================ */

security_cl_retention_level_t security_cl_monitor_retention(
    security_cl_bridge_t* bridge,
    uint32_t task_id,
    float current_accuracy,
    float* retention)
{
    if (!bridge) {
        if (retention) *retention = 0.0f;
        return SECURITY_CL_RETENTION_COMPROMISED;
    }

    internal_stats_t* stats = get_internal_stats(bridge);
    stats->retention_checks++;

    BRIDGE_LOCK(bridge);

    int idx = find_task(bridge, task_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        if (retention) *retention = 1.0f;
        return SECURITY_CL_RETENTION_HEALTHY;
    }

    security_cl_task_info_t* task = &bridge->tasks[idx];
    task->current_accuracy = current_accuracy;
    task->last_evaluated_ms = nimcp_time_monotonic_ms();

    /* Compute retention rate */
    float retention_rate = 1.0f;
    if (task->baseline_accuracy > 0.0f) {
        retention_rate = current_accuracy / task->baseline_accuracy;
        retention_rate = nimcp_clampf(retention_rate, 0.0f, 1.0f);
    }
    task->retention_rate = retention_rate;

    /* Track minimum retention */
    if (retention_rate < stats->min_retention_observed) {
        stats->min_retention_observed = retention_rate;
    }

    /* Classify retention level */
    security_cl_retention_level_t level;
    if (retention_rate >= bridge->config.retention_threshold) {
        level = SECURITY_CL_RETENTION_HEALTHY;
    } else if (retention_rate >= bridge->config.critical_threshold) {
        level = SECURITY_CL_RETENTION_DEGRADING;
        stats->retention_anomalies++;

        /* Apply EWC boost if enabled */
        if (bridge->config.enable_ewc_boost) {
            bridge->security_effects.ewc_lambda_boost *= 1.5f;
            stats->ewc_boosts_applied++;
        }
    } else {
        level = SECURITY_CL_RETENTION_CRITICAL;
        stats->emergency_responses++;

        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_WARN("Critical retention loss on task %u (%.2f)",
                              task_id, retention_rate);
        }
    }

    bridge->security_effects.retention_status = level;

    BRIDGE_UNLOCK(bridge);

    if (retention) *retention = retention_rate;
    return level;
}

security_cl_retention_level_t security_cl_get_retention_status(
    const security_cl_bridge_t* bridge,
    float* overall_retention)
{
    if (!bridge) {
        if (overall_retention) *overall_retention = 0.0f;
        return SECURITY_CL_RETENTION_COMPROMISED;
    }

    if (bridge->num_tasks == 0) {
        if (overall_retention) *overall_retention = 1.0f;
        return SECURITY_CL_RETENTION_HEALTHY;
    }

    /* Compute average retention across all tasks */
    float total = 0.0f;
    for (uint32_t i = 0; i < bridge->num_tasks; i++) {
        total += bridge->tasks[i].retention_rate;
    }
    float avg = total / (float)bridge->num_tasks;

    if (overall_retention) *overall_retention = avg;

    if (avg >= bridge->config.retention_threshold) {
        return SECURITY_CL_RETENTION_HEALTHY;
    } else if (avg >= bridge->config.critical_threshold) {
        return SECURITY_CL_RETENTION_DEGRADING;
    }
    return SECURITY_CL_RETENTION_CRITICAL;
}

bool security_cl_is_retention_compromised(const security_cl_bridge_t* bridge) {
    if (!bridge) return true;
    float overall;
    security_cl_retention_level_t level = security_cl_get_retention_status(
        bridge, &overall);
    return (level == SECURITY_CL_RETENTION_CRITICAL ||
            level == SECURITY_CL_RETENTION_COMPROMISED);
}

/* ============================================================================
 * Learning Rate Monitoring API Implementation
 * ============================================================================ */

bool security_cl_detect_lr_manipulation(
    security_cl_bridge_t* bridge,
    float proposed_lr)
{
    BRIDGE_NULL_CHECK_BOOL(bridge);

    if (!bridge->config.enable_lr_monitoring) {
        return false;
    }

    internal_stats_t* stats = get_internal_stats(bridge);
    stats->lr_checks++;

    /* Check bounds */
    if (proposed_lr < bridge->config.lr_min_bound ||
        proposed_lr > bridge->config.lr_max_bound) {
        stats->lr_manipulations_detected++;
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_WARN("LR out of bounds: %.6f (bounds: %.6f-%.6f)",
                              proposed_lr, bridge->config.lr_min_bound,
                              bridge->config.lr_max_bound);
        }
        return true;
    }

    BRIDGE_LOCK(bridge);

    /* Check for sudden change if we have history */
    if (bridge->lr_history_count > 0) {
        float last_lr = bridge->lr_history[
            (bridge->lr_history_idx + SECURITY_CL_LR_HISTORY_SIZE - 1) %
            SECURITY_CL_LR_HISTORY_SIZE];

        float change = fabsf(proposed_lr - last_lr) / (last_lr + 1e-10f);
        if (change > bridge->config.lr_change_threshold) {
            stats->lr_manipulations_detected++;
            BRIDGE_UNLOCK(bridge);

            if (bridge->config.enable_logging) {
                NIMCP_LOGGING_WARN("LR sudden change: %.6f -> %.6f (%.1f%%)",
                                  last_lr, proposed_lr, change * 100.0f);
            }
            return true;
        }
    }

    BRIDGE_UNLOCK(bridge);

    /* No manipulation detected */
    return false;
}

float security_cl_get_safe_lr(const security_cl_bridge_t* bridge) {
    if (!bridge) return SECURITY_CL_DEFAULT_LR_MIN;

    float lr = bridge->security_effects.lr_max_allowed *
               bridge->security_effects.lr_scale_factor;
    return nimcp_clampf(lr, bridge->security_effects.lr_min_allowed,
                  bridge->security_effects.lr_max_allowed);
}

int security_cl_record_lr(security_cl_bridge_t* bridge, float lr) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    bridge->lr_history[bridge->lr_history_idx] = lr;
    bridge->lr_history_idx = (bridge->lr_history_idx + 1) %
                              SECURITY_CL_LR_HISTORY_SIZE;
    if (bridge->lr_history_count < SECURITY_CL_LR_HISTORY_SIZE) {
        bridge->lr_history_count++;
    }

    bridge->cl_effects.current_lr = lr;
    if (bridge->lr_history_count >= 2) {
        bridge->cl_effects.lr_variance = compute_variance(
            bridge->lr_history, bridge->lr_history_count);
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_cl_set_lr_bounds(
    security_cl_bridge_t* bridge,
    float lr_min,
    float lr_max)
{
    BRIDGE_NULL_CHECK(bridge);

    if (lr_min >= lr_max) return NIMCP_ERROR_INVALID_PARAM;
    if (lr_min < 0.0f) return NIMCP_ERROR_INVALID_PARAM;

    BRIDGE_LOCK(bridge);

    bridge->config.lr_min_bound = lr_min;
    bridge->config.lr_max_bound = lr_max;
    bridge->security_effects.lr_min_allowed = lr_min;
    bridge->security_effects.lr_max_allowed = lr_max;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bidirectional Update API Implementation
 * ============================================================================ */

int security_cl_update_security_effects(security_cl_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    /* Aggregate threat signals */
    float threat_level = 0.0f;

    /* Factor 1: Retention degradation (from direct monitoring) */
    if (bridge->security_effects.retention_status == SECURITY_CL_RETENTION_DEGRADING) {
        threat_level += 0.2f;
    } else if (bridge->security_effects.retention_status == SECURITY_CL_RETENTION_CRITICAL) {
        threat_level += 0.4f;
    }

    /* Factor 1b: Retention anomaly (from CL effects update) */
    if (bridge->cl_effects.valid && bridge->cl_effects.retention_anomaly) {
        threat_level += 0.3f;
    }

    /* Factor 2: Drift anomaly */
    if (bridge->cl_effects.drift_anomaly) {
        threat_level += 0.2f;
    }

    /* Factor 3: Replay anomaly */
    if (bridge->cl_effects.replay_anomaly) {
        threat_level += 0.3f;
    }

    /* Factor 4: LR anomaly */
    if (bridge->cl_effects.lr_anomaly) {
        threat_level += 0.2f;
    }

    /* Factor 5: Task sequence anomaly */
    if (bridge->cl_effects.task_sequence_anomaly) {
        threat_level += 0.1f;
    }

    threat_level = nimcp_clampf(threat_level, 0.0f, 1.0f);

    /* Update security effects */
    bridge->security_effects.threat_level = threat_level;
    bridge->security_effects.under_attack = (threat_level > 0.6f);

    /* Adjust EWC boost based on threat */
    if (threat_level > 0.5f) {
        bridge->security_effects.ewc_lambda_boost =
            1.0f + (threat_level - 0.5f) * 2.0f;
    }

    /* Adjust LR scale based on threat */
    if (threat_level > 0.3f) {
        bridge->security_effects.lr_scale_factor =
            1.0f - (threat_level - 0.3f) * 0.5f;
        bridge->security_effects.lr_scale_factor =
            nimcp_clampf(bridge->security_effects.lr_scale_factor, 0.5f, 1.0f);
    }

    /* Block learning if under severe attack */
    if (threat_level > 0.8f) {
        bridge->security_effects.new_task_blocked = true;
        bridge->security_effects.knowledge_lock_active = true;
    }

    /* Update phase */
    if (threat_level > 0.7f) {
        bridge->phase = SECURITY_CL_PHASE_DEFENDING;
    } else if (threat_level > 0.3f) {
        bridge->phase = SECURITY_CL_PHASE_PROTECTING;
    } else if (bridge->cl_connected) {
        /* When CL is connected, minimum phase is PROTECTING */
        bridge->phase = SECURITY_CL_PHASE_PROTECTING;
    } else {
        bridge->phase = SECURITY_CL_PHASE_MONITORING;
    }

    bridge->security_effects.last_update_ms = nimcp_time_monotonic_ms();
    bridge->security_effects.valid = true;

    BRIDGE_UNLOCK(bridge);

    stats->total_updates++;
    bridge->last_update_ms = nimcp_time_monotonic_ms();

    return 0;
}

int security_cl_update_cl_effects(
    security_cl_bridge_t* bridge,
    float retention,
    float drift_score,
    float lr,
    uint32_t task_id)
{
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    bridge->cl_effects.current_retention = retention;
    bridge->cl_effects.current_drift_score = drift_score;
    bridge->cl_effects.current_lr = lr;
    bridge->cl_effects.current_task_id = task_id;

    /* Compute retention delta if we have previous */
    static float last_retention = 1.0f;
    bridge->cl_effects.retention_delta = retention - last_retention;
    last_retention = retention;

    bridge->cl_effects.retention_anomaly =
        (retention < bridge->config.critical_threshold);

    bridge->cl_effects.timestamp_ms = nimcp_time_monotonic_ms();
    bridge->cl_effects.valid = true;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_cl_get_security_effects(
    const security_cl_bridge_t* bridge,
    security_cl_effects_t* effects)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(effects);

    *effects = bridge->security_effects;
    return 0;
}

int security_cl_get_cl_effects(
    const security_cl_bridge_t* bridge,
    cl_security_effects_t* effects)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(effects);

    *effects = bridge->cl_effects;
    return 0;
}

/* ============================================================================
 * Bio-async Integration API Implementation
 * ============================================================================ */

int security_cl_connect_bio_async(security_cl_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_connect_bio_async(&bridge->base);
}

int security_cl_disconnect_bio_async(security_cl_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool security_cl_is_bio_async_connected(const security_cl_bridge_t* bridge) {
    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Query and Statistics API Implementation
 * ============================================================================ */

security_cl_phase_t security_cl_get_phase(const security_cl_bridge_t* bridge) {
    if (!bridge) return SECURITY_CL_PHASE_INACTIVE;
    return bridge->phase;
}

float security_cl_get_threat_level(const security_cl_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->security_effects.threat_level;
}

bool security_cl_is_under_attack(const security_cl_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->security_effects.under_attack;
}

int security_cl_get_stats(
    const security_cl_bridge_t* bridge,
    security_cl_stats_t* stats)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(stats);

    const internal_stats_t* internal = get_internal_stats(
        (security_cl_bridge_t*)bridge);

    memset(stats, 0, sizeof(security_cl_stats_t));

    /* Copy from internal stats */
    stats->knowledge_protections = internal->knowledge_protections;
    stats->ewc_boosts_applied = internal->ewc_boosts_applied;
    stats->si_boosts_applied = internal->si_boosts_applied;
    stats->weights_locked = internal->weights_locked;
    stats->drift_checks = internal->drift_checks;
    stats->drift_detections = internal->drift_detections;
    stats->adversarial_drifts_blocked = internal->adversarial_drifts_blocked;
    stats->max_drift_score = internal->max_drift_score;
    stats->replay_verifications = internal->replay_verifications;
    stats->replay_failures = internal->replay_failures;
    stats->samples_quarantined = internal->samples_quarantined;
    stats->lr_checks = internal->lr_checks;
    stats->lr_manipulations_detected = internal->lr_manipulations_detected;
    stats->lr_overrides_applied = internal->lr_overrides_applied;
    stats->retention_checks = internal->retention_checks;
    stats->retention_anomalies = internal->retention_anomalies;
    stats->emergency_responses = internal->emergency_responses;
    stats->min_retention_observed = internal->min_retention_observed;
    stats->tasks_registered = internal->tasks_registered;
    stats->task_switches_blocked = internal->task_switches_blocked;
    stats->task_sequence_violations = internal->task_sequence_violations;
    stats->forgetting_attacks_detected = internal->forgetting_attacks_detected;
    memcpy(stats->forgetting_by_type, internal->forgetting_by_type,
           sizeof(stats->forgetting_by_type));

    /* Connection status */
    stats->cl_connected = bridge->cl_connected;
    stats->bbb_connected = bridge->bbb_connected;
    stats->anomaly_connected = bridge->anomaly_connected;
    stats->bio_async_connected = bridge->base.bio_async_enabled;

    /* Current state */
    stats->current_phase = bridge->phase;
    stats->current_threat_level = bridge->security_effects.threat_level;
    stats->retention_level = bridge->security_effects.retention_status;

    /* Timing */
    stats->total_updates = internal->total_updates;
    if (internal->replay_verifications > 0) {
        stats->avg_verification_time_us =
            internal->total_verification_time_us /
            (float)internal->replay_verifications;
    }
    stats->uptime_ms = nimcp_time_monotonic_ms() - bridge->creation_time_ms;

    return 0;
}

int security_cl_reset_stats(security_cl_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    internal_stats_t* stats = get_internal_stats(bridge);
    memset(stats, 0, sizeof(internal_stats_t));
    stats->min_retention_observed = 1.0f;

    return 0;
}

/* ============================================================================
 * String Conversion Utilities Implementation
 * ============================================================================ */

const char* security_cl_forgetting_type_to_string(
    security_cl_forgetting_type_t type)
{
    switch (type) {
        case SECURITY_CL_FORGETTING_NONE:           return "none";
        case SECURITY_CL_FORGETTING_GRADIENT_FLOOD: return "gradient_flood";
        case SECURITY_CL_FORGETTING_WEIGHT_ERASURE: return "weight_erasure";
        case SECURITY_CL_FORGETTING_TASK_OVERWRITE: return "task_overwrite";
        case SECURITY_CL_FORGETTING_REPLAY_POISON:  return "replay_poison";
        case SECURITY_CL_FORGETTING_LR_SPIKE:       return "lr_spike";
        default:                                     return "unknown";
    }
}

const char* security_cl_drift_type_to_string(security_cl_drift_type_t type) {
    switch (type) {
        case SECURITY_CL_DRIFT_NONE:        return "none";
        case SECURITY_CL_DRIFT_NATURAL:     return "natural";
        case SECURITY_CL_DRIFT_TASK_SWITCH: return "task_switch";
        case SECURITY_CL_DRIFT_ADVERSARIAL: return "adversarial";
        case SECURITY_CL_DRIFT_MANIPULATION: return "manipulation";
        default:                             return "unknown";
    }
}

const char* security_cl_retention_level_to_string(
    security_cl_retention_level_t level)
{
    switch (level) {
        case SECURITY_CL_RETENTION_HEALTHY:     return "healthy";
        case SECURITY_CL_RETENTION_DEGRADING:   return "degrading";
        case SECURITY_CL_RETENTION_CRITICAL:    return "critical";
        case SECURITY_CL_RETENTION_COMPROMISED: return "compromised";
        default:                                 return "unknown";
    }
}

const char* security_cl_phase_to_string(security_cl_phase_t phase) {
    switch (phase) {
        case SECURITY_CL_PHASE_INACTIVE:   return "inactive";
        case SECURITY_CL_PHASE_MONITORING: return "monitoring";
        case SECURITY_CL_PHASE_PROTECTING: return "protecting";
        case SECURITY_CL_PHASE_DEFENDING:  return "defending";
        case SECURITY_CL_PHASE_RECOVERY:   return "recovery";
        default:                            return "unknown";
    }
}

const char* security_cl_replay_status_to_string(
    security_cl_replay_status_t status)
{
    switch (status) {
        case SECURITY_CL_REPLAY_OK:                 return "ok";
        case SECURITY_CL_REPLAY_HASH_MISMATCH:      return "hash_mismatch";
        case SECURITY_CL_REPLAY_POISON_DETECTED:    return "poison_detected";
        case SECURITY_CL_REPLAY_DISTRIBUTION_SHIFT: return "distribution_shift";
        case SECURITY_CL_REPLAY_TAMPERED:           return "tampered";
        default:                                     return "unknown";
    }
}
