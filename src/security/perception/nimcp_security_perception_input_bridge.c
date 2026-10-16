/**
 * @file nimcp_security_perception_input_bridge.c
 * @brief Security-Perception Input Bridge - Input Validation for Audio/Visual Streams
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bridge for validating raw perceptual inputs (audio samples, image frames)
 *       before they enter the perception processing pipeline.
 * WHY:  Adversarial inputs can compromise perception systems at the earliest stage.
 *       Validating raw input data (before feature extraction) provides defense-in-depth.
 * HOW:  Integrate security validation with cochlea (audio) and visual cortex (visual),
 *       using BBB for input gating and anomaly detector for ML-based threat detection.
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#include "security/perception/nimcp_security_perception_input_bridge.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_perception_input_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Static String Tables
 * ============================================================================ */

static const char* s_validation_result_names[] = {
    "VALID",
    "RANGE_WARNING",
    "STATS_WARNING",
    "ANOMALY_DETECTED",
    "ADVERSARIAL_DETECTED",
    "SPOOFING_DETECTED",
    "MALFORMED",
    "REJECTED"
};

static const char* s_gate_action_names[] = {
    "PASS",
    "ATTENUATE",
    "SANITIZE",
    "HOLD",
    "BLOCK"
};

static const char* s_state_names[] = {
    "UNINITIALIZED",
    "READY",
    "PROCESSING",
    "DEGRADED",
    "ERROR"
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Check if a float is valid (not NaN or Inf)
 */
static bool is_valid_float(float value) {
    return !isnan(value) && !isinf(value);
}

/**
 * @brief Compute audio statistics from samples
 */
static void compute_audio_stats(
    const float* samples,
    size_t num_samples,
    float* mean,
    float* variance,
    float* peak,
    float* clipping_ratio
) {
    if (!samples || num_samples == 0) {
        *mean = 0.0f;
        *variance = 0.0f;
        *peak = 0.0f;
        *clipping_ratio = 0.0f;
        return;
    }

    /* First pass: compute mean and find peak */
    double sum = 0.0;
    float max_abs = 0.0f;
    uint32_t clipped = 0;
    const float clip_threshold = 0.99f;

    for (size_t i = 0; i < num_samples; i++) {
        float s = samples[i];
        sum += s;
        float abs_s = fabsf(s);
        if (abs_s > max_abs) max_abs = abs_s;
        if (abs_s >= clip_threshold) clipped++;
    }

    *mean = (float)(sum / (double)num_samples);
    *peak = max_abs;
    *clipping_ratio = (float)clipped / (float)num_samples;

    /* Second pass: compute variance */
    double var_sum = 0.0;
    for (size_t i = 0; i < num_samples; i++) {
        double diff = samples[i] - *mean;
        var_sum += diff * diff;
    }
    *variance = (float)(var_sum / (double)num_samples);
}

/**
 * @brief Compute visual statistics from pixels
 */
static void compute_visual_stats(
    const uint8_t* pixels,
    size_t num_pixels,
    float* mean_intensity,
    float* variance,
    float* saturation_ratio
) {
    if (!pixels || num_pixels == 0) {
        *mean_intensity = 0.0f;
        *variance = 0.0f;
        *saturation_ratio = 0.0f;
        return;
    }

    /* First pass: compute mean */
    double sum = 0.0;
    uint32_t saturated = 0;

    for (size_t i = 0; i < num_pixels; i++) {
        sum += pixels[i];
        if (pixels[i] == 0 || pixels[i] == 255) saturated++;
    }

    *mean_intensity = (float)(sum / (double)num_pixels);
    *saturation_ratio = (float)saturated / (float)num_pixels;

    /* Second pass: compute variance */
    double var_sum = 0.0;
    for (size_t i = 0; i < num_pixels; i++) {
        double diff = pixels[i] - *mean_intensity;
        var_sum += diff * diff;
    }
    *variance = (float)(var_sum / (double)num_pixels);
}

/**
 * @brief Detect ultrasonic content in audio (high frequency energy)
 */
static bool detect_ultrasonic_content(
    const float* samples,
    size_t num_samples,
    uint32_t sample_rate
) {
    /* Simplified ultrasonic detection: check for high-frequency oscillations */
    /* Human hearing range is 20Hz-20kHz; ultrasonic is > 20kHz */
    if (num_samples < 4 || sample_rate < 40000) {
        return false;
    }

    /* Count zero crossings - high count suggests high frequency content */
    uint32_t zero_crossings = 0;
    for (size_t i = 1; i < num_samples; i++) {
        if ((samples[i-1] >= 0.0f && samples[i] < 0.0f) ||
            (samples[i-1] < 0.0f && samples[i] >= 0.0f)) {
            zero_crossings++;
        }
    }

    /* Estimate dominant frequency from zero crossing rate */
    float duration_sec = (float)num_samples / (float)sample_rate;
    float estimated_freq = (float)zero_crossings / (2.0f * duration_sec);

    return estimated_freq > 20000.0f;
}

/**
 * @brief Detect adversarial patterns in visual input
 *
 * Simple heuristic: look for unnaturally uniform noise patterns
 */
static bool detect_adversarial_visual_pattern(
    const uint8_t* pixels,
    uint32_t width,
    uint32_t height,
    uint32_t channels
) {
    if (!pixels || width < 8 || height < 8) {
        return false;
    }

    /* Check for repeating patterns (potential adversarial patches) */
    /* Look for grid-like patterns with specific stride */
    uint32_t pattern_count = 0;
    uint32_t row_stride = width * channels;

    for (uint32_t y = 0; y < height - 1; y++) {
        for (uint32_t x = 0; x < width - 1; x++) {
            size_t idx = y * row_stride + x * channels;
            size_t idx_right = idx + channels;
            size_t idx_down = idx + row_stride;

            /* Check for checkerboard pattern (common in adversarial) */
            uint8_t curr = pixels[idx];
            uint8_t right = pixels[idx_right];
            uint8_t down = pixels[idx_down];

            if (abs((int)curr - (int)right) > 200 &&
                abs((int)curr - (int)down) > 200) {
                pattern_count++;
            }
        }
    }

    /* If more than 10% of pixels show adversarial-like pattern */
    float ratio = (float)pattern_count / (float)(width * height);
    return ratio > 0.1f;
}

/**
 * @brief Update bridge active state based on connections
 */
static void update_bridge_active_state(security_perception_input_bridge_t* bridge) {
    bool has_any_connection = bridge->state.cochlea_connected ||
                              bridge->state.visual_cortex_connected;
    bridge->base.bridge_active = has_any_connection;

    if (bridge->state.operational_state == SEC_INPUT_STATE_UNINITIALIZED) {
        bridge->state.operational_state = SEC_INPUT_STATE_READY;
    }
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int security_perception_input_default_config(sec_percept_input_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(sec_percept_input_config_t));

    /* Audio validation settings */
    config->enable_audio_validation = true;
    config->audio_anomaly_threshold = SEC_PERCEPT_INPUT_DEFAULT_AUDIO_THRESHOLD;
    config->audio_min_value = -1.0f;
    config->audio_max_value = 1.0f;
    config->detect_ultrasonic = true;
    config->detect_infrasonic = false;

    /* Visual validation settings */
    config->enable_visual_validation = true;
    config->visual_anomaly_threshold = SEC_PERCEPT_INPUT_DEFAULT_VISUAL_THRESHOLD;
    config->visual_min_value = 0;
    config->visual_max_value = 255;
    config->detect_adversarial_patches = true;
    config->detect_spoofing = true;

    /* Anomaly detection settings */
    config->statistical_threshold = 0.75f;
    config->ml_confidence_threshold = 0.8f;
    config->enable_online_learning = false;

    /* Gating settings */
    config->gate_threshold = SEC_PERCEPT_INPUT_DEFAULT_GATE_THRESHOLD;
    config->attenuation_factor = 0.5f;
    config->enable_auto_gating = true;

    /* Integration settings */
    config->enable_bbb = true;
    config->enable_anomaly_detector = true;
    config->enable_bio_async = false;
    config->enable_logging = true;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

security_perception_input_bridge_t* security_perception_input_bridge_create(
    const sec_percept_input_config_t* config
) {
    /* Allocate bridge structure */
    security_perception_input_bridge_t* bridge = nimcp_malloc(
        sizeof(security_perception_input_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_perception_input_bridge_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate security_perception_input_bridge");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(security_perception_input_bridge_t));

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SEC_PERCEPT_INPUT,
                         SEC_PERCEPT_INPUT_MODULE_NAME) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_perception_input_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_perception_input_default_config(&bridge->config);
    }

    /* Initialize state */
    bridge->state.operational_state = SEC_INPUT_STATE_READY;
    bridge->state.last_audio_result = SEC_INPUT_VALID;
    bridge->state.last_visual_result = SEC_INPUT_VALID;

    /* Initialize effects */
    bridge->sec_to_percept.audio_gate_action = SEC_INPUT_GATE_PASS;
    bridge->sec_to_percept.visual_gate_action = SEC_INPUT_GATE_PASS;
    bridge->sec_to_percept.audio_attenuation = 1.0f;
    bridge->sec_to_percept.visual_attenuation = 1.0f;

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async) {
        bridge_base_connect_bio_async(&bridge->base);
    }

    return bridge;
}

void security_perception_input_bridge_destroy(
    security_perception_input_bridge_t* bridge
) {
    if (!bridge) return;

    /* Cleanup base bridge (handles bio-async disconnection) */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge structure */
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int security_perception_input_connect_cochlea(
    security_perception_input_bridge_t* bridge,
    cochlea_t* cochlea
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(cochlea, NIMCP_ERROR_NULL_POINTER, "cochlea is NULL");

    BRIDGE_LOCK(bridge);

    bridge->cochlea = cochlea;
    bridge->state.cochlea_connected = true;
    update_bridge_active_state(bridge);

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-Perception Input Bridge: Cochlea connected");
    }

    return 0;
}

int security_perception_input_connect_visual_cortex(
    security_perception_input_bridge_t* bridge,
    visual_cortex_t* visual_cortex
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(visual_cortex, NIMCP_ERROR_NULL_POINTER, "visual_cortex is NULL");

    BRIDGE_LOCK(bridge);

    bridge->visual_cortex = visual_cortex;
    bridge->state.visual_cortex_connected = true;
    update_bridge_active_state(bridge);

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-Perception Input Bridge: Visual cortex connected");
    }

    return 0;
}

int security_perception_input_connect_bbb(
    security_perception_input_bridge_t* bridge,
    bbb_system_t bbb
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bbb, NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

    BRIDGE_LOCK(bridge);

    bridge->bbb = bbb;
    bridge->state.bbb_connected = true;

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-Perception Input Bridge: BBB connected");
    }

    return 0;
}

int security_perception_input_connect_anomaly_detector(
    security_perception_input_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(detector, NIMCP_ERROR_NULL_POINTER, "detector is NULL");

    BRIDGE_LOCK(bridge);

    bridge->anomaly_detector = detector;
    bridge->state.anomaly_detector_connected = true;

    BRIDGE_UNLOCK(bridge);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Security-Perception Input Bridge: Anomaly detector connected");
    }

    return 0;
}

/* ============================================================================
 * Audio Validation API
 * ============================================================================ */

int security_perception_validate_audio_input(
    security_perception_input_bridge_t* bridge,
    const float* samples,
    size_t num_samples,
    uint32_t sample_rate,
    sec_input_validation_result_t* result
) {
    NIMCP_CHECK_THROW(bridge && samples && result, NIMCP_ERROR_NULL_POINTER, "bridge, samples, or result is NULL");
    if (num_samples == 0 || num_samples > SEC_PERCEPT_INPUT_MAX_AUDIO_SAMPLES) {
        *result = SEC_INPUT_MALFORMED;
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAMETER, "invalid num_samples");
    }

    BRIDGE_LOCK(bridge);

    uint64_t start_time = nimcp_time_monotonic_us();
    *result = SEC_INPUT_VALID;
    bridge->state.operational_state = SEC_INPUT_STATE_PROCESSING;

    /* Range validation */
    bool has_invalid = false;
    bool has_out_of_range = false;

    for (size_t i = 0; i < num_samples; i++) {
        if (!is_valid_float(samples[i])) {
            has_invalid = true;
            break;
        }
        if (samples[i] < bridge->config.audio_min_value ||
            samples[i] > bridge->config.audio_max_value) {
            has_out_of_range = true;
        }
    }

    if (has_invalid) {
        *result = SEC_INPUT_MALFORMED;
        bridge->stats.audio_validations_failed++;
    } else if (has_out_of_range) {
        *result = SEC_INPUT_RANGE_WARNING;
    }

    /* Statistical validation */
    float mean, variance, peak, clipping;
    compute_audio_stats(samples, num_samples, &mean, &variance, &peak, &clipping);

    bridge->percept_to_sec.audio_mean = mean;
    bridge->percept_to_sec.audio_variance = variance;
    bridge->percept_to_sec.audio_peak_amplitude = peak;
    bridge->percept_to_sec.audio_dc_offset = mean;
    bridge->percept_to_sec.audio_sample_count = (uint32_t)num_samples;
    bridge->percept_to_sec.audio_clipping_ratio = clipping;

    /* Check for statistical anomalies */
    if (clipping > 0.1f || fabsf(mean) > 0.5f) {
        if (*result == SEC_INPUT_VALID) {
            *result = SEC_INPUT_STATS_WARNING;
        }
    }

    /* Ultrasonic detection */
    if (bridge->config.detect_ultrasonic &&
        detect_ultrasonic_content(samples, num_samples, sample_rate)) {
        *result = SEC_INPUT_ANOMALY_DETECTED;
        bridge->stats.audio_anomalies_detected++;
    }

    /* Update statistics */
    bridge->stats.audio_validations_total++;
    if (*result == SEC_INPUT_VALID || *result == SEC_INPUT_RANGE_WARNING) {
        bridge->stats.audio_validations_passed++;
    } else {
        bridge->stats.audio_validations_failed++;
    }

    /* Update state */
    bridge->state.last_audio_result = *result;
    bridge->state.last_validation_time_us = nimcp_time_get_us();
    bridge->state.operational_state = SEC_INPUT_STATE_READY;

    /* Update timing stats */
    uint64_t elapsed = nimcp_time_monotonic_us() - start_time;
    float elapsed_f = (float)elapsed;
    bridge->stats.avg_audio_validation_time_us =
        (bridge->stats.avg_audio_validation_time_us *
         (bridge->stats.audio_validations_total - 1) + elapsed_f) /
        bridge->stats.audio_validations_total;
    if (elapsed_f > bridge->stats.max_validation_time_us) {
        bridge->stats.max_validation_time_us = elapsed_f;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_perception_detect_audio_anomaly(
    security_perception_input_bridge_t* bridge,
    const float* samples,
    size_t num_samples,
    float* anomaly_score,
    float* confidence
) {
    NIMCP_CHECK_THROW(bridge && samples && anomaly_score && confidence, NIMCP_ERROR_NULL_POINTER, "bridge, samples, anomaly_score, or confidence is NULL");
    NIMCP_CHECK_THROW(num_samples > 0, NIMCP_ERROR_INVALID_PARAMETER, "num_samples is zero");

    *anomaly_score = 0.0f;
    *confidence = 0.5f;

    BRIDGE_LOCK(bridge);

    /* If anomaly detector connected, use it */
    if (bridge->state.anomaly_detector_connected && bridge->anomaly_detector) {
        nimcp_anomaly_result_t result;
        nimcp_error_t err = nimcp_anomaly_detect(
            bridge->anomaly_detector, samples,
            num_samples * sizeof(float), &result);

        if (err == NIMCP_SUCCESS) {
            *anomaly_score = result.anomaly_score;
            *confidence = result.confidence;
            bridge->percept_to_sec.audio_anomaly_score = result.anomaly_score;

            if (result.anomaly_score >= bridge->config.audio_anomaly_threshold) {
                bridge->stats.audio_anomalies_detected++;
                bridge->percept_to_sec.anomaly_flag_raised = true;
                bridge->percept_to_sec.anomaly_timestamp_us = nimcp_time_get_us();
            }
        }
    } else {
        /* Simple statistical anomaly detection */
        float mean, variance, peak, clipping;
        compute_audio_stats(samples, num_samples, &mean, &variance, &peak, &clipping);

        /* Score based on statistical measures */
        float dc_score = fabsf(mean) * 2.0f;
        float clip_score = clipping * 5.0f;
        float peak_score = (peak > 0.99f) ? 0.5f : 0.0f;

        *anomaly_score = nimcp_clampf(dc_score + clip_score + peak_score, 0.0f, 1.0f);
        *confidence = 0.6f;

        bridge->percept_to_sec.audio_anomaly_score = *anomaly_score;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Visual Validation API
 * ============================================================================ */

int security_perception_validate_visual_input(
    security_perception_input_bridge_t* bridge,
    const uint8_t* pixels,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    sec_input_validation_result_t* result
) {
    NIMCP_CHECK_THROW(bridge && pixels && result, NIMCP_ERROR_NULL_POINTER, "bridge, pixels, or result is NULL");
    if (width == 0 || height == 0 || channels == 0) {
        *result = SEC_INPUT_MALFORMED;
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAMETER, "width, height, or channels is zero");
    }
    if (width > SEC_PERCEPT_INPUT_MAX_IMAGE_WIDTH ||
        height > SEC_PERCEPT_INPUT_MAX_IMAGE_HEIGHT) {
        *result = SEC_INPUT_MALFORMED;
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAMETER, "image dimensions exceed maximum");
    }
    if (channels > 4) {
        *result = SEC_INPUT_MALFORMED;
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAMETER, "channels exceeds 4");
    }

    BRIDGE_LOCK(bridge);

    uint64_t start_time = nimcp_time_monotonic_us();
    *result = SEC_INPUT_VALID;
    bridge->state.operational_state = SEC_INPUT_STATE_PROCESSING;

    size_t num_pixels = (size_t)width * height * channels;

    /* Statistical validation */
    float mean_intensity, variance, saturation_ratio;
    compute_visual_stats(pixels, num_pixels, &mean_intensity, &variance,
                         &saturation_ratio);

    bridge->percept_to_sec.visual_mean_intensity = mean_intensity;
    bridge->percept_to_sec.visual_variance = variance;
    bridge->percept_to_sec.visual_pixel_count = (uint32_t)(width * height);
    bridge->percept_to_sec.visual_saturation_ratio = saturation_ratio;

    /* Compute contrast (simplified) */
    float contrast = sqrtf(variance) / 128.0f;
    bridge->percept_to_sec.visual_contrast = nimcp_clampf(contrast, 0.0f, 1.0f);

    /* Check for statistical anomalies */
    if (saturation_ratio > 0.5f) {
        *result = SEC_INPUT_STATS_WARNING;
    }

    /* Adversarial pattern detection */
    if (bridge->config.detect_adversarial_patches) {
        if (detect_adversarial_visual_pattern(pixels, width, height, channels)) {
            *result = SEC_INPUT_ADVERSARIAL_DETECTED;
            bridge->stats.adversarial_detected++;
        }
    }

    /* Update statistics */
    bridge->stats.visual_validations_total++;
    if (*result == SEC_INPUT_VALID || *result == SEC_INPUT_STATS_WARNING) {
        bridge->stats.visual_validations_passed++;
    } else {
        bridge->stats.visual_validations_failed++;
    }

    /* Update state */
    bridge->state.last_visual_result = *result;
    bridge->state.last_validation_time_us = nimcp_time_get_us();
    bridge->state.operational_state = SEC_INPUT_STATE_READY;

    /* Update timing stats */
    uint64_t elapsed = nimcp_time_monotonic_us() - start_time;
    float elapsed_f = (float)elapsed;
    bridge->stats.avg_visual_validation_time_us =
        (bridge->stats.avg_visual_validation_time_us *
         (bridge->stats.visual_validations_total - 1) + elapsed_f) /
        bridge->stats.visual_validations_total;
    if (elapsed_f > bridge->stats.max_validation_time_us) {
        bridge->stats.max_validation_time_us = elapsed_f;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_perception_detect_visual_anomaly(
    security_perception_input_bridge_t* bridge,
    const uint8_t* pixels,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    float* anomaly_score,
    float* confidence
) {
    NIMCP_CHECK_THROW(bridge && pixels && anomaly_score && confidence, NIMCP_ERROR_NULL_POINTER, "bridge, pixels, anomaly_score, or confidence is NULL");
    NIMCP_CHECK_THROW(width > 0 && height > 0 && channels > 0, NIMCP_ERROR_INVALID_PARAMETER, "width, height, or channels is zero");

    *anomaly_score = 0.0f;
    *confidence = 0.5f;

    BRIDGE_LOCK(bridge);

    size_t num_pixels = (size_t)width * height * channels;

    /* If anomaly detector connected, use it */
    if (bridge->state.anomaly_detector_connected && bridge->anomaly_detector) {
        nimcp_anomaly_result_t result;
        nimcp_error_t err = nimcp_anomaly_detect(
            bridge->anomaly_detector, pixels, num_pixels, &result);

        if (err == NIMCP_SUCCESS) {
            *anomaly_score = result.anomaly_score;
            *confidence = result.confidence;
            bridge->percept_to_sec.visual_anomaly_score = result.anomaly_score;

            if (result.anomaly_score >= bridge->config.visual_anomaly_threshold) {
                bridge->stats.visual_anomalies_detected++;
                bridge->percept_to_sec.anomaly_flag_raised = true;
                bridge->percept_to_sec.anomaly_timestamp_us = nimcp_time_get_us();
            }
        }
    } else {
        /* Simple statistical anomaly detection */
        float mean_intensity, variance, saturation_ratio;
        compute_visual_stats(pixels, num_pixels, &mean_intensity, &variance,
                             &saturation_ratio);

        /* Score based on statistical measures */
        float sat_score = saturation_ratio * 2.0f;
        float var_score = (variance < 100.0f) ? 0.3f : 0.0f; /* Low variance suspicious */

        /* Check for adversarial patterns */
        float pattern_score = 0.0f;
        if (detect_adversarial_visual_pattern(pixels, width, height, channels)) {
            pattern_score = 0.6f;
        }

        *anomaly_score = nimcp_clampf(sat_score + var_score + pattern_score, 0.0f, 1.0f);
        *confidence = 0.6f;

        bridge->percept_to_sec.visual_anomaly_score = *anomaly_score;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Input Gating API
 * ============================================================================ */

/* Internal unlocked version for use when lock is already held */
static void security_perception_gate_input_unlocked(
    security_perception_input_bridge_t* bridge,
    float threat_score,
    sec_input_gate_action_t* action,
    float* attenuation
) {
    threat_score = nimcp_clampf(threat_score, 0.0f, 1.0f);

    /* Determine gating action based on threat score */
    if (threat_score < 0.3f) {
        *action = SEC_INPUT_GATE_PASS;
        *attenuation = 1.0f;
    } else if (threat_score < 0.5f) {
        *action = SEC_INPUT_GATE_ATTENUATE;
        *attenuation = 1.0f - (threat_score - 0.3f);
    } else if (threat_score < 0.7f) {
        *action = SEC_INPUT_GATE_SANITIZE;
        *attenuation = 0.5f * (1.0f - threat_score);
    } else if (threat_score < bridge->config.gate_threshold) {
        *action = SEC_INPUT_GATE_HOLD;
        *attenuation = 0.2f;
    } else {
        *action = SEC_INPUT_GATE_BLOCK;
        *attenuation = 0.0f;
        bridge->stats.inputs_blocked++;
    }

    /* Update stats based on action */
    if (*action == SEC_INPUT_GATE_ATTENUATE) {
        bridge->stats.inputs_attenuated++;
    } else if (*action == SEC_INPUT_GATE_SANITIZE) {
        bridge->stats.inputs_sanitized++;
    }
}

int security_perception_gate_input(
    security_perception_input_bridge_t* bridge,
    float threat_score,
    sec_input_gate_action_t* action,
    float* attenuation
) {
    NIMCP_CHECK_THROW(bridge && action && attenuation, NIMCP_ERROR_NULL_POINTER, "bridge, action, or attenuation is NULL");

    BRIDGE_LOCK(bridge);
    security_perception_gate_input_unlocked(bridge, threat_score, action, attenuation);
    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_perception_apply_audio_gating(
    security_perception_input_bridge_t* bridge,
    float* samples,
    size_t num_samples
) {
    NIMCP_CHECK_THROW(bridge && samples, NIMCP_ERROR_NULL_POINTER, "bridge or samples is NULL");
    if (num_samples == 0) return 0;

    BRIDGE_LOCK(bridge);

    float attenuation = bridge->sec_to_percept.audio_attenuation;
    sec_input_gate_action_t action = bridge->sec_to_percept.audio_gate_action;

    if (action == SEC_INPUT_GATE_BLOCK) {
        /* Zero out all samples */
        memset(samples, 0, num_samples * sizeof(float));
    } else if (action != SEC_INPUT_GATE_PASS && attenuation < 1.0f) {
        /* Apply attenuation */
        for (size_t i = 0; i < num_samples; i++) {
            samples[i] *= attenuation;
        }
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_perception_apply_visual_gating(
    security_perception_input_bridge_t* bridge,
    uint8_t* pixels,
    uint32_t width,
    uint32_t height,
    uint32_t channels
) {
    NIMCP_CHECK_THROW(bridge && pixels, NIMCP_ERROR_NULL_POINTER, "bridge or pixels is NULL");
    if (width == 0 || height == 0 || channels == 0) return 0;

    BRIDGE_LOCK(bridge);

    float attenuation = bridge->sec_to_percept.visual_attenuation;
    sec_input_gate_action_t action = bridge->sec_to_percept.visual_gate_action;
    size_t num_pixels = (size_t)width * height * channels;

    if (action == SEC_INPUT_GATE_BLOCK) {
        /* Zero out all pixels */
        memset(pixels, 0, num_pixels);
    } else if (action != SEC_INPUT_GATE_PASS && attenuation < 1.0f) {
        /* Apply attenuation */
        for (size_t i = 0; i < num_pixels; i++) {
            pixels[i] = (uint8_t)((float)pixels[i] * attenuation);
        }
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int security_perception_input_update_sec_to_percept(
    security_perception_input_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Compute combined threat level from individual scores */
    float audio_threat = bridge->percept_to_sec.audio_anomaly_score;
    float visual_threat = bridge->percept_to_sec.visual_anomaly_score;

    bridge->sec_to_percept.audio_threat_level = audio_threat;
    bridge->sec_to_percept.visual_threat_level = visual_threat;
    bridge->sec_to_percept.combined_threat_level =
        (audio_threat + visual_threat) / 2.0f;

    /* Determine gating actions - use unlocked version since we hold the lock */
    sec_input_gate_action_t audio_action;
    float audio_atten;
    security_perception_gate_input_unlocked(bridge, audio_threat, &audio_action, &audio_atten);
    bridge->sec_to_percept.audio_gate_action = audio_action;
    bridge->sec_to_percept.audio_attenuation = audio_atten;

    sec_input_gate_action_t visual_action;
    float visual_atten;
    security_perception_gate_input_unlocked(bridge, visual_threat, &visual_action, &visual_atten);
    bridge->sec_to_percept.visual_gate_action = visual_action;
    bridge->sec_to_percept.visual_attenuation = visual_atten;

    /* Set processing guidance flags */
    bridge->sec_to_percept.require_enhanced_validation =
        (bridge->sec_to_percept.combined_threat_level > 0.5f);
    bridge->sec_to_percept.require_cross_modal_check =
        (audio_threat > 0.3f && visual_threat > 0.3f);

    /* Update threat active flags */
    bridge->state.audio_threat_active = (audio_threat > 0.3f);
    bridge->state.visual_threat_active = (visual_threat > 0.3f);
    bridge->state.current_audio_threat = audio_threat;
    bridge->state.current_visual_threat = visual_threat;

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_perception_input_update_percept_to_sec(
    security_perception_input_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Update timestamp */
    bridge->percept_to_sec.anomaly_timestamp_us = nimcp_time_get_us();

    /* Check if anomaly flag should be raised */
    bool audio_anomaly =
        bridge->percept_to_sec.audio_anomaly_score >= bridge->config.audio_anomaly_threshold;
    bool visual_anomaly =
        bridge->percept_to_sec.visual_anomaly_score >= bridge->config.visual_anomaly_threshold;

    bridge->percept_to_sec.anomaly_flag_raised = audio_anomaly || visual_anomaly;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_perception_input_update(
    security_perception_input_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    int ret = security_perception_input_update_percept_to_sec(bridge);
    if (ret != 0) return ret;

    return security_perception_input_update_sec_to_percept(bridge);
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int security_perception_input_get_sec_to_percept_effects(
    const security_perception_input_bridge_t* bridge,
    sec_to_percept_input_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");

    /* Note: cast away const for mutex lock, safe as we only read */
    security_perception_input_bridge_t* mutable_bridge =
        (security_perception_input_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *effects = bridge->sec_to_percept;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_perception_input_get_percept_to_sec_effects(
    const security_perception_input_bridge_t* bridge,
    percept_to_sec_input_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");

    security_perception_input_bridge_t* mutable_bridge =
        (security_perception_input_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *effects = bridge->percept_to_sec;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_perception_input_get_state(
    const security_perception_input_bridge_t* bridge,
    sec_percept_input_state_t* state
) {
    NIMCP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    security_perception_input_bridge_t* mutable_bridge =
        (security_perception_input_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *state = bridge->state;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_perception_input_get_stats(
    const security_perception_input_bridge_t* bridge,
    sec_percept_input_stats_t* stats
) {
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    security_perception_input_bridge_t* mutable_bridge =
        (security_perception_input_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *stats = bridge->stats;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

void security_perception_input_reset_stats(
    security_perception_input_bridge_t* bridge
) {
    if (!bridge) return;

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(sec_percept_input_stats_t));
    BRIDGE_UNLOCK(bridge);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* security_perception_input_result_name(
    sec_input_validation_result_t result
) {
    if (result < 0 || result > SEC_INPUT_REJECTED) {
        return "UNKNOWN";
    }
    return s_validation_result_names[result];
}

const char* security_perception_input_gate_action_name(
    sec_input_gate_action_t action
) {
    if (action < 0 || action > SEC_INPUT_GATE_BLOCK) {
        return "UNKNOWN";
    }
    return s_gate_action_names[action];
}

const char* security_perception_input_state_name(
    sec_input_state_t state
) {
    if (state < 0 || state > SEC_INPUT_STATE_ERROR) {
        return "UNKNOWN";
    }
    return s_state_names[state];
}

int security_perception_input_report_false_positive(
    security_perception_input_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->stats.false_positives_reported++;

    /* Update estimated precision */
    uint64_t total_positives = bridge->stats.audio_anomalies_detected +
                               bridge->stats.visual_anomalies_detected +
                               bridge->stats.adversarial_detected +
                               bridge->stats.spoofing_detected;

    if (total_positives > 0) {
        uint64_t true_positives = 0;
        if (total_positives > bridge->stats.false_positives_reported) {
            true_positives = total_positives - bridge->stats.false_positives_reported;
        }
        bridge->stats.estimated_precision =
            (float)true_positives / (float)total_positives;
    }

    /* Notify anomaly detector of false positive for threshold adjustment */
    if (bridge->state.anomaly_detector_connected && bridge->anomaly_detector) {
        nimcp_anomaly_update_thresholds(bridge->anomaly_detector, true, false);
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}
