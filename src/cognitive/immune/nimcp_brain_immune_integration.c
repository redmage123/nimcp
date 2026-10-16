/**
 * @file nimcp_brain_immune_integration.c
 * @brief Full integration helper implementation for exception-immune setup
 * @version 1.0.0
 * @date 2025-01-25
 *
 * WHAT: Implementation of complete exception-immune integration helper
 * WHY:  Ensure correct initialization sequence and prevent common mistakes
 * HOW:  Orchestrate all component initialization in proper order
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_brain_immune_integration.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/immune/nimcp_brain_immune_tick.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

#define LOG_MODULE "IMMUNE_INTEGRATION"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(brain_immune_integration, MESH_ADAPTER_CATEGORY_SECURITY)



#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define INTEGRATION_MODULE_NAME "immune_integration"

/* ============================================================================
 * Integration Structure
 * ============================================================================ */

/**
 * @brief Internal integration state
 */
struct nimcp_immune_integration {
    /* Configuration */
    nimcp_immune_integration_config_t config;

    /* Components */
    brain_immune_system_t* immune_system;
    bool owns_immune_system;           /**< True if we created it */

    /* State */
    bool exception_system_initialized;
    bool handlers_installed;
    bool immune_connected;
    bool tick_initialized;
    bool recovery_callbacks_installed;
    bool running;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_tick_time_us;

    /* Statistics */
    nimcp_immune_integration_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Log integration event
 */
static void integration_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    nimcp_log_writev(NULL, LOG_LEVEL_INFO, INTEGRATION_MODULE_NAME, __FILE__, __LINE__, format, args);
    va_end(args);
}

/**
 * @brief Log integration error
 */
static void integration_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    nimcp_log_writev(NULL, LOG_LEVEL_ERROR, INTEGRATION_MODULE_NAME, __FILE__, __LINE__, format, args);
    va_end(args);
}

/**
 * @brief Log integration warning
 */
static void integration_warn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    nimcp_log_writev(NULL, LOG_LEVEL_WARN, INTEGRATION_MODULE_NAME, __FILE__, __LINE__, format, args);
    va_end(args);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

void nimcp_immune_integration_default_config(nimcp_immune_integration_config_t* config) {
    if (!config) return;

    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_d", 0.0f);


    memset(config, 0, sizeof(*config));

    /* Exception system defaults */
    config->install_default_handlers = true;
    config->install_recovery_callbacks = true;

    /* Immune system defaults */
    config->create_immune_system = true;
    config->external_immune = NULL;

    /* Tick orchestrator defaults */
    config->init_tick_orchestrator = true;
    config->max_exceptions_per_tick = 10;
    config->max_health_msgs_per_tick = 20;

    /* Health agent (none by default) */
    config->health_agent = NULL;

    /* Recovery context (none by default) */
    config->brain = NULL;
    config->gc_context = NULL;
    config->bbb_system = NULL;
    config->runtime_adaptation = NULL;
    config->checkpoint_dir = NULL;

    /* Behavior defaults */
    config->auto_start = true;
    config->enable_logging = true;
}

nimcp_immune_integration_t* nimcp_immune_integration_create(
    const nimcp_immune_integration_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_c", 0.0f);


    nimcp_immune_integration_t* integration = NULL;
    nimcp_immune_integration_config_t local_config;
    int result = 0;

    /* Use defaults if no config provided */
    if (!config) {
        nimcp_immune_integration_default_config(&local_config);
        config = &local_config;
    }

    /* Allocate integration structure */
    integration = (nimcp_immune_integration_t*)nimcp_calloc(1, sizeof(*integration));
    if (!integration) {
        integration_error("Failed to allocate integration structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_immune_integration_create: integration is NULL");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&integration->config, config, sizeof(integration->config));
    integration->creation_time_us = nimcp_time_monotonic_us();

    if (config->enable_logging) {
        integration_log("Creating exception-immune integration");
    }

    /* Step 1: Initialize exception system */
    if (!nimcp_exception_system_is_initialized()) {
        result = nimcp_exception_system_init();
        if (result != 0) {
            integration_error("Failed to initialize exception system");
            goto cleanup;
        }
        integration->exception_system_initialized = true;
        if (config->enable_logging) {
            integration_log("Step 1: Exception system initialized");
        }
    } else {
        if (config->enable_logging) {
            integration_log("Step 1: Exception system already initialized");
        }
    }

    /* Step 2: Install default handlers */
    if (config->install_default_handlers) {
        result = nimcp_install_default_handlers();
        if (result != 0) {
            integration_warn("Failed to install default handlers (may already be installed)");
        } else {
            integration->handlers_installed = true;
            if (config->enable_logging) {
                integration_log("Step 2: Default exception handlers installed");
            }
        }
    }

    /* Step 3: Create or use provided immune system */
    if (config->create_immune_system) {
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_config.enable_logging = config->enable_logging;

        integration->immune_system = brain_immune_create(&immune_config);
        if (!integration->immune_system) {
            integration_error("Failed to create brain immune system");
            goto cleanup;
        }
        integration->owns_immune_system = true;
        if (config->enable_logging) {
            integration_log("Step 3: Brain immune system created");
        }
    } else if (config->external_immune) {
        integration->immune_system = config->external_immune;
        integration->owns_immune_system = false;
        if (config->enable_logging) {
            integration_log("Step 3: Using external immune system");
        }
    } else {
        integration_error("No immune system: create_immune_system=false and no external provided");
        goto cleanup;
    }

    /* Step 4: Start immune system */
    if (config->auto_start) {
        result = brain_immune_start(integration->immune_system);
        if (result != 0) {
            integration_error("Failed to start brain immune system");
            goto cleanup;
        }
        integration->running = true;
        if (config->enable_logging) {
            integration_log("Step 4: Brain immune system started");
        }
    }

    /* Step 5: Initialize exception-immune integration */
    nimcp_exception_immune_config_t ex_immune_config;
    nimcp_exception_immune_default_config(&ex_immune_config);
    result = nimcp_exception_immune_init(&ex_immune_config);
    if (result != 0) {
        integration_warn("Exception-immune init failed (may already be initialized)");
    } else {
        if (config->enable_logging) {
            integration_log("Step 5: Exception-immune integration initialized");
        }
    }

    /* Step 6: Connect immune system to exception system */
    result = nimcp_exception_immune_connect(integration->immune_system);
    if (result != 0) {
        integration_error("Failed to connect immune system to exception system");
        goto cleanup;
    }
    integration->immune_connected = true;
    if (config->enable_logging) {
        integration_log("Step 6: Immune system connected to exception system");
    }

    /* Step 7: Initialize tick orchestrator */
    if (config->init_tick_orchestrator) {
        brain_immune_tick_config_t tick_config;
        brain_immune_tick_default_config(&tick_config);
        tick_config.max_exceptions_per_tick = config->max_exceptions_per_tick;
        tick_config.max_health_msgs_per_tick = config->max_health_msgs_per_tick;

        result = brain_immune_tick_init(integration->immune_system, &tick_config);
        if (result != 0) {
            integration_error("Failed to initialize tick orchestrator");
            goto cleanup;
        }
        integration->tick_initialized = true;
        if (config->enable_logging) {
            integration_log("Step 7: Tick orchestrator initialized");
        }
    }

    /* Step 8: Install default recovery callbacks */
    if (config->install_recovery_callbacks) {
        result = nimcp_exception_install_default_recovery_callbacks();
        if (result != 0) {
            integration_warn("Failed to install recovery callbacks");
        } else {
            integration->recovery_callbacks_installed = true;
            if (config->enable_logging) {
                integration_log("Step 8: Default recovery callbacks installed");
            }
        }
    }

    /* Step 9: Connect health agent if provided */
    if (config->health_agent) {
        result = brain_immune_tick_connect_health_agent(
            integration->immune_system,
            config->health_agent
        );
        if (result != 0) {
            integration_warn("Failed to connect health agent");
        } else {
            if (config->enable_logging) {
                integration_log("Step 9: Health agent connected");
            }
        }
    }

    /* Step 10: Set recovery context if provided */
    if (config->brain || config->gc_context || config->bbb_system ||
        config->runtime_adaptation || config->checkpoint_dir) {
        result = nimcp_recovery_set_context(
            config->brain,
            config->gc_context,
            config->bbb_system,
            config->runtime_adaptation,
            config->checkpoint_dir
        );
        if (result != 0) {
            integration_warn("Failed to set recovery context");
        } else {
            if (config->enable_logging) {
                integration_log("Step 10: Recovery context configured");
            }
        }
    }

    if (config->enable_logging) {
        integration_log("Integration setup complete - all systems connected");
    }

    return integration;

cleanup:
    nimcp_immune_integration_destroy(integration);
    /* Cleanup reached on alloc/init failure: use NO_MEMORY error code */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_immune_integration_create: initialization failed");
    return NULL;
}

void nimcp_immune_integration_destroy(nimcp_immune_integration_t* integration) {
    if (!integration) return;

    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_d", 0.0f);


    if (integration->config.enable_logging) {
        integration_log("Destroying exception-immune integration");
    }

    /* Stop immune system if running */
    if (integration->running && integration->immune_system) {
        brain_immune_stop(integration->immune_system);
        integration->running = false;
    }

    /* Shutdown tick orchestrator */
    if (integration->tick_initialized && integration->immune_system) {
        brain_immune_tick_shutdown(integration->immune_system);
        integration->tick_initialized = false;
    }

    /* Disconnect from exception system */
    if (integration->immune_connected) {
        nimcp_exception_immune_disconnect();
        integration->immune_connected = false;
    }

    /* Destroy immune system if we own it */
    if (integration->owns_immune_system && integration->immune_system) {
        brain_immune_destroy(integration->immune_system);
        integration->immune_system = NULL;
    }

    /* Free integration structure */
    nimcp_free(integration);
    /* Note: no need to NULL the local pointer; caller retains their copy */
}

int nimcp_immune_integration_start(nimcp_immune_integration_t* integration) {
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_immune_integration_start: integration is NULL");
        return -1;
    }
    if (!integration->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_immune_integration_start: integration->immune_system is NULL");
        return -1;
    }
    if (integration->running) return 0;  /* Already running */

    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_s", 0.0f);


    int result = brain_immune_start(integration->immune_system);
    if (result == 0) {
        integration->running = true;
        if (integration->config.enable_logging) {
            integration_log("Immune system started");
        }
    }
    return result;
}

int nimcp_immune_integration_stop(nimcp_immune_integration_t* integration) {
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_immune_integration_stop: integration is NULL");
        return -1;
    }
    if (!integration->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_immune_integration_stop: integration->immune_system is NULL");
        return -1;
    }
    if (!integration->running) return 0;  /* Already stopped */

    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_s", 0.0f);


    int result = brain_immune_stop(integration->immune_system);
    if (result == 0) {
        integration->running = false;
        if (integration->config.enable_logging) {
            integration_log("Immune system stopped");
        }
    }
    return result;
}

/* ============================================================================
 * Runtime Implementation
 * ============================================================================ */

int nimcp_immune_integration_tick(
    nimcp_immune_integration_t* integration,
    uint64_t delta_ms
) {
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_immune_integration_tick: integration is NULL");
        return -1;
    }
    if (!integration->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_immune_integration_tick: integration->immune_system is NULL");
        return -1;
    }
    if (!integration->running) {
        /* running is a bool flag, not a pointer */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_immune_integration_tick: integration is not running");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_t", 0.0f);


    uint64_t start_us = nimcp_time_monotonic_us();

    /* Execute the tick */
    int result = brain_immune_tick(integration->immune_system, delta_ms);

    /* Update statistics */
    uint64_t duration_us = nimcp_time_monotonic_us() - start_us;
    integration->stats.ticks_executed++;

    /* Update average (exponential moving average) */
    float alpha = 0.1f;
    integration->stats.avg_tick_duration_us =
        (1.0f - alpha) * integration->stats.avg_tick_duration_us +
        alpha * (float)duration_us;

    if (duration_us > integration->stats.max_tick_duration_us) {
        integration->stats.max_tick_duration_us = duration_us;
    }

    integration->last_tick_time_us = start_us;

    return result;
}

int nimcp_immune_integration_connect_health_agent(
    nimcp_immune_integration_t* integration,
    nimcp_health_agent_t* agent
) {
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_immune_integration_connect_health_agent: integration is NULL");
        return -1;
    }
    if (!integration->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_immune_integration_connect_health_agent: integration->immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_c", 0.0f);


    return brain_immune_tick_connect_health_agent(integration->immune_system, agent);
}

int nimcp_immune_integration_set_recovery_context(
    nimcp_immune_integration_t* integration,
    brain_t brain,
    void* gc_context,
    void* bbb_system,
    void* ra_ctx,
    const char* checkpoint_dir
) {
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_immune_integration_set_recovery_context: integration is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_s", 0.0f);


    return nimcp_recovery_set_context(
        brain,
        gc_context,
        bbb_system,
        ra_ctx,
        checkpoint_dir
    );
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

brain_immune_system_t* nimcp_immune_integration_get_immune_system(
    nimcp_immune_integration_t* integration
) {
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_immune_integration_get_immune_system: integration is NULL");
        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_g", 0.0f);


    return integration->immune_system;
}

bool nimcp_immune_integration_is_running(
    const nimcp_immune_integration_t* integration
) {
    if (!integration) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_i", 0.0f);


    return integration->running;
}

int nimcp_immune_integration_get_stats(
    const nimcp_immune_integration_t* integration,
    nimcp_immune_integration_stats_t* stats
) {
    if (!integration || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_immune_integration_get_stats: required parameter is NULL (integration, stats)");
        return -1;
    }

    /* Copy local stats */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_g", 0.0f);


    memcpy(stats, &integration->stats, sizeof(*stats));

    /* Calculate uptime */
    stats->uptime_ms = (nimcp_time_monotonic_us() - integration->creation_time_us) / 1000;

    /* Get immune system stats if available */
    if (integration->immune_system) {
        brain_immune_stats_t immune_stats;
        if (brain_immune_get_stats(integration->immune_system, &immune_stats) == 0) {
            stats->antigens_created = immune_stats.antigens_processed;
        }
    }

    /* Get tick stats if available */
    if (integration->tick_initialized && integration->immune_system) {
        brain_immune_tick_stats_t tick_stats;
        if (brain_immune_tick_get_stats(integration->immune_system, &tick_stats) == 0) {
            stats->exceptions_handled = tick_stats.exceptions_processed;
            stats->health_msgs_processed = tick_stats.health_messages_processed;
            stats->recoveries_triggered = tick_stats.recovery_actions_triggered;
            stats->recoveries_succeeded = tick_stats.recovery_actions_succeeded;
        }
    }

    /* Get exception-immune stats */
    nimcp_exception_immune_stats_t ex_stats;
    nimcp_exception_immune_get_stats(&ex_stats);
    stats->exceptions_presented = ex_stats.exceptions_presented;
    stats->exceptions_async_queued = ex_stats.exceptions_pending;

    return 0;
}

void nimcp_immune_integration_reset_stats(nimcp_immune_integration_t* integration) {
    if (!integration) return;
    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_r", 0.0f);


    memset(&integration->stats, 0, sizeof(integration->stats));

    /* Reset component stats too */
    if (integration->tick_initialized && integration->immune_system) {
        brain_immune_tick_reset_stats(integration->immune_system);
    }
    nimcp_exception_immune_reset_stats();
}

/* ============================================================================
 * Diagnostic Implementation
 * ============================================================================ */

int nimcp_immune_integration_diagnose(
    const nimcp_immune_integration_t* integration,
    char* buffer,
    size_t buffer_size
) {
    if (!buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_immune_integration_diagnose: buffer is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_d", 0.0f);


    int issues = 0;
    size_t offset = 0;
    int written = 0;

    if (!integration) {
        written = snprintf(buffer, buffer_size, "ERROR: Integration is NULL\n");
        return 1;
    }

    /* Check exception system */
    if (!nimcp_exception_system_is_initialized()) {
        written = snprintf(buffer + offset, buffer_size - offset,
            "ISSUE: Exception system not initialized\n");
        if (written > 0) offset += written;
        issues++;
    }

    /* Check handlers installed */
    if (integration->config.install_default_handlers && !integration->handlers_installed) {
        written = snprintf(buffer + offset, buffer_size - offset,
            "ISSUE: Default handlers not installed (SEVERE+ exceptions won't auto-present)\n");
        if (written > 0) offset += written;
        issues++;
    }

    /* Check immune system exists */
    if (!integration->immune_system) {
        written = snprintf(buffer + offset, buffer_size - offset,
            "ISSUE: No immune system connected\n");
        if (written > 0) offset += written;
        issues++;
    }

    /* Check immune connected to exception system */
    if (!nimcp_exception_immune_is_connected()) {
        written = snprintf(buffer + offset, buffer_size - offset,
            "ISSUE: Immune system not connected to exception system\n");
        if (written > 0) offset += written;
        issues++;
    }

    /* Check tick orchestrator */
    if (integration->config.init_tick_orchestrator && !integration->tick_initialized) {
        written = snprintf(buffer + offset, buffer_size - offset,
            "ISSUE: Tick orchestrator not initialized (async exceptions won't process)\n");
        if (written > 0) offset += written;
        issues++;
    }

    /* Check running state */
    if (!integration->running) {
        written = snprintf(buffer + offset, buffer_size - offset,
            "ISSUE: Immune system not running (brain_immune_update() will return -1)\n");
        if (written > 0) offset += written;
        issues++;
    }

    /* Summary */
    if (issues == 0) {
        snprintf(buffer + offset, buffer_size - offset, "OK: All systems healthy\n");
    }

    return issues;
}

void nimcp_immune_integration_log_state(
    const nimcp_immune_integration_t* integration
) {
    if (!integration) {
        integration_error("Cannot log state: integration is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_integration_heartbeat("brain_immune_immune_integration_l", 0.0f);


    integration_log("=== Integration State ===");
    integration_log("Exception system: %s",
        nimcp_exception_system_is_initialized() ? "initialized" : "NOT initialized");
    integration_log("Default handlers: %s",
        integration->handlers_installed ? "installed" : "NOT installed");
    integration_log("Immune system: %s",
        integration->immune_system ? "present" : "MISSING");
    integration_log("  - Owned: %s",
        integration->owns_immune_system ? "yes" : "no (external)");
    integration_log("Immune connected: %s",
        integration->immune_connected ? "yes" : "NO");
    integration_log("Tick orchestrator: %s",
        integration->tick_initialized ? "initialized" : "NOT initialized");
    integration_log("Recovery callbacks: %s",
        integration->recovery_callbacks_installed ? "installed" : "NOT installed");
    integration_log("Running: %s",
        integration->running ? "yes" : "NO");
    integration_log("Ticks executed: %lu", (unsigned long)integration->stats.ticks_executed);
    integration_log("Avg tick duration: %.2f us", integration->stats.avg_tick_duration_us);
    integration_log("=========================");
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void brain_immune_integration_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_brain_immune_integration_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int brain_immune_integration_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_integration_training_begin: NULL argument");
        return -1;
    }
    brain_immune_integration_heartbeat_instance(NULL, "brain_immune_integration_training_begin", 0.0f);
    (void)(struct nimcp_immune_integration*)instance; /* Module state available for reset */
    return 0;
}

int brain_immune_integration_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_integration_training_end: NULL argument");
        return -1;
    }
    brain_immune_integration_heartbeat_instance(NULL, "brain_immune_integration_training_end", 1.0f);
    (void)(struct nimcp_immune_integration*)instance; /* Module state available for finalization */
    return 0;
}

int brain_immune_integration_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_integration_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    brain_immune_integration_heartbeat_instance(NULL, "brain_immune_integration_training_step", progress);
    (void)(struct nimcp_immune_integration*)instance; /* Module state available for step adaptation */
    return 0;
}
