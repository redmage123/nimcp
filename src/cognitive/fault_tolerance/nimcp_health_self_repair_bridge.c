/**
 * @file nimcp_health_self_repair_bridge.c
 * @brief Implementation of Health Detection to Self-Repair Automation Bridge
 * @version 1.0.0
 * @date 2025-01-20
 */

#include "cognitive/fault_tolerance/nimcp_health_self_repair_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(health_self_repair_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_health_self_repair_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_health_self_repair_bridge_mesh_registry = NULL;

nimcp_error_t health_self_repair_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_health_self_repair_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "health_self_repair_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "health_self_repair_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_health_self_repair_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_health_self_repair_bridge_mesh_registry = registry;
    return err;
}

void health_self_repair_bridge_mesh_unregister(void) {
    if (g_health_self_repair_bridge_mesh_registry && g_health_self_repair_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_health_self_repair_bridge_mesh_registry, g_health_self_repair_bridge_mesh_id);
        g_health_self_repair_bridge_mesh_id = 0;
        g_health_self_repair_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from health_self_repair_bridge module (instance-level) */
static inline void health_self_repair_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_health_self_repair_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_health_self_repair_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_health_self_repair_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "HEALTH_SELF_REPAIR_BRIDGE"


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Aggregation batch entry
 */
typedef struct {
    diagnostic_result_t* diagnostic;    /**< Diagnostic to repair */
    uint64_t added_at;                  /**< When added to batch */
} aggregation_entry_t;

/**
 * @brief Health self-repair bridge internal state
 */
struct health_self_repair_bridge {
    bridge_base_t base;                         /**< MUST be first: base bridge infrastructure */
    uint32_t magic;                             /**< Magic number for validation */
    health_self_repair_bridge_config_t config;  /**< Configuration */

    /* Dependencies */
    health_diag_bridge_t* diagnostic_bridge;    /**< Diagnostic converter */
    self_repair_coordinator_t* self_repair;     /**< Self-repair coordinator */
    nimcp_health_agent_t* health_agent;         /**< Health agent (optional) */

    /* Tracking */
    health_repair_tracking_t* tracking;         /**< Tracking records */
    uint32_t tracking_count;                    /**< Current tracking count */
    uint32_t tracking_capacity;                 /**< Tracking capacity */
    uint64_t next_request_id;                   /**< Next request ID */

    /* Rate limiting */
    uint64_t window_start_ms;                   /**< Current window start time */
    uint32_t window_repair_count;               /**< Repairs in current window */
    uint64_t cooldown_until_ms;                 /**< Cooldown end time (if active) */

    /* Aggregation */
    aggregation_entry_t* aggregation_batch;     /**< Current aggregation batch */
    uint32_t aggregation_count;                 /**< Items in aggregation batch */
    uint64_t aggregation_window_start_ms;       /**< Aggregation window start */

    /* Callbacks */
    health_repair_trigger_cb_t trigger_callback;
    health_repair_outcome_cb_t outcome_callback;
    void* callback_user_data;

    /* Statistics */
    health_self_repair_bridge_stats_t stats;

    /* Timing for stats */
    uint64_t total_repair_time_ms;
    uint64_t repair_count_for_avg;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;

    /* State */
    bool initialized;
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Check if severity meets trigger policy threshold
 */
static bool should_trigger_by_policy(
    health_trigger_policy_t policy,
    diag_severity_t severity
) {
    switch (policy) {
        case HEALTH_TRIGGER_MANUAL:
            return false;

        case HEALTH_TRIGGER_FATAL_ONLY:
            return severity == DIAG_SEVERITY_FATAL;

        case HEALTH_TRIGGER_CRITICAL:
            return severity >= DIAG_SEVERITY_CRITICAL;

        case HEALTH_TRIGGER_ERROR:
            return severity >= DIAG_SEVERITY_ERROR;

        case HEALTH_TRIGGER_AUTO:
            /* Auto mode triggers on ERROR and above */
            return severity >= DIAG_SEVERITY_ERROR;

        default:
            return false;
    }
}

/**
 * @brief Update rate limit window if needed
 */
static void update_rate_limit_window(health_self_repair_bridge_t* bridge) {
    uint64_t now = nimcp_time_get_ms();

    /* Check if window has expired */
    if (now >= bridge->window_start_ms + bridge->config.rate_limit.window_duration_ms) {
        bridge->window_start_ms = now;
        bridge->window_repair_count = 0;
    }

    /* Check if cooldown has expired */
    if (bridge->cooldown_until_ms > 0 && now >= bridge->cooldown_until_ms) {
        bridge->cooldown_until_ms = 0;
    }
}

/**
 * @brief Check rate limit and update counters
 */
static bool check_and_update_rate_limit(
    health_self_repair_bridge_t* bridge,
    error_type_t error_type
) {
    (void)error_type; /* TODO: implement per-type limiting */

    update_rate_limit_window(bridge);

    /* Check cooldown */
    if (bridge->cooldown_until_ms > 0) {
        return true; /* Rate limited during cooldown */
    }

    /* Check window limit */
    if (bridge->window_repair_count >= bridge->config.rate_limit.max_repairs_per_window) {
        /* Enter cooldown */
        bridge->cooldown_until_ms = nimcp_time_get_ms() + bridge->config.rate_limit.cooldown_ms;
        bridge->stats.rate_limited_count++;
        return true;
    }

    /* Increment counter */
    bridge->window_repair_count++;
    bridge->stats.current_window_count = bridge->window_repair_count;

    return false;
}

/**
 * @brief Add tracking record
 */
static health_repair_tracking_t* add_tracking_record(
    health_self_repair_bridge_t* bridge,
    const diagnostic_result_t* diagnostic
) {
    if (bridge->tracking_count >= bridge->tracking_capacity) {
        /* Shift out oldest record */
        memmove(&bridge->tracking[0], &bridge->tracking[1],
                (bridge->tracking_capacity - 1) * sizeof(health_repair_tracking_t));
        bridge->tracking_count = bridge->tracking_capacity - 1;
    }

    health_repair_tracking_t* record = &bridge->tracking[bridge->tracking_count++];
    memset(record, 0, sizeof(*record));

    record->request_id = bridge->next_request_id++;
    record->diagnostic_id = diagnostic->error_id;
    record->error_type = diagnostic->error_type;
    record->severity = diagnostic->severity;
    record->confidence = diagnostic->confidence;
    record->outcome = HEALTH_REPAIR_OUTCOME_PENDING;
    record->submitted_at = nimcp_time_get_ms();
    record->async = bridge->config.async_repairs;

    return record;
}

/**
 * @brief Find tracking record by request ID
 */
static health_repair_tracking_t* find_tracking_record(
    health_self_repair_bridge_t* bridge,
    uint64_t request_id
) {
    for (uint32_t i = 0; i < bridge->tracking_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->tracking_count > 256) {
            health_self_repair_bridge_heartbeat("health_self__loop",
                             (float)(i + 1) / (float)bridge->tracking_count);
        }

        if (bridge->tracking[i].request_id == request_id) {
            return &bridge->tracking[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_tracking_record: validation failed");
    return NULL;
}

/**
 * @brief Update tracking record with outcome
 */
static void update_tracking_outcome(
    health_repair_tracking_t* record,
    health_repair_outcome_t outcome,
    const self_repair_result_t* result
) {
    record->outcome = outcome;
    record->completed_at = nimcp_time_get_ms();
    record->duration_ms = record->completed_at - record->submitted_at;

    if (result && !result->success && result->error_message[0]) {
        snprintf(record->error_message, sizeof(record->error_message),
                 "%s", result->error_message);
    }
}

/**
 * @brief Submit repair to self-repair coordinator
 */
static int submit_repair(
    health_self_repair_bridge_t* bridge,
    diagnostic_result_t* diagnostic,
    health_repair_tracking_t* tracking
) {
    self_repair_request_t request = {0};
    request.diagnosis = diagnostic;

    if (bridge->config.async_repairs) {
        uint64_t repair_id = 0;
        int ret = self_repair_initiate_async(bridge->self_repair, &request, &repair_id);
        if (ret != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "submit_repair: validation failed");
            return -1;
        }
        tracking->repair_id = repair_id;
    } else {
        self_repair_result_t result = {0};
        int ret = self_repair_initiate(bridge->self_repair, &request, &result);

        /* Update tracking immediately */
        health_repair_outcome_t outcome;
        if (ret == 0 && result.success) {
            outcome = HEALTH_REPAIR_OUTCOME_SUCCESS;
            bridge->stats.repairs_succeeded++;
        } else {
            outcome = HEALTH_REPAIR_OUTCOME_FAILED;
            bridge->stats.repairs_failed++;
        }

        update_tracking_outcome(tracking, outcome, &result);

        /* Invoke outcome callback */
        if (bridge->outcome_callback) {
            bridge->outcome_callback(tracking->request_id, outcome, &result,
                                     bridge->callback_user_data);
        }

        /* Update timing stats */
        bridge->total_repair_time_ms += tracking->duration_ms;
        bridge->repair_count_for_avg++;
        bridge->stats.avg_repair_time_ms =
            (float)bridge->total_repair_time_ms / (float)bridge->repair_count_for_avg;

        if (tracking->duration_ms > bridge->stats.max_repair_time_ms) {
            bridge->stats.max_repair_time_ms = (float)tracking->duration_ms;
        }
    }

    return 0;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int health_self_repair_bridge_default_config(
    health_self_repair_bridge_config_t* config
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__default_config", 0.0f);


    memset(config, 0, sizeof(*config));

    config->trigger_policy = HEALTH_TRIGGER_CRITICAL;

    /* Rate limiting defaults */
    config->rate_limit.max_repairs_per_window = 10;
    config->rate_limit.window_duration_ms = 60000;  /* 1 minute */
    config->rate_limit.cooldown_ms = 10000;         /* 10 seconds */
    config->rate_limit.per_error_type_limit = false;

    /* Aggregation defaults */
    config->aggregation.enabled = true;
    config->aggregation.window_ms = 5000;           /* 5 seconds */
    config->aggregation.max_batch_size = 5;
    config->aggregation.aggregate_same_type_only = true;

    /* Repair options */
    config->async_repairs = true;
    config->notify_health_agent = true;
    config->enable_bio_async = true;

    /* Confidence threshold */
    config->min_confidence = 0.6f;

    /* Timeout */
    config->repair_timeout_ms = 30000;  /* 30 seconds */

    /* Learning */
    config->learn_from_outcomes = true;

    /* Logging */
    config->verbose_logging = false;

    return 0;
}

health_self_repair_bridge_t* health_self_repair_bridge_create(
    const health_self_repair_bridge_config_t* config,
    health_diag_bridge_t* diagnostic_bridge,
    self_repair_coordinator_t* self_repair
) {
    if (!diagnostic_bridge || !self_repair) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_self_repair_bridge_create: required parameter is NULL (diagnostic_bridge, self_repair)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__create", 0.0f);


    health_self_repair_bridge_t* bridge = nimcp_calloc(1, sizeof(health_self_repair_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    bridge->magic = HEALTH_SELF_REPAIR_BRIDGE_MAGIC;
    bridge->diagnostic_bridge = diagnostic_bridge;
    bridge->self_repair = self_repair;

    /* Apply config or defaults */
    if (config) {
        bridge->config = *config;
    } else {
        health_self_repair_bridge_default_config(&bridge->config);
    }

    /* Allocate tracking array */
    bridge->tracking_capacity = HEALTH_SELF_REPAIR_MAX_HISTORY;
    bridge->tracking = nimcp_calloc(bridge->tracking_capacity, sizeof(health_repair_tracking_t));
    if (!bridge->tracking) {
        health_self_repair_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "health_self_repair_bridge_create: bridge->tracking is NULL");
        return NULL;
    }

    /* Allocate aggregation batch */
    if (bridge->config.aggregation.enabled) {
        bridge->aggregation_batch = nimcp_calloc(
            bridge->config.aggregation.max_batch_size, sizeof(aggregation_entry_t));
        if (!bridge->aggregation_batch) {
            health_self_repair_bridge_destroy(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "health_self_repair_bridge_create: bridge->aggregation_batch is NULL");
            return NULL;
        }
    }

    /* Create mutex */
    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, 0, "health_self_repair") != 0) {
        health_self_repair_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_UNKNOWN, "health_self_repair_bridge_create: bridge_base_init failed");
        return NULL;
    }

    /* Initialize timing */
    bridge->window_start_ms = nimcp_time_get_ms();
    bridge->next_request_id = 1;

    /* Register with bio-router if enabled */
    if (bridge->config.enable_bio_async) {
        bio_module_info_t bio_info = {0};
        bio_info.module_id = BIO_MODULE_HEALTH_SELF_REPAIR_BRIDGE;
        bio_info.module_name = "health-self-repair-bridge";
        bio_info.user_data = bridge;

        bridge->bio_ctx = bio_router_register_module(&bio_info);
        if (!bridge->bio_ctx) {
            /* Non-fatal - bio-async is optional */
            if (bridge->config.verbose_logging) {
                fprintf(stderr, "Bio-async router not available, skipping registration\n");
            }
        }
    }

    bridge->initialized = true;
    return bridge;
}

void health_self_repair_bridge_destroy(health_self_repair_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "health_self_repair");

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__destroy", 0.0f);


    if (bridge->magic != HEALTH_SELF_REPAIR_BRIDGE_MAGIC) {
        return;
    }

    bridge->magic = 0;
    bridge->initialized = false;

    /* Unregister from bio-router */
    if (bridge->bio_ctx) {
        bio_router_unregister_module(bridge->bio_ctx);
        bridge->bio_ctx = NULL;
    }

    /* Free aggregation batch diagnostics */
    if (bridge->aggregation_batch) {
        for (uint32_t i = 0; i < bridge->aggregation_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->aggregation_count > 256) {
                health_self_repair_bridge_heartbeat("health_self__loop",
                                 (float)(i + 1) / (float)bridge->aggregation_count);
            }

            if (bridge->aggregation_batch[i].diagnostic) {
                diagnostics_free_result(bridge->aggregation_batch[i].diagnostic);
            }
        }
        nimcp_free(bridge->aggregation_batch);
    }

    if (bridge->tracking) {
        nimcp_free(bridge->tracking);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int health_self_repair_bridge_connect_health_agent(
    health_self_repair_bridge_t* bridge,
    nimcp_health_agent_t* health_agent
) {
    if (!bridge || !health_agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_self_repair_bridge_connect_health_agent: required parameter is NULL (bridge, health_agent)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__connect_health_agent", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->health_agent = health_agent;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Trigger API Implementation
 * ============================================================================ */

int health_self_repair_bridge_process_anomaly(
    health_self_repair_bridge_t* bridge,
    const anomaly_t* anomaly,
    uint64_t* request_id
) {
    if (!bridge || !anomaly) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_self_repair_bridge_process_anomaly: required parameter is NULL (bridge, anomaly)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "health_self_repair_bridge_process_anomaly: bridge not initialized");
        return -1;
    }

    /* Convert anomaly to diagnostic */
    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__process_anomaly", 0.0f);


    diagnostic_result_t* diagnostic = NULL;
    int ret = health_diag_bridge_convert_anomaly(bridge->diagnostic_bridge, anomaly, &diagnostic);
    if (ret != 0 || !diagnostic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_self_repair_bridge_process_anomaly: diagnostic is NULL");
        return -1;
    }

    /* Trigger from diagnostic */
    ret = health_self_repair_bridge_trigger_from_diagnostic(bridge, diagnostic, request_id);

    /* Clean up if not triggered (diagnostic ownership transferred on success) */
    if (ret != 0) {
        diagnostics_free_result(diagnostic);
    }

    return ret;
}

int health_self_repair_bridge_process_agent_message(
    health_self_repair_bridge_t* bridge,
    const health_agent_message_t* message,
    uint64_t* request_id
) {
    if (!bridge || !message) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_self_repair_bridge_process_agent_message: required parameter is NULL (bridge, message)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "health_self_repair_bridge_process_agent_message: bridge not initialized");
        return -1;
    }

    /* Convert message to diagnostic */
    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__process_agent_messag", 0.0f);


    diagnostic_result_t* diagnostic = NULL;
    int ret = health_diag_bridge_convert_agent_message(
        bridge->diagnostic_bridge, message, &diagnostic);
    if (ret != 0 || !diagnostic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_self_repair_bridge_process_agent_message: diagnostic is NULL");
        return -1;
    }

    /* Trigger from diagnostic */
    ret = health_self_repair_bridge_trigger_from_diagnostic(bridge, diagnostic, request_id);

    /* Clean up if not triggered */
    if (ret != 0) {
        diagnostics_free_result(diagnostic);
    }

    return ret;
}

int health_self_repair_bridge_trigger_from_diagnostic(
    health_self_repair_bridge_t* bridge,
    diagnostic_result_t* diagnostic,
    uint64_t* request_id
) {
    if (!bridge || !diagnostic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_self_repair_bridge_trigger_from_diagnostic: required parameter is NULL (bridge, diagnostic)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "health_self_repair_bridge_trigger_from_diagnostic: bridge not initialized");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__trigger_from_diagnos", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check trigger policy */
    if (!should_trigger_by_policy(bridge->config.trigger_policy, diagnostic->severity)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge->stats.repairs_skipped++;
        return 1; /* Skipped due to policy */
    }

    /* Check confidence threshold */
    if (diagnostic->confidence < bridge->config.min_confidence) {
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge->stats.repairs_skipped++;
        return 1; /* Skipped due to low confidence */
    }

    /* Check rate limit */
    if (check_and_update_rate_limit(bridge, diagnostic->error_type)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge->stats.repairs_skipped++;
        return 1; /* Rate limited */
    }

    /* Check if aggregation is enabled */
    if (bridge->config.aggregation.enabled) {
        /* Add to aggregation batch instead of immediate submit */
        if (bridge->aggregation_count < bridge->config.aggregation.max_batch_size) {
            if (bridge->aggregation_count == 0) {
                bridge->aggregation_window_start_ms = nimcp_time_get_ms();
            }

            bridge->aggregation_batch[bridge->aggregation_count].diagnostic = diagnostic;
            bridge->aggregation_batch[bridge->aggregation_count].added_at = nimcp_time_get_ms();
            bridge->aggregation_count++;

            /* Create tracking record */
            health_repair_tracking_t* tracking = add_tracking_record(bridge, diagnostic);
            tracking->aggregated = true;

            if (request_id) {
                *request_id = tracking->request_id;
            }

            /* Invoke trigger callback (even for aggregated repairs) */
            if (bridge->trigger_callback) {
                bridge->trigger_callback(tracking->request_id, diagnostic,
                                         bridge->callback_user_data);
            }

            /* Broadcast via bio-async */
            if (bridge->config.enable_bio_async) {
                health_self_repair_bridge_broadcast_trigger(bridge,
                    tracking->request_id, diagnostic);
            }

            bridge->stats.repairs_triggered++;
            bridge->stats.by_severity[diagnostic->severity]++;

            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    /* Create tracking record */
    health_repair_tracking_t* tracking = add_tracking_record(bridge, diagnostic);

    if (request_id) {
        *request_id = tracking->request_id;
    }

    /* Invoke trigger callback */
    if (bridge->trigger_callback) {
        bridge->trigger_callback(tracking->request_id, diagnostic, bridge->callback_user_data);
    }

    /* Broadcast via bio-async */
    if (bridge->config.enable_bio_async) {
        health_self_repair_bridge_broadcast_trigger(bridge, tracking->request_id, diagnostic);
    }

    /* Update statistics */
    bridge->stats.repairs_triggered++;
    bridge->stats.by_severity[diagnostic->severity]++;

    /* Submit to self-repair */
    int ret = submit_repair(bridge, diagnostic, tracking);

    /* Free diagnostic - self_repair copies data, doesn't take ownership */
    diagnostics_free_result(diagnostic);

    nimcp_mutex_unlock(bridge->base.mutex);

    return ret;
}

int health_self_repair_bridge_force_trigger(
    health_self_repair_bridge_t* bridge,
    diagnostic_result_t* diagnostic,
    uint64_t* request_id
) {
    if (!bridge || !diagnostic || !request_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_self_repair_bridge_force_trigger: required parameter is NULL (bridge, diagnostic, request_id)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "health_self_repair_bridge_force_trigger: bridge not initialized");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__force_trigger", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Create tracking record (bypass all checks) */
    health_repair_tracking_t* tracking = add_tracking_record(bridge, diagnostic);
    *request_id = tracking->request_id;

    /* Update rate limit counter (but don't block) */
    update_rate_limit_window(bridge);
    bridge->window_repair_count++;

    /* Invoke trigger callback */
    if (bridge->trigger_callback) {
        bridge->trigger_callback(tracking->request_id, diagnostic, bridge->callback_user_data);
    }

    /* Broadcast via bio-async */
    if (bridge->config.enable_bio_async) {
        health_self_repair_bridge_broadcast_trigger(bridge, tracking->request_id, diagnostic);
    }

    /* Update statistics */
    bridge->stats.repairs_triggered++;
    bridge->stats.by_severity[diagnostic->severity]++;

    /* Submit to self-repair */
    int ret = submit_repair(bridge, diagnostic, tracking);

    /* Free diagnostic - self_repair copies data, doesn't take ownership */
    diagnostics_free_result(diagnostic);

    nimcp_mutex_unlock(bridge->base.mutex);

    return ret;
}

/* ============================================================================
 * Rate Limiting API Implementation
 * ============================================================================ */

bool health_self_repair_bridge_is_rate_limited(
    const health_self_repair_bridge_t* bridge,
    error_type_t error_type
) {
    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__is_rate_limited", 0.0f);


    (void)error_type;

    if (!bridge || !bridge->initialized) {
        return true;
    }

    /* Cast away const for mutex - safe as we're just reading */
    health_self_repair_bridge_t* mutable_bridge = (health_self_repair_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    uint64_t now = nimcp_time_get_ms();

    /* Check cooldown */
    if (bridge->cooldown_until_ms > 0 && now < bridge->cooldown_until_ms) {
        nimcp_mutex_unlock(mutable_bridge->base.mutex);
        return true;
    }

    /* Check if window has expired */
    bool in_window = (now < bridge->window_start_ms + bridge->config.rate_limit.window_duration_ms);
    bool at_limit = (bridge->window_repair_count >= bridge->config.rate_limit.max_repairs_per_window);

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return in_window && at_limit;
}

void health_self_repair_bridge_reset_rate_limit(
    health_self_repair_bridge_t* bridge
) {
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__reset_rate_limit", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->window_start_ms = nimcp_time_get_ms();
    bridge->window_repair_count = 0;
    bridge->cooldown_until_ms = 0;
    bridge->stats.current_window_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);
}

/* ============================================================================
 * Tracking and Query API Implementation
 * ============================================================================ */

const health_repair_tracking_t* health_self_repair_bridge_get_tracking(
    const health_self_repair_bridge_t* bridge,
    uint64_t request_id
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "health_self_repair_bridge_get_tracking: bridge not initialized");
        return NULL;
    }

    /* Cast away const for mutex */
    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__get_tracking", 0.0f);


    health_self_repair_bridge_t* mutable_bridge = (health_self_repair_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    const health_repair_tracking_t* record = find_tracking_record(mutable_bridge, request_id);
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return record;
}

uint32_t health_self_repair_bridge_get_recent_tracking(
    const health_self_repair_bridge_t* bridge,
    health_repair_tracking_t* records,
    uint32_t max_records
) {
    if (!bridge || !records || !bridge->initialized) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__get_recent_tracking", 0.0f);


    health_self_repair_bridge_t* mutable_bridge = (health_self_repair_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    uint32_t count = bridge->tracking_count;
    if (count > max_records) {
        count = max_records;
    }

    /* Copy most recent records (from end of array) */
    if (count > 0) {
        uint32_t start = bridge->tracking_count - count;
        memcpy(records, &bridge->tracking[start], count * sizeof(health_repair_tracking_t));
    }

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return count;
}

uint32_t health_self_repair_bridge_get_pending_count(
    const health_self_repair_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__get_pending_count", 0.0f);


    health_self_repair_bridge_t* mutable_bridge = (health_self_repair_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    uint32_t pending = 0;
    for (uint32_t i = 0; i < bridge->tracking_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->tracking_count > 256) {
            health_self_repair_bridge_heartbeat("health_self__loop",
                             (float)(i + 1) / (float)bridge->tracking_count);
        }

        if (bridge->tracking[i].outcome == HEALTH_REPAIR_OUTCOME_PENDING) {
            pending++;
        }
    }

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return pending;
}

/* ============================================================================
 * Callback Registration Implementation
 * ============================================================================ */

int health_self_repair_bridge_set_trigger_callback(
    health_self_repair_bridge_t* bridge,
    health_repair_trigger_cb_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__set_trigger_callback", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->trigger_callback = callback;
    bridge->callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int health_self_repair_bridge_set_outcome_callback(
    health_self_repair_bridge_t* bridge,
    health_repair_outcome_cb_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__set_outcome_callback", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->outcome_callback = callback;
    bridge->callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int health_self_repair_bridge_get_stats(
    const health_self_repair_bridge_t* bridge,
    health_self_repair_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_self_repair_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__get_stats", 0.0f);


    health_self_repair_bridge_t* mutable_bridge = (health_self_repair_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    *stats = bridge->stats;

    /* Calculate success rate */
    uint64_t total_completed = stats->repairs_succeeded + stats->repairs_failed;
    if (total_completed > 0) {
        stats->success_rate = (float)stats->repairs_succeeded / (float)total_completed;
    } else {
        stats->success_rate = 0.0f;
    }

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

void health_self_repair_bridge_reset_stats(
    health_self_repair_bridge_t* bridge
) {
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->total_repair_time_ms = 0;
    bridge->repair_count_for_avg = 0;
    nimcp_mutex_unlock(bridge->base.mutex);
}

/* ============================================================================
 * Processing API Implementation
 * ============================================================================ */

uint32_t health_self_repair_bridge_process_outcomes(
    health_self_repair_bridge_t* bridge,
    uint32_t max_process
) {
    if (!bridge || !bridge->initialized) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__process_outcomes", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t processed = 0;

    for (uint32_t i = 0; i < bridge->tracking_count && processed < max_process; i++) {
        health_repair_tracking_t* tracking = &bridge->tracking[i];

        if (tracking->outcome != HEALTH_REPAIR_OUTCOME_PENDING) {
            continue;
        }

        /* Check async repair status */
        self_repair_result_t result = {0};
        repair_stage_t stage = self_repair_get_status(
            bridge->self_repair, tracking->repair_id, &result);

        if (stage == REPAIR_STAGE_COMPLETED || stage == REPAIR_STAGE_FAILED) {
            health_repair_outcome_t outcome;
            if (stage == REPAIR_STAGE_COMPLETED && result.success) {
                outcome = HEALTH_REPAIR_OUTCOME_SUCCESS;
                bridge->stats.repairs_succeeded++;
            } else {
                outcome = HEALTH_REPAIR_OUTCOME_FAILED;
                bridge->stats.repairs_failed++;
            }

            update_tracking_outcome(tracking, outcome, &result);

            /* Update timing stats */
            bridge->total_repair_time_ms += tracking->duration_ms;
            bridge->repair_count_for_avg++;
            bridge->stats.avg_repair_time_ms =
                (float)bridge->total_repair_time_ms / (float)bridge->repair_count_for_avg;

            if (tracking->duration_ms > bridge->stats.max_repair_time_ms) {
                bridge->stats.max_repair_time_ms = (float)tracking->duration_ms;
            }

            /* Invoke outcome callback */
            if (bridge->outcome_callback) {
                bridge->outcome_callback(tracking->request_id, outcome, &result,
                                         bridge->callback_user_data);
            }

            /* Broadcast via bio-async */
            if (bridge->config.enable_bio_async) {
                health_self_repair_bridge_broadcast_outcome(
                    bridge, tracking->request_id, outcome);
            }

            processed++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return processed;
}

uint32_t health_self_repair_bridge_process_aggregation(
    health_self_repair_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized || !bridge->config.aggregation.enabled) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__process_aggregation", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->aggregation_count == 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    uint64_t now = nimcp_time_get_ms();
    uint64_t window_elapsed = now - bridge->aggregation_window_start_ms;

    /* Check if window has expired or batch is full */
    bool should_submit = (window_elapsed >= bridge->config.aggregation.window_ms) ||
                         (bridge->aggregation_count >= bridge->config.aggregation.max_batch_size);

    if (!should_submit) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    uint32_t submitted = 0;

    /* Submit all items in aggregation batch */
    for (uint32_t i = 0; i < bridge->aggregation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->aggregation_count > 256) {
            health_self_repair_bridge_heartbeat("health_self__loop",
                             (float)(i + 1) / (float)bridge->aggregation_count);
        }

        diagnostic_result_t* diagnostic = bridge->aggregation_batch[i].diagnostic;
        if (!diagnostic) {
            continue;
        }

        /* Find tracking record for this diagnostic */
        health_repair_tracking_t* tracking = NULL;
        for (uint32_t j = 0; j < bridge->tracking_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && bridge->tracking_count > 256) {
                health_self_repair_bridge_heartbeat("health_self__loop",
                                 (float)(j + 1) / (float)bridge->tracking_count);
            }

            if (bridge->tracking[j].diagnostic_id == diagnostic->error_id &&
                bridge->tracking[j].aggregated) {
                tracking = &bridge->tracking[j];
                break;
            }
        }

        if (!tracking) {
            /* Create new tracking if not found */
            tracking = add_tracking_record(bridge, diagnostic);
        }

        /* Submit repair */
        if (submit_repair(bridge, diagnostic, tracking) == 0) {
            submitted++;
        }

        /* Free diagnostic - self_repair copies data, doesn't take ownership */
        diagnostics_free_result(diagnostic);
        bridge->aggregation_batch[i].diagnostic = NULL;
    }

    /* Update aggregation stats */
    if (submitted > 0) {
        bridge->stats.batches_created++;
        float total_batch_size = bridge->stats.avg_batch_size * (bridge->stats.batches_created - 1);
        bridge->stats.avg_batch_size = (total_batch_size + submitted) / bridge->stats.batches_created;
    }

    /* Reset aggregation state */
    bridge->aggregation_count = 0;
    bridge->aggregation_window_start_ms = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    return submitted;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

int health_self_repair_bridge_broadcast_trigger(
    health_self_repair_bridge_t* bridge,
    uint64_t request_id,
    const diagnostic_result_t* diagnostic
) {
    if (!bridge || !diagnostic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_self_repair_bridge_broadcast_trigger: required parameter is NULL (bridge, diagnostic)");
        return -1;
    }

    /* Skip if bio-async not enabled or not registered */
    if (!bridge->bio_ctx) {
        return 0;
    }

    /* Build trigger message */
    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__broadcast_trigger", 0.0f);


    bio_msg_health_repair_trigger_t msg = {0};
    bio_msg_init_header(&msg.header,
                        BIO_MSG_HEALTH_SELF_REPAIR_TRIGGER,
                        BIO_MODULE_HEALTH_SELF_REPAIR_BRIDGE,
                        BIO_MODULE_ALL,
                        sizeof(msg) - sizeof(bio_message_header_t));

    msg.request_id = request_id;
    msg.diagnostic_id = diagnostic->error_id;
    msg.error_type = (uint32_t)diagnostic->error_type;
    msg.severity = (uint32_t)diagnostic->severity;
    msg.confidence = diagnostic->confidence;
    msg.trigger_policy = (uint32_t)bridge->config.trigger_policy;
    msg.aggregated = false;

    /* Broadcast via bio-router */
    nimcp_error_t err = bio_router_broadcast(bridge->bio_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        if (bridge->config.verbose_logging) {
            fprintf(stderr, "Failed to broadcast repair trigger: %d\n", err);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_self_repair_bridge_broadcast_trigger: validation failed");
        return -1;
    }

    return 0;
}

int health_self_repair_bridge_broadcast_outcome(
    health_self_repair_bridge_t* bridge,
    uint64_t request_id,
    health_repair_outcome_t outcome
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Skip if bio-async not enabled or not registered */
    if (!bridge->bio_ctx) {
        return 0;
    }

    /* Find tracking record for duration */
    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__broadcast_outcome", 0.0f);


    const health_repair_tracking_t* tracking = NULL;
    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t i = 0; i < bridge->tracking_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->tracking_count > 256) {
            health_self_repair_bridge_heartbeat("health_self__loop",
                             (float)(i + 1) / (float)bridge->tracking_count);
        }

        if (bridge->tracking[i].request_id == request_id) {
            tracking = &bridge->tracking[i];
            break;
        }
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Build outcome message */
    bio_msg_health_repair_outcome_t msg = {0};
    bio_msg_init_header(&msg.header,
                        BIO_MSG_HEALTH_SELF_REPAIR_OUTCOME,
                        BIO_MODULE_HEALTH_SELF_REPAIR_BRIDGE,
                        BIO_MODULE_ALL,
                        sizeof(msg) - sizeof(bio_message_header_t));

    msg.request_id = request_id;
    msg.outcome = (uint32_t)outcome;
    msg.success = (outcome == HEALTH_REPAIR_OUTCOME_SUCCESS);
    msg.duration_ms = tracking ? tracking->duration_ms : 0;

    if (tracking && outcome == HEALTH_REPAIR_OUTCOME_FAILED) {
        strncpy(msg.error_message, tracking->error_message, sizeof(msg.error_message) - 1);
    }

    /* Broadcast via bio-router */
    nimcp_error_t err = bio_router_broadcast(bridge->bio_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        if (bridge->config.verbose_logging) {
            fprintf(stderr, "Failed to broadcast repair outcome: %d\n", err);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_self_repair_bridge_broadcast_outcome: validation failed");
        return -1;
    }

    return 0;
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* health_self_repair_bridge_policy_name(health_trigger_policy_t policy) {
    switch (policy) {
        case HEALTH_TRIGGER_MANUAL: return "MANUAL";
        case HEALTH_TRIGGER_FATAL_ONLY: return "FATAL_ONLY";
        case HEALTH_TRIGGER_CRITICAL: return "CRITICAL";
        case HEALTH_TRIGGER_ERROR: return "ERROR";
        case HEALTH_TRIGGER_AUTO: return "AUTO";
        default: return "UNKNOWN";
    }
}

const char* health_self_repair_bridge_outcome_name(health_repair_outcome_t outcome) {
    switch (outcome) {
        case HEALTH_REPAIR_OUTCOME_PENDING: return "PENDING";
        case HEALTH_REPAIR_OUTCOME_SUCCESS: return "SUCCESS";
        case HEALTH_REPAIR_OUTCOME_FAILED: return "FAILED";
        case HEALTH_REPAIR_OUTCOME_SKIPPED: return "SKIPPED";
        case HEALTH_REPAIR_OUTCOME_TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN";
    }
}

const char* health_self_repair_bridge_version(void) {
    return HEALTH_SELF_REPAIR_BRIDGE_VERSION;
}

bool health_self_repair_bridge_is_ready(const health_self_repair_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    health_self_repair_bridge_heartbeat("health_self__is_ready", 0.0f);


    return bridge->initialized && bridge->magic == HEALTH_SELF_REPAIR_BRIDGE_MAGIC;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void health_self_repair_bridge_set_instance_health_agent(health_self_repair_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "health_self_repair_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int health_self_repair_bridge_training_begin(health_self_repair_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "health_self_repair_bridge_training_begin: NULL argument");
        return -1;
    }
    health_self_repair_bridge_heartbeat_instance(bridge->health_agent, "health_self_repair_bridge_training_begin", 0.0f);
    return 0;
}

int health_self_repair_bridge_training_end(health_self_repair_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "health_self_repair_bridge_training_end: NULL argument");
        return -1;
    }
    health_self_repair_bridge_heartbeat_instance(bridge->health_agent, "health_self_repair_bridge_training_end", 1.0f);
    return 0;
}

int health_self_repair_bridge_training_step(health_self_repair_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "health_self_repair_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    health_self_repair_bridge_heartbeat_instance(bridge->health_agent, "health_self_repair_bridge_training_step", progress);
    return 0;
}
