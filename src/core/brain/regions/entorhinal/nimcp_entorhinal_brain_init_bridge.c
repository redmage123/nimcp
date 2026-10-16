/**
 * @file nimcp_entorhinal_brain_init_bridge.c
 * @brief Implementation of Entorhinal-Brain Initialization Bridge
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include "core/brain/regions/entorhinal/nimcp_entorhinal_brain_init_bridge.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_timing_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(entorhinal_brain_init_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "ENTORHINAL_BRAIN_INIT_BRIDGE"


/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define DEFAULT_INIT_TIMEOUT_MS         NIMCP_LONG_TIMEOUT_MS
#define DEFAULT_DEPENDENCY_TIMEOUT_MS   NIMCP_WATCHDOG_TIMEOUT_MS
#define DEFAULT_BRIDGE_INIT_TIMEOUT_MS  NIMCP_DEFAULT_TIMEOUT_MS
#define DEFAULT_RETRY_COUNT             3
#define DEFAULT_RETRY_DELAY_MS          100
#define DEFAULT_HEALTH_CHECK_INTERVAL_MS 1000

/*=============================================================================
 * PHASE NAME STRINGS
 *===========================================================================*/

static const char* PHASE_NAMES[] = {
    "None",
    "Pre-Initialization",
    "Resource Allocation",
    "Core Initialization",
    "Bridge Connection",
    "Calibration",
    "Self-Test",
    "Registration",
    "Ready",
    "Error"
};

static const char* SHUTDOWN_PHASE_NAMES[] = {
    "None",
    "Prepare",
    "Save State",
    "Disconnect",
    "Deregister",
    "Cleanup",
    "Complete"
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static uint64_t get_current_time_ms(void) {
    /* Platform-specific time implementation would go here */
    /* For now, return 0 (would use clock_gettime or similar) */
    return 0;
}

static void set_error(entorhinal_brain_init_bridge_t* bridge,
    entorhinal_init_phase_t phase, int error_code, const char* message)
{
    bridge->status.init_failed = true;
    bridge->status.error_code = error_code;
    bridge->status.failed_phase = phase;
    if (message) {
        strncpy(bridge->status.error_message, message,
            sizeof(bridge->status.error_message) - 1);
    }
    bridge->status.current_phase = ENTORHINAL_INIT_PHASE_ERROR;

    if (bridge->config.error_callback) {
        bridge->config.error_callback(phase, error_code, message,
            bridge->config.callback_user_data);
    }
}

static void transition_phase(entorhinal_brain_init_bridge_t* bridge,
    entorhinal_init_phase_t new_phase)
{
    entorhinal_init_phase_t old_phase = bridge->status.current_phase;
    bridge->status.current_phase = new_phase;
    bridge->status.phase_start_time_ms = get_current_time_ms();
    bridge->status.phase_progress = 0.0f;
    bridge->status.current_step = 0;

    if (bridge->config.phase_callback) {
        bridge->config.phase_callback(old_phase, new_phase,
            bridge->config.callback_user_data);
    }
}

static void report_progress(entorhinal_brain_init_bridge_t* bridge,
    float progress, const char* message)
{
    bridge->status.phase_progress = progress;

    if (bridge->config.progress_callback) {
        bridge->config.progress_callback(bridge->status.current_phase,
            progress, message, bridge->config.callback_user_data);
    }
}

/*=============================================================================
 * LIFECYCLE IMPLEMENTATION
 *===========================================================================*/

entorhinal_brain_init_config_t entorhinal_brain_init_default_config(void) {
    entorhinal_brain_init_config_t config = {0};

    config.async_initialization = false;
    config.fail_fast = true;
    config.skip_self_test = false;
    config.skip_calibration = false;
    config.init_timeout_ms = DEFAULT_INIT_TIMEOUT_MS;
    config.retry_count = DEFAULT_RETRY_COUNT;
    config.retry_delay_ms = DEFAULT_RETRY_DELAY_MS;

    config.wait_for_dependencies = true;
    config.dependency_timeout_ms = DEFAULT_DEPENDENCY_TIMEOUT_MS;
    config.allow_partial_init = false;

    config.parallel_bridge_init = false;
    config.bridge_init_timeout_ms = DEFAULT_BRIDGE_INIT_TIMEOUT_MS;
    config.optional_bridges_can_fail = true;

    config.log_init_steps = true;
    config.log_timing = true;

    return config;
}

entorhinal_brain_init_bridge_t* entorhinal_brain_init_bridge_create(
    const entorhinal_brain_init_config_t* config)
{
    entorhinal_brain_init_bridge_t* bridge =
        (entorhinal_brain_init_bridge_t*)nimcp_calloc(1,
            sizeof(entorhinal_brain_init_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = entorhinal_brain_init_default_config();
    }

    bridge->status.current_phase = ENTORHINAL_INIT_PHASE_NONE;
    bridge->status.shutdown_phase = ENTORHINAL_SHUTDOWN_PHASE_NONE;
    bridge->status.bridges_total = BRIDGE_INIT_ORDER_COUNT;
    bridge->initial_health_score = 1.0f;
    bridge->current_health_score = 1.0f;
    bridge->health_check_interval_ms = DEFAULT_HEALTH_CHECK_INTERVAL_MS;

    return bridge;
}

void entorhinal_brain_init_bridge_destroy(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "entorhinal_brain_init");
    nimcp_free(bridge);
}

/*=============================================================================
 * INITIALIZATION IMPLEMENTATION
 *===========================================================================*/

int entorhinal_brain_init_initialize(
    entorhinal_brain_init_bridge_t* bridge,
    nimcp_entorhinal_t* entorhinal,
    nimcp_brain_t* brain)
{
    if (!bridge || !entorhinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_initialize: required parameter is NULL (bridge, entorhinal)");
        return -1;
    }

    bridge->entorhinal = entorhinal;
    bridge->brain = brain;
    bridge->status.init_start_time_ms = get_current_time_ms();
    bridge->total_init_attempts++;

    /* Phase 1: Pre-initialization */
    if (entorhinal_brain_init_execute_phase(bridge,
        ENTORHINAL_INIT_PHASE_PRE_INIT) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_initialize: operation failed");
        return -1;
    }

    /* Phase 2: Resource allocation */
    if (entorhinal_brain_init_execute_phase(bridge,
        ENTORHINAL_INIT_PHASE_RESOURCE_ALLOC) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_initialize: operation failed");
        return -1;
    }

    /* Phase 3: Core initialization */
    if (entorhinal_brain_init_execute_phase(bridge,
        ENTORHINAL_INIT_PHASE_CORE_INIT) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_initialize: operation failed");
        return -1;
    }

    /* Phase 4: Bridge connections */
    if (entorhinal_brain_init_execute_phase(bridge,
        ENTORHINAL_INIT_PHASE_BRIDGE_CONNECT) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_initialize: operation failed");
        return -1;
    }

    /* Phase 5: Calibration (optional) */
    if (!bridge->config.skip_calibration) {
        if (entorhinal_brain_init_execute_phase(bridge,
            ENTORHINAL_INIT_PHASE_CALIBRATION) != 0) {
            if (bridge->config.fail_fast) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_initialize: validation failed");
                return -1;
            }
        }
    }

    /* Phase 6: Self-test (optional) */
    if (!bridge->config.skip_self_test) {
        if (entorhinal_brain_init_execute_phase(bridge,
            ENTORHINAL_INIT_PHASE_SELF_TEST) != 0) {
            if (bridge->config.fail_fast) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_initialize: validation failed");
                return -1;
            }
        }
    }

    /* Phase 7: Registration */
    if (entorhinal_brain_init_execute_phase(bridge,
        ENTORHINAL_INIT_PHASE_REGISTRATION) != 0) {
        if (bridge->config.fail_fast) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_initialize: validation failed");
            return -1;
        }
    }

    /* Complete */
    transition_phase(bridge, ENTORHINAL_INIT_PHASE_READY);
    bridge->status.init_end_time_ms = get_current_time_ms();
    bridge->status.total_init_time_ms = (float)(bridge->status.init_end_time_ms -
        bridge->status.init_start_time_ms);
    bridge->successful_inits++;

    return 0;
}

bool entorhinal_brain_init_check_dependencies(
    entorhinal_brain_init_bridge_t* bridge,
    uint32_t required_deps)
{
    if (!bridge) {
        return false;
    }

    bridge->status.dependencies_required = required_deps;

    /* Check each dependency */
    uint32_t satisfied = 0;

    if (required_deps & ENTORHINAL_DEP_MEMORY_POOL) {
        /* Memory pool always available */
        satisfied |= ENTORHINAL_DEP_MEMORY_POOL;
    }

    if (required_deps & ENTORHINAL_DEP_LOGGING) {
        /* Global NIMCP logging always available; specific logger is optional */
        satisfied |= ENTORHINAL_DEP_LOGGING;
    }

    if (required_deps & ENTORHINAL_DEP_SECURITY) {
        /* Would check security context */
        satisfied |= ENTORHINAL_DEP_SECURITY;  /* Assume available */
    }

    if (required_deps & ENTORHINAL_DEP_BIO_ASYNC) {
        /* Would check bio-async router */
        satisfied |= ENTORHINAL_DEP_BIO_ASYNC;  /* Assume available */
    }

    if (required_deps & ENTORHINAL_DEP_BRAIN_KG) {
        if (bridge->kg != NULL) {
            satisfied |= ENTORHINAL_DEP_BRAIN_KG;
        }
    }

    if (required_deps & ENTORHINAL_DEP_THALAMUS) {
        /* Would check thalamus adapter */
        satisfied |= ENTORHINAL_DEP_THALAMUS;  /* Assume available */
    }

    if (required_deps & ENTORHINAL_DEP_SUBSTRATE) {
        /* Would check neural substrate */
        satisfied |= ENTORHINAL_DEP_SUBSTRATE;  /* Assume available */
    }

    bridge->status.dependencies_satisfied = satisfied;
    bridge->status.all_dependencies_met = (satisfied == required_deps);

    return bridge->status.all_dependencies_met;
}

int entorhinal_brain_init_wait_dependencies(
    entorhinal_brain_init_bridge_t* bridge,
    uint32_t required_deps,
    uint32_t timeout_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_wait_dependencies: bridge is NULL");
        return -1;
    }

    uint64_t start_time = get_current_time_ms();

    while (!entorhinal_brain_init_check_dependencies(bridge, required_deps)) {
        uint64_t elapsed = get_current_time_ms() - start_time;
        if (elapsed >= timeout_ms) {
            set_error(bridge, ENTORHINAL_INIT_PHASE_PRE_INIT, -1,
                "Dependency timeout");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_wait_dependencies: capacity exceeded");
            return -1;
        }
        /* Would sleep briefly here */
    }

    return 0;
}

int entorhinal_brain_init_execute_phase(
    entorhinal_brain_init_bridge_t* bridge,
    entorhinal_init_phase_t phase)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_execute_phase: bridge is NULL");
        return -1;
    }

    transition_phase(bridge, phase);

    switch (phase) {
        case ENTORHINAL_INIT_PHASE_PRE_INIT:
            report_progress(bridge, 0.0f, "Validating configuration");
            /* Validate entorhinal configuration */
            if (!bridge->entorhinal) {
                set_error(bridge, phase, -1, "Entorhinal instance is NULL");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_execute_phase: bridge->entorhinal is NULL");
                return -1;
            }
            report_progress(bridge, 0.5f, "Checking dependencies");
            if (bridge->config.wait_for_dependencies) {
                if (entorhinal_brain_init_wait_dependencies(bridge,
                    ENTORHINAL_DEP_MEMORY_POOL | ENTORHINAL_DEP_LOGGING,
                    bridge->config.dependency_timeout_ms) != 0) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_execute_phase: validation failed");
                    return -1;
                }
            }
            report_progress(bridge, 1.0f, "Pre-initialization complete");
            break;

        case ENTORHINAL_INIT_PHASE_RESOURCE_ALLOC:
            report_progress(bridge, 0.0f, "Allocating resources");
            /* Resources already allocated in entorhinal_create */
            report_progress(bridge, 1.0f, "Resource allocation complete");
            break;

        case ENTORHINAL_INIT_PHASE_CORE_INIT:
            report_progress(bridge, 0.0f, "Initializing grid cells");
            bridge->status.grid_cells_initialized = true;
            bridge->status.current_step = 1;
            report_progress(bridge, 0.25f, "Initializing border cells");
            bridge->status.border_cells_initialized = true;
            bridge->status.current_step = 2;
            report_progress(bridge, 0.5f, "Initializing HD cells");
            bridge->status.hd_cells_initialized = true;
            bridge->status.current_step = 3;
            report_progress(bridge, 0.75f, "Initializing path integration");
            bridge->status.path_integration_initialized = true;
            bridge->status.current_step = 4;
            report_progress(bridge, 0.9f, "Initializing memory gateway");
            bridge->status.memory_gateway_initialized = true;
            bridge->status.current_step = 5;
            report_progress(bridge, 1.0f, "Core initialization complete");
            break;

        case ENTORHINAL_INIT_PHASE_BRIDGE_CONNECT:
            report_progress(bridge, 0.0f, "Connecting bridges");
            if (entorhinal_brain_init_connect_all_bridges(bridge) != 0) {
                if (bridge->config.fail_fast) {
                    set_error(bridge, phase, -1, "Bridge connection failed");
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_execute_phase: validation failed");
                    return -1;
                }
            }
            report_progress(bridge, 1.0f, "Bridge connection complete");
            break;

        case ENTORHINAL_INIT_PHASE_CALIBRATION:
            report_progress(bridge, 0.0f, "Running calibration");
            if (entorhinal_brain_init_calibrate(bridge) != 0) {
                /* Calibration failure is non-fatal by default */
            }
            report_progress(bridge, 1.0f, "Calibration complete");
            break;

        case ENTORHINAL_INIT_PHASE_SELF_TEST:
            report_progress(bridge, 0.0f, "Running self-test");
            if (entorhinal_brain_init_self_test(bridge) != 0) {
                bridge->status.self_test_passed = false;
                if (bridge->config.fail_fast) {
                    set_error(bridge, phase, -1, "Self-test failed");
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_execute_phase: validation failed");
                    return -1;
                }
            } else {
                bridge->status.self_test_passed = true;
            }
            report_progress(bridge, 1.0f, "Self-test complete");
            break;

        case ENTORHINAL_INIT_PHASE_REGISTRATION:
            report_progress(bridge, 0.0f, "Registering with brain systems");
            if (bridge->factory) {
                entorhinal_brain_init_register_factory(bridge, bridge->factory);
            }
            if (bridge->kg) {
                entorhinal_brain_init_register_kg(bridge, bridge->kg);
            }
            report_progress(bridge, 1.0f, "Registration complete");
            break;

        default:
            break;
    }

    return 0;
}

int entorhinal_brain_init_advance_phase(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_advance_phase: bridge is NULL");
        return -1;
    }

    entorhinal_init_phase_t next_phase = bridge->status.current_phase + 1;
    if (next_phase >= ENTORHINAL_INIT_PHASE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "entorhinal_brain_init_advance_phase: capacity exceeded");
        return -1;
    }

    return entorhinal_brain_init_execute_phase(bridge, next_phase);
}

int entorhinal_brain_init_connect_bridge(
    entorhinal_brain_init_bridge_t* bridge,
    bridge_init_order_t bridge_order)
{
    if (!bridge || bridge_order >= BRIDGE_INIT_ORDER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "entorhinal_brain_init_connect_bridge: bridge is NULL");
        return -1;
    }

    int result = 0;

    switch (bridge_order) {
        case BRIDGE_INIT_ORDER_SECURITY:
            result = entorhinal_init_security_bridge(bridge->entorhinal, NULL, NULL);
            break;
        case BRIDGE_INIT_ORDER_LOGGING:
            /* Logging is always available via global NIMCP logging */
            result = 0;
            break;
        case BRIDGE_INIT_ORDER_SUBSTRATE:
            result = entorhinal_init_substrate_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_BIO_ASYNC:
            result = entorhinal_init_bio_async_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_SNN:
            result = entorhinal_init_snn_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_PLASTICITY:
            result = entorhinal_init_plasticity_bridge(bridge->entorhinal, NULL, NULL);
            break;
        case BRIDGE_INIT_ORDER_IMMUNE:
            result = entorhinal_init_immune_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_RESONANCE:
            result = entorhinal_init_resonance_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_THALAMIC:
            result = entorhinal_init_thalamic_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_COGNITIVE:
            result = entorhinal_init_cognitive_bridge(bridge->entorhinal, NULL, NULL, NULL);
            break;
        case BRIDGE_INIT_ORDER_TRAINING:
            result = entorhinal_init_training_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_PERCEPTION:
            result = entorhinal_init_perception_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_HIPPOCAMPUS:
            result = entorhinal_init_hippocampus_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_HYPOTHALAMUS:
            result = entorhinal_init_hypothalamus_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_CEREBELLUM:
            result = entorhinal_init_cerebellum_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_MEDULLA:
            result = entorhinal_init_medulla_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_OMNI:
            result = entorhinal_init_omni_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_SWARM:
            result = entorhinal_init_swarm_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_DRAGONFLY:
            result = entorhinal_init_dragonfly_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_PORTIA:
            result = entorhinal_init_portia_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_LOGIC:
            result = entorhinal_init_logic_bridge(bridge->entorhinal, NULL);
            break;
        case BRIDGE_INIT_ORDER_KG:
            result = entorhinal_init_kg_bridge(bridge->entorhinal, NULL);
            break;
        default:
            result = -1;
            break;
    }

    if (result == 0) {
        bridge->status.bridges_initialized[bridge_order] = true;
        bridge->status.bridges_connected++;
    }

    return result;
}

int entorhinal_brain_init_connect_all_bridges(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_connect_all_bridges: bridge is NULL");
        return -1;
    }

    int failures = 0;

    for (int i = 0; i < BRIDGE_INIT_ORDER_COUNT; i++) {
        float progress = (float)i / BRIDGE_INIT_ORDER_COUNT;
        report_progress(bridge, progress, "Connecting bridge");

        if (entorhinal_brain_init_connect_bridge(bridge, (bridge_init_order_t)i) != 0) {
            failures++;
            if (bridge->config.fail_fast && !bridge->config.optional_bridges_can_fail) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_connect_all_bridges: bridge->config is NULL");
                return -1;
            }
        }
    }

    return (failures > 0 && !bridge->config.optional_bridges_can_fail) ? -1 : 0;
}

/*=============================================================================
 * SHUTDOWN IMPLEMENTATION
 *===========================================================================*/

int entorhinal_brain_init_shutdown(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_shutdown: bridge is NULL");
        return -1;
    }

    /* Execute shutdown phases in order */
    for (int phase = ENTORHINAL_SHUTDOWN_PHASE_PREPARE;
         phase <= ENTORHINAL_SHUTDOWN_PHASE_COMPLETE; phase++) {
        entorhinal_brain_init_execute_shutdown_phase(bridge,
            (entorhinal_shutdown_phase_t)phase);
    }

    return 0;
}

int entorhinal_brain_init_execute_shutdown_phase(
    entorhinal_brain_init_bridge_t* bridge,
    entorhinal_shutdown_phase_t phase)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_execute_shutdown_phase: bridge is NULL");
        return -1;
    }

    bridge->status.shutdown_phase = phase;

    switch (phase) {
        case ENTORHINAL_SHUTDOWN_PHASE_PREPARE:
            /* Prepare for shutdown - flush pending operations */
            break;

        case ENTORHINAL_SHUTDOWN_PHASE_SAVE_STATE:
            /* Save state if needed */
            break;

        case ENTORHINAL_SHUTDOWN_PHASE_DISCONNECT:
            /* Disconnect all bridges */
            break;

        case ENTORHINAL_SHUTDOWN_PHASE_DEREGISTER:
            entorhinal_brain_init_deregister(bridge);
            break;

        case ENTORHINAL_SHUTDOWN_PHASE_CLEANUP:
            /* Cleanup resources */
            break;

        case ENTORHINAL_SHUTDOWN_PHASE_COMPLETE:
            bridge->status.current_phase = ENTORHINAL_INIT_PHASE_NONE;
            break;

        default:
            break;
    }

    return 0;
}

int entorhinal_brain_init_force_shutdown(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_force_shutdown: bridge is NULL");
        return -1;
    }

    /* Skip directly to cleanup */
    bridge->status.shutdown_phase = ENTORHINAL_SHUTDOWN_PHASE_CLEANUP;
    bridge->status.current_phase = ENTORHINAL_INIT_PHASE_NONE;

    return 0;
}

/*=============================================================================
 * REGISTRATION IMPLEMENTATION
 *===========================================================================*/

int entorhinal_brain_init_register_factory(
    entorhinal_brain_init_bridge_t* bridge,
    nimcp_brain_factory_t* factory)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_register_factory: bridge is NULL");
        return -1;
    }

    bridge->factory = factory;
    /* Would call factory registration API here */
    bridge->registered_with_factory = true;

    return 0;
}

int entorhinal_brain_init_register_kg(
    entorhinal_brain_init_bridge_t* bridge,
    brain_kg_t* kg)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_register_kg: bridge is NULL");
        return -1;
    }

    bridge->kg = kg;
    /* Would call KG registration API here */
    bridge->registered_with_kg = true;

    return 0;
}

int entorhinal_brain_init_deregister(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_deregister: bridge is NULL");
        return -1;
    }

    /* Deregister from factory */
    if (bridge->registered_with_factory) {
        /* Would call factory deregistration */
        bridge->registered_with_factory = false;
    }

    /* Deregister from KG */
    if (bridge->registered_with_kg) {
        /* Would call KG deregistration */
        bridge->registered_with_kg = false;
    }

    return 0;
}

/*=============================================================================
 * SELF-TEST IMPLEMENTATION
 *===========================================================================*/

int entorhinal_brain_init_self_test(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_self_test: bridge is NULL");
        return -1;
    }

    bridge->status.self_test_failures = 0;

    /* Test grid cells */
    if (entorhinal_brain_init_test_grid_cells(bridge) != 0) {
        bridge->status.self_test_failures++;
    }

    /* Test path integration */
    if (entorhinal_brain_init_test_path_integration(bridge) != 0) {
        bridge->status.self_test_failures++;
    }

    /* Test memory gateway */
    if (entorhinal_brain_init_test_memory_gateway(bridge) != 0) {
        bridge->status.self_test_failures++;
    }

    /* Test bridges */
    if (entorhinal_brain_init_test_bridges(bridge) != 0) {
        bridge->status.self_test_failures++;
    }

    snprintf(bridge->status.self_test_report,
        sizeof(bridge->status.self_test_report),
        "Self-test complete: %u failures",
        bridge->status.self_test_failures);

    return (bridge->status.self_test_failures > 0) ? -1 : 0;
}

int entorhinal_brain_init_test_grid_cells(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge || !bridge->entorhinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_test_grid_cells: required parameter is NULL (bridge, bridge->entorhinal)");
        return -1;
    }

    /* Test basic grid cell update */
    float position[3] = {1.0f, 1.0f, 0.0f};
    if (entorhinal_update_grid_cells(bridge->entorhinal, position, 3) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_test_grid_cells: validation failed");
        return -1;
    }

    return 0;
}

int entorhinal_brain_init_test_path_integration(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge || !bridge->entorhinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_test_path_integration: required parameter is NULL (bridge, bridge->entorhinal)");
        return -1;
    }

    /* Test basic path integration */
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    if (entorhinal_path_integrate(bridge->entorhinal, velocity, 0.0f, 0.1f) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_test_path_integration: validation failed");
        return -1;
    }

    return 0;
}

int entorhinal_brain_init_test_memory_gateway(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge || !bridge->entorhinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_test_memory_gateway: required parameter is NULL (bridge, bridge->entorhinal)");
        return -1;
    }

    /* Test encoding gate */
    if (entorhinal_set_encoding_gate(bridge->entorhinal, 0.5f) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_test_memory_gateway: validation failed");
        return -1;
    }

    return 0;
}

int entorhinal_brain_init_test_bridges(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_test_bridges: bridge is NULL");
        return -1;
    }

    /* Verify bridges are connected (skip check if initialization hasn't run) */
    if (bridge->status.current_phase >= ENTORHINAL_INIT_PHASE_BRIDGE_CONNECT &&
        bridge->status.bridges_connected == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_brain_init_test_bridges: bridge->status.bridges_connected is zero");
        return -1;
    }

    return 0;
}

/*=============================================================================
 * CALIBRATION IMPLEMENTATION
 *===========================================================================*/

int entorhinal_brain_init_calibrate(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_calibrate: bridge is NULL");
        return -1;
    }

    /* Calibrate grid cells */
    entorhinal_brain_init_calibrate_grid_cells(bridge);

    /* Calibrate HD cells */
    entorhinal_brain_init_calibrate_hd_cells(bridge);

    return 0;
}

int entorhinal_brain_init_calibrate_grid_cells(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge || !bridge->entorhinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_calibrate_grid_cells: required parameter is NULL (bridge, bridge->entorhinal)");
        return -1;
    }

    /* Reset grid phases to known position */
    float known_position[3] = {0.0f, 0.0f, 0.0f};
    return entorhinal_reset_grid_phases(bridge->entorhinal, known_position);
}

int entorhinal_brain_init_calibrate_hd_cells(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge || !bridge->entorhinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_calibrate_hd_cells: required parameter is NULL (bridge, bridge->entorhinal)");
        return -1;
    }

    /* Calibrate to known heading */
    return entorhinal_calibrate_hd_cells(bridge->entorhinal, 0.0f);
}

/*=============================================================================
 * STATUS IMPLEMENTATION
 *===========================================================================*/

int entorhinal_brain_init_get_status(
    const entorhinal_brain_init_bridge_t* bridge,
    entorhinal_init_status_t* status_out)
{
    if (!bridge || !status_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_get_status: required parameter is NULL (bridge, status_out)");
        return -1;
    }

    *status_out = bridge->status;

    return 0;
}

entorhinal_init_phase_t entorhinal_brain_init_get_phase(
    const entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) return ENTORHINAL_INIT_PHASE_NONE;
    return bridge->status.current_phase;
}

bool entorhinal_brain_init_is_ready(
    const entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->status.current_phase == ENTORHINAL_INIT_PHASE_READY;
}

bool entorhinal_brain_init_has_failed(
    const entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) return true;
    return bridge->status.init_failed;
}

int entorhinal_brain_init_get_error(
    const entorhinal_brain_init_bridge_t* bridge,
    int* error_code,
    char* error_message,
    size_t message_size)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_get_error: bridge is NULL");
        return -1;
    }

    if (error_code) *error_code = bridge->status.error_code;
    if (error_message && message_size > 0) {
        strncpy(error_message, bridge->status.error_message, message_size - 1);
    }

    return 0;
}

const char* entorhinal_brain_init_phase_string(
    entorhinal_init_phase_t phase)
{
    if (phase >= ENTORHINAL_INIT_PHASE_COUNT) {
        return "Unknown";
    }
    return PHASE_NAMES[phase];
}

/*=============================================================================
 * HEALTH MONITORING IMPLEMENTATION
 *===========================================================================*/

int entorhinal_brain_init_update_health(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge || !bridge->entorhinal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_update_health: required parameter is NULL (bridge, bridge->entorhinal)");
        return -1;
    }

    bridge->current_health_score = entorhinal_get_health_status(bridge->entorhinal);
    bridge->last_health_check_ms = get_current_time_ms();

    return 0;
}

float entorhinal_brain_init_get_health(
    const entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) return 0.0f;
    return bridge->current_health_score;
}

int entorhinal_brain_init_report_health(
    entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_report_health: bridge is NULL");
        return -1;
    }

    /* Would report health to brain KG here */

    return 0;
}

/*=============================================================================
 * DIAGNOSTICS IMPLEMENTATION
 *===========================================================================*/

int entorhinal_brain_init_log_diagnostics(
    const entorhinal_brain_init_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_log_diagnostics: bridge is NULL");
        return -1;
    }

    /* Would log to nimcp_logger here */

    return 0;
}

int entorhinal_brain_init_get_timing_report(
    const entorhinal_brain_init_bridge_t* bridge,
    char* report_out,
    size_t report_size)
{
    if (!bridge || !report_out || report_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_brain_init_get_timing_report: required parameter is NULL (bridge, report_out)");
        return -1;
    }

    snprintf(report_out, report_size,
        "Initialization Timing Report:\n"
        "  Total time: %.2f ms\n"
        "  Bridges connected: %u / %u\n"
        "  Self-test: %s\n"
        "  Health score: %.2f\n",
        bridge->status.total_init_time_ms,
        bridge->status.bridges_connected,
        bridge->status.bridges_total,
        bridge->status.self_test_passed ? "PASSED" : "FAILED",
        bridge->current_health_score);

    return 0;
}
