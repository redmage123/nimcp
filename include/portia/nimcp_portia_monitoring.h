//=============================================================================
// nimcp_portia_monitoring.h - Platform Monitoring for Portia System
//=============================================================================
/**
 * @file nimcp_portia_monitoring.h
 * @brief Cross-platform system monitoring for CPU temperature, battery, and load
 *
 * WHAT: Real-time system metric collection for resource-aware adaptation
 * WHY:  Enable dynamic tier switching based on actual hardware conditions
 * HOW:  Platform-specific implementations with fallback mechanisms
 *
 * PORTIA BIOLOGICAL ANALOGY:
 * Just as Portia fimbriata monitors its metabolic state and environmental
 * conditions before hunting, NIMCP monitors system resources to optimize
 * computational strategies.
 *
 * PLATFORM SUPPORT:
 * - Linux: /sys/class/thermal/, /sys/class/power_supply/, /proc/stat
 * - macOS: IOKit thermal sensors, IOPSCopyPowerSourcesInfo()
 * - Windows: WMI queries, GetSystemPowerStatus()
 * - Fallback: Stub implementations returning safe defaults
 *
 * THREAD SAFETY:
 * All functions are thread-safe with internal caching to minimize syscalls.
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 * @version 1.0.0
 */

#ifndef NIMCP_PORTIA_MONITORING_H
#define NIMCP_PORTIA_MONITORING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Export Macro
//=============================================================================

#include "common/nimcp_export.h"

//=============================================================================
// Constants
//=============================================================================

/** @brief Invalid temperature reading indicator */
#define PORTIA_MONITOR_TEMP_INVALID (-273.15f)

/** @brief Invalid battery reading indicator (not available) */
#define PORTIA_MONITOR_BATTERY_UNAVAILABLE (-1.0f)

/** @brief Invalid CPU load reading indicator */
#define PORTIA_MONITOR_LOAD_INVALID (-1.0f)

/** @brief Default cache timeout in milliseconds */
#define PORTIA_MONITOR_DEFAULT_CACHE_MS 1000

/** @brief Maximum number of thermal zones to track */
#define PORTIA_MONITOR_MAX_THERMAL_ZONES 16

//=============================================================================
// Monitoring Capability Flags
//=============================================================================

/**
 * @brief Platform monitoring capabilities bitmask
 */
typedef enum {
    PORTIA_MONITOR_CAP_NONE         = 0x00,
    PORTIA_MONITOR_CAP_CPU_TEMP     = 0x01,  /**< CPU temperature monitoring */
    PORTIA_MONITOR_CAP_BATTERY      = 0x02,  /**< Battery level monitoring */
    PORTIA_MONITOR_CAP_CPU_LOAD     = 0x04,  /**< CPU load monitoring */
    PORTIA_MONITOR_CAP_DISK_TEMP    = 0x08,  /**< Disk temperature monitoring */
    PORTIA_MONITOR_CAP_GPU_TEMP     = 0x10,  /**< GPU temperature monitoring */
    PORTIA_MONITOR_CAP_FAN_SPEED    = 0x20,  /**< Fan speed monitoring */
    PORTIA_MONITOR_CAP_POWER_DRAW   = 0x40,  /**< Power draw monitoring */
    PORTIA_MONITOR_CAP_ALL          = 0x7F
} portia_monitor_capability_t;

//=============================================================================
// Thermal Zone Information
//=============================================================================

/**
 * @brief Thermal zone type classification
 */
typedef enum {
    PORTIA_THERMAL_ZONE_CPU,        /**< CPU package or core temperature */
    PORTIA_THERMAL_ZONE_GPU,        /**< GPU temperature */
    PORTIA_THERMAL_ZONE_SSD,        /**< SSD/NVMe temperature */
    PORTIA_THERMAL_ZONE_MEMORY,     /**< Memory module temperature */
    PORTIA_THERMAL_ZONE_BATTERY,    /**< Battery temperature */
    PORTIA_THERMAL_ZONE_AMBIENT,    /**< Ambient/chassis temperature */
    PORTIA_THERMAL_ZONE_UNKNOWN     /**< Unknown thermal zone type */
} portia_thermal_zone_type_t;

/**
 * @brief Information about a single thermal zone
 */
typedef struct {
    char name[64];                      /**< Zone name (e.g., "x86_pkg_temp") */
    portia_thermal_zone_type_t type;    /**< Zone type classification */
    float temperature_c;                /**< Current temperature (Celsius) */
    float trip_point_c;                 /**< Critical trip point (if available) */
    bool available;                     /**< Zone is readable */
    int zone_index;                     /**< Zone index in sysfs */
} portia_thermal_zone_t;

//=============================================================================
// Battery Information
//=============================================================================

/**
 * @brief Battery charging state
 */
typedef enum {
    PORTIA_BATTERY_DISCHARGING,     /**< Running on battery */
    PORTIA_BATTERY_CHARGING,        /**< Battery is charging */
    PORTIA_BATTERY_FULL,            /**< Battery fully charged */
    PORTIA_BATTERY_NOT_PRESENT,     /**< No battery installed */
    PORTIA_BATTERY_UNKNOWN          /**< Unknown state */
} portia_battery_state_t;

/**
 * @brief Comprehensive battery status information
 */
typedef struct {
    float level_pct;                    /**< Battery level (0-100%) */
    portia_battery_state_t state;       /**< Charging state */
    float power_draw_mw;                /**< Current power draw (milliwatts) */
    float voltage_mv;                   /**< Current voltage (millivolts) */
    float temperature_c;                /**< Battery temperature (Celsius) */
    float time_to_empty_s;              /**< Estimated time to empty (seconds) */
    float time_to_full_s;               /**< Estimated time to full (seconds) */
    float design_capacity_mah;          /**< Design capacity (mAh) */
    float current_capacity_mah;         /**< Current max capacity (mAh) */
    float health_pct;                   /**< Battery health (0-100%) */
    bool ac_connected;                  /**< AC adapter connected */
    bool available;                     /**< Battery info is available */
    uint64_t timestamp_us;              /**< Reading timestamp */
} portia_battery_status_t;

//=============================================================================
// CPU Load Information
//=============================================================================

/**
 * @brief Per-CPU core load information
 */
typedef struct {
    uint32_t core_id;                   /**< Logical core ID */
    float load_pct;                     /**< Core load (0-100%) */
    float user_pct;                     /**< User time percentage */
    float system_pct;                   /**< System/kernel time percentage */
    float iowait_pct;                   /**< I/O wait percentage */
    float idle_pct;                     /**< Idle time percentage */
} portia_cpu_core_load_t;

/**
 * @brief System-wide CPU load information
 */
typedef struct {
    float total_load_pct;               /**< Overall CPU load (0-100%) */
    float user_pct;                     /**< Total user time */
    float system_pct;                   /**< Total system time */
    float iowait_pct;                   /**< Total I/O wait time */
    float idle_pct;                     /**< Total idle time */
    uint32_t num_cores;                 /**< Number of logical cores */
    float load_1m;                      /**< 1-minute load average */
    float load_5m;                      /**< 5-minute load average */
    float load_15m;                     /**< 15-minute load average */
    bool available;                     /**< Load info is available */
    uint64_t timestamp_us;              /**< Reading timestamp */
} portia_cpu_load_t;

//=============================================================================
// Monitoring Configuration
//=============================================================================

/**
 * @brief Configuration for monitoring system
 */
typedef struct {
    uint32_t cache_timeout_ms;          /**< Cache timeout (def: 1000ms) */
    bool enable_cpu_temp;               /**< Enable CPU temperature monitoring */
    bool enable_battery;                /**< Enable battery monitoring */
    bool enable_cpu_load;               /**< Enable CPU load monitoring */
    bool enable_per_core_load;          /**< Enable per-core load tracking */
    const char* thermal_zone_override;  /**< Override thermal zone path */
    const char* battery_path_override;  /**< Override battery path */
} portia_monitor_config_t;

//=============================================================================
// Opaque Handle
//=============================================================================

/**
 * @brief Opaque monitoring system handle
 */
typedef struct portia_monitor_struct* portia_monitor_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default monitoring configuration
 *
 * WHAT: Return sensible defaults for monitoring
 * WHY:  Simplify initialization
 * HOW:  Pre-populate with recommended values
 *
 * @return Default configuration
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT portia_monitor_config_t portia_monitor_default_config(void);

/**
 * @brief Initialize monitoring system
 *
 * WHAT: Create monitoring context with platform detection
 * WHY:  Enable system metric collection
 * HOW:  Detect platform capabilities, initialize caches
 *
 * INITIALIZATION FLOW:
 * 1. Detect platform (Linux, macOS, Windows)
 * 2. Probe for available thermal zones
 * 3. Probe for battery presence
 * 4. Initialize /proc/stat parsing (Linux)
 * 5. Initialize caching mechanisms
 *
 * @param config Configuration (NULL for defaults)
 * @return Monitor handle or NULL on failure
 *
 * THREAD SAFETY: Thread-safe
 * COMPLEXITY: O(n) where n = number of thermal zones
 */
NIMCP_EXPORT portia_monitor_t portia_monitor_init(
    const portia_monitor_config_t* config);

/**
 * @brief Shutdown monitoring system
 *
 * WHAT: Clean shutdown of monitoring
 * WHY:  Release resources
 * HOW:  Close file handles, free memory
 *
 * @param monitor Monitor handle
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT void portia_monitor_shutdown(portia_monitor_t monitor);

/**
 * @brief Query platform monitoring capabilities
 *
 * WHAT: Determine what metrics are available
 * WHY:  Allow graceful handling of missing sensors
 * HOW:  Return bitmask of available capabilities
 *
 * @param monitor Monitor handle (NULL to probe without init)
 * @return Capability bitmask
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT uint32_t portia_monitor_get_capabilities(portia_monitor_t monitor);

//=============================================================================
// CPU Temperature API
//=============================================================================

/**
 * @brief Get current CPU temperature
 *
 * WHAT: Query CPU package or core temperature
 * WHY:  Enable thermal-aware tier switching
 * HOW:  Read from sysfs thermal zone (Linux) or IOKit (macOS)
 *
 * FALLBACK BEHAVIOR:
 * - If CPU temp unavailable, returns PORTIA_MONITOR_TEMP_INVALID
 * - Caller should check return value before use
 *
 * CACHING:
 * - Results cached for cache_timeout_ms
 * - Multiple calls within cache window return cached value
 *
 * @param monitor Monitor handle
 * @return CPU temperature in Celsius, or PORTIA_MONITOR_TEMP_INVALID
 *
 * THREAD SAFETY: Thread-safe (uses internal mutex)
 * COMPLEXITY: O(1) with cache hit, O(1) syscall with cache miss
 */
NIMCP_EXPORT float portia_monitor_get_cpu_temp(portia_monitor_t monitor);

/**
 * @brief Get CPU temperature with critical threshold
 *
 * WHAT: Query CPU temp and check against threshold
 * WHY:  Quick thermal emergency detection
 * HOW:  Read temp and return comparison result
 *
 * @param monitor Monitor handle
 * @param temp_out Output: current temperature (optional)
 * @param threshold_c Critical threshold to compare against
 * @return true if temp >= threshold (thermal emergency)
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool portia_monitor_cpu_temp_critical(
    portia_monitor_t monitor,
    float* temp_out,
    float threshold_c);

/**
 * @brief Get all thermal zone information
 *
 * WHAT: Query all available thermal sensors
 * WHY:  Comprehensive thermal monitoring
 * HOW:  Enumerate and read all sysfs thermal zones
 *
 * @param monitor Monitor handle
 * @param zones Output array for zone information
 * @param max_zones Maximum zones to return
 * @return Number of zones populated
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT int portia_monitor_get_thermal_zones(
    portia_monitor_t monitor,
    portia_thermal_zone_t* zones,
    int max_zones);

//=============================================================================
// Battery API
//=============================================================================

/**
 * @brief Get current battery percentage
 *
 * WHAT: Query battery charge level
 * WHY:  Enable battery-aware resource management
 * HOW:  Read from sysfs power_supply (Linux) or IOKit (macOS)
 *
 * FALLBACK BEHAVIOR:
 * - If no battery, returns PORTIA_MONITOR_BATTERY_UNAVAILABLE
 * - On AC without battery, returns 100.0f
 *
 * @param monitor Monitor handle
 * @return Battery percentage (0-100), or PORTIA_MONITOR_BATTERY_UNAVAILABLE
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT float portia_monitor_get_battery_pct(portia_monitor_t monitor);

/**
 * @brief Get comprehensive battery status
 *
 * WHAT: Query detailed battery information
 * WHY:  Enable sophisticated power management
 * HOW:  Read multiple sysfs files or IOKit properties
 *
 * @param monitor Monitor handle
 * @param status Output battery status structure
 * @return true on success, false if battery unavailable
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool portia_monitor_get_battery_status(
    portia_monitor_t monitor,
    portia_battery_status_t* status);

/**
 * @brief Check if running on battery power
 *
 * WHAT: Determine if system is on battery
 * WHY:  Quick check for power-constrained operation
 * HOW:  Check AC adapter status
 *
 * @param monitor Monitor handle
 * @return true if on battery, false if on AC or no battery
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool portia_monitor_on_battery(portia_monitor_t monitor);

/**
 * @brief Check if battery is below critical threshold
 *
 * WHAT: Quick battery emergency check
 * WHY:  Trigger emergency power-saving measures
 * HOW:  Compare battery level against threshold
 *
 * @param monitor Monitor handle
 * @param level_out Output: current battery level (optional)
 * @param threshold_pct Critical threshold percentage
 * @return true if battery <= threshold (battery critical)
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool portia_monitor_battery_critical(
    portia_monitor_t monitor,
    float* level_out,
    float threshold_pct);

//=============================================================================
// CPU Load API
//=============================================================================

/**
 * @brief Get current CPU load percentage
 *
 * WHAT: Query overall CPU utilization
 * WHY:  Enable load-aware tier switching
 * HOW:  Parse /proc/stat (Linux) or use host_processor_info (macOS)
 *
 * CALCULATION:
 * - Load = (user + system + nice) / total * 100
 * - Requires two samples to calculate delta
 * - First call may return 0 or stale value
 *
 * @param monitor Monitor handle
 * @return CPU load percentage (0-100), or PORTIA_MONITOR_LOAD_INVALID
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT float portia_monitor_get_cpu_load(portia_monitor_t monitor);

/**
 * @brief Get detailed CPU load information
 *
 * WHAT: Query comprehensive CPU utilization metrics
 * WHY:  Enable sophisticated load analysis
 * HOW:  Parse /proc/stat and /proc/loadavg
 *
 * @param monitor Monitor handle
 * @param load Output load information
 * @return true on success
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool portia_monitor_get_cpu_load_detailed(
    portia_monitor_t monitor,
    portia_cpu_load_t* load);

/**
 * @brief Get per-core CPU load
 *
 * WHAT: Query load for each CPU core
 * WHY:  Detect unbalanced load or single-threaded bottlenecks
 * HOW:  Parse per-CPU lines from /proc/stat
 *
 * @param monitor Monitor handle
 * @param cores Output array for per-core load
 * @param max_cores Maximum cores to query
 * @return Number of cores populated
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT int portia_monitor_get_per_core_load(
    portia_monitor_t monitor,
    portia_cpu_core_load_t* cores,
    int max_cores);

/**
 * @brief Check if CPU load exceeds threshold
 *
 * WHAT: Quick CPU load emergency check
 * WHY:  Trigger load-based tier downgrade
 * HOW:  Compare load against threshold
 *
 * @param monitor Monitor handle
 * @param load_out Output: current CPU load (optional)
 * @param threshold_pct Load threshold percentage
 * @return true if load >= threshold (load spike)
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool portia_monitor_cpu_load_high(
    portia_monitor_t monitor,
    float* load_out,
    float threshold_pct);

//=============================================================================
// Convenience API
//=============================================================================

/**
 * @brief Get all monitoring metrics in one call
 *
 * WHAT: Query CPU temp, battery, and load atomically
 * WHY:  Efficient single-call monitoring for tier switch evaluation
 * HOW:  Read all metrics with shared cache
 *
 * @param monitor Monitor handle
 * @param cpu_temp_c Output: CPU temperature (or PORTIA_MONITOR_TEMP_INVALID)
 * @param battery_pct Output: Battery percentage (or PORTIA_MONITOR_BATTERY_UNAVAILABLE)
 * @param cpu_load_pct Output: CPU load (or PORTIA_MONITOR_LOAD_INVALID)
 * @return true if at least one metric was successfully read
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool portia_monitor_get_all(
    portia_monitor_t monitor,
    float* cpu_temp_c,
    float* battery_pct,
    float* cpu_load_pct);

/**
 * @brief Force cache refresh on next query
 *
 * WHAT: Invalidate cached readings
 * WHY:  Get fresh data after known state change
 * HOW:  Reset cache timestamps
 *
 * @param monitor Monitor handle
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT void portia_monitor_refresh(portia_monitor_t monitor);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get thermal zone type name
 *
 * @param type Thermal zone type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* portia_monitor_thermal_zone_name(
    portia_thermal_zone_type_t type);

/**
 * @brief Get battery state name
 *
 * @param state Battery state
 * @return Human-readable name
 */
NIMCP_EXPORT const char* portia_monitor_battery_state_name(
    portia_battery_state_t state);

/**
 * @brief Check if temperature value is valid
 *
 * @param temp Temperature value to check
 * @return true if valid reading
 */
NIMCP_EXPORT bool portia_monitor_temp_valid(float temp);

/**
 * @brief Check if battery value is valid
 *
 * @param battery Battery percentage to check
 * @return true if valid reading
 */
NIMCP_EXPORT bool portia_monitor_battery_valid(float battery);

/**
 * @brief Check if load value is valid
 *
 * @param load Load percentage to check
 * @return true if valid reading
 */
NIMCP_EXPORT bool portia_monitor_load_valid(float load);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PORTIA_MONITORING_H
