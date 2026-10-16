/**
 * @file nimcp_portia_power.c
 * @brief Power-Aware Tier System Implementation
 *
 * WHAT: Battery monitoring and power-aware resource management
 * WHY:  Extend NIMCP battery life on mobile/embedded platforms
 * HOW:  Monitor /sys/class/power_supply, auto-adjust tier configs
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "portia/nimcp_portia_power.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_power)

//=============================================================================
// Constants
//=============================================================================

#define POWER_MODULE "PORTIA_POWER"
#define POWER_MAGIC 0x504F5752  // 'POWR'

// Battery sysfs paths
#define BAT0_PATH "/sys/class/power_supply/BAT0"
#define BAT1_PATH "/sys/class/power_supply/BAT1"
#define AC_PATH "/sys/class/power_supply/AC"

// Discharge rate history size
#define MAX_DISCHARGE_SAMPLES 60

// Default thresholds
#define DEFAULT_PERFORMANCE_THRESHOLD 80.0f
#define DEFAULT_BALANCED_THRESHOLD 40.0f
#define DEFAULT_SAVER_THRESHOLD 20.0f
#define DEFAULT_CRITICAL_THRESHOLD 10.0f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Discharge rate history for runtime estimation
 */
typedef struct {
    float samples[MAX_DISCHARGE_SAMPLES];
    uint32_t count;
    uint32_t index;
    float sum;
} discharge_history_t;

/**
 * @brief Power manager internal state
 */
typedef struct portia_power_manager_struct {
    uint32_t magic;
    bool initialized;

    // Configuration
    portia_power_config_t config;

    // Current state
    power_status_t current_status;
    power_profile_t current_profile;
    power_profile_t forced_profile;
    bool profile_forced;

    // Battery path
    char battery_path[NIMCP_SHORT_PATH_SIZE];
    bool battery_available;

    // Discharge history
    discharge_history_t discharge_history;

    // Statistics
    portia_power_stats_t stats;

    // Threading
    nimcp_thread_t poll_thread;
    bool poll_thread_running;
    nimcp_mutex_t* state_mutex;

    // Bio-async
    uint32_t bio_async_module_id;
    bool bio_async_registered;
    bio_module_context_t bio_module_ctx;  /* Bio-router module context for messaging */

    // BBB security
    bbb_system_t bbb;
} portia_power_manager_struct_t;

//=============================================================================
// Forward Declarations
//=============================================================================

static void* power_poll_thread(void* arg);
static bool read_battery_status(portia_power_manager_t mgr, power_status_t* status);
static float read_sysfs_float(const char* path, const char* file);
static const char* read_sysfs_string(const char* path, const char* file, char* buf, size_t len);
static power_profile_t determine_profile(const power_status_t* status, const portia_power_config_t* config);
static void send_power_event(portia_power_manager_t mgr, power_event_type_t event, const power_status_t* status);
static void update_discharge_history(discharge_history_t* hist, float rate);
static float calculate_avg_discharge_rate(const discharge_history_t* hist);

//=============================================================================
// Configuration API
//=============================================================================

portia_power_config_t portia_power_default_config(void) {
    portia_power_config_t config = {0};

    // Monitoring
    config.poll_interval_ms = 5000;  // 5 seconds
    config.auto_adjust_profile = true;
    config.enable_bio_async_events = true;

    // Thresholds
    config.performance_threshold = DEFAULT_PERFORMANCE_THRESHOLD;
    config.balanced_threshold = DEFAULT_BALANCED_THRESHOLD;
    config.saver_threshold = DEFAULT_SAVER_THRESHOLD;
    config.critical_threshold = DEFAULT_CRITICAL_THRESHOLD;

    // Thermal
    config.max_safe_temp_c = 45.0F;
    config.thermal_throttle_temp_c = 40.0F;

    // Runtime estimation
    config.discharge_history_s = 60.0F;

    // Platform
    config.battery_path = NULL;
    config.force_battery_mode = false;

    return config;
}

//=============================================================================
// Lifecycle API
//=============================================================================

portia_power_manager_t portia_power_init(const portia_power_config_t* config) {
    LOG_INFO("[%s] Initializing power monitoring system", POWER_MODULE);

    // Allocate manager
    portia_power_manager_t mgr = (portia_power_manager_t)nimcp_calloc(
        1, sizeof(portia_power_manager_struct_t));
    if (!mgr) {
        LOG_ERROR("[%s] Failed to allocate power manager", POWER_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mgr allocation failed");

        return NULL;
    }

    mgr->magic = POWER_MAGIC;

    // Copy configuration
    if (config) {
        mgr->config = *config;
    } else {
        mgr->config = portia_power_default_config();
    }

    // Initialize mutex
    mgr->state_mutex = nimcp_mutex_create(NULL);
    if (!mgr->state_mutex) {
        LOG_ERROR("[%s] Failed to create mutex", POWER_MODULE);
        nimcp_free(mgr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_power_init: mutex creation failed");
        return NULL;
    }

    // Detect battery path
    mgr->battery_available = false;
    if (mgr->config.battery_path) {
        strncpy(mgr->battery_path, mgr->config.battery_path, sizeof(mgr->battery_path) - 1);
        mgr->battery_available = (access(mgr->battery_path, F_OK) == 0);
    } else {
        // Try BAT0 first
        if (access(BAT0_PATH, F_OK) == 0) {
            strncpy(mgr->battery_path, BAT0_PATH, sizeof(mgr->battery_path) - 1);
            mgr->battery_available = true;
        }
        // Try BAT1
        else if (access(BAT1_PATH, F_OK) == 0) {
            strncpy(mgr->battery_path, BAT1_PATH, sizeof(mgr->battery_path) - 1);
            mgr->battery_available = true;
        }
    }

    if (mgr->battery_available) {
        LOG_INFO("[%s] Battery detected at: %s", POWER_MODULE, mgr->battery_path);
    } else {
        LOG_WARN("[%s] No battery detected, power monitoring limited", POWER_MODULE);
    }

    // Read initial status
    if (!read_battery_status(mgr, &mgr->current_status)) {
        LOG_WARN("[%s] Failed to read initial battery status", POWER_MODULE);
    }

    // Determine initial profile
    mgr->current_profile = determine_profile(&mgr->current_status, &mgr->config);
    mgr->forced_profile = POWER_PROFILE_BALANCED;
    mgr->profile_forced = false;

    LOG_INFO("[%s] Initial power profile: %s (%.1f%%)",
             POWER_MODULE,
             portia_power_get_profile_name(mgr->current_profile),
             mgr->current_status.battery_level_pct);

    /* Register with bio-async if enabled */
    if (mgr->config.enable_bio_async_events) {
        /* Register power manager module with bio-router */
        bio_module_info_t module_info = {
            .module_id = BIO_MODULE_SYSTEM,
            .module_name = "PortiaPower",
            .inbox_capacity = 64,
            .user_data = mgr
        };

        mgr->bio_module_ctx = bio_router_register_module(&module_info);
        if (mgr->bio_module_ctx) {
            mgr->bio_async_module_id = BIO_MODULE_SYSTEM;
            mgr->bio_async_registered = true;
            LOG_DEBUG("[%s] Bio-async events enabled and registered", POWER_MODULE);
        } else {
            mgr->bio_async_module_id = BIO_MODULE_SYSTEM;
            mgr->bio_async_registered = false;
            LOG_WARN("[%s] Bio-async registration failed, events disabled", POWER_MODULE);
        }
    }

    // Set running flag BEFORE creating thread to avoid race
    mgr->poll_thread_running = true;

    // Start polling thread
    if (nimcp_thread_create(&mgr->poll_thread, power_poll_thread, mgr, NULL) != 0) {
        LOG_ERROR("[%s] Failed to create polling thread", POWER_MODULE);
        mgr->poll_thread_running = false;
        nimcp_mutex_destroy(mgr->state_mutex);
        nimcp_free(mgr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_power_init: thread creation failed");
        return NULL;
    }
    mgr->initialized = true;

    LOG_INFO("[%s] Power monitoring initialized successfully", POWER_MODULE);
    return mgr;
}

void portia_power_shutdown(portia_power_manager_t mgr) {
    if (!mgr || mgr->magic != POWER_MAGIC) {
        return;
    }

    LOG_INFO("[%s] Shutting down power monitoring", POWER_MODULE);

    // Stop polling thread
    mgr->poll_thread_running = false;
    if (mgr->poll_thread) {
        nimcp_thread_join(mgr->poll_thread, NULL);
        mgr->poll_thread = 0;
    }

    // Cleanup
    if (mgr->state_mutex) {
        nimcp_mutex_destroy(mgr->state_mutex);
        mgr->state_mutex = NULL;
    }

    mgr->magic = 0;
    mgr->initialized = false;

    nimcp_free(mgr);

    LOG_INFO("[%s] Power monitoring shutdown complete", POWER_MODULE);
}

//=============================================================================
// Status Query API
//=============================================================================

bool portia_power_get_status(portia_power_manager_t mgr, power_status_t* status) {
    if (!mgr || mgr->magic != POWER_MAGIC || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_power_get_status: required parameter is NULL (mgr, status)");
        return false;
    }

    // Validate pointer
    bbb_validation_result_t result;
    if (mgr->bbb && !bbb_validate_pointer(mgr->bbb, status, sizeof(*status), &result)) {
        LOG_ERROR("[%s] Invalid status pointer: %s", POWER_MODULE, result.reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_power_get_status: bbb_validate_pointer is NULL");
        return false;
    }

    nimcp_mutex_lock(mgr->state_mutex);
    *status = mgr->current_status;
    nimcp_mutex_unlock(mgr->state_mutex);

    return true;
}

power_profile_t portia_power_get_profile(portia_power_manager_t mgr) {
    if (!mgr || mgr->magic != POWER_MAGIC) {
        return POWER_PROFILE_BALANCED;
    }

    nimcp_mutex_lock(mgr->state_mutex);

    power_profile_t profile;
    if (mgr->profile_forced) {
        profile = mgr->forced_profile;
    } else {
        profile = mgr->current_profile;
    }

    nimcp_mutex_unlock(mgr->state_mutex);

    return profile;
}

power_profile_t portia_power_set_profile(portia_power_manager_t mgr, power_profile_t profile) {
    if (!mgr || mgr->magic != POWER_MAGIC) {
        return POWER_PROFILE_BALANCED;
    }

    // Validate profile
    if (profile >= POWER_PROFILE_COUNT) {
        LOG_ERROR("[%s] Invalid power profile: %d", POWER_MODULE, profile);
        return mgr->current_profile;
    }

    nimcp_mutex_lock(mgr->state_mutex);

    power_profile_t old_profile = mgr->profile_forced ? mgr->forced_profile : mgr->current_profile;

    mgr->forced_profile = profile;
    mgr->profile_forced = true;

    nimcp_mutex_unlock(mgr->state_mutex);

    LOG_INFO("[%s] Power profile manually set to %s (was %s)",
             POWER_MODULE,
             portia_power_get_profile_name(profile),
             portia_power_get_profile_name(old_profile));

    // Send bio-async event
    if (old_profile != profile) {
        send_power_event(mgr, POWER_EVENT_PROFILE_CHANGE, &mgr->current_status);
    }

    return old_profile;
}

//=============================================================================
// Runtime Estimation API
//=============================================================================

float portia_power_estimate_runtime(portia_power_manager_t mgr, float safety_margin) {
    if (!mgr || mgr->magic != POWER_MAGIC) {
        return 0.0F;
    }

    // Validate safety margin
    if (safety_margin < 0.0F || safety_margin > 1.0F) {
        safety_margin = 0.9F;  // Default
    }

    nimcp_mutex_lock(mgr->state_mutex);

    // If on AC, runtime is "infinite"
    if (mgr->current_status.source == POWER_SOURCE_AC ||
        mgr->current_status.plugged_in) {
        nimcp_mutex_unlock(mgr->state_mutex);
        return 0.0F;  // 0 = unlimited
    }

    // Calculate average discharge rate
    float avg_rate_mw = calculate_avg_discharge_rate(&mgr->discharge_history);
    if (avg_rate_mw <= 0.0F) {
        avg_rate_mw = mgr->current_status.discharge_rate_mw;
    }

    // Estimate remaining capacity (assume 50Wh typical battery)
    float capacity_wh = 50.0F * (mgr->current_status.battery_level_pct / 100.0F);
    float capacity_mw_s = capacity_wh * 3600.0F * 1000.0F;  // Convert to mW*s

    // Runtime = capacity / discharge_rate
    float runtime_s = 0.0F;
    if (avg_rate_mw > 0.0F) {
        runtime_s = capacity_mw_s / avg_rate_mw;
        runtime_s *= safety_margin;
    }

    nimcp_mutex_unlock(mgr->state_mutex);

    return runtime_s;
}

//=============================================================================
// Configuration API
//=============================================================================

power_tier_config_t portia_power_get_tier_config(
    portia_power_manager_t mgr,
    platform_tier_t base_tier,
    power_profile_t profile)
{
    power_tier_config_t config = {0};

    // Get current profile if not specified
    if (profile < 0 || profile >= POWER_PROFILE_COUNT) {
        profile = portia_power_get_profile(mgr);
    }

    config.profile = profile;

    // Get base tier configuration
    platform_tier_config_t base_config = platform_tier_get_config(base_tier);

    // Apply power profile scaling
    float neuron_scale = 1.0F;
    float rate_scale = 1.0F;
    uint32_t cognitive_modules = base_config.cognitive_modules_enabled;

    switch (profile) {
        case POWER_PROFILE_PERFORMANCE:
            // Full capabilities
            neuron_scale = 1.0F;
            rate_scale = 1.0F;
            // All modules enabled
            break;

        case POWER_PROFILE_BALANCED:
            // Moderate reduction
            neuron_scale = 0.75F;
            rate_scale = 0.8F;
            // Most modules enabled
            break;

        case POWER_PROFILE_SAVER:
            // Significant reduction
            neuron_scale = 0.5F;
            rate_scale = 0.5F;
            // Disable non-essential modules
            cognitive_modules &= ~(COGNITIVE_MODULE_CURIOSITY |
                                   COGNITIVE_MODULE_META_LEARNING |
                                   COGNITIVE_MODULE_INTROSPECTION |
                                   COGNITIVE_MODULE_THEORY_OF_MIND);
            break;

        case POWER_PROFILE_CRITICAL:
            // Minimal operation
            neuron_scale = 0.25F;
            rate_scale = 0.2F;
            // Essential modules only
            cognitive_modules = (COGNITIVE_MODULE_ATTENTION |
                                COGNITIVE_MODULE_WORKING_MEMORY |
                                COGNITIVE_MODULE_SALIENCE);
            break;

        case POWER_PROFILE_EMERGENCY:
            // Survival mode
            neuron_scale = 0.1F;
            rate_scale = 0.05F;
            // Reactive only
            cognitive_modules = COGNITIVE_MODULE_ATTENTION;
            break;

        default:
            neuron_scale = 1.0F;
            rate_scale = 1.0F;
            break;
    }

    // Apply scaling
    config.max_neurons = (uint32_t)(base_config.max_neurons * neuron_scale);
    config.max_synapses = config.max_neurons * 100;  // Typical synapse ratio
    config.cognitive_modules = cognitive_modules;
    config.processing_rate_hz = 100.0F * rate_scale;
    config.sampling_rate = base_config.sampling_rate * rate_scale;
    config.batch_size = base_config.update_batch_size;

    // Feature flags
    config.enable_learning = (profile <= POWER_PROFILE_BALANCED);
    config.enable_persistence = (profile <= POWER_PROFILE_SAVER);
    config.enable_bio_async = base_config.enable_bio_async;
    config.enable_gpu = (profile == POWER_PROFILE_PERFORMANCE && base_config.enable_gpu);

    // Sleep/wake for low power modes
    if (profile >= POWER_PROFILE_SAVER) {
        config.wake_interval_s = 1.0F / config.processing_rate_hz;
        config.active_duty_cycle = (profile == POWER_PROFILE_SAVER) ? 0.5F : 0.25F;
    } else {
        config.wake_interval_s = 0.0F;
        config.active_duty_cycle = 1.0F;
    }

    // Resource budgets
    config.memory_budget_mb = (uint32_t)(base_config.memory_budget_mb * neuron_scale);
    config.compute_budget_gops = (uint32_t)(base_config.compute_budget_ops / 1000000 * rate_scale);

    return config;
}

//=============================================================================
// Statistics API
//=============================================================================

bool portia_power_get_stats(portia_power_manager_t mgr, portia_power_stats_t* stats) {
    if (!mgr || mgr->magic != POWER_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_power_get_stats: required parameter is NULL (mgr, stats)");
        return false;
    }

    // Validate pointer
    bbb_validation_result_t result;
    if (mgr->bbb && !bbb_validate_pointer(mgr->bbb, stats, sizeof(*stats), &result)) {
        LOG_ERROR("[%s] Invalid stats pointer: %s", POWER_MODULE, result.reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_power_get_stats: bbb_validate_pointer is NULL");
        return false;
    }

    nimcp_mutex_lock(mgr->state_mutex);
    *stats = mgr->stats;
    nimcp_mutex_unlock(mgr->state_mutex);

    return true;
}

void portia_power_reset_stats(portia_power_manager_t mgr) {
    if (!mgr || mgr->magic != POWER_MAGIC) {
        return;
    }

    nimcp_mutex_lock(mgr->state_mutex);
    memset(&mgr->stats, 0, sizeof(mgr->stats));
    nimcp_mutex_unlock(mgr->state_mutex);

    LOG_INFO("[%s] Statistics reset", POWER_MODULE);
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* portia_power_get_source_name(power_source_t source) {
    switch (source) {
        case POWER_SOURCE_AC: return "AC";
        case POWER_SOURCE_BATTERY: return "Battery";
        case POWER_SOURCE_SOLAR: return "Solar";
        case POWER_SOURCE_USB: return "USB";
        case POWER_SOURCE_UNKNOWN: return "Unknown";
        default: return "Invalid";
    }
}

const char* portia_power_get_profile_name(power_profile_t profile) {
    switch (profile) {
        case POWER_PROFILE_PERFORMANCE: return "Performance";
        case POWER_PROFILE_BALANCED: return "Balanced";
        case POWER_PROFILE_SAVER: return "Power Saver";
        case POWER_PROFILE_CRITICAL: return "Critical";
        case POWER_PROFILE_EMERGENCY: return "Emergency";
        default: return "Invalid";
    }
}

bool portia_power_is_limited(power_source_t source) {
    return (source == POWER_SOURCE_BATTERY || source == POWER_SOURCE_SOLAR);
}

//=============================================================================
// Internal Functions
//=============================================================================

static void* power_poll_thread(void* arg) {
    portia_power_manager_t mgr = (portia_power_manager_t)arg;

    LOG_DEBUG("[%s] Polling thread started", POWER_MODULE);

    while (mgr->poll_thread_running) {
        power_status_t new_status = {0};

        // Read battery status
        if (read_battery_status(mgr, &new_status)) {
            nimcp_mutex_lock(mgr->state_mutex);

            power_status_t old_status = mgr->current_status;
            mgr->current_status = new_status;

            // Update discharge history
            if (!new_status.charging && new_status.discharge_rate_mw > 0.0F) {
                update_discharge_history(&mgr->discharge_history, new_status.discharge_rate_mw);
            }

            // Update statistics
            mgr->stats.samples_taken++;
            if (mgr->stats.samples_taken > 0) {
                float new_avg_bat = (mgr->stats.avg_battery_level * (mgr->stats.samples_taken - 1) +
                     new_status.battery_level_pct) / mgr->stats.samples_taken;
                if (isfinite(new_avg_bat)) {
                    mgr->stats.avg_battery_level = new_avg_bat;
                }
                float new_avg_rate = (mgr->stats.avg_discharge_rate_mw * (mgr->stats.samples_taken - 1) +
                     new_status.discharge_rate_mw) / mgr->stats.samples_taken;
                if (isfinite(new_avg_rate)) {
                    mgr->stats.avg_discharge_rate_mw = new_avg_rate;
                }
            }
            if (new_status.temperature_c > mgr->stats.max_temperature_c) {
                mgr->stats.max_temperature_c = new_status.temperature_c;
            }

            // Check for profile change
            if (!mgr->profile_forced && mgr->config.auto_adjust_profile) {
                power_profile_t new_profile = determine_profile(&new_status, &mgr->config);
                if (new_profile != mgr->current_profile) {
                    power_profile_t old_profile = mgr->current_profile;
                    mgr->current_profile = new_profile;
                    mgr->stats.profile_changes++;

                    nimcp_mutex_unlock(mgr->state_mutex);

                    LOG_INFO("[%s] Power profile changed: %s -> %s (%.1f%%)",
                             POWER_MODULE,
                             portia_power_get_profile_name(old_profile),
                             portia_power_get_profile_name(new_profile),
                             new_status.battery_level_pct);

                    send_power_event(mgr, POWER_EVENT_PROFILE_CHANGE, &new_status);
                } else {
                    nimcp_mutex_unlock(mgr->state_mutex);
                }
            } else {
                nimcp_mutex_unlock(mgr->state_mutex);
            }

            // Check for other events
            if (new_status.battery_level_pct < mgr->config.critical_threshold &&
                old_status.battery_level_pct >= mgr->config.critical_threshold) {
                send_power_event(mgr, POWER_EVENT_BATTERY_CRITICAL, &new_status);
            }
            else if (new_status.battery_level_pct < mgr->config.saver_threshold &&
                     old_status.battery_level_pct >= mgr->config.saver_threshold) {
                send_power_event(mgr, POWER_EVENT_BATTERY_LOW, &new_status);
            }

            if (new_status.charging && !old_status.charging) {
                send_power_event(mgr, POWER_EVENT_CHARGING_STARTED, &new_status);
            }

            if (new_status.temperature_c > mgr->config.thermal_throttle_temp_c) {
                send_power_event(mgr, POWER_EVENT_THERMAL_WARNING, &new_status);
                nimcp_mutex_lock(mgr->state_mutex);
                mgr->stats.thermal_throttles++;
                nimcp_mutex_unlock(mgr->state_mutex);
            }
        }

        // Sleep until next poll
        nimcp_platform_sleep_ms(mgr->config.poll_interval_ms);
    }

    LOG_DEBUG("[%s] Polling thread exited", POWER_MODULE);
    return NULL;
}

static bool read_battery_status(portia_power_manager_t mgr, power_status_t* status) {
    if (!mgr->battery_available) {
        // No battery, assume AC power
        status->source = POWER_SOURCE_AC;
        status->battery_level_pct = 100.0F;
        status->discharge_rate_mw = 0.0F;
        status->estimated_runtime_s = 0.0F;
        status->temperature_c = 25.0F;
        status->charging = false;
        status->health_good = true;
        status->plugged_in = true;
        status->timestamp_us = nimcp_time_monotonic_us();
        return true;
    }

    // Read capacity
    float capacity = read_sysfs_float(mgr->battery_path, "capacity");
    if (capacity < 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_battery_status: validation failed");
        return false;
    }
    status->battery_level_pct = capacity;

    // Read status (charging/discharging)
    char status_str[32];
    read_sysfs_string(mgr->battery_path, "status", status_str, sizeof(status_str));
    status->charging = (strstr(status_str, "Charging") != NULL);
    status->plugged_in = (strstr(status_str, "Full") != NULL || status->charging);

    // Read power draw
    float power_now = read_sysfs_float(mgr->battery_path, "power_now");
    if (power_now > 0.0F) {
        status->discharge_rate_mw = power_now / 1000.0F;  // Convert μW to mW
    } else {
        status->discharge_rate_mw = 0.0F;
    }

    // Read temperature
    float temp = read_sysfs_float(mgr->battery_path, "temp");
    if (temp > 0.0F) {
        status->temperature_c = temp / 10.0F;  // 0.1°C units
    } else {
        status->temperature_c = 25.0F;  // Default
    }

    // Determine power source
    if (status->plugged_in) {
        status->source = POWER_SOURCE_AC;
    } else {
        status->source = POWER_SOURCE_BATTERY;
    }

    // Health check
    status->health_good = (status->temperature_c < mgr->config.max_safe_temp_c);

    // Timestamp
    status->timestamp_us = nimcp_time_monotonic_us();

    return true;
}

static float read_sysfs_float(const char* path, const char* file) {
    char filepath[NIMCP_METRICS_PATH_SIZE];
    snprintf(filepath, sizeof(filepath), "%s/%s", path, file);

    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        return -1.0F;
    }

    float value = -1.0F;
    if (fscanf(fp, "%f", &value) != 1) {
        value = -1.0F;
    }

    fclose(fp);
    return value;
}

static const char* read_sysfs_string(const char* path, const char* file, char* buf, size_t len) {
    char filepath[NIMCP_METRICS_PATH_SIZE];
    snprintf(filepath, sizeof(filepath), "%s/%s", path, file);

    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        buf[0] = '\0';
        return buf;
    }

    if (fgets(buf, len, fp) == NULL) {
        buf[0] = '\0';
    } else {
        // Remove trailing newline
        size_t slen = strlen(buf);
        if (slen > 0 && buf[slen-1] == '\n') {
            buf[slen-1] = '\0';
        }
    }

    fclose(fp);
    return buf;
}

static power_profile_t determine_profile(const power_status_t* status, const portia_power_config_t* config) {
    // AC power → Performance
    if (status->source == POWER_SOURCE_AC || status->plugged_in) {
        return POWER_PROFILE_PERFORMANCE;
    }

    // Battery thresholds
    if (status->battery_level_pct >= config->performance_threshold) {
        return POWER_PROFILE_PERFORMANCE;
    } else if (status->battery_level_pct >= config->balanced_threshold) {
        return POWER_PROFILE_BALANCED;
    } else if (status->battery_level_pct >= config->saver_threshold) {
        return POWER_PROFILE_SAVER;
    } else if (status->battery_level_pct >= config->critical_threshold) {
        return POWER_PROFILE_CRITICAL;
    } else {
        return POWER_PROFILE_EMERGENCY;
    }
}

static void send_power_event(portia_power_manager_t mgr, power_event_type_t event, const power_status_t* status) {
    if (!mgr->config.enable_bio_async_events || !mgr->bio_async_registered) {
        return;
    }

    /* Guard: Need valid module context for bio-async messaging */
    if (!mgr->bio_module_ctx) {
        LOG_WARN("[%s] Bio-async context not available", POWER_MODULE);
        return;
    }

    /* Create power event message */
    bio_msg_power_event_t msg = {0};
    bio_msg_init_header(&msg.header, (bio_message_type_t)event,
                       mgr->bio_async_module_id, BIO_MODULE_ALL,
                       sizeof(msg) - sizeof(msg.header));

    msg.event_type = event;
    msg.old_profile = mgr->current_profile;
    msg.new_profile = portia_power_get_profile(mgr);
    msg.battery_level_pct = status->battery_level_pct;
    msg.temperature_c = status->temperature_c;
    msg.timestamp_us = status->timestamp_us;

    /* Send via norepinephrine channel, URGENT for critical events */
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    if (event == POWER_EVENT_BATTERY_CRITICAL || event == POWER_EVENT_THERMAL_WARNING) {
        msg.header.flags |= BIO_MSG_FLAG_URGENT;
    }

    /* Send via bio-router broadcast system */
    nimcp_error_t err = bio_router_broadcast(mgr->bio_module_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        LOG_WARN("[%s] Failed to broadcast power event (err=%d)", POWER_MODULE, err);
        return;
    }

    nimcp_mutex_lock(mgr->state_mutex);
    mgr->stats.events_sent++;
    nimcp_mutex_unlock(mgr->state_mutex);

    LOG_DEBUG("[%s] Power event sent: type=%d, profile=%d->%d, battery=%.1f%%",
              POWER_MODULE, event, msg.old_profile, msg.new_profile, msg.battery_level_pct);
}

static void update_discharge_history(discharge_history_t* hist, float rate) {
    // Remove old sample from sum
    if (hist->count >= MAX_DISCHARGE_SAMPLES) {
        hist->sum -= hist->samples[hist->index];
    } else {
        hist->count++;
    }

    // Add new sample
    hist->samples[hist->index] = rate;
    hist->sum += rate;

    // Advance index
    hist->index = (hist->index + 1) % MAX_DISCHARGE_SAMPLES;
}

static float calculate_avg_discharge_rate(const discharge_history_t* hist) {
    if (hist->count == 0) {
        return 0.0F;
    }
    return hist->sum / hist->count;
}
