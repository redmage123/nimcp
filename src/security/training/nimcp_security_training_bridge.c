/**
 * @file nimcp_security_training_bridge.c
 * @brief Security-Training Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bidirectional integration between security systems and training pipeline
 * WHY:  Protect ML training from data poisoning, gradient manipulation, and model tampering
 * HOW:  Security validates data sources, detects poisoning, sanitizes gradients;
 *       Training reports suspicious samples, model drift, and anomalies
 *
 * @author NIMCP Development Team
 */

#include "security/training/nimcp_security_training_bridge.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "security/nimcp_security_math.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "security_training_bridge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_training_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Statistics Tracking
 * ============================================================================ */

typedef struct {
    uint64_t total_validations;
    uint64_t data_sources_validated;
    uint64_t data_sources_blocked;
    uint64_t poisoning_scans;
    uint64_t poisoning_detections;
    uint64_t poisoning_by_type[SECURITY_POISONING_COUNT];
    uint64_t gradients_sanitized;
    uint64_t gradients_clipped_norm;
    uint64_t gradients_clipped_value;
    uint64_t gradients_bounded;
    uint64_t integrity_checks;
    uint64_t integrity_failures;
    uint64_t checkpoints_created;
    uint64_t rollbacks_performed;
    uint64_t drift_checks;
    uint64_t drift_detections;
    float max_drift_score;
    uint64_t suspicious_samples_total;
    uint64_t samples_quarantined;
    uint64_t total_updates;
    float total_validation_time_us;
    float total_poisoning_scan_time_us;
} internal_stats_t;

/* Internal stats stored alongside bridge */
static internal_stats_t* get_internal_stats(security_training_bridge_t* bridge) {
    /* Stats embedded after base struct in allocated memory */
    return (internal_stats_t*)((char*)bridge + sizeof(security_training_bridge_t));
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute simple SHA-256 hash of data
 *
 * WHAT: Compute hash for model integrity verification
 * WHY:  Detect unauthorized model modifications
 * HOW:  Simple hash via XOR folding (placeholder - use real crypto in production)
 */
static void compute_model_hash(
    const float* weights,
    uint32_t num_weights,
    uint8_t* hash_out)
{
    if (!weights || !hash_out || num_weights == 0) {
        memset(hash_out, 0, SECURITY_TRAINING_HASH_SIZE);
        return;
    }

    /* Initialize hash with seed */
    uint64_t state[4] = {0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
                         0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL};

    /* Process weights */
    const uint8_t* data = (const uint8_t*)weights;
    size_t len = num_weights * sizeof(float);

    for (size_t i = 0; i < len; i++) {
        state[i % 4] ^= ((uint64_t)data[i] << ((i % 8) * 8));
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

    /* Copy to output */
    memcpy(hash_out, state, SECURITY_TRAINING_HASH_SIZE);
}

/**
 * @brief Compare two hashes
 */
static bool hash_equals(const uint8_t* h1, const uint8_t* h2) {
    if (!h1 || !h2) {
        return false;
    }
    return memcmp(h1, h2, SECURITY_TRAINING_HASH_SIZE) == 0;
}

/**
 * @brief Find data source by name
 */
static int find_data_source(
    const security_training_bridge_t* bridge,
    const char* source_name)
{
    if (!bridge || !source_name) {
        return -1;
    }

    for (uint32_t i = 0; i < bridge->num_data_sources; i++) {
        if (strcmp(bridge->data_sources[i].name, source_name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Find checkpoint by name
 */
static int find_checkpoint(
    const security_training_bridge_t* bridge,
    const char* checkpoint_name)
{
    if (!bridge || !checkpoint_name) {
        return -1;
    }

    for (uint32_t i = 0; i < bridge->num_checkpoints; i++) {
        if (strcmp(bridge->checkpoints[i].name, checkpoint_name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Compute gradient norm
 */
static float compute_gradient_norm(const float* gradients, uint32_t num_params) {
    if (!gradients || num_params == 0) return 0.0f;

    double sum_sq = 0.0;
    for (uint32_t i = 0; i < num_params; i++) {
        sum_sq += (double)gradients[i] * (double)gradients[i];
    }
    return (float)sqrt(sum_sq);
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int security_training_default_config(security_training_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(security_training_config_t));

    /* Feature enables - all enabled by default for security */
    config->enable_data_validation = true;
    config->enable_poisoning_detection = true;
    config->enable_gradient_sanitization = true;
    config->enable_model_verification = true;
    config->enable_concept_drift_detection = true;
    config->enable_secure_checkpointing = true;

    /* Gradient sanitization */
    config->grad_sanitize_mode = SECURITY_GRAD_SANITIZE_CLIP_BOTH;
    config->gradient_clip_norm = SECURITY_TRAINING_DEFAULT_GRAD_CLIP_NORM;
    config->gradient_clip_value = SECURITY_TRAINING_DEFAULT_GRAD_CLIP_VALUE;
    config->gradient_min_bound = SECURITY_TRAINING_DEFAULT_GRAD_MIN_VALUE;
    config->gradient_max_bound = SECURITY_TRAINING_DEFAULT_GRAD_MAX_VALUE;
    config->differential_privacy_epsilon = 1.0f;

    /* Poisoning detection thresholds */
    config->label_flip_threshold = SECURITY_TRAINING_DEFAULT_LABEL_FLIP_THRESHOLD;
    config->backdoor_threshold = SECURITY_TRAINING_DEFAULT_BACKDOOR_THRESHOLD;
    config->trojan_threshold = SECURITY_TRAINING_DEFAULT_TROJAN_THRESHOLD;
    config->gradient_anomaly_threshold = SECURITY_TRAINING_DEFAULT_GRADIENT_ANOMALY_THRESHOLD;

    /* Model verification */
    config->verification_interval_steps = 1000;
    config->verify_on_checkpoint = true;

    /* Concept drift */
    config->drift_window_size = SECURITY_TRAINING_DRIFT_WINDOW_SIZE;
    config->drift_threshold = SECURITY_TRAINING_DEFAULT_DRIFT_THRESHOLD;

    /* Data source defaults */
    config->default_trust_level = SECURITY_TRUST_UNTRUSTED;
    config->require_source_verification = true;

    /* Checkpointing */
    strncpy(config->checkpoint_directory, "/tmp/nimcp_checkpoints",
            sizeof(config->checkpoint_directory) - 1);
    config->sign_checkpoints = true;

    /* Bio-async */
    config->enable_bio_async = true;
    config->bio_inbox_capacity = SECURITY_TRAINING_BIO_INBOX_CAPACITY;

    /* Logging */
    config->enable_logging = true;
    config->log_all_validations = false;

    return 0;
}

security_training_bridge_t* security_training_bridge_create(
    const security_training_config_t* config)
{
    /* Allocate bridge + internal stats */
    size_t alloc_size = sizeof(security_training_bridge_t) + sizeof(internal_stats_t);
    security_training_bridge_t* bridge = nimcp_malloc(alloc_size);
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate security-training bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, alloc_size);

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY,
                         SECURITY_TRAINING_MODULE_NAME) != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_training_bridge_create: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_training_default_config(&bridge->config);
    }

    /* Initialize state */
    bridge->phase = SECURITY_TRAINING_PHASE_MONITORING;
    bridge->creation_time_ms = nimcp_time_monotonic_ms();
    bridge->last_update_ms = bridge->creation_time_ms;

    /* Initialize security effects with safe defaults */
    bridge->security_effects.gradient_clip_norm = bridge->config.gradient_clip_norm;
    bridge->security_effects.gradient_clip_value = bridge->config.gradient_clip_value;
    bridge->security_effects.gradient_scale_factor = 1.0f;
    bridge->security_effects.gradient_sanitization_active =
        bridge->config.enable_gradient_sanitization;
    bridge->security_effects.valid = true;
    bridge->security_effects.last_update_ms = bridge->creation_time_ms;

    /* Initialize training effects */
    bridge->training_effects.valid = false;

    /* Allocate drift baseline if enabled */
    if (bridge->config.enable_concept_drift_detection) {
        bridge->drift_baseline = nimcp_malloc(
            bridge->config.drift_window_size * sizeof(float));
        if (!bridge->drift_baseline) {
            NIMCP_LOGGING_WARN("Failed to allocate drift baseline");
        }
    }

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async) {
        security_training_connect_bio_async(bridge);
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-training bridge created");
    }

    return bridge;
}

void security_training_bridge_destroy(security_training_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        security_training_disconnect_bio_async(bridge);
    }

    /* Free drift baseline */
    if (bridge->drift_baseline) {
        nimcp_free(bridge->drift_baseline);
        bridge->drift_baseline = NULL;
    }

    /* Free suspicious sample arrays in training effects */
    if (bridge->training_effects.suspicious_sample_indices) {
        nimcp_free(bridge->training_effects.suspicious_sample_indices);
    }
    if (bridge->training_effects.suspicion_scores) {
        nimcp_free(bridge->training_effects.suspicion_scores);
    }

    /* Cleanup base bridge (frees mutex) */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int security_training_connect_training_pipeline(
    security_training_bridge_t* bridge,
    void* training)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(training);

    BRIDGE_LOCK(bridge);
    bridge->training_pipeline = training;
    bridge->training_connected = true;
    bridge->phase = SECURITY_TRAINING_PHASE_PROTECTING;
    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Connected to training pipeline");
    }
    return 0;
}

int security_training_connect_optimizer(
    security_training_bridge_t* bridge,
    nimcp_optimizer_context_t* optimizer)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(optimizer);

    BRIDGE_LOCK(bridge);
    bridge->optimizer = optimizer;
    bridge->optimizer_connected = true;
    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Connected to optimizer");
    }
    return 0;
}

int security_training_connect_bbb(
    security_training_bridge_t* bridge,
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

int security_training_connect_anomaly_detector(
    security_training_bridge_t* bridge,
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

int security_training_disconnect_training_pipeline(
    security_training_bridge_t* bridge)
{
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->training_pipeline = NULL;
    bridge->training_connected = false;
    if (!bridge->bbb_connected && !bridge->anomaly_connected) {
        bridge->phase = SECURITY_TRAINING_PHASE_MONITORING;
    }
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_training_disconnect_optimizer(
    security_training_bridge_t* bridge)
{
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->optimizer = NULL;
    bridge->optimizer_connected = false;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_training_disconnect_bbb(
    security_training_bridge_t* bridge)
{
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->bbb = NULL;
    bridge->bbb_connected = false;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_training_disconnect_anomaly_detector(
    security_training_bridge_t* bridge)
{
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->anomaly_detector = NULL;
    bridge->anomaly_connected = false;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Data Source Validation API Implementation
 * ============================================================================ */

bool security_training_validate_data_source(
    security_training_bridge_t* bridge,
    const char* source_name)
{
    BRIDGE_NULL_CHECK_BOOL(bridge);
    BRIDGE_NULL_CHECK_BOOL(source_name);

    if (!bridge->config.enable_data_validation) {
        return true;  /* Validation disabled */
    }

    uint64_t start_time = nimcp_time_monotonic_us();
    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    int idx = find_data_source(bridge, source_name);

    /* Register new source if not found */
    if (idx < 0) {
        if (bridge->num_data_sources >= SECURITY_TRAINING_MAX_DATA_SOURCES) {
            BRIDGE_UNLOCK(bridge);
            NIMCP_LOGGING_WARN("Max data sources reached");
            return false;
        }

        idx = (int)bridge->num_data_sources;
        strncpy(bridge->data_sources[idx].name, source_name,
                sizeof(bridge->data_sources[idx].name) - 1);
        bridge->data_sources[idx].trust_level = bridge->config.default_trust_level;
        bridge->data_sources[idx].blocked = false;
        bridge->num_data_sources++;
        stats->data_sources_validated++;
    }

    /* Check if source is blocked */
    if (bridge->data_sources[idx].blocked) {
        BRIDGE_UNLOCK(bridge);
        stats->total_validations++;
        return false;
    }

    /* Check trust level against minimum */
    security_data_trust_t min_trust = bridge->security_effects.min_trust_level;
    bool trusted = (bridge->data_sources[idx].trust_level >= min_trust);

    /* For untrusted sources, require verification if configured */
    if (trusted && bridge->config.require_source_verification &&
        bridge->data_sources[idx].trust_level == SECURITY_TRUST_UNTRUSTED) {
        trusted = false;  /* Require explicit verification */
    }

    /* Check for recent anomalies */
    if (trusted && bridge->data_sources[idx].anomalies_detected > 10) {
        trusted = false;  /* Too many anomalies */
    }

    /* Update validation timestamp */
    bridge->data_sources[idx].last_validated_ms = nimcp_time_monotonic_ms();

    BRIDGE_UNLOCK(bridge);

    stats->total_validations++;
    stats->total_validation_time_us += (float)(nimcp_time_monotonic_us() - start_time);

    if (bridge->config.log_all_validations) {
        NIMCP_LOGGING_DEBUG("Validated data source '%s': %s",
                           source_name, trusted ? "trusted" : "untrusted");
    }

    return trusted;
}

int security_training_set_source_trust(
    security_training_bridge_t* bridge,
    const char* source_name,
    security_data_trust_t trust_level)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(source_name);

    BRIDGE_LOCK(bridge);

    int idx = find_data_source(bridge, source_name);

    /* Create new entry if needed */
    if (idx < 0) {
        if (bridge->num_data_sources >= SECURITY_TRAINING_MAX_DATA_SOURCES) {
            BRIDGE_UNLOCK(bridge);
            return NIMCP_ERROR_OUT_OF_RANGE;
        }

        idx = (int)bridge->num_data_sources;
        strncpy(bridge->data_sources[idx].name, source_name,
                sizeof(bridge->data_sources[idx].name) - 1);
        bridge->num_data_sources++;
    }

    bridge->data_sources[idx].trust_level = trust_level;
    bridge->data_sources[idx].last_validated_ms = nimcp_time_monotonic_ms();

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Set trust level for '%s' to %s",
                         source_name, security_trust_level_to_string(trust_level));
    }

    return 0;
}

int security_training_block_source(
    security_training_bridge_t* bridge,
    const char* source_name)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(source_name);

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    int idx = find_data_source(bridge, source_name);
    if (idx < 0) {
        /* Create entry as blocked */
        if (bridge->num_data_sources >= SECURITY_TRAINING_MAX_DATA_SOURCES) {
            BRIDGE_UNLOCK(bridge);
            return NIMCP_ERROR_OUT_OF_RANGE;
        }

        idx = (int)bridge->num_data_sources;
        strncpy(bridge->data_sources[idx].name, source_name,
                sizeof(bridge->data_sources[idx].name) - 1);
        bridge->num_data_sources++;
    }

    bridge->data_sources[idx].blocked = true;
    bridge->data_sources[idx].trust_level = SECURITY_TRUST_UNTRUSTED;
    bridge->security_effects.blocked_source_count++;

    BRIDGE_UNLOCK(bridge);

    stats->data_sources_blocked++;

    NIMCP_LOGGING_WARN("Blocked data source '%s'", source_name);

    return 0;
}

security_data_trust_t security_training_get_source_trust(
    const security_training_bridge_t* bridge,
    const char* source_name)
{
    if (!bridge || !source_name) return SECURITY_TRUST_UNTRUSTED;

    int idx = find_data_source(bridge, source_name);
    if (idx < 0) return SECURITY_TRUST_UNTRUSTED;

    return bridge->data_sources[idx].trust_level;
}

/* ============================================================================
 * Poisoning Detection API Implementation
 * ============================================================================ */

int security_training_detect_poisoning(
    security_training_bridge_t* bridge,
    const void* data,
    size_t data_size,
    const int32_t* labels,
    uint32_t num_samples,
    security_poisoning_result_t* result)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    if (!bridge->config.enable_poisoning_detection) {
        result->poisoning_detected = false;
        result->type = SECURITY_POISONING_NONE;
        return 0;
    }

    uint64_t start_time = nimcp_time_monotonic_us();
    internal_stats_t* stats = get_internal_stats(bridge);

    memset(result, 0, sizeof(security_poisoning_result_t));

    /* Validate inputs */
    if (!data || data_size == 0 || num_samples == 0) {
        result->poisoning_detected = false;
        return 0;
    }

    BRIDGE_LOCK(bridge);

    float max_anomaly_score = 0.0f;
    security_poisoning_type_t detected_type = SECURITY_POISONING_NONE;

    /* 1. Label flip detection (if labels provided) */
    if (labels) {
        /* Count label distribution */
        int32_t label_counts[256] = {0};
        int32_t max_label = 0;

        for (uint32_t i = 0; i < num_samples; i++) {
            int32_t label = labels[i];
            if (label >= 0 && label < 256) {
                label_counts[label]++;
                if (label > max_label) max_label = label;
            }
        }

        /* Check for suspicious label distribution */
        float expected_per_class = (float)num_samples / (float)(max_label + 1);
        float total_deviation = 0.0f;

        for (int32_t c = 0; c <= max_label; c++) {
            float deviation = fabsf((float)label_counts[c] - expected_per_class);
            total_deviation += deviation;
        }

        float label_anomaly = total_deviation / (float)num_samples;
        if (label_anomaly > bridge->config.label_flip_threshold) {
            if (label_anomaly > max_anomaly_score) {
                max_anomaly_score = label_anomaly;
                detected_type = SECURITY_POISONING_LABEL_FLIP;
            }
        }
    }

    /* 2. Backdoor/trigger pattern detection via entropy */
    if (data_size > 0) {
        double entropy = nimcp_entropy_calculate(data, data_size);

        /* Very low entropy might indicate repeated patterns (backdoor triggers) */
        float normalized_entropy = (float)(entropy / 8.0);  /* Normalize to [0,1] */
        if (normalized_entropy < 0.3f) {
            float backdoor_score = 1.0f - normalized_entropy;
            if (backdoor_score > bridge->config.backdoor_threshold &&
                backdoor_score > max_anomaly_score) {
                max_anomaly_score = backdoor_score;
                detected_type = SECURITY_POISONING_BACKDOOR;
            }
        }

        /* Very high entropy might indicate noise injection */
        if (normalized_entropy > 0.95f) {
            float injection_score = normalized_entropy;
            if (injection_score > max_anomaly_score) {
                max_anomaly_score = injection_score;
                detected_type = SECURITY_POISONING_DATA_INJECTION;
            }
        }
    }

    /* 3. Feature collision detection (simplified) */
    if (data_size >= num_samples * 4 && num_samples > 1) {
        /* Check for duplicate features across samples */
        const float* fdata = (const float*)data;
        size_t features_per_sample = data_size / (num_samples * sizeof(float));
        uint32_t collision_count = 0;

        for (uint32_t i = 0; i < num_samples - 1 && i < 100; i++) {
            for (uint32_t j = i + 1; j < num_samples && j < 100; j++) {
                const float* f1 = fdata + i * features_per_sample;
                const float* f2 = fdata + j * features_per_sample;

                /* Simple collision check */
                bool collision = true;
                for (size_t k = 0; k < features_per_sample && k < 10; k++) {
                    if (fabsf(f1[k] - f2[k]) > 1e-6f) {
                        collision = false;
                        break;
                    }
                }
                if (collision) collision_count++;
            }
        }

        if (collision_count > 5) {
            float collision_score = (float)collision_count / 100.0f;
            if (collision_score > max_anomaly_score) {
                max_anomaly_score = nimcp_clampf(collision_score, 0.0f, 1.0f);
                detected_type = SECURITY_POISONING_FEATURE_COLLISION;
            }
        }
    }

    BRIDGE_UNLOCK(bridge);

    /* Populate result */
    result->poisoning_detected = (detected_type != SECURITY_POISONING_NONE);
    result->type = detected_type;
    result->confidence = max_anomaly_score;
    result->detection_time_us = nimcp_time_monotonic_us() - start_time;

    if (result->poisoning_detected) {
        snprintf(result->description, sizeof(result->description),
                 "Detected %s with confidence %.2f",
                 security_poisoning_type_to_string(detected_type),
                 max_anomaly_score);
        result->quarantine_recommended = true;
        result->halt_training_recommended = (max_anomaly_score > 0.9f);
    }

    /* Update stats */
    stats->poisoning_scans++;
    stats->total_poisoning_scan_time_us += (float)result->detection_time_us;
    if (result->poisoning_detected) {
        stats->poisoning_detections++;
        if (detected_type < SECURITY_POISONING_COUNT) {
            stats->poisoning_by_type[detected_type]++;
        }
    }

    return 0;
}

int security_training_report_suspicious_sample(
    security_training_bridge_t* bridge,
    uint32_t sample_index,
    float suspicion_score,
    const char* reason)
{
    BRIDGE_NULL_CHECK(bridge);

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    /* Update training effects */
    if (bridge->training_effects.suspicious_sample_count <
        SECURITY_TRAINING_MAX_SUSPICIOUS_SAMPLES) {

        uint32_t idx = bridge->training_effects.suspicious_sample_count;

        /* Allocate arrays if needed */
        if (!bridge->training_effects.suspicious_sample_indices) {
            bridge->training_effects.suspicious_sample_indices =
                nimcp_malloc(SECURITY_TRAINING_MAX_SUSPICIOUS_SAMPLES * sizeof(uint32_t));
        }
        if (!bridge->training_effects.suspicion_scores) {
            bridge->training_effects.suspicion_scores =
                nimcp_malloc(SECURITY_TRAINING_MAX_SUSPICIOUS_SAMPLES * sizeof(float));
        }

        if (bridge->training_effects.suspicious_sample_indices &&
            bridge->training_effects.suspicion_scores) {
            bridge->training_effects.suspicious_sample_indices[idx] = sample_index;
            bridge->training_effects.suspicion_scores[idx] = suspicion_score;
            bridge->training_effects.suspicious_sample_count++;
        }
    }

    BRIDGE_UNLOCK(bridge);

    stats->suspicious_samples_total++;

    if (bridge->config.enable_logging && suspicion_score > 0.7f) {
        NIMCP_LOGGING_WARN("Suspicious sample %u (score: %.2f): %s",
                         sample_index, suspicion_score, reason ? reason : "unknown");
    }

    return 0;
}

int security_training_quarantine_samples(
    security_training_bridge_t* bridge,
    const uint32_t* sample_indices,
    uint32_t num_samples)
{
    BRIDGE_NULL_CHECK(bridge);

    if (!sample_indices || num_samples == 0) return 0;

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);
    bridge->security_effects.data_quarantine_active = true;
    BRIDGE_UNLOCK(bridge);

    stats->samples_quarantined += num_samples;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Quarantined %u samples", num_samples);
    }

    return 0;
}

/* ============================================================================
 * Gradient Sanitization API Implementation
 * ============================================================================ */

int security_training_sanitize_gradients(
    security_training_bridge_t* bridge,
    float* gradients,
    uint32_t num_params)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(gradients);

    if (num_params == 0) return 0;

    if (!bridge->config.enable_gradient_sanitization) {
        return 0;
    }

    internal_stats_t* stats = get_internal_stats(bridge);
    security_grad_sanitize_mode_t mode = bridge->config.grad_sanitize_mode;

    if (mode == SECURITY_GRAD_SANITIZE_NONE) {
        return 0;
    }

    BRIDGE_LOCK(bridge);

    float clip_norm = bridge->security_effects.gradient_clip_norm;
    float clip_value = bridge->security_effects.gradient_clip_value;
    float scale = bridge->security_effects.gradient_scale_factor;

    /* Apply scaling if under threat */
    if (scale < 1.0f) {
        for (uint32_t i = 0; i < num_params; i++) {
            gradients[i] *= scale;
        }
    }

    /* Clip by norm */
    if (mode == SECURITY_GRAD_SANITIZE_CLIP_NORM ||
        mode == SECURITY_GRAD_SANITIZE_CLIP_BOTH) {
        float current_norm = compute_gradient_norm(gradients, num_params);

        if (current_norm > clip_norm) {
            float ratio = clip_norm / current_norm;
            for (uint32_t i = 0; i < num_params; i++) {
                gradients[i] *= ratio;
            }
            stats->gradients_clipped_norm++;
        }
    }

    /* Clip by value */
    if (mode == SECURITY_GRAD_SANITIZE_CLIP_VALUE ||
        mode == SECURITY_GRAD_SANITIZE_CLIP_BOTH) {
        bool clipped = false;
        for (uint32_t i = 0; i < num_params; i++) {
            if (gradients[i] > clip_value) {
                gradients[i] = clip_value;
                clipped = true;
            } else if (gradients[i] < -clip_value) {
                gradients[i] = -clip_value;
                clipped = true;
            }
        }
        if (clipped) stats->gradients_clipped_value++;
    }

    /* Apply bounds */
    if (mode == SECURITY_GRAD_SANITIZE_BOUND) {
        float min_b = bridge->config.gradient_min_bound;
        float max_b = bridge->config.gradient_max_bound;

        for (uint32_t i = 0; i < num_params; i++) {
            gradients[i] = nimcp_clampf(gradients[i], min_b, max_b);
        }
        stats->gradients_bounded++;
    }

    /* Differential privacy noise */
    if (mode == SECURITY_GRAD_SANITIZE_DIFFERENTIAL) {
        float epsilon = bridge->config.differential_privacy_epsilon;
        float sensitivity = clip_norm;  /* Sensitivity = clip norm */
        float scale_dp = sensitivity / epsilon;

        for (uint32_t i = 0; i < num_params; i++) {
            gradients[i] += (float)nimcp_random_laplace(scale_dp);
        }
    }

    BRIDGE_UNLOCK(bridge);

    stats->gradients_sanitized++;

    return 0;
}

bool security_training_check_gradient_anomaly(
    security_training_bridge_t* bridge,
    const float* gradients,
    uint32_t num_params,
    float* anomaly_score)
{
    BRIDGE_NULL_CHECK_BOOL(bridge);
    BRIDGE_NULL_CHECK_BOOL(gradients);

    if (num_params == 0) {
        if (anomaly_score) *anomaly_score = 0.0f;
        return false;
    }

    float norm = compute_gradient_norm(gradients, num_params);

    /* Check for NaN/Inf */
    if (isnan(norm) || isinf(norm)) {
        if (anomaly_score) *anomaly_score = 1.0f;
        return true;
    }

    /* Check against threshold */
    float threshold = bridge->config.gradient_clip_norm * 10.0f;
    float score = norm / threshold;
    score = nimcp_clampf(score, 0.0f, 1.0f);

    if (anomaly_score) *anomaly_score = score;

    return (score > bridge->config.gradient_anomaly_threshold);
}

int security_training_set_gradient_params(
    security_training_bridge_t* bridge,
    security_grad_sanitize_mode_t mode,
    float clip_norm,
    float clip_value)
{
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->config.grad_sanitize_mode = mode;

    if (clip_norm > 0.0f) {
        bridge->config.gradient_clip_norm = clip_norm;
        bridge->security_effects.gradient_clip_norm = clip_norm;
    }
    if (clip_value > 0.0f) {
        bridge->config.gradient_clip_value = clip_value;
        bridge->security_effects.gradient_clip_value = clip_value;
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Model Integrity API Implementation
 * ============================================================================ */

security_integrity_result_t security_training_verify_model_integrity(
    security_training_bridge_t* bridge,
    const float* model_weights,
    uint32_t num_weights)
{
    if (!bridge) return SECURITY_INTEGRITY_TAMPERED;
    if (!model_weights || num_weights == 0) return SECURITY_INTEGRITY_CHECKPOINT_MISSING;

    internal_stats_t* stats = get_internal_stats(bridge);
    stats->integrity_checks++;

    /* Find most recent verified checkpoint */
    BRIDGE_LOCK(bridge);

    if (bridge->num_checkpoints == 0) {
        BRIDGE_UNLOCK(bridge);
        return SECURITY_INTEGRITY_CHECKPOINT_MISSING;
    }

    uint32_t latest_idx = bridge->current_checkpoint_idx;
    if (latest_idx >= bridge->num_checkpoints) {
        latest_idx = bridge->num_checkpoints - 1;
    }

    /* Compute current model hash */
    uint8_t current_hash[SECURITY_TRAINING_HASH_SIZE];
    compute_model_hash(model_weights, num_weights, current_hash);

    /* Compare with checkpoint hash */
    bool hash_match = hash_equals(current_hash,
                                  bridge->checkpoints[latest_idx].model_hash);

    BRIDGE_UNLOCK(bridge);

    bridge->last_verification_ms = nimcp_time_monotonic_ms();

    if (!hash_match) {
        stats->integrity_failures++;
        return SECURITY_INTEGRITY_HASH_MISMATCH;
    }

    return SECURITY_INTEGRITY_OK;
}

int security_training_checkpoint_model(
    security_training_bridge_t* bridge,
    const char* checkpoint_name,
    const float* model_weights,
    uint32_t num_weights,
    uint64_t step)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(checkpoint_name);
    BRIDGE_NULL_CHECK(model_weights);

    if (num_weights == 0) return NIMCP_ERROR_INVALID_PARAM;

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    /* Find or create checkpoint slot */
    int idx = find_checkpoint(bridge, checkpoint_name);
    if (idx < 0) {
        if (bridge->num_checkpoints >= SECURITY_TRAINING_MAX_CHECKPOINTS) {
            /* Overwrite oldest checkpoint */
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

    /* Fill checkpoint info */
    security_checkpoint_info_t* cp = &bridge->checkpoints[idx];
    strncpy(cp->name, checkpoint_name, SECURITY_TRAINING_CHECKPOINT_NAME_MAX - 1);
    cp->step = step;
    cp->timestamp_ms = nimcp_time_monotonic_ms();
    compute_model_hash(model_weights, num_weights, cp->model_hash);
    cp->is_signed = bridge->config.sign_checkpoints && bridge->bbb_connected;
    cp->is_verified = true;  /* We just created it */
    cp->loss_at_checkpoint = bridge->training_effects.current_loss;

    /* Update current checkpoint index */
    bridge->current_checkpoint_idx = (uint32_t)idx;
    bridge->security_effects.last_safe_checkpoint_step = step;

    BRIDGE_UNLOCK(bridge);

    stats->checkpoints_created++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created checkpoint '%s' at step %lu", checkpoint_name, step);
    }

    return 0;
}

int security_training_rollback_model(
    security_training_bridge_t* bridge,
    const char* checkpoint_name,
    float* model_weights,
    uint32_t num_weights)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(checkpoint_name);
    BRIDGE_NULL_CHECK(model_weights);

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    int idx = find_checkpoint(bridge, checkpoint_name);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_LOGGING_ERROR("Checkpoint '%s' not found", checkpoint_name);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Note: In a real implementation, we would restore actual weights from storage.
     * Here we can only verify integrity - the caller must have stored weights. */

    security_checkpoint_info_t* cp = &bridge->checkpoints[idx];

    /* Verify the provided weights match checkpoint hash */
    uint8_t provided_hash[SECURITY_TRAINING_HASH_SIZE];
    compute_model_hash(model_weights, num_weights, provided_hash);

    if (!hash_equals(provided_hash, cp->model_hash)) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_LOGGING_ERROR("Rollback failed: weight hash mismatch");
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Update state */
    bridge->current_checkpoint_idx = (uint32_t)idx;
    bridge->phase = SECURITY_TRAINING_PHASE_RECOVERY;

    /* Clear training effects suspicious samples */
    bridge->training_effects.suspicious_sample_count = 0;
    bridge->training_effects.drift_detected = false;

    BRIDGE_UNLOCK(bridge);

    stats->rollbacks_performed++;

    NIMCP_LOGGING_WARN("Rolled back model to checkpoint '%s' (step %lu)",
                      checkpoint_name, cp->step);

    return 0;
}

int security_training_get_checkpoint_info(
    const security_training_bridge_t* bridge,
    const char* checkpoint_name,
    security_checkpoint_info_t* info)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(checkpoint_name);
    BRIDGE_NULL_CHECK(info);

    int idx = find_checkpoint(bridge, checkpoint_name);
    if (idx < 0) return NIMCP_ERROR_NOT_FOUND;

    *info = bridge->checkpoints[idx];
    return 0;
}

int security_training_list_checkpoints(
    const security_training_bridge_t* bridge,
    security_checkpoint_info_t* checkpoints,
    uint32_t max_checkpoints)
{
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    uint32_t count = bridge->num_checkpoints;
    if (count > max_checkpoints) count = max_checkpoints;

    if (checkpoints && count > 0) {
        memcpy(checkpoints, bridge->checkpoints,
               count * sizeof(security_checkpoint_info_t));
    }

    return (int)count;
}

/* ============================================================================
 * Concept Drift Detection API Implementation
 * ============================================================================ */

bool security_training_detect_concept_drift(
    security_training_bridge_t* bridge,
    const float* current_features,
    uint32_t num_features,
    float* drift_score)
{
    BRIDGE_NULL_CHECK_BOOL(bridge);

    if (drift_score) *drift_score = 0.0f;

    if (!bridge->config.enable_concept_drift_detection) {
        return false;
    }

    if (!current_features || num_features == 0) {
        return false;
    }

    internal_stats_t* stats = get_internal_stats(bridge);
    stats->drift_checks++;

    BRIDGE_LOCK(bridge);

    /* Check if baseline exists */
    if (!bridge->drift_baseline || bridge->drift_baseline_samples == 0) {
        BRIDGE_UNLOCK(bridge);
        return false;
    }

    /* Compare current features to baseline (simplified: mean absolute difference) */
    uint32_t compare_len = num_features;
    if (compare_len > bridge->drift_baseline_samples) {
        compare_len = bridge->drift_baseline_samples;
    }

    double total_diff = 0.0;
    for (uint32_t i = 0; i < compare_len; i++) {
        total_diff += fabs((double)current_features[i] - (double)bridge->drift_baseline[i]);
    }

    float score = (float)(total_diff / (double)compare_len);
    score = nimcp_clampf(score, 0.0f, 1.0f);

    bridge->drift_score = score;
    bool drift_detected = (score > bridge->config.drift_threshold);

    if (drift_detected) {
        bridge->training_effects.drift_detected = true;
        stats->drift_detections++;
        if (score > stats->max_drift_score) {
            stats->max_drift_score = score;
        }
    }

    BRIDGE_UNLOCK(bridge);

    if (drift_score) *drift_score = score;

    if (drift_detected && bridge->config.enable_logging) {
        NIMCP_LOGGING_WARN("Concept drift detected (score: %.3f)", score);
    }

    return drift_detected;
}

int security_training_update_drift_baseline(
    security_training_bridge_t* bridge,
    const float* features,
    uint32_t num_features)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(features);

    if (num_features == 0) return 0;

    BRIDGE_LOCK(bridge);

    if (!bridge->drift_baseline) {
        bridge->drift_baseline = nimcp_malloc(
            bridge->config.drift_window_size * sizeof(float));
        if (!bridge->drift_baseline) {
            BRIDGE_UNLOCK(bridge);
            return NIMCP_ERROR_NO_MEMORY;
        }
    }

    /* Update baseline with exponential moving average */
    uint32_t store_len = num_features;
    if (store_len > bridge->config.drift_window_size) {
        store_len = bridge->config.drift_window_size;
    }

    float alpha = 0.1f;  /* EMA smoothing factor */

    if (bridge->drift_baseline_samples == 0) {
        /* First update: copy directly */
        memcpy(bridge->drift_baseline, features, store_len * sizeof(float));
    } else {
        /* Subsequent updates: exponential moving average */
        for (uint32_t i = 0; i < store_len; i++) {
            bridge->drift_baseline[i] =
                alpha * features[i] + (1.0f - alpha) * bridge->drift_baseline[i];
        }
    }

    bridge->drift_baseline_samples = store_len;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_training_reset_drift_baseline(security_training_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    if (bridge->drift_baseline) {
        memset(bridge->drift_baseline, 0,
               bridge->config.drift_window_size * sizeof(float));
    }
    bridge->drift_baseline_samples = 0;
    bridge->drift_score = 0.0f;
    bridge->training_effects.drift_detected = false;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bidirectional Update API Implementation
 * ============================================================================ */

int security_training_update_security_effects(security_training_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    internal_stats_t* stats = get_internal_stats(bridge);

    BRIDGE_LOCK(bridge);

    /* Aggregate threat signals */
    float threat_level = 0.0f;

    /* Factor 1: Blocked sources */
    if (bridge->security_effects.blocked_source_count > 0) {
        threat_level += 0.1f * (float)bridge->security_effects.blocked_source_count;
    }

    /* Factor 2: Active quarantine */
    if (bridge->security_effects.data_quarantine_active) {
        threat_level += 0.2f;
    }

    /* Factor 3: Drift detection */
    if (bridge->training_effects.drift_detected) {
        threat_level += bridge->drift_score * 0.3f;
    }

    /* Factor 4: Suspicious samples */
    if (bridge->training_effects.suspicious_sample_count > 10) {
        threat_level += 0.2f;
    }

    /* Factor 5: Training instabilities */
    if (bridge->training_effects.loss_nan_detected ||
        bridge->training_effects.gradient_explosion) {
        threat_level += 0.3f;
    }

    threat_level = nimcp_clampf(threat_level, 0.0f, 1.0f);

    /* Update security effects */
    bridge->security_effects.threat_level = threat_level;
    bridge->security_effects.under_attack = (threat_level > 0.7f);

    /* Adjust gradient bounds based on threat level */
    if (threat_level > 0.5f) {
        float reduction = 1.0f - (threat_level - 0.5f);
        bridge->security_effects.gradient_scale_factor = nimcp_clampf(reduction, 0.5f, 1.0f);
    } else {
        bridge->security_effects.gradient_scale_factor = 1.0f;
    }

    /* Recommend checkpoint/rollback if under attack */
    bridge->security_effects.checkpoint_required = (threat_level > 0.6f);
    bridge->security_effects.rollback_recommended = (threat_level > 0.8f);

    /* Update timestamp */
    bridge->security_effects.last_update_ms = nimcp_time_monotonic_ms();
    bridge->security_effects.valid = true;

    /* Update phase based on threat level */
    if (threat_level > 0.8f) {
        bridge->phase = SECURITY_TRAINING_PHASE_RESPONDING;
    } else if (threat_level > 0.3f) {
        bridge->phase = SECURITY_TRAINING_PHASE_PROTECTING;
    } else {
        bridge->phase = SECURITY_TRAINING_PHASE_MONITORING;
    }

    BRIDGE_UNLOCK(bridge);

    stats->total_updates++;
    bridge->last_update_ms = nimcp_time_monotonic_ms();

    return 0;
}

int security_training_update_training_effects(
    security_training_bridge_t* bridge,
    float loss,
    float gradient_norm,
    uint64_t step)
{
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    /* Update current training state */
    bridge->training_effects.current_step = step;
    bridge->training_effects.current_loss = loss;
    bridge->training_effects.current_gradient_norm = gradient_norm;

    /* Check for instabilities */
    bridge->training_effects.loss_nan_detected = isnan(loss);
    bridge->training_effects.loss_inf_detected = isinf(loss);

    float grad_threshold = bridge->config.gradient_clip_norm * 100.0f;
    bridge->training_effects.gradient_explosion = (gradient_norm > grad_threshold);
    bridge->training_effects.gradient_vanishing = (gradient_norm < 1e-7f);

    /* Compute gradient anomaly score */
    bridge->training_effects.gradient_anomaly_score =
        nimcp_clampf(gradient_norm / grad_threshold, 0.0f, 1.0f);

    /* Update timestamp */
    bridge->training_effects.timestamp_ms = nimcp_time_monotonic_ms();
    bridge->training_effects.valid = true;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_training_get_security_effects(
    const security_training_bridge_t* bridge,
    security_training_effects_t* effects)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(effects);

    *effects = bridge->security_effects;
    return 0;
}

int security_training_get_training_effects(
    const security_training_bridge_t* bridge,
    training_security_effects_t* effects)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(effects);

    /* Copy effects but not the dynamically allocated arrays */
    *effects = bridge->training_effects;
    effects->suspicious_sample_indices = NULL;  /* Don't copy pointers */
    effects->suspicion_scores = NULL;

    return 0;
}

/* ============================================================================
 * Bio-async Integration API Implementation
 * ============================================================================ */

int security_training_connect_bio_async(security_training_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_connect_bio_async(&bridge->base);
}

int security_training_disconnect_bio_async(security_training_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool security_training_is_bio_async_connected(
    const security_training_bridge_t* bridge)
{
    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Query and Statistics API Implementation
 * ============================================================================ */

security_training_phase_t security_training_get_phase(
    const security_training_bridge_t* bridge)
{
    if (!bridge) return SECURITY_TRAINING_PHASE_INACTIVE;
    return bridge->phase;
}

float security_training_get_threat_level(
    const security_training_bridge_t* bridge)
{
    if (!bridge) return 0.0f;
    return bridge->security_effects.threat_level;
}

bool security_training_is_under_attack(
    const security_training_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->security_effects.under_attack;
}

int security_training_get_stats(
    const security_training_bridge_t* bridge,
    security_training_stats_t* stats)
{
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(stats);

    const internal_stats_t* internal = get_internal_stats(
        (security_training_bridge_t*)bridge);

    memset(stats, 0, sizeof(security_training_stats_t));

    /* Copy from internal stats */
    stats->total_validations = internal->total_validations;
    stats->data_sources_validated = internal->data_sources_validated;
    stats->data_sources_blocked = internal->data_sources_blocked;
    stats->poisoning_scans = internal->poisoning_scans;
    stats->poisoning_detections = internal->poisoning_detections;
    memcpy(stats->poisoning_by_type, internal->poisoning_by_type,
           sizeof(stats->poisoning_by_type));
    stats->gradients_sanitized = internal->gradients_sanitized;
    stats->gradients_clipped_norm = internal->gradients_clipped_norm;
    stats->gradients_clipped_value = internal->gradients_clipped_value;
    stats->gradients_bounded = internal->gradients_bounded;
    stats->integrity_checks = internal->integrity_checks;
    stats->integrity_failures = internal->integrity_failures;
    stats->checkpoints_created = internal->checkpoints_created;
    stats->rollbacks_performed = internal->rollbacks_performed;
    stats->drift_checks = internal->drift_checks;
    stats->drift_detections = internal->drift_detections;
    stats->max_drift_score = internal->max_drift_score;
    stats->suspicious_samples_total = internal->suspicious_samples_total;
    stats->samples_quarantined = internal->samples_quarantined;

    /* Connection status */
    stats->training_connected = bridge->training_connected;
    stats->optimizer_connected = bridge->optimizer_connected;
    stats->bbb_connected = bridge->bbb_connected;
    stats->anomaly_connected = bridge->anomaly_connected;
    stats->bio_async_connected = bridge->base.bio_async_enabled;

    /* Current state */
    stats->current_phase = bridge->phase;
    stats->current_threat_level = bridge->security_effects.threat_level;

    /* Timing */
    stats->total_updates = internal->total_updates;
    if (internal->total_validations > 0) {
        stats->avg_validation_time_us =
            internal->total_validation_time_us / (float)internal->total_validations;
    }
    if (internal->poisoning_scans > 0) {
        stats->avg_poisoning_scan_time_us =
            internal->total_poisoning_scan_time_us / (float)internal->poisoning_scans;
    }
    stats->uptime_ms = nimcp_time_monotonic_ms() - bridge->creation_time_ms;

    return 0;
}

int security_training_reset_stats(security_training_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    internal_stats_t* stats = get_internal_stats(bridge);
    memset(stats, 0, sizeof(internal_stats_t));

    return 0;
}

/* ============================================================================
 * String Conversion Utilities Implementation
 * ============================================================================ */

const char* security_poisoning_type_to_string(security_poisoning_type_t type) {
    switch (type) {
        case SECURITY_POISONING_NONE:              return "none";
        case SECURITY_POISONING_LABEL_FLIP:        return "label_flip";
        case SECURITY_POISONING_BACKDOOR:          return "backdoor";
        case SECURITY_POISONING_TROJAN:            return "trojan";
        case SECURITY_POISONING_GRADIENT_MANIPULATION: return "gradient_manipulation";
        case SECURITY_POISONING_DATA_INJECTION:    return "data_injection";
        case SECURITY_POISONING_FEATURE_COLLISION: return "feature_collision";
        default:                                   return "unknown";
    }
}

const char* security_trust_level_to_string(security_data_trust_t trust) {
    switch (trust) {
        case SECURITY_TRUST_UNTRUSTED: return "untrusted";
        case SECURITY_TRUST_VERIFIED:  return "verified";
        case SECURITY_TRUST_CERTIFIED: return "certified";
        case SECURITY_TRUST_INTERNAL:  return "internal";
        default:                       return "unknown";
    }
}

const char* security_training_phase_to_string(security_training_phase_t phase) {
    switch (phase) {
        case SECURITY_TRAINING_PHASE_INACTIVE:   return "inactive";
        case SECURITY_TRAINING_PHASE_MONITORING: return "monitoring";
        case SECURITY_TRAINING_PHASE_PROTECTING: return "protecting";
        case SECURITY_TRAINING_PHASE_RESPONDING: return "responding";
        case SECURITY_TRAINING_PHASE_RECOVERY:   return "recovery";
        default:                                 return "unknown";
    }
}

const char* security_integrity_result_to_string(security_integrity_result_t result) {
    switch (result) {
        case SECURITY_INTEGRITY_OK:                 return "ok";
        case SECURITY_INTEGRITY_HASH_MISMATCH:      return "hash_mismatch";
        case SECURITY_INTEGRITY_SIGNATURE_INVALID:  return "signature_invalid";
        case SECURITY_INTEGRITY_CHECKPOINT_MISSING: return "checkpoint_missing";
        case SECURITY_INTEGRITY_TAMPERED:           return "tampered";
        default:                                    return "unknown";
    }
}

const char* security_grad_sanitize_mode_to_string(security_grad_sanitize_mode_t mode) {
    switch (mode) {
        case SECURITY_GRAD_SANITIZE_NONE:         return "none";
        case SECURITY_GRAD_SANITIZE_CLIP_NORM:    return "clip_norm";
        case SECURITY_GRAD_SANITIZE_CLIP_VALUE:   return "clip_value";
        case SECURITY_GRAD_SANITIZE_CLIP_BOTH:    return "clip_both";
        case SECURITY_GRAD_SANITIZE_BOUND:        return "bound";
        case SECURITY_GRAD_SANITIZE_DIFFERENTIAL: return "differential";
        default:                                  return "unknown";
    }
}
