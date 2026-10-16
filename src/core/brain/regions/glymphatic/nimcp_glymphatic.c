/**
 * @file nimcp_glymphatic.c
 * @brief Glymphatic System - Core Implementation
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Core glymphatic waste clearance system implementation
 * WHY:  Models brain waste clearance dynamics during sleep/wake cycles
 * HOW:  State machine driven by sleep state, with AQP4-mediated clearance,
 *       CSF flow modeling, and differential waste marker half-lives
 *
 * BIOLOGICAL BASIS:
 * - During NREM sleep, interstitial space expands ~60%, enabling 10-20x
 *   more efficient waste clearance than during wakefulness
 * - AQP4 water channels on astrocytic endfeet are essential for CSF
 *   exchange across the glymphatic pathway
 * - Beta-amyloid and tau protein are cleared at different rates
 * - Norepinephrine suppression during sleep permits space expansion
 *
 * @author NIMCP Development Team
 */

#include "core/brain/regions/glymphatic/nimcp_glymphatic.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <math.h>
#include <string.h>
#include <time.h>

#define LOG_MODULE "GLYMPHATIC"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(glymphatic, MESH_ADAPTER_CATEGORY_GLIAL)

/*=============================================================================
 * Local Helpers
 *===========================================================================*/

static float nimcp_clampf(float val, float lo, float hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/**
 * @brief Get current monotonic time in microseconds
 */
static uint64_t glymphatic_get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Validate system pointer and magic number
 */
static bool glymphatic_is_valid(const glymphatic_system_t* system) {
    return system && system->magic == GLYM_MAGIC;
}

/**
 * @brief Get sleep-state-dependent clearance multiplier
 */
static float glymphatic_sleep_multiplier(const glymphatic_system_t* system) {
    switch (system->current_sleep_state) {
        case GLYM_SLEEP_DEEP_NREM:
            return system->config.nrem_clearance_multiplier;
        case GLYM_SLEEP_REM:
            return system->config.rem_clearance_multiplier;
        case GLYM_SLEEP_LIGHT:
            return system->config.rem_clearance_multiplier * 0.5f;
        case GLYM_SLEEP_AWAKE:
        default:
            return system->config.awake_clearance_multiplier;
    }
}

/**
 * @brief Get sleep-state-dependent waste generation rate
 *
 * Waste generation is highest during wakefulness (active metabolism)
 * and lowest during deep NREM sleep.
 */
static float glymphatic_waste_gen_multiplier(uint32_t sleep_state) {
    switch (sleep_state) {
        case GLYM_SLEEP_AWAKE:     return 1.0f;
        case GLYM_SLEEP_LIGHT:     return 0.6f;
        case GLYM_SLEEP_REM:       return 0.4f;
        case GLYM_SLEEP_DEEP_NREM: return 0.2f;
        default:                    return 1.0f;
    }
}

/**
 * @brief Compute target interstitial space volume for current sleep state
 *
 * NREM sleep: 60% expansion (ISV = 1.6)
 * REM sleep:  30% expansion (ISV = 1.3)
 * Awake:      baseline (ISV = 1.0)
 */
static float glymphatic_target_isv(uint32_t sleep_state) {
    switch (sleep_state) {
        case GLYM_SLEEP_DEEP_NREM: return GLYM_ISV_NREM_EXPANSION;
        case GLYM_SLEEP_REM:       return 1.3f;
        case GLYM_SLEEP_LIGHT:     return 1.15f;
        case GLYM_SLEEP_AWAKE:
        default:                    return 1.0f;
    }
}

/*=============================================================================
 * Lifecycle Implementation
 *===========================================================================*/

glymphatic_config_t glymphatic_default_config(void) {
    glymphatic_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.base_clearance_rate       = GLYM_DEFAULT_BASE_CLEARANCE;
    cfg.waste_generation_rate     = GLYM_DEFAULT_WASTE_GEN_RATE;
    cfg.aqp4_expression           = GLYM_DEFAULT_AQP4_EXPRESSION;
    cfg.nrem_clearance_multiplier = GLYM_DEFAULT_NREM_MULTIPLIER;
    cfg.rem_clearance_multiplier  = GLYM_DEFAULT_REM_MULTIPLIER;
    cfg.awake_clearance_multiplier = GLYM_DEFAULT_AWAKE_MULTIPLIER;
    cfg.waste_alert_threshold     = GLYM_DEFAULT_WASTE_ALERT;

    return cfg;
}

glymphatic_system_t* glymphatic_create(const glymphatic_config_t* config) {
    glymphatic_system_t* system = (glymphatic_system_t*)nimcp_calloc(
        1, sizeof(glymphatic_system_t));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "glymphatic_create: failed to allocate glymphatic_system_t");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        system->config = glymphatic_default_config();
    }

    /* Initialize state */
    system->magic                 = GLYM_MAGIC;
    system->state                 = GLYM_INACTIVE;
    system->clearance_rate        = system->config.base_clearance_rate *
                                    system->config.awake_clearance_multiplier;
    system->waste_accumulation    = 0.0f;
    system->csf_flow_rate         = 0.0f;
    system->aquaporin4_expression = system->config.aqp4_expression;
    system->interstitial_space_volume = 1.0f;  /* baseline */
    system->beta_amyloid_level    = 0.0f;
    system->tau_protein_level     = 0.0f;
    system->metabolic_waste_level = 0.0f;
    system->fractal_dimension     = GLYM_HEALTHY_FRACTAL_DIM;
    system->quantum_tunneling_rate = 0.0f;
    system->current_sleep_state   = GLYM_SLEEP_AWAKE;
    system->last_update_us        = glymphatic_get_time_us();
    system->priming_elapsed_s     = 0.0f;

    /* Create mutex */
    system->lock = nimcp_mutex_create(NULL);
    if (!system->lock) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "glymphatic_create: failed to create mutex");
        nimcp_free(system);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Glymphatic system created: base_clearance=%.4f, "
                       "aqp4=%.2f, nrem_mult=%.1f",
                       system->config.base_clearance_rate,
                       system->aquaporin4_expression,
                       system->config.nrem_clearance_multiplier);

    return system;
}

void glymphatic_destroy(glymphatic_system_t* system) {
    if (!system) {
        return;
    }

    if (system->magic != GLYM_MAGIC) {
        NIMCP_LOGGING_WARN("glymphatic_destroy: invalid magic, possible double-free");
        return;
    }

    /* Invalidate magic before cleanup */
    system->magic = 0;

    if (system->lock) {
        nimcp_mutex_destroy(system->lock);
        system->lock = NULL;
    }

    nimcp_free(system);
    NIMCP_LOGGING_INFO("Glymphatic system destroyed");
}

/*=============================================================================
 * Core Operations Implementation
 *===========================================================================*/

int glymphatic_update(glymphatic_system_t* system, float dt_s) {
    if (!glymphatic_is_valid(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "glymphatic_update: system is NULL or invalid");
        return -1;
    }

    if (!isfinite(dt_s) || dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "glymphatic_update: dt_s must be positive and finite");
        return -1;
    }

    /* Cap dt_s to avoid huge jumps */
    if (dt_s > 10.0f) {
        dt_s = 10.0f;
    }

    nimcp_mutex_lock(system->lock);

    /* --- 1. Generate metabolic waste proportional to wake state --- */
    float waste_gen_mult = glymphatic_waste_gen_multiplier(system->current_sleep_state);
    float waste_generated = system->config.waste_generation_rate * waste_gen_mult * dt_s;

    /* Distribute across waste markers with different weights */
    float ba_gen = waste_generated * 0.35f;  /* Beta-amyloid: 35% of waste */
    float tau_gen = waste_generated * 0.25f; /* Tau: 25% of waste */
    float met_gen = waste_generated * 0.40f; /* Metabolic: 40% of waste */

    system->beta_amyloid_level  += ba_gen;
    system->tau_protein_level   += tau_gen;
    system->metabolic_waste_level += met_gen;

    /* --- 2. Compute clearance rate from sleep state + AQP4 --- */
    float sleep_mult = glymphatic_sleep_multiplier(system);
    float aqp4_factor = system->aquaporin4_expression; /* 0.0-1.0 */
    float effective_clearance = system->config.base_clearance_rate *
                                sleep_mult * aqp4_factor;

    /* Fractal dimension modulates clearance efficiency */
    float fractal_factor = system->fractal_dimension / GLYM_HEALTHY_FRACTAL_DIM;
    if (isfinite(fractal_factor)) {
        effective_clearance *= nimcp_clampf(fractal_factor, 0.1f, 2.0f);
    }

    /* Interstitial space volume amplifies clearance */
    effective_clearance *= system->interstitial_space_volume;

    system->clearance_rate = nimcp_clampf(effective_clearance, 0.0f, 1.0f);

    /* --- 3. CSF flow follows clearance rate --- */
    float target_csf = system->clearance_rate * 3.0f; /* ~3 mL/min at peak */
    float csf_tau = 0.1f; /* CSF flow time constant */
    float csf_alpha = 1.0f - expf(-dt_s / csf_tau);
    if (!isfinite(csf_alpha)) {
        csf_alpha = 1.0f;
    }
    system->csf_flow_rate += csf_alpha * (target_csf - system->csf_flow_rate);
    if (!isfinite(system->csf_flow_rate)) {
        system->csf_flow_rate = 0.0f;
    }

    /* --- 4. Interstitial space volume tracks toward target --- */
    float target_isv = glymphatic_target_isv(system->current_sleep_state);
    float isv_tau = 5.0f; /* Volume changes over ~5 seconds */
    float isv_alpha = 1.0f - expf(-dt_s / isv_tau);
    if (!isfinite(isv_alpha)) {
        isv_alpha = 1.0f;
    }
    system->interstitial_space_volume +=
        isv_alpha * (target_isv - system->interstitial_space_volume);
    system->interstitial_space_volume =
        nimcp_clampf(system->interstitial_space_volume, 0.5f, 2.0f);

    /* --- 5. Clear waste markers with different half-lives --- */
    float ba_clear  = effective_clearance * GLYM_BETA_AMYLOID_HALFLIFE * dt_s;
    float tau_clear = effective_clearance / GLYM_TAU_HALFLIFE * dt_s;
    float met_clear = effective_clearance / GLYM_METABOLIC_HALFLIFE * dt_s;

    system->beta_amyloid_level   -= ba_clear;
    system->tau_protein_level    -= tau_clear;
    system->metabolic_waste_level -= met_clear;

    /* Clamp waste markers to [0, 1] */
    system->beta_amyloid_level   = nimcp_clampf(system->beta_amyloid_level, 0.0f, 1.0f);
    system->tau_protein_level    = nimcp_clampf(system->tau_protein_level, 0.0f, 1.0f);
    system->metabolic_waste_level = nimcp_clampf(system->metabolic_waste_level, 0.0f, 1.0f);

    /* Composite waste accumulation is weighted average */
    system->waste_accumulation = (system->beta_amyloid_level * 0.4f +
                                  system->tau_protein_level * 0.35f +
                                  system->metabolic_waste_level * 0.25f);
    system->waste_accumulation = nimcp_clampf(system->waste_accumulation, 0.0f, 1.0f);

    /* --- 6. State machine transitions --- */
    switch (system->state) {
        case GLYM_INACTIVE:
            /* Transition to PRIMING when sleep detected */
            if (system->current_sleep_state != GLYM_SLEEP_AWAKE) {
                system->state = GLYM_PRIMING;
                system->priming_elapsed_s = 0.0f;
                NIMCP_LOGGING_DEBUG("Glymphatic: INACTIVE -> PRIMING (sleep detected)");
            }
            break;

        case GLYM_PRIMING:
            system->priming_elapsed_s += dt_s;
            /* Back to INACTIVE if we woke up */
            if (system->current_sleep_state == GLYM_SLEEP_AWAKE) {
                system->state = GLYM_INACTIVE;
                system->priming_elapsed_s = 0.0f;
                NIMCP_LOGGING_DEBUG("Glymphatic: PRIMING -> INACTIVE (woke up)");
            }
            /* Transition to ACTIVE after warmup period */
            else if (system->priming_elapsed_s >= GLYM_PRIMING_DURATION_S) {
                system->state = GLYM_ACTIVE;
                NIMCP_LOGGING_DEBUG("Glymphatic: PRIMING -> ACTIVE (warmup complete)");
            }
            break;

        case GLYM_ACTIVE:
            /* Back to INACTIVE if we woke up */
            if (system->current_sleep_state == GLYM_SLEEP_AWAKE) {
                system->state = GLYM_INACTIVE;
                NIMCP_LOGGING_DEBUG("Glymphatic: ACTIVE -> INACTIVE (woke up)");
            }
            /* Transition to FLUSHING when waste is low */
            else if (system->waste_accumulation < GLYM_FLUSHING_THRESHOLD) {
                system->state = GLYM_FLUSHING;
                NIMCP_LOGGING_DEBUG("Glymphatic: ACTIVE -> FLUSHING (waste=%.3f < %.3f)",
                                    system->waste_accumulation, GLYM_FLUSHING_THRESHOLD);
            }
            break;

        case GLYM_FLUSHING:
            /* Back to INACTIVE if we woke up */
            if (system->current_sleep_state == GLYM_SLEEP_AWAKE) {
                system->state = GLYM_INACTIVE;
                NIMCP_LOGGING_DEBUG("Glymphatic: FLUSHING -> INACTIVE (woke up)");
            }
            /* If waste rises again, go back to ACTIVE */
            else if (system->waste_accumulation >= GLYM_FLUSHING_THRESHOLD * 2.0f) {
                system->state = GLYM_ACTIVE;
                NIMCP_LOGGING_DEBUG("Glymphatic: FLUSHING -> ACTIVE (waste increased)");
            }
            break;
    }

    /* --- 7. Update timestamp --- */
    system->last_update_us = glymphatic_get_time_us();

    nimcp_mutex_unlock(system->lock);
    return 0;
}

int glymphatic_on_sleep_state_change(glymphatic_system_t* system,
                                      uint32_t sleep_state) {
    if (!glymphatic_is_valid(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "glymphatic_on_sleep_state_change: system is NULL or invalid");
        return -1;
    }

    if (sleep_state > GLYM_SLEEP_REM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "glymphatic_on_sleep_state_change: invalid sleep state %u",
            sleep_state);
        return -1;
    }

    nimcp_mutex_lock(system->lock);

    uint32_t old_state = system->current_sleep_state;
    system->current_sleep_state = sleep_state;

    /* Log transition */
    static const char* state_names[] = { "AWAKE", "LIGHT", "DEEP_NREM", "REM" };
    if (old_state != sleep_state) {
        NIMCP_LOGGING_INFO("Glymphatic: sleep state %s -> %s",
                           old_state <= GLYM_SLEEP_REM ? state_names[old_state] : "UNKNOWN",
                           sleep_state <= GLYM_SLEEP_REM ? state_names[sleep_state] : "UNKNOWN");
    }

    /* If transitioning to awake, reset AQP4 toward baseline */
    if (sleep_state == GLYM_SLEEP_AWAKE && old_state != GLYM_SLEEP_AWAKE) {
        /* AQP4 expression relaxes back toward config baseline when awake */
        system->aquaporin4_expression = system->config.aqp4_expression;
    }

    /* If entering deep NREM, boost AQP4 expression */
    if (sleep_state == GLYM_SLEEP_DEEP_NREM) {
        /* AQP4 upregulation during deep sleep */
        system->aquaporin4_expression = nimcp_clampf(
            system->config.aqp4_expression * 1.3f, 0.0f, 1.0f);
    }

    nimcp_mutex_unlock(system->lock);
    return 0;
}

int glymphatic_flush(glymphatic_system_t* system) {
    if (!glymphatic_is_valid(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "glymphatic_flush: system is NULL or invalid");
        return -1;
    }

    nimcp_mutex_lock(system->lock);

    system->state = GLYM_FLUSHING;

    /* Boost clearance rate for emergency flush */
    system->clearance_rate = nimcp_clampf(
        system->config.base_clearance_rate * system->config.nrem_clearance_multiplier *
        system->aquaporin4_expression * 1.5f,
        0.0f, 1.0f);

    /* Accelerated waste reduction */
    system->beta_amyloid_level   *= 0.5f;
    system->tau_protein_level    *= 0.6f;  /* Tau clears slower */
    system->metabolic_waste_level *= 0.3f; /* Metabolic clears fastest */

    /* Recalculate composite */
    system->waste_accumulation = (system->beta_amyloid_level * 0.4f +
                                  system->tau_protein_level * 0.35f +
                                  system->metabolic_waste_level * 0.25f);
    system->waste_accumulation = nimcp_clampf(system->waste_accumulation, 0.0f, 1.0f);

    NIMCP_LOGGING_INFO("Glymphatic: forced flush, waste now %.3f",
                       system->waste_accumulation);

    nimcp_mutex_unlock(system->lock);
    return 0;
}

/*=============================================================================
 * Query API Implementation
 *===========================================================================*/

float glymphatic_get_clearance_rate(const glymphatic_system_t* system) {
    if (!glymphatic_is_valid(system)) {
        return -1.0f;
    }
    return system->clearance_rate;
}

float glymphatic_get_waste_level(const glymphatic_system_t* system) {
    if (!glymphatic_is_valid(system)) {
        return -1.0f;
    }
    return system->waste_accumulation;
}

glymphatic_state_t glymphatic_get_state(const glymphatic_system_t* system) {
    if (!glymphatic_is_valid(system)) {
        return GLYM_INACTIVE;
    }
    return system->state;
}

float glymphatic_get_csf_flow(const glymphatic_system_t* system) {
    if (!glymphatic_is_valid(system)) {
        return 0.0f;
    }
    return system->csf_flow_rate;
}

float glymphatic_get_beta_amyloid(const glymphatic_system_t* system) {
    if (!glymphatic_is_valid(system)) {
        return -1.0f;
    }
    return system->beta_amyloid_level;
}

float glymphatic_get_tau_level(const glymphatic_system_t* system) {
    if (!glymphatic_is_valid(system)) {
        return -1.0f;
    }
    return system->tau_protein_level;
}

float glymphatic_get_interstitial_volume(const glymphatic_system_t* system) {
    if (!glymphatic_is_valid(system)) {
        return 0.0f;
    }
    return system->interstitial_space_volume;
}
