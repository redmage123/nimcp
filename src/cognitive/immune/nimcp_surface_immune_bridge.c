/**
 * @file nimcp_surface_immune_bridge.c
 * @brief Surface Geometry Immune Integration Bridge Implementation
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "cognitive/immune/nimcp_surface_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(surface_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_surface_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_surface_immune_bridge_mesh_registry = NULL;

nimcp_error_t surface_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_surface_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "surface_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "surface_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_surface_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_surface_immune_bridge_mesh_registry = registry;
    return err;
}

void surface_immune_bridge_mesh_unregister(void) {
    if (g_surface_immune_bridge_mesh_registry && g_surface_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_surface_immune_bridge_mesh_registry, g_surface_immune_bridge_mesh_id);
        g_surface_immune_bridge_mesh_id = 0;
        g_surface_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from surface_immune_bridge module (instance-level) */
static inline void surface_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_surface_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_surface_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_surface_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "SURFACE_IMMUNE_BRIDGE"


//=============================================================================
// CONSTANTS
//=============================================================================

#define MODULE_NAME "surface_immune"
#define DEFAULT_MAX_ANTIGENS 128
#define DEFAULT_MAX_ANTIBODIES 64

//=============================================================================
// NAME TABLES
//=============================================================================

static const char* ANTIGEN_TYPE_NAMES[] = {
    "NONE",
    "INVALID_CHI",
    "IMPOSSIBLE_TRIFURCATION",
    "ANGLE_VIOLATION",
    "MATERIAL_OVERFLOW",
    "TOPOLOGY_ERROR",
    "RHO_OUT_OF_RANGE",
    "DEGENERATE_GEOMETRY",
    "DISCONNECTED_NETWORK"
};

static const char* SEVERITY_NAMES[] = {
    "INFO",
    "WARNING",
    "ERROR",
    "CRITICAL"
};

//=============================================================================
// CONFIGURATION
//=============================================================================

int surface_immune_default_config(surface_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_defau", 0.0f);


    memset(config, 0, sizeof(*config));

    /* Detection thresholds */
    config->chi_min = 0.0f;
    config->chi_max = 2.0f;
    config->angle_tolerance = 5.0f;          /* degrees */
    config->material_budget = 1e6f;          /* arbitrary units */
    config->rho_min = 0.0f;
    config->rho_max = 1.0f;

    /* Immune response */
    config->b_cell_activation_threshold = 3; /* 3 occurrences */
    config->antibody_production_threshold = 5;
    config->cytokine_release_severity = (float)SURFACE_SEVERITY_CRITICAL;

    /* Timing */
    config->antigen_persistence_ms = 30000;  /* 30 seconds */
    config->antibody_lifetime_ms = 300000;   /* 5 minutes */

    /* Limits */
    config->max_active_antigens = DEFAULT_MAX_ANTIGENS;
    config->max_antibodies = DEFAULT_MAX_ANTIBODIES;

    return 0;
}

//=============================================================================
// LIFECYCLE
//=============================================================================

surface_immune_bridge_t* surface_immune_bridge_create(
    const surface_immune_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_create", 0.0f);


    BRIDGE_CREATE_BEGIN(surface_immune_bridge_t, bridge,
                        BIO_MODULE_SURFACE_IMMUNE, MODULE_NAME);

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(surface_immune_config_t));
    } else {
        surface_immune_default_config(&bridge->config);
    }

    /* Allocate antigens */
    bridge->max_antigens = bridge->config.max_active_antigens;
    bridge->active_antigens = nimcp_malloc(
        bridge->max_antigens * sizeof(surface_antigen_t));
    if (!bridge->active_antigens) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_immune_bridge_create: bridge->active_antigens is NULL");
        return NULL;
    }
    memset(bridge->active_antigens, 0,
           bridge->max_antigens * sizeof(surface_antigen_t));
    bridge->num_active_antigens = 0;

    /* Allocate antibodies */
    bridge->max_antibodies = bridge->config.max_antibodies;
    bridge->antibodies = nimcp_malloc(
        bridge->max_antibodies * sizeof(surface_antibody_t));
    if (!bridge->antibodies) {
        nimcp_free(bridge->active_antigens);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_immune_bridge_create: bridge->antibodies is NULL");
        return NULL;
    }
    memset(bridge->antibodies, 0,
           bridge->max_antibodies * sizeof(surface_antibody_t));
    bridge->num_antibodies = 0;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(surface_immune_stats_t));

    /* ID counters */
    bridge->next_antigen_id = 1;
    bridge->next_antibody_id = 1;

    return bridge;
}

void surface_immune_bridge_destroy(surface_immune_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "surface_immune");

    /* Free antigens */
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_destroy", 0.0f);


    if (bridge->active_antigens) {
        nimcp_free(bridge->active_antigens);
    }

    /* Free antibodies */
    if (bridge->antibodies) {
        nimcp_free(bridge->antibodies);
    }

    BRIDGE_DESTROY(bridge);
}

int surface_immune_bridge_reset(surface_immune_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_reset", 0.0f);


    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    /* Clear antigens */
    memset(bridge->active_antigens, 0,
           bridge->max_antigens * sizeof(surface_antigen_t));
    bridge->num_active_antigens = 0;

    /* Clear antibodies */
    memset(bridge->antibodies, 0,
           bridge->max_antibodies * sizeof(surface_antibody_t));
    bridge->num_antibodies = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(surface_immune_stats_t));

    /* Reset ID counters */
    bridge->next_antigen_id = 1;
    bridge->next_antibody_id = 1;

    bridge_base_reset_unlocked(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

//=============================================================================
// CONNECTION
//=============================================================================

int surface_immune_bridge_connect_geometry(
    surface_immune_bridge_t* bridge,
    void* ctx
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_connect_geometry", 0.0f);


    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(ctx);

    /* Note: bridge_base_connect_a handles its own locking */
    bridge->geometry_ctx = ctx;
    return bridge_base_connect_a(&bridge->base, ctx);
}

int surface_immune_bridge_connect_immune(
    surface_immune_bridge_t* bridge,
    void* immune
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_connect_immune", 0.0f);


    BRIDGE_NULL_CHECK(bridge);

    /* Note: bridge_base_connect_b handles its own locking */
    bridge->immune_system = immune;
    return bridge_base_connect_b(&bridge->base, immune);
}

bool surface_immune_bridge_is_connected(const surface_immune_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_is_connected", 0.0f);


    BRIDGE_NULL_CHECK_BOOL(bridge);
    return bridge_base_is_connected(&bridge->base);
}

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

/* Forward declarations for internal unlocked functions */
static int produce_antibody_unlocked(
    surface_immune_bridge_t* bridge,
    surface_antigen_type_t target_type,
    uint32_t* antibody_id
);
static int activate_b_cell_unlocked(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id
);
static int release_cytokine_unlocked(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id,
    const char* message
);

//=============================================================================
// INTERNAL HELPERS
//=============================================================================

static surface_antigen_severity_t get_severity_for_type(
    surface_antigen_type_t type
) {
    switch (type) {
        case SURFACE_ANTIGEN_INVALID_CHI:
            return SURFACE_SEVERITY_ERROR;
        case SURFACE_ANTIGEN_IMPOSSIBLE_TRIFURCATION:
            return SURFACE_SEVERITY_ERROR;
        case SURFACE_ANTIGEN_ANGLE_VIOLATION:
            return SURFACE_SEVERITY_WARNING;
        case SURFACE_ANTIGEN_MATERIAL_OVERFLOW:
            return SURFACE_SEVERITY_CRITICAL;
        case SURFACE_ANTIGEN_TOPOLOGY_ERROR:
            return SURFACE_SEVERITY_CRITICAL;
        case SURFACE_ANTIGEN_RHO_OUT_OF_RANGE:
            return SURFACE_SEVERITY_WARNING;
        case SURFACE_ANTIGEN_DEGENERATE_GEOMETRY:
            return SURFACE_SEVERITY_ERROR;
        case SURFACE_ANTIGEN_DISCONNECTED_NETWORK:
            return SURFACE_SEVERITY_ERROR;
        default:
            return SURFACE_SEVERITY_INFO;
    }
}

static surface_antigen_t* find_antigen(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id
) {
    for (uint32_t i = 0; i < bridge->max_antigens; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_antigens > 256) {
            surface_immune_bridge_heartbeat("surface_immu_loop",
                             (float)(i + 1) / (float)bridge->max_antigens);
        }

        if (bridge->active_antigens[i].id == antigen_id &&
            bridge->active_antigens[i].active) {
            return &bridge->active_antigens[i];
        }
    }
    return NULL;
}

static surface_antigen_t* find_similar_antigen(
    surface_immune_bridge_t* bridge,
    surface_antigen_type_t type,
    uint32_t branch_point_id
) {
    for (uint32_t i = 0; i < bridge->max_antigens; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_antigens > 256) {
            surface_immune_bridge_heartbeat("surface_immu_loop",
                             (float)(i + 1) / (float)bridge->max_antigens);
        }

        surface_antigen_t* ag = &bridge->active_antigens[i];
        if (ag->active && ag->type == type &&
            ag->branch_point_id == branch_point_id) {
            return ag;
        }
    }
    return NULL;
}

static surface_antigen_t* find_empty_antigen_slot(
    surface_immune_bridge_t* bridge
) {
    for (uint32_t i = 0; i < bridge->max_antigens; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_antigens > 256) {
            surface_immune_bridge_heartbeat("surface_immu_loop",
                             (float)(i + 1) / (float)bridge->max_antigens);
        }

        if (!bridge->active_antigens[i].active) {
            return &bridge->active_antigens[i];
        }
    }
    return NULL;
}

static surface_antibody_t* find_antibody(
    surface_immune_bridge_t* bridge,
    uint32_t antibody_id
) {
    for (uint32_t i = 0; i < bridge->max_antibodies; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_antibodies > 256) {
            surface_immune_bridge_heartbeat("surface_immu_loop",
                             (float)(i + 1) / (float)bridge->max_antibodies);
        }

        if (bridge->antibodies[i].id == antibody_id &&
            bridge->antibodies[i].active) {
            return &bridge->antibodies[i];
        }
    }
    return NULL;
}

static surface_antibody_t* find_antibody_for_type(
    surface_immune_bridge_t* bridge,
    surface_antigen_type_t type
) {
    for (uint32_t i = 0; i < bridge->max_antibodies; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_antibodies > 256) {
            surface_immune_bridge_heartbeat("surface_immu_loop",
                             (float)(i + 1) / (float)bridge->max_antibodies);
        }

        if (bridge->antibodies[i].active &&
            bridge->antibodies[i].target_type == type) {
            return &bridge->antibodies[i];
        }
    }
    return NULL;
}

//=============================================================================
// VALIDATION
//=============================================================================

int surface_immune_validate_geometry(
    surface_immune_bridge_t* bridge,
    const surface_geometry_params_t* params,
    bool* is_valid,
    surface_antigen_type_t* violation_type
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_valid", 0.0f);


    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(params);
    BRIDGE_NULL_CHECK(is_valid);

    BRIDGE_LOCK(bridge);

    bridge->stats.total_validations++;

    *is_valid = true;
    if (violation_type) *violation_type = SURFACE_ANTIGEN_NONE;

    /* Check chi range */
    if (params->chi < bridge->config.chi_min ||
        params->chi > bridge->config.chi_max) {
        *is_valid = false;
        if (violation_type) *violation_type = SURFACE_ANTIGEN_INVALID_CHI;
        bridge->stats.anomalies_detected++;
        bridge->stats.antigen_counts[SURFACE_ANTIGEN_INVALID_CHI]++;
    }

    /* Check for impossible trifurcation */
    if (*is_valid && params->branch_type == SURFACE_BRANCH_TRIFURCATION &&
        params->chi < SURFACE_CHI_TRIFURCATION_THRESHOLD) {
        *is_valid = false;
        if (violation_type) *violation_type = SURFACE_ANTIGEN_IMPOSSIBLE_TRIFURCATION;
        bridge->stats.anomalies_detected++;
        bridge->stats.antigen_counts[SURFACE_ANTIGEN_IMPOSSIBLE_TRIFURCATION]++;
    }

    /* Check rho range */
    if (*is_valid && (params->rho < bridge->config.rho_min ||
                       params->rho > bridge->config.rho_max)) {
        *is_valid = false;
        if (violation_type) *violation_type = SURFACE_ANTIGEN_RHO_OUT_OF_RANGE;
        bridge->stats.anomalies_detected++;
        bridge->stats.antigen_counts[SURFACE_ANTIGEN_RHO_OUT_OF_RANGE]++;
    }

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int surface_immune_validate_branch(
    surface_immune_bridge_t* bridge,
    const surface_branch_point_t* branch,
    bool* is_valid,
    uint32_t* antigen_id
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_valid", 0.0f);


    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(branch);
    BRIDGE_NULL_CHECK(is_valid);

    if (antigen_id) *antigen_id = 0;

    BRIDGE_LOCK(bridge);

    bridge->stats.total_validations++;

    *is_valid = true;

    /* Check for degenerate geometry */
    if (branch->degree == 0 && !branch->is_terminal) {
        *is_valid = false;

        /* Present antigen */
        surface_antigen_t* ag = find_similar_antigen(
            bridge, SURFACE_ANTIGEN_DEGENERATE_GEOMETRY, branch->id);

        if (ag) {
            ag->occurrence_count++;
            if (antigen_id) *antigen_id = ag->id;
        } else {
            ag = find_empty_antigen_slot(bridge);
            if (ag) {
                ag->id = __atomic_add_fetch(&bridge->next_antigen_id, 1, __ATOMIC_SEQ_CST);
                ag->type = SURFACE_ANTIGEN_DEGENERATE_GEOMETRY;
                ag->severity = SURFACE_SEVERITY_ERROR;
                ag->branch_point_id = branch->id;
                ag->position[0] = branch->position.x;
                ag->position[1] = branch->position.y;
                ag->position[2] = branch->position.z;
                ag->expected_value = 1.0f;  /* At least degree 1 */
                ag->actual_value = 0.0f;
                ag->deviation = 1.0f;
                ag->timestamp_ms = nimcp_time_monotonic_ms();
                ag->active = true;
                ag->occurrence_count = 1;

                bridge->num_active_antigens++;
                if (antigen_id) *antigen_id = ag->id;
            }
        }

        bridge->stats.anomalies_detected++;
        bridge->stats.antigen_counts[SURFACE_ANTIGEN_DEGENERATE_GEOMETRY]++;
    }

    /* Check for topology errors (too high degree) */
    if (*is_valid && branch->degree > SURFACE_MAX_BRANCH_DEGREE) {
        *is_valid = false;

        surface_antigen_t* ag = find_empty_antigen_slot(bridge);
        if (ag) {
            ag->id = __atomic_add_fetch(&bridge->next_antigen_id, 1, __ATOMIC_SEQ_CST);
            ag->type = SURFACE_ANTIGEN_TOPOLOGY_ERROR;
            ag->severity = SURFACE_SEVERITY_CRITICAL;
            ag->branch_point_id = branch->id;
            ag->position[0] = branch->position.x;
            ag->position[1] = branch->position.y;
            ag->position[2] = branch->position.z;
            ag->expected_value = (float)SURFACE_MAX_BRANCH_DEGREE;
            ag->actual_value = (float)branch->degree;
            ag->deviation = (float)(branch->degree - SURFACE_MAX_BRANCH_DEGREE);
            ag->timestamp_ms = nimcp_time_monotonic_ms();
            ag->active = true;
            ag->occurrence_count = 1;

            bridge->num_active_antigens++;
            if (antigen_id) *antigen_id = ag->id;
        }

        bridge->stats.anomalies_detected++;
        bridge->stats.antigen_counts[SURFACE_ANTIGEN_TOPOLOGY_ERROR]++;
    }

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

//=============================================================================
// ANTIGEN MANAGEMENT
//=============================================================================

int surface_immune_present_anomaly(
    surface_immune_bridge_t* bridge,
    surface_antigen_type_t type,
    const surface_branch_point_t* branch,
    float expected,
    float actual,
    uint32_t* antigen_id
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_prese", 0.0f);


    BRIDGE_NULL_CHECK(bridge);

    if (antigen_id) *antigen_id = 0;
    if (type == SURFACE_ANTIGEN_NONE || type >= SURFACE_ANTIGEN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "surface_immune_present_anomaly: capacity exceeded");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    uint32_t branch_id = branch ? branch->id : 0;

    /* Check for existing similar antigen */
    surface_antigen_t* ag = find_similar_antigen(bridge, type, branch_id);

    if (ag) {
        /* Update existing antigen */
        ag->occurrence_count++;
        ag->actual_value = actual;
        ag->deviation = fabsf(actual - expected);

        /* Check if should activate B cell - use unlocked version since we hold lock */
        if (ag->occurrence_count >= bridge->config.b_cell_activation_threshold) {
            activate_b_cell_unlocked(bridge, ag->id);
        }

        if (antigen_id) *antigen_id = ag->id;
    } else {
        /* Create new antigen */
        ag = find_empty_antigen_slot(bridge);
        if (!ag) {
            bridge->stats.anomalies_detected++;  /* Count but can't track */
            BRIDGE_UNLOCK(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_immune_present_anomaly: ag is NULL");
            return -1;  /* No room */
        }

        ag->id = __atomic_add_fetch(&bridge->next_antigen_id, 1, __ATOMIC_SEQ_CST);
        ag->type = type;
        ag->severity = get_severity_for_type(type);
        ag->branch_point_id = branch_id;
        if (branch) {
            ag->position[0] = branch->position.x;
            ag->position[1] = branch->position.y;
            ag->position[2] = branch->position.z;
        }
        ag->expected_value = expected;
        ag->actual_value = actual;
        ag->deviation = fabsf(actual - expected);
        ag->timestamp_ms = nimcp_time_monotonic_ms();
        ag->active = true;
        ag->occurrence_count = 1;

        bridge->num_active_antigens++;
        if (antigen_id) *antigen_id = ag->id;
    }

    /* Update statistics */
    bridge->stats.anomalies_detected++;
    bridge->stats.antigen_counts[type]++;
    bridge->stats.active_antigens = bridge->num_active_antigens;

    /* Check for cytokine release on critical - use unlocked version since we hold lock */
    if (ag->severity >= (surface_antigen_severity_t)bridge->config.cytokine_release_severity) {
        release_cytokine_unlocked(bridge, ag->id, "Critical geometry anomaly");
    }

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int surface_immune_acknowledge_antigen(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_ackno", 0.0f);


    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    surface_antigen_t* ag = find_antigen(bridge, antigen_id);
    if (ag) {
        ag->acknowledged = true;
    }

    BRIDGE_UNLOCK(bridge);
    return ag ? 0 : -1;
}

int surface_immune_resolve_antigen(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_resol", 0.0f);


    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    surface_antigen_t* ag = find_antigen(bridge, antigen_id);
    if (ag) {
        ag->active = false;
        ag->duration_ms = nimcp_time_monotonic_ms() - ag->timestamp_ms;
        if (bridge->num_active_antigens > 0) {
            bridge->num_active_antigens--;
        }
        bridge->stats.anomalies_resolved++;
        bridge->stats.active_antigens = bridge->num_active_antigens;
    }

    BRIDGE_UNLOCK(bridge);
    return ag ? 0 : -1;
}

int surface_immune_get_active_antigens(
    const surface_immune_bridge_t* bridge,
    surface_antigen_t* antigens,
    uint32_t max_antigens,
    uint32_t* num_antigens
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_get_a", 0.0f);


    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(antigens);
    BRIDGE_NULL_CHECK(num_antigens);

    *num_antigens = 0;

    for (uint32_t i = 0; i < bridge->max_antigens && *num_antigens < max_antigens; i++) {
        if (bridge->active_antigens[i].active) {
            memcpy(&antigens[*num_antigens], &bridge->active_antigens[i],
                   sizeof(surface_antigen_t));
            (*num_antigens)++;
        }
    }

    return 0;
}

//=============================================================================
// ANTIBODY MANAGEMENT
//=============================================================================

/* Internal unlocked version - caller must hold lock */
static int produce_antibody_unlocked(
    surface_immune_bridge_t* bridge,
    surface_antigen_type_t target_type,
    uint32_t* antibody_id
) {
    if (antibody_id) *antibody_id = 0;

    /* Check if antibody already exists */
    surface_antibody_t* existing = find_antibody_for_type(bridge, target_type);
    if (existing) {
        /* Boost existing antibody */
        existing->affinity = fminf(existing->affinity + 0.1f, 1.0f);
        if (antibody_id) *antibody_id = existing->id;
        return 0;
    }

    /* Find empty slot */
    surface_antibody_t* ab = NULL;
    for (uint32_t i = 0; i < bridge->max_antibodies; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_antibodies > 256) {
            surface_immune_bridge_heartbeat("surface_immu_loop",
                             (float)(i + 1) / (float)bridge->max_antibodies);
        }

        if (!bridge->antibodies[i].active) {
            ab = &bridge->antibodies[i];
            break;
        }
    }

    if (!ab) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "produce_antibody_unlocked: ab is NULL");
        return -1;  /* No room */
    }

    /* Create new antibody */
    ab->id = __atomic_add_fetch(&bridge->next_antibody_id, 1, __ATOMIC_SEQ_CST);
    ab->target_type = target_type;
    ab->correction_factor = 0.5f;  /* Start conservative */
    ab->max_correction = 0.2f;
    ab->affinity = 0.5f;
    ab->created_ms = nimcp_time_monotonic_ms();
    ab->active = true;

    bridge->num_antibodies++;
    bridge->stats.antibodies_produced++;
    bridge->stats.active_antibodies = bridge->num_antibodies;

    if (antibody_id) *antibody_id = ab->id;

    return 0;
}

int surface_immune_produce_antibody(
    surface_immune_bridge_t* bridge,
    surface_antigen_type_t target_type,
    uint32_t* antibody_id
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_produ", 0.0f);


    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    int ret = produce_antibody_unlocked(bridge, target_type, antibody_id);
    BRIDGE_UNLOCK(bridge);
    return ret;
}

int surface_immune_apply_antibody(
    surface_immune_bridge_t* bridge,
    uint32_t antibody_id,
    surface_geometry_params_t* params,
    bool* success
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_apply", 0.0f);


    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(params);
    BRIDGE_NULL_CHECK(success);

    *success = false;

    BRIDGE_LOCK(bridge);

    surface_antibody_t* ab = find_antibody(bridge, antibody_id);
    if (!ab) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_immune_apply_antibody: ab is NULL");
        return -1;
    }

    bridge->stats.corrections_attempted++;

    /* Apply correction based on target type */
    float correction = ab->correction_factor * ab->affinity * ab->max_correction;

    switch (ab->target_type) {
        case SURFACE_ANTIGEN_INVALID_CHI:
            /* Clamp chi to valid range */
            if (params->chi < bridge->config.chi_min) {
                params->chi = bridge->config.chi_min + correction;
            } else if (params->chi > bridge->config.chi_max) {
                params->chi = bridge->config.chi_max - correction;
            }
            *success = true;
            break;

        case SURFACE_ANTIGEN_RHO_OUT_OF_RANGE:
            /* Clamp rho to valid range */
            if (params->rho < bridge->config.rho_min) {
                params->rho = bridge->config.rho_min + correction;
            } else if (params->rho > bridge->config.rho_max) {
                params->rho = bridge->config.rho_max - correction;
            }
            *success = true;
            break;

        case SURFACE_ANTIGEN_IMPOSSIBLE_TRIFURCATION:
            /* Change branch type if chi too low */
            if (params->chi < SURFACE_CHI_TRIFURCATION_THRESHOLD) {
                params->branch_type = SURFACE_BRANCH_BIFURCATION;
            }
            *success = true;
            break;

        default:
            /* Cannot correct other types */
            break;
    }

    if (*success) {
        ab->successful_corrections++;
        ab->affinity = fminf(ab->affinity + 0.05f, 1.0f);
        bridge->stats.corrections_successful++;
    } else {
        ab->failed_corrections++;
        ab->affinity = fmaxf(ab->affinity - 0.1f, 0.1f);
    }

    ab->last_used_ms = nimcp_time_monotonic_ms();

    /* Update success rate */
    if (bridge->stats.corrections_attempted > 0) {
        bridge->stats.correction_success_rate =
            (float)bridge->stats.corrections_successful /
            (float)bridge->stats.corrections_attempted;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

//=============================================================================
// IMMUNE RESPONSE
//=============================================================================

/* Internal unlocked version for cytokine release - caller must hold lock or
 * ensure thread safety */
static int release_cytokine_unlocked(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id,
    const char* message
) {
    (void)antigen_id;  /* Would be used in actual cytokine message */
    (void)message;     /* Would be used in actual cytokine message */

    /* In full implementation, this would send bio-async message to immune system */
    bridge->stats.cytokines_released++;

    return 0;
}

int surface_immune_release_cytokine(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id,
    const char* message
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_relea", 0.0f);


    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    int ret = release_cytokine_unlocked(bridge, antigen_id, message);
    BRIDGE_UNLOCK(bridge);
    return ret;
}

/* Internal unlocked version for B cell activation - caller must hold lock */
static int activate_b_cell_unlocked(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id
) {
    surface_antigen_t* ag = find_antigen(bridge, antigen_id);
    if (!ag) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ag is NULL");

        return -1;

    }

    /* In full implementation, this would activate a B cell in the immune system */
    bridge->stats.b_cells_activated++;

    /* Automatically produce antibody - use unlocked version since we hold lock */
    uint32_t ab_id = 0;
    produce_antibody_unlocked(bridge, ag->type, &ab_id);

    return 0;
}

int surface_immune_activate_b_cell(
    surface_immune_bridge_t* bridge,
    uint32_t antigen_id
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_activ", 0.0f);


    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    int ret = activate_b_cell_unlocked(bridge, antigen_id);
    BRIDGE_UNLOCK(bridge);
    return ret;
}

//=============================================================================
// STATISTICS
//=============================================================================

int surface_immune_get_stats(
    const surface_immune_bridge_t* bridge,
    surface_immune_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_get_s", 0.0f);


    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(stats);

    BRIDGE_LOCK(bridge);
    memcpy(stats, &bridge->stats, sizeof(*stats));
    BRIDGE_UNLOCK(bridge);
    return 0;
}

int surface_immune_reset_stats(surface_immune_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    surface_immune_bridge_heartbeat("surface_immu_surface_immune_reset", 0.0f);


    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    /* Preserve active counts */
    uint32_t active_ag = bridge->stats.active_antigens;
    uint32_t active_ab = bridge->stats.active_antibodies;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->stats.active_antigens = active_ag;
    bridge->stats.active_antibodies = active_ab;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

//=============================================================================
// UTILITY
//=============================================================================

const char* surface_antigen_type_name(surface_antigen_type_t type) {
    if (type < SURFACE_ANTIGEN_COUNT) {
        return ANTIGEN_TYPE_NAMES[type];
    }
    return "UNKNOWN";
}

const char* surface_antigen_severity_name(surface_antigen_severity_t severity) {
    if (severity <= SURFACE_SEVERITY_CRITICAL) {
        return SEVERITY_NAMES[severity];
    }
    return "UNKNOWN";
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void surface_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_surface_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int surface_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surface_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    surface_immune_bridge_heartbeat_instance(NULL, "surface_immune_bridge_training_begin", 0.0f);
    return 0;
}

int surface_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surface_immune_bridge_training_end: NULL argument");
        return -1;
    }
    surface_immune_bridge_heartbeat_instance(NULL, "surface_immune_bridge_training_end", 1.0f);
    return 0;
}

int surface_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surface_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    surface_immune_bridge_heartbeat_instance(NULL, "surface_immune_bridge_training_step", progress);
    return 0;
}
