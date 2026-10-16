/**
 * @file nimcp_rate_limiter_immune_bridge.c
 * @brief Rate Limiter-Immune System Integration Bridge Implementation
 */

#include "security/immune/nimcp_rate_limiter_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rate_limiter_immune_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute cytokine effects on rate limits
 */
static void compute_cytokine_effects(
    rate_limiter_immune_bridge_t* bridge,
    const brain_immune_system_t* immune
) {
    if (!bridge || !immune) return;

    /* Reset effects */
    memset(&bridge->cytokine_effects, 0, sizeof(rate_limiter_cytokine_effects_t));

    /* Compute individual cytokine effects */
    bridge->cytokine_effects.il1_rps_reduction = CYTOKINE_IL1_RPS_IMPACT;
    bridge->cytokine_effects.il6_rps_reduction = CYTOKINE_IL6_RPS_IMPACT;
    bridge->cytokine_effects.tnf_rps_reduction = CYTOKINE_TNF_RPS_IMPACT;
    bridge->cytokine_effects.ifn_gamma_rps_reduction = CYTOKINE_IFN_GAMMA_RPS_IMPACT;
    bridge->cytokine_effects.il10_rps_increase = CYTOKINE_IL10_RPS_IMPACT;

    /* Aggregate modulation */
    bridge->cytokine_effects.total_rps_modulation =
        bridge->cytokine_effects.il1_rps_reduction +
        bridge->cytokine_effects.il6_rps_reduction +
        bridge->cytokine_effects.tnf_rps_reduction +
        bridge->cytokine_effects.ifn_gamma_rps_reduction +
        bridge->cytokine_effects.il10_rps_increase;

    /* Similar for burst */
    bridge->cytokine_effects.total_burst_modulation =
        bridge->cytokine_effects.total_rps_modulation * 0.8f; /* Burst affected similarly */

    /* Compute effective factors */
    bridge->cytokine_effects.effective_rps_factor =
        1.0f + bridge->cytokine_effects.total_rps_modulation;
    bridge->cytokine_effects.effective_burst_factor =
        1.0f + bridge->cytokine_effects.total_burst_modulation;

    /* Clamp to min values */
    if (bridge->cytokine_effects.effective_rps_factor < bridge->config.min_rps_factor) {
        bridge->cytokine_effects.effective_rps_factor = bridge->config.min_rps_factor;
    }
    if (bridge->cytokine_effects.effective_burst_factor < bridge->config.min_burst_factor) {
        bridge->cytokine_effects.effective_burst_factor = bridge->config.min_burst_factor;
    }
}

/**
 * @brief Compute inflammation effects on rate limits
 */
static void compute_inflammation_effects(
    rate_limiter_immune_bridge_t* bridge,
    const brain_immune_system_t* immune
) {
    if (!bridge || !immune) return;

    /* Get current inflammation level */
    brain_immune_phase_t phase = brain_immune_get_phase((brain_immune_system_t*)immune);

    /* Map phase to inflammation */
    brain_inflammation_level_t level = INFLAMMATION_NONE;
    if (phase >= IMMUNE_PHASE_ACTIVATION) {
        level = INFLAMMATION_LOCAL;
    }
    if (phase >= IMMUNE_PHASE_EFFECTOR) {
        level = INFLAMMATION_REGIONAL;
    }

    bridge->inflammation_state.current_level = level;

    /* Set RPS factor based on level */
    switch (level) {
        case INFLAMMATION_NONE:
            bridge->inflammation_state.rps_factor = INFLAMMATION_NONE_RPS_FACTOR;
            bridge->inflammation_state.burst_factor = INFLAMMATION_NONE_BURST_FACTOR;
            break;
        case INFLAMMATION_LOCAL:
            bridge->inflammation_state.rps_factor = INFLAMMATION_LOCAL_RPS_FACTOR;
            bridge->inflammation_state.burst_factor = INFLAMMATION_LOCAL_BURST_FACTOR;
            break;
        case INFLAMMATION_REGIONAL:
            bridge->inflammation_state.rps_factor = INFLAMMATION_REGIONAL_RPS_FACTOR;
            bridge->inflammation_state.burst_factor = INFLAMMATION_REGIONAL_BURST_FACTOR;
            break;
        case INFLAMMATION_SYSTEMIC:
            bridge->inflammation_state.rps_factor = INFLAMMATION_SYSTEMIC_RPS_FACTOR;
            bridge->inflammation_state.burst_factor = INFLAMMATION_SYSTEMIC_BURST_FACTOR;
            break;
        case INFLAMMATION_STORM:
            bridge->inflammation_state.rps_factor = INFLAMMATION_STORM_RPS_FACTOR;
            bridge->inflammation_state.burst_factor = INFLAMMATION_STORM_BURST_FACTOR;
            break;
    }

    /* Set mode flags */
    bridge->inflammation_state.emergency_mode = (level == INFLAMMATION_STORM);
    bridge->inflammation_state.resource_conservation_mode = (level >= INFLAMMATION_SYSTEMIC);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int rate_limiter_immune_default_config(rate_limiter_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    memset(config, 0, sizeof(rate_limiter_immune_config_t));

    /* Feature enables */
    config->enable_cytokine_rps_modulation = true;
    config->enable_inflammation_throttling = true;
    config->enable_violation_antigen_presentation = true;
    config->enable_blocked_client_quarantine = true;
    config->enable_auto_inflammation_trigger = true;

    /* Rate modulation config */
    config->base_rps = 100.0f;
    config->base_burst_size = 150.0f;
    config->min_rps_factor = 0.1f; /* Never go below 10% */
    config->min_burst_factor = 0.1f;

    /* Violation mapping config */
    config->min_violations_for_antigen = VIOLATION_ANTIGEN_THRESHOLD;
    config->violations_for_inflammation = VIOLATION_INFLAMMATION_THRESHOLD;
    config->violation_severity_multiplier = 1.0f;

    /* Auto-response */
    config->auto_quarantine_on_block = true;
    config->auto_memory_on_repeated_violations = true;

    return 0;
}

rate_limiter_immune_bridge_t* rate_limiter_immune_create(
    const rate_limiter_immune_config_t* config,
    nimcp_rate_limiter_t rate_limiter,
    brain_immune_system_t* immune_system
) {
    /* Guard clauses */
    if (!rate_limiter || !immune_system) {
        NIMCP_LOGGING_ERROR("Invalid parameters for rate limiter immune bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_limiter_immune_create: required parameter is NULL (rate_limiter, immune_system)");
        return NULL;
    }

    /* Allocate bridge */
    rate_limiter_immune_bridge_t* bridge = (rate_limiter_immune_bridge_t*)
        nimcp_malloc(sizeof(rate_limiter_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate rate limiter immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rate_limiter_immune_create: failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(rate_limiter_immune_bridge_t));

    /* Set handles */
    bridge->rate_limiter = rate_limiter;
    bridge->immune_system = immune_system;

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        rate_limiter_immune_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "rate_limiter_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for rate limiter immune bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rate_limiter_immune_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created rate limiter immune bridge");
    return bridge;
}

void rate_limiter_immune_destroy(rate_limiter_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        rate_limiter_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed rate limiter immune bridge");
}

/* ============================================================================
 * Update and Modulation API
 * ============================================================================ */

int rate_limiter_immune_update(rate_limiter_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute cytokine effects */
    if (bridge->config.enable_cytokine_rps_modulation) {
        compute_cytokine_effects(bridge, bridge->immune_system);
        bridge->rps_modulations++;
    }

    /* Compute inflammation effects */
    if (bridge->config.enable_inflammation_throttling) {
        compute_inflammation_effects(bridge, bridge->immune_system);
    }

    bridge->total_updates++;
    bridge->last_update_time = 0; /* Would use actual timestamp */

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rate_limiter_immune_apply_modulation(rate_limiter_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute effective RPS combining cytokine and inflammation effects */
    float effective_rps = bridge->config.base_rps;
    effective_rps *= bridge->inflammation_state.rps_factor;
    effective_rps *= bridge->cytokine_effects.effective_rps_factor;

    /* Compute effective burst */
    float effective_burst = bridge->config.base_burst_size;
    effective_burst *= bridge->inflammation_state.burst_factor;
    effective_burst *= bridge->cytokine_effects.effective_burst_factor;

    /* Clamp to min values */
    float min_rps = bridge->config.base_rps * bridge->config.min_rps_factor;
    if (effective_rps < min_rps) effective_rps = min_rps;

    float min_burst = bridge->config.base_burst_size * bridge->config.min_burst_factor;
    if (effective_burst < min_burst) effective_burst = min_burst;

    /* Would update rate limiter here */
    /* nimcp_rate_limiter_set_rate(bridge->rate_limiter, effective_rps); */
    /* nimcp_rate_limiter_set_burst(bridge->rate_limiter, (uint32_t)effective_burst); */

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rate_limiter_immune_present_violation(
    rate_limiter_immune_bridge_t* bridge,
    const char* client_id,
    uint32_t violation_count,
    uint32_t* antigen_id
) {
    if (!bridge || !client_id || !antigen_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_limiter_immune_present_violation: required parameter is NULL (bridge, client_id, antigen_id)");
        return -1;
    }
    if (!bridge->config.enable_violation_antigen_presentation) return 0;

    /* Check if violations are high enough */
    if (violation_count < bridge->config.min_violations_for_antigen) {
        return 0; /* Not presented */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute severity from violation count */
    uint32_t severity = VIOLATION_SEVERITY_BASE +
        (violation_count - 1) * VIOLATION_SEVERITY_PER_COUNT;
    if (severity > 10) severity = 10;

    /* Create epitope from client ID hash */
    uint8_t epitope[CLIENT_ID_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));
    size_t id_len = strlen(client_id);
    if (id_len > sizeof(epitope)) id_len = sizeof(epitope);
    memcpy(epitope, client_id, id_len);

    /* Present to immune system */
    int ret = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        sizeof(epitope),
        severity,
        0, /* source node */
        antigen_id
    );

    if (ret == 0) {
        bridge->antigens_presented++;
        bridge->immune_modulation.antigens_presented++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return ret;
}

int rate_limiter_immune_quarantine_client(
    rate_limiter_immune_bridge_t* bridge,
    const char* client_id
) {
    if (!bridge || !client_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_limiter_immune_quarantine_client: required parameter is NULL (bridge, client_id)");
        return -1;
    }
    if (!bridge->config.enable_blocked_client_quarantine) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Would activate killer T cell action here */
    /* This maps client block to immune quarantine */

    bridge->quarantine_actions++;
    bridge->immune_modulation.quarantine_actions++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int rate_limiter_immune_connect_bio_async(rate_limiter_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_RATE_LIMITER,
        .module_name = "rate_limiter_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Rate limiter immune bridge connected to bio-async router");
    }

    return bridge->base.bio_ctx ? 0 : -1;
}

int rate_limiter_immune_disconnect_bio_async(rate_limiter_immune_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return -1;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Rate limiter immune bridge disconnected from bio-async router");
    return 0;
}

bool rate_limiter_immune_is_bio_async_connected(const rate_limiter_immune_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

float rate_limiter_immune_get_effective_rps(const rate_limiter_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    float rps = bridge->config.base_rps;
    rps *= bridge->inflammation_state.rps_factor;
    rps *= bridge->cytokine_effects.effective_rps_factor;
    return rps;
}

uint32_t rate_limiter_immune_get_effective_burst(const rate_limiter_immune_bridge_t* bridge) {
    if (!bridge) return 0;

    float burst = bridge->config.base_burst_size;
    burst *= bridge->inflammation_state.burst_factor;
    burst *= bridge->cytokine_effects.effective_burst_factor;
    return (uint32_t)burst;
}

bool rate_limiter_immune_is_emergency_mode(const rate_limiter_immune_bridge_t* bridge) {
    return bridge ? bridge->inflammation_state.emergency_mode : false;
}
