/**
 * @file nimcp_mesh_cycle_coordinator.c
 * @brief Mesh-Brain Cycle Coordinator Integration Implementation
 *
 * WHAT: Bridges brain cycle coordinator with mesh network for unified timing
 * WHY:  Enforce brain cycle timing constraints on mesh transactions
 * HOW:  Maps cycle types to mesh timing, stall detection triggers recovery
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_cycle_coordinator.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "mesh/nimcp_mesh_resilience_integration.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_exception_bridge.h"
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal mesh-cycle coordinator integration structure
 */
struct mesh_cycle_coordinator_integration {
    uint32_t magic;                             /**< Magic number for validation */
    mesh_cycle_coordinator_config_t config;     /**< Configuration */

    /* Dependencies */
    mesh_bootstrap_t* bootstrap;                /**< Mesh bootstrap handle */
    brain_cycle_coordinator_t* cycle_coord;     /**< Brain cycle coordinator */

    /* Connected subsystems */
    mesh_ordering_service_t* ordering;          /**< Ordering service */
    mesh_resilience_integration_t* resilience;  /**< Resilience integration */
    mesh_health_bridge_t* health_bridge;        /**< Health bridge */
    mesh_exception_bridge_t* exception_bridge;  /**< Exception bridge */
    mesh_msp_t* msp;                            /**< MSP for BBB validation */

    /* Per-cycle timing constraints */
    mesh_cycle_timing_constraint_t timing_constraints[BRAIN_CYCLE_COUNT];

    /* Stall tracking per cycle */
    uint32_t consecutive_stalls[BRAIN_CYCLE_COUNT];
    uint64_t last_stall_ns[BRAIN_CYCLE_COUNT];
    uint64_t total_stalls[BRAIN_CYCLE_COUNT];
    mesh_recovery_action_type_t last_recovery_action[BRAIN_CYCLE_COUNT];
    bool recovery_in_progress[BRAIN_CYCLE_COUNT];
    uint32_t recovery_attempts[BRAIN_CYCLE_COUNT];
    uint64_t recovery_started_ns[BRAIN_CYCLE_COUNT];
    bool last_recovery_succeeded[BRAIN_CYCLE_COUNT];

    /* Callbacks */
    mesh_cycle_stall_callback_t stall_callback;
    void* stall_callback_data;
    mesh_cycle_recovery_callback_t recovery_callback;
    void* recovery_callback_data;
    mesh_cycle_timing_callback_t timing_callback;
    void* timing_callback_data;

    /* Statistics */
    mesh_cycle_coordinator_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Logger */
    nimcp_logger_t logger;
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void initialize_timing_constraints(mesh_cycle_coordinator_integration_t* integration);
static mesh_recovery_action_type_t map_stalls_to_recovery_action(uint32_t consecutive_stalls);
static void route_stall_to_exception_bridge(
    mesh_cycle_coordinator_integration_t* integration,
    brain_cycle_type_t cycle_type,
    uint64_t stall_duration_us
);
static float compute_cycle_health(
    const mesh_cycle_coordinator_integration_t* integration,
    brain_cycle_type_t cycle_type
);

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_cycle_coordinator_integration_default_config(
    mesh_cycle_coordinator_config_t* config
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_cycle_coordinator: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(*config));

    /* Feature enables */
    config->enable_timing_constraints = true;
    config->enable_stall_recovery = true;
    config->enable_distributed_health = true;
    config->enable_cross_channel_sync = false;

    /* Stall recovery settings */
    config->stall_recovery_threshold = MESH_CYCLE_DEFAULT_STALL_RECOVERY_THRESHOLD;

    /* Timing settings */
    config->timing_batch_multiplier = MESH_CYCLE_DEFAULT_TIMING_BATCH_MULTIPLIER;

    /* Health consensus settings */
    config->health_endorsement_weight = MESH_CYCLE_DEFAULT_HEALTH_ENDORSEMENT_WEIGHT;

    /* Logging */
    config->verbose_logging = false;
    config->enable_debug_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Timing Constraint Initialization
 * ============================================================================ */

/**
 * @brief Get default interval for a cycle type in microseconds
 */
static uint64_t get_cycle_interval_us(brain_cycle_type_t type) {
    switch (type) {
        case BRAIN_CYCLE_OSCILLATIONS:
            return 10 * 1000;       /* 10ms */
        case BRAIN_CYCLE_BRAIN_UPDATE:
            return 16 * 1000;       /* 16ms (~60 fps) */
        case BRAIN_CYCLE_IMMUNE_TICK:
            return 50 * 1000;       /* 50ms */
        case BRAIN_CYCLE_HEALTH_AGENT:
            return 100 * 1000;      /* 100ms */
        case BRAIN_CYCLE_SLEEP_WAKE:
            return 1000 * 1000;     /* 1 second */
        case BRAIN_CYCLE_CIRCADIAN:
            return 60 * 1000 * 1000;/* 1 minute for simulation */
        case BRAIN_CYCLE_AROUSAL:
            return 500 * 1000;      /* 500ms event-driven */
        case BRAIN_CYCLE_GC_AGENT:
            return 60 * 1000 * 1000;/* 60 seconds */
        case BRAIN_CYCLE_IO_DISPATCHER:
            return 100 * 1000;      /* 100ms queue-driven */
        default:
            return 100 * 1000;      /* Default 100ms */
    }
}

/**
 * @brief Initialize timing constraints for all cycle types
 */
static void initialize_timing_constraints(mesh_cycle_coordinator_integration_t* integration) {
    float multiplier = integration->config.timing_batch_multiplier;

    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        brain_cycle_type_t type = (brain_cycle_type_t)i;
        mesh_cycle_timing_constraint_t* constraint = &integration->timing_constraints[i];

        constraint->cycle_type = type;
        constraint->interval_us = get_cycle_interval_us(type);
        constraint->deadline_us = (uint64_t)(constraint->interval_us * multiplier);
        constraint->priority = mesh_cycle_get_timing_priority(type);
        constraint->affects_ordering = true;
    }

    LOG_DEBUG("Initialized timing constraints for %d cycle types", BRAIN_CYCLE_COUNT);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_cycle_coordinator_integration_t* mesh_cycle_coordinator_integration_create(
    mesh_bootstrap_t* bootstrap,
    brain_cycle_coordinator_t* cycle_coordinator,
    const mesh_cycle_coordinator_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create mesh-cycle integration without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_cycle_coordinator_integration_create: bootstrap is NULL");
        return NULL;
    }
    if (!cycle_coordinator) {
        LOG_ERROR("Cannot create mesh-cycle integration without cycle coordinator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_cycle_coordinator_integration_create: cycle_coordinator is NULL");
        return NULL;
    }

    mesh_cycle_coordinator_config_t default_config;
    if (!config) {
        mesh_cycle_coordinator_integration_default_config(&default_config);
        config = &default_config;
    }

    mesh_cycle_coordinator_integration_t* integration = nimcp_calloc(1, sizeof(*integration));
    if (!integration) {
        LOG_ERROR("Failed to allocate mesh-cycle integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_cycle_coordinator_integration_create: integration is NULL");
        return NULL;
    }

    integration->magic = MESH_CYCLE_COORDINATOR_MAGIC;
    integration->config = *config;
    integration->bootstrap = bootstrap;
    integration->cycle_coord = cycle_coordinator;

    /* Create mutex */
    mutex_attr_t attr = {0};
    integration->mutex = nimcp_mutex_create(&attr);
    if (!integration->mutex) {
        LOG_ERROR("Failed to create integration mutex");
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_cycle_coordinator_integration_create: integration->mutex is NULL");
        return NULL;
    }

    /* Initialize timing constraints */
    initialize_timing_constraints(integration);

    /* Initialize stall tracking */
    memset(integration->consecutive_stalls, 0, sizeof(integration->consecutive_stalls));
    memset(integration->last_stall_ns, 0, sizeof(integration->last_stall_ns));
    memset(integration->total_stalls, 0, sizeof(integration->total_stalls));
    memset(integration->last_recovery_action, 0, sizeof(integration->last_recovery_action));
    memset(integration->recovery_in_progress, 0, sizeof(integration->recovery_in_progress));
    memset(integration->recovery_attempts, 0, sizeof(integration->recovery_attempts));
    memset(integration->recovery_started_ns, 0, sizeof(integration->recovery_started_ns));
    memset(integration->last_recovery_succeeded, 0, sizeof(integration->last_recovery_succeeded));

    /* Get logger */
    integration->logger = nimcp_log_get_global();

    LOG_DEBUG("Mesh-cycle coordinator integration created");
    return integration;
}

void mesh_cycle_coordinator_integration_destroy(
    mesh_cycle_coordinator_integration_t* integration
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        return;
    }

    nimcp_mutex_lock(integration->mutex);
    /* Clear references */
    integration->ordering = NULL;
    integration->resilience = NULL;
    integration->health_bridge = NULL;
    integration->exception_bridge = NULL;
    integration->msp = NULL;
    integration->cycle_coord = NULL;
    integration->bootstrap = NULL;
    nimcp_mutex_unlock(integration->mutex);

    nimcp_mutex_destroy(integration->mutex);
    integration->magic = 0;
    nimcp_free(integration);

    LOG_DEBUG("Mesh-cycle coordinator integration destroyed");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

nimcp_error_t mesh_cycle_coordinator_connect_ordering(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_ordering_service_t* ordering_service
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->ordering = ordering_service;
    nimcp_mutex_unlock(integration->mutex);

    if (integration->config.verbose_logging) {
        LOG_DEBUG("Connected ordering service to mesh-cycle integration");
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_cycle_coordinator_connect_resilience(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_resilience_integration_t* resilience
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->resilience = resilience;
    nimcp_mutex_unlock(integration->mutex);

    if (integration->config.verbose_logging) {
        LOG_DEBUG("Connected resilience integration to mesh-cycle integration");
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_cycle_coordinator_connect_health_bridge(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_health_bridge_t* health_bridge
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->health_bridge = health_bridge;
    nimcp_mutex_unlock(integration->mutex);

    if (integration->config.verbose_logging) {
        LOG_DEBUG("Connected health bridge to mesh-cycle integration");
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_cycle_coordinator_connect_exception_bridge(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_exception_bridge_t* exception_bridge
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->exception_bridge = exception_bridge;
    nimcp_mutex_unlock(integration->mutex);

    if (integration->config.verbose_logging) {
        LOG_DEBUG("Connected exception bridge to mesh-cycle integration");
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_cycle_coordinator_connect_msp(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_msp_t* msp
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->msp = msp;
    nimcp_mutex_unlock(integration->mutex);

    if (integration->config.verbose_logging) {
        LOG_DEBUG("Connected MSP to mesh-cycle integration");
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Timing API
 * ============================================================================ */

nimcp_error_t mesh_cycle_coordinator_get_timing_constraint(
    const mesh_cycle_coordinator_integration_t* integration,
    brain_cycle_type_t cycle_type,
    mesh_cycle_timing_constraint_t* constraint_out
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!constraint_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_cycle_coordinator: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (cycle_type < 0 || cycle_type >= BRAIN_CYCLE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mesh_cycle_coordinator: error condition");
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    nimcp_mutex_lock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);
    *constraint_out = integration->timing_constraints[cycle_type];
    nimcp_mutex_unlock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);

    return NIMCP_SUCCESS;
}

uint64_t mesh_cycle_coordinator_get_batch_window(
    const mesh_cycle_coordinator_integration_t* integration
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        return 0;
    }

    nimcp_mutex_lock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);

    /* Find the minimum deadline among active cycles */
    uint64_t min_deadline = UINT64_MAX;

    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        const mesh_cycle_timing_constraint_t* c = &integration->timing_constraints[i];
        if (c->affects_ordering && c->deadline_us < min_deadline) {
            min_deadline = c->deadline_us;
        }
    }

    nimcp_mutex_unlock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);

    /* Return half the minimum deadline as batch window */
    if (min_deadline == UINT64_MAX) {
        return MESH_DEFAULT_ORDERING_BATCH_TIMEOUT * 1000; /* Default in us */
    }
    return min_deadline / 2;
}

uint64_t mesh_cycle_coordinator_get_commit_deadline(
    const mesh_cycle_coordinator_integration_t* integration
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        return 0;
    }

    nimcp_mutex_lock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);

    /* Find the minimum deadline for brain update cycle */
    uint64_t deadline = integration->timing_constraints[BRAIN_CYCLE_BRAIN_UPDATE].deadline_us;

    nimcp_mutex_unlock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);

    return deadline;
}

nimcp_error_t mesh_cycle_coordinator_notify_transaction_timed(
    mesh_cycle_coordinator_integration_t* integration,
    const mesh_transaction_t* tx,
    bool timing_met
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_cycle_coordinator: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->stats.mesh_transactions_timed++;
    if (!timing_met) {
        integration->stats.total_timing_violations++;
    }
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Stall Recovery API
 * ============================================================================ */

/**
 * @brief Map consecutive stall count to recovery action
 */
static mesh_recovery_action_type_t map_stalls_to_recovery_action(uint32_t consecutive_stalls) {
    if (consecutive_stalls <= 2) {
        return MESH_RECOVERY_RESTART_MODULE;
    } else if (consecutive_stalls <= 5) {
        return MESH_RECOVERY_TRIGGER_ELECTION;
    } else {
        return MESH_RECOVERY_CHECKPOINT;
    }
}

/**
 * @brief Route stall exception to exception bridge for immune awareness
 */
static void route_stall_to_exception_bridge(
    mesh_cycle_coordinator_integration_t* integration,
    brain_cycle_type_t cycle_type,
    uint64_t stall_duration_us
) {
    if (!integration->exception_bridge) {
        return;
    }

    /* Create exception response placeholder */
    mesh_exception_response_t response = {0};

    /* Route error to exception bridge */
    mesh_exception_bridge_route_error(
        integration->exception_bridge,
        NIMCP_ERROR_TIMEOUT,
        "Brain cycle stall detected",
        0, /* No specific participant */
        __FILE__,
        __LINE__,
        &response
    );

    integration->stats.exceptions_routed_to_immune++;

    if (integration->config.verbose_logging) {
        LOG_DEBUG("Routed stall for cycle %s to exception bridge",
                 brain_cycle_type_name(cycle_type));
    }

    (void)stall_duration_us;
}

nimcp_error_t mesh_cycle_coordinator_on_stall(
    mesh_cycle_coordinator_integration_t* integration,
    brain_cycle_type_t cycle_type,
    uint64_t stall_duration_us
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (cycle_type < 0 || cycle_type >= BRAIN_CYCLE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mesh_cycle_coordinator: error condition");
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Update stall tracking */
    integration->consecutive_stalls[cycle_type]++;
    integration->total_stalls[cycle_type]++;
    integration->last_stall_ns[cycle_type] = nimcp_time_now_ns();
    integration->stats.stalls_detected++;

    uint32_t stalls = integration->consecutive_stalls[cycle_type];

    LOG_WARN("Cycle %s stalled for %lu us (consecutive: %u)",
            brain_cycle_type_name(cycle_type),
            (unsigned long)stall_duration_us,
            stalls);

    /* Invoke stall callback if registered */
    if (integration->stall_callback) {
        integration->stall_callback(
            cycle_type,
            stall_duration_us,
            stalls,
            integration->stall_callback_data
        );
    }

    /* Route to exception bridge for immune awareness */
    route_stall_to_exception_bridge(integration, cycle_type, stall_duration_us);

    /* Check if recovery threshold exceeded */
    if (integration->config.enable_stall_recovery &&
        stalls >= integration->config.stall_recovery_threshold) {

        mesh_recovery_action_type_t action = map_stalls_to_recovery_action(stalls);

        /* Request recovery through resilience integration */
        if (integration->resilience) {
            mesh_recovery_action_t recovery = {0};
            recovery.type = action;
            recovery.severity = mesh_cycle_compute_stall_severity(
                stall_duration_us,
                stalls,
                integration->timing_constraints[cycle_type].interval_us
            );
            recovery.requested_at_ns = nimcp_time_now_ns();
            snprintf(recovery.reason, sizeof(recovery.reason),
                    "Cycle %s stalled %u times",
                    brain_cycle_type_name(cycle_type), stalls);

            mesh_resilience_request_recovery(integration->resilience, &recovery);

            integration->recovery_in_progress[cycle_type] = true;
            integration->recovery_attempts[cycle_type]++;
            integration->recovery_started_ns[cycle_type] = nimcp_time_now_ns();
            integration->last_recovery_action[cycle_type] = action;
            integration->stats.recoveries_triggered++;

            LOG_WARN("Triggered recovery action %s for cycle %s",
                    mesh_recovery_action_to_string(action),
                    brain_cycle_type_name(cycle_type));
        }
    }

    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_cycle_coordinator_request_recovery(
    mesh_cycle_coordinator_integration_t* integration,
    brain_cycle_type_t cycle_type,
    mesh_recovery_action_type_t action
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (cycle_type < 0 || cycle_type >= BRAIN_CYCLE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mesh_cycle_coordinator: error condition");
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    nimcp_mutex_lock(integration->mutex);

    if (!integration->resilience) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_WARN("Cannot request recovery: resilience not connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_cycle_coordinator: not initialized");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    /* Create recovery action */
    mesh_recovery_action_t recovery = {0};
    recovery.type = action;
    recovery.severity = MESH_FAILURE_DEGRADED;
    recovery.requested_at_ns = nimcp_time_now_ns();
    snprintf(recovery.reason, sizeof(recovery.reason),
            "Explicit recovery request for cycle %s",
            brain_cycle_type_name(cycle_type));

    mesh_resilience_request_recovery(integration->resilience, &recovery);

    integration->recovery_in_progress[cycle_type] = true;
    integration->recovery_attempts[cycle_type]++;
    integration->recovery_started_ns[cycle_type] = nimcp_time_now_ns();
    integration->last_recovery_action[cycle_type] = action;
    integration->stats.recoveries_triggered++;

    nimcp_mutex_unlock(integration->mutex);

    LOG_INFO("Requested recovery action %s for cycle %s",
            mesh_recovery_action_to_string(action),
            brain_cycle_type_name(cycle_type));

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_cycle_coordinator_get_recovery_status(
    const mesh_cycle_coordinator_integration_t* integration,
    brain_cycle_type_t cycle_type,
    mesh_cycle_recovery_status_t* status_out
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!status_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_cycle_coordinator: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (cycle_type < 0 || cycle_type >= BRAIN_CYCLE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mesh_cycle_coordinator: error condition");
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    nimcp_mutex_lock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);

    status_out->cycle_type = cycle_type;
    status_out->recovery_in_progress = integration->recovery_in_progress[cycle_type];
    status_out->current_action = integration->last_recovery_action[cycle_type];
    status_out->recovery_attempts = integration->recovery_attempts[cycle_type];
    status_out->recovery_started_ns = integration->recovery_started_ns[cycle_type];
    status_out->last_recovery_succeeded = integration->last_recovery_succeeded[cycle_type];

    nimcp_mutex_unlock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Health API
 * ============================================================================ */

/**
 * @brief Compute health score for a single cycle
 */
static float compute_cycle_health(
    const mesh_cycle_coordinator_integration_t* integration,
    brain_cycle_type_t cycle_type
) {
    /* Base health is 1.0 */
    float health = 1.0f;

    /* Reduce for consecutive stalls */
    uint32_t stalls = integration->consecutive_stalls[cycle_type];
    if (stalls > 0) {
        health -= 0.2f * (float)stalls;
        if (health < 0.0f) health = 0.0f;
    }

    /* Reduce if recovery in progress */
    if (integration->recovery_in_progress[cycle_type]) {
        health *= 0.7f;
    }

    return health;
}

nimcp_error_t mesh_cycle_coordinator_get_health_endorsement(
    const mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_health_endorsement_t* endorsement_out
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!endorsement_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_cycle_coordinator: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);

    memset(endorsement_out, 0, sizeof(*endorsement_out));

    float total_health = 0.0f;
    uint32_t healthy = 0;
    uint32_t degraded = 0;
    uint32_t stalled = 0;

    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        float h = compute_cycle_health(integration, (brain_cycle_type_t)i);
        endorsement_out->cycle_health[i] = h;
        total_health += h;

        if (h >= 0.8f) {
            healthy++;
        } else if (h >= 0.4f) {
            degraded++;
        } else {
            stalled++;
        }
    }

    endorsement_out->overall_health = total_health / (float)BRAIN_CYCLE_COUNT;
    endorsement_out->healthy_cycles = healthy;
    endorsement_out->degraded_cycles = degraded;
    endorsement_out->stalled_cycles = stalled;
    endorsement_out->endorsement_weight = integration->config.health_endorsement_weight;
    endorsement_out->computed_at_ns = nimcp_time_now_ns();

    ((mesh_cycle_coordinator_integration_t*)integration)->stats.health_endorsements++;

    nimcp_mutex_unlock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_cycle_coordinator_contribute_health(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_channel_id_t channel,
    float health_score
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Route to health bridge if connected */
    if (integration->health_bridge) {
        /* Health bridge doesn't have a direct contribute API, but we can
           update metrics for a synthetic participant */
        /* For now, log the contribution */
        if (integration->config.verbose_logging) {
            LOG_DEBUG("Health contribution for channel %u: %.2f",
                     channel, health_score);
        }
    }

    return NIMCP_SUCCESS;
}

float mesh_cycle_coordinator_get_aggregate_health(
    const mesh_cycle_coordinator_integration_t* integration
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        return -1.0f;
    }

    mesh_cycle_health_endorsement_t endorsement;
    if (mesh_cycle_coordinator_get_health_endorsement(integration, &endorsement) != NIMCP_SUCCESS) {
        return -1.0f;
    }

    return endorsement.overall_health;
}

/* ============================================================================
 * Callback Registration API
 * ============================================================================ */

nimcp_error_t mesh_cycle_coordinator_set_stall_callback(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_stall_callback_t callback,
    void* user_data
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->stall_callback = callback;
    integration->stall_callback_data = user_data;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_cycle_coordinator_set_recovery_callback(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_recovery_callback_t callback,
    void* user_data
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->recovery_callback = callback;
    integration->recovery_callback_data = user_data;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_cycle_coordinator_set_timing_callback(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_timing_callback_t callback,
    void* user_data
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->timing_callback = callback;
    integration->timing_callback_data = user_data;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_cycle_coordinator_get_stats(
    const mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_coordinator_stats_t* stats_out
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_cycle_coordinator: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);
    *stats_out = integration->stats;
    nimcp_mutex_unlock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_cycle_coordinator_reset_stats(
    mesh_cycle_coordinator_integration_t* integration
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    memset(&integration->stats, 0, sizeof(integration->stats));
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

nimcp_error_t mesh_cycle_coordinator_update(
    mesh_cycle_coordinator_integration_t* integration,
    uint64_t delta_ms
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cycle_coordinator: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    uint64_t now_ns = nimcp_time_now_ns();

    /* Check for recovery completion */
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        if (integration->recovery_in_progress[i]) {
            /* Check if we haven't had a stall in a while (recovery succeeded) */
            uint64_t since_stall_ns = now_ns - integration->last_stall_ns[i];
            uint64_t timeout_ns = integration->timing_constraints[i].interval_us * 1000 * 5;

            if (since_stall_ns > timeout_ns) {
                /* Consider recovery complete */
                integration->recovery_in_progress[i] = false;
                integration->consecutive_stalls[i] = 0;
                integration->last_recovery_succeeded[i] = true;

                if (integration->recovery_callback) {
                    integration->recovery_callback(
                        (brain_cycle_type_t)i,
                        integration->last_recovery_action[i],
                        true,
                        integration->recovery_callback_data
                    );
                }

                if (integration->config.verbose_logging) {
                    LOG_INFO("Recovery complete for cycle %s",
                            brain_cycle_type_name((brain_cycle_type_t)i));
                }
            }
        }
    }

    /* Update timing statistics */
    if (integration->stats.mesh_transactions_timed > 0) {
        /* Compute average batch window */
        uint64_t batch = mesh_cycle_coordinator_get_batch_window(integration);
        integration->stats.avg_batch_window_us = (float)batch;

        /* Compute average commit deadline */
        uint64_t deadline = mesh_cycle_coordinator_get_commit_deadline(integration);
        integration->stats.avg_commit_deadline_us = (float)deadline;
    }

    nimcp_mutex_unlock(integration->mutex);

    (void)delta_ms;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bootstrap Integration
 * ============================================================================ */

mesh_cycle_coordinator_integration_t* mesh_bootstrap_get_cycle_coordinator_integration(
    mesh_bootstrap_t* bootstrap
) {
    /* This would need to be added to mesh_bootstrap_t structure */
    /* For now, return NULL as placeholder */
    (void)bootstrap;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap_get_cycle_coordinator_integration: operation failed");
    return NULL;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

uint32_t mesh_cycle_get_timing_priority(brain_cycle_type_t cycle_type) {
    /* Lower number = higher priority */
    switch (cycle_type) {
        case BRAIN_CYCLE_OSCILLATIONS:
            return 0;       /* Highest priority - 10ms */
        case BRAIN_CYCLE_BRAIN_UPDATE:
            return 1;       /* Second - 16ms */
        case BRAIN_CYCLE_IMMUNE_TICK:
            return 2;       /* 50ms */
        case BRAIN_CYCLE_HEALTH_AGENT:
            return 3;       /* 100ms */
        case BRAIN_CYCLE_AROUSAL:
            return 4;       /* 500ms */
        case BRAIN_CYCLE_SLEEP_WAKE:
            return 5;       /* 1s */
        case BRAIN_CYCLE_IO_DISPATCHER:
            return 6;       /* Queue-driven */
        case BRAIN_CYCLE_GC_AGENT:
            return 7;       /* 60s */
        case BRAIN_CYCLE_CIRCADIAN:
            return 8;       /* Lowest priority */
        default:
            return 100;
    }
}

mesh_failure_severity_t mesh_cycle_compute_stall_severity(
    uint64_t stall_duration_us,
    uint32_t consecutive_stalls,
    uint64_t expected_interval_us
) {
    /* Compute stall ratio */
    float ratio = (float)stall_duration_us / (float)expected_interval_us;

    if (consecutive_stalls >= 5 || ratio >= 10.0f) {
        return MESH_FAILURE_FATAL;
    } else if (consecutive_stalls >= 3 || ratio >= 5.0f) {
        return MESH_FAILURE_CRITICAL;
    } else if (consecutive_stalls >= 2 || ratio >= 2.0f) {
        return MESH_FAILURE_DEGRADED;
    } else {
        return MESH_FAILURE_WARNING;
    }
}

void mesh_cycle_coordinator_print_status(
    const mesh_cycle_coordinator_integration_t* integration
) {
    if (!integration || integration->magic != MESH_CYCLE_COORDINATOR_MAGIC) {
        LOG_ERROR("Invalid mesh-cycle coordinator integration");
        return;
    }

    LOG_INFO("=== Mesh-Cycle Coordinator Integration Status ===");

    nimcp_mutex_lock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);

    /* Print configuration */
    LOG_INFO("Configuration:");
    LOG_INFO("  Timing constraints: %s",
            integration->config.enable_timing_constraints ? "enabled" : "disabled");
    LOG_INFO("  Stall recovery: %s (threshold: %u)",
            integration->config.enable_stall_recovery ? "enabled" : "disabled",
            integration->config.stall_recovery_threshold);
    LOG_INFO("  Distributed health: %s (weight: %.2f)",
            integration->config.enable_distributed_health ? "enabled" : "disabled",
            integration->config.health_endorsement_weight);
    LOG_INFO("  Timing batch multiplier: %.2f",
            integration->config.timing_batch_multiplier);

    /* Print connected subsystems */
    LOG_INFO("Connected Subsystems:");
    LOG_INFO("  Ordering service: %s", integration->ordering ? "yes" : "no");
    LOG_INFO("  Resilience: %s", integration->resilience ? "yes" : "no");
    LOG_INFO("  Health bridge: %s", integration->health_bridge ? "yes" : "no");
    LOG_INFO("  Exception bridge: %s", integration->exception_bridge ? "yes" : "no");
    LOG_INFO("  MSP: %s", integration->msp ? "yes" : "no");

    /* Print per-cycle status */
    LOG_INFO("Per-Cycle Status:");
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        brain_cycle_type_t type = (brain_cycle_type_t)i;
        LOG_INFO("  %s: interval=%lu us, stalls=%u, health=%.2f",
                brain_cycle_type_name(type),
                (unsigned long)integration->timing_constraints[i].interval_us,
                integration->consecutive_stalls[i],
                compute_cycle_health(integration, type));
    }

    /* Print statistics */
    LOG_INFO("Statistics:");
    LOG_INFO("  Cycles reported: %lu", (unsigned long)integration->stats.cycles_reported);
    LOG_INFO("  Stalls detected: %lu", (unsigned long)integration->stats.stalls_detected);
    LOG_INFO("  Recoveries triggered: %lu", (unsigned long)integration->stats.recoveries_triggered);
    LOG_INFO("  Timing violations: %lu", (unsigned long)integration->stats.total_timing_violations);
    LOG_INFO("  Health endorsements: %lu", (unsigned long)integration->stats.health_endorsements);
    LOG_INFO("  Exceptions routed: %lu", (unsigned long)integration->stats.exceptions_routed_to_immune);
    LOG_INFO("  BBB validations: %lu", (unsigned long)integration->stats.bbb_validations);

    nimcp_mutex_unlock(((mesh_cycle_coordinator_integration_t*)integration)->mutex);

    LOG_INFO("=================================================");
}
