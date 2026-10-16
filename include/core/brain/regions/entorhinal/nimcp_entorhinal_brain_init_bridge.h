/**
 * @file nimcp_entorhinal_brain_init_bridge.h
 * @brief Entorhinal-Brain Initialization Integration Bridge
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 *
 * WHAT: Bridge for integrating entorhinal cortex with the brain initialization
 *       system, managing startup sequences, dependencies, and lifecycle.
 *
 * WHY:  Proper initialization is critical for:
 *       - Correct dependency ordering (grid cells before path integration)
 *       - Resource allocation and validation
 *       - Bridge connection sequencing
 *       - Graceful startup and shutdown
 *       - Health monitoring from first activation
 *
 * HOW:  Initialization phases:
 *       1. Pre-init: Validate configuration, allocate resources
 *       2. Core init: Initialize grid cells, border cells, HD cells
 *       3. Bridge init: Connect all 21 module bridges
 *       4. Post-init: Calibration, self-test, registration with brain KG
 *       5. Ready: Begin normal operation
 *
 * INTEGRATION POINTS:
 * - Brain Factory: Component registration and discovery
 * - Brain Knowledge Graph: Self-awareness and health reporting
 * - Logging Module: Initialization audit trail
 * - Security Module: Access control setup
 * - All 21 bridge modules: Ordered connection
 */

#ifndef NIMCP_ENTORHINAL_BRAIN_INIT_BRIDGE_H
#define NIMCP_ENTORHINAL_BRAIN_INIT_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

typedef struct nimcp_entorhinal nimcp_entorhinal_t;
typedef struct nimcp_brain_handle* nimcp_brain_t;
typedef struct brain_kg brain_kg_t;
typedef struct nimcp_brain_factory nimcp_brain_factory_t;
typedef struct nimcp_logger_struct* nimcp_logger_t;

/*=============================================================================
 * INITIALIZATION PHASE ENUMERATION
 *===========================================================================*/

/**
 * @brief Initialization phases for entorhinal cortex
 */
typedef enum {
    ENTORHINAL_INIT_PHASE_NONE = 0,
    ENTORHINAL_INIT_PHASE_PRE_INIT,         /* Configuration validation */
    ENTORHINAL_INIT_PHASE_RESOURCE_ALLOC,   /* Memory allocation */
    ENTORHINAL_INIT_PHASE_CORE_INIT,        /* Core cell initialization */
    ENTORHINAL_INIT_PHASE_BRIDGE_CONNECT,   /* Bridge connections */
    ENTORHINAL_INIT_PHASE_CALIBRATION,      /* Initial calibration */
    ENTORHINAL_INIT_PHASE_SELF_TEST,        /* Validation tests */
    ENTORHINAL_INIT_PHASE_REGISTRATION,     /* KG registration */
    ENTORHINAL_INIT_PHASE_READY,            /* Fully operational */
    ENTORHINAL_INIT_PHASE_ERROR,            /* Initialization failed */
    ENTORHINAL_INIT_PHASE_COUNT
} entorhinal_init_phase_t;

/**
 * @brief Shutdown phases
 */
typedef enum {
    ENTORHINAL_SHUTDOWN_PHASE_NONE = 0,
    ENTORHINAL_SHUTDOWN_PHASE_PREPARE,      /* Prepare for shutdown */
    ENTORHINAL_SHUTDOWN_PHASE_SAVE_STATE,   /* Persist state if needed */
    ENTORHINAL_SHUTDOWN_PHASE_DISCONNECT,   /* Disconnect bridges */
    ENTORHINAL_SHUTDOWN_PHASE_DEREGISTER,   /* Remove from KG */
    ENTORHINAL_SHUTDOWN_PHASE_CLEANUP,      /* Free resources */
    ENTORHINAL_SHUTDOWN_PHASE_COMPLETE,     /* Shutdown complete */
    ENTORHINAL_SHUTDOWN_PHASE_COUNT
} entorhinal_shutdown_phase_t;

/*=============================================================================
 * DEPENDENCY ENUMERATION
 *===========================================================================*/

/**
 * @brief Dependencies that must be satisfied before initialization
 */
typedef enum {
    ENTORHINAL_DEP_NONE = 0,
    ENTORHINAL_DEP_MEMORY_POOL = (1 << 0),      /* Memory allocation ready */
    ENTORHINAL_DEP_LOGGING = (1 << 1),          /* Logging available */
    ENTORHINAL_DEP_SECURITY = (1 << 2),         /* Security context ready */
    ENTORHINAL_DEP_BIO_ASYNC = (1 << 3),        /* Bio-async router ready */
    ENTORHINAL_DEP_BRAIN_KG = (1 << 4),         /* Knowledge graph ready */
    ENTORHINAL_DEP_THALAMUS = (1 << 5),         /* Thalamus for routing */
    ENTORHINAL_DEP_SUBSTRATE = (1 << 6),        /* Neural substrate */
    ENTORHINAL_DEP_ALL = 0x7F                   /* All dependencies */
} entorhinal_dependency_t;

/**
 * @brief Bridge initialization order
 */
typedef enum {
    BRIDGE_INIT_ORDER_SECURITY = 0,     /* Security first */
    BRIDGE_INIT_ORDER_LOGGING,          /* Then logging */
    BRIDGE_INIT_ORDER_SUBSTRATE,        /* Neural substrate */
    BRIDGE_INIT_ORDER_BIO_ASYNC,        /* Communication */
    BRIDGE_INIT_ORDER_SNN,              /* Spiking network */
    BRIDGE_INIT_ORDER_PLASTICITY,       /* Plasticity rules */
    BRIDGE_INIT_ORDER_IMMUNE,           /* Immune system */
    BRIDGE_INIT_ORDER_RESONANCE,        /* Oscillations */
    BRIDGE_INIT_ORDER_THALAMIC,         /* Thalamic relay */
    BRIDGE_INIT_ORDER_COGNITIVE,        /* Cognitive systems */
    BRIDGE_INIT_ORDER_TRAINING,         /* Training adapters */
    BRIDGE_INIT_ORDER_PERCEPTION,       /* Perception input */
    BRIDGE_INIT_ORDER_HIPPOCAMPUS,      /* Hippocampus connection */
    BRIDGE_INIT_ORDER_HYPOTHALAMUS,     /* Motivation */
    BRIDGE_INIT_ORDER_CEREBELLUM,       /* Motor timing */
    BRIDGE_INIT_ORDER_MEDULLA,          /* Vital functions */
    BRIDGE_INIT_ORDER_OMNI,             /* Omnidirectional */
    BRIDGE_INIT_ORDER_SWARM,            /* Swarm coordination */
    BRIDGE_INIT_ORDER_DRAGONFLY,        /* Fast processing */
    BRIDGE_INIT_ORDER_PORTIA,           /* Planning */
    BRIDGE_INIT_ORDER_LOGIC,            /* Logical reasoning */
    BRIDGE_INIT_ORDER_KG,               /* Knowledge graph */
    BRIDGE_INIT_ORDER_COUNT
} bridge_init_order_t;

/*=============================================================================
 * INITIALIZATION STATUS STRUCTURE
 *===========================================================================*/

/**
 * @brief Detailed initialization status
 */
typedef struct {
    /* Current phase */
    entorhinal_init_phase_t current_phase;
    entorhinal_shutdown_phase_t shutdown_phase;

    /* Progress within phase */
    float phase_progress;           /* [0,1] progress in current phase */
    uint32_t current_step;
    uint32_t total_steps;

    /* Dependency status */
    uint32_t dependencies_required;
    uint32_t dependencies_satisfied;
    bool all_dependencies_met;

    /* Bridge initialization status */
    bool bridges_initialized[BRIDGE_INIT_ORDER_COUNT];
    uint32_t bridges_connected;
    uint32_t bridges_total;

    /* Core component status */
    bool grid_cells_initialized;
    bool border_cells_initialized;
    bool hd_cells_initialized;
    bool path_integration_initialized;
    bool memory_gateway_initialized;

    /* Timing */
    uint64_t init_start_time_ms;
    uint64_t init_end_time_ms;
    uint64_t phase_start_time_ms;
    float total_init_time_ms;

    /* Error tracking */
    bool init_failed;
    int error_code;
    char error_message[256];
    entorhinal_init_phase_t failed_phase;

    /* Self-test results */
    bool self_test_passed;
    uint32_t self_test_failures;
    char self_test_report[512];
} entorhinal_init_status_t;

/*=============================================================================
 * INITIALIZATION CALLBACKS
 *===========================================================================*/

/**
 * @brief Callback for initialization phase transitions
 */
typedef void (*entorhinal_init_phase_callback_t)(
    entorhinal_init_phase_t old_phase,
    entorhinal_init_phase_t new_phase,
    void* user_data);

/**
 * @brief Callback for initialization progress
 */
typedef void (*entorhinal_init_progress_callback_t)(
    entorhinal_init_phase_t phase,
    float progress,
    const char* message,
    void* user_data);

/**
 * @brief Callback for initialization errors
 */
typedef void (*entorhinal_init_error_callback_t)(
    entorhinal_init_phase_t phase,
    int error_code,
    const char* error_message,
    void* user_data);

/*=============================================================================
 * BRIDGE CONFIGURATION
 *===========================================================================*/

/**
 * @brief Brain initialization bridge configuration
 */
typedef struct {
    /* Initialization behavior */
    bool async_initialization;          /* Run init in background */
    bool fail_fast;                     /* Stop on first error */
    bool skip_self_test;                /* Skip validation tests */
    bool skip_calibration;              /* Skip initial calibration */
    uint32_t init_timeout_ms;           /* Maximum init time */
    uint32_t retry_count;               /* Retries for failed steps */
    uint32_t retry_delay_ms;            /* Delay between retries */

    /* Dependency handling */
    bool wait_for_dependencies;         /* Block until deps ready */
    uint32_t dependency_timeout_ms;     /* Max wait for deps */
    bool allow_partial_init;            /* Init with missing deps */

    /* Bridge initialization */
    bool parallel_bridge_init;          /* Init bridges in parallel */
    uint32_t bridge_init_timeout_ms;    /* Per-bridge timeout */
    bool optional_bridges_can_fail;     /* Continue if optional fails */

    /* Callbacks */
    entorhinal_init_phase_callback_t phase_callback;
    entorhinal_init_progress_callback_t progress_callback;
    entorhinal_init_error_callback_t error_callback;
    void* callback_user_data;

    /* Logging */
    bool log_init_steps;                /* Log each init step */
    bool log_timing;                    /* Log phase timing */
    nimcp_logger_t* logger;             /* Logger to use */
} entorhinal_brain_init_config_t;

/*=============================================================================
 * BRIDGE STATE STRUCTURE
 *===========================================================================*/

/**
 * @brief Brain initialization bridge state
 */
typedef struct {
    /* Configuration */
    entorhinal_brain_init_config_t config;

    /* Connected systems */
    nimcp_entorhinal_t* entorhinal;
    nimcp_brain_t* brain;
    nimcp_brain_factory_t* factory;
    brain_kg_t* kg;

    /* Initialization status */
    entorhinal_init_status_t status;

    /* Registration info */
    uint32_t kg_node_id;                /* ID in knowledge graph */
    uint32_t factory_component_id;      /* ID in brain factory */
    bool registered_with_kg;
    bool registered_with_factory;

    /* Health monitoring */
    float initial_health_score;
    float current_health_score;
    uint64_t health_check_interval_ms;
    uint64_t last_health_check_ms;

    /* Statistics */
    uint64_t total_init_attempts;
    uint64_t successful_inits;
    uint64_t failed_inits;
    float mean_init_time_ms;
} entorhinal_brain_init_bridge_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
entorhinal_brain_init_config_t entorhinal_brain_init_default_config(void);

/**
 * @brief Create brain init bridge
 */
entorhinal_brain_init_bridge_t* entorhinal_brain_init_bridge_create(
    const entorhinal_brain_init_config_t* config);

/**
 * @brief Destroy brain init bridge
 */
void entorhinal_brain_init_bridge_destroy(
    entorhinal_brain_init_bridge_t* bridge);

/*=============================================================================
 * INITIALIZATION API
 *===========================================================================*/

/**
 * @brief Initialize entorhinal cortex through brain system
 *
 * This is the main entry point for initializing the entorhinal cortex
 * as part of the brain initialization sequence.
 */
int entorhinal_brain_init_initialize(
    entorhinal_brain_init_bridge_t* bridge,
    nimcp_entorhinal_t* entorhinal,
    nimcp_brain_t* brain);

/**
 * @brief Check if dependencies are satisfied
 */
bool entorhinal_brain_init_check_dependencies(
    entorhinal_brain_init_bridge_t* bridge,
    uint32_t required_deps);

/**
 * @brief Wait for dependencies to be satisfied
 */
int entorhinal_brain_init_wait_dependencies(
    entorhinal_brain_init_bridge_t* bridge,
    uint32_t required_deps,
    uint32_t timeout_ms);

/**
 * @brief Execute specific initialization phase
 */
int entorhinal_brain_init_execute_phase(
    entorhinal_brain_init_bridge_t* bridge,
    entorhinal_init_phase_t phase);

/**
 * @brief Advance to next initialization phase
 */
int entorhinal_brain_init_advance_phase(
    entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Initialize specific bridge by order
 */
int entorhinal_brain_init_connect_bridge(
    entorhinal_brain_init_bridge_t* bridge,
    bridge_init_order_t bridge_order);

/**
 * @brief Initialize all bridges in order
 */
int entorhinal_brain_init_connect_all_bridges(
    entorhinal_brain_init_bridge_t* bridge);

/*=============================================================================
 * SHUTDOWN API
 *===========================================================================*/

/**
 * @brief Shutdown entorhinal cortex gracefully
 */
int entorhinal_brain_init_shutdown(
    entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Execute specific shutdown phase
 */
int entorhinal_brain_init_execute_shutdown_phase(
    entorhinal_brain_init_bridge_t* bridge,
    entorhinal_shutdown_phase_t phase);

/**
 * @brief Force immediate shutdown (emergency)
 */
int entorhinal_brain_init_force_shutdown(
    entorhinal_brain_init_bridge_t* bridge);

/*=============================================================================
 * REGISTRATION API
 *===========================================================================*/

/**
 * @brief Register with brain factory
 */
int entorhinal_brain_init_register_factory(
    entorhinal_brain_init_bridge_t* bridge,
    nimcp_brain_factory_t* factory);

/**
 * @brief Register with brain knowledge graph
 */
int entorhinal_brain_init_register_kg(
    entorhinal_brain_init_bridge_t* bridge,
    brain_kg_t* kg);

/**
 * @brief Deregister from all systems
 */
int entorhinal_brain_init_deregister(
    entorhinal_brain_init_bridge_t* bridge);

/*=============================================================================
 * SELF-TEST API
 *===========================================================================*/

/**
 * @brief Run comprehensive self-test
 */
int entorhinal_brain_init_self_test(
    entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Test grid cell functionality
 */
int entorhinal_brain_init_test_grid_cells(
    entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Test path integration
 */
int entorhinal_brain_init_test_path_integration(
    entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Test memory gateway
 */
int entorhinal_brain_init_test_memory_gateway(
    entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Test bridge connections
 */
int entorhinal_brain_init_test_bridges(
    entorhinal_brain_init_bridge_t* bridge);

/*=============================================================================
 * CALIBRATION API
 *===========================================================================*/

/**
 * @brief Run initial calibration
 */
int entorhinal_brain_init_calibrate(
    entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Calibrate grid cells
 */
int entorhinal_brain_init_calibrate_grid_cells(
    entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Calibrate head direction cells
 */
int entorhinal_brain_init_calibrate_hd_cells(
    entorhinal_brain_init_bridge_t* bridge);

/*=============================================================================
 * STATUS API
 *===========================================================================*/

/**
 * @brief Get current initialization status
 */
int entorhinal_brain_init_get_status(
    const entorhinal_brain_init_bridge_t* bridge,
    entorhinal_init_status_t* status_out);

/**
 * @brief Get current phase
 */
entorhinal_init_phase_t entorhinal_brain_init_get_phase(
    const entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Check if fully initialized
 */
bool entorhinal_brain_init_is_ready(
    const entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Check if initialization failed
 */
bool entorhinal_brain_init_has_failed(
    const entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Get initialization error
 */
int entorhinal_brain_init_get_error(
    const entorhinal_brain_init_bridge_t* bridge,
    int* error_code,
    char* error_message,
    size_t message_size);

/**
 * @brief Get phase name string
 */
const char* entorhinal_brain_init_phase_string(
    entorhinal_init_phase_t phase);

/*=============================================================================
 * HEALTH MONITORING API
 *===========================================================================*/

/**
 * @brief Update health status
 */
int entorhinal_brain_init_update_health(
    entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Get current health score
 */
float entorhinal_brain_init_get_health(
    const entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Report health to brain KG
 */
int entorhinal_brain_init_report_health(
    entorhinal_brain_init_bridge_t* bridge);

/*=============================================================================
 * DIAGNOSTICS API
 *===========================================================================*/

/**
 * @brief Log initialization diagnostics
 */
int entorhinal_brain_init_log_diagnostics(
    const entorhinal_brain_init_bridge_t* bridge);

/**
 * @brief Get initialization timing report
 */
int entorhinal_brain_init_get_timing_report(
    const entorhinal_brain_init_bridge_t* bridge,
    char* report_out,
    size_t report_size);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENTORHINAL_BRAIN_INIT_BRIDGE_H */
