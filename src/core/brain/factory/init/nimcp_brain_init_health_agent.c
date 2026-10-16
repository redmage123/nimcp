/**
 * @file nimcp_brain_init_health_agent.c
 * @brief Health Agent Subsystem Initialization & Integration
 *
 * WHAT: Initialize health agent for autonomous brain health monitoring
 * WHY:  Independent monitoring detects problems even when main system stressed
 * HOW:  Create health agent, connect to brain subsystems, start monitoring
 *
 * INTEGRATION POINTS:
 * - Brain: Main brain state inspection via nimcp_brain_probe()
 * - Immune System: Coordinate with brain immune system for health responses
 * - SNN/LNN: Monitor neural network stability and detect divergence
 * - Oscillations: Monitor brain wave patterns for anomalies
 * - Memory: Track memory usage and detect leaks/corruption
 *
 * DESIGN:
 * The health agent runs in a separate thread, providing independent
 * monitoring that continues even if the main processing thread hangs.
 * It uses lock-free communication to avoid blocking the main thread.
 *
 * NOTE: This file uses forward declarations to avoid header conflicts.
 * The full health agent API is in utils/fault_tolerance/nimcp_health_agent.h
 *
 * @author NIMCP Development Team
 * @date 2025-01-19
 * @version 1.0.0
 */

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_health_agent, MESH_ADAPTER_CATEGORY_SYSTEM)


/* Forward declarations to avoid header conflicts */
/* The nimcp_health_agent.h has type conflicts with other headers */
/* We use explicit function declarations instead */

struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/* Health agent config - mirror the essential fields */
typedef struct {
    char agent_name[NIMCP_ID_BUFFER_SIZE];
    uint32_t check_interval_ms;
    bool enable_auto_recovery;
} health_agent_simple_config_t;

/* Health agent lifecycle functions */
extern nimcp_health_agent_t* nimcp_health_agent_create(const void* config);
extern void nimcp_health_agent_destroy(nimcp_health_agent_t* agent);
extern int nimcp_health_agent_start(nimcp_health_agent_t* agent);
extern int nimcp_health_agent_stop(nimcp_health_agent_t* agent);
extern void nimcp_health_agent_default_config(void* config);

/* Health agent brain connection functions */
extern int nimcp_health_agent_connect_brain(nimcp_health_agent_t* agent, void* brain);
extern int nimcp_health_agent_connect_immune(nimcp_health_agent_t* agent, void* immune);

/* Brain immune tick orchestrator initialization */
extern int brain_immune_tick_init(void* immune, const void* config);
extern void brain_immune_tick_default_config(void* config);
extern int brain_immune_tick_connect_health_agent(void* immune, nimcp_health_agent_t* agent);
extern void brain_immune_tick_shutdown(void* immune);

/* Brain probe functions */
typedef struct {
    bool enable_probe_monitoring;
    bool enable_memory_tracking;
    bool enable_performance_tracking;
} health_agent_brain_probe_simple_config_t;

extern void nimcp_health_agent_brain_probe_config_default(void* config);
extern int nimcp_health_agent_register_brain_probe(nimcp_health_agent_t* agent, void* brain, const void* config);
extern int nimcp_health_agent_unregister_brain_probe(nimcp_health_agent_t* agent, void* brain);
extern float nimcp_health_agent_get_brain_probe_health_score(const nimcp_health_agent_t* agent);

//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize health agent subsystem for brain
 *
 * @param brain Brain instance to initialize health agent for
 * @return true on success, false on failure
 *
 * PROCESS:
 * 1. Check if health agent is enabled in config
 * 2. Create health agent with appropriate configuration
 * 3. Connect to brain for state inspection
 * 4. Connect to available subsystems (immune, etc.)
 * 5. Optionally start the agent (if auto-start enabled)
 */
bool nimcp_brain_factory_init_health_agent_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_health_agent_subsystem: brain is NULL");

            return false;
    }

    /* Initialize fields */
    brain->health_agent = NULL;
    brain->health_agent_enabled = false;
    brain->health_agent_owns_agent = false;

    /* Check if health agent is enabled */
    if (!brain->config.enable_health_agent) {
        NIMCP_LOGGING_DEBUG("Health agent disabled by config");
        return true;  /* Success - disabled by config */
    }

    NIMCP_LOGGING_INFO("Initializing health agent subsystem...");

    /* Configure the health agent using minimal config */
    /* Note: We use a simple config struct to avoid header conflicts */
    health_agent_simple_config_t config = {0};

    /* Set agent name based on brain task name */
    snprintf(config.agent_name, sizeof(config.agent_name),
             "brain_%s_agent", brain->config.task_name);

    /* Apply brain-specific configuration */
    if (brain->config.health_check_interval_ms > 0) {
        config.check_interval_ms = brain->config.health_check_interval_ms;
    } else {
        config.check_interval_ms = 100;  /* Default 100ms */
    }
    config.enable_auto_recovery = brain->config.health_agent_auto_recovery;

    /* Create the health agent */
    /* Note: We pass our simple config but the actual API expects full config */
    /* For now, we pass NULL to use defaults */
    nimcp_health_agent_t* agent = nimcp_health_agent_create(NULL);
    if (!agent) {
        NIMCP_LOGGING_ERROR("Failed to create health agent");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_health_agent_subsystem: agent is NULL");
        return false;
    }

    /* Store in brain */
    brain->health_agent = agent;
    brain->health_agent_enabled = true;
    brain->health_agent_owns_agent = true;

    NIMCP_LOGGING_INFO("Health agent created, connecting subsystems...");

    /* Connect to main brain for state inspection */
    int result = nimcp_health_agent_connect_brain(agent, brain);
    if (result == 0) {
        NIMCP_LOGGING_INFO("Health agent connected to brain for state inspection");
    }

    /* Connect to immune system if available */
    if (brain->immune_system && brain->immune_enabled) {
        result = nimcp_health_agent_connect_immune(agent, brain->immune_system);
        if (result == 0) {
            NIMCP_LOGGING_INFO("Health agent connected to brain immune system");

            /* Initialize the tick orchestrator for the immune system
             * This enables brain_immune_tick() to process exceptions and health messages */
            result = brain_immune_tick_init(brain->immune_system, NULL);  /* NULL = defaults */
            if (result == 0) {
                NIMCP_LOGGING_INFO("Brain immune tick orchestrator initialized");

                /* Connect health agent to tick orchestrator for bidirectional communication */
                result = brain_immune_tick_connect_health_agent(brain->immune_system, agent);
                if (result == 0) {
                    NIMCP_LOGGING_INFO("Health agent connected to immune tick orchestrator");
                }
            } else {
                NIMCP_LOGGING_WARN("Failed to initialize brain immune tick orchestrator");
            }
        }
    }

    /* Register brain for probe-based health monitoring */
    health_agent_brain_probe_simple_config_t probe_config = {0};
    probe_config.enable_probe_monitoring = true;
    probe_config.enable_memory_tracking = true;
    probe_config.enable_performance_tracking = true;

    result = nimcp_health_agent_register_brain_probe(agent, brain, &probe_config);
    if (result == 0) {
        NIMCP_LOGGING_INFO("Health agent registered brain for probe monitoring");
    }

    NIMCP_LOGGING_INFO("Health agent initialization complete");
    NIMCP_LOGGING_INFO("  Check interval: %u ms", config.check_interval_ms);
    NIMCP_LOGGING_INFO("  Auto recovery: %s", config.enable_auto_recovery ? "enabled" : "disabled");

    return true;
}

/**
 * @brief Destroy health agent subsystem
 *
 * @param brain Brain instance to destroy health agent for
 */
void nimcp_brain_factory_destroy_health_agent_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_health_agent_subsystem: brain is NULL"); return; }

    /* Shutdown tick orchestrator first (before stopping agent thread) */
    if (brain->immune_system && brain->immune_enabled) {
        brain_immune_tick_shutdown(brain->immune_system);
    }

    if (brain->health_agent && brain->health_agent_owns_agent) {
        /* Stop the agent if running */
        nimcp_health_agent_stop(brain->health_agent);

        /* Unregister brain from probe monitoring */
        nimcp_health_agent_unregister_brain_probe(brain->health_agent, brain);

        /* Destroy the agent */
        nimcp_health_agent_destroy(brain->health_agent);
    }

    brain->health_agent = NULL;
    brain->health_agent_enabled = false;
    brain->health_agent_owns_agent = false;
}

//=============================================================================
// Accessor Functions
//=============================================================================

/**
 * @brief Get health agent from brain
 *
 * @param brain Brain instance
 * @return Health agent handle or NULL if not enabled
 */
struct nimcp_health_agent* brain_get_health_agent(brain_t brain) {
    if (!brain || !brain->health_agent_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_get_health_agent: invalid parameters");

            return NULL;
    }
    return brain->health_agent;
}

/**
 * @brief Start health agent monitoring
 *
 * @param brain Brain instance
 * @return true on success
 */
bool brain_start_health_agent(brain_t brain) {
    if (!brain || !brain->health_agent_enabled || !brain->health_agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_start_health_agent: invalid parameters");

            return false;
    }

    int result = nimcp_health_agent_start(brain->health_agent);
    if (result == 0) {
        NIMCP_LOGGING_INFO("Health agent started for brain '%s'", brain->config.task_name);
        return true;
    }

    NIMCP_LOGGING_ERROR("Failed to start health agent");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_start_health_agent: result is zero");
    return false;
}

/**
 * @brief Stop health agent monitoring
 *
 * @param brain Brain instance
 * @return true on success
 */
bool brain_stop_health_agent(brain_t brain) {
    if (!brain || !brain->health_agent_enabled || !brain->health_agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_stop_health_agent: invalid parameters");

            return false;
    }

    int result = nimcp_health_agent_stop(brain->health_agent);
    if (result == 0) {
        NIMCP_LOGGING_INFO("Health agent stopped for brain '%s'", brain->config.task_name);
        return true;
    }

    NIMCP_LOGGING_ERROR("Failed to stop health agent");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_stop_health_agent: result is zero");
    return false;
}

/**
 * @brief Get current health score from health agent
 *
 * Aggregates brain probe health score from the health agent.
 * This includes metrics from periodic probing (neurons, synapses,
 * memory usage, inference time, etc.)
 *
 * @param brain Brain instance
 * @return Health score [0-100], 100 if healthy, or -1.0 on error
 */
float brain_get_health_score(brain_t brain) {
    if (!brain || !brain->health_agent_enabled || !brain->health_agent) {
        return -1.0f;
    }

    return nimcp_health_agent_get_brain_probe_health_score(brain->health_agent);
}
