/**
 * @file nimcp_ph_dynamics.c
 * @brief pH Dynamics Module Implementation
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "chemistry/ph/nimcp_ph_dynamics.h"
#include "chemistry/ph/nimcp_proton_pumps.h"
#include "chemistry/ph/nimcp_buffer_systems.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(ph_dynamics)

//=============================================================================
// Internal Constants
//=============================================================================

/** Natural log of 10 for pH calculations */
#define LN_10 2.302585093f

/** Minimum valid pH for calculations */
#define PH_MIN_VALID 0.0f

/** Maximum valid pH for calculations */
#define PH_MAX_VALID 14.0f

/** Activity-to-acid production coefficient */
#define ACTIVITY_ACID_COEFFICIENT 0.01f

/** pH recovery time constant (ms) */
#define PH_RECOVERY_TAU 1000.0f

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Convert pH to H+ concentration (molar)
 */
static inline float ph_to_h_concentration(float ph) {
    return powf(10.0f, -ph);
}

/**
 * @brief Convert H+ concentration to pH
 */
static inline float h_concentration_to_ph(float h_conc) {
    if (h_conc <= 0.0f) return PH_MAX_VALID;
    return -log10f(h_conc);
}

/**
 * @brief Determine pH status from pH value
 */
static nimcp_ph_status_t determine_ph_status(float ph, float target_ph) {
    float deviation = ph - target_ph;

    if (fabsf(deviation) < 0.05f) {
        return PH_STATUS_NORMAL;
    } else if (deviation < -0.3f) {
        return PH_STATUS_SEVERE_ACIDOSIS;
    } else if (deviation < -0.15f) {
        return PH_STATUS_MODERATE_ACIDOSIS;
    } else if (deviation < 0.0f) {
        return PH_STATUS_MILD_ACIDOSIS;
    } else if (deviation > 0.3f) {
        return PH_STATUS_SEVERE_ALKALOSIS;
    } else if (deviation > 0.15f) {
        return PH_STATUS_MODERATE_ALKALOSIS;
    } else {
        return PH_STATUS_MILD_ALKALOSIS;
    }
}

/**
 * @brief Calculate conductance modifier from pH
 * Acidosis decreases conductance, alkalosis can increase it
 */
static float calculate_conductance_modifier(float ph) {
    /* Optimal pH is 7.4 for most channels */
    float optimal = 7.4f;
    float deviation = fabsf(ph - optimal);

    /* Sigmoidal decrease with deviation */
    float modifier = 1.0f / (1.0f + expf(5.0f * (deviation - 0.3f)));
    return nimcp_clampf(modifier, 0.1f, 1.0f);
}

/**
 * @brief Calculate release modifier from pH
 * Vesicle pH affects NT release
 */
static float calculate_release_modifier(float vesicular_ph, float extracellular_ph) {
    /* Optimal vesicle pH ~5.5 for loading */
    float vesicle_optimal = 5.5f;
    float vesicle_dev = fabsf(vesicular_ph - vesicle_optimal);

    /* Extracellular pH affects release machinery */
    float extra_optimal = 7.4f;
    float extra_dev = fabsf(extracellular_ph - extra_optimal);

    float vesicle_factor = expf(-vesicle_dev * 2.0f);
    float extra_factor = expf(-extra_dev * 3.0f);

    return nimcp_clampf(vesicle_factor * extra_factor, 0.1f, 1.0f);
}

/**
 * @brief Calculate metabolic modifier from pH
 */
static float calculate_metabolic_modifier(float intracellular_ph) {
    /* Enzymes have optimal pH ranges */
    float optimal = 7.2f;
    float deviation = fabsf(intracellular_ph - optimal);

    /* Gaussian-like decrease */
    float modifier = expf(-deviation * deviation * 10.0f);
    return nimcp_clampf(modifier, 0.2f, 1.0f);
}

/**
 * @brief Initialize default configuration
 */
static void init_default_config(nimcp_ph_config_t* config) {
    config->initial_extracellular_ph = PH_EXTRACELLULAR_NORMAL;
    config->initial_intracellular_ph = PH_INTRACELLULAR_NORMAL;
    config->initial_vesicular_ph = PH_VESICLE_NORMAL;

    config->bicarbonate_concentration = 24.0f;  /* mM */
    config->phosphate_concentration = 1.0f;     /* mM */
    config->protein_concentration = 70.0f;      /* g/L */

    config->v_atpase_activity = 1.0f;
    config->nhe_activity = 1.0f;

    config->ph_recovery_rate = 0.01f;           /* pH units/sec */
    config->activity_acid_factor = ACTIVITY_ACID_COEFFICIENT;

    config->acidosis_threshold = 7.35f;
    config->alkalosis_threshold = 7.45f;

    config->on_status_change = NULL;
    config->on_critical = NULL;
    config->callback_data = NULL;
}

/**
 * @brief Initialize a region with default values
 */
static void init_region_defaults(nimcp_ph_region_t* region, uint32_t id) {
    memset(region, 0, sizeof(nimcp_ph_region_t));

    region->id = id;
    snprintf(region->name, sizeof(region->name), "Region_%u", id);

    /* Set default pH values */
    region->ph[PH_COMPARTMENT_EXTRACELLULAR] = PH_EXTRACELLULAR_NORMAL;
    region->ph[PH_COMPARTMENT_INTRACELLULAR] = PH_INTRACELLULAR_NORMAL;
    region->ph[PH_COMPARTMENT_VESICULAR] = PH_VESICLE_NORMAL;
    region->ph[PH_COMPARTMENT_MITOCHONDRIAL] = 8.0f;  /* Mitochondria more alkaline */

    /* Set targets */
    region->ph_target[PH_COMPARTMENT_EXTRACELLULAR] = PH_EXTRACELLULAR_NORMAL;
    region->ph_target[PH_COMPARTMENT_INTRACELLULAR] = PH_INTRACELLULAR_NORMAL;
    region->ph_target[PH_COMPARTMENT_VESICULAR] = PH_VESICLE_NORMAL;
    region->ph_target[PH_COMPARTMENT_MITOCHONDRIAL] = 8.0f;

    /* Initialize pumps */
    region->pumps[PH_PUMP_V_ATPASE].type = PH_PUMP_V_ATPASE;
    region->pumps[PH_PUMP_V_ATPASE].max_activity = 1.0f;
    region->pumps[PH_PUMP_V_ATPASE].current_activity = 1.0f;
    region->pumps[PH_PUMP_V_ATPASE].source = PH_COMPARTMENT_INTRACELLULAR;
    region->pumps[PH_PUMP_V_ATPASE].target = PH_COMPARTMENT_VESICULAR;
    region->pumps[PH_PUMP_V_ATPASE].enabled = true;

    region->pumps[PH_PUMP_NHE].type = PH_PUMP_NHE;
    region->pumps[PH_PUMP_NHE].max_activity = 1.0f;
    region->pumps[PH_PUMP_NHE].current_activity = 1.0f;
    region->pumps[PH_PUMP_NHE].source = PH_COMPARTMENT_INTRACELLULAR;
    region->pumps[PH_PUMP_NHE].target = PH_COMPARTMENT_EXTRACELLULAR;
    region->pumps[PH_PUMP_NHE].enabled = true;

    region->status = PH_STATUS_NORMAL;
    region->initialized = true;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

nimcp_ph_error_t nimcp_ph_init(
    nimcp_ph_system_t* system,
    const nimcp_ph_config_t* config
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pH system is NULL");
        return PH_ERR_NULL_PTR;
    }

    if (system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "pH system already initialized");
        return PH_ERR_ALREADY_INITIALIZED;
    }

    memset(system, 0, sizeof(nimcp_ph_system_t));

    /* Apply configuration */
    if (config) {
        memcpy(&system->config, config, sizeof(nimcp_ph_config_t));
    } else {
        init_default_config(&system->config);
    }

    /* Initialize metrics */
    system->metrics.mean_extracellular_ph = system->config.initial_extracellular_ph;
    system->metrics.mean_intracellular_ph = system->config.initial_intracellular_ph;
    system->metrics.mean_vesicular_ph = system->config.initial_vesicular_ph;

    /* Initialize effect modifiers */
    system->conductance_modifier = 1.0f;
    system->release_modifier = 1.0f;
    system->metabolic_modifier = 1.0f;

    system->initialized = true;
    system->update_count = 0;

    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_shutdown(nimcp_ph_system_t* system) {
    if (!system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH system is NULL in shutdown");
        return PH_ERR_NULL_PTR;
    }

    /* Clear all data */
    memset(system, 0, sizeof(nimcp_ph_system_t));

    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_reset(nimcp_ph_system_t* system) {
    if (!system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH system is NULL in reset");
        return PH_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        NIMCP_THROW(NIMCP_ERROR_NOT_INITIALIZED, "pH system not initialized in reset");
        return PH_ERR_NOT_INITIALIZED;
    }

    /* Reset all regions */
    for (uint32_t i = 0; i < system->num_regions; i++) {
        nimcp_ph_region_t* region = &system->regions[i];
        region->ph[PH_COMPARTMENT_EXTRACELLULAR] = system->config.initial_extracellular_ph;
        region->ph[PH_COMPARTMENT_INTRACELLULAR] = system->config.initial_intracellular_ph;
        region->ph[PH_COMPARTMENT_VESICULAR] = system->config.initial_vesicular_ph;
        region->activity_level = 0.0f;
        region->metabolic_acid_production = 0.0f;
        region->status = PH_STATUS_NORMAL;
    }

    /* Reset metrics */
    memset(&system->metrics, 0, sizeof(nimcp_ph_metrics_t));
    system->metrics.mean_extracellular_ph = system->config.initial_extracellular_ph;
    system->metrics.mean_intracellular_ph = system->config.initial_intracellular_ph;
    system->metrics.mean_vesicular_ph = system->config.initial_vesicular_ph;

    /* Reset modifiers */
    system->conductance_modifier = 1.0f;
    system->release_modifier = 1.0f;
    system->metabolic_modifier = 1.0f;

    system->update_count = 0;

    return PH_OK;
}

//=============================================================================
// Region Management API Implementation
//=============================================================================

nimcp_ph_error_t nimcp_ph_add_region(
    nimcp_ph_system_t* system,
    const char* name,
    uint32_t* region_id
) {
    if (!system || !name || !region_id) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH add_region: NULL argument");
        return PH_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        NIMCP_THROW(NIMCP_ERROR_NOT_INITIALIZED, "pH system not initialized in add_region");
        return PH_ERR_NOT_INITIALIZED;
    }

    if (system->num_regions >= PH_MAX_REGIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pH region capacity exceeded: max %d", PH_MAX_REGIONS);
        return PH_ERR_REGION_FULL;
    }

    uint32_t id = system->num_regions;
    nimcp_ph_region_t* region = &system->regions[id];

    init_region_defaults(region, id);
    strncpy(region->name, name, sizeof(region->name) - 1);
    region->name[sizeof(region->name) - 1] = '\0';

    /* Set initial pH from config */
    region->ph[PH_COMPARTMENT_EXTRACELLULAR] = system->config.initial_extracellular_ph;
    region->ph[PH_COMPARTMENT_INTRACELLULAR] = system->config.initial_intracellular_ph;
    region->ph[PH_COMPARTMENT_VESICULAR] = system->config.initial_vesicular_ph;

    system->num_regions++;
    *region_id = id;

    return PH_OK;
}

nimcp_ph_region_t* nimcp_ph_get_region(
    nimcp_ph_system_t* system,
    uint32_t region_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ph_get_region: system is NULL");
        return NULL;
    }
    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_ph_get_region: system not initialized");
        return NULL;
    }

    if (region_id >= system->num_regions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_ph_get_region: region_id out of range");
        return NULL;
    }

    return &system->regions[region_id];
}

nimcp_ph_error_t nimcp_ph_remove_region(
    nimcp_ph_system_t* system,
    uint32_t region_id
) {
    if (!system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH remove_region: system is NULL");
        return PH_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        NIMCP_THROW(NIMCP_ERROR_NOT_INITIALIZED, "pH system not initialized in remove_region");
        return PH_ERR_NOT_INITIALIZED;
    }

    if (region_id >= system->num_regions) {
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "pH region_id %u not found (max: %u)", region_id, system->num_regions);
        return PH_ERR_REGION_NOT_FOUND;
    }

    /* Shift regions down */
    for (uint32_t i = region_id; i < system->num_regions - 1; i++) {
        memcpy(&system->regions[i], &system->regions[i + 1], sizeof(nimcp_ph_region_t));
        system->regions[i].id = i;
    }

    system->num_regions--;

    return PH_OK;
}

//=============================================================================
// pH Control API Implementation
//=============================================================================

nimcp_ph_error_t nimcp_ph_set_compartment_ph(
    nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment,
    float ph_value
) {
    if (!region) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH set_compartment_ph: region is NULL");
        return PH_ERR_NULL_PTR;
    }

    if (compartment >= PH_COMPARTMENT_COUNT) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Invalid pH compartment: %d", compartment);
        return PH_ERR_INVALID_PARAM;
    }

    if (ph_value < PH_MIN_VALID || ph_value > PH_MAX_VALID) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "pH value out of range: %f (valid: %f-%f)", ph_value, PH_MIN_VALID, PH_MAX_VALID);
        return PH_ERR_INVALID_PARAM;
    }

    region->ph[compartment] = ph_value;

    /* Update status */
    region->status = determine_ph_status(
        region->ph[PH_COMPARTMENT_EXTRACELLULAR],
        region->ph_target[PH_COMPARTMENT_EXTRACELLULAR]
    );

    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_get_compartment_ph(
    const nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment,
    float* ph_value
) {
    if (!region || !ph_value) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH get_compartment_ph: NULL argument");
        return PH_ERR_NULL_PTR;
    }

    if (compartment >= PH_COMPARTMENT_COUNT) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Invalid pH compartment: %d", compartment);
        return PH_ERR_INVALID_PARAM;
    }

    *ph_value = region->ph[compartment];
    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_apply_acid_load(
    nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment,
    float acid_load
) {
    if (!region) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pH region is NULL in acid load");
        return PH_ERR_NULL_PTR;
    }

    if (compartment >= PH_COMPARTMENT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid pH compartment");
        return PH_ERR_INVALID_PARAM;
    }

    /* Convert current pH to H+ concentration */
    float current_h = ph_to_h_concentration(region->ph[compartment]);

    /* Add acid load (convert mM to M) */
    float new_h = current_h + (acid_load * 0.001f);

    /* Convert back to pH */
    float new_ph = h_concentration_to_ph(new_h);
    new_ph = nimcp_clampf(new_ph, PH_MINIMUM_VIABLE, PH_MAXIMUM_VIABLE);

    region->ph[compartment] = new_ph;

    /* Check for critical */
    if (new_ph < PH_MINIMUM_VIABLE + 0.2f) {
        return PH_ERR_CRITICAL_ACIDOSIS;
    }

    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_apply_base_load(
    nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment,
    float base_load
) {
    if (!region) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH apply_base_load: region is NULL");
        return PH_ERR_NULL_PTR;
    }

    if (compartment >= PH_COMPARTMENT_COUNT) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Invalid pH compartment in apply_base_load: %d", compartment);
        return PH_ERR_INVALID_PARAM;
    }

    /* Convert current pH to H+ concentration */
    float current_h = ph_to_h_concentration(region->ph[compartment]);

    /* Remove H+ (base neutralizes acid) */
    float new_h = current_h - (base_load * 0.001f);
    if (new_h < 1e-14f) new_h = 1e-14f;  /* Prevent negative */

    /* Convert back to pH */
    float new_ph = h_concentration_to_ph(new_h);
    new_ph = nimcp_clampf(new_ph, PH_MINIMUM_VIABLE, PH_MAXIMUM_VIABLE);

    region->ph[compartment] = new_ph;

    /* Check for critical */
    if (new_ph > PH_MAXIMUM_VIABLE - 0.2f) {
        return PH_ERR_CRITICAL_ALKALOSIS;
    }

    return PH_OK;
}

//=============================================================================
// Buffer System API Implementation
//=============================================================================

nimcp_ph_error_t nimcp_ph_add_buffer(
    nimcp_ph_region_t* region,
    nimcp_ph_buffer_type_t type,
    float concentration
) {
    if (!region) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH add_buffer: region is NULL");
        return PH_ERR_NULL_PTR;
    }

    if (type >= PH_BUFFER_COUNT) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Invalid pH buffer type: %d", type);
        return PH_ERR_INVALID_PARAM;
    }

    if (region->num_buffers >= PH_MAX_BUFFERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pH buffer capacity exceeded: max %d", PH_MAX_BUFFERS);
        return PH_ERR_REGION_FULL;
    }

    nimcp_ph_buffer_t* buffer = &region->buffers[region->num_buffers];
    buffer->type = type;
    buffer->concentration = concentration;
    buffer->active = true;
    buffer->saturation = 0.0f;

    /* Set pKa based on type */
    switch (type) {
        case PH_BUFFER_BICARBONATE:
            buffer->pka = 6.1f;
            buffer->buffering_capacity = concentration * 2.3f;
            break;
        case PH_BUFFER_PHOSPHATE:
            buffer->pka = 6.8f;
            buffer->buffering_capacity = concentration * 2.3f;
            break;
        case PH_BUFFER_PROTEIN:
            buffer->pka = 6.0f;
            buffer->buffering_capacity = concentration * 0.1f;  /* Per g/L */
            break;
        case PH_BUFFER_HEMOGLOBIN:
            buffer->pka = 6.5f;
            buffer->buffering_capacity = concentration * 0.15f;
            break;
        default:
            buffer->pka = 7.0f;
            buffer->buffering_capacity = concentration;
            break;
    }

    region->num_buffers++;

    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_get_buffering_capacity(
    const nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment,
    float* capacity
) {
    if (!region || !capacity) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH get_buffering_capacity: NULL argument");
        return PH_ERR_NULL_PTR;
    }

    (void)compartment;  /* Currently same buffers for all compartments */

    float total = 0.0f;
    for (uint32_t i = 0; i < region->num_buffers; i++) {
        if (region->buffers[i].active) {
            total += region->buffers[i].buffering_capacity;
        }
    }

    *capacity = total;
    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_calculate_buffer_response(
    const nimcp_ph_region_t* region,
    float delta_h,
    float* delta_ph
) {
    if (!region || !delta_ph) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH calculate_buffer_response: NULL argument");
        return PH_ERR_NULL_PTR;
    }

    float total_capacity = 0.0f;
    for (uint32_t i = 0; i < region->num_buffers; i++) {
        if (region->buffers[i].active) {
            total_capacity += region->buffers[i].buffering_capacity;
        }
    }

    if (total_capacity < 0.001f) {
        /* No buffering - direct pH change */
        *delta_ph = -delta_h * 1000.0f;  /* Very large change */
    } else {
        /* Buffered change */
        *delta_ph = -delta_h / total_capacity;
    }

    return PH_OK;
}

//=============================================================================
// Proton Pump API Implementation
//=============================================================================

nimcp_ph_error_t nimcp_ph_set_pump_activity(
    nimcp_ph_region_t* region,
    nimcp_ph_pump_type_t pump_type,
    float activity
) {
    if (!region) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH set_pump_activity: region is NULL");
        return PH_ERR_NULL_PTR;
    }

    if (pump_type >= PH_PUMP_COUNT) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Invalid pH pump type: %d", pump_type);
        return PH_ERR_INVALID_PARAM;
    }

    region->pumps[pump_type].current_activity = nimcp_clampf(activity, 0.0f, 1.0f);

    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_get_pump_activity(
    const nimcp_ph_region_t* region,
    nimcp_ph_pump_type_t pump_type,
    float* activity
) {
    if (!region || !activity) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH get_pump_activity: NULL argument");
        return PH_ERR_NULL_PTR;
    }

    if (pump_type >= PH_PUMP_COUNT) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Invalid pH pump type: %d", pump_type);
        return PH_ERR_INVALID_PARAM;
    }

    *activity = region->pumps[pump_type].current_activity;

    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_set_pump_enabled(
    nimcp_ph_region_t* region,
    nimcp_ph_pump_type_t pump_type,
    bool enabled
) {
    if (!region) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH set_pump_enabled: region is NULL");
        return PH_ERR_NULL_PTR;
    }

    if (pump_type >= PH_PUMP_COUNT) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Invalid pH pump type: %d", pump_type);
        return PH_ERR_INVALID_PARAM;
    }

    region->pumps[pump_type].enabled = enabled;

    return PH_OK;
}

//=============================================================================
// Update API Implementation
//=============================================================================

nimcp_ph_error_t nimcp_ph_update_region(
    nimcp_ph_system_t* system,
    nimcp_ph_region_t* region,
    float dt
) {
    if (!system || !region) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH update_region: NULL argument");
        return PH_ERR_NULL_PTR;
    }

    float dt_sec = dt / 1000.0f;

    /* Activity-dependent acid production */
    float acid_production = region->activity_level *
                           system->config.activity_acid_factor * dt_sec;
    region->metabolic_acid_production = acid_production;

    /* Apply acid production to intracellular compartment */
    if (acid_production > 0.0f) {
        float current_h = ph_to_h_concentration(region->ph[PH_COMPARTMENT_INTRACELLULAR]);
        current_h += acid_production * 0.001f;
        region->ph[PH_COMPARTMENT_INTRACELLULAR] = h_concentration_to_ph(current_h);
    }

    /* Pump-mediated recovery */
    for (int pump_type = 0; pump_type < PH_PUMP_COUNT; pump_type++) {
        nimcp_ph_pump_t* pump = &region->pumps[pump_type];
        if (!pump->enabled || pump->current_activity < 0.01f) {
            continue;
        }

        float pump_rate = pump->max_activity * pump->current_activity * dt_sec;

        /* V-ATPase: acidifies vesicles */
        if (pump_type == PH_PUMP_V_ATPASE) {
            float vesicle_h = ph_to_h_concentration(region->ph[PH_COMPARTMENT_VESICULAR]);
            vesicle_h += pump_rate * 0.001f;
            region->ph[PH_COMPARTMENT_VESICULAR] = h_concentration_to_ph(vesicle_h);
            region->ph[PH_COMPARTMENT_VESICULAR] = nimcp_clampf(
                region->ph[PH_COMPARTMENT_VESICULAR], 4.5f, 7.0f);
        }
        /* NHE: exports H+ from intracellular */
        else if (pump_type == PH_PUMP_NHE) {
            float intra_h = ph_to_h_concentration(region->ph[PH_COMPARTMENT_INTRACELLULAR]);
            float extra_h = ph_to_h_concentration(region->ph[PH_COMPARTMENT_EXTRACELLULAR]);

            /* Only active if intracellular is more acidic than setpoint */
            if (region->ph[PH_COMPARTMENT_INTRACELLULAR] < region->ph_target[PH_COMPARTMENT_INTRACELLULAR]) {
                intra_h -= pump_rate * 0.0001f;
                extra_h += pump_rate * 0.0001f;
                region->ph[PH_COMPARTMENT_INTRACELLULAR] = h_concentration_to_ph(intra_h);
                region->ph[PH_COMPARTMENT_EXTRACELLULAR] = h_concentration_to_ph(extra_h);
            }
        }
    }

    /* pH recovery toward target (passive buffering/regulation) */
    for (int comp = 0; comp < PH_COMPARTMENT_COUNT; comp++) {
        float error = region->ph_target[comp] - region->ph[comp];
        float recovery = error * (1.0f - expf(-dt / PH_RECOVERY_TAU));
        region->ph[comp] += recovery * 0.1f;  /* Slow recovery */
    }

    /* Clamp pH values */
    for (int comp = 0; comp < PH_COMPARTMENT_COUNT; comp++) {
        region->ph[comp] = nimcp_clampf(region->ph[comp], PH_MINIMUM_VIABLE, PH_MAXIMUM_VIABLE);
    }

    /* Update status */
    nimcp_ph_status_t old_status = region->status;
    region->status = determine_ph_status(
        region->ph[PH_COMPARTMENT_EXTRACELLULAR],
        region->ph_target[PH_COMPARTMENT_EXTRACELLULAR]
    );

    /* Callbacks */
    if (old_status != region->status && system->config.on_status_change) {
        system->config.on_status_change(region, old_status, region->status,
                                        system->config.callback_data);
    }

    /* Check for critical */
    for (int comp = 0; comp < PH_COMPARTMENT_COUNT; comp++) {
        if (nimcp_ph_is_critical(region, (nimcp_ph_compartment_t)comp)) {
            if (system->config.on_critical) {
                system->config.on_critical(region, (nimcp_ph_compartment_t)comp,
                                           region->ph[comp], system->config.callback_data);
            }
            system->metrics.critical_events++;
        }
    }

    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_update(
    nimcp_ph_system_t* system,
    float dt
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pH system is NULL in update");
        return PH_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pH system not initialized");
        return PH_ERR_NOT_INITIALIZED;
    }

    /* Update all regions */
    for (uint32_t i = 0; i < system->num_regions; i++) {
        nimcp_ph_error_t err = nimcp_ph_update_region(system, &system->regions[i], dt);
        if (err != PH_OK && err != PH_ERR_CRITICAL_ACIDOSIS && err != PH_ERR_CRITICAL_ALKALOSIS) {
            return err;
        }
    }

    /* Update metrics */
    float sum_extra = 0.0f, sum_intra = 0.0f, sum_vesicle = 0.0f;
    float max_dev = 0.0f, min_dev = 0.0f;

    for (uint32_t i = 0; i < system->num_regions; i++) {
        nimcp_ph_region_t* region = &system->regions[i];
        sum_extra += region->ph[PH_COMPARTMENT_EXTRACELLULAR];
        sum_intra += region->ph[PH_COMPARTMENT_INTRACELLULAR];
        sum_vesicle += region->ph[PH_COMPARTMENT_VESICULAR];

        float dev = region->ph[PH_COMPARTMENT_EXTRACELLULAR] - PH_EXTRACELLULAR_NORMAL;
        if (dev > max_dev) max_dev = dev;
        if (dev < min_dev) min_dev = dev;
    }

    if (system->num_regions > 0) {
        system->metrics.mean_extracellular_ph = sum_extra / system->num_regions;
        system->metrics.mean_intracellular_ph = sum_intra / system->num_regions;
        system->metrics.mean_vesicular_ph = sum_vesicle / system->num_regions;
    }

    system->metrics.max_ph_deviation = max_dev;
    system->metrics.min_ph_deviation = min_dev;
    system->metrics.total_simulation_time += dt;

    /* Update global modifiers */
    system->conductance_modifier = calculate_conductance_modifier(
        system->metrics.mean_extracellular_ph);
    system->release_modifier = calculate_release_modifier(
        system->metrics.mean_vesicular_ph,
        system->metrics.mean_extracellular_ph);
    system->metabolic_modifier = calculate_metabolic_modifier(
        system->metrics.mean_intracellular_ph);

    system->update_count++;

    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_set_activity(
    nimcp_ph_region_t* region,
    float activity
) {
    if (!region) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH set_activity: region is NULL");
        return PH_ERR_NULL_PTR;
    }

    region->activity_level = nimcp_clampf(activity, 0.0f, 1.0f);

    return PH_OK;
}

//=============================================================================
// Effects API Implementation
//=============================================================================

nimcp_ph_error_t nimcp_ph_get_conductance_modifier(
    const nimcp_ph_system_t* system,
    uint32_t region_id,
    float* modifier
) {
    if (!system || !modifier) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH get_conductance_modifier: NULL argument");
        return PH_ERR_NULL_PTR;
    }

    if (region_id >= system->num_regions) {
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "pH region_id %u not found in get_conductance_modifier", region_id);
        return PH_ERR_REGION_NOT_FOUND;
    }

    const nimcp_ph_region_t* region = &system->regions[region_id];
    *modifier = calculate_conductance_modifier(region->ph[PH_COMPARTMENT_EXTRACELLULAR]);

    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_get_release_modifier(
    const nimcp_ph_system_t* system,
    uint32_t region_id,
    float* modifier
) {
    if (!system || !modifier) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH get_release_modifier: NULL argument");
        return PH_ERR_NULL_PTR;
    }

    if (region_id >= system->num_regions) {
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "pH region_id %u not found in get_release_modifier", region_id);
        return PH_ERR_REGION_NOT_FOUND;
    }

    const nimcp_ph_region_t* region = &system->regions[region_id];
    *modifier = calculate_release_modifier(
        region->ph[PH_COMPARTMENT_VESICULAR],
        region->ph[PH_COMPARTMENT_EXTRACELLULAR]
    );

    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_get_metabolic_modifier(
    const nimcp_ph_system_t* system,
    uint32_t region_id,
    float* modifier
) {
    if (!system || !modifier) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH get_metabolic_modifier: NULL argument");
        return PH_ERR_NULL_PTR;
    }

    if (region_id >= system->num_regions) {
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "pH region_id %u not found in get_metabolic_modifier", region_id);
        return PH_ERR_REGION_NOT_FOUND;
    }

    const nimcp_ph_region_t* region = &system->regions[region_id];
    *modifier = calculate_metabolic_modifier(region->ph[PH_COMPARTMENT_INTRACELLULAR]);

    return PH_OK;
}

nimcp_ph_error_t nimcp_ph_get_function_modifier(
    const nimcp_ph_system_t* system,
    float* modifier
) {
    if (!system || !modifier) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH get_function_modifier: NULL argument");
        return PH_ERR_NULL_PTR;
    }

    /* Combine all modifiers */
    *modifier = system->conductance_modifier *
                system->release_modifier *
                system->metabolic_modifier;

    return PH_OK;
}

//=============================================================================
// Status and Metrics API Implementation
//=============================================================================

nimcp_ph_status_t nimcp_ph_get_status(const nimcp_ph_region_t* region) {
    if (!region) {
        return PH_STATUS_NORMAL;
    }
    return region->status;
}

nimcp_ph_error_t nimcp_ph_get_metrics(
    const nimcp_ph_system_t* system,
    nimcp_ph_metrics_t* metrics
) {
    if (!system || !metrics) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "pH get_metrics: NULL argument");
        return PH_ERR_NULL_PTR;
    }

    memcpy(metrics, &system->metrics, sizeof(nimcp_ph_metrics_t));

    return PH_OK;
}

bool nimcp_ph_is_critical(
    const nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment
) {
    if (!region || compartment >= PH_COMPARTMENT_COUNT) {
        return false;
    }

    float ph = region->ph[compartment];

    /* Critical thresholds depend on compartment */
    switch (compartment) {
        case PH_COMPARTMENT_EXTRACELLULAR:
            return (ph < 7.0f || ph > 7.8f);
        case PH_COMPARTMENT_INTRACELLULAR:
            return (ph < 6.8f || ph > 7.6f);
        case PH_COMPARTMENT_VESICULAR:
            return (ph < 4.5f || ph > 6.5f);
        case PH_COMPARTMENT_MITOCHONDRIAL:
            return (ph < 7.5f || ph > 8.5f);
        default:
            return false;
    }
}

const char* nimcp_ph_error_string(nimcp_ph_error_t error) {
    switch (error) {
        case PH_OK:
            return "OK";
        case PH_ERR_NULL_PTR:
            return "Null pointer";
        case PH_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case PH_ERR_NOT_INITIALIZED:
            return "Not initialized";
        case PH_ERR_ALREADY_INITIALIZED:
            return "Already initialized";
        case PH_ERR_NO_MEMORY:
            return "Out of memory";
        case PH_ERR_REGION_NOT_FOUND:
            return "Region not found";
        case PH_ERR_REGION_FULL:
            return "Region capacity full";
        case PH_ERR_CRITICAL_ACIDOSIS:
            return "Critical acidosis";
        case PH_ERR_CRITICAL_ALKALOSIS:
            return "Critical alkalosis";
        case PH_ERR_PUMP_FAILURE:
            return "Pump failure";
        default:
            return "Unknown error";
    }
}
