//=============================================================================
// nimcp_portia.h - Portia Spider Adaptive Intelligence System
//=============================================================================
/**
 * @file nimcp_portia.h
 * @brief Portia fimbriata-inspired adaptive intelligence for resource-constrained environments
 *
 * WHAT: Dynamic resource optimization and platform adaptation system
 * WHY:  Enable NIMCP to intelligently adapt to varying hardware constraints
 * HOW:  Real-time monitoring, tier switching, graceful degradation, sensor fusion
 *
 * PORTIA SPIDER INTELLIGENCE:
 * Named after Portia fimbriata, a jumping spider that demonstrates remarkable
 * cognitive flexibility despite having only ~600,000 neurons. Portia exhibits:
 * - Trial-and-error learning
 * - Planning and problem-solving
 * - Adaptive hunting strategies based on available resources
 * - Graceful degradation when energy-constrained
 *
 * CORE SUBSYSTEMS:
 * 1. Tier Manager - Dynamic platform tier switching
 * 2. Power Monitor - Battery/energy awareness
 * 3. Resource Tracker - CPU/memory/thermal monitoring
 * 4. Degradation Controller - Graceful feature reduction
 * 5. Accelerator Detector - GPU/NPU/TPU discovery
 * 6. Sensor Fusion - Multi-metric decision making
 * 7. Planning Engine - Strategic resource allocation
 * 8. Target Classifier - Workload characterization
 *
 * INTEGRATION:
 * Brain → portia_init() → monitor resources → adapt tier → broadcast changes
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_PORTIA_H
#define NIMCP_PORTIA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/platform/nimcp_platform_tier.h"
#include "utils/platform/nimcp_system_resources.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct portia_context_t portia_context_t;
typedef struct portia_tier_manager_t portia_tier_manager_t;
typedef struct portia_power_monitor_t portia_power_monitor_t;
typedef struct portia_resource_tracker_t portia_resource_tracker_t;
typedef struct portia_degradation_controller_t portia_degradation_controller_t;
typedef struct portia_accelerator_detector_t portia_accelerator_detector_t;
typedef struct portia_sensor_fusion_t portia_sensor_fusion_t;
typedef struct portia_planning_engine_t portia_planning_engine_t;
typedef struct portia_target_classifier_t portia_target_classifier_t;

//=============================================================================
// Error Codes
//=============================================================================

/* Include canonical error type definition */
#ifndef NIMCP_ERROR_TYPE_DEFINED
#include "utils/error/nimcp_error_codes.h"
#endif

#define NIMCP_PORTIA_ERROR_BASE 20000
#define NIMCP_PORTIA_ERROR_NOT_INITIALIZED      (NIMCP_PORTIA_ERROR_BASE + 1)
#define NIMCP_PORTIA_ERROR_ALREADY_INITIALIZED  (NIMCP_PORTIA_ERROR_BASE + 2)
#define NIMCP_PORTIA_ERROR_TIER_LOCKED          (NIMCP_PORTIA_ERROR_BASE + 3)
#define NIMCP_PORTIA_ERROR_POWER_CRITICAL       (NIMCP_PORTIA_ERROR_BASE + 4)
#define NIMCP_PORTIA_ERROR_THERMAL_THROTTLE     (NIMCP_PORTIA_ERROR_BASE + 5)
#define NIMCP_PORTIA_ERROR_NO_ACCELERATOR       (NIMCP_PORTIA_ERROR_BASE + 6)
#define NIMCP_PORTIA_ERROR_DEGRADATION_FAILED   (NIMCP_PORTIA_ERROR_BASE + 7)

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Power state enumeration
 */
typedef enum {
    PORTIA_POWER_AC = 0,           /**< Plugged in, unlimited power */
    PORTIA_POWER_BATTERY_FULL = 1, /**< Battery > 80% */
    PORTIA_POWER_BATTERY_MID = 2,  /**< Battery 20-80% */
    PORTIA_POWER_BATTERY_LOW = 3,  /**< Battery 5-20% */
    PORTIA_POWER_BATTERY_CRITICAL = 4, /**< Battery < 5% */
    PORTIA_POWER_UNKNOWN = 5       /**< Unable to determine */
} portia_power_state_t;

/**
 * @brief Thermal state enumeration
 */
typedef enum {
    PORTIA_THERMAL_NOMINAL = 0,    /**< Normal operating temperature */
    PORTIA_THERMAL_WARM = 1,       /**< Elevated but safe */
    PORTIA_THERMAL_HOT = 2,        /**< Approaching throttle threshold */
    PORTIA_THERMAL_THROTTLED = 3,  /**< System throttling active */
    PORTIA_THERMAL_CRITICAL = 4    /**< Critical temperature */
} portia_thermal_state_t;

/**
 * @brief Accelerator type enumeration
 */
typedef enum {
    PORTIA_ACCEL_NONE = 0,         /**< CPU only */
    PORTIA_ACCEL_GPU = 1,          /**< Graphics Processing Unit */
    PORTIA_ACCEL_NPU = 2,          /**< Neural Processing Unit */
    PORTIA_ACCEL_TPU = 3,          /**< Tensor Processing Unit */
    PORTIA_ACCEL_DSP = 4,          /**< Digital Signal Processor */
    PORTIA_ACCEL_FPGA = 5,         /**< Field-Programmable Gate Array */
    PORTIA_ACCEL_ASIC = 6          /**< Application-Specific IC */
} portia_accelerator_type_t;

/**
 * @brief Workload type enumeration
 */
typedef enum {
    PORTIA_WORKLOAD_TRAINING = 0,  /**< Model training (high resource) */
    PORTIA_WORKLOAD_INFERENCE = 1, /**< Inference (moderate) */
    PORTIA_WORKLOAD_MONITORING = 2,/**< Background monitoring (light) */
    PORTIA_WORKLOAD_IDLE = 3,      /**< Idle/standby (minimal) */
    PORTIA_WORKLOAD_UNKNOWN = 4    /**< Unknown workload */
} portia_workload_type_t;

/**
 * @brief Degradation level enumeration
 */
typedef enum {
    PORTIA_DEGRADATION_NONE = 0,   /**< Full functionality */
    PORTIA_DEGRADATION_MINOR = 1,  /**< Disable minor features */
    PORTIA_DEGRADATION_MODERATE = 2, /**< Reduce precision/resolution */
    PORTIA_DEGRADATION_SEVERE = 3, /**< Minimal essential features */
    PORTIA_DEGRADATION_EMERGENCY = 4 /**< Survival mode */
} portia_degradation_level_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Tier manager configuration
 */
typedef struct {
    bool enable_auto_switching;    /**< Allow automatic tier changes */
    uint32_t switch_hysteresis_ms; /**< Delay before switching tiers */
    float upgrade_threshold;       /**< Resource threshold to upgrade (0-1) */
    float downgrade_threshold;     /**< Resource threshold to downgrade (0-1) */
    bool lock_tier;                /**< Lock to current tier */
} portia_tier_config_t;

/**
 * @brief Power monitor configuration
 */
typedef struct {
    bool enable_battery_awareness; /**< Monitor battery state */
    uint32_t poll_interval_ms;     /**< How often to check battery */
    float low_battery_threshold;   /**< Threshold for low battery (0-1) */
    float critical_battery_threshold; /**< Critical threshold (0-1) */
    bool enable_ac_detection;      /**< Detect AC power */
} portia_power_config_t;

/**
 * @brief Resource tracker configuration
 */
typedef struct {
    uint32_t sample_interval_ms;   /**< How often to sample resources */
    uint32_t history_size;         /**< Number of samples to keep */
    float cpu_threshold;           /**< CPU usage threshold (0-1) */
    float memory_threshold;        /**< Memory usage threshold (0-1) */
    float thermal_threshold;       /**< Temperature threshold (celsius) */
} portia_resource_config_t;

/**
 * @brief Degradation controller configuration
 */
typedef struct {
    bool enable_graceful_degradation; /**< Allow feature reduction */
    portia_degradation_level_t max_degradation; /**< Maximum allowed degradation */
    uint32_t recovery_delay_ms;    /**< Wait time before recovering features */
    float recovery_threshold;      /**< Resource threshold to recover */
} portia_degradation_config_t;

/**
 * @brief Accelerator detector configuration
 */
typedef struct {
    bool enable_gpu_detection;     /**< Detect GPU */
    bool enable_npu_detection;     /**< Detect NPU */
    bool enable_auto_offload;      /**< Automatically offload to accelerators */
    uint32_t detection_timeout_ms; /**< Max time for detection */
} portia_accelerator_config_t;

/**
 * @brief Main Portia configuration
 */
typedef struct {
    portia_tier_config_t tier_config;
    portia_power_config_t power_config;
    portia_resource_config_t resource_config;
    portia_degradation_config_t degradation_config;
    portia_accelerator_config_t accelerator_config;

    /* General settings */
    bool enable_bio_async;         /**< Enable bio-async messaging */
    uint32_t update_interval_ms;   /**< Main update loop interval */
    bool enable_logging;           /**< Enable debug logging */
    bool enable_metrics;           /**< Track performance metrics */
} portia_config_t;

//=============================================================================
// Status Structures
//=============================================================================

/**
 * @brief Current Portia system status
 */
typedef struct {
    /* Current state */
    platform_tier_t current_tier;
    portia_power_state_t power_state;
    portia_thermal_state_t thermal_state;
    portia_degradation_level_t degradation_level;
    portia_workload_type_t current_workload;

    /* Resource metrics */
    float cpu_usage;               /**< CPU utilization (0-1) */
    float memory_usage;            /**< Memory utilization (0-1) */
    float battery_level;           /**< Battery level (0-1, -1 if N/A) */
    float temperature_celsius;     /**< System temperature */

    /* Accelerators */
    uint32_t num_accelerators;     /**< Number of detected accelerators */
    portia_accelerator_type_t accelerator_types[8]; /**< Accelerator types */

    /* Statistics */
    uint64_t tier_switches;        /**< Number of tier changes */
    uint64_t degradations;         /**< Number of degradation events */
    uint64_t updates;              /**< Number of update cycles */
    float avg_update_time_ms;      /**< Average update time */
} portia_status_t;

//=============================================================================
// Main API Functions
//=============================================================================

/**
 * @brief Get default Portia configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provide easy initialization for common use cases
 * HOW:  Returns pre-configured structure with safe defaults
 *
 * @return Default configuration structure
 */
portia_config_t portia_get_default_config(void);

/**
 * @brief Initialize Portia system
 *
 * WHAT: Initialize all Portia subsystems
 * WHY:  Must be called before using Portia
 * HOW:  Creates context, initializes subsystems, registers with bio-router
 *
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_SUCCESS or error code
 *
 * THREAD SAFETY: Must be called from single thread
 * BIO-ASYNC: Registers module "portia" if bio-async enabled
 */
nimcp_error_t portia_init(const portia_config_t* config);

/**
 * @brief Shutdown Portia system
 *
 * WHAT: Clean shutdown of all subsystems
 * WHY:  Free resources, unregister from bio-router
 * HOW:  Destroys all subsystems, frees context
 *
 * THREAD SAFETY: Must be called from single thread
 */
void portia_destroy(void);

/**
 * @brief Check if Portia is initialized
 *
 * @return true if initialized, false otherwise
 */
bool portia_is_initialized(void);

/**
 * @brief Get Portia context
 *
 * WHAT: Returns the global Portia context
 * WHY:  Allow integration modules to access Portia context
 * HOW:  Returns pointer to singleton context
 *
 * @return Portia context or NULL if not initialized
 *
 * THREAD SAFETY: Thread-safe
 */
portia_context_t* portia_get_context(void);

/**
 * @brief Update Portia system
 *
 * WHAT: Main update loop for Portia
 * WHY:  Monitor resources, adjust tier, handle degradation
 * HOW:  Polls all sensors, runs decision logic, broadcasts events
 *
 * @return NIMCP_SUCCESS or error code
 *
 * USAGE: Call periodically (e.g., every 100ms) or let bio-async handle it
 * THREAD SAFETY: Thread-safe
 */
nimcp_error_t portia_update(void);

/**
 * @brief Get current Portia status
 *
 * WHAT: Query current state of all subsystems
 * WHY:  Allow external monitoring and debugging
 * HOW:  Populates status structure with current values
 *
 * @param status Output status structure
 * @return NIMCP_SUCCESS or error code
 *
 * THREAD SAFETY: Thread-safe
 */
nimcp_error_t portia_get_status(portia_status_t* status);

/**
 * @brief Force tier change
 *
 * WHAT: Manually override automatic tier selection
 * WHY:  User may want explicit control
 * HOW:  Validates tier, updates config, broadcasts change
 *
 * @param tier New tier to switch to
 * @return NIMCP_SUCCESS or error code
 *
 * ERRORS:
 * - NIMCP_PORTIA_ERROR_TIER_LOCKED: Tier switching disabled
 * - NIMCP_ERROR_INVALID_ARG: Invalid tier value
 */
nimcp_error_t portia_set_tier(platform_tier_t tier);

/**
 * @brief Get current platform tier
 *
 * @return Current tier
 */
platform_tier_t portia_get_current_tier(void);

/**
 * @brief Set degradation level
 *
 * WHAT: Manually set degradation level
 * WHY:  User may want explicit power saving
 * HOW:  Updates degradation controller, broadcasts event
 *
 * @param level Degradation level to set
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t portia_set_degradation_level(portia_degradation_level_t level);

/**
 * @brief Get recommended neuron count for current state
 *
 * WHAT: Calculate optimal neuron count based on current resources
 * WHY:  Allow dynamic brain resizing
 * HOW:  Considers tier, power, thermal state, degradation
 *
 * @return Recommended neuron count
 *
 * THREAD SAFETY: Thread-safe
 */
uint32_t portia_recommend_neuron_count(void);

/**
 * @brief Enable/disable automatic tier switching
 *
 * @param enable True to enable, false to disable
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t portia_set_auto_switching(bool enable);

/**
 * @brief Get detected accelerators
 *
 * WHAT: Query available hardware accelerators
 * WHY:  Allow system to utilize specialized hardware
 * HOW:  Returns array of detected accelerator types
 *
 * @param out_accelerators Output array
 * @param max_accelerators Size of output array
 * @param out_count Number of accelerators found
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t portia_get_accelerators(
    portia_accelerator_type_t* out_accelerators,
    uint32_t max_accelerators,
    uint32_t* out_count
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get power state name
 */
const char* portia_power_state_name(portia_power_state_t state);

/**
 * @brief Get thermal state name
 */
const char* portia_thermal_state_name(portia_thermal_state_t state);

/**
 * @brief Get accelerator type name (for portia_accelerator_type_t)
 */
const char* portia_accel_type_name(portia_accelerator_type_t type);

/**
 * @brief Get workload type name
 */
const char* portia_workload_type_name(portia_workload_type_t type);

/**
 * @brief Get degradation level name
 */
const char* portia_degradation_level_name(portia_degradation_level_t level);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PORTIA_H
