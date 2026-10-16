//=============================================================================
// nimcp_portia_tier_switch.h - Dynamic Tier Switching for Portia System
//=============================================================================
/**
 * @file nimcp_portia_tier_switch.h
 * @brief Runtime platform tier adaptation based on resource conditions
 *
 * WHAT: Dynamic tier switching system for adaptive resource management
 * WHY:  Enable NIMCP to adapt to changing hardware conditions in real-time
 * HOW:  Monitor system metrics and perform graceful tier transitions
 *
 * PORTIA ADAPTIVE HUNTING ANALOGY:
 * Just as Portia fimbriata adjusts its hunting strategy based on prey type,
 * energy reserves, and environmental constraints, NIMCP's tier system
 * dynamically adapts computational complexity to available resources.
 *
 * TIER SWITCHING TRIGGERS:
 * - Memory pressure (approaching RAM limits)
 * - Thermal throttling (CPU temperature)
 * - Battery level (mobile/edge devices)
 * - Load spikes (CPU saturation)
 * - User request (manual override)
 * - Performance goals (latency targets)
 *
 * DESIGN PHILOSOPHY:
 * 1. Graceful degradation: Downgrade tiers before OOM kills
 * 2. Opportunistic upgrade: Upgrade when resources available
 * 3. Hysteresis: Prevent rapid oscillation between tiers
 * 4. Cognitive awareness: Notify cognitive modules of transitions
 * 5. Bio-async coordination: Use neuromodulator channels for events
 *
 * INTEGRATION:
 * - platform_tier.h: Base tier configuration
 * - system_resources.h: Real-time resource monitoring
 * - bio_async.h: Event broadcasting
 * - blood_brain_barrier.h: Security validation
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_PORTIA_TIER_SWITCH_H
#define NIMCP_PORTIA_TIER_SWITCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/platform/nimcp_platform_tier.h"
#include "utils/platform/nimcp_system_resources.h"
#include "async/nimcp_bio_async.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Export Macro
//=============================================================================

#include "common/nimcp_export.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct portia_tier_switch_struct* portia_tier_switch_t;

//=============================================================================
// Tier Switch Triggers
//=============================================================================

/**
 * @brief Reasons for tier switching
 */
typedef enum {
    TIER_SWITCH_TRIGGER_MEMORY_PRESSURE,    /**< RAM usage too high */
    TIER_SWITCH_TRIGGER_THERMAL_THROTTLE,   /**< CPU overheating */
    TIER_SWITCH_TRIGGER_BATTERY_LOW,        /**< Low battery level */
    TIER_SWITCH_TRIGGER_LOAD_SPIKE,         /**< CPU load saturated */
    TIER_SWITCH_TRIGGER_USER_REQUEST,       /**< Manual override */
    TIER_SWITCH_TRIGGER_PERFORMANCE_GOAL,   /**< Latency target missed */
    TIER_SWITCH_TRIGGER_RESOURCE_AVAILABLE, /**< Resources freed up */
    TIER_SWITCH_TRIGGER_INIT,               /**< Initial tier selection */
    TIER_SWITCH_TRIGGER_COUNT
} tier_switch_trigger_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Configuration for tier switching system
 *
 * WHAT: Thresholds and parameters controlling tier transitions
 * WHY:  Allow tuning for different deployment scenarios
 * HOW:  Set percentage thresholds and timing constraints
 *
 * TUNING GUIDELINES:
 * - Servers: Higher thresholds (90%+ memory), faster switching
 * - Mobile: Lower thresholds (70% memory), slower switching
 * - IoT: Aggressive downgrade, conservative upgrade
 */
typedef struct {
    // Memory thresholds (percentage of total RAM)
    float memory_high_threshold;    /**< Trigger downgrade at this % (def: 85.0) */
    float memory_low_threshold;     /**< Allow upgrade below this % (def: 60.0) */
    float memory_critical_threshold; /**< Emergency downgrade % (def: 95.0) */

    // Thermal thresholds (Celsius)
    float thermal_threshold_c;      /**< Trigger downgrade at this temp (def: 80.0) */
    float thermal_safe_c;           /**< Allow upgrade below this temp (def: 65.0) */

    // Battery thresholds (percentage)
    float battery_threshold_pct;    /**< Trigger downgrade at this % (def: 20.0) */
    float battery_safe_pct;         /**< Allow upgrade above this % (def: 50.0) */

    // Load thresholds (percentage of CPU capacity)
    float load_threshold;           /**< Trigger downgrade at this load (def: 90.0) */
    float load_safe;                /**< Allow upgrade below this load (def: 50.0) */

    // Timing parameters (milliseconds)
    uint32_t hysteresis_ms;         /**< Prevent rapid switching (def: 30000) */
    uint32_t evaluation_interval_ms; /**< Check interval (def: 5000) */
    uint32_t transition_timeout_ms; /**< Max transition time (def: 60000) */

    // Feature flags
    bool auto_switch_enabled;       /**< Enable automatic switching (def: true) */
    bool allow_upgrade;             /**< Allow automatic upgrades (def: true) */
    bool allow_downgrade;           /**< Allow automatic downgrades (def: true) */
    bool broadcast_events;          /**< Send bio-async events (def: true) */
    bool emergency_downgrade;       /**< Enable emergency downgrades (def: true) */

    // Module coordination
    uint32_t module_shutdown_timeout_ms; /**< Wait for modules (def: 10000) */
    bool wait_for_module_ack;       /**< Wait for module acknowledgment (def: true) */

    // Bio-async integration
    void* bio_ctx;                  /**< Bio-async context (optional) */
    nimcp_bio_channel_type_t event_channel; /**< Channel for events (def: SEROTONIN) */
} tier_switch_config_t;

/**
 * @brief State of tier switching system
 *
 * WHAT: Current operational state and history
 * WHY:  Track transitions and prevent oscillation
 * HOW:  Maintain current/target tiers and timing info
 *
 * THREAD SAFETY: Protected by internal mutex
 */
typedef struct {
    platform_tier_t current_tier;   /**< Current active tier */
    platform_tier_t target_tier;    /**< Target tier for transition */
    platform_tier_t previous_tier;  /**< Previous tier (for rollback) */

    tier_switch_trigger_t last_trigger; /**< What triggered last switch */
    uint64_t last_switch_time_ms;   /**< Timestamp of last switch */
    uint64_t last_evaluation_ms;    /**< Timestamp of last evaluation */

    uint32_t switch_count;          /**< Total switches performed */
    uint32_t upgrade_count;         /**< Count of upgrades */
    uint32_t downgrade_count;       /**< Count of downgrades */
    uint32_t failed_switch_count;   /**< Failed transition attempts */

    bool switch_in_progress;        /**< Transition currently active */
    bool auto_switch_active;        /**< Auto-switching enabled */
    bool emergency_mode;            /**< Emergency downgrade active */

    // Current system metrics (cached for efficiency)
    float current_memory_usage_pct; /**< Current memory usage % */
    float current_cpu_temp_c;       /**< Current CPU temperature */
    float current_battery_pct;      /**< Current battery level % */
    float current_cpu_load_pct;     /**< Current CPU load % */

    // Continuous resource pressure metric [0.0 - 1.0]
    float resource_pressure;        /**< Composite resource pressure (0=idle, 1=critical) */
} tier_switch_state_t;

//=============================================================================
// Continuous Allocation Structures
//=============================================================================

/**
 * @brief Continuous resource allocation output
 *
 * WHAT: Smooth resource allocation derived from continuous pressure metric
 * WHY:  Replace hard tier thresholds with smooth sigmoid-based transitions
 * HOW:  All fields vary continuously in [0.0, 1.0] based on resource_pressure
 *
 * DESIGN:
 * Instead of discrete tier switching (if usage > 80% -> SEVERE),
 * resource allocation varies continuously:
 *   quality_scale = sigmoid(resource_pressure, center=0.7, steepness=10)
 *   allocation_fraction = 1.0 - resource_pressure
 *   feature_gate = sigmoid(resource_pressure, center=threshold, steepness=12)
 *
 * The tier enum is still derived for logging/serialization via
 * portia_tier_from_pressure(), but all actual resource decisions
 * use these continuous values.
 */
typedef struct {
    float quality_scale;            /**< Quality scaling factor [0.0-1.0] (1.0 = full quality) */
    float allocation_fraction;      /**< Resource allocation fraction [0.0-1.0] (1.0 = max resources) */
    float feature_gate_plasticity;  /**< Plasticity feature gate [0.0-1.0] (1.0 = fully enabled) */
    float feature_gate_learning;    /**< Learning feature gate [0.0-1.0] (1.0 = fully enabled) */
    float feature_gate_emotions;    /**< Emotions feature gate [0.0-1.0] (1.0 = fully enabled) */
    float feature_gate_planning;    /**< Planning feature gate [0.0-1.0] (1.0 = fully enabled) */
    float feature_gate_meta;        /**< Meta-cognition feature gate [0.0-1.0] (1.0 = fully enabled) */
    float compute_budget_scale;     /**< Compute budget scaling [0.0-1.0] (1.0 = full budget) */
    float memory_budget_scale;      /**< Memory budget scaling [0.0-1.0] (1.0 = full budget) */
    float thread_budget_scale;      /**< Thread pool scaling [0.0-1.0] (1.0 = all threads) */
    platform_tier_t derived_tier;   /**< Tier label derived from pressure (backward compat) */
} portia_allocation_t;

/**
 * @brief Tier transition event notification
 *
 * WHAT: Information about tier switch event
 * WHY:  Inform modules and log transitions
 * HOW:  Broadcast via bio-async or callback
 */
typedef struct {
    platform_tier_t old_tier;       /**< Tier before switch */
    platform_tier_t new_tier;       /**< Tier after switch */
    tier_switch_trigger_t trigger;  /**< What triggered switch */
    uint64_t timestamp_ms;          /**< When switch occurred */
    bool was_emergency;             /**< Emergency downgrade */
    uint32_t transition_time_ms;    /**< Time taken for switch */
    char reason[128];               /**< Human-readable reason */
} tier_switch_event_t;

/**
 * @brief Callback for tier switch notifications
 */
typedef void (*tier_switch_callback_t)(
    const tier_switch_event_t* event,
    void* user_data
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize tier switching system
 *
 * WHAT: Create tier switch manager with configuration
 * WHY:  Enable dynamic tier adaptation throughout runtime
 * HOW:  Allocate state, register with bio-async, start monitoring
 *
 * INITIALIZATION FLOW:
 * 1. Detect current platform tier
 * 2. Validate configuration thresholds
 * 3. Register with bio-async system (if enabled)
 * 4. Register with BBB for security validation
 * 5. Start background monitoring thread (if auto-switch enabled)
 *
 * @param config Configuration (NULL for defaults)
 * @return Tier switch handle or NULL on failure
 *
 * THREAD SAFETY: Thread-safe
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT portia_tier_switch_t portia_tier_switch_init(
    const tier_switch_config_t* config
);

/**
 * @brief Shutdown tier switching system
 *
 * WHAT: Clean shutdown of tier switch manager
 * WHY:  Free resources and stop monitoring
 * HOW:  Stop threads, notify modules, free memory
 *
 * @param switcher Tier switch handle
 *
 * THREAD SAFETY: Thread-safe
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT void portia_tier_switch_shutdown(
    portia_tier_switch_t switcher
);

/**
 * @brief Get default configuration
 *
 * @return Default config with sensible values
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT tier_switch_config_t portia_tier_switch_default_config(void);

//=============================================================================
// Evaluation and Decision API
//=============================================================================

/**
 * @brief Evaluate if tier change is needed
 *
 * WHAT: Check current conditions and determine if switch required
 * WHY:  Main decision point for automatic tier adaptation
 * HOW:  Query system resources, apply thresholds, check hysteresis
 *
 * EVALUATION LOGIC:
 * 1. Query system_resources (RAM, CPU, battery, temperature)
 * 2. Compare against configured thresholds
 * 3. Check hysteresis timer (prevent rapid switching)
 * 4. Determine target tier (upgrade/downgrade/stay)
 * 5. Return recommendation with trigger reason
 *
 * HYSTERESIS:
 * - Must wait hysteresis_ms since last switch
 * - Emergency conditions bypass hysteresis
 * - Prevents oscillation between tiers
 *
 * @param switcher Tier switch handle
 * @param target_tier Output: recommended target tier (may be same as current)
 * @param trigger Output: reason for recommendation
 * @return true if switch recommended, false if stay at current tier
 *
 * THREAD SAFETY: Thread-safe
 * COMPLEXITY: O(1) with system calls
 */
NIMCP_EXPORT bool portia_tier_switch_evaluate(
    portia_tier_switch_t switcher,
    platform_tier_t* target_tier,
    tier_switch_trigger_t* trigger
);

/**
 * @brief Execute tier transition
 *
 * WHAT: Perform actual tier switch with module coordination
 * WHY:  Apply new tier configuration across system
 * HOW:  Notify modules, reconfigure subsystems, update state
 *
 * TRANSITION FLOW:
 * 1. Validate target tier and current state
 * 2. Broadcast pre-switch event (via bio-async SEROTONIN channel)
 * 3. Wait for module acknowledgments (with timeout)
 * 4. Shutdown/suspend modules not available in target tier
 * 5. Apply new platform_tier_config_t settings
 * 6. Initialize/resume modules available in target tier
 * 7. Update internal state (current_tier, statistics)
 * 8. Broadcast post-switch event
 * 9. Log transition with BBB audit
 *
 * MODULE COORDINATION:
 * - Modules receive TIER_SWITCH_BEGIN message
 * - Modules suspend non-critical operations
 * - Modules acknowledge readiness
 * - After switch, modules receive TIER_SWITCH_COMPLETE
 * - Modules adapt to new tier configuration
 *
 * ROLLBACK:
 * - If transition fails, attempt rollback to previous tier
 * - If rollback fails, enter safe mode (MINIMAL tier)
 *
 * @param switcher Tier switch handle
 * @param target_tier Target tier to switch to
 * @param trigger Reason for switch (for logging)
 * @return 0 on success, negative error code on failure
 *
 * THREAD SAFETY: Thread-safe (uses internal mutex)
 * COMPLEXITY: O(n) where n = number of cognitive modules
 */
NIMCP_EXPORT int portia_tier_switch_execute(
    portia_tier_switch_t switcher,
    platform_tier_t target_tier,
    tier_switch_trigger_t trigger
);

/**
 * @brief Check if tier upgrade is safe
 *
 * WHAT: Validate that upgrade won't cause resource issues
 * WHY:  Prevent upgrades that would immediately trigger downgrade
 * HOW:  Check resources against target tier requirements
 *
 * SAFETY CHECKS:
 * - Target tier memory budget < available RAM - safety margin
 * - CPU temperature below thermal_safe_c
 * - Battery above battery_safe_pct (if applicable)
 * - CPU load below load_safe
 * - Hysteresis period elapsed
 * - No emergency mode active
 *
 * @param switcher Tier switch handle
 * @param target_tier Tier to upgrade to
 * @return true if upgrade is safe, false otherwise
 *
 * THREAD SAFETY: Thread-safe
 * COMPLEXITY: O(1) with system calls
 */
NIMCP_EXPORT bool portia_tier_switch_can_upgrade(
    portia_tier_switch_t switcher,
    platform_tier_t target_tier
);

/**
 * @brief Check if tier downgrade is needed
 *
 * WHAT: Determine if conditions require downgrade
 * WHY:  Proactive response to resource pressure
 * HOW:  Check if any threshold exceeded
 *
 * DOWNGRADE TRIGGERS:
 * - Memory usage > memory_high_threshold
 * - CPU temperature > thermal_threshold_c
 * - Battery < battery_threshold_pct
 * - CPU load > load_threshold
 * - Emergency condition detected
 *
 * @param switcher Tier switch handle
 * @param target_tier Output: recommended downgrade tier
 * @param trigger Output: which threshold triggered downgrade
 * @return true if downgrade needed, false otherwise
 *
 * THREAD SAFETY: Thread-safe
 * COMPLEXITY: O(1) with system calls
 */
NIMCP_EXPORT bool portia_tier_switch_can_downgrade(
    portia_tier_switch_t switcher,
    platform_tier_t* target_tier,
    tier_switch_trigger_t* trigger
);

//=============================================================================
// State Query API
//=============================================================================

/**
 * @brief Get current tier switch state
 *
 * WHAT: Query current operational state
 * WHY:  Allow monitoring and debugging
 * HOW:  Copy internal state to output structure
 *
 * @param switcher Tier switch handle
 * @param state Output: state structure
 * @return 0 on success, negative on error
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT int portia_tier_switch_get_state(
    portia_tier_switch_t switcher,
    tier_switch_state_t* state
);

/**
 * @brief Get current platform tier
 *
 * @param switcher Tier switch handle
 * @return Current tier
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT platform_tier_t portia_tier_switch_get_current_tier(
    portia_tier_switch_t switcher
);

/**
 * @brief Check if tier switch is in progress
 *
 * @param switcher Tier switch handle
 * @return true if transitioning, false if stable
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool portia_tier_switch_is_transitioning(
    portia_tier_switch_t switcher
);

/**
 * @brief Get switch statistics
 *
 * @param switcher Tier switch handle
 * @param total_switches Output: total switches performed
 * @param upgrades Output: upgrade count
 * @param downgrades Output: downgrade count
 * @param failed Output: failed transitions
 * @return 0 on success, negative on error
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT int portia_tier_switch_get_statistics(
    portia_tier_switch_t switcher,
    uint32_t* total_switches,
    uint32_t* upgrades,
    uint32_t* downgrades,
    uint32_t* failed
);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Enable/disable automatic switching
 *
 * @param switcher Tier switch handle
 * @param enabled Enable auto-switching
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT void portia_tier_switch_set_auto_switch(
    portia_tier_switch_t switcher,
    bool enabled
);

/**
 * @brief Update thresholds at runtime
 *
 * @param switcher Tier switch handle
 * @param config New configuration
 * @return 0 on success, negative on error
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT int portia_tier_switch_update_config(
    portia_tier_switch_t switcher,
    const tier_switch_config_t* config
);

/**
 * @brief Register callback for tier switch events
 *
 * @param switcher Tier switch handle
 * @param callback Callback function
 * @param user_data User context
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT void portia_tier_switch_set_callback(
    portia_tier_switch_t switcher,
    tier_switch_callback_t callback,
    void* user_data
);

//=============================================================================
// Manual Control API
//=============================================================================

/**
 * @brief Manually request tier switch
 *
 * WHAT: User-initiated tier change
 * WHY:  Allow manual override of automatic system
 * HOW:  Execute transition with USER_REQUEST trigger
 *
 * @param switcher Tier switch handle
 * @param target_tier Desired tier
 * @return 0 on success, negative on error
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT int portia_tier_switch_request(
    portia_tier_switch_t switcher,
    platform_tier_t target_tier
);

/**
 * @brief Force emergency downgrade to minimal tier
 *
 * WHAT: Immediate downgrade bypassing normal flow
 * WHY:  Respond to critical resource exhaustion
 * HOW:  Skip module coordination, apply minimal config
 *
 * @param switcher Tier switch handle
 * @return 0 on success, negative on error
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT int portia_tier_switch_emergency_downgrade(
    portia_tier_switch_t switcher
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get trigger name string
 *
 * @param trigger Trigger enum
 * @return Human-readable name
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT const char* portia_tier_switch_trigger_name(
    tier_switch_trigger_t trigger
);

/**
 * @brief Print current state to stdout
 *
 * @param switcher Tier switch handle
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT void portia_tier_switch_print_state(
    portia_tier_switch_t switcher
);

//=============================================================================
// Continuous Resource Pressure API
//=============================================================================

/**
 * @brief Compute composite resource pressure from system metrics
 *
 * WHAT: Combine memory, thermal, battery, and load into single [0.0-1.0] pressure
 * WHY:  Continuous metric enables smooth allocation without hard thresholds
 * HOW:  Weighted max of individual normalized pressures
 *
 * @param switcher Tier switch handle
 * @param pressure Output: composite resource pressure [0.0-1.0]
 * @return 0 on success, negative on error
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT int portia_compute_resource_pressure(
    portia_tier_switch_t switcher,
    float* pressure
);

/**
 * @brief Compute continuous resource allocation from pressure
 *
 * WHAT: Map resource_pressure to smooth allocation values
 * WHY:  Eliminate discrete jumps at tier boundaries
 * HOW:  Sigmoid curves gate features; linear scaling for budgets
 *
 * @param resource_pressure Input pressure [0.0-1.0]
 * @param out Output: continuous allocation values
 * @return 0 on success, negative on error
 *
 * THREAD SAFETY: Thread-safe (pure function, no shared state)
 */
NIMCP_EXPORT int portia_compute_allocation(
    float resource_pressure,
    portia_allocation_t* out
);

/**
 * @brief Derive discrete tier label from continuous pressure
 *
 * WHAT: Map continuous pressure to tier enum for logging/serialization
 * WHY:  Backward compatibility with code expecting tier enums
 * HOW:  Simple threshold mapping
 *
 * MAPPING:
 *   pressure < 0.25  -> PLATFORM_TIER_FULL
 *   pressure < 0.50  -> PLATFORM_TIER_MEDIUM
 *   pressure < 0.75  -> PLATFORM_TIER_CONSTRAINED
 *   pressure >= 0.75 -> PLATFORM_TIER_MINIMAL
 *
 * @param resource_pressure Input pressure [0.0-1.0]
 * @return Derived platform tier
 *
 * THREAD SAFETY: Thread-safe (pure function)
 */
NIMCP_EXPORT platform_tier_t portia_tier_from_pressure(
    float resource_pressure
);

/**
 * @brief Get current continuous allocation from switcher
 *
 * WHAT: Convenience function to query pressure and compute allocation
 * WHY:  Single call for consumers that need current allocation state
 * HOW:  Calls portia_compute_resource_pressure then portia_compute_allocation
 *
 * @param switcher Tier switch handle
 * @param out Output: current continuous allocation
 * @return 0 on success, negative on error
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT int portia_tier_switch_get_allocation(
    portia_tier_switch_t switcher,
    portia_allocation_t* out
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PORTIA_TIER_SWITCH_H
