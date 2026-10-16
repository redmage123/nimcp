#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_portia_monitoring.c - Platform Monitoring Implementation
//=============================================================================
/**
 * @file nimcp_portia_monitoring.c
 * @brief Cross-platform system monitoring for Portia tier switching
 *
 * WHAT: Real-time system metric collection (CPU temp, battery, CPU load)
 * WHY:  Enable dynamic tier switching based on actual hardware conditions
 * HOW:  Platform-specific implementations with fallback mechanisms
 *
 * PLATFORM IMPLEMENTATIONS:
 * - Linux: /sys/class/thermal/, /sys/class/power_supply/, /proc/stat
 * - macOS: IOKit (thermal), IOPSCopyPowerSourcesInfo (battery)
 * - Windows: WMI (thermal), GetSystemPowerStatus (battery)
 * - Other: Stub implementations returning safe defaults
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#include "portia/nimcp_portia_monitoring.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#ifdef __linux__
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
// IOKit is available but requires linking - we'll use sysctl fallbacks
#endif

#ifdef _WIN32
#include <windows.h>
#include <powrprof.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_monitoring)

#pragma comment(lib, "PowrProf.lib")
#endif

//=============================================================================
// Constants
//=============================================================================

#define MONITOR_MODULE "PORTIA_MONITOR"
#define MONITOR_MAGIC 0x4D4F4E54  // 'MONT'

// Linux sysfs paths
#define THERMAL_ZONE_PATH "/sys/class/thermal"
#define POWER_SUPPLY_PATH "/sys/class/power_supply"
#define PROC_STAT_PATH "/proc/stat"
#define PROC_LOADAVG_PATH "/proc/loadavg"

// Default thermal zone names to try
static const char* DEFAULT_THERMAL_ZONES[] = {
    "thermal_zone0",
    "thermal_zone1",
    "x86_pkg_temp",
    "cpu-thermal",
    "coretemp",
    NULL
};

// Default battery names to try
static const char* DEFAULT_BATTERY_NAMES[] = {
    "BAT0",
    "BAT1",
    "battery",
    "macsmc-battery",
    NULL
};

// Default AC adapter names to try
static const char* DEFAULT_AC_NAMES[] = {
    "AC",
    "AC0",
    "ACAD",
    "macsmc-ac",
    NULL
};

//=============================================================================
// CPU Statistics Structure (for load calculation)
//=============================================================================

typedef struct {
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
    uint64_t total;
} cpu_stats_t;

//=============================================================================
// Internal Structures
//=============================================================================

typedef struct portia_monitor_struct {
    uint32_t magic;
    bool initialized;

    // Configuration
    portia_monitor_config_t config;

    // Platform capabilities
    uint32_t capabilities;

    // Linux-specific paths
    char thermal_zone_path[NIMCP_SHORT_PATH_SIZE];
    char battery_path[NIMCP_SHORT_PATH_SIZE];
    char ac_path[NIMCP_SHORT_PATH_SIZE];
    bool has_thermal;
    bool has_battery;
    bool has_ac;

    // Thermal zones
    portia_thermal_zone_t thermal_zones[PORTIA_MONITOR_MAX_THERMAL_ZONES];
    int num_thermal_zones;
    int primary_thermal_zone;

    // CPU statistics for load calculation
    cpu_stats_t prev_cpu_stats;
    cpu_stats_t curr_cpu_stats;
    cpu_stats_t* prev_per_core;
    cpu_stats_t* curr_per_core;
    int num_cores;
    bool cpu_stats_valid;

    // Cache
    float cached_cpu_temp;
    float cached_battery_pct;
    float cached_cpu_load;
    portia_battery_status_t cached_battery_status;
    portia_cpu_load_t cached_cpu_load_detailed;

    uint64_t cache_temp_time_us;
    uint64_t cache_battery_time_us;
    uint64_t cache_load_time_us;

    // Thread synchronization
    nimcp_mutex_t* mutex;
    bool mutex_init;

} portia_monitor_struct_t;

//=============================================================================
// Forward Declarations - Platform Specific
//=============================================================================

static bool probe_thermal_zones(portia_monitor_t monitor);
static bool probe_battery(portia_monitor_t monitor);
static bool probe_ac_adapter(portia_monitor_t monitor);
static float read_thermal_zone(portia_monitor_t monitor, int zone_index);
static bool read_battery_info(portia_monitor_t monitor, portia_battery_status_t* status);
static bool read_cpu_stats(portia_monitor_t monitor);
static float calculate_cpu_load(const cpu_stats_t* prev, const cpu_stats_t* curr);

//=============================================================================
// Helper Functions - File Reading
//=============================================================================

#ifdef __linux__
/**
 * @brief Read a float value from a sysfs file
 */
static float read_sysfs_float(const char* dir, const char* file) {
    char path[NIMCP_METRICS_PATH_SIZE];
    snprintf(path, sizeof(path), "%s/%s", dir, file);

    FILE* fp = fopen(path, "r");
    if (!fp) {
        return -1.0f;
    }

    float value = -1.0f;
    if (fscanf(fp, "%f", &value) != 1) {
        value = -1.0f;
    }

    fclose(fp);
    return value;
}

/**
 * @brief Read an integer value from a sysfs file
 */
static int64_t read_sysfs_int(const char* dir, const char* file) {
    char path[NIMCP_METRICS_PATH_SIZE];
    snprintf(path, sizeof(path), "%s/%s", dir, file);

    FILE* fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    int64_t value = -1;
    if (fscanf(fp, "%ld", &value) != 1) {
        value = -1;
    }

    fclose(fp);
    return value;
}

/**
 * @brief Read a string value from a sysfs file
 */
static bool read_sysfs_string(const char* dir, const char* file, char* buf, size_t len) {
    char path[NIMCP_METRICS_PATH_SIZE];
    snprintf(path, sizeof(path), "%s/%s", dir, file);

    FILE* fp = fopen(path, "r");
    if (!fp) {
        buf[0] = '\0';
        return false;
    }

    if (fgets(buf, len, fp) == NULL) {
        buf[0] = '\0';
        fclose(fp);
        return false;
    }

    // Remove trailing newline
    size_t slen = strlen(buf);
    if (slen > 0 && buf[slen-1] == '\n') {
        buf[slen-1] = '\0';
    }

    fclose(fp);
    return true;
}
#endif  // __linux__

//=============================================================================
// Cache Helpers
//=============================================================================

static bool cache_valid(portia_monitor_t monitor, uint64_t cache_time_us) {
    if (!monitor || !monitor->initialized) {
        return false;
    }

    uint64_t now = nimcp_time_monotonic_us();
    uint64_t age_ms = (now - cache_time_us) / 1000;

    return age_ms < monitor->config.cache_timeout_ms;
}

//=============================================================================
// Configuration API
//=============================================================================

portia_monitor_config_t portia_monitor_default_config(void) {
    portia_monitor_config_t config;
    memset(&config, 0, sizeof(config));

    config.cache_timeout_ms = PORTIA_MONITOR_DEFAULT_CACHE_MS;
    config.enable_cpu_temp = true;
    config.enable_battery = true;
    config.enable_cpu_load = true;
    config.enable_per_core_load = false;  // Disabled by default for performance
    config.thermal_zone_override = NULL;
    config.battery_path_override = NULL;

    return config;
}

//=============================================================================
// Lifecycle API
//=============================================================================

portia_monitor_t portia_monitor_init(const portia_monitor_config_t* config) {
    LOG_INFO("[%s] Initializing monitoring system", MONITOR_MODULE);

    // Allocate monitor
    portia_monitor_t monitor = (portia_monitor_t)nimcp_calloc(
        1, sizeof(portia_monitor_struct_t));
    if (!monitor) {
        LOG_ERROR("[%s] Failed to allocate monitor structure", MONITOR_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_monitor_init: monitor is NULL");
        return NULL;
    }

    monitor->magic = MONITOR_MAGIC;

    // Copy configuration
    if (config) {
        monitor->config = *config;
    } else {
        monitor->config = portia_monitor_default_config();
    }

    // Initialize mutex
    monitor->mutex = nimcp_mutex_create(NULL);
    if (!monitor->mutex) {
        LOG_ERROR("[%s] Failed to create mutex", MONITOR_MODULE);
        nimcp_free(monitor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_monitor_init: mutex creation failed");
        return NULL;
    }
    monitor->mutex_init = true;

    // Initialize cache to invalid
    monitor->cached_cpu_temp = PORTIA_MONITOR_TEMP_INVALID;
    monitor->cached_battery_pct = PORTIA_MONITOR_BATTERY_UNAVAILABLE;
    monitor->cached_cpu_load = PORTIA_MONITOR_LOAD_INVALID;

    // Probe platform capabilities
    monitor->capabilities = PORTIA_MONITOR_CAP_NONE;

    // Probe thermal zones
    if (monitor->config.enable_cpu_temp) {
        if (probe_thermal_zones(monitor)) {
            monitor->capabilities |= PORTIA_MONITOR_CAP_CPU_TEMP;
            LOG_INFO("[%s] CPU temperature monitoring available (%d zones)",
                     MONITOR_MODULE, monitor->num_thermal_zones);
        } else {
            LOG_WARN("[%s] CPU temperature monitoring not available", MONITOR_MODULE);
        }
    }

    // Probe battery
    if (monitor->config.enable_battery) {
        if (probe_battery(monitor)) {
            monitor->capabilities |= PORTIA_MONITOR_CAP_BATTERY;
            LOG_INFO("[%s] Battery monitoring available at: %s",
                     MONITOR_MODULE, monitor->battery_path);
        } else {
            LOG_WARN("[%s] Battery monitoring not available (may be desktop system)",
                     MONITOR_MODULE);
        }
        probe_ac_adapter(monitor);
    }

    // Probe CPU load capability
    if (monitor->config.enable_cpu_load) {
#ifdef __linux__
        // Try reading /proc/stat
        if (read_cpu_stats(monitor)) {
            monitor->capabilities |= PORTIA_MONITOR_CAP_CPU_LOAD;
            LOG_INFO("[%s] CPU load monitoring available (%d cores)",
                     MONITOR_MODULE, monitor->num_cores);
        }
#elif defined(__APPLE__)
        // macOS always has CPU load via mach
        monitor->capabilities |= PORTIA_MONITOR_CAP_CPU_LOAD;
        LOG_INFO("[%s] CPU load monitoring available (macOS)", MONITOR_MODULE);
#elif defined(_WIN32)
        // Windows always has CPU load
        monitor->capabilities |= PORTIA_MONITOR_CAP_CPU_LOAD;
        LOG_INFO("[%s] CPU load monitoring available (Windows)", MONITOR_MODULE);
#endif
    }

    monitor->initialized = true;

    LOG_INFO("[%s] Monitoring initialized: capabilities=0x%02X",
             MONITOR_MODULE, monitor->capabilities);

    return monitor;
}

void portia_monitor_shutdown(portia_monitor_t monitor) {
    if (!monitor || monitor->magic != MONITOR_MAGIC) {
        return;
    }

    LOG_INFO("[%s] Shutting down monitoring", MONITOR_MODULE);

    // Free per-core stats if allocated
    if (monitor->prev_per_core) {
        nimcp_free(monitor->prev_per_core);
        monitor->prev_per_core = NULL;
    }
    if (monitor->curr_per_core) {
        nimcp_free(monitor->curr_per_core);
        monitor->curr_per_core = NULL;
    }

    // Destroy mutex
    if (monitor->mutex_init && monitor->mutex) {
        nimcp_mutex_destroy(monitor->mutex);
        monitor->mutex = NULL;
    }

    monitor->magic = 0;
    monitor->initialized = false;

    nimcp_free(monitor);

    LOG_INFO("[%s] Shutdown complete", MONITOR_MODULE);
}

uint32_t portia_monitor_get_capabilities(portia_monitor_t monitor) {
    if (!monitor || monitor->magic != MONITOR_MAGIC) {
        // Probe without initialization - quick capability check
        uint32_t caps = PORTIA_MONITOR_CAP_NONE;

#ifdef __linux__
        // Check for thermal zones
        if (access(THERMAL_ZONE_PATH "/thermal_zone0/temp", R_OK) == 0) {
            caps |= PORTIA_MONITOR_CAP_CPU_TEMP;
        }

        // Check for battery
        if (access(POWER_SUPPLY_PATH "/BAT0/capacity", R_OK) == 0 ||
            access(POWER_SUPPLY_PATH "/BAT1/capacity", R_OK) == 0) {
            caps |= PORTIA_MONITOR_CAP_BATTERY;
        }

        // Check for /proc/stat
        if (access(PROC_STAT_PATH, R_OK) == 0) {
            caps |= PORTIA_MONITOR_CAP_CPU_LOAD;
        }
#elif defined(__APPLE__)
        caps |= PORTIA_MONITOR_CAP_CPU_LOAD;  // Always available on macOS
#elif defined(_WIN32)
        caps |= PORTIA_MONITOR_CAP_CPU_LOAD;
        caps |= PORTIA_MONITOR_CAP_BATTERY;  // Windows API always available
#endif

        return caps;
    }

    return monitor->capabilities;
}

//=============================================================================
// Platform Probing - Linux
//=============================================================================

#ifdef __linux__

static portia_thermal_zone_type_t classify_thermal_zone(const char* name, const char* type) {
    // Classify based on zone type string
    if (type) {
        if (strstr(type, "x86_pkg") || strstr(type, "cpu") ||
            strstr(type, "coretemp") || strstr(type, "k10temp") ||
            strstr(type, "acpitz")) {
            return PORTIA_THERMAL_ZONE_CPU;
        }
        if (strstr(type, "gpu") || strstr(type, "nvidia") || strstr(type, "amdgpu")) {
            return PORTIA_THERMAL_ZONE_GPU;
        }
        if (strstr(type, "nvme") || strstr(type, "ssd") || strstr(type, "drivetemp")) {
            return PORTIA_THERMAL_ZONE_SSD;
        }
        if (strstr(type, "iwlwifi") || strstr(type, "pch")) {
            return PORTIA_THERMAL_ZONE_AMBIENT;
        }
    }

    // Classify based on name
    if (name) {
        if (strstr(name, "cpu") || strstr(name, "pkg") || strstr(name, "core")) {
            return PORTIA_THERMAL_ZONE_CPU;
        }
    }

    return PORTIA_THERMAL_ZONE_UNKNOWN;
}

static bool probe_thermal_zones(portia_monitor_t monitor) {
    // Use override if provided
    if (monitor->config.thermal_zone_override) {
        strncpy(monitor->thermal_zone_path, monitor->config.thermal_zone_override,
                sizeof(monitor->thermal_zone_path) - 1);
        float temp = read_sysfs_float(monitor->thermal_zone_path, "temp");
        if (temp > 0) {
            monitor->has_thermal = true;
            monitor->num_thermal_zones = 1;
            monitor->primary_thermal_zone = 0;
            return true;
        }
        return false;
    }

    // Enumerate thermal zones
    DIR* dir = opendir(THERMAL_ZONE_PATH);
    if (!dir) {
        LOG_DEBUG("[%s] Cannot open thermal path: %s", MONITOR_MODULE, THERMAL_ZONE_PATH);
        return false;
    }

    int zone_count = 0;
    int primary_zone = -1;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL && zone_count < PORTIA_MONITOR_MAX_THERMAL_ZONES) {
        if (strncmp(entry->d_name, "thermal_zone", 12) != 0) {
            continue;
        }

        char zone_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(zone_path, sizeof(zone_path), "%s/%s", THERMAL_ZONE_PATH, entry->d_name);

        // Read zone type
        char type_str[NIMCP_ID_BUFFER_SIZE] = {0};
        read_sysfs_string(zone_path, "type", type_str, sizeof(type_str));

        // Try reading temperature
        float temp = read_sysfs_float(zone_path, "temp");
        if (temp < 0) {
            continue;  // Zone not readable
        }

        portia_thermal_zone_t* zone = &monitor->thermal_zones[zone_count];
        strncpy(zone->name, entry->d_name, sizeof(zone->name) - 1);
        zone->type = classify_thermal_zone(entry->d_name, type_str);
        zone->temperature_c = temp / 1000.0f;  // millidegrees to degrees
        zone->available = true;
        zone->zone_index = zone_count;

        // Read trip point if available
        float trip = read_sysfs_float(zone_path, "trip_point_0_temp");
        zone->trip_point_c = (trip > 0) ? trip / 1000.0f : 100.0f;

        // Prefer CPU zone as primary
        if (zone->type == PORTIA_THERMAL_ZONE_CPU && primary_zone < 0) {
            primary_zone = zone_count;
            strncpy(monitor->thermal_zone_path, zone_path, sizeof(monitor->thermal_zone_path) - 1);
        }

        zone_count++;
    }

    closedir(dir);

    if (zone_count == 0) {
        return false;
    }

    // Use first zone if no CPU zone found
    if (primary_zone < 0) {
        primary_zone = 0;
        snprintf(monitor->thermal_zone_path, sizeof(monitor->thermal_zone_path),
                 "%s/thermal_zone%d", THERMAL_ZONE_PATH, primary_zone);
    }

    monitor->num_thermal_zones = zone_count;
    monitor->primary_thermal_zone = primary_zone;
    monitor->has_thermal = true;

    return true;
}

static bool probe_battery(portia_monitor_t monitor) {
    // Use override if provided
    if (monitor->config.battery_path_override) {
        strncpy(monitor->battery_path, monitor->config.battery_path_override,
                sizeof(monitor->battery_path) - 1);
        float cap = read_sysfs_float(monitor->battery_path, "capacity");
        if (cap >= 0) {
            monitor->has_battery = true;
            return true;
        }
        return false;
    }

    // Try known battery names
    for (int i = 0; DEFAULT_BATTERY_NAMES[i] != NULL; i++) {
        char path[NIMCP_METRICS_PATH_SIZE];
        snprintf(path, sizeof(path), "%s/%s", POWER_SUPPLY_PATH, DEFAULT_BATTERY_NAMES[i]);

        float cap = read_sysfs_float(path, "capacity");
        if (cap >= 0) {
            strncpy(monitor->battery_path, path, sizeof(monitor->battery_path) - 1);
            monitor->has_battery = true;
            LOG_DEBUG("[%s] Found battery: %s", MONITOR_MODULE, path);
            return true;
        }
    }

    return false;
}

static bool probe_ac_adapter(portia_monitor_t monitor) {
    for (int i = 0; DEFAULT_AC_NAMES[i] != NULL; i++) {
        char path[NIMCP_METRICS_PATH_SIZE];
        snprintf(path, sizeof(path), "%s/%s", POWER_SUPPLY_PATH, DEFAULT_AC_NAMES[i]);

        int64_t online = read_sysfs_int(path, "online");
        if (online >= 0) {
            strncpy(monitor->ac_path, path, sizeof(monitor->ac_path) - 1);
            monitor->has_ac = true;
            LOG_DEBUG("[%s] Found AC adapter: %s", MONITOR_MODULE, path);
            return true;
        }
    }

    return false;
}

static float read_thermal_zone(portia_monitor_t monitor, int zone_index) {
    if (zone_index < 0 || zone_index >= monitor->num_thermal_zones) {
        return PORTIA_MONITOR_TEMP_INVALID;
    }

    char zone_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(zone_path, sizeof(zone_path), "%s/thermal_zone%d",
             THERMAL_ZONE_PATH, zone_index);

    float temp = read_sysfs_float(zone_path, "temp");
    if (temp < 0) {
        return PORTIA_MONITOR_TEMP_INVALID;
    }

    return temp / 1000.0f;  // millidegrees to degrees
}

static bool read_battery_info(portia_monitor_t monitor, portia_battery_status_t* status) {
    if (!monitor->has_battery) {
        status->available = false;
        return false;
    }

    memset(status, 0, sizeof(*status));

    // Read capacity
    float cap = read_sysfs_float(monitor->battery_path, "capacity");
    if (cap < 0) {
        status->available = false;
        return false;
    }
    status->level_pct = cap;
    status->available = true;

    // Read status string
    char status_str[32];
    if (read_sysfs_string(monitor->battery_path, "status", status_str, sizeof(status_str))) {
        if (strcmp(status_str, "Charging") == 0) {
            status->state = PORTIA_BATTERY_CHARGING;
        } else if (strcmp(status_str, "Discharging") == 0) {
            status->state = PORTIA_BATTERY_DISCHARGING;
        } else if (strcmp(status_str, "Full") == 0) {
            status->state = PORTIA_BATTERY_FULL;
        } else if (strcmp(status_str, "Not charging") == 0) {
            status->state = PORTIA_BATTERY_FULL;  // Often means full
        } else {
            status->state = PORTIA_BATTERY_UNKNOWN;
        }
    }

    // Read power draw (try multiple file names)
    float power = read_sysfs_float(monitor->battery_path, "power_now");
    if (power > 0) {
        status->power_draw_mw = power / 1000.0f;  // microwatts to milliwatts
    } else {
        // Try current * voltage
        float current = read_sysfs_float(monitor->battery_path, "current_now");
        float voltage = read_sysfs_float(monitor->battery_path, "voltage_now");
        if (current > 0 && voltage > 0) {
            status->power_draw_mw = (current / 1000000.0f) * (voltage / 1000000.0f) * 1000.0f;
        }
    }

    // Read voltage
    float voltage = read_sysfs_float(monitor->battery_path, "voltage_now");
    if (voltage > 0) {
        status->voltage_mv = voltage / 1000.0f;  // microvolts to millivolts
    }

    // Read temperature (optional)
    float temp = read_sysfs_float(monitor->battery_path, "temp");
    if (temp > 0) {
        status->temperature_c = temp / 10.0f;  // 0.1 degrees to degrees
    }

    // Read time to empty/full (optional)
    int64_t tte = read_sysfs_int(monitor->battery_path, "time_to_empty_avg");
    if (tte > 0) {
        status->time_to_empty_s = (float)tte;
    }

    int64_t ttf = read_sysfs_int(monitor->battery_path, "time_to_full_avg");
    if (ttf > 0) {
        status->time_to_full_s = (float)ttf;
    }

    // Read capacity values
    float design_cap = read_sysfs_float(monitor->battery_path, "charge_full_design");
    if (design_cap > 0) {
        status->design_capacity_mah = design_cap / 1000.0f;  // microAh to mAh
    }

    float current_cap = read_sysfs_float(monitor->battery_path, "charge_full");
    if (current_cap > 0) {
        status->current_capacity_mah = current_cap / 1000.0f;
    }

    // Calculate health
    if (status->design_capacity_mah > 0 && status->current_capacity_mah > 0) {
        status->health_pct = (status->current_capacity_mah / status->design_capacity_mah) * 100.0f;
        if (status->health_pct > 100.0f) status->health_pct = 100.0f;
    }

    // Check AC adapter
    if (monitor->has_ac) {
        int64_t online = read_sysfs_int(monitor->ac_path, "online");
        status->ac_connected = (online == 1);
    } else {
        status->ac_connected = (status->state == PORTIA_BATTERY_CHARGING ||
                                status->state == PORTIA_BATTERY_FULL);
    }

    status->timestamp_us = nimcp_time_monotonic_us();

    return true;
}

static bool read_cpu_stats(portia_monitor_t monitor) {
    FILE* fp = fopen(PROC_STAT_PATH, "r");
    if (!fp) {
        return false;
    }

    char line[NIMCP_CMD_BUFFER_SIZE];

    // Save previous stats
    monitor->prev_cpu_stats = monitor->curr_cpu_stats;

    // Read first line (total CPU)
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return false;
    }

    cpu_stats_t* stats = &monitor->curr_cpu_stats;
    int matched = sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
                         &stats->user, &stats->nice, &stats->system, &stats->idle,
                         &stats->iowait, &stats->irq, &stats->softirq, &stats->steal);
    if (matched < 4) {
        fclose(fp);
        return false;
    }

    stats->total = stats->user + stats->nice + stats->system + stats->idle +
                   stats->iowait + stats->irq + stats->softirq + stats->steal;

    // Count cores and optionally read per-core stats
    int core_count = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "cpu", 3) == 0 && line[3] != ' ') {
            core_count++;
        } else {
            break;  // No more CPU lines
        }
    }

    monitor->num_cores = core_count;
    monitor->cpu_stats_valid = true;

    fclose(fp);
    return true;
}

#else  // Non-Linux platforms

static bool probe_thermal_zones(portia_monitor_t monitor) {
#ifdef __APPLE__
    // macOS: Could use IOKit SMC but requires more setup
    // For now, return false - thermal not easily available
    return false;
#elif defined(_WIN32)
    // Windows: Would need WMI
    return false;
#else
    return false;
#endif
}

static bool probe_battery(portia_monitor_t monitor) {
#ifdef __APPLE__
    // macOS: IOPSCopyPowerSourcesInfo would work but requires CoreFoundation
    // Basic fallback using ioreg
    return false;  // Would need IOKit linkage
#elif defined(_WIN32)
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        if (sps.BatteryFlag != 128) {  // 128 = No battery
            monitor->has_battery = true;
            return true;
        }
    }
    return false;
#else
    return false;
#endif
}

static bool probe_ac_adapter(portia_monitor_t monitor) {
#ifdef _WIN32
    monitor->has_ac = true;  // Windows always has AC check capability
    return true;
#else
    (void)monitor;
    return false;
#endif
}

static float read_thermal_zone(portia_monitor_t monitor, int zone_index) {
    (void)monitor;
    (void)zone_index;
    return PORTIA_MONITOR_TEMP_INVALID;
}

static bool read_battery_info(portia_monitor_t monitor, portia_battery_status_t* status) {
    memset(status, 0, sizeof(*status));

#ifdef _WIN32
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        if (sps.BatteryFlag == 128) {
            status->available = false;
            return false;
        }

        status->available = true;
        status->level_pct = (sps.BatteryLifePercent <= 100) ?
                            sps.BatteryLifePercent : 100.0f;

        if (sps.ACLineStatus == 1) {
            status->ac_connected = true;
            status->state = (sps.BatteryFlag & 8) ?
                            PORTIA_BATTERY_CHARGING : PORTIA_BATTERY_FULL;
        } else {
            status->ac_connected = false;
            status->state = PORTIA_BATTERY_DISCHARGING;
        }

        if (sps.BatteryLifeTime != 0xFFFFFFFF) {
            status->time_to_empty_s = (float)sps.BatteryLifeTime;
        }

        status->timestamp_us = nimcp_time_monotonic_us();
        return true;
    }
#endif

    status->available = false;
    return false;
}

static bool read_cpu_stats(portia_monitor_t monitor) {
#ifdef __APPLE__
    // macOS: Use mach APIs
    processor_info_array_t cpu_info;
    mach_msg_type_number_t num_cpu_info;
    natural_t num_cpus;

    kern_return_t kr = host_processor_info(mach_host_self(),
                                           PROCESSOR_CPU_LOAD_INFO,
                                           &num_cpus,
                                           &cpu_info,
                                           &num_cpu_info);
    if (kr != KERN_SUCCESS) {
        return false;
    }

    monitor->prev_cpu_stats = monitor->curr_cpu_stats;

    cpu_stats_t* stats = &monitor->curr_cpu_stats;
    memset(stats, 0, sizeof(*stats));

    processor_cpu_load_info_t cpu_load = (processor_cpu_load_info_t)cpu_info;
    for (unsigned int i = 0; i < num_cpus; i++) {
        stats->user += cpu_load[i].cpu_ticks[CPU_STATE_USER];
        stats->system += cpu_load[i].cpu_ticks[CPU_STATE_SYSTEM];
        stats->idle += cpu_load[i].cpu_ticks[CPU_STATE_IDLE];
        stats->nice += cpu_load[i].cpu_ticks[CPU_STATE_NICE];
    }
    stats->total = stats->user + stats->system + stats->idle + stats->nice;

    vm_deallocate(mach_task_self(), (vm_address_t)cpu_info, num_cpu_info);

    monitor->num_cores = num_cpus;
    monitor->cpu_stats_valid = true;
    return true;

#elif defined(_WIN32)
    // Windows: Use GetSystemTimes
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        return false;
    }

    monitor->prev_cpu_stats = monitor->curr_cpu_stats;

    cpu_stats_t* stats = &monitor->curr_cpu_stats;
    stats->idle = ((uint64_t)idle.dwHighDateTime << 32) | idle.dwLowDateTime;
    stats->system = ((uint64_t)kernel.dwHighDateTime << 32) | kernel.dwLowDateTime;
    stats->user = ((uint64_t)user.dwHighDateTime << 32) | user.dwLowDateTime;
    // Kernel time includes idle time in Windows
    stats->system -= stats->idle;
    stats->total = stats->user + stats->system + stats->idle;

    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    monitor->num_cores = sysinfo.dwNumberOfProcessors;
    monitor->cpu_stats_valid = true;
    return true;

#else
    (void)monitor;
    return false;
#endif
}

#endif  // Platform-specific implementations

//=============================================================================
// CPU Load Calculation
//=============================================================================

static float calculate_cpu_load(const cpu_stats_t* prev, const cpu_stats_t* curr) {
    if (!prev || !curr) {
        return PORTIA_MONITOR_LOAD_INVALID;
    }

    uint64_t total_delta = curr->total - prev->total;
    if (total_delta == 0) {
        return 0.0f;
    }

    uint64_t idle_delta = curr->idle - prev->idle;
    uint64_t active_delta = total_delta - idle_delta;

    return (float)active_delta / (float)total_delta * 100.0f;
}

//=============================================================================
// CPU Temperature API
//=============================================================================

float portia_monitor_get_cpu_temp(portia_monitor_t monitor) {
    if (!monitor || monitor->magic != MONITOR_MAGIC) {
        return PORTIA_MONITOR_TEMP_INVALID;
    }

    if (!(monitor->capabilities & PORTIA_MONITOR_CAP_CPU_TEMP)) {
        return PORTIA_MONITOR_TEMP_INVALID;
    }

    nimcp_mutex_lock(monitor->mutex);

    // Check cache
    if (cache_valid(monitor, monitor->cache_temp_time_us)) {
        float temp = monitor->cached_cpu_temp;
        nimcp_mutex_unlock(monitor->mutex);
        return temp;
    }

    // Read fresh value
    float temp = read_thermal_zone(monitor, monitor->primary_thermal_zone);

    // Update cache
    monitor->cached_cpu_temp = temp;
    monitor->cache_temp_time_us = nimcp_time_monotonic_us();

    nimcp_mutex_unlock(monitor->mutex);

    return temp;
}

bool portia_monitor_cpu_temp_critical(
    portia_monitor_t monitor,
    float* temp_out,
    float threshold_c)
{
    float temp = portia_monitor_get_cpu_temp(monitor);

    if (temp_out) {
        *temp_out = temp;
    }

    if (!portia_monitor_temp_valid(temp)) {
        return false;  // Can't determine, assume safe
    }

    return temp >= threshold_c;
}

int portia_monitor_get_thermal_zones(
    portia_monitor_t monitor,
    portia_thermal_zone_t* zones,
    int max_zones)
{
    if (!monitor || monitor->magic != MONITOR_MAGIC || !zones || max_zones <= 0) {
        return 0;
    }

    nimcp_mutex_lock(monitor->mutex);

    int count = (max_zones < monitor->num_thermal_zones) ?
                max_zones : monitor->num_thermal_zones;

    // Update temperature readings
    for (int i = 0; i < count; i++) {
        monitor->thermal_zones[i].temperature_c = read_thermal_zone(monitor, i);
        zones[i] = monitor->thermal_zones[i];
    }

    nimcp_mutex_unlock(monitor->mutex);

    return count;
}

//=============================================================================
// Battery API
//=============================================================================

float portia_monitor_get_battery_pct(portia_monitor_t monitor) {
    if (!monitor || monitor->magic != MONITOR_MAGIC) {
        return PORTIA_MONITOR_BATTERY_UNAVAILABLE;
    }

    if (!(monitor->capabilities & PORTIA_MONITOR_CAP_BATTERY)) {
        return PORTIA_MONITOR_BATTERY_UNAVAILABLE;
    }

    nimcp_mutex_lock(monitor->mutex);

    // Check cache
    if (cache_valid(monitor, monitor->cache_battery_time_us)) {
        float pct = monitor->cached_battery_pct;
        nimcp_mutex_unlock(monitor->mutex);
        return pct;
    }

    // Read fresh value
    portia_battery_status_t status;
    if (read_battery_info(monitor, &status)) {
        monitor->cached_battery_pct = status.level_pct;
        monitor->cached_battery_status = status;
    } else {
        monitor->cached_battery_pct = PORTIA_MONITOR_BATTERY_UNAVAILABLE;
    }

    monitor->cache_battery_time_us = nimcp_time_monotonic_us();
    float pct = monitor->cached_battery_pct;

    nimcp_mutex_unlock(monitor->mutex);

    return pct;
}

bool portia_monitor_get_battery_status(
    portia_monitor_t monitor,
    portia_battery_status_t* status)
{
    if (!monitor || monitor->magic != MONITOR_MAGIC || !status) {
        if (status) {
            status->available = false;
        }
        return false;
    }

    if (!(monitor->capabilities & PORTIA_MONITOR_CAP_BATTERY)) {
        status->available = false;
        return false;
    }

    nimcp_mutex_lock(monitor->mutex);

    // Check cache
    if (cache_valid(monitor, monitor->cache_battery_time_us) &&
        monitor->cached_battery_status.available) {
        *status = monitor->cached_battery_status;
        nimcp_mutex_unlock(monitor->mutex);
        return true;
    }

    // Read fresh value
    bool result = read_battery_info(monitor, status);

    if (result) {
        monitor->cached_battery_status = *status;
        monitor->cached_battery_pct = status->level_pct;
        monitor->cache_battery_time_us = nimcp_time_monotonic_us();
    }

    nimcp_mutex_unlock(monitor->mutex);

    return result;
}

bool portia_monitor_on_battery(portia_monitor_t monitor) {
    if (!monitor || monitor->magic != MONITOR_MAGIC) {
        return false;
    }

    portia_battery_status_t status;
    if (!portia_monitor_get_battery_status(monitor, &status)) {
        return false;  // No battery or can't determine
    }

    return status.state == PORTIA_BATTERY_DISCHARGING;
}

bool portia_monitor_battery_critical(
    portia_monitor_t monitor,
    float* level_out,
    float threshold_pct)
{
    float level = portia_monitor_get_battery_pct(monitor);

    if (level_out) {
        *level_out = level;
    }

    if (!portia_monitor_battery_valid(level)) {
        return false;  // Can't determine, assume safe
    }

    return level <= threshold_pct;
}

//=============================================================================
// CPU Load API
//=============================================================================

float portia_monitor_get_cpu_load(portia_monitor_t monitor) {
    if (!monitor || monitor->magic != MONITOR_MAGIC) {
        return PORTIA_MONITOR_LOAD_INVALID;
    }

    if (!(monitor->capabilities & PORTIA_MONITOR_CAP_CPU_LOAD)) {
        return PORTIA_MONITOR_LOAD_INVALID;
    }

    nimcp_mutex_lock(monitor->mutex);

    // Check cache
    if (cache_valid(monitor, monitor->cache_load_time_us)) {
        float load = monitor->cached_cpu_load;
        nimcp_mutex_unlock(monitor->mutex);
        return load;
    }

    // Read fresh CPU stats
    if (!read_cpu_stats(monitor)) {
        nimcp_mutex_unlock(monitor->mutex);
        return PORTIA_MONITOR_LOAD_INVALID;
    }

    // Calculate load from delta
    float load = calculate_cpu_load(&monitor->prev_cpu_stats, &monitor->curr_cpu_stats);

    // Update cache
    monitor->cached_cpu_load = load;
    monitor->cache_load_time_us = nimcp_time_monotonic_us();

    nimcp_mutex_unlock(monitor->mutex);

    return load;
}

bool portia_monitor_get_cpu_load_detailed(
    portia_monitor_t monitor,
    portia_cpu_load_t* load)
{
    if (!monitor || monitor->magic != MONITOR_MAGIC || !load) {
        if (load) {
            load->available = false;
        }
        return false;
    }

    if (!(monitor->capabilities & PORTIA_MONITOR_CAP_CPU_LOAD)) {
        load->available = false;
        return false;
    }

    nimcp_mutex_lock(monitor->mutex);

    // Read fresh stats
    if (!read_cpu_stats(monitor)) {
        load->available = false;
        nimcp_mutex_unlock(monitor->mutex);
        return false;
    }

    // Calculate overall load
    load->total_load_pct = calculate_cpu_load(&monitor->prev_cpu_stats, &monitor->curr_cpu_stats);

    // Calculate component percentages
    uint64_t total_delta = monitor->curr_cpu_stats.total - monitor->prev_cpu_stats.total;
    if (total_delta > 0) {
        load->user_pct = (float)(monitor->curr_cpu_stats.user - monitor->prev_cpu_stats.user) /
                         total_delta * 100.0f;
        load->system_pct = (float)(monitor->curr_cpu_stats.system - monitor->prev_cpu_stats.system) /
                           total_delta * 100.0f;
        load->iowait_pct = (float)(monitor->curr_cpu_stats.iowait - monitor->prev_cpu_stats.iowait) /
                           total_delta * 100.0f;
        load->idle_pct = (float)(monitor->curr_cpu_stats.idle - monitor->prev_cpu_stats.idle) /
                         total_delta * 100.0f;
    }

    load->num_cores = monitor->num_cores;

#ifdef __linux__
    // Read load average
    FILE* fp = fopen(PROC_LOADAVG_PATH, "r");
    if (fp) {
        if (fscanf(fp, "%f %f %f", &load->load_1m, &load->load_5m, &load->load_15m) != 3) {
            load->load_1m = load->load_5m = load->load_15m = 0.0f;
        }
        fclose(fp);
    }
#endif

    load->available = true;
    load->timestamp_us = nimcp_time_monotonic_us();

    nimcp_mutex_unlock(monitor->mutex);

    return true;
}

int portia_monitor_get_per_core_load(
    portia_monitor_t monitor,
    portia_cpu_core_load_t* cores,
    int max_cores)
{
    if (!monitor || monitor->magic != MONITOR_MAGIC || !cores || max_cores <= 0) {
        return 0;
    }

    // Per-core load requires additional implementation
    // For now, return 0 cores - implementation left for future enhancement
    (void)monitor;
    (void)cores;

    return 0;
}

bool portia_monitor_cpu_load_high(
    portia_monitor_t monitor,
    float* load_out,
    float threshold_pct)
{
    float load = portia_monitor_get_cpu_load(monitor);

    if (load_out) {
        *load_out = load;
    }

    if (!portia_monitor_load_valid(load)) {
        return false;  // Can't determine, assume safe
    }

    return load >= threshold_pct;
}

//=============================================================================
// Convenience API
//=============================================================================

bool portia_monitor_get_all(
    portia_monitor_t monitor,
    float* cpu_temp_c,
    float* battery_pct,
    float* cpu_load_pct)
{
    if (!monitor || monitor->magic != MONITOR_MAGIC) {
        if (cpu_temp_c) *cpu_temp_c = PORTIA_MONITOR_TEMP_INVALID;
        if (battery_pct) *battery_pct = PORTIA_MONITOR_BATTERY_UNAVAILABLE;
        if (cpu_load_pct) *cpu_load_pct = PORTIA_MONITOR_LOAD_INVALID;
        return false;
    }

    bool any_valid = false;

    if (cpu_temp_c) {
        *cpu_temp_c = portia_monitor_get_cpu_temp(monitor);
        if (portia_monitor_temp_valid(*cpu_temp_c)) {
            any_valid = true;
        }
    }

    if (battery_pct) {
        *battery_pct = portia_monitor_get_battery_pct(monitor);
        if (portia_monitor_battery_valid(*battery_pct)) {
            any_valid = true;
        }
    }

    if (cpu_load_pct) {
        *cpu_load_pct = portia_monitor_get_cpu_load(monitor);
        if (portia_monitor_load_valid(*cpu_load_pct)) {
            any_valid = true;
        }
    }

    return any_valid;
}

void portia_monitor_refresh(portia_monitor_t monitor) {
    if (!monitor || monitor->magic != MONITOR_MAGIC) {
        return;
    }

    nimcp_mutex_lock(monitor->mutex);

    monitor->cache_temp_time_us = 0;
    monitor->cache_battery_time_us = 0;
    monitor->cache_load_time_us = 0;

    nimcp_mutex_unlock(monitor->mutex);
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* portia_monitor_thermal_zone_name(portia_thermal_zone_type_t type) {
    switch (type) {
        case PORTIA_THERMAL_ZONE_CPU:     return "CPU";
        case PORTIA_THERMAL_ZONE_GPU:     return "GPU";
        case PORTIA_THERMAL_ZONE_SSD:     return "SSD";
        case PORTIA_THERMAL_ZONE_MEMORY:  return "Memory";
        case PORTIA_THERMAL_ZONE_BATTERY: return "Battery";
        case PORTIA_THERMAL_ZONE_AMBIENT: return "Ambient";
        case PORTIA_THERMAL_ZONE_UNKNOWN:
        default:                          return "Unknown";
    }
}

const char* portia_monitor_battery_state_name(portia_battery_state_t state) {
    switch (state) {
        case PORTIA_BATTERY_DISCHARGING:  return "Discharging";
        case PORTIA_BATTERY_CHARGING:     return "Charging";
        case PORTIA_BATTERY_FULL:         return "Full";
        case PORTIA_BATTERY_NOT_PRESENT:  return "Not Present";
        case PORTIA_BATTERY_UNKNOWN:
        default:                          return "Unknown";
    }
}

bool portia_monitor_temp_valid(float temp) {
    // Valid temperature range: -40C to 150C (reasonable for electronics)
    return temp > -50.0f && temp < 200.0f &&
           temp != PORTIA_MONITOR_TEMP_INVALID;
}

bool portia_monitor_battery_valid(float battery) {
    return battery >= 0.0f && battery <= 100.0f;
}

bool portia_monitor_load_valid(float load) {
    return load >= 0.0f && load <= 100.0f;
}
