/**
 * @file nimcp_snn_immune.c
 * @brief Brain immune system integration implementation for SNN
 *
 * WHAT: Implements bidirectional SNN-immune integration
 * WHY:  Enable immune-modulated learning and instability detection
 * HOW:  Cytokine effects, health monitoring, threat reporting
 *
 * @author NIMCP Team
 * @date 2024
 */

#include "snn/nimcp_snn_immune.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_immune)

/*=============================================================================
 * Default Configuration
 *===========================================================================*/

void snn_immune_config_default(snn_immune_config_t* config) {
    if (!config) return;

    /* Inflammation effects on STDP (fever model) */
    config->stdp_il1_factor = 0.7f;     /* IL-1 reduces LTP by 30% */
    config->stdp_il6_factor = 0.8f;     /* IL-6 reduces by 20% */
    config->stdp_tnf_factor = 0.75f;    /* TNF-alpha by 25% */
    config->stdp_il10_factor = 1.1f;    /* IL-10 is protective */

    /* Excitability effects */
    config->threshold_shift_per_level = 2.0f;  /* +2mV per inflammation level */
    config->rate_suppression_factor = 0.15f;   /* 15% rate reduction per level */

    /* Instability thresholds */
    config->max_spike_rate = 100.0f;    /* 100 Hz max (epileptiform) */
    config->min_spike_rate = 0.1f;      /* 0.1 Hz min (silent) */
    config->burst_threshold = 0.3f;     /* 30% burst ratio */
    config->sync_threshold = 0.8f;      /* 80% synchrony (hypersync) */

    /* Response configuration */
    config->auto_report_instabilities = true;
    config->enable_learning_modulation = true;
    config->update_interval_ms = 100.0f;  /* 10 Hz update */

    /* Bio-async */
    config->enable_bio_async = false;
}

/*=============================================================================
 * Bridge Lifecycle
 *===========================================================================*/

snn_immune_bridge_t* snn_immune_bridge_create(
    const snn_immune_config_t* config,
    snn_network_t* network,
    brain_immune_system_t* immune
) {
    if (!config || !network || !immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_immune_bridge_create: NULL %s",
            !config ? "config" : !network ? "network" : "immune");
        return NULL;
    }

    snn_immune_bridge_t* bridge = nimcp_malloc(sizeof(snn_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_immune_bridge_t),
            "snn_immune_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_immune_bridge_t));
    bridge->network = network;
    bridge->immune = immune;
    memcpy(&bridge->config, config, sizeof(snn_immune_config_t));

    /* Initialize bridge base (mutex + module name) */
    if (bridge_base_init(&bridge->base, 0, "snn_immune") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize effects to neutral */
    bridge->effects.stdp_amplitude_factor = 1.0f;
    bridge->effects.learning_rate_factor = 1.0f;
    bridge->effects.threshold_shift = 0.0f;
    bridge->effects.excitability_factor = 1.0f;
    bridge->effects.refractory_extension = 0.0f;
    bridge->effects.current_level = INFLAMMATION_NONE;

    /* Initialize health to healthy */
    bridge->health.health = SNN_STATE_HEALTHY;
    bridge->health.has_instability = false;

    bridge->connected = true;

    NIMCP_LOGGING_DEBUG("Created SNN-immune bridge for network %u", network->id);

    return bridge;
}

void snn_immune_bridge_destroy(snn_immune_bridge_t* bridge) {
    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int snn_immune_bridge_connect_bio_async(snn_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_immune_bridge_connect_bio_async: NULL bridge");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;  /* Already connected */

    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_INFO("Bio-async router not available for SNN immune bridge");
        return SNN_ERROR_NOT_INITIALIZED;
    }

    char module_name[NIMCP_ID_BUFFER_SIZE];
    snprintf(module_name, sizeof(module_name), "snn_immune_%u", bridge->network->id);

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_IMMUNE,
        .module_name = module_name,
        .inbox_capacity = 16,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (!bridge->base.bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "SNN immune bridge: failed to register bio-async module");
        return SNN_ERROR_OPERATION_FAILED;
    }

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("SNN immune bridge connected to bio-async");

    return 0;
}

int snn_immune_bridge_disconnect_bio_async(snn_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_immune_bridge_disconnect_bio_async: NULL bridge");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_immune_bridge_is_bio_async_connected(const snn_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/*=============================================================================
 * Update Functions
 *===========================================================================*/

/**
 * @brief Compute STDP factor from cytokine levels
 */
static float compute_stdp_factor(const snn_immune_bridge_t* bridge, float cytokine_level, float factor) {
    (void)bridge;
    /* Cytokine effect is multiplicative */
    float effect = 1.0f - (1.0f - factor) * cytokine_level;
    if (effect < 0.1f) effect = 0.1f;  /* Min 10% */
    if (effect > 1.5f) effect = 1.5f;  /* Max 150% */
    return effect;
}

int snn_immune_update_effects(snn_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_immune_update_effects: NULL %s", !bridge ? "bridge" : "immune system");
        return SNN_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Get current inflammation level */
    bridge->effects.current_level = brain_immune_get_inflammation_level(bridge->immune);

    /* Get cytokine levels using individual queries */
    float il1_level = brain_immune_get_cytokine_level(bridge->immune, BRAIN_CYTOKINE_IL1);
    float il6_level = brain_immune_get_cytokine_level(bridge->immune, BRAIN_CYTOKINE_IL6);
    float tnf_level = brain_immune_get_cytokine_level(bridge->immune, BRAIN_CYTOKINE_TNF);
    float il10_level = brain_immune_get_cytokine_level(bridge->immune, BRAIN_CYTOKINE_IL10);

    /* Compute STDP amplitude factor */
    float factor = 1.0f;
    factor *= compute_stdp_factor(bridge, il1_level, bridge->config.stdp_il1_factor);
    factor *= compute_stdp_factor(bridge, il6_level, bridge->config.stdp_il6_factor);
    factor *= compute_stdp_factor(bridge, tnf_level, bridge->config.stdp_tnf_factor);
    factor *= compute_stdp_factor(bridge, il10_level, bridge->config.stdp_il10_factor);
    bridge->effects.stdp_amplitude_factor = factor;

    /* Learning rate factor based on inflammation level */
    float lr_factor = 1.0f;
    switch (bridge->effects.current_level) {
        case INFLAMMATION_NONE:
            lr_factor = 1.0f;
            break;
        case INFLAMMATION_LOCAL:
            lr_factor = 0.95f;
            break;
        case INFLAMMATION_REGIONAL:
            lr_factor = 0.80f;
            break;
        case INFLAMMATION_SYSTEMIC:
            lr_factor = 0.50f;
            break;
        case INFLAMMATION_STORM:
            lr_factor = 0.10f;  /* Minimal learning during cytokine storm */
            break;
        default:
            lr_factor = 1.0f;
    }
    bridge->effects.learning_rate_factor = lr_factor;

    /* Threshold shift */
    bridge->effects.threshold_shift = (float)bridge->effects.current_level *
                                       bridge->config.threshold_shift_per_level;

    /* Excitability factor */
    bridge->effects.excitability_factor = 1.0f -
        (float)bridge->effects.current_level * bridge->config.rate_suppression_factor;
    if (bridge->effects.excitability_factor < 0.2f) {
        bridge->effects.excitability_factor = 0.2f;
    }

    /* Refractory extension during inflammation */
    bridge->effects.refractory_extension = (float)bridge->effects.current_level * 0.5f;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int snn_immune_compute_health(snn_immune_bridge_t* bridge) {
    if (!bridge || !bridge->network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_immune_compute_health: NULL %s", !bridge ? "bridge" : "network");
        return SNN_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Get network statistics */
    snn_stats_t net_stats;
    snn_network_get_stats(bridge->network, &net_stats);

    /* Compute metrics */
    bridge->health.mean_rate = net_stats.mean_firing_rate;
    bridge->health.max_rate = net_stats.max_firing_rate;
    bridge->health.min_rate = net_stats.mean_firing_rate * 0.5f;  /* Estimate */

    /* Simple health check */
    bridge->health.has_instability = false;
    bridge->health.health = SNN_STATE_HEALTHY;

    /* Check for hyperexcitation */
    if (bridge->health.mean_rate > bridge->config.max_spike_rate) {
        bridge->health.has_instability = true;
        bridge->health.health = SNN_STATE_EXPLOSION;
        bridge->instability_count++;
    }

    /* Check for silence */
    if (bridge->health.mean_rate < bridge->config.min_spike_rate) {
        bridge->health.has_instability = true;
        bridge->health.health = SNN_STATE_SILENT;
        bridge->instability_count++;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int snn_immune_update(snn_immune_bridge_t* bridge, float t) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_immune_update: NULL bridge");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Check update interval */
    float dt = t - bridge->last_update_time;
    if (dt < bridge->config.update_interval_ms) {
        return 0;  /* Not time yet */
    }

    bridge->last_update_time = t;

    /* Update effects from immune system */
    int result = snn_immune_update_effects(bridge);
    if (result != 0) return result;

    /* Compute health metrics */
    result = snn_immune_compute_health(bridge);
    if (result != 0) return result;

    /* Auto-report if enabled */
    if (bridge->config.auto_report_instabilities && bridge->health.has_instability) {
        snn_immune_check_and_report(bridge);
    }

    return 0;
}

/*=============================================================================
 * Modulation Functions
 *===========================================================================*/

void snn_immune_modulate_stdp(
    const snn_immune_bridge_t* bridge,
    float* a_plus,
    float* a_minus
) {
    if (!bridge || !a_plus || !a_minus) return;

    float factor = bridge->effects.stdp_amplitude_factor;
    *a_plus *= factor;
    *a_minus *= factor;
}

float snn_immune_modulate_threshold(
    const snn_immune_bridge_t* bridge,
    float base_threshold
) {
    if (!bridge) return base_threshold;
    return base_threshold + bridge->effects.threshold_shift;
}

float snn_immune_modulate_learning_rate(
    const snn_immune_bridge_t* bridge,
    float base_lr
) {
    if (!bridge) return base_lr;
    return base_lr * bridge->effects.learning_rate_factor;
}

/*=============================================================================
 * Instability Reporting
 *===========================================================================*/

int snn_immune_report_instability(
    snn_immune_bridge_t* bridge,
    snn_state_health_t instability_type,
    uint8_t severity
) {
    if (!bridge || !bridge->immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_immune_report_instability: NULL %s", !bridge ? "bridge" : "immune system");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Create epitope from instability type */
    uint8_t epitope[8];
    memset(epitope, 0, sizeof(epitope));
    epitope[0] = (uint8_t)instability_type;
    epitope[1] = severity;
    epitope[2] = (bridge->network->id >> 8) & 0xFF;
    epitope[3] = bridge->network->id & 0xFF;

    /* Present to immune system */
    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        bridge->immune,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        sizeof(epitope),
        severity,
        bridge->network->id,
        &antigen_id
    );

    if (result == 0) {
        bridge->immune_reports++;
        NIMCP_LOGGING_WARN("SNN immune: reported instability type %d, severity %u",
                          instability_type, severity);
    }

    return result;
}

uint32_t snn_immune_check_and_report(snn_immune_bridge_t* bridge) {
    if (!bridge) return 0;

    /* Compute current health */
    snn_immune_compute_health(bridge);

    if (!bridge->health.has_instability) {
        return 0;
    }

    uint32_t reports = 0;

    /* Report based on type */
    uint8_t severity = 5;  /* Default medium severity */

    if (bridge->health.health == SNN_STATE_EXPLOSION) {
        severity = 8;  /* High severity for potential seizure */
    } else if (bridge->health.health == SNN_STATE_SILENT) {
        severity = 6;
    }

    if (snn_immune_report_instability(bridge, bridge->health.health, severity) == 0) {
        reports++;
    }

    return reports;
}

/*=============================================================================
 * Query Functions
 *===========================================================================*/

int snn_immune_get_effects(
    const snn_immune_bridge_t* bridge,
    snn_cytokine_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_immune_get_effects: NULL %s", !bridge ? "bridge" : "effects");
        return SNN_ERROR_NULL_POINTER;
    }
    memcpy(effects, &bridge->effects, sizeof(snn_cytokine_effects_t));
    return 0;
}

int snn_immune_get_health(
    const snn_immune_bridge_t* bridge,
    snn_health_metrics_t* health
) {
    if (!bridge || !health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_immune_get_health: NULL %s", !bridge ? "bridge" : "health");
        return SNN_ERROR_NULL_POINTER;
    }
    memcpy(health, &bridge->health, sizeof(snn_health_metrics_t));
    return 0;
}

brain_inflammation_level_t snn_immune_get_inflammation(const snn_immune_bridge_t* bridge) {
    if (!bridge) return INFLAMMATION_NONE;
    return bridge->effects.current_level;
}

bool snn_immune_is_network_healthy(const snn_immune_bridge_t* bridge) {
    if (!bridge) return true;  /* Assume healthy if no bridge */
    return !bridge->health.has_instability;
}

/*=============================================================================
 * Statistics
 *===========================================================================*/

int snn_immune_get_stats(
    const snn_immune_bridge_t* bridge,
    uint32_t* instability_count,
    uint32_t* reports_sent,
    uint32_t* updates
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_immune_get_stats: NULL bridge");
        return SNN_ERROR_NULL_POINTER;
    }

    if (instability_count) *instability_count = bridge->instability_count;
    if (reports_sent) *reports_sent = bridge->immune_reports;
    if (updates) *updates = 0;  /* Not tracked currently */

    return 0;
}

void snn_immune_reset_stats(snn_immune_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    bridge->instability_count = 0;
    bridge->immune_reports = 0;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
}
