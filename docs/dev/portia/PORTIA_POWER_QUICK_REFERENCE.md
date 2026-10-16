# Portia Power System - Quick Reference

## One-Minute Quickstart

```c
#include "portia/nimcp_portia_power.h"

// Initialize
portia_power_manager_t pm = portia_power_init(NULL);

// Get current status
power_status_t status;
portia_power_get_status(pm, &status);
printf("Battery: %.1f%%\n", status.battery_level_pct);

// Get recommended tier config
platform_tier_t tier = platform_tier_detect();
power_tier_config_t config = portia_power_get_tier_config(pm, tier, -1);

// Use config.max_neurons, config.processing_rate_hz, etc.

// Cleanup
portia_power_shutdown(pm);
```

## Power Profiles at a Glance

| Profile | Battery | Neurons | Rate | Modules | Use Case |
|---------|---------|---------|------|---------|----------|
| **PERFORMANCE** | >80% or AC | 100% | 100 Hz | All | Full capability |
| **BALANCED** | 40-80% | 75% | 80 Hz | Core | Normal operation |
| **SAVER** | 20-40% | 50% | 50 Hz | Essential | Extended battery |
| **CRITICAL** | 10-20% | 25% | 20 Hz | Minimal | Low power |
| **EMERGENCY** | <10% | 10% | 5 Hz | Reactive | Survival mode |

## API Cheat Sheet

### Initialization
```c
portia_power_manager_t portia_power_init(const portia_power_config_t* config);
void portia_power_shutdown(portia_power_manager_t manager);
```

### Status
```c
bool portia_power_get_status(portia_power_manager_t mgr, power_status_t* status);
power_profile_t portia_power_get_profile(portia_power_manager_t mgr);
float portia_power_estimate_runtime(portia_power_manager_t mgr, float safety_margin);
```

### Configuration
```c
power_tier_config_t portia_power_get_tier_config(
    portia_power_manager_t mgr,
    platform_tier_t base_tier,
    power_profile_t profile);  // Use -1 for current profile
```

### Manual Control
```c
power_profile_t portia_power_set_profile(portia_power_manager_t mgr, power_profile_t profile);
```

### Statistics
```c
bool portia_power_get_stats(portia_power_manager_t mgr, portia_power_stats_t* stats);
void portia_power_reset_stats(portia_power_manager_t mgr);
```

## Power Status Fields

```c
typedef struct {
    power_source_t source;          // AC, Battery, Solar, USB, Unknown
    float battery_level_pct;        // 0-100
    float discharge_rate_mw;        // Current draw (mW)
    float estimated_runtime_s;      // Seconds remaining
    float temperature_c;            // Battery temp (°C)
    bool charging;                  // Currently charging
    bool health_good;               // Battery health OK
    bool plugged_in;                // AC adapter connected
    uint64_t timestamp_us;          // When sampled
} power_status_t;
```

## Tier Config Fields

```c
typedef struct {
    power_profile_t profile;        // Current profile
    uint32_t max_neurons;           // Max neurons allowed
    uint32_t max_synapses;          // Max synapses allowed
    uint32_t cognitive_modules;     // Enabled modules (bitmask)
    float processing_rate_hz;       // Update frequency
    float sampling_rate;            // State sampling (0-1)
    uint32_t batch_size;            // Neurons per batch
    bool enable_learning;           // Learning enabled
    bool enable_persistence;        // Checkpointing enabled
    bool enable_bio_async;          // Bio-async enabled
    bool enable_gpu;                // GPU enabled
    float wake_interval_s;          // Sleep/wake interval
    float active_duty_cycle;        // Active time (0-1)
    uint32_t memory_budget_mb;      // Memory budget (MB)
    uint32_t compute_budget_gops;   // Compute budget (GOPS)
} power_tier_config_t;
```

## Configuration

```c
portia_power_config_t config = portia_power_default_config();

// Monitoring
config.poll_interval_ms = 5000;           // Poll every 5 seconds
config.auto_adjust_profile = true;        // Auto-adjust on battery change
config.enable_bio_async_events = true;    // Send events

// Thresholds (percentage)
config.performance_threshold = 80.0f;     // >80% = Performance
config.balanced_threshold = 40.0f;        // 40-80% = Balanced
config.saver_threshold = 20.0f;           // 20-40% = Saver
config.critical_threshold = 10.0f;        // 10-20% = Critical

// Thermal
config.max_safe_temp_c = 45.0f;           // Max safe temp
config.thermal_throttle_temp_c = 40.0f;   // Throttle temp

// Runtime estimation
config.discharge_history_s = 60.0f;       // 60s history window
```

## Bio-Async Events

```c
// Event types
POWER_EVENT_PROFILE_CHANGE      // Profile changed
POWER_EVENT_BATTERY_LOW         // <20% battery
POWER_EVENT_BATTERY_CRITICAL    // <10% battery
POWER_EVENT_CHARGING_STARTED    // Started charging
POWER_EVENT_CHARGING_COMPLETE   // Battery full
POWER_EVENT_THERMAL_WARNING     // Battery too hot
POWER_EVENT_HEALTH_DEGRADED     // Battery health issue

// Event message
typedef struct {
    bio_message_header_t header;
    power_event_type_t event_type;
    power_profile_t old_profile;
    power_profile_t new_profile;
    float battery_level_pct;
    float temperature_c;
    uint64_t timestamp_us;
} bio_msg_power_event_t;
```

## Cognitive Modules

```c
// Core modules (always try to keep)
COGNITIVE_MODULE_ATTENTION
COGNITIVE_MODULE_WORKING_MEMORY
COGNITIVE_MODULE_SALIENCE

// Emotional modules
COGNITIVE_MODULE_EMOTIONS
COGNITIVE_MODULE_EMOTIONAL_TAG

// Memory systems
COGNITIVE_MODULE_SEMANTIC_MEMORY
COGNITIVE_MODULE_EPISODIC_MEMORY
COGNITIVE_MODULE_CONSOLIDATION

// Executive functions
COGNITIVE_MODULE_EXECUTIVE
COGNITIVE_MODULE_REASONING
COGNITIVE_MODULE_CURIOSITY

// Meta-cognitive (disabled in low power)
COGNITIVE_MODULE_META_LEARNING
COGNITIVE_MODULE_INTROSPECTION
COGNITIVE_MODULE_SELF_AWARENESS

// Social cognition (disabled in low power)
COGNITIVE_MODULE_THEORY_OF_MIND
COGNITIVE_MODULE_MIRROR_NEURONS
COGNITIVE_MODULE_EMPATHY

// Advanced features (disabled in low power)
COGNITIVE_MODULE_GLOBAL_WORKSPACE
COGNITIVE_MODULE_PREDICTIVE
COGNITIVE_MODULE_ETHICS
```

## Utility Functions

```c
const char* portia_power_get_source_name(power_source_t source);
const char* portia_power_get_profile_name(power_profile_t profile);
bool portia_power_is_limited(power_source_t source);  // True for battery/solar
```

## Statistics

```c
typedef struct {
    uint64_t samples_taken;          // Total samples
    uint64_t profile_changes;        // Profile change count
    uint64_t events_sent;            // Bio-async events sent
    float avg_battery_level;         // Average battery level
    float avg_discharge_rate_mw;     // Average discharge rate
    float total_runtime_hours;       // Total runtime on battery
    float max_temperature_c;         // Peak temperature
    uint32_t thermal_throttles;      // Thermal throttle events
    uint64_t uptime_s;               // Power monitor uptime
} portia_power_stats_t;
```

## Linux Battery Paths

```bash
/sys/class/power_supply/BAT0/status         # Charging/Discharging/Full
/sys/class/power_supply/BAT0/capacity       # 0-100 percent
/sys/class/power_supply/BAT0/power_now      # μW
/sys/class/power_supply/BAT0/charge_now     # μAh
/sys/class/power_supply/BAT0/charge_full    # μAh
/sys/class/power_supply/BAT0/temp           # 0.1°C units
```

## Typical Usage Pattern

```c
// At startup
portia_power_manager_t pm = portia_power_init(NULL);
platform_tier_t tier = platform_tier_detect();

// In main loop
power_profile_t profile = portia_power_get_profile(pm);
power_tier_config_t config = portia_power_get_tier_config(pm, tier, profile);

// Apply configuration
brain_set_max_neurons(brain, config.max_neurons);
brain_set_update_rate(brain, config.processing_rate_hz);

// Handle low power
if (profile >= POWER_PROFILE_SAVER) {
    disable_non_essential_modules();
}

// At shutdown
portia_power_shutdown(pm);
```

## Error Handling

```c
portia_power_manager_t pm = portia_power_init(NULL);
if (!pm) {
    LOG_ERROR("Failed to initialize power monitoring");
    return -1;
}

power_status_t status;
if (!portia_power_get_status(pm, &status)) {
    LOG_WARN("Could not read battery status");
    // Continue with assumed values
}
```

## Testing

```bash
# Run unit tests
./build/test/unit/portia/test_portia_power

# Run demo
./build/examples/portia_power_demo

# Simulate battery drain (for testing)
echo "50" > /sys/class/power_supply/BAT0/capacity
```

## Performance

- **CPU Overhead**: ~0.1% (5s poll interval)
- **Memory**: ~1KB per manager
- **Latency**: <1ms per status read
- **Thread-Safe**: Yes (mutex-protected)

## Common Patterns

### Auto-Adjustment
```c
config.auto_adjust_profile = true;  // Enable
// Profile changes automatically based on battery
```

### Manual Override
```c
portia_power_set_profile(pm, POWER_PROFILE_SAVER);
// Force saver mode regardless of battery
```

### Runtime Check
```c
float runtime = portia_power_estimate_runtime(pm, 0.9f);
if (runtime > 0 && runtime < 600) {  // <10 minutes
    save_checkpoint();
}
```

## Files

- **Header**: `include/portia/nimcp_portia_power.h`
- **Implementation**: `src/portia/nimcp_portia_power.c`
- **Demo**: `examples/portia_power_demo.c`
- **Tests**: `test/unit/portia/test_portia_power.cpp`
- **Docs**: `docs/PORTIA_POWER_SYSTEM.md`

## See Also

- Platform Tier System: `docs/PLATFORM_TIER_SYSTEM.md`
- Bio-Async Messages: `include/async/nimcp_bio_messages.h`
- Cognitive Modules: `include/cognitive/`
