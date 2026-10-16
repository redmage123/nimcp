/**
 * @file nimcp_nitric_oxide.c
 * @brief Nitric Oxide Signaling Module Implementation
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "chemistry/gasotransmitters/nimcp_nitric_oxide.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(nitric_oxide)

//=============================================================================
// Internal Constants
//=============================================================================

/** Decay rate constant from half-life */
#define NO_DECAY_RATE (0.693f / NO_HALF_LIFE_SEC)

/** Minimum concentration threshold */
#define NO_MIN_CONCENTRATION 0.001f

/** Maximum NOS activity */
#define NOS_MAX_ACTIVITY 1.0f

/** Calcium threshold for nNOS activation (µM) */
#define NOS_CA_THRESHOLD 0.1f

/** cGMP production rate per NO (µM/nM/s) */
#define CGMP_PRODUCTION_RATE 0.1f

/** cGMP decay rate (1/s) */
#define CGMP_DECAY_RATE 0.5f

/** Potentiation sigmoid steepness */
#define POTENTIATION_SIGMOID_SLOPE 0.1f

/**
 * @brief Michaelis-Menten kinetics
 */
static float michaelis_menten(float substrate, float km, float vmax) {
    if (substrate <= 0.0f) return 0.0f;
    return (vmax * substrate) / (km + substrate);
}

/**
 * @brief Calculate 3D distance
 */
static float distance_3d(const float a[3], const float b[3]) {
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/**
 * @brief Calculate NO concentration at distance (Gaussian diffusion)
 */
static float no_at_distance(float source_conc, float distance, float effective_radius) {
    if (distance <= 0.0f) return source_conc;
    if (distance > effective_radius * 3.0f) return 0.0f;

    /* Gaussian decay with distance */
    float sigma = effective_radius / 2.0f;
    float decay = expf(-(distance * distance) / (2.0f * sigma * sigma));
    return source_conc * decay;
}

/**
 * @brief Initialize default configuration
 */
static void init_default_config(nimcp_no_config_t* config) {
    config->default_nos_type = NOS_TYPE_NNOS;
    config->nos_km_arginine = 3.0f;     /* µM */
    config->nos_km_calcium = 0.3f;       /* µM */
    config->nos_vmax = 1.0f;

    config->diffusion_coefficient = NO_DIFFUSION_COEFF;
    config->decay_rate = NO_DECAY_RATE;
    config->effective_radius = NO_DIFFUSION_RADIUS;

    config->gc_sensitivity = 1.0f;
    config->cgmp_decay_rate = CGMP_DECAY_RATE;
    config->pde_activity = 1.0f;

    config->potentiation_max = 2.0f;     /* 2x release probability */
    config->potentiation_threshold = 10.0f;  /* 10 nM NO */

    config->vasodilation_sensitivity = 1.0f;

    config->on_release = NULL;
    config->on_retrograde = NULL;
    config->callback_data = NULL;
}

/**
 * @brief Initialize source with defaults
 */
static void init_source_defaults(nimcp_no_source_t* source, uint32_t id) {
    memset(source, 0, sizeof(nimcp_no_source_t));

    source->id = id;
    source->nos_type = NOS_TYPE_NNOS;
    source->nos_activity = 0.0f;
    source->nos_expression = 1.0f;

    /* Default substrate levels */
    source->arginine_level = 100.0f;    /* µM - abundant */
    source->oxygen_level = 1.0f;        /* Normal oxygenation */
    source->bh4_level = 1.0f;           /* Normal cofactor */

    source->no_concentration = NO_BASAL_CONCENTRATION;
    source->state = NO_STATE_BASAL;
    source->initialized = true;
}

/**
 * @brief Calculate NOS activity based on Ca2+ and other factors
 */
static float calculate_nos_activity(
    const nimcp_no_source_t* source,
    const nimcp_no_config_t* config
) {
    /* nNOS requires Ca2+/calmodulin */
    if (source->nos_type == NOS_TYPE_NNOS) {
        /* Ca2+ activation (cooperative binding to calmodulin) */
        float ca_factor = source->calcium_level * source->calcium_level /
                         (config->nos_km_calcium * config->nos_km_calcium +
                          source->calcium_level * source->calcium_level);

        /* NMDA receptor contribution */
        float nmda_factor = source->nmda_activation;

        /* Combined activation */
        float activation = ca_factor * (0.3f + 0.7f * nmda_factor);

        /* Substrate availability */
        float arginine_factor = michaelis_menten(source->arginine_level,
                                                 config->nos_km_arginine, 1.0f);

        /* Cofactor and oxygen requirements */
        float substrate_factor = arginine_factor * source->oxygen_level * source->bh4_level;

        return nimcp_clampf(activation * substrate_factor * source->nos_expression,
                     0.0f, NOS_MAX_ACTIVITY);
    }
    /* iNOS - constitutively active when expressed */
    else if (source->nos_type == NOS_TYPE_INOS) {
        float substrate_factor = michaelis_menten(source->arginine_level,
                                                 config->nos_km_arginine, 1.0f);
        return nimcp_clampf(source->nos_expression * substrate_factor,
                     0.0f, NOS_MAX_ACTIVITY);
    }
    /* eNOS - Ca2+/calmodulin and phosphorylation dependent */
    else if (source->nos_type == NOS_TYPE_ENOS) {
        float ca_factor = source->calcium_level /
                         (config->nos_km_calcium + source->calcium_level);
        float substrate_factor = michaelis_menten(source->arginine_level,
                                                 config->nos_km_arginine, 1.0f);
        return nimcp_clampf(ca_factor * substrate_factor * source->nos_expression,
                     0.0f, NOS_MAX_ACTIVITY);
    }

    return 0.0f;
}

/**
 * @brief Determine NO state from activity and concentration
 */
static nimcp_no_state_t determine_no_state(float activity, float concentration) {
    if (activity < 0.01f && concentration < NO_BASAL_CONCENTRATION * 2.0f) {
        return NO_STATE_INACTIVE;
    } else if (activity < 0.1f) {
        return NO_STATE_BASAL;
    } else if (concentration > NO_PEAK_CONCENTRATION * 0.8f) {
        return NO_STATE_PATHOLOGICAL;
    } else if (activity > 0.5f) {
        return NO_STATE_ACTIVATED;
    } else {
        return NO_STATE_SUSTAINED;
    }
}

/**
 * @brief Calculate potentiation factor from NO concentration
 */
static float calculate_potentiation(
    float no_conc,
    float max_potentiation,
    float threshold
) {
    if (no_conc <= 0.0f) return 1.0f;

    /* Sigmoidal response */
    float x = (no_conc - threshold) * POTENTIATION_SIGMOID_SLOPE;
    float sigmoid = 1.0f / (1.0f + expf(-x));

    return 1.0f + (max_potentiation - 1.0f) * sigmoid;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

nimcp_no_error_t nimcp_no_init(
    nimcp_no_system_t* system,
    const nimcp_no_config_t* config
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Nitric oxide system is NULL");
        return NO_ERR_NULL_PTR;
    }

    memset(system, 0, sizeof(nimcp_no_system_t));

    /* Apply configuration */
    if (config) {
        memcpy(&system->config, config, sizeof(nimcp_no_config_t));
    } else {
        init_default_config(&system->config);
    }

    /* Initialize global effects */
    system->global_no_level = NO_BASAL_CONCENTRATION;
    system->global_cgmp_level = CGMP_BASAL_CONCENTRATION;
    system->vasodilation_factor = 1.0f;
    system->plasticity_modifier = 1.0f;

    system->initialized = true;
    system->update_count = 0;

    return NO_OK;
}

nimcp_no_error_t nimcp_no_shutdown(nimcp_no_system_t* system) {
    if (!system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Nitric oxide system is NULL in shutdown");
        return NO_ERR_NULL_PTR;
    }

    memset(system, 0, sizeof(nimcp_no_system_t));

    return NO_OK;
}

nimcp_no_error_t nimcp_no_reset(nimcp_no_system_t* system) {
    if (!system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Nitric oxide system is NULL in reset");
        return NO_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        NIMCP_THROW(NIMCP_ERROR_NOT_INITIALIZED, "Nitric oxide system not initialized in reset");
        return NO_ERR_NOT_INITIALIZED;
    }

    /* Reset all sources */
    for (uint32_t i = 0; i < system->num_sources; i++) {
        nimcp_no_source_t* source = &system->sources[i];
        source->nos_activity = 0.0f;
        source->calcium_level = 0.0f;
        source->calmodulin_bound = 0.0f;
        source->nmda_activation = 0.0f;
        source->no_production_rate = 0.0f;
        source->no_concentration = NO_BASAL_CONCENTRATION;
        source->state = NO_STATE_BASAL;

        /* Reset targets */
        for (uint32_t j = 0; j < source->num_targets; j++) {
            source->targets[j].no_concentration = 0.0f;
            source->targets[j].cgmp_concentration = CGMP_BASAL_CONCENTRATION;
            source->targets[j].potentiation_factor = 1.0f;
        }
    }

    /* Reset global state */
    system->global_no_level = NO_BASAL_CONCENTRATION;
    system->global_cgmp_level = CGMP_BASAL_CONCENTRATION;
    system->vasodilation_factor = 1.0f;
    system->plasticity_modifier = 1.0f;

    /* Reset metrics */
    memset(&system->metrics, 0, sizeof(nimcp_no_metrics_t));

    system->update_count = 0;

    return NO_OK;
}

//=============================================================================
// Source Management API Implementation
//=============================================================================

nimcp_no_error_t nimcp_no_add_source(
    nimcp_no_system_t* system,
    const float position[3],
    nimcp_nos_type_t nos_type,
    uint32_t* source_id
) {
    if (!system || !position || !source_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NO add_source: NULL argument");
        return NO_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "NO system not initialized in add_source");
        return NO_ERR_NOT_INITIALIZED;
    }

    if (system->num_sources >= NO_MAX_SOURCES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "NO source capacity exceeded");
        return NO_ERR_CAPACITY_EXCEEDED;
    }

    uint32_t id = system->num_sources;
    nimcp_no_source_t* source = &system->sources[id];

    init_source_defaults(source, id);
    source->nos_type = nos_type;
    source->position[0] = position[0];
    source->position[1] = position[1];
    source->position[2] = position[2];

    system->num_sources++;
    *source_id = id;

    system->metrics.total_sources++;

    return NO_OK;
}

nimcp_no_source_t* nimcp_no_get_source(
    nimcp_no_system_t* system,
    uint32_t source_id
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_no_get_source: required parameter is NULL (system, system->initialized)");
        return NULL;
    }

    if (source_id >= system->num_sources) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_no_get_source: capacity exceeded");
        return NULL;
    }

    return &system->sources[source_id];
}

nimcp_no_error_t nimcp_no_remove_source(
    nimcp_no_system_t* system,
    uint32_t source_id
) {
    if (!system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO remove_source: system is NULL");
        return NO_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        NIMCP_THROW(NIMCP_ERROR_NOT_INITIALIZED, "NO system not initialized in remove_source");
        return NO_ERR_NOT_INITIALIZED;
    }

    if (source_id >= system->num_sources) {
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "NO source_id %u not found (max: %u)", source_id, system->num_sources);
        return NO_ERR_SOURCE_NOT_FOUND;
    }

    /* Shift sources down */
    for (uint32_t i = source_id; i < system->num_sources - 1; i++) {
        memcpy(&system->sources[i], &system->sources[i + 1], sizeof(nimcp_no_source_t));
        system->sources[i].id = i;
    }

    system->num_sources--;

    return NO_OK;
}

//=============================================================================
// Target Management API Implementation
//=============================================================================

nimcp_no_error_t nimcp_no_add_target(
    nimcp_no_source_t* source,
    uint32_t synapse_id,
    float distance,
    nimcp_no_retrograde_mode_t mode
) {
    if (!source) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO add_target: source is NULL");
        return NO_ERR_NULL_PTR;
    }

    if (source->num_targets >= NO_MAX_TARGETS_PER_SOURCE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "NO target capacity exceeded: max %d", NO_MAX_TARGETS_PER_SOURCE);
        return NO_ERR_CAPACITY_EXCEEDED;
    }

    nimcp_no_target_synapse_t* target = &source->targets[source->num_targets];
    memset(target, 0, sizeof(nimcp_no_target_synapse_t));

    target->synapse_id = synapse_id;
    target->distance = distance;
    target->mode = mode;
    target->cgmp_concentration = CGMP_BASAL_CONCENTRATION;
    target->potentiation_factor = 1.0f;
    target->active = true;

    source->num_targets++;

    return NO_OK;
}

nimcp_no_error_t nimcp_no_remove_target(
    nimcp_no_source_t* source,
    uint32_t synapse_id
) {
    if (!source) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO remove_target: source is NULL");
        return NO_ERR_NULL_PTR;
    }

    /* Find and remove target */
    for (uint32_t i = 0; i < source->num_targets; i++) {
        if (source->targets[i].synapse_id == synapse_id) {
            /* Shift targets down */
            for (uint32_t j = i; j < source->num_targets - 1; j++) {
                memcpy(&source->targets[j], &source->targets[j + 1],
                       sizeof(nimcp_no_target_synapse_t));
            }
            source->num_targets--;
            return NO_OK;
        }
    }

    return NO_ERR_SOURCE_NOT_FOUND;
}

nimcp_no_error_t nimcp_no_get_target_potentiation(
    const nimcp_no_source_t* source,
    uint32_t synapse_id,
    float* potentiation
) {
    if (!source || !potentiation) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO get_target_potentiation: NULL argument");
        return NO_ERR_NULL_PTR;
    }

    for (uint32_t i = 0; i < source->num_targets; i++) {
        if (source->targets[i].synapse_id == synapse_id) {
            *potentiation = source->targets[i].potentiation_factor;
            return NO_OK;
        }
    }

    return NO_ERR_SOURCE_NOT_FOUND;
}

//=============================================================================
// NOS Activation API Implementation
//=============================================================================

nimcp_no_error_t nimcp_no_set_calcium(
    nimcp_no_source_t* source,
    float calcium_um
) {
    if (!source) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO set_calcium: source is NULL");
        return NO_ERR_NULL_PTR;
    }

    source->calcium_level = nimcp_clampf(calcium_um, 0.0f, 100.0f);

    /* Update calmodulin binding */
    float ca_for_cam = source->calcium_level;
    source->calmodulin_bound = ca_for_cam * ca_for_cam /
                              (0.3f * 0.3f + ca_for_cam * ca_for_cam);

    return NO_OK;
}

nimcp_no_error_t nimcp_no_set_nmda_activation(
    nimcp_no_source_t* source,
    float activation
) {
    if (!source) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO set_nmda_activation: source is NULL");
        return NO_ERR_NULL_PTR;
    }

    source->nmda_activation = nimcp_clampf(activation, 0.0f, 1.0f);

    return NO_OK;
}

nimcp_no_error_t nimcp_no_set_substrate(
    nimcp_no_source_t* source,
    float arginine,
    float oxygen,
    float bh4
) {
    if (!source) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO set_substrate: source is NULL");
        return NO_ERR_NULL_PTR;
    }

    source->arginine_level = nimcp_clampf(arginine, 0.0f, 1000.0f);
    source->oxygen_level = nimcp_clampf(oxygen, 0.0f, 1.0f);
    source->bh4_level = nimcp_clampf(bh4, 0.0f, 1.0f);

    return NO_OK;
}

nimcp_no_error_t nimcp_no_get_nos_activity(
    const nimcp_no_source_t* source,
    float* activity
) {
    if (!source || !activity) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO get_nos_activity: NULL argument");
        return NO_ERR_NULL_PTR;
    }

    *activity = source->nos_activity;

    return NO_OK;
}

//=============================================================================
// Update API Implementation
//=============================================================================

nimcp_no_error_t nimcp_no_update_source(
    nimcp_no_system_t* system,
    nimcp_no_source_t* source,
    float dt
) {
    if (!system || !source) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO update_source: NULL argument");
        return NO_ERR_NULL_PTR;
    }

    float dt_sec = dt / 1000.0f;

    /* Calculate NOS activity */
    float new_activity = calculate_nos_activity(source, &system->config);

    /* Smooth activity transition */
    float activity_tau = NOS_ACTIVATION_TAU;
    source->nos_activity += (new_activity - source->nos_activity) *
                           (1.0f - expf(-dt / activity_tau));

    /* Calculate NO production rate */
    source->no_production_rate = source->nos_activity * system->config.nos_vmax * 100.0f;

    /* Update NO concentration */
    float production = source->no_production_rate * dt_sec;
    float decay = source->no_concentration * system->config.decay_rate * dt_sec;
    source->no_concentration += production - decay;
    source->no_concentration = nimcp_clampf(source->no_concentration, 0.0f, NO_PEAK_CONCENTRATION * 1.5f);

    /* Callback for significant release */
    if (production > 1.0f && system->config.on_release) {
        system->config.on_release(source, source->no_concentration,
                                  system->config.callback_data);
        system->metrics.release_events++;
    }

    /* Diffuse to targets */
    nimcp_no_diffuse(system, source);

    /* Update state */
    source->state = determine_no_state(source->nos_activity, source->no_concentration);

    /* Track metrics */
    system->metrics.total_no_produced += production;
    if (source->no_concentration > system->metrics.peak_no_concentration) {
        system->metrics.peak_no_concentration = source->no_concentration;
    }
    if (source->nos_activity > 0.1f) {
        system->metrics.active_sources++;
    }

    return NO_OK;
}

nimcp_no_error_t nimcp_no_diffuse(
    nimcp_no_system_t* system,
    nimcp_no_source_t* source
) {
    if (!system || !source) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO diffuse: NULL argument");
        return NO_ERR_NULL_PTR;
    }

    /* Update each target */
    for (uint32_t i = 0; i < source->num_targets; i++) {
        nimcp_no_target_synapse_t* target = &source->targets[i];
        if (!target->active) continue;

        /* Calculate NO at target distance */
        float no_at_target = no_at_distance(source->no_concentration,
                                            target->distance,
                                            system->config.effective_radius);
        target->no_concentration = no_at_target;

        /* Update cGMP based on NO */
        float cgmp_production = no_at_target * CGMP_PRODUCTION_RATE *
                               system->config.gc_sensitivity;
        float cgmp_decay = target->cgmp_concentration *
                          system->config.cgmp_decay_rate *
                          system->config.pde_activity;

        target->cgmp_concentration += (cgmp_production - cgmp_decay) * 0.001f;
        target->cgmp_concentration = nimcp_clampf(target->cgmp_concentration,
                                            CGMP_BASAL_CONCENTRATION * 0.1f,
                                            CGMP_PEAK_CONCENTRATION);

        /* Calculate potentiation */
        float old_potentiation = target->potentiation_factor;

        if (target->mode == NO_RETROGRADE_PRESYNAPTIC_RELEASE) {
            target->potentiation_factor = calculate_potentiation(
                no_at_target,
                system->config.potentiation_max,
                system->config.potentiation_threshold
            );
        } else if (target->mode == NO_RETROGRADE_PRESYNAPTIC_INHIBIT) {
            float inhibition = calculate_potentiation(
                no_at_target,
                system->config.potentiation_max,
                system->config.potentiation_threshold
            );
            target->potentiation_factor = 2.0f - inhibition;  /* Invert */
            target->potentiation_factor = nimcp_clampf(target->potentiation_factor, 0.5f, 1.0f);
        }

        /* Callback for significant change */
        if (fabsf(target->potentiation_factor - old_potentiation) > 0.1f) {
            if (system->config.on_retrograde) {
                system->config.on_retrograde(target, target->potentiation_factor,
                                             system->config.callback_data);
                system->metrics.retrograde_events++;
            }
        }

        if (target->potentiation_factor != 1.0f) {
            system->metrics.activated_targets++;
        }
    }

    return NO_OK;
}

nimcp_no_error_t nimcp_no_update(
    nimcp_no_system_t* system,
    float dt
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Nitric oxide system is NULL in update");
        return NO_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "Nitric oxide system not initialized");
        return NO_ERR_NOT_INITIALIZED;
    }

    /* Reset per-update metrics */
    system->metrics.active_sources = 0;
    system->metrics.activated_targets = 0;

    /* Update all sources */
    float total_no = 0.0f;
    float total_cgmp = 0.0f;
    uint32_t cgmp_count = 0;

    for (uint32_t i = 0; i < system->num_sources; i++) {
        nimcp_no_error_t err = nimcp_no_update_source(system, &system->sources[i], dt);
        if (err != NO_OK) {
            return err;
        }

        total_no += system->sources[i].no_concentration;

        for (uint32_t j = 0; j < system->sources[i].num_targets; j++) {
            total_cgmp += system->sources[i].targets[j].cgmp_concentration;
            cgmp_count++;
        }
    }

    /* Update global levels */
    if (system->num_sources > 0) {
        system->global_no_level = total_no / system->num_sources;
        system->metrics.mean_no_concentration = system->global_no_level;
    }

    if (cgmp_count > 0) {
        system->global_cgmp_level = total_cgmp / cgmp_count;
        system->metrics.mean_cgmp_level = system->global_cgmp_level;
    }

    /* Calculate vasodilation */
    system->vasodilation_factor = 1.0f + (system->global_no_level / NO_PEAK_CONCENTRATION) *
                                  system->config.vasodilation_sensitivity;
    system->vasodilation_factor = nimcp_clampf(system->vasodilation_factor, 1.0f, 2.0f);
    system->metrics.vasodilation_index = system->vasodilation_factor;

    /* Calculate plasticity modifier */
    system->plasticity_modifier = 1.0f + (system->global_no_level - NO_BASAL_CONCENTRATION) /
                                  (NO_PEAK_CONCENTRATION - NO_BASAL_CONCENTRATION) * 0.5f;
    system->plasticity_modifier = nimcp_clampf(system->plasticity_modifier, 1.0f, 1.5f);

    /* Update time tracking */
    system->metrics.total_simulation_time += dt;
    system->metrics.update_count++;
    system->update_count++;

    return NO_OK;
}

//=============================================================================
// Effects API Implementation
//=============================================================================

nimcp_no_error_t nimcp_no_get_cgmp(
    const nimcp_no_system_t* system,
    uint32_t source_id,
    float* cgmp
) {
    if (!system || !cgmp) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO get_cgmp: NULL argument");
        return NO_ERR_NULL_PTR;
    }

    if (source_id >= system->num_sources) {
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "NO source_id %u not found in get_cgmp", source_id);
        return NO_ERR_SOURCE_NOT_FOUND;
    }

    /* Average cGMP across targets */
    const nimcp_no_source_t* source = &system->sources[source_id];
    float total = 0.0f;

    for (uint32_t i = 0; i < source->num_targets; i++) {
        total += source->targets[i].cgmp_concentration;
    }

    *cgmp = (source->num_targets > 0) ? (total / source->num_targets) : CGMP_BASAL_CONCENTRATION;

    return NO_OK;
}

nimcp_no_error_t nimcp_no_get_vasodilation(
    const nimcp_no_system_t* system,
    float* factor
) {
    if (!system || !factor) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO get_vasodilation: NULL argument");
        return NO_ERR_NULL_PTR;
    }

    *factor = system->vasodilation_factor;

    return NO_OK;
}

nimcp_no_error_t nimcp_no_get_plasticity_modifier(
    const nimcp_no_system_t* system,
    float* modifier
) {
    if (!system || !modifier) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO get_plasticity_modifier: NULL argument");
        return NO_ERR_NULL_PTR;
    }

    *modifier = system->plasticity_modifier;

    return NO_OK;
}

//=============================================================================
// Metrics API Implementation
//=============================================================================

nimcp_no_error_t nimcp_no_get_metrics(
    const nimcp_no_system_t* system,
    nimcp_no_metrics_t* metrics
) {
    if (!system || !metrics) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NO get_metrics: NULL argument");
        return NO_ERR_NULL_PTR;
    }

    memcpy(metrics, &system->metrics, sizeof(nimcp_no_metrics_t));

    return NO_OK;
}

nimcp_no_state_t nimcp_no_get_state(const nimcp_no_source_t* source) {
    if (!source) {
        return NO_STATE_INACTIVE;
    }
    return source->state;
}

const char* nimcp_no_error_string(nimcp_no_error_t error) {
    switch (error) {
        case NO_OK:
            return "OK";
        case NO_ERR_NULL_PTR:
            return "Null pointer";
        case NO_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case NO_ERR_NOT_INITIALIZED:
            return "Not initialized";
        case NO_ERR_ALREADY_INITIALIZED:
            return "Already initialized";
        case NO_ERR_NO_MEMORY:
            return "Out of memory";
        case NO_ERR_SOURCE_NOT_FOUND:
            return "Source not found";
        case NO_ERR_CAPACITY_EXCEEDED:
            return "Capacity exceeded";
        case NO_ERR_NOS_INACTIVE:
            return "NOS inactive";
        case NO_ERR_SUBSTRATE_DEPLETED:
            return "Substrate depleted";
        default:
            return "Unknown error";
    }
}
