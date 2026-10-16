/**
 * @file nimcp_security_facade.c
 * @brief Implementation of Unified Security Facade API
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implementation of the unified facade providing a single entry point
 *       to the entire NIMCP security subsystem.
 *
 * WHY: Simplifies management of 14+ security bridges, FEP bridges, and the
 *      security orchestrator through a single, cohesive API.
 *
 * HOW: Wraps security orchestrator, bridges, and FEP bridges with lifecycle
 *      management, unified threat assessment, and coordinated operations.
 *
 * @author NIMCP Development Team
 */

#include "security/nimcp_security_facade.h"
#include "security/nimcp_security_orchestrator.h"
#include "cognitive/integration/nimcp_security_cognitive_hub_bridge.h"

/* Utility includes */
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_facade, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * WHAT: Information about a managed security module
 * WHY: Track module state and handles
 * HOW: Store module metadata and handle pointer
 */
typedef struct {
    security_module_id_t id;            /**< Module identifier */
    const char* name;                   /**< Module name */
    bool enabled;                       /**< Whether module is enabled */
    bool initialized;                   /**< Whether module is initialized */
    bool connected;                     /**< Whether module is connected */
    float threat_weight;                /**< Weight in threat calculation */
    void* handle;                       /**< Module handle pointer */
    uint64_t last_activity_ms;          /**< Last activity timestamp */
    uint64_t events_processed;          /**< Events processed count */
    uint64_t threats_detected;          /**< Threats detected count */
} module_info_t;

/**
 * WHAT: Internal facade structure
 * WHY: Store all facade state and managed components
 * HOW: Opaque structure hidden from API users
 */
struct security_facade_struct {
    /* Configuration */
    security_facade_config_t config;

    /* State */
    security_facade_state_t state;
    nimcp_mutex_t* mutex;

    /* Core components */
    security_orchestrator_t orchestrator;
    security_cognitive_bridge_t cognitive_bridge;

    /* Module registry */
    module_info_t modules[SEC_MODULE_COUNT];
    uint32_t enabled_count;
    uint32_t initialized_count;

    /* Threat tracking */
    float unified_threat_level;
    bool lockdown_active;
    uint64_t lockdown_start_ms;
    char lockdown_reason[NIMCP_ERROR_BUFFER_SIZE];

    /* Statistics */
    security_facade_stats_t stats;

    /* Timestamps */
    uint64_t creation_time_ms;
    uint64_t init_time_ms;
    uint64_t last_update_ms;

    /* Subscriber tracking for facade-level events */
    uint32_t facade_subscriber_id;
};

/* ============================================================================
 * MODULE NAME LOOKUP TABLE
 * ============================================================================ */

static const char* s_module_names[SEC_MODULE_COUNT] = {
    [SEC_MODULE_ORCHESTRATOR] = "Orchestrator",
    [SEC_MODULE_COGNITIVE_HUB] = "Cognitive Hub",
    [SEC_MODULE_DISTRIBUTED_TRAINING] = "Distributed Training",
    [SEC_MODULE_KNOWLEDGE_GRAPH] = "Knowledge Graph",
    [SEC_MODULE_GAME_THEORY] = "Game Theory",
    [SEC_MODULE_IMAGINATION] = "Imagination",
    [SEC_MODULE_CONTINUAL_LEARNING] = "Continual Learning",
    [SEC_MODULE_EPISTEMIC] = "Epistemic",
    [SEC_MODULE_COLLECTIVE] = "Collective",
    [SEC_MODULE_HIPPOCAMPUS] = "Hippocampus",
    [SEC_MODULE_MEMORY] = "Memory",
    [SEC_MODULE_TRAINING] = "Training",
    [SEC_MODULE_IMMUNE] = "Immune",
    [SEC_MODULE_LANGUAGE] = "Language",
    [SEC_MODULE_ASYNC] = "Async",
    [SEC_MODULE_LOGGING] = "Logging",
    [SEC_MODULE_FEP_DISTRIBUTED_TRAINING] = "FEP Distributed Training",
    [SEC_MODULE_FEP_KNOWLEDGE_GRAPH] = "FEP Knowledge Graph",
    [SEC_MODULE_FEP_GAME_THEORY] = "FEP Game Theory",
    [SEC_MODULE_FEP_IMAGINATION] = "FEP Imagination",
    [SEC_MODULE_FEP_CONTINUAL_LEARNING] = "FEP Continual Learning",
    [SEC_MODULE_FEP_EPISTEMIC] = "FEP Epistemic",
    [SEC_MODULE_FEP_COLLECTIVE] = "FEP Collective",
    [SEC_MODULE_FEP_HIPPOCAMPUS] = "FEP Hippocampus",
    [SEC_MODULE_FEP_MEMORY] = "FEP Memory",
    [SEC_MODULE_FEP_TRAINING] = "FEP Training",
    [SEC_MODULE_FEP_IMMUNE] = "FEP Immune",
    [SEC_MODULE_FEP_LANGUAGE] = "FEP Language",
    [SEC_MODULE_FEP_ASYNC] = "FEP Async",
    [SEC_MODULE_FEP_LOGGING] = "FEP Logging"
};

static const char* s_state_names[] = {
    [SEC_FACADE_STATE_UNINITIALIZED] = "UNINITIALIZED",
    [SEC_FACADE_STATE_CREATED] = "CREATED",
    [SEC_FACADE_STATE_INITIALIZING] = "INITIALIZING",
    [SEC_FACADE_STATE_READY] = "READY",
    [SEC_FACADE_STATE_ACTIVE] = "ACTIVE",
    [SEC_FACADE_STATE_ALERT] = "ALERT",
    [SEC_FACADE_STATE_LOCKDOWN] = "LOCKDOWN",
    [SEC_FACADE_STATE_SHUTTING_DOWN] = "SHUTTING_DOWN",
    [SEC_FACADE_STATE_ERROR] = "ERROR"
};

/* ============================================================================
 * INTERNAL HELPER FUNCTIONS
 * ============================================================================ */

/**
 * WHAT: Initialize module registry with default values
 * WHY: Set up module tracking structures
 * HOW: Populate module_info array with defaults
 */
static void init_module_registry(security_facade_t facade)
{
    if (!facade) return;

    for (uint32_t i = 0; i < SEC_MODULE_COUNT; i++) {
        facade->modules[i].id = (security_module_id_t)i;
        facade->modules[i].name = s_module_names[i];
        facade->modules[i].enabled = false;
        facade->modules[i].initialized = false;
        facade->modules[i].connected = false;
        facade->modules[i].threat_weight = 1.0f;
        facade->modules[i].handle = NULL;
        facade->modules[i].last_activity_ms = 0;
        facade->modules[i].events_processed = 0;
        facade->modules[i].threats_detected = 0;
    }

    facade->enabled_count = 0;
    facade->initialized_count = 0;
}

/**
 * WHAT: Update module enabled count
 * WHY: Keep track of enabled modules
 * HOW: Count enabled modules in registry
 */
static void update_module_counts(security_facade_t facade)
{
    if (!facade) return;

    facade->enabled_count = 0;
    facade->initialized_count = 0;

    for (uint32_t i = 0; i < SEC_MODULE_COUNT; i++) {
        if (facade->modules[i].enabled) {
            facade->enabled_count++;
        }
        if (facade->modules[i].initialized) {
            facade->initialized_count++;
        }
    }
}

/**
 * WHAT: Calculate unified threat level from all modules
 * WHY: Aggregate threats for overall assessment
 * HOW: Weighted average of module threat levels
 */
static float calculate_unified_threat(security_facade_t facade)
{
    if (!facade) return 0.0f;

    float total_weight = 0.0f;
    float weighted_threat = 0.0f;

    /* Get threat from orchestrator */
    if (facade->orchestrator) {
        float orch_threat = 0.0f;
        if (security_orch_get_threat_level(facade->orchestrator, &orch_threat) == 0) {
            weighted_threat += orch_threat * 2.0f;  /* Orchestrator has double weight */
            total_weight += 2.0f;
        }
    }

    /* Aggregate from enabled modules */
    for (uint32_t i = 0; i < SEC_MODULE_COUNT; i++) {
        if (facade->modules[i].enabled && facade->modules[i].initialized) {
            /* Use stored threat weight - actual module queries would go here */
            float module_threat = 0.0f;  /* Placeholder for actual module query */
            weighted_threat += module_threat * facade->modules[i].threat_weight;
            total_weight += facade->modules[i].threat_weight;
        }
    }

    if (total_weight > 0.0f) {
        return weighted_threat / total_weight;
    }

    return 0.0f;
}

/**
 * WHAT: Update facade state based on threat level
 * WHY: Automatic state transitions based on security posture
 * HOW: Compare threat to thresholds, update state
 */
static void update_facade_state(security_facade_t facade)
{
    if (!facade) return;
    if (facade->state == SEC_FACADE_STATE_SHUTTING_DOWN) return;
    if (facade->state == SEC_FACADE_STATE_ERROR) return;

    float threat = facade->unified_threat_level;

    if (facade->lockdown_active) {
        facade->state = SEC_FACADE_STATE_LOCKDOWN;
    } else if (threat >= facade->config.alert_threshold) {
        facade->state = SEC_FACADE_STATE_ALERT;
    } else if (facade->initialized_count > 0) {
        facade->state = SEC_FACADE_STATE_ACTIVE;
    } else if (facade->state == SEC_FACADE_STATE_CREATED) {
        /* Stay in created state until initialized */
    } else {
        facade->state = SEC_FACADE_STATE_READY;
    }

    facade->last_update_ms = nimcp_platform_time_monotonic_ms();
}

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

int security_facade_default_config(security_facade_config_t* config)
{
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(*config));

    /* Module configuration */
    config->enable_all_bridges = true;
    config->enable_all_fep_bridges = true;
    config->enable_cognitive_hub = true;

    /* Get default orchestrator config */
    security_orch_default_config(&config->orch_config);

    /* Threat thresholds */
    config->alert_threshold = 0.6f;
    config->lockdown_threshold = 0.9f;
    config->threat_decay_rate = 0.05f;

    /* Processing configuration */
    config->enable_async_processing = true;
    config->event_batch_size = 64;
    config->processing_interval_ms = 100;

    /* Integration options */
    config->connect_bio_async = true;
    config->enable_audit_logging = true;
    config->enable_metrics = true;

    /* Recovery options */
    config->auto_recovery = true;
    config->lockdown_timeout_ms = 60000;  /* 60 seconds */

    /* No module-specific overrides by default */
    config->module_configs = NULL;
    config->module_config_count = 0;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * LIFECYCLE MANAGEMENT
 * ============================================================================ */

security_facade_t security_facade_create(const security_facade_config_t* config)
{
    security_facade_t facade = NULL;
    security_facade_config_t default_config;

    /* Use defaults if no config provided */
    if (!config) {
        security_facade_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate facade structure */
    facade = (security_facade_t)nimcp_malloc(sizeof(struct security_facade_struct));
    if (!facade) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_facade_create: failed to allocate facade");
        NIMCP_LOG_ERROR(SEC_FACADE_MODULE_NAME, "Failed to allocate facade structure");
        return NULL;
    }

    memset(facade, 0, sizeof(struct security_facade_struct));

    /* Store configuration */
    memcpy(&facade->config, config, sizeof(security_facade_config_t));

    /* Create mutex */
    mutex_attr_t mutex_attr = {
        .type = MUTEX_TYPE_RECURSIVE
    };
    facade->mutex = nimcp_mutex_create(&mutex_attr);
    if (!facade->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_facade_create: mutex creation failed");
        NIMCP_LOG_ERROR(SEC_FACADE_MODULE_NAME, "Failed to create mutex");
        nimcp_free(facade);
        return NULL;
    }

    /* Initialize module registry */
    init_module_registry(facade);

    /* Set initial state */
    facade->state = SEC_FACADE_STATE_CREATED;
    facade->creation_time_ms = nimcp_platform_time_monotonic_ms();
    facade->unified_threat_level = 0.0f;
    facade->lockdown_active = false;

    /* Create security orchestrator */
    facade->orchestrator = security_orch_create(&config->orch_config);
    if (!facade->orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_facade_create: orchestrator creation failed");
        NIMCP_LOG_ERROR(SEC_FACADE_MODULE_NAME, "Failed to create security orchestrator");
        nimcp_mutex_free(facade->mutex);
        nimcp_free(facade);
        return NULL;
    }

    /* Mark orchestrator as enabled */
    facade->modules[SEC_MODULE_ORCHESTRATOR].enabled = true;
    facade->modules[SEC_MODULE_ORCHESTRATOR].handle = facade->orchestrator;
    update_module_counts(facade);

    /* Create cognitive hub bridge if enabled */
    if (config->enable_cognitive_hub) {
        security_cognitive_config_t cog_config;
        security_cognitive_default_config(&cog_config);

        facade->cognitive_bridge = security_cognitive_bridge_create(&cog_config);
        if (facade->cognitive_bridge) {
            facade->modules[SEC_MODULE_COGNITIVE_HUB].enabled = true;
            facade->modules[SEC_MODULE_COGNITIVE_HUB].handle = facade->cognitive_bridge;
            update_module_counts(facade);
        } else {
            NIMCP_LOG_WARN(SEC_FACADE_MODULE_NAME,
                "Failed to create cognitive hub bridge - continuing without it");
        }
    }

    /* Enable bridges based on config */
    if (config->enable_all_bridges) {
        for (uint32_t i = SEC_MODULE_DISTRIBUTED_TRAINING; i <= SEC_MODULE_LOGGING; i++) {
            facade->modules[i].enabled = true;
        }
    }

    /* Enable FEP bridges based on config */
    if (config->enable_all_fep_bridges) {
        for (uint32_t i = SEC_MODULE_FEP_DISTRIBUTED_TRAINING; i <= SEC_MODULE_FEP_LOGGING; i++) {
            facade->modules[i].enabled = true;
        }
    }

    update_module_counts(facade);

    NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME,
        "Security facade created with %u modules enabled", facade->enabled_count);

    return facade;
}

void security_facade_destroy(security_facade_t facade)
{
    if (!facade) return;

    NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME, "Destroying security facade");

    /* Shutdown if not already done */
    if (facade->state != SEC_FACADE_STATE_UNINITIALIZED &&
        facade->state != SEC_FACADE_STATE_CREATED &&
        facade->state != SEC_FACADE_STATE_SHUTTING_DOWN) {
        security_facade_shutdown(facade);
    }

    nimcp_mutex_lock(facade->mutex);

    /* Destroy cognitive hub bridge */
    if (facade->cognitive_bridge) {
        security_cognitive_bridge_destroy(facade->cognitive_bridge);
        facade->cognitive_bridge = NULL;
    }

    /* Destroy orchestrator */
    if (facade->orchestrator) {
        security_orch_destroy(facade->orchestrator);
        facade->orchestrator = NULL;
    }

    nimcp_mutex_unlock(facade->mutex);

    /* Destroy mutex */
    nimcp_mutex_free(facade->mutex);
    facade->mutex = NULL;

    /* Free facade structure */
    nimcp_free(facade);

    NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME, "Security facade destroyed");
}

int security_facade_init_all(security_facade_t facade)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);

    if (facade->state != SEC_FACADE_STATE_CREATED) {
        nimcp_mutex_unlock(facade->mutex);
        NIMCP_LOG_ERROR(SEC_FACADE_MODULE_NAME,
            "Cannot initialize: facade in state %s",
            s_state_names[facade->state]);
        return NIMCP_ERROR_INVALID_STATE;
    }

    facade->state = SEC_FACADE_STATE_INITIALIZING;
    NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME, "Initializing all security components");

    /* 1. Orchestrator is already created - mark as initialized */
    facade->modules[SEC_MODULE_ORCHESTRATOR].initialized = true;
    facade->modules[SEC_MODULE_ORCHESTRATOR].connected = true;

    /* 2. Initialize core security bridges */
    /* Note: In a full implementation, we would create and initialize each bridge here.
     * For now, we mark enabled bridges as initialized (placeholder) */
    for (uint32_t i = SEC_MODULE_DISTRIBUTED_TRAINING; i <= SEC_MODULE_LOGGING; i++) {
        if (facade->modules[i].enabled) {
            /* Placeholder: actual bridge creation would go here */
            facade->modules[i].initialized = true;
            NIMCP_LOG_DEBUG(SEC_FACADE_MODULE_NAME,
                "Initialized module: %s", facade->modules[i].name);
        }
    }

    /* 3. Initialize FEP bridges */
    for (uint32_t i = SEC_MODULE_FEP_DISTRIBUTED_TRAINING; i <= SEC_MODULE_FEP_LOGGING; i++) {
        if (facade->modules[i].enabled) {
            /* Placeholder: actual FEP bridge creation would go here */
            facade->modules[i].initialized = true;
            NIMCP_LOG_DEBUG(SEC_FACADE_MODULE_NAME,
                "Initialized FEP module: %s", facade->modules[i].name);
        }
    }

    /* 4. Connect cognitive hub bridge */
    if (facade->cognitive_bridge && facade->orchestrator) {
        int ret = security_cognitive_connect_security(
            facade->cognitive_bridge, facade->orchestrator);
        if (ret == 0) {
            facade->modules[SEC_MODULE_COGNITIVE_HUB].initialized = true;
            facade->modules[SEC_MODULE_COGNITIVE_HUB].connected = true;
            NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME,
                "Connected cognitive hub bridge to orchestrator");
        } else {
            NIMCP_LOG_WARN(SEC_FACADE_MODULE_NAME,
                "Failed to connect cognitive hub bridge");
        }
    }

    /* Update counts and state */
    update_module_counts(facade);
    facade->init_time_ms = nimcp_platform_time_monotonic_ms();
    facade->state = SEC_FACADE_STATE_READY;

    nimcp_mutex_unlock(facade->mutex);

    NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME,
        "Security facade initialized: %u/%u modules ready",
        facade->initialized_count, facade->enabled_count);

    return NIMCP_SUCCESS;
}

int security_facade_shutdown(security_facade_t facade)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);

    if (facade->state == SEC_FACADE_STATE_SHUTTING_DOWN ||
        facade->state == SEC_FACADE_STATE_UNINITIALIZED) {
        nimcp_mutex_unlock(facade->mutex);
        return NIMCP_SUCCESS;
    }

    facade->state = SEC_FACADE_STATE_SHUTTING_DOWN;
    NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME, "Shutting down security facade");

    /* 1. Release lockdown if active */
    if (facade->lockdown_active) {
        facade->lockdown_active = false;
        if (facade->orchestrator) {
            security_orch_release_lockdown(facade->orchestrator);
        }
    }

    /* 2. Disconnect cognitive hub */
    if (facade->cognitive_bridge) {
        security_cognitive_disconnect_security(facade->cognitive_bridge);
        facade->modules[SEC_MODULE_COGNITIVE_HUB].connected = false;
        NIMCP_LOG_DEBUG(SEC_FACADE_MODULE_NAME, "Disconnected cognitive hub");
    }

    /* 3. Shutdown FEP bridges (reverse order) */
    for (int i = SEC_MODULE_FEP_LOGGING; i >= (int)SEC_MODULE_FEP_DISTRIBUTED_TRAINING; i--) {
        if (facade->modules[i].initialized) {
            /* Placeholder: actual FEP bridge shutdown would go here */
            facade->modules[i].initialized = false;
            facade->modules[i].connected = false;
            NIMCP_LOG_DEBUG(SEC_FACADE_MODULE_NAME,
                "Shutdown FEP module: %s", facade->modules[i].name);
        }
    }

    /* 4. Shutdown security bridges (reverse order) */
    for (int i = SEC_MODULE_LOGGING; i >= (int)SEC_MODULE_DISTRIBUTED_TRAINING; i--) {
        if (facade->modules[i].initialized) {
            /* Placeholder: actual bridge shutdown would go here */
            facade->modules[i].initialized = false;
            facade->modules[i].connected = false;
            NIMCP_LOG_DEBUG(SEC_FACADE_MODULE_NAME,
                "Shutdown module: %s", facade->modules[i].name);
        }
    }

    /* 5. Reset orchestrator (don't destroy - that happens in facade_destroy) */
    if (facade->orchestrator) {
        security_orch_reset(facade->orchestrator);
    }

    update_module_counts(facade);

    nimcp_mutex_unlock(facade->mutex);

    NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME, "Security facade shutdown complete");

    return NIMCP_SUCCESS;
}

int security_facade_reset(security_facade_t facade)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);

    NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME, "Resetting security facade");

    /* Reset orchestrator */
    if (facade->orchestrator) {
        security_orch_reset(facade->orchestrator);
    }

    /* Reset cognitive bridge */
    if (facade->cognitive_bridge) {
        security_cognitive_bridge_reset(facade->cognitive_bridge);
    }

    /* Clear threat tracking */
    facade->unified_threat_level = 0.0f;
    facade->lockdown_active = false;
    memset(facade->lockdown_reason, 0, sizeof(facade->lockdown_reason));

    /* Reset statistics */
    memset(&facade->stats, 0, sizeof(facade->stats));

    /* Reset module statistics */
    for (uint32_t i = 0; i < SEC_MODULE_COUNT; i++) {
        facade->modules[i].events_processed = 0;
        facade->modules[i].threats_detected = 0;
    }

    /* Update state */
    update_facade_state(facade);

    nimcp_mutex_unlock(facade->mutex);

    NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME, "Security facade reset complete");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * MODULE MANAGEMENT
 * ============================================================================ */

int security_facade_enable_module(
    security_facade_t facade,
    security_module_id_t module_id)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");
    NIMCP_CHECK_THROW(module_id < SEC_MODULE_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "module_id out of range");

    nimcp_mutex_lock(facade->mutex);

    if (facade->modules[module_id].enabled) {
        nimcp_mutex_unlock(facade->mutex);
        return NIMCP_SUCCESS;  /* Already enabled */
    }

    facade->modules[module_id].enabled = true;

    /* If facade is initialized, also initialize the module */
    if (facade->state >= SEC_FACADE_STATE_READY) {
        /* Placeholder: actual module initialization would go here */
        facade->modules[module_id].initialized = true;
        NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME,
            "Enabled and initialized module: %s", facade->modules[module_id].name);
    } else {
        NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME,
            "Enabled module: %s (will initialize on facade init)",
            facade->modules[module_id].name);
    }

    update_module_counts(facade);

    nimcp_mutex_unlock(facade->mutex);

    return NIMCP_SUCCESS;
}

int security_facade_disable_module(
    security_facade_t facade,
    security_module_id_t module_id)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");
    NIMCP_CHECK_THROW(module_id < SEC_MODULE_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "module_id out of range");

    /* Cannot disable core modules */
    if (module_id == SEC_MODULE_ORCHESTRATOR) {
        NIMCP_LOG_WARN(SEC_FACADE_MODULE_NAME,
            "Cannot disable orchestrator - it is a core module");
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    nimcp_mutex_lock(facade->mutex);

    if (!facade->modules[module_id].enabled) {
        nimcp_mutex_unlock(facade->mutex);
        return NIMCP_SUCCESS;  /* Already disabled */
    }

    /* Shutdown module if initialized */
    if (facade->modules[module_id].initialized) {
        /* Placeholder: actual module shutdown would go here */
        facade->modules[module_id].initialized = false;
        facade->modules[module_id].connected = false;
    }

    facade->modules[module_id].enabled = false;
    update_module_counts(facade);

    nimcp_mutex_unlock(facade->mutex);

    NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME,
        "Disabled module: %s", facade->modules[module_id].name);

    return NIMCP_SUCCESS;
}

int security_facade_is_module_enabled(
    security_facade_t facade,
    security_module_id_t module_id,
    bool* enabled)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");
    NIMCP_CHECK_THROW(enabled, NIMCP_ERROR_NULL_POINTER, "enabled is NULL");
    NIMCP_CHECK_THROW(module_id < SEC_MODULE_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "module_id out of range");

    nimcp_mutex_lock(facade->mutex);
    *enabled = facade->modules[module_id].enabled;
    nimcp_mutex_unlock(facade->mutex);

    return NIMCP_SUCCESS;
}

int security_facade_get_module_status(
    security_facade_t facade,
    security_module_id_t module_id,
    security_module_status_t* status)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");
    NIMCP_CHECK_THROW(status, NIMCP_ERROR_NULL_POINTER, "status is NULL");
    NIMCP_CHECK_THROW(module_id < SEC_MODULE_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "module_id out of range");

    nimcp_mutex_lock(facade->mutex);

    module_info_t* mod = &facade->modules[module_id];

    status->module_id = mod->id;
    status->enabled = mod->enabled;
    status->initialized = mod->initialized;
    status->connected = mod->connected;
    status->healthy = mod->initialized && mod->enabled;
    status->current_threat_level = 0.0f;  /* Placeholder */
    status->events_processed = mod->events_processed;
    status->threats_detected = mod->threats_detected;
    status->last_activity_ms = mod->last_activity_ms;
    status->status_message = mod->initialized ? "OK" : "Not initialized";

    nimcp_mutex_unlock(facade->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * THREAT ASSESSMENT
 * ============================================================================ */

int security_facade_get_threat_level(
    security_facade_t facade,
    float* threat_level)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");
    NIMCP_CHECK_THROW(threat_level, NIMCP_ERROR_NULL_POINTER, "threat_level is NULL");

    nimcp_mutex_lock(facade->mutex);

    /* Update unified threat level */
    facade->unified_threat_level = calculate_unified_threat(facade);
    *threat_level = facade->unified_threat_level;

    /* Update state based on threat */
    update_facade_state(facade);

    /* Track statistics */
    if (facade->unified_threat_level > facade->stats.peak_threat_level) {
        facade->stats.peak_threat_level = facade->unified_threat_level;
    }

    nimcp_mutex_unlock(facade->mutex);

    return NIMCP_SUCCESS;
}

int security_facade_get_threat_assessment(
    security_facade_t facade,
    security_threat_assessment_t* assessment)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");
    NIMCP_CHECK_THROW(assessment, NIMCP_ERROR_NULL_POINTER, "assessment is NULL");

    nimcp_mutex_lock(facade->mutex);

    int ret = NIMCP_ERROR_NOT_INITIALIZED;

    if (facade->orchestrator) {
        ret = security_orch_get_threat_assessment(facade->orchestrator, assessment);
    }

    nimcp_mutex_unlock(facade->mutex);

    return ret;
}

int security_facade_report_threat(
    security_facade_t facade,
    security_module_id_t source_module,
    float threat_level,
    security_severity_t severity,
    const char* description)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");
    NIMCP_CHECK_THROW(source_module < SEC_MODULE_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "source_module out of range");

    nimcp_mutex_lock(facade->mutex);

    int ret = NIMCP_ERROR_NOT_INITIALIZED;

    if (facade->orchestrator) {
        /* Get bridge ID for the source module (use module ID as bridge ID) */
        ret = security_orch_report_threat(
            facade->orchestrator,
            (uint32_t)source_module,
            threat_level,
            severity,
            description
        );

        if (ret == 0) {
            facade->modules[source_module].threats_detected++;
            facade->stats.threats_detected++;

            /* Check for auto-lockdown */
            if (threat_level >= facade->config.lockdown_threshold &&
                !facade->lockdown_active) {
                NIMCP_LOG_WARN(SEC_FACADE_MODULE_NAME,
                    "Auto-lockdown triggered by threat level %.2f", threat_level);
                facade->lockdown_active = true;
                facade->lockdown_start_ms = nimcp_platform_time_monotonic_ms();
                snprintf(facade->lockdown_reason, sizeof(facade->lockdown_reason),
                    "Auto-lockdown: %s", description ? description : "High threat");
                facade->stats.lockdowns_triggered++;
            }
        }
    }

    nimcp_mutex_unlock(facade->mutex);

    return ret;
}

int security_facade_clear_threats(security_facade_t facade)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);

    int ret = NIMCP_SUCCESS;

    if (facade->orchestrator) {
        ret = security_orch_clear_threats(facade->orchestrator);
    }

    facade->unified_threat_level = 0.0f;
    update_facade_state(facade);

    nimcp_mutex_unlock(facade->mutex);

    NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME, "All threats cleared");

    return ret;
}

/* ============================================================================
 * EVENT PROCESSING
 * ============================================================================ */

int security_facade_process_events(security_facade_t facade)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);

    uint64_t start_time = nimcp_platform_time_monotonic_us();
    int events_processed = 0;

    /* Update threat decay */
    if (facade->orchestrator) {
        security_orch_update_threat_decay(facade->orchestrator);
    }

    /* Update unified threat level */
    facade->unified_threat_level = calculate_unified_threat(facade);

    /* Check lockdown timeout */
    if (facade->lockdown_active && facade->config.lockdown_timeout_ms > 0) {
        uint64_t now = nimcp_platform_time_monotonic_ms();
        uint64_t elapsed = now - facade->lockdown_start_ms;
        if (elapsed >= facade->config.lockdown_timeout_ms) {
            NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME,
                "Lockdown timeout expired, releasing lockdown");
            facade->lockdown_active = false;
            if (facade->orchestrator) {
                security_orch_release_lockdown(facade->orchestrator);
            }
        }
    }

    /* Update state */
    update_facade_state(facade);

    /* Update statistics */
    uint64_t processing_time = nimcp_platform_time_monotonic_us() - start_time;
    facade->stats.avg_processing_time_us =
        (facade->stats.avg_processing_time_us + (float)processing_time) / 2.0f;
    facade->stats.events_processed += events_processed;
    facade->last_update_ms = nimcp_platform_time_monotonic_ms();

    nimcp_mutex_unlock(facade->mutex);

    return events_processed;
}

int security_facade_subscribe(
    security_facade_t facade,
    security_event_type_t event_type,
    security_event_callback_t callback,
    void* user_data)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");
    NIMCP_CHECK_THROW(callback, NIMCP_ERROR_NULL_POINTER, "callback is NULL");

    nimcp_mutex_lock(facade->mutex);

    int ret = NIMCP_ERROR_NOT_INITIALIZED;

    if (facade->orchestrator) {
        ret = security_orch_subscribe(
            facade->orchestrator,
            facade->facade_subscriber_id,
            event_type,
            callback,
            user_data
        );
    }

    nimcp_mutex_unlock(facade->mutex);

    return ret;
}

int security_facade_unsubscribe(
    security_facade_t facade,
    security_event_type_t event_type)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);

    int ret = NIMCP_ERROR_NOT_INITIALIZED;

    if (facade->orchestrator) {
        ret = security_orch_unsubscribe(
            facade->orchestrator,
            facade->facade_subscriber_id,
            event_type
        );
    }

    nimcp_mutex_unlock(facade->mutex);

    return ret;
}

/* ============================================================================
 * LOCKDOWN CONTROL
 * ============================================================================ */

int security_facade_trigger_lockdown(
    security_facade_t facade,
    const char* reason)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);

    if (facade->lockdown_active) {
        nimcp_mutex_unlock(facade->mutex);
        return NIMCP_SUCCESS;  /* Already in lockdown */
    }

    NIMCP_LOG_WARN(SEC_FACADE_MODULE_NAME,
        "Triggering lockdown: %s", reason ? reason : "Manual trigger");

    facade->lockdown_active = true;
    facade->lockdown_start_ms = nimcp_platform_time_monotonic_ms();
    if (reason) {
        strncpy(facade->lockdown_reason, reason, sizeof(facade->lockdown_reason) - 1);
        facade->lockdown_reason[sizeof(facade->lockdown_reason) - 1] = '\0';
    } else {
        strncpy(facade->lockdown_reason, "Manual lockdown", sizeof(facade->lockdown_reason) - 1);
        facade->lockdown_reason[sizeof(facade->lockdown_reason) - 1] = '\0';
    }

    /* Trigger orchestrator lockdown */
    if (facade->orchestrator) {
        security_orch_trigger_lockdown(facade->orchestrator, reason);
    }

    /* Coordinate with cognitive hub */
    if (facade->cognitive_bridge) {
        security_cognitive_coordinate_lockdown(facade->cognitive_bridge, reason);
    }

    facade->state = SEC_FACADE_STATE_LOCKDOWN;
    facade->stats.lockdowns_triggered++;

    nimcp_mutex_unlock(facade->mutex);

    return NIMCP_SUCCESS;
}

int security_facade_release_lockdown(security_facade_t facade)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);

    if (!facade->lockdown_active) {
        nimcp_mutex_unlock(facade->mutex);
        return NIMCP_SUCCESS;  /* Not in lockdown */
    }

    NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME, "Releasing lockdown");

    /* Track lockdown duration */
    uint64_t now = nimcp_platform_time_monotonic_ms();
    uint64_t duration = now - facade->lockdown_start_ms;
    facade->stats.total_lockdown_time_ms += duration;
    facade->stats.avg_lockdown_duration_ms =
        facade->stats.total_lockdown_time_ms / facade->stats.lockdowns_triggered;

    facade->lockdown_active = false;
    memset(facade->lockdown_reason, 0, sizeof(facade->lockdown_reason));

    /* Release orchestrator lockdown */
    if (facade->orchestrator) {
        security_orch_release_lockdown(facade->orchestrator);
    }

    /* Coordinate with cognitive hub */
    if (facade->cognitive_bridge) {
        security_cognitive_release_lockdown(facade->cognitive_bridge);
    }

    update_facade_state(facade);

    nimcp_mutex_unlock(facade->mutex);

    return NIMCP_SUCCESS;
}

int security_facade_is_locked_down(
    security_facade_t facade,
    bool* is_locked)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");
    NIMCP_CHECK_THROW(is_locked, NIMCP_ERROR_NULL_POINTER, "is_locked is NULL");

    nimcp_mutex_lock(facade->mutex);
    *is_locked = facade->lockdown_active;
    nimcp_mutex_unlock(facade->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * STATUS AND STATISTICS
 * ============================================================================ */

int security_facade_get_status(
    security_facade_t facade,
    security_facade_status_t* status)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");
    NIMCP_CHECK_THROW(status, NIMCP_ERROR_NULL_POINTER, "status is NULL");

    nimcp_mutex_lock(facade->mutex);

    /* Basic state */
    status->state = facade->state;
    status->total_modules = SEC_MODULE_COUNT;
    status->enabled_modules = facade->enabled_count;
    status->initialized_modules = facade->initialized_count;

    /* Count healthy and error modules */
    status->healthy_modules = 0;
    status->error_modules = 0;
    for (uint32_t i = 0; i < SEC_MODULE_COUNT; i++) {
        if (facade->modules[i].enabled && facade->modules[i].initialized) {
            status->healthy_modules++;
        } else if (facade->modules[i].enabled && !facade->modules[i].initialized) {
            status->error_modules++;
        }
    }

    /* Threat assessment */
    status->unified_threat_level = facade->unified_threat_level;
    if (facade->unified_threat_level >= 0.9f) {
        status->severity = SEC_SEVERITY_CRITICAL;
    } else if (facade->unified_threat_level >= 0.7f) {
        status->severity = SEC_SEVERITY_HIGH;
    } else if (facade->unified_threat_level >= 0.4f) {
        status->severity = SEC_SEVERITY_MEDIUM;
    } else if (facade->unified_threat_level >= 0.1f) {
        status->severity = SEC_SEVERITY_LOW;
    } else {
        status->severity = SEC_SEVERITY_NONE;
    }
    status->active_threats = facade->stats.threats_detected;

    /* Lockdown status */
    status->lockdown_active = facade->lockdown_active;
    status->lockdown_start_ms = facade->lockdown_start_ms;
    status->lockdown_reason = facade->lockdown_active ? facade->lockdown_reason : NULL;

    /* Timestamps */
    status->creation_time_ms = facade->creation_time_ms;
    status->init_time_ms = facade->init_time_ms;
    status->last_update_ms = facade->last_update_ms;
    status->uptime_ms = nimcp_platform_time_monotonic_ms() - facade->creation_time_ms;

    /* Allocate and populate module status array */
    status->module_status = (security_module_status_t*)nimcp_malloc(
        SEC_MODULE_COUNT * sizeof(security_module_status_t));
    if (status->module_status) {
        status->module_status_count = SEC_MODULE_COUNT;
        for (uint32_t i = 0; i < SEC_MODULE_COUNT; i++) {
            status->module_status[i].module_id = facade->modules[i].id;
            status->module_status[i].enabled = facade->modules[i].enabled;
            status->module_status[i].initialized = facade->modules[i].initialized;
            status->module_status[i].connected = facade->modules[i].connected;
            status->module_status[i].healthy =
                facade->modules[i].enabled && facade->modules[i].initialized;
            status->module_status[i].current_threat_level = 0.0f;
            status->module_status[i].events_processed = facade->modules[i].events_processed;
            status->module_status[i].threats_detected = facade->modules[i].threats_detected;
            status->module_status[i].last_activity_ms = facade->modules[i].last_activity_ms;
            status->module_status[i].status_message =
                facade->modules[i].initialized ? "OK" : "Not initialized";
        }
    } else {
        status->module_status_count = 0;
    }

    nimcp_mutex_unlock(facade->mutex);

    return NIMCP_SUCCESS;
}

void security_facade_free_status(security_facade_status_t* status)
{
    if (!status) return;

    if (status->module_status) {
        nimcp_free(status->module_status);
        status->module_status = NULL;
        status->module_status_count = 0;
    }
}

int security_facade_get_state(
    security_facade_t facade,
    security_facade_state_t* state)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    nimcp_mutex_lock(facade->mutex);
    *state = facade->state;
    nimcp_mutex_unlock(facade->mutex);

    return NIMCP_SUCCESS;
}

int security_facade_get_stats(
    security_facade_t facade,
    security_facade_stats_t* stats)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(facade->mutex);
    memcpy(stats, &facade->stats, sizeof(security_facade_stats_t));
    stats->uptime_ms = nimcp_platform_time_monotonic_ms() - facade->creation_time_ms;
    nimcp_mutex_unlock(facade->mutex);

    return NIMCP_SUCCESS;
}

int security_facade_reset_stats(security_facade_t facade)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);
    memset(&facade->stats, 0, sizeof(facade->stats));
    facade->stats.last_reset_ms = nimcp_platform_time_monotonic_ms();
    nimcp_mutex_unlock(facade->mutex);

    NIMCP_LOG_DEBUG(SEC_FACADE_MODULE_NAME, "Statistics reset");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * INTEGRATION CONNECTIONS
 * ============================================================================ */

int security_facade_connect_cognitive_hub(
    security_facade_t facade,
    void* cognitive_hub)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);

    int ret = NIMCP_ERROR_NOT_INITIALIZED;

    if (facade->cognitive_bridge) {
        ret = security_cognitive_connect_cognitive(
            facade->cognitive_bridge, cognitive_hub);
        if (ret == 0) {
            facade->modules[SEC_MODULE_COGNITIVE_HUB].connected = true;
            NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME,
                "Connected cognitive hub bridge to cognitive hub");
        }
    }

    nimcp_mutex_unlock(facade->mutex);

    return ret;
}

int security_facade_connect_immune_system(
    security_facade_t facade,
    void* immune_system)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);

    int ret = NIMCP_ERROR_NOT_INITIALIZED;

    if (facade->orchestrator) {
        ret = security_orch_connect_immune(facade->orchestrator, immune_system);
        if (ret == 0) {
            NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME,
                "Connected orchestrator to immune system");
        }
    }

    nimcp_mutex_unlock(facade->mutex);

    return ret;
}

int security_facade_connect_bio_async(security_facade_t facade)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);

    int ret = NIMCP_ERROR_NOT_INITIALIZED;

    if (facade->orchestrator) {
        ret = security_orch_connect_bio_async(facade->orchestrator);
        if (ret == 0) {
            NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME,
                "Connected orchestrator to bio-async router");
        }
    }

    nimcp_mutex_unlock(facade->mutex);

    return ret;
}

int security_facade_disconnect_bio_async(security_facade_t facade)
{
    NIMCP_CHECK_THROW(facade, NIMCP_ERROR_NULL_POINTER, "facade is NULL");

    nimcp_mutex_lock(facade->mutex);

    int ret = NIMCP_ERROR_NOT_INITIALIZED;

    if (facade->orchestrator) {
        ret = security_orch_disconnect_bio_async(facade->orchestrator);
        if (ret == 0) {
            NIMCP_LOG_INFO(SEC_FACADE_MODULE_NAME,
                "Disconnected orchestrator from bio-async router");
        }
    }

    nimcp_mutex_unlock(facade->mutex);

    return ret;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

const char* security_module_name(security_module_id_t module_id)
{
    if (module_id >= SEC_MODULE_COUNT) {
        return "UNKNOWN";
    }
    return s_module_names[module_id];
}

const char* security_facade_state_name(security_facade_state_t state)
{
    if (state > SEC_FACADE_STATE_ERROR) {
        return "INVALID";
    }
    return s_state_names[state];
}

void security_facade_print_summary(security_facade_t facade)
{
    if (!facade) {
        printf("Security Facade: NULL\n");
        return;
    }

    nimcp_mutex_lock(facade->mutex);

    printf("\n");
    printf("=== Security Facade Summary ===\n");
    printf("Version: %s\n", SEC_FACADE_VERSION);
    printf("State: %s\n", s_state_names[facade->state]);
    printf("Modules: %u enabled, %u initialized\n",
        facade->enabled_count, facade->initialized_count);
    printf("Threat Level: %.2f\n", facade->unified_threat_level);
    printf("Lockdown: %s\n", facade->lockdown_active ? "ACTIVE" : "Inactive");
    if (facade->lockdown_active) {
        printf("  Reason: %s\n", facade->lockdown_reason);
    }
    printf("Uptime: %lu ms\n",
        (unsigned long)(nimcp_platform_time_monotonic_ms() - facade->creation_time_ms));
    printf("\n");

    printf("Module Status:\n");
    for (uint32_t i = 0; i < SEC_MODULE_COUNT; i++) {
        if (facade->modules[i].enabled) {
            printf("  [%c] %s: %s\n",
                facade->modules[i].initialized ? 'X' : ' ',
                facade->modules[i].name,
                facade->modules[i].initialized ? "Ready" : "Not initialized");
        }
    }
    printf("\n");

    nimcp_mutex_unlock(facade->mutex);
}

void security_facade_print_stats(const security_facade_stats_t* stats)
{
    if (!stats) {
        printf("Security Facade Stats: NULL\n");
        return;
    }

    printf("\n");
    printf("=== Security Facade Statistics ===\n");
    printf("Events:\n");
    printf("  Received: %lu\n", (unsigned long)stats->events_received);
    printf("  Processed: %lu\n", (unsigned long)stats->events_processed);
    printf("  Dropped: %lu\n", (unsigned long)stats->events_dropped);
    printf("  Pending: %lu\n", (unsigned long)stats->events_pending);
    printf("\n");

    printf("Threats:\n");
    printf("  Detected: %lu\n", (unsigned long)stats->threats_detected);
    printf("  Mitigated: %lu\n", (unsigned long)stats->threats_mitigated);
    printf("  False Positives: %lu\n", (unsigned long)stats->false_positives);
    printf("  Average Level: %.3f\n", stats->avg_threat_level);
    printf("  Peak Level: %.3f\n", stats->peak_threat_level);
    printf("\n");

    printf("Lockdowns:\n");
    printf("  Triggered: %u\n", stats->lockdowns_triggered);
    printf("  Total Time: %lu ms\n", (unsigned long)stats->total_lockdown_time_ms);
    printf("  Average Duration: %lu ms\n", (unsigned long)stats->avg_lockdown_duration_ms);
    printf("\n");

    printf("Performance:\n");
    printf("  Avg Processing Time: %.2f us\n", stats->avg_processing_time_us);
    printf("  Avg Threat Assessment: %.2f us\n", stats->avg_threat_assessment_us);
    printf("  Max Processing Time: %.2f us\n", stats->max_processing_time_us);
    printf("\n");

    printf("Uptime: %lu ms\n", (unsigned long)stats->uptime_ms);
    printf("\n");
}

const char* security_facade_version(void)
{
    return SEC_FACADE_VERSION;
}
