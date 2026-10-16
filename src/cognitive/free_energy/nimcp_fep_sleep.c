/**
 * @file nimcp_fep_sleep.c
 * @brief Sleep-Dependent Memory Consolidation for Free Energy Principle
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of sleep-dependent memory consolidation for FEP
 * WHY:  Sleep is essential for synaptic homeostasis and model optimization
 * HOW:  Sleep stages, replay consolidation, synaptic downscaling
 */

#include "cognitive/free_energy/nimcp_fep_sleep.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(fep_sleep, MESH_ADAPTER_CATEGORY_COGNITIVE)

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static void free_experience(fep_experience_t* exp) {
    if (!exp) return;
    if (exp->state) nimcp_free(exp->state);
    if (exp->observation) nimcp_free(exp->observation);
    if (exp->next_state) nimcp_free(exp->next_state);
}

/* Get stage duration from config */
static uint32_t get_stage_duration(const fep_sleep_system_t* sys, fep_sleep_stage_t stage) {
    switch (stage) {
        case SLEEP_STAGE_N1:  return sys->config.n1_duration_ms;
        case SLEEP_STAGE_N2:  return sys->config.n2_duration_ms;
        case SLEEP_STAGE_SWS: return sys->config.sws_duration_ms;
        case SLEEP_STAGE_REM: return sys->config.rem_duration_ms;
        default:              return 0;
    }
}

/* Get next stage in sleep cycle */
static fep_sleep_stage_t get_next_stage(fep_sleep_stage_t current) {
    switch (current) {
        case SLEEP_STAGE_WAKE: return SLEEP_STAGE_N1;
        case SLEEP_STAGE_N1:   return SLEEP_STAGE_N2;
        case SLEEP_STAGE_N2:   return SLEEP_STAGE_SWS;
        case SLEEP_STAGE_SWS:  return SLEEP_STAGE_REM;
        case SLEEP_STAGE_REM:  return SLEEP_STAGE_N2;  /* Cycle back */
        default:               return SLEEP_STAGE_WAKE;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

void fep_sleep_default_config(fep_sleep_config_t* config) {
    if (!config) return;

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_default_config", 0.0f);


    config->n1_duration_ms = FEP_SLEEP_N1_DURATION_MS;
    config->n2_duration_ms = FEP_SLEEP_N2_DURATION_MS;
    config->sws_duration_ms = FEP_SLEEP_SWS_DURATION_MS;
    config->rem_duration_ms = FEP_SLEEP_REM_DURATION_MS;

    config->replays_per_cycle = FEP_SLEEP_DEFAULT_REPLAY_COUNT;
    config->downscale_factor = FEP_SLEEP_DEFAULT_DOWNSCALE_FACTOR;
    config->experience_buffer_size = FEP_SLEEP_DEFAULT_BUFFER_SIZE;

    config->enable_auto_cycle = true;
    config->enable_synaptic_homeostasis = true;
    config->enable_replay_consolidation = true;
    config->enable_rem_integration = true;
}

fep_sleep_system_t* fep_sleep_create(const fep_sleep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_create", 0.0f);


    fep_sleep_system_t* sys = (fep_sleep_system_t*)nimcp_calloc(
        1, sizeof(fep_sleep_system_t));
    if (!sys) {
        NIMCP_LOGGING_ERROR("Failed to allocate sleep system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate sys");

        return NULL;
    }

    /* Apply configuration */
    fep_sleep_config_t default_cfg;
    if (!config) {
        fep_sleep_default_config(&default_cfg);
        config = &default_cfg;
    }
    sys->config = *config;

    /* Initialize state */
    sys->state.current_stage = SLEEP_STAGE_WAKE;
    sys->state.stage_start_ms = get_time_ms();
    sys->state.total_sleep_ms = 0;
    sys->state.cycle_count = 0;
    sys->state.sleep_efficiency = 1.0f;
    sys->state.consolidation_quality = 0.0f;

    /* Allocate experience buffer */
    sys->buffer_capacity = config->experience_buffer_size;
    sys->experience_buffer = (fep_experience_t*)nimcp_calloc(
        sys->buffer_capacity, sizeof(fep_experience_t));
    if (!sys->experience_buffer) {
        fep_sleep_destroy(sys);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_sleep_create: sys->experience_buffer is NULL");
        return NULL;
    }

    /* Create mutex */
    sys->mutex = nimcp_platform_mutex_create();
    if (!sys->mutex) {
        fep_sleep_destroy(sys);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_sleep_create: sys->mutex is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Sleep system created: buffer_size=%zu", config->experience_buffer_size);
    return sys;
}

void fep_sleep_destroy(fep_sleep_system_t* sys) {
    if (!sys) return;

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_destroy", 0.0f);


    if (sys->bio_async_enabled) {
        fep_sleep_disconnect_bio_async(sys);
    }

    /* Free experience buffer */
    if (sys->experience_buffer) {
        for (size_t i = 0; i < sys->buffer_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && sys->buffer_count > 256) {
                fep_sleep_heartbeat("fep_sleep_loop",
                                 (float)(i + 1) / (float)sys->buffer_count);
            }

            free_experience(&sys->experience_buffer[i]);
        }
        nimcp_free(sys->experience_buffer);
    }

    if (sys->mutex) {
        nimcp_platform_mutex_destroy(sys->mutex);
        nimcp_free(sys->mutex);
        sys->mutex = NULL;
    }

    nimcp_free(sys);
    sys = NULL;
    NIMCP_LOGGING_INFO("Sleep system destroyed");
}

/* ============================================================================
 * Sleep Stage Implementation
 * ============================================================================ */

int fep_sleep_set_stage(fep_sleep_system_t* sys, fep_sleep_stage_t stage) {
    if (!sys) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_set_stage", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    fep_sleep_stage_t prev_stage = sys->state.current_stage;
    uint64_t now = get_time_ms();
    uint64_t stage_duration = now - sys->state.stage_start_ms;

    /* Update time tracking for previous stage */
    switch (prev_stage) {
        case SLEEP_STAGE_WAKE:
            sys->stats.total_wake_time_ms += stage_duration;
            break;
        case SLEEP_STAGE_N1:
            sys->stats.total_n1_time_ms += stage_duration;
            sys->state.total_sleep_ms += stage_duration;
            break;
        case SLEEP_STAGE_N2:
            sys->stats.total_n2_time_ms += stage_duration;
            sys->state.total_sleep_ms += stage_duration;
            break;
        case SLEEP_STAGE_SWS:
            sys->stats.total_sws_time_ms += stage_duration;
            sys->state.total_sleep_ms += stage_duration;
            break;
        case SLEEP_STAGE_REM:
            sys->stats.total_rem_time_ms += stage_duration;
            sys->state.total_sleep_ms += stage_duration;
            /* Check for complete cycle */
            sys->state.cycle_count++;
            sys->stats.total_cycles++;
            break;
    }

    /* Set new stage */
    sys->state.current_stage = stage;
    sys->state.stage_start_ms = now;
    sys->state.replays_this_cycle = 0;

    NIMCP_LOGGING_INFO("Sleep stage transition: %s -> %s",
                      fep_sleep_stage_to_string(prev_stage),
                      fep_sleep_stage_to_string(stage));

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

fep_sleep_stage_t fep_sleep_get_stage(const fep_sleep_system_t* sys) {
    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_get_stage", 0.0f);

    if (!sys) return SLEEP_STAGE_WAKE;
    nimcp_platform_mutex_lock(((fep_sleep_system_t*)sys)->mutex);
    fep_sleep_stage_t stage = sys->state.current_stage;
    nimcp_platform_mutex_unlock(((fep_sleep_system_t*)sys)->mutex);
    return stage;
}

int fep_sleep_update(fep_sleep_system_t* sys, uint64_t delta_ms) {
    if (!sys) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_update", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* Update time tracking for current stage based on delta_ms */
    fep_sleep_stage_t current = sys->state.current_stage;
    switch (current) {
        case SLEEP_STAGE_WAKE:
            sys->stats.total_wake_time_ms += delta_ms;
            break;
        case SLEEP_STAGE_N1:
            sys->stats.total_n1_time_ms += delta_ms;
            sys->state.total_sleep_ms += delta_ms;
            break;
        case SLEEP_STAGE_N2:
            sys->stats.total_n2_time_ms += delta_ms;
            sys->state.total_sleep_ms += delta_ms;
            break;
        case SLEEP_STAGE_SWS:
            sys->stats.total_sws_time_ms += delta_ms;
            sys->state.total_sleep_ms += delta_ms;
            break;
        case SLEEP_STAGE_REM:
            sys->stats.total_rem_time_ms += delta_ms;
            sys->state.total_sleep_ms += delta_ms;
            break;
    }

    uint64_t now = get_time_ms();
    uint64_t stage_elapsed = now - sys->state.stage_start_ms;

    /* Auto-cycle through stages if enabled */
    if (sys->config.enable_auto_cycle && current != SLEEP_STAGE_WAKE) {
        uint32_t stage_duration = get_stage_duration(sys, current);

        if (stage_elapsed >= stage_duration) {
            fep_sleep_stage_t next = get_next_stage(current);

            /* Run stage-specific consolidation before transitioning */
            if (current == SLEEP_STAGE_SWS && sys->config.enable_replay_consolidation) {
                if (sys->fep_system) {
                    fep_sleep_replay_consolidation(sys, sys->fep_system,
                                                   sys->config.replays_per_cycle);
                }

                if (sys->config.enable_synaptic_homeostasis && sys->fep_system) {
                    fep_sleep_apply_downscaling(sys, sys->fep_system,
                                               sys->config.downscale_factor);
                }
            }

            if (current == SLEEP_STAGE_REM && sys->config.enable_rem_integration) {
                if (sys->fep_system) {
                    fep_sleep_rem_integration(sys, sys->fep_system);
                }
            }

            nimcp_platform_mutex_unlock(sys->mutex);
            fep_sleep_set_stage(sys, next);
            return 0;
        }
    }

    /* Update consolidation progress */
    if (current == SLEEP_STAGE_SWS) {
        uint32_t stage_duration = get_stage_duration(sys, current);
        sys->state.consolidation_progress = (float)stage_elapsed / (float)stage_duration;
    }

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

/* ============================================================================
 * Consolidation Implementation
 * ============================================================================ */

int fep_sleep_add_experience(
    fep_sleep_system_t* sys,
    const float* state,
    const float* observation,
    const float* next_state,
    size_t dim,
    size_t obs_dim
) {
    if (!sys || !state || !observation || !next_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_sleep_add_experience: required parameter is NULL (sys, state, observation, next_state)");
        return -1;
    }
    if (dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_sleep_add_experience: dim is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_add_experience", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* Check buffer capacity */
    if (sys->buffer_count >= sys->buffer_capacity) {
        /* Overwrite oldest experience (FIFO) */
        free_experience(&sys->experience_buffer[0]);
        memmove(sys->experience_buffer, sys->experience_buffer + 1,
                (sys->buffer_count - 1) * sizeof(fep_experience_t));
        sys->buffer_count--;
    }

    /* Add new experience */
    fep_experience_t* exp = &sys->experience_buffer[sys->buffer_count];

    exp->state = (float*)nimcp_calloc(dim, sizeof(float));
    if (!exp->state) {
        nimcp_platform_mutex_unlock(sys->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_sleep_add_experience: state alloc failed");
        return -1;
    }
    exp->next_state = (float*)nimcp_calloc(dim, sizeof(float));
    if (!exp->next_state) {
        nimcp_free(exp->state);
        exp->state = NULL;
        nimcp_platform_mutex_unlock(sys->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_sleep_add_experience: next_state alloc failed");
        return -1;
    }
    if (observation && obs_dim > 0) {
        exp->observation = (float*)nimcp_calloc(obs_dim, sizeof(float));
        if (!exp->observation) {
            nimcp_free(exp->state);
            exp->state = NULL;
            nimcp_free(exp->next_state);
            exp->next_state = NULL;
            nimcp_platform_mutex_unlock(sys->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_sleep_add_experience: observation alloc failed");
            return -1;
        }
    }

    if (!exp->state || !exp->next_state) {
        free_experience(exp);
        nimcp_platform_mutex_unlock(sys->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_sleep_add_experience: required parameter is NULL (exp->state, exp->next_state)");
        return -1;
    }

    memcpy(exp->state, state, dim * sizeof(float));
    memcpy(exp->next_state, next_state, dim * sizeof(float));
    if (observation && exp->observation) {
        memcpy(exp->observation, observation, obs_dim * sizeof(float));
    }

    exp->dim = (uint32_t)dim;
    exp->obs_dim = observation ? (uint32_t)obs_dim : 0;
    exp->importance = 1.0f;  /* Default importance */

    sys->buffer_count++;

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_sleep_replay_consolidation(
    fep_sleep_system_t* sys,
    fep_system_t* fep,
    uint32_t num_replays
) {
    if (!sys || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_sleep_replay_consolidation: required parameter is NULL (sys, fep)");
        return -1;
    }
    if (sys->buffer_count == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_replay_consolidation", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* Replay experiences and update FEP model */
    uint32_t replays_done = 0;
    float total_error = 0.0f;

    for (uint32_t r = 0; r < num_replays && r < sys->buffer_count; r++) {
        /* Sample experience (simple: sequential, could use prioritized) */
        size_t idx = (size_t)(nimcp_tl_rand() % sys->buffer_count);
        fep_experience_t* exp = &sys->experience_buffer[idx];

        /* Compute prediction error */
        if (fep->num_levels > 0 && exp->dim > 0) {
            fep_hierarchy_level_t* level = &fep->levels[0];
            size_t dim = exp->dim < level->beliefs.dim ? exp->dim : level->beliefs.dim;

            float error = 0.0f;
            for (size_t i = 0; i < dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && dim > 256) {
                    fep_sleep_heartbeat("fep_sleep_loop",
                                     (float)(i + 1) / (float)dim);
                }

                float pred = level->beliefs.mean[i];
                float actual = exp->next_state[i];
                float diff = actual - pred;
                error += diff * diff;

                /* Update belief toward actual (learning during replay) */
                float lr = NIMCP_LEARNING_RATE_DEFAULT;  /* Sleep learning rate */
                level->beliefs.mean[i] += lr * diff;
            }
            total_error += sqrtf(error / (float)dim);
        }

        replays_done++;
    }

    sys->state.replays_this_cycle += replays_done;
    sys->stats.total_replays += replays_done;

    /* Update consolidation quality based on error reduction */
    if (replays_done > 0) {
        float avg_error = total_error / (float)replays_done;
        sys->state.consolidation_quality = 1.0f / (1.0f + avg_error);
    }

    NIMCP_LOGGING_DEBUG("Replay consolidation: %u replays, avg_error=%.4f",
                       replays_done, total_error / (float)replays_done);

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_sleep_apply_downscaling(
    fep_sleep_system_t* sys,
    fep_system_t* fep,
    float factor
) {
    if (!sys || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_sleep_apply_downscaling: required parameter is NULL (sys, fep)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_apply_downscaling", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    factor = nimcp_clampf(factor, 0.5f, 1.0f);

    /* Apply synaptic downscaling to all FEP levels */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && fep->num_levels > 256) {
            fep_sleep_heartbeat("fep_sleep_loop",
                             (float)(l + 1) / (float)fep->num_levels);
        }

        fep_hierarchy_level_t* level = &fep->levels[l];

        /* Downscale precision (inverse of variance) */
        for (uint32_t i = 0; i < level->errors.dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && level->errors.dim > 256) {
                fep_sleep_heartbeat("fep_sleep_loop",
                                 (float)(i + 1) / (float)level->errors.dim);
            }

            level->errors.precision[i] *= factor;
        }

        /* Downscale belief variance */
        for (uint32_t i = 0; i < level->beliefs.dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && level->beliefs.dim > 256) {
                fep_sleep_heartbeat("fep_sleep_loop",
                                 (float)(i + 1) / (float)level->beliefs.dim);
            }

            level->beliefs.variance[i] /= factor;
        }
    }

    sys->state.total_downscaling += (1.0f - factor);
    sys->stats.avg_downscaling =
        (sys->stats.avg_downscaling * 0.9f) + ((1.0f - factor) * 0.1f);

    NIMCP_LOGGING_DEBUG("Synaptic downscaling applied: factor=%.3f", factor);

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_sleep_rem_integration(
    fep_sleep_system_t* sys,
    fep_system_t* fep
) {
    if (!sys || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_sleep_rem_integration: required parameter is NULL (sys, fep)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_rem_integration", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* REM integration: Creative recombination and abstraction */
    /* This is a simplified version - full implementation would involve
     * hippocampal-cortical transfer and schema extraction */

    if (fep->num_levels > 1) {
        /* Propagate lower-level patterns to higher levels */
        fep_hierarchy_level_t* lower = &fep->levels[0];
        fep_hierarchy_level_t* higher = &fep->levels[1];

        size_t dim = lower->beliefs.dim < higher->beliefs.dim ?
                     lower->beliefs.dim : higher->beliefs.dim;

        /* Abstract/generalize lower-level beliefs to higher level */
        float smoothing = 0.1f;
        for (size_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                fep_sleep_heartbeat("fep_sleep_loop",
                                 (float)(i + 1) / (float)dim);
            }

            float lower_mean = lower->beliefs.mean[i];
            higher->beliefs.mean[i] =
                (1.0f - smoothing) * higher->beliefs.mean[i] +
                smoothing * lower_mean;
        }
    }

    /* Update model improvement metric */
    float avg_precision = 0.0f;
    uint32_t total_dims = 0;
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && fep->num_levels > 256) {
            fep_sleep_heartbeat("fep_sleep_loop",
                             (float)(l + 1) / (float)fep->num_levels);
        }

        for (uint32_t i = 0; i < fep->levels[l].errors.dim; i++) {
            avg_precision += fep->levels[l].errors.precision[i];
            total_dims++;
        }
    }
    if (total_dims > 0) {
        sys->stats.model_improvement = avg_precision / (float)total_dims;
    }

    NIMCP_LOGGING_DEBUG("REM integration completed");

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

/* ============================================================================
 * State/Stats Implementation
 * ============================================================================ */

int fep_sleep_get_state(
    const fep_sleep_system_t* sys,
    fep_sleep_state_t* state
) {
    if (!sys || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_sleep_get_state: required parameter is NULL (sys, state)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_get_state", 0.0f);

    nimcp_platform_mutex_lock(((fep_sleep_system_t*)sys)->mutex);
    *state = sys->state;
    nimcp_platform_mutex_unlock(((fep_sleep_system_t*)sys)->mutex);

    return 0;
}

int fep_sleep_get_stats(
    const fep_sleep_system_t* sys,
    fep_sleep_stats_t* stats
) {
    if (!sys || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_sleep_get_stats: required parameter is NULL (sys, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_get_stats", 0.0f);

    nimcp_platform_mutex_lock(((fep_sleep_system_t*)sys)->mutex);
    *stats = sys->stats;
    nimcp_platform_mutex_unlock(((fep_sleep_system_t*)sys)->mutex);

    return 0;
}

float fep_sleep_get_precision_modifier(const fep_sleep_system_t* sys) {
    if (!sys) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_get_precision_modifi", 0.0f);

    nimcp_platform_mutex_lock(((fep_sleep_system_t*)sys)->mutex);
    fep_sleep_stage_t stage = sys->state.current_stage;
    nimcp_platform_mutex_unlock(((fep_sleep_system_t*)sys)->mutex);

    switch (stage) {
        case SLEEP_STAGE_WAKE: return FEP_SLEEP_WAKE_PRECISION;
        case SLEEP_STAGE_N1:   return FEP_SLEEP_N1_PRECISION;
        case SLEEP_STAGE_N2:   return FEP_SLEEP_N2_PRECISION;
        case SLEEP_STAGE_SWS:  return FEP_SLEEP_SWS_PRECISION;
        case SLEEP_STAGE_REM:  return FEP_SLEEP_REM_PRECISION;
        default:               return 1.0f;
    }
}

/* ============================================================================
 * Integration Implementation
 * ============================================================================ */

int fep_sleep_connect(fep_sleep_system_t* sleep, fep_system_t* fep) {
    if (!sleep || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_sleep_connect: required parameter is NULL (sleep, fep)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_connect", 0.0f);


    nimcp_platform_mutex_lock(sleep->mutex);
    sleep->fep_system = fep;
    nimcp_platform_mutex_unlock(sleep->mutex);

    NIMCP_LOGGING_INFO("Sleep system connected to FEP");
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int fep_sleep_connect_bio_async(fep_sleep_system_t* sys) {
    if (!sys) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return -1;

    }
    if (sys->bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_connect_bio_async", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_SLEEP,
        .module_name = "fep_sleep",
        .inbox_capacity = 32,
        .user_data = sys
    };

    sys->bio_ctx = bio_router_register_module(&info);
    if (sys->bio_ctx) {
        sys->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Sleep connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }
    return 0;
}

int fep_sleep_disconnect_bio_async(fep_sleep_system_t* sys) {
    if (!sys) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return -1;

    }
    if (!sys->bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_disconnect_bio_async", 0.0f);


    if (sys->bio_ctx) {
        bio_router_unregister_module(sys->bio_ctx);
        sys->bio_ctx = NULL;
    }
    sys->bio_async_enabled = false;
    return 0;
}

bool fep_sleep_is_bio_async_connected(const fep_sleep_system_t* sys) {
    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_is_bio_async_connect", 0.0f);


    return sys && sys->bio_async_enabled;
}

/* ============================================================================
 * FEP → Sleep Bidirectional Integration Implementation
 * ============================================================================ */

int fep_sleep_on_prediction_error(fep_sleep_system_t* sys, float prediction_error) {
    if (!sys) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_on_prediction_error", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* Accumulate prediction error */
    sys->pressure.accumulated_prediction_error += prediction_error;

    /* Add to sleep pressure (scaled by gain) */
    float pressure_contribution = prediction_error * FEP_SLEEP_PE_PRESSURE_GAIN;
    sys->pressure.sleep_pressure += pressure_contribution;

    /* Clamp pressure to bounds */
    sys->pressure.sleep_pressure = nimcp_clampf(
        sys->pressure.sleep_pressure,
        FEP_SLEEP_MIN_PRESSURE,
        FEP_SLEEP_MAX_PRESSURE
    );

    /* Track high prediction error events */
    if (prediction_error > FEP_SLEEP_HIGH_PE_THRESHOLD) {
        sys->pressure.high_pe_events++;
    }

    /* Update sleep recommendation */
    sys->pressure.sleep_recommended =
        (sys->pressure.sleep_pressure >= FEP_SLEEP_PRESSURE_THRESHOLD);

    NIMCP_LOGGING_DEBUG("FEP->Sleep: PE=%.4f, pressure=%.4f, recommended=%d",
                        prediction_error, sys->pressure.sleep_pressure,
                        sys->pressure.sleep_recommended);

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_sleep_on_uncertainty(fep_sleep_system_t* sys, float uncertainty) {
    if (!sys) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_on_uncertainty", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* Update running average of uncertainty */
    const float alpha = 0.1f;  /* Smoothing factor */
    sys->pressure.avg_uncertainty =
        (1.0f - alpha) * sys->pressure.avg_uncertainty + alpha * uncertainty;

    /* High uncertainty increases sleep pressure */
    if (uncertainty > 0.5f) {
        float pressure_contribution = (uncertainty - 0.5f) * FEP_SLEEP_UNCERTAINTY_PRESSURE;
        sys->pressure.sleep_pressure += pressure_contribution;
        sys->pressure.sleep_pressure = nimcp_clampf(
            sys->pressure.sleep_pressure,
            FEP_SLEEP_MIN_PRESSURE,
            FEP_SLEEP_MAX_PRESSURE
        );
    }

    /* Update sleep recommendation */
    sys->pressure.sleep_recommended =
        (sys->pressure.sleep_pressure >= FEP_SLEEP_PRESSURE_THRESHOLD);

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_sleep_on_convergence(fep_sleep_system_t* sys, bool converged, float convergence_quality) {
    if (!sys) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_on_convergence", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    sys->pressure.model_converged = converged;
    sys->pressure.convergence_quality = nimcp_clampf(convergence_quality, 0.0f, 1.0f);

    /* Good convergence is a good time for sleep (consolidation) */
    if (converged && convergence_quality > 0.8f) {
        /* Slight boost to sleep pressure - good time to consolidate */
        sys->pressure.sleep_pressure += 0.05f;
        sys->pressure.sleep_pressure = nimcp_clampf(
            sys->pressure.sleep_pressure,
            FEP_SLEEP_MIN_PRESSURE,
            FEP_SLEEP_MAX_PRESSURE
        );
    }

    /* Update sleep recommendation */
    sys->pressure.sleep_recommended =
        (sys->pressure.sleep_pressure >= FEP_SLEEP_PRESSURE_THRESHOLD);

    NIMCP_LOGGING_DEBUG("FEP->Sleep: convergence=%d, quality=%.4f, pressure=%.4f",
                        converged, convergence_quality, sys->pressure.sleep_pressure);

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

float fep_sleep_get_pressure(const fep_sleep_system_t* sys) {
    if (!sys) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_get_pressure", 0.0f);

    nimcp_platform_mutex_lock(((fep_sleep_system_t*)sys)->mutex);
    float pressure = sys->pressure.sleep_pressure;
    nimcp_platform_mutex_unlock(((fep_sleep_system_t*)sys)->mutex);
    return pressure;
}

int fep_sleep_get_pressure_state(fep_sleep_system_t* sys, fep_sleep_pressure_t* pressure) {
    if (!sys || !pressure) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_sleep_get_pressure_state: required parameter is NULL (sys, pressure)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_get_pressure_state", 0.0f);


    nimcp_platform_mutex_lock((nimcp_mutex_t*)sys->mutex);
    *pressure = sys->pressure;
    nimcp_platform_mutex_unlock((nimcp_mutex_t*)sys->mutex);

    return 0;
}

bool fep_sleep_is_sleep_recommended(const fep_sleep_system_t* sys) {
    if (!sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_sleep_is_sleep_recommended: sys is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_is_sleep_recommended", 0.0f);

    nimcp_platform_mutex_lock(((fep_sleep_system_t*)sys)->mutex);
    bool recommended = sys->pressure.sleep_recommended;
    nimcp_platform_mutex_unlock(((fep_sleep_system_t*)sys)->mutex);
    return recommended;
}

int fep_sleep_reset_pressure(fep_sleep_system_t* sys) {
    if (!sys) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_reset_pressure", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    memset(&sys->pressure, 0, sizeof(fep_sleep_pressure_t));

    NIMCP_LOGGING_INFO("FEP->Sleep: pressure reset");

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_sleep_update_pressure(fep_sleep_system_t* sys, uint64_t delta_ms) {
    if (!sys) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_update_pressure", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* Only accumulate wake time when actually awake */
    if (sys->state.current_stage == SLEEP_STAGE_WAKE) {
        sys->pressure.wake_duration_ms += delta_ms;

        /* Homeostatic sleep pressure increases with time awake */
        /* Approximately ~0.001 per ms = 3.6% per hour */
        float time_pressure = (float)delta_ms * FEP_SLEEP_PRESSURE_DECAY;
        sys->pressure.sleep_pressure += time_pressure;
        sys->pressure.sleep_pressure = nimcp_clampf(
            sys->pressure.sleep_pressure,
            FEP_SLEEP_MIN_PRESSURE,
            FEP_SLEEP_MAX_PRESSURE
        );
    }

    /* Update sleep recommendation */
    sys->pressure.sleep_recommended =
        (sys->pressure.sleep_pressure >= FEP_SLEEP_PRESSURE_THRESHOLD);

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

/* ============================================================================
 * String Conversion Implementation
 * ============================================================================ */

const char* fep_sleep_stage_to_string(fep_sleep_stage_t stage) {
    switch (stage) {
        case SLEEP_STAGE_WAKE: return "WAKE";
        case SLEEP_STAGE_N1:   return "N1";
        case SLEEP_STAGE_N2:   return "N2";
        case SLEEP_STAGE_SWS:  return "SWS";
        case SLEEP_STAGE_REM:  return "REM";
        default:               return "UNKNOWN";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for FEP Sleep self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int fep_sleep_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    fep_sleep_heartbeat("fep_sleep_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "FEP_Sleep_System");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                fep_sleep_heartbeat("fep_sleep_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("FEP Sleep self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "FEP_Sleep_System");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "FEP_Sleep_System");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void fep_sleep_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_fep_sleep_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int fep_sleep_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_sleep_training_begin: NULL argument");
        return -1;
    }
    fep_sleep_heartbeat_instance(g_fep_sleep_health_agent, "fep_sleep_training_begin", 0.0f);
    return 0;
}

int fep_sleep_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_sleep_training_end: NULL argument");
        return -1;
    }
    fep_sleep_heartbeat_instance(g_fep_sleep_health_agent, "fep_sleep_training_end", 1.0f);
    return 0;
}

int fep_sleep_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_sleep_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    fep_sleep_heartbeat_instance(g_fep_sleep_health_agent, "fep_sleep_training_step", progress);
    return 0;
}
