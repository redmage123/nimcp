/**
 * @file nimcp_lgss_adversarial_detector.c
 * @brief LGSS Component A10: Perception Safety - Adversarial Detection Implementation
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Implementation of ML-based adversarial perturbation detection
 * WHY:  Detect adversarial attacks that try to fool neural network perception
 * HOW:  Multi-method approach: statistical, squeezing, transform, ML classifier
 *
 * @author NIMCP Development Team
 */

#include "security/lgss/perception/nimcp_lgss_adversarial_detector.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <float.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_adversarial_detector, MESH_ADAPTER_CATEGORY_SECURITY)


/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Adversarial detector internal structure
 */
struct lgss_adversarial_detector {
    uint32_t magic;                    /**< Magic number for validation */
    lgss_adversarial_config_t config;  /**< Configuration */
    lgss_adversarial_stats_t stats;    /**< Statistics */
    lgss_input_validator_t* validator; /**< Connected input validator */
    nimcp_mutex_t* mutex;              /**< Thread safety mutex */
    bool initialized;                  /**< Initialization flag */
};

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Validate magic number
 */
static inline bool detector_is_valid(const lgss_adversarial_detector_t* detector) {
    return detector != NULL && detector->magic == LGSS_ADVERSARIAL_DETECTOR_MAGIC;
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
 * @brief Calculate mean of float array
 */
static float calculate_mean(const float* data, size_t len) {
    if (!data || len == 0) return 0.0f;
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (float)(sum / (double)len);
}

/**
 * @brief Calculate standard deviation of float array
 */
static float calculate_stddev(const float* data, size_t len, float mean) {
    if (!data || len == 0) return 0.0f;
    double var_sum = 0.0;
    for (size_t i = 0; i < len; i++) {
        double diff = data[i] - mean;
        var_sum += diff * diff;
    }
    return sqrtf((float)(var_sum / (double)len));
}

/**
 * @brief Initialize detection result with defaults
 */
static void init_detection_result(adversarial_detection_result_t* result,
                                   const void* input_data,
                                   input_modality_t modality) {
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->input_data = input_data;
    result->modality = modality;
    result->status = LGSS_INPUT_VALID;
    result->is_adversarial = false;
    result->attack_type = LGSS_ADV_ATTACK_NONE;
    result->confidence = 0.0f;
    result->timestamp_us = get_timestamp_us();
}

/*=============================================================================
 * STATISTICAL ANALYSIS
 *============================================================================*/

/**
 * @brief Statistical analysis for adversarial detection
 *
 * WHAT: Detect statistical anomalies in input
 * WHY:  Adversarial perturbations often create statistical outliers
 * HOW:  Check for unusual variance, kurtosis, gradient patterns
 */
static float statistical_analysis_visual(const lgss_visual_input_t* input) {
    if (!input || !input->pixels) return 0.0f;

    size_t total = (size_t)input->width * input->height * input->channels;
    if (total == 0) return 0.0f;

    /* Calculate pixel statistics */
    double sum = 0.0;
    for (size_t i = 0; i < total; i++) {
        sum += input->pixels[i];
    }
    double mean = sum / (double)total;

    /* Calculate variance and look for anomalies */
    double var_sum = 0.0;
    uint32_t extreme_count = 0;
    for (size_t i = 0; i < total; i++) {
        double diff = input->pixels[i] - mean;
        var_sum += diff * diff;
        /* Count extreme values */
        if (input->pixels[i] == 0 || input->pixels[i] == 255) {
            extreme_count++;
        }
    }
    double variance = var_sum / (double)total;
    float stddev = sqrtf((float)variance);

    /* Anomaly indicators */
    float score = 0.0f;

    /* Very low variance (flat image) or very high variance (noisy) */
    if (stddev < 5.0f || stddev > 100.0f) {
        score += 0.3f;
    }

    /* High percentage of extreme values */
    float extreme_ratio = (float)extreme_count / (float)total;
    if (extreme_ratio > 0.3f) {
        score += 0.3f;
    }

    /* Check for unusual gradients (high-frequency patterns) */
    uint32_t gradient_spikes = 0;
    for (size_t i = 1; i < total; i++) {
        int diff = (int)input->pixels[i] - (int)input->pixels[i - 1];
        if (abs(diff) > 100) {
            gradient_spikes++;
        }
    }
    float spike_ratio = (float)gradient_spikes / (float)total;
    if (spike_ratio > 0.1f) {
        score += 0.4f;
    }

    return fminf(score, 1.0f);
}

static float statistical_analysis_audio(const lgss_audio_input_t* input) {
    if (!input || !input->samples || input->num_samples == 0) return 0.0f;

    float mean = calculate_mean(input->samples, input->num_samples);
    float stddev = calculate_stddev(input->samples, input->num_samples, mean);

    float score = 0.0f;

    /* Very low variance (silence) or very high variance (noise) */
    if (stddev < 0.001f || stddev > 0.9f) {
        score += 0.3f;
    }

    /* High DC offset */
    if (fabsf(mean) > 0.05f) {
        score += 0.2f;
    }

    /* Check for sudden amplitude changes */
    uint32_t spikes = 0;
    for (size_t i = 1; i < input->num_samples; i++) {
        float diff = fabsf(input->samples[i] - input->samples[i - 1]);
        if (diff > 0.5f) {
            spikes++;
        }
    }
    float spike_ratio = (float)spikes / (float)input->num_samples;
    if (spike_ratio > 0.05f) {
        score += 0.5f;
    }

    return fminf(score, 1.0f);
}

/*=============================================================================
 * FEATURE SQUEEZING
 *============================================================================*/

/**
 * @brief Feature squeezing defense for visual input
 *
 * WHAT: Compare output before/after color depth reduction
 * WHY:  Adversarial perturbations are often lost when bits are reduced
 * HOW:  Reduce bit depth, compare to original
 */
static float feature_squeezing_visual(const lgss_visual_input_t* input,
                                       uint8_t bit_depth) {
    if (!input || !input->pixels) return 0.0f;
    if (bit_depth >= 8 || bit_depth == 0) return 0.0f;

    size_t total = (size_t)input->width * input->height * input->channels;
    if (total == 0) return 0.0f;

    /* Calculate difference between original and squeezed */
    double diff_sum = 0.0;
    uint8_t shift = 8 - bit_depth;
    uint8_t mask = ~((1 << shift) - 1);

    for (size_t i = 0; i < total; i++) {
        uint8_t squeezed = (input->pixels[i] & mask) | (input->pixels[i] >> (8 - shift));
        int diff = (int)input->pixels[i] - (int)squeezed;
        diff_sum += (double)(diff * diff);
    }

    /* Normalize difference */
    double mse = diff_sum / (double)total;
    float rmse = sqrtf((float)mse);

    /* Higher RMSE suggests the input may rely on fine details */
    /* that could be adversarial perturbations */
    /* Typical clean images have RMSE around 1-5 with 4-bit squeezing */
    float score = rmse / 20.0f;  /* Normalize to ~1.0 for RMSE=20 */

    return fminf(score, 1.0f);
}

/*=============================================================================
 * CONFIGURATION API
 *============================================================================*/

nimcp_error_t lgss_adversarial_detector_default_config(
    lgss_adversarial_config_t* config)
{
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(*config));

    /* Enable all detection methods by default */
    config->detection_methods = LGSS_ADV_METHOD_ALL;
    config->handling_flags = LGSS_ADV_HANDLE_DEFAULT;

    /* Overall threshold */
    config->detection_threshold = LGSS_ADV_DEFAULT_THRESHOLD;

    /* Per-method thresholds */
    config->statistical_threshold = LGSS_ADV_STATISTICAL_THRESHOLD;
    config->squeezing_threshold = LGSS_ADV_SQUEEZING_THRESHOLD;
    config->transform_threshold = LGSS_ADV_TRANSFORM_THRESHOLD;
    config->ml_threshold = 0.5f;
    config->gradient_threshold = 0.5f;

    /* Feature squeezing parameters */
    config->squeezing_bit_depth = 4;  /* 4-bit color depth */
    config->squeezing_filter_size = 3;

    /* Transform defense parameters */
    config->num_transforms = 5;
    config->max_rotation_deg = 5.0f;
    config->max_translation_pct = 0.05f;

    /* ML model configuration */
    config->model_path = NULL;  /* No model by default (placeholder) */
    config->enable_online_learning = false;

    /* Performance tuning */
    config->enable_fast_mode = false;
    config->max_detection_time_ms = 50;

    /* Integration */
    config->enable_logging = true;
    config->enable_bio_async = false;

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * LIFECYCLE API
 *============================================================================*/

lgss_adversarial_detector_t* lgss_adversarial_detector_create(
    lgss_input_validator_t* validator,
    const lgss_adversarial_config_t* config)
{
    if (!validator) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validator is NULL");

        return NULL;

    }

    lgss_adversarial_detector_t* detector = nimcp_calloc(1, sizeof(*detector));
    if (!detector) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lgss_adversarial_detector_create: allocation failed");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        memcpy(&detector->config, config, sizeof(detector->config));
    } else {
        lgss_adversarial_detector_default_config(&detector->config);
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    detector->mutex = nimcp_mutex_create(&attr);
    if (!detector->mutex) {
        nimcp_free(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lgss_adversarial_detector_create: detector->mutex is NULL");
        return NULL;
    }

    /* Initialize */
    detector->magic = LGSS_ADVERSARIAL_DETECTOR_MAGIC;
    detector->validator = validator;
    detector->initialized = true;
    memset(&detector->stats, 0, sizeof(detector->stats));

    return detector;
}

void lgss_adversarial_detector_destroy(lgss_adversarial_detector_t* detector) {
    if (!detector) return;
    if (detector->magic != LGSS_ADVERSARIAL_DETECTOR_MAGIC) return;

    /* Invalidate magic first */
    detector->magic = 0;
    detector->initialized = false;

    /* Destroy mutex */
    if (detector->mutex) {
        nimcp_mutex_free(detector->mutex);
        detector->mutex = NULL;
    }

    /* Zero and free */
    memset(detector, 0, sizeof(*detector));
    nimcp_free(detector);
}

/*=============================================================================
 * VISUAL DETECTION
 *============================================================================*/

nimcp_error_t lgss_adversarial_detector_check_visual(
    lgss_adversarial_detector_t* detector,
    const lgss_visual_input_t* input,
    adversarial_detection_result_t* result)
{
    NIMCP_CHECK_THROW(detector_is_valid(detector), NIMCP_ERROR_INVALID_PARAM, "invalid detector");
    NIMCP_CHECK_THROW(input && result, NIMCP_ERROR_NULL_POINTER, "input or result is NULL");

    uint64_t start_time = get_timestamp_us();
    init_detection_result(result, input->pixels, LGSS_MODALITY_VISUAL);

    float max_score = 0.0f;
    uint32_t triggered = 0;

    /* Statistical analysis */
    if (detector->config.detection_methods & LGSS_ADV_METHOD_STATISTICAL) {
        result->statistical_score = statistical_analysis_visual(input);
        if (result->statistical_score > detector->config.statistical_threshold / 3.0f) {
            triggered |= LGSS_ADV_METHOD_STATISTICAL;
            if (result->statistical_score > max_score) {
                max_score = result->statistical_score;
                result->primary_method = "statistical";
            }
        }
    }

    /* Feature squeezing */
    if (detector->config.detection_methods & LGSS_ADV_METHOD_SQUEEZING) {
        result->squeezing_score = feature_squeezing_visual(
            input, detector->config.squeezing_bit_depth);
        if (result->squeezing_score > detector->config.squeezing_threshold) {
            triggered |= LGSS_ADV_METHOD_SQUEEZING;
            if (result->squeezing_score > max_score) {
                max_score = result->squeezing_score;
                result->primary_method = "squeezing";
            }
        }
    }

    /* ML classifier (placeholder - returns 0 until model is loaded) */
    if (detector->config.detection_methods & LGSS_ADV_METHOD_ML_CLASSIFIER) {
        /* TODO: Implement actual ML classifier when model is available */
        result->ml_classifier_score = 0.0f;
    }

    /* Aggregate results */
    result->triggered_methods = triggered;
    result->confidence = max_score;

    /* Decision threshold */
    if (max_score > detector->config.detection_threshold) {
        result->is_adversarial = true;
        result->status = LGSS_INPUT_ADVERSARIAL;
        result->attack_type = LGSS_ADV_ATTACK_PERTURBATION;  /* Most common */

        snprintf(result->explanation, sizeof(result->explanation),
                 "Adversarial visual input detected: %.1f%% confidence via %s",
                 max_score * 100.0f,
                 result->primary_method ? result->primary_method : "unknown");
    }

    /* Update statistics */
    nimcp_mutex_lock(detector->mutex);
    detector->stats.total_checks++;
    if (result->is_adversarial) {
        detector->stats.adversarial_detected++;
        detector->stats.perturbation_attacks++;
        if (triggered & LGSS_ADV_METHOD_STATISTICAL) {
            detector->stats.statistical_triggers++;
        }
        if (triggered & LGSS_ADV_METHOD_SQUEEZING) {
            detector->stats.squeezing_triggers++;
        }
    } else {
        detector->stats.clean_inputs++;
    }
    nimcp_mutex_unlock(detector->mutex);

    result->detection_time_us = (uint32_t)(get_timestamp_us() - start_time);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * AUDIO DETECTION
 *============================================================================*/

nimcp_error_t lgss_adversarial_detector_check_audio(
    lgss_adversarial_detector_t* detector,
    const lgss_audio_input_t* input,
    adversarial_detection_result_t* result)
{
    NIMCP_CHECK_THROW(detector_is_valid(detector), NIMCP_ERROR_INVALID_PARAM, "invalid detector");
    NIMCP_CHECK_THROW(input && result, NIMCP_ERROR_NULL_POINTER, "input or result is NULL");

    uint64_t start_time = get_timestamp_us();
    init_detection_result(result, input->samples, LGSS_MODALITY_AUDIO);

    float max_score = 0.0f;
    uint32_t triggered = 0;

    /* Statistical analysis */
    if (detector->config.detection_methods & LGSS_ADV_METHOD_STATISTICAL) {
        result->statistical_score = statistical_analysis_audio(input);
        if (result->statistical_score > detector->config.statistical_threshold / 3.0f) {
            triggered |= LGSS_ADV_METHOD_STATISTICAL;
            if (result->statistical_score > max_score) {
                max_score = result->statistical_score;
                result->primary_method = "statistical";
            }
        }
    }

    /* ML classifier (placeholder) */
    if (detector->config.detection_methods & LGSS_ADV_METHOD_ML_CLASSIFIER) {
        result->ml_classifier_score = 0.0f;
    }

    /* Aggregate results */
    result->triggered_methods = triggered;
    result->confidence = max_score;

    /* Decision threshold */
    if (max_score > detector->config.detection_threshold) {
        result->is_adversarial = true;
        result->status = LGSS_INPUT_ADVERSARIAL;
        result->attack_type = LGSS_ADV_ATTACK_NOISE;

        snprintf(result->explanation, sizeof(result->explanation),
                 "Adversarial audio input detected: %.1f%% confidence via %s",
                 max_score * 100.0f,
                 result->primary_method ? result->primary_method : "unknown");
    }

    /* Update statistics */
    nimcp_mutex_lock(detector->mutex);
    detector->stats.total_checks++;
    if (result->is_adversarial) {
        detector->stats.adversarial_detected++;
        detector->stats.noise_attacks++;
    } else {
        detector->stats.clean_inputs++;
    }
    nimcp_mutex_unlock(detector->mutex);

    result->detection_time_us = (uint32_t)(get_timestamp_us() - start_time);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * GENERIC DETECTION API
 *============================================================================*/

nimcp_error_t lgss_adversarial_detector_is_adversarial(
    lgss_adversarial_detector_t* detector,
    const void* input,
    input_modality_t modality,
    size_t input_size,
    adversarial_detection_result_t* result)
{
    NIMCP_CHECK_THROW(detector_is_valid(detector), NIMCP_ERROR_INVALID_PARAM, "invalid detector");
    NIMCP_CHECK_THROW(input && result, NIMCP_ERROR_NULL_POINTER, "input or result is NULL");

    /* For visual and audio, we need proper structures */
    /* For generic inputs, use statistical analysis only */
    uint64_t start_time = get_timestamp_us();
    init_detection_result(result, input, modality);

    /* Basic statistical analysis on raw bytes */
    if (detector->config.detection_methods & LGSS_ADV_METHOD_STATISTICAL) {
        const uint8_t* bytes = (const uint8_t*)input;

        /* Calculate entropy */
        uint32_t freq[256] = {0};
        for (size_t i = 0; i < input_size; i++) {
            freq[bytes[i]]++;
        }

        float entropy = 0.0f;
        float len_f = (float)input_size;
        for (int i = 0; i < 256; i++) {
            if (freq[i] > 0) {
                float p = (float)freq[i] / len_f;
                entropy -= p * log2f(p);
            }
        }

        /* Very high entropy might indicate adversarial noise */
        if (entropy > 7.8f) {
            result->statistical_score = (entropy - 7.0f) / 1.0f;
        }

        result->confidence = result->statistical_score;

        if (result->statistical_score > detector->config.detection_threshold) {
            result->is_adversarial = true;
            result->status = LGSS_INPUT_ADVERSARIAL;
            result->attack_type = LGSS_ADV_ATTACK_UNKNOWN;
            result->triggered_methods = LGSS_ADV_METHOD_STATISTICAL;
            result->primary_method = "statistical";

            snprintf(result->explanation, sizeof(result->explanation),
                     "High entropy input detected: entropy=%.2f", entropy);
        }
    }

    /* Update statistics */
    nimcp_mutex_lock(detector->mutex);
    detector->stats.total_checks++;
    if (result->is_adversarial) {
        detector->stats.adversarial_detected++;
        detector->stats.unknown_attacks++;
    } else {
        detector->stats.clean_inputs++;
    }
    nimcp_mutex_unlock(detector->mutex);

    result->detection_time_us = (uint32_t)(get_timestamp_us() - start_time);
    return NIMCP_SUCCESS;
}

float lgss_adversarial_detector_get_confidence(
    const lgss_adversarial_detector_t* detector,
    const adversarial_detection_result_t* result)
{
    if (!detector_is_valid(detector)) return -1.0f;
    if (!result) return -1.0f;
    return result->confidence;
}

/*=============================================================================
 * STATISTICS API
 *============================================================================*/

nimcp_error_t lgss_adversarial_detector_get_stats(
    const lgss_adversarial_detector_t* detector,
    lgss_adversarial_stats_t* stats)
{
    NIMCP_CHECK_THROW(detector_is_valid(detector), NIMCP_ERROR_INVALID_PARAM, "invalid detector");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(((lgss_adversarial_detector_t*)detector)->mutex);
    memcpy(stats, &detector->stats, sizeof(*stats));
    nimcp_mutex_unlock(((lgss_adversarial_detector_t*)detector)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t lgss_adversarial_detector_reset_stats(
    lgss_adversarial_detector_t* detector)
{
    NIMCP_CHECK_THROW(detector_is_valid(detector), NIMCP_ERROR_INVALID_PARAM, "invalid detector");

    nimcp_mutex_lock(detector->mutex);
    memset(&detector->stats, 0, sizeof(detector->stats));
    nimcp_mutex_unlock(detector->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t lgss_adversarial_detector_report_false_positive(
    lgss_adversarial_detector_t* detector)
{
    NIMCP_CHECK_THROW(detector_is_valid(detector), NIMCP_ERROR_INVALID_PARAM, "invalid detector");

    nimcp_mutex_lock(detector->mutex);
    detector->stats.false_positives++;
    /* Update precision estimate */
    if (detector->stats.adversarial_detected > 0) {
        detector->stats.estimated_precision =
            1.0f - ((float)detector->stats.false_positives /
                    (float)detector->stats.adversarial_detected);
    }
    nimcp_mutex_unlock(detector->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t lgss_adversarial_detector_report_false_negative(
    lgss_adversarial_detector_t* detector)
{
    NIMCP_CHECK_THROW(detector_is_valid(detector), NIMCP_ERROR_INVALID_PARAM, "invalid detector");

    nimcp_mutex_lock(detector->mutex);
    detector->stats.false_negatives++;
    /* Update recall estimate */
    uint64_t true_positives = detector->stats.adversarial_detected -
                              detector->stats.false_positives;
    uint64_t total_actual_adversarial = true_positives +
                                        detector->stats.false_negatives;
    if (total_actual_adversarial > 0) {
        detector->stats.estimated_recall =
            (float)true_positives / (float)total_actual_adversarial;
    }
    nimcp_mutex_unlock(detector->mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

const char* lgss_adv_attack_type_name(lgss_adv_attack_type_t attack_type) {
    static const char* names[] = {
        "NONE",
        "PERTURBATION",
        "PATCH",
        "NOISE",
        "UNIVERSAL",
        "PHYSICAL",
        "UNKNOWN"
    };
    if ((int)attack_type < 0 || attack_type > LGSS_ADV_ATTACK_UNKNOWN) {
        return "INVALID";
    }
    return names[attack_type];
}

const char* lgss_adv_method_name(lgss_adv_method_flags_t method) {
    switch (method) {
        case LGSS_ADV_METHOD_STATISTICAL:   return "STATISTICAL";
        case LGSS_ADV_METHOD_SQUEEZING:     return "SQUEEZING";
        case LGSS_ADV_METHOD_TRANSFORM:     return "TRANSFORM";
        case LGSS_ADV_METHOD_ML_CLASSIFIER: return "ML_CLASSIFIER";
        case LGSS_ADV_METHOD_GRADIENT:      return "GRADIENT";
        case LGSS_ADV_METHOD_ALL:           return "ALL";
        default:                            return "UNKNOWN";
    }
}
