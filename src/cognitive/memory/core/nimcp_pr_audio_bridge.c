//=============================================================================
// nimcp_pr_audio_bridge.c - Prime Resonant Audio Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_audio_bridge.c
 * @brief Implementation of audio-PR memory integration bridge
 * @version 1.0.0
 * @date 2026-01-09
 *
 * Implements the bridge connecting cochlea, audio cortex, and FEP to the
 * Prime Resonant memory system. Handles audio feature extraction, prime
 * signature generation, quaternion state computation, and memory encoding.
 */

#include "cognitive/memory/core/nimcp_pr_audio_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_audio_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_audio_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_audio_bridge_mesh_registry = NULL;

nimcp_error_t pr_audio_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_audio_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_audio_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_audio_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_audio_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_audio_bridge_mesh_registry = registry;
    return err;
}

void pr_audio_bridge_mesh_unregister(void) {
    if (g_pr_audio_bridge_mesh_registry && g_pr_audio_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_audio_bridge_mesh_registry, g_pr_audio_bridge_mesh_id);
        g_pr_audio_bridge_mesh_id = 0;
        g_pr_audio_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_audio_bridge module (instance-level) */
static inline void pr_audio_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_audio_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_audio_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_audio_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_AUDIO_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

/** Pi if not defined */

/** Two times Pi */
#ifndef NIMCP_TWO_PI_F
#endif

/** Internal buffer alignment */
#define PR_AUDIO_ALIGN 64

/** Maximum tempo for emotion mapping (BPM) */
#define PR_AUDIO_MAX_TEMPO 200.0f

/** Minimum tempo for emotion mapping (BPM) */
#define PR_AUDIO_MIN_TEMPO 40.0f

/** Maximum brightness for emotion mapping (Hz) */
#define PR_AUDIO_MAX_BRIGHTNESS 8000.0f

/** Minimum brightness for emotion mapping (Hz) */
#define PR_AUDIO_MIN_BRIGHTNESS 500.0f

//=============================================================================
// Static Function Prototypes
//=============================================================================

static float normalize_range(float val, float min_val, float max_val);
static uint64_t get_timestamp_ms(void);
static uint32_t float_to_bin(float val, float min_val, float max_val, uint32_t num_bins);
static float compute_interval_regularity(const float* intervals, uint32_t count);
static void update_history_buffer(float* buffer, uint32_t* pos, uint32_t len,
                                   const float* new_data, uint32_t new_count);

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT pr_audio_bridge_config_t pr_audio_bridge_config_default(void) {
    pr_audio_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    /* Sample processing */
    config.sample_rate = 16000;
    config.frame_size = 512;
    config.num_mel_filters = 40;
    config.num_mfcc = 13;

    /* History buffers */
    config.mfcc_history_len = PR_AUDIO_DEFAULT_HISTORY_LEN;
    config.onset_history_len = PR_AUDIO_DEFAULT_ONSET_HISTORY;

    /* Sub-configurations */
    config.sig_config = pr_audio_sig_config_default();
    config.quat_config = pr_audio_quat_config_default();

    /* Encoding parameters */
    config.encoding_threshold = 0.3f;
    config.retrieval_threshold = 0.5f;
    config.auto_entangle = true;
    config.entangle_threshold = 0.7f;

    /* Theta-gamma */
    config.use_theta_gating = true;
    config.use_gamma_boost = true;

    /* FEP */
    config.use_prediction_error = true;
    config.pe_salience_factor = 0.5f;

    /* Default tier */
    config.default_tier = PR_MEMORY_TIER_Z0;

    return config;
}

NIMCP_EXPORT pr_audio_sig_config_t pr_audio_sig_config_default(void) {
    pr_audio_sig_config_t config;
    memset(&config, 0, sizeof(config));

    /* MFCC config */
    config.num_mfcc = 13;
    config.mfcc_bins = PR_AUDIO_MFCC_BINS;
    config.use_delta_mfcc = true;
    config.use_delta_delta_mfcc = false;

    /* Spectral weights */
    config.centroid_weight = 1.0f;
    config.spread_weight = 0.8f;
    config.flux_weight = 0.6f;
    config.rolloff_weight = 0.5f;

    /* Temporal weights */
    config.onset_weight = 0.7f;
    config.rhythm_weight = 0.6f;

    /* Normalization */
    config.normalize_features = true;
    config.sparsity_target = 0.4f;

    return config;
}

NIMCP_EXPORT pr_audio_quat_config_t pr_audio_quat_config_default(void) {
    pr_audio_quat_config_t config;
    memset(&config, 0, sizeof(config));

    /* Consolidation (w) */
    config.repetition_decay = NIMCP_EMA_DECAY_FAST;
    config.coherence_weight = 0.3f;

    /* Emotion (x) */
    config.tempo_influence = 0.4f;
    config.brightness_influence = 0.3f;
    config.mode_influence = 0.3f;
    config.neutral_tempo_bpm = PR_AUDIO_NEUTRAL_TEMPO;
    config.neutral_brightness_hz = PR_AUDIO_NEUTRAL_BRIGHTNESS;

    /* Salience (y) */
    config.onset_salience_weight = 0.4f;
    config.loudness_salience_weight = 0.3f;
    config.contrast_salience_weight = 0.2f;
    config.novelty_salience_weight = 0.1f;

    /* Accessibility (z) */
    config.familiarity_weight = 0.5f;
    config.recency_weight = 0.5f;
    config.access_decay_rate = 0.95f;

    return config;
}

NIMCP_EXPORT bool pr_audio_bridge_config_validate(
    const pr_audio_bridge_config_t* config) {

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_audio_quat_config_default: config is NULL");
        return false;
    }

    /* Sample rate */
    if (config->sample_rate < 8000 || config->sample_rate > 96000) {
        return false;
    }

    /* Frame size */
    if (config->frame_size < 64 || config->frame_size > 4096) {
        return false;
    }

    /* MFCC count */
    if (config->num_mfcc < 1 || config->num_mfcc > PR_AUDIO_MAX_MFCC) {
        return false;
    }

    /* History lengths */
    if (config->mfcc_history_len < 1 ||
        config->mfcc_history_len > PR_AUDIO_MAX_HISTORY_FRAMES) {
        return false;
    }

    /* Thresholds */
    if (config->encoding_threshold < 0.0f || config->encoding_threshold > 1.0f) {
        return false;
    }

    if (config->retrieval_threshold < 0.0f || config->retrieval_threshold > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT pr_audio_bridge_t* pr_audio_bridge_create(
    const pr_audio_bridge_config_t* config,
    pr_node_manager_t node_manager) {

    /* Use defaults if config is NULL */
    pr_audio_bridge_config_t effective_config;
    if (config) {
        effective_config = *config;
    } else {
        effective_config = pr_audio_bridge_config_default();
    }

    /* Validate configuration */
    if (!pr_audio_bridge_config_validate(&effective_config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_audio_quat_config_default: pr_audio_bridge_config_validate is NULL");
        return NULL;
    }

    /* Allocate bridge structure */
    pr_audio_bridge_t* bridge = (pr_audio_bridge_t*)nimcp_calloc(1, sizeof(pr_audio_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Store configuration */
    bridge->config = effective_config;
    bridge->sig_config = effective_config.sig_config;
    bridge->quat_config = effective_config.quat_config;
    bridge->num_mfcc = effective_config.num_mfcc;

    /* Store node manager */
    bridge->node_manager = node_manager;

    /* Allocate MFCC history buffer */
    bridge->mfcc_history_len = effective_config.mfcc_history_len;
    size_t mfcc_buffer_size = bridge->mfcc_history_len * bridge->num_mfcc * sizeof(float);
    bridge->mfcc_history = (float*)nimcp_calloc(1, mfcc_buffer_size);
    if (!bridge->mfcc_history) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_audio_quat_config_default: bridge->mfcc_history is NULL");
        return NULL;
    }
    bridge->mfcc_history_pos = 0;

    /* Allocate onset history buffer */
    bridge->onset_history_len = effective_config.onset_history_len;
    bridge->onset_history = (float*)nimcp_calloc(bridge->onset_history_len, sizeof(float));
    if (!bridge->onset_history) {
        nimcp_free(bridge->mfcc_history);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_audio_quat_config_default: bridge->onset_history is NULL");
        return NULL;
    }
    bridge->onset_history_pos = 0;
    bridge->last_onset_time_ms = 0;

    /* Allocate current signature */
    bridge->current_signature = prime_sig_create();
    if (!bridge->current_signature) {
        nimcp_free(bridge->onset_history);
        nimcp_free(bridge->mfcc_history);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_audio_quat_config_default: bridge->current_signature is NULL");
        return NULL;
    }

    /* Initialize quaternion to neutral state */
    bridge->current_audio_quat = quat_create(0.5f, 0.0f, 0.3f, 0.5f);

    /* Initialize features */
    memset(&bridge->current_features, 0, sizeof(bridge->current_features));
    memset(&bridge->current_pattern, 0, sizeof(bridge->current_pattern));

    /* Initialize state */
    bridge->repetition_count = 0;
    bridge->familiarity_score = 0.0f;
    bridge->current_prediction_error = 0.0f;
    bridge->temporal_prediction_accuracy = 0.5f;

    /* Clear statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Set state flags */
    bridge->initialized = true;
    bridge->cochlea_connected = false;
    bridge->cortex_connected = false;
    bridge->fep_connected = false;

    /* Audio components not connected yet */
    bridge->cochlea = NULL;
    bridge->audio_cortex = NULL;
    bridge->fep_bridge = NULL;
    bridge->theta_gamma = NULL;
    bridge->current_audio_memory = NULL;

    return bridge;
}

NIMCP_EXPORT void pr_audio_bridge_destroy(pr_audio_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_audio");

    /* Free signature */
    if (bridge->current_signature) {
        prime_sig_destroy(bridge->current_signature);
        bridge->current_signature = NULL;
    }

    /* Free history buffers */
    if (bridge->mfcc_history) {
        nimcp_free(bridge->mfcc_history);
        bridge->mfcc_history = NULL;
    }

    if (bridge->onset_history) {
        nimcp_free(bridge->onset_history);
        bridge->onset_history = NULL;
    }

    /* Note: We do NOT destroy connected components - they are owned externally */
    bridge->cochlea = NULL;
    bridge->audio_cortex = NULL;
    bridge->fep_bridge = NULL;
    bridge->theta_gamma = NULL;

    bridge->initialized = false;

    nimcp_free(bridge);
    bridge = NULL;
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_reset(pr_audio_bridge_t* bridge) {
    if (!bridge) return PR_AUDIO_ERROR_NULL_POINTER;

    /* Clear history buffers */
    if (bridge->mfcc_history) {
        memset(bridge->mfcc_history, 0,
               bridge->mfcc_history_len * bridge->num_mfcc * sizeof(float));
        bridge->mfcc_history_pos = 0;
    }

    if (bridge->onset_history) {
        memset(bridge->onset_history, 0, bridge->onset_history_len * sizeof(float));
        bridge->onset_history_pos = 0;
    }

    /* Reset signature */
    if (bridge->current_signature) {
        memset(bridge->current_signature->exponents, 0, PRIME_SIG_DIM);
        bridge->current_signature->num_factors = 0;
        bridge->current_signature->hash = 0;
    }

    /* Reset quaternion */
    bridge->current_audio_quat = quat_create(0.5f, 0.0f, 0.3f, 0.5f);

    /* Reset features and pattern */
    memset(&bridge->current_features, 0, sizeof(bridge->current_features));
    memset(&bridge->current_pattern, 0, sizeof(bridge->current_pattern));

    /* Reset state */
    bridge->repetition_count = 0;
    bridge->familiarity_score = 0.0f;
    bridge->current_prediction_error = 0.0f;
    bridge->temporal_prediction_accuracy = 0.5f;
    bridge->last_onset_time_ms = 0;
    bridge->current_audio_memory = NULL;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return PR_AUDIO_SUCCESS;
}

//=============================================================================
// Connection Functions
//=============================================================================

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_connect_cochlea(
    pr_audio_bridge_t* bridge,
    cochlea_t* cochlea) {

    if (!bridge) return PR_AUDIO_ERROR_NULL_POINTER;
    if (!cochlea) return PR_AUDIO_ERROR_NULL_POINTER;

    bridge->cochlea = cochlea;
    bridge->cochlea_connected = true;

    return PR_AUDIO_SUCCESS;
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_connect_audio_cortex(
    pr_audio_bridge_t* bridge,
    audio_cortex_t* cortex) {

    if (!bridge) return PR_AUDIO_ERROR_NULL_POINTER;
    if (!cortex) return PR_AUDIO_ERROR_NULL_POINTER;

    bridge->audio_cortex = cortex;
    bridge->cortex_connected = true;

    return PR_AUDIO_SUCCESS;
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_connect_fep(
    pr_audio_bridge_t* bridge,
    audio_cortex_fep_bridge_t* fep_bridge) {

    if (!bridge) return PR_AUDIO_ERROR_NULL_POINTER;
    if (!fep_bridge) return PR_AUDIO_ERROR_NULL_POINTER;

    bridge->fep_bridge = fep_bridge;
    bridge->fep_connected = true;

    return PR_AUDIO_SUCCESS;
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_connect_theta_gamma(
    pr_audio_bridge_t* bridge,
    theta_gamma_manager_t theta_gamma) {

    if (!bridge) return PR_AUDIO_ERROR_NULL_POINTER;
    if (!theta_gamma) return PR_AUDIO_ERROR_NULL_POINTER;

    bridge->theta_gamma = theta_gamma;

    return PR_AUDIO_SUCCESS;
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_connect_entanglement(
    pr_audio_bridge_t* bridge,
    entangle_graph_t graph) {

    if (!bridge) return PR_AUDIO_ERROR_NULL_POINTER;

    bridge->audio_entanglement = graph;

    return PR_AUDIO_SUCCESS;
}

//=============================================================================
// Main Processing Functions
//=============================================================================

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_process_frame(
    pr_audio_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples) {

    if (!bridge) return PR_AUDIO_ERROR_NULL_POINTER;
    if (!audio_data) return PR_AUDIO_ERROR_NULL_POINTER;
    if (num_samples == 0) return PR_AUDIO_ERROR_INVALID_AUDIO;

    pr_audio_error_t err;
    uint64_t start_time = get_timestamp_ms();

    /* Step 1: Extract audio features */
    err = pr_audio_bridge_extract_features(bridge, audio_data, num_samples,
                                            &bridge->current_features);
    if (err != PR_AUDIO_SUCCESS) {
        return err;
    }

    /* Step 2: Compute prime signature from features */
    err = pr_audio_bridge_compute_audio_prime_sig(bridge,
                                                    &bridge->current_features,
                                                    bridge->current_signature);
    if (err != PR_AUDIO_SUCCESS) {
        return err;
    }

    /* Step 3: Compute quaternion semantic state */
    err = pr_audio_bridge_compute_audio_quaternion(bridge,
                                                     &bridge->current_features,
                                                     &bridge->current_audio_quat);
    if (err != PR_AUDIO_SUCCESS) {
        return err;
    }

    /* Step 4: Detect temporal patterns */
    err = pr_audio_bridge_detect_temporal_pattern(bridge, &bridge->current_pattern);
    /* Pattern detection failure is non-fatal */

    /* Step 5: Update from FEP if connected */
    if (bridge->fep_connected && bridge->config.use_prediction_error) {
        pr_audio_bridge_update_from_fep(bridge);
    }

    /* Step 6: Check theta phase for encoding */
    bool should_encode = true;
    if (bridge->theta_gamma && bridge->config.use_theta_gating) {
        should_encode = pr_audio_bridge_can_encode(bridge);
        if (!should_encode) {
            bridge->stats.blocked_by_phase++;
        }
    }

    /* Step 7: Encode to memory if appropriate */
    if (should_encode) {
        /* Check salience threshold */
        float salience = bridge->current_audio_quat.y;
        if (salience >= bridge->config.encoding_threshold) {
            pr_memory_node_t* new_memory = NULL;
            err = pr_audio_bridge_encode_to_memory(bridge,
                                                     PR_AUDIO_TYPE_UNKNOWN,
                                                     &new_memory);
            if (err == PR_AUDIO_SUCCESS && new_memory) {
                /* Auto-entangle with similar memories */
                if (bridge->config.auto_entangle) {
                    pr_audio_bridge_auto_entangle(bridge, new_memory);
                }
            }
        }
        bridge->stats.encoding_windows++;
    } else {
        bridge->stats.retrieval_windows++;
    }

    /* Update frame count and timing stats */
    bridge->stats.frames_processed++;

    uint64_t end_time = get_timestamp_ms();
    float frame_time = (float)(end_time - start_time);
    bridge->stats.avg_frame_time_us = (bridge->stats.avg_frame_time_us *
                                        (bridge->stats.frames_processed - 1) +
                                        frame_time * 1000.0f) /
                                       bridge->stats.frames_processed;

    return PR_AUDIO_SUCCESS;
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_extract_features(
    pr_audio_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    pr_audio_features_t* features) {

    if (!bridge || !audio_data || !features) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    memset(features, 0, sizeof(pr_audio_features_t));
    features->timestamp_ms = get_timestamp_ms();

    /* Compute RMS energy */
    float sum_sq = 0.0f;
    float peak = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_samples > 256) {
            pr_audio_bridge_heartbeat("pr_audio_bri_loop",
                             (float)(i + 1) / (float)num_samples);
        }

        float sample = audio_data[i];
        sum_sq += sample * sample;
        float abs_sample = fabsf(sample);
        if (abs_sample > peak) {
            peak = abs_sample;
        }
    }
    features->rms_energy = sqrtf(sum_sq / (fabsf(num_samples) > 1e-7f ? num_samples : 1e-7f));
    features->peak_energy = peak;
    features->loudness_db = 20.0f * log10f(features->rms_energy + PR_AUDIO_EPSILON);

    /* Simple spectral analysis using zero-crossing rate as proxy */
    uint32_t zero_crossings = 0;
    for (uint32_t i = 1; i < num_samples; i++) {
        if ((audio_data[i-1] >= 0 && audio_data[i] < 0) ||
            (audio_data[i-1] < 0 && audio_data[i] >= 0)) {
            zero_crossings++;
        }
    }

    /* Estimate spectral centroid from zero-crossing rate */
    float zcr = (float)zero_crossings / (float)num_samples;
    features->spectral_centroid = zcr * bridge->config.sample_rate * 0.5f;

    /* Spectral spread as variance estimate */
    features->spectral_spread = features->spectral_centroid * 0.4f;

    /* Spectral flux from energy change */
    static _Thread_local float last_energy = 0.0f;
    features->spectral_flux = fabsf(features->rms_energy - last_energy);
    last_energy = features->rms_energy;

    /* Spectral rolloff estimate */
    features->spectral_rolloff = features->spectral_centroid * 1.5f;

    /* Brightness */
    features->brightness = features->spectral_centroid / 4000.0f;
    features->brightness = nimcp_myelin_clamp(features->brightness, 0.0f, 1.0f);

    /* Onset detection based on energy change */
    features->onset_strength = nimcp_myelin_clamp(features->spectral_flux * 10.0f, 0.0f, 1.0f);

    /* Update onset history if onset detected */
    if (features->onset_strength > PR_AUDIO_ONSET_THRESHOLD) {
        uint64_t current_time = features->timestamp_ms;
        if (bridge->last_onset_time_ms > 0) {
            float interval_ms = (float)(current_time - bridge->last_onset_time_ms);
            if (interval_ms > 0 && interval_ms < 5000.0f) {
                /* Add to onset history */
                bridge->onset_history[bridge->onset_history_pos] = interval_ms;
                bridge->onset_history_pos =
                    (bridge->onset_history_pos + 1) % bridge->onset_history_len;
            }
        }
        bridge->last_onset_time_ms = current_time;
    }

    /* Estimate tempo from onset intervals */
    float interval_sum = 0.0f;
    uint32_t valid_intervals = 0;
    for (uint32_t i = 0; i < bridge->onset_history_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->onset_history_len > 256) {
            pr_audio_bridge_heartbeat("pr_audio_bri_loop",
                             (float)(i + 1) / (float)bridge->onset_history_len);
        }

        if (bridge->onset_history[i] > 50.0f &&
            bridge->onset_history[i] < 2000.0f) {
            interval_sum += bridge->onset_history[i];
            valid_intervals++;
        }
    }

    if (valid_intervals > 2) {
        float mean_interval_ms = interval_sum / valid_intervals;
        features->tempo_estimate_bpm = 60000.0f / mean_interval_ms;
        features->tempo_estimate_bpm = nimcp_myelin_clamp(features->tempo_estimate_bpm,
                                                    PR_AUDIO_MIN_TEMPO,
                                                    PR_AUDIO_MAX_TEMPO);
    } else {
        features->tempo_estimate_bpm = PR_AUDIO_NEUTRAL_TEMPO;
    }

    /* Generate synthetic MFCC-like features based on spectral characteristics */
    features->num_mfcc = bridge->num_mfcc;
    for (uint32_t i = 0; i < bridge->num_mfcc && i < PR_AUDIO_MAX_MFCC; i++) {
        /* Create pseudo-MFCCs based on spectral centroid bands */
        float band_center = features->spectral_centroid * (1.0f + (float)i * 0.1f);
        float band_energy = features->rms_energy *
                            expf(-fabsf(band_center - features->spectral_centroid) /
                                 (features->spectral_spread + PR_AUDIO_EPSILON));
        features->mfcc[i] = band_energy;

        /* Delta MFCC (approximation) */
        if (bridge->mfcc_history && bridge->stats.frames_processed > 0) {
            uint32_t prev_pos = (bridge->mfcc_history_pos +
                                 bridge->mfcc_history_len - 1) %
                                bridge->mfcc_history_len;
            float prev_mfcc = bridge->mfcc_history[prev_pos * bridge->num_mfcc + i];
            features->delta_mfcc[i] = features->mfcc[i] - prev_mfcc;
        }
    }

    /* Update MFCC history */
    if (bridge->mfcc_history) {
        uint32_t offset = bridge->mfcc_history_pos * bridge->num_mfcc;
        for (uint32_t i = 0; i < bridge->num_mfcc; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_mfcc > 256) {
                pr_audio_bridge_heartbeat("pr_audio_bri_loop",
                                 (float)(i + 1) / (float)bridge->num_mfcc);
            }

            bridge->mfcc_history[offset + i] = features->mfcc[i];
        }
        bridge->mfcc_history_pos =
            (bridge->mfcc_history_pos + 1) % bridge->mfcc_history_len;
    }

    /* Spectral contrast */
    features->spectral_contrast = features->brightness;

    /* Pitch estimation (simple autocorrelation-based approach) */
    features->pitch_estimate_hz = 0.0f;  /* Not implemented without FFT */

    /* Roughness estimation */
    features->roughness = features->spectral_flux * 0.5f;

    features->valid = true;

    return PR_AUDIO_SUCCESS;
}

//=============================================================================
// Prime Signature Functions
//=============================================================================

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_compute_audio_prime_sig(
    pr_audio_bridge_t* bridge,
    const pr_audio_features_t* features,
    prime_signature_t* signature) {

    if (!bridge || !features || !signature) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    if (!features->valid) {
        return PR_AUDIO_ERROR_INVALID_AUDIO;
    }

    /* Clear signature */
    memset(signature->exponents, 0, PRIME_SIG_DIM);

    /* Map MFCC coefficients to prime indices [0-31] */
    for (uint32_t i = 0; i < features->num_mfcc && i < 32; i++) {
        float mfcc_val = features->mfcc[i];

        /* Normalize to [0, 1] range (assuming mfcc is in [-1, 1] approx) */
        float normalized = (mfcc_val + 1.0f) * 0.5f;
        normalized = nimcp_myelin_clamp(normalized, 0.0f, 1.0f);

        /* Quantize to bins */
        uint32_t bin = float_to_bin(normalized, 0.0f, 1.0f,
                                     bridge->sig_config.mfcc_bins);

        /* Map to prime exponent */
        uint32_t prime_idx = i;
        uint8_t current = signature->exponents[prime_idx];
        signature->exponents[prime_idx] = (uint8_t)(current + bin + 1);
    }

    /* Map spectral centroid to primes [PR_AUDIO_PRIME_CENTROID_START...] */
    {
        float normalized = normalize_range(features->spectral_centroid,
                                            PR_AUDIO_MIN_BRIGHTNESS,
                                            PR_AUDIO_MAX_BRIGHTNESS);
        uint32_t bin = float_to_bin(normalized, 0.0f, 1.0f, 8);
        uint32_t idx = PR_AUDIO_PRIME_CENTROID_START + bin;
        if (idx < PRIME_SIG_DIM) {
            signature->exponents[idx] += (uint8_t)(bridge->sig_config.centroid_weight * 16);
        }
    }

    /* Map spectral spread to primes */
    {
        float normalized = normalize_range(features->spectral_spread, 100.0f, 2000.0f);
        uint32_t bin = float_to_bin(normalized, 0.0f, 1.0f, 8);
        uint32_t idx = PR_AUDIO_PRIME_SPREAD_START + bin;
        if (idx < PRIME_SIG_DIM) {
            signature->exponents[idx] += (uint8_t)(bridge->sig_config.spread_weight * 16);
        }
    }

    /* Map spectral flux to primes */
    {
        float normalized = nimcp_myelin_clamp(features->spectral_flux * 5.0f, 0.0f, 1.0f);
        uint32_t bin = float_to_bin(normalized, 0.0f, 1.0f, 8);
        uint32_t idx = PR_AUDIO_PRIME_FLUX_START + bin;
        if (idx < PRIME_SIG_DIM) {
            signature->exponents[idx] += (uint8_t)(bridge->sig_config.flux_weight * 16);
        }
    }

    /* Map spectral rolloff to primes */
    {
        float normalized = normalize_range(features->spectral_rolloff,
                                            500.0f, 8000.0f);
        uint32_t bin = float_to_bin(normalized, 0.0f, 1.0f, 8);
        uint32_t idx = PR_AUDIO_PRIME_ROLLOFF_START + bin;
        if (idx < PRIME_SIG_DIM) {
            signature->exponents[idx] += (uint8_t)(bridge->sig_config.rolloff_weight * 16);
        }
    }

    /* Map onset intervals (rhythmic signature) to primes [32-47] */
    for (uint32_t i = 0; i < bridge->onset_history_len && i < 16; i++) {
        float interval = bridge->onset_history[i];
        if (interval > 50.0f && interval < 2000.0f) {
            /* Normalize interval to [0, 1] */
            float normalized = (logf(interval) - logf(50.0f)) /
                               (logf(2000.0f) - logf(50.0f));
            normalized = nimcp_myelin_clamp(normalized, 0.0f, 1.0f);

            uint32_t bin = float_to_bin(normalized, 0.0f, 1.0f, 8);
            uint32_t idx = PR_AUDIO_PRIME_ONSET_START + (i % 16);
            if (idx < PRIME_SIG_DIM) {
                signature->exponents[idx] += (uint8_t)(bridge->sig_config.onset_weight * (bin + 1));
            }
        }
    }

    /* Map tempo to rhythm primes [48-55] */
    {
        float tempo_normalized = normalize_range(features->tempo_estimate_bpm,
                                                  PR_AUDIO_MIN_TEMPO,
                                                  PR_AUDIO_MAX_TEMPO);
        uint32_t bin = float_to_bin(tempo_normalized, 0.0f, 1.0f, 8);
        uint32_t idx = PR_AUDIO_PRIME_RHYTHM_START + bin;
        if (idx < PRIME_SIG_DIM) {
            signature->exponents[idx] += (uint8_t)(bridge->sig_config.rhythm_weight * 16);
        }
    }

    /* Include delta MFCCs if configured */
    if (bridge->sig_config.use_delta_mfcc) {
        for (uint32_t i = 0; i < features->num_mfcc && i < 8; i++) {
            float delta = features->delta_mfcc[i];
            float normalized = (delta + 0.5f);
            normalized = nimcp_myelin_clamp(normalized, 0.0f, 1.0f);

            uint32_t bin = float_to_bin(normalized, 0.0f, 1.0f, 4);
            uint32_t idx = 56 + i;
            if (idx < PRIME_SIG_DIM) {
                signature->exponents[idx] += (uint8_t)(bin + 1);
            }
        }
    }

    /* Recount factors and compute hash */
    signature->num_factors = prime_sig_recount_factors(signature);
    signature->hash = prime_sig_hash(signature);

    /* Normalize if configured */
    if (bridge->sig_config.normalize_features) {
        prime_sig_normalize(signature);
    }

    return PR_AUDIO_SUCCESS;
}

NIMCP_EXPORT float pr_audio_bridge_signature_similarity(
    pr_audio_bridge_t* bridge,
    const prime_signature_t* sig1,
    const prime_signature_t* sig2) {

    if (!bridge || !sig1 || !sig2) {
        return -1.0f;
    }

    return prime_sig_jaccard(sig1, sig2);
}

//=============================================================================
// Quaternion Functions
//=============================================================================

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_compute_audio_quaternion(
    pr_audio_bridge_t* bridge,
    const pr_audio_features_t* features,
    nimcp_quaternion_t* quaternion) {

    if (!bridge || !features || !quaternion) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    if (!features->valid) {
        return PR_AUDIO_ERROR_INVALID_AUDIO;
    }

    pr_audio_quat_config_t* cfg = &bridge->quat_config;

    /* Compute consolidation (w): f(repetition, temporal_coherence) */
    float repetition_factor = 1.0f - powf(cfg->repetition_decay,
                                           (float)bridge->repetition_count);
    float coherence = compute_interval_regularity(bridge->onset_history,
                                                   bridge->onset_history_len);
    float consolidation = repetition_factor * (1.0f - cfg->coherence_weight) +
                          coherence * cfg->coherence_weight;
    consolidation = nimcp_myelin_clamp(consolidation, 0.0f, 1.0f);

    /* Compute emotion (x): f(mode, tempo, brightness) */
    float emotion = pr_audio_bridge_compute_emotion_valence(bridge, features);
    emotion = nimcp_myelin_clamp(emotion, -1.0f, 1.0f);

    /* Compute salience (y): f(onset, loudness, contrast, novelty) */
    float salience = pr_audio_bridge_compute_onset_salience(
        bridge,
        features->onset_strength,
        features->peak_energy,
        features->spectral_contrast);

    /* Add novelty from FEP prediction error */
    if (bridge->fep_connected && bridge->config.use_prediction_error) {
        float pe_contribution = bridge->current_prediction_error *
                                cfg->novelty_salience_weight;
        salience += pe_contribution;
    }
    salience = nimcp_myelin_clamp(salience, 0.0f, 1.0f);

    /* Compute accessibility (z): f(familiarity, recent_access) */
    float familiarity = bridge->familiarity_score;
    /* Recency would come from memory access tracking - use default for now */
    float recency = 0.5f;
    float accessibility = familiarity * cfg->familiarity_weight +
                          recency * cfg->recency_weight;
    accessibility = nimcp_myelin_clamp(accessibility, 0.0f, 1.0f);

    /* Create quaternion */
    *quaternion = quat_create(consolidation, emotion, salience, accessibility);

    /* Optionally modulate by theta phase */
    if (bridge->theta_gamma && bridge->config.use_theta_gating) {
        *quaternion = pr_audio_bridge_modulate_by_phase(bridge, *quaternion);
    }

    return PR_AUDIO_SUCCESS;
}

NIMCP_EXPORT float pr_audio_bridge_compute_onset_salience(
    pr_audio_bridge_t* bridge,
    float onset_strength,
    float loudness_peak,
    float spectral_contrast) {

    if (!bridge) return 0.0f;

    pr_audio_quat_config_t* cfg = &bridge->quat_config;

    /* Threshold onset */
    float onset_contrib = 0.0f;
    if (onset_strength > PR_AUDIO_ONSET_THRESHOLD) {
        onset_contrib = (onset_strength - PR_AUDIO_ONSET_THRESHOLD) /
                        (1.0f - PR_AUDIO_ONSET_THRESHOLD);
    }

    /* Normalize loudness (assuming -60 to 0 dB range) */
    float loudness_normalized = nimcp_myelin_clamp((loudness_peak + 0.1f), 0.0f, 1.0f);

    /* Contrast is already [0, 1] */
    float contrast = nimcp_myelin_clamp(spectral_contrast, 0.0f, 1.0f);

    /* Weighted sum */
    float salience = onset_contrib * cfg->onset_salience_weight +
                     loudness_normalized * cfg->loudness_salience_weight +
                     contrast * cfg->contrast_salience_weight;

    return nimcp_myelin_clamp(salience, 0.0f, 1.0f);
}

NIMCP_EXPORT float pr_audio_bridge_compute_emotion_valence(
    pr_audio_bridge_t* bridge,
    const pr_audio_features_t* features) {

    if (!bridge || !features) return 0.0f;

    pr_audio_quat_config_t* cfg = &bridge->quat_config;

    /* Tempo contribution: Fast = positive, slow = negative */
    float tempo_norm = normalize_range(features->tempo_estimate_bpm,
                                        PR_AUDIO_MIN_TEMPO, PR_AUDIO_MAX_TEMPO);
    float tempo_emotion = (tempo_norm - 0.5f) * 2.0f;  /* Map to [-1, 1] */

    /* Brightness contribution: Bright = positive, dark = negative */
    float brightness_norm = normalize_range(features->spectral_centroid,
                                             PR_AUDIO_MIN_BRIGHTNESS,
                                             PR_AUDIO_MAX_BRIGHTNESS);
    float brightness_emotion = (brightness_norm - 0.5f) * 2.0f;

    /* Mode contribution would require harmonic analysis - use brightness as proxy */
    float mode_emotion = brightness_emotion * 0.5f;

    /* Weighted combination */
    float valence = tempo_emotion * cfg->tempo_influence +
                    brightness_emotion * cfg->brightness_influence +
                    mode_emotion * cfg->mode_influence;

    return nimcp_myelin_clamp(valence, -1.0f, 1.0f);
}

//=============================================================================
// Memory Encoding Functions
//=============================================================================

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_encode_to_memory(
    pr_audio_bridge_t* bridge,
    pr_audio_type_t audio_type,
    pr_memory_node_t** memory_out) {

    if (!bridge) return PR_AUDIO_ERROR_NULL_POINTER;

    return pr_audio_bridge_encode_with_features(
        bridge,
        &bridge->current_features,
        bridge->current_signature,
        bridge->current_audio_quat,
        audio_type,
        memory_out);
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_encode_with_features(
    pr_audio_bridge_t* bridge,
    const pr_audio_features_t* features,
    const prime_signature_t* signature,
    nimcp_quaternion_t quaternion,
    pr_audio_type_t audio_type,
    pr_memory_node_t** memory_out) {

    if (!bridge || !features || !signature) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    if (!bridge->node_manager) {
        return PR_AUDIO_ERROR_NOT_CONNECTED;
    }

    /* Check theta phase gating */
    if (bridge->theta_gamma && bridge->config.use_theta_gating) {
        if (!pr_audio_bridge_can_encode(bridge)) {
            return PR_AUDIO_ERROR_PHASE_BLOCKED;
        }
    }

    /* Prepare audio data for storage (pack features into buffer) */
    uint8_t audio_data[256];
    size_t data_size = sizeof(pr_audio_features_t);
    if (data_size > sizeof(audio_data)) {
        data_size = sizeof(audio_data);
    }
    memcpy(audio_data, features, data_size);

    /* Store audio type marker at end if space permits */
    if (data_size + sizeof(uint32_t) <= sizeof(audio_data)) {
        uint32_t type_marker = (uint32_t)audio_type;
        memcpy(audio_data + data_size, &type_marker, sizeof(uint32_t));
        data_size += sizeof(uint32_t);
    }

    /* Create node configuration */
    pr_node_config_t cfg = {
        .initial_tier = bridge->config.default_tier,
        .initial_strength = quaternion.w,       /* consolidation -> strength */
        .emotional_valence = quaternion.x,      /* emotion */
        .salience = quaternion.y,               /* salience */
        .accessibility = quaternion.z,          /* accessibility */
        .compute_signature = false,             /* We provide our own signature */
        .enable_cow = true
    };

    /* Create memory node with explicit signature */
    pr_memory_node_t* node = pr_memory_node_create_with_signature(
        bridge->node_manager,
        audio_data,
        data_size,
        signature,
        &cfg
    );

    if (!node) {
        return PR_AUDIO_ERROR_NO_MEMORY;
    }

    /* Update quaternion state explicitly */
    pr_memory_node_update_state(node, quaternion);

    /* Store current memory reference */
    bridge->current_audio_memory = node;

    /* Update statistics */
    bridge->stats.memories_encoded++;

    /* Return memory pointer if requested */
    if (memory_out) {
        *memory_out = node;
    }

    return PR_AUDIO_SUCCESS;
}

//=============================================================================
// Retrieval Functions
//=============================================================================

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_retrieve_similar_audio(
    pr_audio_bridge_t* bridge,
    uint32_t max_results,
    pr_audio_retrieval_result_t* results,
    uint32_t* num_results) {

    if (!bridge || !results || !num_results) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    return pr_audio_bridge_retrieve_by_query(
        bridge,
        bridge->current_signature,
        bridge->current_audio_quat,
        max_results,
        results,
        num_results);
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_retrieve_by_query(
    pr_audio_bridge_t* bridge,
    const prime_signature_t* query_signature,
    nimcp_quaternion_t query_quaternion,
    uint32_t max_results,
    pr_audio_retrieval_result_t* results,
    uint32_t* num_results) {

    if (!bridge || !query_signature || !results || !num_results) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    *num_results = 0;

    /* Check theta phase gating for retrieval */
    if (bridge->theta_gamma && bridge->config.use_theta_gating) {
        if (!pr_audio_bridge_can_retrieve(bridge)) {
            /* Not in retrieval window - could return cached or proceed anyway */
        }
    }

    /* Build resonance query */
    resonance_query_t query;
    resonance_query_init(&query);
    query.signature = (prime_signature_t*)query_signature;
    query.quaternion = query_quaternion;
    query.phase = 0.0f;
    query.module_id = 0;

    /* Get resonance config */
    resonance_config_t res_config = resonance_config_default();
    res_config.threshold = bridge->config.retrieval_threshold;

    /* Note: Full implementation would iterate through all audio memories
     * and compute resonance scores. For now, return empty results. */

    /* Update statistics */
    bridge->stats.retrievals_performed++;

    return PR_AUDIO_SUCCESS;
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_retrieve_by_type(
    pr_audio_bridge_t* bridge,
    pr_audio_type_t audio_type,
    uint32_t max_results,
    pr_audio_retrieval_result_t* results,
    uint32_t* num_results) {

    if (!bridge || !results || !num_results) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    *num_results = 0;

    /* Note: Full implementation would filter by audio_type */
    (void)audio_type;
    (void)max_results;

    bridge->stats.retrievals_performed++;

    return PR_AUDIO_SUCCESS;
}

//=============================================================================
// Temporal Pattern Functions
//=============================================================================

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_detect_temporal_pattern(
    pr_audio_bridge_t* bridge,
    pr_audio_pattern_result_t* result) {

    if (!bridge || !result) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    memset(result, 0, sizeof(pr_audio_pattern_result_t));

    /* Count valid intervals */
    uint32_t valid_count = 0;
    float interval_sum = 0.0f;
    float interval_sq_sum = 0.0f;

    for (uint32_t i = 0; i < bridge->onset_history_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->onset_history_len > 256) {
            pr_audio_bridge_heartbeat("pr_audio_bri_loop",
                             (float)(i + 1) / (float)bridge->onset_history_len);
        }

        float interval = bridge->onset_history[i];
        if (interval > 50.0f && interval < 2000.0f) {
            result->onset_intervals[valid_count] = interval;
            interval_sum += interval;
            interval_sq_sum += interval * interval;
            valid_count++;
            if (valid_count >= PR_AUDIO_MAX_ONSET_INTERVALS) break;
        }
    }

    result->num_intervals = valid_count;

    if (valid_count < 3) {
        result->pattern_type = PR_AUDIO_PATTERN_NONE;
        result->confidence = 0.0f;
        return PR_AUDIO_SUCCESS;
    }

    /* Compute statistics */
    float mean_interval = interval_sum / (valid_count > 0 ? valid_count : 1);
    float variance = (interval_sq_sum / (valid_count > 0 ? valid_count : 1)) - (mean_interval * mean_interval);
    float std_dev = sqrtf(fabsf(variance));

    result->mean_interval_ms = mean_interval;
    result->interval_variance = variance;

    /* Compute interval regularity (coefficient of variation) */
    float cv = std_dev / (mean_interval + PR_AUDIO_EPSILON);
    result->interval_regularity = 1.0f - nimcp_myelin_clamp(cv, 0.0f, 1.0f);

    /* Estimate tempo */
    result->tempo_bpm = 60000.0f / mean_interval;
    result->tempo_bpm = nimcp_myelin_clamp(result->tempo_bpm,
                                     PR_AUDIO_MIN_TEMPO, PR_AUDIO_MAX_TEMPO);
    result->tempo_confidence = result->interval_regularity;

    /* Classify pattern type */
    if (result->interval_regularity > 0.8f) {
        result->pattern_type = PR_AUDIO_PATTERN_RHYTHMIC;
        result->confidence = result->interval_regularity;
        result->beats_per_bar = 4;  /* Default assumption */
    } else if (result->interval_regularity > 0.4f) {
        result->pattern_type = PR_AUDIO_PATTERN_SPEECH;
        result->confidence = result->interval_regularity;
    } else if (valid_count == 1) {
        result->pattern_type = PR_AUDIO_PATTERN_TRANSIENT;
        result->confidence = 0.8f;
    } else {
        result->pattern_type = PR_AUDIO_PATTERN_STEADY;
        result->confidence = 0.5f;
    }

    bridge->stats.patterns_detected++;

    return PR_AUDIO_SUCCESS;
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_compute_rhythm_signature(
    pr_audio_bridge_t* bridge,
    const float* intervals,
    uint32_t num_intervals,
    prime_signature_t* signature) {

    if (!bridge || !intervals || !signature) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    if (num_intervals < 2) {
        return PR_AUDIO_SUCCESS;  /* Nothing to do */
    }

    /* Compute interval ratios and map to prime indices */
    for (uint32_t i = 0; i < num_intervals - 1 && i < 8; i++) {
        float ratio = intervals[i+1] / (intervals[i] + PR_AUDIO_EPSILON);
        ratio = nimcp_myelin_clamp(ratio, 0.25f, 4.0f);

        /* Quantize ratio to musical subdivisions */
        uint32_t ratio_bin = 0;
        if (ratio < 0.6f) ratio_bin = 0;       /* 1:2 or less */
        else if (ratio < 0.8f) ratio_bin = 1;  /* 2:3 */
        else if (ratio < 1.2f) ratio_bin = 2;  /* 1:1 */
        else if (ratio < 1.6f) ratio_bin = 3;  /* 3:2 */
        else if (ratio < 2.2f) ratio_bin = 4;  /* 2:1 */
        else ratio_bin = 5;                     /* 2:1 or more */

        uint32_t idx = PR_AUDIO_PRIME_RHYTHM_START + (i % 8);
        if (idx < PRIME_SIG_DIM) {
            signature->exponents[idx] += (uint8_t)(ratio_bin * 4);
        }
    }

    /* Update signature hash */
    signature->num_factors = prime_sig_recount_factors(signature);
    signature->hash = prime_sig_hash(signature);

    return PR_AUDIO_SUCCESS;
}

//=============================================================================
// FEP Integration Functions
//=============================================================================

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_update_from_fep(
    pr_audio_bridge_t* bridge) {

    if (!bridge) return PR_AUDIO_ERROR_NULL_POINTER;

    if (!bridge->fep_connected || !bridge->fep_bridge) {
        return PR_AUDIO_ERROR_NOT_CONNECTED;
    }

    /* Get prediction error from FEP bridge */
    /* Note: This would call actual FEP bridge API */
    /* For now, use a simulated PE based on spectral flux */
    bridge->current_prediction_error = bridge->current_features.spectral_flux * 2.0f;
    bridge->current_prediction_error = nimcp_myelin_clamp(bridge->current_prediction_error,
                                                    0.0f, 1.0f);

    /* Update statistics */
    bridge->stats.mean_prediction_error =
        (bridge->stats.mean_prediction_error * bridge->stats.frames_processed +
         bridge->current_prediction_error) /
        (bridge->stats.frames_processed + 1);

    if (bridge->current_prediction_error > 0.7f) {
        bridge->stats.high_pe_events++;
    }

    return PR_AUDIO_SUCCESS;
}

NIMCP_EXPORT float pr_audio_bridge_get_prediction_error(
    const pr_audio_bridge_t* bridge) {

    if (!bridge || !bridge->fep_connected) {
        return -1.0f;
    }

    return bridge->current_prediction_error;
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_report_to_fep(
    pr_audio_bridge_t* bridge,
    const pr_audio_features_t* features) {

    if (!bridge || !features) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    if (!bridge->fep_connected) {
        return PR_AUDIO_ERROR_NOT_CONNECTED;
    }

    /* Note: Would call FEP bridge to report actual features */
    (void)features;

    return PR_AUDIO_SUCCESS;
}

//=============================================================================
// Theta-Gamma Integration Functions
//=============================================================================

NIMCP_EXPORT bool pr_audio_bridge_can_encode(const pr_audio_bridge_t* bridge) {
    if (!bridge || !bridge->theta_gamma) {
        return true;  /* No gating if not connected */
    }

    return theta_gamma_can_encode(bridge->theta_gamma);
}

NIMCP_EXPORT bool pr_audio_bridge_can_retrieve(const pr_audio_bridge_t* bridge) {
    if (!bridge || !bridge->theta_gamma) {
        return true;  /* No gating if not connected */
    }

    return theta_gamma_can_retrieve(bridge->theta_gamma);
}

NIMCP_EXPORT float pr_audio_bridge_get_encode_strength(
    const pr_audio_bridge_t* bridge) {

    if (!bridge || !bridge->theta_gamma) {
        return 1.0f;
    }

    return theta_gamma_get_encode_strength(bridge->theta_gamma);
}

NIMCP_EXPORT float pr_audio_bridge_get_retrieve_strength(
    const pr_audio_bridge_t* bridge) {

    if (!bridge || !bridge->theta_gamma) {
        return 1.0f;
    }

    return theta_gamma_get_retrieve_strength(bridge->theta_gamma);
}

NIMCP_EXPORT nimcp_quaternion_t pr_audio_bridge_modulate_by_phase(
    pr_audio_bridge_t* bridge,
    nimcp_quaternion_t quaternion) {

    if (!bridge || !bridge->theta_gamma) {
        return quaternion;
    }

    return theta_gamma_modulate_quaternion(bridge->theta_gamma, quaternion);
}

//=============================================================================
// Entanglement Functions
//=============================================================================

NIMCP_EXPORT uint32_t pr_audio_bridge_auto_entangle(
    pr_audio_bridge_t* bridge,
    pr_memory_node_t* memory) {

    if (!bridge || !memory) {
        return 0;
    }

    /* Note: Full implementation would:
     * 1. Find similar memories via retrieval
     * 2. Create entanglement edges weighted by resonance
     * 3. Return count of new entanglements */

    uint32_t entanglements_created = 0;

    /* Update statistics */
    bridge->stats.total_entanglements += entanglements_created;

    return entanglements_created;
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_entangle_memories(
    pr_audio_bridge_t* bridge,
    pr_memory_node_t* memory1,
    pr_memory_node_t* memory2,
    entangle_edge_type_t edge_type) {

    if (!bridge || !memory1 || !memory2) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    /* Note: Would call entanglement graph API */
    (void)edge_type;

    bridge->stats.total_entanglements++;

    return PR_AUDIO_SUCCESS;
}

//=============================================================================
// State and Statistics Functions
//=============================================================================

NIMCP_EXPORT const pr_audio_features_t* pr_audio_bridge_get_current_features(
    const pr_audio_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    return &bridge->current_features;
}

NIMCP_EXPORT const prime_signature_t* pr_audio_bridge_get_current_signature(
    const pr_audio_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    return bridge->current_signature;
}

NIMCP_EXPORT nimcp_quaternion_t pr_audio_bridge_get_current_quaternion(
    const pr_audio_bridge_t* bridge) {

    if (!bridge) {
        return quat_create(0.0f, 0.0f, 0.0f, 0.0f);
    }
    return bridge->current_audio_quat;
}

NIMCP_EXPORT const pr_audio_pattern_result_t* pr_audio_bridge_get_current_pattern(
    const pr_audio_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    return &bridge->current_pattern;
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_get_stats(
    const pr_audio_bridge_t* bridge,
    pr_audio_bridge_stats_t* stats) {

    if (!bridge || !stats) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    *stats = bridge->stats;
    return PR_AUDIO_SUCCESS;
}

NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_reset_stats(
    pr_audio_bridge_t* bridge) {

    if (!bridge) {
        return PR_AUDIO_ERROR_NULL_POINTER;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return PR_AUDIO_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* pr_audio_error_string(pr_audio_error_t error) {
    switch (error) {
        case PR_AUDIO_SUCCESS:            return "Success";
        case PR_AUDIO_ERROR_NULL_POINTER: return "Null pointer";
        case PR_AUDIO_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case PR_AUDIO_ERROR_NOT_CONNECTED: return "Component not connected";
        case PR_AUDIO_ERROR_NO_MEMORY:    return "Memory allocation failed";
        case PR_AUDIO_ERROR_ENCODING:     return "Encoding failed";
        case PR_AUDIO_ERROR_RETRIEVAL:    return "Retrieval failed";
        case PR_AUDIO_ERROR_PHASE_BLOCKED: return "Operation blocked by theta phase";
        case PR_AUDIO_ERROR_INVALID_AUDIO: return "Invalid audio data";
        default:                          return "Unknown error";
    }
}

NIMCP_EXPORT const char* pr_audio_type_name(pr_audio_type_t type) {
    switch (type) {
        case PR_AUDIO_TYPE_UNKNOWN:  return "Unknown";
        case PR_AUDIO_TYPE_SPEECH:   return "Speech";
        case PR_AUDIO_TYPE_MUSIC:    return "Music";
        case PR_AUDIO_TYPE_AMBIENT:  return "Ambient";
        case PR_AUDIO_TYPE_ALARM:    return "Alarm";
        case PR_AUDIO_TYPE_RHYTHM:   return "Rhythm";
        case PR_AUDIO_TYPE_MELODY:   return "Melody";
        default:                     return "Invalid";
    }
}

NIMCP_EXPORT const char* pr_audio_pattern_name(pr_audio_pattern_t pattern) {
    switch (pattern) {
        case PR_AUDIO_PATTERN_NONE:      return "None";
        case PR_AUDIO_PATTERN_STEADY:    return "Steady";
        case PR_AUDIO_PATTERN_RHYTHMIC:  return "Rhythmic";
        case PR_AUDIO_PATTERN_MELODIC:   return "Melodic";
        case PR_AUDIO_PATTERN_SPEECH:    return "Speech";
        case PR_AUDIO_PATTERN_TRANSIENT: return "Transient";
        default:                         return "Invalid";
    }
}

NIMCP_EXPORT void pr_audio_bridge_print_state(const pr_audio_bridge_t* bridge) {
    if (!bridge) {
        printf("PR Audio Bridge: NULL\n");
        return;
    }

    printf("PR Audio Bridge State:\n");
    printf("  Initialized: %s\n", bridge->initialized ? "yes" : "no");
    printf("  Cochlea connected: %s\n", bridge->cochlea_connected ? "yes" : "no");
    printf("  Cortex connected: %s\n", bridge->cortex_connected ? "yes" : "no");
    printf("  FEP connected: %s\n", bridge->fep_connected ? "yes" : "no");
    printf("  Frames processed: %lu\n", (unsigned long)bridge->stats.frames_processed);
    printf("  Memories encoded: %lu\n", (unsigned long)bridge->stats.memories_encoded);
    printf("  Current quaternion: (%.3f, %.3f, %.3f, %.3f)\n",
           bridge->current_audio_quat.w,
           bridge->current_audio_quat.x,
           bridge->current_audio_quat.y,
           bridge->current_audio_quat.z);
    printf("  Prediction error: %.3f\n", bridge->current_prediction_error);
}

NIMCP_EXPORT void pr_audio_features_print(const pr_audio_features_t* features) {
    if (!features) {
        printf("Audio Features: NULL\n");
        return;
    }

    printf("Audio Features:\n");
    printf("  Valid: %s\n", features->valid ? "yes" : "no");
    printf("  Spectral centroid: %.1f Hz\n", features->spectral_centroid);
    printf("  Spectral spread: %.1f Hz\n", features->spectral_spread);
    printf("  Spectral flux: %.4f\n", features->spectral_flux);
    printf("  Onset strength: %.3f\n", features->onset_strength);
    printf("  Tempo estimate: %.1f BPM\n", features->tempo_estimate_bpm);
    printf("  RMS energy: %.4f\n", features->rms_energy);
    printf("  Loudness: %.1f dB\n", features->loudness_db);
    printf("  Brightness: %.3f\n", features->brightness);
}

//=============================================================================
// Static Helper Functions
//=============================================================================

/**
 * @brief Normalize value to [0, 1] range
 */
static float normalize_range(float val, float min_val, float max_val) {
    if (max_val <= min_val) return 0.5f;
    float normalized = (val - min_val) / (max_val - min_val);
    return nimcp_myelin_clamp(normalized, 0.0f, 1.0f);
}

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

/**
 * @brief Convert float value to bin index
 */
static uint32_t float_to_bin(float val, float min_val, float max_val,
                              uint32_t num_bins) {
    if (num_bins == 0) return 0;
    float normalized = normalize_range(val, min_val, max_val);
    uint32_t bin = (uint32_t)(normalized * (float)num_bins);
    if (bin >= num_bins) bin = num_bins - 1;
    return bin;
}

/**
 * @brief Compute regularity of interval sequence (coefficient of variation inverse)
 */
static float compute_interval_regularity(const float* intervals, uint32_t count) {
    if (!intervals || count < 2) return 0.0f;

    /* Count valid intervals and compute mean */
    float sum = 0.0f;
    uint32_t valid = 0;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_audio_bridge_heartbeat("pr_audio_bri_loop",
                             (float)(i + 1) / (float)count);
        }

        if (intervals[i] > 50.0f && intervals[i] < 2000.0f) {
            sum += intervals[i];
            valid++;
        }
    }

    if (valid < 2) return 0.0f;

    float mean = sum / valid;

    /* Compute variance */
    float var_sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_audio_bridge_heartbeat("pr_audio_bri_loop",
                             (float)(i + 1) / (float)count);
        }

        if (intervals[i] > 50.0f && intervals[i] < 2000.0f) {
            float diff = intervals[i] - mean;
            var_sum += diff * diff;
        }
    }

    float variance = var_sum / valid;
    float std_dev = sqrtf(variance);

    /* Coefficient of variation */
    float cv = std_dev / (mean + PR_AUDIO_EPSILON);

    /* Regularity is inverse of CV, clamped to [0, 1] */
    float regularity = 1.0f / (1.0f + cv);

    return nimcp_myelin_clamp(regularity, 0.0f, 1.0f);
}

/**
 * @brief Update ring buffer with new data
 */
static void update_history_buffer(float* buffer, uint32_t* pos, uint32_t len,
                                   const float* new_data, uint32_t new_count) {
    if (!buffer || !pos || !new_data || len == 0) return;

    for (uint32_t i = 0; i < new_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && new_count > 256) {
            pr_audio_bridge_heartbeat("pr_audio_bri_loop",
                             (float)(i + 1) / (float)new_count);
        }

        buffer[*pos] = new_data[i];
        *pos = (*pos + 1) % len;
    }
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_audio_bridge_set_instance_health_agent(
    pr_audio_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_audio_bridge_training_begin(pr_audio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_audio_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_audio_bridge_heartbeat_instance(bridge->health_agent, "pr_audio_bridge_training_begin", 0.0f);
    return 0;
}

int pr_audio_bridge_training_end(pr_audio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_audio_bridge_training_end: NULL argument");
        return -1;
    }
    pr_audio_bridge_heartbeat_instance(bridge->health_agent, "pr_audio_bridge_training_end", 1.0f);
    return 0;
}

int pr_audio_bridge_training_step(pr_audio_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_audio_bridge_training_step: NULL argument");
        return -1;
    }
    pr_audio_bridge_heartbeat_instance(bridge->health_agent, "pr_audio_bridge_training_step", progress);
    return 0;
}
