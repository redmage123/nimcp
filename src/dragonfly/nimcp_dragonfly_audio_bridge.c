/**
 * @file nimcp_dragonfly_audio_bridge.c
 * @brief Audio Cortex Bridge for Dragonfly Module - Implementation
 *
 * Sound localization and audio-visual fusion for multi-modal target tracking.
 *
 * @author NIMCP Team
 * @date 2024-12-28
 */

#include "dragonfly/nimcp_dragonfly_audio_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "dragonfly/nimcp_dragonfly_visual_bridge.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_audio_bridge)

#define LOG_MODULE "DRAGONFLY_AUDIO_BRIDGE"



//=============================================================================
// Internal Time Helper
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Source tracking history entry
 */
typedef struct {
    audio_source_t source;
    uint64_t last_seen_us;
    bool active;
} source_history_t;

/**
 * @brief Audio bridge internal state
 */
struct dragonfly_audio_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Connected systems */
    dragonfly_system_t* dragonfly;
    audio_cortex_t* audio_cortex;

    /* Configuration */
    audio_bridge_config_t config;

    /* State */
    bool initialized;

    /* Latest result */
    audio_detection_result_t latest_result;

    /* Source tracking */
    source_history_t source_history[AUDIO_BRIDGE_MAX_SOURCES];
    uint32_t num_active_sources;
    uint32_t next_source_id;

    /* Smoothed directions */
    float smoothed_azimuth;
    float smoothed_elevation;
    bool has_smoothed_direction;

    /* History for correlation */
    audio_source_t history_buffer[AUDIO_BRIDGE_MAX_HISTORY];
    uint32_t history_write_idx;
    uint32_t history_count;

    /* Statistics */
    audio_bridge_stats_t stats;
    uint64_t process_time_sum;
};

//=============================================================================
// Configuration Functions
//=============================================================================

audio_bridge_config_t audio_bridge_default_config(void) {
    audio_bridge_config_t config = {0};

    /* Sound detection */
    config.min_intensity_db = 40.0f;       /* 40 dB threshold */
    config.min_frequency_hz = 200.0f;      /* Low frequency cutoff */
    config.max_frequency_hz = 8000.0f;     /* High frequency cutoff */
    config.detection_threshold = 0.3f;

    /* Localization */
    config.loc_mode = AUDIO_LOC_COMBINED;
    config.ear_separation_m = 0.15f;       /* ~15cm typical */
    config.speed_of_sound_mps = 343.0f;    /* Speed of sound at 20C */

    /* Distance estimation */
    config.estimate_distance = true;
    config.reference_db_at_1m = 70.0f;     /* Reference level at 1m */
    config.attenuation_exp = 2.0f;         /* Inverse square law */

    /* Multi-modal fusion */
    config.enable_visual_fusion = true;
    config.correlation_threshold = 0.6f;
    config.max_angular_diff = 0.3f;        /* ~17 degrees */
    config.max_temporal_diff_ms = 100.0f;  /* 100ms window */

    /* Cueing */
    config.enable_attention_cue = true;
    config.cue_priority_boost = 0.3f;

    /* Filtering */
    config.persistence_ms = 200.0f;
    config.smoothing_alpha = 0.3f;

    return config;
}

bool audio_bridge_validate_config(const audio_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_bridge_validate_config: config is NULL");
        return false;
    }

    if (config->min_intensity_db < 0 || config->min_intensity_db > 120.0f) {
        return false;
    }

    if (config->min_frequency_hz <= 0 || config->max_frequency_hz <= config->min_frequency_hz) {
        return false;
    }

    if (config->detection_threshold < 0 || config->detection_threshold > 1.0f) {
        return false;
    }

    if (config->loc_mode > AUDIO_LOC_SPECTRAL) {
        return false;
    }

    if (config->ear_separation_m <= 0 || config->ear_separation_m > 1.0f) {
        return false;
    }

    if (config->speed_of_sound_mps < 300.0f || config->speed_of_sound_mps > 400.0f) {
        return false;
    }

    if (config->correlation_threshold < 0 || config->correlation_threshold > 1.0f) {
        return false;
    }

    if (config->max_angular_diff < 0 || config->max_angular_diff > M_PI) {
        return false;
    }

    if (config->smoothing_alpha < 0 || config->smoothing_alpha > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_audio_bridge_t* dragonfly_audio_bridge_create(
    dragonfly_system_t* dragonfly,
    audio_cortex_t* audio_cortex,
    const audio_bridge_config_t* config
) {
    dragonfly_audio_bridge_t* bridge = nimcp_calloc(1, sizeof(dragonfly_audio_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dragonfly_audio_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Store connected systems */
    bridge->dragonfly = dragonfly;
    bridge->audio_cortex = audio_cortex;

    /* Apply configuration */
    if (config) {
        if (!audio_bridge_validate_config(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_audio_bridge_create: invalid configuration");
            nimcp_free(bridge);
            return NULL;
        }
        bridge->config = *config;
    } else {
        bridge->config = audio_bridge_default_config();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "dragonfly_audio") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_audio_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize state */
    bridge->next_source_id = 1;
    bridge->has_smoothed_direction = false;
    bridge->initialized = true;

    return bridge;
}

void dragonfly_audio_bridge_destroy(dragonfly_audio_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_audio");

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int dragonfly_audio_bridge_reset(dragonfly_audio_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_audio_bridge_reset: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->latest_result, 0, sizeof(audio_detection_result_t));
    memset(bridge->source_history, 0, sizeof(bridge->source_history));
    bridge->num_active_sources = 0;

    memset(bridge->history_buffer, 0, sizeof(bridge->history_buffer));
    bridge->history_write_idx = 0;
    bridge->history_count = 0;

    bridge->has_smoothed_direction = false;
    bridge->smoothed_azimuth = 0;
    bridge->smoothed_elevation = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Calculate ITD from azimuth angle
 */
static float calculate_itd(float azimuth, float ear_separation, float speed_of_sound) {
    /* ITD = d * sin(azimuth) / c */
    return ear_separation * sinf(azimuth) / speed_of_sound;
}

/**
 * @brief Calculate azimuth from ITD
 */
static float calculate_azimuth_from_itd(float itd, float ear_separation, float speed_of_sound) {
    /* azimuth = asin(ITD * c / d) */
    float ratio = itd * speed_of_sound / ear_separation;
    if (ratio > 1.0f) ratio = 1.0f;
    if (ratio < -1.0f) ratio = -1.0f;
    return asinf(ratio);
}

/**
 * @brief Calculate ILD from azimuth (simplified model)
 */
static float calculate_ild_db(float azimuth) {
    /* Simplified ILD model: up to ~8dB difference for lateral sources */
    return 8.0f * sinf(azimuth);
}

/**
 * @brief Estimate distance from intensity drop
 */
static float estimate_distance_from_db(float intensity_db, float ref_db, float attenuation_exp) {
    /* dB drop = attenuation_exp * 10 * log10(distance) */
    /* At ref_db, distance = 1m. Quieter = farther. */
    float db_drop = ref_db - intensity_db;
    if (db_drop <= 0) return 1.0f;  /* At or louder than reference = 1m or closer */

    float log_dist = db_drop / (attenuation_exp * 10.0f);
    return powf(10.0f, log_dist);
}

/**
 * @brief Find or create source slot
 */
static int find_source_slot(dragonfly_audio_bridge_t* bridge, float azimuth, float elevation) {
    uint64_t now = get_time_us();
    uint64_t persistence_us = (uint64_t)(bridge->config.persistence_ms * 1000.0f);

    /* First try to find matching existing source */
    for (uint32_t i = 0; i < AUDIO_BRIDGE_MAX_SOURCES; i++) {
        if (bridge->source_history[i].active) {
            float az_diff = fabsf(bridge->source_history[i].source.azimuth - azimuth);
            float el_diff = fabsf(bridge->source_history[i].source.elevation - elevation);

            if (az_diff < 0.2f && el_diff < 0.2f) {
                return (int)i;
            }
        }
    }

    /* Find empty or expired slot */
    for (uint32_t i = 0; i < AUDIO_BRIDGE_MAX_SOURCES; i++) {
        if (!bridge->source_history[i].active) {
            return (int)i;
        }
        if (now - bridge->source_history[i].last_seen_us > persistence_us) {
            bridge->source_history[i].active = false;
            return (int)i;
        }
    }

    /* All slots full - replace oldest */
    uint64_t oldest_time = UINT64_MAX;
    int oldest_idx = 0;
    for (uint32_t i = 0; i < AUDIO_BRIDGE_MAX_SOURCES; i++) {
        if (bridge->source_history[i].last_seen_us < oldest_time) {
            oldest_time = bridge->source_history[i].last_seen_us;
            oldest_idx = (int)i;
        }
    }
    return oldest_idx;
}

/**
 * @brief Update source tracking
 */
static void update_source(dragonfly_audio_bridge_t* bridge, const audio_source_t* source) {
    int slot = find_source_slot(bridge, source->azimuth, source->elevation);
    if (slot < 0 || slot >= AUDIO_BRIDGE_MAX_SOURCES) return;

    source_history_t* hist = &bridge->source_history[slot];

    if (!hist->active) {
        /* New source */
        hist->source = *source;
        hist->source.source_id = bridge->next_source_id++;
        hist->active = true;
    } else {
        /* Update existing - smooth the values */
        float alpha = bridge->config.smoothing_alpha;
        hist->source.azimuth = alpha * source->azimuth + (1.0f - alpha) * hist->source.azimuth;
        hist->source.elevation = alpha * source->elevation + (1.0f - alpha) * hist->source.elevation;
        hist->source.intensity_db = alpha * source->intensity_db + (1.0f - alpha) * hist->source.intensity_db;
        hist->source.distance_est = alpha * source->distance_est + (1.0f - alpha) * hist->source.distance_est;
        hist->source.confidence = source->confidence;
        hist->source.frequency_hz = source->frequency_hz;
        hist->source.bandwidth_hz = source->bandwidth_hz;
        hist->source.timestamp_us = source->timestamp_us;
    }

    hist->last_seen_us = source->timestamp_us;

    /* Add to history buffer */
    bridge->history_buffer[bridge->history_write_idx] = hist->source;
    bridge->history_write_idx = (bridge->history_write_idx + 1) % AUDIO_BRIDGE_MAX_HISTORY;
    if (bridge->history_count < AUDIO_BRIDGE_MAX_HISTORY) {
        bridge->history_count++;
    }
}

/**
 * @brief Cue dragonfly attention to audio direction
 */
static void cue_dragonfly_attention(dragonfly_audio_bridge_t* bridge, float azimuth, float elevation, float priority) {
    if (!bridge->dragonfly || !bridge->config.enable_attention_cue) return;

    /* Convert to observation for dragonfly */
    float position[3];
    dragonfly_audio_bridge_angles_to_vector(azimuth, elevation, position);

    /* Scale by arbitrary distance (1m default) */
    position[0] *= 1.0f;
    position[1] *= 1.0f;
    position[2] *= 1.0f;

    /* Could call dragonfly_system_add_observation here if available */
    /* For now, this is a placeholder for integration */
}

//=============================================================================
// Processing Functions
//=============================================================================

int dragonfly_audio_bridge_process_frame(
    dragonfly_audio_bridge_t* bridge,
    const float* samples,
    uint32_t num_samples,
    uint32_t num_channels,
    uint32_t sample_rate
) {
    if (!bridge || !bridge->initialized || !samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_audio_bridge_process_frame: required parameter is NULL (bridge, bridge->initialized, samples)");
        return -1;
    }
    if (num_samples == 0 || num_channels == 0 || sample_rate == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_audio_bridge_process_frame: num_samples is zero");
        return -1;
    }

    uint64_t start_us = get_time_us();
    nimcp_mutex_lock(bridge->base.mutex);

    /* For stereo input, compute cross-correlation for ITD */
    if (num_channels == 2) {
        /* Simplified: find dominant peaks and compute ITD */
        float left_energy = 0, right_energy = 0;
        float cross_sum = 0;
        float left_peak = 0, right_peak = 0;

        for (uint32_t i = 0; i < num_samples; i++) {
            float left = samples[i * 2];
            float right = samples[i * 2 + 1];
            left_energy += left * left;
            right_energy += right * right;
            cross_sum += left * right;
            if (fabsf(left) > left_peak) left_peak = fabsf(left);
            if (fabsf(right) > right_peak) right_peak = fabsf(right);
        }

        /* Compute intensity */
        float intensity = sqrtf((left_energy + right_energy) / (2.0f * num_samples));
        float intensity_db = 20.0f * log10f(intensity + 1e-10f) + 94.0f;  /* Approx dB SPL */

        if (intensity_db >= bridge->config.min_intensity_db) {
            audio_source_t source = {0};
            source.timestamp_us = start_us;

            /* ITD-based azimuth estimation (simplified) */
            float ild = 0;
            if (left_peak > 0 && right_peak > 0) {
                ild = 20.0f * log10f(left_peak / right_peak);
            }

            /* Combine ITD and ILD */
            source.azimuth = ild * 0.05f;  /* Rough mapping */
            if (source.azimuth > M_PI / 2) source.azimuth = M_PI / 2;
            if (source.azimuth < -M_PI / 2) source.azimuth = -M_PI / 2;

            source.elevation = 0;  /* Cannot determine from stereo alone */
            source.intensity_db = intensity_db;
            {
                float db_range = 90.0f - bridge->config.min_intensity_db;
                if (db_range > 0.01f) {
                    source.confidence = 0.5f + 0.5f * (intensity_db - bridge->config.min_intensity_db) / db_range;
                } else {
                    source.confidence = 0.5f;
                }
            }
            if (source.confidence > 1.0f) source.confidence = 1.0f;

            /* Distance estimation */
            if (bridge->config.estimate_distance) {
                source.distance_est = estimate_distance_from_db(
                    intensity_db,
                    bridge->config.reference_db_at_1m,
                    bridge->config.attenuation_exp
                );
            }

            /* TODO: Frequency analysis would go here */
            source.frequency_hz = 1000.0f;  /* Placeholder */
            source.bandwidth_hz = 500.0f;

            update_source(bridge, &source);
            bridge->stats.sources_detected++;
        }
    }

    /* Update latest result */
    bridge->latest_result.timestamp_us = start_us;
    bridge->latest_result.num_sources = 0;

    for (uint32_t i = 0; i < AUDIO_BRIDGE_MAX_SOURCES; i++) {
        if (bridge->source_history[i].active) {
            uint32_t idx = bridge->latest_result.num_sources;
            if (idx < AUDIO_BRIDGE_MAX_SOURCES) {
                bridge->latest_result.sources[idx] = bridge->source_history[i].source;
                bridge->latest_result.num_sources++;
            }
        }
    }

    /* Find loudest source for cueing */
    if (bridge->latest_result.num_sources > 0) {
        float max_db = -100.0f;
        uint32_t max_idx = 0;
        for (uint32_t i = 0; i < bridge->latest_result.num_sources; i++) {
            if (bridge->latest_result.sources[i].intensity_db > max_db) {
                max_db = bridge->latest_result.sources[i].intensity_db;
                max_idx = i;
            }
        }

        bridge->latest_result.peak_azimuth = bridge->latest_result.sources[max_idx].azimuth;
        bridge->latest_result.peak_elevation = bridge->latest_result.sources[max_idx].elevation;

        /* Generate attention cue */
        if (bridge->config.enable_attention_cue) {
            float priority = bridge->latest_result.sources[max_idx].confidence;
            cue_dragonfly_attention(bridge,
                bridge->latest_result.peak_azimuth,
                bridge->latest_result.peak_elevation,
                priority);
            bridge->stats.cues_generated++;
        }
    }

    /* Update statistics */
    bridge->stats.frames_processed++;
    uint64_t elapsed = get_time_us() - start_us;
    bridge->process_time_sum += elapsed;
    bridge->stats.avg_process_time_us = (float)bridge->process_time_sum / bridge->stats.frames_processed;
    bridge->stats.avg_sources_per_frame = (float)bridge->stats.sources_detected / bridge->stats.frames_processed;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int dragonfly_audio_bridge_process_spectrum(
    dragonfly_audio_bridge_t* bridge,
    const float* left_spectrum,
    const float* right_spectrum,
    const float* left_phase,
    const float* right_phase,
    uint32_t num_bins,
    uint32_t sample_rate
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_audio_bridge_process_spectrum: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }
    if (!left_spectrum || !right_spectrum || num_bins == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_audio_bridge_process_spectrum: required parameter is NULL (left_spectrum, right_spectrum)");
        return -1;
    }

    uint64_t start_us = get_time_us();
    nimcp_mutex_lock(bridge->base.mutex);

    /* Find dominant frequency bin */
    uint32_t peak_bin = 0;
    float peak_mag = 0;
    float total_left = 0, total_right = 0;

    float freq_per_bin = (float)sample_rate / (2.0f * num_bins);

    for (uint32_t i = 0; i < num_bins; i++) {
        float freq = i * freq_per_bin;
        if (freq < bridge->config.min_frequency_hz) continue;
        if (freq > bridge->config.max_frequency_hz) break;

        float combined = left_spectrum[i] + right_spectrum[i];
        if (combined > peak_mag) {
            peak_mag = combined;
            peak_bin = i;
        }
        total_left += left_spectrum[i];
        total_right += right_spectrum[i];
    }

    if (peak_mag > 0) {
        audio_source_t source = {0};
        source.timestamp_us = start_us;

        /* ILD from spectrum magnitude */
        float ild_db = 0;
        if (right_spectrum[peak_bin] > 1e-10f) {
            ild_db = 20.0f * log10f(left_spectrum[peak_bin] / right_spectrum[peak_bin]);
        }

        /* ITD from phase difference (if available) */
        float phase_diff = 0;
        if (left_phase && right_phase) {
            phase_diff = left_phase[peak_bin] - right_phase[peak_bin];
            /* Wrap to [-pi, pi] */
            while (phase_diff > M_PI) phase_diff -= 2 * M_PI;
            while (phase_diff < -M_PI) phase_diff += 2 * M_PI;
        }

        /* Combine ITD and ILD for azimuth */
        float azimuth_ild = ild_db * 0.05f;
        float freq = peak_bin * freq_per_bin;
        float azimuth_itd = 0;
        if (freq > 0 && left_phase && right_phase) {
            float itd = phase_diff / (2.0f * M_PI * freq);
            azimuth_itd = calculate_azimuth_from_itd(itd,
                bridge->config.ear_separation_m,
                bridge->config.speed_of_sound_mps);
        }

        /* Weighted combination based on mode */
        switch (bridge->config.loc_mode) {
            case AUDIO_LOC_ITD:
                source.azimuth = azimuth_itd;
                break;
            case AUDIO_LOC_ILD:
                source.azimuth = azimuth_ild;
                break;
            case AUDIO_LOC_COMBINED:
            default:
                source.azimuth = 0.5f * azimuth_itd + 0.5f * azimuth_ild;
                break;
            case AUDIO_LOC_SPECTRAL:
                source.azimuth = azimuth_ild;  /* HRTF would go here */
                break;
        }

        /* Clamp azimuth */
        if (source.azimuth > M_PI / 2) source.azimuth = M_PI / 2;
        if (source.azimuth < -M_PI / 2) source.azimuth = -M_PI / 2;

        source.elevation = 0;
        source.frequency_hz = freq;
        source.intensity_db = 20.0f * log10f(peak_mag + 1e-10f);
        source.confidence = peak_mag / (total_left + total_right + 1e-10f);

        if (bridge->config.estimate_distance) {
            source.distance_est = estimate_distance_from_db(
                source.intensity_db,
                bridge->config.reference_db_at_1m,
                bridge->config.attenuation_exp
            );
        }

        update_source(bridge, &source);
        bridge->stats.sources_detected++;
    }

    bridge->stats.frames_processed++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int dragonfly_audio_bridge_inject_source(
    dragonfly_audio_bridge_t* bridge,
    const audio_source_t* source
) {
    if (!bridge || !bridge->initialized || !source) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_audio_bridge_inject_source: required parameter is NULL (bridge, bridge->initialized, source)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    audio_source_t src = *source;
    if (src.timestamp_us == 0) {
        src.timestamp_us = get_time_us();
    }

    update_source(bridge, &src);
    bridge->stats.sources_detected++;

    /* Update latest result immediately */
    bridge->latest_result.timestamp_us = src.timestamp_us;
    bridge->latest_result.num_sources = 0;

    for (uint32_t i = 0; i < AUDIO_BRIDGE_MAX_SOURCES; i++) {
        if (bridge->source_history[i].active) {
            uint32_t idx = bridge->latest_result.num_sources;
            if (idx < AUDIO_BRIDGE_MAX_SOURCES) {
                bridge->latest_result.sources[idx] = bridge->source_history[i].source;
                bridge->latest_result.num_sources++;
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int dragonfly_audio_bridge_get_result(
    const dragonfly_audio_bridge_t* bridge,
    audio_detection_result_t* result
) {
    if (!bridge || !bridge->initialized || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_audio_bridge_get_result: required parameter is NULL (bridge, bridge->initialized, result)");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_audio_bridge_t*)bridge)->base.mutex);
    *result = bridge->latest_result;
    nimcp_mutex_unlock(((dragonfly_audio_bridge_t*)bridge)->base.mutex);

    return 0;
}

//=============================================================================
// Multi-Modal Fusion Functions
//=============================================================================

int dragonfly_audio_bridge_correlate_visual(
    dragonfly_audio_bridge_t* bridge,
    const void* visual_result_ptr,
    audio_visual_correlation_t* correlations,
    uint32_t max_correlations
) {
    if (!bridge || !bridge->initialized || !correlations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_audio_bridge_correlate_visual: required parameter is NULL (bridge, bridge->initialized, correlations)");
        return -1;
    }
    if (!visual_result_ptr || max_correlations == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_audio_bridge_correlate_visual: visual_result_ptr is NULL");
        return -1;
    }

    const visual_motion_result_t* visual = (const visual_motion_result_t*)visual_result_ptr;

    nimcp_mutex_lock(bridge->base.mutex);

    int num_found = 0;
    uint64_t now = get_time_us();

    for (uint32_t a = 0; a < bridge->latest_result.num_sources && num_found < (int)max_correlations; a++) {
        const audio_source_t* audio_src = &bridge->latest_result.sources[a];

        /* Convert audio direction to pseudo-pixel coordinates for comparison */
        /* Assuming camera FOV of ~60 degrees */
        float audio_norm_x = audio_src->azimuth / (M_PI / 3.0f);  /* Normalize to [-1, 1] */
        float audio_norm_y = audio_src->elevation / (M_PI / 6.0f);

        for (uint32_t v = 0; v < visual->num_blobs; v++) {
            /* Normalize visual position to [-1, 1] assuming center is origin */
            float visual_norm_x = 0;  /* Would need calibration data */
            float visual_norm_y = 0;

            float angular_diff = sqrtf(
                (audio_norm_x - visual_norm_x) * (audio_norm_x - visual_norm_x) +
                (audio_norm_y - visual_norm_y) * (audio_norm_y - visual_norm_y)
            );

            /* Temporal difference */
            float temporal_diff_ms = fabsf((float)(now - visual->timestamp_us)) / 1000.0f;

            if (angular_diff <= bridge->config.max_angular_diff &&
                temporal_diff_ms <= bridge->config.max_temporal_diff_ms) {

                float score = 1.0f - (angular_diff / bridge->config.max_angular_diff) * 0.5f;
                score -= (temporal_diff_ms / bridge->config.max_temporal_diff_ms) * 0.5f;
                if (score < 0) score = 0;

                if (score >= bridge->config.correlation_threshold) {
                    correlations[num_found].audio_source_id = audio_src->source_id;
                    correlations[num_found].visual_target_id = visual->blobs[v].track_id;
                    correlations[num_found].correlation_score = score;
                    correlations[num_found].angular_difference = angular_diff;
                    correlations[num_found].temporal_offset_ms = temporal_diff_ms;
                    correlations[num_found].is_matched = true;
                    num_found++;
                    bridge->stats.correlations_found++;
                }
            }
        }
    }

    /* Update correlation success rate */
    if (bridge->stats.frames_processed > 0) {
        bridge->stats.correlation_success_rate =
            (float)bridge->stats.correlations_found / bridge->stats.frames_processed;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return num_found;
}

int dragonfly_audio_bridge_get_attention_cue(
    const dragonfly_audio_bridge_t* bridge,
    float cue_direction[2],
    float* cue_priority
) {
    if (!bridge || !bridge->initialized || !cue_direction || !cue_priority) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_audio_bridge_get_attention_cue: required parameter is NULL (bridge, bridge->initialized, cue_direction, cue_priority)");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_audio_bridge_t*)bridge)->base.mutex);

    if (bridge->latest_result.num_sources == 0) {
        nimcp_mutex_unlock(((dragonfly_audio_bridge_t*)bridge)->base.mutex);
        return 1;  /* No cue available */
    }

    /* Find highest priority source */
    float max_priority = 0;
    float best_az = 0, best_el = 0;

    for (uint32_t i = 0; i < bridge->latest_result.num_sources; i++) {
        float priority = bridge->latest_result.sources[i].confidence *
                        (bridge->latest_result.sources[i].intensity_db / 90.0f);
        if (priority > max_priority) {
            max_priority = priority;
            best_az = bridge->latest_result.sources[i].azimuth;
            best_el = bridge->latest_result.sources[i].elevation;
        }
    }

    cue_direction[0] = best_az;
    cue_direction[1] = best_el;
    *cue_priority = max_priority + bridge->config.cue_priority_boost;
    if (*cue_priority > 1.0f) *cue_priority = 1.0f;

    nimcp_mutex_unlock(((dragonfly_audio_bridge_t*)bridge)->base.mutex);
    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

void dragonfly_audio_bridge_angles_to_vector(
    float azimuth,
    float elevation,
    float direction[3]
) {
    /* Spherical to Cartesian */
    float cos_el = cosf(elevation);
    direction[0] = cos_el * sinf(azimuth);   /* x: right */
    direction[1] = sinf(elevation);           /* y: up */
    direction[2] = cos_el * cosf(azimuth);   /* z: forward */
}

float dragonfly_audio_bridge_estimate_distance(
    const dragonfly_audio_bridge_t* bridge,
    float intensity_db
) {
    if (!bridge) return -1.0f;
    return estimate_distance_from_db(
        intensity_db,
        bridge->config.reference_db_at_1m,
        bridge->config.attenuation_exp
    );
}

float dragonfly_audio_bridge_azimuth_to_itd(
    const dragonfly_audio_bridge_t* bridge,
    float azimuth
) {
    if (!bridge) return 0;
    return calculate_itd(azimuth,
        bridge->config.ear_separation_m,
        bridge->config.speed_of_sound_mps);
}

//=============================================================================
// Statistics Functions
//=============================================================================

int dragonfly_audio_bridge_get_stats(
    const dragonfly_audio_bridge_t* bridge,
    audio_bridge_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_audio_bridge_get_stats: required parameter is NULL (bridge, bridge->initialized, stats)");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_audio_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((dragonfly_audio_bridge_t*)bridge)->base.mutex);

    return 0;
}

int dragonfly_audio_bridge_reset_stats(dragonfly_audio_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_audio_bridge_reset_stats: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(audio_bridge_stats_t));
    bridge->process_time_sum = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Configuration Update
//=============================================================================

int dragonfly_audio_bridge_set_config(
    dragonfly_audio_bridge_t* bridge,
    const audio_bridge_config_t* config
) {
    if (!bridge || !bridge->initialized || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_audio_bridge_set_config: required parameter is NULL (bridge, bridge->initialized, config)");
        return -1;
    }
    if (!audio_bridge_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_audio_bridge_set_config: audio_bridge_validate_config is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_audio_bridge_get_config(
    const dragonfly_audio_bridge_t* bridge,
    audio_bridge_config_t* config
) {
    if (!bridge || !bridge->initialized || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_audio_bridge_get_config: required parameter is NULL (bridge, bridge->initialized, config)");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_audio_bridge_t*)bridge)->base.mutex);
    *config = bridge->config;
    nimcp_mutex_unlock(((dragonfly_audio_bridge_t*)bridge)->base.mutex);

    return 0;
}

const char* dragonfly_audio_localization_mode_name(audio_localization_mode_t mode) {
    switch (mode) {
        case AUDIO_LOC_ITD: return "ITD";
        case AUDIO_LOC_ILD: return "ILD";
        case AUDIO_LOC_COMBINED: return "Combined";
        case AUDIO_LOC_SPECTRAL: return "Spectral";
        default: return "Unknown";
    }
}
