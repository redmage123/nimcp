#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_portia_tier_switch.c - Dynamic Tier Switching Implementation
//=============================================================================

#include "portia/nimcp_portia_tier_switch.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>

#define LOG_MODULE "portia_tier_switch"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_timing_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_tier_switch)

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

//=============================================================================
// Constants
//=============================================================================

#define PORTIA_TIER_SWITCH_MAGIC 0x54495357  // 'TISW'
#define PORTIA_MODULE_NAME "portia_tier_switch"
#define PORTIA_MAX_CALLBACKS 8
#define PORTIA_MEMORY_SAFETY_MARGIN 0.15f  // Keep 15% RAM free
#define PORTIA_DEFAULT_EVALUATION_INTERVAL_MS 5000

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Callback registration entry
 */
typedef struct {
    tier_switch_callback_t callback;
    void* user_data;
    bool active;
} callback_entry_t;

/**
 * @brief Internal tier switch manager structure
 */
struct portia_tier_switch_struct {
    uint32_t magic;                     // Validation magic

    // Configuration
    tier_switch_config_t config;

    // State
    tier_switch_state_t state;

    // Thread synchronization
    nimcp_platform_mutex_t state_mutex;
    nimcp_thread_t monitor_thread;
    bool monitor_running;
    bool monitor_thread_created;

    // Callbacks
    callback_entry_t callbacks[PORTIA_MAX_CALLBACKS];
    nimcp_platform_mutex_t callback_mutex;
    bool callback_mutex_init;

    // Bio-async integration
    void* bio_ctx;
    bool bio_async_registered;
    uint32_t bio_module_id;

    // BBB integration
    bbb_system_t bbb_system;
    bool security_enabled;

    // Cached system resources
    system_resources_t last_resources;
    uint64_t last_resource_query_ms;

    // Statistics
    uint64_t creation_time_ms;
    uint64_t total_evaluation_time_ms;
    uint64_t total_transition_time_ms;
};

//=============================================================================
// Helper Functions - Time
//=============================================================================

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Helper Functions - Validation
//=============================================================================

static bool validate_switcher(portia_tier_switch_t switcher) {
    if (!bbb_validate_pointer(NULL, switcher, sizeof(struct portia_tier_switch_struct), NULL)) {
        LOG_ERROR("[%s] Invalid switcher pointer", PORTIA_MODULE_NAME);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_switcher: bbb_validate_pointer is NULL");
        return false;
    }

    if (switcher->magic != PORTIA_TIER_SWITCH_MAGIC) {
        LOG_ERROR("[%s] Invalid switcher magic: 0x%08X", PORTIA_MODULE_NAME, switcher->magic);
        bbb_audit_log(BBB_AUDIT_WARNING, LOG_MODULE, "invalid_magic", "invalid switcher magic");
        return false;
    }

    return true;
}

static bool validate_tier(platform_tier_t tier) {
    if (tier >= PLATFORM_TIER_COUNT) {
        LOG_ERROR("[%s] Invalid tier: %d", PORTIA_MODULE_NAME, tier);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "validate_tier: capacity exceeded");
        return false;
    }
    return true;
}

static bool validate_config(const tier_switch_config_t* config) {
    if (!config) {
        LOG_ERROR("[%s] Invalid config pointer", PORTIA_MODULE_NAME);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_config: config is NULL");
        return false;
    }

    // Validate threshold ranges
    if (config->memory_high_threshold < 0.0F || config->memory_high_threshold > 100.0F) {
        LOG_ERROR("[%s] Invalid memory_high_threshold: %.2f", PORTIA_MODULE_NAME, config->memory_high_threshold);
        return false;
    }

    if (config->memory_low_threshold < 0.0F || config->memory_low_threshold > 100.0F) {
        LOG_ERROR("[%s] Invalid memory_low_threshold: %.2f", PORTIA_MODULE_NAME, config->memory_low_threshold);
        return false;
    }

    if (config->memory_low_threshold >= config->memory_high_threshold) {
        LOG_ERROR("[%s] memory_low_threshold must be < memory_high_threshold", PORTIA_MODULE_NAME);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "validate_config: capacity exceeded");
        return false;
    }

    LOG_DEBUG("[%s] Configuration validated successfully", PORTIA_MODULE_NAME);
    return true;
}

//=============================================================================
// Helper Functions - System Metrics
//=============================================================================

static bool query_system_metrics(portia_tier_switch_t switcher) {
    if (!validate_switcher(switcher)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "query_system_metrics: validate_switcher is NULL");
        return false;
    }

    // Check cache freshness (cache for 1 second)
    uint64_t now = get_time_ms();
    if (now - switcher->last_resource_query_ms < 1000) {
        LOG_DEBUG("[%s] Using cached resource metrics", PORTIA_MODULE_NAME);
        return true;
    }

    // Query system resources
    system_resources_t resources;
    if (!system_resources_query(&resources)) {
        LOG_ERROR("[%s] Failed to query system resources", PORTIA_MODULE_NAME);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "query_system_metrics: system_resources_query is NULL");
        return false;
    }

    // Update cached resources
    memcpy(&switcher->last_resources, &resources, sizeof(system_resources_t));
    switcher->last_resource_query_ms = now;

    // Calculate metrics
    nimcp_platform_mutex_lock(&switcher->state_mutex);

    if (resources.total_ram_mb > 0) {
        uint64_t used_ram = resources.total_ram_mb - resources.available_ram_mb;
        switcher->state.current_memory_usage_pct =
            (float)used_ram / (float)resources.total_ram_mb * 100.0F;
    }

    // Note: CPU temperature and battery require platform-specific APIs
    // For now, we'll use placeholder values (0.0 = not available)
    switcher->state.current_cpu_temp_c = 0.0F;  // TODO: Implement thermal monitoring
    switcher->state.current_battery_pct = 100.0F;  // TODO: Implement battery monitoring

    // CPU load (simple approximation based on thread count vs cores)
    if (resources.num_cpu_cores > 0) {
        // This is a placeholder - real implementation would query /proc/stat or similar
        switcher->state.current_cpu_load_pct = 0.0F;  // TODO: Implement CPU load monitoring
    }

    // Compute continuous resource pressure [0.0, 1.0]
    // Use max of individual normalized pressures
    {
        float mem_p = switcher->state.current_memory_usage_pct / 100.0F;
        float thermal_p = 0.0F;
        if (switcher->state.current_cpu_temp_c > 0.0F && switcher->config.thermal_threshold_c > 0.0F) {
            thermal_p = switcher->state.current_cpu_temp_c / switcher->config.thermal_threshold_c;
        }
        float battery_p = 0.0F;
        if (switcher->state.current_battery_pct < 100.0F) {
            battery_p = 1.0F - (switcher->state.current_battery_pct / 100.0F);
        }
        float load_p = switcher->state.current_cpu_load_pct / 100.0F;

        // Clamp individual pressures to [0, 1]
        if (mem_p < 0.0F) mem_p = 0.0F;
        if (mem_p > 1.0F) mem_p = 1.0F;
        if (thermal_p < 0.0F) thermal_p = 0.0F;
        if (thermal_p > 1.0F) thermal_p = 1.0F;
        if (battery_p < 0.0F) battery_p = 0.0F;
        if (battery_p > 1.0F) battery_p = 1.0F;
        if (load_p < 0.0F) load_p = 0.0F;
        if (load_p > 1.0F) load_p = 1.0F;

        // Composite pressure = max of all individual pressures
        float pressure = mem_p;
        if (thermal_p > pressure) pressure = thermal_p;
        if (battery_p > pressure) pressure = battery_p;
        if (load_p > pressure) pressure = load_p;

        switcher->state.resource_pressure = pressure;
    }

    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    LOG_DEBUG("[%s] System metrics: Memory=%.1f%%, Temp=%.1f%cC, Battery=%.1f%%, Load=%.1f%%, Pressure=%.3f",
              PORTIA_MODULE_NAME,
              switcher->state.current_memory_usage_pct,
              switcher->state.current_cpu_temp_c,
              0xB0,  /* degree symbol */
              switcher->state.current_battery_pct,
              switcher->state.current_cpu_load_pct,
              switcher->state.resource_pressure);

    return true;
}

//=============================================================================
// Helper Functions - Bio-Async Integration
//=============================================================================

static void broadcast_tier_switch_event(
    portia_tier_switch_t switcher,
    const tier_switch_event_t* event)
{
    if (!switcher->config.broadcast_events || !switcher->bio_async_registered) {
        return;
    }

    if (!switcher->config.bio_ctx) {
        LOG_WARN("[%s] Bio-async context not available for event broadcast", PORTIA_MODULE_NAME);
        return;
    }

    LOG_INFO("[%s] Broadcasting tier switch event: %s -> %s (trigger: %s)",
             PORTIA_MODULE_NAME,
             platform_tier_get_name(event->old_tier),
             platform_tier_get_name(event->new_tier),
             portia_tier_switch_trigger_name(event->trigger));

    // Create bio-async message (using system message type)
    // In a full implementation, this would use nimcp_bio_send_message() or similar
    // For now, we log the event
    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "tier_switch_event", "%s", event->reason);
}

//=============================================================================
// Helper Functions - Callbacks
//=============================================================================

static void invoke_callbacks(
    portia_tier_switch_t switcher,
    const tier_switch_event_t* event)
{
    if (!switcher->callback_mutex_init) {
        return;
    }

    /* Copy callback entries under lock to avoid calling external code
     * while holding the mutex (deadlock risk). */
    typedef struct {
        tier_switch_callback_t callback;
        void* user_data;
    } cb_snapshot_t;

    cb_snapshot_t snapshot[PORTIA_MAX_CALLBACKS];
    int snapshot_count = 0;

    nimcp_platform_mutex_lock(&switcher->callback_mutex);
    for (int i = 0; i < PORTIA_MAX_CALLBACKS; i++) {
        if (switcher->callbacks[i].active && switcher->callbacks[i].callback) {
            snapshot[snapshot_count].callback = switcher->callbacks[i].callback;
            snapshot[snapshot_count].user_data = switcher->callbacks[i].user_data;
            snapshot_count++;
        }
    }
    nimcp_platform_mutex_unlock(&switcher->callback_mutex);

    /* Invoke callbacks outside the lock */
    for (int i = 0; i < snapshot_count; i++) {
        LOG_DEBUG("[%s] Invoking callback %d", PORTIA_MODULE_NAME, i);
        snapshot[i].callback(event, snapshot[i].user_data);
    }
}

//=============================================================================
// Helper Functions - Tier Logic
//=============================================================================

// NOTE: calculate_downgrade_tier and calculate_upgrade_tier removed.
// Tier derivation is now done via portia_tier_from_pressure() using
// continuous resource pressure instead of discrete one-step transitions.

//=============================================================================
// Monitoring Thread
//=============================================================================

static void* monitoring_thread_func(void* arg) {
    portia_tier_switch_t switcher = (portia_tier_switch_t)arg;

    LOG_INFO("[%s] Monitoring thread started", PORTIA_MODULE_NAME);

    while (switcher->monitor_running) {
        // Sleep for evaluation interval
        usleep(switcher->config.evaluation_interval_ms * 1000);

        if (!switcher->monitor_running) {
            break;
        }

        // Evaluate tier switch need
        platform_tier_t target_tier;
        tier_switch_trigger_t trigger;

        if (portia_tier_switch_evaluate(switcher, &target_tier, &trigger)) {
            LOG_INFO("[%s] Auto-switch recommended: %s -> %s (trigger: %s)",
                     PORTIA_MODULE_NAME,
                     platform_tier_get_name(switcher->state.current_tier),
                     platform_tier_get_name(target_tier),
                     portia_tier_switch_trigger_name(trigger));

            // Execute switch
            int result = portia_tier_switch_execute(switcher, target_tier, trigger);
            if (result != 0) {
                LOG_ERROR("[%s] Auto-switch failed: %d", PORTIA_MODULE_NAME, result);
            }
        }
    }

    LOG_INFO("[%s] Monitoring thread stopped", PORTIA_MODULE_NAME);
    return NULL;
}

//=============================================================================
// Public API - Lifecycle
//=============================================================================

tier_switch_config_t portia_tier_switch_default_config(void) {
    tier_switch_config_t config;
    memset(&config, 0, sizeof(tier_switch_config_t));

    // Memory thresholds
    config.memory_high_threshold = 85.0F;
    config.memory_low_threshold = 60.0F;
    config.memory_critical_threshold = 95.0F;

    // Thermal thresholds
    config.thermal_threshold_c = 80.0F;
    config.thermal_safe_c = 65.0F;

    // Battery thresholds
    config.battery_threshold_pct = 20.0F;
    config.battery_safe_pct = 50.0F;

    // Load thresholds
    config.load_threshold = 90.0F;
    config.load_safe = 50.0F;

    // Timing
    config.hysteresis_ms = 30000;  // 30 seconds
    config.evaluation_interval_ms = 5000;  // 5 seconds
    config.transition_timeout_ms = 60000;  // 60 seconds

    // Features
    config.auto_switch_enabled = true;
    config.allow_upgrade = true;
    config.allow_downgrade = true;
    config.broadcast_events = true;
    config.emergency_downgrade = true;

    // Module coordination
    config.module_shutdown_timeout_ms = NIMCP_WATCHDOG_TIMEOUT_MS;  // 10 seconds
    config.wait_for_module_ack = true;

    // Bio-async
    config.bio_ctx = NULL;
    config.event_channel = BIO_CHANNEL_SEROTONIN;  // Slow, deliberative changes

    return config;
}

portia_tier_switch_t portia_tier_switch_init(const tier_switch_config_t* config) {
    LOG_INFO("[%s] Initializing tier switching system", PORTIA_MODULE_NAME);

    // Use default config if none provided
    tier_switch_config_t default_config = portia_tier_switch_default_config();
    const tier_switch_config_t* use_config = config ? config : &default_config;

    // Validate configuration
    if (!validate_config(use_config)) {
        LOG_ERROR("[%s] Configuration validation failed", PORTIA_MODULE_NAME);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_tier_switch_init: validate_config is NULL");
        return NULL;
    }

    // Allocate switcher
    portia_tier_switch_t switcher = (portia_tier_switch_t)nimcp_calloc(
        1, sizeof(struct portia_tier_switch_struct));
    if (!switcher) {
        LOG_ERROR("[%s] Failed to allocate switcher structure", PORTIA_MODULE_NAME);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tier_switch_create: allocation failed");

        return NULL;
    }

    // Initialize magic
    switcher->magic = PORTIA_TIER_SWITCH_MAGIC;

    // Copy configuration
    memcpy(&switcher->config, use_config, sizeof(tier_switch_config_t));

    // Initialize state
    switcher->state.current_tier = platform_tier_detect();
    switcher->state.target_tier = switcher->state.current_tier;
    switcher->state.previous_tier = switcher->state.current_tier;
    switcher->state.last_trigger = TIER_SWITCH_TRIGGER_INIT;
    switcher->state.last_switch_time_ms = get_time_ms();
    switcher->state.last_evaluation_ms = 0;
    switcher->state.switch_count = 0;
    switcher->state.upgrade_count = 0;
    switcher->state.downgrade_count = 0;
    switcher->state.failed_switch_count = 0;
    switcher->state.switch_in_progress = false;
    switcher->state.auto_switch_active = use_config->auto_switch_enabled;
    switcher->state.emergency_mode = false;

    // Create mutexes
    if (nimcp_platform_mutex_init(&switcher->state_mutex, false) != 0) {
        LOG_ERROR("[%s] Failed to create state mutex", PORTIA_MODULE_NAME);
        nimcp_free(switcher);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_tier_switch_init: validation failed");
        return NULL;
    }

    if (nimcp_platform_mutex_init(&switcher->callback_mutex, false) != 0) {
        LOG_ERROR("[%s] Failed to create callback mutex", PORTIA_MODULE_NAME);
        nimcp_platform_mutex_destroy(&switcher->state_mutex);
        nimcp_free(switcher);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_tier_switch_init: validation failed");
        return NULL;
    }
    switcher->callback_mutex_init = true;

    // Initialize callbacks
    for (int i = 0; i < PORTIA_MAX_CALLBACKS; i++) {
        switcher->callbacks[i].active = false;
    }

    // Initialize bio-async integration
    switcher->bio_ctx = use_config->bio_ctx;
    switcher->bio_async_registered = false;
    switcher->bio_module_id = BIO_MODULE_UNKNOWN;

    // Initialize BBB integration
    switcher->bbb_system = NULL;  // Will be set by security system if available
    switcher->security_enabled = false;

    // Initialize timestamps
    switcher->creation_time_ms = get_time_ms();
    switcher->last_resource_query_ms = 0;

    // Query initial system resources
    if (!query_system_metrics(switcher)) {
        LOG_WARN("[%s] Failed to query initial system metrics", PORTIA_MODULE_NAME);
    }

    // Start monitoring thread if auto-switch enabled
    if (use_config->auto_switch_enabled) {
        switcher->monitor_running = true;
        if (nimcp_thread_create(&switcher->monitor_thread, monitoring_thread_func, switcher, NULL) != 0) {
            LOG_ERROR("[%s] Failed to create monitoring thread", PORTIA_MODULE_NAME);
            nimcp_platform_mutex_destroy(&switcher->callback_mutex);
            nimcp_platform_mutex_destroy(&switcher->state_mutex);
            nimcp_free(switcher);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_tier_switch_init: validation failed");
            return NULL;
        }
        switcher->monitor_thread_created = true;
        LOG_INFO("[%s] Monitoring thread started", PORTIA_MODULE_NAME);
    }

    LOG_INFO("[%s] Tier switching initialized: tier=%s, auto_switch=%s",
             PORTIA_MODULE_NAME,
             platform_tier_get_name(switcher->state.current_tier),
             use_config->auto_switch_enabled ? "enabled" : "disabled");

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "tier_switch_init", "success");

    return switcher;
}

void portia_tier_switch_shutdown(portia_tier_switch_t switcher) {
    if (!validate_switcher(switcher)) {
        return;
    }

    LOG_INFO("[%s] Shutting down tier switching system", PORTIA_MODULE_NAME);

    // Stop monitoring thread
    if (switcher->monitor_thread_created) {
        switcher->monitor_running = false;
        nimcp_thread_join(switcher->monitor_thread, NULL);
        LOG_DEBUG("[%s] Monitoring thread stopped", PORTIA_MODULE_NAME);
    }

    // Destroy mutexes
    if (switcher->callback_mutex_init) {
        nimcp_platform_mutex_destroy(&switcher->callback_mutex);
    }

    nimcp_platform_mutex_destroy(&switcher->state_mutex);

    // Clear magic
    switcher->magic = 0;

    // Free memory
    nimcp_free(switcher);

    LOG_INFO("[%s] Shutdown complete", PORTIA_MODULE_NAME);
    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "tier_switch_shutdown", "success");
}

//=============================================================================
// Public API - Evaluation and Decision
//=============================================================================

bool portia_tier_switch_evaluate(
    portia_tier_switch_t switcher,
    platform_tier_t* target_tier,
    tier_switch_trigger_t* trigger)
{
    if (!validate_switcher(switcher)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_evaluate: validate_switcher is NULL");
        return false;
    }

    if (!bbb_validate_pointer(NULL, target_tier, sizeof(platform_tier_t), NULL) ||
        !bbb_validate_pointer(NULL, trigger, sizeof(tier_switch_trigger_t), NULL)) {
        LOG_ERROR("[%s] Invalid output parameters", PORTIA_MODULE_NAME);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_evaluate: bbb_validate_pointer is NULL");
        return false;
    }

    uint64_t eval_start = get_time_ms();

    // Query current system metrics
    if (!query_system_metrics(switcher)) {
        LOG_ERROR("[%s] Failed to query system metrics", PORTIA_MODULE_NAME);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_evaluate: query_system_metrics is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&switcher->state_mutex);

    // Check if switch is already in progress
    if (switcher->state.switch_in_progress) {
        LOG_DEBUG("[%s] Switch already in progress, skipping evaluation", PORTIA_MODULE_NAME);
        nimcp_platform_mutex_unlock(&switcher->state_mutex);
        return false;
    }

    platform_tier_t current = switcher->state.current_tier;
    uint64_t now = get_time_ms();
    uint64_t time_since_last_switch = now - switcher->state.last_switch_time_ms;

    // Update evaluation timestamp
    switcher->state.last_evaluation_ms = now;

    // Default: stay at current tier
    *target_tier = current;
    *trigger = TIER_SWITCH_TRIGGER_INIT;
    bool should_switch = false;

    // --- Continuous pressure-driven evaluation ---
    // Use the composite resource_pressure (computed in query_system_metrics)
    // to derive the target tier. Identify dominant trigger for logging/backward compat.
    float pressure = switcher->state.resource_pressure;

    // Compute individual normalized pressures to identify dominant trigger
    float mem_p = switcher->state.current_memory_usage_pct / 100.0F;
    float thermal_p = 0.0F;
    if (switcher->state.current_cpu_temp_c > 0.0F && switcher->config.thermal_threshold_c > 0.0F) {
        thermal_p = switcher->state.current_cpu_temp_c / switcher->config.thermal_threshold_c;
    }
    float battery_p = 0.0F;
    if (switcher->state.current_battery_pct < 100.0F) {
        battery_p = 1.0F - (switcher->state.current_battery_pct / 100.0F);
    }
    float load_p = switcher->state.current_cpu_load_pct / 100.0F;

    // Identify dominant trigger (which metric contributes most pressure)
    tier_switch_trigger_t dominant_trigger = TIER_SWITCH_TRIGGER_MEMORY_PRESSURE;
    float max_individual = mem_p;
    if (thermal_p > max_individual) {
        max_individual = thermal_p;
        dominant_trigger = TIER_SWITCH_TRIGGER_THERMAL_THROTTLE;
    }
    if (battery_p > max_individual) {
        max_individual = battery_p;
        dominant_trigger = TIER_SWITCH_TRIGGER_BATTERY_LOW;
    }
    if (load_p > max_individual) {
        max_individual = load_p;
        dominant_trigger = TIER_SWITCH_TRIGGER_LOAD_SPIKE;
    }

    // Derive target tier from continuous pressure
    platform_tier_t pressure_derived_tier = portia_tier_from_pressure(pressure);

    // Determine if a switch is needed based on continuous tier derivation
    if (pressure_derived_tier != current) {
        bool is_downgrade = (pressure_derived_tier > current);  // Higher enum = lower capability
        bool is_upgrade = (pressure_derived_tier < current);

        if (is_downgrade && switcher->config.allow_downgrade) {
            *target_tier = pressure_derived_tier;
            *trigger = dominant_trigger;
            should_switch = true;
            LOG_INFO("[%s] Continuous pressure %.3f -> downgrade %s -> %s (trigger: %s)",
                     PORTIA_MODULE_NAME, (double)pressure,
                     platform_tier_get_name(current),
                     platform_tier_get_name(pressure_derived_tier),
                     portia_tier_switch_trigger_name(dominant_trigger));
        }
        else if (is_upgrade && switcher->config.allow_upgrade) {
            *target_tier = pressure_derived_tier;
            *trigger = TIER_SWITCH_TRIGGER_RESOURCE_AVAILABLE;
            should_switch = true;
            LOG_INFO("[%s] Continuous pressure %.3f -> upgrade %s -> %s",
                     PORTIA_MODULE_NAME, (double)pressure,
                     platform_tier_get_name(current),
                     platform_tier_get_name(pressure_derived_tier));
        }
    }

    // Check hysteresis (except for emergency conditions)
    if (should_switch && *trigger != TIER_SWITCH_TRIGGER_MEMORY_PRESSURE) {
        if (time_since_last_switch < switcher->config.hysteresis_ms) {
            LOG_DEBUG("[%s] Hysteresis active: %lu ms < %u ms",
                      PORTIA_MODULE_NAME,
                      (unsigned long)time_since_last_switch,
                      switcher->config.hysteresis_ms);
            should_switch = false;
        }
    }

    // Emergency downgrade bypasses hysteresis
    if (switcher->config.emergency_downgrade &&
        switcher->state.current_memory_usage_pct > switcher->config.memory_critical_threshold) {
        *target_tier = PLATFORM_TIER_MINIMAL;
        *trigger = TIER_SWITCH_TRIGGER_MEMORY_PRESSURE;
        should_switch = true;
        LOG_WARN("[%s] EMERGENCY: Critical memory pressure %.1f%%",
                 PORTIA_MODULE_NAME,
                 switcher->state.current_memory_usage_pct);
    }

    // Update evaluation time statistics
    uint64_t eval_time = get_time_ms() - eval_start;
    switcher->total_evaluation_time_ms += eval_time;

    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    if (should_switch) {
        LOG_INFO("[%s] Evaluation complete: %s -> %s (trigger: %s, eval_time: %lu ms)",
                 PORTIA_MODULE_NAME,
                 platform_tier_get_name(current),
                 platform_tier_get_name(*target_tier),
                 portia_tier_switch_trigger_name(*trigger),
                 (unsigned long)eval_time);
    } else {
        LOG_DEBUG("[%s] Evaluation complete: staying at %s (eval_time: %lu ms)",
                  PORTIA_MODULE_NAME,
                  platform_tier_get_name(current),
                  (unsigned long)eval_time);
    }

    return should_switch;
}

int portia_tier_switch_execute(
    portia_tier_switch_t switcher,
    platform_tier_t target_tier,
    tier_switch_trigger_t trigger)
{
    if (!validate_switcher(switcher)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_execute: validate_switcher is NULL");
        return -1;
    }

    if (!validate_tier(target_tier)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_execute: validate_tier is NULL");
        return -1;
    }

    uint64_t transition_start = get_time_ms();

    nimcp_platform_mutex_lock(&switcher->state_mutex);

    platform_tier_t current_tier = switcher->state.current_tier;

    // Check if already at target tier
    if (current_tier == target_tier) {
        LOG_DEBUG("[%s] Already at target tier: %s",
                  PORTIA_MODULE_NAME,
                  platform_tier_get_name(target_tier));
        nimcp_platform_mutex_unlock(&switcher->state_mutex);
        return 0;
    }

    // Check if switch already in progress
    if (switcher->state.switch_in_progress) {
        LOG_WARN("[%s] Switch already in progress", PORTIA_MODULE_NAME);
        nimcp_platform_mutex_unlock(&switcher->state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_execute: validation failed");
        return -1;
    }

    // Mark switch in progress
    switcher->state.switch_in_progress = true;
    switcher->state.target_tier = target_tier;
    switcher->state.last_trigger = trigger;

    bool is_upgrade = target_tier < current_tier;  // Lower tier number = higher capability
    bool is_emergency = (trigger == TIER_SWITCH_TRIGGER_MEMORY_PRESSURE &&
                         switcher->state.current_memory_usage_pct > switcher->config.memory_critical_threshold);

    switcher->state.emergency_mode = is_emergency;

    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    LOG_INFO("[%s] Executing tier switch: %s -> %s (trigger: %s, %s%s)",
             PORTIA_MODULE_NAME,
             platform_tier_get_name(current_tier),
             platform_tier_get_name(target_tier),
             portia_tier_switch_trigger_name(trigger),
             is_upgrade ? "UPGRADE" : "DOWNGRADE",
             is_emergency ? ", EMERGENCY" : "");

    // Create event for callbacks
    tier_switch_event_t event;
    event.old_tier = current_tier;
    event.new_tier = target_tier;
    event.trigger = trigger;
    event.timestamp_ms = get_time_ms();
    event.was_emergency = is_emergency;
    event.transition_time_ms = 0;
    snprintf(event.reason, sizeof(event.reason),
             "Tier switch %s -> %s: %s",
             platform_tier_get_name(current_tier),
             platform_tier_get_name(target_tier),
             portia_tier_switch_trigger_name(trigger));

    // Broadcast pre-switch event
    broadcast_tier_switch_event(switcher, &event);

    // In a full implementation, this would:
    // 1. Send bio-async messages to cognitive modules
    // 2. Wait for module acknowledgments
    // 3. Suspend/shutdown modules not in target tier
    // 4. Apply new platform_tier_config_t
    // 5. Initialize/resume modules in target tier
    // For now, we simulate this with a delay

    if (!is_emergency && switcher->config.wait_for_module_ack) {
        LOG_DEBUG("[%s] Waiting for module acknowledgments...", PORTIA_MODULE_NAME);
        usleep(100000);  // 100ms simulation delay
    }

    // Apply new tier configuration
    platform_tier_config_t new_config = platform_tier_get_config(target_tier);
    LOG_INFO("[%s] Applied tier configuration: max_neurons=%u, cognitive_modules=0x%08X",
             PORTIA_MODULE_NAME,
             new_config.max_neurons,
             new_config.cognitive_modules_enabled);

    // Update state
    nimcp_platform_mutex_lock(&switcher->state_mutex);

    switcher->state.previous_tier = current_tier;
    switcher->state.current_tier = target_tier;
    switcher->state.target_tier = target_tier;
    switcher->state.last_switch_time_ms = get_time_ms();
    switcher->state.switch_count++;

    if (is_upgrade) {
        switcher->state.upgrade_count++;
    } else {
        switcher->state.downgrade_count++;
    }

    switcher->state.switch_in_progress = false;
    switcher->state.emergency_mode = false;

    uint64_t transition_time = get_time_ms() - transition_start;
    switcher->total_transition_time_ms += transition_time;

    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    // Update event with transition time
    event.transition_time_ms = (uint32_t)transition_time;

    // Broadcast post-switch event
    broadcast_tier_switch_event(switcher, &event);

    // Invoke callbacks
    invoke_callbacks(switcher, &event);

    LOG_INFO("[%s] Tier switch complete: %s -> %s (transition_time: %lu ms)",
             PORTIA_MODULE_NAME,
             platform_tier_get_name(current_tier),
             platform_tier_get_name(target_tier),
             (unsigned long)transition_time);

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "tier_switch_success", "%s", event.reason);

    return 0;
}

bool portia_tier_switch_can_upgrade(
    portia_tier_switch_t switcher,
    platform_tier_t target_tier)
{
    if (!validate_switcher(switcher) || !validate_tier(target_tier)) {
        return false;
    }

    // Query system metrics
    if (!query_system_metrics(switcher)) {
        LOG_ERROR("[%s] Failed to query system metrics", PORTIA_MODULE_NAME);
        return false;
    }

    nimcp_platform_mutex_lock(&switcher->state_mutex);

    platform_tier_t current = switcher->state.current_tier;

    // Can't upgrade if already at target or higher
    if (current <= target_tier) {
        nimcp_platform_mutex_unlock(&switcher->state_mutex);
        return false;
    }

    // Check hysteresis
    uint64_t now = get_time_ms();
    uint64_t time_since_last = now - switcher->state.last_switch_time_ms;
    if (time_since_last < switcher->config.hysteresis_ms) {
        LOG_DEBUG("[%s] Hysteresis active, cannot upgrade yet", PORTIA_MODULE_NAME);
        nimcp_platform_mutex_unlock(&switcher->state_mutex);
        return false;
    }

    // Use continuous pressure to determine if upgrade is safe
    float pressure = switcher->state.resource_pressure;
    platform_tier_t pressure_tier = portia_tier_from_pressure(pressure);

    // Can only upgrade if pressure supports the target tier
    bool can_upgrade = (pressure_tier <= target_tier);

    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    if (can_upgrade) {
        LOG_DEBUG("[%s] Upgrade to %s is safe (pressure=%.3f, pressure_tier=%s)",
                  PORTIA_MODULE_NAME,
                  platform_tier_get_name(target_tier),
                  (double)pressure,
                  platform_tier_get_name(pressure_tier));
    } else {
        LOG_DEBUG("[%s] Upgrade blocked: pressure=%.3f -> %s (need <= %s)",
                  PORTIA_MODULE_NAME,
                  (double)pressure,
                  platform_tier_get_name(pressure_tier),
                  platform_tier_get_name(target_tier));
    }

    return can_upgrade;
}

bool portia_tier_switch_can_downgrade(
    portia_tier_switch_t switcher,
    platform_tier_t* target_tier,
    tier_switch_trigger_t* trigger)
{
    if (!validate_switcher(switcher)) {
        return false;
    }

    if (!bbb_validate_pointer(NULL, target_tier, sizeof(platform_tier_t), NULL) ||
        !bbb_validate_pointer(NULL, trigger, sizeof(tier_switch_trigger_t), NULL)) {
        LOG_ERROR("[%s] Invalid output parameters", PORTIA_MODULE_NAME);
        return false;
    }

    // Query system metrics
    if (!query_system_metrics(switcher)) {
        LOG_ERROR("[%s] Failed to query system metrics", PORTIA_MODULE_NAME);
        return false;
    }

    nimcp_platform_mutex_lock(&switcher->state_mutex);

    platform_tier_t current = switcher->state.current_tier;
    *target_tier = current;
    *trigger = TIER_SWITCH_TRIGGER_INIT;
    bool should_downgrade = false;

    // Can't downgrade if already at minimum
    if (current == PLATFORM_TIER_MINIMAL) {
        nimcp_platform_mutex_unlock(&switcher->state_mutex);
        return false;
    }

    // Use continuous pressure to determine downgrade need
    float pressure = switcher->state.resource_pressure;
    platform_tier_t pressure_tier = portia_tier_from_pressure(pressure);

    if (pressure_tier > current) {
        // Pressure-derived tier is lower capability than current -> downgrade needed
        *target_tier = pressure_tier;

        // Identify dominant trigger from individual pressures
        float mem_p = switcher->state.current_memory_usage_pct / 100.0F;
        float thermal_p = 0.0F;
        if (switcher->state.current_cpu_temp_c > 0.0F && switcher->config.thermal_threshold_c > 0.0F) {
            thermal_p = switcher->state.current_cpu_temp_c / switcher->config.thermal_threshold_c;
        }
        float battery_p = 0.0F;
        if (switcher->state.current_battery_pct < 100.0F) {
            battery_p = 1.0F - (switcher->state.current_battery_pct / 100.0F);
        }
        float load_p = switcher->state.current_cpu_load_pct / 100.0F;

        *trigger = TIER_SWITCH_TRIGGER_MEMORY_PRESSURE;
        float max_p = mem_p;
        if (thermal_p > max_p) { max_p = thermal_p; *trigger = TIER_SWITCH_TRIGGER_THERMAL_THROTTLE; }
        if (battery_p > max_p) { max_p = battery_p; *trigger = TIER_SWITCH_TRIGGER_BATTERY_LOW; }
        if (load_p > max_p) { *trigger = TIER_SWITCH_TRIGGER_LOAD_SPIKE; }

        should_downgrade = true;
    }

    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    if (should_downgrade) {
        LOG_DEBUG("[%s] Downgrade needed: %s -> %s (trigger: %s)",
                  PORTIA_MODULE_NAME,
                  platform_tier_get_name(current),
                  platform_tier_get_name(*target_tier),
                  portia_tier_switch_trigger_name(*trigger));
    }

    return should_downgrade;
}

//=============================================================================
// Public API - State Query
//=============================================================================

int portia_tier_switch_get_state(
    portia_tier_switch_t switcher,
    tier_switch_state_t* state)
{
    if (!validate_switcher(switcher)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_get_state: validate_switcher is NULL");
        return -1;
    }

    if (!bbb_validate_pointer(NULL, state, sizeof(tier_switch_state_t), NULL)) {
        LOG_ERROR("[%s] Invalid state pointer", PORTIA_MODULE_NAME);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_get_state: bbb_validate_pointer is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(&switcher->state_mutex);
    memcpy(state, &switcher->state, sizeof(tier_switch_state_t));
    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    return 0;
}

platform_tier_t portia_tier_switch_get_current_tier(portia_tier_switch_t switcher) {
    if (!validate_switcher(switcher)) {
        return PLATFORM_TIER_MINIMAL;
    }

    nimcp_platform_mutex_lock(&switcher->state_mutex);
    platform_tier_t tier = switcher->state.current_tier;
    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    return tier;
}

bool portia_tier_switch_is_transitioning(portia_tier_switch_t switcher) {
    if (!validate_switcher(switcher)) {
        return false;
    }

    nimcp_platform_mutex_lock(&switcher->state_mutex);
    bool transitioning = switcher->state.switch_in_progress;
    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    return transitioning;
}

int portia_tier_switch_get_statistics(
    portia_tier_switch_t switcher,
    uint32_t* total_switches,
    uint32_t* upgrades,
    uint32_t* downgrades,
    uint32_t* failed)
{
    if (!validate_switcher(switcher)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_get_statistics: validate_switcher is NULL");
        return -1;
    }

    if (!bbb_validate_pointer(NULL, total_switches, sizeof(uint32_t), NULL) ||
        !bbb_validate_pointer(NULL, upgrades, sizeof(uint32_t), NULL) ||
        !bbb_validate_pointer(NULL, downgrades, sizeof(uint32_t), NULL) ||
        !bbb_validate_pointer(NULL, failed, sizeof(uint32_t), NULL)) {
        LOG_ERROR("[%s] Invalid output parameters", PORTIA_MODULE_NAME);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_get_statistics: bbb_validate_pointer is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(&switcher->state_mutex);
    *total_switches = switcher->state.switch_count;
    *upgrades = switcher->state.upgrade_count;
    *downgrades = switcher->state.downgrade_count;
    *failed = switcher->state.failed_switch_count;
    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    return 0;
}

//=============================================================================
// Public API - Configuration
//=============================================================================

void portia_tier_switch_set_auto_switch(
    portia_tier_switch_t switcher,
    bool enabled)
{
    if (!validate_switcher(switcher)) {
        return;
    }

    nimcp_platform_mutex_lock(&switcher->state_mutex);
    bool was_enabled = switcher->state.auto_switch_active;
    switcher->state.auto_switch_active = enabled;
    switcher->config.auto_switch_enabled = enabled;
    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    LOG_INFO("[%s] Auto-switch %s -> %s",
             PORTIA_MODULE_NAME,
             was_enabled ? "enabled" : "disabled",
             enabled ? "enabled" : "disabled");

    // Note: In full implementation, would start/stop monitoring thread here
}

int portia_tier_switch_update_config(
    portia_tier_switch_t switcher,
    const tier_switch_config_t* config)
{
    if (!validate_switcher(switcher) || !validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_update_config: required parameter is NULL (validate_switcher, validate_config)");
        return -1;
    }

    nimcp_platform_mutex_lock(&switcher->state_mutex);
    memcpy(&switcher->config, config, sizeof(tier_switch_config_t));
    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    LOG_INFO("[%s] Configuration updated", PORTIA_MODULE_NAME);
    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "config_update", "success");

    return 0;
}

void portia_tier_switch_set_callback(
    portia_tier_switch_t switcher,
    tier_switch_callback_t callback,
    void* user_data)
{
    if (!validate_switcher(switcher)) {
        return;
    }

    if (!callback) {
        LOG_ERROR("[%s] Invalid callback pointer", PORTIA_MODULE_NAME);
        return;
    }

    nimcp_platform_mutex_lock(&switcher->callback_mutex);

    // Find first available slot
    for (int i = 0; i < PORTIA_MAX_CALLBACKS; i++) {
        if (!switcher->callbacks[i].active) {
            switcher->callbacks[i].callback = callback;
            switcher->callbacks[i].user_data = user_data;
            switcher->callbacks[i].active = true;
            LOG_DEBUG("[%s] Callback registered in slot %d", PORTIA_MODULE_NAME, i);
            nimcp_platform_mutex_unlock(&switcher->callback_mutex);
            return;
        }
    }

    nimcp_platform_mutex_unlock(&switcher->callback_mutex);
    LOG_WARN("[%s] No callback slots available", PORTIA_MODULE_NAME);
}

//=============================================================================
// Public API - Manual Control
//=============================================================================

int portia_tier_switch_request(
    portia_tier_switch_t switcher,
    platform_tier_t target_tier)
{
    if (!validate_switcher(switcher) || !validate_tier(target_tier)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_request: required parameter is NULL (validate_switcher, validate_tier)");
        return -1;
    }

    LOG_INFO("[%s] Manual tier switch requested: %s -> %s",
             PORTIA_MODULE_NAME,
             platform_tier_get_name(switcher->state.current_tier),
             platform_tier_get_name(target_tier));

    return portia_tier_switch_execute(switcher, target_tier, TIER_SWITCH_TRIGGER_USER_REQUEST);
}

int portia_tier_switch_emergency_downgrade(portia_tier_switch_t switcher) {
    if (!validate_switcher(switcher)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_tier_switch_emergency_downgrade: validate_switcher is NULL");
        return -1;
    }

    LOG_WARN("[%s] EMERGENCY DOWNGRADE requested", PORTIA_MODULE_NAME);

    nimcp_platform_mutex_lock(&switcher->state_mutex);
    switcher->state.emergency_mode = true;
    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    int result = portia_tier_switch_execute(
        switcher,
        PLATFORM_TIER_MINIMAL,
        TIER_SWITCH_TRIGGER_MEMORY_PRESSURE);

    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "emergency_downgrade",
                  "%s", result == 0 ? "success" : "failed");

    return result;
}

//=============================================================================
// Public API - Utility Functions
//=============================================================================

const char* portia_tier_switch_trigger_name(tier_switch_trigger_t trigger) {
    static const char* names[] = {
        "MEMORY_PRESSURE",
        "THERMAL_THROTTLE",
        "BATTERY_LOW",
        "LOAD_SPIKE",
        "USER_REQUEST",
        "PERFORMANCE_GOAL",
        "RESOURCE_AVAILABLE",
        "INIT"
    };

    if (trigger >= 0 && trigger < TIER_SWITCH_TRIGGER_COUNT) {
        return names[trigger];
    }

    return "UNKNOWN";
}

void portia_tier_switch_print_state(portia_tier_switch_t switcher) {
    if (!validate_switcher(switcher)) {
        return;
    }

    tier_switch_state_t state;
    if (portia_tier_switch_get_state(switcher, &state) != 0) {
        printf("Failed to get state\n");
        return;
    }

    printf("\n=== Portia Tier Switching State ===\n");
    printf("Current Tier:       %s\n", platform_tier_get_name(state.current_tier));
    printf("Target Tier:        %s\n", platform_tier_get_name(state.target_tier));
    printf("Previous Tier:      %s\n", platform_tier_get_name(state.previous_tier));
    printf("Last Trigger:       %s\n", portia_tier_switch_trigger_name(state.last_trigger));
    printf("Switch In Progress: %s\n", state.switch_in_progress ? "YES" : "NO");
    printf("Auto-Switch:        %s\n", state.auto_switch_active ? "ENABLED" : "DISABLED");
    printf("Emergency Mode:     %s\n", state.emergency_mode ? "YES" : "NO");
    printf("\n--- Statistics ---\n");
    printf("Total Switches:     %u\n", state.switch_count);
    printf("Upgrades:           %u\n", state.upgrade_count);
    printf("Downgrades:         %u\n", state.downgrade_count);
    printf("Failed Switches:    %u\n", state.failed_switch_count);
    printf("\n--- Current Metrics ---\n");
    printf("Memory Usage:       %.1f%%\n", state.current_memory_usage_pct);
    printf("CPU Temperature:    %.1f°C\n", state.current_cpu_temp_c);
    printf("Battery Level:      %.1f%%\n", state.current_battery_pct);
    printf("CPU Load:           %.1f%%\n", state.current_cpu_load_pct);
    printf("Resource Pressure:  %.3f\n", state.resource_pressure);
    printf("==================================\n\n");
}

//=============================================================================
// Public API - Continuous Resource Pressure
//=============================================================================

/**
 * @brief Sigmoid function for smooth feature gating
 *
 * Returns 1.0 at low x, 0.0 at high x (inverse sigmoid).
 */
static float portia_sigmoid(float x, float center, float steepness) {
    float exponent = steepness * (x - center);
    // Clamp exponent to prevent overflow
    if (exponent > 30.0F) return 0.0F;
    if (exponent < -30.0F) return 1.0F;
    return 1.0F / (1.0F + expf(exponent));
}

int portia_compute_resource_pressure(
    portia_tier_switch_t switcher,
    float* pressure)
{
    if (!validate_switcher(switcher)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "portia_compute_resource_pressure: validate_switcher failed");
        return -1;
    }

    if (!bbb_validate_pointer(NULL, pressure, sizeof(float), NULL)) {
        LOG_ERROR("[%s] Invalid pressure output pointer", PORTIA_MODULE_NAME);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "portia_compute_resource_pressure: pressure pointer is NULL");
        return -1;
    }

    // Refresh system metrics (updates resource_pressure internally)
    if (!query_system_metrics(switcher)) {
        LOG_ERROR("[%s] Failed to query system metrics for pressure", PORTIA_MODULE_NAME);
        return -1;
    }

    nimcp_platform_mutex_lock(&switcher->state_mutex);
    *pressure = switcher->state.resource_pressure;
    nimcp_platform_mutex_unlock(&switcher->state_mutex);

    return 0;
}

int portia_compute_allocation(
    float resource_pressure,
    portia_allocation_t* out)
{
    if (!out) {
        return -1;
    }

    // Clamp input to [0.0, 1.0]
    if (resource_pressure < 0.0F) resource_pressure = 0.0F;
    if (resource_pressure > 1.0F) resource_pressure = 1.0F;

    // Quality scale: sigmoid centered at 0.7, steepness 10
    out->quality_scale = portia_sigmoid(resource_pressure, 0.7F, 10.0F);

    // Allocation fraction: linear with floor at 10%
    out->allocation_fraction = 0.1F + 0.9F * (1.0F - resource_pressure);

    // Feature gates: sigmoids with different centers (costlier features gate off earlier)
    out->feature_gate_plasticity = portia_sigmoid(resource_pressure, 0.5F, 12.0F);
    out->feature_gate_learning   = portia_sigmoid(resource_pressure, 0.55F, 12.0F);
    out->feature_gate_emotions   = portia_sigmoid(resource_pressure, 0.6F, 12.0F);
    out->feature_gate_planning   = portia_sigmoid(resource_pressure, 0.65F, 12.0F);
    out->feature_gate_meta       = portia_sigmoid(resource_pressure, 0.45F, 12.0F);

    // Budget scales: sigmoid curves
    out->compute_budget_scale = portia_sigmoid(resource_pressure, 0.6F, 8.0F);
    out->memory_budget_scale  = portia_sigmoid(resource_pressure, 0.65F, 8.0F);

    // Thread budget: linear with floor at 20%
    out->thread_budget_scale = 0.2F + 0.8F * (1.0F - resource_pressure);

    // Derive tier label from pressure for backward compatibility
    out->derived_tier = portia_tier_from_pressure(resource_pressure);

    LOG_DEBUG("[%s] Allocation: pressure=%.3f, quality=%.3f, alloc=%.3f, tier=%s",
              PORTIA_MODULE_NAME,
              (double)resource_pressure,
              (double)out->quality_scale,
              (double)out->allocation_fraction,
              platform_tier_get_name(out->derived_tier));

    return 0;
}

platform_tier_t portia_tier_from_pressure(float resource_pressure) {
    // Clamp
    if (resource_pressure < 0.0F) resource_pressure = 0.0F;
    if (resource_pressure > 1.0F) resource_pressure = 1.0F;

    // Map continuous pressure to discrete tier labels (for logging only)
    if (resource_pressure < 0.25F) {
        return PLATFORM_TIER_FULL;
    } else if (resource_pressure < 0.50F) {
        return PLATFORM_TIER_MEDIUM;
    } else if (resource_pressure < 0.75F) {
        return PLATFORM_TIER_CONSTRAINED;
    } else {
        return PLATFORM_TIER_MINIMAL;
    }
}

int portia_tier_switch_get_allocation(
    portia_tier_switch_t switcher,
    portia_allocation_t* out)
{
    if (!validate_switcher(switcher)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "portia_tier_switch_get_allocation: validate_switcher failed");
        return -1;
    }

    if (!out) {
        return -1;
    }

    float pressure;
    int result = portia_compute_resource_pressure(switcher, &pressure);
    if (result != 0) {
        return result;
    }

    return portia_compute_allocation(pressure, out);
}
