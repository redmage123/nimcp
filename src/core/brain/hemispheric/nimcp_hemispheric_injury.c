//=============================================================================
// nimcp_hemispheric_injury.c - Brain Injury and Recovery Modeling
//=============================================================================
/**
 * @file nimcp_hemispheric_injury.c
 * @brief Implementation of hemispheric brain injury and recovery simulation
 */

#include "core/brain/hemispheric/nimcp_hemispheric_injury.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hemispheric_injury, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Initialize region damage states
 */
static void init_region_states(region_damage_state_t* states, hemisphere_id_t hemisphere) {
    for (int i = 0; i < INJURY_REGION_COUNT; i++) {
        states[i].region = (injury_region_t)i;
        states[i].hemisphere = hemisphere;
        states[i].structural_damage = 0.0f;
        states[i].functional_impairment = 0.0f;
        states[i].connectivity_loss = 0.0f;
        states[i].recovery_progress = 0.0f;
        states[i].compensatory_function = 0.0f;
    }
}

/**
 * @brief Initialize recovery parameters
 */
static void init_recovery_params(recovery_params_t* params) {
    params->spontaneous_recovery_rate = 0.02f;  // 2% per day initially
    params->plasticity_potential = 1.0f;        // Full potential
    params->contralateral_compensation = 0.0f;
    params->perilesional_expansion = 0.0f;
    params->synaptic_strengthening = 1.0f;      // Baseline
    params->axonal_sprouting_rate = 0.01f;
}

/**
 * @brief Compute diaschisis effects (remote damage from lesion)
 */
static void compute_diaschisis(
    hemispheric_injury_system_t* system,
    const lesion_t* lesion
) {
    if (!system || !lesion || !system->config.enable_diaschisis) return;

    // Connected regions experience secondary damage
    // Simplified connectivity model: primary region connects to specific others
    region_damage_state_t* regions = (lesion->hemisphere == HEMISPHERE_LEFT)
        ? system->left_regions
        : system->right_regions;

    float secondary_damage = lesion->primary_damage * DIASCHISIS_FACTOR;

    // Region-specific connectivity patterns
    switch (lesion->primary_region) {
        case INJURY_REGION_MOTOR_CORTEX:
            regions[INJURY_REGION_BASAL_GANGLIA].structural_damage += secondary_damage * 0.5f;
            regions[INJURY_REGION_CEREBELLUM].structural_damage += secondary_damage * 0.4f;
            regions[INJURY_REGION_THALAMUS].structural_damage += secondary_damage * 0.3f;
            break;

        case INJURY_REGION_BROCA:
            regions[INJURY_REGION_WERNICKE].structural_damage += secondary_damage * 0.6f;
            regions[INJURY_REGION_PREFRONTAL].structural_damage += secondary_damage * 0.4f;
            regions[INJURY_REGION_MOTOR_CORTEX].structural_damage += secondary_damage * 0.3f;
            break;

        case INJURY_REGION_HIPPOCAMPUS:
            regions[INJURY_REGION_TEMPORAL].structural_damage += secondary_damage * 0.5f;
            regions[INJURY_REGION_PREFRONTAL].structural_damage += secondary_damage * 0.4f;
            regions[INJURY_REGION_AMYGDALA].structural_damage += secondary_damage * 0.3f;
            break;

        case INJURY_REGION_PREFRONTAL:
            regions[INJURY_REGION_BASAL_GANGLIA].structural_damage += secondary_damage * 0.4f;
            regions[INJURY_REGION_HIPPOCAMPUS].structural_damage += secondary_damage * 0.3f;
            break;

        default:
            // General diaschisis to thalamus (hub)
            regions[INJURY_REGION_THALAMUS].structural_damage += secondary_damage * 0.2f;
            break;
    }

    // Clamp all damage values
    for (int i = 0; i < INJURY_REGION_COUNT; i++) {
        if (regions[i].structural_damage > 1.0f) {
            regions[i].structural_damage = 1.0f;
        }
    }
}

/**
 * @brief Update functional impairment based on damage
 */
static void update_functional_impairment(
    hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere
) {
    region_damage_state_t* regions = (hemisphere == HEMISPHERE_LEFT)
        ? system->left_regions
        : system->right_regions;

    for (int i = 0; i < INJURY_REGION_COUNT; i++) {
        // Base impairment from structural damage
        float impairment = regions[i].structural_damage;

        // Add connectivity loss effect
        impairment += regions[i].connectivity_loss * 0.5f;

        // Subtract recovery and compensation
        impairment -= regions[i].recovery_progress;
        impairment -= regions[i].compensatory_function;

        // Clamp to valid range
        if (impairment < 0.0f) impairment = 0.0f;
        if (impairment > 1.0f) impairment = 1.0f;

        // Ensure minimum residual function
        if (impairment > (1.0f - MINIMUM_RESIDUAL_FUNCTION) &&
            regions[i].structural_damage < 1.0f) {
            impairment = 1.0f - MINIMUM_RESIDUAL_FUNCTION;
        }

        regions[i].functional_impairment = impairment;
    }
}

/**
 * @brief Process spontaneous recovery
 */
static void process_spontaneous_recovery(
    hemispheric_injury_system_t* system,
    float dt
) {
    if (!system->config.enable_spontaneous_recovery) return;

    // Recovery rate depends on phase
    float phase_multiplier = 1.0f;
    switch (system->current_phase) {
        case RECOVERY_PHASE_ACUTE:
            phase_multiplier = 2.0f;  // Rapid initial recovery
            break;
        case RECOVERY_PHASE_SUBACUTE:
            phase_multiplier = 1.0f;
            break;
        case RECOVERY_PHASE_CHRONIC:
            phase_multiplier = 0.3f;  // Slowing down
            break;
        case RECOVERY_PHASE_PLATEAU:
            phase_multiplier = 0.05f;  // Minimal
            break;
    }

    // Process left hemisphere
    for (int i = 0; i < INJURY_REGION_COUNT; i++) {
        if (system->left_regions[i].structural_damage > 0.0f) {
            float recovery_rate = system->left_recovery.spontaneous_recovery_rate *
                                  phase_multiplier * dt;
            float max_recovery = 1.0f - system->left_regions[i].structural_damage +
                                 system->config.max_recovery_potential;
            if (max_recovery > 1.0f) max_recovery = 1.0f;

            system->left_regions[i].recovery_progress += recovery_rate;
            if (system->left_regions[i].recovery_progress > max_recovery) {
                system->left_regions[i].recovery_progress = max_recovery;
            }
        }
    }

    // Process right hemisphere
    for (int i = 0; i < INJURY_REGION_COUNT; i++) {
        if (system->right_regions[i].structural_damage > 0.0f) {
            float recovery_rate = system->right_recovery.spontaneous_recovery_rate *
                                  phase_multiplier * dt;
            float max_recovery = 1.0f - system->right_regions[i].structural_damage +
                                 system->config.max_recovery_potential;
            if (max_recovery > 1.0f) max_recovery = 1.0f;

            system->right_regions[i].recovery_progress += recovery_rate;
            if (system->right_regions[i].recovery_progress > max_recovery) {
                system->right_regions[i].recovery_progress = max_recovery;
            }
        }
    }
}

/**
 * @brief Process contralateral compensation
 */
static void process_contralateral_compensation(
    hemispheric_injury_system_t* system,
    float dt
) {
    if (!system->config.enable_contralateral_compensation) return;

    // Left damage → right compensation
    for (int i = 0; i < INJURY_REGION_COUNT; i++) {
        if (system->left_regions[i].functional_impairment > 0.3f) {
            // Right hemisphere compensates
            float compensation_rate = 0.01f * dt;
            system->left_regions[i].compensatory_function += compensation_rate;

            if (system->left_regions[i].compensatory_function > MAX_CONTRALATERAL_COMPENSATION) {
                system->left_regions[i].compensatory_function = MAX_CONTRALATERAL_COMPENSATION;
            }
        }
    }

    // Right damage → left compensation
    for (int i = 0; i < INJURY_REGION_COUNT; i++) {
        if (system->right_regions[i].functional_impairment > 0.3f) {
            float compensation_rate = 0.01f * dt;
            system->right_regions[i].compensatory_function += compensation_rate;

            if (system->right_regions[i].compensatory_function > MAX_CONTRALATERAL_COMPENSATION) {
                system->right_regions[i].compensatory_function = MAX_CONTRALATERAL_COMPENSATION;
            }
        }
    }
}

/**
 * @brief Update recovery phase based on time
 */
static void update_recovery_phase(hemispheric_injury_system_t* system) {
    float days = system->time_since_injury_days;

    if (days < 7.0f) {
        system->current_phase = RECOVERY_PHASE_ACUTE;
    } else if (days < 90.0f) {
        system->current_phase = RECOVERY_PHASE_SUBACUTE;
    } else if (days < 365.0f) {
        system->current_phase = RECOVERY_PHASE_CHRONIC;
    } else {
        system->current_phase = RECOVERY_PHASE_PLATEAU;
    }
}

//=============================================================================
// Lifecycle API
//=============================================================================

hemispheric_injury_config_t hemispheric_injury_default_config(void) {
    hemispheric_injury_config_t config = {
        .recovery_tau_days = DEFAULT_RECOVERY_TAU_DAYS,
        .max_recovery_potential = 0.7f,  // Can recover up to 70% of lost function
        .enable_spontaneous_recovery = true,
        .enable_contralateral_compensation = true,
        .enable_perilesional_plasticity = true,
        .enable_diaschisis = true,
        .diaschisis_decay_factor = 0.1f,
        .enable_rehabilitation = true,
        .rehabilitation_boost = 1.5f,
        .enable_bio_async = true
    };
    return config;
}

hemispheric_injury_system_t* hemispheric_injury_create(
    const hemispheric_injury_config_t* config,
    hemispheric_brain_t* brain
) {
    if (!brain) {
        NIMCP_LOGGING_ERROR("hemispheric_injury_create: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    hemispheric_injury_system_t* system = nimcp_malloc(sizeof(hemispheric_injury_system_t));
    if (!system) {
        NIMCP_LOGGING_ERROR("hemispheric_injury_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "system is NULL");

        return NULL;
    }
    memset(system, 0, sizeof(hemispheric_injury_system_t));

    // Connect to brain
    system->brain = brain;

    // Apply configuration
    if (config) {
        system->config = *config;
    } else {
        system->config = hemispheric_injury_default_config();
    }

    // Initialize region states
    init_region_states(system->left_regions, HEMISPHERE_LEFT);
    init_region_states(system->right_regions, HEMISPHERE_RIGHT);

    // Initialize recovery params
    init_recovery_params(&system->left_recovery);
    init_recovery_params(&system->right_recovery);

    // Initialize rehabilitation
    system->left_rehab.active = false;
    system->right_rehab.active = false;

    // Set initial phase
    system->current_phase = RECOVERY_PHASE_ACUTE;
    system->time_since_injury_days = 0.0f;

    // Allocate mutex
    system->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!system->mutex) {
        NIMCP_LOGGING_ERROR("hemispheric_injury_create: mutex allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "hemispheric_injury_create: failed to allocate mutex");
        nimcp_free(system);
        return NULL;
    }
    nimcp_mutex_init(system->mutex, NULL);

    system->initialized = true;

    NIMCP_LOGGING_INFO("Created hemispheric injury system");
    return system;
}

void hemispheric_injury_destroy(hemispheric_injury_system_t* system) {
    if (!system) return;

    if (system->bio_async_enabled) {
        hemispheric_injury_disconnect_bio_async(system);
    }

    if (system->mutex) {
        nimcp_mutex_free(system->mutex);
    }

    system->initialized = false;
    nimcp_free(system);

    NIMCP_LOGGING_INFO("Destroyed hemispheric injury system");
}

//=============================================================================
// Lesion API
//=============================================================================

int hemispheric_injury_induce_lesion(
    hemispheric_injury_system_t* system,
    injury_type_t type,
    injury_severity_t severity,
    hemisphere_id_t hemisphere,
    injury_region_t region,
    float damage,
    uint32_t* lesion_id
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_induce_lesion: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (system->num_lesions >= MAX_LESIONS) {
        NIMCP_LOGGING_ERROR("hemispheric_injury_induce_lesion: max lesions reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hemispheric_injury_induce_lesion: capacity exceeded");
        return -1;
    }

    if (damage < 0.0f) damage = 0.0f;
    if (damage > 1.0f) damage = 1.0f;

    nimcp_mutex_lock(system->mutex);

    // Find free slot
    uint32_t slot = system->num_lesions;
    lesion_t* lesion = &system->lesions[slot];

    lesion->lesion_id = system->stats.total_lesions;
    lesion->type = type;
    lesion->severity = severity;
    lesion->hemisphere = hemisphere;
    lesion->primary_region = region;
    lesion->primary_damage = damage;
    lesion->secondary_damage = damage * DIASCHISIS_FACTOR;
    lesion->axonal_damage = damage * 0.5f;
    lesion->active = true;
    lesion->is_progressive = (type == INJURY_TYPE_DEGENERATIVE);
    lesion->progression_rate = lesion->is_progressive ? 0.001f : 0.0f;

    // Set affected regions
    lesion->affected_regions[0] = region;
    lesion->num_affected_regions = 1;

    // Apply damage to primary region
    region_damage_state_t* regions = (hemisphere == HEMISPHERE_LEFT)
        ? system->left_regions
        : system->right_regions;

    regions[region].structural_damage += damage;
    if (regions[region].structural_damage > 1.0f) {
        regions[region].structural_damage = 1.0f;
    }

    // Compute connectivity loss based on type
    if (type == INJURY_TYPE_TBI_DIFFUSE) {
        // Diffuse injury affects connectivity more
        for (int i = 0; i < INJURY_REGION_COUNT; i++) {
            regions[i].connectivity_loss += damage * 0.3f;
            if (regions[i].connectivity_loss > 1.0f) {
                regions[i].connectivity_loss = 1.0f;
            }
        }
    } else {
        regions[region].connectivity_loss += damage * 0.5f;
        if (regions[region].connectivity_loss > 1.0f) {
            regions[region].connectivity_loss = 1.0f;
        }
    }

    // Compute diaschisis
    compute_diaschisis(system, lesion);

    // Update functional impairment
    update_functional_impairment(system, hemisphere);

    system->num_lesions++;
    system->stats.total_lesions++;
    system->stats.active_lesions++;

    if (lesion_id) {
        *lesion_id = lesion->lesion_id;
    }

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Induced %s lesion in %s hemisphere, region %d, damage %.2f",
        hemispheric_injury_type_name(type),
        hemisphere == HEMISPHERE_LEFT ? "left" : "right",
        (int)region, damage);

    return 0;
}

int hemispheric_injury_remove_lesion(
    hemispheric_injury_system_t* system,
    uint32_t lesion_id
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_remove_lesion: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    // Find lesion
    for (uint32_t i = 0; i < system->num_lesions; i++) {
        if (system->lesions[i].lesion_id == lesion_id && system->lesions[i].active) {
            system->lesions[i].active = false;
            system->stats.active_lesions--;

            // Note: We don't remove structural damage - that persists
            // Only the active lesion tracking is removed

            nimcp_mutex_unlock(system->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(system->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hemispheric_injury_remove_lesion: operation failed");
    return -1;  // Not found
}

const lesion_t* hemispheric_injury_get_lesion(
    const hemispheric_injury_system_t* system,
    uint32_t lesion_id
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_get_lesion: required parameter is NULL (system, system->initialized)");
        return NULL;
    }

    for (uint32_t i = 0; i < system->num_lesions; i++) {
        if (system->lesions[i].lesion_id == lesion_id) {
            return &system->lesions[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_get_lesion: validation failed");
    return NULL;
}

int hemispheric_injury_expand_lesion(
    hemispheric_injury_system_t* system,
    uint32_t lesion_id,
    float additional_damage
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_expand_lesion: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    // Find lesion
    lesion_t* lesion = NULL;
    for (uint32_t i = 0; i < system->num_lesions; i++) {
        if (system->lesions[i].lesion_id == lesion_id && system->lesions[i].active) {
            lesion = &system->lesions[i];
            break;
        }
    }

    if (!lesion) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_expand_lesion: lesion is NULL");
        return -1;
    }

    // Expand damage
    lesion->primary_damage += additional_damage;
    if (lesion->primary_damage > 1.0f) {
        lesion->primary_damage = 1.0f;
    }

    // Apply to region
    region_damage_state_t* regions = (lesion->hemisphere == HEMISPHERE_LEFT)
        ? system->left_regions
        : system->right_regions;

    regions[lesion->primary_region].structural_damage += additional_damage;
    if (regions[lesion->primary_region].structural_damage > 1.0f) {
        regions[lesion->primary_region].structural_damage = 1.0f;
    }

    // Recompute diaschisis
    compute_diaschisis(system, lesion);
    update_functional_impairment(system, lesion->hemisphere);

    nimcp_mutex_unlock(system->mutex);

    return 0;
}

//=============================================================================
// Damage Query API
//=============================================================================

float hemispheric_injury_get_damage(
    const hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    injury_region_t region
) {
    if (!system || !system->initialized || region >= INJURY_REGION_COUNT) {
        return 0.0f;
    }

    const region_damage_state_t* regions = (hemisphere == HEMISPHERE_LEFT)
        ? system->left_regions
        : system->right_regions;

    return regions[region].structural_damage;
}

float hemispheric_injury_get_function(
    const hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    injury_region_t region
) {
    if (!system || !system->initialized || region >= INJURY_REGION_COUNT) {
        return 1.0f;
    }

    const region_damage_state_t* regions = (hemisphere == HEMISPHERE_LEFT)
        ? system->left_regions
        : system->right_regions;

    // Function = 1 - impairment
    return 1.0f - regions[region].functional_impairment;
}

float hemispheric_injury_get_connectivity_loss(
    const hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    injury_region_t region
) {
    if (!system || !system->initialized || region >= INJURY_REGION_COUNT) {
        return 0.0f;
    }

    const region_damage_state_t* regions = (hemisphere == HEMISPHERE_LEFT)
        ? system->left_regions
        : system->right_regions;

    return regions[region].connectivity_loss;
}

region_damage_state_t hemispheric_injury_get_region_state(
    const hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    injury_region_t region
) {
    region_damage_state_t state = {0};
    if (!system || !system->initialized || region >= INJURY_REGION_COUNT) {
        return state;
    }

    const region_damage_state_t* regions = (hemisphere == HEMISPHERE_LEFT)
        ? system->left_regions
        : system->right_regions;

    return regions[region];
}

//=============================================================================
// Recovery API
//=============================================================================

int hemispheric_injury_update(
    hemispheric_injury_system_t* system,
    float dt
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_update: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    // Advance time
    system->time_since_injury_days += dt;

    // Update recovery phase
    update_recovery_phase(system);

    // Process spontaneous recovery
    process_spontaneous_recovery(system, dt);

    // Process contralateral compensation
    process_contralateral_compensation(system, dt);

    // Process progressive lesions
    for (uint32_t i = 0; i < system->num_lesions; i++) {
        if (system->lesions[i].active && system->lesions[i].is_progressive) {
            float progression = system->lesions[i].progression_rate * dt;
            hemispheric_injury_expand_lesion(system,
                system->lesions[i].lesion_id, progression);
        }
    }

    // Update functional impairment
    update_functional_impairment(system, HEMISPHERE_LEFT);
    update_functional_impairment(system, HEMISPHERE_RIGHT);

    // Apply rehabilitation if active
    if (system->left_rehab.active) {
        hemispheric_injury_apply_rehabilitation(system, HEMISPHERE_LEFT);
    }
    if (system->right_rehab.active) {
        hemispheric_injury_apply_rehabilitation(system, HEMISPHERE_RIGHT);
    }

    // Update statistics
    system->stats.injury_updates++;

    // Compute average damage
    float total_damage = 0.0f;
    float total_recovery = 0.0f;
    for (int i = 0; i < INJURY_REGION_COUNT; i++) {
        total_damage += system->left_regions[i].structural_damage;
        total_damage += system->right_regions[i].structural_damage;
        total_recovery += system->left_regions[i].recovery_progress;
        total_recovery += system->right_regions[i].recovery_progress;
    }
    system->stats.avg_damage_level = total_damage / (2.0f * INJURY_REGION_COUNT);
    system->stats.avg_recovery_level = total_recovery / (2.0f * INJURY_REGION_COUNT);

    nimcp_mutex_unlock(system->mutex);

    return 0;
}

float hemispheric_injury_get_recovery(
    const hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    injury_region_t region
) {
    if (!system || !system->initialized || region >= INJURY_REGION_COUNT) {
        return 0.0f;
    }

    const region_damage_state_t* regions = (hemisphere == HEMISPHERE_LEFT)
        ? system->left_regions
        : system->right_regions;

    return regions[region].recovery_progress;
}

recovery_phase_t hemispheric_injury_get_phase(
    const hemispheric_injury_system_t* system
) {
    if (!system || !system->initialized) {
        return RECOVERY_PHASE_ACUTE;
    }
    return system->current_phase;
}

int hemispheric_injury_set_recovery_params(
    hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    const recovery_params_t* params
) {
    if (!system || !system->initialized || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_set_recovery_params: required parameter is NULL (system, system->initialized, params)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    if (hemisphere == HEMISPHERE_LEFT) {
        system->left_recovery = *params;
    } else {
        system->right_recovery = *params;
    }

    nimcp_mutex_unlock(system->mutex);

    return 0;
}

int hemispheric_injury_boost_plasticity(
    hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    float boost
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_boost_plasticity: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (boost < 1.0f) boost = 1.0f;
    if (boost > 5.0f) boost = 5.0f;

    nimcp_mutex_lock(system->mutex);

    if (hemisphere == HEMISPHERE_LEFT) {
        system->left_recovery.spontaneous_recovery_rate *= boost;
        system->left_recovery.synaptic_strengthening *= boost;
    } else {
        system->right_recovery.spontaneous_recovery_rate *= boost;
        system->right_recovery.synaptic_strengthening *= boost;
    }

    nimcp_mutex_unlock(system->mutex);

    return 0;
}

//=============================================================================
// Rehabilitation API
//=============================================================================

int hemispheric_injury_start_rehabilitation(
    hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere,
    injury_region_t target_region,
    float intensity,
    float frequency
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_start_rehabilitation: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (!system->config.enable_rehabilitation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_start_rehabilitation: system->config is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    rehabilitation_t* rehab = (hemisphere == HEMISPHERE_LEFT)
        ? &system->left_rehab
        : &system->right_rehab;

    rehab->active = true;
    rehab->target_region = target_region;
    rehab->intensity = fminf(1.0f, fmaxf(0.0f, intensity));
    rehab->frequency = fminf(10.0f, fmaxf(0.1f, frequency));
    rehab->efficacy = 1.0f;  // Can be modified based on injury type

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Started rehabilitation for %s hemisphere, region %d",
        hemisphere == HEMISPHERE_LEFT ? "left" : "right", (int)target_region);

    return 0;
}

int hemispheric_injury_stop_rehabilitation(
    hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_stop_rehabilitation: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    rehabilitation_t* rehab = (hemisphere == HEMISPHERE_LEFT)
        ? &system->left_rehab
        : &system->right_rehab;

    rehab->active = false;

    nimcp_mutex_unlock(system->mutex);

    return 0;
}

int hemispheric_injury_apply_rehabilitation(
    hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_apply_rehabilitation: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    rehabilitation_t* rehab = (hemisphere == HEMISPHERE_LEFT)
        ? &system->left_rehab
        : &system->right_rehab;

    if (!rehab->active) {
        return 0;
    }

    region_damage_state_t* regions = (hemisphere == HEMISPHERE_LEFT)
        ? system->left_regions
        : system->right_regions;

    // Rehabilitation boosts recovery in target region
    float boost = rehab->intensity * rehab->frequency * rehab->efficacy *
                  system->config.rehabilitation_boost * 0.01f;

    regions[rehab->target_region].recovery_progress += boost;
    if (regions[rehab->target_region].recovery_progress > 1.0f) {
        regions[rehab->target_region].recovery_progress = 1.0f;
    }

    system->stats.rehab_sessions++;

    return 0;
}

//=============================================================================
// Statistics API
//=============================================================================

hemispheric_injury_stats_t hemispheric_injury_get_stats(
    const hemispheric_injury_system_t* system
) {
    hemispheric_injury_stats_t stats = {0};
    if (!system || !system->initialized) {
        return stats;
    }
    return system->stats;
}

void hemispheric_injury_reset_stats(hemispheric_injury_system_t* system) {
    if (!system || !system->initialized) return;

    nimcp_mutex_lock(system->mutex);
    // Preserve total/active lesion counts
    uint32_t total = system->stats.total_lesions;
    uint32_t active = system->stats.active_lesions;
    memset(&system->stats, 0, sizeof(hemispheric_injury_stats_t));
    system->stats.total_lesions = total;
    system->stats.active_lesions = active;
    nimcp_mutex_unlock(system->mutex);
}

float hemispheric_injury_get_total_deficit(
    const hemispheric_injury_system_t* system,
    hemisphere_id_t hemisphere
) {
    if (!system || !system->initialized) {
        return 0.0f;
    }

    const region_damage_state_t* regions = (hemisphere == HEMISPHERE_LEFT)
        ? system->left_regions
        : system->right_regions;

    float total = 0.0f;
    for (int i = 0; i < INJURY_REGION_COUNT; i++) {
        total += regions[i].functional_impairment;
    }

    return total / INJURY_REGION_COUNT;
}

//=============================================================================
// Utility API
//=============================================================================

const char* hemispheric_injury_region_name(injury_region_t region) {
    static const char* names[] = {
        "Motor Cortex",
        "Sensory Cortex",
        "Prefrontal",
        "Temporal",
        "Parietal",
        "Occipital",
        "Broca's Area",
        "Wernicke's Area",
        "Hippocampus",
        "Amygdala",
        "Basal Ganglia",
        "Cerebellum",
        "Thalamus",
        "Brainstem",
        "Corpus Callosum"
    };

    if (region >= INJURY_REGION_COUNT) return "Unknown";
    return names[region];
}

const char* hemispheric_injury_type_name(injury_type_t type) {
    switch (type) {
        case INJURY_TYPE_STROKE_ISCHEMIC: return "Ischemic Stroke";
        case INJURY_TYPE_STROKE_HEMORRHAGIC: return "Hemorrhagic Stroke";
        case INJURY_TYPE_TBI_FOCAL: return "Focal TBI";
        case INJURY_TYPE_TBI_DIFFUSE: return "Diffuse TBI";
        case INJURY_TYPE_DEGENERATIVE: return "Degenerative";
        case INJURY_TYPE_SURGICAL: return "Surgical";
        case INJURY_TYPE_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

const char* hemispheric_injury_phase_name(recovery_phase_t phase) {
    switch (phase) {
        case RECOVERY_PHASE_ACUTE: return "Acute";
        case RECOVERY_PHASE_SUBACUTE: return "Subacute";
        case RECOVERY_PHASE_CHRONIC: return "Chronic";
        case RECOVERY_PHASE_PLATEAU: return "Plateau";
        default: return "Unknown";
    }
}

//=============================================================================
// Bio-async API
//=============================================================================

int hemispheric_injury_connect_bio_async(hemispheric_injury_system_t* system) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_connect_bio_async: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (system->bio_async_enabled) {
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HEMISPHERIC_INJURY,
        .module_name = "hemispheric_injury_system",
        .inbox_capacity = 16,
        .user_data = system
    };

    system->bio_ctx = bio_router_register_module(&info);
    if (system->bio_ctx) {
        system->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hemispheric injury system connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return 0;
}

int hemispheric_injury_disconnect_bio_async(hemispheric_injury_system_t* system) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_injury_disconnect_bio_async: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (!system->bio_async_enabled) {
        return 0;
    }

    if (system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
    }

    system->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Hemispheric injury system disconnected from bio-async router");

    return 0;
}
