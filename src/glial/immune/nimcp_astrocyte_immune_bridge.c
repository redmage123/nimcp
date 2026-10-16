/**
 * @file nimcp_astrocyte_immune_bridge.c
 * @brief Astrocyte-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "glial/immune/nimcp_astrocyte_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(astrocyte_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Compute inflammation level from immune system
 *
 * WHAT: Extract current inflammation level from brain immune system
 * WHY:  Need unified inflammation state for astrocyte modulation
 * HOW:  Query immune stats, map to inflammation level enum
 */
static brain_inflammation_level_t get_inflammation_level(
    brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(immune, &stats) != 0) {
        return INFLAMMATION_NONE;
    }

    /* Map inflammation sites to level */
    if (stats.inflammation_sites == 0) return INFLAMMATION_NONE;
    if (stats.inflammation_sites <= 2) return INFLAMMATION_LOCAL;
    if (stats.inflammation_sites <= 5) return INFLAMMATION_REGIONAL;
    if (stats.inflammation_sites <= 10) return INFLAMMATION_SYSTEMIC;
    return INFLAMMATION_STORM;
}

/**
 * @brief Get cytokine concentration from immune system
 *
 * WHAT: Query specific cytokine level
 * WHY:  Need cytokine levels to modulate astrocyte state
 * HOW:  Iterate through active cytokines, sum concentrations by type
 */
static float get_cytokine_concentration(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune || !immune->cytokines) return 0.0f;

    float total = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        if (immune->cytokines[i].type == type) {
            total += immune->cytokines[i].concentration;
        }
    }
    return total;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int astro_network_default_config(astro_network_config_t* config) {
    /* Guard: validate parameters */
    if (!config) {
        NIMCP_LOGGING_ERROR("astro_network_default_config: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astro_network_default_config: config is NULL");
        return -1;
    }

    /* Set biological defaults */
    config->enable_cytokine_reactivity = true;
    config->enable_inflammation_astrogliosis = true;
    config->enable_reactive_cytokine_production = true;
    config->enable_calcium_immune_signaling = true;
    config->enable_bbb_inflammation_coupling = true;

    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->reactivity_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    config->reactive_threshold = REACTIVE_ASTROCYTE_THRESHOLD;
    config->astrogliosis_threshold = ASTROGLIOSIS_THRESHOLD;
    config->calcium_immune_threshold = CALCIUM_WAVE_IMMUNE_THRESHOLD;

    return 0;
}

astro_network_bridge_t* astro_network_bridge_create(
    const astro_network_config_t* config,
    brain_immune_system_t* immune_system,
    astrocyte_network_t* astrocyte_network
) {
    /* Guard: validate critical parameters */
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("astro_network_bridge_create: NULL immune_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }
    if (!astrocyte_network) {
        NIMCP_LOGGING_ERROR("astro_network_bridge_create: NULL astrocyte_network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_network is NULL");

        return NULL;
    }

    /* Allocate bridge */
    astro_network_bridge_t* bridge = (astro_network_bridge_t*)
        nimcp_malloc(sizeof(astro_network_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("astro_network_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(astro_network_bridge_t));

    /* Use default config if not provided */
    astro_network_config_t default_config;
    if (!config) {
        astro_network_default_config(&default_config);
        config = &default_config;
    }

    /* Initialize handles */
    bridge->immune_system = immune_system;
    bridge->astrocyte_network = astrocyte_network;

    /* Copy configuration */
    bridge->enable_cytokine_reactivity = config->enable_cytokine_reactivity;
    bridge->enable_inflammation_astrogliosis = config->enable_inflammation_astrogliosis;
    bridge->enable_reactive_cytokine_production = config->enable_reactive_cytokine_production;
    bridge->enable_calcium_immune_signaling = config->enable_calcium_immune_signaling;
    bridge->enable_bbb_inflammation_coupling = config->enable_bbb_inflammation_coupling;

    /* Initialize state */
    memset(&bridge->cytokine_effects, 0, sizeof(cytokine_astro_network_effects_t));
    memset(&bridge->inflammation_state, 0, sizeof(inflammation_astro_network_state_t));
    memset(&bridge->astrocyte_modulation, 0, sizeof(astro_network_modulation_t));

    /* Initialize timing */
    bridge->last_update_time = nimcp_time_get_us();
    bridge->chronic_inflammation_accumulator = 0.0f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "astrocyte_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("astro_network_bridge_create: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "astro_network_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("astro_network_bridge_create: bridge created successfully");
    return bridge;
}

void astro_network_bridge_destroy(astro_network_bridge_t* bridge) {
    /* Guard: NULL-safe */
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        astro_network_disconnect_bio_async(bridge);
    }

    /* Destroy mutex (created with nimcp_platform_mutex_create) */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge */
    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → Astrocyte Implementation
 * ============================================================================ */

int astro_network_apply_cytokine_effects(astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system || !bridge->astrocyte_network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->enable_cytokine_reactivity) {
        return NIMCP_SUCCESS; /* Feature disabled */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Query cytokine levels */
    float il1 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float il10 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL10);
    float ifn_gamma = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);

    /* Compute reactivity effects */
    bridge->cytokine_effects.il1_reactivity_boost = il1 * CYTOKINE_IL1_REACTIVITY_IMPACT;
    bridge->cytokine_effects.il6_reactivity_boost = il6 * CYTOKINE_IL6_REACTIVITY_IMPACT;
    bridge->cytokine_effects.tnf_reactivity_boost = tnf * CYTOKINE_TNF_REACTIVITY_IMPACT;
    bridge->cytokine_effects.ifn_gamma_reactivity_boost = ifn_gamma * CYTOKINE_IFN_GAMMA_REACTIVITY_IMPACT;
    bridge->cytokine_effects.il10_reactivity_reduction = il10 * CYTOKINE_IL10_REACTIVITY_IMPACT;

    /* Total reactivity */
    bridge->cytokine_effects.total_reactivity = fmaxf(0.0f, fminf(1.0f,
        bridge->cytokine_effects.il1_reactivity_boost +
        bridge->cytokine_effects.il6_reactivity_boost +
        bridge->cytokine_effects.tnf_reactivity_boost +
        bridge->cytokine_effects.ifn_gamma_reactivity_boost +
        bridge->cytokine_effects.il10_reactivity_reduction
    ));

    /* Calcium modulation */
    bridge->cytokine_effects.calcium_amplitude_change =
        il1 * CYTOKINE_IL1_CALCIUM_BOOST +
        il6 * CYTOKINE_IL6_CALCIUM_BOOST +
        tnf * CYTOKINE_TNF_CALCIUM_BOOST +
        il10 * CYTOKINE_IL10_CALCIUM_REDUCTION;

    /* Glutamate clearance */
    bridge->cytokine_effects.glutamate_clearance_factor = fmaxf(0.0f, fminf(1.0f, 1.0f +
        il1 * CYTOKINE_IL1_GLUTAMATE_IMPAIRMENT +
        il6 * CYTOKINE_IL6_GLUTAMATE_IMPAIRMENT +
        tnf * CYTOKINE_TNF_GLUTAMATE_IMPAIRMENT +
        il10 * CYTOKINE_IL10_GLUTAMATE_RECOVERY
    ));

    /* Update reactivity flags */
    bridge->cytokine_effects.is_reactive =
        bridge->cytokine_effects.total_reactivity >= REACTIVE_ASTROCYTE_THRESHOLD;
    bridge->cytokine_effects.is_astrogliosis =
        bridge->cytokine_effects.total_reactivity >= ASTROGLIOSIS_THRESHOLD;

    /* Update statistics */
    if (bridge->cytokine_effects.is_reactive) {
        bridge->reactivity_events++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int astro_network_apply_inflammation_effects(astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system || !bridge->astrocyte_network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->enable_inflammation_astrogliosis) {
        return NIMCP_SUCCESS; /* Feature disabled */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current inflammation level */
    brain_inflammation_level_t level = get_inflammation_level(bridge->immune_system);
    bridge->inflammation_state.current_level = level;

    /* Map inflammation to reactivity */
    float reactivity_factor = 0.0f;
    switch (level) {
        case INFLAMMATION_NONE:
            reactivity_factor = INFLAMMATION_NONE_REACTIVITY_FACTOR;
            break;
        case INFLAMMATION_LOCAL:
            reactivity_factor = INFLAMMATION_LOCAL_REACTIVITY_FACTOR;
            break;
        case INFLAMMATION_REGIONAL:
            reactivity_factor = INFLAMMATION_REGIONAL_REACTIVITY_FACTOR;
            break;
        case INFLAMMATION_SYSTEMIC:
            reactivity_factor = INFLAMMATION_SYSTEMIC_REACTIVITY_FACTOR;
            break;
        case INFLAMMATION_STORM:
            reactivity_factor = INFLAMMATION_STORM_REACTIVITY_FACTOR;
            bridge->inflammation_state.glial_scar_forming = true;
            break;
    }

    bridge->inflammation_state.reactive_astrocyte_fraction = reactivity_factor;

    /* BBB permeability from inflammation */
    bridge->inflammation_state.bbb_permeability =
        INFLAMMATION_BBB_PERMEABILITY_BASE +
        (float)level * INFLAMMATION_BBB_PERMEABILITY_PER_LEVEL;

    /* Chronic inflammation check */
    if (level >= INFLAMMATION_REGIONAL) {
        bridge->chronic_inflammation_accumulator += 1.0f;
        bridge->inflammation_state.is_chronic =
            bridge->chronic_inflammation_accumulator >= CHRONIC_INFLAMMATION_UPDATE_THRESHOLD;
    } else {
        bridge->chronic_inflammation_accumulator *= NIMCP_EMA_WEIGHT_SLOW; /* Decay */
        if (bridge->chronic_inflammation_accumulator < 1.0f) {
            bridge->inflammation_state.is_chronic = false;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float astro_network_compute_reactivity(const astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) return 0.0f;

    /* Combine cytokine and inflammation reactivity */
    float cytokine_reactivity = bridge->cytokine_effects.total_reactivity;
    float inflammation_reactivity = bridge->inflammation_state.reactive_astrocyte_fraction;

    return fminf(1.0f, cytokine_reactivity + inflammation_reactivity);
}

float astro_network_compute_glutamate_clearance(const astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) return 1.0f;

    return bridge->cytokine_effects.glutamate_clearance_factor;
}

/* ============================================================================
 * Astrocyte → Immune Implementation
 * ============================================================================ */

int astro_network_release_cytokines_from_reactive(astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system || !bridge->astrocyte_network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->enable_reactive_cytokine_production) {
        return NIMCP_SUCCESS; /* Feature disabled */
    }

    float reactivity = astro_network_compute_reactivity(bridge);

    /* Only release if above threshold */
    if (reactivity < REACTIVE_ASTROCYTE_THRESHOLD) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Release pro-inflammatory cytokines */
    float release_strength = reactivity - REACTIVE_ASTROCYTE_THRESHOLD;
    uint32_t cytokine_id;

    /* IL-1β */
    brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL1,
        0, /* source_cell not tracked for astrocytes */
        release_strength * CYTOKINE_IL1B_RELEASE_RATIO,
        0, /* broadcast */
        &cytokine_id
    );

    /* IL-6 */
    brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL6,
        0,
        release_strength * CYTOKINE_IL6_RELEASE_RATIO,
        0,
        &cytokine_id
    );

    /* TNF-α if severe */
    if (bridge->cytokine_effects.is_astrogliosis) {
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_TNF,
            0,
            release_strength * CYTOKINE_TNFA_RELEASE_RATIO,
            0,
            &cytokine_id
        );
    }

    bridge->cytokine_releases++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int astro_network_trigger_calcium_immune_alert(astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system || !bridge->astrocyte_network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->enable_calcium_immune_signaling) {
        return NIMCP_SUCCESS; /* Feature disabled */
    }

    /* Check calcium levels in astrocyte network */
    float avg_calcium = 0.0f;
    float max_calcium = 0.0f;
    float avg_glutamate = 0.0f;

    astrocyte_network_get_stats(
        bridge->astrocyte_network,
        &avg_calcium,
        &max_calcium,
        &avg_glutamate
    );

    /* Trigger alert if calcium above threshold */
    if (max_calcium >= CALCIUM_WAVE_IMMUNE_THRESHOLD) {
        nimcp_mutex_lock(bridge->base.mutex);

        bridge->astrocyte_modulation.calcium_wave_immune_trigger = true;
        bridge->calcium_immune_triggers++;

        /* Broadcast immune alert if bio-async enabled */
        if (bridge->base.bio_async_enabled) {
            /* Alert severity based on calcium level */
            brain_inflammation_level_t severity = INFLAMMATION_LOCAL;
            if (max_calcium >= 5.0f) severity = INFLAMMATION_REGIONAL;
            if (max_calcium >= 8.0f) severity = INFLAMMATION_SYSTEMIC;

            /* Broadcast via immune system */
            brain_immune_broadcast_alert(
                bridge->immune_system,
                0, /* antigen_id not specific */
                severity
            );
        }

        nimcp_mutex_unlock(bridge->base.mutex);
    } else {
        bridge->astrocyte_modulation.calcium_wave_immune_trigger = false;
    }

    return NIMCP_SUCCESS;
}

int astro_network_modulate_bbb_permeability(astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->enable_bbb_inflammation_coupling) {
        return NIMCP_SUCCESS; /* Feature disabled */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* BBB permeability from reactivity */
    float reactivity = astro_network_compute_reactivity(bridge);
    float bbb_permeability = bridge->inflammation_state.bbb_permeability +
                             reactivity * 0.3f;

    bridge->cytokine_effects.bbb_permeability_increase =
        fminf(1.0f, bbb_permeability);

    /* Track disruption events */
    if (bbb_permeability >= 0.4f) {
        bridge->bbb_disruption_events++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int astro_network_release_il10_homeostatic(astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge || !bridge->immune_system || !bridge->astrocyte_network) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    float reactivity = astro_network_compute_reactivity(bridge);

    /* Only release IL-10 when in resting state */
    if (reactivity >= 0.2f) {
        return NIMCP_SUCCESS; /* Not resting */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Release anti-inflammatory IL-10 */
    uint32_t cytokine_id;
    brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL10,
        0,
        0.3f, /* moderate homeostatic release */
        0,    /* broadcast */
        &cytokine_id
    );

    bridge->astrocyte_modulation.homeostatic_il10_release = true;
    bridge->astrocyte_modulation.anti_inflammatory_signal = 0.3f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int astro_network_bridge_update(
    astro_network_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard: validate parameters */
    if (!bridge) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Apply immune → astrocyte effects */
    astro_network_apply_cytokine_effects(bridge);
    astro_network_apply_inflammation_effects(bridge);

    /* Apply astrocyte → immune effects */
    astro_network_release_cytokines_from_reactive(bridge);
    astro_network_trigger_calcium_immune_alert(bridge);
    astro_network_modulate_bbb_permeability(bridge);
    astro_network_release_il10_homeostatic(bridge);

    /* Update timing */
    bridge->last_update_time = nimcp_time_get_us();
    bridge->total_updates++;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int astro_network_get_cytokine_effects(
    const astro_network_bridge_t* bridge,
    cytokine_astro_network_effects_t* effects
) {
    /* Guard: validate parameters */
    if (!bridge || !effects) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_astro_network_effects_t));
    return NIMCP_SUCCESS;
}

int astro_network_get_inflammation_state(
    const astro_network_bridge_t* bridge,
    inflammation_astro_network_state_t* state
) {
    /* Guard: validate parameters */
    if (!bridge || !state) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_astro_network_state_t));
    return NIMCP_SUCCESS;
}

bool astro_network_has_astrogliosis(const astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) {
        return false;
    }

    return bridge->cytokine_effects.is_astrogliosis ||
           bridge->inflammation_state.glial_scar_forming;
}

float astro_network_get_reactivity_factor(const astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) return 0.0f;

    return astro_network_compute_reactivity(bridge);
}

float astro_network_get_bbb_permeability(const astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) return 0.0f;

    return bridge->cytokine_effects.bbb_permeability_increase;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

int astro_network_connect_bio_async(astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS; /* Already connected */
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_ASTROCYTE,
        .module_name = "astrocyte_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("astro_network_connect_bio_async: connected to bio-async router");
    } else {
        NIMCP_LOGGING_DEBUG("Bio-async router not available, skipping registration");
    }

    return NIMCP_SUCCESS;
}

int astro_network_disconnect_bio_async(astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS; /* Already disconnected */
    }

    /* Unregister from bio-async router */
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("astro_network_disconnect_bio_async: disconnected");
    return NIMCP_SUCCESS;
}

bool astro_network_is_bio_async_connected(const astro_network_bridge_t* bridge) {
    /* Guard: validate parameters */
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}
