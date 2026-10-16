/**
 * @file nimcp_mood_regulation.c
 * @brief Mood regulation system implementation
 * @date 2026-01-11
 */

#include "core/brain/regions/raphe/nimcp_mood_regulation.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(mood_regulation, MESH_ADAPTER_CATEGORY_COGNITIVE)

static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_mood_config_t nimcp_mood_default_config(void) {
    nimcp_mood_config_t config = {
        .time_constant = MOOD_TIME_CONSTANT,
        .stability_baseline = MOOD_DEFAULT_STABILITY,
        .anxiety_baseline = ANXIETY_BASELINE,
        .stress_sensitivity = 0.5f,
        .reward_sensitivity = 0.4f,
        .ht_mood_gain = 0.6f,
        .circadian_amplitude = 0.15f
    };
    return config;
}

int nimcp_mood_init(nimcp_mood_system_t* system, const nimcp_mood_config_t* config) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    memset(system, 0, sizeof(nimcp_mood_system_t));

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        system->config = nimcp_mood_default_config();
    }

    /* Initialize state */
    system->valence = MOOD_DEFAULT_NEUTRAL;
    system->stability = system->config.stability_baseline;
    system->anxiety = system->config.anxiety_baseline;
    system->irritability = 0.2f;
    system->energy = 0.5f;

    /* Initialize dynamics */
    system->valence_velocity = 0.0f;
    system->target_valence = MOOD_DEFAULT_NEUTRAL;

    /* Initialize inputs */
    system->stress_input = 0.0f;
    system->reward_input = 0.0f;
    system->social_input = 0.0f;
    system->circadian_phase = 0.0f;

    /* Initialize 5-HT state */
    system->current_5ht = 20.0f;  /* nM baseline */
    system->baseline_5ht = 20.0f;

    /* Initialize history */
    system->history_count = 0;
    system->history_index = 0;

    /* Initialize statistics */
    system->avg_valence = 0.0f;
    system->valence_variance = 0.0f;
    system->time_depressed = 0.0f;
    system->time_positive = 0.0f;

    system->initialized = true;
    return 0;
}

int nimcp_mood_shutdown(nimcp_mood_system_t* system) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }
    system->initialized = false;
    return 0;
}

int nimcp_mood_reset(nimcp_mood_system_t* system) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    nimcp_mood_config_t config = system->config;
    return nimcp_mood_init(system, &config);
}

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_mood_update(nimcp_mood_system_t* system, float ht_level, float dt) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_update: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    float dt_sec = dt / 1000.0f;
    system->current_5ht = ht_level;

    /* 1. Compute 5-HT contribution to mood */
    /* Higher 5-HT -> better mood (positive valence) */
    float ht_ratio = ht_level / system->baseline_5ht;
    float ht_mood_contribution = (ht_ratio - 1.0f) * system->config.ht_mood_gain;

    /* 2. Compute circadian contribution */
    float circadian_contribution = system->config.circadian_amplitude *
                                   sinf(system->circadian_phase * NIMCP_TWO_PI_F);

    /* 3. Compute target valence */
    system->target_valence = ht_mood_contribution + circadian_contribution;

    /* Apply stress (negative) */
    system->target_valence -= system->stress_input * system->config.stress_sensitivity;

    /* Apply reward (positive) */
    system->target_valence += system->reward_input * system->config.reward_sensitivity;

    /* Apply social input */
    system->target_valence += system->social_input * 0.3f;

    /* Clamp target */
    system->target_valence = nimcp_clampf(system->target_valence, -1.0f, 1.0f);

    /* 4. Update valence with time constant (slow mood changes) */
    float alpha = 1.0f - expf(-dt / system->config.time_constant);
    float old_valence = system->valence;
    system->valence = lerp(system->valence, system->target_valence, alpha);
    system->valence_velocity = (system->valence - old_valence) / dt_sec;

    /* 5. Update stability based on 5-HT */
    /* Higher 5-HT -> more stable mood */
    float stability_target = system->config.stability_baseline + (ht_ratio - 1.0f) * 0.3f;
    stability_target = nimcp_clampf(stability_target, 0.2f, 0.95f);
    system->stability = lerp(system->stability, stability_target, alpha * 0.5f);

    /* 6. Update anxiety */
    /* Low 5-HT -> higher anxiety */
    float anxiety_target = system->config.anxiety_baseline - (ht_ratio - 1.0f) * 0.4f;
    anxiety_target += system->stress_input * 0.5f;  /* Stress increases anxiety */
    anxiety_target = nimcp_clampf(anxiety_target, 0.0f, 1.0f);
    system->anxiety = lerp(system->anxiety, anxiety_target, alpha);

    /* 7. Update irritability */
    /* Low 5-HT -> higher irritability */
    float irritability_target = 0.3f - (ht_ratio - 1.0f) * 0.3f;
    irritability_target += system->stress_input * 0.4f;
    irritability_target = nimcp_clampf(irritability_target, 0.0f, 1.0f);
    system->irritability = lerp(system->irritability, irritability_target, alpha);

    /* 8. Update energy */
    /* Mood affects energy, but with some independence */
    float energy_target = 0.5f + system->valence * 0.3f;
    energy_target -= system->anxiety * 0.2f;  /* Anxiety drains energy */
    energy_target = nimcp_clampf(energy_target, 0.1f, 1.0f);
    system->energy = lerp(system->energy, energy_target, alpha * 0.8f);

    /* 9. Decay inputs */
    system->stress_input *= (1.0f - 0.1f * dt_sec);
    system->reward_input *= (1.0f - 0.2f * dt_sec);
    system->social_input *= (1.0f - 0.15f * dt_sec);

    /* 10. Update statistics */
    /* Running average */
    float stat_alpha = 0.001f;
    system->avg_valence = lerp(system->avg_valence, system->valence, stat_alpha);

    /* Variance (exponential moving) */
    float diff = system->valence - system->avg_valence;
    system->valence_variance = lerp(system->valence_variance, diff * diff, stat_alpha);

    /* Time in states */
    if (system->valence < -0.3f) {
        system->time_depressed += dt_sec;
    }
    if (system->valence > 0.3f) {
        system->time_positive += dt_sec;
    }

    /* 11. Record history */
    nimcp_mood_snapshot_t snapshot = {
        .valence = system->valence,
        .stability = system->stability,
        .anxiety = system->anxiety,
        .irritability = system->irritability,
        .energy = system->energy,
        .timestamp = 0  /* Would need global time */
    };

    system->history[system->history_index] = snapshot;
    system->history_index = (system->history_index + 1) % MOOD_MAX_HISTORY;
    if (system->history_count < MOOD_MAX_HISTORY) {
        system->history_count++;
    }

    return 0;
}

/*=============================================================================
 * Input API
 *===========================================================================*/

int nimcp_mood_apply_stress(nimcp_mood_system_t* system, float stress) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_apply_stress: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->stress_input = nimcp_clampf(system->stress_input + stress, 0.0f, 1.0f);
    return 0;
}

int nimcp_mood_apply_reward(nimcp_mood_system_t* system, float reward) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_apply_reward: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->reward_input = nimcp_clampf(system->reward_input + reward, 0.0f, 1.0f);
    return 0;
}

int nimcp_mood_apply_social(nimcp_mood_system_t* system, float social_input) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_apply_social: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->social_input = nimcp_clampf(system->social_input + social_input, -1.0f, 1.0f);
    return 0;
}

int nimcp_mood_set_circadian_phase(nimcp_mood_system_t* system, float phase) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_set_circadian_phase: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    /* Phase: 0-1 represents 24-hour cycle */
    system->circadian_phase = fmodf(phase, 1.0f);
    if (system->circadian_phase < 0.0f) {
        system->circadian_phase += 1.0f;
    }
    return 0;
}

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_mood_get_valence(nimcp_mood_system_t* system, float* valence) {
    if (!system || !system->initialized || !valence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_get_valence: required parameter is NULL (system, system->initialized, valence)");
        return -1;
    }

    *valence = system->valence;
    return 0;
}

int nimcp_mood_get_stability(nimcp_mood_system_t* system, float* stability) {
    if (!system || !system->initialized || !stability) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_get_stability: required parameter is NULL (system, system->initialized, stability)");
        return -1;
    }

    *stability = system->stability;
    return 0;
}

int nimcp_mood_get_anxiety(nimcp_mood_system_t* system, float* anxiety) {
    if (!system || !system->initialized || !anxiety) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_get_anxiety: required parameter is NULL (system, system->initialized, anxiety)");
        return -1;
    }

    *anxiety = system->anxiety;
    return 0;
}

int nimcp_mood_get_energy(nimcp_mood_system_t* system, float* energy) {
    if (!system || !system->initialized || !energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_get_energy: required parameter is NULL (system, system->initialized, energy)");
        return -1;
    }

    *energy = system->energy;
    return 0;
}

int nimcp_mood_get_state(nimcp_mood_system_t* system, nimcp_mood_snapshot_t* state) {
    if (!system || !system->initialized || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_get_state: required parameter is NULL (system, system->initialized, state)");
        return -1;
    }

    state->valence = system->valence;
    state->stability = system->stability;
    state->anxiety = system->anxiety;
    state->irritability = system->irritability;
    state->energy = system->energy;
    state->timestamp = 0;  /* Would need global time */

    return 0;
}

/*=============================================================================
 * Analysis API
 *===========================================================================*/

int nimcp_mood_get_trend(nimcp_mood_system_t* system, float* trend) {
    if (!system || !system->initialized || !trend) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_get_trend: required parameter is NULL (system, system->initialized, trend)");
        return -1;
    }

    /* Compute trend from history */
    if (system->history_count < 2) {
        *trend = 0.0f;
        return 0;
    }

    /* Simple linear trend from oldest to newest */
    uint32_t oldest_idx = (system->history_count >= MOOD_MAX_HISTORY) ?
                          system->history_index : 0;
    uint32_t newest_idx = (system->history_index + MOOD_MAX_HISTORY - 1) % MOOD_MAX_HISTORY;

    *trend = system->history[newest_idx].valence - system->history[oldest_idx].valence;
    return 0;
}

int nimcp_mood_get_variability(nimcp_mood_system_t* system, float* variability) {
    if (!system || !system->initialized || !variability) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_get_variability: required parameter is NULL (system, system->initialized, variability)");
        return -1;
    }

    /* Variability as sqrt of variance */
    *variability = sqrtf(system->valence_variance);
    return 0;
}

int nimcp_mood_is_depressed(nimcp_mood_system_t* system, bool* depressed) {
    if (!system || !system->initialized || !depressed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mood_is_depressed: required parameter is NULL (system, system->initialized, depressed)");
        return -1;
    }

    /* Depression indicators:
     * - Low valence (< -0.4)
     * - Low energy (< 0.3)
     * - High anxiety (> 0.6)
     * - Sustained negative mood
     */
    bool low_valence = system->valence < -0.4f;
    bool low_energy = system->energy < 0.3f;
    bool high_anxiety = system->anxiety > 0.6f;
    bool sustained = system->time_depressed > 60.0f;  /* > 60 seconds in depressed state */

    /* Need multiple criteria */
    int criteria_met = 0;
    if (low_valence) criteria_met++;
    if (low_energy) criteria_met++;
    if (high_anxiety) criteria_met++;
    if (sustained) criteria_met++;

    *depressed = (criteria_met >= 2);
    return 0;
}
