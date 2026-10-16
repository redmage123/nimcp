/**
 * @file nimcp_thermodynamics_adapter.c
 * @brief Thermodynamics Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements thermal dynamics for Physics layer
 * WHY:  Models temperature effects on neural computation
 * HOW:  Computes heat diffusion and Q10 kinetic scaling
 *
 * @author NIMCP Development Team
 */

#include "integration/adapters/physics/nimcp_thermodynamics_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(thermodynamics_adapter)

//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_thermo_adapter_struct {
    nimcp_thermo_adapter_config_t config;
    nimcp_module_interface_t interface;

    /* Temperature state */
    float* temperatures;                /**< Regional temperatures (C) */
    float* heat_sources;                /**< Heat added this step (J) */
    float external_temp;                /**< External/environment temp (C) */

    /* State tracking */
    nimcp_thermo_adapter_state_t state;
    nimcp_thermo_adapter_stats_t stats;
    bool is_initialized;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute Q10 scaling factor
 *
 * Q10 = rate at (T+10) / rate at T
 * Scaling = Q10^((T - T_base)/10)
 */
static float compute_q10_scaling(float temp_c, float base_temp_c, float q10) {
    float delta_t = temp_c - base_temp_c;
    return powf(q10, delta_t / 10.0f);
}

//=============================================================================
// Module Interface Callbacks
//=============================================================================

static nimcp_layer_error_t thermo_init(void* module, void* config) {
    nimcp_thermo_adapter_t adapter = (nimcp_thermo_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    if (config) {
        adapter->config = *(nimcp_thermo_adapter_config_t*)config;
    }

    /* Allocate arrays */
    uint32_t n = adapter->config.num_regions;
    adapter->temperatures = (float*)nimcp_calloc(n, sizeof(float));
    adapter->heat_sources = (float*)nimcp_calloc(n, sizeof(float));

    if (!adapter->temperatures || !adapter->heat_sources) {
        return NIMCP_LAYER_ERR_NO_MEMORY;
    }

    /* Initialize to base temperature */
    for (uint32_t i = 0; i < n; i++) {
        adapter->temperatures[i] = adapter->config.base_temperature_c;
    }
    adapter->external_temp = adapter->config.base_temperature_c;

    adapter->is_initialized = true;
    adapter->state.is_active = true;
    adapter->state.mean_temperature_c = adapter->config.base_temperature_c;
    adapter->state.max_temperature_c = adapter->config.base_temperature_c;
    adapter->state.min_temperature_c = adapter->config.base_temperature_c;
    adapter->state.kinetic_scaling = 1.0f;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t thermo_shutdown(void* module) {
    nimcp_thermo_adapter_t adapter = (nimcp_thermo_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    nimcp_free(adapter->temperatures);
    nimcp_free(adapter->heat_sources);

    adapter->temperatures = NULL;
    adapter->heat_sources = NULL;

    adapter->is_initialized = false;
    adapter->state.is_active = false;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t thermo_update(void* module, float dt) {
    nimcp_thermo_adapter_t adapter = (nimcp_thermo_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t n = adapter->config.num_regions;
    float diffusivity = adapter->config.thermal_diffusivity;
    float base_temp = adapter->config.base_temperature_c;
    float q10 = adapter->config.q10_coefficient;

    /* Specific heat capacity approximation for brain tissue */
    float specific_heat = 3.6f;  /* J/(g*K), brain tissue ~= water */
    float region_mass = 1.0f;    /* g, normalized mass per region */

    float sum_temp = 0.0f;
    float max_temp = -1000.0f;
    float min_temp = 1000.0f;

    /* Process each region */
    for (uint32_t i = 0; i < n; i++) {
        float temp = adapter->temperatures[i];

        /* Add metabolic heat */
        if (adapter->config.enable_metabolic_coupling) {
            float heat_rate = adapter->config.metabolic_heat_rate;
            /* Heat is added proportional to kinetic scaling (more activity = more heat) */
            float scaling = compute_q10_scaling(temp, base_temp, q10);
            adapter->heat_sources[i] += heat_rate * scaling * dt;
        }

        /* Convert accumulated heat to temperature change: dT = Q / (m * c) */
        float dT_heat = adapter->heat_sources[i] / (region_mass * specific_heat);
        temp += dT_heat;
        adapter->state.total_heat_generated += adapter->heat_sources[i];
        adapter->heat_sources[i] = 0.0f;  /* Reset for next step */

        /* Heat dissipation to environment */
        if (adapter->config.enable_heat_dissipation) {
            float temp_diff = temp - adapter->external_temp;
            float dissipation_rate = 0.1f;  /* Heat transfer coefficient */
            temp -= temp_diff * dissipation_rate * dt;
        }

        /* Thermal diffusion between regions (simplified 1D diffusion) */
        if (i > 0 && i < n - 1) {
            float t_left = adapter->temperatures[i - 1];
            float t_right = adapter->temperatures[i + 1];
            float laplacian = t_left - 2.0f * temp + t_right;
            temp += diffusivity * laplacian * dt;
        }

        adapter->temperatures[i] = temp;
        sum_temp += temp;
        if (temp > max_temp) max_temp = temp;
        if (temp < min_temp) min_temp = temp;
    }

    /* Update state */
    adapter->state.mean_temperature_c = sum_temp / (float)n;
    adapter->state.max_temperature_c = max_temp;
    adapter->state.min_temperature_c = min_temp;
    adapter->state.kinetic_scaling = compute_q10_scaling(
        adapter->state.mean_temperature_c, base_temp, q10);

    adapter->stats.updates_processed++;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t thermo_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_thermo_adapter_t adapter = (nimcp_thermo_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->stats.messages_handled++;

    switch (msg->header.msg_type) {
        case NIMCP_LAYER_MSG_DATA_PUSH:
            /* Incoming metabolic activity data - convert to heat */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float activity = *(float*)msg->payload;
                /* Distribute activity as heat across all regions */
                float heat_per_region = activity * 0.001f;  /* Convert to J */
                for (uint32_t i = 0; i < adapter->config.num_regions; i++) {
                    adapter->heat_sources[i] += heat_per_region;
                }
                adapter->stats.temperature_events++;
            }
            break;

        case NIMCP_LAYER_MSG_STATE_QUERY:
            /* Could respond with temperature data */
            break;

        default:
            break;
    }

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t thermo_get_state(void* module, void* state_out, size_t* size) {
    nimcp_thermo_adapter_t adapter = (nimcp_thermo_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;

    if (*size < sizeof(nimcp_thermo_adapter_state_t)) {
        *size = sizeof(nimcp_thermo_adapter_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }

    *(nimcp_thermo_adapter_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_thermo_adapter_state_t);

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t thermo_set_state(void* module, const void* state, size_t size) {
    nimcp_thermo_adapter_t adapter = (nimcp_thermo_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;

    if (size < sizeof(nimcp_thermo_adapter_state_t)) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    adapter->state = *(const nimcp_thermo_adapter_state_t*)state;

    return NIMCP_LAYER_OK;
}

static const char* thermo_get_name(void* module) {
    (void)module;
    return "Thermodynamics_Adapter";
}

//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_thermo_adapter_config_t nimcp_thermo_adapter_default_config(void) {
    nimcp_thermo_adapter_config_t config = {
        .num_regions = 10,
        .base_temperature_c = 37.0f,    /* Normal body temperature */
        .q10_coefficient = 2.3f,        /* Typical neural Q10 */
        .thermal_diffusivity = 0.14f,   /* mm^2/s for brain tissue */
        .metabolic_heat_rate = 0.01f,   /* J/s baseline */
        .enable_heat_dissipation = true,
        .enable_metabolic_coupling = true,
        .enable_logging = false
    };
    return config;
}

nimcp_thermo_adapter_t nimcp_thermo_adapter_create(
    const nimcp_thermo_adapter_config_t* config
) {
    nimcp_thermo_adapter_t adapter = (nimcp_thermo_adapter_t)nimcp_calloc(
        1, sizeof(struct nimcp_thermo_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate thermodynamics adapter");

    adapter->config = config ? *config : nimcp_thermo_adapter_default_config();

    /* Set up module interface */
    adapter->interface.init = thermo_init;
    adapter->interface.shutdown = thermo_shutdown;
    adapter->interface.update = thermo_update;
    adapter->interface.handle_message = thermo_handle_message;
    adapter->interface.get_state = thermo_get_state;
    adapter->interface.set_state = thermo_set_state;
    adapter->interface.get_name = thermo_get_name;

    return adapter;
}

void nimcp_thermo_adapter_destroy(nimcp_thermo_adapter_t adapter) {
    if (!adapter) return;

    if (adapter->is_initialized) {
        thermo_shutdown(adapter);
    }

    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_thermo_adapter_get_interface(
    nimcp_thermo_adapter_t adapter
) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_thermo_adapter_add_heat(
    nimcp_thermo_adapter_t adapter,
    int region_idx,
    float heat_joules
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    if (region_idx < 0) {
        /* Add to all regions */
        float per_region = heat_joules / (float)adapter->config.num_regions;
        for (uint32_t i = 0; i < adapter->config.num_regions; i++) {
            adapter->heat_sources[i] += per_region;
        }
    } else if ((uint32_t)region_idx < adapter->config.num_regions) {
        adapter->heat_sources[region_idx] += heat_joules;
    } else {
        return NIMCP_LAYER_ERR_INVALID_MODULE;
    }

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_thermo_adapter_get_kinetic_scaling(
    nimcp_thermo_adapter_t adapter,
    float* scaling_out
) {
    if (!adapter || !scaling_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *scaling_out = adapter->state.kinetic_scaling;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_thermo_adapter_get_temperatures(
    nimcp_thermo_adapter_t adapter,
    float* temps_out,
    uint32_t max_temps,
    uint32_t* count_out
) {
    if (!adapter || !temps_out || !count_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t count = adapter->config.num_regions;
    if (count > max_temps) count = max_temps;

    memcpy(temps_out, adapter->temperatures, count * sizeof(float));
    *count_out = count;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_thermo_adapter_set_external_temp(
    nimcp_thermo_adapter_t adapter,
    float temp_c
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    adapter->external_temp = temp_c;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_thermo_adapter_get_state(
    nimcp_thermo_adapter_t adapter,
    nimcp_thermo_adapter_state_t* state_out
) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_thermo_adapter_get_stats(
    nimcp_thermo_adapter_t adapter,
    nimcp_thermo_adapter_stats_t* stats_out
) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_thermo_adapter_reset_stats(nimcp_thermo_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
