/**
 * @file nimcp_surface_geometry_bridge.c
 * @brief Surface Geometry Brain Integration Bridge Implementation
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "core/brain/bridges/nimcp_surface_geometry_bridge.h"
#include "core/geometry/nimcp_surface_geometry.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(surface_geometry_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "SURFACE_GEOMETRY_BRIDGE"


//=============================================================================
// CONSTANTS
//=============================================================================

/** Maximum message subscriptions */
#define MAX_SUBSCRIPTIONS 32

/** Module name for logging */
static const char* MODULE_NAME = "surface_geometry_bridge";

//=============================================================================
// MESSAGE TYPE NAMES
//=============================================================================

static const char* MSG_TYPE_NAMES[] = {
    "GEOMETRY_UPDATE",
    "BRANCH_FORMED",
    "TRIFURCATION_DETECTED",
    "SPROUT_FORMED",
    "SYNAPSE_SPROUT",
    "OPTIMIZATION_COMPLETE",
    "ANOMALY_DETECTED",
    "MATERIAL_BUDGET_UPDATE",
    "REQUEST_GEOMETRY",
    "MODULATE_PARAMS"
};

//=============================================================================
// CONFIGURATION
//=============================================================================

int surface_geometry_bridge_default_config(surface_geometry_bridge_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(*config));

    /* Base configuration */
    config->enable_modulation = true;
    config->sensitivity = 1.0f;

    /* Paper-derived thresholds */
    config->chi_trifurcation_threshold = SURFACE_CHI_TRIFURCATION_THRESHOLD;
    config->rho_threshold = SURFACE_RHO_THRESHOLD_DEFAULT;
    config->angle_tolerance = 5.0f;  /* degrees */

    /* Bio-async */
    config->enable_bio_async = true;
    config->update_interval_ms = 100;

    /* Optimization defaults */
    config->default_method = SURFACE_OPT_GRADIENT_DESCENT;
    config->max_optimization_iterations = 1000;

    return 0;
}

//=============================================================================
// LIFECYCLE
//=============================================================================

surface_geometry_bridge_t* surface_geometry_bridge_create(
    const surface_geometry_bridge_config_t* config
) {
    /* Use bridge base macro for allocation and initialization */
    BRIDGE_CREATE_BEGIN(surface_geometry_bridge_t, bridge,
                        BIO_MODULE_SURFACE_GEOMETRY_BRAIN, "surface_geometry_bridge");

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(surface_geometry_bridge_config_t));
    } else {
        surface_geometry_bridge_default_config(&bridge->config);
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(surface_geometry_bridge_stats_t));

    /* Allocate subscription array */
    bridge->max_subscriptions = MAX_SUBSCRIPTIONS;
    bridge->message_subscriptions = nimcp_malloc(
        bridge->max_subscriptions * sizeof(uint32_t));
    if (!bridge->message_subscriptions) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_geometry_bridge_create: bridge->message_subscriptions is NULL");
        return NULL;
    }
    memset(bridge->message_subscriptions, 0,
           bridge->max_subscriptions * sizeof(uint32_t));
    bridge->num_subscriptions = 0;

    /* Create spine cache for lazy geometry computation */
    bridge->spine_cache = surface_spine_cache_create(SURFACE_SPINE_CACHE_DEFAULT_CAPACITY);
    /* Cache creation failure is non-fatal - continue without caching */

    return bridge;
}

void surface_geometry_bridge_destroy(surface_geometry_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "surface_geometry");

    /* Disconnect systems */
    surface_geometry_bridge_disconnect_geometry(bridge);
    surface_geometry_bridge_disconnect_brain(bridge);

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        surface_geometry_bridge_disconnect_bio_async(bridge);
    }

    /* Free spine cache */
    if (bridge->spine_cache) {
        surface_spine_cache_destroy(bridge->spine_cache);
    }

    /* Free subscriptions */
    if (bridge->message_subscriptions) {
        nimcp_free(bridge->message_subscriptions);
    }

    /* Use bridge base cleanup and free */
    BRIDGE_DESTROY(bridge);
}

int surface_geometry_bridge_reset(surface_geometry_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    /* Reset base statistics */
    bridge_base_reset_unlocked(&bridge->base);

    /* Reset bridge-specific statistics */
    memset(&bridge->stats, 0, sizeof(surface_geometry_bridge_stats_t));

    /* Clear spine cache */
    if (bridge->spine_cache) {
        surface_spine_cache_clear(bridge->spine_cache);
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

//=============================================================================
// CONNECTION FUNCTIONS
//=============================================================================

int surface_geometry_bridge_connect_geometry(
    surface_geometry_bridge_t* bridge,
    surface_geometry_ctx_t* ctx
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(ctx);

    /* Note: bridge_base_connect_a handles its own locking */
    bridge->geometry_ctx = ctx;
    return bridge_base_connect_a(&bridge->base, ctx);
}

int surface_geometry_bridge_connect_brain(
    surface_geometry_bridge_t* bridge,
    void* brain_region
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(brain_region);

    /* Note: bridge_base_connect_b handles its own locking */
    return bridge_base_connect_b(&bridge->base, brain_region);
}

int surface_geometry_bridge_disconnect_geometry(surface_geometry_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    /* Note: bridge_base_disconnect_a handles its own locking */
    bridge->geometry_ctx = NULL;
    return bridge_base_disconnect_a(&bridge->base);
}

int surface_geometry_bridge_disconnect_brain(surface_geometry_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    /* Note: bridge_base_disconnect_b handles its own locking */
    return bridge_base_disconnect_b(&bridge->base);
}

bool surface_geometry_bridge_is_connected(const surface_geometry_bridge_t* bridge) {
    BRIDGE_NULL_CHECK_BOOL(bridge);
    return bridge_base_is_connected(&bridge->base);
}

//=============================================================================
// BIO-ASYNC FUNCTIONS
//=============================================================================

/* Use macro to define standard bio-async functions */
BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE(surface_geometry_bridge, surface_geometry_bridge_t)

int surface_geometry_bridge_subscribe(
    surface_geometry_bridge_t* bridge,
    surface_bio_msg_type_t msg_type
) {
    BRIDGE_NULL_CHECK(bridge);

    if (msg_type >= SURFACE_BIO_MSG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "surface_geometry_bridge_subscribe: capacity exceeded");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Check if already subscribed */
    for (uint32_t i = 0; i < bridge->num_subscriptions; i++) {
        if (bridge->message_subscriptions[i] == (uint32_t)msg_type) {
            BRIDGE_UNLOCK(bridge);
            return 0;  /* Already subscribed */
        }
    }

    /* Add subscription */
    if (bridge->num_subscriptions >= bridge->max_subscriptions) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "surface_geometry_bridge_subscribe: capacity exceeded");
        return -1;  /* No room */
    }

    bridge->message_subscriptions[bridge->num_subscriptions++] = (uint32_t)msg_type;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int surface_geometry_bridge_unsubscribe(
    surface_geometry_bridge_t* bridge,
    surface_bio_msg_type_t msg_type
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    /* Find and remove subscription */
    for (uint32_t i = 0; i < bridge->num_subscriptions; i++) {
        if (bridge->message_subscriptions[i] == (uint32_t)msg_type) {
            /* Shift remaining subscriptions */
            for (uint32_t j = i; j < bridge->num_subscriptions - 1; j++) {
                bridge->message_subscriptions[j] = bridge->message_subscriptions[j + 1];
            }
            bridge->num_subscriptions--;
            BRIDGE_UNLOCK(bridge);
            return 0;
        }
    }

    BRIDGE_UNLOCK(bridge);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_geometry_bridge_unsubscribe: operation failed");
    return -1;  /* Not found */
}

//=============================================================================
// GEOMETRY COMPUTATION
//=============================================================================

int surface_geometry_bridge_compute_branch(
    surface_geometry_bridge_t* bridge,
    const surface_branch_point_t* branch,
    surface_geometry_params_t* params
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(branch);
    BRIDGE_NULL_CHECK(params);

    if (!bridge->geometry_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_geometry_bridge_compute_branch: bridge->geometry_ctx is NULL");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    int result = surface_compute_branch_params(bridge->geometry_ctx, branch, params);

    if (result == 0) {
        bridge->stats.total_geometry_computations++;

        /* Update branch type statistics */
        switch (params->branch_type) {
            case SURFACE_BRANCH_BIFURCATION:
                bridge->stats.bifurcations_detected++;
                break;
            case SURFACE_BRANCH_TRIFURCATION:
                bridge->stats.trifurcations_detected++;
                break;
            case SURFACE_BRANCH_HIGHER:
                bridge->stats.higher_order_detected++;
                break;
            default:
                break;
        }

        /* Record update */
        bridge_base_record_update(&bridge->base);
    }

    BRIDGE_UNLOCK(bridge);
    return result;
}

int surface_geometry_bridge_compute_spine(
    surface_geometry_bridge_t* bridge,
    float parent_diameter,
    float spine_diameter,
    const surface_vec3_t* spine_position,
    spine_surface_geometry_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    if (!bridge->geometry_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_geometry_bridge_compute_spine: bridge->geometry_ctx is NULL");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    int ret = surface_compute_spine_geometry(
        bridge->geometry_ctx,
        parent_diameter,
        spine_diameter,
        spine_position,
        result
    );

    if (ret == 0) {
        bridge->stats.total_geometry_computations++;

        /* Track sprout statistics */
        if (result->is_sprout) {
            bridge->stats.sprouts_formed++;
            if (result->ends_at_synapse) {
                bridge->stats.synapse_sprouts++;
            }

            /* Update ratio */
            if (bridge->stats.sprouts_formed > 0) {
                bridge->stats.sprout_synapse_ratio =
                    (float)bridge->stats.synapse_sprouts /
                    (float)bridge->stats.sprouts_formed;
            }
        }

        bridge_base_record_update(&bridge->base);
    }

    BRIDGE_UNLOCK(bridge);
    return ret;
}

int surface_geometry_bridge_compute_axon_branch(
    surface_geometry_bridge_t* bridge,
    float parent_diameter,
    const float* child_diameters,
    uint32_t num_children,
    axon_branch_surface_geometry_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(child_diameters);
    BRIDGE_NULL_CHECK(result);

    if (!bridge->geometry_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_geometry_bridge_compute_axon_branch: bridge->geometry_ctx is NULL");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    int ret = surface_compute_axon_branch_geometry(
        bridge->geometry_ctx,
        parent_diameter,
        child_diameters,
        num_children,
        result
    );

    if (ret == 0) {
        bridge->stats.total_geometry_computations++;
        bridge_base_record_update(&bridge->base);
    }

    BRIDGE_UNLOCK(bridge);
    return ret;
}

//=============================================================================
// OPTIMIZATION
//=============================================================================

int surface_geometry_bridge_optimize(
    surface_geometry_bridge_t* bridge,
    const float (*terminals)[3],
    uint32_t num_terminals,
    float min_circumference,
    surface_optimization_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(terminals);
    BRIDGE_NULL_CHECK(result);

    if (!bridge->geometry_ctx || num_terminals < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_geometry_bridge_optimize: bridge->geometry_ctx is NULL");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Record start time for performance tracking */
    uint64_t start_time = bridge->base.last_update_time_ms;

    int ret = surface_optimize_network(
        bridge->geometry_ctx,
        terminals,
        num_terminals,
        min_circumference,
        result
    );

    if (ret == 0) {
        bridge->stats.total_optimizations++;

        /* Update performance metrics */
        uint64_t end_time = bridge->base.last_update_time_ms;
        float elapsed = (float)(end_time - start_time);

        /* Running average */
        bridge->stats.avg_optimization_time_ms =
            (bridge->stats.avg_optimization_time_ms *
             (bridge->stats.total_optimizations - 1) + elapsed) /
            bridge->stats.total_optimizations;

        bridge_base_record_update(&bridge->base);

        /* Broadcast completion if bio-async enabled */
        if (bridge->base.bio_async_enabled) {
            /* Message broadcast would go here */
            bridge->stats.messages_sent++;
        }
    }

    BRIDGE_UNLOCK(bridge);
    return ret;
}

int surface_geometry_bridge_validate(
    surface_geometry_bridge_t* bridge,
    const surface_geometry_params_t* params,
    surface_validation_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(params);
    BRIDGE_NULL_CHECK(result);

    if (!bridge->geometry_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_geometry_bridge_validate: bridge->geometry_ctx is NULL");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    int ret = surface_validate_geometry(bridge->geometry_ctx, params, result);

    if (ret == 0) {
        bridge->stats.total_validations++;

        /* Track anomalies */
        if (result->status != SURFACE_VALIDATION_VALID) {
            bridge->stats.anomalies_detected++;

            /* Broadcast anomaly if bio-async enabled */
            if (bridge->base.bio_async_enabled) {
                bridge->stats.messages_sent++;
            }
        }

        bridge_base_record_update(&bridge->base);
    }

    BRIDGE_UNLOCK(bridge);
    return ret;
}

//=============================================================================
// MESSAGING
//=============================================================================

int surface_geometry_bridge_broadcast_update(
    surface_geometry_bridge_t* bridge,
    const surface_geometry_params_t* params
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(params);

    if (!bridge->base.bio_async_enabled) {
        return 0;  /* Silently succeed if bio-async disabled */
    }

    BRIDGE_LOCK(bridge);

    /* In a full implementation, this would send a bio-async message */
    /* For now, just track the send */
    bridge->stats.messages_sent++;
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int surface_geometry_bridge_broadcast_branch(
    surface_geometry_bridge_t* bridge,
    surface_branch_type_t branch_type,
    const float position[3]
) {
    BRIDGE_NULL_CHECK(bridge);
    (void)position;  /* Used in actual message */

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    BRIDGE_LOCK(bridge);

    /* Message type depends on branch type */
    surface_bio_msg_type_t msg_type = SURFACE_BIO_MSG_BRANCH_FORMED;
    if (branch_type == SURFACE_BRANCH_TRIFURCATION) {
        msg_type = SURFACE_BIO_MSG_TRIFURCATION_DETECTED;
    }
    (void)msg_type;  /* Would be used in actual message */

    bridge->stats.messages_sent++;
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int surface_geometry_bridge_broadcast_anomaly(
    surface_geometry_bridge_t* bridge,
    surface_error_t error_code,
    const surface_branch_point_t* branch
) {
    BRIDGE_NULL_CHECK(bridge);
    (void)error_code;  /* Used in actual message */
    (void)branch;      /* Used in actual message */

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    BRIDGE_LOCK(bridge);

    bridge->stats.messages_sent++;
    bridge->stats.anomalies_detected++;
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

//=============================================================================
// STATISTICS
//=============================================================================

int surface_geometry_bridge_get_stats(
    const surface_geometry_bridge_t* bridge,
    surface_geometry_bridge_stats_t* stats
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(stats);

    /* Note: Not locking for read-only stats access */
    memcpy(stats, &bridge->stats, sizeof(surface_geometry_bridge_stats_t));

    return 0;
}

int surface_geometry_bridge_reset_stats(surface_geometry_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(surface_geometry_bridge_stats_t));
    BRIDGE_UNLOCK(bridge);

    return 0;
}

//=============================================================================
// CONFIGURATION
//=============================================================================

int surface_geometry_bridge_get_config(
    const surface_geometry_bridge_t* bridge,
    surface_geometry_bridge_config_t* config
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(config);

    memcpy(config, &bridge->config, sizeof(surface_geometry_bridge_config_t));
    return 0;
}

int surface_geometry_bridge_set_config(
    surface_geometry_bridge_t* bridge,
    const surface_geometry_bridge_config_t* config
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(config);

    BRIDGE_LOCK(bridge);

    memcpy(&bridge->config, config, sizeof(surface_geometry_bridge_config_t));

    /* Update geometry context if connected */
    if (bridge->geometry_ctx) {
        surface_geometry_config_t geo_config;
        if (surface_geometry_get_config(bridge->geometry_ctx, &geo_config) == 0) {
            geo_config.chi_trifurcation_threshold = config->chi_trifurcation_threshold;
            geo_config.rho_threshold = config->rho_threshold;
            surface_geometry_set_config(bridge->geometry_ctx, &geo_config);
        }
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

//=============================================================================
// UTILITY
//=============================================================================

const char* surface_bio_msg_type_name(surface_bio_msg_type_t msg_type) {
    if (msg_type < SURFACE_BIO_MSG_COUNT) {
        return MSG_TYPE_NAMES[msg_type];
    }
    return "UNKNOWN";
}
