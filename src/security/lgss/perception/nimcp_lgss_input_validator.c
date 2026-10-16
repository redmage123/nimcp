/**
 * @file nimcp_lgss_input_validator.c
 * @brief LGSS Component A10: Perception Safety - Input Validation Implementation
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Implementation of central input validation for perceptual inputs
 * WHY:  Protect the system from malformed, adversarial, and malicious inputs
 * HOW:  Multi-stage validation per modality with statistical and ML analysis
 *
 * @author NIMCP Development Team
 */

#include "security/lgss/perception/nimcp_lgss_input_validator.h"
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
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_input_validator, MESH_ADAPTER_CATEGORY_SECURITY)


/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Input validator internal structure
 */
struct lgss_input_validator {
    uint32_t magic;                    /**< Magic number for validation */
    lgss_input_validator_config_t config; /**< Configuration */
    lgss_validator_stats_t stats;      /**< Statistics */
    nimcp_mutex_t* mutex;              /**< Thread safety mutex */
    bool initialized;                  /**< Initialization flag */
};

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Validate magic number
 */
static inline bool validator_is_valid(const lgss_input_validator_t* validator) {
    return validator != NULL && validator->magic == LGSS_INPUT_VALIDATOR_MAGIC;
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
 * @brief Calculate Shannon entropy of byte array
 *
 * WHAT: Compute entropy of byte sequence
 * WHY:  Anomalous entropy may indicate attacks
 * HOW:  H = -sum(p(x) * log2(p(x)))
 */
static float calculate_entropy(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0.0f;

    uint32_t freq[256] = {0};
    for (size_t i = 0; i < len; i++) {
        freq[data[i]]++;
    }

    float entropy = 0.0f;
    float len_f = (float)len;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            float p = (float)freq[i] / len_f;
            entropy -= p * log2f(p);
        }
    }
    return entropy;
}

/**
 * @brief Calculate mean and variance of float array
 */
static void calculate_stats_float(const float* data, size_t len,
                                   float* mean_out, float* var_out) {
    if (!data || len == 0) {
        if (mean_out) *mean_out = 0.0f;
        if (var_out) *var_out = 0.0f;
        return;
    }

    /* Calculate mean */
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    double mean = sum / (double)len;
    if (mean_out) *mean_out = (float)mean;

    /* Calculate variance */
    if (var_out) {
        double var_sum = 0.0;
        for (size_t i = 0; i < len; i++) {
            double diff = data[i] - mean;
            var_sum += diff * diff;
        }
        *var_out = (float)(var_sum / (double)len);
    }
}

/**
 * @brief Check for NaN/Inf values in float array
 */
static bool check_float_validity(const float* data, size_t len) {
    if (!data) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (isnan(data[i]) || isinf(data[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check for range violations in float array
 */
static bool check_range_float(const float* data, size_t len,
                               float min_val, float max_val) {
    if (!data) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (data[i] < min_val || data[i] > max_val) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Initialize validation result with defaults
 */
static void init_validation_result(lgss_validation_result_t* result,
                                    input_modality_t modality) {
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->status = LGSS_INPUT_VALID;
    result->modality = modality;
    result->confidence = 1.0f;
    result->timestamp_us = get_timestamp_us();
}

/*=============================================================================
 * CONFIGURATION API
 *============================================================================*/

nimcp_error_t lgss_input_validator_default_config(
    lgss_input_validator_config_t* config)
{
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(*config));

    /* Enable all modality validations */
    config->enable_visual_validation = true;
    config->enable_audio_validation = true;
    config->enable_text_validation = true;
    config->enable_proprio_validation = true;
    config->enable_tactile_validation = true;

    /* Set default thresholds */
    config->anomaly_threshold = LGSS_DEFAULT_ANOMALY_THRESHOLD;
    config->adversarial_threshold = LGSS_DEFAULT_ADVERSARIAL_THRESHOLD;
    config->injection_threshold = LGSS_DEFAULT_INJECTION_THRESHOLD;
    config->overflow_threshold = LGSS_DEFAULT_OVERFLOW_THRESHOLD;

    /* Per-modality thresholds */
    config->visual_anomaly_threshold = 0.75f;
    config->audio_anomaly_threshold = 0.70f;
    config->text_anomaly_threshold = 0.65f;

    /* Enable all validation checks */
    config->validation_flags = LGSS_CHECK_ALL;

    /* Performance tuning */
    config->enable_caching = false;
    config->enable_fast_mode = false;
    config->max_validation_time_ms = 100;

    /* Integration */
    config->enable_logging = true;
    config->enable_bio_async = false;
    config->alert_module_id = 0;

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * LIFECYCLE API
 *============================================================================*/

lgss_input_validator_t* lgss_input_validator_create(
    const lgss_input_validator_config_t* config)
{
    lgss_input_validator_t* validator = nimcp_calloc(1, sizeof(*validator));
    if (!validator) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "validator is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        memcpy(&validator->config, config, sizeof(validator->config));
    } else {
        lgss_input_validator_default_config(&validator->config);
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    validator->mutex = nimcp_mutex_create(&attr);
    if (!validator->mutex) {
        nimcp_free(validator);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lgss_input_validator_create: validator->mutex is NULL");
        return NULL;
    }

    /* Initialize */
    validator->magic = LGSS_INPUT_VALIDATOR_MAGIC;
    validator->initialized = true;
    memset(&validator->stats, 0, sizeof(validator->stats));

    return validator;
}

void lgss_input_validator_destroy(lgss_input_validator_t* validator) {
    if (!validator) return;
    if (validator->magic != LGSS_INPUT_VALIDATOR_MAGIC) return;

    /* Invalidate magic first */
    validator->magic = 0;
    validator->initialized = false;

    /* Destroy mutex */
    if (validator->mutex) {
        nimcp_mutex_free(validator->mutex);
        validator->mutex = NULL;
    }

    /* Zero and free */
    memset(validator, 0, sizeof(*validator));
    nimcp_free(validator);
}

/*=============================================================================
 * VISUAL VALIDATION
 *============================================================================*/

nimcp_error_t lgss_input_validator_check_visual(
    lgss_input_validator_t* validator,
    const lgss_visual_input_t* input,
    lgss_validation_result_t* result)
{
    NIMCP_CHECK_THROW(validator_is_valid(validator), NIMCP_ERROR_INVALID_PARAM, "invalid validator");
    NIMCP_CHECK_THROW(input && result, NIMCP_ERROR_NULL_POINTER, "input or result is NULL");
    if (!validator->config.enable_visual_validation) {
        init_validation_result(result, LGSS_MODALITY_VISUAL);
        return NIMCP_SUCCESS;
    }

    uint64_t start_time = get_timestamp_us();
    init_validation_result(result, LGSS_MODALITY_VISUAL);

    /* Structure validation */
    if (validator->config.validation_flags & LGSS_CHECK_STRUCTURE) {
        if (!input->pixels) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_STRUCTURE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Visual input has NULL pixel data");
            goto done;
        }
        if (input->width == 0 || input->height == 0) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_STRUCTURE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Visual input has zero dimensions");
            goto done;
        }
        if (input->channels != 1 && input->channels != 3 && input->channels != 4) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_STRUCTURE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Invalid channel count: %u", input->channels);
            goto done;
        }
    }

    /* Overflow check */
    if (validator->config.validation_flags & LGSS_CHECK_OVERFLOW) {
        if (input->width > LGSS_MAX_VISUAL_WIDTH ||
            input->height > LGSS_MAX_VISUAL_HEIGHT) {
            result->status = LGSS_INPUT_OVERFLOW;
            result->failed_checks |= LGSS_CHECK_OVERFLOW;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Visual dimensions exceed maximum: %ux%u > %ux%u",
                     input->width, input->height,
                     LGSS_MAX_VISUAL_WIDTH, LGSS_MAX_VISUAL_HEIGHT);
            goto done;
        }
    }

    /* Statistical anomaly check */
    if (validator->config.validation_flags & LGSS_CHECK_ANOMALY) {
        size_t total_bytes = (size_t)input->width * input->height * input->channels;
        float entropy = calculate_entropy(input->pixels, total_bytes);

        /* Very low entropy (all same color) or very high (random noise) is suspicious */
        if (entropy < 0.5f || entropy > 7.9f) {
            result->anomaly_score = 0.8f;
            if (result->anomaly_score > validator->config.visual_anomaly_threshold) {
                result->status = LGSS_INPUT_SUSPICIOUS;
                result->failed_checks |= LGSS_CHECK_ANOMALY;
                snprintf(result->explanation, sizeof(result->explanation),
                         "Abnormal entropy: %.2f (expected 2.0-7.5)", entropy);
                goto done;
            }
        }
    }

    result->input_size = (size_t)input->width * input->height * input->channels;

done:
    nimcp_mutex_lock(validator->mutex);
    validator->stats.total_validations++;
    validator->stats.visual_validations++;
    if (result->status == LGSS_INPUT_VALID) {
        validator->stats.valid_count++;
    } else if (result->status == LGSS_INPUT_MALFORMED) {
        validator->stats.malformed_count++;
    } else if (result->status == LGSS_INPUT_ADVERSARIAL) {
        validator->stats.adversarial_count++;
    } else if (result->status == LGSS_INPUT_OVERFLOW) {
        validator->stats.overflow_count++;
    } else if (result->status == LGSS_INPUT_SUSPICIOUS) {
        validator->stats.suspicious_count++;
    }
    nimcp_mutex_unlock(validator->mutex);

    result->validation_time_us = (uint32_t)(get_timestamp_us() - start_time);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * AUDIO VALIDATION
 *============================================================================*/

nimcp_error_t lgss_input_validator_check_audio(
    lgss_input_validator_t* validator,
    const lgss_audio_input_t* input,
    lgss_validation_result_t* result)
{
    NIMCP_CHECK_THROW(validator_is_valid(validator), NIMCP_ERROR_INVALID_PARAM, "invalid validator");
    NIMCP_CHECK_THROW(input && result, NIMCP_ERROR_NULL_POINTER, "input or result is NULL");
    if (!validator->config.enable_audio_validation) {
        init_validation_result(result, LGSS_MODALITY_AUDIO);
        return NIMCP_SUCCESS;
    }

    uint64_t start_time = get_timestamp_us();
    init_validation_result(result, LGSS_MODALITY_AUDIO);

    /* Structure validation */
    if (validator->config.validation_flags & LGSS_CHECK_STRUCTURE) {
        if (!input->samples) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_STRUCTURE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Audio input has NULL sample data");
            goto done;
        }
        if (input->num_samples == 0) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_STRUCTURE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Audio input has zero samples");
            goto done;
        }
        if (input->sample_rate < 8000 || input->sample_rate > 192000) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_STRUCTURE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Invalid sample rate: %u Hz", input->sample_rate);
            goto done;
        }
    }

    /* Overflow check */
    if (validator->config.validation_flags & LGSS_CHECK_OVERFLOW) {
        if (input->num_samples > LGSS_MAX_AUDIO_SAMPLES) {
            result->status = LGSS_INPUT_OVERFLOW;
            result->failed_checks |= LGSS_CHECK_OVERFLOW;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Audio samples exceed maximum: %zu > %u",
                     input->num_samples, LGSS_MAX_AUDIO_SAMPLES);
            goto done;
        }
    }

    /* Range validation */
    if (validator->config.validation_flags & LGSS_CHECK_RANGE) {
        /* Check for NaN/Inf */
        if (!check_float_validity(input->samples, input->num_samples)) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_RANGE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Audio contains NaN or Inf values");
            goto done;
        }
        /* Check range (normalized audio: -1.0 to 1.0) */
        if (!check_range_float(input->samples, input->num_samples, -1.0f, 1.0f)) {
            result->status = LGSS_INPUT_SUSPICIOUS;
            result->failed_checks |= LGSS_CHECK_RANGE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Audio samples out of normalized range [-1, 1]");
            goto done;
        }
    }

    /* Statistical anomaly check */
    if (validator->config.validation_flags & LGSS_CHECK_ANOMALY) {
        float mean, variance;
        calculate_stats_float(input->samples, input->num_samples, &mean, &variance);

        /* Audio should have near-zero mean (DC offset check) */
        if (fabsf(mean) > 0.1f) {
            result->anomaly_score = fabsf(mean);
            if (result->anomaly_score > validator->config.audio_anomaly_threshold) {
                result->status = LGSS_INPUT_SUSPICIOUS;
                result->failed_checks |= LGSS_CHECK_ANOMALY;
                snprintf(result->explanation, sizeof(result->explanation),
                         "High DC offset detected: %.3f", mean);
                goto done;
            }
        }
    }

    result->input_size = input->num_samples * sizeof(float);

done:
    nimcp_mutex_lock(validator->mutex);
    validator->stats.total_validations++;
    validator->stats.audio_validations++;
    if (result->status == LGSS_INPUT_VALID) {
        validator->stats.valid_count++;
    } else if (result->status == LGSS_INPUT_MALFORMED) {
        validator->stats.malformed_count++;
    } else if (result->status == LGSS_INPUT_OVERFLOW) {
        validator->stats.overflow_count++;
    } else if (result->status == LGSS_INPUT_SUSPICIOUS) {
        validator->stats.suspicious_count++;
    }
    nimcp_mutex_unlock(validator->mutex);

    result->validation_time_us = (uint32_t)(get_timestamp_us() - start_time);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * TEXT VALIDATION
 *============================================================================*/

nimcp_error_t lgss_input_validator_check_text(
    lgss_input_validator_t* validator,
    const lgss_text_input_t* input,
    lgss_validation_result_t* result)
{
    NIMCP_CHECK_THROW(validator_is_valid(validator), NIMCP_ERROR_INVALID_PARAM, "invalid validator");
    NIMCP_CHECK_THROW(input && result, NIMCP_ERROR_NULL_POINTER, "input or result is NULL");
    if (!validator->config.enable_text_validation) {
        init_validation_result(result, LGSS_MODALITY_TEXT);
        return NIMCP_SUCCESS;
    }

    uint64_t start_time = get_timestamp_us();
    init_validation_result(result, LGSS_MODALITY_TEXT);

    /* Structure validation */
    if (validator->config.validation_flags & LGSS_CHECK_STRUCTURE) {
        if (!input->text) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_STRUCTURE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Text input has NULL content");
            goto done;
        }
    }

    /* Overflow check */
    if (validator->config.validation_flags & LGSS_CHECK_OVERFLOW) {
        if (input->length > LGSS_MAX_TEXT_LENGTH) {
            result->status = LGSS_INPUT_OVERFLOW;
            result->failed_checks |= LGSS_CHECK_OVERFLOW;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Text length exceeds maximum: %zu > %u",
                     input->length, LGSS_MAX_TEXT_LENGTH);
            goto done;
        }
    }

    /* Check for null bytes (potential injection) */
    if (validator->config.validation_flags & LGSS_CHECK_INJECTION) {
        for (size_t i = 0; i < input->length; i++) {
            if (input->text[i] == '\0' && i < input->length - 1) {
                /* Null byte before end of declared length */
                result->status = LGSS_INPUT_INJECTION;
                result->failed_checks |= LGSS_CHECK_INJECTION;
                result->injection_score = 0.9f;
                snprintf(result->explanation, sizeof(result->explanation),
                         "Null byte injection detected at offset %zu", i);
                goto done;
            }
        }
    }

    /* Statistical anomaly check */
    if (validator->config.validation_flags & LGSS_CHECK_ANOMALY) {
        float entropy = calculate_entropy((const uint8_t*)input->text, input->length);

        /* Very high entropy text might be encoded/encrypted content */
        if (entropy > 6.5f && input->length > 100) {
            result->anomaly_score = (entropy - 6.5f) / 1.5f;
            if (result->anomaly_score > validator->config.text_anomaly_threshold) {
                result->status = LGSS_INPUT_SUSPICIOUS;
                result->failed_checks |= LGSS_CHECK_ANOMALY;
                snprintf(result->explanation, sizeof(result->explanation),
                         "High entropy text detected: %.2f (possible encoding)", entropy);
                goto done;
            }
        }
    }

    result->input_size = input->length;

done:
    nimcp_mutex_lock(validator->mutex);
    validator->stats.total_validations++;
    validator->stats.text_validations++;
    if (result->status == LGSS_INPUT_VALID) {
        validator->stats.valid_count++;
    } else if (result->status == LGSS_INPUT_MALFORMED) {
        validator->stats.malformed_count++;
    } else if (result->status == LGSS_INPUT_INJECTION) {
        validator->stats.injection_count++;
    } else if (result->status == LGSS_INPUT_OVERFLOW) {
        validator->stats.overflow_count++;
    } else if (result->status == LGSS_INPUT_SUSPICIOUS) {
        validator->stats.suspicious_count++;
    }
    nimcp_mutex_unlock(validator->mutex);

    result->validation_time_us = (uint32_t)(get_timestamp_us() - start_time);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PROPRIOCEPTIVE VALIDATION
 *============================================================================*/

nimcp_error_t lgss_input_validator_check_proprio(
    lgss_input_validator_t* validator,
    const lgss_proprio_input_t* input,
    lgss_validation_result_t* result)
{
    NIMCP_CHECK_THROW(validator_is_valid(validator), NIMCP_ERROR_INVALID_PARAM, "invalid validator");
    NIMCP_CHECK_THROW(input && result, NIMCP_ERROR_NULL_POINTER, "input or result is NULL");
    if (!validator->config.enable_proprio_validation) {
        init_validation_result(result, LGSS_MODALITY_PROPRIOCEPTIVE);
        return NIMCP_SUCCESS;
    }

    uint64_t start_time = get_timestamp_us();
    init_validation_result(result, LGSS_MODALITY_PROPRIOCEPTIVE);

    /* Structure validation */
    if (validator->config.validation_flags & LGSS_CHECK_STRUCTURE) {
        if (!input->joint_positions) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_STRUCTURE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Proprioceptive input has NULL position data");
            goto done;
        }
        if (input->num_joints == 0) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_STRUCTURE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Proprioceptive input has zero joints");
            goto done;
        }
    }

    /* Overflow check */
    if (validator->config.validation_flags & LGSS_CHECK_OVERFLOW) {
        if (input->num_joints > LGSS_MAX_PROPRIO_JOINTS) {
            result->status = LGSS_INPUT_OVERFLOW;
            result->failed_checks |= LGSS_CHECK_OVERFLOW;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Joint count exceeds maximum: %u > %u",
                     input->num_joints, LGSS_MAX_PROPRIO_JOINTS);
            goto done;
        }
    }

    /* Range validation */
    if (validator->config.validation_flags & LGSS_CHECK_RANGE) {
        if (!check_float_validity(input->joint_positions, input->num_joints)) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_RANGE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Joint positions contain NaN or Inf values");
            goto done;
        }
        /* Joint positions typically in radians: -2*PI to 2*PI */
        if (!check_range_float(input->joint_positions, input->num_joints,
                               -NIMCP_TWO_PI_F, NIMCP_TWO_PI_F)) {
            result->status = LGSS_INPUT_SUSPICIOUS;
            result->failed_checks |= LGSS_CHECK_RANGE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Joint positions out of expected range");
            goto done;
        }
    }

    result->input_size = input->num_joints * sizeof(float);

done:
    nimcp_mutex_lock(validator->mutex);
    validator->stats.total_validations++;
    validator->stats.proprio_validations++;
    if (result->status == LGSS_INPUT_VALID) {
        validator->stats.valid_count++;
    } else {
        validator->stats.malformed_count++;
    }
    nimcp_mutex_unlock(validator->mutex);

    result->validation_time_us = (uint32_t)(get_timestamp_us() - start_time);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * TACTILE VALIDATION
 *============================================================================*/

nimcp_error_t lgss_input_validator_check_tactile(
    lgss_input_validator_t* validator,
    const lgss_tactile_input_t* input,
    lgss_validation_result_t* result)
{
    NIMCP_CHECK_THROW(validator_is_valid(validator), NIMCP_ERROR_INVALID_PARAM, "invalid validator");
    NIMCP_CHECK_THROW(input && result, NIMCP_ERROR_NULL_POINTER, "input or result is NULL");
    if (!validator->config.enable_tactile_validation) {
        init_validation_result(result, LGSS_MODALITY_TACTILE);
        return NIMCP_SUCCESS;
    }

    uint64_t start_time = get_timestamp_us();
    init_validation_result(result, LGSS_MODALITY_TACTILE);

    /* Structure validation */
    if (validator->config.validation_flags & LGSS_CHECK_STRUCTURE) {
        if (!input->pressure_values) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_STRUCTURE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Tactile input has NULL pressure data");
            goto done;
        }
        if (input->num_points == 0) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_STRUCTURE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Tactile input has zero touch points");
            goto done;
        }
    }

    /* Overflow check */
    if (validator->config.validation_flags & LGSS_CHECK_OVERFLOW) {
        if (input->num_points > LGSS_MAX_TACTILE_POINTS) {
            result->status = LGSS_INPUT_OVERFLOW;
            result->failed_checks |= LGSS_CHECK_OVERFLOW;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Touch points exceed maximum: %u > %u",
                     input->num_points, LGSS_MAX_TACTILE_POINTS);
            goto done;
        }
    }

    /* Range validation */
    if (validator->config.validation_flags & LGSS_CHECK_RANGE) {
        if (!check_float_validity(input->pressure_values, input->num_points)) {
            result->status = LGSS_INPUT_MALFORMED;
            result->failed_checks |= LGSS_CHECK_RANGE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Pressure values contain NaN or Inf");
            goto done;
        }
        /* Pressure typically normalized 0-1 */
        if (!check_range_float(input->pressure_values, input->num_points, 0.0f, 1.0f)) {
            result->status = LGSS_INPUT_SUSPICIOUS;
            result->failed_checks |= LGSS_CHECK_RANGE;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Pressure values out of normalized range [0, 1]");
            goto done;
        }
    }

    result->input_size = input->num_points * sizeof(float);

done:
    nimcp_mutex_lock(validator->mutex);
    validator->stats.total_validations++;
    validator->stats.tactile_validations++;
    if (result->status == LGSS_INPUT_VALID) {
        validator->stats.valid_count++;
    } else {
        validator->stats.malformed_count++;
    }
    nimcp_mutex_unlock(validator->mutex);

    result->validation_time_us = (uint32_t)(get_timestamp_us() - start_time);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * CENTRAL VALIDATION API
 *============================================================================*/

nimcp_error_t lgss_input_validator_check(
    lgss_input_validator_t* validator,
    const lgss_input_t* input,
    lgss_validation_result_t* result)
{
    NIMCP_CHECK_THROW(validator_is_valid(validator), NIMCP_ERROR_INVALID_PARAM, "invalid validator");
    NIMCP_CHECK_THROW(input && result, NIMCP_ERROR_NULL_POINTER, "input or result is NULL");

    switch (input->modality) {
        case LGSS_MODALITY_VISUAL:
            return lgss_input_validator_check_visual(validator, &input->data.visual, result);
        case LGSS_MODALITY_AUDIO:
            return lgss_input_validator_check_audio(validator, &input->data.audio, result);
        case LGSS_MODALITY_TEXT:
            return lgss_input_validator_check_text(validator, &input->data.text, result);
        case LGSS_MODALITY_PROPRIOCEPTIVE:
            return lgss_input_validator_check_proprio(validator, &input->data.proprio, result);
        case LGSS_MODALITY_TACTILE:
            return lgss_input_validator_check_tactile(validator, &input->data.tactile, result);
        default:
            init_validation_result(result, input->modality);
            result->status = LGSS_INPUT_MALFORMED;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Unknown modality: %d", (int)input->modality);
            return NIMCP_SUCCESS;
    }
}

/*=============================================================================
 * STATISTICS API
 *============================================================================*/

nimcp_error_t lgss_input_validator_get_stats(
    const lgss_input_validator_t* validator,
    lgss_validator_stats_t* stats)
{
    NIMCP_CHECK_THROW(validator_is_valid(validator), NIMCP_ERROR_INVALID_PARAM, "invalid validator");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(((lgss_input_validator_t*)validator)->mutex);
    memcpy(stats, &validator->stats, sizeof(*stats));
    nimcp_mutex_unlock(((lgss_input_validator_t*)validator)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t lgss_input_validator_reset_stats(lgss_input_validator_t* validator) {
    NIMCP_CHECK_THROW(validator_is_valid(validator), NIMCP_ERROR_INVALID_PARAM, "invalid validator");

    nimcp_mutex_lock(validator->mutex);
    memset(&validator->stats, 0, sizeof(validator->stats));
    nimcp_mutex_unlock(validator->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t lgss_input_validator_report_false_positive(
    lgss_input_validator_t* validator)
{
    NIMCP_CHECK_THROW(validator_is_valid(validator), NIMCP_ERROR_INVALID_PARAM, "invalid validator");

    nimcp_mutex_lock(validator->mutex);
    validator->stats.false_positives++;
    /* Update precision estimate */
    uint64_t total_positives = validator->stats.total_validations -
                               validator->stats.valid_count;
    if (total_positives > 0) {
        validator->stats.estimated_precision =
            1.0f - ((float)validator->stats.false_positives / (float)total_positives);
    }
    nimcp_mutex_unlock(validator->mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

const char* lgss_validation_status_name(input_validation_status_t status) {
    static const char* names[] = {
        "VALID",
        "MALFORMED",
        "ADVERSARIAL",
        "INJECTION",
        "OVERFLOW",
        "SUSPICIOUS"
    };
    if ((int)status < 0 || status > LGSS_INPUT_SUSPICIOUS) {
        return "UNKNOWN";
    }
    return names[status];
}

const char* lgss_modality_name(input_modality_t modality) {
    static const char* names[] = {
        "VISUAL",
        "AUDIO",
        "TEXT",
        "PROPRIOCEPTIVE",
        "TACTILE"
    };
    if ((int)modality < 0 || modality >= LGSS_MODALITY_COUNT) {
        return "UNKNOWN";
    }
    return names[modality];
}

const char* lgss_validation_flag_name(lgss_validation_flags_t flag) {
    switch (flag) {
        case LGSS_CHECK_STRUCTURE:   return "STRUCTURE";
        case LGSS_CHECK_RANGE:       return "RANGE";
        case LGSS_CHECK_ANOMALY:     return "ANOMALY";
        case LGSS_CHECK_ADVERSARIAL: return "ADVERSARIAL";
        case LGSS_CHECK_INJECTION:   return "INJECTION";
        case LGSS_CHECK_OVERFLOW:    return "OVERFLOW";
        case LGSS_CHECK_ALL:         return "ALL";
        default:                     return "UNKNOWN";
    }
}
