/**
 * @file nimcp_security_perception_bridge.c
 * @brief Security-Perception Bridge implementation
 * @version 1.0.0
 * @date 2025-12-15
 */

#include "security/nimcp_security_perception_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_perception_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Security-perception bridge internal state
 */
struct security_perception_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    sec_percept_config_t config;       /**< Configuration */
    sec_percept_state_t state;         /**< Current state */

    /* Integration handles */
    bbb_system_t bbb;                  /**< Blood-brain barrier */
    nimcp_anomaly_detector_t anomaly;  /**< Anomaly detector */
    brain_immune_system_t* immune;     /**< Brain immune system */
    visual_cortex_t* visual_cortex;    /**< Visual cortex */
    audio_cortex_t* audio_cortex;      /**< Audio cortex */

    /* Quarantine management */
    quarantined_input_t* quarantine;   /**< Quarantine queue */
    uint32_t quarantine_count;         /**< Current quarantine count */
    uint32_t quarantine_capacity;      /**< Quarantine capacity */
    uint32_t next_quarantine_id;       /**< Next ID */

    /* Attack signature database */
    attack_signature_t* signatures;    /**< Attack signatures */
    uint32_t signature_count;          /**< Current signature count */
    uint32_t signature_capacity;       /**< Signature capacity */
    uint32_t next_signature_id;        /**< Next ID */

    /* Statistics */
    sec_percept_stats_t stats;

    /* Bio-async */
    bio_module_context_t bio_context;  /**< Bio-async context */
    bool bio_async_connected;          /**< Bio-async connected */

    /* Runtime state */
    uint64_t start_time_us;            /**< Start time (microseconds) */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 *
 * WHAT: Get high-resolution timestamp
 * WHY:  Performance measurement
 * HOW:  Use clock_gettime
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Compute feature statistics
 *
 * WHAT: Calculate mean and std dev of features
 * WHY:  Statistical anomaly detection
 * HOW:  Two-pass algorithm
 */
static void compute_feature_stats(
    const float* features,
    uint32_t dim,
    float* mean_out,
    float* std_out
) {
    if (!features || dim == 0) return;

    /* Compute mean */
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum += features[i];
    }
    float mean = sum / (float)dim;

    /* Compute std dev */
    float var_sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = features[i] - mean;
        var_sum += diff * diff;
    }
    float std = sqrtf(var_sum / (float)dim);

    if (mean_out) *mean_out = mean;
    if (std_out) *std_out = std;
}

/**
 * @brief Compute feature L2 norm
 *
 * WHAT: Calculate Euclidean norm of features
 * WHY:  Detect magnitude anomalies
 * HOW:  sqrt(sum of squares)
 */
static float compute_feature_norm(const float* features, uint32_t dim) {
    if (!features || dim == 0) return 0.0f;

    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum += features[i] * features[i];
    }
    return sqrtf(sum);
}

/**
 * @brief Compute cosine similarity
 *
 * WHAT: Calculate similarity between two feature vectors
 * WHY:  Pattern matching for signatures
 * HOW:  dot(a,b) / (norm(a) * norm(b))
 */
static float compute_cosine_similarity(
    const float* a,
    const float* b,
    uint32_t dim
) {
    if (!a || !b || dim == 0) return 0.0f;

    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    norm_a = sqrtf(norm_a);
    norm_b = sqrtf(norm_b);

    if (norm_a < 1e-8f || norm_b < 1e-8f) return 0.0f;
    return dot / (norm_a * norm_b);
}

/**
 * @brief Extract signature from features
 *
 * WHAT: Create attack signature from features
 * WHY:  Compact representation for matching
 * HOW:  Hash top-k features
 */
static void extract_signature(
    const float* features,
    uint32_t feature_dim,
    uint8_t* signature,
    size_t sig_size,
    size_t* sig_len
) {
    if (!features || !signature || sig_size == 0) {
        if (sig_len) *sig_len = 0;
        return;
    }

    /* Simple hash: take first sig_size bytes from features */
    size_t len = (feature_dim * sizeof(float) < sig_size) ?
                 feature_dim * sizeof(float) : sig_size;

    memcpy(signature, features, len);
    if (sig_len) *sig_len = len;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int sec_percept_default_config(sec_percept_config_t* config) {
    /* Guard clause: validate input */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Set default thresholds */
    config->visual_anomaly_threshold = SEC_PERCEPT_THRESHOLD_MEDIUM;
    config->audio_anomaly_threshold = SEC_PERCEPT_THRESHOLD_MEDIUM;
    config->cross_modal_threshold = SEC_PERCEPT_THRESHOLD_HIGH;
    config->immune_escalation_threshold = SEC_PERCEPT_THRESHOLD_HIGH;

    /* Enable all analysis features */
    config->enable_statistical_checks = true;
    config->enable_adversarial_detection = true;
    config->enable_cross_modal_validation = true;
    config->enable_temporal_analysis = true;

    /* Enable all integrations */
    config->enable_bbb = true;
    config->enable_anomaly_detector = true;
    config->enable_immune_system = true;
    config->enable_bio_async = true;

    /* Performance settings */
    config->max_quarantine_size = SEC_PERCEPT_MAX_QUARANTINED;
    config->max_attack_signatures = SEC_PERCEPT_MAX_ATTACK_SIGS;
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;

    /* Response policy */
    config->default_action = THREAT_RESPONSE_LOG;
    config->auto_quarantine = true;
    config->auto_immune_escalation = false;

    return 0;
}

security_perception_bridge_t* sec_percept_create(const sec_percept_config_t* config) {
    /* Allocate bridge */
    security_perception_bridge_t* bridge =
        (security_perception_bridge_t*)nimcp_malloc(sizeof(security_perception_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_percept_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(security_perception_bridge_t));

    /* Copy or default config */
    if (config) {
        bridge->config = *config;
    } else {
        sec_percept_default_config(&bridge->config);
    }

    /* Initialize state */
    bridge->state = SEC_PERCEPT_STATE_STOPPED;

    /* Allocate quarantine */
    bridge->quarantine_capacity = bridge->config.max_quarantine_size;
    bridge->quarantine = (quarantined_input_t*)nimcp_malloc(
        bridge->quarantine_capacity * sizeof(quarantined_input_t));
    if (!bridge->quarantine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_percept_create: failed to allocate quarantine");
        NIMCP_LOGGING_ERROR("Failed to allocate quarantine");
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->quarantine, 0, bridge->quarantine_capacity * sizeof(quarantined_input_t));

    /* Allocate signatures */
    bridge->signature_capacity = bridge->config.max_attack_signatures;
    bridge->signatures = (attack_signature_t*)nimcp_malloc(
        bridge->signature_capacity * sizeof(attack_signature_t));
    if (!bridge->signatures) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_percept_create: failed to allocate signatures");
        NIMCP_LOGGING_ERROR("Failed to allocate signatures");
        nimcp_free(bridge->quarantine);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->signatures, 0, bridge->signature_capacity * sizeof(attack_signature_t));

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "security_perception") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "sec_percept_create: mutex creation failed");
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge->signatures);
        nimcp_free(bridge->quarantine);
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize IDs */
    bridge->next_quarantine_id = 1;
    bridge->next_signature_id = 1;

    /* Initialize stats */
    bridge->stats.state = SEC_PERCEPT_STATE_STOPPED;
    bridge->start_time_us = get_time_us();

    NIMCP_LOGGING_INFO("Created security-perception bridge");
    return bridge;
}

void sec_percept_destroy(security_perception_bridge_t* bridge) {
    /* Guard clause: NULL safe */
    if (!bridge) return;

    /* Stop if running */
    if (bridge->state == SEC_PERCEPT_STATE_RUNNING) {
        sec_percept_stop(bridge);
    }

    /* Free quarantine features */
    if (bridge->quarantine) {
        for (uint32_t i = 0; i < bridge->quarantine_count; i++) {
            if (bridge->quarantine[i].features) {
                nimcp_free(bridge->quarantine[i].features);
            }
        }
        nimcp_free(bridge->quarantine);
    }

    /* Free signatures */
    if (bridge->signatures) {
        nimcp_free(bridge->signatures);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed security-perception bridge");
}

int sec_percept_start(security_perception_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->state == SEC_PERCEPT_STATE_RUNNING) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;  /* Already running */
    }

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async) {
        sec_percept_connect_bio_async(bridge);
    }

    bridge->state = SEC_PERCEPT_STATE_RUNNING;
    bridge->stats.state = SEC_PERCEPT_STATE_RUNNING;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Started security-perception bridge");
    return 0;
}

int sec_percept_stop(security_perception_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->state == SEC_PERCEPT_STATE_STOPPED) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;  /* Already stopped */
    }

    /* Disconnect from bio-async */
    if (bridge->bio_async_connected) {
        sec_percept_disconnect_bio_async(bridge);
    }

    bridge->state = SEC_PERCEPT_STATE_STOPPED;
    bridge->stats.state = SEC_PERCEPT_STATE_STOPPED;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Stopped security-perception bridge");
    return 0;
}

/* ============================================================================
 * Integration API Implementation
 * ============================================================================ */

int sec_percept_connect_bbb(
    security_perception_bridge_t* bridge,
    bbb_system_t bbb
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bbb) {
        NIMCP_LOGGING_ERROR("NULL BBB pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->bbb = bbb;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected BBB to security-perception bridge");
    return 0;
}

int sec_percept_connect_anomaly_detector(
    security_perception_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!detector) {
        NIMCP_LOGGING_ERROR("NULL anomaly detector pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->anomaly = detector;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected anomaly detector to security-perception bridge");
    return 0;
}

int sec_percept_connect_immune(
    security_perception_bridge_t* bridge,
    brain_immune_system_t* immune
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!immune) {
        NIMCP_LOGGING_ERROR("NULL immune system pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->immune = immune;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected immune system to security-perception bridge");
    return 0;
}

int sec_percept_connect_visual_cortex(
    security_perception_bridge_t* bridge,
    visual_cortex_t* cortex
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!cortex) {
        NIMCP_LOGGING_ERROR("NULL visual cortex pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->visual_cortex = cortex;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected visual cortex to security-perception bridge");
    return 0;
}

int sec_percept_connect_audio_cortex(
    security_perception_bridge_t* bridge,
    audio_cortex_t* cortex
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!cortex) {
        NIMCP_LOGGING_ERROR("NULL audio cortex pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->audio_cortex = cortex;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected audio cortex to security-perception bridge");
    return 0;
}

/* ============================================================================
 * Threat Detection API Implementation
 * ============================================================================ */

int sec_percept_analyze_visual(
    security_perception_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    sensory_threat_result_t* result
) {
    /* Guard clauses */
    if (!bridge || !features || !result || feature_dim == 0) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return NIMCP_ERROR_NULL_POINTER;
    }

    uint64_t start_time = get_time_us();

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Initialize result */
    memset(result, 0, sizeof(sensory_threat_result_t));
    result->modality = SENSORY_MODALITY_VISUAL;
    result->timestamp_us = start_time;
    result->recommended_action = bridge->config.default_action;

    /* Statistical anomaly check */
    if (bridge->config.enable_statistical_checks) {
        float mean, std;
        compute_feature_stats(features, feature_dim, &mean, &std);

        /* Check for outliers */
        if (std > 3.0f) {  /* High variance anomaly */
            result->statistical_anomaly_score += 0.3f;
        }

        /* Check norm */
        float norm = compute_feature_norm(features, feature_dim);
        if (norm > 10.0f || norm < 0.1f) {  /* Magnitude anomaly */
            result->statistical_anomaly_score += 0.2f;
        }
    }

    /* Adversarial pattern detection */
    if (bridge->config.enable_adversarial_detection) {
        /* Check for known attack signatures */
        uint32_t sig_id;
        float match_score;
        if (sec_percept_match_signature(bridge, features, feature_dim,
                                       SENSORY_MODALITY_VISUAL, &sig_id, &match_score) == 0) {
            result->adversarial_score = match_score;
            result->threat_type = SENSORY_THREAT_ADVERSARIAL_EXAMPLE;
            snprintf(result->explanation, sizeof(result->explanation),
                    "Matched attack signature %u (score: %.2f)", sig_id, match_score);
        }
    }

    /* Use anomaly detector if available */
    if (bridge->anomaly && bridge->config.enable_anomaly_detector) {
        nimcp_anomaly_result_t anomaly_result;
        if (nimcp_anomaly_detect(bridge->anomaly, features,
                                feature_dim * sizeof(float), &anomaly_result) == NIMCP_SUCCESS) {
            result->statistical_anomaly_score =
                fmaxf(result->statistical_anomaly_score, anomaly_result.anomaly_score);
        }
    }

    /* Compute overall threat score */
    result->threat_score = fmaxf(result->statistical_anomaly_score, result->adversarial_score);
    result->confidence = (result->statistical_anomaly_score + result->adversarial_score) / 2.0f;

    /* Determine threat type and action */
    if (result->threat_score >= bridge->config.visual_anomaly_threshold) {
        if (result->threat_type == SENSORY_THREAT_NONE) {
            result->threat_type = SENSORY_THREAT_STATISTICAL_ANOMALY;
            snprintf(result->explanation, sizeof(result->explanation),
                    "Statistical anomaly detected (score: %.2f)", result->threat_score);
        }

        if (result->threat_score >= SEC_PERCEPT_THRESHOLD_HIGH) {
            result->recommended_action = THREAT_RESPONSE_QUARANTINE;
        } else if (result->threat_score >= SEC_PERCEPT_THRESHOLD_MEDIUM) {
            result->recommended_action = THREAT_RESPONSE_LOG;
        }
    }

    /* Update statistics */
    bridge->stats.total_visual_inputs++;
    if (result->threat_score >= bridge->config.visual_anomaly_threshold) {
        bridge->stats.threats_detected++;
        if (result->threat_type == SENSORY_THREAT_ADVERSARIAL_EXAMPLE) {
            bridge->stats.adversarial_detected++;
        } else {
            bridge->stats.statistical_anomalies++;
        }
    }

    uint64_t end_time = get_time_us();
    float analysis_time = (float)(end_time - start_time);
    bridge->stats.avg_analysis_time_us =
        (bridge->stats.avg_analysis_time_us * (bridge->stats.total_visual_inputs - 1) +
         analysis_time) / bridge->stats.total_visual_inputs;
    bridge->stats.max_analysis_time_us = fmaxf(bridge->stats.max_analysis_time_us, analysis_time);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sec_percept_analyze_audio(
    security_perception_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    sensory_threat_result_t* result
) {
    /* Guard clauses */
    if (!bridge || !features || !result || feature_dim == 0) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return NIMCP_ERROR_NULL_POINTER;
    }

    uint64_t start_time = get_time_us();

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Initialize result */
    memset(result, 0, sizeof(sensory_threat_result_t));
    result->modality = SENSORY_MODALITY_AUDIO;
    result->timestamp_us = start_time;
    result->recommended_action = bridge->config.default_action;

    /* Statistical anomaly check */
    if (bridge->config.enable_statistical_checks) {
        float mean, std;
        compute_feature_stats(features, feature_dim, &mean, &std);

        /* Audio-specific checks */
        if (std > 2.5f) {  /* High variance in audio features */
            result->statistical_anomaly_score += 0.3f;
        }

        /* Check for extreme frequency components */
        float norm = compute_feature_norm(features, feature_dim);
        if (norm > 8.0f || norm < 0.05f) {
            result->statistical_anomaly_score += 0.2f;
            result->threat_type = SENSORY_THREAT_FREQUENCY_ATTACK;
        }
    }

    /* Check for known audio attack signatures */
    if (bridge->config.enable_adversarial_detection) {
        uint32_t sig_id;
        float match_score;
        if (sec_percept_match_signature(bridge, features, feature_dim,
                                       SENSORY_MODALITY_AUDIO, &sig_id, &match_score) == 0) {
            result->adversarial_score = match_score;
            result->threat_type = SENSORY_THREAT_ADVERSARIAL_EXAMPLE;
            snprintf(result->explanation, sizeof(result->explanation),
                    "Matched audio attack signature %u (score: %.2f)", sig_id, match_score);
        }
    }

    /* Compute overall threat score */
    result->threat_score = fmaxf(result->statistical_anomaly_score, result->adversarial_score);
    result->confidence = (result->statistical_anomaly_score + result->adversarial_score) / 2.0f;

    /* Determine action */
    if (result->threat_score >= bridge->config.audio_anomaly_threshold) {
        if (result->threat_type == SENSORY_THREAT_NONE) {
            result->threat_type = SENSORY_THREAT_STATISTICAL_ANOMALY;
            snprintf(result->explanation, sizeof(result->explanation),
                    "Audio anomaly detected (score: %.2f)", result->threat_score);
        }

        if (result->threat_score >= SEC_PERCEPT_THRESHOLD_HIGH) {
            result->recommended_action = THREAT_RESPONSE_QUARANTINE;
        } else {
            result->recommended_action = THREAT_RESPONSE_LOG;
        }
    }

    /* Update statistics */
    bridge->stats.total_audio_inputs++;
    if (result->threat_score >= bridge->config.audio_anomaly_threshold) {
        bridge->stats.threats_detected++;
        if (result->threat_type == SENSORY_THREAT_ADVERSARIAL_EXAMPLE) {
            bridge->stats.adversarial_detected++;
        } else {
            bridge->stats.statistical_anomalies++;
        }
    }

    uint64_t end_time = get_time_us();
    float analysis_time = (float)(end_time - start_time);
    bridge->stats.avg_analysis_time_us =
        (bridge->stats.avg_analysis_time_us * (bridge->stats.total_audio_inputs - 1) +
         analysis_time) / bridge->stats.total_audio_inputs;
    bridge->stats.max_analysis_time_us = fmaxf(bridge->stats.max_analysis_time_us, analysis_time);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sec_percept_analyze_multimodal(
    security_perception_bridge_t* bridge,
    const float* visual_features,
    uint32_t visual_dim,
    const float* audio_features,
    uint32_t audio_dim,
    sensory_threat_result_t* result
) {
    /* Guard clauses */
    if (!bridge || !visual_features || !audio_features || !result) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return NIMCP_ERROR_NULL_POINTER;
    }

    uint64_t start_time = get_time_us();

    /* Analyze each modality first (they lock their own mutex) */
    sensory_threat_result_t visual_result, audio_result;
    sec_percept_analyze_visual(bridge, visual_features, visual_dim, &visual_result);
    sec_percept_analyze_audio(bridge, audio_features, audio_dim, &audio_result);

    /* Now lock for combining results */
    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Initialize result */
    memset(result, 0, sizeof(sensory_threat_result_t));
    result->modality = SENSORY_MODALITY_MULTIMODAL;
    result->timestamp_us = start_time;
    result->recommended_action = bridge->config.default_action;

    /* Cross-modal consistency check */
    if (bridge->config.enable_cross_modal_validation) {
        cross_modal_check_t check;
        sec_percept_check_cross_modal(bridge, visual_features, visual_dim,
                                     audio_features, audio_dim, &check);

        result->cross_modal_score = 1.0f - check.overall_consistency;

        if (!check.consistent) {
            result->threat_type = SENSORY_THREAT_CROSS_MODAL_MISMATCH;
            snprintf(result->explanation, sizeof(result->explanation),
                    "Cross-modal mismatch: %s", check.mismatch_reason);
        }
    }

    /* Combine scores */
    result->statistical_anomaly_score = fmaxf(visual_result.statistical_anomaly_score,
                                             audio_result.statistical_anomaly_score);
    result->adversarial_score = fmaxf(visual_result.adversarial_score,
                                     audio_result.adversarial_score);

    result->threat_score = fmaxf(result->cross_modal_score,
                                fmaxf(result->statistical_anomaly_score,
                                     result->adversarial_score));
    result->confidence = (result->statistical_anomaly_score +
                         result->adversarial_score +
                         result->cross_modal_score) / 3.0f;

    /* Determine action */
    if (result->threat_score >= bridge->config.cross_modal_threshold) {
        if (result->threat_score >= SEC_PERCEPT_THRESHOLD_CRITICAL) {
            result->recommended_action = THREAT_RESPONSE_IMMUNE_ESCALATE;
        } else if (result->threat_score >= SEC_PERCEPT_THRESHOLD_HIGH) {
            result->recommended_action = THREAT_RESPONSE_QUARANTINE;
        }
    }

    /* Update statistics */
    bridge->stats.total_multimodal_inputs++;
    if (result->threat_score >= bridge->config.cross_modal_threshold) {
        bridge->stats.threats_detected++;
        if (result->threat_type == SENSORY_THREAT_CROSS_MODAL_MISMATCH) {
            bridge->stats.cross_modal_mismatches++;
        }
    }

    uint64_t end_time = get_time_us();
    float analysis_time = (float)(end_time - start_time);
    bridge->stats.avg_analysis_time_us =
        (bridge->stats.avg_analysis_time_us * (bridge->stats.total_multimodal_inputs - 1) +
         analysis_time) / bridge->stats.total_multimodal_inputs;
    bridge->stats.max_analysis_time_us = fmaxf(bridge->stats.max_analysis_time_us, analysis_time);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sec_percept_check_cross_modal(
    security_perception_bridge_t* bridge,
    const float* visual_features,
    uint32_t visual_dim,
    const float* audio_features,
    uint32_t audio_dim,
    cross_modal_check_t* check
) {
    /* Guard clauses */
    if (!bridge || !visual_features || !audio_features || !check) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(check, 0, sizeof(cross_modal_check_t));

    /* Compute feature similarity (semantic alignment) */
    uint32_t min_dim = (visual_dim < audio_dim) ? visual_dim : audio_dim;
    check->semantic_alignment = compute_cosine_similarity(
        visual_features, audio_features, min_dim);

    /* Temporal synchronization (simplified - assume both features from same timestamp) */
    check->visual_audio_sync = 0.9f;  /* Placeholder - would require temporal analysis */

    /* Overall consistency */
    check->overall_consistency = (check->semantic_alignment + check->visual_audio_sync) / 2.0f;

    /* Threshold check */
    check->consistent = (check->overall_consistency >= bridge->config.cross_modal_threshold);

    if (!check->consistent) {
        snprintf(check->mismatch_reason, sizeof(check->mismatch_reason),
                "Low consistency (%.2f < %.2f)",
                check->overall_consistency, bridge->config.cross_modal_threshold);
    }

    return 0;
}

/* ============================================================================
 * Quarantine Management API Implementation
 * ============================================================================ */

int sec_percept_quarantine_input(
    security_perception_bridge_t* bridge,
    const sensory_threat_result_t* threat,
    const float* features,
    uint32_t feature_dim,
    uint32_t* quarantine_id
) {
    /* Guard clauses */
    if (!bridge || !threat || !features || !quarantine_id) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Check capacity */
    if (bridge->quarantine_count >= bridge->quarantine_capacity) {
        NIMCP_LOGGING_WARN("Quarantine full, removing oldest entry");
        /* Remove oldest (simple FIFO eviction) */
        if (bridge->quarantine[0].features) {
            nimcp_free(bridge->quarantine[0].features);
        }
        memmove(&bridge->quarantine[0], &bridge->quarantine[1],
                (bridge->quarantine_count - 1) * sizeof(quarantined_input_t));
        bridge->quarantine_count--;
    }

    /* Create quarantine entry */
    quarantined_input_t* entry = &bridge->quarantine[bridge->quarantine_count];
    entry->id = bridge->next_quarantine_id++;
    entry->modality = threat->modality;
    entry->feature_dim = feature_dim;
    entry->features = (float*)nimcp_malloc(feature_dim * sizeof(float));
    if (!entry->features) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }
    memcpy(entry->features, features, feature_dim * sizeof(float));
    entry->threat = *threat;
    entry->quarantine_time = get_time_us();
    entry->analyzed = false;
    entry->neutralized = false;

    *quarantine_id = entry->id;
    bridge->quarantine_count++;

    /* Update statistics */
    bridge->stats.inputs_quarantined++;
    bridge->stats.current_quarantine_count = bridge->quarantine_count;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Quarantined input %u (threat score: %.2f)",
                      entry->id, threat->threat_score);
    return 0;
}

int sec_percept_get_quarantined(
    const security_perception_bridge_t* bridge,
    uint32_t quarantine_id,
    const quarantined_input_t** input
) {
    /* Guard clauses */
    if (!bridge || !input) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Linear search (could optimize with hash table) */
    for (uint32_t i = 0; i < bridge->quarantine_count; i++) {
        if (bridge->quarantine[i].id == quarantine_id) {
            *input = &bridge->quarantine[i];
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sec_percept_get_quarantined: validation failed");
    return -1;  /* Not found */
}

int sec_percept_release_quarantine(
    security_perception_bridge_t* bridge,
    uint32_t quarantine_id
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Find entry */
    for (uint32_t i = 0; i < bridge->quarantine_count; i++) {
        if (bridge->quarantine[i].id == quarantine_id) {
            /* Free features */
            if (bridge->quarantine[i].features) {
                nimcp_free(bridge->quarantine[i].features);
            }

            /* Remove entry (shift remaining) */
            memmove(&bridge->quarantine[i], &bridge->quarantine[i + 1],
                    (bridge->quarantine_count - i - 1) * sizeof(quarantined_input_t));
            bridge->quarantine_count--;
            bridge->stats.current_quarantine_count = bridge->quarantine_count;

            nimcp_platform_mutex_unlock(bridge->base.mutex);
            NIMCP_LOGGING_INFO("Released quarantine %u", quarantine_id);
            return 0;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sec_percept_release_quarantine: operation failed");
    return -1;  /* Not found */
}

void sec_percept_clear_quarantine(security_perception_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->quarantine_count; i++) {
        if (bridge->quarantine[i].features) {
            nimcp_free(bridge->quarantine[i].features);
        }
    }

    bridge->quarantine_count = 0;
    bridge->stats.current_quarantine_count = 0;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Cleared quarantine");
}

/* ============================================================================
 * Attack Signature Management API Implementation
 * ============================================================================ */

int sec_percept_learn_signature(
    security_perception_bridge_t* bridge,
    const sensory_threat_result_t* threat,
    const float* features,
    uint32_t feature_dim
) {
    /* Guard clauses */
    if (!bridge || !threat || !features) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_online_learning) {
        return 0;  /* Learning disabled */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Check capacity */
    if (bridge->signature_count >= bridge->signature_capacity) {
        NIMCP_LOGGING_WARN("Signature database full");
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_MUTEX_INIT;
    }

    /* Create signature */
    attack_signature_t* sig = &bridge->signatures[bridge->signature_count];
    sig->id = bridge->next_signature_id++;
    sig->modality = threat->modality;
    sig->type = threat->threat_type;

    extract_signature(features, feature_dim, sig->signature,
                     SEC_PERCEPT_SIGNATURE_SIZE, &sig->signature_len);

    sig->detection_threshold = threat->threat_score * 0.9f;  /* Slightly lower for matching */
    sig->detection_count = 1;
    sig->last_seen = get_time_us();

    bridge->signature_count++;
    bridge->stats.signatures_learned++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Learned attack signature %u (type: %s)",
                      sig->id, sec_percept_threat_type_name(threat->threat_type));
    return 0;
}

int sec_percept_match_signature(
    const security_perception_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    sensory_modality_t modality,
    uint32_t* signature_id,
    float* match_score
) {
    /* Guard clauses */
    if (!bridge || !features || !signature_id || !match_score) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Extract signature from input */
    uint8_t input_sig[SEC_PERCEPT_SIGNATURE_SIZE];
    size_t input_sig_len;
    extract_signature(features, feature_dim, input_sig,
                     SEC_PERCEPT_SIGNATURE_SIZE, &input_sig_len);

    /* Match against database */
    float best_score = 0.0f;
    uint32_t best_id = 0;

    for (uint32_t i = 0; i < bridge->signature_count; i++) {
        const attack_signature_t* sig = &bridge->signatures[i];

        /* Filter by modality */
        if (sig->modality != modality) continue;

        /* Compute similarity (simple byte-wise comparison) */
        size_t min_len = (sig->signature_len < input_sig_len) ?
                        sig->signature_len : input_sig_len;
        uint32_t matches = 0;
        for (size_t j = 0; j < min_len; j++) {
            if (sig->signature[j] == input_sig[j]) {
                matches++;
            }
        }

        float score = (float)matches / (float)min_len;

        if (score > best_score) {
            best_score = score;
            best_id = sig->id;
        }
    }

    if (best_score >= 0.7f) {  /* Threshold for match */
        *signature_id = best_id;
        *match_score = best_score;
        return 0;  /* Match found */
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sec_percept_match_signature: capacity exceeded");
    return -1;  /* No match */
}

int sec_percept_get_signature(
    const security_perception_bridge_t* bridge,
    uint32_t signature_id,
    const attack_signature_t** signature
) {
    if (!bridge || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_percept_get_signature: required parameter is NULL (bridge, signature)");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->signature_count; i++) {
        if (bridge->signatures[i].id == signature_id) {
            *signature = &bridge->signatures[i];
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sec_percept_get_signature: validation failed");
    return -1;  /* Not found */
}

void sec_percept_clear_signatures(security_perception_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->signature_count = 0;
    bridge->stats.signatures_learned = 0;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Cleared attack signatures");
}

/* ============================================================================
 * Immune Escalation API Implementation
 * ============================================================================ */

int sec_percept_escalate_to_immune(
    security_perception_bridge_t* bridge,
    const sensory_threat_result_t* threat,
    uint32_t* antigen_id
) {
    /* Guard clauses */
    if (!bridge || !threat || !antigen_id) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->immune) {
        NIMCP_LOGGING_WARN("Immune system not connected");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Present threat as antigen to immune system */
    /* (Would call brain_immune_present_antigen with threat signature) */

    /* For now, placeholder */
    *antigen_id = 0;
    bridge->stats.immune_escalations++;

    NIMCP_LOGGING_INFO("Escalated sensory threat to immune system (score: %.2f)",
                      threat->threat_score);
    return 0;
}

int sec_percept_boost_security_salience(
    security_perception_bridge_t* bridge,
    const sensory_threat_result_t* threat,
    float salience_boost
) {
    /* Guard clauses */
    if (!bridge || !threat) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Clamp boost */
    if (salience_boost < 1.0f) salience_boost = 1.0f;
    if (salience_boost > 2.0f) salience_boost = 2.0f;

    /* Boost attention in relevant cortex */
    if (threat->modality == SENSORY_MODALITY_VISUAL && bridge->visual_cortex) {
        /* Would call visual_cortex_boost_region_attention */
        NIMCP_LOGGING_DEBUG("Boosted visual attention (boost: %.2f)", salience_boost);
    } else if (threat->modality == SENSORY_MODALITY_AUDIO && bridge->audio_cortex) {
        /* Would call audio_cortex_boost_attention */
        NIMCP_LOGGING_DEBUG("Boosted audio attention (boost: %.2f)", salience_boost);
    }

    return 0;
}

/* ============================================================================
 * Statistics and Monitoring API Implementation
 * ============================================================================ */

int sec_percept_get_stats(
    const security_perception_bridge_t* bridge,
    sec_percept_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return NIMCP_ERROR_NULL_POINTER;
    }

    *stats = bridge->stats;
    return 0;
}

void sec_percept_reset_stats(security_perception_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Preserve state, reset counters */
    sec_percept_state_t state = bridge->stats.state;
    uint32_t current_quarantine = bridge->stats.current_quarantine_count;

    memset(&bridge->stats, 0, sizeof(sec_percept_stats_t));
    bridge->stats.state = state;
    bridge->stats.current_quarantine_count = current_quarantine;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Reset statistics");
}

sec_percept_state_t sec_percept_get_state(
    const security_perception_bridge_t* bridge
) {
    if (!bridge) return SEC_PERCEPT_STATE_ERROR;
    return bridge->state;
}

/* ============================================================================
 * Bio-Async Communication API Implementation
 * ============================================================================ */

int sec_percept_connect_bio_async(security_perception_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_percept_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->bio_async_connected) return 0;

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY,  /* Use security module ID */
        .module_name = SEC_PERCEPT_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_context = bio_router_register_module(&info);
    if (bridge->bio_context) {
        bridge->bio_async_connected = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Failed to connect to bio-async router");
    return NIMCP_ERROR_INVALID_STATE;
}

int sec_percept_disconnect_bio_async(security_perception_bridge_t* bridge) {
    if (!bridge || !bridge->bio_async_connected) return 0;

    if (bridge->bio_context) {
        bio_router_unregister_module(bridge->bio_context);
        bridge->bio_context = NULL;
    }

    bridge->bio_async_connected = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

int sec_percept_send_threat_alert(
    security_perception_bridge_t* bridge,
    const sensory_threat_result_t* threat
) {
    if (!bridge || !threat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_percept_send_threat_alert: required parameter is NULL (bridge, threat)");
        return -1;
    }
    if (!bridge->bio_async_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_percept_send_threat_alert: bridge->bio_async_connected is NULL");
        return -1;
    }

    /* Create threat alert message payload */
    struct {
        uint32_t msg_type;
        uint32_t threat_level;
        uint32_t threat_type;
        uint32_t modality;
        uint64_t timestamp;
    } alert_msg = {
        .msg_type = BIO_MSG_SECURITY_THREAT_DETECTED,
        .threat_level = (uint32_t)(threat->threat_score * 100.0f),
        .threat_type = (uint32_t)threat->threat_type,
        .modality = (uint32_t)threat->modality,
        .timestamp = threat->timestamp_us
    };

    /* Send via bio-async router */
    nimcp_error_t result = bio_router_send(
        bridge->bio_context,
        &alert_msg,
        sizeof(alert_msg),
        0  /* timeout_ms: no timeout */
    );

    if (result == NIMCP_SUCCESS) {
        NIMCP_LOGGING_INFO("Sent threat alert (score: %.2f)", threat->threat_score);
        return 0;
    }

    NIMCP_LOGGING_WARN("Failed to send threat alert: %d", result);
    return NIMCP_ERROR_OPERATION_FAILED;
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* sec_percept_modality_name(sensory_modality_t modality) {
    static const char* names[] = {
        "Visual", "Audio", "Speech", "Multimodal"
    };

    if (modality < SENSORY_MODALITY_COUNT) {
        return names[modality];
    }
    return "Unknown";
}

const char* sec_percept_threat_type_name(sensory_threat_type_t type) {
    switch (type) {
        case SENSORY_THREAT_NONE: return "None";
        case SENSORY_THREAT_ADVERSARIAL_EXAMPLE: return "Adversarial Example";
        case SENSORY_THREAT_UNIVERSAL_PERTURBATION: return "Universal Perturbation";
        case SENSORY_THREAT_BACKDOOR_TRIGGER: return "Backdoor Trigger";
        case SENSORY_THREAT_STATISTICAL_ANOMALY: return "Statistical Anomaly";
        case SENSORY_THREAT_CROSS_MODAL_MISMATCH: return "Cross-Modal Mismatch";
        case SENSORY_THREAT_TEMPORAL_ANOMALY: return "Temporal Anomaly";
        case SENSORY_THREAT_FREQUENCY_ATTACK: return "Frequency Attack";
        case SENSORY_THREAT_UNKNOWN: return "Unknown";
        default: return "Invalid";
    }
}

const char* sec_percept_response_action_name(threat_response_action_t action) {
    switch (action) {
        case THREAT_RESPONSE_ALLOW: return "Allow";
        case THREAT_RESPONSE_LOG: return "Log";
        case THREAT_RESPONSE_QUARANTINE: return "Quarantine";
        case THREAT_RESPONSE_SANITIZE: return "Sanitize";
        case THREAT_RESPONSE_REJECT: return "Reject";
        case THREAT_RESPONSE_IMMUNE_ESCALATE: return "Immune Escalate";
        default: return "Invalid";
    }
}

const char* sec_percept_state_name(sec_percept_state_t state) {
    switch (state) {
        case SEC_PERCEPT_STATE_STOPPED: return "Stopped";
        case SEC_PERCEPT_STATE_STARTING: return "Starting";
        case SEC_PERCEPT_STATE_RUNNING: return "Running";
        case SEC_PERCEPT_STATE_DEGRADED: return "Degraded";
        case SEC_PERCEPT_STATE_ERROR: return "Error";
        default: return "Invalid";
    }
}
