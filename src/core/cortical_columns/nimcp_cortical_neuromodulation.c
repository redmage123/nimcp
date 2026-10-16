/**
 * @file nimcp_cortical_neuromodulation.c
 * @brief Implementation of cortical neuromodulation system
 * @version 1.0.0
 * @date 2025-12-15
 */

#include "core/cortical_columns/nimcp_cortical_neuromodulation.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/rng/nimcp_rand.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_neural_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_neuromodulation)

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Internal neuromodulation system structure
 */
struct cortical_neuromod_system {
    /* Configuration */
    cortical_neuromod_config_t config;

    /* State */
    cortical_neuromod_state_t state;

    /* Statistics */
    cortical_neuromod_stats_t stats;

    /* Integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
    neuromodulator_system_t global_system;
    bool global_connected;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Update running average
 * WHY:  Track historical neuromodulator levels
 * HOW:  Exponential moving average
 */
static inline void update_avg(float* avg, float new_value, float alpha) {
    *avg = alpha * new_value + (1.0f - alpha) * (*avg);
}

/**
 * WHAT: Get current timestamp in microseconds
 * WHY:  Track timing for decay computation
 * HOW:  System time query
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

void cortical_neuromod_default_config(cortical_neuromod_config_t* config) {
    /* Guard clause */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        return;
    }

    /* Acetylcholine effects (Hasselmo 2006) */
    config->ach_snr_boost = 1.5f;
    config->ach_lateral_inhibition_reduction = 0.35f;
    config->ach_plasticity_gate = 0.5f;

    /* Dopamine effects (Schultz 1997, Seamans 2004) */
    config->da_reward_sensitivity = 1.0f;
    config->da_plasticity_modulation = 0.5f;
    config->da_gain_modulation = 0.3f;

    /* Norepinephrine effects (Aston-Jones 2005) */
    config->ne_gain_boost = 0.4f;
    config->ne_reset_probability = 0.01f;
    config->ne_exploration_boost = 0.3f;

    /* Serotonin effects (Froemke 2015) */
    config->serotonin_inhibition_boost = 0.4f;
    config->serotonin_impulsivity_reduction = 0.3f;

    /* Time constants */
    config->release_tau_ms = 50.0f;
    config->clearance_tau_ms = 200.0f;

    /* Spatial resolution */
    config->num_columns = 0;

    /* Integration */
    config->enable_bio_async = false;
    config->connect_global_neuromod = false;
}

cortical_neuromod_system_t* cortical_neuromod_create(
    const cortical_neuromod_config_t* config
) {
    /* Guard clause */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    /* Allocate system */
    cortical_neuromod_system_t* system =
        (cortical_neuromod_system_t*)nimcp_calloc(1, sizeof(cortical_neuromod_system_t));
    if (!system) {
        NIMCP_LOGGING_ERROR("Failed to allocate neuromod system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_neuromod_create: system alloc failed");

        return NULL;
    }

    /* Copy configuration */
    memcpy(&system->config, config, sizeof(cortical_neuromod_config_t));

    /* Initialize state to baseline (0.5 = neutral) */
    system->state.current_levels.ach_level = 0.5f;
    system->state.current_levels.da_level = 0.5f;
    system->state.current_levels.ne_level = 0.5f;
    system->state.current_levels.serotonin_level = NIMCP_SEROTONIN_BASELINE;
    memcpy(&system->state.target_levels, &system->state.current_levels,
           sizeof(cortical_neuromod_levels_t));

    /* Allocate per-column DA if needed */
    if (config->num_columns > 0) {
        system->state.per_column_da =
            (float*)nimcp_calloc(config->num_columns, sizeof(float));
        if (!system->state.per_column_da) {
            NIMCP_LOGGING_ERROR("Failed to allocate per-column DA");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_neuromod_create: system->state is NULL");
            goto cleanup;
        }
        system->state.num_columns = config->num_columns;
        /* Initialize to baseline */
        for (uint32_t i = 0; i < config->num_columns; i++) {
            system->state.per_column_da[i] = 0.5f;
        }
    }

    system->state.last_update_time_us = get_timestamp_us();

    /* Create and initialize mutex */
    system->mutex = nimcp_malloc(sizeof(nimcp_platform_mutex_t));
    if (!system->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_neuromod_create: mutex allocation failed");
        goto cleanup;
    }

    if (nimcp_platform_mutex_init(system->mutex, false) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cortical_neuromod_create: mutex init failed");
        goto cleanup;
    }

    /* Initialize statistics */
    memset(&system->stats, 0, sizeof(cortical_neuromod_stats_t));
    system->stats.avg_ach = 0.5f;
    system->stats.avg_da = 0.5f;
    system->stats.avg_ne = 0.5f;
    system->stats.avg_serotonin = 0.5f;

    /* Compute initial effects */
    cortical_neuromod_compute_effects(system);

    /* Bio-async integration */
    if (config->enable_bio_async) {
        cortical_neuromod_connect_bio_async(system);
    }

    NIMCP_LOGGING_INFO("Created cortical neuromodulation system");
    return system;

cleanup:
    if (system) {
        nimcp_free(system->mutex);
        nimcp_free(system->state.per_column_da);
        nimcp_free(system);
    }
    return NULL;
}

void cortical_neuromod_destroy(cortical_neuromod_system_t* system) {
    /* Guard clause */
    if (!system) {
        return;
    }

    /* Disconnect bio-async */
    if (system->bio_async_enabled) {
        cortical_neuromod_disconnect_bio_async(system);
    }

    /* Free per-column DA */
    if (system->state.per_column_da) {
        nimcp_free(system->state.per_column_da);
    }

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    /* Free system */
    nimcp_free(system);
    NIMCP_LOGGING_INFO("Destroyed cortical neuromodulation system");
}

//=============================================================================
// Level Control Functions
//=============================================================================

int cortical_neuromod_set_level(
    cortical_neuromod_system_t* system,
    cortical_neuromodulator_type_t type,
    float level
) {
    /* Guard clauses */
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }
    if (type >= CORTICAL_NEUROMOD_COUNT) return -1;

    nimcp_platform_mutex_lock(system->mutex);

    /* Clamp level */
    level = nimcp_clamp01(level);

    /* Update level */
    switch (type) {
        case CORTICAL_NEUROMOD_ACETYLCHOLINE:
            system->state.current_levels.ach_level = level;
            system->state.target_levels.ach_level = level;
            break;
        case CORTICAL_NEUROMOD_DOPAMINE:
            system->state.current_levels.da_level = level;
            system->state.target_levels.da_level = level;
            break;
        case CORTICAL_NEUROMOD_NOREPINEPHRINE:
            system->state.current_levels.ne_level = level;
            system->state.target_levels.ne_level = level;
            break;
        case CORTICAL_NEUROMOD_SEROTONIN:
            system->state.current_levels.serotonin_level = level;
            system->state.target_levels.serotonin_level = level;
            break;
        default:
            nimcp_platform_mutex_unlock(system->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_neuromod_set_level: operation failed");
            return -1;
    }

    /* Recompute effects */
    cortical_neuromod_compute_effects(system);

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

float cortical_neuromod_get_level(
    const cortical_neuromod_system_t* system,
    cortical_neuromodulator_type_t type
) {
    /* Guard clauses */
    if (!system) return -1.0f;
    if (type >= CORTICAL_NEUROMOD_COUNT) return -1.0f;

    float level = 0.0f;
    nimcp_platform_mutex_lock(system->mutex);

    switch (type) {
        case CORTICAL_NEUROMOD_ACETYLCHOLINE:
            level = system->state.current_levels.ach_level;
            break;
        case CORTICAL_NEUROMOD_DOPAMINE:
            level = system->state.current_levels.da_level;
            break;
        case CORTICAL_NEUROMOD_NOREPINEPHRINE:
            level = system->state.current_levels.ne_level;
            break;
        case CORTICAL_NEUROMOD_SEROTONIN:
            level = system->state.current_levels.serotonin_level;
            break;
        default:
            level = -1.0f;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return level;
}

float cortical_neuromod_release(
    cortical_neuromod_system_t* system,
    cortical_neuromodulator_type_t type,
    float magnitude
) {
    /* Guard clauses */
    if (!system) return -1.0f;
    if (type >= CORTICAL_NEUROMOD_COUNT) return -1.0f;
    if (magnitude < 0.0f) return -1.0f;

    nimcp_platform_mutex_lock(system->mutex);

    float* level_ptr = NULL;
    switch (type) {
        case CORTICAL_NEUROMOD_ACETYLCHOLINE:
            level_ptr = &system->state.current_levels.ach_level;
            system->stats.ach_releases++;
            break;
        case CORTICAL_NEUROMOD_DOPAMINE:
            level_ptr = &system->state.current_levels.da_level;
            system->stats.da_releases++;
            break;
        case CORTICAL_NEUROMOD_NOREPINEPHRINE:
            level_ptr = &system->state.current_levels.ne_level;
            system->stats.ne_releases++;
            break;
        case CORTICAL_NEUROMOD_SEROTONIN:
            level_ptr = &system->state.current_levels.serotonin_level;
            system->stats.serotonin_releases++;
            break;
        default:
            nimcp_platform_mutex_unlock(system->mutex);
            return -1.0f;
    }

    /* Add release (phasic burst) */
    *level_ptr = nimcp_clamp01(*level_ptr + magnitude);
    system->stats.total_releases++;

    /* Recompute effects */
    cortical_neuromod_compute_effects(system);

    float result = *level_ptr;
    nimcp_platform_mutex_unlock(system->mutex);
    return result;
}

int cortical_neuromod_set_column_da(
    cortical_neuromod_system_t* system,
    uint32_t column_index,
    float level
) {
    /* Guard clauses */
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }
    if (!system->state.per_column_da) return -1;
    if (column_index >= system->state.num_columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cortical_neuromod_set_column_da: capacity exceeded");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);
    system->state.per_column_da[column_index] = nimcp_clamp01(level);
    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

float cortical_neuromod_get_column_da(
    const cortical_neuromod_system_t* system,
    uint32_t column_index
) {
    /* Guard clauses */
    if (!system) return -1.0f;
    if (!system->state.per_column_da) return -1.0f;
    if (column_index >= system->state.num_columns) return -1.0f;

    nimcp_platform_mutex_lock(system->mutex);
    float level = system->state.per_column_da[column_index];
    nimcp_platform_mutex_unlock(system->mutex);
    return level;
}

//=============================================================================
// Dynamics Functions
//=============================================================================

int cortical_neuromod_update(
    cortical_neuromod_system_t* system,
    float dt_ms
) {
    /* Guard clauses */
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }
    if (dt_ms < 0.0f) return -1;

    nimcp_platform_mutex_lock(system->mutex);

    /* Compute decay factor: exp(-dt/tau) */
    float decay = expf(-dt_ms / system->config.clearance_tau_ms);

    /* Apply decay to all modulators */
    system->state.current_levels.ach_level *= decay;
    system->state.current_levels.da_level *= decay;
    system->state.current_levels.ne_level *= decay;
    system->state.current_levels.serotonin_level *= decay;

    /* Update per-column DA */
    if (system->state.per_column_da) {
        for (uint32_t i = 0; i < system->state.num_columns; i++) {
            system->state.per_column_da[i] *= decay;
        }
    }

    /* Update timestamp */
    system->state.last_update_time_us = get_timestamp_us();

    /* Recompute effects */
    cortical_neuromod_compute_effects(system);

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

int cortical_neuromod_compute_effects(cortical_neuromod_system_t* system) {
    /* Guard clause */
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    /* Note: Caller should hold mutex */

    const cortical_neuromod_levels_t* levels = &system->state.current_levels;
    cortical_neuromod_effects_t* effects = &system->state.current_effects;
    const cortical_neuromod_config_t* cfg = &system->config;

    /* Gain modulation: DA and NE increase excitability */
    effects->gain_modulation = 1.0f +
        (levels->da_level * cfg->da_gain_modulation) +
        (levels->ne_level * cfg->ne_gain_boost);

    /* Lateral inhibition: ACh reduces, serotonin increases */
    effects->lateral_inhibition_modulation = 1.0f -
        (levels->ach_level * cfg->ach_lateral_inhibition_reduction) +
        (levels->serotonin_level * cfg->serotonin_inhibition_boost);

    /* Plasticity gate: ACh enables learning */
    if (levels->ach_level >= cfg->ach_plasticity_gate) {
        effects->plasticity_gate = 1.0f;
        system->stats.plasticity_gated_on++;
    } else {
        effects->plasticity_gate = 0.0f;
        system->stats.plasticity_gated_off++;
    }

    /* SNR modulation: ACh sharpens */
    effects->snr_modulation = 1.0f + (levels->ach_level * cfg->ach_snr_boost);

    /* Exploration: NE promotes exploration */
    effects->exploration_modulation = 1.0f +
        (levels->ne_level * cfg->ne_exploration_boost);

    /* Update running averages (alpha = 0.05 for slow averaging) */
    update_avg(&system->stats.avg_ach, levels->ach_level, 0.05f);
    update_avg(&system->stats.avg_da, levels->da_level, 0.05f);
    update_avg(&system->stats.avg_ne, levels->ne_level, 0.05f);
    update_avg(&system->stats.avg_serotonin, levels->serotonin_level, 0.05f);

    /* Copy current state to stats */
    memcpy(&system->stats.current_levels, levels, sizeof(cortical_neuromod_levels_t));
    memcpy(&system->stats.current_effects, effects, sizeof(cortical_neuromod_effects_t));

    return 0;
}

//=============================================================================
// Effect Application Functions
//=============================================================================

int cortical_neuromod_apply_ach_effects(
    const cortical_neuromod_system_t* system,
    float base_lateral_inhibition,
    float base_snr,
    float* modulated_li,
    float* modulated_snr
) {
    /* Guard clauses */
    if (!system || !modulated_li || !modulated_snr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_neuromod_apply_ach_effects: required parameter is NULL (system, modulated_li, modulated_snr)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    const cortical_neuromod_effects_t* effects = &system->state.current_effects;

    /* Apply lateral inhibition modulation */
    *modulated_li = base_lateral_inhibition * effects->lateral_inhibition_modulation;

    /* Apply SNR modulation */
    *modulated_snr = base_snr * effects->snr_modulation;

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

int cortical_neuromod_apply_da_effects(
    const cortical_neuromod_system_t* system,
    float base_gain,
    float base_learning_rate,
    float* modulated_gain,
    float* modulated_lr
) {
    /* Guard clauses */
    if (!system || !modulated_gain || !modulated_lr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_neuromod_apply_da_effects: required parameter is NULL (system, modulated_gain, modulated_lr)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    const cortical_neuromod_effects_t* effects = &system->state.current_effects;
    const cortical_neuromod_levels_t* levels = &system->state.current_levels;

    /* Apply gain modulation */
    *modulated_gain = base_gain * effects->gain_modulation;

    /* Apply DA plasticity modulation */
    float da_factor = 1.0f + (levels->da_level * system->config.da_plasticity_modulation);
    *modulated_lr = base_learning_rate * da_factor;

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

int cortical_neuromod_apply_ne_effects(
    cortical_neuromod_system_t* system,
    float base_gain,
    float* modulated_gain,
    bool* should_reset
) {
    /* Guard clauses */
    if (!system || !modulated_gain || !should_reset) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_neuromod_apply_ne_effects: required parameter is NULL (system, modulated_gain, should_reset)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    const cortical_neuromod_effects_t* effects = &system->state.current_effects;
    const cortical_neuromod_levels_t* levels = &system->state.current_levels;

    /* Apply gain modulation */
    *modulated_gain = base_gain * effects->gain_modulation;

    /* Check for network reset (probabilistic, high NE increases probability) */
    float reset_prob = levels->ne_level * system->config.ne_reset_probability;
    float random_val = nimcp_rand_uniform();
    *should_reset = (random_val < reset_prob);

    if (*should_reset) {
        system->stats.ne_triggered_resets++;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

int cortical_neuromod_apply_serotonin_effects(
    const cortical_neuromod_system_t* system,
    float base_inhibition,
    float* modulated_inhibition
) {
    /* Guard clauses */
    if (!system || !modulated_inhibition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_neuromod_apply_serotonin_effects: required parameter is NULL (system, modulated_inhibition)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);

    const cortical_neuromod_effects_t* effects = &system->state.current_effects;

    /* Apply inhibition modulation */
    *modulated_inhibition = base_inhibition * effects->lateral_inhibition_modulation;

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

float cortical_neuromod_get_plasticity_gate(
    const cortical_neuromod_system_t* system
) {
    /* Guard clause */
    if (!system) return -1.0f;

    nimcp_platform_mutex_lock(system->mutex);
    float gate = system->state.current_effects.plasticity_gate;
    nimcp_platform_mutex_unlock(system->mutex);
    return gate;
}

float cortical_neuromod_modulate_plasticity(
    const cortical_neuromod_system_t* system,
    float base_learning_rate
) {
    /* Guard clause */
    if (!system) return -1.0f;

    nimcp_platform_mutex_lock(system->mutex);

    const cortical_neuromod_effects_t* effects = &system->state.current_effects;
    const cortical_neuromod_levels_t* levels = &system->state.current_levels;

    /* Three-factor learning: gate × DA modulation */
    float da_factor = 1.0f + (levels->da_level * system->config.da_plasticity_modulation);
    float effective_lr = base_learning_rate * effects->plasticity_gate * da_factor;

    nimcp_platform_mutex_unlock(system->mutex);
    return effective_lr;
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

int cortical_neuromod_get_stats(
    const cortical_neuromod_system_t* system,
    cortical_neuromod_stats_t* stats
) {
    /* Guard clauses */
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_neuromod_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);
    memcpy(stats, &system->stats, sizeof(cortical_neuromod_stats_t));
    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

int cortical_neuromod_reset(cortical_neuromod_system_t* system) {
    /* Guard clause */
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Reset levels to baseline (0.5) */
    system->state.current_levels.ach_level = 0.5f;
    system->state.current_levels.da_level = 0.5f;
    system->state.current_levels.ne_level = 0.5f;
    system->state.current_levels.serotonin_level = NIMCP_SEROTONIN_BASELINE;

    memcpy(&system->state.target_levels, &system->state.current_levels,
           sizeof(cortical_neuromod_levels_t));

    /* Reset per-column DA */
    if (system->state.per_column_da) {
        for (uint32_t i = 0; i < system->state.num_columns; i++) {
            system->state.per_column_da[i] = 0.5f;
        }
    }

    /* Reset statistics */
    memset(&system->stats, 0, sizeof(cortical_neuromod_stats_t));
    system->stats.avg_ach = 0.5f;
    system->stats.avg_da = 0.5f;
    system->stats.avg_ne = 0.5f;
    system->stats.avg_serotonin = 0.5f;

    /* Recompute effects */
    cortical_neuromod_compute_effects(system);

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Reset cortical neuromodulation system");
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int cortical_neuromod_connect_bio_async(cortical_neuromod_system_t* system) {
    /* Guard clause */
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Check if already connected */
    if (system->bio_async_enabled) {
        nimcp_platform_mutex_unlock(system->mutex);
        return 0;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_CORTICAL_NEUROMOD,
        .module_name = "cortical_neuromodulation",
        .inbox_capacity = 32,
        .user_data = system
    };

    system->bio_ctx = bio_router_register_module(&info);
    if (system->bio_ctx) {
        system->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

int cortical_neuromod_disconnect_bio_async(cortical_neuromod_system_t* system) {
    /* Guard clause */
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(system->mutex);

    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        system->bio_async_enabled = false;
        NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

bool cortical_neuromod_is_bio_async_connected(
    const cortical_neuromod_system_t* system
) {
    /* Guard clause */
    if (!system) {
        return false;
    }

    nimcp_platform_mutex_lock(system->mutex);
    bool connected = system->bio_async_enabled;
    nimcp_platform_mutex_unlock(system->mutex);
    return connected;
}

//=============================================================================
// Global System Integration
//=============================================================================

int cortical_neuromod_connect_global_system(
    cortical_neuromod_system_t* system,
    neuromodulator_system_t global_system
) {
    /* Guard clauses */
    if (!system || !global_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_neuromod_connect_global_system: required parameter is NULL (system, global_system)");
        return -1;
    }

    nimcp_platform_mutex_lock(system->mutex);
    system->global_system = global_system;
    system->global_connected = true;
    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Connected to global neuromodulator system");
    return 0;
}

int cortical_neuromod_sync_with_global(cortical_neuromod_system_t* system) {
    /* Guard clause */
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Check if connected */
    if (!system->global_connected || !system->global_system) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_neuromod_sync_with_global: required parameter is NULL (system->global_connected, system->global_system)");
        return -1;
    }

    /* Query global levels */
    float ach = neuromodulator_get_level(system->global_system, NEUROMOD_ACETYLCHOLINE);
    float da = neuromodulator_get_level(system->global_system, NEUROMOD_DOPAMINE);
    float ne = neuromodulator_get_level(system->global_system, NEUROMOD_NOREPINEPHRINE);
    float serotonin = neuromodulator_get_level(system->global_system, NEUROMOD_SEROTONIN);

    /* Update local levels */
    if (ach >= 0.0f) {
        system->state.current_levels.ach_level = ach;
    }
    if (da >= 0.0f) {
        system->state.current_levels.da_level = da;
    }
    if (ne >= 0.0f) {
        system->state.current_levels.ne_level = ne;
    }
    if (serotonin >= 0.0f) {
        system->state.current_levels.serotonin_level = serotonin;
    }

    /* Recompute effects */
    cortical_neuromod_compute_effects(system);

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}
