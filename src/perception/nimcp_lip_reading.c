/**
 * @file nimcp_lip_reading.c
 * @brief Lip Reading System - Visual Speech Perception Implementation
 *
 * WHAT: Biologically-inspired lip reading that maps visual mouth movements
 *       to phonemes and speech comprehension
 * WHY:  Enable speech understanding in noisy environments, deaf individuals,
 *       multimodal robustness via McGurk effect
 * HOW:  Visual cortex (mouth ROI) -> STS (visual speech) -> Speech cortex
 *       (phoneme integration) -> Mirror neurons (motor theory)
 *
 * @version Phase B5: Audiovisual Speech Integration
 * @date 2026-01-15
 */

#include "perception/nimcp_lip_reading.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lip_reading)

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Internal face detector state
 */
struct face_detector {
    face_detection_result_t last_result;
    kalman_filter_t* mouth_tracker;
    uint32_t frames_since_detection;
    float smoothed_mouth_bbox[4];
    bool tracking_active;
};

/**
 * @brief Internal viseme classifier state
 */
struct viseme_classifier {
    /* Viseme transition probabilities */
    float transition_matrix[VISEME_COUNT][VISEME_COUNT];

    /* History buffer */
    viseme_t viseme_history[LIP_READING_MAX_VISEME_HISTORY];
    uint32_t history_count;

    /* Classification thresholds (per-speaker adjustable) */
    float classification_thresholds[VISEME_COUNT];

    /* Feature weights */
    float feature_weights[LIP_READING_FEATURE_DIM];
};

/**
 * @brief Internal temporal integrator state
 */
struct temporal_integrator {
    visual_speech_features_t previous_features;
    bool has_previous;

    /* Velocity/acceleration tracking */
    float lip_velocity[2];
    float lip_acceleration[2];

    /* Plosive/closure detection */
    bool plosive_burst_detected;
    bool closure_detected;
    uint64_t last_burst_time_ms;
    uint64_t last_closure_time_ms;
};

/**
 * @brief Internal audiovisual integrator state
 */
struct audiovisual_integrator {
    /* Visual buffer for temporal alignment */
    viseme_t visual_buffer[32];
    float visual_confidences[32];
    uint64_t visual_timestamps[32];
    uint32_t visual_buffer_head;
    uint32_t visual_buffer_count;

    /* Last integration result */
    audiovisual_integration_t last_result;

    /* Calibration */
    float temporal_offset_calibrated_ms;
    uint32_t calibration_samples;
};

/**
 * @brief Speaker profile storage
 */
typedef struct {
    speaker_profile_t profile;
    bool active;
} speaker_slot_t;

/**
 * @brief Lip reading system internal state
 */
struct lip_reading_system {
    lip_reading_config_t config;
    lip_reading_status_t status;
    lip_reading_error_t last_error;
    lip_reading_stats_t stats;

    /* Core components */
    face_detector_t* face_detector;
    viseme_classifier_t* viseme_classifier;
    temporal_integrator_t* temporal_integrator;
    audiovisual_integrator_t* av_integrator;

    /* Speaker profiles */
    speaker_slot_t* speakers;
    uint32_t speaker_count;
    uint32_t active_speaker_id;
    uint32_t next_speaker_id;

    /* Integration */
    visual_cortex_t* visual_cortex;
    speech_cortex_t* speech_cortex;
    brain_t* brain;
    bio_router_t* router;
    bool bio_registered;

    /* Temporary buffers */
    uint8_t* mouth_roi_buffer;
    float* feature_buffer;

    /* Timing */
    uint64_t last_frame_time_ms;
    double last_processing_time_ms;
};

/*=============================================================================
 * VISEME/PHONEME MAPPING TABLES
 *===========================================================================*/

static const char* VISEME_NAMES[] = {
    "BILABIAL",
    "LABIODENTAL",
    "DENTAL",
    "ALVEOLAR",
    "VELAR",
    "ROUNDED_CLOSE",
    "ROUNDED_OPEN",
    "UNROUNDED_CLOSE",
    "UNROUNDED_MID",
    "UNROUNDED_OPEN",
    "SILENCE",
    "UNKNOWN"
};

static const char* PHONEME_NAMES[] = {
    "UNKNOWN",
    "P", "B", "M",
    "F", "V",
    "TH", "DH",
    "T", "D", "N", "L", "S", "Z",
    "K", "G", "NG",
    "SH", "ZH", "CH", "J",
    "H",
    "R", "W", "Y",
    "IY", "IH", "EY", "EH", "AE", "AA", "AO", "OW", "UH", "UW", "AH", "ER"
};

/* Viseme transition probability matrix (row=from, col=to) */
static const float DEFAULT_TRANSITION_MATRIX[VISEME_COUNT][VISEME_COUNT] = {
    /* From BILABIAL */
    {0.3f, 0.1f, 0.05f, 0.1f, 0.1f, 0.1f, 0.05f, 0.1f, 0.1f, 0.05f, 0.05f, 0.0f},
    /* From LABIODENTAL */
    {0.1f, 0.3f, 0.05f, 0.1f, 0.1f, 0.1f, 0.05f, 0.1f, 0.1f, 0.05f, 0.05f, 0.0f},
    /* From DENTAL */
    {0.05f, 0.05f, 0.3f, 0.15f, 0.1f, 0.05f, 0.05f, 0.1f, 0.1f, 0.05f, 0.0f, 0.0f},
    /* From ALVEOLAR */
    {0.1f, 0.1f, 0.1f, 0.3f, 0.1f, 0.05f, 0.05f, 0.1f, 0.1f, 0.05f, 0.05f, 0.0f},
    /* From VELAR */
    {0.1f, 0.05f, 0.05f, 0.1f, 0.3f, 0.1f, 0.1f, 0.05f, 0.1f, 0.05f, 0.0f, 0.0f},
    /* From ROUNDED_CLOSE */
    {0.1f, 0.1f, 0.05f, 0.1f, 0.1f, 0.3f, 0.15f, 0.05f, 0.05f, 0.0f, 0.0f, 0.0f},
    /* From ROUNDED_OPEN */
    {0.05f, 0.05f, 0.05f, 0.1f, 0.1f, 0.15f, 0.3f, 0.05f, 0.1f, 0.05f, 0.0f, 0.0f},
    /* From UNROUNDED_CLOSE */
    {0.1f, 0.1f, 0.1f, 0.1f, 0.05f, 0.05f, 0.05f, 0.3f, 0.1f, 0.05f, 0.0f, 0.0f},
    /* From UNROUNDED_MID */
    {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.05f, 0.1f, 0.1f, 0.2f, 0.05f, 0.0f, 0.0f},
    /* From UNROUNDED_OPEN */
    {0.05f, 0.05f, 0.05f, 0.1f, 0.1f, 0.05f, 0.1f, 0.05f, 0.1f, 0.3f, 0.05f, 0.0f},
    /* From SILENCE */
    {0.15f, 0.1f, 0.05f, 0.15f, 0.1f, 0.1f, 0.05f, 0.1f, 0.1f, 0.1f, 0.0f, 0.0f},
    /* From UNKNOWN */
    {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.0f, 0.0f}
};

/* Phoneme voicing table */
static const bool PHONEME_VOICED[] = {
    false,  /* UNKNOWN */
    false, true, true,    /* P, B, M */
    false, true,          /* F, V */
    false, true,          /* TH, DH */
    false, true, true, true, false, true,  /* T, D, N, L, S, Z */
    false, true, true,    /* K, G, NG */
    false, true, false, true,  /* SH, ZH, CH, J */
    false,                /* H */
    true, true, true,     /* R, W, Y */
    true, true, true, true, true, true, true, true, true, true, true, true  /* vowels */
};

/* Phoneme nasal table */
static const bool PHONEME_NASAL[] = {
    false,  /* UNKNOWN */
    false, false, true,   /* P, B, M */
    false, false,         /* F, V */
    false, false,         /* TH, DH */
    false, false, true, false, false, false,  /* T, D, N, L, S, Z */
    false, false, true,   /* K, G, NG */
    false, false, false, false,  /* SH, ZH, CH, J */
    false,                /* H */
    false, false, false,  /* R, W, Y */
    false, false, false, false, false, false, false, false, false, false, false, false  /* vowels */
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static double get_timestamp_ms_precise(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int argmax_float(const float* arr, int len) {
    int max_idx = 0;
    float max_val = arr[0];
    for (int i = 1; i < len; i++) {
        if (arr[i] > max_val) {
            max_val = arr[i];
            max_idx = i;
        }
    }
    return max_idx;
}

static void softmax(float* arr, int len) {
    float max_val = arr[0];
    for (int i = 1; i < len; i++) {
        if (arr[i] > max_val) max_val = arr[i];
    }

    float sum = 0.0f;
    for (int i = 0; i < len; i++) {
        arr[i] = expf(arr[i] - max_val);
        sum += arr[i];
    }

    if (sum > 0.0f) {
        for (int i = 0; i < len; i++) {
            arr[i] /= sum;
        }
    }
}

/*=============================================================================
 * COMPONENT CREATION
 *===========================================================================*/

static face_detector_t* face_detector_create(void) {
    face_detector_t* detector = (face_detector_t*)nimcp_calloc(1, sizeof(face_detector_t));
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate face_detector_t");
        return NULL;
    }

    detector->tracking_active = false;
    detector->frames_since_detection = 0;

    return detector;
}

static void face_detector_destroy(face_detector_t* detector) {
    if (!detector) return;
    nimcp_free(detector);
}

static viseme_classifier_t* viseme_classifier_create(void) {
    viseme_classifier_t* classifier = (viseme_classifier_t*)nimcp_calloc(1, sizeof(viseme_classifier_t));
    if (!classifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate viseme_classifier_t");
        return NULL;
    }

    /* Initialize transition matrix */
    memcpy(classifier->transition_matrix, DEFAULT_TRANSITION_MATRIX,
           sizeof(DEFAULT_TRANSITION_MATRIX));

    /* Initialize default thresholds */
    for (int i = 0; i < VISEME_COUNT; i++) {
        classifier->classification_thresholds[i] = 0.5f;
    }

    /* Initialize default feature weights */
    for (int i = 0; i < LIP_READING_FEATURE_DIM; i++) {
        classifier->feature_weights[i] = 1.0f / LIP_READING_FEATURE_DIM;
    }

    return classifier;
}

static void viseme_classifier_destroy(viseme_classifier_t* classifier) {
    if (!classifier) return;
    nimcp_free(classifier);
}

static temporal_integrator_t* temporal_integrator_create(void) {
    temporal_integrator_t* integrator = (temporal_integrator_t*)nimcp_calloc(1, sizeof(temporal_integrator_t));
    if (!integrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate temporal_integrator_t");
        return NULL;
    }

    integrator->has_previous = false;
    return integrator;
}

static void temporal_integrator_destroy(temporal_integrator_t* integrator) {
    if (!integrator) return;
    nimcp_free(integrator);
}

static audiovisual_integrator_t* audiovisual_integrator_create(void) {
    audiovisual_integrator_t* integrator = (audiovisual_integrator_t*)nimcp_calloc(1, sizeof(audiovisual_integrator_t));
    if (!integrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate audiovisual_integrator_t");
        return NULL;
    }

    integrator->temporal_offset_calibrated_ms = 200.0f;  /* Default visual lead */
    return integrator;
}

static void audiovisual_integrator_destroy(audiovisual_integrator_t* integrator) {
    if (!integrator) return;
    nimcp_free(integrator);
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

lip_reading_config_t lip_reading_default_config(void) {
    lip_reading_config_t config = {0};

    /* Processing settings */
    config.mouth_roi_width = LIP_READING_DEFAULT_MOUTH_ROI_SIZE;
    config.mouth_roi_height = LIP_READING_DEFAULT_MOUTH_ROI_SIZE;
    config.min_face_confidence = 0.6f;
    config.min_viseme_confidence = 0.4f;
    config.target_frame_rate = LIP_READING_DEFAULT_FRAME_RATE;

    /* Audiovisual integration */
    config.enable_audiovisual_fusion = true;
    config.visual_lead_ms = LIP_READING_VISUAL_LEAD_MS;
    config.snr_visual_threshold = 0.0f;  /* Below 0dB, rely more on visual */

    /* Temporal processing */
    config.enable_temporal_smoothing = true;
    config.viseme_history_length = LIP_READING_MAX_VISEME_HISTORY;
    config.transition_threshold = 0.1f;

    /* Speaker adaptation */
    config.enable_speaker_adaptation = true;
    config.adaptation_frames = LIP_READING_DEFAULT_ADAPTATION_FRAMES;
    config.max_speakers = LIP_READING_MAX_SPEAKERS;

    /* Learning */
    config.enable_stdp_learning = false;
    config.enable_meta_learning = false;

    /* Attention */
    config.enable_attention_modulation = true;
    config.speech_salience_threshold = 0.6f;

    /* Integration */
    config.enable_bio_async = true;
    config.enable_mirror_neurons = true;

    /* Debug */
    config.debug_mode = false;
    config.save_mouth_roi = false;

    return config;
}

lip_reading_system_t* lip_reading_create(const lip_reading_config_t* config) {
    lip_reading_system_t* system = (lip_reading_system_t*)nimcp_calloc(1, sizeof(lip_reading_system_t));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate lip_reading_system_t");
        return NULL;
    }

    /* Copy or use default config */
    if (config) {
        system->config = *config;
    } else {
        system->config = lip_reading_default_config();
    }

    /* Create core components */
    system->face_detector = face_detector_create();
    if (!system->face_detector) goto error;

    system->viseme_classifier = viseme_classifier_create();
    if (!system->viseme_classifier) goto error;

    system->temporal_integrator = temporal_integrator_create();
    if (!system->temporal_integrator) goto error;

    system->av_integrator = audiovisual_integrator_create();
    if (!system->av_integrator) goto error;

    /* Allocate speaker slots */
    system->speakers = (speaker_slot_t*)nimcp_calloc(
        system->config.max_speakers, sizeof(speaker_slot_t));
    if (!system->speakers) goto error;

    system->next_speaker_id = 1;
    system->active_speaker_id = 0;

    /* Allocate temporary buffers */
    size_t roi_size = system->config.mouth_roi_width * system->config.mouth_roi_height * 3;
    system->mouth_roi_buffer = (uint8_t*)nimcp_calloc(roi_size, sizeof(uint8_t));
    if (!system->mouth_roi_buffer) goto error;

    system->feature_buffer = (float*)nimcp_calloc(LIP_READING_FEATURE_DIM, sizeof(float));
    if (!system->feature_buffer) goto error;

    system->status = LIP_READING_STATUS_IDLE;
    system->last_error = LIP_READING_ERROR_NONE;

    return system;

error:
    lip_reading_destroy(system);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lip_reading_create: buffer allocation failed");
    return NULL;
}

void lip_reading_destroy(lip_reading_system_t* system) {
    if (!system) return;

    face_detector_destroy(system->face_detector);
    viseme_classifier_destroy(system->viseme_classifier);
    temporal_integrator_destroy(system->temporal_integrator);
    audiovisual_integrator_destroy(system->av_integrator);

    if (system->speakers) nimcp_free(system->speakers);
    if (system->mouth_roi_buffer) nimcp_free(system->mouth_roi_buffer);
    if (system->feature_buffer) nimcp_free(system->feature_buffer);

    nimcp_free(system);
}

bool lip_reading_reset(lip_reading_system_t* system) {
    if (!system) {
        return false;
    }

    /* Reset face detector */
    if (system->face_detector) {
        memset(&system->face_detector->last_result, 0, sizeof(face_detection_result_t));
        system->face_detector->tracking_active = false;
        system->face_detector->frames_since_detection = 0;
    }

    /* Reset viseme classifier */
    if (system->viseme_classifier) {
        memset(system->viseme_classifier->viseme_history, 0,
               sizeof(system->viseme_classifier->viseme_history));
        system->viseme_classifier->history_count = 0;
    }

    /* Reset temporal integrator */
    if (system->temporal_integrator) {
        system->temporal_integrator->has_previous = false;
        system->temporal_integrator->plosive_burst_detected = false;
        system->temporal_integrator->closure_detected = false;
    }

    /* Reset audiovisual integrator */
    if (system->av_integrator) {
        system->av_integrator->visual_buffer_count = 0;
        system->av_integrator->visual_buffer_head = 0;
    }

    /* Reset statistics */
    memset(&system->stats, 0, sizeof(lip_reading_stats_t));

    system->status = LIP_READING_STATUS_IDLE;
    system->last_error = LIP_READING_ERROR_NONE;

    return true;
}

/*=============================================================================
 * FACE/MOUTH DETECTION
 *===========================================================================*/

bool lip_reading_detect_face(
    lip_reading_system_t* system,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    face_detection_result_t* result)
{
    if (!system || !image || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in lip_reading_detect_face");
        if (system) system->last_error = LIP_READING_ERROR_INVALID_INPUT;
        return false;
    }

    /* Initialize result */
    memset(result, 0, sizeof(face_detection_result_t));

    /*
     * Face detection algorithm (simplified):
     * In a real implementation, this would use a pre-trained face detector
     * such as Haar cascades, HOG+SVM, or a CNN-based detector.
     *
     * For this implementation, we use a simple skin-color based approach
     * combined with heuristics for face location.
     */

    /* Calculate image statistics for face region estimation */
    float sum_x = 0, sum_y = 0;
    uint32_t skin_count = 0;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t idx = (y * width + x) * channels;

            /* Simple skin color detection (for RGB images) */
            bool is_skin = false;
            if (channels >= 3) {
                /* Bounds check: ensure we don't read beyond image buffer */
                size_t max_idx = (size_t)width * height * channels;
                if (idx + 2 >= max_idx) continue;  /* Skip if would overflow */

                uint8_t r = image[idx];
                uint8_t g = image[idx + 1];
                uint8_t b = image[idx + 2];

                /* Basic skin color heuristic */
                if (r > 95 && g > 40 && b > 20 &&
                    r > g && r > b &&
                    (r - (g < b ? g : b)) > 15 &&
                    abs((int)r - (int)g) > 15) {
                    is_skin = true;
                }
            } else {
                /* Grayscale: use intensity heuristic */
                /* Bounds check for grayscale */
                size_t max_idx = (size_t)width * height * channels;
                if (idx < max_idx && image[idx] > 100 && image[idx] < 220) {
                    is_skin = true;
                }
            }

            if (is_skin) {
                sum_x += x;
                sum_y += y;
                skin_count++;
            }
        }
    }

    if (skin_count < (width * height) / 50) {
        /* Not enough skin-colored pixels - likely no face */
        system->last_error = LIP_READING_ERROR_NO_FACE_DETECTED;
        system->stats.face_detection_failures++;
        return false;  /* Not enough skin pixels - normal detection failure */
    }

    /* Estimate face center from skin pixels */
    float face_center_x = sum_x / skin_count;
    float face_center_y = sum_y / skin_count;

    /* Estimate face size based on image dimensions */
    float face_width = width * 0.5f;
    float face_height = height * 0.6f;

    /* Set face bounding box */
    result->face_bbox[0] = face_center_x - face_width / 2;
    result->face_bbox[1] = face_center_y - face_height / 2;
    result->face_bbox[2] = face_width;
    result->face_bbox[3] = face_height;

    /* Clamp to image bounds */
    if (result->face_bbox[0] < 0) result->face_bbox[0] = 0;
    if (result->face_bbox[1] < 0) result->face_bbox[1] = 0;
    if (result->face_bbox[0] + result->face_bbox[2] > width) {
        result->face_bbox[2] = width - result->face_bbox[0];
    }
    if (result->face_bbox[1] + result->face_bbox[3] > height) {
        result->face_bbox[3] = height - result->face_bbox[1];
    }

    /* Estimate mouth location (lower third of face) */
    result->mouth_bbox[0] = result->face_bbox[0] + result->face_bbox[2] * 0.2f;
    result->mouth_bbox[1] = result->face_bbox[1] + result->face_bbox[3] * 0.65f;
    result->mouth_bbox[2] = result->face_bbox[2] * 0.6f;
    result->mouth_bbox[3] = result->face_bbox[3] * 0.25f;

    result->mouth_center[0] = result->mouth_bbox[0] + result->mouth_bbox[2] / 2;
    result->mouth_center[1] = result->mouth_bbox[1] + result->mouth_bbox[3] / 2;

    result->face_detected = true;
    result->face_confidence = 0.7f;  /* Moderate confidence for heuristic */
    result->track_id = 1;
    result->tracking_stable = true;

    /* Update detector state */
    system->face_detector->last_result = *result;
    system->face_detector->tracking_active = true;
    system->face_detector->frames_since_detection = 0;

    system->status = LIP_READING_STATUS_FACE_DETECTED;
    system->stats.faces_detected++;

    return true;
}

bool lip_reading_extract_mouth_roi(
    lip_reading_system_t* system,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    const face_detection_result_t* face_result,
    uint8_t* mouth_roi,
    uint32_t roi_width,
    uint32_t roi_height)
{
    if (!system || !image || !face_result || !mouth_roi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in lip_reading_extract_mouth_roi");
        if (system) system->last_error = LIP_READING_ERROR_INVALID_INPUT;
        return false;
    }

    if (!face_result->face_detected) {
        system->last_error = LIP_READING_ERROR_NO_FACE_DETECTED;
        return false;  /* No face detected - normal detection failure */
    }

    /* Calculate source region */
    int src_x = (int)face_result->mouth_bbox[0];
    int src_y = (int)face_result->mouth_bbox[1];
    int src_w = (int)face_result->mouth_bbox[2];
    int src_h = (int)face_result->mouth_bbox[3];

    /* Clamp source region */
    if (src_x < 0) src_x = 0;
    if (src_y < 0) src_y = 0;
    if (src_x + src_w > (int)width) src_w = width - src_x;
    if (src_y + src_h > (int)height) src_h = height - src_y;

    if (src_w <= 0 || src_h <= 0) {
        system->last_error = LIP_READING_ERROR_INVALID_INPUT;
        return false;  /* Invalid mouth region dimensions - normal detection failure */
    }

    /* Simple bilinear resize */
    float scale_x = (float)src_w / roi_width;
    float scale_y = (float)src_h / roi_height;

    /* Calculate buffer sizes for bounds checking */
    size_t src_buffer_size = (size_t)width * height * channels;
    size_t dst_buffer_size = (size_t)roi_width * roi_height * channels;

    for (uint32_t y = 0; y < roi_height; y++) {
        for (uint32_t x = 0; x < roi_width; x++) {
            float src_fx = src_x + x * scale_x;
            float src_fy = src_y + y * scale_y;

            int sx = (int)src_fx;
            int sy = (int)src_fy;

            /* Clamp */
            if (sx >= (int)width - 1) sx = width - 2;
            if (sy >= (int)height - 1) sy = height - 2;
            if (sx < 0) sx = 0;
            if (sy < 0) sy = 0;

            /* Simple nearest neighbor for now */
            for (uint32_t c = 0; c < channels && c < 3; c++) {
                size_t src_idx = (size_t)(sy * width + sx) * channels + c;
                size_t dst_idx = (size_t)(y * roi_width + x) * channels + c;

                /* Bounds check before access */
                if (src_idx >= src_buffer_size || dst_idx >= dst_buffer_size) {
                    continue;  /* Skip if out of bounds */
                }
                mouth_roi[dst_idx] = image[src_idx];
            }
        }
    }

    system->status = LIP_READING_STATUS_MOUTH_TRACKED;
    return true;
}

bool lip_reading_detect_lip_landmarks(
    lip_reading_system_t* system,
    const uint8_t* mouth_roi,
    uint32_t roi_width,
    uint32_t roi_height,
    face_detection_result_t* result)
{
    if (!system || !mouth_roi || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in lip_reading_detect_lip_landmarks");
        if (system) system->last_error = LIP_READING_ERROR_INVALID_INPUT;
        return false;
    }

    /*
     * Lip landmark detection (simplified):
     * In a real implementation, this would use a trained landmark detector.
     * Here we use edge detection and contour analysis.
     */

    float cx = roi_width / 2.0f;
    float cy = roi_height / 2.0f;

    /* Generate approximate lip contour points */
    for (uint32_t i = 0; i < LIP_READING_MAX_LIP_CONTOUR_POINTS; i++) {
        float angle = NIMCP_TWO_PI_F * i / LIP_READING_MAX_LIP_CONTOUR_POINTS;

        /* Elliptical approximation for outer lip */
        float rx = roi_width * 0.4f;
        float ry = roi_height * 0.35f;

        result->lip_outer_contour[i][0] = cx + rx * cosf(angle);
        result->lip_outer_contour[i][1] = cy + ry * sinf(angle);
    }
    result->outer_contour_count = LIP_READING_MAX_LIP_CONTOUR_POINTS;

    /* Generate inner lip contour */
    for (uint32_t i = 0; i < LIP_READING_INNER_CONTOUR_POINTS; i++) {
        float angle = NIMCP_TWO_PI_F * i / LIP_READING_INNER_CONTOUR_POINTS;

        /* Smaller ellipse for inner lip */
        float rx = roi_width * 0.25f;
        float ry = roi_height * 0.2f;

        result->lip_inner_contour[i][0] = cx + rx * cosf(angle);
        result->lip_inner_contour[i][1] = cy + ry * sinf(angle);
    }
    result->inner_contour_count = LIP_READING_INNER_CONTOUR_POINTS;

    return true;
}

/*=============================================================================
 * VISUAL SPEECH FEATURE EXTRACTION
 *===========================================================================*/

bool lip_reading_extract_features(
    lip_reading_system_t* system,
    const uint8_t* mouth_roi,
    uint32_t roi_width,
    uint32_t roi_height,
    const face_detection_result_t* face_result,
    visual_speech_features_t* features)
{
    if (!system || !mouth_roi || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in lip_reading_extract_features");
        if (system) system->last_error = LIP_READING_ERROR_INVALID_INPUT;
        return false;
    }

    memset(features, 0, sizeof(visual_speech_features_t));

    /* Calculate geometric features from lip contours */
    if (face_result && face_result->outer_contour_count > 0) {
        /* Find lip width (horizontal extent) */
        float min_x = face_result->lip_outer_contour[0][0];
        float max_x = min_x;
        float min_y = face_result->lip_outer_contour[0][1];
        float max_y = min_y;

        for (uint32_t i = 1; i < face_result->outer_contour_count; i++) {
            if (face_result->lip_outer_contour[i][0] < min_x) {
                min_x = face_result->lip_outer_contour[i][0];
            }
            if (face_result->lip_outer_contour[i][0] > max_x) {
                max_x = face_result->lip_outer_contour[i][0];
            }
            if (face_result->lip_outer_contour[i][1] < min_y) {
                min_y = face_result->lip_outer_contour[i][1];
            }
            if (face_result->lip_outer_contour[i][1] > max_y) {
                max_y = face_result->lip_outer_contour[i][1];
            }
        }

        features->lip_width = max_x - min_x;
        features->lip_height = max_y - min_y;
        features->lip_area = features->lip_width * features->lip_height * 0.785f;  /* Ellipse */
        features->lip_aspect_ratio = (features->lip_height > 0) ?
            features->lip_width / features->lip_height : 1.0f;
    } else {
        /* Estimate from ROI dimensions */
        features->lip_width = roi_width * 0.8f;
        features->lip_height = roi_height * 0.5f;
        features->lip_area = features->lip_width * features->lip_height * 0.785f;
        features->lip_aspect_ratio = features->lip_width / features->lip_height;
    }

    /* Estimate teeth visibility from brightness */
    float upper_brightness = 0;
    float lower_brightness = 0;
    uint32_t upper_count = 0;
    uint32_t lower_count = 0;

    /* Calculate buffer size for bounds checking */
    size_t roi_buffer_size = (size_t)roi_width * roi_height;

    uint32_t cy = roi_height / 2;
    /* Calculate safe Y bounds to avoid underflow */
    uint32_t upper_y_start = (cy > roi_height/6) ? (cy - roi_height/6) : 0;
    for (uint32_t y = upper_y_start; y < cy && y < roi_height; y++) {
        for (uint32_t x = roi_width/4; x < 3*roi_width/4 && x < roi_width; x++) {
            size_t idx = (size_t)y * roi_width + x;
            if (idx < roi_buffer_size) {
                upper_brightness += mouth_roi[idx];
                upper_count++;
            }
        }
    }
    for (uint32_t y = cy; y < cy + roi_height/6 && y < roi_height; y++) {
        for (uint32_t x = roi_width/4; x < 3*roi_width/4 && x < roi_width; x++) {
            size_t idx = (size_t)y * roi_width + x;
            if (idx < roi_buffer_size) {
                lower_brightness += mouth_roi[idx];
                lower_count++;
            }
        }
    }

    if (upper_count > 0) {
        upper_brightness /= upper_count;
        features->upper_teeth_visible = nimcp_clampf((upper_brightness - 100) / 155.0f, 0, 1);
    }
    if (lower_count > 0) {
        lower_brightness /= lower_count;
        features->lower_teeth_visible = nimcp_clampf((lower_brightness - 100) / 155.0f, 0, 1);
    }

    features->teeth_gap = features->lip_height * 0.3f *
        (features->upper_teeth_visible + features->lower_teeth_visible) / 2;

    /* Tongue visibility estimation */
    float tongue_region_brightness = 0;
    uint32_t tongue_count = 0;
    for (uint32_t y = cy; y < cy + roi_height/4 && y < roi_height; y++) {
        for (uint32_t x = roi_width/3; x < 2*roi_width/3 && x < roi_width; x++) {
            size_t idx = (size_t)y * roi_width + x;
            if (idx < roi_buffer_size) {
                tongue_region_brightness += mouth_roi[idx];
                tongue_count++;
            }
        }
    }
    if (tongue_count > 0) {
        tongue_region_brightness /= tongue_count;
        /* Tongue is typically darker (pink/red) than teeth (white) */
        features->tongue_visible = nimcp_clampf(
            (120 - tongue_region_brightness) / 80.0f, 0, 1);
    }

    /* Estimate lip protrusion from aspect ratio changes */
    features->lip_protrusion = nimcp_clampf(
        1.0f - features->lip_aspect_ratio / 2.0f, 0, 1);

    /* Calculate normalized luminance */
    float total_luminance = 0;
    for (size_t i = 0; i < roi_buffer_size; i++) {
        total_luminance += mouth_roi[i];
    }
    features->normalized_luminance = (roi_buffer_size > 0) ?
        total_luminance / (roi_buffer_size * 255.0f) : 0.0f;

    features->feature_confidence = 0.7f;
    features->timestamp_ms = get_timestamp_ms();

    return true;
}

bool lip_reading_update_dynamics(
    lip_reading_system_t* system,
    visual_speech_features_t* current_features,
    const visual_speech_features_t* previous_features,
    float delta_time_ms)
{
    if (!system || !current_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in lip_reading_update_dynamics");
        if (system) system->last_error = LIP_READING_ERROR_INVALID_INPUT;
        return false;
    }

    if (!previous_features || delta_time_ms <= 0) {
        current_features->lip_velocity_x = 0;
        current_features->lip_velocity_y = 0;
        current_features->lip_acceleration = 0;
        return true;
    }

    float dt_s = delta_time_ms / 1000.0f;

    /* Calculate velocity */
    float prev_vel_x = system->temporal_integrator->lip_velocity[0];
    float prev_vel_y = system->temporal_integrator->lip_velocity[1];

    float delta_width = current_features->lip_width - previous_features->lip_width;
    float delta_height = current_features->lip_height - previous_features->lip_height;

    current_features->lip_velocity_x = delta_width / dt_s;
    current_features->lip_velocity_y = delta_height / dt_s;

    /* Calculate acceleration */
    float accel_x = (current_features->lip_velocity_x - prev_vel_x) / dt_s;
    float accel_y = (current_features->lip_velocity_y - prev_vel_y) / dt_s;
    current_features->lip_acceleration = sqrtf(accel_x*accel_x + accel_y*accel_y);

    /* Update integrator state */
    system->temporal_integrator->lip_velocity[0] = current_features->lip_velocity_x;
    system->temporal_integrator->lip_velocity[1] = current_features->lip_velocity_y;
    system->temporal_integrator->lip_acceleration[0] = accel_x;
    system->temporal_integrator->lip_acceleration[1] = accel_y;

    /* Detect plosive burst (sudden opening) */
    if (current_features->lip_height > 5.0f &&
        current_features->lip_velocity_y > 20.0f &&
        current_features->lip_acceleration > 100.0f) {
        system->temporal_integrator->plosive_burst_detected = true;
        system->temporal_integrator->last_burst_time_ms = current_features->timestamp_ms;
    } else {
        system->temporal_integrator->plosive_burst_detected = false;
    }

    /* Detect closure */
    if (current_features->lip_height < 2.0f &&
        current_features->lip_velocity_y < -10.0f) {
        system->temporal_integrator->closure_detected = true;
        system->temporal_integrator->last_closure_time_ms = current_features->timestamp_ms;
    } else {
        system->temporal_integrator->closure_detected = false;
    }

    return true;
}

/*=============================================================================
 * VISEME CLASSIFICATION
 *===========================================================================*/

bool lip_reading_classify_viseme(
    lip_reading_system_t* system,
    const visual_speech_features_t* features,
    viseme_classification_t* result)
{
    if (!system || !features || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in lip_reading_classify_viseme");
        if (system) system->last_error = LIP_READING_ERROR_INVALID_INPUT;
        return false;
    }

    memset(result, 0, sizeof(viseme_classification_t));

    viseme_classifier_t* classifier = system->viseme_classifier;

    /* Compute viseme probabilities based on features */
    float scores[VISEME_COUNT] = {0};

    /* BILABIAL: Low lip height, lips together */
    scores[VISEME_BILABIAL] = (1.0f - features->lip_height / 20.0f) * 0.5f;
    if (features->lip_height < 3.0f) scores[VISEME_BILABIAL] += 0.5f;

    /* LABIODENTAL: Teeth on lip */
    scores[VISEME_LABIODENTAL] = features->upper_teeth_visible * 0.4f +
                                  (1.0f - features->lip_height / 15.0f) * 0.3f;

    /* DENTAL: Tongue visible between teeth */
    scores[VISEME_DENTAL] = features->tongue_visible * 0.6f +
                            features->teeth_gap / 10.0f * 0.2f;

    /* ALVEOLAR: Mouth slightly open, tongue tip up (less visible) */
    scores[VISEME_ALVEOLAR] = nimcp_clampf(features->lip_height / 10.0f, 0, 1) * 0.3f +
                              (1.0f - features->tongue_visible) * 0.2f;

    /* VELAR: Mouth open, no tongue visible */
    scores[VISEME_VELAR] = nimcp_clampf(features->lip_height / 15.0f, 0, 1) * 0.4f +
                           (1.0f - features->tongue_visible) * 0.3f;

    /* ROUNDED_CLOSE: Lips rounded, small aperture */
    scores[VISEME_ROUNDED_CLOSE] = features->lip_protrusion * 0.4f +
                                    (1.0f - features->lip_aspect_ratio / 2.0f) * 0.3f;

    /* ROUNDED_OPEN: Lips rounded, large aperture */
    scores[VISEME_ROUNDED_OPEN] = features->lip_protrusion * 0.3f +
                                   features->lip_height / 20.0f * 0.4f;

    /* UNROUNDED_CLOSE: Lips spread, small aperture */
    scores[VISEME_UNROUNDED_CLOSE] = (features->lip_aspect_ratio / 3.0f) * 0.4f +
                                      (1.0f - features->lip_height / 15.0f) * 0.3f;

    /* UNROUNDED_MID: Neutral lips */
    scores[VISEME_UNROUNDED_MID] = 0.3f;  /* Base probability */
    if (features->lip_aspect_ratio > 1.0f && features->lip_aspect_ratio < 2.5f) {
        scores[VISEME_UNROUNDED_MID] += 0.3f;
    }

    /* UNROUNDED_OPEN: Mouth wide open */
    scores[VISEME_UNROUNDED_OPEN] = nimcp_clampf(features->lip_height / 25.0f, 0, 1) * 0.5f +
                                     features->lip_area / 500.0f * 0.3f;

    /* SILENCE: Mouth closed, no movement */
    if (features->lip_height < 2.0f &&
        fabsf(features->lip_velocity_x) < 1.0f &&
        fabsf(features->lip_velocity_y) < 1.0f) {
        scores[VISEME_SILENCE] = 0.8f;
    } else {
        scores[VISEME_SILENCE] = 0.1f;
    }

    scores[VISEME_UNKNOWN] = 0.05f;  /* Base for unknown */

    /* Apply softmax */
    softmax(scores, VISEME_COUNT);

    /* Copy probabilities to result */
    memcpy(result->probabilities, scores, sizeof(scores));

    /* Apply temporal smoothing with transition matrix */
    if (classifier->history_count > 0 && system->config.enable_temporal_smoothing) {
        viseme_t prev = classifier->viseme_history[0];

        for (int v = 0; v < VISEME_COUNT; v++) {
            result->probabilities[v] *= classifier->transition_matrix[prev][v];
        }

        /* Re-normalize */
        float sum = 0;
        for (int v = 0; v < VISEME_COUNT; v++) {
            sum += result->probabilities[v];
        }
        if (sum > 0) {
            for (int v = 0; v < VISEME_COUNT; v++) {
                result->probabilities[v] /= sum;
            }
        }
    }

    /* Select best viseme */
    int best_idx = argmax_float(result->probabilities, VISEME_COUNT);
    result->viseme = (viseme_t)best_idx;
    result->confidence = result->probabilities[best_idx];

    /* Update history - use safe bounds */
    if (LIP_READING_MAX_VISEME_HISTORY > 1) {
        memmove(&classifier->viseme_history[1], &classifier->viseme_history[0],
                (LIP_READING_MAX_VISEME_HISTORY - 1) * sizeof(viseme_t));
    }
    classifier->viseme_history[0] = result->viseme;
    if (classifier->history_count < LIP_READING_MAX_VISEME_HISTORY) {
        classifier->history_count++;
    }

    /* Copy history to result - ensure we don't overflow result buffer */
    size_t copy_size = sizeof(classifier->viseme_history);
    if (copy_size > sizeof(result->viseme_history)) {
        copy_size = sizeof(result->viseme_history);
    }
    memcpy(result->viseme_history, classifier->viseme_history, copy_size);
    result->history_count = (classifier->history_count <= LIP_READING_MAX_VISEME_HISTORY) ?
        classifier->history_count : LIP_READING_MAX_VISEME_HISTORY;

    /* Detect transitions */
    if (classifier->history_count > 1 &&
        classifier->viseme_history[0] != classifier->viseme_history[1]) {
        result->is_transition = true;
        result->transition_progress = 0.5f;
        result->transition_target = classifier->viseme_history[0];
    }

    /* Plosive/closure from temporal integrator */
    result->plosive_burst_detected = system->temporal_integrator->plosive_burst_detected;
    result->closure_detected = system->temporal_integrator->closure_detected;

    result->timestamp_ms = get_timestamp_ms();

    system->status = LIP_READING_STATUS_VISEME_CLASSIFIED;
    system->stats.visemes_classified++;
    float new_avg_vc =
        (system->stats.avg_viseme_confidence * (system->stats.visemes_classified - 1) +
         result->confidence) / system->stats.visemes_classified;
    if (isfinite(new_avg_vc)) system->stats.avg_viseme_confidence = new_avg_vc;

    return true;
}

bool lip_reading_classify_viseme_from_roi(
    lip_reading_system_t* system,
    const uint8_t* mouth_roi,
    uint32_t roi_width,
    uint32_t roi_height,
    viseme_classification_t* result)
{
    if (!system || !mouth_roi || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in lip_reading_classify_viseme_from_roi");
        if (system) system->last_error = LIP_READING_ERROR_INVALID_INPUT;
        return false;
    }

    /* Extract features */
    visual_speech_features_t features;
    if (!lip_reading_extract_features(system, mouth_roi, roi_width, roi_height,
                                       NULL, &features)) {
        return false;  /* Feature extraction failed - normal processing failure */
    }

    /* Update dynamics if we have previous features */
    if (system->temporal_integrator->has_previous) {
        float delta_ms = (float)(features.timestamp_ms -
                         system->temporal_integrator->previous_features.timestamp_ms);
        lip_reading_update_dynamics(system, &features,
                                    &system->temporal_integrator->previous_features,
                                    delta_ms);
    }

    /* Store current as previous */
    system->temporal_integrator->previous_features = features;
    system->temporal_integrator->has_previous = true;

    /* Classify */
    return lip_reading_classify_viseme(system, &features, result);
}

viseme_t lip_reading_phoneme_to_viseme(phoneme_t phoneme) {
    switch (phoneme) {
        case PHONEME_P:
        case PHONEME_B:
        case PHONEME_M:
            return VISEME_BILABIAL;

        case PHONEME_F:
        case PHONEME_V:
            return VISEME_LABIODENTAL;

        case PHONEME_TH:
        case PHONEME_DH:
            return VISEME_DENTAL;

        case PHONEME_T:
        case PHONEME_D:
        case PHONEME_N:
        case PHONEME_L:
        case PHONEME_S:
        case PHONEME_Z:
            return VISEME_ALVEOLAR;

        case PHONEME_K:
        case PHONEME_G:
        case PHONEME_NG:
            return VISEME_VELAR;

        case PHONEME_UW:
        case PHONEME_OW:
            return VISEME_ROUNDED_CLOSE;

        case PHONEME_AO:
            return VISEME_ROUNDED_OPEN;

        case PHONEME_IY:
        case PHONEME_IH:
            return VISEME_UNROUNDED_CLOSE;

        case PHONEME_EY:
        case PHONEME_EH:
        case PHONEME_AH:
        case PHONEME_UH:
        case PHONEME_ER:
            return VISEME_UNROUNDED_MID;

        case PHONEME_AE:
        case PHONEME_AA:
            return VISEME_UNROUNDED_OPEN;

        case PHONEME_SH:
        case PHONEME_ZH:
        case PHONEME_CH:
        case PHONEME_J:
            return VISEME_ROUNDED_CLOSE;  /* Lip rounding */

        case PHONEME_H:
        case PHONEME_R:
        case PHONEME_W:
        case PHONEME_Y:
            return VISEME_UNROUNDED_MID;

        default:
            return VISEME_UNKNOWN;
    }
}

uint32_t lip_reading_viseme_to_phonemes(
    viseme_t viseme,
    phoneme_t* phonemes,
    uint32_t max_phonemes)
{
    if (!phonemes || max_phonemes == 0) return 0;

    uint32_t count = 0;

    switch (viseme) {
        case VISEME_BILABIAL:
            if (count < max_phonemes) phonemes[count++] = PHONEME_P;
            if (count < max_phonemes) phonemes[count++] = PHONEME_B;
            if (count < max_phonemes) phonemes[count++] = PHONEME_M;
            break;

        case VISEME_LABIODENTAL:
            if (count < max_phonemes) phonemes[count++] = PHONEME_F;
            if (count < max_phonemes) phonemes[count++] = PHONEME_V;
            break;

        case VISEME_DENTAL:
            if (count < max_phonemes) phonemes[count++] = PHONEME_TH;
            if (count < max_phonemes) phonemes[count++] = PHONEME_DH;
            break;

        case VISEME_ALVEOLAR:
            if (count < max_phonemes) phonemes[count++] = PHONEME_T;
            if (count < max_phonemes) phonemes[count++] = PHONEME_D;
            if (count < max_phonemes) phonemes[count++] = PHONEME_N;
            if (count < max_phonemes) phonemes[count++] = PHONEME_L;
            if (count < max_phonemes) phonemes[count++] = PHONEME_S;
            if (count < max_phonemes) phonemes[count++] = PHONEME_Z;
            break;

        case VISEME_VELAR:
            if (count < max_phonemes) phonemes[count++] = PHONEME_K;
            if (count < max_phonemes) phonemes[count++] = PHONEME_G;
            if (count < max_phonemes) phonemes[count++] = PHONEME_NG;
            break;

        case VISEME_ROUNDED_CLOSE:
            if (count < max_phonemes) phonemes[count++] = PHONEME_UW;
            if (count < max_phonemes) phonemes[count++] = PHONEME_OW;
            break;

        case VISEME_ROUNDED_OPEN:
            if (count < max_phonemes) phonemes[count++] = PHONEME_AO;
            break;

        case VISEME_UNROUNDED_CLOSE:
            if (count < max_phonemes) phonemes[count++] = PHONEME_IY;
            if (count < max_phonemes) phonemes[count++] = PHONEME_IH;
            break;

        case VISEME_UNROUNDED_MID:
            if (count < max_phonemes) phonemes[count++] = PHONEME_EH;
            if (count < max_phonemes) phonemes[count++] = PHONEME_AH;
            if (count < max_phonemes) phonemes[count++] = PHONEME_ER;
            break;

        case VISEME_UNROUNDED_OPEN:
            if (count < max_phonemes) phonemes[count++] = PHONEME_AE;
            if (count < max_phonemes) phonemes[count++] = PHONEME_AA;
            break;

        default:
            if (count < max_phonemes) phonemes[count++] = PHONEME_UNKNOWN;
            break;
    }

    return count;
}

const char* lip_reading_viseme_name(viseme_t viseme) {
    /* Bounds check with compile-time array size validation */
    if (viseme >= 0 && (size_t)viseme < sizeof(VISEME_NAMES)/sizeof(VISEME_NAMES[0]) &&
        viseme < VISEME_COUNT) {
        return VISEME_NAMES[viseme];
    }
    return "INVALID";
}

const char* lip_reading_phoneme_name(phoneme_t phoneme) {
    /* Bounds check with compile-time array size validation */
    if (phoneme >= 0 && (size_t)phoneme < sizeof(PHONEME_NAMES)/sizeof(PHONEME_NAMES[0]) &&
        phoneme < PHONEME_COUNT) {
        return PHONEME_NAMES[phoneme];
    }
    return "INVALID";
}

/*=============================================================================
 * AUDIOVISUAL INTEGRATION
 *===========================================================================*/

bool lip_reading_integrate_audiovisual(
    lip_reading_system_t* system,
    viseme_t visual_viseme,
    float visual_confidence,
    phoneme_t auditory_phoneme,
    float auditory_confidence,
    float auditory_snr,
    audiovisual_integration_t* result)
{
    if (!system || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in lip_reading_integrate_audiovisual");
        if (system) system->last_error = LIP_READING_ERROR_INVALID_INPUT;
        return false;
    }

    memset(result, 0, sizeof(audiovisual_integration_t));

    result->visual_viseme = visual_viseme;
    result->visual_confidence = visual_confidence;
    result->auditory_phoneme = auditory_phoneme;
    result->auditory_confidence = auditory_confidence;
    result->auditory_snr = auditory_snr;

    /* Compute reliability (inverse variance) */
    float var_visual = 1.0f - visual_confidence;
    float var_auditory = 1.0f - auditory_confidence;

    if (var_visual < 0.01f) var_visual = 0.01f;
    if (var_auditory < 0.01f) var_auditory = 0.01f;

    result->reliability_visual = 1.0f / var_visual;
    result->reliability_auditory = 1.0f / var_auditory;

    /* Adjust auditory reliability for SNR */
    /* SNR in dB: negative = noise dominates, positive = signal dominates */
    float snr_factor = nimcp_clampf((auditory_snr + 10.0f) / 30.0f, 0.1f, 1.0f);
    result->reliability_auditory *= snr_factor;

    /* Compute optimal weights (MLE) */
    float total_reliability = result->reliability_visual + result->reliability_auditory;
    result->visual_weight = result->reliability_visual / total_reliability;
    result->auditory_weight = result->reliability_auditory / total_reliability;

    /* Check for McGurk conflict */
    viseme_t expected_viseme = lip_reading_phoneme_to_viseme(auditory_phoneme);
    if (expected_viseme != visual_viseme && expected_viseme != VISEME_UNKNOWN) {
        result->mcgurk_conflict_detected = true;

        /* Classic McGurk: Visual /ga/ (velar) + Auditory /ba/ (bilabial) -> /da/ (alveolar) */
        if (visual_viseme == VISEME_VELAR && auditory_phoneme == PHONEME_B) {
            result->fused_phoneme = PHONEME_D;
            result->fusion_confidence = 0.7f;
            system->stats.mcgurk_effects_detected++;
            goto finish;
        }

        /* Similar McGurk-like fusions */
        if (visual_viseme == VISEME_VELAR && auditory_phoneme == PHONEME_P) {
            result->fused_phoneme = PHONEME_T;
            result->fusion_confidence = 0.65f;
            system->stats.mcgurk_effects_detected++;
            goto finish;
        }
    }

    /* Standard fusion based on weights */
    if (result->visual_weight > 0.7f) {
        /* Visual dominates - disambiguate viseme */
        result->fused_phoneme = lip_reading_disambiguate_viseme(
            visual_viseme, auditory_phoneme);
        result->fusion_confidence = visual_confidence * 0.8f;
    } else if (result->auditory_weight > 0.7f) {
        /* Auditory dominates */
        result->fused_phoneme = auditory_phoneme;
        result->fusion_confidence = auditory_confidence * 0.9f;
    } else {
        /* Balanced - use auditory with visual verification */
        viseme_t expected = lip_reading_phoneme_to_viseme(auditory_phoneme);
        if (expected == visual_viseme) {
            /* Agreement - high confidence */
            result->fused_phoneme = auditory_phoneme;
            result->fusion_confidence = (visual_confidence + auditory_confidence) / 2 * 1.1f;
            if (result->fusion_confidence > 1.0f) result->fusion_confidence = 1.0f;
        } else {
            /* Disagreement - lower confidence, prefer auditory */
            result->fused_phoneme = auditory_phoneme;
            result->fusion_confidence = auditory_confidence * 0.7f;
        }
    }

finish:
    result->temporal_offset_ms = system->av_integrator->temporal_offset_calibrated_ms;
    result->audio_visual_aligned = true;
    result->timestamp_ms = get_timestamp_ms();

    /* Update statistics */
    system->stats.audiovisual_fusions++;
    float new_avg_fc =
        (system->stats.avg_fusion_confidence * (system->stats.audiovisual_fusions - 1) +
         result->fusion_confidence) / system->stats.audiovisual_fusions;
    if (isfinite(new_avg_fc)) system->stats.avg_fusion_confidence = new_avg_fc;

    system->status = LIP_READING_STATUS_AUDIO_INTEGRATED;
    system->av_integrator->last_result = *result;

    return true;
}

phoneme_t lip_reading_disambiguate_viseme(viseme_t viseme, phoneme_t auditory_hint) {
    switch (viseme) {
        case VISEME_BILABIAL:
            /* /p/, /b/, /m/ - use voicing and nasality */
            if (lip_reading_is_phoneme_nasal(auditory_hint)) return PHONEME_M;
            if (lip_reading_is_phoneme_voiced(auditory_hint)) return PHONEME_B;
            return PHONEME_P;

        case VISEME_LABIODENTAL:
            /* /f/, /v/ - use voicing */
            if (lip_reading_is_phoneme_voiced(auditory_hint)) return PHONEME_V;
            return PHONEME_F;

        case VISEME_DENTAL:
            /* /th/, /dh/ - use voicing */
            if (lip_reading_is_phoneme_voiced(auditory_hint)) return PHONEME_DH;
            return PHONEME_TH;

        case VISEME_ALVEOLAR:
            /* Many possibilities - use auditory hint directly if alveolar */
            if (auditory_hint >= PHONEME_T && auditory_hint <= PHONEME_Z) {
                return auditory_hint;
            }
            return PHONEME_T;  /* Default */

        case VISEME_VELAR:
            /* /k/, /g/, /ng/ - use voicing and nasality */
            if (lip_reading_is_phoneme_nasal(auditory_hint)) return PHONEME_NG;
            if (lip_reading_is_phoneme_voiced(auditory_hint)) return PHONEME_G;
            return PHONEME_K;

        default:
            return auditory_hint;  /* Use auditory for vowels and others */
    }
}

bool lip_reading_is_phoneme_voiced(phoneme_t phoneme) {
    /* Bounds check with compile-time array size validation */
    if (phoneme >= 0 && (size_t)phoneme < sizeof(PHONEME_VOICED)/sizeof(PHONEME_VOICED[0]) &&
        phoneme < PHONEME_COUNT) {
        return PHONEME_VOICED[phoneme];
    }
    return false;
}

bool lip_reading_is_phoneme_nasal(phoneme_t phoneme) {
    /* Bounds check with compile-time array size validation */
    if (phoneme >= 0 && (size_t)phoneme < sizeof(PHONEME_NASAL)/sizeof(PHONEME_NASAL[0]) &&
        phoneme < PHONEME_COUNT) {
        return PHONEME_NASAL[phoneme];
    }
    return false;
}

/*=============================================================================
 * MOTOR THEORY (MIRROR NEURONS)
 *===========================================================================*/

bool lip_reading_viseme_to_motor_command(
    viseme_t viseme,
    articulatory_action_t* action)
{
    if (!action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lip_reading_viseme_to_motor_command: action is NULL");
        return false;
    }

    memset(action, 0, sizeof(articulatory_action_t));

    switch (viseme) {
        case VISEME_BILABIAL:
            action->type = ARTICULATORY_LIPS_CLOSED;
            action->lips_closed = true;
            action->mouth_aperture = 0.0f;
            break;

        case VISEME_LABIODENTAL:
            action->type = ARTICULATORY_TEETH_ON_LIP;
            action->upper_teeth_on_lower_lip = true;
            action->airflow_friction = true;
            action->mouth_aperture = 0.2f;
            break;

        case VISEME_DENTAL:
            action->type = ARTICULATORY_TONGUE_BETWEEN_TEETH;
            action->tongue_between_teeth = true;
            action->airflow_friction = true;
            action->mouth_aperture = 0.3f;
            break;

        case VISEME_ALVEOLAR:
            action->type = ARTICULATORY_TONGUE_TIP_UP;
            action->tongue_tip_up = true;
            action->mouth_aperture = 0.4f;
            break;

        case VISEME_VELAR:
            action->type = ARTICULATORY_TONGUE_BACK;
            action->tongue_back = true;
            action->mouth_aperture = 0.5f;
            break;

        case VISEME_ROUNDED_CLOSE:
            action->type = ARTICULATORY_LIPS_ROUNDED;
            action->lips_rounded = true;
            action->mouth_aperture = 0.3f;
            break;

        case VISEME_ROUNDED_OPEN:
            action->type = ARTICULATORY_LIPS_ROUNDED;
            action->lips_rounded = true;
            action->mouth_aperture = 0.7f;
            break;

        case VISEME_UNROUNDED_CLOSE:
            action->type = ARTICULATORY_LIPS_SPREAD;
            action->lips_spread = true;
            action->mouth_aperture = 0.3f;
            break;

        case VISEME_UNROUNDED_MID:
            action->type = ARTICULATORY_MOUTH_OPEN;
            action->mouth_aperture = 0.5f;
            break;

        case VISEME_UNROUNDED_OPEN:
            action->type = ARTICULATORY_MOUTH_OPEN;
            action->mouth_aperture = 0.9f;
            break;

        case VISEME_SILENCE:
            action->type = ARTICULATORY_LIPS_CLOSED;
            action->lips_closed = true;
            action->mouth_aperture = 0.0f;
            break;

        default:
            action->type = ARTICULATORY_NONE;
            break;
    }

    return true;
}

bool lip_reading_simulate_articulation(
    lip_reading_system_t* system,
    viseme_t viseme,
    const articulatory_action_t* action)
{
    if (!system || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lip_reading_simulate_articulation: required parameter is NULL (system, action)");
        return false;
    }

    /* In a full implementation, this would activate mirror neuron circuits
     * to simulate the articulatory gesture internally. For now, we just
     * log the simulation. */

    if (system->config.debug_mode) {
        printf("Simulating articulation: viseme=%s, aperture=%.2f\n",
               lip_reading_viseme_name(viseme), action->mouth_aperture);
    }

    return true;
}

/*=============================================================================
 * SPEAKER ADAPTATION
 *===========================================================================*/

uint32_t lip_reading_register_speaker(
    lip_reading_system_t* system,
    const char* speaker_name)
{
    if (!system) return 0;

    /* Find empty slot */
    int slot = -1;
    for (uint32_t i = 0; i < system->config.max_speakers; i++) {
        if (!system->speakers[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        system->last_error = LIP_READING_ERROR_BUFFER_FULL;
        return 0;
    }

    speaker_slot_t* s = &system->speakers[slot];
    memset(&s->profile, 0, sizeof(speaker_profile_t));

    s->profile.speaker_id = system->next_speaker_id++;
    if (speaker_name) {
        strncpy(s->profile.speaker_name, speaker_name, sizeof(s->profile.speaker_name) - 1);
        /* Ensure null termination even if source string is longer than buffer */
        s->profile.speaker_name[sizeof(s->profile.speaker_name) - 1] = '\0';
    }
    s->profile.accent = ACCENT_UNKNOWN;
    s->profile.adaptation_quality = 0.0f;
    s->profile.last_seen_ms = get_timestamp_ms();
    s->active = true;

    system->speaker_count++;
    system->stats.speakers_tracked++;

    return s->profile.speaker_id;
}

bool lip_reading_update_speaker_profile(
    lip_reading_system_t* system,
    uint32_t speaker_id,
    const visual_speech_features_t* features,
    phoneme_t actual_phoneme)
{
    if (!system || !features || speaker_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lip_reading_update_speaker_profile: required parameter is NULL (system, features)");
        return false;
    }

    /* Find speaker */
    speaker_slot_t* s = NULL;
    for (uint32_t i = 0; i < system->config.max_speakers; i++) {
        if (system->speakers[i].active &&
            system->speakers[i].profile.speaker_id == speaker_id) {
            s = &system->speakers[i];
            break;
        }
    }

    if (!s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lip_reading_update_speaker_profile: s is NULL");
        return false;
    }

    /* Update running statistics */
    float n = (float)s->profile.frames_observed;
    float new_avg_lw = (s->profile.avg_lip_width * n + features->lip_width) / (n + 1);
    float new_avg_lh = (s->profile.avg_lip_height * n + features->lip_height) / (n + 1);
    if (isfinite(new_avg_lw)) s->profile.avg_lip_width = new_avg_lw;
    if (isfinite(new_avg_lh)) s->profile.avg_lip_height = new_avg_lh;

    /* Update range */
    if (s->profile.frames_observed == 0) {
        s->profile.lip_width_range[0] = features->lip_width;
        s->profile.lip_width_range[1] = features->lip_width;
        s->profile.lip_height_range[0] = features->lip_height;
        s->profile.lip_height_range[1] = features->lip_height;
    } else {
        if (features->lip_width < s->profile.lip_width_range[0]) {
            s->profile.lip_width_range[0] = features->lip_width;
        }
        if (features->lip_width > s->profile.lip_width_range[1]) {
            s->profile.lip_width_range[1] = features->lip_width;
        }
        if (features->lip_height < s->profile.lip_height_range[0]) {
            s->profile.lip_height_range[0] = features->lip_height;
        }
        if (features->lip_height > s->profile.lip_height_range[1]) {
            s->profile.lip_height_range[1] = features->lip_height;
        }
    }

    s->profile.frames_observed++;
    s->profile.last_seen_ms = get_timestamp_ms();

    /* Update adaptation quality */
    if (s->profile.frames_observed >= system->config.adaptation_frames) {
        s->profile.adaptation_quality = 1.0f;
        system->stats.speakers_adapted++;
    } else {
        s->profile.adaptation_quality =
            (float)s->profile.frames_observed / system->config.adaptation_frames;
    }

    return true;
}

bool lip_reading_get_speaker_profile(
    lip_reading_system_t* system,
    uint32_t speaker_id,
    speaker_profile_t* profile)
{
    if (!system || !profile || speaker_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lip_reading_get_speaker_profile: required parameter is NULL (system, profile)");
        return false;
    }

    for (uint32_t i = 0; i < system->config.max_speakers; i++) {
        if (system->speakers[i].active &&
            system->speakers[i].profile.speaker_id == speaker_id) {
            *profile = system->speakers[i].profile;
            return true;
        }
    }

    return false;  /* Speaker not found - normal lookup behavior */
}

bool lip_reading_set_active_speaker(
    lip_reading_system_t* system,
    uint32_t speaker_id)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lip_reading_set_active_speaker: system is NULL");
        return false;
    }

    if (speaker_id == 0) {
        system->active_speaker_id = 0;
        return true;
    }

    /* Verify speaker exists */
    for (uint32_t i = 0; i < system->config.max_speakers; i++) {
        if (system->speakers[i].active &&
            system->speakers[i].profile.speaker_id == speaker_id) {
            system->active_speaker_id = speaker_id;
            return true;
        }
    }

    return false;  /* Speaker not found - normal lookup behavior */
}

/*=============================================================================
 * FULL PIPELINE
 *===========================================================================*/

lip_reading_status_t lip_reading_process_frame(
    lip_reading_system_t* system,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    viseme_classification_t* classification)
{
    if (!system || !image || !classification) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in lip_reading_process_frame");
        if (system) {
            system->last_error = LIP_READING_ERROR_INVALID_INPUT;
            system->status = LIP_READING_STATUS_ERROR;
        }
        return LIP_READING_STATUS_ERROR;
    }

    double start_time = get_timestamp_ms_precise();

    /* 1. Face detection */
    face_detection_result_t face_result;
    if (!lip_reading_detect_face(system, image, width, height, channels, &face_result)) {
        system->stats.face_detection_failures++;
        return system->status;
    }

    double face_time = (get_timestamp_ms_precise() - start_time);
    double new_avg_fd =
        (system->stats.avg_face_detection_ms * system->stats.frames_processed +
         face_time) / (system->stats.frames_processed + 1);
    if (isfinite(new_avg_fd)) system->stats.avg_face_detection_ms = new_avg_fd;

    /* 2. Extract mouth ROI */
    if (!lip_reading_extract_mouth_roi(system, image, width, height, channels,
                                        &face_result, system->mouth_roi_buffer,
                                        system->config.mouth_roi_width,
                                        system->config.mouth_roi_height)) {
        return system->status;
    }

    /* 3. Detect lip landmarks */
    lip_reading_detect_lip_landmarks(system, system->mouth_roi_buffer,
                                     system->config.mouth_roi_width,
                                     system->config.mouth_roi_height,
                                     &face_result);

    /* 4. Extract features */
    visual_speech_features_t features;
    if (!lip_reading_extract_features(system, system->mouth_roi_buffer,
                                       system->config.mouth_roi_width,
                                       system->config.mouth_roi_height,
                                       &face_result, &features)) {
        return system->status;
    }

    /* 5. Update dynamics */
    if (system->temporal_integrator->has_previous) {
        float delta_ms = (float)(features.timestamp_ms -
                         system->temporal_integrator->previous_features.timestamp_ms);
        lip_reading_update_dynamics(system, &features,
                                    &system->temporal_integrator->previous_features,
                                    delta_ms);
    }
    system->temporal_integrator->previous_features = features;
    system->temporal_integrator->has_previous = true;

    /* 6. Classify viseme */
    double class_start = get_timestamp_ms_precise();
    if (!lip_reading_classify_viseme(system, &features, classification)) {
        return system->status;
    }

    double class_time = (get_timestamp_ms_precise() - class_start);
    double new_avg_vc_ms =
        (system->stats.avg_viseme_classification_ms * system->stats.frames_processed +
         class_time) / (system->stats.frames_processed + 1);
    if (isfinite(new_avg_vc_ms)) system->stats.avg_viseme_classification_ms = new_avg_vc_ms;

    /* Update frame counter and timing */
    system->stats.frames_processed++;
    double total_time = (get_timestamp_ms_precise() - start_time);
    double new_avg_tp =
        (system->stats.avg_total_processing_ms * (system->stats.frames_processed - 1) +
         total_time) / system->stats.frames_processed;
    if (isfinite(new_avg_tp)) system->stats.avg_total_processing_ms = new_avg_tp;

    system->last_frame_time_ms = get_timestamp_ms();

    return system->status;
}

lip_reading_status_t lip_reading_process_frame_with_audio(
    lip_reading_system_t* system,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    phoneme_t auditory_phoneme,
    float auditory_confidence,
    float auditory_snr,
    audiovisual_integration_t* integration)
{
    if (!system || !image || !integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in lip_reading_process_frame_with_audio");
        if (system) {
            system->last_error = LIP_READING_ERROR_INVALID_INPUT;
            system->status = LIP_READING_STATUS_ERROR;
        }
        return LIP_READING_STATUS_ERROR;
    }

    /* First, process visual frame */
    viseme_classification_t classification;
    lip_reading_status_t status = lip_reading_process_frame(
        system, image, width, height, channels, &classification);

    if (status != LIP_READING_STATUS_VISEME_CLASSIFIED) {
        return status;
    }

    /* Now integrate with audio */
    uint64_t av_start = get_timestamp_ms();
    if (!lip_reading_integrate_audiovisual(system,
                                            classification.viseme,
                                            classification.confidence,
                                            auditory_phoneme,
                                            auditory_confidence,
                                            auditory_snr,
                                            integration)) {
        return system->status;
    }

    double av_time = (get_timestamp_ms() - av_start);
    double new_avg_av =
        (system->stats.avg_audiovisual_fusion_ms * system->stats.audiovisual_fusions +
         av_time) / (system->stats.audiovisual_fusions + 1);
    if (isfinite(new_avg_av)) system->stats.avg_audiovisual_fusion_ms = new_avg_av;

    return system->status;
}

/*=============================================================================
 * APPLICATIONS
 *===========================================================================*/

uint32_t lip_reading_enhance_speech_in_noise(
    lip_reading_system_t* system,
    const uint8_t** video_frames,
    uint32_t num_frames,
    uint32_t frame_width,
    uint32_t frame_height,
    uint32_t channels,
    const phoneme_t* audio_phonemes,
    const float* audio_confidences,
    float audio_snr,
    phoneme_t* recognized_phonemes,
    uint32_t max_phonemes)
{
    if (!system || !video_frames || !recognized_phonemes || num_frames == 0) {
        return 0;
    }

    uint32_t phoneme_count = 0;

    for (uint32_t i = 0; i < num_frames && phoneme_count < max_phonemes; i++) {
        audiovisual_integration_t integration;

        phoneme_t audio_phoneme = audio_phonemes ? audio_phonemes[i] : PHONEME_UNKNOWN;
        float audio_conf = audio_confidences ? audio_confidences[i] : 0.5f;

        lip_reading_status_t status = lip_reading_process_frame_with_audio(
            system,
            video_frames[i],
            frame_width,
            frame_height,
            channels,
            audio_phoneme,
            audio_conf,
            audio_snr,
            &integration);

        if (status == LIP_READING_STATUS_AUDIO_INTEGRATED) {
            recognized_phonemes[phoneme_count++] = integration.fused_phoneme;
        }
    }

    return phoneme_count;
}

uint32_t lip_reading_recognize_silent_speech(
    lip_reading_system_t* system,
    const uint8_t** video_frames,
    uint32_t num_frames,
    uint32_t frame_width,
    uint32_t frame_height,
    uint32_t channels,
    viseme_t* viseme_sequence,
    uint32_t max_visemes)
{
    if (!system || !video_frames || !viseme_sequence || num_frames == 0) {
        return 0;
    }

    uint32_t viseme_count = 0;

    for (uint32_t i = 0; i < num_frames && viseme_count < max_visemes; i++) {
        viseme_classification_t classification;

        lip_reading_status_t status = lip_reading_process_frame(
            system,
            video_frames[i],
            frame_width,
            frame_height,
            channels,
            &classification);

        if (status == LIP_READING_STATUS_VISEME_CLASSIFIED) {
            /* Only add if different from previous (reduce redundancy) */
            if (viseme_count == 0 ||
                viseme_sequence[viseme_count - 1] != classification.viseme) {
                viseme_sequence[viseme_count++] = classification.viseme;
            }
        }
    }

    return viseme_count;
}

/*=============================================================================
 * INTEGRATION
 *===========================================================================*/

bool lip_reading_connect_visual_cortex(
    lip_reading_system_t* system,
    visual_cortex_t* visual_cortex)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lip_reading_connect_visual_cortex: system is NULL");
        return false;
    }
    system->visual_cortex = visual_cortex;
    return true;
}

bool lip_reading_connect_speech_cortex(
    lip_reading_system_t* system,
    speech_cortex_t* speech_cortex)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lip_reading_connect_speech_cortex: system is NULL");
        return false;
    }
    system->speech_cortex = speech_cortex;
    return true;
}

bool lip_reading_connect_brain(
    lip_reading_system_t* system,
    brain_t* brain)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lip_reading_connect_brain: system is NULL");
        return false;
    }
    system->brain = brain;
    return true;
}

bool lip_reading_connect_bio_router(
    lip_reading_system_t* system,
    bio_router_t* router)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lip_reading_connect_bio_router: system is NULL");
        return false;
    }
    system->router = router;

    if (router && system->config.enable_bio_async) {
        /* Register with bio-async system */
        system->bio_registered = true;
    }

    return true;
}

/*=============================================================================
 * STATUS & DIAGNOSTICS
 *===========================================================================*/

lip_reading_status_t lip_reading_get_status(const lip_reading_system_t* system) {
    if (!system) return LIP_READING_STATUS_ERROR;
    return system->status;
}

lip_reading_error_t lip_reading_get_last_error(const lip_reading_system_t* system) {
    if (!system) return LIP_READING_ERROR_INTERNAL;
    return system->last_error;
}

const char* lip_reading_error_message(lip_reading_error_t error) {
    switch (error) {
        case LIP_READING_ERROR_NONE:
            return "No error";
        case LIP_READING_ERROR_INVALID_INPUT:
            return "Invalid input parameters";
        case LIP_READING_ERROR_NO_FACE_DETECTED:
            return "No face detected in image";
        case LIP_READING_ERROR_MOUTH_OCCLUDED:
            return "Mouth region is occluded";
        case LIP_READING_ERROR_LOW_CONFIDENCE:
            return "Classification confidence too low";
        case LIP_READING_ERROR_TRACKING_LOST:
            return "Face tracking lost";
        case LIP_READING_ERROR_BUFFER_FULL:
            return "Internal buffer full";
        case LIP_READING_ERROR_INTERNAL:
            return "Internal error";
        default:
            return "Unknown error";
    }
}

bool lip_reading_get_stats(
    const lip_reading_system_t* system,
    lip_reading_stats_t* stats)
{
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lip_reading_get_stats: required parameter is NULL (system, stats)");
        return false;
    }
    *stats = system->stats;
    return true;
}

bool lip_reading_reset_stats(lip_reading_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lip_reading_reset_stats: system is NULL");
        return false;
    }
    memset(&system->stats, 0, sizeof(lip_reading_stats_t));
    return true;
}
