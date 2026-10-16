/**
 * @file nimcp_portia_power.h
 * @brief Power-Aware Tier System for Portia Spider (Battery Management)
 *
 * WHAT: Battery monitoring and power profile management for mobile/embedded platforms
 * WHY:  Adapt NIMCP resource usage to battery state and power source
 * HOW:  Monitor power_supply sysfs, auto-adjust tier configs based on battery level
 *
 * BIOLOGICAL INSPIRATION:
 * Named after Portia fimbriata, the jumping spider that optimizes energy expenditure
 * based on metabolic state. Just as Portia switches between high-energy hunting and
 * low-energy waiting based on hunger, NIMCP adapts computational intensity to power
 * availability.
 *
 * POWER PROFILES:
 * - PERFORMANCE (>80% or AC): Full capabilities, all modules enabled
 * - BALANCED (40-80%): Normal operation, most features active
 * - SAVER (20-40%): Reduced features, lower update rates
 * - CRITICAL (10-20%): Minimal operation, essential modules only
 * - EMERGENCY (<10%): Survival mode, reactive processing only
 *
 * INTEGRATION:
 * 1. Initialize: portia_power_init() - Start power monitoring
 * 2. Query: portia_power_get_status() - Check current power state
 * 3. Profile: portia_power_get_profile() - Get recommended profile
 * 4. Apply: portia_power_get_tier_config() - Get tier config for profile
 * 5. Events: Bio-async messages on battery threshold crossings
 *
 * LINUX INTEGRATION:
 * Reads from /sys/class/power_supply/BAT0/ or BAT1/:
 * - status: Charging/Discharging/Full
 * - capacity: Battery percentage (0-100)
 * - power_now: Current power draw (μW)
 * - charge_now, charge_full: Current/max charge (μAh)
 * - temp: Battery temperature (0.1°C)
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_PORTIA_POWER_H
#define NIMCP_PORTIA_POWER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/platform/nimcp_platform_tier.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Export Macro
//=============================================================================

#include "common/nimcp_export.h"

//=============================================================================
// Power Source and Profile Enumerations
//=============================================================================

/**
 * @brief Power source type
 */
typedef enum {
    POWER_SOURCE_AC,            /**< Wall power (unlimited) */
    POWER_SOURCE_BATTERY,       /**< Battery power (limited) */
    POWER_SOURCE_SOLAR,         /**< Solar power (variable) */
    POWER_SOURCE_USB,           /**< USB power (limited) */
    POWER_SOURCE_UNKNOWN        /**< Cannot determine */
} power_source_t;

/**
 * @brief Power profile for resource management
 */
typedef enum {
    POWER_PROFILE_PERFORMANCE = 0,  /**< Full power, max features (>80% or AC) */
    POWER_PROFILE_BALANCED = 1,     /**< Normal operation (40-80%) */
    POWER_PROFILE_SAVER = 2,        /**< Reduced features (20-40%) */
    POWER_PROFILE_CRITICAL = 3,     /**< Minimum viable (10-20%) */
    POWER_PROFILE_EMERGENCY = 4,    /**< Survival mode only (<10%) */
    POWER_PROFILE_COUNT = 5
} power_profile_t;

//=============================================================================
// Power Status Structure
//=============================================================================

/**
 * @brief Current power/battery status
 */
typedef struct {
    power_source_t source;          /**< Power source type */
    float battery_level_pct;        /**< Battery level 0-100 (100 = full) */
    float discharge_rate_mw;        /**< Current draw in milliwatts */
    float estimated_runtime_s;      /**< Estimated remaining seconds */
    float temperature_c;            /**< Battery temperature (Celsius) */
    bool charging;                  /**< Currently charging */
    bool health_good;               /**< Battery health OK */
    bool plugged_in;                /**< AC adapter connected */
    uint64_t timestamp_us;          /**< When this status was sampled */
} power_status_t;

//=============================================================================
// Power Tier Configuration
//=============================================================================

/**
 * @brief Power-aware tier configuration
 *
 * WHAT: Platform tier config adjusted for current power profile
 * WHY:  Automatically reduce resource usage on low battery
 * HOW:  Scale neuron counts, disable modules, reduce update rates
 */
typedef struct {
    power_profile_t profile;        /**< Power profile */

    // Neural network constraints
    uint32_t max_neurons;           /**< Maximum neurons allowed */
    uint32_t max_synapses;          /**< Maximum synapses allowed */

    // Cognitive modules (bitmask)
    uint32_t cognitive_modules;     /**< Enabled cognitive modules */

    // Processing parameters
    float processing_rate_hz;       /**< Update frequency (Hz) */
    float sampling_rate;            /**< State sampling rate (0-1) */
    uint32_t batch_size;            /**< Neurons per batch */

    // Feature flags
    bool enable_learning;           /**< Enable plasticity/learning */
    bool enable_persistence;        /**< Enable checkpointing */
    bool enable_bio_async;          /**< Enable bio-async messaging */
    bool enable_gpu;                /**< Enable GPU acceleration */

    // Sleep/wake cycle
    float wake_interval_s;          /**< Wake interval for sleep modes */
    float active_duty_cycle;        /**< Active time fraction (0-1) */

    // Resource budgets
    uint32_t memory_budget_mb;      /**< Memory budget (MB) */
    uint32_t compute_budget_gops;   /**< Compute budget (GOPS) */
} power_tier_config_t;

//=============================================================================
// Power Event Types (for bio-async notifications)
//=============================================================================

/**
 * @brief Power event types for bio-async messages
 */
typedef enum {
    POWER_EVENT_PROFILE_CHANGE = 0x7000,    /**< Power profile changed */
    POWER_EVENT_BATTERY_LOW,                /**< Battery below 20% */
    POWER_EVENT_BATTERY_CRITICAL,           /**< Battery below 10% */
    POWER_EVENT_CHARGING_STARTED,           /**< Started charging */
    POWER_EVENT_CHARGING_COMPLETE,          /**< Battery full */
    POWER_EVENT_THERMAL_WARNING,            /**< Battery too hot */
    POWER_EVENT_HEALTH_DEGRADED             /**< Battery health issue */
} power_event_type_t;

/**
 * @brief Power event message for bio-async
 */
typedef struct {
    bio_message_header_t header;
    power_event_type_t event_type;
    power_profile_t old_profile;
    power_profile_t new_profile;
    float battery_level_pct;
    float temperature_c;
    uint64_t timestamp_us;
} bio_msg_power_event_t;

//=============================================================================
// Power Monitoring Configuration
//=============================================================================

/**
 * @brief Power monitoring configuration
 */
typedef struct {
    // Monitoring behavior
    uint32_t poll_interval_ms;      /**< Status polling interval */
    bool auto_adjust_profile;       /**< Auto-adjust on battery change */
    bool enable_bio_async_events;   /**< Send bio-async notifications */

    // Battery thresholds (percentage)
    float performance_threshold;    /**< Enter performance mode (default: 80%) */
    float balanced_threshold;       /**< Enter balanced mode (default: 40%) */
    float saver_threshold;          /**< Enter saver mode (default: 20%) */
    float critical_threshold;       /**< Enter critical mode (default: 10%) */

    // Thermal limits
    float max_safe_temp_c;          /**< Max safe temperature (default: 45°C) */
    float thermal_throttle_temp_c;  /**< Start throttling (default: 40°C) */

    // Runtime estimation
    float discharge_history_s;      /**< History window for rate calc (default: 60s) */

    // Platform detection
    const char* battery_path;       /**< Override battery sysfs path */
    bool force_battery_mode;        /**< Pretend always on battery */
} portia_power_config_t;

//=============================================================================
// Power Monitoring Statistics
//=============================================================================

/**
 * @brief Power monitoring statistics
 */
typedef struct {
    uint64_t samples_taken;         /**< Total status samples */
    uint64_t profile_changes;       /**< Profile change count */
    uint64_t events_sent;           /**< Bio-async events sent */
    float avg_battery_level;        /**< Average battery level */
    float avg_discharge_rate_mw;    /**< Average discharge rate */
    float total_runtime_hours;      /**< Total runtime on battery */
    float max_temperature_c;        /**< Peak temperature observed */
    uint32_t thermal_throttles;     /**< Thermal throttle events */
    uint64_t uptime_s;              /**< Power monitor uptime */
} portia_power_stats_t;

//=============================================================================
// Power Manager Handle
//=============================================================================

/**
 * @brief Opaque power manager handle
 */
typedef struct portia_power_manager_struct* portia_power_manager_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default power monitoring configuration
 *
 * WHAT: Return sensible defaults for power monitoring
 * WHY:  Simplify initialization for common use cases
 * HOW:  Pre-populate config with recommended values
 *
 * @return Default configuration
 */
NIMCP_EXPORT portia_power_config_t portia_power_default_config(void);

/**
 * @brief Initialize power monitoring system
 *
 * WHAT: Start power monitoring with given configuration
 * WHY:  Enable battery-aware resource management
 * HOW:  Detect battery, start polling thread, register bio-async
 *
 * THREAD SAFETY: Thread-safe (call once at startup)
 *
 * @param config Configuration (NULL for defaults)
 * @return Power manager handle or NULL on failure
 *
 * EXAMPLE:
 * @code
 * portia_power_config_t config = portia_power_default_config();
 * config.poll_interval_ms = 5000;  // Poll every 5 seconds
 * portia_power_manager_t pm = portia_power_init(&config);
 * if (!pm) {
 *     LOG_ERROR("Failed to initialize power monitoring");
 * }
 * @endcode
 */
NIMCP_EXPORT portia_power_manager_t portia_power_init(
    const portia_power_config_t* config);

/**
 * @brief Shutdown power monitoring system
 *
 * WHAT: Stop monitoring and free resources
 * WHY:  Clean shutdown before exit
 * HOW:  Stop threads, unregister bio-async, free memory
 *
 * @param manager Power manager handle
 */
NIMCP_EXPORT void portia_power_shutdown(portia_power_manager_t manager);

//=============================================================================
// Status Query API
//=============================================================================

/**
 * @brief Query current power status
 *
 * WHAT: Get current battery/power state
 * WHY:  Check power availability before expensive operations
 * HOW:  Read cached status (updated by polling thread)
 *
 * THREAD SAFETY: Thread-safe (read cached data with lock)
 *
 * @param manager Power manager handle
 * @param status Output power status
 * @return true on success, false if unavailable
 *
 * EXAMPLE:
 * @code
 * power_status_t status;
 * if (portia_power_get_status(pm, &status)) {
 *     LOG_INFO("Battery: %.1f%%, %.1fW, %ds remaining",
 *              status.battery_level_pct,
 *              status.discharge_rate_mw / 1000.0f,
 *              (int)status.estimated_runtime_s);
 * }
 * @endcode
 */
NIMCP_EXPORT bool portia_power_get_status(
    portia_power_manager_t manager,
    power_status_t* status);

/**
 * @brief Get current recommended power profile
 *
 * WHAT: Get profile based on current battery/power state
 * WHY:  Automatic profile selection based on battery level
 * HOW:  Apply threshold rules to current battery percentage
 *
 * PROFILE SELECTION:
 * - AC power → PERFORMANCE
 * - Battery >80% → PERFORMANCE
 * - Battery 40-80% → BALANCED
 * - Battery 20-40% → SAVER
 * - Battery 10-20% → CRITICAL
 * - Battery <10% → EMERGENCY
 *
 * @param manager Power manager handle
 * @return Recommended power profile
 */
NIMCP_EXPORT power_profile_t portia_power_get_profile(
    portia_power_manager_t manager);

/**
 * @brief Force specific power profile
 *
 * WHAT: Override automatic profile selection
 * WHY:  Manual control for testing or user preference
 * HOW:  Set profile, send bio-async event, return old profile
 *
 * @param manager Power manager handle
 * @param profile Profile to set
 * @return Previous profile
 */
NIMCP_EXPORT power_profile_t portia_power_set_profile(
    portia_power_manager_t manager,
    power_profile_t profile);

//=============================================================================
// Runtime Estimation API
//=============================================================================

/**
 * @brief Estimate remaining runtime
 *
 * WHAT: Calculate estimated seconds until battery depleted
 * WHY:  Plan operations based on available energy
 * HOW:  Use recent discharge rate and current capacity
 *
 * ALGORITHM:
 * 1. Track recent discharge rate over history window
 * 2. Calculate current capacity from battery percentage
 * 3. Runtime = capacity / avg_discharge_rate
 * 4. Apply safety margin (0.9x)
 *
 * @param manager Power manager handle
 * @param safety_margin Safety factor (0-1, default 0.9)
 * @return Estimated seconds remaining (0 if on AC)
 */
NIMCP_EXPORT float portia_power_estimate_runtime(
    portia_power_manager_t manager,
    float safety_margin);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get tier configuration for power profile
 *
 * WHAT: Get platform tier config adjusted for power profile
 * WHY:  Automatically scale resources based on battery state
 * HOW:  Start with base tier config, apply profile-specific scaling
 *
 * SCALING RULES:
 * - PERFORMANCE: 100% of base tier config
 * - BALANCED: 75% neurons, 80% update rate
 * - SAVER: 50% neurons, 50% update rate, disable non-essential modules
 * - CRITICAL: 25% neurons, 20% update rate, essential modules only
 * - EMERGENCY: 10% neurons, 5% update rate, reactive processing only
 *
 * @param manager Power manager handle
 * @param base_tier Base platform tier (from platform_tier_detect)
 * @param profile Power profile (use current if < 0)
 * @return Power-adjusted tier configuration
 *
 * EXAMPLE:
 * @code
 * platform_tier_t tier = platform_tier_detect();
 * power_tier_config_t config = portia_power_get_tier_config(pm, tier, -1);
 * LOG_INFO("Max neurons: %u (profile: %s)",
 *          config.max_neurons,
 *          portia_power_get_profile_name(config.profile));
 * @endcode
 */
NIMCP_EXPORT power_tier_config_t portia_power_get_tier_config(
    portia_power_manager_t manager,
    platform_tier_t base_tier,
    power_profile_t profile);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get power monitoring statistics
 *
 * @param manager Power manager handle
 * @param stats Output statistics
 * @return true on success
 */
NIMCP_EXPORT bool portia_power_get_stats(
    portia_power_manager_t manager,
    portia_power_stats_t* stats);

/**
 * @brief Reset power statistics
 *
 * @param manager Power manager handle
 */
NIMCP_EXPORT void portia_power_reset_stats(portia_power_manager_t manager);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get power source name
 *
 * @param source Power source
 * @return Human-readable name
 */
NIMCP_EXPORT const char* portia_power_get_source_name(power_source_t source);

/**
 * @brief Get power profile name
 *
 * @param profile Power profile
 * @return Human-readable name
 */
NIMCP_EXPORT const char* portia_power_get_profile_name(power_profile_t profile);

/**
 * @brief Check if power source is limited
 *
 * WHAT: Test if power source has finite capacity
 * WHY:  Determine if power-aware adaptation is needed
 * HOW:  Return true for battery/solar, false for AC/USB
 *
 * @param source Power source
 * @return true if power is limited (battery/solar)
 */
NIMCP_EXPORT bool portia_power_is_limited(power_source_t source);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PORTIA_POWER_H */
