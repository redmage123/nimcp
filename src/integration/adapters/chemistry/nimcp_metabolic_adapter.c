/**
 * @file nimcp_metabolic_adapter.c
 * @brief Metabolic/ATP Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements metabolic (ATP/glucose) dynamics for Chemistry layer
 * WHY:  Energy metabolism constrains neural computation
 * HOW:  Models ATP production, consumption, and metabolic fatigue
 *
 * @author NIMCP Development Team
 */

#include "integration/adapters/chemistry/nimcp_metabolic_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(metabolic_adapter)

//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_metabolic_adapter_struct {
    nimcp_metabolic_adapter_config_t config;
    nimcp_module_interface_t interface;

    /* Metabolic state */
    float* atp_levels;                  /**< [ATP] per compartment (mM) */
    float* adp_levels;                  /**< [ADP] per compartment (mM) */
    float glucose;                      /**< Blood glucose (mM) */
    float lactate;                      /**< Lactate (mM) */

    /* State tracking */
    nimcp_metabolic_adapter_state_t state;
    nimcp_metabolic_adapter_stats_t stats;
    bool is_initialized;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Michaelis-Menten kinetics
 */
static float michaelis_menten(float s, float vmax, float km) {
    return vmax * s / (km + s);
}

//=============================================================================
// Module Interface Callbacks
//=============================================================================

static nimcp_layer_error_t metabolic_init(void* module, void* config) {
    nimcp_metabolic_adapter_t adapter = (nimcp_metabolic_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    if (config) {
        adapter->config = *(nimcp_metabolic_adapter_config_t*)config;
    }

    /* Allocate arrays */
    uint32_t n = adapter->config.num_compartments;
    adapter->atp_levels = (float*)nimcp_calloc(n, sizeof(float));
    adapter->adp_levels = (float*)nimcp_calloc(n, sizeof(float));

    if (!adapter->atp_levels || !adapter->adp_levels) {
        return NIMCP_LAYER_ERR_NO_MEMORY;
    }

    /* Initialize to basal ATP */
    for (uint32_t i = 0; i < n; i++) {
        adapter->atp_levels[i] = adapter->config.basal_atp_mm;
        adapter->adp_levels[i] = 0.5f;  /* Typical resting ADP ~0.5 mM */
    }

    adapter->glucose = 5.0f;  /* Normal blood glucose ~5 mM */
    adapter->lactate = 1.0f;  /* Normal lactate ~1 mM */

    adapter->is_initialized = true;
    adapter->state.is_active = true;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t metabolic_shutdown(void* module) {
    nimcp_metabolic_adapter_t adapter = (nimcp_metabolic_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    nimcp_free(adapter->atp_levels);
    nimcp_free(adapter->adp_levels);

    adapter->atp_levels = NULL;
    adapter->adp_levels = NULL;

    adapter->is_initialized = false;
    adapter->state.is_active = false;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t metabolic_update(void* module, float dt) {
    nimcp_metabolic_adapter_t adapter = (nimcp_metabolic_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t n = adapter->config.num_compartments;
    float basal = adapter->config.basal_atp_mm;
    float max_atp = adapter->config.max_atp_mm;
    float prod_rate = adapter->config.atp_production_rate;
    float km = adapter->config.glucose_km;

    float sum_atp = 0.0f;
    float sum_adp = 0.0f;

    /* Update each compartment */
    for (uint32_t i = 0; i < n; i++) {
        float atp = adapter->atp_levels[i];
        float adp = adapter->adp_levels[i];

        /* ATP production via glycolysis (Michaelis-Menten kinetics) */
        float glucose_uptake = michaelis_menten(adapter->glucose, 1.0f, km);
        float atp_produced = prod_rate * glucose_uptake * dt;

        /* Consume ADP to produce ATP */
        float adp_consumed = fminf(atp_produced, adp);
        atp += adp_consumed;
        adp -= adp_consumed;

        /* Lactate shuttle (if enabled) */
        if (adapter->config.enable_lactate) {
            /* Astrocyte lactate can be converted to ATP */
            float lactate_contribution = 0.1f * adapter->lactate * dt;
            atp += lactate_contribution;
            adapter->lactate -= lactate_contribution * 0.5f;
            if (adapter->lactate < 0.0f) adapter->lactate = 0.0f;
        }

        /* Basal ATP consumption (Na+/K+ ATPase, etc.) */
        float basal_consumption = 0.1f * dt;
        atp -= basal_consumption;
        adp += basal_consumption;
        adapter->stats.total_atp_consumed += basal_consumption;

        /* Clamp values */
        if (atp < 0.0f) {
            atp = 0.0f;
            adapter->stats.atp_depletion_events++;
        }
        if (atp > max_atp) atp = max_atp;
        if (adp < 0.0f) adp = 0.0f;
        if (adp > 2.0f) adp = 2.0f;  /* Max ADP ~2 mM */

        adapter->atp_levels[i] = atp;
        adapter->adp_levels[i] = adp;

        sum_atp += atp;
        sum_adp += adp;
    }

    /* Update state */
    adapter->state.mean_atp_mm = sum_atp / (float)n;
    adapter->state.mean_adp_mm = sum_adp / (float)n;
    adapter->state.glucose_level = adapter->glucose;
    adapter->state.lactate_level = adapter->lactate;

    /* Energy charge: (ATP + 0.5*ADP) / (ATP + ADP + AMP) */
    /* Simplified: ATP / (ATP + ADP) since AMP is typically low */
    if (sum_atp + sum_adp > 0.0f) {
        adapter->state.energy_ratio = sum_atp / (sum_atp + sum_adp);
    } else {
        adapter->state.energy_ratio = 0.0f;
    }

    /* Fatigue index */
    if (adapter->config.enable_fatigue) {
        float target_fatigue = 1.0f - (adapter->state.mean_atp_mm / basal);
        if (target_fatigue < 0.0f) target_fatigue = 0.0f;
        if (target_fatigue > 1.0f) target_fatigue = 1.0f;
        /* Slow fatigue adaptation */
        adapter->state.fatigue_index += (target_fatigue - adapter->state.fatigue_index) * 0.01f;
    }

    adapter->stats.updates_processed++;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t metabolic_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_metabolic_adapter_t adapter = (nimcp_metabolic_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->stats.messages_handled++;

    switch (msg->header.msg_type) {
        case NIMCP_LAYER_MSG_DATA_PUSH:
            /* Incoming activity data = ATP consumption */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float activity = *(float*)msg->payload;
                /* More activity = more ATP consumption */
                float consumption = activity * 0.01f;
                for (uint32_t i = 0; i < adapter->config.num_compartments; i++) {
                    adapter->atp_levels[i] -= consumption;
                    adapter->adp_levels[i] += consumption;
                    if (adapter->atp_levels[i] < 0.0f) {
                        adapter->atp_levels[i] = 0.0f;
                        adapter->stats.atp_depletion_events++;
                    }
                }
                adapter->stats.total_atp_consumed += consumption * adapter->config.num_compartments;
            }
            break;

        case NIMCP_LAYER_MSG_MODULATE:
            /* Metabolic modulation (e.g., from hypothalamus) */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float mod = *(float*)msg->payload;
                /* Modulates glucose availability */
                adapter->glucose *= (1.0f + mod * 0.1f);
                if (adapter->glucose < 0.5f) adapter->glucose = 0.5f;
                if (adapter->glucose > 15.0f) adapter->glucose = 15.0f;
            }
            break;

        default:
            break;
    }

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t metabolic_get_state(void* module, void* state_out, size_t* size) {
    nimcp_metabolic_adapter_t adapter = (nimcp_metabolic_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;

    if (*size < sizeof(nimcp_metabolic_adapter_state_t)) {
        *size = sizeof(nimcp_metabolic_adapter_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }

    *(nimcp_metabolic_adapter_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_metabolic_adapter_state_t);

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t metabolic_set_state(void* module, const void* state, size_t size) {
    nimcp_metabolic_adapter_t adapter = (nimcp_metabolic_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;

    if (size < sizeof(nimcp_metabolic_adapter_state_t)) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    adapter->state = *(const nimcp_metabolic_adapter_state_t*)state;

    return NIMCP_LAYER_OK;
}

static const char* metabolic_get_name(void* module) {
    (void)module;
    return "Metabolic_Adapter";
}

//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_metabolic_adapter_config_t nimcp_metabolic_adapter_default_config(void) {
    nimcp_metabolic_adapter_config_t config = {
        .num_compartments = 100,
        .basal_atp_mm = 3.0f,           /* Typical neuronal ATP ~3 mM */
        .max_atp_mm = 5.0f,             /* Max ATP ~5 mM */
        .atp_production_rate = 0.5f,    /* ATP synthesis rate */
        .glucose_km = 1.5f,             /* Glucose Km ~1.5 mM */
        .enable_fatigue = true,
        .enable_lactate = true,
        .enable_logging = false
    };
    return config;
}

nimcp_metabolic_adapter_t nimcp_metabolic_adapter_create(
    const nimcp_metabolic_adapter_config_t* config
) {
    nimcp_metabolic_adapter_t adapter = (nimcp_metabolic_adapter_t)nimcp_calloc(
        1, sizeof(struct nimcp_metabolic_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate metabolic adapter");

    adapter->config = config ? *config : nimcp_metabolic_adapter_default_config();

    /* Set up module interface */
    adapter->interface.init = metabolic_init;
    adapter->interface.shutdown = metabolic_shutdown;
    adapter->interface.update = metabolic_update;
    adapter->interface.handle_message = metabolic_handle_message;
    adapter->interface.get_state = metabolic_get_state;
    adapter->interface.set_state = metabolic_set_state;
    adapter->interface.get_name = metabolic_get_name;

    return adapter;
}

void nimcp_metabolic_adapter_destroy(nimcp_metabolic_adapter_t adapter) {
    if (!adapter) return;

    if (adapter->is_initialized) {
        metabolic_shutdown(adapter);
    }

    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_metabolic_adapter_get_interface(
    nimcp_metabolic_adapter_t adapter
) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_metabolic_adapter_consume_atp(
    nimcp_metabolic_adapter_t adapter,
    int compartment_idx,
    float atp_consumed
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    if (compartment_idx < 0) {
        /* Consume from all compartments */
        float per_compartment = atp_consumed / (float)adapter->config.num_compartments;
        for (uint32_t i = 0; i < adapter->config.num_compartments; i++) {
            adapter->atp_levels[i] -= per_compartment;
            adapter->adp_levels[i] += per_compartment;
            if (adapter->atp_levels[i] < 0.0f) {
                adapter->atp_levels[i] = 0.0f;
                adapter->stats.atp_depletion_events++;
            }
        }
    } else if ((uint32_t)compartment_idx < adapter->config.num_compartments) {
        adapter->atp_levels[compartment_idx] -= atp_consumed;
        adapter->adp_levels[compartment_idx] += atp_consumed;
        if (adapter->atp_levels[compartment_idx] < 0.0f) {
            adapter->atp_levels[compartment_idx] = 0.0f;
            adapter->stats.atp_depletion_events++;
        }
    } else {
        return NIMCP_LAYER_ERR_INVALID_MODULE;
    }

    adapter->stats.total_atp_consumed += atp_consumed;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_metabolic_adapter_set_glucose(
    nimcp_metabolic_adapter_t adapter,
    float glucose_mm
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    adapter->glucose = glucose_mm;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_metabolic_adapter_get_atp(
    nimcp_metabolic_adapter_t adapter,
    float* atp_out,
    uint32_t max_count,
    uint32_t* count_out
) {
    if (!adapter || !atp_out || !count_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t count = adapter->config.num_compartments;
    if (count > max_count) count = max_count;

    memcpy(atp_out, adapter->atp_levels, count * sizeof(float));
    *count_out = count;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_metabolic_adapter_get_energy_charge(
    nimcp_metabolic_adapter_t adapter,
    float* energy_charge_out
) {
    if (!adapter || !energy_charge_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *energy_charge_out = adapter->state.energy_ratio;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_metabolic_adapter_get_state(
    nimcp_metabolic_adapter_t adapter,
    nimcp_metabolic_adapter_state_t* state_out
) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_metabolic_adapter_get_stats(
    nimcp_metabolic_adapter_t adapter,
    nimcp_metabolic_adapter_stats_t* stats_out
) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_metabolic_adapter_reset_stats(nimcp_metabolic_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
