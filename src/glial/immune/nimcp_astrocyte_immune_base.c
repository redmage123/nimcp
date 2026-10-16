/**
 * @file nimcp_astrocyte_immune_base.c
 * @brief Abstract Base Implementation for Astrocyte-Immune Bridges
 * @version 2.0.0
 * @date 2025-12-27
 *
 * WHAT: Common implementation for polymorphic astrocyte-immune operations
 * WHY:  Avoid code duplication across derived bridge types
 * HOW:  Dispatch through vtable, provide shared utility functions
 */

#include "glial/immune/nimcp_astrocyte_immune_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(astrocyte_immune_base)

/* ============================================================================
 * Polymorphic API Implementation (Virtual Dispatch)
 * ============================================================================ */

void astro_immune_destroy(astrocyte_immune_base_t* bridge) {
    if (!bridge) return;
    if (!bridge->ops || !bridge->ops->destroy) {
        NIMCP_LOGGING_ERROR("astro_immune_destroy: no destroy function in vtable");
        return;
    }
    bridge->ops->destroy(bridge);
}

int astro_immune_update(astrocyte_immune_base_t* bridge, uint64_t delta_ms) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->ops && bridge->ops->update, NIMCP_ERROR_NOT_IMPLEMENTED, "update function not implemented");
    return bridge->ops->update(bridge, delta_ms);
}

int astro_immune_apply_cytokines(astrocyte_immune_base_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->ops && bridge->ops->apply_cytokine_effects, NIMCP_ERROR_NOT_IMPLEMENTED, "apply_cytokine_effects not implemented");
    return bridge->ops->apply_cytokine_effects(bridge);
}

int astro_immune_apply_inflammation(astrocyte_immune_base_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->ops && bridge->ops->apply_inflammation_effects, NIMCP_ERROR_NOT_IMPLEMENTED, "apply_inflammation_effects not implemented");
    return bridge->ops->apply_inflammation_effects(bridge);
}

float astro_immune_get_reactivity(const astrocyte_immune_base_t* bridge) {
    if (!bridge) return 0.0f;
    if (!bridge->ops || !bridge->ops->compute_reactivity) {
        return bridge->cytokine_state.total_reactivity;
    }
    return bridge->ops->compute_reactivity(bridge);
}

float astro_immune_get_glutamate_clearance(const astrocyte_immune_base_t* bridge) {
    if (!bridge) return 1.0f;
    if (!bridge->ops || !bridge->ops->compute_glutamate_clearance) {
        return bridge->cytokine_state.glutamate_clearance;
    }
    return bridge->ops->compute_glutamate_clearance(bridge);
}

astrocyte_phenotype_t astro_immune_get_phenotype(const astrocyte_immune_base_t* bridge) {
    if (!bridge) return ASTROCYTE_PHENOTYPE_RESTING;
    if (!bridge->ops || !bridge->ops->get_phenotype) {
        /* Default based on reactivity */
        if (bridge->cytokine_state.is_astrogliosis) return ASTROCYTE_PHENOTYPE_A1_REACTIVE;
        if (bridge->cytokine_state.is_reactive) return ASTROCYTE_PHENOTYPE_A1_REACTIVE;
        return ASTROCYTE_PHENOTYPE_RESTING;
    }
    return bridge->ops->get_phenotype(bridge);
}

bool astro_immune_has_astrogliosis(const astrocyte_immune_base_t* bridge) {
    if (!bridge) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "astro_immune_has_astrogliosis: bridge is NULL");

            return false;

        }
    return bridge->cytokine_state.is_astrogliosis ||
           bridge->inflammation.glial_scar_forming;
}

astrocyte_immune_type_t astro_immune_get_type(const astrocyte_immune_base_t* bridge) {
    if (!bridge) return ASTRO_IMMUNE_TYPE_PLASTICITY;
    return bridge->type;
}

int astro_immune_get_cytokine_state(
    const astrocyte_immune_base_t* bridge,
    astro_cytokine_state_t* out_state
) {
    NIMCP_CHECK_THROW(bridge && out_state, NIMCP_ERROR_NULL_POINTER, "bridge or out_state is NULL");

    nimcp_platform_mutex_lock(bridge->infra.mutex);
    memcpy(out_state, &bridge->cytokine_state, sizeof(astro_cytokine_state_t));
    nimcp_platform_mutex_unlock(bridge->infra.mutex);

    return 0;
}

int astro_immune_get_inflammation_state(
    const astrocyte_immune_base_t* bridge,
    astro_inflammation_state_t* out_state
) {
    NIMCP_CHECK_THROW(bridge && out_state, NIMCP_ERROR_NULL_POINTER, "bridge or out_state is NULL");

    nimcp_platform_mutex_lock(bridge->infra.mutex);
    memcpy(out_state, &bridge->inflammation, sizeof(astro_inflammation_state_t));
    nimcp_platform_mutex_unlock(bridge->infra.mutex);

    return 0;
}

int astro_immune_get_stats(
    const astrocyte_immune_base_t* bridge,
    astro_immune_stats_t* out_stats
) {
    NIMCP_CHECK_THROW(bridge && out_stats, NIMCP_ERROR_NULL_POINTER, "bridge or out_stats is NULL");

    nimcp_platform_mutex_lock(bridge->infra.mutex);
    memcpy(out_stats, &bridge->stats, sizeof(astro_immune_stats_t));
    nimcp_platform_mutex_unlock(bridge->infra.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration (Common Implementation)
 * ============================================================================ */

#define ASTRO_IMMUNE_MODULE_NAME "astro_immune_bridge"

int astro_immune_connect_bio_async(astrocyte_immune_base_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->infra.bio_async_enabled) return NIMCP_SUCCESS;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_ASTROCYTE,
        .module_name = ASTRO_IMMUNE_MODULE_NAME,
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->infra.bio_ctx = bio_router_register_module(&info);
    if (bridge->infra.bio_ctx) {
        bridge->infra.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Astrocyte-immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_DEBUG("Bio-async router not available, skipping registration");
    }

    return 0;
}

int astro_immune_disconnect_bio_async(astrocyte_immune_base_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->infra.bio_async_enabled) return NIMCP_SUCCESS;

    if (bridge->infra.bio_ctx) {
        bio_router_unregister_module(bridge->infra.bio_ctx);
        bridge->infra.bio_ctx = NULL;
    }
    bridge->infra.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("Astrocyte-immune bridge disconnected from bio-async");
    return 0;
}

bool astro_immune_is_bio_async_connected(const astrocyte_immune_base_t* bridge) {
    if (!bridge) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "astro_immune_is_bio_async_connected: bridge is NULL");

            return false;

        }
    return bridge->infra.bio_async_enabled;
}

/* ============================================================================
 * Base Initialization (for derived types)
 * ============================================================================ */

int astro_immune_base_init(
    astrocyte_immune_base_t* base,
    astrocyte_immune_type_t type,
    const astrocyte_immune_ops_t* ops,
    brain_immune_system_t* immune_system
) {
    if (!base || !ops) {
        NIMCP_LOGGING_ERROR("astro_immune_base_init: NULL base or ops");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    memset(base, 0, sizeof(astrocyte_immune_base_t));

    /* Set type and vtable */
    base->type = type;
    base->ops = ops;

    /* Link immune system */
    base->immune_system = immune_system;

    /* Create mutex */
    base->infra.mutex = nimcp_platform_mutex_create();
    if (!base->infra.mutex) {
        NIMCP_LOGGING_ERROR("astro_immune_base_init: mutex creation failed");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NO_MEMORY, "memory allocation failed");
    }

    /* Initialize timing */
    base->last_update_us = nimcp_time_get_us();
    base->chronic_accumulator = 0.0f;

    /* Enable all features by default */
    base->enable_cytokine_effects = true;
    base->enable_inflammation_effects = true;
    base->enable_reactive_cytokines = true;

    /* Initialize state defaults */
    base->cytokine_state.glutamate_clearance = 1.0f;
    base->cytokine_state.d_serine_modulation = 1.0f;

    NIMCP_LOGGING_DEBUG("astro_immune_base_init: initialized type %d", type);
    return 0;
}

void astro_immune_base_cleanup(astrocyte_immune_base_t* base) {
    if (!base) return;

    /* Disconnect bio-async */
    if (base->infra.bio_async_enabled) {
        astro_immune_disconnect_bio_async(base);
    }

    /* Destroy mutex */
    if (base->infra.mutex) {
        nimcp_platform_mutex_destroy(base->infra.mutex);
        nimcp_free(base->infra.mutex);
        base->infra.mutex = NULL;
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* astrocyte_phenotype_to_string(astrocyte_phenotype_t phenotype) {
    switch (phenotype) {
        case ASTROCYTE_PHENOTYPE_RESTING:     return "RESTING";
        case ASTROCYTE_PHENOTYPE_A1_REACTIVE: return "A1_REACTIVE";
        case ASTROCYTE_PHENOTYPE_A2_REACTIVE: return "A2_REACTIVE";
        case ASTROCYTE_PHENOTYPE_SCAR_FORMING: return "SCAR_FORMING";
        default: return "UNKNOWN";
    }
}
