//=============================================================================
// nimcp_bg_neuromodulators.c - Multi-Neuromodulator Implementation
//=============================================================================

#include "core/brain/subcortical/nimcp_bg_neuromodulators.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bg_neuromodulators, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* ============================================================================
 * INTERNAL STRUCTURE
 * ============================================================================ */

struct bg_neuromod_system {
    bg_neuromod_config_t config;
    bg_neuromod_levels_t levels;
    bg_neuromod_levels_t targets;
    bg_receptor_state_t receptors;
    bg_motive_state_t motive_state;
    bg_neuromod_stats_t stats;
    nimcp_mutex_t* mutex;
    float time_ms;
};

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */

void bg_neuromod_default_config(bg_neuromod_config_t* config) {
    if (!config) return;
    config->baseline.dopamine = BG_DOPAMINE_BASELINE;
    config->baseline.serotonin = BG_SEROTONIN_BASELINE;
    config->baseline.acetylcholine = BG_ACETYLCHOLINE_BASELINE;
    config->baseline.norepinephrine = BG_NOREPINEPHRINE_BASELINE;
    config->baseline.adenosine = BG_ADENOSINE_BASELINE;
    config->d1_sensitivity = BG_D1_SENSITIVITY;
    config->d2_sensitivity = BG_D2_SENSITIVITY;
    config->ht2a_sensitivity = BG_5HT2A_SENSITIVITY;
    config->m4_sensitivity = BG_M4_SENSITIVITY;
    config->alpha2_sensitivity = BG_ALPHA2_SENSITIVITY;
    config->a2a_sensitivity = 0.6f;
    config->release_rate = 0.2f;
    config->reuptake_rate = 0.1f;
    config->synthesis_rate = 0.05f;
    config->enable_da_5ht_interaction = true;
    config->enable_ach_da_interaction = true;
    config->enable_adenosine_da_antagonism = true;
    config->state_threshold = 0.3f;
}

bg_neuromod_system_t* bg_neuromod_create(const bg_neuromod_config_t* config) {
    bg_neuromod_system_t* sys = nimcp_calloc(1, sizeof(bg_neuromod_system_t));
    if (!sys) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sys is NULL");

        return NULL;

    }

    if (config) {
        sys->config = *config;
    } else {
        bg_neuromod_default_config(&sys->config);
    }

    sys->levels = sys->config.baseline;
    sys->targets = sys->config.baseline;
    sys->motive_state = BG_MOTIVE_STATE_NEUTRAL;
    sys->mutex = nimcp_mutex_create(NULL);
    if (!sys->mutex) {
        nimcp_free(sys);
        return NULL;
    }

    return sys;
}

void bg_neuromod_destroy(bg_neuromod_system_t* system) {
    if (!system) return;
    if (system->mutex) {
        nimcp_mutex_free(system->mutex);
    }
    nimcp_free(system);
}

int bg_neuromod_reset(bg_neuromod_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_reset: system is NULL");
        return -1;
    }
    nimcp_mutex_lock(system->mutex);
    system->levels = system->config.baseline;
    system->targets = system->config.baseline;
    system->motive_state = BG_MOTIVE_STATE_NEUTRAL;
    memset(&system->receptors, 0, sizeof(system->receptors));
    memset(&system->stats, 0, sizeof(system->stats));
    system->time_ms = 0;
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * NEUROMODULATOR CONTROL
 * ============================================================================ */

int bg_neuromod_set_level(bg_neuromod_system_t* system, bg_neuromod_type_t type, float level) {
    if (!system || type >= BG_NEUROMOD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_neuromod_set_level: system is NULL");
        return -1;
    }
    nimcp_mutex_lock(system->mutex);
    level = nimcp_clampf(level, 0.0f, 1.0f);
    switch (type) {
        case BG_NEUROMOD_DOPAMINE: system->levels.dopamine = level; break;
        case BG_NEUROMOD_SEROTONIN: system->levels.serotonin = level; break;
        case BG_NEUROMOD_ACETYLCHOLINE: system->levels.acetylcholine = level; break;
        case BG_NEUROMOD_NOREPINEPHRINE: system->levels.norepinephrine = level; break;
        case BG_NEUROMOD_ADENOSINE: system->levels.adenosine = level; break;
        default: break;
    }
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_neuromod_trigger_release(bg_neuromod_system_t* system, bg_neuromod_type_t type, float magnitude) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_trigger_release: system is NULL");
        return -1;
    }
    nimcp_mutex_lock(system->mutex);
    magnitude = nimcp_clampf(magnitude, 0.0f, 1.0f);
    float boost = magnitude * system->config.release_rate;
    switch (type) {
        case BG_NEUROMOD_DOPAMINE: system->levels.dopamine = nimcp_clampf(system->levels.dopamine + boost, 0, 1); break;
        case BG_NEUROMOD_SEROTONIN: system->levels.serotonin = nimcp_clampf(system->levels.serotonin + boost, 0, 1); break;
        case BG_NEUROMOD_ACETYLCHOLINE: system->levels.acetylcholine = nimcp_clampf(system->levels.acetylcholine + boost, 0, 1); break;
        case BG_NEUROMOD_NOREPINEPHRINE: system->levels.norepinephrine = nimcp_clampf(system->levels.norepinephrine + boost, 0, 1); break;
        case BG_NEUROMOD_ADENOSINE: system->levels.adenosine = nimcp_clampf(system->levels.adenosine + boost, 0, 1); break;
        default: break;
    }
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_neuromod_trigger_pause(bg_neuromod_system_t* system, bg_neuromod_type_t type, float magnitude) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_trigger_pause: system is NULL");
        return -1;
    }
    nimcp_mutex_lock(system->mutex);
    magnitude = nimcp_clampf(magnitude, 0.0f, 1.0f);
    float drop = magnitude * system->config.release_rate;
    switch (type) {
        case BG_NEUROMOD_DOPAMINE: system->levels.dopamine = nimcp_clampf(system->levels.dopamine - drop, 0, 1); break;
        case BG_NEUROMOD_SEROTONIN: system->levels.serotonin = nimcp_clampf(system->levels.serotonin - drop, 0, 1); break;
        case BG_NEUROMOD_ACETYLCHOLINE: system->levels.acetylcholine = nimcp_clampf(system->levels.acetylcholine - drop, 0, 1); break;
        case BG_NEUROMOD_NOREPINEPHRINE: system->levels.norepinephrine = nimcp_clampf(system->levels.norepinephrine - drop, 0, 1); break;
        case BG_NEUROMOD_ADENOSINE: system->levels.adenosine = nimcp_clampf(system->levels.adenosine - drop, 0, 1); break;
        default: break;
    }
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_neuromod_process_reward(bg_neuromod_system_t* system, float reward, float prediction) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_process_reward: system is NULL");
        return -1;
    }
    float rpe = reward - prediction;
    if (rpe > 0) {
        bg_neuromod_trigger_release(system, BG_NEUROMOD_DOPAMINE, rpe);
    } else {
        bg_neuromod_trigger_pause(system, BG_NEUROMOD_DOPAMINE, -rpe);
    }
    return 0;
}

int bg_neuromod_process_aversion(bg_neuromod_system_t* system, float aversion_level) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_process_aversion: system is NULL");
        return -1;
    }
    bg_neuromod_trigger_release(system, BG_NEUROMOD_SEROTONIN, aversion_level);
    bg_neuromod_trigger_release(system, BG_NEUROMOD_NOREPINEPHRINE, aversion_level * 0.5f);
    return 0;
}

int bg_neuromod_process_uncertainty(bg_neuromod_system_t* system, float uncertainty) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_process_uncertainty: system is NULL");
        return -1;
    }
    bg_neuromod_trigger_release(system, BG_NEUROMOD_NOREPINEPHRINE, uncertainty);
    bg_neuromod_trigger_release(system, BG_NEUROMOD_ACETYLCHOLINE, uncertainty * 0.7f);
    return 0;
}

int bg_neuromod_process_effort(bg_neuromod_system_t* system, float effort_level) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_process_effort: system is NULL");
        return -1;
    }
    bg_neuromod_trigger_release(system, BG_NEUROMOD_ADENOSINE, effort_level);
    return 0;
}

int bg_neuromod_process_attention_cue(bg_neuromod_system_t* system, float salience) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_process_attention_cue: system is NULL");
        return -1;
    }
    bg_neuromod_trigger_release(system, BG_NEUROMOD_ACETYLCHOLINE, salience);
    return 0;
}

/* ============================================================================
 * PROCESSING
 * ============================================================================ */

int bg_neuromod_step(bg_neuromod_system_t* system, float dt_ms) {
    if (!system || dt_ms <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_neuromod_step: system is NULL");
        return -1;
    }
    nimcp_mutex_lock(system->mutex);

    float dt_s = dt_ms / 1000.0f;
    system->time_ms += dt_ms;

    /* Decay toward baseline */
    float decay = system->config.reuptake_rate * dt_s;
    system->levels.dopamine += (system->config.baseline.dopamine - system->levels.dopamine) * decay;
    system->levels.serotonin += (system->config.baseline.serotonin - system->levels.serotonin) * decay;
    system->levels.acetylcholine += (system->config.baseline.acetylcholine - system->levels.acetylcholine) * decay;
    system->levels.norepinephrine += (system->config.baseline.norepinephrine - system->levels.norepinephrine) * decay;
    system->levels.adenosine += (system->config.baseline.adenosine - system->levels.adenosine) * decay;

    /* Update receptor states */
    system->receptors.d1_activation = system->levels.dopamine * system->config.d1_sensitivity;
    system->receptors.d2_activation = system->levels.dopamine * system->config.d2_sensitivity;
    system->receptors.ht2a_activation = system->levels.serotonin * system->config.ht2a_sensitivity;
    system->receptors.m4_activation = system->levels.acetylcholine * system->config.m4_sensitivity;
    system->receptors.alpha2_activation = system->levels.norepinephrine * system->config.alpha2_sensitivity;
    system->receptors.a2a_activation = system->levels.adenosine * system->config.a2a_sensitivity;

    /* Determine motivational state */
    float threshold = system->config.state_threshold;
    if (system->levels.dopamine > system->config.baseline.dopamine + threshold) {
        system->motive_state = BG_MOTIVE_STATE_APPROACH;
    } else if (system->levels.serotonin > system->config.baseline.serotonin + threshold) {
        system->motive_state = BG_MOTIVE_STATE_AVOID;
    } else if (system->levels.norepinephrine > system->config.baseline.norepinephrine + threshold) {
        system->motive_state = BG_MOTIVE_STATE_ALERT;
    } else if (system->levels.adenosine > system->config.baseline.adenosine + threshold) {
        system->motive_state = BG_MOTIVE_STATE_FATIGUED;
    } else if (system->levels.acetylcholine > system->config.baseline.acetylcholine + threshold) {
        system->motive_state = BG_MOTIVE_STATE_FOCUSED;
    } else {
        system->motive_state = BG_MOTIVE_STATE_NEUTRAL;
    }

    /* Update stats */
    system->stats.current_levels = system->levels;
    system->stats.receptor_state = system->receptors;
    system->stats.motive_state = system->motive_state;
    system->stats.da_5ht_ratio = system->levels.dopamine / (system->levels.serotonin + 0.01f);
    system->stats.arousal_level = system->levels.norepinephrine;
    system->stats.fatigue_level = system->levels.adenosine;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bg_neuromod_compute_effects(bg_neuromod_system_t* system, bg_neuromod_effects_t* effects) {
    if (!system || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_compute_effects: required parameter is NULL (system, effects)");
        return -1;
    }
    nimcp_mutex_lock(system->mutex);

    effects->direct_pathway_gain = 1.0f + (system->receptors.d1_activation - 0.5f);
    effects->indirect_pathway_gain = 1.0f + (system->receptors.d2_activation - 0.5f) * 0.8f;
    effects->hyperdirect_gain = 1.0f + system->receptors.alpha2_activation * 0.5f;

    effects->temporal_discount = 1.0f - system->levels.serotonin * 0.5f; /* Higher 5HT = more patient */
    effects->effort_cost = 1.0f + system->levels.adenosine * 0.5f;
    effects->exploration_bonus = system->levels.norepinephrine * 0.3f;
    effects->attention_gate = system->levels.acetylcholine;

    effects->action_threshold_mod = 1.0f + system->levels.adenosine * 0.3f - system->levels.dopamine * 0.2f;
    effects->habit_threshold_mod = 1.0f - system->levels.acetylcholine * 0.2f;

    effects->ltp_rate_mod = 1.0f + system->levels.dopamine * 0.5f;
    effects->ltd_rate_mod = 1.0f + system->levels.serotonin * 0.3f;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

bg_motive_state_t bg_neuromod_get_state(const bg_neuromod_system_t* system) {
    if (!system) return BG_MOTIVE_STATE_NEUTRAL;
    return system->motive_state;
}

/* ============================================================================
 * QUERY
 * ============================================================================ */

float bg_neuromod_get_level(const bg_neuromod_system_t* system, bg_neuromod_type_t type) {
    if (!system) return 0.0f;
    switch (type) {
        case BG_NEUROMOD_DOPAMINE: return system->levels.dopamine;
        case BG_NEUROMOD_SEROTONIN: return system->levels.serotonin;
        case BG_NEUROMOD_ACETYLCHOLINE: return system->levels.acetylcholine;
        case BG_NEUROMOD_NOREPINEPHRINE: return system->levels.norepinephrine;
        case BG_NEUROMOD_ADENOSINE: return system->levels.adenosine;
        default: return 0.0f;
    }
}

int bg_neuromod_get_all_levels(const bg_neuromod_system_t* system, bg_neuromod_levels_t* levels) {
    if (!system || !levels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_get_all_levels: required parameter is NULL (system, levels)");
        return -1;
    }
    *levels = system->levels;
    return 0;
}

int bg_neuromod_get_receptor_state(const bg_neuromod_system_t* system, bg_receptor_state_t* state) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_get_receptor_state: required parameter is NULL (system, state)");
        return -1;
    }
    *state = system->receptors;
    return 0;
}

int bg_neuromod_get_stats(const bg_neuromod_system_t* system, bg_neuromod_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }
    *stats = system->stats;
    return 0;
}

/* ============================================================================
 * INTEGRATION
 * ============================================================================ */

int bg_neuromod_get_striatal_modulation(const bg_neuromod_system_t* system, float* d1_mod, float* d2_mod) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_neuromod_get_striatal_modulation: system is NULL");
        return -1;
    }
    if (d1_mod) *d1_mod = 1.0f + (system->levels.dopamine - 0.5f) * system->config.d1_sensitivity;
    if (d2_mod) *d2_mod = 1.0f - (system->levels.dopamine - 0.5f) * system->config.d2_sensitivity * 0.8f;
    return 0;
}

float bg_neuromod_get_temporal_discount(const bg_neuromod_system_t* system) {
    if (!system) return 1.0f;
    return 1.0f - system->levels.serotonin * 0.5f;
}

float bg_neuromod_get_effort_cost(const bg_neuromod_system_t* system) {
    if (!system) return 1.0f;
    return 1.0f + system->levels.adenosine * 0.5f;
}

float bg_neuromod_get_exploration_bonus(const bg_neuromod_system_t* system) {
    if (!system) return 0.0f;
    return system->levels.norepinephrine * 0.3f;
}

float bg_neuromod_get_attention_gate(const bg_neuromod_system_t* system) {
    if (!system) return 1.0f;
    return system->levels.acetylcholine;
}
