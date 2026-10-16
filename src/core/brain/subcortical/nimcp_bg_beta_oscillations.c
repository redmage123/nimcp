//=============================================================================
// nimcp_bg_beta_oscillations.c - Basal Ganglia Beta Oscillation Implementation
//=============================================================================

#include "core/brain/subcortical/nimcp_bg_beta_oscillations.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bg_beta_oscillations, MESH_ADAPTER_CATEGORY_SUBCORTICAL)



/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

struct bg_beta_system {
    /* Configuration */
    bg_beta_config_t config;

    /* State */
    bg_beta_state_t state;
    bg_pathology_t pathology;
    float pathology_severity;

    /* Oscillation channels */
    bg_beta_channel_t* channels;
    uint32_t num_channels;

    /* Global oscillation state */
    float global_phase;
    float global_frequency;
    float global_amplitude;
    float global_power;

    /* Movement dynamics */
    float suppression_level;        /* Current suppression depth */
    float movement_intent;          /* Current movement intention */
    float rebound_timer;            /* Time since movement end */

    /* STN-GPe loop */
    float stn_activity;
    float gpe_activity;
    float loop_gain;

    /* Dopamine modulation */
    float dopamine_level;
    float dopamine_effect;

    /* Pathological state */
    float lock_timer;               /* Time in locked state */
    float tremor_phase;             /* Tremor oscillation phase */

    /* Treatment effects */
    float dbs_effect;
    float ldopa_effect;

    /* Statistics */
    bg_beta_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Time tracking */
    float time_ms;
};

static float lerp_f(float a, float b, float t) {
    return a + t * (b - a);
}

/* Generate oscillation sample */
static float generate_oscillation(float phase, float amplitude, float noise_level) {
    float base = sinf(phase) * amplitude;
    /* Add slight noise for biological realism */
    float noise = ((float)nimcp_tl_rand() / RAND_MAX - 0.5f) * 2.0f * noise_level;
    return base + noise;
}

/* Compute band power from history */
static float compute_band_power(const float* history, uint32_t size,
                                 float sample_rate, float low_hz, float high_hz) {
    /* Simplified power estimation using zero-crossing rate and amplitude */
    float sum_sq = 0.0f;
    float zero_crossings = 0.0f;
    float prev = history[0];

    for (uint32_t i = 1; i < size; i++) {
        sum_sq += history[i] * history[i];
        if ((prev < 0 && history[i] >= 0) || (prev >= 0 && history[i] < 0)) {
            zero_crossings += 1.0f;
        }
        prev = history[i];
    }

    float rms = sqrtf(sum_sq / size);
    float est_freq = (zero_crossings / 2.0f) * (sample_rate / size);

    /* Weight by frequency band match */
    float center = (low_hz + high_hz) / 2.0f;
    float freq_match = 1.0f - fabsf(est_freq - center) / center;
    freq_match = nimcp_clampf(freq_match, 0.0f, 1.0f);

    return rms * freq_match;
}

/* ============================================================================
 * LIFECYCLE IMPLEMENTATION
 * ============================================================================ */

void bg_beta_default_config(bg_beta_config_t* config) {
    if (!config) return;

    config->baseline_frequency = BG_BETA_PEAK_HZ;
    config->bandwidth = 8.0f;
    config->baseline_power = BG_BETA_NORMAL_POWER;
    config->suppression_threshold = 0.5f;
    config->lock_threshold = BG_BETA_PATHOLOGICAL;
    config->dopamine_sensitivity = 0.5f;
    config->stn_gpe_coupling = 0.7f;
    config->num_channels = 8;
    config->enable_pathology = true;
    config->enable_tremor = false;
    config->tremor_frequency = 5.0f;
}

bg_beta_system_t* bg_beta_create(const bg_beta_config_t* config) {
    bg_beta_system_t* system = nimcp_calloc(1, sizeof(bg_beta_system_t));
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bg_beta_create: allocation failed");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        bg_beta_default_config(&system->config);
    }

    /* Validate channels */
    if (system->config.num_channels == 0) {
        system->config.num_channels = 8;
    }
    if (system->config.num_channels > BG_BETA_MAX_CHANNELS) {
        system->config.num_channels = BG_BETA_MAX_CHANNELS;
    }
    system->num_channels = system->config.num_channels;

    /* Allocate channels */
    system->channels = nimcp_calloc(system->num_channels, sizeof(bg_beta_channel_t));
    if (!system->channels) {
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bg_beta_create: system->channels is NULL");
        return NULL;
    }

    /* Initialize channels with slightly different phases */
    for (uint32_t i = 0; i < system->num_channels; i++) {
        system->channels[i].phase = (float)i * (2.0f * M_PI / system->num_channels);
        system->channels[i].frequency = system->config.baseline_frequency;
        system->channels[i].amplitude = system->config.baseline_power;
        system->channels[i].power = system->config.baseline_power;
        system->channels[i].history_idx = 0;
    }

    /* Initialize global state */
    system->global_phase = 0.0f;
    system->global_frequency = system->config.baseline_frequency;
    system->global_amplitude = system->config.baseline_power;
    system->global_power = system->config.baseline_power;

    /* Initialize state */
    system->state = BG_BETA_STATE_BASELINE;
    system->pathology = BG_PATHOLOGY_NONE;
    system->pathology_severity = 0.0f;

    /* Initialize dynamics */
    system->suppression_level = 0.0f;
    system->movement_intent = 0.0f;
    system->rebound_timer = 0.0f;
    system->loop_gain = system->config.stn_gpe_coupling;
    system->dopamine_level = 0.5f;
    system->dopamine_effect = 1.0f;

    /* Create mutex */
    system->mutex = nimcp_mutex_create(NULL);
    if (!system->mutex) {
        bg_beta_destroy(system);
        return NULL;
    }

    return system;
}

void bg_beta_destroy(bg_beta_system_t* system) {
    if (!system) return;

    if (system->mutex) {
        nimcp_mutex_free(system->mutex);
    }

    nimcp_free(system->channels);
    nimcp_free(system);
}

int bg_beta_reset(bg_beta_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_beta_reset: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Reset channels */
    for (uint32_t i = 0; i < system->num_channels; i++) {
        system->channels[i].phase = (float)i * (2.0f * M_PI / system->num_channels);
        system->channels[i].frequency = system->config.baseline_frequency;
        system->channels[i].amplitude = system->config.baseline_power;
        system->channels[i].power = system->config.baseline_power;
        memset(system->channels[i].history, 0, sizeof(system->channels[i].history));
        system->channels[i].history_idx = 0;
    }

    /* Reset state */
    system->state = BG_BETA_STATE_BASELINE;
    system->global_phase = 0.0f;
    system->global_frequency = system->config.baseline_frequency;
    system->global_amplitude = system->config.baseline_power;
    system->global_power = system->config.baseline_power;
    system->suppression_level = 0.0f;
    system->movement_intent = 0.0f;
    system->rebound_timer = 0.0f;
    system->lock_timer = 0.0f;
    system->tremor_phase = 0.0f;
    system->dbs_effect = 0.0f;
    system->ldopa_effect = 0.0f;
    system->time_ms = 0.0f;

    memset(&system->stats, 0, sizeof(system->stats));

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * PROCESSING IMPLEMENTATION
 * ============================================================================ */

int bg_beta_step(bg_beta_system_t* system, float dt_ms) {
    if (!system || dt_ms <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_beta_step: system is NULL or dt_ms <= 0");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    float dt_s = dt_ms / 1000.0f;
    system->time_ms += dt_ms;

    /* Calculate target power based on state and pathology */
    float target_power = system->config.baseline_power;

    /* Apply pathology effects */
    if (system->config.enable_pathology && system->pathology != BG_PATHOLOGY_NONE) {
        float pathology_boost = 0.0f;
        switch (system->pathology) {
            case BG_PATHOLOGY_PARKINSON_EARLY:
                pathology_boost = 0.2f * system->pathology_severity;
                break;
            case BG_PATHOLOGY_PARKINSON_MOD:
                pathology_boost = 0.4f * system->pathology_severity;
                break;
            case BG_PATHOLOGY_PARKINSON_SEVERE:
                pathology_boost = 0.6f * system->pathology_severity;
                break;
            case BG_PATHOLOGY_DYSTONIA:
                pathology_boost = 0.3f * system->pathology_severity;
                break;
            case BG_PATHOLOGY_TREMOR:
                pathology_boost = 0.15f * system->pathology_severity;
                break;
            default:
                break;
        }
        target_power += pathology_boost;
    }

    /* Apply treatment effects */
    target_power -= system->dbs_effect * 0.4f;
    target_power -= system->ldopa_effect * 0.3f;

    /* Apply dopamine modulation (higher DA = lower beta) */
    float da_mod = 1.0f - (system->dopamine_level - 0.5f) * system->config.dopamine_sensitivity;
    da_mod = nimcp_clampf(da_mod, 0.5f, 1.5f);
    target_power *= da_mod;
    system->dopamine_effect = da_mod;

    /* State machine for movement-related dynamics */
    switch (system->state) {
        case BG_BETA_STATE_BASELINE:
            /* Check for movement intent */
            if (system->movement_intent > system->config.suppression_threshold) {
                system->state = BG_BETA_STATE_SUPPRESSING;
                system->stats.movement_events++;
            }
            /* Check for pathological lock */
            if (target_power > system->config.lock_threshold) {
                system->state = BG_BETA_STATE_LOCKED;
                system->stats.lock_events++;
            }
            break;

        case BG_BETA_STATE_SUPPRESSING:
            system->suppression_level += BG_BETA_SUPPRESSION_RATE * dt_s * 10.0f;
            if (system->suppression_level >= 1.0f) {
                system->suppression_level = 1.0f;
                system->state = BG_BETA_STATE_SUPPRESSED;
            }
            target_power *= (1.0f - system->suppression_level * (1.0f - BG_BETA_SUPPRESSION_DEPTH));
            break;

        case BG_BETA_STATE_SUPPRESSED:
            target_power *= BG_BETA_SUPPRESSION_DEPTH;
            /* Stay suppressed while movement intent is high */
            if (system->movement_intent < system->config.suppression_threshold * 0.5f) {
                system->state = BG_BETA_STATE_REBOUNDING;
                system->rebound_timer = 0.0f;
            }
            break;

        case BG_BETA_STATE_REBOUNDING:
            system->rebound_timer += dt_s;
            system->suppression_level -= BG_BETA_REBOUND_RATE * dt_s * 10.0f;
            if (system->suppression_level <= 0.0f) {
                system->suppression_level = 0.0f;
                system->state = BG_BETA_STATE_BASELINE;
            }
            /* Post-movement beta rebound (overshoot) */
            float rebound_factor = 1.0f + 0.3f * expf(-system->rebound_timer / 0.5f);
            target_power *= rebound_factor * (1.0f - system->suppression_level);
            break;

        case BG_BETA_STATE_LOCKED:
            system->lock_timer += dt_s;
            system->stats.time_in_locked = system->lock_timer;
            /* Can only exit locked state with treatment or strong dopamine */
            if (target_power < system->config.lock_threshold * 0.8f) {
                system->state = BG_BETA_STATE_BASELINE;
                system->lock_timer = 0.0f;
            }
            break;

        default:
            break;
    }

    target_power = nimcp_clampf(target_power, 0.0f, 1.0f);

    /* Update global oscillation */
    float freq_rad = system->global_frequency * 2.0f * M_PI;
    system->global_phase += freq_rad * dt_s;
    if (system->global_phase > 2.0f * M_PI) {
        system->global_phase -= 2.0f * M_PI;
    }

    /* Smooth power transition */
    system->global_power = lerp_f(system->global_power, target_power, 0.1f);
    system->global_amplitude = system->global_power;

    /* Update individual channels */
    float coherence_sum = 0.0f;
    for (uint32_t i = 0; i < system->num_channels; i++) {
        bg_beta_channel_t* ch = &system->channels[i];

        /* Channel frequency varies slightly around global */
        float freq_var = 1.0f + 0.05f * sinf(ch->phase * 0.1f);
        ch->frequency = system->global_frequency * freq_var;

        /* Update phase */
        ch->phase += ch->frequency * 2.0f * M_PI * dt_s;
        if (ch->phase > 2.0f * M_PI) {
            ch->phase -= 2.0f * M_PI;
        }

        /* Update amplitude with coupling to global */
        ch->amplitude = lerp_f(ch->amplitude, system->global_amplitude, 0.2f);

        /* Generate sample and store in history */
        float sample = generate_oscillation(ch->phase, ch->amplitude, 0.02f);
        ch->history[ch->history_idx] = sample;
        ch->history_idx = (ch->history_idx + 1) % BG_BETA_HISTORY_SIZE;

        /* Update power estimate */
        ch->power = compute_band_power(ch->history, BG_BETA_HISTORY_SIZE,
                                        1000.0f / dt_ms, BG_BETA_LOW_HZ, BG_BETA_HIGH_HZ);

        /* Coherence calculation (phase difference from global) */
        float phase_diff = fabsf(ch->phase - system->global_phase);
        if (phase_diff > M_PI) phase_diff = 2.0f * M_PI - phase_diff;
        coherence_sum += 1.0f - (phase_diff / M_PI);
    }

    /* Update statistics */
    system->stats.mean_power = system->global_power;
    system->stats.peak_frequency = system->global_frequency;
    system->stats.phase_coherence = coherence_sum / system->num_channels;
    system->stats.suppression_depth = system->suppression_level;
    system->stats.dopamine_effect = system->dopamine_effect;

    /* Add tremor component if enabled */
    if (system->config.enable_tremor && system->pathology == BG_PATHOLOGY_TREMOR) {
        system->tremor_phase += system->config.tremor_frequency * 2.0f * M_PI * dt_s;
        if (system->tremor_phase > 2.0f * M_PI) {
            system->tremor_phase -= 2.0f * M_PI;
        }
    }

    /* Decay movement intent */
    system->movement_intent *= (1.0f - 0.5f * dt_s);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

float bg_beta_process_stn_gpe_loop(bg_beta_system_t* system,
                                    float stn_activity,
                                    float gpe_activity) {
    if (!system) return 0.0f;

    nimcp_mutex_lock(system->mutex);

    system->stn_activity = nimcp_clampf(stn_activity, 0.0f, 1.0f);
    system->gpe_activity = nimcp_clampf(gpe_activity, 0.0f, 1.0f);

    /* STN-GPe loop generates beta oscillations */
    /* Higher coupling = stronger oscillations */
    float loop_output = system->stn_activity * system->gpe_activity * system->loop_gain;

    /* Modulate global amplitude based on loop */
    system->global_amplitude = lerp_f(system->global_amplitude,
                                       system->config.baseline_power * (1.0f + loop_output),
                                       0.1f);

    float result = system->global_amplitude;
    nimcp_mutex_unlock(system->mutex);
    return result;
}

float bg_beta_apply_dopamine(bg_beta_system_t* system, float dopamine_level) {
    if (!system) return 0.0f;

    nimcp_mutex_lock(system->mutex);
    system->dopamine_level = nimcp_clampf(dopamine_level, 0.0f, 1.0f);
    float result = system->global_power;
    nimcp_mutex_unlock(system->mutex);
    return result;
}

int bg_beta_signal_movement_intent(bg_beta_system_t* system,
                                    float intention_strength) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_beta_signal_movement_intent: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);
    system->movement_intent = nimcp_clampf(intention_strength, 0.0f, 1.0f);
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_beta_signal_movement_complete(bg_beta_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_beta_signal_movement_complete: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);
    system->movement_intent = 0.0f;
    if (system->state == BG_BETA_STATE_SUPPRESSED) {
        system->state = BG_BETA_STATE_REBOUNDING;
        system->rebound_timer = 0.0f;
    }
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * PATHOLOGY IMPLEMENTATION
 * ============================================================================ */

int bg_beta_set_pathology(bg_beta_system_t* system,
                          bg_pathology_t pathology,
                          float severity) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_beta_set_pathology: system is NULL");
        return -1;
    }
    if (pathology >= BG_PATHOLOGY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_beta_set_pathology: pathology out of range");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);
    system->pathology = pathology;
    system->pathology_severity = nimcp_clampf(severity, 0.0f, 1.0f);
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_beta_get_pathology(const bg_beta_system_t* system,
                          bg_pathology_t* out_pathology,
                          float* out_severity) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_beta_get_pathology: system is NULL");
        return -1;
    }

    if (out_pathology) *out_pathology = system->pathology;
    if (out_severity) *out_severity = system->pathology_severity;
    return 0;
}

float bg_beta_apply_dbs(bg_beta_system_t* system,
                        float dbs_frequency,
                        float dbs_amplitude) {
    if (!system) return 0.0f;

    nimcp_mutex_lock(system->mutex);

    /* DBS at ~130 Hz disrupts pathological beta */
    float freq_effectiveness = 1.0f;
    if (dbs_frequency < 100.0f || dbs_frequency > 180.0f) {
        freq_effectiveness = 0.5f;  /* Suboptimal frequency */
    }

    system->dbs_effect = nimcp_clampf(dbs_amplitude, 0.0f, 1.0f) * freq_effectiveness;

    /* DBS can break locked state */
    if (system->dbs_effect > 0.5f && system->state == BG_BETA_STATE_LOCKED) {
        system->state = BG_BETA_STATE_BASELINE;
        system->lock_timer = 0.0f;
    }

    float result = system->dbs_effect;
    nimcp_mutex_unlock(system->mutex);
    return result;
}

float bg_beta_apply_ldopa(bg_beta_system_t* system, float ldopa_level) {
    if (!system) return 0.0f;

    nimcp_mutex_lock(system->mutex);
    system->ldopa_effect = nimcp_clampf(ldopa_level, 0.0f, 1.0f);

    /* L-DOPA increases effective dopamine */
    system->dopamine_level = nimcp_clampf(system->dopamine_level + ldopa_level * 0.3f, 0.0f, 1.0f);

    float result = system->ldopa_effect;
    nimcp_mutex_unlock(system->mutex);
    return result;
}

/* ============================================================================
 * QUERY IMPLEMENTATION
 * ============================================================================ */

bg_beta_state_t bg_beta_get_state(const bg_beta_system_t* system) {
    if (!system) return BG_BETA_STATE_BASELINE;
    return system->state;
}

float bg_beta_get_power(const bg_beta_system_t* system, bg_beta_band_t band) {
    if (!system) return 0.0f;

    /* For now, return global power for all bands */
    /* TODO: Implement sub-band filtering */
    (void)band;
    return system->global_power;
}

float bg_beta_get_channel_power(const bg_beta_system_t* system, uint32_t channel) {
    if (!system || channel >= system->num_channels) return 0.0f;
    return system->channels[channel].power;
}

float bg_beta_get_coherence(const bg_beta_system_t* system) {
    if (!system) return 0.0f;
    return system->stats.phase_coherence;
}

bool bg_beta_is_movement_blocked(const bg_beta_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_beta_is_movement_blocked: system is NULL");
        return false;
    }
    return system->state == BG_BETA_STATE_LOCKED;
}

float bg_beta_get_movement_readiness(const bg_beta_system_t* system) {
    if (!system) return 0.0f;

    /* Movement readiness is inverse of beta power */
    float readiness = 1.0f - system->global_power;

    /* Locked state blocks movement */
    if (system->state == BG_BETA_STATE_LOCKED) {
        readiness *= 0.1f;
    }

    /* Suppressed state enables movement */
    if (system->state == BG_BETA_STATE_SUPPRESSED) {
        readiness = 0.9f + readiness * 0.1f;
    }

    return nimcp_clampf(readiness, 0.0f, 1.0f);
}

int bg_beta_get_output(const bg_beta_system_t* system, float* output) {
    if (!system || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_beta_get_output: required parameter is NULL (system, output)");
        return -1;
    }

    for (uint32_t i = 0; i < system->num_channels; i++) {
        output[i] = generate_oscillation(system->channels[i].phase,
                                          system->channels[i].amplitude,
                                          0.01f);
    }
    return 0;
}

int bg_beta_get_stats(const bg_beta_system_t* system, bg_beta_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_beta_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }
    *stats = system->stats;
    return 0;
}

/* ============================================================================
 * INTEGRATION IMPLEMENTATION
 * ============================================================================ */

float bg_beta_modulate_action_threshold(const bg_beta_system_t* system,
                                         float base_threshold) {
    if (!system) return base_threshold;

    /* Higher beta = higher threshold = harder to initiate action */
    float beta_factor = 1.0f + system->global_power * 0.5f;

    /* Locked state significantly raises threshold */
    if (system->state == BG_BETA_STATE_LOCKED) {
        beta_factor *= 2.0f;
    }

    return base_threshold * beta_factor;
}

float bg_beta_get_stn_modulation(const bg_beta_system_t* system) {
    if (!system) return 1.0f;

    /* Beta power increases STN output */
    return 1.0f + system->global_power * 0.5f;
}

float bg_beta_get_motor_gate(const bg_beta_system_t* system) {
    if (!system) return 1.0f;
    return bg_beta_get_movement_readiness(system);
}
