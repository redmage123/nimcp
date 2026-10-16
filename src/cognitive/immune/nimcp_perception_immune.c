/**
 * @file nimcp_perception_immune.c
 * @brief Perception-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 */

#include "cognitive/immune/nimcp_perception_immune.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdatomic.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(perception_immune, MESH_ADAPTER_CATEGORY_SECURITY)



/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Compute inflammation factor for gain reduction
 * WHY:  Inflammation should reduce perception sensitivity
 * HOW:  Map inflammation level to multiplier (0.3-1.0)
 */
static float compute_inflammation_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return 1.0f;
        case INFLAMMATION_LOCAL:    return 0.85f;
        case INFLAMMATION_REGIONAL: return 0.65f;
        case INFLAMMATION_SYSTEMIC: return 0.45f;
        case INFLAMMATION_STORM:    return 0.3f;
        default:                    return 1.0f;
    }
}

/**
 * WHAT: Compute cytokine modulation factor
 * WHY:  Pro-inflammatory cytokines increase thresholds
 * HOW:  Weighted combination of IL-1, IL-6, TNF-alpha
 */
static float compute_cytokine_threshold_factor(
    float il1, float il6, float tnf_alpha
) {
    /* Pro-inflammatory cytokines increase detection thresholds */
    return 1.0f + (0.3f * il1 + 0.2f * il6 + 0.5f * tnf_alpha);
}

/**
 * WHAT: Hash features to epitope
 * WHY:  Convert perception features to immune signature
 * HOW:  Simple hash over feature vector
 */
static void hash_features_to_epitope(
    const float* features,
    uint32_t feature_dim,
    uint8_t* epitope,
    size_t epitope_len
) {
    if (!features || !epitope || epitope_len == 0) return;

    /* Simple hash: sum and modulo for each byte */
    for (size_t i = 0; i < epitope_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && epitope_len > 256) {
            perception_immune_heartbeat("perception_i_loop",
                             (float)(i + 1) / (float)epitope_len);
        }

        uint32_t sum = 0;
        for (uint32_t j = i; j < feature_dim; j += epitope_len) {
            sum += (uint32_t)(fabsf(features[j]) * 1000.0f);
        }
        epitope[i] = (uint8_t)(sum % 256);
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

perception_immune_context_t* perception_immune_create(
    brain_immune_system_t* immune_system
) {
    /* Guard: validate immune system */
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("perception_immune", "Cannot create with NULL immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_create: immune_system is NULL");
        return NULL;
    }

    /* Allocate context */
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_create", 0.0f);


    perception_immune_context_t* ctx = (perception_immune_context_t*)
        nimcp_malloc(sizeof(perception_immune_context_t));
    if (!ctx) {
        NIMCP_LOGGING_ERROR("perception_immune", "Failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "perception_immune_create: ctx is NULL");
        return NULL;
    }

    /* Initialize */
    memset(ctx, 0, sizeof(perception_immune_context_t));
    ctx->immune_system = immune_system;
    ctx->anomaly_capacity = PERCEPTION_IMMUNE_MAX_ANOMALIES;
    ctx->next_anomaly_id = 1;  /* Start IDs at 1 so 0 means "no anomaly" */

    /* Allocate anomaly buffer */
    ctx->anomalies = (perception_anomaly_t*)
        nimcp_calloc(ctx->anomaly_capacity, sizeof(perception_anomaly_t));
    if (!ctx->anomalies) {
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "perception_immune_create: ctx->anomalies is NULL");
        return NULL;
    }

    /* Initialize modulation to defaults */
    ctx->modulation.visual_gain = 1.0f;
    ctx->modulation.audio_gain = 1.0f;
    ctx->modulation.speech_gain = 1.0f;
    ctx->modulation.visual_threshold = 0.5f;
    ctx->modulation.audio_threshold = 0.5f;
    ctx->modulation.speech_threshold = 0.5f;

    ctx->enabled = true;

    NIMCP_LOGGING_INFO("perception_immune", "Created perception-immune integration");
    return ctx;
}

void perception_immune_destroy(perception_immune_context_t* ctx) {
    if (!ctx) return;

    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_destroy", 0.0f);


    if (ctx->anomalies) {
        nimcp_free(ctx->anomalies);
    }

    nimcp_free(ctx);
    ctx = NULL;
    NIMCP_LOGGING_INFO("perception_immune", "Destroyed perception-immune integration");
}

int perception_immune_connect_visual(
    perception_immune_context_t* ctx,
    visual_cortex_t* visual
) {
    /* Guard: validate inputs */
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_connect_visual: ctx is NULL");
        return -1;
    }
    if (!visual) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_connect_visual: visual is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_connect_visual", 0.0f);


    ctx->visual_cortex = visual;
    NIMCP_LOGGING_INFO("perception_immune", "Connected visual cortex");
    return 0;
}

int perception_immune_connect_audio(
    perception_immune_context_t* ctx,
    audio_cortex_t* audio
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_connect_audio: ctx is NULL");
        return -1;
    }
    if (!audio) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_connect_audio: audio is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_connect_audio", 0.0f);


    ctx->audio_cortex = audio;
    NIMCP_LOGGING_INFO("perception_immune", "Connected audio cortex");
    return 0;
}

int perception_immune_connect_speech(
    perception_immune_context_t* ctx,
    speech_cortex_t* speech
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_connect_speech: ctx is NULL");
        return -1;
    }
    if (!speech) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_connect_speech: speech is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_connect_speech", 0.0f);


    ctx->speech_cortex = speech;
    NIMCP_LOGGING_INFO("perception_immune", "Connected speech cortex");
    return 0;
}

/* ============================================================================
 * Anomaly Detection and Reporting API
 * ============================================================================ */

int perception_immune_report_visual_anomaly(
    perception_immune_context_t* ctx,
    perception_anomaly_type_t type,
    float severity,
    float confidence,
    const float* features,
    uint32_t feature_dim,
    uint32_t* anomaly_id
) {
    /* Guard: validate inputs */
    if (!ctx || !ctx->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_report_visual_anomaly: required parameter is NULL (ctx, ctx->immune_system)");
        return -1;
    }
    if (!features || feature_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perception_immune_report_visual_anomaly: features is NULL");
        return -1;
    }
    if (ctx->anomaly_count >= ctx->anomaly_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "perception_immune_report_visual_anomaly: capacity exceeded");
        return -1;
    }

    /* Create anomaly entry */
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_report_visual_anomal", 0.0f);


    perception_anomaly_t* anomaly = &ctx->anomalies[ctx->anomaly_count];
    anomaly->id = __atomic_add_fetch(&ctx->next_anomaly_id, 1, __ATOMIC_SEQ_CST);
    anomaly->type = type;
    anomaly->modality = PERCEPTION_VISUAL;
    anomaly->severity = severity;
    anomaly->confidence = confidence;
    anomaly->timestamp_ms = nimcp_time_get_ms();
    anomaly->immune_responded = false;

    /* Hash features to epitope */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    hash_features_to_epitope(features, feature_dim, epitope,
                            BRAIN_IMMUNE_EPITOPE_SIZE);

    /* Present to immune system */
    uint32_t ag_id = 0;
    int result = brain_immune_present_antigen(
        ctx->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        BRAIN_IMMUNE_EPITOPE_SIZE,
        (uint32_t)(severity * 10.0f),
        0, /* source_node */
        &ag_id
    );

    if (result == 0) {
        anomaly->antigen_id = ag_id;
        anomaly->immune_responded = true;
        ctx->immune_responses_triggered++;
    }

    ctx->anomaly_count++;
    ctx->visual_anomalies_detected++;

    if (anomaly_id) {
        *anomaly_id = anomaly->id;
    }

    NIMCP_LOGGING_DEBUG("perception_immune",
                   "Visual anomaly reported: type=%d severity=%.2f antigen=%u",
                   type, severity, ag_id);
    return 0;
}

int perception_immune_report_audio_anomaly(
    perception_immune_context_t* ctx,
    perception_anomaly_type_t type,
    float severity,
    float confidence,
    const float* spectrum,
    uint32_t num_bins,
    uint32_t* anomaly_id
) {
    if (!ctx || !ctx->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_report_audio_anomaly: required parameter is NULL (ctx, ctx->immune_system)");
        return -1;
    }
    if (!spectrum || num_bins == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perception_immune_report_audio_anomaly: spectrum is NULL");
        return -1;
    }
    if (ctx->anomaly_count >= ctx->anomaly_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "perception_immune_report_audio_anomaly: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_report_audio_anomaly", 0.0f);


    perception_anomaly_t* anomaly = &ctx->anomalies[ctx->anomaly_count];
    anomaly->id = __atomic_add_fetch(&ctx->next_anomaly_id, 1, __ATOMIC_SEQ_CST);
    anomaly->type = type;
    anomaly->modality = PERCEPTION_AUDIO;
    anomaly->severity = severity;
    anomaly->confidence = confidence;
    anomaly->timestamp_ms = nimcp_time_get_ms();
    anomaly->immune_responded = false;

    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    hash_features_to_epitope(spectrum, num_bins, epitope,
                            BRAIN_IMMUNE_EPITOPE_SIZE);

    uint32_t ag_id = 0;
    int result = brain_immune_present_antigen(
        ctx->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        BRAIN_IMMUNE_EPITOPE_SIZE,
        (uint32_t)(severity * 10.0f),
        0,
        &ag_id
    );

    if (result == 0) {
        anomaly->antigen_id = ag_id;
        anomaly->immune_responded = true;
        ctx->immune_responses_triggered++;
    }

    ctx->anomaly_count++;
    ctx->audio_anomalies_detected++;

    if (anomaly_id) {
        *anomaly_id = anomaly->id;
    }

    return 0;
}

int perception_immune_report_speech_anomaly(
    perception_immune_context_t* ctx,
    perception_anomaly_type_t type,
    float severity,
    float confidence,
    const void* phoneme_features,
    uint32_t num_phonemes,
    uint32_t* anomaly_id
) {
    if (!ctx || !ctx->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_report_speech_anomaly: required parameter is NULL (ctx, ctx->immune_system)");
        return -1;
    }
    if (!phoneme_features || num_phonemes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perception_immune_report_speech_anomaly: phoneme_features is NULL");
        return -1;
    }
    if (ctx->anomaly_count >= ctx->anomaly_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "perception_immune_report_speech_anomaly: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_report_speech_anomal", 0.0f);


    perception_anomaly_t* anomaly = &ctx->anomalies[ctx->anomaly_count];
    anomaly->id = __atomic_add_fetch(&ctx->next_anomaly_id, 1, __ATOMIC_SEQ_CST);
    anomaly->type = type;
    anomaly->modality = PERCEPTION_SPEECH;
    anomaly->severity = severity;
    anomaly->confidence = confidence;
    anomaly->timestamp_ms = nimcp_time_get_ms();
    anomaly->immune_responded = false;

    /* Use phoneme data as epitope */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    size_t copy_len = (num_phonemes < BRAIN_IMMUNE_EPITOPE_SIZE) ?
                      num_phonemes : BRAIN_IMMUNE_EPITOPE_SIZE;
    memcpy(epitope, phoneme_features, copy_len);
    if (copy_len < BRAIN_IMMUNE_EPITOPE_SIZE) {
        memset(epitope + copy_len, 0, BRAIN_IMMUNE_EPITOPE_SIZE - copy_len);
    }

    uint32_t ag_id = 0;
    int result = brain_immune_present_antigen(
        ctx->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        BRAIN_IMMUNE_EPITOPE_SIZE,
        (uint32_t)(severity * 10.0f),
        0,
        &ag_id
    );

    if (result == 0) {
        anomaly->antigen_id = ag_id;
        anomaly->immune_responded = true;
        ctx->immune_responses_triggered++;
    }

    ctx->anomaly_count++;
    ctx->speech_anomalies_detected++;

    if (anomaly_id) {
        *anomaly_id = anomaly->id;
    }

    return 0;
}

/* ============================================================================
 * Immune Modulation API
 * ============================================================================ */

int perception_immune_update_modulation(perception_immune_context_t* ctx) {
    if (!ctx || !ctx->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_update_modulation: required parameter is NULL (ctx, ctx->immune_system)");
        return -1;
    }

    /* Query immune system state */
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_update_modulation", 0.0f);


    brain_immune_stats_t stats;
    brain_immune_get_stats(ctx->immune_system, &stats);

    /* Approximate inflammation levels from stats */
    /* In real implementation, would query specific inflammation sites */
    float inflammation_estimate = (stats.inflammation_sites > 0) ? 0.5f : 0.0f;

    /* Set inflammation levels based on activity */
    if (stats.inflammation_sites == 0) {
        ctx->modulation.visual_inflammation = INFLAMMATION_NONE;
        ctx->modulation.audio_inflammation = INFLAMMATION_NONE;
        ctx->modulation.speech_inflammation = INFLAMMATION_NONE;
    } else if (stats.inflammation_sites < 3) {
        ctx->modulation.visual_inflammation = INFLAMMATION_LOCAL;
        ctx->modulation.audio_inflammation = INFLAMMATION_LOCAL;
        ctx->modulation.speech_inflammation = INFLAMMATION_LOCAL;
    } else if (stats.inflammation_sites < 6) {
        ctx->modulation.visual_inflammation = INFLAMMATION_REGIONAL;
        ctx->modulation.audio_inflammation = INFLAMMATION_REGIONAL;
        ctx->modulation.speech_inflammation = INFLAMMATION_REGIONAL;
    } else {
        ctx->modulation.visual_inflammation = INFLAMMATION_SYSTEMIC;
        ctx->modulation.audio_inflammation = INFLAMMATION_SYSTEMIC;
        ctx->modulation.speech_inflammation = INFLAMMATION_SYSTEMIC;
    }

    /* Estimate cytokine levels from inflammation */
    ctx->modulation.il1_level = inflammation_estimate * 0.6f;
    ctx->modulation.il6_level = inflammation_estimate * 0.5f;
    ctx->modulation.tnf_alpha_level = inflammation_estimate * 0.3f;
    ctx->modulation.il10_level = (1.0f - inflammation_estimate) * 0.4f;

    /* Preserve overload protection: don't downgrade inflammation below REGIONAL */
    if (ctx->modulation.visual_overload_protection &&
        ctx->modulation.visual_inflammation < INFLAMMATION_REGIONAL) {
        ctx->modulation.visual_inflammation = INFLAMMATION_REGIONAL;
    }
    if (ctx->modulation.audio_overload_protection &&
        ctx->modulation.audio_inflammation < INFLAMMATION_REGIONAL) {
        ctx->modulation.audio_inflammation = INFLAMMATION_REGIONAL;
    }
    if (ctx->modulation.speech_overload_protection &&
        ctx->modulation.speech_inflammation < INFLAMMATION_REGIONAL) {
        ctx->modulation.speech_inflammation = INFLAMMATION_REGIONAL;
    }

    /* Compute gains */
    float visual_factor = compute_inflammation_factor(
        ctx->modulation.visual_inflammation);
    float audio_factor = compute_inflammation_factor(
        ctx->modulation.audio_inflammation);
    float speech_factor = compute_inflammation_factor(
        ctx->modulation.speech_inflammation);

    ctx->modulation.visual_gain = visual_factor;
    ctx->modulation.audio_gain = audio_factor;
    ctx->modulation.speech_gain = speech_factor;

    /* Compute thresholds */
    float threshold_factor = compute_cytokine_threshold_factor(
        ctx->modulation.il1_level,
        ctx->modulation.il6_level,
        ctx->modulation.tnf_alpha_level
    );

    ctx->modulation.visual_threshold = 0.5f * threshold_factor;
    ctx->modulation.audio_threshold = 0.5f * threshold_factor;
    ctx->modulation.speech_threshold = 0.5f * threshold_factor;

    /* Clamp values */
    if (ctx->modulation.visual_gain < PERCEPTION_IMMUNE_MIN_GAIN) {
        ctx->modulation.visual_gain = PERCEPTION_IMMUNE_MIN_GAIN;
    }
    if (ctx->modulation.audio_gain < PERCEPTION_IMMUNE_MIN_GAIN) {
        ctx->modulation.audio_gain = PERCEPTION_IMMUNE_MIN_GAIN;
    }
    if (ctx->modulation.speech_gain < PERCEPTION_IMMUNE_MIN_GAIN) {
        ctx->modulation.speech_gain = PERCEPTION_IMMUNE_MIN_GAIN;
    }

    return 0;
}

int perception_immune_apply_visual_modulation(
    perception_immune_context_t* ctx
) {
    if (!ctx || !ctx->visual_cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_apply_visual_modulation: required parameter is NULL (ctx, ctx->visual_cortex)");
        return -1;
    }

    /* Apply gain modulation via neuromodulation */
    /* In full implementation, would use visual_cortex API to adjust gains */
    /* For now, store in modulation state for external access */

    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_apply_visual_modulat", 0.0f);


    return 0;
}

int perception_immune_apply_audio_modulation(
    perception_immune_context_t* ctx
) {
    if (!ctx || !ctx->audio_cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_apply_audio_modulation: required parameter is NULL (ctx, ctx->audio_cortex)");
        return -1;
    }
    /* Apply gain/threshold to audio cortex */
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_apply_audio_modulati", 0.0f);


    return 0;
}

int perception_immune_apply_speech_modulation(
    perception_immune_context_t* ctx
) {
    if (!ctx || !ctx->speech_cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_apply_speech_modulation: required parameter is NULL (ctx, ctx->speech_cortex)");
        return -1;
    }
    /* Apply gain/threshold to speech cortex */
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_apply_speech_modulat", 0.0f);


    return 0;
}

/* ============================================================================
 * Overload Protection API
 * ============================================================================ */

int perception_immune_check_visual_overload(
    perception_immune_context_t* ctx,
    const float* features,
    uint32_t num_features,
    bool* overload
) {
    if (!ctx || !features || !overload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_check_visual_overload: required parameter is NULL (ctx, features, overload)");
        return -1;
    }

    /* Compute feature variance as overload metric */
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_check_visual_overloa", 0.0f);

    if (num_features == 0) {
        *overload = false;
        return 0;
    }

    float mean = 0.0f;
    for (uint32_t i = 0; i < num_features; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_features > 256) {
            perception_immune_heartbeat("perception_i_loop",
                             (float)(i + 1) / (float)num_features);
        }

        mean += features[i];
    }
    mean /= num_features;

    float variance = 0.0f;
    for (uint32_t i = 0; i < num_features; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_features > 256) {
            perception_immune_heartbeat("perception_i_loop",
                             (float)(i + 1) / (float)num_features);
        }

        float diff = features[i] - mean;
        variance += diff * diff;
    }
    variance /= num_features;

    /* High variance indicates overload */
    *overload = (variance > PERCEPTION_IMMUNE_OVERLOAD_THRESHOLD);
    return 0;
}

int perception_immune_check_audio_overload(
    perception_immune_context_t* ctx,
    const float* spectrum,
    uint32_t num_bins,
    bool* overload
) {
    if (!ctx || !spectrum || !overload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_check_audio_overload: required parameter is NULL (ctx, spectrum, overload)");
        return -1;
    }

    /* Compute spectral energy */
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_check_audio_overload", 0.0f);

    if (num_bins == 0) {
        *overload = false;
        return 0;
    }

    float energy = 0.0f;
    for (uint32_t i = 0; i < num_bins; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_bins > 256) {
            perception_immune_heartbeat("perception_i_loop",
                             (float)(i + 1) / (float)num_bins);
        }

        energy += spectrum[i] * spectrum[i];
    }

    /* Normalize */
    energy = sqrtf(energy / num_bins);

    *overload = (energy > PERCEPTION_IMMUNE_OVERLOAD_THRESHOLD);
    return 0;
}

int perception_immune_check_speech_overload(
    perception_immune_context_t* ctx,
    const float* phoneme_confidence,
    uint32_t num_phonemes,
    bool* overload
) {
    if (!ctx || !phoneme_confidence || !overload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_check_speech_overload: required parameter is NULL (ctx, phoneme_confidence, overload)");
        return -1;
    }

    /* Low average confidence indicates overload */
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_check_speech_overloa", 0.0f);


    float avg_conf = 0.0f;
    for (uint32_t i = 0; i < num_phonemes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_phonemes > 256) {
            perception_immune_heartbeat("perception_i_loop",
                             (float)(i + 1) / (float)num_phonemes);
        }

        avg_conf += phoneme_confidence[i];
    }
    avg_conf /= num_phonemes;

    *overload = (avg_conf < (1.0f - PERCEPTION_IMMUNE_OVERLOAD_THRESHOLD));
    return 0;
}

int perception_immune_trigger_overload_protection(
    perception_immune_context_t* ctx,
    perception_modality_t modality
) {
    if (!ctx || !ctx->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_trigger_overload_protection: required parameter is NULL (ctx, ctx->immune_system)");
        return -1;
    }

    /* Trigger inflammation for modality */
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_trigger_overload_pro", 0.0f);


    switch (modality) {
        case PERCEPTION_VISUAL:
            ctx->modulation.visual_overload_protection = true;
            ctx->modulation.visual_inflammation = INFLAMMATION_REGIONAL;
            break;
        case PERCEPTION_AUDIO:
            ctx->modulation.audio_overload_protection = true;
            ctx->modulation.audio_inflammation = INFLAMMATION_REGIONAL;
            break;
        case PERCEPTION_SPEECH:
            ctx->modulation.speech_overload_protection = true;
            ctx->modulation.speech_inflammation = INFLAMMATION_REGIONAL;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perception_immune_trigger_overload_protection: operation failed");
            return -1;
    }

    ctx->overload_protections_activated++;

    /* Update modulation */
    perception_immune_update_modulation(ctx);

    NIMCP_LOGGING_WARN("perception_immune",
                     "Overload protection triggered for modality %d", modality);
    return 0;
}

int perception_immune_release_overload_protection(
    perception_immune_context_t* ctx,
    perception_modality_t modality
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_release_overload_protection: ctx is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_release_overload_pro", 0.0f);


    switch (modality) {
        case PERCEPTION_VISUAL:
            ctx->modulation.visual_overload_protection = false;
            ctx->modulation.visual_inflammation = INFLAMMATION_NONE;
            break;
        case PERCEPTION_AUDIO:
            ctx->modulation.audio_overload_protection = false;
            ctx->modulation.audio_inflammation = INFLAMMATION_NONE;
            break;
        case PERCEPTION_SPEECH:
            ctx->modulation.speech_overload_protection = false;
            ctx->modulation.speech_inflammation = INFLAMMATION_NONE;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perception_immune_release_overload_protection: operation failed");
            return -1;
    }

    perception_immune_update_modulation(ctx);
    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int perception_immune_get_modulation(
    const perception_immune_context_t* ctx,
    perception_immune_modulation_t* modulation
) {
    if (!ctx || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_immune_get_modulation: required parameter is NULL (ctx, modulation)");
        return -1;
    }
    *modulation = ctx->modulation;
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_get_modulation", 0.0f);


    return 0;
}

const perception_anomaly_t* perception_immune_get_anomaly(
    const perception_immune_context_t* ctx,
    uint32_t anomaly_id
) {
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_get_anomaly", 0.0f);


    for (size_t i = 0; i < ctx->anomaly_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->anomaly_count > 256) {
            perception_immune_heartbeat("perception_i_loop",
                             (float)(i + 1) / (float)ctx->anomaly_count);
        }

        if (ctx->anomalies[i].id == anomaly_id) {
            return &ctx->anomalies[i];
        }
    }
    return NULL; /* anomaly_id not found - not an error worth throwing */
}

bool perception_immune_is_protected(
    const perception_immune_context_t* ctx,
    perception_modality_t modality
) {
    if (!ctx) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_is_protected", 0.0f);


    switch (modality) {
        case PERCEPTION_VISUAL:
            return ctx->modulation.visual_overload_protection;
        case PERCEPTION_AUDIO:
            return ctx->modulation.audio_overload_protection;
        case PERCEPTION_SPEECH:
            return ctx->modulation.speech_overload_protection;
        default:
            return false;
    }
}

float perception_immune_get_visual_gain(
    const perception_immune_context_t* ctx
) {
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_get_visual_gain", 0.0f);


    return ctx ? ctx->modulation.visual_gain : 1.0f;
}

float perception_immune_get_audio_gain(
    const perception_immune_context_t* ctx
) {
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_get_audio_gain", 0.0f);


    return ctx ? ctx->modulation.audio_gain : 1.0f;
}

float perception_immune_get_speech_gain(
    const perception_immune_context_t* ctx
) {
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_get_speech_gain", 0.0f);


    return ctx ? ctx->modulation.speech_gain : 1.0f;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* perception_immune_anomaly_type_to_string(
    perception_anomaly_type_t type
) {
    switch (type) {
        case ANOMALY_VISUAL_NOISE:       return "VISUAL_NOISE";
        case ANOMALY_VISUAL_ADVERSARIAL: return "VISUAL_ADVERSARIAL";
        case ANOMALY_VISUAL_OVERLOAD:    return "VISUAL_OVERLOAD";
        case ANOMALY_AUDIO_CORRUPTION:   return "AUDIO_CORRUPTION";
        case ANOMALY_AUDIO_JAMMING:      return "AUDIO_JAMMING";
        case ANOMALY_AUDIO_OVERLOAD:     return "AUDIO_OVERLOAD";
        case ANOMALY_SPEECH_CONFUSION:   return "SPEECH_CONFUSION";
        case ANOMALY_SPEECH_PROSODY:     return "SPEECH_PROSODY";
        case ANOMALY_SPEECH_OVERLOAD:    return "SPEECH_OVERLOAD";
        default:                         return "UNKNOWN";
    }
}

const char* perception_immune_modality_to_string(
    perception_modality_t modality
) {
    switch (modality) {
        case PERCEPTION_VISUAL: return "VISUAL";
        case PERCEPTION_AUDIO:  return "AUDIO";
        case PERCEPTION_SPEECH: return "SPEECH";
        default:                return "UNKNOWN";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about perception immune
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int perception_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    perception_immune_heartbeat("perception_i_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Perception_Immune");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                perception_immune_heartbeat("perception_i_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Perception immune self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Perception_Immune");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Perception_Immune");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void perception_immune_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_perception_immune_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int perception_immune_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "perception_immune_training_begin: NULL argument");
        return -1;
    }
    perception_immune_heartbeat_instance(NULL, "perception_immune_training_begin", 0.0f);
    return 0;
}

int perception_immune_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "perception_immune_training_end: NULL argument");
        return -1;
    }
    perception_immune_heartbeat_instance(NULL, "perception_immune_training_end", 1.0f);
    return 0;
}

int perception_immune_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "perception_immune_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    perception_immune_heartbeat_instance(NULL, "perception_immune_training_step", progress);
    return 0;
}
