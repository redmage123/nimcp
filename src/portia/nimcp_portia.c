//=============================================================================
// nimcp_portia.c - Portia Spider Adaptive Intelligence Implementation
//=============================================================================
/**
 * @file nimcp_portia.c
 * @brief Portia system implementation
 *
 * WHAT: Dynamic resource optimization and platform adaptation
 * WHY:  Enable NIMCP to intelligently adapt to varying hardware constraints
 * HOW:  Monitor resources, adjust tier, handle degradation, coordinate via bio-async
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "portia/nimcp_portia.h"
#include "portia/nimcp_portia_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include "constants/nimcp_timing_constants.h"

#define LOG_MODULE "portia"

//=============================================================================
// Health Agent Integration (Phase 8: System-Wide Health Integration)
//=============================================================================
NIMCP_DECLARE_HEALTH_AGENT(portia)


//=============================================================================
// Subsystem Structures
//=============================================================================

/**
 * @brief Tier manager subsystem
 */
struct portia_tier_manager_t {
    platform_tier_t current_tier;
    platform_tier_t recommended_tier;
    portia_tier_config_t config;
    uint64_t last_switch_time_us;
    uint64_t tier_switch_count;
    nimcp_mutex_t lock;
};

/**
 * @brief Power monitor subsystem
 */
struct portia_power_monitor_t {
    portia_power_state_t current_state;
    portia_power_config_t config;
    float battery_level;
    bool is_on_ac;
    uint64_t last_poll_time_us;
    nimcp_mutex_t lock;
};

/**
 * @brief Resource tracker subsystem
 */
struct portia_resource_tracker_t {
    portia_resource_config_t config;
    system_resources_t current_resources;
    float cpu_usage;
    float memory_usage;
    float temperature_celsius;
    portia_thermal_state_t thermal_state;
    uint64_t last_sample_time_us;
    nimcp_mutex_t lock;
};

/**
 * @brief Degradation controller subsystem
 */
struct portia_degradation_controller_t {
    portia_degradation_level_t current_level;
    portia_degradation_config_t config;
    uint64_t degradation_count;
    uint64_t last_degradation_time_us;
    nimcp_mutex_t lock;
};

/**
 * @brief Accelerator detector subsystem
 */
struct portia_accelerator_detector_t {
    portia_accelerator_config_t config;
    uint32_t num_accelerators;
    portia_accelerator_type_t accelerator_types[8];
    bool detection_complete;
    nimcp_mutex_t lock;
};

/**
 * @brief Sensor fusion subsystem
 */
struct portia_sensor_fusion_t {
    float overall_health;
    float resource_pressure;
    float performance_score;
    float efficiency_score;
    uint64_t last_fusion_time_us;
    nimcp_mutex_t lock;
};

/**
 * @brief Planning engine subsystem
 */
struct portia_planning_engine_t {
    portia_workload_type_t current_workload;
    platform_tier_t planned_tier;
    bool planning_active;
    nimcp_mutex_t lock;
};

/**
 * @brief Target classifier subsystem
 */
struct portia_target_classifier_t {
    portia_workload_type_t classified_workload;
    float classification_confidence;
    uint32_t pattern_id;
    nimcp_mutex_t lock;
};

/**
 * @brief Main Portia context
 */
struct portia_context_t {
    portia_config_t config;
    bool initialized;

    /* Subsystems */
    portia_tier_manager_t* tier_manager;
    portia_power_monitor_t* power_monitor;
    portia_resource_tracker_t* resource_tracker;
    portia_degradation_controller_t* degradation_controller;
    portia_accelerator_detector_t* accelerator_detector;
    portia_sensor_fusion_t* sensor_fusion;
    portia_planning_engine_t* planning_engine;
    portia_target_classifier_t* target_classifier;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;

    /* Statistics */
    uint64_t update_count;
    float total_update_time_ms;
    uint64_t last_update_time_us;

    /* Thread safety */
    nimcp_mutex_t lock;
};

//=============================================================================
// Global Context (Thread-Safe)
//=============================================================================

/* Mutex protecting global state modifications during init/destroy */
static nimcp_mutex_t g_portia_state_mutex = NIMCP_MUTEX_INITIALIZER;

/* Atomic pointer for thread-safe read access without locking */
static _Atomic(portia_context_t*) g_portia_ctx = NULL;

//=============================================================================
// Forward Declarations
//=============================================================================

static nimcp_error_t portia_message_handler(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static int portia_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data);

static nimcp_error_t portia_tier_manager_create(portia_tier_manager_t** out_mgr, const portia_tier_config_t* config);
static void portia_tier_manager_destroy(portia_tier_manager_t* mgr);
static nimcp_error_t portia_tier_manager_update(portia_tier_manager_t* mgr, const system_resources_t* resources);

static nimcp_error_t portia_power_monitor_create(portia_power_monitor_t** out_mon, const portia_power_config_t* config);
static void portia_power_monitor_destroy(portia_power_monitor_t* mon);
static nimcp_error_t portia_power_monitor_update(portia_power_monitor_t* mon);

static nimcp_error_t portia_resource_tracker_create(portia_resource_tracker_t** out_tracker, const portia_resource_config_t* config);
static void portia_resource_tracker_destroy(portia_resource_tracker_t* tracker);
static nimcp_error_t portia_resource_tracker_update(portia_resource_tracker_t* tracker);

static nimcp_error_t portia_degradation_controller_create(portia_degradation_controller_t** out_ctrl, const portia_degradation_config_t* config);
static void portia_degradation_controller_destroy(portia_degradation_controller_t* ctrl);
static nimcp_error_t portia_degradation_controller_update(portia_degradation_controller_t* ctrl, const portia_status_t* status);

static nimcp_error_t portia_accelerator_detector_create(portia_accelerator_detector_t** out_det, const portia_accelerator_config_t* config);
static void portia_accelerator_detector_destroy(portia_accelerator_detector_t* det);
static nimcp_error_t portia_accelerator_detector_scan(portia_accelerator_detector_t* det);

static nimcp_error_t portia_sensor_fusion_create(portia_sensor_fusion_t** out_fusion);
static void portia_sensor_fusion_destroy(portia_sensor_fusion_t* fusion);
static nimcp_error_t portia_sensor_fusion_update(portia_sensor_fusion_t* fusion, const portia_status_t* status);

static nimcp_error_t portia_planning_engine_create(portia_planning_engine_t** out_planner);
static void portia_planning_engine_destroy(portia_planning_engine_t* planner);

static nimcp_error_t portia_target_classifier_create(portia_target_classifier_t** out_classifier);
static void portia_target_classifier_destroy(portia_target_classifier_t* classifier);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_portia_part_accessors.c"  // 8 functions: accessors
#include "nimcp_portia_part_lifecycle.c"  // 21 functions: lifecycle
#include "nimcp_portia_part_processing.c"  // 8 functions: processing
#include "nimcp_portia_part_core.c"  // 5 functions: core
