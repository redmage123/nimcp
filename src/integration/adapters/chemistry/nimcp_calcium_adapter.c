/**
 * @file nimcp_calcium_adapter.c
 * @brief Calcium Signaling Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements calcium signaling dynamics for Chemistry layer
 * WHY:  Calcium is a key second messenger controlling plasticity
 * HOW:  Models calcium influx, decay, and downstream signaling
 *
 * @author NIMCP Development Team
 */

#include "integration/adapters/chemistry/nimcp_calcium_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(calcium_adapter)

//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_calcium_adapter_struct {
    nimcp_calcium_adapter_config_t config;
    nimcp_module_interface_t interface;

    /* Calcium state */
    float* calcium_conc;                /**< [Ca2+] per compartment (nM) */
    float* calcium_er;                  /**< ER calcium store (nM) */

    /* Kinase state */
    float camkii_active;                /**< CaMKII activation fraction */
    float calcineurin_active;           /**< Calcineurin activation */

    /* State tracking */
    nimcp_calcium_adapter_state_t state;
    nimcp_calcium_adapter_stats_t stats;
    bool is_initialized;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Hill function for cooperative binding
 */
static float hill_function(float x, float k, float n) {
    float xn = powf(x, n);
    float kn = powf(k, n);
    return xn / (xn + kn);
}

//=============================================================================
// Module Interface Callbacks
//=============================================================================

static nimcp_layer_error_t calcium_init(void* module, void* config) {
    nimcp_calcium_adapter_t adapter = (nimcp_calcium_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    if (config) {
        adapter->config = *(nimcp_calcium_adapter_config_t*)config;
    }

    /* Allocate arrays */
    uint32_t n = adapter->config.num_compartments;
    adapter->calcium_conc = (float*)nimcp_calloc(n, sizeof(float));
    adapter->calcium_er = (float*)nimcp_calloc(n, sizeof(float));

    if (!adapter->calcium_conc || !adapter->calcium_er) {
        return NIMCP_LAYER_ERR_NO_MEMORY;
    }

    /* Initialize to basal calcium */
    for (uint32_t i = 0; i < n; i++) {
        adapter->calcium_conc[i] = adapter->config.basal_calcium_nm;
        adapter->calcium_er[i] = 100000.0f;  /* ER stores ~100 uM */
    }

    adapter->camkii_active = 0.0f;
    adapter->calcineurin_active = 0.0f;

    adapter->is_initialized = true;
    adapter->state.is_active = true;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t calcium_shutdown(void* module) {
    nimcp_calcium_adapter_t adapter = (nimcp_calcium_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    nimcp_free(adapter->calcium_conc);
    nimcp_free(adapter->calcium_er);

    adapter->calcium_conc = NULL;
    adapter->calcium_er = NULL;

    adapter->is_initialized = false;
    adapter->state.is_active = false;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t calcium_update(void* module, float dt) {
    nimcp_calcium_adapter_t adapter = (nimcp_calcium_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t n = adapter->config.num_compartments;
    float tau = adapter->config.decay_tau_ms;
    float basal = adapter->config.basal_calcium_nm;
    float dt_ms = dt * 1000.0f;

    float sum_ca = 0.0f;
    float max_ca = 0.0f;

    /* Update calcium dynamics for each compartment */
    for (uint32_t i = 0; i < n; i++) {
        float ca = adapter->calcium_conc[i];

        /* Exponential decay toward basal */
        float decay = (ca - basal) / tau;
        ca -= decay * dt_ms;

        /* ER pump removes excess calcium */
        if (adapter->config.enable_store_release && ca > basal * 2.0f) {
            float pump = adapter->config.er_pump_rate * (ca - basal);
            ca -= pump * dt_ms;
            adapter->calcium_er[i] += pump * dt_ms;
        }

        /* Clamp to reasonable range */
        if (ca < 0.0f) ca = 0.0f;
        if (ca > 100000.0f) ca = 100000.0f;

        adapter->calcium_conc[i] = ca;
        sum_ca += ca;
        if (ca > max_ca) max_ca = ca;
    }

    adapter->state.mean_calcium_nm = sum_ca / (float)n;
    adapter->state.max_calcium_nm = max_ca;

    /* Update CaMKII dynamics */
    if (adapter->config.enable_camkii) {
        /* CaMKII has Hill coefficient ~4 for Ca2+/CaM binding */
        float ca_target = hill_function(adapter->state.mean_calcium_nm, 500.0f, 4.0f);
        /* CaMKII activation with slow time constant */
        float camkii_tau = 100.0f;  /* ms */
        adapter->camkii_active += (ca_target - adapter->camkii_active) / camkii_tau * dt_ms;

        /* Calcineurin has lower affinity but also activated by calcium */
        float cn_target = hill_function(adapter->state.mean_calcium_nm, 300.0f, 2.0f);
        float cn_tau = 50.0f;  /* ms, faster than CaMKII */
        adapter->calcineurin_active += (cn_target - adapter->calcineurin_active) / cn_tau * dt_ms;
    }

    adapter->state.camkii_activity = adapter->camkii_active;
    adapter->state.calcineurin_activity = adapter->calcineurin_active;

    adapter->stats.updates_processed++;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t calcium_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_calcium_adapter_t adapter = (nimcp_calcium_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->stats.messages_handled++;

    switch (msg->header.msg_type) {
        case NIMCP_LAYER_MSG_DATA_PUSH:
            /* Incoming voltage/spike data triggers calcium influx */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float voltage = *(float*)msg->payload;
                /* VGCC-mediated influx proportional to voltage */
                if (voltage > -40.0f) {
                    float influx = adapter->config.vgcc_conductance *
                                   (voltage + 40.0f) * 10.0f;
                    for (uint32_t i = 0; i < adapter->config.num_compartments; i++) {
                        adapter->calcium_conc[i] += influx;
                    }
                    adapter->stats.influx_events++;
                    adapter->state.calcium_events++;
                }
            }
            break;

        case NIMCP_LAYER_MSG_EXCITE:
            /* NMDA-like calcium influx */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float glutamate = *(float*)msg->payload;
                float influx = adapter->config.nmda_fraction * glutamate * 100.0f;
                for (uint32_t i = 0; i < adapter->config.num_compartments; i++) {
                    adapter->calcium_conc[i] += influx;
                }
                adapter->stats.influx_events++;
            }
            break;

        default:
            break;
    }

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t calcium_get_state(void* module, void* state_out, size_t* size) {
    nimcp_calcium_adapter_t adapter = (nimcp_calcium_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;

    if (*size < sizeof(nimcp_calcium_adapter_state_t)) {
        *size = sizeof(nimcp_calcium_adapter_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }

    *(nimcp_calcium_adapter_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_calcium_adapter_state_t);

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t calcium_set_state(void* module, const void* state, size_t size) {
    nimcp_calcium_adapter_t adapter = (nimcp_calcium_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;

    if (size < sizeof(nimcp_calcium_adapter_state_t)) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    adapter->state = *(const nimcp_calcium_adapter_state_t*)state;

    return NIMCP_LAYER_OK;
}

static const char* calcium_get_name(void* module) {
    (void)module;
    return "Calcium_Signaling_Adapter";
}

//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_calcium_adapter_config_t nimcp_calcium_adapter_default_config(void) {
    nimcp_calcium_adapter_config_t config = {
        .num_compartments = 100,
        .basal_calcium_nm = 100.0f,     /* 100 nM resting [Ca2+] */
        .decay_tau_ms = 50.0f,          /* 50 ms calcium clearance */
        .vgcc_conductance = 0.5f,
        .nmda_fraction = 0.15f,         /* ~15% NMDA is Ca2+ permeable */
        .er_pump_rate = 0.01f,
        .enable_store_release = true,
        .enable_camkii = true,
        .enable_logging = false
    };
    return config;
}

nimcp_calcium_adapter_t nimcp_calcium_adapter_create(
    const nimcp_calcium_adapter_config_t* config
) {
    nimcp_calcium_adapter_t adapter = (nimcp_calcium_adapter_t)nimcp_calloc(
        1, sizeof(struct nimcp_calcium_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate calcium adapter");

    adapter->config = config ? *config : nimcp_calcium_adapter_default_config();

    /* Set up module interface */
    adapter->interface.init = calcium_init;
    adapter->interface.shutdown = calcium_shutdown;
    adapter->interface.update = calcium_update;
    adapter->interface.handle_message = calcium_handle_message;
    adapter->interface.get_state = calcium_get_state;
    adapter->interface.set_state = calcium_set_state;
    adapter->interface.get_name = calcium_get_name;

    return adapter;
}

void nimcp_calcium_adapter_destroy(nimcp_calcium_adapter_t adapter) {
    if (!adapter) return;

    if (adapter->is_initialized) {
        calcium_shutdown(adapter);
    }

    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_calcium_adapter_get_interface(
    nimcp_calcium_adapter_t adapter
) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_calcium_adapter_influx(
    nimcp_calcium_adapter_t adapter,
    int compartment_idx,
    float calcium_nm
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    if (compartment_idx < 0) {
        /* Add to all compartments */
        for (uint32_t i = 0; i < adapter->config.num_compartments; i++) {
            adapter->calcium_conc[i] += calcium_nm;
        }
    } else if ((uint32_t)compartment_idx < adapter->config.num_compartments) {
        adapter->calcium_conc[compartment_idx] += calcium_nm;
    } else {
        return NIMCP_LAYER_ERR_INVALID_MODULE;
    }

    adapter->stats.influx_events++;
    adapter->state.calcium_events++;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_calcium_adapter_get_calcium(
    nimcp_calcium_adapter_t adapter,
    float* calcium_out,
    uint32_t max_count,
    uint32_t* count_out
) {
    if (!adapter || !calcium_out || !count_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t count = adapter->config.num_compartments;
    if (count > max_count) count = max_count;

    memcpy(calcium_out, adapter->calcium_conc, count * sizeof(float));
    *count_out = count;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_calcium_adapter_get_kinase_activity(
    nimcp_calcium_adapter_t adapter,
    float* camkii_out,
    float* calcineurin_out
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    if (camkii_out) *camkii_out = adapter->camkii_active;
    if (calcineurin_out) *calcineurin_out = adapter->calcineurin_active;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_calcium_adapter_get_state(
    nimcp_calcium_adapter_t adapter,
    nimcp_calcium_adapter_state_t* state_out
) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_calcium_adapter_get_stats(
    nimcp_calcium_adapter_t adapter,
    nimcp_calcium_adapter_stats_t* stats_out
) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_calcium_adapter_reset_stats(nimcp_calcium_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
