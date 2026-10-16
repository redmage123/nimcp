/**
 * @file nimcp_anomaly_detector.c
 * @brief Main anomaly detector implementation with Bayesian network
 *
 * WHAT: Complete anomaly detection system integrating features and Bayesian inference
 * WHY:  Detect security threats using probabilistic ML
 * HOW:  Extract features, run BN inference, compute scores, adaptive thresholds
 *
 * ARCHITECTURE:
 *   Input → Feature Extraction → Bayesian Network → Score Computation
 *                                                         ↓
 *                                               Threshold Check
 *                                                         ↓
 *                                               Result + Explanation
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 */

#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/random.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_learning_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(anomaly_detector, MESH_ADAPTER_CATEGORY_SECURITY)


/*=============================================================================
 * INTERNAL CONSTANTS
 *============================================================================*/

#define DEFAULT_CONTENT_THRESHOLD 0.7f
#define DEFAULT_BEHAVIOR_THRESHOLD 0.65f
#define DEFAULT_TIMING_THRESHOLD 0.6f
#define DEFAULT_OVERALL_THRESHOLD 0.75f
#define DEFAULT_LEARNING_WINDOW 1000
#define DEFAULT_LEARNING_RATE NIMCP_LEARNING_RATE_DEFAULT
#define ADAPTIVE_THRESHOLD_RATE 0.05f

/*=============================================================================
 * TIMING CONTEXT STRUCTURE
 *============================================================================*/

typedef struct {
    uint64_t timestamps_us[100];
    uint32_t count;
    uint32_t write_idx;
    float window_sec;
} timing_context_t;

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

struct nimcp_anomaly_detector_internal {
    uint32_t magic;
    nimcp_anomaly_config_t config;

    /** Bayesian network for inference */
    nimcp_bayesian_network_t bn;

    /** Timing context for rate features */
    timing_context_t* timing_ctx;

    /** Statistics */
    nimcp_anomaly_stats_t stats;

    /** Adaptive thresholds */
    float adaptive_content_threshold;
    float adaptive_behavior_threshold;
    float adaptive_timing_threshold;

    /** Bio-async integration */
    bio_module_context_t bio_ctx;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

static inline bool detector_is_valid(nimcp_anomaly_detector_t detector) {
    return detector != NULL && detector->magic == NIMCP_ANOMALY_DETECTOR_MAGIC;
}

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Build Bayesian network structure for anomaly detection
 *
 * STRUCTURE:
 *   [Length] [Entropy] [SpecialChars] [Ngrams] [Timing]
 *       \       |         |            /         /
 *        v      v         v           v         v
 *        [Content Anomaly]    [Behavior Anomaly]
 *                  \                /
 *                   v              v
 *                  [Overall Anomaly]
 */
static nimcp_error_t build_bn_structure(nimcp_bayesian_network_t bn) {
    nimcp_error_t err;

    /* Add edges: features → intermediate nodes */
    /* Length → Content Anomaly */
    err = nimcp_bn_add_edge(bn, NIMCP_BN_NODE_LENGTH, NIMCP_BN_NODE_CONTENT_ANOMALY);
    if (err != NIMCP_SUCCESS) return err;

    /* Entropy → Content Anomaly */
    err = nimcp_bn_add_edge(bn, NIMCP_BN_NODE_ENTROPY, NIMCP_BN_NODE_CONTENT_ANOMALY);
    if (err != NIMCP_SUCCESS) return err;

    /* SpecialChars → Content Anomaly */
    err = nimcp_bn_add_edge(bn, NIMCP_BN_NODE_SPECIAL_CHARS, NIMCP_BN_NODE_CONTENT_ANOMALY);
    if (err != NIMCP_SUCCESS) return err;

    /* Ngrams → Behavior Anomaly */
    err = nimcp_bn_add_edge(bn, NIMCP_BN_NODE_NGRAMS, NIMCP_BN_NODE_BEHAVIOR_ANOMALY);
    if (err != NIMCP_SUCCESS) return err;

    /* Timing → Behavior Anomaly */
    err = nimcp_bn_add_edge(bn, NIMCP_BN_NODE_TIMING, NIMCP_BN_NODE_BEHAVIOR_ANOMALY);
    if (err != NIMCP_SUCCESS) return err;

    /* Add edges: intermediate → overall */
    /* Content Anomaly → Overall Anomaly */
    err = nimcp_bn_add_edge(bn, NIMCP_BN_NODE_CONTENT_ANOMALY, NIMCP_BN_NODE_OVERALL_ANOMALY);
    if (err != NIMCP_SUCCESS) return err;

    /* Behavior Anomaly → Overall Anomaly */
    err = nimcp_bn_add_edge(bn, NIMCP_BN_NODE_BEHAVIOR_ANOMALY, NIMCP_BN_NODE_OVERALL_ANOMALY);
    if (err != NIMCP_SUCCESS) return err;

    return NIMCP_SUCCESS;
}

/**
 * @brief Map features to BN evidence
 */
static void features_to_evidence(const float* features, float* evidence) {
    /* Map feature vector to BN nodes */
    evidence[NIMCP_BN_NODE_LENGTH] = features[NIMCP_FEATURE_LENGTH];
    evidence[NIMCP_BN_NODE_ENTROPY] = features[NIMCP_FEATURE_ENTROPY];

    /* Combine special + control chars */
    evidence[NIMCP_BN_NODE_SPECIAL_CHARS] =
        (features[NIMCP_FEATURE_SPECIAL_RATIO] + features[NIMCP_FEATURE_CONTROL_RATIO]) / 2.0F;

    /* Combine n-gram entropies */
    evidence[NIMCP_BN_NODE_NGRAMS] =
        (features[NIMCP_FEATURE_BIGRAM_ENTROPY] + features[NIMCP_FEATURE_TRIGRAM_ENTROPY]) / 2.0F;

    /* Combine timing features */
    evidence[NIMCP_BN_NODE_TIMING] =
        (features[NIMCP_FEATURE_REQUEST_RATE] + features[NIMCP_FEATURE_BURST_SCORE]) / 2.0F;

    /* Intermediate nodes: unobserved (NAN) */
    evidence[NIMCP_BN_NODE_CONTENT_ANOMALY] = NAN;
    evidence[NIMCP_BN_NODE_BEHAVIOR_ANOMALY] = NAN;
    evidence[NIMCP_BN_NODE_OVERALL_ANOMALY] = NAN;
}

/**
 * @brief Determine triggered features
 */
static uint32_t determine_triggered_features(const float* features, const float* posteriors,
                                               const nimcp_anomaly_config_t* config) {
    uint32_t triggered = 0;

    /* Check each feature against its contribution to anomaly score */
    if (features[NIMCP_FEATURE_LENGTH] > 0.8F) {
        triggered |= NIMCP_TRIGGER_LENGTH;
    }

    if (features[NIMCP_FEATURE_ENTROPY] > 0.9F || features[NIMCP_FEATURE_ENTROPY] < 0.1F) {
        triggered |= NIMCP_TRIGGER_ENTROPY;
    }

    if (features[NIMCP_FEATURE_ALPHA_RATIO] < 0.2F) {
        triggered |= NIMCP_TRIGGER_ALPHA_RATIO;
    }

    if (features[NIMCP_FEATURE_NUMERIC_RATIO] > 0.8F) {
        triggered |= NIMCP_TRIGGER_NUMERIC_RATIO;
    }

    if (features[NIMCP_FEATURE_SPECIAL_RATIO] > 0.5F) {
        triggered |= NIMCP_TRIGGER_SPECIAL_RATIO;
    }

    if (features[NIMCP_FEATURE_CONTROL_RATIO] > 0.3F) {
        triggered |= NIMCP_TRIGGER_CONTROL_RATIO;
    }

    if (features[NIMCP_FEATURE_BIGRAM_ENTROPY] < 0.3F) {
        triggered |= NIMCP_TRIGGER_BIGRAM_ENTROPY;
    }

    if (features[NIMCP_FEATURE_TRIGRAM_ENTROPY] < 0.3F) {
        triggered |= NIMCP_TRIGGER_TRIGRAM_ENTROPY;
    }

    if (features[NIMCP_FEATURE_NESTING_DEPTH] > 0.5F) {
        triggered |= NIMCP_TRIGGER_NESTING_DEPTH;
    }

    if (features[NIMCP_FEATURE_REQUEST_RATE] > 0.7F) {
        triggered |= NIMCP_TRIGGER_REQUEST_RATE;
    }

    if (features[NIMCP_FEATURE_BURST_SCORE] > 0.6F) {
        triggered |= NIMCP_TRIGGER_BURST_SCORE;
    }

    if (features[NIMCP_FEATURE_REPEAT_RATIO] > 0.4F) {
        triggered |= NIMCP_TRIGGER_REPEAT_RATIO;
    }

    return triggered;
}

/**
 * @brief Generate human-readable explanation
 */
static void generate_explanation(uint32_t triggered, float anomaly_score, char* explanation, size_t max_len) {
    if (anomaly_score < 0.3F) {
        snprintf(explanation, max_len, "Input appears normal (score: %.2f)", anomaly_score);
        return;
    }

    char reasons[NIMCP_ERROR_BUFFER_SIZE] = "";
    size_t offset = 0;

    if (triggered & NIMCP_TRIGGER_LENGTH) {
        offset += snprintf(reasons + offset, sizeof(reasons) - offset, "excessive length, ");
    }
    if (triggered & NIMCP_TRIGGER_ENTROPY) {
        offset += snprintf(reasons + offset, sizeof(reasons) - offset, "unusual entropy, ");
    }
    if (triggered & NIMCP_TRIGGER_SPECIAL_RATIO) {
        offset += snprintf(reasons + offset, sizeof(reasons) - offset, "high special chars, ");
    }
    if (triggered & NIMCP_TRIGGER_CONTROL_RATIO) {
        offset += snprintf(reasons + offset, sizeof(reasons) - offset, "control characters, ");
    }
    if (triggered & NIMCP_TRIGGER_NESTING_DEPTH) {
        offset += snprintf(reasons + offset, sizeof(reasons) - offset, "deep nesting, ");
    }
    if (triggered & NIMCP_TRIGGER_REQUEST_RATE) {
        offset += snprintf(reasons + offset, sizeof(reasons) - offset, "high request rate, ");
    }
    if (triggered & NIMCP_TRIGGER_BURST_SCORE) {
        offset += snprintf(reasons + offset, sizeof(reasons) - offset, "burst pattern, ");
    }
    if (triggered & NIMCP_TRIGGER_REPEAT_RATIO) {
        offset += snprintf(reasons + offset, sizeof(reasons) - offset, "repeated patterns, ");
    }

    /* Remove trailing ", " */
    if (offset >= 2) {
        reasons[offset - 2] = '\0';
    }

    if (offset > 0) {
        snprintf(explanation, max_len, "Anomaly detected (%.2f): %s", anomaly_score, reasons);
    } else {
        snprintf(explanation, max_len, "Anomaly detected (%.2f): statistical deviation", anomaly_score);
    }
}

/*=============================================================================
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/

nimcp_anomaly_config_t nimcp_anomaly_detector_default_config(void) {
    nimcp_anomaly_config_t config = {
        .content_anomaly_threshold = DEFAULT_CONTENT_THRESHOLD,
        .behavior_anomaly_threshold = DEFAULT_BEHAVIOR_THRESHOLD,
        .timing_anomaly_threshold = DEFAULT_TIMING_THRESHOLD,
        .overall_anomaly_threshold = DEFAULT_OVERALL_THRESHOLD,

        .learning_window_size = DEFAULT_LEARNING_WINDOW,
        .learning_rate = DEFAULT_LEARNING_RATE,
        .enable_adaptive_threshold = true,
        .enable_online_learning = true,

        .max_input_length = 10240,
        .max_ngram_size = 3,
        .timing_window_sec = 10.0F,

        .enable_caching = false,
        .enable_fast_mode = false,

        .enable_bio_async = false,
        .alert_module_id = 0
    };

    return config;
}

nimcp_anomaly_detector_t nimcp_anomaly_detector_create(const nimcp_anomaly_config_t* config) {
    LOG_MODULE_DEBUG("anomaly_detector", "Creating anomaly detector");

    /* Use default config if none provided */
    nimcp_anomaly_config_t default_config = nimcp_anomaly_detector_default_config();
    const nimcp_anomaly_config_t* cfg = config ? config : &default_config;

    /* Allocate detector */
    nimcp_anomaly_detector_t detector = (nimcp_anomaly_detector_t)nimcp_calloc(1, sizeof(struct nimcp_anomaly_detector_internal));
    if (!detector) {
        LOG_MODULE_ERROR("anomaly_detector", "Failed to allocate anomaly detector");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate anomaly detector");
        return NULL;
    }

    detector->magic = NIMCP_ANOMALY_DETECTOR_MAGIC;
    detector->config = *cfg;

    /* Initialize adaptive thresholds */
    detector->adaptive_content_threshold = cfg->content_anomaly_threshold;
    detector->adaptive_behavior_threshold = cfg->behavior_anomaly_threshold;
    detector->adaptive_timing_threshold = cfg->timing_anomaly_threshold;

    /* Create Bayesian network */
    detector->bn = nimcp_bn_create(NIMCP_BN_NODE_COUNT);
    if (!detector->bn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create Bayesian network for anomaly detector");
        nimcp_free(detector);
        return NULL;
    }

    /* Build network structure */
    if (build_bn_structure(detector->bn) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to build Bayesian network structure");
        nimcp_bn_destroy(detector->bn);
        nimcp_free(detector);
        return NULL;
    }

    /* Create timing context */
    detector->timing_ctx = (timing_context_t*)nimcp_calloc(1, sizeof(timing_context_t));
    if (!detector->timing_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate timing context");
        nimcp_bn_destroy(detector->bn);
        nimcp_free(detector);
        return NULL;
    }
    detector->timing_ctx->window_sec = cfg->timing_window_sec;

    /* Initialize statistics */
    memset(&detector->stats, 0, sizeof(nimcp_anomaly_stats_t));
    detector->stats.current_content_threshold = detector->adaptive_content_threshold;
    detector->stats.current_behavior_threshold = detector->adaptive_behavior_threshold;
    detector->stats.current_timing_threshold = detector->adaptive_timing_threshold;

    /* Bio-async registration */
    if (cfg->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t module_info = {
            .module_id = BIO_MODULE_SECURITY,
            .module_name = "anomaly_detector",
            .inbox_capacity = 100,
            .user_data = detector
        };

        detector->bio_ctx = bio_router_register_module(&module_info);
        if (!detector->bio_ctx) {
            /* Non-fatal: continue without bio-async */
            LOG_MODULE_WARN("anomaly_detector", "Bio-async registration failed, continuing without");
            detector->config.enable_bio_async = false;
        }
    }

    LOG_MODULE_INFO("anomaly_detector", "Anomaly detector created (threshold=%.2f)",
                   cfg->overall_anomaly_threshold);
    return detector;
}

void nimcp_anomaly_detector_destroy(nimcp_anomaly_detector_t detector) {
    if (!detector) {
        return;
    }

    if (detector->bn) {
        nimcp_bn_destroy(detector->bn);
    }

    nimcp_free(detector->timing_ctx);

    if (detector->bio_ctx) {
        bio_router_unregister_module(detector->bio_ctx);
    }

    detector->magic = 0;
    nimcp_free(detector);
}

nimcp_error_t nimcp_anomaly_detect(nimcp_anomaly_detector_t detector,
                                     const void* input, size_t input_len,
                                     nimcp_anomaly_result_t* result) {
    if (!detector_is_valid(detector) || !input || !result) {
        return NIMCP_INVALID_PARAM;
    }

    uint64_t start_time = get_timestamp_us();

    /* Clear result */
    memset(result, 0, sizeof(nimcp_anomaly_result_t));

    /* Check input length */
    if (input_len > detector->config.max_input_length) {
        input_len = detector->config.max_input_length;
    }

    /* Extract features */
    float features[NIMCP_FEATURE_COUNT];
    nimcp_error_t err = nimcp_extract_features(input, input_len, features, detector->timing_ctx);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    /* Map features to BN evidence */
    float evidence[NIMCP_BN_NODE_COUNT];
    features_to_evidence(features, evidence);

    /* Run Bayesian inference */
    float posteriors[NIMCP_BN_NODE_COUNT];
    err = nimcp_bn_infer(detector->bn, evidence, posteriors);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    /* Extract component scores */
    result->content_score = posteriors[NIMCP_BN_NODE_CONTENT_ANOMALY];
    result->behavior_score = posteriors[NIMCP_BN_NODE_BEHAVIOR_ANOMALY];
    result->timing_score = posteriors[NIMCP_BN_NODE_TIMING];

    /* Overall anomaly score */
    result->anomaly_score = posteriors[NIMCP_BN_NODE_OVERALL_ANOMALY];

    /* Confidence based on number of training samples */
    if (detector->stats.training_samples < 10) {
        result->confidence = 0.3F;
    } else if (detector->stats.training_samples < 100) {
        result->confidence = 0.6F;
    } else if (detector->stats.training_samples < 1000) {
        result->confidence = 0.8F;
    } else {
        result->confidence = 0.95F;
    }

    /* Determine triggered features */
    result->triggered_features = determine_triggered_features(features, posteriors, &detector->config);

    /* Generate explanation */
    generate_explanation(result->triggered_features, result->anomaly_score,
                         result->explanation, sizeof(result->explanation));

    /* Metadata */
    result->timestamp_us = get_timestamp_us();
    result->sample_count = (uint32_t)detector->stats.training_samples;

    /* Update statistics */
    detector->stats.total_detections++;

    if (result->anomaly_score >= detector->config.overall_anomaly_threshold) {
        detector->stats.anomalies_detected++;
        LOG_MODULE_WARN("anomaly_detector", "Anomaly detected: score=%.2f, triggered=0x%x",
                       result->anomaly_score, result->triggered_features);
    }

    /* Update performance metrics */
    uint64_t detection_time = result->timestamp_us - start_time;
    float alpha = 0.1F;  /* EMA smoothing factor */
    detector->stats.avg_detection_time_us =
        alpha * (float)detection_time + (1.0F - alpha) * detector->stats.avg_detection_time_us;

    if ((float)detection_time > detector->stats.max_detection_time_us) {
        detector->stats.max_detection_time_us = (float)detection_time;
    }

    /* Online learning if enabled */
    if (detector->config.enable_online_learning && result->anomaly_score < 0.5F) {
        /* Assume low-score samples are normal for training */
        float sample[NIMCP_BN_NODE_COUNT];
        memcpy(sample, evidence, sizeof(sample));
        sample[NIMCP_BN_NODE_CONTENT_ANOMALY] = result->content_score;
        sample[NIMCP_BN_NODE_BEHAVIOR_ANOMALY] = result->behavior_score;
        sample[NIMCP_BN_NODE_OVERALL_ANOMALY] = result->anomaly_score;

        nimcp_bn_learn(detector->bn, sample);
        detector->stats.model_update_count++;
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_anomaly_train(nimcp_anomaly_detector_t detector,
                                    const void* input, size_t input_len,
                                    bool is_normal) {
    if (!detector_is_valid(detector) || !input) {
        return NIMCP_INVALID_PARAM;
    }

    /* Extract features */
    float features[NIMCP_FEATURE_COUNT];
    nimcp_error_t err = nimcp_extract_features(input, input_len, features, detector->timing_ctx);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    /* Map to evidence */
    float evidence[NIMCP_BN_NODE_COUNT];
    features_to_evidence(features, evidence);

    /* Set labels for intermediate and output nodes */
    if (is_normal) {
        /* Normal samples have low anomaly scores */
        evidence[NIMCP_BN_NODE_CONTENT_ANOMALY] = 0.2F;
        evidence[NIMCP_BN_NODE_BEHAVIOR_ANOMALY] = 0.2F;
        evidence[NIMCP_BN_NODE_OVERALL_ANOMALY] = 0.1F;
        detector->stats.normal_samples++;
    } else {
        /* Anomalous samples have high anomaly scores */
        evidence[NIMCP_BN_NODE_CONTENT_ANOMALY] = 0.8F;
        evidence[NIMCP_BN_NODE_BEHAVIOR_ANOMALY] = 0.8F;
        evidence[NIMCP_BN_NODE_OVERALL_ANOMALY] = 0.9F;
        detector->stats.anomalous_samples++;
    }

    /* Learn from sample */
    err = nimcp_bn_learn(detector->bn, evidence);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    detector->stats.training_samples++;
    detector->stats.model_update_count++;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_anomaly_get_stats(nimcp_anomaly_detector_t detector,
                                       nimcp_anomaly_stats_t* stats) {
    if (!detector_is_valid(detector) || !stats) {
        return NIMCP_INVALID_PARAM;
    }

    /* Update computed metrics */
    if (detector->stats.anomalies_detected + detector->stats.false_positives > 0) {
        detector->stats.precision = (float)detector->stats.anomalies_detected /
            (float)(detector->stats.anomalies_detected + detector->stats.false_positives);
    }

    if (detector->stats.anomalies_detected + detector->stats.false_negatives > 0) {
        detector->stats.recall = (float)detector->stats.anomalies_detected /
            (float)(detector->stats.anomalies_detected + detector->stats.false_negatives);
    }

    if (detector->stats.precision + detector->stats.recall > 0.0F) {
        detector->stats.f1_score = 2.0F * detector->stats.precision * detector->stats.recall /
            (detector->stats.precision + detector->stats.recall);
    }

    /* Update current thresholds */
    detector->stats.current_content_threshold = detector->adaptive_content_threshold;
    detector->stats.current_behavior_threshold = detector->adaptive_behavior_threshold;
    detector->stats.current_timing_threshold = detector->adaptive_timing_threshold;

    *stats = detector->stats;
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_anomaly_reset_stats(nimcp_anomaly_detector_t detector) {
    if (!detector_is_valid(detector)) {
        return NIMCP_INVALID_PARAM;
    }

    /* Keep training samples and model updates, reset detection stats */
    uint64_t training_samples = detector->stats.training_samples;
    uint64_t normal_samples = detector->stats.normal_samples;
    uint64_t anomalous_samples = detector->stats.anomalous_samples;
    uint64_t model_update_count = detector->stats.model_update_count;

    memset(&detector->stats, 0, sizeof(nimcp_anomaly_stats_t));

    detector->stats.training_samples = training_samples;
    detector->stats.normal_samples = normal_samples;
    detector->stats.anomalous_samples = anomalous_samples;
    detector->stats.model_update_count = model_update_count;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_anomaly_update_thresholds(nimcp_anomaly_detector_t detector,
                                                bool false_positive, bool false_negative) {
    if (!detector_is_valid(detector)) {
        return NIMCP_INVALID_PARAM;
    }

    if (!detector->config.enable_adaptive_threshold) {
        return NIMCP_SUCCESS;  /* Not enabled */
    }

    if (false_positive) {
        detector->stats.false_positives++;

        /* Increase thresholds to reduce false positives */
        detector->adaptive_content_threshold += ADAPTIVE_THRESHOLD_RATE;
        detector->adaptive_behavior_threshold += ADAPTIVE_THRESHOLD_RATE;
        detector->adaptive_timing_threshold += ADAPTIVE_THRESHOLD_RATE;

        /* Clamp to [0, 1] */
        if (detector->adaptive_content_threshold > 1.0F) {
            detector->adaptive_content_threshold = 1.0F;
        }
        if (detector->adaptive_behavior_threshold > 1.0F) {
            detector->adaptive_behavior_threshold = 1.0F;
        }
        if (detector->adaptive_timing_threshold > 1.0F) {
            detector->adaptive_timing_threshold = 1.0F;
        }
    }

    if (false_negative) {
        detector->stats.false_negatives++;

        /* Decrease thresholds to reduce false negatives */
        detector->adaptive_content_threshold -= ADAPTIVE_THRESHOLD_RATE;
        detector->adaptive_behavior_threshold -= ADAPTIVE_THRESHOLD_RATE;
        detector->adaptive_timing_threshold -= ADAPTIVE_THRESHOLD_RATE;

        /* Clamp to [0, 1] */
        if (detector->adaptive_content_threshold < 0.0F) {
            detector->adaptive_content_threshold = 0.0F;
        }
        if (detector->adaptive_behavior_threshold < 0.0F) {
            detector->adaptive_behavior_threshold = 0.0F;
        }
        if (detector->adaptive_timing_threshold < 0.0F) {
            detector->adaptive_timing_threshold = 0.0F;
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_anomaly_save_model(nimcp_anomaly_detector_t detector, const char* filepath) {
    if (!detector_is_valid(detector) || !filepath) {
        return NIMCP_INVALID_PARAM;
    }

    /* Not implemented in this version */
    return NIMCP_NOT_IMPLEMENTED;
}

nimcp_error_t nimcp_anomaly_load_model(nimcp_anomaly_detector_t detector, const char* filepath) {
    if (!detector_is_valid(detector) || !filepath) {
        return NIMCP_INVALID_PARAM;
    }

    /* Not implemented in this version */
    return NIMCP_NOT_IMPLEMENTED;
}
