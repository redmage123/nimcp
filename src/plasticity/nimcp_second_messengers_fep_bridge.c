/**
 * @file nimcp_second_messengers_fep_bridge.c
 * @brief Second Messengers FEP Bridge Implementation
 */

#include "plasticity/nimcp_second_messengers_fep_bridge.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include <string.h>
#include <math.h>
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(second_messengers_fep_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(sm_fep_bridge)

int sm_fep_bridge_default_config(sm_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Config is NULL in default_config");
        return -1;
    }
    config->ca_pe_sensitivity = SM_FEP_CA_PE_GAIN;
    config->camp_precision_sensitivity = SM_FEP_CAMP_PRECISION_GAIN;
    config->ip3_complexity_sensitivity = SM_FEP_IP3_COMPLEXITY_GAIN;
    config->creb_efe_sensitivity = SM_FEP_CREB_EFE_THRESHOLD;
    config->enable_ca_pe_coupling = true;
    config->enable_camp_precision_coupling = true;
    config->enable_ip3_complexity_coupling = true;
    config->enable_creb_plasticity_coupling = true;
    return 0;
}

sm_fep_bridge_t* sm_fep_bridge_create(const sm_fep_config_t* config, uint32_t neuron_id) {
    sm_fep_bridge_t* bridge = (sm_fep_bridge_t*)nimcp_malloc(sizeof(sm_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate sm_fep_bridge");
        return NULL;
    }

    if (config) memcpy(&bridge->config, config, sizeof(sm_fep_config_t));
    else sm_fep_bridge_default_config(&bridge->config);

    memset(&bridge->fep_effects, 0, sizeof(sm_fep_effects_t));
    memset(&bridge->sm_effects, 0, sizeof(sm_fep_feedback_t));
    memset(&bridge->stats, 0, sizeof(sm_fep_stats_t));

    if (bridge_base_init(&bridge->base, 0, "second_messengers_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create mutex");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->fep_system = NULL;
    bridge->sm_system = NULL;
    bridge->neuron_id = neuron_id;
    bridge->base.bio_async_enabled = false;
    return bridge;
}

void sm_fep_bridge_destroy(sm_fep_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) sm_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int sm_fep_bridge_connect_fep(sm_fep_bridge_t* bridge, fep_system_t* fep) {
    if (!bridge || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in connect_fep");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sm_fep_bridge_connect_sm(sm_fep_bridge_t* bridge, second_messenger_system_t* sm) {
    if (!bridge || !sm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in connect_sm");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->sm_system = sm;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sm_fep_bridge_disconnect(sm_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Bridge is NULL in disconnect");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->sm_system = NULL;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float sm_fep_compute_ca_from_pe(sm_fep_bridge_t* bridge, float pe) {
    if (!bridge || !bridge->config.enable_ca_pe_coupling) return 0.0f;
    /* Ca²⁺ release proportional to prediction error magnitude */
    return fabsf(pe) * bridge->config.ca_pe_sensitivity;
}

float sm_fep_compute_camp_from_precision(sm_fep_bridge_t* bridge, float precision) {
    if (!bridge || !bridge->config.enable_camp_precision_coupling) return 0.0f;
    /* cAMP levels proportional to precision */
    return precision * bridge->config.camp_precision_sensitivity;
}

float sm_fep_compute_ip3_from_complexity(sm_fep_bridge_t* bridge, float complexity) {
    if (!bridge || !bridge->config.enable_ip3_complexity_coupling) return 0.0f;
    /* IP3 release proportional to complexity cost */
    return complexity * bridge->config.ip3_complexity_sensitivity;
}

int sm_fep_trigger_creb_from_efe(sm_fep_bridge_t* bridge, float efe) {
    if (!bridge || !bridge->config.enable_creb_plasticity_coupling) return 0;
    if (!bridge->sm_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_LEARNING_FAILED, "SM system not connected in trigger_creb");
        return -1;
    }

    /* CREB phosphorylation if expected free energy exceeds threshold */
    if (efe > bridge->config.creb_efe_sensitivity) {
        bridge->stats.creb_phosphorylations++;
        NIMCP_LOGGING_DEBUG("CREB phosphorylation triggered by EFE");
    }

    return 0;
}

float sm_fep_get_plasticity_modulation(const sm_fep_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->sm_effects.plasticity_modulation;
}

int sm_fep_bridge_update(sm_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Bridge is NULL in update");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->fep_system && bridge->sm_system) {
        /* Get FEP state */
        float pe = fep_get_prediction_error(bridge->fep_system, 0);
        bridge->fep_effects.pe_magnitude = fabsf(pe);

        fep_belief_t beliefs;
        fep_get_beliefs(bridge->fep_system, 0, &beliefs);
        bridge->fep_effects.precision_value = beliefs.precision ? beliefs.precision[0] : 1.0f;

        fep_free_energy_t fe;
        fep_compute_free_energy(bridge->fep_system, &fe);
        bridge->fep_effects.complexity_value = fe.complexity;

        /* Compute second messenger modulation */
        bridge->fep_effects.ca_release = sm_fep_compute_ca_from_pe(bridge, pe);
        bridge->fep_effects.camp_modulation = sm_fep_compute_camp_from_precision(bridge, bridge->fep_effects.precision_value);
        bridge->fep_effects.ip3_modulation = sm_fep_compute_ip3_from_complexity(bridge, bridge->fep_effects.complexity_value);

        /* Apply Ca²⁺ injection if enabled */
        if (bridge->config.enable_ca_pe_coupling && bridge->fep_effects.ca_release > 0.0f) {
            second_messenger_inject_calcium(bridge->sm_system, bridge->neuron_id, bridge->fep_effects.ca_release, delta_ms);
            bridge->stats.ca_releases++;
        }

        /* Get second messenger state */
        second_messenger_state_t sm_state;
        if (second_messenger_get_state(bridge->sm_system, bridge->neuron_id, &sm_state) == NIMCP_SUCCESS) {
            bridge->sm_effects.ca_level = sm_state.calcium.ca_cytoplasmic;
            bridge->sm_effects.camp_level = sm_state.camp.camp_concentration;
            bridge->sm_effects.pka_activity = sm_state.camp.pka_activity;
            bridge->sm_effects.camkii_activity = sm_state.calcium.camkii_activity;
            bridge->sm_effects.creb_phosphorylation = sm_state.gene_expr.creb_phosphorylation;

            /* Compute plasticity modulation from kinase activities */
            bridge->sm_effects.plasticity_modulation = 1.0f +
                0.3f * bridge->sm_effects.pka_activity +
                0.5f * bridge->sm_effects.camkii_activity;

            /* Precision estimate from cAMP/PKA */
            bridge->sm_effects.precision_estimate = bridge->sm_effects.pka_activity;
        }

        /* Update statistics */
        bridge->stats.total_updates++;
        bridge->stats.avg_ca_level = (bridge->stats.avg_ca_level * (bridge->stats.total_updates - 1) + bridge->sm_effects.ca_level) / bridge->stats.total_updates;
        bridge->stats.avg_pka_activity = (bridge->stats.avg_pka_activity * (bridge->stats.total_updates - 1) + bridge->sm_effects.pka_activity) / bridge->stats.total_updates;
        bridge->stats.avg_plasticity_modulation = (bridge->stats.avg_plasticity_modulation * (bridge->stats.total_updates - 1) + bridge->sm_effects.plasticity_modulation) / bridge->stats.total_updates;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int sm_fep_bridge_get_stats(const sm_fep_bridge_t* bridge, sm_fep_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in get_stats");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(sm_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sm_fep_bridge_connect_bio_async(sm_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Bridge is NULL in connect_bio_async");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_SECOND_MESSENGERS_BRIDGE,
        .module_name = "second_messengers_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
    }
    return 0;
}

int sm_fep_bridge_disconnect_bio_async(sm_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Bridge is NULL in disconnect_bio_async");
        return -1;
    }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool sm_fep_bridge_is_bio_async_connected(const sm_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
